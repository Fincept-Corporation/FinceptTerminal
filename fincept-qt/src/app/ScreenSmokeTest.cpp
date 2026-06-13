#include "app/ScreenSmokeTest.h"

#include "app/DockScreenRouter.h"
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
        std::fprintf(stderr, "[Smoke] OK: all %d screens constructed\n",
                     static_cast<int>(ids.size()));
        std::fflush(stderr);
        LOG_INFO("Smoke", "All screens constructed OK");
        return 0;
    }

    std::fprintf(stderr, "[Smoke] FAIL: %d screen(s) did not construct: %s\n",
                 static_cast<int>(failures.size()), qUtf8Printable(failures.join(", ")));
    std::fflush(stderr);
    LOG_ERROR("Smoke", QString("%1 screen(s) failed: %2")
                           .arg(failures.size())
                           .arg(failures.join(", ")));
    return 1;
}

} // namespace fincept
