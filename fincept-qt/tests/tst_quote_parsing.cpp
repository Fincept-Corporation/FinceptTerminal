// Unit tests for src/services/markets/MarketDataParsing.{h,cpp} and
// src/services/markets/MarketPerformance.{h,cpp} (Qt Core only).
//
//   * quote_from_reply_row — a yfinance_data.py quote row (batch_all / batch_quotes
//     / quote) becomes QuoteData; optional keys an older script omits stay
//     ABSENT (NaN / "" / -1 / 0 time), never 0-as-data.
//   * quote_to_cache_json / quote_from_cache_json — the quote cache round trip
//     keeps every field (fetch_quotes() serves cache hits from it).
//   * info_from_batch_row — a batch_info row becomes InfoData with NaN for
//     everything Yahoo did not report, original fields included.
//   * compute_performance — 1W/1M/3M/6M/YTD/1Y change, 52-week range and
//     20-day realised volatility from a daily series.

#include "services/markets/MarketDataParsing.h"
#include "services/markets/MarketPerformance.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace fincept::services;

namespace {

QJsonObject tqp_json(const char* text) {
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

// Seconds of a daily bar stamped at local midnight of `d` in a zone `utc_offset_h` from UTC.
qint64 tqp_bar_ts(const QDate& d, int utc_offset_h) {
    return QDate(1970, 1, 1).daysTo(d) * 86'400LL - static_cast<qint64>(utc_offset_h) * 3'600;
}

// One bar per weekday from `first` to `last` (inclusive); close = f(index).
template <typename F>
QVector<HistoryPoint> tqp_series(const QDate& first, const QDate& last, int utc_offset_h, F close_of) {
    QVector<HistoryPoint> out;
    int i = 0;
    for (QDate d = first; d <= last; d = d.addDays(1)) {
        if (d.dayOfWeek() > 5)
            continue;
        HistoryPoint p;
        p.timestamp = tqp_bar_ts(d, utc_offset_h);
        p.close = close_of(i, d);
        p.open = p.close;
        p.high = p.close * 1.01;
        p.low = p.close * 0.99;
        p.volume = 1000;
        out.append(p);
        ++i;
    }
    return out;
}

double tqp_close_on(const QVector<HistoryPoint>& s, const QDate& on_or_before, int utc_offset_h) {
    double c = std::nan("");
    for (const auto& p : s)
        if (p.timestamp <= tqp_bar_ts(on_or_before, utc_offset_h))
            c = p.close;
    return c;
}

} // namespace

class TstQuoteParsing : public QObject {
    Q_OBJECT

  private slots:
    void market_state_is_normalised();
    void full_batch_row_parses_every_field();
    void old_script_row_leaves_new_fields_absent();
    void null_values_are_absent_not_zero();
    void cache_round_trip_keeps_every_field();
    void cache_written_by_old_build_still_reads();
    void info_row_parses_and_marks_absent_as_nan();
    void info_row_without_symbol_uses_requested_symbol();

