#include "app/PhaseOneServerCli.h"

#include <QStringView>

namespace fincept {

namespace {

PhaseOneServerCliParseResult parse_error(const PhaseOneServerCliOptions& options, const QString& error_code,
                                         const QString& message) {
    return {false, options, error_code, message};
}

bool requires_value(const QString& arg) {
    return arg == QStringLiteral("--server-host") || arg == QStringLiteral("--server-port")
           || arg == QStringLiteral("--server-db") || arg == QStringLiteral("--profile");
}

} // namespace

PhaseOneServerCliParseResult PhaseOneServerCli::parse(int argc, const char* const argv[]) {
    PhaseOneServerCliOptions options;

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg == QStringLiteral("--")) {
            for (int j = i + 1; j < argc; ++j)
                options.client_passthrough_args.append(QString::fromUtf8(argv[j]));
            break;
        }

        if (arg == QStringLiteral("--server")) {
            options.mode = PhaseOneProcessMode::Server;
            continue;
        }

        if (requires_value(arg)) {
            if (i + 1 >= argc)
                return parse_error(options, QStringLiteral("missing_cli_value"),
                                   QStringLiteral("Missing value for %1.").arg(arg));

            const QString value = QString::fromUtf8(argv[++i]);
            if (arg == QStringLiteral("--server-host")) {
                options.host = value.trimmed();
            } else if (arg == QStringLiteral("--server-port")) {
                bool ok = false;
                const uint port = value.toUInt(&ok);
                if (!ok || port > 65535U)
                    return parse_error(options, QStringLiteral("invalid_server_port"),
                                       QStringLiteral("Invalid value for --server-port."));
                options.port = static_cast<quint16>(port);
            } else if (arg == QStringLiteral("--server-db")) {
                options.db_path = value.trimmed();
            } else if (arg == QStringLiteral("--profile")) {
                options.profile = value.trimmed();
            }
            continue;
        }

        options.client_passthrough_args.append(arg);
    }

    return {true, options, {}, {}};
}

} // namespace fincept
