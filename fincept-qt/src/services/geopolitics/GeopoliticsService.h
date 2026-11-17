// src/services/geopolitics/GeopoliticsService.h
#pragma once
#include "auth/AuthTypes.h"
#include "datahub/Producer.h"
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>

#include <functional>

namespace fincept::services::geo {

/// Singleton service for geopolitics data — news events API + HDX Python + trade analysis.
class GeopoliticsService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static GeopoliticsService& instance();

    /// Register with the hub + install geopolitics:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    // ── Conflict Monitor (HTTP API) ─────────────────────────────────────────
    //
    // The Fincept news-events endpoint now supports pagination + extra
    // filters echoed in `filters_applied`. `limit` maps to events_per_page,
    // `page` is 1-indexed. `source`, `date_from`, `date_to` are forwarded
    // verbatim — server validates and ignores empty values.
    void fetch_events(const QString& country = {}, const QString& city = {}, const QString& category = {},
                      int limit = 100, int page = 1, const QString& source = {}, const QString& date_from = {},
                      const QString& date_to = {});
    /// Reference lists (3 credits each when fetched). A CacheManager copy (6 h)
    /// answers without a request. `cache_only`: answer only from that copy and
    /// never fetch — opening a screen must not spend credits. Returns true when
    /// the list was emitted from the cache.
    bool fetch_unique_countries(bool cache_only = false);
    bool fetch_unique_categories(bool cache_only = false);
    /// As above, except that the city list (unbounded — every city the events
    /// database knows) is parsed, shaped and serialised off the GUI thread:
    /// true means the cache copy answers and cities_loaded follows shortly.
    bool fetch_unique_cities(bool cache_only = false);

    // ── HDX Humanitarian Data (Python) ──────────────────────────────────────
    // `limit` = most datasets returned (the script's row count); hdx_total_count
    // reports how many the search matched in all.
    static constexpr int kHdxDefaultLimit = 20;
    void search_hdx_conflicts(int limit = kHdxDefaultLimit);
    void search_hdx_humanitarian(int limit = kHdxDefaultLimit);
    void search_hdx_by_country(const QString& country, int limit = kHdxDefaultLimit);
    void search_hdx_by_topic(const QString& topic, int limit = kHdxDefaultLimit);
    void search_hdx_advanced(const QString& query, int limit = kHdxDefaultLimit);
    /// Datasets published by one HDX organisation (`org_slug`, e.g. "unhcr");
    /// results arrive as context "organization".
    void search_hdx_by_organization(const QString& org_slug, int limit = kHdxDefaultLimit);

    // ── Trade Analysis (Python) ─────────────────────────────────────────────
    void analyze_trade_benefits(const QJsonObject& params);
    void analyze_trade_restrictions(const QJsonObject& params);
    /// Trading-bloc integration (trade creation vs diversion) — script mode `trading_blocs`.
    void analyze_trading_blocs(const QJsonObject& params);
    /// Impact of removing trade barriers (FDI / employment / GDP) — script mode `barrier_removal`.
    void analyze_barrier_removal(const QJsonObject& params);

  signals:
    void events_loaded(fincept::services::geo::EventsPage page);
    void countries_loaded(QVector<fincept::services::geo::UniqueCountry> countries);
    void categories_loaded(QVector<fincept::services::geo::UniqueCategory> categories);
    void cities_loaded(QStringList cities);
    void hdx_results_loaded(QString context, QVector<fincept::services::geo::HDXDataset> datasets);
    /// Emitted just before hdx_results_loaded for the same `context`: how many
    /// datasets the search matched (HDX "count"; may exceed the rows returned).
    void hdx_total_count(QString context, int total);
    void trade_result_ready(QString context, QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit GeopoliticsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(GeopoliticsService)

    void run_python(const QString& script, const QStringList& args, const QString& context,
                    std::function<void(bool, const QString&)> cb);

    // Every news-events call (events page or reference list) costs API credits.
    /// Hub TTL for the shared `geopolitics:events` topic — the source updates ~every 30 min.
    static constexpr int kEventsHubTtlSec = 30 * 60;
    /// Countries / categories / cities lists (CacheManager + hub TTL).
    static constexpr int kRefDataTtlSec = 6 * 60 * 60;

    void publish_hdx_result(const QString& context, const QVector<fincept::services::geo::HDXDataset>& datasets);
    /// Common completion of an HDX search: error_occurred(error_context) on
    /// failure, else hdx_total_count + hdx_results_loaded(result_context) and the
    /// hub publish under geopolitics:hdx:<hub_context>.
    void finish_hdx_search(bool ok, const QString& out, const QString& error_context, const QString& result_context,
                           const QString& hub_context);
    bool hub_registered_ = false;

    // Unique-cities answers come back from a worker, so they are ordered here (GUI
    // thread only): a fetched list is emitted only if no newer fetch's list was
    // emitted before it, and a cache copy only if no fetched list was emitted
    // since it was read (a fresher list is never replaced by an older one).
    quint64 cities_fetch_seq_ = 0;   ///< fetches started
    quint64 cities_fetch_shown_ = 0; ///< newest fetch whose list was emitted (0 = none)

    // A hub topic refused with 401/402/403 backs off for hours (not the hub's
    // 5-minute ladder) until a sign-in or a top-up; see auth::RefusalLatch.
    void note_hub_refusal(const QString& topic, auth::RefusalLatch::Kind kind);
    void release_hub_refusal(const char* why);
    auth::RefusalLatch refusal_;
    QSet<QString> refused_topics_;
};

} // namespace fincept::services::geo