    void perf_empty_series_is_all_absent();
    void perf_horizon_changes_use_close_on_or_before_anchor();
    void perf_ytd_uses_last_close_of_previous_year();
    void perf_one_year_tolerates_series_starting_after_anchor();
    void perf_short_series_gives_absent_long_horizons();
    void perf_live_price_moves_changes_and_range();
    void perf_52w_range_and_distance();
    void perf_volatility_matches_closed_form();
    void perf_asian_midnight_stamps_keep_their_session_date();
    void perf_unsorted_and_bad_bars_are_handled();
};

void TstQuoteParsing::market_state_is_normalised() {
    QCOMPARE(normalize_market_state("REGULAR"), QStringLiteral("REGULAR"));
    QCOMPARE(normalize_market_state("pre"), QStringLiteral("PRE"));
    QCOMPARE(normalize_market_state(" POST "), QStringLiteral("POST"));
    QCOMPARE(normalize_market_state("CLOSED"), QStringLiteral("CLOSED"));
    QCOMPARE(normalize_market_state("PREPRE"), QStringLiteral("CLOSED"));
    QCOMPARE(normalize_market_state("POSTPOST"), QStringLiteral("CLOSED"));
    QCOMPARE(normalize_market_state(""), QString());
    QCOMPARE(normalize_market_state("HALTED"), QString());
}

void TstQuoteParsing::full_batch_row_parses_every_field() {
    // Shape of a live batch_all quote row (AAPL, Nov 2026) from yfinance_data.py.
    const QJsonObject row = tqp_json(R"({"symbol": "AAPL", "price": 356.27, "change": 1.39, "change_percent": 0.39,
        "volume": 39985300, "high": 360.83, "low": 355.75, "open": 357.96, "previous_close": 354.88,
        "timestamp": 1794772605, "exchange": "NMS", "market_date": "2026-11-13", "currency": "USD",
        "exchange_name": "NasdaqGS", "exchange_timezone": "America/New_York", "quote_type": "EQUITY",
        "exchange_delay_min": 15, "market_state": "POSTPOST", "market_time": 1794603601,
        "pre_market_price": 357.1, "pre_market_change_percent": 0.2331, "pre_market_time": 1794580000,
        "post_market_price": 355.3849, "post_market_change_percent": -0.2484, "post_market_time": 1794617980,
        "average_volume": 42760568.0, "average_volume_10d": 52276110.0, "fifty_two_week_high": 360.83,
        "fifty_two_week_low": 243.42, "fifty_day_average": 339.0364, "two_hundred_day_average": 301.2529,
        "price_digits": 2})");
    const QuoteData q = quote_from_reply_row(row);
    QCOMPARE(q.symbol, QStringLiteral("AAPL"));
    QCOMPARE(q.name, QStringLiteral("AAPL")); // no name in the row -> the symbol
    QCOMPARE(q.price, 356.27);
    QCOMPARE(q.change, 1.39);
    QCOMPARE(q.change_pct, 0.39);
    QCOMPARE(q.high, 360.83);
    QCOMPARE(q.low, 355.75);
    QCOMPARE(q.volume, 39985300.0);
    QCOMPARE(q.market_date, QStringLiteral("2026-11-13"));
    QCOMPARE(q.market_time, qint64(1794603601));
    QCOMPARE(q.market_state, QStringLiteral("CLOSED"));
    QCOMPARE(q.currency, QStringLiteral("USD"));
    QCOMPARE(q.price_digits, 2);
    QCOMPARE(q.quote_type, QStringLiteral("EQUITY"));
    QCOMPARE(q.exchange, QStringLiteral("NMS"));
    QCOMPARE(q.exchange_name, QStringLiteral("NasdaqGS"));
    QCOMPARE(q.exchange_timezone, QStringLiteral("America/New_York"));
    QCOMPARE(q.exchange_delay_min, 15);
    QCOMPARE(q.open, 357.96);
    QCOMPARE(q.previous_close, 354.88);
    QCOMPARE(q.pre_market_price, 357.1);
    QCOMPARE(q.pre_market_change_pct, 0.2331);
    QCOMPARE(q.pre_market_time, qint64(1794580000));
    QCOMPARE(q.post_market_price, 355.3849);
    QCOMPARE(q.post_market_change_pct, -0.2484);
    QCOMPARE(q.post_market_time, qint64(1794617980));
    QCOMPARE(q.average_volume, 42760568.0);
    QCOMPARE(q.average_volume_10d, 52276110.0);
    QCOMPARE(q.fifty_two_week_high, 360.83);
    QCOMPARE(q.fifty_two_week_low, 243.42);
    QCOMPARE(q.fifty_day_average, 339.0364);
    QCOMPARE(q.two_hundred_day_average, 301.2529);
}

