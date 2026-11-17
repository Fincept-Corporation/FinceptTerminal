// ProcessTree.cpp — process-tree control for child processes (see ProcessTree.h).
// The Win32 suspended-start / job-object code came from
// services/harness/HarnessLaunch.cpp, which now delegates here, and replaces
// PythonSetupManager's own job object (assigned after the child had already
// started, so its first grandchildren could escape).

#include "core/process/ProcessTree.h"

#include "core/process/HostExec.h"

#include <QObject>
#include <QProcess>
#include <QVariant>

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <tlhelp32.h>
#else
#    include <QCoreApplication>

#    include <cerrno>
#    include <chrono>
#    include <mutex>
#    include <thread>
#    include <utility>
#    include <vector>

#    include <signal.h>
#    include <sys/resource.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

namespace fincept::process {

namespace {

#ifndef Q_OS_WIN
// ── POSIX: reaping the group a deleted / restarted QProcess left behind ──────
//
// The child leads its own process group (CreateNewSession), so its pid is the
// group id. Two facts make signalling that id safe after the child has been
// reaped: the system reuses no pid while a group with that id still exists, and
// once it does reuse it the new process holds that pid. So: leader not reaped →
// the group is ours; reaped → ours only while NO process has that pid and the
// group still has members (our leftovers).

/// SIGTERM, then this long (polled) for the group to empty, then SIGKILL.
constexpr int kReapGraceMs = 3000;
constexpr int kReapPollMs = 50;

/// Some process of group `pgid` is still alive (members we may not signal count).
bool group_alive(pid_t pgid) {
    if (::kill(-pgid, 0) == 0)
        return true;
    return errno == EPERM;
}

/// Group `pgid` is still the tree we started (see above).
bool group_is_ours(pid_t pgid, bool leader_reaped) {
    if (pgid <= 1)
        return false; // never 0 / 1: kill(-0) would signal our own group
    if (leader_reaped) {
        errno = 0;
        if (::getpgid(pgid) != -1 || errno != ESRCH)
            return false; // that pid lives again: our group ended before it was reused
    }
    return group_alive(pgid);
}

/// Groups between their SIGTERM and SIGKILL. Never destroyed: a helper thread
/// may still look at it while static objects are torn down.
struct PendingReaps {
    std::mutex mutex;
    std::vector<std::pair<pid_t, bool>> groups; ///< pgid, leader reaped
};

PendingReaps& pending_reaps() {
    static PendingReaps* const pending = new PendingReaps;
    return *pending;
}

/// Removes `pgid` from the pending list; false when someone else already finished it.
bool take_pending(pid_t pgid) {
    PendingReaps& p = pending_reaps();
    const std::lock_guard<std::mutex> lock(p.mutex);
    for (auto it = p.groups.begin(); it != p.groups.end(); ++it) {
        if (it->first == pgid) {
            p.groups.erase(it);
            return true;
        }
    }
    return false;
}

/// QCoreApplication is going away: no grace left (the helper threads would die
/// with the process before their SIGKILL) — every pending group gets it now.
void kill_pending_groups() {
    std::vector<std::pair<pid_t, bool>> all;
    {
        PendingReaps& p = pending_reaps();
        const std::lock_guard<std::mutex> lock(p.mutex);
        all.swap(p.groups);
    }
    for (const auto& g : all) {
        if (group_is_ours(g.first, g.second))
            ::kill(-g.first, SIGKILL);
    }
}

void add_pending(pid_t pgid, bool leader_reaped) {
    static std::once_flag registered;
    std::call_once(registered, []() { qAddPostRoutine(kill_pending_groups); });
    PendingReaps& p = pending_reaps();
    const std::lock_guard<std::mutex> lock(p.mutex);
    p.groups.emplace_back(pgid, leader_reaped);
}

/// On a detached helper thread: wait for the group to empty after SIGTERM,
/// SIGKILL what is left at the deadline. Only POSIX calls — no Qt object.
void finish_reap(pid_t pgid, bool leader_reaped) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kReapGraceMs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kReapPollMs));
        if (!group_is_ours(pgid, leader_reaped)) {
            take_pending(pgid); // everything ended on SIGTERM
            return;
        }
    }
    if (take_pending(pgid) && group_is_ours(pgid, leader_reaped))
        ::kill(-pgid, SIGKILL);
}

