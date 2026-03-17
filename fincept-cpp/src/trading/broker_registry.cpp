#include "broker_registry.h"
#include "broker_interface.h"
#include "brokers/fyers_broker.h"
#include "brokers/zerodha_broker.h"
#include "brokers/angelone_broker.h"
#include "brokers/upstox_broker.h"
#include "brokers/dhan_broker.h"
#include "brokers/kotak_broker.h"
#include "brokers/groww_broker.h"
#include "brokers/aliceblue_broker.h"
#include "brokers/fivepaisa_broker.h"
#include "brokers/iifl_broker.h"
#include "brokers/motilal_broker.h"
#include "brokers/shoonya_broker.h"
#include "brokers/alpaca_broker.h"
#include "brokers/ibkr_broker.h"
#include "brokers/tradier_broker.h"
#include "brokers/saxobank_broker.h"
#include "brokers/generic_broker.h"
#include "storage/database.h"

namespace fincept::trading {

// ============================================================================
// IBroker credential helpers — load/save from existing DB credential system
// ============================================================================

BrokerCredentials IBroker::load_credentials() const {
    BrokerCredentials creds;
    creds.broker_id = broker_id_str(id());
    auto opt = db::ops::get_credential_by_service(creds.broker_id);
    if (opt) {
        creds.api_key      = opt->api_key.value_or("");
        creds.api_secret   = opt->api_secret.value_or("");
        creds.access_token = opt->password.value_or("");  // stored in password field
        creds.user_id      = opt->username.value_or("");
        creds.additional_data = opt->additional_data.value_or("");
    }
    return creds;
}

void IBroker::save_credentials(const BrokerCredentials& creds) const {
    db::Credential c;
    c.service_name    = creds.broker_id;
    c.api_key         = creds.api_key;
    c.api_secret      = creds.api_secret;
    c.password        = creds.access_token;  // store access_token in password field
    c.username        = creds.user_id;
    c.additional_data = creds.additional_data;
    db::ops::save_credential(c);
}

// ============================================================================
// BrokerRegistry
// ============================================================================

BrokerRegistry& BrokerRegistry::instance() {
    static BrokerRegistry reg;
    return reg;
}

BrokerRegistry::BrokerRegistry() {
    register_all();
}

void BrokerRegistry::register_all() {
    // Fully implemented Indian brokers
    brokers_["fyers"]     = std::make_unique<FyersBroker>();
    brokers_["zerodha"]   = std::make_unique<ZerodhaBroker>();
    brokers_["angelone"]  = std::make_unique<AngelOneBroker>();
    brokers_["upstox"]    = std::make_unique<UpstoxBroker>();
    brokers_["dhan"]      = std::make_unique<DhanBroker>();
    brokers_["kotak"]     = std::make_unique<KotakBroker>();
    brokers_["groww"]     = std::make_unique<GrowwBroker>();
    brokers_["aliceblue"] = std::make_unique<AliceBlueBroker>();
    brokers_["fivepaisa"] = std::make_unique<FivePaisaBroker>();
    brokers_["iifl"]      = std::make_unique<IIFLBroker>();
    brokers_["motilal"]   = std::make_unique<MotilalBroker>();
    brokers_["shoonya"]   = std::make_unique<ShoonyaBroker>();

    // Fully implemented US brokers
    brokers_["alpaca"]    = std::make_unique<AlpacaBroker>();
    brokers_["ibkr"]      = std::make_unique<IBKRBroker>();
    brokers_["tradier"]   = std::make_unique<TradierBroker>();

    // Fully implemented EU brokers
    brokers_["saxobank"]  = std::make_unique<SaxoBankBroker>();
}

IBroker* BrokerRegistry::get(const std::string& broker_id) const {
    auto it = brokers_.find(broker_id);
    return it != brokers_.end() ? it->second.get() : nullptr;
}

IBroker* BrokerRegistry::get(BrokerId id) const {
    return get(broker_id_str(id));
}

std::vector<std::string> BrokerRegistry::list_brokers() const {
    std::vector<std::string> result;
    result.reserve(brokers_.size());
    for (auto& [key, _] : brokers_) result.push_back(key);
    return result;
}

bool BrokerRegistry::has(const std::string& broker_id) const {
    return brokers_.count(broker_id) > 0;
}

} // namespace fincept::trading