void TstQuoteParsing::old_script_row_leaves_new_fields_absent() {
    // What batch_quotes returned before this change.
    const QJsonObject row = tqp_json(R"({"symbol": "VOD.L", "price": 116.95, "change": -1.1, "change_percent": -0.93,
        "volume": 28338396, "high": 117.15, "low": 115.3, "timestamp": 1794772022, "exchange": "",
        "market_date": "2026-11-13"})");
    const QuoteData q = quote_from_reply_row(row);
    QCOMPARE(q.price, 116.95);
    QCOMPARE(q.market_date, QStringLiteral("2026-11-13"));
    QVERIFY(q.exchange.isEmpty());
    QVERIFY(q.currency.isEmpty());
    QVERIFY(q.quote_type.isEmpty());
    QVERIFY(q.market_state.isEmpty());
    QVERIFY(q.exchange_timezone.isEmpty());
    QCOMPARE(q.market_time, qint64(0));
    QCOMPARE(q.price_digits, -1);
    QCOMPARE(q.exchange_delay_min, -1);
    QVERIFY(std::isnan(q.open));
    QVERIFY(std::isnan(q.previous_close));
    QVERIFY(std::isnan(q.pre_market_price));
    QVERIFY(std::isnan(q.post_market_change_pct));
    QVERIFY(std::isnan(q.average_volume));
    QVERIFY(std::isnan(q.fifty_two_week_high));
    QVERIFY(std::isnan(q.two_hundred_day_average));
}

void TstQuoteParsing::null_values_are_absent_not_zero() {
    const QJsonObject row = tqp_json(R"({"symbol": "X", "price": 10, "high": null, "low": null, "open": null,
        "previous_close": null, "average_volume": 0, "fifty_two_week_high": "n/a", "price_digits": 40,
        "exchange_delay_min": -5, "currency": "  GBp ", "minor": 1})");
    const QuoteData q = quote_from_reply_row(row);
    QCOMPARE(q.high, 0.0); // original field: historic 0-when-unknown
    QCOMPARE(q.low, 0.0);
    QVERIFY(std::isnan(q.open));
    QVERIFY(std::isnan(q.previous_close));
    QVERIFY(std::isnan(q.average_volume)); // 0 average volume = not reported (FX, funds)
    QVERIFY(std::isnan(q.fifty_two_week_high));
    QCOMPARE(q.price_digits, -1); // out of range
    QCOMPARE(q.exchange_delay_min, -1);
    QCOMPARE(q.currency, QStringLiteral("GBp")); // trimmed, case kept (pence != pounds)
}