/// What is left of a tree that was neither killed nor released: SIGTERM now,
/// SIGKILL after kReapGraceMs on a helper thread (the calling thread never waits).
void reap_group(qint64 group, bool leader_reaped) {
    const auto pgid = static_cast<pid_t>(group);
    if (!group_is_ours(pgid, leader_reaped))
        return;
    if (QCoreApplication::closingDown() || !QCoreApplication::instance()) {
        ::kill(-pgid, SIGKILL); // quitting: as on Windows, no grace period
        return;
    }
    ::kill(-pgid, SIGTERM);
    add_pending(pgid, leader_reaped);
    try {
        std::thread(finish_reap, pgid, leader_reaped).detach();
    } catch (...) {
        // No thread to wait on: no grace period either.
        if (take_pending(pgid) && group_is_ours(pgid, leader_reaped))
            ::kill(-pgid, SIGKILL);
    }
}
#endif

/// Dynamic property holding the PROCESS_INFORMATION* Qt hands to CreateProcess.
/// Read only from the QProcess::started slot — emitted synchronously inside
/// start(), while Qt's structure is alive — and cleared right after.
[[maybe_unused]] constexpr const char kProcInfoProperty[] = "_fincept_tree_procinfo"; // Windows branch only
/// own_tree() already connected its started handler to this QProcess.
constexpr const char kHookedProperty[] = "_fincept_tree_hooked";
constexpr const char kHandleName[] = "_fincept_process_tree";

/// The tree of the current run, owned by its QProcess (a child object), so a
/// QProcess deleted without release_tree() takes its leftovers with it.
/// Windows: closing a job that still has KILL_ON_JOB_CLOSE reaps them.
/// POSIX: the group is sent SIGTERM, then SIGKILL (reap_group).
class TreeHandle final : public QObject {
  public:
    explicit TreeHandle(QProcess* owner) : QObject(owner) { setObjectName(QLatin1String(kHandleName)); }
    ~TreeHandle() override {
#ifdef Q_OS_WIN
        close_job(job);
#else
        // Runs after ~QProcess, which killed and reaped a child still running.
        reap_group(group, leader_exited);
#endif
    }

    void* job = nullptr;        // Windows job object; nullptr after release or on POSIX
    bool owned = false;         // the current run sits in its tree
    qint64 group = 0;           // POSIX: process group of the run (= the child's pid); 0 = nothing left to reach
    bool leader_exited = false; // POSIX: the child ended and was reaped (its pid may be reused once the group is empty)
};

TreeHandle* handle_of(const QProcess* proc) {
    if (!proc)
        return nullptr;
    for (QObject* c : proc->children()) {
        if (c->objectName() == QLatin1String(kHandleName)) {
            if (auto* h = dynamic_cast<TreeHandle*>(c))
                return h;
        }
    }
    return nullptr;
}

/// QProcess::started of a process prepared by own_tree().
void adopt_tree(QProcess* proc) {
    TreeHandle* h = handle_of(proc);
    if (h) {
        // A previous run of the same QProcess that was never released: its leftovers go.
        close_job(h->job);
        h->job = nullptr;
#ifndef Q_OS_WIN
        reap_group(h->group, /*leader_reaped=*/true); // a QProcess starts again only after its child ended
        h->group = 0;
#endif
    } else {
        h = new TreeHandle(proc);
    }
#ifdef Q_OS_WIN
    void* job = create_kill_on_close_job();
    const Adoption a = adopt_started_process(proc, job);
    if (a.assigned) {
        h->job = job;
        h->owned = true;
    } else {
        close_job(job); // empty: closing it touches no process
        h->owned = false;
    }
    if (!a.resumed)
        proc->kill(); // a child left suspended would never run, and never end
#else
    h->owned = true; // CreateNewSession: the child leads its own process group
    const qint64 pid = proc->processId();
    h->group = pid > 1 ? pid : 0;
    h->leader_exited = false;
#endif
}

