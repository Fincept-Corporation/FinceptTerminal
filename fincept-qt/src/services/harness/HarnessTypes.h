#pragma once
// HarnessTypes.h — value types shared by the agent-harness layer
// (src/services/harness): the adapters (one per CLI agent: Claude Code, Codex, …),
// detection, launch and HarnessRegistry.
//
// Plain data, QtCore only — no widgets, logger, AppConfig or network — so the
// adapters and the detection logic can be listed explicitly by a unit test.
//
// Credential rule for everything in this layer: a harness credential is never
// read, stored, logged or transmitted. Allowed inputs are status commands,
// environment-variable NAMES and file EXISTENCE.

#include "core/config/EditionNames.h"

#include <QByteArray>
#include <QHash>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::harness {

/// The environment variable that carries the terminal's MCP bearer token to a
/// harness child — never argv, never disk. Edition-specific
/// (FINCEPT_COMMUNITY_MCP_TOKEN, EditionNames.h): the private build sets its own
/// token in FINCEPT_MCP_TOKEN, which child_environment() removes. Equal to
/// mcp::McpHttpServer::kTokenEnvVar (a static_assert in
/// CodingAgentService_Process.cpp keeps the two in step).
inline constexpr const char* kMcpTokenEnvVar = edition::kMcpTokenEnvVar;

/// The entry name of this edition's MCP server in harness configs, per-run and
/// registered ("fincept_community", EditionIdentity.h).
inline constexpr const char* kMcpServerName = edition::kMcpServerName;

/// Stands for the MCP bearer token in the copy-paste snippets an adapter
/// describes (McpRegistration::manual_args / config_snippet). The UI replaces it
/// with the masked or the real token; an adapter never sees the token itself.
inline constexpr const char kMcpTokenPlaceholder[] = "<FINCEPT_COMMUNITY_MCP_TOKEN>";

enum class Transport { PrintJsonl, Acp, PlainText };
enum class PermissionMode { ReadOnly, WorkspaceWrite, FullAccess };
enum class AuthState { Unknown, SignedIn, SignedOut, Error };
enum class FailureKind { None, Auth, RateLimit, NotInstalled, Timeout, Model, Other };

// ── Stable string ids (persisted: coding_agent_runs.permission_mode) ─────────
inline QString permission_id(PermissionMode m) {
    switch (m) {
        case PermissionMode::WorkspaceWrite:
            return QStringLiteral("workspace_write");
        case PermissionMode::FullAccess:
            return QStringLiteral("full_access");
        case PermissionMode::ReadOnly:
            break;
    }
    return QStringLiteral("read_only");
}

/// Unknown or empty id → nullopt (the caller decides the fallback).
inline std::optional<PermissionMode> permission_from_id(const QString& id) {
    if (id == QLatin1String("read_only"))
        return PermissionMode::ReadOnly;
    if (id == QLatin1String("workspace_write"))
        return PermissionMode::WorkspaceWrite;
    if (id == QLatin1String("full_access"))
        return PermissionMode::FullAccess;
    return std::nullopt;
}

/// "auth" | "rate_limit" | "not_installed" | "timeout" | "model" | "other" | "" (None).
inline QString failure_id(FailureKind k) {
    switch (k) {
        case FailureKind::Auth:
            return QStringLiteral("auth");
        case FailureKind::RateLimit:
            return QStringLiteral("rate_limit");
        case FailureKind::NotInstalled:
            return QStringLiteral("not_installed");
        case FailureKind::Timeout:
            return QStringLiteral("timeout");
        case FailureKind::Model:
            return QStringLiteral("model");
        case FailureKind::Other:
            return QStringLiteral("other");
        case FailureKind::None:
            break;
    }
    return {};
}

inline FailureKind failure_from_id(const QString& id) {
    if (id == QLatin1String("auth"))
        return FailureKind::Auth;
    if (id == QLatin1String("rate_limit"))
        return FailureKind::RateLimit;
    if (id == QLatin1String("not_installed"))
        return FailureKind::NotInstalled;
    if (id == QLatin1String("timeout"))
        return FailureKind::Timeout;
    if (id == QLatin1String("model"))
        return FailureKind::Model;
    if (id == QLatin1String("other"))
        return FailureKind::Other;
    return FailureKind::None;
}

