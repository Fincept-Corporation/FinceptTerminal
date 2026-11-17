#pragma once
// FinceptDataService — client of the Fincept /data REST API (`<api_base_url>/data/...`):
// options chains, analyst and ownership overviews, earnings / IPO / split /
// economic calendars (and, later, the other datasets the API lists).
//
// Every data call is billed (same price as /market) — cache hits on the server
// included — so this service exists to ask as rarely as possible:
//  * nothing is sent until a caller asks (no prefetch, no polling);
//  * a client copy keyed by path + sorted query is reused while the server's own
//    freshness window (meta.max_age_seconds - meta.age_seconds) lasts, with a short
//    floor so a tab switch / re-open never re-bills; a "refresh" skips the copy only
//    once that window has passed (inside it the server would bill for the same answer);
//  * identical requests in flight share one HTTP request;
//  * after a throttle / rate-limit answer the source is not asked again before its
//    Retry-After (calls fail locally until then); 402/403/404/422 are never retried.
//
// Capability discovery: the first call after a sign-in asks GET /data/datasets
// (free) and keeps the catalogue (which datasets exist, which are configured).
// A 404 there means this server has no /data API yet: remembered for the sign-in,
// and every call then fails at once, locally, with ErrorKind::ServerLacksApi —
// no request, no retry. Re-checked when the signed-in account changes.
//
// Requests go through the shared HttpClient (auth headers, stale-session recovery,
// IP-block reporting). Callbacks run on the GUI thread, always later (never inside
// the call that asked), and never after their `context` object was destroyed.
//
// Failures are classified from the response headers when the server sends them —
// Retry-After (how long to hold off), X-Data-Error (the kind) and
// X-Symbol-Suggestions — read through HttpClient::error_header(). Without them (an
// older server, a proxy's answer) the kind is inferred from the status and the
// server's fixed wording, and the wait falls back to a per-kind default
// (classify_error() / default_retry_after_s() in FinceptDataParse).

#include "services/fincept_data/FinceptDataModels.h"
#include "services/fincept_data/FinceptDataParse.h"

#include "core/result/Result.h"

#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services::fincept_data {

class FinceptDataService : public QObject {
    Q_OBJECT
  public:
    /// What this server offers, as far as the client knows.
    enum class Availability {
        Unknown,     ///< not asked yet this sign-in (or the last check failed in transit)
        Checking,    ///< GET /data/datasets in flight
        Available,   ///< the server has the /data API; catalog() lists its datasets
        Unavailable, ///< the server has no /data API (404) — nothing is sent until the account changes
    };
    enum class DatasetState {
        Unknown,       ///< no catalogue yet
        Available,     ///< listed and configured
        NotOnServer,   ///< the API exists but this dataset is not listed (older server)
        NotConfigured, ///< listed with available = false (its source is not configured)
    };
    using Callback = std::function<void(const DataResult&)>;

    static FinceptDataService& instance();

    Availability availability() const { return availability_; }
    bool is_available() const { return availability_ == Availability::Available; }
    /// True once this server answered that it has no /data API.
    bool server_lacks_api() const { return availability_ == Availability::Unavailable; }
    DatasetState dataset_state(const QString& dataset_id) const;
    const DatasetCatalog& catalog() const { return catalog_; }

    /// GET `path` (a /data route, segments already encoded with path_segment())
    /// with `query`. `dataset_id` is the catalogue id ("options.chain") used for
    /// availability and Retry-After bookkeeping. `context` scopes the callback.
    /// `refresh`: the user asked for newer data (see the class comment).
    void fetch(const QString& dataset_id, const QString& path, const QueryParams& query, const QObject* context,
               Callback callback, bool refresh = false);

