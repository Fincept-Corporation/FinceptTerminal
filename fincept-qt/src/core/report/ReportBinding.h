#pragma once
// ReportBinding.h — "what am I showing": the data recipe of a Report Builder block.
//
// A bound block remembers its source and parameters so it can be refreshed, and keeps
// the last fetched numbers in its ordinary payload keys (csv / data / price ...) as a
// snapshot: a report always opens, prints and syncs with content, offline included,
// and a refresh only replaces the snapshot. Everything is stored inside the existing
// string map of the component, so old .fincept files, old `reports` rows and the cloud
// JSON stay valid:
//
//   config["bind"]          compact JSON {"source":"<kind>","params":{...},"view":"<view>","v":1}
//   config["as_of"]         when the snapshot was taken, ISO-8601 UTC ("2026-11-17T13:02:00Z");
//                           files written before this header used "yyyy-MM-dd HH:mm t" (still read)
//   config["source_label"]  provenance shown under the block ("Derivatives", "Yahoo Finance")
//
// Pure data + JSON helpers, Qt Core only: services, other tabs and tests include it freely.

#include "core/report/ReportDocument.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTimeZone>

namespace fincept::report {

namespace keys {
inline constexpr char kBind[] = "bind";
inline constexpr char kAsOf[] = "as_of";
inline constexpr char kSourceLabel[] = "source_label";
} // namespace keys

/// Source kinds (Binding::source). Every one is free and already reachable in the terminal.
namespace source {
inline constexpr char kQuote[] = "quote";           ///< {symbol}                      market:quote:<SYM>
inline constexpr char kQuotes[] = "quotes";         ///< {symbols[], sort, limit}      MarketDataService::fetch_quotes
inline constexpr char kInfo[] = "info";             ///< {symbol}                      market:info:<SYM>
inline constexpr char kHistory[] = "history";       ///< {symbols[], range, interval, mode=price|rebased|pct}
inline constexpr char kFinancials[] = "financials"; ///< {symbol, statement=income|balance|cashflow, period=annual|quarterly, n}
inline constexpr char kPeers[] = "peers";           ///< {symbol, count}               equity research peers
inline constexpr char kNews[] = "news";             ///< {symbol, count}               equity research news
inline constexpr char kHeadlines[] = "headlines";   ///< {count}                       News tab feeds
inline constexpr char kPortfolio[] = "portfolio";   ///< {portfolio_id, what=holdings|allocation|summary}
inline constexpr char kWatchlist[] = "watchlist";   ///< {watchlist_id ("" = the default list), sort, limit}  quotes of its symbols
/// {date ("" = today, "{{date}}" = the report's date), period=day|week|rest_of_week, impact=high|medium|low|all,
///  currencies, limit}  Forex Factory's free weekly export through the data_economic_calendar connector
///  (services/report_builder/ReportEconCalendar.h) — current week only, no key, no credit.
inline constexpr char kEconCalendar[] = "econ_calendar";
inline constexpr char kStatic[] = "static";        ///< {origin} snapshot pushed by another tab; refresh is a no-op
} // namespace source

/// How the fetched data is laid out in the block (Binding::view).
namespace view {
inline constexpr char kQuote[] = "quote";         ///< market_data block
inline constexpr char kKpi[] = "kpi";             ///< stats_block ("Label: Value" lines)
inline constexpr char kTable[] = "table";         ///< table (config["csv"])
inline constexpr char kChart[] = "chart";         ///< chart (data / labels / series / ohlc)
inline constexpr char kList[] = "list";           ///< list (one item per line)
inline constexpr char kSparkline[] = "sparkline"; ///< sparkline
} // namespace view

struct Binding {
    QString source;
    QJsonObject params;
    QString view;
    int version = 1;

    bool is_valid() const { return !source.isEmpty(); }
    /// Something the Report Builder can re-fetch (a static snapshot cannot).
    bool is_refreshable() const { return is_valid() && source != QLatin1String(source::kStatic); }

    QJsonObject to_json() const {
        QJsonObject o;
        o.insert(QStringLiteral("source"), source);
        o.insert(QStringLiteral("params"), params);
        if (!view.isEmpty())
            o.insert(QStringLiteral("view"), view);
        o.insert(QStringLiteral("v"), version);
        return o;
    }
    QString to_string() const {
        return is_valid() ? QString::fromUtf8(QJsonDocument(to_json()).toJson(QJsonDocument::Compact)) : QString();
    }
    static Binding from_json(const QJsonObject& o) {
        Binding b;
        b.source = o.value(QStringLiteral("source")).toString().trimmed();
        b.params = o.value(QStringLiteral("params")).toObject();
        b.view = o.value(QStringLiteral("view")).toString();
        b.version = o.value(QStringLiteral("v")).toInt(1);
        return b;
    }
    static Binding parse(const QString& text) {
        if (text.trimmed().isEmpty())
            return {};
        const QJsonDocument d = QJsonDocument::fromJson(text.toUtf8());
        return d.isObject() ? from_json(d.object()) : Binding{};
    }

