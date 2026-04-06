// src/services/workflow/WorkflowCache.cpp
// Thin wrapper over CacheManager — preserves the original public API.
#include "services/workflow/WorkflowCache.h"

#include "core/logging/Logger.h"
#include "storage/cache/CacheManager.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::workflow {

static constexpr const char* kCategory = "workflow";
// Key prefix so workflow entries are namespaced inside CacheManager
static constexpr const char* kPrefix = "wf:";

WorkflowCache& WorkflowCache::instance() {
    static WorkflowCache s;
    return s;
}

WorkflowCache::WorkflowCache(QObject* parent) : QObject(parent) {}

// ── Read ──────────────────────────────────────────────────────────────────────

QJsonValue WorkflowCache::get(const QString& key) const {
    if (!enabled_) {
        misses_++;
        return {};
    }
    const QVariant cv = fincept::CacheManager::instance().get(kPrefix + key);
    if (cv.isNull()) {
        misses_++;
        return {};
    }
    hits_++;
    // Values are stored as compact JSON strings
    const QByteArray raw = cv.toString().toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isArray())   return doc.array();
    if (doc.isObject())  return doc.object();
    // Scalar: stored as {"v": <value>}
    return doc.object()["v"];
}

bool WorkflowCache::has(const QString& key) const {
    if (!enabled_) return false;
    return fincept::CacheManager::instance().has(kPrefix + key);
}

// ── Write ─────────────────────────────────────────────────────────────────────

void WorkflowCache::put(const QString& key, const QJsonValue& value, int ttl_ms) {
    if (!enabled_) return;

    // Serialize the QJsonValue to a string CacheManager can store
    QString serialized;
    if (value.isArray()) {
        serialized = QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    } else if (value.isObject()) {
        serialized = QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    } else {
        // Scalar — wrap in {"v": <value>}
        QJsonObject wrapper;
        wrapper["v"] = value;
        serialized = QString::fromUtf8(QJsonDocument(wrapper).toJson(QJsonDocument::Compact));
    }

    const int ttl_sec = std::max(1, ttl_ms / 1000);
    fincept::CacheManager::instance().put(kPrefix + key, QVariant(serialized), ttl_sec, kCategory);
}

// ── Remove ────────────────────────────────────────────────────────────────────

void WorkflowCache::remove(const QString& key) {
    fincept::CacheManager::instance().remove(kPrefix + key);
}

void WorkflowCache::invalidate_prefix(const QString& prefix) {
    fincept::CacheManager::instance().remove_prefix(kPrefix + prefix);
    LOG_DEBUG("WorkflowCache", "Invalidated prefix: " + prefix);
}

void WorkflowCache::clear() {
    fincept::CacheManager::instance().clear_category(kCategory);
    hits_   = 0;
    misses_ = 0;
    LOG_INFO("WorkflowCache", "Cleared all workflow cache entries");
}

// ── Stats ─────────────────────────────────────────────────────────────────────

int WorkflowCache::size() const {
    // CacheManager has no per-category count — return total entries as proxy
    return fincept::CacheManager::instance().entry_count();
}

double WorkflowCache::hit_rate() const {
    const int total = hits_ + misses_;
    return total > 0 ? static_cast<double>(hits_) / total : 0.0;
}

} // namespace fincept::workflow