void TstQuoteParsing::cache_round_trip_keeps_every_field() {
    QuoteData q{QStringLiteral("7203.T"), QStringLiteral("Toyota"), 3418, 10, 0.29, 3429, 3343, 19521200};
    q.market_date = QStringLiteral("2026-11-13");
    q.market_time = 1794551400;
    q.market_state = QStringLiteral("CLOSED");
    q.currency = QStringLiteral("JPY");
    q.price_digits = 2;
    q.quote_type = QStringLiteral("EQUITY");
    q.exchange = QStringLiteral("JPX");
    q.exchange_name = QStringLiteral("Tokyo");
    q.exchange_timezone = QStringLiteral("Asia/Tokyo");
    q.exchange_delay_min = 20;
    q.open = 3357;
    q.previous_close = 3408;
    q.post_market_price = 3420;
    q.post_market_change_pct = 0.06;
    q.post_market_time = 1794560000;
    q.average_volume = 25888128;
    q.average_volume_10d = 36316820;
    q.fifty_two_week_high = 4000;
    q.fifty_two_week_low = 2686;
    q.fifty_day_average = 3073.15;
    q.two_hundred_day_average = 3129.8;

    // Through text, as CacheManager stores it.
    const QByteArray text = QJsonDocument(quote_to_cache_json(q)).toJson(QJsonDocument::Compact);
    QVERIFY(!text.contains("nan") && !text.contains("null")); // absent fields are left out
    const QuoteData r = quote_from_cache_json(QJsonDocument::fromJson(text).object());

    QCOMPARE(r.symbol, q.symbol);
    QCOMPARE(r.name, q.name);
    QCOMPARE(r.price, q.price);
    QCOMPARE(r.change, q.change);
    QCOMPARE(r.change_pct, q.change_pct);
    QCOMPARE(r.high, q.high);
    QCOMPARE(r.low, q.low);
    QCOMPARE(r.volume, q.volume);
    QCOMPARE(r.market_date, q.market_date);
    QCOMPARE(r.market_time, q.market_time);
    QCOMPARE(r.market_state, q.market_state);
    QCOMPARE(r.currency, q.currency);
    QCOMPARE(r.price_digits, q.price_digits);
    QCOMPARE(r.quote_type, q.quote_type);
    QCOMPARE(r.exchange, q.exchange);
    QCOMPARE(r.exchange_name, q.exchange_name);
    QCOMPARE(r.exchange_timezone, q.exchange_timezone);
    QCOMPARE(r.exchange_delay_min, q.exchange_delay_min);
    QCOMPARE(r.open, q.open);
    QCOMPARE(r.previous_close, q.previous_close);
    QVERIFY(std::isnan(r.pre_market_price)); // was absent, stays absent
    QVERIFY(std::isnan(r.pre_market_change_pct));
    QCOMPARE(r.pre_market_time, qint64(0));
    QCOMPARE(r.post_market_price, q.post_market_price);
    QCOMPARE(r.post_market_change_pct, q.post_market_change_pct);
    QCOMPARE(r.post_market_time, q.post_market_time);
    QCOMPARE(r.average_volume, q.average_volume);
    QCOMPARE(r.average_volume_10d, q.average_volume_10d);
    QCOMPARE(r.fifty_two_week_high, q.fifty_two_week_high);
    QCOMPARE(r.fifty_two_week_low, q.fifty_two_week_low);
    QCOMPARE(r.fifty_day_average, q.fifty_day_average);
    QCOMPARE(r.two_hundred_day_average, q.two_hundred_day_average);
}

void TstQuoteParsing::cache_written_by_old_build_still_reads() {
    const QJsonObject o = tqp_json(R"({"symbol": "MSFT", "name": "MSFT", "price": 523.88, "change": 0.88,
        "change_pct": 0.17, "high": 526.42, "low": 513.82, "volume": 26155900, "market_state": "REGULAR"})");
    const QuoteData q = quote_from_cache_json(o);
    QCOMPARE(q.change_pct, 0.17);
    QCOMPARE(q.market_state, QStringLiteral("REGULAR"));
    QVERIFY(q.currency.isEmpty());
    QVERIFY(std::isnan(q.previous_close));
}