    QString symbol() const { return params.value(QStringLiteral("symbol")).toString(); }
    QStringList symbols() const {
        QStringList out;
        for (const QJsonValue& v : params.value(QStringLiteral("symbols")).toArray()) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty())
                out << s;
        }
        if (out.isEmpty() && !symbol().isEmpty())
            out << symbol();
        return out;
    }
    QString param(const char* key, const QString& fallback = {}) const {
        const QJsonValue v = params.value(QLatin1String(key));
        if (v.isString())
            return v.toString().isEmpty() ? fallback : v.toString();
        if (v.isDouble())
            return QString::number(v.toDouble());
        return fallback;
    }
    int int_param(const char* key, int fallback) const { return params.value(QLatin1String(key)).toInt(fallback); }

    /// Identity of the data asked for: two blocks with the same key share one fetch.
    QString key() const {
        return source + QLatin1Char('|') +
               QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)) + QLatin1Char('|') + view;
    }
    bool operator==(const Binding& o) const {
        return source == o.source && params == o.params && view == o.view && version == o.version;
    }
    bool operator!=(const Binding& o) const { return !(*this == o); }
};

// ── Constructors (one per source kind) ───────────────────────────────────────

inline QJsonArray to_json_array(const QStringList& items) {
    QJsonArray a;
    for (const QString& s : items)
        a.append(s);
    return a;
}

inline Binding quote_binding(const QString& symbol) {
    return {QString::fromLatin1(source::kQuote), QJsonObject{{QStringLiteral("symbol"), symbol.trimmed()}},
            QString::fromLatin1(view::kQuote), 1};
}
inline Binding quotes_binding(const QStringList& symbols, const QString& sort = {}, int limit = 0) {
    QJsonObject p{{QStringLiteral("symbols"), to_json_array(symbols)}};
    if (!sort.isEmpty())
        p.insert(QStringLiteral("sort"), sort); // "change_desc" | "change_asc" | "abs_change_desc"
    if (limit > 0)
        p.insert(QStringLiteral("limit"), limit);
    return {QString::fromLatin1(source::kQuotes), p, QString::fromLatin1(view::kTable), 1};
}
inline Binding info_binding(const QString& symbol) {
    return {QString::fromLatin1(source::kInfo), QJsonObject{{QStringLiteral("symbol"), symbol.trimmed()}},
            QString::fromLatin1(view::kKpi), 1};
}
inline Binding history_binding(const QStringList& symbols, const QString& range = QStringLiteral("6mo"),
                               const QString& interval = QStringLiteral("1d"),
                               const QString& mode = QStringLiteral("price"), const QString& chart = {}) {
    QJsonObject p{{QStringLiteral("symbols"), to_json_array(symbols)},
                  {QStringLiteral("range"), range},
                  {QStringLiteral("interval"), interval},
                  {QStringLiteral("mode"), mode}};
    if (!chart.isEmpty())
        p.insert(QStringLiteral("chart"), chart); // "line" | "candle" | "bar"
    return {QString::fromLatin1(source::kHistory), p, QString::fromLatin1(view::kChart), 1};
}
inline Binding financials_binding(const QString& symbol, const QString& statement = QStringLiteral("income"),
                                  const QString& period = QStringLiteral("annual"), int n = 4) {
    return {QString::fromLatin1(source::kFinancials),
            QJsonObject{{QStringLiteral("symbol"), symbol.trimmed()},
                        {QStringLiteral("statement"), statement},
                        {QStringLiteral("period"), period},
                        {QStringLiteral("n"), n}},
            QString::fromLatin1(view::kTable), 1};
}
inline Binding peers_binding(const QString& symbol, int count = 5) {
    return {QString::fromLatin1(source::kPeers),
            QJsonObject{{QStringLiteral("symbol"), symbol.trimmed()}, {QStringLiteral("count"), count}},
            QString::fromLatin1(view::kTable), 1};
}
inline Binding news_binding(const QString& symbol, int count = 8) {
    return {QString::fromLatin1(source::kNews),
            QJsonObject{{QStringLiteral("symbol"), symbol.trimmed()}, {QStringLiteral("count"), count}},
            QString::fromLatin1(view::kTable), 1};
}
inline Binding headlines_binding(int count = 10) {
    return {QString::fromLatin1(source::kHeadlines), QJsonObject{{QStringLiteral("count"), count}},
            QString::fromLatin1(view::kList), 1};
}
inline Binding portfolio_binding(const QString& portfolio_id, const QString& what) {
    const QString v = what == QLatin1String("allocation") ? QString::fromLatin1(view::kChart)
                      : what == QLatin1String("summary")  ? QString::fromLatin1(view::kKpi)
                                                          : QString::fromLatin1(view::kTable);
    return {QString::fromLatin1(source::kPortfolio),
            QJsonObject{{QStringLiteral("portfolio_id"), portfolio_id}, {QStringLiteral("what"), what}}, v, 1};
}
inline Binding watchlist_binding(const QString& watchlist_id, const QString& sort = {}, int limit = 0) {
    QJsonObject p{{QStringLiteral("watchlist_id"), watchlist_id}};
    if (!sort.isEmpty())
        p.insert(QStringLiteral("sort"), sort);
    if (limit > 0)
        p.insert(QStringLiteral("limit"), limit);
    return {QString::fromLatin1(source::kWatchlist), p, QString::fromLatin1(view::kTable), 1};
}
/// `view_kind` = view::kTable or view::kList.
inline Binding econ_calendar_binding(const QString& date = QStringLiteral("{{date}}"),
                                     const QString& period = QStringLiteral("week"),
                                     const QString& impact = QStringLiteral("medium"),
                                     const QString& view_kind = QString::fromLatin1(view::kTable)) {
    return {QString::fromLatin1(source::kEconCalendar),
            QJsonObject{{QStringLiteral("date"), date}, {QStringLiteral("period"), period}, {QStringLiteral("impact"), impact}},
            view_kind, 1};
}
inline Binding static_binding(const QString& origin) {
    return {QString::fromLatin1(source::kStatic), QJsonObject{{QStringLiteral("origin"), origin}}, QString(), 1};
}

