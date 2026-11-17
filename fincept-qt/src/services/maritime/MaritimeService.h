// src/services/maritime/MaritimeService.h
#pragma once
#include "auth/AuthTypes.h"
#include "datahub/Producer.h"
#include "services/maritime/MaritimeTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>

#include <functional>

namespace fincept::services::maritime {

/// Singleton service for Maritime Intelligence — HTTP calls to api.fincept.in/marine/
class MaritimeService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static MaritimeService& instance();

    /// Register with the hub + install maritime:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    // Every vessel call is billed (area 8, history 5, multi 3, position 1 credit).
    // Answers are kept in CacheManager (persisted) so reopening the screen or
    // repeating a query inside the window costs nothing; `force` (the user's
    // REFRESH, or the hub refreshing a topic it already holds) skips that copy.

    /// Search vessels in a geographic bounding box
    void search_vessels_by_area(const AreaSearchParams& params, bool force = false);
    /// Identity of an area search (= its cache key); stamped on the VesselsPage
    /// that answers it (VesselsPage::request_key).
    static QString area_request_key(const AreaSearchParams& params);
    /// The kept copy of this exact search (kept 14 days; a search still only
    /// answers from it while it is younger than 10 min). Cheap — a small
    /// side entry, not the copy itself: true + when it was bought, or false.
    bool area_cached_at(const AreaSearchParams& params, qint64* fetched_at_ms) const;
    /// Hand the kept copy to `done` (on the GUI thread, in `context`), parsed off
    /// the GUI thread — never a request, never the shared vessels_loaded signal
    /// (a screen shows its last result on open with it; 8 credits saved).
    /// False when there is no copy.
    bool load_area_cached(const AreaSearchParams& params, QObject* context,
                          std::function<void(fincept::services::maritime::VesselsPage)> done);

    /// Get single vessel position by IMO number
    void get_vessel_position(const QString& imo, bool force = false);

    /// Get multiple vessel positions by IMO array
    void get_multi_vessel_positions(const QStringList& imos);

    /// Get vessel history. vessel_history_loaded follows asynchronously — from the
    /// kept copy as well (parsed off the GUI thread).
    void get_vessel_history(const QString& imo, bool force = false);

    /// Health check for marine API module
    void check_health();

  signals:
    /// Emitted by both area-search and multi-vessel calls. The page envelope
    /// carries credit metering and (for multi-vessel) the list of IMOs the
    /// API could not resolve.
    void vessels_loaded(fincept::services::maritime::VesselsPage page);
    void vessel_found(fincept::services::maritime::VesselData vessel);
    void vessel_history_loaded(fincept::services::maritime::VesselHistoryPage page);
    void health_loaded(QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit MaritimeService(QObject* parent = nullptr);
    Q_DISABLE_COPY(MaritimeService)

    VesselData parse_vessel(const QJsonObject& obj) const;
    /// Emit the CacheManager copy under `cache_key` when it is at most
    /// `max_age_sec` old (0 = any age). True when emitted.
    bool emit_cached_area(const QString& cache_key, int max_age_sec);

    bool hub_registered_ = false;

    // Voyage histories come back from a worker (fetched or the kept copy), so they
    // are ordered here (GUI thread only), per IMO: a fetched history is emitted
    // only if no newer fetch's was emitted before it, and the kept copy only if no
    // fetched history was emitted since it was read.
    quint64 history_fetch_seq_ = 0;                ///< history fetches started (all vessels)
    QHash<QString, quint64> history_fetch_shown_; ///< IMO -> newest fetch emitted (absent = none)

    // A hub topic refused with 401/402/403 backs off for hours (not the hub's
    // 5-minute ladder) until a sign-in or a top-up; see auth::RefusalLatch.
    void note_hub_refusal(const QString& topic, auth::RefusalLatch::Kind kind);
    void release_hub_refusal(const char* why);
    auth::RefusalLatch refusal_;
    QSet<QString> refused_topics_;
};

} // namespace fincept::services::maritime
