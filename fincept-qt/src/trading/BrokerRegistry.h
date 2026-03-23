#pragma once
// Broker Registry — factory + lookup for all 16 broker implementations

#include "trading/BrokerInterface.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <unordered_map>

namespace fincept::trading {

// Hash adapter for std::unordered_map<QString,...>
struct QStringHash {
    std::size_t operator()(const QString& s) const { return qHash(s); }
};

class BrokerRegistry {
  public:
    static BrokerRegistry& instance();

    IBroker* get(const QString& broker_id) const;
    IBroker* get(BrokerId id) const;
    QStringList list_brokers() const;
    bool has(const QString& broker_id) const;

    BrokerRegistry(const BrokerRegistry&) = delete;
    BrokerRegistry& operator=(const BrokerRegistry&) = delete;

  private:
    BrokerRegistry();
    ~BrokerRegistry() = default;

    void register_all();
    std::unordered_map<QString, BrokerPtr, QStringHash> brokers_;
};

} // namespace fincept::trading
