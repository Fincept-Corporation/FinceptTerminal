#pragma once
// FinceptDataModels.h — value types for the Fincept /data REST API
// (`<api_base_url>/data/...`, see FinceptDataService).
//
//  * DataResult: one answer — the `data` object, its `meta` (where the data came
//    from, how old it is, pagination) or a classified DataError.
//  * Typed payloads the terminal shows: options chain, analyst overview,
//    ownership overview, calendar pages and the dataset catalogue.
//
// Missing numbers are NaN (the Equity Research formatters print NaN as "—"),
// never 0: Yahoo and Cboe send null for "not reported", and 0 is a real value
// for volume, open interest or a bid.
//
// Plain structs, no QObject: parsed by the free functions in FinceptDataParse.h,
// which the stand-alone fixture test drives without the service or any widget.

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>
#include <limits>

namespace fincept::services::fincept_data {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// True when the source sent a number.
inline bool has(double v) {
    return std::isfinite(v);
}

// ── Errors ───────────────────────────────────────────────────────────────────

enum class ErrorKind {
    None,
    ServerLacksApi,     ///< this Fincept server has no /data API yet (404 on the route itself)
    DatasetUnavailable, ///< the server does not offer this dataset, or its source is not configured there
    NotSignedIn,        ///< no session, or 401
    NoCredits,          ///< 402: not enough credits (refunded server-side; nothing was charged)
    Forbidden,          ///< 403: IP block / account state (HttpClient reports IP blocks itself)
    NotFound,           ///< 404: unknown symbol, no listed options, expiration not listed, ...
    Invalid,            ///< 422: a parameter the server refused
    RateLimited,        ///< 429: the plan's hourly request budget or concurrency limit
    Throttled,          ///< 503: the source is rate-limiting / this server's budget for it is spent
    NotConfigured,      ///< 503: the source is not configured on this server
    Upstream,           ///< 502: the source is unreachable or answered with an error
    Timeout,            ///< 504: the source did not answer in time
    Network,            ///< no HTTP status at all (DNS, refused, TLS, transfer timeout)
    BadResponse,        ///< a 2xx that is not a /data envelope
    Server,             ///< any other HTTP status
};

struct DataError {
    ErrorKind kind = ErrorKind::None;
    int status = 0;          ///< HTTP status (0 = none)
    QString code;            ///< the server's X-Data-Error kind ("not_found", "throttled", "busy", ...)
    QString message;         ///< the server's own sentence; may be empty
    int retry_after_s = 0;   ///< do not ask again for this long (Retry-After); 0 = no constraint
    QStringList suggestions; ///< "did you mean" symbols of a 404 (X-Symbol-Suggestions)

    bool ok() const { return kind == ErrorKind::None; }
};

// ── Envelope ─────────────────────────────────────────────────────────────────

struct Pagination {
    bool present = false;
    int page = 0;
    int page_size = 0;
    int total = 0;
    int total_pages = 0;
    bool has_more = false;
};

/// The envelope's `meta` object.
struct DataMeta {
    QString dataset;     ///< dataset id, e.g. "options.chain"
    QString source;      ///< provider id: "cboe", "yahoo", ...
    QString source_name; ///< "Cboe delayed quotes (cdn.cboe.com)"
    QDateTime as_of;     ///< UTC: when the server fetched the data from the source
    bool cached = false; ///< served from the server's cache
    int age_seconds = 0; ///< age of the data when the server answered
    int max_age_seconds = 0;
    bool stale = false;  ///< served past its freshness because the source is failing
    QString stale_reason;
    int delayed_minutes = 0;  ///< exchange delay of quotes (options: 15)
    QString source_time;      ///< the source's own quote time, as sent (options)
    QString source_timezone;  ///< time zone of source_time
    Pagination pagination;
};

/// One answer of the /data API (or a local refusal: no data API, throttled, ...).
struct DataResult {
    DataError error;
    QJsonObject data;
    DataMeta meta;
    qint64 received_ms = 0;        ///< wall clock (ms since epoch) when the answer arrived
    bool from_client_cache = false; ///< answered from this client's copy (no request, nothing billed)

    bool ok() const { return error.ok(); }