#ifndef Q_OS_WIN
/// QProcess::finished of a process prepared by own_tree() (POSIX): the child
/// has been reaped. An empty group is forgotten at once — once empty, its id
/// may be reused by an unrelated process, so it is never signalled again.
void note_leader_exit(QProcess* proc) {
    TreeHandle* h = handle_of(proc);
    if (!h || h->group <= 1)
        return;
    h->leader_exited = true;
    if (!group_alive(static_cast<pid_t>(h->group)))
        h->group = 0;
}
#endif

#ifdef Q_OS_WIN
/// Fallback when Qt's PROCESS_INFORMATION is not usable: resume every thread
/// of `pid` (a process created suspended has exactly one).
bool resume_process_threads(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    bool any = false;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (th) {
                    if (ResumeThread(th) != static_cast<DWORD>(-1))
                        any = true;
                    CloseHandle(th);
                }
            }
            te.dwSize = sizeof(te);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
    return any;
}
#endif

} // namespace

// ── One QProcess and its tree ───────────────────────────────────────────────

void own_tree(QProcess* proc, const TreeOptions& opt) {
    if (!proc)
        return;
    set_start_flags(proc, /*suspended=*/true, opt);
#ifndef Q_OS_WIN
    proc->setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
#endif
    if (!proc->property(kHookedProperty).toBool()) {
        proc->setProperty(kHookedProperty, true);
        // Direct: the started signal comes from inside start() on this thread,
        // and on Windows the child stays suspended until this has run.
        QObject::connect(proc, &QProcess::started, proc, [proc]() { adopt_tree(proc); }, Qt::DirectConnection);
#ifndef Q_OS_WIN
        // Direct: before the caller's own finished handlers (connected later), which
        // may call kill_tree() / release_tree() or delete the QProcess.
        QObject::connect(proc, &QProcess::finished, proc, [proc]() { note_leader_exit(proc); },
                         Qt::DirectConnection);
#endif
    }
}

bool kill_tree(QProcess* proc) {
    if (!proc)
        return false;
    // A program run on the host (Flatpak, HostExec.h): its tree is the host
    // process group; the local one below is only flatpak-spawn.
    bool reached = host::signal_host_group(proc, /*force=*/true);
    TreeHandle* h = handle_of(proc);
#ifdef Q_OS_WIN
    if (h && h->job)
        reached = terminate_job(h->job); // also after the child itself ended: reaps its leftovers
#else
    if (h && h->owned && h->group > 1) {
        // While the child runs, its pid (= the group id) is ours. Once it has been
        // reaped the group is ours only while no process holds that pid
        // (group_is_ours): then its leftovers are reached, as on Windows.
        const bool reaped = proc->state() == QProcess::NotRunning;
        if (group_is_ours(static_cast<pid_t>(h->group), reaped)) {
            signal_group(h->group, /*force=*/true);
            reached = true;
        }
        h->group = 0; // killed: nothing is left for the destructor to reap
    }
#endif
    if (proc->state() != QProcess::NotRunning)
        proc->kill();
    return reached;
}

void release_tree(QProcess* proc) {
    TreeHandle* h = handle_of(proc);
    if (!h)
        return;
    release_job(h->job);
    h->job = nullptr;
    h->owned = false;
    h->group = 0; // POSIX: what the child left running is not ours to reap any more
}

bool tree_owned(const QProcess* proc) {
    const TreeHandle* h = handle_of(proc);
    return h && h->owned && proc->state() != QProcess::NotRunning;
}

// ── Primitives ──────────────────────────────────────────────────────────────

