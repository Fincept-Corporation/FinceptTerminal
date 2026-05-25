#include "app/PhaseOneStartupCoordinator.h"

#include "app/PhaseOneServerPaths.h"
#include "core/config/AppPaths.h"
#include "core/crash/CrashHandler.h"
#include "core/logging/Logger.h"
#include "multiuser/server/PhaseOneServerRuntime.h"

#include <QCoreApplication>
#include <QFile>
#include <QLibrary>
#include <QSslSocket>

#include <cstdio>

namespace fincept {

void PhaseOneStartupCoordinator::initialize_pre_app() const {
    qputenv("QT_TLS_BACKEND", "openssl");
    fprintf(stderr, "[PhaseOneStartup] QT_TLS_BACKEND=openssl\n");

#ifdef Q_OS_MACOS
    const QStringList crypto_candidates = {
        QStringLiteral("/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib"),
        QStringLiteral("/usr/local/opt/openssl@3/lib/libcrypto.3.dylib"),
    };
    const QStringList ssl_candidates = {
        QStringLiteral("/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib"),
        QStringLiteral("/usr/local/opt/openssl@3/lib/libssl.3.dylib"),
    };
    for (const auto& path : crypto_candidates) {
        if (QFile::exists(path)) {
            QLibrary(path).load();
            break;
        }
    }
    for (const auto& path : ssl_candidates) {
        if (QFile::exists(path)) {
            QLibrary(path).load();
            break;
        }
    }
#endif
}

void PhaseOneStartupCoordinator::initialize_post_app() const {
    const auto backends = QSslSocket::availableBackends();
    if (QSslSocket::activeBackend() != QStringLiteral("openssl")
        && backends.contains(QStringLiteral("openssl"))) {
        QSslSocket::setActiveBackend(QStringLiteral("openssl"));
    }
}

void PhaseOneStartupCoordinator::install_qt_message_handler() const {
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
        const char* category = (ctx.category && *ctx.category) ? ctx.category : "Qt";
        switch (type) {
        case QtDebugMsg:    fincept::Logger::instance().debug(category, msg); break;
        case QtInfoMsg:     fincept::Logger::instance().info(category, msg); break;
        case QtWarningMsg:  fincept::Logger::instance().warn(category, msg); break;
        case QtCriticalMsg: fincept::Logger::instance().error(category, msg); break;
        case QtFatalMsg:
            fincept::Logger::instance().error(category, msg);
            fincept::Logger::instance().flush_and_close();
            break;
        }
    });
}

void PhaseOneStartupCoordinator::configure_client_logging() const {
    Logger::instance().set_file(AppPaths::logs() + QStringLiteral("/fincept.log"));
    install_qt_message_handler();
}

std::optional<int> PhaseOneStartupCoordinator::maybe_run(int& argc, char* argv[],
                                                         const PhaseOneServerCliOptions& options) const {
    if (options.mode != PhaseOneProcessMode::Server)
        return std::nullopt;

    const PhaseOneResolvedServerOptions resolved = PhaseOneServerPaths::resolve(options);
    AppPaths::set_overrides(resolved.app_path_overrides);
    AppPaths::ensure_all();
    crash::install();

    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FinceptTerminal"));
    app.setOrganizationName(QStringLiteral("Fincept"));
#ifndef FINCEPT_VERSION_STRING
#    define FINCEPT_VERSION_STRING "0.0.0-dev"
#endif
    app.setApplicationVersion(QStringLiteral(FINCEPT_VERSION_STRING));

    initialize_post_app();
    Logger::instance().set_file(AppPaths::logs() + QStringLiteral("/phase1-server.log"));
    install_qt_message_handler();

    if (resolved.ignored_profile) {
        LOG_INFO("PhaseOneServer",
                 QString("Ignoring --profile=%1 for server mode; using server-specific paths under %2")
                     .arg(resolved.requested_profile, resolved.server_root));
    }

    PhaseOneServerRuntime runtime;
    const PhaseOneServerStartResult result = runtime.start(resolved);
    if (!result.ok) {
        LOG_ERROR("PhaseOneServer", result.error_context.message);
        Logger::instance().flush_and_close();
        return 1;
    }

    return app.exec();
}

} // namespace fincept
