#pragma once
// ProcessTree — a child process and everything it starts, controlled as one unit.
//
//   auto* p = new QProcess(owner);
//   process::own_tree(p);        // before start()
//   p->start(program, args);
//   …cancel / timeout / quit:    process::kill_tree(p);     // the child AND its descendants
//   …finished by itself:         process::release_tree(p);  // what it left running stays
//   …died abnormally (crash, a lost kernel), or deleted without either call:
//                                what is left of the tree is killed — call
//                                kill_tree(p) after it ended (at once), or just
//                                never release_tree() (when the QProcess goes)
//
// Windows: the child is created SUSPENDED (and without a console window). From
// QProcess::started — emitted synchronously inside start() — it is assigned to
// a job object with KILL_ON_JOB_CLOSE and only then resumed, so nothing it
// spawns can start outside the job. kill_tree() terminates the job;
// release_tree() clears KILL_ON_JOB_CLOSE and closes the handle, so a server
// the child started on purpose keeps running. While the handle is open, the OS
// kills the whole tree if this application dies (crash included).
// POSIX: the child starts a new session (setsid), so it leads its own process
// group (group id = its pid). kill_tree() sends SIGKILL to the group — while
// the child runs, and also after it ended (its leftovers), as on Windows. A
// QProcess deleted (or restarted) without release_tree() sends SIGTERM to what
// is left of the group and SIGKILL ~3 s later, on a detached helper thread (the
// calling thread never waits); while QCoreApplication is being destroyed, or
// once it is gone, SIGKILL goes at once (pending groups too). After the child
// has been reaped the group is signalled only while it still has members and
// no process holds its id (the system never reuses an id while such a group
// exists); an empty group is forgotten when the child's finished() arrives.
// Remaining differences from Windows: a descendant that starts its own
// session or group (setsid / setpgid — Python start_new_session=True, a daemon
// that double-forks) has left the group and is not reached; and nothing reaps
// the group if this application crashes or is SIGKILLed (no kill-on-close).
//
// Flatpak: a QProcess that runs a program on the host (process::host::wrap,
// HostExec.h) is only flatpak-spawn here; kill_tree() also sends SIGKILL to the
// program's HOST process group, and flatpak-spawn's --watch-bus interrupts that
// group when flatpak-spawn itself goes.
//
// Thread: call everything on the QProcess's own thread. QtCore only (plus the
// Win32 job / thread API, POSIX signals and one std::thread per reaped POSIX
// group); no logger — callers log the outcome.

#include <QtGlobal>

class QProcess;

namespace fincept::process {

struct TreeOptions {
    /// Windows: BELOW_NORMAL_PRIORITY_CLASS, inherited by the whole tree.
    /// POSIX: nice +10 in the child.
    bool low_priority = false;
};

// ── One QProcess and its tree ───────────────────────────────────────────────

/// Before start(). Replaces any CreateProcessArgumentsModifier (Windows) or
/// child-process modifier (POSIX) already set on `proc`. Safe to call again
/// before a restart of the same QProcess: each run gets a fresh job.
void own_tree(QProcess* proc, const TreeOptions& opt = {});

/// Kill the child and every process of its tree now (cancel, timeout, quit).
/// Also reaps what is left after the child itself ended — a crashed child's
/// workers, a lost kernel's servers: "it died abnormally, treat its tree as
/// killed" is kill_tree() from the finished() handler (both platforms; POSIX
/// since 2026-11-17). Returns true when the tree was reached (job terminated /
/// group signalled), false when only the child itself could be killed or
/// nothing was left.
bool kill_tree(QProcess* proc);

/// The child ended by itself: processes it left behind keep running, and the
/// job handle is freed. Call before deleting a QProcess that exited normally
/// when its leftovers are meant to outlive it. Without it, deleting the
/// QProcess kills them (see the top of this file).
void release_tree(QProcess* proc);

/// Whether the running child sits in its tree (Windows: inside its job; POSIX:
/// own session). False before start, after release, or when assignment failed.
bool tree_owned(const QProcess* proc);

// ── Primitives (HarnessLaunch keeps one job per run itself) ────────────────

/// Creation flags for `proc`: no console window; `suspended` (Windows) creates
/// the main thread suspended until adopt_started_process() resumes it.
void set_start_flags(QProcess* proc, bool suspended, const TreeOptions& opt = {});

struct Adoption {
    bool assigned = false; ///< inside `job` (POSIX: always true — process group)
    bool resumed = true;   ///< false = a suspended child could not be resumed: kill it
};

/// From QProcess::started for a process prepared with set_start_flags(…, true):
/// assign it to `job`, THEN resume its main thread (resumed also when the
/// assignment fails). POSIX: no-op.
Adoption adopt_started_process(QProcess* proc, void* job);

/// Job object that kills every member when its last handle closes. nullptr on
/// failure and on POSIX.
void* create_kill_on_close_job();
bool terminate_job(void* job); ///< kill every process in the job now
void close_job(void* job);     ///< last handle: kill-on-close reaps the members
void release_job(void* job);   ///< clear kill-on-close, then close: members keep running

/// POSIX: SIGTERM (or SIGKILL when `force`) to the process group `pid` leads.
/// No-op on Windows and for pid <= 1.
void signal_group(qint64 pid, bool force);

} // namespace fincept::process
