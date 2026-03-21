// Broker Registry — factory + lookup for all 16 broker implementations

#include "trading/BrokerRegistry.h"
#include "trading/brokers/GenericBroker.h"
#include "trading/brokers/FyersBroker.h"
#include "trading/brokers/ZerodhaBroker.h"
#include "trading/brokers/AngelOneBroker.h"
#include "trading/brokers/UpstoxBroker.h"
#include "trading/brokers/DhanBroker.h"
#include "trading/brokers/KotakBroker.h"
#include "trading/brokers/GrowwBroker.h"
#include "trading/brokers/AliceBlueBroker.h"
#include "trading/brokers/FivePaisaBroker.h"
#include "trading/brokers/IIFLBroker.h"
#include "trading/brokers/MotilalBroker.h"
#include "trading/brokers/ShoonyaBroker.h"
#include "trading/brokers/AlpacaBroker.h"
#include "trading/brokers/IBKRBroker.h"
#include "trading/brokers/TradierBroker.h"
#include "trading/brokers/SaxoBankBroker.h"
#include "storage/sqlite/Database.h"
#include "storage/secure/SecureStorage.h"
#include "core/logging/Logger.h"

namespace fincept::trading {

// ============================================================================
// IBroker credential helpers
// ============================================================================

BrokerCredentials IBroker::load_credentials() const {
    BrokerCredentials creds;
    creds.broker_id = broker_id_str(id());
    auto& secure = SecureStorage::instance();
    QString prefix = QString("broker.%1.").arg(creds.broker_id);
    auto key_r    = secure.retrieve(prefix + "api_key");
    auto secret_r = secure.retrieve(prefix + "api_secret");
    auto token_r  = secure.retrieve(prefix + "access_token");
    auto user_r   = secure.retrieve(prefix + "user_id");
    auto extra_r  = secure.retrieve(prefix + "additional_data");

    if (key_r.is_ok())    creds.api_key         = key_r.value();
    if (secret_r.is_ok()) creds.api_secret      = secret_r.value();
    if (token_r.is_ok())  creds.access_token    = token_r.value();
    if (user_r.is_ok())   creds.user_id         = user_r.value();
    if (extra_r.is_ok())  creds.additional_data = extra_r.value();
    return creds;
}

void IBroker::save_credentials(const BrokerCredentials& creds) const {
    auto& secure = SecureStorage::instance();
    QString prefix = QString("broker.%1.").arg(creds.broker_id);
    secure.store(prefix + "api_key",         creds.api_key);
    secure.store(prefix + "api_secret",      creds.api_secret);
    secure.store(prefix + "access_token",    creds.access_token);
    secure.store(prefix + "user_id",         creds.user_id);
    secure.store(prefix + "additional_data", creds.additional_data);
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
    // Indian brokers
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

    // US brokers
    brokers_["alpaca"]    = std::make_unique<AlpacaBroker>();
    brokers_["ibkr"]      = std::make_unique<IBKRBroker>();
    brokers_["tradier"]   = std::make_unique<TradierBroker>();

    // EU brokers
    brokers_["saxobank"]  = std::make_unique<SaxoBankBroker>();

    LOG_INFO("BrokerRegistry", QString("Registered %1 brokers").arg(brokers_.size()));
}

IBroker* BrokerRegistry::get(const QString& broker_id) const {
    auto it = brokers_.find(broker_id);
    return it != brokers_.end() ? it->second.get() : nullptr;
}

IBroker* BrokerRegistry::get(BrokerId id) const {
    return get(QString(broker_id_str(id)));
}

QStringList BrokerRegistry::list_brokers() const {
    QStringList result;
    for (const auto& [key, _] : brokers_) result.append(key);
    return result;
}

bool BrokerRegistry::has(const QString& broker_id) const {
    return brokers_.count(broker_id) > 0;
}

} // namespace fincept::trading