void TstQuoteParsing::info_row_parses_and_marks_absent_as_nan() {
    // VOD.L as batch_info returned it (Nov 2026): no trailing P/E (a loss), no
    // earnings date scheduled, statements in EUR while the price is in pence.
    const QJsonObject row = tqp_json(R"({"symbol": "VOD.L", "name": "Vodafone Group Public Limited Company",
        "sector": "Communication Services", "industry": "Telecom Services", "country": "United Kingdom",
        "currency": "GBp", "financial_currency": "EUR", "quote_type": "equity", "exchange": "LSE",
        "exchange_name": "LSE", "market_cap": 27095721984.0, "forward_pe": 11.714355,
        "price_to_book": 0.6417, "price_to_sales": 0.7608, "peg_ratio": 0.59, "enterprise_value": 67025605320.0,
        "ev_to_ebitda": 8.532, "dividend_yield": 0.034, "dividend_rate": 0.04, "payout_ratio": 1.0171,
        "ex_dividend_date": "2026-11-19", "beta": 0.321, "fifty_two_week_high": 132.072,
        "fifty_two_week_low": 88.398, "average_volume": 61211213.0, "average_volume_10d": 65378058.0,
        "shares_outstanding": 23168639889.0, "float_shares": 19695457540.0, "revenue_per_share": 1.831,
        "trailing_eps": -0.22, "forward_eps": 0.0998, "roe": -0.08879, "profit_margin": -0.14085,
        "gross_margin": 0.31741, "operating_margin": 0.07533, "debt_to_equity": 126.373, "current_ratio": 1.04,
        "revenue_growth": 0.114, "target_mean_price": 122.45576, "recommendation_mean": 2.73333,
        "recommendation_key": "HOLD"})");
    const InfoData d = info_from_batch_row(row, QStringLiteral("VOD.L"));
    QCOMPARE(d.symbol, QStringLiteral("VOD.L"));
    QCOMPARE(d.name, QStringLiteral("Vodafone Group Public Limited Company"));
    QCOMPARE(d.sector, QStringLiteral("Communication Services"));
    QCOMPARE(d.industry, QStringLiteral("Telecom Services"));
    QCOMPARE(d.country, QStringLiteral("United Kingdom"));
    QCOMPARE(d.currency, QStringLiteral("GBp"));
    QCOMPARE(d.financial_currency, QStringLiteral("EUR"));
    QCOMPARE(d.quote_type, QStringLiteral("EQUITY"));
    QCOMPARE(d.exchange, QStringLiteral("LSE"));
    QCOMPARE(d.market_cap, 27095721984.0);
    QVERIFY(std::isnan(d.pe_ratio)); // not reported: NaN, not 0
    QCOMPARE(d.forward_pe, 11.714355);
    QCOMPARE(d.price_to_book, 0.6417);
    QCOMPARE(d.dividend_yield, 0.034);
    QCOMPARE(d.beta, 0.321);
    QCOMPARE(d.week52_high, 132.072);
    QCOMPARE(d.week52_low, 88.398);
    QCOMPARE(d.avg_volume, 61211213.0);
    QCOMPARE(d.revenue_per_share, 1.831);
    QCOMPARE(d.eps, 1.831); // historic meaning of the field
    QCOMPARE(d.trailing_eps, -0.22);
    QCOMPARE(d.roe, -0.08879);
    QCOMPARE(d.debt_to_equity, 126.373);
    QCOMPARE(d.ex_dividend_date, QDate(2026, 11, 19));
    QVERIFY(!d.earnings_date.isValid());
    QVERIFY(std::isnan(d.short_ratio));
    QVERIFY(std::isnan(d.short_percent_float));
    QVERIFY(std::isnan(d.earnings_growth));
    QCOMPARE(d.recommendation_key, QStringLiteral("hold"));
    QCOMPARE(d.recommendation_mean, 2.73333);
}

void TstQuoteParsing::info_row_without_symbol_uses_requested_symbol() {
    const InfoData d = info_from_batch_row(tqp_json(R"({"name": "EUR/USD", "quote_type": "CURRENCY",
        "earnings_date": "garbage"})"),
                                           QStringLiteral("EURUSD=X"));
    QCOMPARE(d.symbol, QStringLiteral("EURUSD=X"));
    QCOMPARE(d.quote_type, QStringLiteral("CURRENCY"));
    QVERIFY(d.sector.isEmpty());
    QVERIFY(std::isnan(d.market_cap));
    QVERIFY(std::isnan(d.pe_ratio));
    QVERIFY(std::isnan(d.dividend_yield));
    QVERIFY(std::isnan(d.beta));
    QVERIFY(std::isnan(d.eps));
    QVERIFY(!d.earnings_date.isValid());
}

// ── compute_performance ─────────────────────────────────────────────────────

void TstQuoteParsing::perf_empty_series_is_all_absent() {
    const PerformanceStats s = compute_performance({});
    QVERIFY(!s.as_of.isValid());
    QVERIFY(std::isnan(s.last));
    QVERIFY(std::isnan(s.change_1w_pct));
    QVERIFY(std::isnan(s.change_1y_pct));
    QVERIFY(std::isnan(s.high_52w));
    QVERIFY(std::isnan(s.volatility_20d_pct));
}

