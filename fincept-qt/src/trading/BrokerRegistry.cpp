// Broker Registry — factory + lookup for all 22 broker implementations

#include "trading/BrokerRegistry.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"
#include "storage/sqlite/Database.h"
#include "trading/brokers/aliceblue/AliceBlueBroker.h"
#include "trading/brokers/alpaca/AlpacaBroker.h"
#include "trading/brokers/angelone/AngelOneBroker.h"
#include "trading/brokers/dhan/DhanBroker.h"
#include "trading/brokers/fivepaisa/FivePaisaBroker.h"
#include "trading/brokers/flattrade/FlattradeBroker.h"
#include "trading/brokers/fyers/FyersBroker.h"
#include "trading/brokers/groww/GrowwBroker.h"
#include "trading/brokers/ibkr/IBKRBroker.h"
#include "trading/brokers/icicidirect/IciciDirectBroker.h"
#include "trading/brokers/iifl/IIFLBroker.h"
#include "trading/brokers/kotak/KotakBroker.h"
#include "trading/brokers/metaapi/MetaApiBroker.h"
#include "trading/brokers/tickerall/TickerAllBroker.h"
#include "trading/brokers/motilal/MotilalBroker.h"
#include "trading/brokers/paytm/PaytmBroker.h"
#include "trading/brokers/samco/SamcoBroker.h"
#include "trading/brokers/saxo/SaxoBankBroker.h"
#include "trading/brokers/shoonya/ShoonyaBroker.h"
#include "trading/brokers/tradejini/TradejiniBroker.h"
#include "trading/brokers/tradier/TradierBroker.h"
#include "trading/brokers/upstox/UpstoxBroker.h"
#include "trading/brokers/zerodha/ZerodhaBroker.h"

#include <QCoreApplication>

#include <utility>

