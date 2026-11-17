// tst_process_tree — src/core/process/ProcessTree against a REAL process tree.
// The test re-runs its own executable: as the child ("parent" role) it starts a
// grandchild (itself again, "grandchild" role), waits until the grandchild is
// set up, prints its pid, then either exits at once (as a script that started a
// server) or waits to be killed (as a script that hangs). Every grandchild exits
// by itself after 60 s, so a failing run leaves nothing behind for long.
#include "core/process/ProcessTree.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
#include <QtTest>

#include <cstdio>
#include <cstdlib>
#include <memory>

#ifdef Q_OS_WIN
#    include <windows.h>
#else
#    include <signal.h>
#    include <sys/types.h>
#endif

using namespace fincept::process;

namespace {

constexpr const char kRole[] = "FINCEPT_TREE_ROLE";
constexpr const char kMode[] = "FINCEPT_TREE_PARENT_MODE";
/// POSIX: the grandchild ignores SIGTERM (only SIGKILL ends it).
constexpr const char kIgnoreTerm[] = "FINCEPT_TREE_GC_IGNORE_TERM";

int run_grandchild() {
#ifndef Q_OS_WIN
    if (qEnvironmentVariableIsSet(kIgnoreTerm))
        ::signal(SIGTERM, SIG_IGN);
#endif
    std::printf("READY\n"); // set up (signal disposition included)
    std::fflush(stdout);
    for (int i = 0; i < 600; ++i)
        QThread::msleep(100);
    return 0;
}

int run_parent() {
    // Leaked on purpose: its destructor would kill the grandchild.
    auto* gc = new QProcess;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QString::fromLatin1(kRole), QStringLiteral("grandchild"));
    gc->setProcessEnvironment(env);
    gc->setStandardErrorFile(QProcess::nullDevice());
    gc->start(QCoreApplication::applicationFilePath(), {});
    if (!gc->waitForStarted(10000))
        return 3;
    // Report the grandchild only once it is set up, so a signal sent right after
    // finds its final disposition.
    QByteArray ready;
    QElapsedTimer t;
    t.start();
    while (!ready.contains("READY") && t.elapsed() < 10000) {
        if (!gc->waitForReadyRead(500) && gc->state() == QProcess::NotRunning)
            break;
        ready += gc->readAllStandardOutput();
    }
    if (!ready.contains("READY"))
        return 4;
    std::printf("GRANDCHILD %lld\n", static_cast<long long>(gc->processId()));
    std::fflush(stdout);
    if (qEnvironmentVariable(kMode) == QLatin1String("exit"))
        std::_Exit(0); // a normal exit that runs no destructor
    return run_grandchild();
}

bool alive(qint64 pid) {
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return false;
    const bool running = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return running;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

void kill_pid(qint64 pid) {
    if (pid <= 0)
        return;
#ifdef Q_OS_WIN
    if (HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid))) {
        TerminateProcess(h, 1);
        CloseHandle(h);
    }
#else
    ::kill(static_cast<pid_t>(pid), SIGKILL);
#endif
}

/// Start the child in `mode` and return the grandchild's pid (0 on failure).
qint64 start_parent(QProcess& p, const QString& mode, const TreeOptions& opt = {}, bool gc_ignores_term = false) {
    own_tree(&p, opt);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QString::fromLatin1(kRole), QStringLiteral("parent"));
    env.insert(QString::fromLatin1(kMode), mode);
    if (gc_ignores_term)
        env.insert(QString::fromLatin1(kIgnoreTerm), QStringLiteral("1")); // inherited by the grandchild
    else
        env.remove(QString::fromLatin1(kIgnoreTerm));
    p.setProcessEnvironment(env);
    p.start(QCoreApplication::applicationFilePath(), {});
    if (!p.waitForStarted(10000))
        return 0;
    QByteArray out;
    QElapsedTimer t;
    t.start();
    while (!out.contains('\n') && t.elapsed() < 15000) {
        if (!p.waitForReadyRead(500) && p.state() == QProcess::NotRunning)
            break;
        out += p.readAllStandardOutput();
    }
    out += p.readAllStandardOutput();
    const int at = out.indexOf("GRANDCHILD ");
    if (at < 0)
        return 0;
    return out.mid(at + 11).split('\n').value(0).trimmed().toLongLong();
}

bool wait_until_dead(qint64 pid, int ms) {
    QElapsedTimer t;
    t.start();
    while (alive(pid) && t.elapsed() < ms)
        QThread::msleep(50);
    return !alive(pid);
}

} // namespace

class TestProcessTree : public QObject {
    Q_OBJECT
  private slots:
    void nothing_running_is_a_no_op() {
        QVERIFY(!kill_tree(nullptr));
        release_tree(nullptr);
        QProcess p;
        QVERIFY(!tree_owned(&p));
        QVERIFY(!kill_tree(&p));
        release_tree(&p);
        own_tree(&p);
        QVERIFY(!kill_tree(&p)); // prepared, never started
    }

