#include "app/ScreenSmokeTest.h"

#include "app/DockScreenRouter.h"
#include "auth/InactivityGuard.h"
#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QStringList>

#include <cstdio>

namespace fincept {

namespace {
// Pump the event loop for `ms` so showEvent, deferred singleShot(100) init, and
// lazy web-view / chart construction actually run before we judge the screen.
// We do NOT wait for data fetches to finish — only for the widget tree to build.
void spin(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
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
    // an authentication gate.
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
        router->navigate(id, /*exclusive=*/true);
        spin(300);

        if (!router->screen_widget(id)) {
            failures << id;
            std::fprintf(stderr, "[Smoke] !!! %s did not construct\n", qUtf8Printable(id));
            std::fflush(stderr);
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