/// What HarnessRegistry::refresh() re-runs. Paths implies Version (a new path
/// search always re-validates the binary it finds).
enum class RefreshScope : unsigned {
    Paths = 0x1,
    Version = 0x2,
    Auth = 0x4,
    Models = 0x8,
    Detect = 0x3, // Paths | Version
    All = 0xF,
};
constexpr RefreshScope operator|(RefreshScope a, RefreshScope b) {
    return static_cast<RefreshScope>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr bool scope_has(RefreshScope scope, RefreshScope part) {
    return (static_cast<unsigned>(scope) & static_cast<unsigned>(part)) != 0;
}

struct Capabilities {
    bool structured_stream = false;  // one JSON event per stdout line
    bool text_deltas = false;        // token streaming
    bool thinking = false;
    bool resume = false;
    bool per_run_mcp = false;        // MCP servers can be given per run (flag / temp config)
    bool mcp_http = false;           // streamable-HTTP MCP transport
    bool enforced_read_only = false; // read-only is enforced by the harness on this OS
    bool model_list = false;         // the CLI can list its models
    bool effort = false;             // reasoning effort can be chosen per run
    bool budget_usd = false;
    bool max_turns = false;
    bool json_schema = false;
    bool system_prompt = false;      // a system-prompt flag exists (else the context is wrapped into the prompt)
    bool persistent_session = false;
    bool usage_tokens = false;
    bool cost_usd = false;
    bool full_access = false;        // PermissionMode::FullAccess can be launched (no sandbox, no approvals)
    /// RunSpec::chat is supported: a terminal-AI call with no built-in tools and
    /// an enforced read-only mode (the terminal's MCP tools only). Adapters
    /// written from documentation keep this off until their read-only mode is
    /// verified.
    bool chat = false;
};

struct HarnessDescriptor {
    QString id;           // stable, persisted in coding_agent_runs.agent ("claude", "codex")
    QString display_name; // "Claude Code"
    QString vendor;
    QString install_url;     // vendor page: how to install
    QString install_command; // one command the user can copy ("npm install -g …"); empty = see install_url
    QString docs_url;
    QString auth_docs_url;   // vendor page: how to sign in; empty = docs_url
    QString signin_hint; // what the user does to sign in; the terminal never signs in and never asks for a key
    QString signin_command;  // the command the user can copy ("codex login"); empty = none
    QStringList executable_names; // without extension
    Transport transport = Transport::PrintJsonl;
    Capabilities caps;
    /// API-key variables this harness honours. Their NAMES drive the "API key in
    /// the environment" warning; env policy "subscription only" strips them from
    /// the child. Values are never read.
    QStringList api_key_env_vars;
    /// The subset of api_key_env_vars that a HEADLESS run is documented to
    /// authenticate with whenever the variable is set, even when a personal
    /// sign-in also exists (Claude Code -p: ANTHROPIC_API_KEY; codex exec:
    /// CODEX_API_KEY). One of these reaching the child is what makes the
    /// harness usable for terminal AI without an API-key sign-in (owner
    /// decision: terminal AI never runs on a consumer subscription). NAMES only.
    QStringList terminal_key_env_vars;
    /// Variables removed from the child of a terminal-AI call: credentials of a
    /// personal subscription that must never serve terminal AI
    /// (CLAUDE_CODE_OAUTH_TOKEN). NAMES only.
    QStringList terminal_unset_env_vars;
    /// Vendor page that explains API-key authentication for this CLI (shown
    /// with "Workbench only — sign in with an API key …"). Empty = auth_docs_url.
    QString api_key_docs_url;
    /// The terminal supports this harness with API-key authentication only (its
    /// vendor does not permit third-party apps to drive a personal sign-in):
    /// runs need one of api_key_env_vars, and env policy "subscription only" does
    /// not apply.
    bool api_key_only = false;
    /// Name the terminal's MCP server gets inside a run (also used in the system
    /// prompt). Distinct per harness where the user's own config could collide.
    QString run_mcp_server_name;
    /// Written from documentation (or --help only) and not yet checked against a
    /// recorded run: shown with a BETA tag.
    bool beta = false;
    QString beta_note; // what is unverified, one sentence
    /// Newest CLI version a transcript fixture was recorded with; a newer
    /// installed version is shown as "newer than tested". Empty = none recorded.
    QString tested_version;
};

/// Everything detection needs from the outside world, injected so tests can
/// describe a fake file system. make_system_detect_context() builds the real one.
struct DetectContext {
    QString home;
    QProcessEnvironment env;
    QString override_path; // user-configured path; empty = auto-detect
#ifdef Q_OS_WIN
    bool windows = true; // search rules (PATHEXT, .cmd shims, ';' separators) — injectable for tests
#else
    bool windows = false;
#endif
    /// Regular file exists (no permission check).
    std::function<bool(const QString&)> is_file;
    /// Regular file that may be executed (POSIX: x bit). Unset = is_file.
    std::function<bool(const QString&)> is_executable;
    /// Names of the sub-directories of `dir` matching `glob` (e.g. "v*").
    std::function<QStringList(const QString& dir, const QString& glob)> list_dir;
    /// At most `max_bytes` of a file (npm .cmd shims only). Unset = no shim resolution.
    std::function<QByteArray(const QString& path, qint64 max_bytes)> read_file;

    /// Environment lookup; case-insensitive for Windows rules (PATH vs Path).
    QString env_value(const QString& name) const {
        if (env.contains(name))
            return env.value(name);
        if (windows) {
            const QStringList keys = env.keys();
            for (const QString& k : keys) {
                if (k.compare(name, Qt::CaseInsensitive) == 0)
                    return env.value(k);
            }
        }
        return {};
    }
    /// The variable is set (its NAME only — used for API-key presence, never the value).
    bool has_env(const QString& name) const {
        if (env.contains(name))
            return true;
        if (windows) {
            const QStringList keys = env.keys();
            for (const QString& k : keys) {
                if (k.compare(name, Qt::CaseInsensitive) == 0)
                    return true;
            }
        }
        return false;
    }
    bool executable(const QString& path) const {
        if (is_executable)
            return is_executable(path);
        return is_file && is_file(path);
    }
};

struct Installation {
    enum Via { Native, CmdShim, NodeScript, Wsl };
    QString path;        // the file detection found (exe, or the npm .cmd shim) — what the user sees
    Via via = Native;
    QString node_path;   // NodeScript: the node executable that runs script_path
    QString script_path; // NodeScript: the JS entry point the npm shim would run
    QString wsl_distro;  // Wsl (not supported yet)
    QString version;     // "2.1.325"; empty = unknown
    QString version_line; // the `--version` line that identified the harness
    bool path_overridden = false;
    // Of the file that changes on update (exe / script / shim) — validates the cache.
    qint64 file_size = -1;
    qint64 file_mtime_ms = 0;
};

struct ProbeCommand {
    QStringList args;
    int timeout_ms = 20000;
};

struct AuthStatus {
    AuthState state = AuthState::Unknown;
    QString method; // subscription | api_key | cloud | env | "" (unknown)
    QString plan;   // subscription tier when reported ("max", "pro", …)
    QString note;   // short, non-identifying detail for the UI
    bool api_key_env_present = false; // an API-key variable is set (name only, never the value)
    qint64 checked_at_ms = 0;
};

struct ModelInfo {
    QString id;
    QString label;
    QString description;
    bool is_default = false;
    QStringList efforts;
    QString default_effort;
    qint64 context_window = 0;
};

struct McpEndpoint {
    QString name;
    QString url;
    QString token_env_var = QString::fromLatin1(kMcpTokenEnvVar);
    int tool_timeout_s = 300;
};

/// One run, as the service asks for it. The adapter turns it into a LaunchPlan.
struct RunSpec {
    QString prompt;
    QString system_prompt;
    QString model;             // empty = the CLI's default
    QString effort;            // empty = the CLI's default (no flag is passed)
    QString cwd;
    QString resume_session_id; // non-empty = continue that session
    QString json_schema;
    PermissionMode permission = PermissionMode::ReadOnly;
    std::optional<McpEndpoint> mcp;
    double max_budget_usd = 0;
    int max_turns = 0;
    /// A terminal-AI call (caps.chat): no built-in tools, read-only, the
    /// terminal's MCP server only, a fresh empty working folder. Adapters that
    /// cannot enforce that refuse the spec.
    bool chat = false;
    /// chat only: keep the harness session so a later call can resume it (Chat
    /// Mode tasks). Off = ephemeral (AI Chat, Quick Ask, arena: stateless).
    bool chat_persist_session = false;
    bool want_deltas = false;  // ask for token-level text deltas (caps.text_deltas)
    QString run_id;   // names per-run temporary files
    QString temp_dir; // private folder the service writes LaunchPlan::temp_files into
    /// An API-key variable of this harness is set in the child's environment
    /// (after the env policy) — NAMES only. Lets an api_key_only adapter refuse
    /// a run that would otherwise fall back to a personal sign-in.
    bool api_key_env_present = false;
    /// MCP server entry NAMES the harness would load from the user's own config
    /// for this run (Codex: the [mcp_servers.<name>] tables of config.toml) —
    /// names only, never values. An adapter whose CLI merges per-run servers
    /// into the user's table disables these so the run sees the terminal's
    /// server only (as Claude Code's --strict-mcp-config does).
    QStringList user_mcp_server_names;
};

/// A file the service writes to RunSpec::temp_dir before the start and deletes
/// with the run. Never contains a secret (tokens travel through the environment).
struct TempFile {
    QString name; // file name inside RunSpec::temp_dir
    QByteArray content;
};

struct LaunchPlan {
    QStringList args;          // after the program (for NodeScript: after the script)
    QByteArray stdin_payload;  // written once the process runs
    bool keep_stdin_open = false;
    QHash<QString, QString> env_set;
    QStringList env_unset;
    QVector<TempFile> temp_files;
    QStringList redact;        // values never shown or logged verbatim
    QString system_prompt_arg; // the argument that carries the system prompt (shown as its length only)
    QString prompt_arg;        // the argument that carries the user prompt, when not on stdin (shown as its length only)
    QString error;             // non-empty = this run cannot be launched
};

struct CancelPlan {
    bool close_stdin = true;
    bool interrupt_first = false;
    int grace_ms = 3000;
};

/// How the CONNECT tab registers the terminal's MCP server with the user's own
/// copy of a harness. Every ProbeCommand here is argv for the harness CLI and
/// NEVER contains the token: a harness that reads it from an environment
/// variable gets `add`; one that only takes a literal token has no one-click
/// `add`, only the copy-paste forms (where kMcpTokenPlaceholder stands for it).
struct McpRegistration {
    QString server_name;                 // the entry name in the harness's config (edition::kMcpServerName)
    std::optional<ProbeCommand> check;   // exit 0 = an entry with server_name exists
    std::optional<ProbeCommand> remove;  // run before `add` when replacing an entry
    std::optional<ProbeCommand> add;     // the one-click ADD; token-free
    QStringList manual_args;             // a command the user can run by hand (may hold kMcpTokenPlaceholder)
    QString config_snippet;              // or: a config-file entry (may hold kMcpTokenPlaceholder)
    QString config_path_hint;            // where config_snippet goes ("~/.codex/config.toml")
    QString note;                        // what else the user must know (e.g. set FINCEPT_MCP_TOKEN)
    bool token_from_env = false;         // `add` makes the harness read kMcpTokenEnvVar at run time
};

/// Everything the registry knows about one harness. Copied out by value.
struct HarnessStatus {
    QString id;
    std::optional<Installation> installation; // empty = not installed (or not detected yet)
    bool detection_complete = false; // at least one detection pass has finished
    bool detecting = false;          // a path search or version probe is in flight
    QString detect_error;            // why the version is unknown, or which binaries were rejected
    /// Flatpak: the sandbox may not start programs on the user's system (the
    /// host-command permission was removed, or flatpak-spawn is missing), so the
    /// CLI could not even be looked for. detect_error says why and how to fix it.
    /// Never "not installed".
    bool host_blocked = false;
    AuthStatus auth;
    bool probing_auth = false;
    QVector<ModelInfo> models;       // the CLI's list when available, else the adapter's built-in list
    bool models_from_cli = false;
    bool probing_models = false;
    qint64 models_checked_at_ms = 0;
    QString models_error;
    qint64 cooldown_until_ms = 0;
    FailureKind last_failure = FailureKind::None;

    bool installed() const { return installation.has_value(); }
};

} // namespace fincept::harness
