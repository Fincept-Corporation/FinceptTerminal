#include "services/prediction/PredictionExchangeRegistry.h"

#include "core/logging/Logger.h"
#include "services/prediction/PredictionExchangeAdapter.h"

namespace fincept::services::prediction {

PredictionExchangeRegistry& PredictionExchangeRegistry::instance() {
    static PredictionExchangeRegistry s;
    return s;
}

PredictionExchangeRegistry::PredictionExchangeRegistry() : QObject(nullptr) {}
PredictionExchangeRegistry::~PredictionExchangeRegistry() = default;

void PredictionExchangeRegistry::register_adapter(std::unique_ptr<PredictionExchangeAdapter> adapter) {
    if (!adapter) return;
    const QString id = adapter->id();
    for (const auto& e : entries_) {
        if (e.id == id) {
            LOG_WARN("PredictionRegistry", "Adapter already registered: " + id);
            return;
        }
    }
    if (active_id_.isEmpty()) active_id_ = id;
    entries_.push_back({id, std::move(adapter)});
    LOG_INFO("PredictionRegistry", "Registered adapter: " + id);
}

QStringList PredictionExchangeRegistry::available_ids() const {
    QStringList out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.id);
    return out;
}

PredictionExchangeAdapter* PredictionExchangeRegistry::adapter(const QString& id) const {
    for (const auto& e : entries_) {
        if (e.id == id) return e.adapter.get();
    }
    return nullptr;
}

PredictionExchangeAdapter* PredictionExchangeRegistry::active() const {
    return adapter(active_id_);
}

QString PredictionExchangeRegistry::active_id() const {
    return active_id_;
}

void PredictionExchangeRegistry::set_active(const QString& id) {
    if (id == active_id_) return;
    if (!adapter(id)) {
        LOG_WARN("PredictionRegistry", "Attempt to activate unknown adapter: " + id);
        return;
    }
    active_id_ = id;
    LOG_INFO("PredictionRegistry", "Active exchange: " + id);
    emit active_changed(id);
}

} // namespace fincept::services::prediction