namespace fincept::trading {

// ============================================================================
// IBroker credential helpers
// ============================================================================

// For brokers with native paper trading (has_native_paper=true), credentials are stored
// with an env suffix: broker.{id}.live.* and broker.{id}.paper.*
// For all other brokers: broker.{id}.*
// The env is determined by additional_data ("live"/"paper"), defaulting to "live".
static QString cred_prefix(const QString& broker_id, const QString& additional_data, bool has_native_paper) {
    if (!has_native_paper)
        return QString("broker.%1.").arg(broker_id);
    const QString env = (additional_data == "paper") ? "paper" : "live";
    return QString("broker.%1.%2.").arg(broker_id, env);
}

BrokerCredentials IBroker::load_credentials(QString* read_error) const {
    return load_credentials_for_env("", read_error);
}

BrokerCredentials IBroker::load_credentials_for_env(const QString& env, QString* read_error) const {
    BrokerCredentials creds;
    creds.broker_id = broker_id_str(id());
    const bool hnp = profile().has_native_paper;
    // env="" means: try to find whichever env has a key stored (live first)
    auto& secure = SecureStorage::instance();
    QString failure; // first unreadable field: the whole load is reported as failed
    QString failed_key;

    // true = this env decided the answer (a key is stored, or a read failed) — stop there.
    auto try_load = [&](const QString& e) {
        QString prefix = cred_prefix(creds.broker_id, e, hnp);
        auto fail = [&](const SecretRead& r, const QString& key) {
            if (failure.isEmpty()) {
                failure = QCoreApplication::translate("IBroker", "Could not read the stored credential: %1.")
                              .arg(r.reason());
                failed_key = key;
            }
        };
        const SecretRead key_r = secure.read(prefix + "api_key");
        if (key_r.failed()) {
            // Unknown whether this env holds a key: never fall through to the other
            // env's credentials (live unreadable must not quietly become paper).
            fail(key_r, prefix + "api_key");
            return true;
        }
        if (!key_r.found())
            return false;
        creds.api_key = key_r.value;
        const SecretRead secret_r = secure.read(prefix + "api_secret");
        const SecretRead token_r = secure.read(prefix + "access_token");
        const SecretRead user_r = secure.read(prefix + "user_id");
        const SecretRead extra_r = secure.read(prefix + "additional_data");
        for (const auto& [r, name] : {std::pair<const SecretRead*, const char*>{&secret_r, "api_secret"},
                                      {&token_r, "access_token"},
                                      {&user_r, "user_id"},
                                      {&extra_r, "additional_data"}}) {
            if (r->failed())
                fail(*r, prefix + QLatin1String(name));
        }
        creds.api_secret = secret_r.value;
        creds.access_token = token_r.value;
        creds.user_id = user_r.value;
        creds.additional_data = extra_r.value;
        return true;
    };

    if (!env.isEmpty()) {
        try_load(env);
    } else if (hnp) {
        // Try live first, then paper
        if (!try_load("live"))
            try_load("paper");
    } else {
        try_load("");
    }

    const bool failing = !failure.isEmpty();
    LOG_TRANSITION("BrokerRegistry", QStringLiteral("cred_read|") + creds.broker_id, failing,
                   failing ? LogLevel::Warn : LogLevel::Info,
                   failing ? logf("Broker credentials could not be read; returned empty, nothing written",
                                  {{"broker", creds.broker_id}, {"key", failed_key}})
                           : logf("Broker credentials readable again", {{"broker", creds.broker_id}}));
    if (failing) {
        // Partial credentials are worse than none: a caller would act on a key without
        // its secret / token. Empty set + the reason.
        const QString broker_id = creds.broker_id;
        creds = BrokerCredentials{};
        creds.broker_id = broker_id;
    }
    if (read_error)
        *read_error = failure;
    return creds;
}

void IBroker::save_credentials(const BrokerCredentials& creds) const {
    const bool hnp = profile().has_native_paper;
    auto& secure = SecureStorage::instance();
    QString prefix = cred_prefix(creds.broker_id, creds.additional_data, hnp);
    secure.store(prefix + "api_key", creds.api_key);
    secure.store(prefix + "api_secret", creds.api_secret);
    secure.store(prefix + "access_token", creds.access_token);
    secure.store(prefix + "user_id", creds.user_id);
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
    brokers_["fyers"] = std::make_unique<FyersBroker>();
    brokers_["zerodha"] = std::make_unique<ZerodhaBroker>();
    brokers_["angelone"] = std::make_unique<AngelOneBroker>();
    brokers_["upstox"] = std::make_unique<UpstoxBroker>();
    brokers_["dhan"] = std::make_unique<DhanBroker>();
    brokers_["kotak"] = std::make_unique<KotakBroker>();
    brokers_["groww"] = std::make_unique<GrowwBroker>();
    brokers_["aliceblue"] = std::make_unique<AliceBlueBroker>();
    brokers_["fivepaisa"] = std::make_unique<FivePaisaBroker>();
    brokers_["iifl"] = std::make_unique<IIFLBroker>();
    brokers_["motilal"] = std::make_unique<MotilalBroker>();
    brokers_["shoonya"] = std::make_unique<ShoonyaBroker>();
    brokers_["samco"] = std::make_unique<SamcoBroker>();
    brokers_["flattrade"] = std::make_unique<FlattradeBroker>();
    brokers_["paytm"] = std::make_unique<PaytmBroker>();
    brokers_["tradejini"] = std::make_unique<TradejiniBroker>();
    brokers_["icicidirect"] = std::make_unique<IciciDirectBroker>();

    // US brokers
    brokers_["alpaca"] = std::make_unique<AlpacaBroker>();
    brokers_["ibkr"] = std::make_unique<IBKRBroker>();
    brokers_["tradier"] = std::make_unique<TradierBroker>();

    // EU brokers
    brokers_["saxobank"] = std::make_unique<SaxoBankBroker>();

    // Hosted MetaTrader bridges — no local terminal required
    brokers_["metatrader4"] = std::make_unique<MetaApiBroker>();   // via metaapi.cloud
    brokers_["metatrader5"] = std::make_unique<TickerAllBroker>(); // via tickerall.com

    LOG_DEBUG("BrokerRegistry", QString("Registered %1 brokers").arg(brokers_.size()));
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
    for (const auto& [key, _] : brokers_)
        result.append(key);
    return result;
}

bool BrokerRegistry::has(const QString& broker_id) const {
    return brokers_.count(broker_id) > 0;
}

} // namespace fincept::trading