    void kill_tree_reaches_the_grandchild() {
        QProcess p;
        const qint64 gc = start_parent(p, QStringLiteral("wait"));
        QVERIFY2(gc > 0, "the child never reported its grandchild (did a suspended start never resume?)");
        QVERIFY(tree_owned(&p));
        QVERIFY(alive(gc));
        QVERIFY(kill_tree(&p));
        QVERIFY(p.waitForFinished(10000));
        const bool dead = wait_until_dead(gc, 5000);
        if (!dead)
            kill_pid(gc);
        QVERIFY(dead);
    }

    void release_tree_keeps_what_the_child_left_running() {
        qint64 gc = 0;
        {
            QProcess p;
            gc = start_parent(p, QStringLiteral("exit"));
            QVERIFY(gc > 0);
            QVERIFY(p.waitForFinished(10000));
            QCOMPARE(p.exitStatus(), QProcess::NormalExit);
            release_tree(&p);
            QVERIFY(!tree_owned(&p));
        } // QProcess destroyed after the release
        QThread::msleep(500);
        const bool still = alive(gc);
        kill_pid(gc);
        QVERIFY2(still, "a released tree must not be killed when its QProcess goes away");
    }

    void deleting_without_release_reaps_the_leftovers() {
        // Windows: the job still has KILL_ON_JOB_CLOSE. POSIX: SIGTERM to the
        // group the ended child left (the grandchild's default action ends it).
        qint64 gc = 0;
        {
            QProcess p;
            gc = start_parent(p, QStringLiteral("exit"));
            QVERIFY(gc > 0);
            QVERIFY(p.waitForFinished(10000));
        } // destroyed without release_tree()
        const bool dead = wait_until_dead(gc, 5000);
        if (!dead)
            kill_pid(gc);
        QVERIFY(dead);
    }

    void deleting_without_release_escalates_to_sigkill() {
#ifdef Q_OS_WIN
        QSKIP("POSIX only: Windows terminates the job outright");
#else
        qint64 gc = 0;
        {
            QProcess p;
            gc = start_parent(p, QStringLiteral("exit"), {}, /*gc_ignores_term=*/true);
            QVERIFY(gc > 0);
            QVERIFY(p.waitForFinished(10000));
        } // SIGTERM now (ignored), SIGKILL ~3 s later from the helper thread
        QThread::msleep(500);
        const bool through_grace = alive(gc); // the GUI-side call never waited for it
        const bool dead = wait_until_dead(gc, 10000);
        if (!dead)
            kill_pid(gc);
        QVERIFY2(through_grace, "a grandchild ignoring SIGTERM must still run during the grace period");
        QVERIFY2(dead, "the grace period must end with SIGKILL");
#endif
    }

    void kill_tree_after_the_child_ended_reaps_the_leftovers() {
        QProcess p;
        const qint64 gc = start_parent(p, QStringLiteral("exit"));
        QVERIFY(gc > 0);
        QVERIFY(p.waitForFinished(10000));
        QVERIFY(alive(gc));
        QVERIFY(kill_tree(&p));
        const bool dead = wait_until_dead(gc, 5000);
        if (!dead)
            kill_pid(gc);
        QVERIFY(dead);
    }

    void low_priority_reaches_the_child() {
#ifndef Q_OS_WIN
        QSKIP("checked on Windows only (priority class)");
#else
        QProcess p;
        TreeOptions opt;
        opt.low_priority = true;
        const qint64 gc = start_parent(p, QStringLiteral("wait"), opt);
        QVERIFY(gc > 0);
        DWORD cls = 0;
        if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(p.processId()))) {
            cls = GetPriorityClass(h);
            CloseHandle(h);
        }
        kill_tree(&p);
        p.waitForFinished(10000);
        wait_until_dead(gc, 5000);
        QCOMPARE(cls, static_cast<DWORD>(BELOW_NORMAL_PRIORITY_CLASS));
#endif
    }

    void restart_of_the_same_qprocess_gets_a_fresh_tree() {
        QProcess p;
        const qint64 first = start_parent(p, QStringLiteral("wait"));
        QVERIFY(first > 0);
        kill_tree(&p);
        QVERIFY(p.waitForFinished(10000));
        QVERIFY(wait_until_dead(first, 5000));
        const qint64 second = start_parent(p, QStringLiteral("wait"));
        QVERIFY(second > 0);
        QVERIFY(tree_owned(&p));
        QVERIFY(kill_tree(&p));
        QVERIFY(p.waitForFinished(10000));
        const bool dead = wait_until_dead(second, 5000);
        if (!dead)
            kill_pid(second);
        QVERIFY(dead);
    }
};

int main(int argc, char** argv) {
    const QByteArray role = qgetenv(kRole);
    if (!role.isEmpty()) {
        QCoreApplication app(argc, argv);
        return role == "parent" ? run_parent() : run_grandchild();
    }
    QCoreApplication app(argc, argv);
    TestProcessTree t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_process_tree.moc"
