#pragma once
// HostExec — starting a program of the USER'S SYSTEM (the coding-agent CLIs:
// Claude Code, Codex, Gemini CLI, …) from inside the Flatpak sandbox, and the
// identity everywhere else (Windows, macOS, Linux without Flatpak: nothing here
// changes a process).
//
// In the Flatpak a harness CLI is not on the sandbox's PATH or file system, so
// every place that starts one ends in HarnessLaunch::apply_launch_spec(), which
// calls wrap(): the QProcess then runs
//
//   flatpak-spawn --host --watch-bus --directory=/ /bin/sh -c <wrapper>
//                 fincept-host <env file> <pid file> <host cwd> <program> <args…>
//
//   * flatpak-spawn forwards stdin/stdout/stderr (always fds 0-2), so streaming
//     and exit codes behave as before — except that a CLI killed by a signal
//     ends with code 128 + signal instead of a crash exit (flatpak-spawn's rule).
//   * --watch-bus: when flatpak-spawn goes (killed, or the app crashed and the
//     sandbox was torn down) the host sends SIGINT to the CLI's process group.
//   * The environment the CLI must see differently from the session helper's —
//     the run's MCP token, PATH, the adapter's variables, API keys the env policy
//     keeps or strips — travels in a 0600 file in $XDG_RUNTIME_DIR/app/<app id>/
//     (per-user tmpfs, visible to the host at the same path). The wrapper reads
//     it and deletes it before it execs the CLI. Never on a command line
//     (--env= would put the token in /proc/<pid>/cmdline of the host).
//   * The wrapper writes its pid (= the CLI's pid, = its process-group id: the
//     host starts it in a new session) to a pid file next to it; Stop / timeout
//     signal that HOST group with `kill` run through flatpak-spawn (the D-Bus
//     HostCommandSignal only accepts pids the same bus connection started).
//   * The working directory is mapped to its host path (host_path()) and
//     created there (mkdir -p, as the run's folder is created natively).
//
// Paths: a file a host CLI must read (per-run MCP config, the run's folder)
// must live where both sides see it. shared_path() gives that spelling:
// ~/.local/share/X (persisted with --persist=.local/share) is
// ~/.var/app/<id>/.local/share/X on the host and in the sandbox; the
// per-app runtime folder keeps $XDG_RUNTIME_DIR/app/<id>; /app, /usr, /tmp …
// are sandbox-private (host_path() = nullopt).
//
// Permission: --talk-name=org.freedesktop.Flatpak (manifest). When the user
// removed it (Flatseal / flatpak override) access() says so and blocked_reason()
// is the sentence the UI shows instead of "not installed".
//
// Thread: wrap(), is_wrapped(), signal_host_group() on the QProcess's thread;
// host_profile_blocking() / run_blocking() on worker threads only (they wait for
// a process); everything else anywhere. QtCore only; no logger.

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

class QProcess;

namespace fincept::process::host {

// ── Where this process runs ─────────────────────────────────────────────────

/// Inside a Flatpak sandbox (/.flatpak-info present — FLATPAK_ID alone also
/// leaks to host programs started from a sandboxed IDE). Always false outside
/// Linux. Cached.
bool in_flatpak();

/// The Flatpak app id ([Application] name= of /.flatpak-info, else FLATPAK_ID);
/// empty outside Flatpak.
QString app_id();

enum class Access {
    Direct,            ///< not sandboxed: host programs are started directly
    Available,         ///< Flatpak, flatpak-spawn --host may be used
    PermissionMissing, ///< Flatpak without talk access to org.freedesktop.Flatpak
    ToolMissing,       ///< Flatpak, but the runtime has no flatpak-spawn
};

/// From /.flatpak-info (the effective permissions, overrides included). Cached.
Access access();

/// access() is PermissionMissing or ToolMissing.
bool blocked();

/// What the UI says when blocked(): what is missing and the command that
/// restores it. Empty when not blocked.
QString blocked_reason();

/// `flatpak override --user --talk-name=org.freedesktop.Flatpak <app id>`.
QString reenable_command();

/// Tests: force in_flatpak() on or off; nullopt = detect. Resets the caches.
void set_flatpak_override_for_testing(std::optional<bool> on);

// ── Paths ───────────────────────────────────────────────────────────────────

/// The spelling of `path` a HOST process must use. Outside Flatpak: `path`.
/// Flatpak: the per-app runtime folder and document-portal paths keep (or get
/// back) their $XDG_RUNTIME_DIR spelling, persisted home folders map to
/// ~/.var/app/<id>/…, sandbox-private trees (/app, /usr, /etc, /tmp, /run …)
/// give nullopt, any other path is the user's own host path and is returned
/// as it is. Relative paths are made absolute first.
std::optional<QString> host_path(const QString& path);

/// host_path(path) when there is one, else `path`. For files written here and
/// read by a host CLI: the spelling that works on both sides.
QString shared_path(const QString& path);

/// A file or folder the document portal handed the app (a file dialog in the
/// sandbox returns /run/user/<uid>/doc/<id>/name): its real host path, from the
/// portal's user.document-portal.host-path attribute. Anything else, or when
/// the attribute is missing (older portals): `path` unchanged.
QString document_host_path(const QString& path);

// ── The host user's environment ─────────────────────────────────────────────

struct HostProfile {
    bool ok = false;
    QString home;
    QString path;   ///< PATH for host CLIs (the login shell's when it could be read)
    QString source; ///< "login-shell", "login-shell (non-interactive)" or "session"
    QString error;
};

/// Worker threads only: HOME and the login shell's PATH of the host user —
/// resolved once (interactive login shell, then a plain login shell, both
/// bounded; else the session helper's PATH) and cached for the session.
HostProfile host_profile_blocking(int timeout_ms = 15000);

/// The cached profile; nullopt before host_profile_blocking() completed. Never blocks.
std::optional<HostProfile> host_profile_cached();

/// PATH for a host CLI when no profile is known yet.
QString fallback_host_path();

// ── One command on the host, synchronously (worker threads only) ────────────

struct HostRun {
    bool started = false;
    bool timed_out = false;
    int exit_code = -1;
    QByteArray out;
    QByteArray err;
    QString error; ///< why it did not start / finish
};

/// `argv` on the host (stdin empty, the session helper's environment, cwd "/"),
/// at most `timeout_ms`. Outside Flatpak argv runs directly.
HostRun run_blocking(const QStringList& argv, int timeout_ms, qint64 max_output_bytes = 8 * 1024 * 1024);

// ── The launch boundary ─────────────────────────────────────────────────────

/// Flatpak: rewrite `proc` — whose program, arguments, working directory and
/// environment are already set — so that the program runs on the host (see the
/// top of this file). `mirror_env`: variable names whose state on the host must
/// equal the environment set on `proc` (set there → exported, absent → unset),
/// e.g. the harness's API-key variables. Besides those, only what differs from
/// the app's own environment and the proxy variables reach the host.
/// Returns false (and changes nothing) outside Flatpak.
bool wrap(QProcess* proc, const QStringList& mirror_env = {});

/// `proc` was prepared by wrap().
bool is_wrapped(const QProcess* proc);

/// SIGTERM (or SIGKILL when `force`) to the HOST process group of a wrapped
/// `proc`, fire-and-forget. False when `proc` is not wrapped or the host pid is
/// not known yet (the wrapper had not run).
bool signal_host_group(const QProcess* proc, bool force);

} // namespace fincept::process::host
