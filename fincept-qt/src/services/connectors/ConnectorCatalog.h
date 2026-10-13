#pragma once
// ConnectorCatalog — the single list of Python data connectors.
//
// Loaded (once, lazily, thread-safe) from resources/data_connectors.json, which
// .github/scripts/gen_data_connectors.py generates from fincept-qt/scripts/*.py plus
// scripts/connector_overrides.json. Every consumer reads this one catalogue:
//   - Data Sources › CATALOG tab (browse / add key / run)
//   - the MCP `data_*` tools (MAAnalyticsTools.cpp append_data_connector_tools)
//   - the workflow data-source nodes (DataSourceNodes.cpp)
//   - the Data Sources connection → env-key credential bridge (ConnectorCredentialBridge)
//
// Key status is derived from whether each env key is stored in SecureStorage (or set in the
// process environment). Values are never returned — only presence.

#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services::connectors {

struct ConnectorCommand {
    QString name;
    QString args_hint; ///< e.g. "<series_id> [start_date] [end_date]" — may be empty
};

struct ConnectorInfo {
    QString name;        ///< catalogue id and MCP tool name, e.g. "data_fred"
    QString script;      ///< path relative to the scripts dir, e.g. "fred_data.py"
    QString title;       ///< display name
    QString description; ///< from the script's module docstring (or an override)
    QString category;
    QString region;
    QVector<ConnectorCommand> commands;
    QString default_command;
    QStringList default_args;
    QStringList env_keys;      ///< credential env vars the script reads AND uses
    bool key_required = false; ///< true only when the connector cannot return data without its key
    QString key_note;
    QString docs_url;
    int ttl_sec = 0; ///< cache TTL override; 0 = ConnectorService default
    bool mcp = true; ///< false when a dedicated MCP tool already covers this script
    /// Non-empty when the connector's upstream did not work when last checked (the reason).
    /// Still runnable from the UI (the user may know better); never offered as an MCP tool.
    QString broken;

    bool has_command(const QString& command) const;
    QString args_hint(const QString& command) const;
    QStringList command_names() const;
};

/// Credential state of one connector, computed from env-key presence.
enum class KeyState {
    NotNeeded, ///< keyless connector
    Ready,     ///< every env key is stored
    Partial,   ///< some (but not all) env keys are stored
    Missing,   ///< none of the env keys is stored
    Unknown,   ///< presence not computed yet (see ConnectorService::refresh_key_status)
};

class ConnectorCatalog {
  public:
    static ConnectorCatalog& instance();

    /// Loads the catalogue on first call (any thread). Returns true when a catalogue is
    /// available. A failed load is not retried on every call; see reload().
    bool ensure_loaded();
    /// Drops the loaded catalogue and loads it again (used by tests / after regeneration).
    bool reload();
    QString load_error() const;
    QString source_path() const;

    /// The connectors, in catalogue order (category, then title). Empty until loaded.
    /// The vector is immutable once loaded, so the reference stays valid.
    const QVector<ConnectorInfo>& connectors() const;
    const ConnectorInfo* find(const QString& name) const;
    const ConnectorInfo* find_by_script(const QString& script) const;
    QStringList categories() const;
    int count() const;

    // ── Key presence (never values) ──────────────────────────────────────
    /// Cached presence of one env key; std::nullopt until refresh_key_presence() or
    /// key_present() computed it.
    std::optional<bool> cached_key_present(const QString& env_key) const;
    /// Presence of one env key; computes (SecureStorage lookup) and caches when unknown.
    bool key_present(const QString& env_key);
    /// Recomputes presence for every env key in the catalogue. Blocking (one SecureStorage
    /// read per key) — call off the GUI thread (ConnectorService::refresh_key_status does).
    void refresh_key_presence();
    /// Records a presence change after a key was stored/removed elsewhere.
    void set_key_present(const QString& env_key, bool present);
    /// Uses cached presence only; KeyState::Unknown when a key has not been checked yet.
    KeyState key_state(const ConnectorInfo& connector) const;
    /// All distinct env keys used by the catalogue.
    QStringList all_env_keys() const;

  private:
    ConnectorCatalog() = default;
    bool load_locked();
    bool parse_locked(const QByteArray& json, const QString& source);
    static bool key_in_environment(const QString& env_key);
    static bool key_in_secure_storage(const QString& env_key);

    mutable QMutex mutex_;
    bool attempted_ = false;
    bool loaded_ = false;
    QString load_error_;
    QString source_path_;
    QVector<ConnectorInfo> connectors_;
    QHash<QString, int> by_name_;
    QHash<QString, int> by_script_;
    QStringList categories_;

    mutable QMutex key_mutex_;
    QHash<QString, bool> key_presence_;
};

/// Short UI/MCP wording for a connector's key requirement, e.g.
/// "Requires FRED_API_KEY" / "Optional key: GITHUB_TOKEN (raises rate limits)" / "No API key".
QString key_requirement_text(const ConnectorInfo& connector);

} // namespace fincept::services::connectors
