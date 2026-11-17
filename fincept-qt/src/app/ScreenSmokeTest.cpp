#include "app/ScreenSmokeTest.h"

#include "app/DockScreenRouter.h"
#include "auth/InactivityGuard.h"
#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QPixmap>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <algorithm>
#include <cstdio>

namespace fincept {

namespace {

// ── First-open timing (FINCEPT_SMOKE_TIMING=1) ───────────────────────────────
// One line per screen on stderr and in the log:
//   [SmokeTiming] <id> factory=<ms> navigate=<ms> paint=<ms> maxblock=<ms> busy=<ms> widgets=<n>
// factory  — the screen's constructor alone (DockScreenRouter::materialize_screen)
// navigate — the whole synchronous navigate(): construction + show/polish + layout
// paint    — from the start of navigate() until the event-loop pass that
//            painted the screen for the first time has returned
// maxblock — longest single event-loop pass after navigate() returned, within
//            the settle window (first paint, deferred singleShot init, …)
// busy     — total of the passes >= 16 ms in that window
// Every number is GUI-thread time the user waits through on first open.
constexpr int kTimingSettleMs = 1500;

/// Notes the first paint of a widget or of anything inside it.
class PaintProbe : public QObject {
  public:
    QPointer<QWidget> target;
    bool painted = false;

  protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (!painted && ev->type() == QEvent::Paint && target && obj->isWidgetType()) {
            auto* w = static_cast<QWidget*>(obj);
            if (w == target || target->isAncestorOf(w))
                painted = true;
        }
        return false;
    }
};

struct ScreenTiming {
    qint64 navigate_ms = 0;
    qint64 paint_ms = -1;
    qint64 max_block_ms = 0;
    qint64 busy_ms = 0;
};

/// Runs the settle window after navigate() and fills the paint/block numbers.
/// `since_nav` started when navigate() was called.
void measure_settle(QWidget* screen, const QElapsedTimer& since_nav, ScreenTiming* out) {
    PaintProbe probe;
    probe.target = screen;
    qApp->installEventFilter(&probe);
    QElapsedTimer settle;
    settle.start();
    while (settle.elapsed() < kTimingSettleMs) {
        QElapsedTimer pass;
        pass.start();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        const qint64 d = pass.elapsed();
        out->max_block_ms = std::max(out->max_block_ms, d);
        if (d >= 16)
            out->busy_ms += d;
        if (probe.painted && out->paint_ms < 0)
            out->paint_ms = since_nav.elapsed();
    }
    qApp->removeEventFilter(&probe);
}

bool timing_enabled() {
    return qEnvironmentVariableIntValue("FINCEPT_SMOKE_TIMING") == 1;
}

// Pump the event loop for `ms` so showEvent, deferred singleShot(100) init, and
// lazy web-view / chart construction actually run before we judge the screen.
// We do NOT wait for data fetches to finish — only for the widget tree to build.
void spin(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

// Upper bound on how long one screen may take to materialise before we call it
// a failure. Generous on purpose: this is a "did it build at all" check, not a
// performance budget, and it gates releases on shared CI runners.
constexpr int kConstructDeadlineMs = 8000;

// Settle time after the widget appears, so showEvent-driven child construction
// runs before we move on. Short because the deadline loop already did the wait.
constexpr int kSettleMs = 120;

// Wait until `id`'s widget exists, or the deadline expires. Returns the elapsed
// milliseconds so a slow-but-passing screen can still be reported.
//
// A fixed spin() budget was the previous approach and it made this test lie: on
// a machine under memory pressure, 8 unrelated screens were reported as "did not
// construct" purely because deferred materialisation had not been serviced yet,
// and the same binary passed on the next run. Polling costs nothing when things
// are healthy — it returns on the first iteration — and stops the test failing
// a release for being busy rather than broken.
int wait_for_construction(DockScreenRouter* router, const QString& id) {
    QElapsedTimer t;
    t.start();
    while (!router->screen_widget(id) && t.elapsed() < kConstructDeadlineMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return static_cast<int>(t.elapsed());
}
} // namespace

int run_screen_smoke_test(DockScreenRouter* router) {
    if (!router) {
        std::fprintf(stderr, "[Smoke] FAIL: no router\n");
        std::fflush(stderr);
        return 2;
    }

    // A PIN-locked terminal makes this test meaningless, and silently so.
    // DockScreenRouter::navigate() hard-refuses while the terminal is locked
    // (deliberately — nothing may mutate panel state behind the PIN gate), so
    // every navigation no-ops and EVERY lazily-registered screen is reported as
    // "did not construct". That produced 46 phantom failures on a developer
    // machine with a PIN set, while CI — a fresh runner with no PIN — passed.
    // A test that only works in one environment and lies in the other is worse
    // than no test, so detect the condition and say exactly what is wrong.
    //
    // Deliberately NOT auto-unlocking: a test harness must not learn to bypass
    // an authentication gate. A timing run (FINCEPT_SMOKE_TIMING=1) measures a
    // signed-in terminal, so it waits for a person — or a driver typing the PIN
    // into the lock screen — to unlock it.
    if (timing_enabled()) {
        QElapsedTimer unlock_wait;
        unlock_wait.start();
        while (auth::InactivityGuard::instance().is_terminal_locked() && unlock_wait.elapsed() < 120000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        spin(3000); // let the unlock's own layout restore / first screen settle
    }
    if (auth::InactivityGuard::instance().is_terminal_locked()) {
        std::fprintf(stderr,
                     "[Smoke] FAIL: terminal is PIN-locked — navigate() is suppressed, so no screen can be\n"
                     "        constructed and every result would be a false failure.\n"
                     "        Clear the PIN (Settings > Security > Change PIN) and re-run.\n"
                     "        NOTE: --profile does NOT help. The PIN lives in machine-wide SecureStorage,\n"
                     "        not in the profile directory, so a fresh profile still hits the lock once it\n"
                     "        has an authenticated session. CI passes only because its runners have no PIN.\n");
        std::fflush(stderr);
        LOG_ERROR("Smoke", "Aborted: terminal is PIN-locked; navigate() is suppressed behind the lock gate");
        return 3;
    }

    const QStringList ids = router->all_screen_ids();
    std::fprintf(stderr, "[Smoke] walking %d screens\n", static_cast<int>(ids.size()));
    std::fflush(stderr);
    LOG_INFO("Smoke", QString("Screen smoke test: %1 screens").arg(ids.size()));

    const bool timing = timing_enabled();
    QStringList failures;
    for (const QString& id : ids) {
        // Breadcrumb BEFORE construction so a hard process abort (e.g. a missing
        // QtWebEngineProcess.exe) names the culprit as the last line in the log.
        std::fprintf(stderr, "[Smoke] >>> constructing %s\n", qUtf8Printable(id));
        std::fflush(stderr);
        LOG_INFO("Smoke", QString("constructing %1").arg(id));

        // exclusive=true closes the previously-opened screen before opening this
        // one, so panels don't accumulate (non-exclusive navigation re-layouts an
        // ever-growing set → O(n^2) and a multi-minute walk). We only need each
        // screen open long enough to construct + fire showEvent.
        QElapsedTimer since_nav;
        since_nav.start();
        router->navigate(id, /*exclusive=*/true);
        ScreenTiming timing_row;
        timing_row.navigate_ms = since_nav.elapsed();
        const int waited_ms = wait_for_construction(router, id);

        if (!router->screen_widget(id)) {
            failures << id;
            std::fprintf(stderr, "[Smoke] !!! %s did not construct within %d ms\n", qUtf8Printable(id),
                         kConstructDeadlineMs);
            std::fflush(stderr);
            continue;
        }

        if (timing) {
            QWidget* screen = router->screen_widget(id);
            measure_settle(screen, since_nav, &timing_row);
            const QString line =
                QStringLiteral("%1 factory=%2 navigate=%3 paint=%4 maxblock=%5 busy=%6 widgets=%7")
                    .arg(id)
                    .arg(screen->property("_fincept_factory_ms").toLongLong())
                    .arg(timing_row.navigate_ms)
                    .arg(timing_row.paint_ms)
                    .arg(timing_row.max_block_ms)
                    .arg(timing_row.busy_ms)
                    .arg(screen->findChildren<QWidget*>().size());
            std::fprintf(stderr, "[SmokeTiming] %s\n", qUtf8Printable(line));
            std::fflush(stderr);
            LOG_INFO("SmokeTiming", line);
            // FINCEPT_SMOKE_SHOTS=<dir>: a picture of the window per screen, for
            // checking that a change still renders the screen as before.
            const QString shots = qEnvironmentVariable("FINCEPT_SMOKE_SHOTS");
            if (!shots.isEmpty())
                screen->window()->grab().save(shots + QLatin1Char('/') + id + QStringLiteral(".png"));
        } else {
            // Let showEvent-driven child construction run before moving on.
            spin(kSettleMs);
        }

        // Surface slow screens without failing them — a screen that needs seconds
        // to build is a real finding, just not this test's verdict to make.
        if (waited_ms > 1000) {
            std::fprintf(stderr, "[Smoke] ... %s took %d ms to construct\n", qUtf8Printable(id), waited_ms);
            std::fflush(stderr);
            LOG_WARN("Smoke", QString("%1 took %2 ms to construct").arg(id).arg(waited_ms));
        }
    }

    if (failures.isEmpty()) {
        std::fprintf(stderr, "[Smoke] OK: all %d screens constructed\n", static_cast<int>(ids.size()));
        std::fflush(stderr);
        LOG_INFO("Smoke", "All screens constructed OK");
        return 0;
    }

    std::fprintf(stderr, "[Smoke] FAIL: %d screen(s) did not construct: %s\n", static_cast<int>(failures.size()),
                 qUtf8Printable(failures.join(", ")));
    std::fflush(stderr);
    LOG_ERROR("Smoke", QString("%1 screen(s) failed: %2").arg(failures.size()).arg(failures.join(", ")));
    return 1;
}

} // namespace fincept
