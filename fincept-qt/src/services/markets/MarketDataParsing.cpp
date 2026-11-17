#include "services/markets/MarketDataParsing.h"

#include <QJsonValue>

#include <cmath>

namespace fincept::services {

namespace {

// Unity builds batch this file with others: every helper carries the mdp_ prefix.

// A finite JSON number, else NaN. Null, strings and missing keys are absent.
double mdp_num(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isDouble())
        return kMarketDataAbsent;
    const double d = v.toDouble();
    return std::isfinite(d) ? d : kMarketDataAbsent;
}

// As mdp_num, but only a value > 0 counts (prices, volumes, averages).
double mdp_pos(const QJsonObject& o, const char* key) {
    const double d = mdp_num(o, key);
    return d > 0 ? d : kMarketDataAbsent;
}

// Epoch seconds from a positive number, else 0.
qint64 mdp_epoch(const QJsonObject& o, const char* key) {
    const double d = mdp_num(o, key);
    return d > 0 ? static_cast<qint64>(d) : 0;
}

// A whole number in [lo, hi], else `absent`.
int mdp_int(const QJsonObject& o, const char* key, int lo, int hi, int absent) {
    const double d = mdp_num(o, key);
    if (std::isnan(d) || d < lo || d > hi || d != std::floor(d))
        return absent;
    return static_cast<int>(d);
}

QString mdp_str(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toString().trimmed();
}

QDate mdp_date(const QJsonObject& o, const char* key) {
    const QString s = mdp_str(o, key);
    return s.isEmpty() ? QDate() : QDate::fromString(s, Qt::ISODate);
}

// The optional QuoteData fields. Reply rows and the cache JSON use the same
// keys for them, so one reader and one writer serve both.
void mdp_read_quote_extras(const QJsonObject& o, QuoteData& d) {
    d.market_date = mdp_str(o, "market_date");
    d.market_time = mdp_epoch(o, "market_time");
    d.market_state = normalize_market_state(o.value(QLatin1String("market_state")).toString());
    d.currency = mdp_str(o, "currency");
    d.price_digits = mdp_int(o, "price_digits", 0, 12, -1);
    d.quote_type = mdp_str(o, "quote_type").toUpper();
    d.exchange = mdp_str(o, "exchange");
    d.exchange_name = mdp_str(o, "exchange_name");
    d.exchange_timezone = mdp_str(o, "exchange_timezone");
    d.exchange_delay_min = mdp_int(o, "exchange_delay_min", 0, 24 * 60, -1);
    // Any finite value: futures can trade below zero (CL=F, April 2020).
    d.open = mdp_num(o, "open");
    d.previous_close = mdp_num(o, "previous_close");
    d.pre_market_price = mdp_pos(o, "pre_market_price");
    d.pre_market_change_pct = mdp_num(o, "pre_market_change_percent");
    d.pre_market_time = mdp_epoch(o, "pre_market_time");
    d.post_market_price = mdp_pos(o, "post_market_price");
    d.post_market_change_pct = mdp_num(o, "post_market_change_percent");
    d.post_market_time = mdp_epoch(o, "post_market_time");
    d.average_volume = mdp_pos(o, "average_volume");
    d.average_volume_10d = mdp_pos(o, "average_volume_10d");
    d.fifty_two_week_high = mdp_pos(o, "fifty_two_week_high");
    d.fifty_two_week_low = mdp_pos(o, "fifty_two_week_low");
    d.fifty_day_average = mdp_pos(o, "fifty_day_average");
    d.two_hundred_day_average = mdp_pos(o, "two_hundred_day_average");
    // Fetch time (epoch seconds). A value in milliseconds is accepted too.
    d.fetched_at = mdp_epoch(o, "timestamp");
    if (d.fetched_at > 100'000'000'000LL)
        d.fetched_at /= 1000;
}

void mdp_put_str(QJsonObject& o, const char* key, const QString& v) {
    if (!v.isEmpty())
        o.insert(QLatin1String(key), v);
}

void mdp_put_num(QJsonObject& o, const char* key, double v) {
    if (std::isfinite(v))
        o.insert(QLatin1String(key), v);
}

void mdp_put_epoch(QJsonObject& o, const char* key, qint64 v) {
    if (v > 0)
        o.insert(QLatin1String(key), static_cast<double>(v));
}

void mdp_write_quote_extras(const QuoteData& q, QJsonObject& o) {
    mdp_put_str(o, "market_date", q.market_date);
    mdp_put_epoch(o, "market_time", q.market_time);
    mdp_put_str(o, "market_state", q.market_state);
    mdp_put_str(o, "currency", q.currency);
    if (q.price_digits >= 0)
        o.insert(QLatin1String("price_digits"), q.price_digits);
    mdp_put_str(o, "quote_type", q.quote_type);
    mdp_put_str(o, "exchange", q.exchange);
    mdp_put_str(o, "exchange_name", q.exchange_name);
    mdp_put_str(o, "exchange_timezone", q.exchange_timezone);
    if (q.exchange_delay_min >= 0)
        o.insert(QLatin1String("exchange_delay_min"), q.exchange_delay_min);
    mdp_put_num(o, "open", q.open);
    mdp_put_num(o, "previous_close", q.previous_close);
    mdp_put_num(o, "pre_market_price", q.pre_market_price);
    mdp_put_num(o, "pre_market_change_percent", q.pre_market_change_pct);
    mdp_put_epoch(o, "pre_market_time", q.pre_market_time);
    mdp_put_num(o, "post_market_price", q.post_market_price);
    mdp_put_num(o, "post_market_change_percent", q.post_market_change_pct);
    mdp_put_epoch(o, "post_market_time", q.post_market_time);
    mdp_put_num(o, "average_volume", q.average_volume);
    mdp_put_num(o, "average_volume_10d", q.average_volume_10d);
    mdp_put_num(o, "fifty_two_week_high", q.fifty_two_week_high);
    mdp_put_num(o, "fifty_two_week_low", q.fifty_two_week_low);
    mdp_put_num(o, "fifty_day_average", q.fifty_day_average);
    mdp_put_num(o, "two_hundred_day_average", q.two_hundred_day_average);
    mdp_put_epoch(o, "timestamp", q.fetched_at);
}

} // namespace

QString normalize_market_state(const QString& raw) {
    const QString s = raw.trimmed().toUpper();
    if (s == QLatin1String("PRE") || s == QLatin1String("REGULAR") || s == QLatin1String("POST") ||
        s == QLatin1String("CLOSED"))
        return s;
    if (s == QLatin1String("PREPRE") || s == QLatin1String("POSTPOST"))
        return QStringLiteral("CLOSED");
    return {};
}

QuoteData quote_from_reply_row(const QJsonObject& row) {
    const QString sym = row.value(QLatin1String("symbol")).toString();
    QuoteData d{sym,
                row.value(QLatin1String("name")).toString(sym),
                row.value(QLatin1String("price")).toDouble(),
                row.value(QLatin1String("change")).toDouble(),
                row.value(QLatin1String("change_percent")).toDouble(),
                row.value(QLatin1String("high")).toDouble(),
                row.value(QLatin1String("low")).toDouble(),
                row.value(QLatin1String("volume")).toDouble()};
    mdp_read_quote_extras(row, d);
    return d;
}

QJsonObject quote_to_cache_json(const QuoteData& q) {
    QJsonObject o;
    o.insert(QLatin1String("symbol"), q.symbol);
    o.insert(QLatin1String("name"), q.name);
    o.insert(QLatin1String("price"), q.price);
    o.insert(QLatin1String("change"), q.change);
    o.insert(QLatin1String("change_pct"), q.change_pct);
    o.insert(QLatin1String("high"), q.high);
    o.insert(QLatin1String("low"), q.low);
    o.insert(QLatin1String("volume"), q.volume);
    mdp_write_quote_extras(q, o);
    return o;
}

QuoteData quote_from_cache_json(const QJsonObject& o) {
    QuoteData d{o.value(QLatin1String("symbol")).toString(),
                o.value(QLatin1String("name")).toString(),
                o.value(QLatin1String("price")).toDouble(),
                o.value(QLatin1String("change")).toDouble(),
                o.value(QLatin1String("change_pct")).toDouble(),
                o.value(QLatin1String("high")).toDouble(),
                o.value(QLatin1String("low")).toDouble(),
                o.value(QLatin1String("volume")).toDouble()};
    mdp_read_quote_extras(o, d);
    return d;
}

InfoData info_from_batch_row(const QJsonObject& row, const QString& symbol) {
    InfoData d;
    d.symbol = mdp_str(row, "symbol");
    if (d.symbol.isEmpty())
        d.symbol = symbol;
    d.name = mdp_str(row, "name");
    d.sector = mdp_str(row, "sector");
    d.industry = mdp_str(row, "industry");
    d.country = mdp_str(row, "country");
    d.currency = mdp_str(row, "currency");
    d.market_cap = mdp_pos(row, "market_cap");
    d.pe_ratio = mdp_num(row, "pe_ratio");
    d.forward_pe = mdp_num(row, "forward_pe");
    d.price_to_book = mdp_num(row, "price_to_book");
    d.dividend_yield = mdp_num(row, "dividend_yield");
    d.beta = mdp_num(row, "beta");
    d.week52_high = mdp_pos(row, "fifty_two_week_high");
    d.week52_low = mdp_pos(row, "fifty_two_week_low");
    d.avg_volume = mdp_pos(row, "average_volume");
    d.revenue_per_share = mdp_num(row, "revenue_per_share");
    d.eps = d.revenue_per_share; // historic meaning of the field, same as fetch_info()
    d.roe = mdp_num(row, "roe");
    d.profit_margin = mdp_num(row, "profit_margin");
    d.debt_to_equity = mdp_num(row, "debt_to_equity");
    d.current_ratio = mdp_num(row, "current_ratio");

    d.quote_type = mdp_str(row, "quote_type").toUpper();
    d.financial_currency = mdp_str(row, "financial_currency");
    d.exchange = mdp_str(row, "exchange");
    d.exchange_name = mdp_str(row, "exchange_name");
    d.recommendation_key = mdp_str(row, "recommendation_key").toLower();
    d.ex_dividend_date = mdp_date(row, "ex_dividend_date");
    d.earnings_date = mdp_date(row, "earnings_date");
    d.trailing_eps = mdp_num(row, "trailing_eps");
    d.forward_eps = mdp_num(row, "forward_eps");
    d.price_to_sales = mdp_num(row, "price_to_sales");
    d.peg_ratio = mdp_num(row, "peg_ratio");
    d.enterprise_value = mdp_num(row, "enterprise_value");
    d.ev_to_ebitda = mdp_num(row, "ev_to_ebitda");
    d.dividend_rate = mdp_num(row, "dividend_rate");
    d.payout_ratio = mdp_num(row, "payout_ratio");
    d.average_volume_10d = mdp_pos(row, "average_volume_10d");
    d.shares_outstanding = mdp_pos(row, "shares_outstanding");
    d.float_shares = mdp_pos(row, "float_shares");
    d.short_ratio = mdp_num(row, "short_ratio");
    d.short_percent_float = mdp_num(row, "short_percent_float");
    d.gross_margin = mdp_num(row, "gross_margin");
    d.operating_margin = mdp_num(row, "operating_margin");
    d.revenue_growth = mdp_num(row, "revenue_growth");
    d.earnings_growth = mdp_num(row, "earnings_growth");
    d.target_mean_price = mdp_pos(row, "target_mean_price");
    d.recommendation_mean = mdp_num(row, "recommendation_mean");
    return d;
}

} // namespace fincept::services