    /// Age of the data at `now_ms`: the server's age at receipt plus the time since.
    int age_seconds_at(qint64 now_ms) const {
        if (received_ms <= 0)
            return meta.age_seconds;
        const qint64 since = now_ms > received_ms ? (now_ms - received_ms) / 1000 : 0;
        return meta.age_seconds + static_cast<int>(since);
    }
};

// ── Dataset catalogue (/data/datasets) ───────────────────────────────────────

struct DatasetInfo {
    QString id;          ///< "options.chain"
    QString path;        ///< "/data/options/{symbol}/chain"
    QString group;       ///< "options"
    QString summary;
    QString source_id;   ///< "cboe"
    QString source_name;
    QString freshness;   ///< human description of the freshness policy
    bool available = true; ///< false: the source is not configured on this server
    bool paginated = false;
    double credits_per_call = 0.0;
    int ttl_seconds = 0;
};

struct DatasetCatalog {
    QHash<QString, DatasetInfo> datasets; ///< by id
    bool contains(const QString& id) const { return datasets.contains(id); }
};

// ── Options ──────────────────────────────────────────────────────────────────

struct OptionUnderlying {
    QString symbol;
    QString security_type;
    double price = kNaN;
    double bid = kNaN;
    double ask = kNaN;
    double prev_close = kNaN;
    double change = kNaN;
    double change_pct = kNaN; ///< percent (0.39 = 0.39 %)
    double volume = kNaN;
    double iv30 = kNaN;       ///< percent (Cboe), NaN from the fallback source
    QString last_trade_time;
};

struct OptionContract {
    QString contract;   ///< OCC symbol, e.g. AAPL261116C00275000
    QString expiration; ///< YYYY-MM-DD
    bool is_call = true;
    double strike = kNaN;
    double bid = kNaN;
    double ask = kNaN;
    double mid = kNaN;
    double last = kNaN;
    double change = kNaN;
    double change_pct = kNaN;
    double volume = kNaN;
    double open_interest = kNaN;
    double iv = kNaN;   ///< fraction (0.8784 = 87.84 %)
    double delta = kNaN;
    double gamma = kNaN;
    double theta = kNaN;
    double vega = kNaN;
    double rho = kNaN;
    double theo = kNaN;
    int dte = -1;       ///< days to expiration (-1 unknown)
    int in_the_money = -1; ///< 1 / 0, -1 when the server could not tell
    QString last_trade_time;
};

struct OptionChain {
    QString symbol;          ///< as requested
    QString resolved_symbol; ///< as the source lists it (SPX root, BRK-B, ...)
    QString exchange;
    OptionUnderlying underlying;
    QString expiration;      ///< the expiration these contracts belong to ("all" for every one)
    QStringList expirations; ///< every listed expiration, ascending
    bool greeks_available = false; ///< false on the fallback source (Yahoo)
    QVector<OptionContract> contracts;
};

/// One strike of a calls | strike | puts table: indexes into OptionChain::contracts (-1 = none).
struct StraddleRow {
    double strike = kNaN;
    int call = -1;
    int put = -1;
};

// ── Analysts ─────────────────────────────────────────────────────────────────

struct PriceTargets {
    double current_price = kNaN;
    double low = kNaN;
    double mean = kNaN;
    double median = kNaN;
    double high = kNaN;
    double analysts = kNaN;
    double recommendation_mean = kNaN; ///< 1 = strong buy … 5 = strong sell
    QString recommendation_key;        ///< "buy", "strong_buy", "hold", ...
};

struct RecommendationPeriod {
    QString period; ///< "0m" (this month), "-1m", ...
    double strong_buy = kNaN;
    double buy = kNaN;
    double hold = kNaN;
    double sell = kNaN;
    double strong_sell = kNaN;
};

struct EstimateBand {
    double avg = kNaN;
    double low = kNaN;
    double high = kNaN;
    double year_ago = kNaN;
    double analysts = kNaN;
    double growth = kNaN; ///< fraction
    QString currency;
};

struct Estimate {
    QString period;   ///< "0q", "+1q", "0y", "+1y"
    QString end_date; ///< YYYY-MM-DD
    double growth = kNaN;
    EstimateBand eps;
    EstimateBand revenue;
    double eps_current = kNaN;
    double eps_7d_ago = kNaN;
    double eps_30d_ago = kNaN;
    double eps_60d_ago = kNaN;
    double eps_90d_ago = kNaN;
    double up_7d = kNaN;
    double up_30d = kNaN;
    double down_7d = kNaN;
    double down_30d = kNaN;
};

struct EarningsSurprise {
    QString quarter; ///< fiscal quarter end, YYYY-MM-DD
    QString period;  ///< "-1q", ...
    double eps_actual = kNaN;
    double eps_estimate = kNaN;
    double eps_difference = kNaN;
    double surprise_pct = kNaN; ///< percent
};

struct RatingChange {
    QDateTime date; ///< UTC
    QString firm;
    QString to_grade;
    QString from_grade;
    QString action;              ///< "up", "down", "main", "init", "reit"
    QString price_target_action; ///< "Raises", "Lowers", "Maintains", "Announces"
    double price_target = kNaN;
    double prior_price_target = kNaN;
};

struct AnalystOverview {
    QString symbol;
    QString resolved_symbol;
    QString name;
    QString currency;
    PriceTargets targets;
    QVector<RecommendationPeriod> trend;
    QVector<Estimate> estimates;
    QVector<EarningsSurprise> history;
    QVector<RatingChange> rating_changes; ///< newest first
    int rating_changes_total = 0;         ///< before the server's limit
};

// ── Ownership ────────────────────────────────────────────────────────────────

struct Holder {
    QString holder;
    QDate report_date;
    double pct_held = kNaN;   ///< percent
    double shares = kNaN;
    double value = kNaN;
    double pct_change = kNaN; ///< percent
};

struct InsiderTransaction {
    QDate date;
    QString insider;
    QString relation;
    QString transaction; ///< "Sale at price 351.36 per share."
    QString ownership;   ///< "direct" / "indirect"
    double shares = kNaN;
    double value = kNaN;
};

struct InsiderRosterEntry {
    QString name;
    QString relation;
    QString latest_transaction;
    QDate latest_transaction_date;
    double shares_direct = kNaN;
    double shares_indirect = kNaN;
};

struct InsiderActivity {
    bool present = false;
    QString period; ///< "6m"
    double buy_count = kNaN;
    double buy_shares = kNaN;
    double sell_count = kNaN;
    double sell_shares = kNaN;
    double net_count = kNaN;
    double net_shares = kNaN;
    double net_pct_insider_shares = kNaN; ///< percent
    double total_insider_shares = kNaN;
    double net_institutional_shares_buying = kNaN;
};

struct OwnershipOverview {
    QString symbol;
    QString resolved_symbol;
    double insiders_pct = kNaN;           ///< percent
    double institutions_pct = kNaN;       ///< percent
    double institutions_float_pct = kNaN; ///< percent
    double institutions_count = kNaN;
    QVector<Holder> institutional;
    QVector<Holder> funds;
    QVector<InsiderTransaction> transactions; ///< newest first
    int transactions_total = 0;               ///< before the server's limit
    QVector<InsiderRosterEntry> roster;
    InsiderActivity activity;
};

// ── Calendars ────────────────────────────────────────────────────────────────

enum class CalendarKind { Earnings, Ipos, Splits, Economic };

/// One calendar row. Which fields are filled depends on the calendar kind.
struct CalendarEvent {
    QDateTime when; ///< UTC: earnings / economic release time, IPO date, split payable date
    QString symbol;
    QString company;
    // earnings
    QString event;  ///< "Q3 2027 Earnings Announcement"
    QString timing; ///< "BMO" / "AMC" / "TAS" / "TNS"
    double market_cap = kNaN;
    double eps_estimate = kNaN;
    double eps_actual = kNaN;
    double eps_surprise_pct = kNaN; ///< percent
    // IPOs
    QString exchange;
    QString currency;
    QString deal_type; ///< "Expected", "Priced", "Filed", "Amended", "Withdrawn"
    QDateTime filing_date;
    QDateTime amended_date;
    double price_from = kNaN;
    double price_to = kNaN;
    double offer_price = kNaN;
    double shares = kNaN;
    // splits
    bool optionable = false;
    double old_shares = kNaN;
    double new_shares = kNaN;
    // economic
    QString region; ///< country code
    QString period; ///< "Oct", "Q3", ...
    double actual = kNaN;
    double consensus = kNaN;
    double prior = kNaN;
    double revised_from = kNaN;
};

struct CalendarPage {
    CalendarKind kind = CalendarKind::Earnings;
    QDate start;
    QDate end;
    QString symbol; ///< the symbol filter as the server resolved it (empty: none)
    QVector<CalendarEvent> events;
};

} // namespace fincept::services::fincept_data
