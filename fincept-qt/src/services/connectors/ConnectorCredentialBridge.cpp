// ConnectorCredentialBridge.cpp — see ConnectorCredentialBridge.h.

#include "services/connectors/ConnectorCredentialBridge.h"

#include "core/logging/Logger.h"
#include "services/connectors/ConnectorCatalog.h"
#include "storage/secure/SecureStorage.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>

namespace fincept::services::connectors {

namespace {

constexpr const char* kConnBridgeTag = "ConnectorCredentials";

struct ConnBridgeEntry {
    const char* provider;
    ConnectionKeyMapping mapping;
};

// Data Sources connection field -> env key read by the matching scripts/*.py connector.
// Field names come from src/screens/data_sources/connectors/*.cpp; env keys are the ones the
// scripts read (resources/data_connectors.json) or that PythonRunner manages by hand.
constexpr ConnBridgeEntry kConnBridgeTable[] = {
    {"alpha-vantage", {"apiKey", "ALPHA_VANTAGE_API_KEY"}}, // alphavantage_data.py, alpha_vantage_extra_data.py
    {"finnhub", {"apiKey", "FINNHUB_API_KEY"}},             // finnhub_data.py
    {"polygon-io", {"apiKey", "POLYGON_API_KEY"}},          // polygon_io_data.py
    {"tiingo", {"apiToken", "TIINGO_API_KEY"}},             // tiingo_data.py
    {"twelve-data", {"apiKey", "TWELVE_DATA_API_KEY"}},     // twelve_data.py
    {"intrinio", {"apiKey", "INTRINIO_API_KEY"}},           // intrinio_data.py
    {"marketstack", {"apiKey", "MARKETSTACK_API_KEY"}},     // marketstack_data.py
    {"openfigi", {"apiKey", "OPENFIGI_API_KEY"}},           // openfigi_data.py
    {"coinmarketcap", {"apiKey", "CMC_API_KEY"}},           // coinmarketcap_data.py
    {"coingecko", {"apiKey", "COINGECKO_API_KEY"}},         // coingecko.py (Pro key)
    {"eod-historical", {"apiToken", "EODHD_API_KEY"}},      // eodhd_data.py
    {"quandl", {"apiKey", "NASDAQ_DATA_LINK_API_KEY"}},     // quandl_nasdaq_data.py, quandl_wiki_data.py
    {"quandl", {"apiKey", "QUANDL_API_KEY"}},               // managed key, older scripts
    {"newsapi", {"apiKey", "NEWSAPI_KEY"}},                 // managed key
};

bool conn_bridge_env_key_empty(const QString& env_key) {
    const auto r = SecureStorage::instance().retrieve(env_key);
    return !(r.is_ok() && !r.value().trimmed().isEmpty()); // value discarded immediately
}

} // namespace

QVector<ConnectionKeyMapping> connection_key_mappings(const QString& provider) {
    QVector<ConnectionKeyMapping> out;
    for (const auto& e : kConnBridgeTable)
        if (provider == QLatin1String(e.provider))
            out.append(e.mapping);
    return out;
}

QStringList bridge_connection_credentials(const QString& provider, const QString& config_json) {
    QStringList filled;
    const QVector<ConnectionKeyMapping> mappings = connection_key_mappings(provider);
    if (mappings.isEmpty())
        return filled;
    const QJsonDocument doc = QJsonDocument::fromJson(config_json.toUtf8());
    if (!doc.isObject())
        return filled;
    const QJsonObject cfg = doc.object();

    for (const auto& m : mappings) {
        const QString value = cfg.value(QLatin1String(m.field)).toString().trimmed();
        if (value.isEmpty())
            continue;
        const QString env_key = QString::fromLatin1(m.env_key);
        if (!conn_bridge_env_key_empty(env_key)) {
            LOG_DEBUG(kConnBridgeTag,
                      QString("%1 already set — connection key for %2 not copied").arg(env_key, provider));
            continue;
        }
        const auto r = SecureStorage::instance().store(env_key, value);
        if (r.is_err()) {
            LOG_WARN(kConnBridgeTag, QString("Could not copy the %1 connection key to %2").arg(provider, env_key));
            continue;
        }
        ConnectorCatalog::instance().set_key_present(env_key, true);
        filled << env_key;
        LOG_INFO(kConnBridgeTag,
                 QString("Copied the %1 connection's API key to %2 so the Python data connectors can use it")
                     .arg(provider, env_key));
    }
    return filled;
}

} // namespace fincept::services::connectors