void set_start_flags(QProcess* proc, bool suspended, const TreeOptions& opt) {
    if (!proc)
        return;
#ifdef Q_OS_WIN
    QProcess* const target = proc;
    const bool low = opt.low_priority;
    proc->setCreateProcessArgumentsModifier([target, suspended, low](QProcess::CreateProcessArguments* cpa) {
        // No console window flashing up for a console-subsystem child of a GUI app.
        cpa->flags |= CREATE_NO_WINDOW;
        if (low)
            cpa->flags |= BELOW_NORMAL_PRIORITY_CLASS; // inherited by the child's own children
        if (suspended) {
            cpa->flags |= CREATE_SUSPENDED;
            target->setProperty(kProcInfoProperty, QVariant::fromValue(static_cast<qulonglong>(
                                                       reinterpret_cast<quintptr>(cpa->processInformation))));
        }
    });
#else
    Q_UNUSED(suspended);
    if (opt.low_priority)
        proc->setChildProcessModifier([]() { ::setpriority(PRIO_PROCESS, 0, 10); });
#endif
}

Adoption adopt_started_process(QProcess* proc, void* job) {
    Adoption r;
#ifdef Q_OS_WIN
    if (!proc)
        return r;
    const QVariant stored = proc->property(kProcInfoProperty);
    proc->setProperty(kProcInfoProperty, QVariant()); // valid only during start()
    const qint64 pid = proc->processId();
    if (pid <= 0)
        return r; // not running (any more): nothing to assign, nothing left suspended
    const DWORD wpid = static_cast<DWORD>(pid);
    auto* pi = stored.isValid()
                   ? reinterpret_cast<PROCESS_INFORMATION*>(static_cast<quintptr>(stored.toULongLong()))
                   : nullptr;
    const bool pi_ok = pi && pi->dwProcessId == wpid && pi->hProcess && pi->hThread;
    if (job) {
        if (pi_ok) {
            r.assigned = AssignProcessToJobObject(static_cast<HANDLE>(job), pi->hProcess) != FALSE;
        } else if (HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, wpid)) {
            r.assigned = AssignProcessToJobObject(static_cast<HANDLE>(job), h) != FALSE;
            CloseHandle(h);
        }
    }
    if (stored.isValid()) {
        // Created suspended by set_start_flags(…, true): resume it now — always,
        // also when the assignment failed.
        r.resumed = (pi_ok && ResumeThread(pi->hThread) != static_cast<DWORD>(-1)) || resume_process_threads(wpid);
    }
#else
    Q_UNUSED(proc);
    Q_UNUSED(job);
    r.assigned = true; // the process group from CreateNewSession
#endif
    return r;
}

void* create_kill_on_close_job() {
#ifdef Q_OS_WIN
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
        return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return nullptr;
    }
    return job;
#else
    return nullptr;
#endif
}

bool terminate_job(void* job) {
#ifdef Q_OS_WIN
    return job && TerminateJobObject(static_cast<HANDLE>(job), 1) != FALSE;
#else
    Q_UNUSED(job);
    return false;
#endif
}

void close_job(void* job) {
#ifdef Q_OS_WIN
    if (job)
        CloseHandle(static_cast<HANDLE>(job));
#else
    Q_UNUSED(job);
#endif
}

void release_job(void* job) {
#ifdef Q_OS_WIN
    if (!job)
        return;
    // Without KILL_ON_JOB_CLOSE the members outlive the last handle. If the
    // limit cannot be changed, closing still reaps them — the safe outcome.
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = 0;
    SetInformationJobObject(static_cast<HANDLE>(job), JobObjectExtendedLimitInformation, &info, sizeof(info));
    CloseHandle(static_cast<HANDLE>(job));
#else
    Q_UNUSED(job);
#endif
}

void signal_group(qint64 pid, bool force) {
#ifndef Q_OS_WIN
    if (pid > 1) // never 0/1: kill(-0) would signal our own process group
        ::kill(-static_cast<pid_t>(pid), force ? SIGKILL : SIGTERM);
#else
    Q_UNUSED(pid);
    Q_UNUSED(force);
#endif
}

} // namespace fincept::process