    // ── Typed requests (paths and parameters of apiback data_api/) ────────────
    /// GET /data/options/{symbol}/chain. Empty `expiration` = the nearest one.
    /// `strikes_around_atm` keeps N strikes below and N above the spot (1..100).
    void fetch_option_chain(const QString& symbol, const QString& expiration, int strikes_around_atm, int page,
                            int page_size, const QObject* context, Callback callback, bool refresh = false);
    /// GET /data/analyst/{symbol}?upgrades_limit=N
    void fetch_analyst(const QString& symbol, int upgrades_limit, const QObject* context, Callback callback,
                       bool refresh = false);
    /// GET /data/ownership/{symbol}?transactions_limit=N
    void fetch_ownership(const QString& symbol, int transactions_limit, const QObject* context, Callback callback,
                         bool refresh = false);
    /// GET /data/calendar/{earnings|ipos|splits|economic}. `symbol` is ignored where the
    /// route has no symbol filter; `sort` ("date" / "market_cap") only applies to earnings.
    void fetch_calendar(CalendarKind kind, const QDate& start, const QDate& end, const QString& symbol,
                        const QString& sort, int page, int page_size, const QObject* context, Callback callback,
                        bool refresh = false);

    /// Drop every callback still waiting for `context` (call it from a destructor).
    void cancel(const QObject* context);

    /// The client copy of this request is inside the server's freshness window
    /// (a refresh would be answered from it).
    bool is_fresh(const QString& path, const QueryParams& query) const;

    // ── User-facing text ─────────────────────────────────────────────────────
    /// One calm sentence for a server without the /data API.
    static QString unavailable_text();
    /// The reason for a failure, in the server's words where it gave some.
    static QString error_text(const DataError& e);
    /// "as of 15:45 · 3 min old · Cboe delayed quotes · 15-min delayed", or the stale form.
    static QString freshness_text(const DataResult& r, qint64 now_ms);
    /// "5 min" / "2 h" / "3 d".
    static QString age_text(int seconds);

  signals:
    /// availability() changed (discovery answered, or the signed-in account changed).
    void availability_changed();

  private:
    FinceptDataService();
    Q_DISABLE_COPY(FinceptDataService)

    struct Waiter {
        QPointer<QObject> context;
        bool scoped = false; ///< a context was given: skip the callback once it is gone
        Callback callback;
    };
    struct Request {
        QString dataset_id;
        QString key; ///< path?sorted-query — also the URL sent
        bool refresh = false;
        Waiter waiter;
    };
    struct CacheEntry {
        DataResult result;
        qint64 fresh_until_ms = 0; ///< the server's freshness window ends
        qint64 keep_until_ms = 0;  ///< reused without a refresh until then
    };
    struct Hold {
        qint64 until_ms = 0;
        DataError error;
    };

    void dispatch(Request request);
    void send(const QString& dataset_id, const QString& key);
    void on_reply(const QString& dataset_id, const QString& key, quint64 generation, const Result<QJsonDocument>& reply);
    void record(const QString& dataset_id, const QString& key, const DataResult& result, qint64 now_ms);
    void trim_cache();

    void start_discovery();
    void on_discovery(quint64 generation, const Result<QJsonDocument>& reply);
    void release_awaiting();
    void set_availability(Availability a);

    void on_auth_state_changed();
    void reset_for_new_account();
    static QByteArray current_identity();
    static bool signed_in();

    QString hold_key(const QString& dataset_id) const;
    const Hold* active_hold(const QString& dataset_id, qint64 now_ms) const;

    void deliver(const QVector<Waiter>& waiters, const DataResult& result);
    void deliver_later(const Waiter& waiter, const DataResult& result);
    static DataResult local_error(ErrorKind kind, const QString& code = {}, const QString& message = {});

    Availability availability_ = Availability::Unknown;
    DatasetCatalog catalog_;
    qint64 discovery_retry_ms_ = 0; ///< after a failed check: optimistic until then, re-check after
    bool discovery_used_ = false;   ///< the feature was used this run: re-check on an account change
    QVector<Request> awaiting_discovery_;

    QHash<QString, QVector<Waiter>> inflight_;
    QHash<QString, CacheEntry> cache_;
    QHash<QString, CacheEntry> negative_; ///< 404 / 422 answers, reused (unless refreshed) for a while
    QHash<QString, Hold> holds_;          ///< provider id (or "*" for the plan limit) → not before

    QByteArray identity_;   ///< hash of the signed-in API key ("" = signed out)
    quint64 generation_ = 0; ///< bumped per account: answers for an older one are not kept
};

} // namespace fincept::services::fincept_data
