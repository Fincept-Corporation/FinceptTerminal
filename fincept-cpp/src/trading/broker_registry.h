#pragma once
// Broker Registry — factory + lookup for all 16 broker implementations
// Usage:
//   auto& reg = BrokerRegistry::instance();
//   auto* broker = reg.get("fyers");
//   auto result = broker->place_order(creds, order);

#include "broker_interface.h"
#include <unordered_map>
#include <memory>

namespace fincept::trading {

class BrokerRegistry {
public:
    static BrokerRegistry& instance();

    // Get a broker by string ID (returns nullptr if not registered)
    IBroker* get(const std::string& broker_id) const;

    // Get a broker by enum ID
    IBroker* get(BrokerId id) const;

    // Get all registered broker IDs
    std::vector<std::string> list_brokers() const;

    // Check if a broker is registered
    bool has(const std::string& broker_id) const;

    BrokerRegistry(const BrokerRegistry&) = delete;
    BrokerRegistry& operator=(const BrokerRegistry&) = delete;

private:
    BrokerRegistry();
    ~BrokerRegistry() = default;

    void register_all();

    std::unordered_map<std::string, BrokerPtr> brokers_;
};

} // namespace fincept::trading
