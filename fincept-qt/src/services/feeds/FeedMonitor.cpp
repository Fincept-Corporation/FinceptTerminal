#include "services/feeds/FeedMonitor.h"

#include "services/feeds/FeedParseUtil.h"
#include "services/feeds/FeedScraper.h"
#include "storage/repositories/FeedItemRepository.h"
#include "storage/repositories/FeedSubscriptionRepository.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace fincept::feeds {

namespace {

const char* kBrowserUA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                         "(KHTML, like Gecko) Chrome/120.0 Safari/537.36";
constexpr int kTransferTimeoutMs = 15000;

QNetworkRequest build_request(const FeedSubscription& sub) {
    QNetworkRequest req{QUrl(sub.url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, kBrowserUA);
    req.setRawHeader("Accept", "application/rss+xml, application/xml, text/xml, application/json, text/csv, */*");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(kTransferTimeoutMs);
    for (auto it = sub.headers.begin(); it != sub.headers.end(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    return req;
}

} // namespace

FeedMonitor& FeedMonitor::instance() {
    static FeedMonitor s;
    return s;
}

FeedMonitor::FeedMonitor() : QObject(nullptr) {
    qRegisterMetaType<fincept::feeds::FeedItem>("fincept::feeds::FeedItem");
    qRegisterMetaType<QVector<fincept::feeds::FeedItem>>("QVector<fincept::feeds::FeedItem>");
    qRegisterMetaType<fincept::feeds::FeedStatus>("fincept::feeds::FeedStatus");
    nam_ = new QNetworkAccessManager(this);
}

void FeedMonitor::ensure_loaded() {
    if (loaded_)
        return;
    loaded_ = true;
    const auto res = FeedSubscriptionRepository::instance().list_all();
    if (res.is_ok()) {
        for (const auto& sub : res.value()) {
            Entry e;
            e.sub = sub;
            // Seed the in-memory cache from stored history so cached items show
            // immediately (and survive a feed being down on first fetch).
            if (sub.persist) {
                const auto cached = FeedItemRepository::instance().recent(sub.id, kMaxItems);
                if (cached.is_ok())
                    e.items = cached.value();
            }
            feeds_.insert(sub.id, e);
            if (sub.enabled)
                arm_timer(sub);
        }
    }
}

QVector<FeedSubscription> FeedMonitor::subscriptions() const {
    QVector<FeedSubscription> out;
    out.reserve(feeds_.size());
    for (const auto& e : feeds_)
        out.append(e.sub);
    std::sort(out.begin(), out.end(),
              [](const FeedSubscription& a, const FeedSubscription& b) { return a.sort_order < b.sort_order; });
    return out;
}

QVector<FeedItem> FeedMonitor::cached_items(const QString& id) const {
    auto it = feeds_.find(id);
    return it != feeds_.end() ? it->items : QVector<FeedItem>{};
}

FeedStatus FeedMonitor::status(const QString& id) const {
    auto it = feeds_.find(id);
    return it != feeds_.end() ? it->status : FeedStatus::Idle;
}

void FeedMonitor::arm_timer(const FeedSubscription& sub) {
    Entry& e = feeds_[sub.id];
    e.sub = sub;
    if (!e.timer) {
        e.timer = new QTimer(this);
        const QString id = sub.id;
        connect(e.timer, &QTimer::timeout, this, [this, id]() {
            auto it = feeds_.find(id);
            if (it != feeds_.end())
                fetch(it->sub);
        });
    }
    e.timer->setInterval(std::max(kMinRefreshSec, sub.refresh_sec) * 1000);
    if (!paused_) {
        e.timer->start();
        fetch(sub); // immediate first fetch
    }
}

void FeedMonitor::disarm_timer(const QString& id) {
    auto it = feeds_.find(id);
    if (it != feeds_.end() && it->timer)
        it->timer->stop();
}

void FeedMonitor::fetch(const FeedSubscription& sub) {
    feeds_[sub.id].status = FeedStatus::Fetching;
    emit feed_status(sub.id, FeedStatus::Fetching, QStringLiteral("Fetching…"));

    auto* reply = nam_->get(build_request(sub));
    const QString id = sub.id;
    connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
        reply->deleteLater();
        auto it = feeds_.find(id);
        if (it == feeds_.end())
            return;
        const FeedSubscription fsub = it->sub;
        if (reply->error() != QNetworkReply::NoError) {
            it->status = FeedStatus::Error;
            emit feed_status(id, FeedStatus::Error, reply->errorString());
            return;
        }
        const QByteArray data = reply->readAll();
        if (looks_like_html(data)) {
            it->status = FeedStatus::Error;
            emit feed_status(id, FeedStatus::Error, QStringLiteral("Access denied / not a feed"));
            return;
        }
        QVector<FeedItem> parsed = FeedScraper::parse(data, fsub);
        if (parsed.isEmpty()) {
            it->status = FeedStatus::Error;
            emit feed_status(id, FeedStatus::Error, QStringLiteral("No items parsed"));
            return;
        }
        // Merge newest-first, dedup by id, cap.
        QVector<FeedItem> merged = parsed;
        for (const auto& old : it->items)
            if (std::none_of(merged.begin(), merged.end(),
                             [&](const FeedItem& x) { return x.id == old.id; }))
                merged.append(old);
        std::stable_sort(merged.begin(), merged.end(),
                         [](const FeedItem& a, const FeedItem& b) { return a.sort_ts > b.sort_ts; });
        if (merged.size() > kMaxItems)
            merged.resize(kMaxItems);
        it->items = merged;
        it->status = FeedStatus::Ok;
        // Persist newly-seen items for offline cache + history (capped per feed).
        if (fsub.persist) {
            const qint64 now_ts = QDateTime::currentSecsSinceEpoch();
            FeedItemRepository::instance().upsert(id, parsed, now_ts);
            FeedItemRepository::instance().prune(id, kMaxHistory);
        }
        emit feed_updated(id, merged);
        emit feed_status(id, FeedStatus::Ok, QStringLiteral("Updated"));
    });
}

void FeedMonitor::add_or_update(const FeedSubscription& sub) {
    FeedSubscriptionRepository::instance().upsert(sub);
    if (sub.enabled) {
        arm_timer(sub);
    } else {
        feeds_[sub.id].sub = sub;
        disarm_timer(sub.id);
    }
    emit subscriptions_changed();
}

void FeedMonitor::remove(const QString& id) {
    FeedSubscriptionRepository::instance().remove(id);
    FeedItemRepository::instance().clear(id); // drop stored history too
    disarm_timer(id);
    feeds_.remove(id);
    emit subscriptions_changed();
}

void FeedMonitor::set_enabled(const QString& id, bool enabled) {
    auto it = feeds_.find(id);
    if (it == feeds_.end())
        return;
    it->sub.enabled = enabled;
    FeedSubscriptionRepository::instance().set_enabled(id, enabled);
    if (enabled)
        arm_timer(it->sub);
    else
        disarm_timer(id);
    emit subscriptions_changed();
}

void FeedMonitor::refresh_now(const QString& id) {
    auto it = feeds_.find(id);
    if (it != feeds_.end())
        fetch(it->sub);
}

void FeedMonitor::pause_all() {
    paused_ = true;
    for (auto& e : feeds_)
        if (e.timer)
            e.timer->stop();
}

void FeedMonitor::resume_all() {
    if (!paused_)
        return;
    paused_ = false;
    for (auto& e : feeds_)
        if (e.sub.enabled && e.timer)
            e.timer->start();
}

void FeedMonitor::fetch_once(const FeedSubscription& sub, PreviewCb cb) {
    auto* reply = nam_->get(build_request(sub));
    connect(reply, &QNetworkReply::finished, this, [reply, sub, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb(false, reply->errorString(), {});
            return;
        }
        const QByteArray data = reply->readAll();
        if (looks_like_html(data)) {
            cb(false, QStringLiteral("Access denied / not a feed"), {});
            return;
        }
        const QVector<FeedItem> items = FeedScraper::parse(data, sub);
        cb(!items.isEmpty(),
           items.isEmpty() ? QStringLiteral("No items parsed") : QString("%1 items").arg(items.size()), items);
    });
}

void FeedMonitor::discover(const FeedSubscription& sub, DiscoverCb cb) {
    auto* reply = nam_->get(build_request(sub));
    connect(reply, &QNetworkReply::finished, this, [reply, sub, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb(false, reply->errorString(), {});
            return;
        }
        const QByteArray data = reply->readAll();
        if (looks_like_html(data)) {
            cb(false, QStringLiteral("Access denied / not a feed"), {});
            return;
        }
        const DiscoveredSchema schema = FeedScraper::discover(data, sub);
        cb(schema.ok, schema.ok ? QString("%1 tags found").arg(schema.fields.size())
                                : QStringLiteral("Couldn't read the feed structure"),
           schema);
    });
}

} // namespace fincept::feeds
