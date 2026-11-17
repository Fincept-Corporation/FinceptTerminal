// HarnessLaunch.cpp — launch specs, the child environment and process-tree
// control for harness CLIs (see HarnessLaunch.h). Moved here from
// CodingAgentService_Process.cpp; the Windows start is suspended until the
// child sits in its job object. The job / suspended-start primitives live in
// core/process/ProcessTree (shared with the Python runners); this file keeps
// the harness API its callers use.

#include "services/harness/HarnessLaunch.h"

#include "core/process/HostExec.h"
#include "core/process/ProcessTree.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace fincept::harness {

QString msvc_quote(const QString& arg) {
    static const QRegularExpression needs_quotes(QStringLiteral("[\\s\"]"));
    if (!arg.isEmpty() && !arg.contains(needs_quotes))
        return arg;
    QString out = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar c : arg) {
        if (c == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (c == QLatin1Char('"')) {
            out += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            out += QLatin1Char('"');
            backslashes = 0;
            continue;
        }
        out += QString(backslashes, QLatin1Char('\\'));
        backslashes = 0;
        out += c;
    }
    out += QString(backslashes * 2, QLatin1Char('\\'));
    out += QLatin1Char('"');
    return out;
}

bool cmd_arg_safe(const QString& arg) {
    static const QRegularExpression bad(QStringLiteral("[%!^&|<>\\r\\n]"));
    return !arg.contains(bad);
}

LaunchSpec make_launch_spec(const Installation& inst, const QStringList& args) {
    LaunchSpec s;
    if (inst.path.isEmpty()) {
        s.error = QStringLiteral("No executable was found.");
        return s;
    }
    switch (inst.via) {
        case Installation::NodeScript:
            // An npm shim, resolved: run its script with node directly — a plain
            // argument list, no cmd.exe, no quoting restrictions.
            if (inst.node_path.isEmpty() || inst.script_path.isEmpty()) {
                s.error = QStringLiteral("The npm launcher %1 could not be resolved to node and its script.")
                              .arg(QDir::toNativeSeparators(inst.path));
                return s;
            }
            s.program = inst.node_path;
            s.args << QDir::toNativeSeparators(inst.script_path) << args;
            return s;
        case Installation::Wsl:
            s.error = QStringLiteral("CLIs installed inside WSL are not supported yet.");
            return s;
        case Installation::CmdShim:
#ifdef Q_OS_WIN
        {
            if (!cmd_arg_safe(inst.path)) {
                s.error = QStringLiteral("The CLI path contains characters cmd.exe cannot pass safely: %1").arg(inst.path);
                return s;
            }
            QString line = msvc_quote(QDir::toNativeSeparators(inst.path));
            for (const auto& a : args) {
                if (!cmd_arg_safe(a)) {
                    s.error = QStringLiteral("An argument contains %^&|<>! or a line break, which cannot pass through the "
                                             "npm .cmd launcher. Install the native %1 binary or point Agent Workbench "
                                             "settings at it.")
                                  .arg(QFileInfo(inst.path).completeBaseName());
                    return s;
                }
                line += QLatin1Char(' ') + msvc_quote(a);
            }
            s.program = qEnvironmentVariable("ComSpec", QStringLiteral("cmd.exe"));
            s.native_args = QStringLiteral("/d /s /c \"%1\"").arg(line);
            s.use_native = true;
            return s;
        }
#else
            break; // no batch launchers outside Windows: run the file itself
#endif
        case Installation::Native:
            break;
    }
    s.program = inst.path;
    s.args = args;
    return s;
}

