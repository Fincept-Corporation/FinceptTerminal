#include "app/PhaseOneClientUiStartup.h"

#include "app/PhaseOneClientLifecycle.h"
#include "app/PhaseOneClientStartup.h"
#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "python/PythonSetupManager.h"
#include "screens/recovery/CrashRecoveryDialog.h"
#include "screens/setup/SetupScreen.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"
#include "storage/workspace/CrashRecovery.h"

#include <QApplication>
#include <QPointer>
#include <QTimer>

#include <cstdio>

namespace fincept {

namespace {

void maybe_log_missing_llm_provider() {
    if (!ai_chat::LlmService::instance().is_configured()) {
        LOG_WARN("App",
                 "LLM provider not configured — AI chat will prompt user to configure Settings → LLM Config");
    }
}

void maybe_restore_windows() {
    bool recovered = false;
    if (auto* recovery = TerminalShell::instance().crash_recovery(); recovery && recovery->needs_recovery()) {
        screens::CrashRecoveryDialog dialog(recovery, TerminalShell::instance().snapshot_ring());
        dialog.exec();
        recovered = dialog.was_restored();
    }

    if (!recovered) {
        auto* primary = new WindowFrame(0);
        primary->setAttribute(Qt::WA_DeleteOnClose);
        primary->show();
    }

    if (!recovered) {
        const QList<int> saved_ids = SessionManager::instance().load_window_ids();
        for (int id : saved_ids) {
            if (id <= 0)
                continue;
            auto* window = new WindowFrame(id);
            window->setAttribute(Qt::WA_DeleteOnClose);
            window->show();
        }
        if (saved_ids.size() > 1) {
            LOG_INFO("App", QStringLiteral("Restored %1 secondary window(s) from last session")
                                .arg(saved_ids.size() - 1));
        }
    }
}

void finish_client_ui_startup(QApplication& app, PhaseOneClientLifecycle& lifecycle,
                              const PhaseOneClientStartup& startup, const bool after_setup) {
    KeyConfigManager::instance();
    maybe_restore_windows();
    lifecycle.wire(app);
    startup.schedule_agent_discovery(app);
    maybe_log_missing_llm_provider();
    LOG_INFO("App", after_setup ? QStringLiteral("Application ready (after setup)")
                                 : QStringLiteral("Application ready"));
}

void maybe_start_background_package_sync(const python::SetupStatus& setup_status) {
    if (!setup_status.needs_package_sync)
        return;

    LOG_INFO("App", "Requirements changed — syncing packages in background");
    auto& manager = python::PythonSetupManager::instance();
    QObject::connect(&manager, &python::PythonSetupManager::setup_complete, &manager,
                     [](const bool success, const QString& error) {
        if (success)
            LOG_INFO("App", "Background package sync completed successfully");
        else
            LOG_WARN("App", QStringLiteral("Background package sync failed (non-fatal): ") + error);
    }, Qt::SingleShotConnection);
    manager.run_setup();
}

} // namespace

void PhaseOneClientUiStartup::initialize_pre_app() {
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    fprintf(stderr, "[PhaseOneStartup] enabled Qt::AA_ShareOpenGLContexts\n");
}

int PhaseOneClientUiStartup::run(QApplication& app, PhaseOneClientLifecycle& lifecycle,
                                 const PhaseOneClientStartup& startup,
                                 const python::SetupStatus& setup_status) const {
    if (setup_status.needs_setup) {
        LOG_INFO("App", "Python environment not ready — showing setup screen");

        auto* setup_screen = new screens::SetupScreen;
        QPointer<screens::SetupScreen> screen_guard(setup_screen);
        setup_screen->setWindowTitle(QStringLiteral("Fincept Terminal — First-Time Setup"));
        setup_screen->resize(800, 600);
        setup_screen->show();

        QObject::connect(setup_screen, &screens::SetupScreen::setup_complete, &app,
                         [&app, &lifecycle, &startup, screen_guard]() {
            if (!screen_guard)
                return;

            screen_guard->hide();
            screen_guard->deleteLater();
            finish_client_ui_startup(app, lifecycle, startup, true);
        }, Qt::SingleShotConnection);

        return app.exec();
    }

    finish_client_ui_startup(app, lifecycle, startup, false);
    maybe_start_background_package_sync(setup_status);
    return app.exec();
}

} // namespace fincept
