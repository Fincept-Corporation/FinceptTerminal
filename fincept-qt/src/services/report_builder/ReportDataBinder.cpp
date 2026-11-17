// ReportDataBinder.cpp — see ReportDataBinder.h.

#include "services/report_builder/ReportDataBinder.h"

#include "core/logging/Logger.h"
#include "core/report/ReportTableCodec.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/equity/EquityResearchService.h"
#include "services/markets/MarketDataService.h"
#include "services/news/NewsService.h"
#include "core/report/ReportVariables.h"
#include "services/connectors/ConnectorArgs.h"
#include "services/connectors/ConnectorCatalog.h"
#include "services/connectors/ConnectorService.h"
#include "services/portfolio/PortfolioService.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportEconCalendar.h"
#include "services/report_builder/ReportLibrary.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>

namespace fincept::services {

namespace rep = ::fincept::report;

namespace {

constexpr int kRdbMaxConcurrent = 4;
constexpr int kRdbMaxChartPoints = 180;
constexpr int kRdbMaxCandles = 260;
constexpr int kRdbMaxStatementRows = 40;

const QString& rdb_dash() {
    static const QString d = QStringLiteral("—");
    return d;
}

QString rdb_tr(const char* text) {
    return QCoreApplication::translate("fincept::services::ReportDataBinder", text);
}

bool rdb_has(double v) {
    return std::isfinite(v);
}

QString rdb_fixed(double v, int dec = 2) {
    return rdb_has(v) ? QString::number(v, 'f', dec) : rdb_dash();
}

// Two decimals for normal prices; small prices (FX crosses, crypto) keep 6 significant
// digits instead of collapsing to "0.00".
QString rdb_price(double v) {
    if (!rdb_has(v))
        return rdb_dash();
    if (std::abs(v) >= 1.0 || v == 0.0)
        return QString::number(v, 'f', 2);
    return QString::number(v, 'g', 6);
}

QString rdb_grouped(double v, int dec = 0) {
    return rdb_has(v) ? QLocale().toString(v, 'f', dec) : rdb_dash();
}

QString rdb_compact(double v) {
    if (!rdb_has(v))
        return rdb_dash();
    const double a = std::abs(v);
    const QString sign = v < 0 ? QStringLiteral("-") : QString();
    if (a >= 1e12)
        return sign + QString::number(a / 1e12, 'f', 2) + QLatin1Char('T');
    if (a >= 1e9)
        return sign + QString::number(a / 1e9, 'f', 2) + QLatin1Char('B');
    if (a >= 1e6)
        return sign + QString::number(a / 1e6, 'f', 2) + QLatin1Char('M');
    if (a >= 1e3)
        return sign + QString::number(a / 1e3, 'f', 1) + QLatin1Char('K');
    return sign + QString::number(a, 'f', 0);
}

QString rdb_pct_of_fraction(double f, int dec = 2) {
    return rdb_has(f) ? QString::number(f * 100.0, 'f', dec) + QLatin1Char('%') : rdb_dash();
}

QString rdb_signed(double v, int dec = 2) {
    if (!rdb_has(v))
        return rdb_dash();
    return (v > 0 ? QStringLiteral("+") : QString()) + QString::number(v, 'f', dec);
}

QString rdb_signed_pct(double pct, int dec = 2) {
    return rdb_has(pct) ? rdb_signed(pct, dec) + QLatin1Char('%') : rdb_dash();
}

BindFetchResult rdb_fail(const QString& why) {
    BindFetchResult r;
    r.ok = false;
    r.error = why;
    return r;
}

QString rdb_json(const QJsonValue& v) {
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
}

QString rdb_stat_line(const QString& label, const QString& value) {
    return label + QStringLiteral(": ") + (value.isEmpty() ? rdb_dash() : value);
}

// ── Quote ───────────────────────────────────────────────────────────────────

BindFetchResult rdb_shape_quote(const QuoteData& q, const QString& block_type, const QString& sym) {
    BindFetchResult r;
    r.ok = true;
    r.source_label = QStringLiteral("Yahoo Finance");
    r.as_of = q.market_time > 0 ? QDateTime::fromSecsSinceEpoch(q.market_time) : QDateTime::currentDateTime();
    const QString name = q.name.isEmpty() ? sym : q.name;
    const QString ccy = q.currency;
    if (block_type == QLatin1String("market_data")) {
        r.patch.insert(QStringLiteral("price"), rdb_price(q.price));
        r.patch.insert(QStringLiteral("change"), rdb_price(q.change));
        r.patch.insert(QStringLiteral("change_pct"), QString::number(q.change_pct, 'f', 2));
        r.patch.insert(QStringLiteral("name"), name);
        r.patch.insert(QStringLiteral("high"), q.high > 0 ? rdb_price(q.high) : QString());
        r.patch.insert(QStringLiteral("low"), q.low > 0 ? rdb_price(q.low) : QString());
        r.patch.insert(QStringLiteral("volume"), q.volume > 0 ? QString::number(q.volume, 'f', 0) : QString());
        r.patch.insert(QStringLiteral("currency"), ccy);
        r.patch.insert(QStringLiteral("market_state"), q.market_state);
    } else if (block_type == QLatin1String("sparkline")) {
        r.patch.insert(QStringLiteral("current"), rdb_price(q.price) + (ccy.isEmpty() ? QString() : QLatin1Char(' ') + ccy));
        r.patch.insert(QStringLiteral("change_pct"), rdb_signed(q.change_pct));
        r.suggested_title = sym;
    } else if (block_type == QLatin1String("table")) {
        r.patch.insert(QStringLiteral("csv"),
                       rep::table::encode({{rdb_tr("Symbol"), rdb_tr("Name"), rdb_tr("Last"), rdb_tr("Change"),
                                            rdb_tr("Change %")},
                                           {sym, name, rdb_price(q.price), rdb_signed(q.change), rdb_signed_pct(q.change_pct)}}));
        r.suggested_title = name;
    } else {
        QStringList lines;
        lines << rdb_stat_line(rdb_tr("Last"), rdb_price(q.price) + (ccy.isEmpty() ? QString() : QLatin1Char(' ') + ccy));
        lines << rdb_stat_line(rdb_tr("Change"), rdb_signed(q.change) + QStringLiteral(" (") + rdb_signed_pct(q.change_pct) + QLatin1Char(')'));
        if (q.low > 0 && q.high > 0)
            lines << rdb_stat_line(rdb_tr("Day range"), rdb_price(q.low) + QStringLiteral(" – ") + rdb_price(q.high));
        if (q.volume > 0)
            lines << rdb_stat_line(rdb_tr("Volume"), rdb_compact(q.volume));
        if (rdb_has(q.fifty_two_week_low) && rdb_has(q.fifty_two_week_high))
            lines << rdb_stat_line(rdb_tr("52W range"), rdb_price(q.fifty_two_week_low) + QStringLiteral(" – ") +
                                                            rdb_price(q.fifty_two_week_high));
        if (!q.market_state.isEmpty())
            lines << rdb_stat_line(rdb_tr("Session"), q.market_state);
        r.patch.insert(QStringLiteral("data"), lines.join(QLatin1Char('\n')));
        r.patch.insert(QStringLiteral("tones"), QStringLiteral(",%1").arg(q.change < 0 ? QStringLiteral("negative")
                                                                          : q.change > 0 ? QStringLiteral("positive")
                                                                                         : QString()));
        r.suggested_title = name + rdb_tr(" quote");
    }
    return r;
}

// ── Quotes (batch) ──────────────────────────────────────────────────────────

QStringList rdb_universe(const QString& u) {
    if (u == QLatin1String("indices"))
        return {"^GSPC", "^IXIC", "^DJI", "^RUT", "^STOXX50E", "^FTSE", "^GDAXI", "^N225", "^HSI", "^NSEI"};
    if (u == QLatin1String("macro"))
        return {"^TNX", "^VIX", "DX-Y.NYB", "EURUSD=X", "USDJPY=X", "GBPUSD=X", "GC=F", "CL=F", "BTC-USD"};
    if (u == QLatin1String("movers"))
        return MarketDataService::mover_symbols();
    if (u == QLatin1String("crypto"))
        return MarketDataService::crypto_symbols();
    return {};
}

BindFetchResult rdb_shape_quotes(QVector<QuoteData> quotes, const QStringList& asked, const rep::Binding& b,
                                 const QString& block_type) {
    BindFetchResult r;
    r.ok = true;
    r.source_label = QStringLiteral("Yahoo Finance");
    r.as_of = QDateTime::currentDateTime();
    QSet<QString> got;
    for (const QuoteData& q : quotes)
        got.insert(q.symbol.toUpper());
    QStringList missing;
    for (const QString& s : asked)
        if (!got.contains(s.toUpper()))
            missing << s;
    const QString sort = b.param("sort");
    if (sort == QLatin1String("change_desc"))
        std::sort(quotes.begin(), quotes.end(), [](const QuoteData& a, const QuoteData& c) { return a.change_pct > c.change_pct; });
    else if (sort == QLatin1String("change_asc"))
        std::sort(quotes.begin(), quotes.end(), [](const QuoteData& a, const QuoteData& c) { return a.change_pct < c.change_pct; });
    else if (sort == QLatin1String("abs_change_desc"))
        std::sort(quotes.begin(), quotes.end(), [](const QuoteData& a, const QuoteData& c) {
            return std::abs(a.change_pct) > std::abs(c.change_pct);
        });
    else { // the order the recipe lists
        std::stable_sort(quotes.begin(), quotes.end(), [&asked](const QuoteData& a, const QuoteData& c) {
            return asked.indexOf(a.symbol) < asked.indexOf(c.symbol);
        });
    }
    const int limit = b.int_param("limit", 0);
    if (limit > 0 && quotes.size() > limit)
        quotes.resize(limit);
    qint64 latest = 0;
    for (const QuoteData& q : quotes)
        latest = std::max(latest, q.market_time);
    if (latest > 0)
        r.as_of = QDateTime::fromSecsSinceEpoch(latest);

    if (block_type == QLatin1String("stats_block")) {
        QStringList lines, tones;
        for (const QuoteData& q : quotes) {
            lines << rdb_stat_line(q.symbol, rdb_price(q.price) + QStringLiteral("  ") + rdb_signed_pct(q.change_pct));
            tones << (q.change_pct < 0 ? QStringLiteral("negative") : q.change_pct > 0 ? QStringLiteral("positive") : QString());
        }
        r.patch.insert(QStringLiteral("data"), lines.join(QLatin1Char('\n')));
        r.patch.insert(QStringLiteral("tones"), tones.join(QLatin1Char(',')));
    } else if (block_type == QLatin1String("list")) {
        QStringList lines;
        for (const QuoteData& q : quotes)
            lines << QStringLiteral("%1 (%2): %3, %4").arg(q.name.isEmpty() ? q.symbol : q.name, q.symbol,
                                                            rdb_price(q.price), rdb_signed_pct(q.change_pct));
        r.has_content = true;
        r.content = lines.join(QLatin1Char('\n'));
    } else {
        QVector<QStringList> grid;
        grid.append({rdb_tr("Symbol"), rdb_tr("Name"), rdb_tr("Last"), rdb_tr("Change"), rdb_tr("Change %"),
                     rdb_tr("Volume")});
        for (const QuoteData& q : quotes)
            grid.append({q.symbol, q.name, rdb_price(q.price), rdb_signed(q.change), rdb_signed_pct(q.change_pct),
                         q.volume > 0 ? rdb_compact(q.volume) : rdb_dash()});
        r.patch.insert(QStringLiteral("csv"), rep::table::encode(grid));
        r.patch.insert(QStringLiteral("rows"), QString::number(grid.size()));
        r.patch.insert(QStringLiteral("cols"), QStringLiteral("6"));
        r.patch.insert(QStringLiteral("note"),
                       missing.isEmpty() ? QString() : rdb_tr("No quote for: %1").arg(missing.join(QStringLiteral(", "))));
    }
    if (quotes.isEmpty())
        return rdb_fail(rdb_tr("no quotes returned for %1").arg(asked.join(QStringLiteral(", "))));
    return r;
}

// ── Info (key statistics) ──────────────────────────────────────────────────

BindFetchResult rdb_shape_info(const InfoData& i, const QString& block_type, const QString& sym) {
    BindFetchResult r;
    r.ok = true;
    r.source_label = QStringLiteral("Yahoo Finance");
    r.as_of = QDateTime::currentDateTime();
    const QString ccy = i.currency;
    QVector<QPair<QString, QString>> rows;
    auto add = [&rows](const QString& label, const QString& value) {
        if (!value.isEmpty() && value != rdb_dash())
            rows.append({label, value});
    };
    add(rdb_tr("Company"), i.name);
    add(rdb_tr("Sector"), i.sector);
    add(rdb_tr("Industry"), i.industry);
    add(rdb_tr("Country"), i.country);
    add(rdb_tr("Market cap"), rdb_has(i.market_cap) && i.market_cap > 0
                                  ? rdb_compact(i.market_cap) + (ccy.isEmpty() ? QString() : QLatin1Char(' ') + ccy)
                                  : QString());
    add(rdb_tr("Enterprise value"), rdb_has(i.enterprise_value) ? rdb_compact(i.enterprise_value) : QString());
    add(rdb_tr("P/E (trailing)"), rdb_has(i.pe_ratio) && i.pe_ratio != 0 ? rdb_fixed(i.pe_ratio) : QString());
    add(rdb_tr("P/E (forward)"), rdb_has(i.forward_pe) && i.forward_pe != 0 ? rdb_fixed(i.forward_pe) : QString());
    add(rdb_tr("EPS (trailing)"), rdb_has(i.trailing_eps) ? rdb_fixed(i.trailing_eps) : QString());
    add(rdb_tr("EV/EBITDA"), rdb_has(i.ev_to_ebitda) ? rdb_fixed(i.ev_to_ebitda) : QString());
    add(rdb_tr("Price/Book"), rdb_has(i.price_to_book) && i.price_to_book != 0 ? rdb_fixed(i.price_to_book) : QString());
    add(rdb_tr("Dividend yield"), rdb_has(i.dividend_yield) && i.dividend_yield != 0 ? rdb_pct_of_fraction(i.dividend_yield) : QString());
    add(rdb_tr("Beta"), rdb_has(i.beta) && i.beta != 0 ? rdb_fixed(i.beta) : QString());
    add(rdb_tr("52W high"), rdb_has(i.week52_high) && i.week52_high > 0 ? rdb_price(i.week52_high) : QString());
    add(rdb_tr("52W low"), rdb_has(i.week52_low) && i.week52_low > 0 ? rdb_price(i.week52_low) : QString());
    add(rdb_tr("Avg volume"), rdb_has(i.avg_volume) && i.avg_volume > 0 ? rdb_compact(i.avg_volume) : QString());
    add(rdb_tr("Gross margin"), rdb_has(i.gross_margin) ? rdb_pct_of_fraction(i.gross_margin) : QString());
    add(rdb_tr("Operating margin"), rdb_has(i.operating_margin) ? rdb_pct_of_fraction(i.operating_margin) : QString());
    add(rdb_tr("Profit margin"), rdb_has(i.profit_margin) && i.profit_margin != 0 ? rdb_pct_of_fraction(i.profit_margin) : QString());
    add(rdb_tr("ROE"), rdb_has(i.roe) && i.roe != 0 ? rdb_pct_of_fraction(i.roe) : QString());
    add(rdb_tr("Revenue growth (YoY)"), rdb_has(i.revenue_growth) ? rdb_pct_of_fraction(i.revenue_growth) : QString());
    add(rdb_tr("Debt/Equity"), rdb_has(i.debt_to_equity) && i.debt_to_equity != 0 ? rdb_fixed(i.debt_to_equity / 100.0) + QLatin1Char('x') : QString());
    add(rdb_tr("Analyst target"), rdb_has(i.target_mean_price) ? rdb_price(i.target_mean_price) : QString());
    if (i.earnings_date.isValid())
        add(rdb_tr("Next earnings"), i.earnings_date.toString(QStringLiteral("d MMM yyyy")));
    if (rows.isEmpty())
        return rdb_fail(rdb_tr("no key statistics returned for %1").arg(sym));

    if (block_type == QLatin1String("table")) {
        QVector<QStringList> grid{{rdb_tr("Metric"), rdb_tr("Value")}};
        for (const auto& p : rows)
            grid.append({p.first, p.second});
        r.patch.insert(QStringLiteral("csv"), rep::table::encode(grid));
        r.patch.insert(QStringLiteral("rows"), QString::number(grid.size()));
        r.patch.insert(QStringLiteral("cols"), QStringLiteral("2"));
    } else {
        QStringList lines;
        for (const auto& p : rows) {
            QString label = p.first;
            label.replace(QLatin1Char(':'), QChar(0x2236));
            lines << rdb_stat_line(label, p.second);
        }
        r.patch.insert(QStringLiteral("data"), lines.join(QLatin1Char('\n')));
        r.patch.insert(QStringLiteral("tones"), QString());
    }
    r.suggested_title = (i.name.isEmpty() ? sym : i.name) + rdb_tr(" — key statistics");
    return r;
}

// ── History ─────────────────────────────────────────────────────────────────

struct RdbSeries {
    QString symbol;
    QVector<HistoryPoint> points;
};

bool rdb_intraday(const QString& interval) {
    return interval.endsWith(QLatin1Char('m')) || interval.endsWith(QLatin1Char('h'));
}

QString rdb_label(qint64 ts, const QString& interval, const QString& range) {
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(ts);
    if (rdb_intraday(interval))
        return dt.toString(QStringLiteral("d MMM HH:mm"));
    if (range == QLatin1String("2y") || range == QLatin1String("5y") || range == QLatin1String("10y") ||
        range == QLatin1String("max"))
        return dt.toString(QStringLiteral("MMM yy"));
    return dt.toString(QStringLiteral("d MMM"));
}

QString rdb_key(qint64 ts, const QString& interval) {
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(ts);
    return rdb_intraday(interval) ? QString::number(ts) : dt.toString(QStringLiteral("yyyy-MM-dd"));
}

// The chart's calendar x axis (config "dates"): a day, or the minute for intraday bars.
QString rdb_iso(qint64 ts, const QString& interval) {
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(ts);
    return rdb_intraday(interval) ? dt.toString(QStringLiteral("yyyy-MM-ddTHH:mm")) : dt.toString(QStringLiteral("yyyy-MM-dd"));
}

QVector<int> rdb_sample_indices(int n, int max_points) {
    QVector<int> idx;
    if (n <= 0)
        return idx;
    const int step = std::max(1, static_cast<int>(std::ceil(static_cast<double>(n) / max_points)));
    for (int i = 0; i < n; i += step)
        idx.append(i);
    if (idx.last() != n - 1)
        idx.append(n - 1);
    return idx;
}

BindFetchResult rdb_shape_history(const QVector<RdbSeries>& all, const QStringList& failed, const rep::Binding& b,
                                  const QString& block_type) {
    BindFetchResult r;
    r.source_label = QStringLiteral("Yahoo Finance");
    QVector<RdbSeries> series;
    for (const RdbSeries& s : all)
        if (!s.points.isEmpty())
            series.append(s);
    if (series.isEmpty())
        return rdb_fail(failed.isEmpty() ? rdb_tr("no price history returned")
                                         : rdb_tr("no price history for %1").arg(failed.join(QStringLiteral(", "))));
    r.ok = true;
    const QString interval = b.param("interval", QStringLiteral("1d"));
    const QString range = b.param("range", QStringLiteral("6mo"));
    const QString mode = b.param("mode", QStringLiteral("price"));
    const QString chart = b.param("chart");
    qint64 last_ts = 0;
    for (const RdbSeries& s : series)
        last_ts = std::max(last_ts, s.points.last().timestamp);
    r.as_of = QDateTime::fromSecsSinceEpoch(last_ts);
    QStringList names;
    for (const RdbSeries& s : series)
        names << s.symbol;
    r.suggested_title = names.join(QStringLiteral(", ")) + QStringLiteral(" (%1)").arg(range);
    if (!failed.isEmpty())
        r.patch.insert(QStringLiteral("caption"), rdb_tr("No price history for: %1").arg(failed.join(QStringLiteral(", "))));
    else
        r.patch.insert(QStringLiteral("caption"), QString());

    const RdbSeries& first = series.first();
    if (block_type == QLatin1String("sparkline")) {
        const QVector<int> idx = rdb_sample_indices(first.points.size(), 60);
        QStringList vals;
        for (int i : idx)
            vals << QString::number(first.points[i].close, 'g', 10);
        const double a = first.points.first().close;
        const double z = first.points.last().close;
        r.patch.insert(QStringLiteral("data"), vals.join(QLatin1Char(',')));
        r.patch.insert(QStringLiteral("current"), rdb_price(z));
        r.patch.insert(QStringLiteral("change_pct"), a != 0 ? rdb_signed((z / a - 1.0) * 100.0) : QString());
        r.suggested_title = first.symbol;
        return r;
    }
    if (block_type == QLatin1String("table")) {
        QVector<QStringList> grid;
        QStringList head{rdb_tr("Date")};
        head << names;
        grid.append(head);
        const QVector<int> idx = rdb_sample_indices(first.points.size(), 60);
        for (int i : idx) {
            QStringList row{QDateTime::fromSecsSinceEpoch(first.points[i].timestamp).toString(QStringLiteral("yyyy-MM-dd"))};
            row << rdb_price(first.points[i].close);
            grid.append(row);
        }
        r.patch.insert(QStringLiteral("csv"), rep::table::encode(grid));
        return r;
    }

    // Chart.
    if (series.size() == 1 && chart == QLatin1String("candle")) {
        const int n = first.points.size();
        const int from = std::max(0, n - kRdbMaxCandles);
        QJsonArray ohlc;
        QStringList labels, closes, dates, volume;
        bool any_volume = false;
        for (int i = from; i < n; ++i) {
            const HistoryPoint& p = first.points[i];
            ohlc.append(QJsonArray{p.open, p.high, p.low, p.close});
            labels << rdb_label(p.timestamp, interval, range);
            closes << QString::number(p.close, 'g', 10);
            dates << rdb_iso(p.timestamp, interval);
            volume << QString::number(p.volume);
            any_volume = any_volume || p.volume > 0;
        }
        r.patch.insert(QStringLiteral("chart_type"), QStringLiteral("candle"));
        r.patch.insert(QStringLiteral("ohlc"), rdb_json(ohlc));
        r.patch.insert(QStringLiteral("labels"), rep::table::encode_list(labels));
        r.patch.insert(QStringLiteral("dates"), dates.join(QLatin1Char(',')));
        r.patch.insert(QStringLiteral("volume"), any_volume ? volume.join(QLatin1Char(',')) : QString());
        r.patch.insert(QStringLiteral("data"), closes.join(QLatin1Char(',')));
        r.patch.insert(QStringLiteral("series"), QString());
        return r;
    }
    if (!chart.isEmpty() && chart != QLatin1String("candle"))
        r.patch.insert(QStringLiteral("chart_type"), chart);
    r.patch.insert(QStringLiteral("ohlc"), QString());
    r.patch.insert(QStringLiteral("volume"), QString());

    if (series.size() == 1 && mode == QLatin1String("price")) {
        const QVector<int> idx = rdb_sample_indices(first.points.size(), kRdbMaxChartPoints);
        QStringList vals, labels, dates;
        for (int i : idx) {
            vals << QString::number(first.points[i].close, 'g', 10);
            labels << rdb_label(first.points[i].timestamp, interval, range);
            dates << rdb_iso(first.points[i].timestamp, interval);
        }
        r.patch.insert(QStringLiteral("data"), vals.join(QLatin1Char(',')));
        r.patch.insert(QStringLiteral("labels"), rep::table::encode_list(labels));
        r.patch.insert(QStringLiteral("dates"), dates.join(QLatin1Char(',')));
        r.patch.insert(QStringLiteral("series"), QString());
        if (chart.isEmpty())
            r.patch.insert(QStringLiteral("chart_type"), QStringLiteral("line"));
        return r;
    }

    // Several symbols (or rebased / % change): align on the union of dates, carry each
    // series forward over the other markets' holidays.
    QMap<QString, qint64> key_ts; // sorted keys
    QVector<QHash<QString, double>> by_key(series.size());
    for (int s = 0; s < series.size(); ++s) {
        for (const HistoryPoint& p : series[s].points) {
            const QString k = rdb_key(p.timestamp, interval);
            key_ts.insert(k, p.timestamp);
            by_key[s].insert(k, p.close);
        }
    }
    const QStringList keys = key_ts.keys();
    const QVector<int> idx = rdb_sample_indices(keys.size(), kRdbMaxChartPoints);
    QStringList labels, dates;
    for (int i : idx) {
        labels << rdb_label(key_ts.value(keys[i]), interval, range);
        dates << rdb_iso(key_ts.value(keys[i]), interval);
    }
    r.patch.insert(QStringLiteral("dates"), dates.join(QLatin1Char(',')));
    QJsonArray arr;
    QStringList first_vals;
    for (int s = 0; s < series.size(); ++s) {
        QVector<double> full(keys.size(), std::numeric_limits<double>::quiet_NaN());
        double last = std::numeric_limits<double>::quiet_NaN();
        for (int k = 0; k < keys.size(); ++k) {
            auto it = by_key[s].constFind(keys[k]);
            if (it != by_key[s].constEnd())
                last = it.value();
            full[k] = last;
        }
        double base = std::numeric_limits<double>::quiet_NaN();
        for (double v : full)
            if (rdb_has(v) && v != 0) {
                base = v;
                break;
            }
        QJsonArray vals;
        for (int i : idx) {
            double v = full[i];
            if (rdb_has(v) && rdb_has(base)) {
                if (mode == QLatin1String("rebased"))
                    v = v / base * 100.0;
                else if (mode == QLatin1String("pct"))
                    v = (v / base - 1.0) * 100.0;
            }
            vals.append(rdb_has(v) ? QJsonValue(std::round(v * 10000.0) / 10000.0) : QJsonValue());
            if (s == 0)
                first_vals << (rdb_has(v) ? QString::number(v, 'g', 10) : QStringLiteral("0"));
        }
        arr.append(QJsonObject{{QStringLiteral("name"), series[s].symbol}, {QStringLiteral("values"), vals}});
    }
    r.patch.insert(QStringLiteral("labels"), rep::table::encode_list(labels));
    r.patch.insert(QStringLiteral("data"), first_vals.join(QLatin1Char(',')));
    r.patch.insert(QStringLiteral("series"), series.size() > 1 ? rdb_json(arr) : QString());
    if (chart.isEmpty())
        r.patch.insert(QStringLiteral("chart_type"), QStringLiteral("line"));
    if (mode == QLatin1String("rebased"))
        r.suggested_title = names.join(QStringLiteral(", ")) + rdb_tr(" — rebased to 100 (%1)").arg(range);
    else if (mode == QLatin1String("pct"))
        r.suggested_title = names.join(QStringLiteral(", ")) + rdb_tr(" — % change (%1)").arg(range);
    return r;
}

// ── Financial statements ───────────────────────────────────────────────────

BindFetchResult rdb_shape_financials(const equity::FinancialsData& d, const rep::Binding& b, const QString& block_type) {
    const QString st = b.param("statement", QStringLiteral("income"));
    const bool quarterly = b.param("period", QStringLiteral("annual")) == QLatin1String("quarterly");
    const int n = std::clamp(b.int_param("n", 4), 1, 12);
    const QVector<QPair<QString, QJsonObject>>* cols = nullptr;
    const QStringList* order = nullptr;
    if (st == QLatin1String("balance")) {
        cols = quarterly ? &d.q_balance : &d.balance_sheet;
        order = quarterly ? &d.q_balance_order : &d.balance_order;
    } else if (st == QLatin1String("cashflow")) {
        cols = quarterly ? &d.q_cash_flow : &d.cash_flow;
        order = quarterly ? &d.q_cash_flow_order : &d.cash_flow_order;
    } else {
        cols = quarterly ? &d.q_income : &d.income_statement;
        order = quarterly ? &d.q_income_order : &d.income_order;
    }
    if (cols->isEmpty())
        return rdb_fail(quarterly ? rdb_tr("no quarterly statements for %1 (it may report half-yearly)").arg(d.symbol)
                                  : rdb_tr("no statement data for %1").arg(d.symbol));
    // Newest first in the source; shown oldest -> newest, left to right.
    QVector<QPair<QString, QJsonObject>> periods;
    for (int i = std::min(n, static_cast<int>(cols->size())) - 1; i >= 0; --i)
        periods.append(cols->at(i));
    QStringList rows = *order;
    if (rows.isEmpty()) {
        QSet<QString> seen;
        for (const auto& p : periods)
            for (const QString& k : p.second.keys())
                if (!seen.contains(k)) {
                    seen.insert(k);
                    rows << k;
                }
    }
    auto per_share = [](const QString& item) {
        return item.contains(QLatin1String("EPS"), Qt::CaseInsensitive) ||
               item.contains(QLatin1String("Per Share"), Qt::CaseInsensitive) ||
               item.contains(QLatin1String("Rate"), Qt::CaseInsensitive);
    };
    double max_abs = 0;
    for (const QString& item : rows) {
        if (per_share(item))
            continue;
        for (const auto& p : periods) {
            const QJsonValue v = p.second.value(item);
            if (v.isDouble())
                max_abs = std::max(max_abs, std::abs(v.toDouble()));
        }
    }
    double scale = 1;
    QString unit = rdb_tr("units");
    if (max_abs >= 1e7) {
        scale = 1e6;
        unit = rdb_tr("millions");
    } else if (max_abs >= 1e4) {
        scale = 1e3;
        unit = rdb_tr("thousands");
    }
    QVector<QStringList> grid;
    QStringList head{rdb_tr("Line item")};
    for (const auto& p : periods) {
        const QDate date = QDate::fromString(p.first.left(10), Qt::ISODate);
        if (!date.isValid())
            head << p.first;
        else
            head << (quarterly ? date.toString(QStringLiteral("MMM yyyy")) : QStringLiteral("FY%1").arg(date.year()));
    }
    grid.append(head);
    int kept = 0, total = 0;
    for (const QString& item : rows) {
        QStringList row{item};
        bool any = false;
        for (const auto& p : periods) {
            const QJsonValue v = p.second.value(item);
            if (!v.isDouble()) {
                row << rdb_dash();
                continue;
            }
            any = true;
            const double x = v.toDouble();
            if (per_share(item)) {
                row << QString::number(x, 'f', 2);
            } else {
                const QString t = QLocale().toString(std::abs(x) / scale, 'f', scale > 1 ? 0 : 2);
                row << (x < 0 ? QLatin1Char('(') + t + QLatin1Char(')') : t);
            }
        }
        if (!any)
            continue;
        ++total;
        if (kept < kRdbMaxStatementRows) {
            grid.append(row);
            ++kept;
        }
    }
    if (kept == 0)
        return rdb_fail(rdb_tr("no statement data for %1").arg(d.symbol));
    BindFetchResult r;
    r.ok = true;
    r.source_label = QStringLiteral("Yahoo Finance");
    r.as_of = d.fetched_at > 0 ? QDateTime::fromSecsSinceEpoch(d.fetched_at) : QDateTime::currentDateTime();

    if (block_type == QLatin1String("chart")) {
        // Grouped bars of a few line items per period (revenue and net income by quarter …).
        QStringList items;
        for (const QJsonValue& v : b.params.value(QStringLiteral("items")).toArray())
            if (!v.toString().trimmed().isEmpty())
                items << v.toString().trimmed();
        if (items.isEmpty()) {
            items = st == QLatin1String("balance")    ? QStringList{QStringLiteral("Total Assets"), QStringLiteral("Total Debt")}
                    : st == QLatin1String("cashflow") ? QStringList{QStringLiteral("Operating Cash Flow"), QStringLiteral("Free Cash Flow")}
                                                      : QStringList{QStringLiteral("Total Revenue"), QStringLiteral("Net Income")};
        }
        auto find_item = [&rows](const QString& want) -> QString {
            for (const QString& row : rows)
                if (row.compare(want, Qt::CaseInsensitive) == 0)
                    return row;
            for (const QString& row : rows)
                if (row.contains(want, Qt::CaseInsensitive))
                    return row;
            return {};
        };
        QJsonArray arr;
        QStringList names;
        for (const QString& want : items) {
            const QString item = find_item(want);
            if (item.isEmpty())
                continue;
            const double div = per_share(item) ? 1.0 : scale;
            QJsonArray vals;
            bool any = false;
            for (const auto& p : periods) {
                const QJsonValue v = p.second.value(item);
                if (v.isDouble()) {
                    vals.append(std::round(v.toDouble() / div * 100.0) / 100.0);
                    any = true;
                } else {
                    vals.append(QJsonValue());
                }
            }
            if (!any)
                continue;
            QJsonObject s{{QStringLiteral("name"), item}, {QStringLiteral("values"), vals}};
            if (per_share(item) && items.size() > 1)
                s.insert(QStringLiteral("axis"), QStringLiteral("right")); // EPS beside revenue
            arr.append(s);
            names << item;
        }
        if (arr.isEmpty())
            return rdb_fail(rdb_tr("none of %1 is reported for %2").arg(items.join(QStringLiteral(", ")), d.symbol));
        r.patch.insert(QStringLiteral("chart_type"), b.param("chart", QStringLiteral("bar")));
        r.patch.insert(QStringLiteral("labels"), rep::table::encode_list(head.mid(1)));
        r.patch.insert(QStringLiteral("series"), rdb_json(arr));
        QStringList first_vals;
        for (const QJsonValue& v : arr.first().toObject().value(QStringLiteral("values")).toArray())
            first_vals << (v.isDouble() ? QString::number(v.toDouble(), 'g', 10) : QStringLiteral("0"));
        r.patch.insert(QStringLiteral("data"), first_vals.join(QLatin1Char(',')));
        r.patch.insert(QStringLiteral("ohlc"), QString());
        r.patch.insert(QStringLiteral("dates"), QString());
        r.patch.insert(QStringLiteral("caption"), rdb_tr("Values in %1 of the reporting currency; per-share figures unscaled.").arg(unit));
        r.suggested_title = QStringLiteral("%1 — %2 (%3)").arg(d.symbol, names.join(QStringLiteral(", ")),
                                                             quarterly ? rdb_tr("quarterly") : rdb_tr("annual"));
        return r;
    }

    r.patch.insert(QStringLiteral("csv"), rep::table::encode(grid));
    r.patch.insert(QStringLiteral("rows"), QString::number(grid.size()));
    r.patch.insert(QStringLiteral("cols"), QString::number(head.size()));
    QString note = rdb_tr("Values in %1 of the reporting currency; negatives in brackets; per-share figures unscaled.").arg(unit);
    if (total > kept)
        note += QLatin1Char(' ') + rdb_tr("Showing the first %1 of %2 line items.").arg(kept).arg(total);
    if (d.stale)
        note += QLatin1Char(' ') + rdb_tr("Cached copy — the latest refresh failed.");
    r.patch.insert(QStringLiteral("note"), note);
    const QString name = st == QLatin1String("balance")    ? rdb_tr("balance sheet")
                         : st == QLatin1String("cashflow") ? rdb_tr("cash flow")
                                                           : rdb_tr("income statement");
    r.suggested_title = QStringLiteral("%1 %2 (%3)").arg(d.symbol, name, quarterly ? rdb_tr("quarterly") : rdb_tr("annual"));
    return r;
}

// ── Peers ───────────────────────────────────────────────────────────────────

BindFetchResult rdb_shape_peers(const QVector<equity::PeerData>& peers, const QString& sym) {
    QVector<equity::PeerData> rows = peers;
    std::stable_sort(rows.begin(), rows.end(), [&sym](const equity::PeerData& a, const equity::PeerData& b) {
        const bool as = a.symbol.compare(sym, Qt::CaseInsensitive) == 0;
        const bool bs = b.symbol.compare(sym, Qt::CaseInsensitive) == 0;
        if (as != bs)
            return as;
        return a.market_cap > b.market_cap;
    });
    auto ratio = [](double v) { return v != 0 && rdb_has(v) ? QString::number(v, 'f', 1) : rdb_dash(); };
    auto frac = [](double v) { return v != 0 && rdb_has(v) ? rdb_pct_of_fraction(v, 1) : rdb_dash(); };
    QVector<QStringList> grid{{rdb_tr("Symbol"), rdb_tr("Name"), rdb_tr("Price"), rdb_tr("Mkt cap"), rdb_tr("P/E"),
                               rdb_tr("Fwd P/E"), rdb_tr("P/B"), rdb_tr("Net margin"), rdb_tr("ROE"), rdb_tr("Rev growth")}};
    for (const auto& p : rows)
        grid.append({p.symbol, p.name, p.price > 0 ? rdb_price(p.price) : rdb_dash(),
                     p.market_cap > 0 ? rdb_compact(p.market_cap) : rdb_dash(), ratio(p.pe_ratio), ratio(p.forward_pe),
                     ratio(p.price_to_book), frac(p.profit_margin), frac(p.roe), frac(p.revenue_growth)});
    if (rows.isEmpty())
        return rdb_fail(rdb_tr("no peer data returned for %1").arg(sym));
    BindFetchResult r;
    r.ok = true;
    r.source_label = QStringLiteral("Yahoo Finance");
    r.as_of = QDateTime::currentDateTime();
    r.patch.insert(QStringLiteral("csv"), rep::table::encode(grid));
    r.patch.insert(QStringLiteral("rows"), QString::number(grid.size()));
    r.patch.insert(QStringLiteral("cols"), QStringLiteral("10"));
    r.patch.insert(QStringLiteral("note"), rdb_tr("Market caps in each listing's own currency."));
    r.suggested_title = sym + rdb_tr(" — peer comparison");
    return r;
}

// ── News / headlines ───────────────────────────────────────────────────────

BindFetchResult rdb_shape_lines(const QVector<std::tuple<qint64, QString, QString>>& items, const QString& block_type,
                                const QString& source_label, const QString& title) {
    if (items.isEmpty())
        return rdb_fail(rdb_tr("no articles returned"));
    BindFetchResult r;
    r.ok = true;
    r.source_label = source_label;
    r.as_of = QDateTime::currentDateTime();
    r.suggested_title = title;
    if (block_type == QLatin1String("table")) {
        QVector<QStringList> grid{{rdb_tr("Date"), rdb_tr("Source"), rdb_tr("Headline")}};
        for (const auto& [ts, source, headline] : items)
            grid.append({ts > 0 ? QDateTime::fromSecsSinceEpoch(ts).toString(QStringLiteral("d MMM HH:mm")) : rdb_dash(),
                         source, headline});
        r.patch.insert(QStringLiteral("csv"), rep::table::encode(grid));
        r.patch.insert(QStringLiteral("rows"), QString::number(grid.size()));
        r.patch.insert(QStringLiteral("cols"), QStringLiteral("3"));
    } else {
        QStringList lines;
        for (const auto& [ts, source, headline] : items) {
            QString line = headline;
            line.replace(QLatin1Char('\n'), QLatin1Char(' '));
            QString meta = source;
            if (ts > 0)
                meta += (meta.isEmpty() ? QString() : QStringLiteral(", ")) +
                        QDateTime::fromSecsSinceEpoch(ts).toString(QStringLiteral("d MMM HH:mm"));
            if (!meta.isEmpty())
                line += QStringLiteral(" — ") + meta;
            lines << line;
        }
        r.has_content = true;
        r.content = lines.join(QLatin1Char('\n'));
    }
    return r;
}

// ── Portfolio ───────────────────────────────────────────────────────────────

BindFetchResult rdb_shape_portfolio(const portfolio::PortfolioSummary& s, const QString& what, const QString& block_type) {
    BindFetchResult r;
    r.ok = true;
    r.source_label = QStringLiteral("Portfolio: ") + s.portfolio.name;
    r.as_of = QDateTime::currentDateTime();
    const QString base = s.base_currency.isEmpty() ? s.portfolio.currency : s.base_currency;
    QVector<portfolio::HoldingWithQuote> h = s.holdings;
    std::sort(h.begin(), h.end(), [](const auto& a, const auto& b) {
        const double av = rdb_has(a.market_value) ? a.market_value : -1;
        const double bv = rdb_has(b.market_value) ? b.market_value : -1;
        return av > bv;
    });
    QString incomplete =
        s.complete ? QString()
                   : rdb_tr("Totals cover %1 of %2 holdings; not priced: %3.")
                         .arg(s.valued_positions)
                         .arg(s.total_positions)
                         .arg((s.unpriced + s.fx_missing).join(QStringLiteral(", ")));
    // Holdings converted at a rate older than the user's "FX stale after": the totals
    // are approximate — say which currencies and how old the oldest rate is.
    if (!s.fx_stale.isEmpty()) {
        const QString when = s.fx_stale_as_of.isValid()
                                 ? s.fx_stale_as_of.toLocalTime().toString(QStringLiteral("d MMM HH:mm"))
                                 : rdb_dash();
        const QString stale = rdb_tr("Converted to %1 at stale exchange rates for %2 (oldest rate %3) — totals are "
                                     "approximate.")
                                  .arg(base, s.fx_stale.join(QStringLiteral(", ")), when);
        incomplete = incomplete.isEmpty() ? stale : incomplete + QLatin1Char(' ') + stale;
    }
    if (what == QLatin1String("allocation") || block_type == QLatin1String("chart")) {
        QStringList labels, vals;
        double other = 0;
        int n = 0;
        for (const auto& x : h) {
            if (!rdb_has(x.market_value) || x.market_value <= 0)
                continue;
            if (n < 8) {
                labels << x.symbol;
                vals << QString::number(x.market_value, 'f', 2);
                ++n;
            } else {
                other += x.market_value;
            }
        }
        if (other > 0) {
            labels << rdb_tr("Other");
            vals << QString::number(other, 'f', 2);
        }
        if (labels.isEmpty())
            return rdb_fail(rdb_tr("the portfolio has no priced holdings yet"));
        r.patch.insert(QStringLiteral("chart_type"), QStringLiteral("donut"));
        r.patch.insert(QStringLiteral("labels"), rep::table::encode_list(labels));
        r.patch.insert(QStringLiteral("data"), vals.join(QLatin1Char(',')));
        r.patch.insert(QStringLiteral("series"), QString());
        r.patch.insert(QStringLiteral("ohlc"), QString());
        r.patch.insert(QStringLiteral("caption"), incomplete);
        r.suggested_title = rdb_tr("%1 — allocation by market value (%2)").arg(s.portfolio.name, base);
        return r;
    }
    if (what == QLatin1String("summary") || block_type == QLatin1String("stats_block")) {
        QStringList lines, tones;
        auto tone = [](double v) {
            return !rdb_has(v) || v == 0 ? QString() : v > 0 ? QStringLiteral("positive") : QStringLiteral("negative");
        };
        lines << rdb_stat_line(rdb_tr("Market value"), rdb_grouped(s.total_market_value, 2) + QLatin1Char(' ') + base);
        tones << QString();
        lines << rdb_stat_line(rdb_tr("Cost basis"), rdb_grouped(s.total_cost_basis, 2) + QLatin1Char(' ') + base);
        tones << QString();
        lines << rdb_stat_line(rdb_tr("Unrealized P&L"), rdb_signed(s.total_unrealized_pnl) + QStringLiteral(" (") +
                                                             rdb_signed_pct(s.total_unrealized_pnl_percent) + QLatin1Char(')'));
        tones << tone(s.total_unrealized_pnl);
        lines << rdb_stat_line(rdb_tr("Day change"), rdb_signed(s.total_day_change) + QStringLiteral(" (") +
                                                         rdb_signed_pct(s.total_day_change_percent) + QLatin1Char(')'));
        tones << tone(s.total_day_change);
        lines << rdb_stat_line(rdb_tr("Realized P&L"), rdb_signed(s.total_realized_pnl));
        tones << tone(s.total_realized_pnl);
        lines << rdb_stat_line(rdb_tr("Dividends"), rdb_grouped(s.total_dividends, 2));
        tones << QString();
        lines << rdb_stat_line(rdb_tr("Positions"), rdb_tr("%1 (%2 up, %3 down)").arg(s.total_positions).arg(s.gainers).arg(s.losers));
        tones << QString();
        if (!s.as_of.isEmpty()) {
            lines << rdb_stat_line(rdb_tr("Prices as of"), s.as_of);
            tones << QString();
        }
        if (!incomplete.isEmpty()) { // partial totals / stale FX: the figures above say less than they seem
            lines << rdb_stat_line(rdb_tr("Note"), incomplete);
            tones << QString();
        }
        r.patch.insert(QStringLiteral("data"), lines.join(QLatin1Char('\n')));
        r.patch.insert(QStringLiteral("tones"), tones.join(QLatin1Char(',')));
        r.suggested_title = s.portfolio.name + rdb_tr(" — performance");
        return r;
    }
    QVector<QStringList> grid{{rdb_tr("Symbol"), rdb_tr("Name"), rdb_tr("Quantity"), rdb_tr("Price"),
                               rdb_tr("Value (%1)").arg(base), rdb_tr("Weight"), rdb_tr("Unrealized P&L"), rdb_tr("P&L %")}};
    for (const auto& x : h)
        grid.append({x.symbol, x.name, rdb_grouped(x.quantity, x.quantity == std::floor(x.quantity) ? 0 : 4),
                     rdb_price(x.current_price), rdb_grouped(x.market_value, 2),
                     rdb_has(x.weight) ? QString::number(x.weight, 'f', 1) + QLatin1Char('%') : rdb_dash(),
                     rdb_signed(x.unrealized_pnl), rdb_signed_pct(x.unrealized_pnl_percent)});
    if (h.isEmpty())
        return rdb_fail(rdb_tr("the portfolio has no holdings yet"));
    r.patch.insert(QStringLiteral("csv"), rep::table::encode(grid));
    r.patch.insert(QStringLiteral("rows"), QString::number(grid.size()));
    r.patch.insert(QStringLiteral("cols"), QStringLiteral("8"));
    r.patch.insert(QStringLiteral("note"), incomplete);
    r.suggested_title = s.portfolio.name + rdb_tr(" — holdings");
    return r;
}

bool rdb_needs_symbol(const rep::Binding& b) {
    const QString& s = b.source;
    if (s == QLatin1String(rep::source::kQuotes))
        return b.symbols().isEmpty() && b.param("universe").isEmpty();
    if (s == QLatin1String(rep::source::kHistory))
        return b.symbols().isEmpty();
    return s == QLatin1String(rep::source::kQuote) || s == QLatin1String(rep::source::kInfo) ||
           s == QLatin1String(rep::source::kFinancials) || s == QLatin1String(rep::source::kPeers) ||
           s == QLatin1String(rep::source::kNews);
}

} // namespace

// ── Singleton / scheduling ──────────────────────────────────────────────────

ReportDataBinder& ReportDataBinder::instance() {
    static ReportDataBinder* s = [] {
        auto* b = new ReportDataBinder();
        if (auto* app = QCoreApplication::instance(); app && b->thread() != app->thread())
            b->moveToThread(app->thread());
        return b;
    }();
    return *s;
}

ReportDataBinder::ReportDataBinder() {
    // A block that disappears (deleted, report switched) takes its state with it.
    auto& svc = ReportBuilderService::instance();
    connect(&svc, &ReportBuilderService::document_loaded, this, [this](const QString&) { cancel_all(); });
    connect(&svc, &ReportBuilderService::component_removed, this, [this](int id, int) {
        if (states_.remove(id) > 0)
            emit block_state_changed(id);
        latest_for_component_.remove(id);
    });
}

int ReportDataBinder::timeout_ms(const rep::Binding& b) const {
    const QString& s = b.source;
    if (s == QLatin1String(rep::source::kQuote) || s == QLatin1String(rep::source::kInfo))
        return 20'000;
    if (s == QLatin1String(rep::source::kQuotes))
        return 25'000;
    if (s == QLatin1String(rep::source::kHistory))
        return 30'000;
    if (s == QLatin1String(rep::source::kPortfolio))
        return 40'000;
    if (s == QLatin1String(rep::source::kEconCalendar))
        return 90'000; // one connector run per day of the week, through the Python queue
    return 60'000; // financials, peers, news, headlines: Python scripts / many feeds
}

rep::Binding ReportDataBinder::effective_binding(const rep::ReportComponent& comp, const rep::ReportMetadata& meta) {
    return rep::vars::expand_binding(rep::binding_of(comp), rep::vars::context(meta));
}

ReportDataBinder::BindPatch ReportDataBinder::make_patch(const rep::ReportComponent& comp,
                                                         const rep::Binding& fetched_with, const BindFetchResult& r) {
    BindPatch out;
    out.config = r.patch;
    out.config.insert(QLatin1String(rep::keys::kAsOf),
                      rep::format_as_of(r.as_of.isValid() ? r.as_of : QDateTime::currentDateTime()));
    if (!r.source_label.isEmpty())
        out.config.insert(QLatin1String(rep::keys::kSourceLabel), r.source_label);
    // Upgrade a block that only implied its recipe (older files) to an explicit one, and
    // drop the transient keys earlier builds stored in the document.
    if (comp.config.value(QLatin1String(rep::keys::kBind)).isEmpty())
        out.config.insert(QLatin1String(rep::keys::kBind), fetched_with.to_string());
    for (const char* k : {"status", "error", "fetch_ticker", "fetch_history", "sample"})
        out.config.insert(QLatin1String(k), QString());
    const QString title = comp.config.value(QStringLiteral("title")).trimmed();
    if (!r.suggested_title.isEmpty() &&
        (title.isEmpty() || title == QLatin1String("Chart") || title == QLatin1String("Key Statistics")))
        out.config.insert(QStringLiteral("title"), r.suggested_title);
    out.has_content = r.has_content;
    out.content = r.content;
    return out;
}

bool ReportDataBinder::refresh(int component_id) {
    auto& svc = ReportBuilderService::instance();
    rep::ReportComponent comp;
    if (!svc.component(component_id, &comp))
        return false;
    const rep::Binding b = effective_binding(comp, svc.metadata());
    if (!b.is_refreshable())
        return false;
    enqueue(b, comp.type, component_id, false);
    return true;
}

int ReportDataBinder::refresh_all(const QVector<int>& ids) {
    auto& svc = ReportBuilderService::instance();
    const auto comps = svc.components();
    const rep::ReportMetadata meta = svc.metadata();
    int started = 0;
    for (const auto& c : comps) {
        if (!ids.isEmpty() && !ids.contains(c.id))
            continue;
        const rep::Binding b = effective_binding(c, meta);
        if (!b.is_refreshable())
            continue;
        if (batch_members_.contains(c.id))
            continue; // already part of the running batch
        if (batch_pending_ == 0) {
            batch_total_ = 0;
            batch_ok_ = 0;
            batch_failed_ = 0;
        }
        batch_members_.insert(c.id, true);
        ++batch_total_;
        ++batch_pending_;
        enqueue(b, c.type, c.id, true);
        ++started;
    }
    if (started > 0)
        emit progress(batch_total_ - batch_pending_, batch_total_);
    return started;
}

quint64 ReportDataBinder::fetch(const rep::Binding& binding, const QString& block_type, QObject* ctx, FetchDone done) {
    if (!binding.is_refreshable())
        return 0;
    auto job = std::make_shared<Job>();
    job->id = next_id_++;
    job->binding = binding;
    job->block_type = block_type;
    job->key = QStringLiteral("oneshot:%1").arg(job->id); // never shared
    job->ctx = ctx;
    job->has_ctx = ctx != nullptr;
    job->done = std::move(done);
    jobs_.insert(job->id, job);
    queue_.append(job->id);
    pump();
    return job->id;
}

quint64 ReportDataBinder::enqueue(const rep::Binding& b, const QString& block_type, int component_id, bool in_batch) {
    if (rdb_needs_symbol(b)) {
        set_state(component_id, State::Failed, rdb_tr("No symbol set — pick one in the Properties panel."));
        if (in_batch)
            note_batch_done(component_id, false);
        return 0;
    }
    const QString key = b.key() + QLatin1Char('|') + block_type;
    for (const JobPtr& j : std::as_const(jobs_)) {
        if (j->key == key && !j->settled) {
            if (!j->component_ids.contains(component_id))
                j->component_ids.append(component_id);
            if (in_batch && !j->batch_ids.contains(component_id))
                j->batch_ids.append(component_id);
            latest_for_component_.insert(component_id, j->id);
            set_state(component_id, j->running ? State::Loading : State::Queued);
            return j->id;
        }
    }
    auto job = std::make_shared<Job>();
    job->id = next_id_++;
    job->binding = b;
    job->block_type = block_type;
    job->key = key;
    job->component_ids.append(component_id);
    if (in_batch)
        job->batch_ids.append(component_id);
    jobs_.insert(job->id, job);
    queue_.append(job->id);
    latest_for_component_.insert(component_id, job->id);
    set_state(component_id, State::Queued);
    pump();
    return job->id;
}

void ReportDataBinder::pump() {
    bool peers_running = false;
    for (const JobPtr& j : std::as_const(jobs_))
        if (j->running && !j->settled && j->binding.source == QLatin1String(rep::source::kPeers))
            peers_running = true;
    for (int i = 0; i < queue_.size() && running_ < kRdbMaxConcurrent;) {
        const JobPtr job = jobs_.value(queue_[i]);
        if (!job) {
            queue_.removeAt(i);
            continue;
        }
        // EquityResearchService::peers_loaded carries no symbol: one peer fetch at a time.
        if (job->binding.source == QLatin1String(rep::source::kPeers)) {
            if (peers_running) {
                ++i;
                continue;
            }
            peers_running = true;
        }
        queue_.removeAt(i);
        start(job);
    }
}

void ReportDataBinder::start(const JobPtr& job) {
    job->running = true;
    ++running_;
    job->scope = new QObject(this);
    for (int cid : job->component_ids)
        set_state(cid, State::Loading);
    const quint64 id = job->id;
    const int limit_ms = timeout_ms(job->binding);
    QTimer::singleShot(limit_ms, job->scope, [this, id, limit_ms]() {
        settle(id, rdb_fail(rdb_tr("no answer from the data source within %1 s").arg(limit_ms / 1000)));
    });
    const QString& s = job->binding.source;
    if (s == QLatin1String(rep::source::kQuote))
        run_quote(job);
    else if (s == QLatin1String(rep::source::kQuotes))
        run_quotes(job);
    else if (s == QLatin1String(rep::source::kInfo))
        run_info(job);
    else if (s == QLatin1String(rep::source::kHistory))
        run_history(job);
    else if (s == QLatin1String(rep::source::kFinancials))
        run_financials(job);
    else if (s == QLatin1String(rep::source::kPeers))
        run_peers(job);
    else if (s == QLatin1String(rep::source::kNews))
        run_news(job);
    else if (s == QLatin1String(rep::source::kHeadlines))
        run_headlines(job);
    else if (s == QLatin1String(rep::source::kPortfolio))
        run_portfolio(job);
    else if (s == QLatin1String(rep::source::kWatchlist))
        run_watchlist(job);
    else if (s == QLatin1String(rep::source::kEconCalendar))
        run_econ_calendar(job);
    else
        settle(id, rdb_fail(rdb_tr("unknown data source \"%1\"").arg(s)));
}

void ReportDataBinder::settle(quint64 job_id, const BindFetchResult& r) {
    const JobPtr job = jobs_.value(job_id);
    if (!job || job->settled)
        return;
    job->settled = true;
    jobs_.remove(job_id);
    queue_.removeAll(job_id);
    if (job->scope) {
        datahub::DataHub::instance().unsubscribe(job->scope);
        job->scope->deleteLater(); // also drops its error subscriptions and connections
        job->scope = nullptr;
    }
    if (job->running)
        --running_;
    deliver(job, r);
    // Start the next fetch on a fresh stack (a source may settle synchronously).
    if (!pump_scheduled_) {
        pump_scheduled_ = true;
        QMetaObject::invokeMethod(
            this,
            [this]() {
                pump_scheduled_ = false;
                pump();
            },
            Qt::QueuedConnection);
    }
}

void ReportDataBinder::cancel(quint64 request_id) {
    const JobPtr job = jobs_.value(request_id);
    if (!job)
        return;
    job->done = nullptr; // a cancelled one-shot reports nothing
    BindFetchResult r = rdb_fail(rdb_tr("cancelled"));
    r.cancelled = true;
    settle(request_id, r);
}

void ReportDataBinder::cancel_all() {
    const auto ids = jobs_.keys();
    for (quint64 id : ids) {
        if (JobPtr j = jobs_.value(id)) {
            j->done = nullptr;
            for (int cid : j->component_ids)
                latest_for_component_.remove(cid);
        }
        BindFetchResult r = rdb_fail(rdb_tr("cancelled"));
        r.cancelled = true;
        settle(id, r);
    }
    const auto states = states_.keys();
    states_.clear();
    for (int cid : states)
        emit block_state_changed(cid);
    if (batch_pending_ > 0) {
        batch_pending_ = 0;
        batch_members_.clear();
        emit batch_finished(batch_ok_, batch_failed_);
    }
}

void ReportDataBinder::deliver(const JobPtr& job, const BindFetchResult& r) {
    if (job->done && (!job->has_ctx || job->ctx))
        job->done(r);
    for (int cid : job->component_ids) {
        const bool counted = job->batch_ids.contains(cid);
        // A newer refresh of this block took over: its own reply decides.
        if (latest_for_component_.value(cid) != job->id) {
            if (counted)
                note_batch_done(cid, r.ok);
            continue;
        }
        latest_for_component_.remove(cid);
        apply_to_component(job, cid, r);
        if (counted)
            note_batch_done(cid, r.ok);
    }
}

void ReportDataBinder::apply_to_component(const JobPtr& job, int cid, const BindFetchResult& r) {
    auto& svc = ReportBuilderService::instance();
    rep::ReportComponent comp;
    if (!svc.component(cid, &comp)) {
        set_state(cid, State::Idle);
        return;
    }
    const rep::Binding now = effective_binding(comp, svc.metadata());
    if (now.key() != job->binding.key() || comp.type != job->block_type) {
        set_state(cid, State::Idle); // the recipe changed while fetching: this reply is stale
        return;
    }
    if (!r.ok) {
        if (r.cancelled)
            set_state(cid, State::Idle);
        else
            set_state(cid, State::Failed, r.error);
        return;
    }
    const BindPatch p = make_patch(comp, job->binding, r);
    svc.patch_component(cid, p.has_content ? &p.content : nullptr, p.config,
                        tr("Refresh %1").arg(rep::describe(job->binding)), /*mergeable=*/false,
                        /*empty_removes=*/true);
    set_state(cid, State::Idle);
}

void ReportDataBinder::set_state(int cid, State s, const QString& message) {
    if (cid <= 0)
        return;
    if (s == State::Idle) {
        if (states_.remove(cid) > 0)
            emit block_state_changed(cid);
        return;
    }
    BlockState& st = states_[cid];
    if (st.state == s && st.message == message)
        return;
    st.state = s;
    st.message = message;
    st.since = QDateTime::currentDateTime();
    emit block_state_changed(cid);
}

void ReportDataBinder::note_batch_done(int cid, bool ok) {
    if (!batch_members_.remove(cid))
        return;
    ok ? ++batch_ok_ : ++batch_failed_;
    batch_pending_ = std::max(0, batch_pending_ - 1);
    emit progress(batch_total_ - batch_pending_, batch_total_);
    if (batch_pending_ == 0)
        emit batch_finished(batch_ok_, batch_failed_);
}

// ── Sources ─────────────────────────────────────────────────────────────────

void ReportDataBinder::run_quote(const JobPtr& job) {
    const QString sym = job->binding.symbol().trimmed().toUpper();
    const QString topic = MarketDataService::quote_topic(sym);
    const quint64 id = job->id;
    const QString type = job->block_type;
    auto& hub = datahub::DataHub::instance();
    hub.subscribe<QuoteData>(job->scope, topic, [this, id, type, sym](const QuoteData& q) {
        if (q.price > 0)
            settle(id, rdb_shape_quote(q, type, sym));
        else
            settle(id, rdb_fail(rdb_tr("no price in the quote for %1").arg(sym)));
    });
    hub.subscribe_errors(job->scope, topic, [this, id](const QString& err) { settle(id, rdb_fail(err)); });
    // A user who presses Refresh wants a quote taken now, not a cached one.
    hub.request(topic, /*force=*/true);
}

void ReportDataBinder::run_quotes(const JobPtr& job) {
    QStringList syms = job->binding.symbols();
    const QString universe = job->binding.param("universe");
    if (!universe.isEmpty())
        syms = rdb_universe(universe);
    if (syms.isEmpty()) {
        settle(job->id, rdb_fail(rdb_tr("no symbols to quote")));
        return;
    }
    run_quote_list(job->id, job->binding, job->block_type, syms, QString());
}

void ReportDataBinder::run_quote_list(quint64 id, const rep::Binding& b, const QString& type, const QStringList& syms,
                                      const QString& title) {
    MarketDataService::instance().fetch_quotes(syms, [this, id, b, type, syms, title](bool ok, QVector<QuoteData> q) {
        const QString why = MarketDataService::instance().last_fetch_error();
        QMetaObject::invokeMethod(
            this,
            [this, id, b, type, syms, title, ok, q, why]() {
                if (!ok || q.isEmpty()) {
                    settle(id, rdb_fail(why.isEmpty() ? rdb_tr("no quotes returned") : why));
                    return;
                }
                BindFetchResult r = rdb_shape_quotes(q, syms, b, type);
                if (r.ok && !title.isEmpty())
                    r.suggested_title = title;
                settle(id, r);
            },
            Qt::QueuedConnection);
    });
}

void ReportDataBinder::run_watchlist(const JobPtr& job) {
    // The list's symbols come from the database on the library's worker (never the GUI
    // thread), then go through the same batched quote fetch as a "Quotes list".
    const QString wanted = job->binding.param("watchlist_id");
    const quint64 id = job->id;
    const rep::Binding b = job->binding;
    const QString type = job->block_type;
    ReportLibrary::instance().run_on_db_worker(
        [wanted]() -> QVariant {
            auto& repo = WatchlistRepository::instance();
            auto lists = repo.list_all();
            if (lists.is_err() || lists.value().isEmpty())
                return QVariantMap{{QStringLiteral("error"), rdb_tr("you have no watchlist yet — create one in the Watchlist tab")}};
            Watchlist pick = lists.value().first();
            bool found = wanted.isEmpty();
            for (const Watchlist& w : lists.value()) {
                if ((!wanted.isEmpty() && w.id == wanted) || (wanted.isEmpty() && w.is_default)) {
                    pick = w;
                    found = true;
                    break;
                }
            }
            if (!found)
                return QVariantMap{{QStringLiteral("error"), rdb_tr("that watchlist no longer exists")}};
            auto stocks = repo.get_stocks(pick.id);
            QStringList syms;
            if (stocks.is_ok())
                for (const WatchlistStock& s : stocks.value())
                    if (!s.symbol.trimmed().isEmpty())
                        syms << s.symbol.trimmed().toUpper();
            return QVariantMap{{QStringLiteral("name"), pick.name}, {QStringLiteral("symbols"), syms}};
        },
        job->scope, [this, id, b, type](const QVariant& v) {
            const QVariantMap m = v.toMap();
            if (m.contains(QStringLiteral("error"))) {
                settle(id, rdb_fail(m.value(QStringLiteral("error")).toString()));
                return;
            }
            QStringList syms = m.value(QStringLiteral("symbols")).toStringList();
            if (syms.isEmpty()) {
                settle(id, rdb_fail(rdb_tr("the watchlist “%1” is empty").arg(m.value(QStringLiteral("name")).toString())));
                return;
            }
            // Quote at most 60 names in one go (the batch endpoint and a readable table).
            if (syms.size() > 60)
                syms = syms.mid(0, 60);
            run_quote_list(id, b, type, syms, rdb_tr("Watchlist — %1").arg(m.value(QStringLiteral("name")).toString()));
        });
}

void ReportDataBinder::run_info(const JobPtr& job) {
    const QString sym = job->binding.symbol().trimmed().toUpper();
    const QString topic = MarketDataService::info_topic(sym);
    const quint64 id = job->id;
    const QString type = job->block_type;
    auto& hub = datahub::DataHub::instance();
    hub.subscribe<InfoData>(job->scope, topic,
                            [this, id, type, sym](const InfoData& info) { settle(id, rdb_shape_info(info, type, sym)); });
    hub.subscribe_errors(job->scope, topic, [this, id](const QString& err) { settle(id, rdb_fail(err)); });
    hub.request(topic, /*force=*/true);
}

void ReportDataBinder::run_history(const JobPtr& job) {
    const QStringList syms = job->binding.symbols();
    const QString range = job->binding.param("range", QStringLiteral("6mo"));
    const QString interval = job->binding.param("interval", QStringLiteral("1d"));
    if (syms.isEmpty()) {
        settle(job->id, rdb_fail(rdb_tr("no symbol set for the chart")));
        return;
    }
    struct RdbHistState {
        QVector<RdbSeries> series;
        QStringList failed;
        int pending = 0;
    };
    auto st = std::make_shared<RdbHistState>();
    st->pending = static_cast<int>(syms.size());
    for (const QString& s : syms)
        st->series.append({s.trimmed().toUpper(), {}});
    const quint64 id = job->id;
    const rep::Binding b = job->binding;
    const QString type = job->block_type;
    auto& hub = datahub::DataHub::instance();
    auto finish_one = [this, id, b, type, st]() {
        if (--st->pending > 0)
            return;
        // Shaping a few years of several symbols is not free: do it on a worker.
        auto series = st->series;
        auto failed = st->failed;
        (void)QtConcurrent::run([this, id, b, type, series, failed]() {
            const BindFetchResult r = rdb_shape_history(series, failed, b, type);
            QMetaObject::invokeMethod(this, [this, id, r]() { settle(id, r); }, Qt::QueuedConnection);
        });
    };
    for (int i = 0; i < syms.size(); ++i) {
        const QString sym = st->series[i].symbol;
        const QString topic = MarketDataService::history_topic(sym, range, interval);
        auto done_flag = std::make_shared<bool>(false);
        hub.subscribe<QVector<HistoryPoint>>(job->scope, topic,
                                             [st, i, done_flag, finish_one, sym](const QVector<HistoryPoint>& pts) {
                                                 if (*done_flag)
                                                     return;
                                                 *done_flag = true;
                                                 st->series[i].points = pts;
                                                 if (pts.isEmpty())
                                                     st->failed << sym;
                                                 finish_one();
                                             });
        hub.subscribe_errors(job->scope, topic, [st, done_flag, finish_one, sym](const QString&) {
            if (*done_flag)
                return;
            *done_flag = true;
            st->failed << sym;
            finish_one();
        });
        const QVariant cached = hub.peek(topic);
        if (!cached.isValid())
            hub.request(topic, /*force=*/true);
        else
            hub.request(topic, /*force=*/false); // fresh enough: the subscription delivers it
    }
}

void ReportDataBinder::run_financials(const JobPtr& job) {
    const QString sym = job->binding.symbol().trimmed().toUpper();
    const quint64 id = job->id;
    const rep::Binding b = job->binding;
    const QString type = job->block_type;
    auto& eq = equity::EquityResearchService::instance();
    auto fallback = std::make_shared<std::optional<equity::FinancialsData>>();
    auto shape = [this, id, b, type](const equity::FinancialsData& d) {
        (void)QtConcurrent::run([this, id, b, type, d]() {
            const BindFetchResult r = rdb_shape_financials(d, b, type);
            QMetaObject::invokeMethod(this, [this, id, r]() { settle(id, r); }, Qt::QueuedConnection);
        });
    };
    connect(&eq, &equity::EquityResearchService::financials_loaded, job->scope,
            [sym, fallback, shape](const equity::FinancialsData& d) {
                if (d.symbol.compare(sym, Qt::CaseInsensitive) != 0)
                    return;
                if (d.stale) { // last-known-good copy while the refresh runs: keep it as a fallback
                    *fallback = d;
                    return;
                }
                shape(d);
            });
    connect(&eq, &equity::EquityResearchService::load_failed, job->scope,
            [this, id, sym, fallback, shape](const equity::LoadFailure& f) {
                if (f.symbol.compare(sym, Qt::CaseInsensitive) != 0 || f.leg != QLatin1String("Financials"))
                    return;
                if (fallback->has_value())
                    shape(fallback->value());
                else
                    settle(id, rdb_fail(f.message));
            });
    eq.fetch_financials(sym);
}

void ReportDataBinder::run_peers(const JobPtr& job) {
    const QString sym = job->binding.symbol().trimmed().toUpper();
    const int count = std::clamp(job->binding.int_param("count", 5), 1, 12);
    const quint64 id = job->id;
    auto& eq = equity::EquityResearchService::instance();
    auto asked = std::make_shared<QSet<QString>>();
    connect(&eq, &equity::EquityResearchService::suggested_peers_loaded, job->scope,
            [this, id, sym, asked](const QString& s, const QStringList& peers) {
                if (s.compare(sym, Qt::CaseInsensitive) != 0 || !asked->isEmpty())
                    return;
                if (peers.isEmpty()) {
                    settle(id, rdb_fail(rdb_tr("no peers found for %1").arg(sym)));
                    return;
                }
                asked->insert(sym);
                for (const QString& p : peers)
                    asked->insert(p.toUpper());
                equity::EquityResearchService::instance().fetch_peers(sym, peers);
            });
    connect(&eq, &equity::EquityResearchService::peers_loaded, job->scope,
            [this, id, sym, asked](const QVector<equity::PeerData>& peers) {
                if (asked->isEmpty())
                    return;
                for (const auto& p : peers) // only the answer to our own request
                    if (!asked->contains(p.symbol.toUpper()))
                        return;
                settle(id, rdb_shape_peers(peers, sym));
            });
    connect(&eq, &equity::EquityResearchService::load_failed, job->scope,
            [this, id, sym](const equity::LoadFailure& f) {
                if (f.symbol.compare(sym, Qt::CaseInsensitive) == 0 && f.leg == QLatin1String("Peers"))
                    settle(id, rdb_fail(f.message));
            });
    eq.fetch_suggested_peers(sym, count);
}

void ReportDataBinder::run_news(const JobPtr& job) {
    const QString sym = job->binding.symbol().trimmed().toUpper();
    const int count = std::clamp(job->binding.int_param("count", 8), 1, 30);
    const quint64 id = job->id;
    const QString type = job->block_type;
    auto& eq = equity::EquityResearchService::instance();
    connect(&eq, &equity::EquityResearchService::news_loaded, job->scope,
            [this, id, sym, count, type](const QString& s, equity::NewsProvider provider, bool,
                                         const QVector<equity::NewsArticle>& articles) {
                if (s.compare(sym, Qt::CaseInsensitive) != 0 || provider != equity::NewsProvider::Auto)
                    return;
                QVector<std::tuple<qint64, QString, QString>> items;
                for (const auto& a : articles) {
                    if (items.size() >= count)
                        break;
                    if (!a.title.trimmed().isEmpty())
                        items.append({equity::news_timestamp(a.published_date), a.publisher, a.title.trimmed()});
                }
                BindFetchResult r = rdb_shape_lines(items, type, QStringLiteral("Yahoo Finance / Google News"),
                                                    sym + rdb_tr(" — news"));
                if (!r.ok)
                    r.error = rdb_tr("no news returned for %1").arg(sym);
                settle(id, r);
            });
    connect(&eq, &equity::EquityResearchService::load_failed, job->scope,
            [this, id, sym](const equity::LoadFailure& f) {
                if (f.symbol.compare(sym, Qt::CaseInsensitive) == 0 && f.leg == QLatin1String("News"))
                    settle(id, rdb_fail(f.message));
            });
    eq.fetch_news(sym, count, equity::NewsProvider::Auto, /*force=*/true);
}

void ReportDataBinder::run_headlines(const JobPtr& job) {
    const int count = std::clamp(job->binding.int_param("count", 10), 1, 40);
    const quint64 id = job->id;
    const QString type = job->block_type;
    NewsService::instance().fetch_all_news(false, [this, id, count, type](bool ok, QVector<NewsArticle> articles) {
        QMetaObject::invokeMethod(
            this,
            [this, id, count, type, ok, articles]() {
                if (!ok && articles.isEmpty()) {
                    settle(id, rdb_fail(rdb_tr("the news feeds could not be read")));
                    return;
                }
                QVector<std::tuple<qint64, QString, QString>> items;
                for (const auto& a : NewsService::brief_articles(articles, count))
                    items.append({a.sort_ts, a.source, a.headline.trimmed()});
                settle(id, rdb_shape_lines(items, type, rdb_tr("News feeds"), rdb_tr("Headlines")));
            },
            Qt::QueuedConnection);
    });
}

void ReportDataBinder::run_portfolio(const JobPtr& job) {
    const QString pid = job->binding.param("portfolio_id");
    if (!pid.isEmpty()) {
        run_portfolio_summary(job, pid);
        return;
    }
    // No portfolio chosen: the first one the user has.
    const quint64 id = job->id;
    auto& ps = PortfolioService::instance();
    auto picked = std::make_shared<bool>(false);
    connect(&ps, &PortfolioService::portfolios_loaded, job->scope,
            [this, id, picked](const QVector<portfolio::Portfolio>& list) {
                if (*picked)
                    return;
                *picked = true;
                const JobPtr j = jobs_.value(id);
                if (!j)
                    return;
                if (list.isEmpty()) {
                    settle(id, rdb_fail(rdb_tr("no portfolio yet — create one in the Portfolio tab")));
                    return;
                }
                run_portfolio_summary(j, list.first().id);
            });
    connect(&ps, &PortfolioService::portfolios_load_failed, job->scope,
            [this, id](const QString& message, const QString&) { settle(id, rdb_fail(message)); });
    ps.load_portfolios();
}

void ReportDataBinder::run_portfolio_summary(const JobPtr& job, const QString& portfolio_id) {
    const quint64 id = job->id;
    const QString what = job->binding.param("what", QStringLiteral("holdings"));
    const QString type = job->block_type;
    auto& ps = PortfolioService::instance();
    connect(&ps, &PortfolioService::summary_loaded, job->scope,
            [this, id, portfolio_id, what, type](const portfolio::PortfolioSummary& s) {
                if (s.portfolio.id != portfolio_id)
                    return;
                settle(id, rdb_shape_portfolio(s, what, type));
            });
    connect(&ps, &PortfolioService::summary_error, job->scope,
            [this, id, portfolio_id](const QString& pid, const QString& err) {
                if (pid == portfolio_id)
                    settle(id, rdb_fail(err));
            });
    ps.load_summary(portfolio_id);
}

// ── Economic calendar ───────────────────────────────────────────────────────
//
// The free data_economic_calendar connector (Forex Factory's weekly export) through
// ConnectorService — never a raw process. It answers one day per call: the first call runs
// alone (it downloads the weekly file, which the script caches for every later call), then
// the other days of the feed's week; PythonRunner queues them. When the job ends early
// (stopped, timed out, replaced), every run still waiting is cancelled.

struct ReportDataBinder::EconRun {
    econcal::Request req;
    QString block_type;
    QDate today;
    QMap<QDate, QStringList> argv;        ///< day -> <command> [args]
    QVector<econcal::DayResult> replies;
    QSet<QString> open;                   ///< connector request ids still waiting
    int pending = 0;
};

void ReportDataBinder::run_econ_calendar(const JobPtr& job) {
    namespace conn = ::fincept::services::connectors;
    const quint64 id = job->id;
    const QDate today = QDate::currentDate();
    const econcal::Request req = econcal::request_of(job->binding, today);
    if (!req.ok) {
        settle(id, rdb_fail(req.error));
        return;
    }
    auto run = std::make_shared<EconRun>();
    run->req = req;
    run->block_type = job->block_type;
    run->today = today;
    connect(job->scope, &QObject::destroyed, this, [run]() {
        for (const QString& rid : std::as_const(run->open))
            conn::ConnectorService::instance().cancel(rid);
        run->open.clear();
    });
    // The catalogue (a file read on first use) and the argument rules, off the GUI thread.
    (void)QtConcurrent::run([this, id, run]() {
        auto& catalog = conn::ConnectorCatalog::instance();
        const conn::ConnectorInfo* info =
            catalog.ensure_loaded() ? catalog.find(QString::fromLatin1(econcal::kConnector)) : nullptr;
        const conn::ConnectorCommand* cmd = info ? info->command(QString::fromLatin1(econcal::kCommand)) : nullptr;
        QString why;
        QMap<QDate, QStringList> argv;
        if (!cmd) {
            why = rdb_tr("the economic calendar connector is missing from this installation's data catalogue");
        } else {
            for (const QDate& d : run->req.days) {
                if (!cmd->params_known) { // older catalogue: `day [date]`
                    argv.insert(d, QStringList{cmd->name, conn::date_arg(d)});
                    continue;
                }
                QVector<conn::ParamInput> inputs(cmd->params.size());
                for (int i = 0; i < cmd->params.size(); ++i) {
                    if (cmd->params[i].kind == conn::ParamKind::Date) {
                        inputs[i] = conn::ParamInput::of(conn::date_arg(d));
                        break;
                    }
                }
                const conn::ArgvResult a = conn::build_argv(cmd->name, cmd->params, inputs);
                if (!a.ok) {
                    why = a.error;
                    break;
                }
                argv.insert(d, a.argv);
            }
        }
        QMetaObject::invokeMethod(
            this,
            [this, id, run, argv, why]() {
                if (!jobs_.contains(id))
                    return; // stopped meanwhile
                if (!why.isEmpty()) {
                    settle(id, rdb_fail(why));
                    return;
                }
                run->argv = argv;
                econ_run_day(id, run, econcal::first_day(run->req, run->today), /*first=*/true);
            },
            Qt::QueuedConnection);
    });
}

void ReportDataBinder::econ_run_day(quint64 job_id, const std::shared_ptr<EconRun>& run, const QDate& day, bool first) {
    namespace conn = ::fincept::services::connectors;
    const QStringList argv = run->argv.value(day);
    if (argv.isEmpty()) {
        if (first)
            settle(job_id, rdb_fail(rdb_tr("no economic calendar request could be built for %1")
                                        .arg(day.toString(QStringLiteral("d MMM yyyy")))));
        return;
    }
    conn::ConnectorService::RunOptions opts;
    opts.timeout_ms = 60'000;
    ++run->pending;
    // Not forced: the service's cache lives as long as the feed copy the script serves (15 min).
    const QString rid = conn::ConnectorService::instance().run_with(
        QString::fromLatin1(econcal::kConnector), argv.first(), argv.mid(1), opts,
        [this, job_id, run, day, first](const conn::ConnectorResult& res) {
            run->open.remove(res.request_id);
            --run->pending;
            if (!jobs_.contains(job_id))
                return; // settled (stopped / timed out): this reply is not wanted
            const econcal::DayResult d = econcal::day_from_connector(day, res.success, res.failure, res.tabular);
            run->replies.append(d);
            if (first) {
                bool stop = false;
                const QVector<QDate> rest = econcal::remaining_days(run->req, d, &stop);
                if (!stop)
                    for (const QDate& next : rest)
                        econ_run_day(job_id, run, next, false);
            }
            if (run->pending == 0)
                econ_finish(job_id, run);
        });
    run->open.insert(rid); // the reply never arrives inside run_with()
}

void ReportDataBinder::econ_finish(quint64 job_id, const std::shared_ptr<EconRun>& run) {
    const econcal::Request req = run->req;
    const QVector<econcal::DayResult> replies = run->replies;
    const QString type = run->block_type;
    (void)QtConcurrent::run([this, job_id, req, replies, type]() {
        const econcal::Shaped s = econcal::shape(req, replies, type);
        BindFetchResult r;
        r.ok = s.ok;
        r.error = s.error;
        r.patch = s.patch;
        r.has_content = s.has_content;
        r.content = s.content;
        r.suggested_title = s.title;
        r.as_of = s.as_of;
        r.source_label = s.source_label;
        QMetaObject::invokeMethod(this, [this, job_id, r]() { settle(job_id, r); }, Qt::QueuedConnection);
    });
}

} // namespace fincept::services