void TstQuoteParsing::perf_horizon_changes_use_close_on_or_before_anchor() {
    const QDate first(2025, 11, 17);
    const QDate last(2026, 11, 13); // a Friday
    const auto s = tqp_series(first, last, -5, [](int i, const QDate&) { return 100.0 + i; });
    const PerformanceStats p = compute_performance(s);
    QCOMPARE(p.as_of, last);
    QCOMPARE(p.last, s.last().close);
    auto expect = [&](const QDate& anchor) { return (s.last().close / tqp_close_on(s, anchor, -5) - 1) * 100; };
    QVERIFY(qFuzzyCompare(p.change_1w_pct, expect(last.addDays(-7))));
    QVERIFY(qFuzzyCompare(p.change_1m_pct, expect(last.addMonths(-1))));
    QVERIFY(qFuzzyCompare(p.change_3m_pct, expect(last.addMonths(-3))));
    QVERIFY(qFuzzyCompare(p.change_6m_pct, expect(last.addMonths(-6))));
    // 1W anchor is Friday Nov 6: its own close, not Thursday's.
    QCOMPARE(tqp_close_on(s, QDate(2026, 11, 6), -5), s.at(s.size() - 6).close);
}

void TstQuoteParsing::perf_ytd_uses_last_close_of_previous_year() {
    const auto s = tqp_series(QDate(2025, 11, 17), QDate(2026, 11, 13), -5,
                              [](int, const QDate& d) { return d.year() == 2025 ? 50.0 : 80.0; });
    const PerformanceStats p = compute_performance(s);
    QVERIFY(qFuzzyCompare(p.change_ytd_pct, 60.0)); // 50 (Dec 31, 2025) -> 80
}

void TstQuoteParsing::perf_one_year_tolerates_series_starting_after_anchor() {
    // A "1y" series fetched on Sun Nov 15 starts Mon Nov 17, 2025; the 1Y
    // anchor (Nov 13, 2025) has no bar on/before it.
    const auto s = tqp_series(QDate(2025, 11, 17), QDate(2026, 11, 13), -5,
                              [](int i, const QDate&) { return i == 0 ? 40.0 : 100.0; });
    const PerformanceStats p = compute_performance(s);
    QVERIFY(qFuzzyCompare(p.change_1y_pct, 150.0)); // first bar used: 40 -> 100
}

void TstQuoteParsing::perf_short_series_gives_absent_long_horizons() {
    const auto s = tqp_series(QDate(2026, 10, 1), QDate(2026, 11, 13), -5, [](int i, const QDate&) { return 10.0 + i; });
    const PerformanceStats p = compute_performance(s);
    QVERIFY(std::isfinite(p.change_1w_pct));
    QVERIFY(std::isfinite(p.change_1m_pct));
    QVERIFY(std::isnan(p.change_3m_pct));
    QVERIFY(std::isnan(p.change_6m_pct));
    QVERIFY(std::isnan(p.change_ytd_pct));
    QVERIFY(std::isnan(p.change_1y_pct));
    QVERIFY(std::isfinite(p.volatility_20d_pct)); // 32 bars >= 21
    QVERIFY(std::isfinite(p.high_52w));           // since listing
}

void TstQuoteParsing::perf_live_price_moves_changes_and_range() {
    const auto s = tqp_series(QDate(2026, 6, 1), QDate(2026, 11, 13), -5, [](int, const QDate&) { return 100.0; });
    const PerformanceStats p = compute_performance(s, 120.0);
    QCOMPARE(p.last, 120.0);
    QVERIFY(qFuzzyCompare(p.change_1w_pct, 20.0));
    QCOMPARE(p.high_52w, 120.0);     // live price above every bar high (101)
    QCOMPARE(p.from_52w_high_pct, 0.0);
    QVERIFY(qFuzzyCompare(p.from_52w_low_pct, (120.0 / 99.0 - 1) * 100));
}

