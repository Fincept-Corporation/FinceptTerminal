#pragma once
// FinceptMacroService — reads Fincept's own macro API (`<api_base_url>/macro/*`): CEIC time series and
// catalogue, WorldGovernmentBonds snapshots (central-bank rates, sovereign CDS, 10Y spreads, credit
// ratings, inverted curves, yield forecasts, per-country bond summary) and the macro event calendar.
//
// Deliberately NOT a DataHub producer (CLAUDE.md D2, one-shot user-initiated exception): every call is
// billed in Fincept credits (~0.5–1 each), so nothing may refresh on a scheduler cadence. A request is
// made only when the user asks (FETCH / REFRESH / opening the CEIC explorer). Responses are cached in
// CacheManager:
//   * inside the endpoint's fresh window a repeat query is answered locally and costs nothing;
//   * after it, the last good copy is still kept for 7 days and served — flagged `stale` — when the
//     network or the server fails, so an outage shows yesterday's numbers clearly labelled instead of a
//     blank panel.
// Requests go through the shared HttpClient, which attaches the signed-in user's credentials to
// same-host api.fincept.in calls — no key is baked into the binary.

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

#include <optional>

namespace fincept::services {

struct FinceptMacroResult {
    enum class Status { Ok, Unauthorized, NoCredits, Offline, Error };
    Status status = Status::Error;
    QString request_id;
    QString endpoint;        // path below /macro, e.g. "wgb/central-bank-rates"
    QJsonArray rows;         // the payload's list (central_bank_rates, series, events, …)
    QJsonObject payload;     // the full `data` object (total_count, returned_count, …)
    QString error;           // technical detail of a failure (server text / network error)
    QDateTime fetched_at;    // UTC time the server produced `rows` — kept across cache hits
    bool from_cache = false; // answered from the local cache: no request, no credits
    bool stale = false;      // last good copy served because the live request failed
    int http_status = 0;     // 0 = no HTTP response (offline / DNS / TLS / timeout)
};

class FinceptMacroService : public QObject {
    Q_OBJECT
  public:
    using Params = QList<QPair<QString, QString>>;

    static FinceptMacroService& instance();

    /// GET /macro/<endpoint>?<params> (empty values are dropped). `force` skips the fresh-cache
    /// shortcut — use it for an explicit REFRESH. Result arrives via result_ready().
    void fetch(const QString& request_id, const QString& endpoint, const Params& params, bool force = false);

    /// The `data` object last cached for this exact query, if any — no request, no credits.
    std::optional<QJsonObject> cached_payload(const QString& endpoint, const Params& params) const;

    /// Credits the backend charges for one non-cached call (UI hint; the server is authoritative).
    static double credit_cost(const QString& endpoint);
    /// How long a response is reused without asking the server again.
    static int fresh_seconds(const QString& endpoint);

  signals:
    void result_ready(const fincept::services::FinceptMacroResult& result);

  private:
    explicit FinceptMacroService(QObject* parent = nullptr);
    Q_DISABLE_COPY(FinceptMacroService)

    struct CachedEntry {
        QJsonObject data;
        QDateTime fetched_at;
    };
    static QString cache_key(const QString& endpoint, const Params& params);
    static std::optional<CachedEntry> read_cache(const QString& key);
    static QJsonArray extract_rows(const QJsonObject& data);
};

} // namespace fincept::services

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::FinceptMacroResult)
