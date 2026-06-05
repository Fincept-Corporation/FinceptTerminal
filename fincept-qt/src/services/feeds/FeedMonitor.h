#pragma once
#include "services/feeds/FeedScraper.h"
#include "services/feeds/FeedTypes.h"

#include <QHash>
#include <QObject>
#include <QVector>

#include <functional>

class QNetworkAccessManager;
class QTimer;

namespace fincept::feeds {

/// App-lifetime singleton. Owns subscriptions + one QTimer per enabled feed,
/// fetches via a raw QNetworkAccessManager (HttpClient is JSON-only and cannot be
/// used for RSS/XML), parses via FeedScraper, caches latest items, and emits
/// per-feed updates consumed by any number of FeedViews (docked or floating).
class FeedMonitor : public QObject {
    Q_OBJECT
  public:
    static FeedMonitor& instance();

    void ensure_loaded(); // load subscriptions from repo (idempotent)
    QVector<FeedSubscription> subscriptions() const;
    QVector<FeedItem> cached_items(const QString& feed_id) const;
    FeedStatus status(const QString& feed_id) const;

    // CRUD — persist + re-arm timers + emit subscriptions_changed().
    void add_or_update(const FeedSubscription& sub);
    void remove(const QString& feed_id);
    void set_enabled(const QString& feed_id, bool enabled);

    void refresh_now(const QString& feed_id);
    void pause_all();
    void resume_all();

    // One-shot fetch for the config dialog Test/Preview (not persisted).
    using PreviewCb = std::function<void(bool ok, QString msg, QVector<FeedItem>)>;
    void fetch_once(const FeedSubscription& sub, PreviewCb cb);

    // One-shot fetch + structure probe for the manual-mapping UI (not persisted).
    using DiscoverCb = std::function<void(bool ok, QString msg, DiscoveredSchema)>;
    void discover(const FeedSubscription& sub, DiscoverCb cb);

  signals:
    void feed_updated(const QString& feed_id, const QVector<fincept::feeds::FeedItem>& items);
    void feed_status(const QString& feed_id, fincept::feeds::FeedStatus status, const QString& msg);
    void subscriptions_changed();

  private:
    FeedMonitor();
    void arm_timer(const FeedSubscription& sub);
    void disarm_timer(const QString& feed_id);
    void fetch(const FeedSubscription& sub);

    struct Entry {
        FeedSubscription sub;
        QTimer* timer = nullptr;
        QVector<FeedItem> items;
        FeedStatus status = FeedStatus::Idle;
    };
    QNetworkAccessManager* nam_ = nullptr;
    QHash<QString, Entry> feeds_;
    bool loaded_ = false;
    bool paused_ = false;

    static constexpr int kMinRefreshSec = 15;
    static constexpr int kMaxItems = 200;     // in-memory cache cap (live display)
    static constexpr int kMaxHistory = 2000;  // persisted-history cap per feed
};

} // namespace fincept::feeds