QProcessEnvironment child_environment(const Installation& inst, const ChildEnvOptions& opt) {
    QProcessEnvironment env = opt.base ? *opt.base : QProcessEnvironment::systemEnvironment();
    // A terminal launched from inside Claude Code must still be able to run it.
    env.remove(QStringLiteral("CLAUDECODE"));
    env.remove(QStringLiteral("CLAUDE_CODE_ENTRYPOINT"));
    // Never inherit a stale token: only the one handed in below reaches the child.
    const QString token_var = QString::fromLatin1(kMcpTokenEnvVar);
    env.remove(token_var);
    // Nor the private build's (or a pre-split) token: a child of this edition
    // must never carry it to any server — the private build's entries in the
    // user's harness configs read exactly this variable.
    env.remove(QString::fromLatin1(edition::kForeignMcpTokenEnvVar));
    for (const QString& name : opt.strip)
        env.remove(name);
    for (const QString& name : opt.unset)
        env.remove(name);
    for (auto it = opt.set.cbegin(); it != opt.set.cend(); ++it) {
        if (it.key() != token_var && it.key() != QLatin1String(edition::kForeignMcpTokenEnvVar))
            env.insert(it.key(), it.value());
    }
    if (!opt.mcp_token.isEmpty())
        env.insert(token_var, opt.mcp_token);
#ifndef Q_OS_WIN
    // GUI apps start with a minimal PATH; node-based launchers must find `node`.
    const QString home = opt.home.isEmpty() ? QDir::homePath() : opt.home;
    QStringList extra;
    if (!inst.path.isEmpty())
        extra << QFileInfo(inst.path).absolutePath();
    if (!inst.node_path.isEmpty())
        extra << QFileInfo(inst.node_path).absolutePath();
    extra << home + QStringLiteral("/.local/bin") << QStringLiteral("/opt/homebrew/bin")
          << QStringLiteral("/usr/local/bin") << home + QStringLiteral("/.npm-global/bin")
          << home + QStringLiteral("/.bun/bin");
    QString base_path = env.value(QStringLiteral("PATH"));
    if (!opt.base && process::host::in_flatpak()) {
        // The CLI runs on the host: start from the host user's PATH, not the
        // sandbox's (/app/bin:/usr/bin). Read during detection; never waited for here.
        const std::optional<process::host::HostProfile> profile = process::host::host_profile_cached();
        base_path = profile ? profile->path : process::host::fallback_host_path();
    }
    QStringList path = base_path.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (auto it = extra.crbegin(); it != extra.crend(); ++it) {
        if (!it->isEmpty() && !path.contains(*it))
            path.prepend(*it);
    }
    env.insert(QStringLiteral("PATH"), path.join(QLatin1Char(':')));
#else
    Q_UNUSED(inst);
#endif
    return env;
}

QStringList host_env_names(const HarnessDescriptor& d, const LaunchPlan* plan) {
    QStringList names = d.api_key_env_vars;
    const auto add = [&names](const QStringList& more) {
        for (const QString& n : more) {
            if (!n.isEmpty() && !names.contains(n))
                names << n;
        }
    };
    add(d.terminal_key_env_vars);
    add(d.terminal_unset_env_vars);
    if (plan) {
        add(plan->env_set.keys());
        add(plan->env_unset);
    }
    return names;
}

void apply_launch_spec(QProcess* proc, const LaunchSpec& spec, bool own_tree, const QStringList& host_env) {
    proc->setProgram(spec.program);
    // No console window flashing up for a console-subsystem child of a GUI app.
    // own_tree (Windows): start suspended, so the started slot can put the child
    // into its job object before it runs.
    process::set_start_flags(proc, /*suspended=*/own_tree);
#ifdef Q_OS_WIN
    Q_UNUSED(host_env);
    if (spec.use_native) {
        proc->setNativeArguments(spec.native_args);
        return;
    }
    proc->setArguments(spec.args);
#else
    // Own session ⇒ Stop can signal the agent AND everything it spawned.
    if (own_tree)
        proc->setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
    proc->setArguments(spec.args);
    // Flatpak: the CLI is a program of the user's system — it runs there, with
    // what child_environment() decided for the variables that matter mirrored
    // (always the token variables and the nested-session markers, which
    // child_environment() removes or sets). Elsewhere a no-op.
    QStringList mirror{QStringLiteral("CLAUDECODE"), QStringLiteral("CLAUDE_CODE_ENTRYPOINT"),
                       QString::fromLatin1(kMcpTokenEnvVar), QString::fromLatin1(edition::kForeignMcpTokenEnvVar)};
    for (const QString& n : host_env) {
        if (!mirror.contains(n))
            mirror << n;
    }
    process::host::wrap(proc, mirror);
#endif
}

TreeStart adopt_started_process(QProcess* proc, void* job) {
    const process::Adoption a = process::adopt_started_process(proc, job);
    TreeStart r;
    r.assigned = a.assigned;
    r.resumed = a.resumed;
    return r;
}

void* create_kill_on_close_job() {
    return process::create_kill_on_close_job();
}

bool terminate_job(void* job) {
    return process::terminate_job(job);
}

void close_job(void* job) {
    process::close_job(job);
}

void signal_tree(qint64 pid, bool force) {
    process::signal_group(pid, force);
}

void signal_tree(const QProcess* proc, qint64 pid, bool force) {
    // The host group first: once the local flatpak-spawn is gone, --watch-bus
    // only interrupts (SIGINT) what it started.
    process::host::signal_host_group(proc, force);
    process::signal_group(pid, force);
}

} // namespace fincept::harness
