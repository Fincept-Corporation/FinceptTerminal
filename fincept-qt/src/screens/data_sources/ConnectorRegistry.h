#pragma once
// ConnectorRegistry — singleton holding all 78 data source connector definitions.
// Connector files self-register via static init.

#include "screens/data_sources/DataSourceTypes.h"

#include <QMap>
#include <QMutex>
#include <QVector>

namespace fincept::screens::datasources {

class ConnectorRegistry {
  public:
    static ConnectorRegistry& instance() {
        static ConnectorRegistry s;
        return s;
    }

    void add(ConnectorConfig cfg) {
        QMutexLocker lock(&mutex_);
        by_id_[cfg.id] = configs_.size();
        configs_.append(std::move(cfg));
    }

    const QVector<ConnectorConfig>& all() const { return configs_; }

    const ConnectorConfig* get(const QString& id) const {
        auto it = by_id_.find(id);
        if (it == by_id_.end()) return nullptr;
        return &configs_[it.value()];
    }

    QVector<ConnectorConfig> by_category(Category cat) const {
        QVector<ConnectorConfig> result;
        for (const auto& c : configs_)
            if (c.category == cat) result.append(c);
        return result;
    }

    int count() const { return configs_.size(); }

  private:
    ConnectorRegistry() = default;
    QVector<ConnectorConfig> configs_;
    QMap<QString, int> by_id_;
    QMutex mutex_;
};

// Force-include all connector registration units
void register_all_connectors();

} // namespace fincept::screens::datasources
