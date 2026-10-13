#pragma once
// ConnectorCredentialBridge — Data Sources connections → Python connector API keys.
//
// The terminal has two credential stores: Data Sources connections (encrypted per connection
// under `datasource:cfg:<id>`) and the env-named keys in SecureStorage that PythonRunner
// injects into every Python process (Settings › Credentials, Data Sources › Catalog). A
// Finnhub / Polygon / Alpha Vantage key typed into a Data Sources connection used to stay in
// the first store, so the Python connectors never saw it.
//
// DataSourceRepository::save() calls bridge_connection_credentials() after a successful save:
// when the connection's provider maps to a connector env key and that env key is still EMPTY,
// the key is copied there. An existing env key is never overwritten (whatever the user set in
// Settings › Credentials wins). Values are never logged.

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::connectors {

/// Env keys a Data Sources connection type can supply, as {config field, env key} pairs.
struct ConnectionKeyMapping {
    const char* field;   ///< field name in the connection config (connectors/*.cpp)
    const char* env_key; ///< SecureStorage / env-var name the Python connector reads
};

/// Mappings for one Data Sources connection type id (e.g. "finnhub"); empty when none.
QVector<ConnectionKeyMapping> connection_key_mappings(const QString& provider);

/// Copies the connection's API key(s) into empty env keys. `config_json` is the full
/// connection config (secrets included). Returns the env keys that were filled.
QStringList bridge_connection_credentials(const QString& provider, const QString& config_json);

} // namespace fincept::services::connectors
