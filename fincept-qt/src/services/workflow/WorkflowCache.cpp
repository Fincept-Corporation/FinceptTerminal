#include "services/workflow/WorkflowCache.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QMutexLocker>

#include <algorithm>

namespace fincept::workflow {

WorkflowCache& WorkflowCache::instance() {
    static WorkflowCache s;
    return s;
}

WorkflowCache::WorkflowCache() : QObject(nullptr) {
    // Cleanup expired entries every 60s
    cleanup_timer_ = new QTimer(this);
    cleanup_timer_->setInterval(60000);
    connect(cleanup_timer_, &QTimer::timeout, this, &WorkflowCache::cleanup_expired);
    cleanup_timer_->start();
}

QJsonValue WorkflowCache::get(const QString& key) const {
    if (!enabled_) {
        misses_++;
        return {};
    }

    QMutexLocker lock(&mutex_);
    auto it = entries_.constFind(key);
    if (it == entries_.constEnd()) {
        misses_++;
        return {};
    }

    // Check TTL
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - it->timestamp_ms > it->ttl_ms) {
        misses_++;
        return {};
    }

    hits_++;
    return it->data;
}

void WorkflowCache::put(const QString& key, const QJsonValue& value, int ttl_ms) {
    if (!enabled_)
        return;

    QMutexLocker lock(&mutex_);

    // Evict oldest if at capacity
    if (entries_.size() >= max_entries_ && !entries_.contains(key)) {
        qint64 oldest_ts = std::numeric_limits<qint64>::max();
        QString oldest_key;
        for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it) {
            if (it->timestamp_ms < oldest_ts) {
                oldest_ts = it->timestamp_ms;
                oldest_key = it.key();
            }
        }
        if (!oldest_key.isEmpty())
            entries_.remove(oldest_key);
    }

    CacheEntry entry;
    entry.data = value;
    entry.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
    entry.ttl_ms = ttl_ms;
    entries_.insert(key, entry);
}

bool WorkflowCache::has(const QString& key) const {
    if (!enabled_)
        return false;

    QMutexLocker lock(&mutex_);
    auto it = entries_.constFind(key);
    if (it == entries_.constEnd())
        return false;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - it->timestamp_ms <= it->ttl_ms);
}

void WorkflowCache::remove(const QString& key) {
    QMutexLocker lock(&mutex_);
    entries_.remove(key);
}

void WorkflowCache::invalidate_prefix(const QString& prefix) {
    QMutexLocker lock(&mutex_);
    QStringList to_remove;
    for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it) {
        if (it.key().startsWith(prefix))
            to_remove.append(it.key());
    }
    for (const auto& key : to_remove)
        entries_.remove(key);

    if (!to_remove.isEmpty())
        LOG_DEBUG("WorkflowCache", QString("Invalidated %1 entries with prefix: %2").arg(to_remove.size()).arg(prefix));
}

void WorkflowCache::clear() {
    QMutexLocker lock(&mutex_);
    int count = entries_.size();
    entries_.clear();
    hits_ = 0;
    misses_ = 0;
    LOG_INFO("WorkflowCache", QString("Cleared %1 entries").arg(count));
}

int WorkflowCache::size() const {
    QMutexLocker lock(&mutex_);
    return entries_.size();
}

double WorkflowCache::hit_rate() const {
    int total = hits_ + misses_;
    return total > 0 ? static_cast<double>(hits_) / total : 0.0;
}

void WorkflowCache::cleanup_expired() {
    QMutexLocker lock(&mutex_);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList expired;

    for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it) {
        if (now - it->timestamp_ms > it->ttl_ms)
            expired.append(it.key());
    }

    for (const auto& key : expired)
        entries_.remove(key);

    if (!expired.isEmpty())
        LOG_DEBUG("WorkflowCache", QString("Cleaned %1 expired entries").arg(expired.size()));
}

} // namespace fincept::workflow