// ── Reading / writing a component's binding ─────────────────────────────────

/// The block's binding: config["bind"], or — for blocks written before bindings
/// existed — what the old live blocks implied: a market_data symbol is a quote, a
/// chart fetched from a ticker is its 6-month daily history, a stats block that
/// stored its symbol is the company's key statistics.
inline Binding binding_of(const ReportComponent& c) {
    Binding b = Binding::parse(c.config.value(QLatin1String(keys::kBind)));
    if (b.is_valid())
        return b;
    if (c.type == QLatin1String("market_data")) {
        const QString sym = c.config.value(QStringLiteral("symbol")).trimmed();
        if (!sym.isEmpty())
            return quote_binding(sym);
    } else if (c.type == QLatin1String("chart")) {
        const QString sym = c.config.value(QStringLiteral("fetch_symbol")).trimmed();
        if (!sym.isEmpty())
            return history_binding({sym}, QStringLiteral("6mo"), QStringLiteral("1d"));
    } else if (c.type == QLatin1String("stats_block")) {
        const QString sym = c.config.value(QStringLiteral("symbol")).trimmed();
        if (!sym.isEmpty())
            return info_binding(sym);
    }
    return {};
}

inline bool is_bound(const ReportComponent& c) {
    return binding_of(c).is_refreshable();
}

/// Writes the binding; legacy readers keep working (a market_data block also keeps
/// config["symbol"], which the canvas and older builds read).
inline void set_binding(ReportComponent& c, const Binding& b) {
    if (!b.is_valid()) {
        c.config.remove(QLatin1String(keys::kBind));
        return;
    }
    c.config.insert(QLatin1String(keys::kBind), b.to_string());
    if (c.type == QLatin1String("market_data") && b.source == QLatin1String(source::kQuote))
        c.config.insert(QStringLiteral("symbol"), b.symbol());
    if (c.type == QLatin1String("stats_block") && b.source == QLatin1String(source::kInfo))
        c.config.insert(QStringLiteral("symbol"), b.symbol());
}

/// "Unbind (keep numbers)": the snapshot stays, the recipe (and the legacy hints that
/// would re-infer one) go.
inline void clear_binding(ReportComponent& c) {
    c.config.remove(QLatin1String(keys::kBind));
    c.config.remove(QStringLiteral("fetch_symbol"));
    if (c.type == QLatin1String("stats_block"))
        c.config.remove(QStringLiteral("symbol"));
}

// ── As of / staleness ────────────────────────────────────────────────────────

inline QString format_as_of(const QDateTime& when) {
    return when.isValid() ? when.toUTC().toString(Qt::ISODate) : QString();
}

/// ISO-8601 (written by this code), or the "yyyy-MM-dd HH:mm[ tz]" text earlier
/// builds stored (read as local time). Invalid when unreadable.
inline QDateTime parse_as_of(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (dt.isValid())
        return dt;
    dt = QDateTime::fromString(s.left(16), QStringLiteral("yyyy-MM-dd HH:mm"));
    return dt.isValid() ? dt : QDateTime();
}