void TstQuoteParsing::perf_52w_range_and_distance() {
    // Older-than-a-year bars (a 2y series) do not count towards the range.
    auto s = tqp_series(QDate(2024, 11, 18), QDate(2026, 11, 13), -5, [](int, const QDate& d) {
        if (d < QDate(2025, 11, 13))
            return 500.0; // outside the window
        return d == QDate(2026, 3, 2) ? 150.0 : 100.0;
    });
    const PerformanceStats p = compute_performance(s);
    QVERIFY(qFuzzyCompare(p.high_52w, 150.0 * 1.01));
    QVERIFY(qFuzzyCompare(p.low_52w, 99.0));
    QVERIFY(qFuzzyCompare(p.from_52w_high_pct, (100.0 / (150.0 * 1.01) - 1) * 100));
    QVERIFY(p.from_52w_high_pct < 0);
    QVERIFY(p.from_52w_low_pct > 0);
}

void TstQuoteParsing::perf_volatility_matches_closed_form() {
    // Alternating +r / -r log returns: mean ~0, sample sd known in closed form.
    const double r = 0.01;
    double level = 100.0;
    QVector<double> closes;
    for (int i = 0; i < 40; ++i) {
        closes.append(level);
        level *= std::exp(i % 2 == 0 ? r : -r);
    }
    int k = 0;
    const auto s = tqp_series(QDate(2026, 9, 1), QDate(2026, 12, 31), 0,
                              [&](int i, const QDate&) { return i < closes.size() ? closes.at(i) : -1.0; });
    QVector<HistoryPoint> trimmed;
    for (const auto& p : s)
        if (p.close > 0 && k++ < 40)
            trimmed.append(p);
    const PerformanceStats p = compute_performance(trimmed, kMarketDataAbsent, 252);
    // Last 20 returns: 10 x +r, 10 x -r -> mean 0, sample sd = r * sqrt(20/19).
    const double expected = r * std::sqrt(20.0 / 19.0) * std::sqrt(252.0) * 100.0;
    QVERIFY(qFuzzyCompare(p.volatility_20d_pct, expected));
    const PerformanceStats c = compute_performance(trimmed, kMarketDataAbsent, 365);
    QVERIFY(qFuzzyCompare(c.volatility_20d_pct, expected * std::sqrt(365.0 / 252.0)));
}

void TstQuoteParsing::perf_asian_midnight_stamps_keep_their_session_date() {
    // Tokyo bars are stamped 15:00 UTC the day before; Jan 5, 2026 must stay in 2026.
    const auto s = tqp_series(QDate(2025, 12, 1), QDate(2026, 1, 9), +9,
                              [](int, const QDate& d) { return d.year() == 2025 ? 10.0 : 12.0; });
    const PerformanceStats p = compute_performance(s);
    QCOMPARE(p.as_of, QDate(2026, 1, 9));
    QVERIFY(qFuzzyCompare(p.change_ytd_pct, 20.0));
}

void TstQuoteParsing::perf_unsorted_and_bad_bars_are_handled() {
    auto s = tqp_series(QDate(2026, 10, 1), QDate(2026, 11, 13), -5, [](int i, const QDate&) { return 10.0 + i; });
    const PerformanceStats sorted = compute_performance(s);
    std::reverse(s.begin(), s.end());
    HistoryPoint bad;
    bad.timestamp = tqp_bar_ts(QDate(2026, 11, 20), -5);
    bad.close = 0; // no close: skipped, must not become the last bar
    s.append(bad);
    HistoryPoint nan_bar;
    nan_bar.timestamp = tqp_bar_ts(QDate(2026, 11, 21), -5);
    nan_bar.close = std::nan("");
    s.append(nan_bar);
    const PerformanceStats p = compute_performance(s);
    QCOMPARE(p.as_of, sorted.as_of);
    QCOMPARE(p.last, sorted.last);
    QCOMPARE(p.change_1w_pct, sorted.change_1w_pct);
    QCOMPARE(p.volatility_20d_pct, sorted.volatility_20d_pct);
}

QTEST_GUILESS_MAIN(TstQuoteParsing)
#include "tst_quote_parsing.moc"
