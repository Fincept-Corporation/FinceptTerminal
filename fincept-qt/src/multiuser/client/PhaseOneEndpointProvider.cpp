#include "multiuser/client/PhaseOneEndpointProvider.h"

#include "core/config/AppConfig.h"
#include "multiuser/contracts/PhaseOneAuthTypes.h"

#include <QRegularExpression>
#include <QUrl>

namespace fincept::multiuser {

namespace {

QString trim_trailing_slash(QString value) {
    while (value.endsWith('/'))
        value.chop(1);
    return value;
}

bool matches_host_only_shape(const QString& value) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9.-]+$"));
    return pattern.match(value).hasMatch();
}

PhaseOneEndpointResolution invalid_resolution(const QString& error_code, const QString& message) {
    return {false, {}, error_code, message};
}

PhaseOneEndpointResolution normalize_value(const QString& raw_value) {
    const QString trimmed = trim_trailing_slash(raw_value.trimmed());
    if (trimmed.isEmpty())
        return invalid_resolution("server_endpoint_unset", "Phase one server address is not configured.");

    const QString hosted_default = trim_trailing_slash(fincept::AppConfig::instance().api_base_url().trimmed());
    if (trimmed.compare(hosted_default, Qt::CaseInsensitive) == 0) {
        return invalid_resolution("invalid_server_endpoint",
                                  "Phase one server address must not use the hosted API default.");
    }

    if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
        const QUrl url(trimmed);
        if (!url.isValid() || url.host().isEmpty() || url.port() <= 0 || !url.userName().isEmpty() || url.hasQuery() ||
            url.hasFragment()) {
            return invalid_resolution("invalid_server_endpoint", "Phase one server address must be http/https host:port.");
        }

        const QString path = url.path();
        if (!path.isEmpty() && path != "/") {
            return invalid_resolution("invalid_server_endpoint", "Phase one server address cannot include a path.");
        }

        return {true,
                QStringLiteral("%1://%2:%3").arg(url.scheme(), url.host(), QString::number(url.port())),
                {},
                {}};
    }

    if (!matches_host_only_shape(trimmed)) {
        return invalid_resolution("invalid_server_endpoint", "Phase one server address must be a bare host or http/https host:port.");
    }

    return {true,
            QStringLiteral("http://%1:%2").arg(trimmed, QString::number(kPhaseOneDefaultPort)),
            {},
            {}};
}

} // namespace

PhaseOneEndpointProvider& PhaseOneEndpointProvider::instance() {
    static PhaseOneEndpointProvider provider;
    return provider;
}

PhaseOneEndpointResolution PhaseOneEndpointProvider::resolve() const {
    const QString env_value = QString::fromUtf8(qgetenv("FINCEPT_PHASE1_SERVER_URL")).trimmed();
    if (!env_value.isEmpty())
        return normalize_value(env_value);

    const QString config_value = fincept::AppConfig::instance().get_phase_one_server_url().trimmed();
    if (!config_value.isEmpty())
        return normalize_value(config_value);

    return invalid_resolution("server_endpoint_unset", "Phase one server address is not configured.");
}

} // namespace fincept::multiuser