inline QDateTime as_of_of(const ReportComponent& c) {
    return parse_as_of(c.config.value(QLatin1String(keys::kAsOf)));
}

/// How old a snapshot may get before the block is marked stale.
inline int default_max_age_seconds(const Binding& b) {
    const QString& s = b.source;
    if (s == QLatin1String(source::kQuote) || s == QLatin1String(source::kQuotes) ||
        s == QLatin1String(source::kWatchlist))
        return 30 * 60;
    if (s == QLatin1String(source::kNews) || s == QLatin1String(source::kHeadlines) ||
        s == QLatin1String(source::kEconCalendar))
        return 6 * 3600;
    if (s == QLatin1String(source::kPortfolio))
        return 3600;
    if (s == QLatin1String(source::kFinancials))
        return 7 * 86400;
    if (s == QLatin1String(source::kHistory)) {
        const QString interval = b.param("interval", QStringLiteral("1d"));
        return interval.endsWith(QLatin1Char('m')) || interval.endsWith(QLatin1Char('h')) ? 3600 : 86400;
    }
    return 86400; // info, peers
}

/// Stale = a refreshable block whose snapshot is older than its source's max age, or
/// one that was never fetched.
inline bool is_stale(const ReportComponent& c, const QDateTime& now) {
    const Binding b = binding_of(c);
    if (!b.is_refreshable())
        return false;
    const QDateTime t = as_of_of(c);
    if (!t.isValid())
        return true;
    return t.secsTo(now) > default_max_age_seconds(b);
}

/// "14:02" today, "17 Nov 14:02" this year, else "17 Nov 2026 14:02" — local time.
inline QString as_of_display(const QDateTime& when, const QDateTime& now = QDateTime::currentDateTime()) {
    if (!when.isValid())
        return {};
    const QDateTime local = when.toLocalTime();
    const QDateTime n = now.toLocalTime();
    if (local.date() == n.date())
        return local.toString(QStringLiteral("HH:mm"));
    if (local.date().year() == n.date().year())
        return local.toString(QStringLiteral("d MMM HH:mm"));
    return local.toString(QStringLiteral("d MMM yyyy HH:mm"));
}

/// Short human description of the recipe, for the properties panel and notices.
inline QString describe(const Binding& b) {
    const QString sym = b.symbols().join(QStringLiteral(", "));
    const QString& s = b.source;
    if (s == QLatin1String(source::kQuote))
        return sym + QStringLiteral(" quote");
    if (s == QLatin1String(source::kQuotes))
        return QStringLiteral("Quotes: ") + sym;
    if (s == QLatin1String(source::kInfo))
        return sym + QStringLiteral(" key statistics");
    if (s == QLatin1String(source::kHistory))
        return sym + QStringLiteral(" price history (%1, %2)")
                         .arg(b.param("range", QStringLiteral("6mo")), b.param("interval", QStringLiteral("1d")));
    if (s == QLatin1String(source::kFinancials)) {
        const QString st = b.param("statement", QStringLiteral("income"));
        const QString name = st == QLatin1String("balance")    ? QStringLiteral("balance sheet")
                             : st == QLatin1String("cashflow") ? QStringLiteral("cash flow")
                                                               : QStringLiteral("income statement");
        return QStringLiteral("%1 %2 (%3)").arg(sym, name, b.param("period", QStringLiteral("annual")));
    }
    if (s == QLatin1String(source::kPeers))
        return sym + QStringLiteral(" peer comparison");
    if (s == QLatin1String(source::kNews))
        return sym + QStringLiteral(" news");
    if (s == QLatin1String(source::kHeadlines))
        return QStringLiteral("Market headlines");
    if (s == QLatin1String(source::kPortfolio))
        return QStringLiteral("Portfolio ") + b.param("what", QStringLiteral("holdings"));
    if (s == QLatin1String(source::kWatchlist))
        return b.param("watchlist_name").isEmpty() ? QStringLiteral("Watchlist quotes")
                                                    : QStringLiteral("Watchlist “%1”").arg(b.param("watchlist_name"));
    if (s == QLatin1String(source::kEconCalendar)) {
        const QString p = b.param("period", QStringLiteral("week"));
        return p == QLatin1String("day")            ? QStringLiteral("Economic calendar (one day)")
               : p == QLatin1String("rest_of_week") ? QStringLiteral("Economic calendar (rest of the week)")
                                                    : QStringLiteral("Economic calendar (week)");
    }
    if (s == QLatin1String(source::kStatic))
        return QStringLiteral("Snapshot from ") + b.param("origin", QStringLiteral("another tab"));
    return s;
}

} // namespace fincept::report
