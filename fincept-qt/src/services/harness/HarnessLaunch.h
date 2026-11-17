#pragma once
// HarnessLaunch.h — turning an Installation + argument list into a started
// child process: program / arguments (native exe, node + script for npm
// shims, cmd.exe only as a last resort), the ONE child-environment function,
// and process-tree control so Stop reaches everything the agent spawned.
//
// Windows: a run is created SUSPENDED, assigned to a kill-on-close job object
// and only then resumed (adopt_started_process), so no grandchild can start
// outside the job — terminating the job kills the whole tree, including the
// node process behind an npm shim. POSIX: the child leads its own session, so
// the process group can be signalled.
//
// QtCore only (plus the Win32 job / thread API); no logger, no settings.

#include "services/harness/HarnessTypes.h"

#include <QHash>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <optional>

class QProcess;

namespace fincept::harness {

struct LaunchSpec {
    QString program;
    QStringList args;
    QString native_args; // Windows cmd.exe shim only (last resort)
    bool use_native = false;
    QString error;
};

/// MSVC / CommandLineToArgvW quoting (what node and Rust parse on Windows).
QString msvc_quote(const QString& arg);
/// cmd.exe expands %VAR% (and !VAR!) even inside quotes and treats ^ & | < >
/// as operators outside them; a line break ends the command.
bool cmd_arg_safe(const QString& arg);

/// Program and arguments for `args` run through `inst`.
LaunchSpec make_launch_spec(const Installation& inst, const QStringList& args);

struct ChildEnvOptions {
    QHash<QString, QString> set;  // adapter LaunchPlan::env_set
    QStringList unset;            // adapter LaunchPlan::env_unset
    QStringList strip;            // API-key variables, when the env policy is "subscription only"
    QString mcp_token;            // value for kMcpTokenEnvVar; empty = none
    std::optional<QProcessEnvironment> base; // default: the system environment
    QString home;                 // default: QDir::homePath() (POSIX PATH fix-up)
};

/// The environment of EVERY harness child: the system environment without
/// Claude Code's nested-session markers and without the terminal's own MCP
/// token (only `opt.mcp_token`, when given, is set), minus `strip` and
/// `unset`, plus `set`; on POSIX the CLI's folder and the usual user bin
/// folders are prepended to PATH (GUI apps start with a minimal PATH, and
/// node-based CLIs need `node`). Flatpak (no `opt.base`): that PATH starts
/// from the HOST user's login-shell PATH — the CLI runs on the host.
QProcessEnvironment child_environment(const Installation& inst, const ChildEnvOptions& opt);

/// Variable names whose state a harness child on the HOST (Flatpak) must get
/// exactly as child_environment() prepared it — set or removed — even where the
/// app's own environment does not differ: the harness's API-key variables (the
/// env policy keeps or strips them), the terminal-AI unset list and the plan's
/// own variables. The token variables and Claude Code's nested-session markers
/// are always included by apply_launch_spec(). Names only.
QStringList host_env_names(const HarnessDescriptor& d, const LaunchPlan* plan = nullptr);

/// Program, arguments and creation flags (no console window). `own_tree`:
/// Windows — created suspended, see adopt_started_process(); POSIX — new
/// session, so the process group can be signalled.
/// The launch boundary for every harness process: call it AFTER the working
/// directory and the environment are set. In the Flatpak the program runs on
/// the user's system (process::host::wrap, HostExec.h) with `host_env`
/// (host_env_names()) mirrored; elsewhere nothing changes.
void apply_launch_spec(QProcess* proc, const LaunchSpec& spec, bool own_tree, const QStringList& host_env = {});

struct TreeStart {
    bool assigned = false; // inside the job object (POSIX: always true — process group)
    bool resumed = true;   // false = a suspended child could not be resumed (kill it)
};

/// Call from QProcess::started for a process prepared with own_tree = true:
/// Windows — assign it to `job` (from create_kill_on_close_job()), THEN resume
/// its main thread; it is resumed even when the assignment fails. POSIX: no-op.
TreeStart adopt_started_process(QProcess* proc, void* job);

/// Windows job object that kills every member when its last handle closes.
/// nullptr on failure and on POSIX.
void* create_kill_on_close_job();
bool terminate_job(void* job); // kill every process in the job now
void close_job(void* job);     // kill-on-close: reaps whatever is left

/// POSIX: SIGTERM (or SIGKILL when `force`) to the process group `pid` leads.
/// No-op on Windows (console children have no graceful signal; see terminate_job).
void signal_tree(qint64 pid, bool force);

/// signal_tree(pid, force) for the process `proc` started — and, when `proc`
/// runs a program on the host (Flatpak), the same signal to that program's
/// HOST process group first (the local pid is only flatpak-spawn).
void signal_tree(const QProcess* proc, qint64 pid, bool force);

} // namespace fincept::harness
