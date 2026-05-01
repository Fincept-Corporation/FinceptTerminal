#include "core/telemetry/TelemetryProvider.h"

#include "core/logging/Logger.h"

#include <QMutex>
#include <QMutexLocker>

namespace fincept::telemetry {

namespace {
QMutex& sink_mutex() {
    static QMutex m;
    return m;
}
} // namespace

TelemetrySink& TelemetrySink::instance() {
    static TelemetrySink s;
    return s;
}

void TelemetrySink::set_provider(TelemetryProvider* provider) {
    QMutexLocker lock(&sink_mutex());
    provider_ = provider;
    LOG_INFO("Telemetry", provider ? "Provider installed" : "Provider uninstalled");
}

void TelemetrySink::record(const QString& event_id, const QVariantMap& payload) {
    TelemetryProvider* p;
    {
        QMutexLocker lock(&sink_mutex());
        p = provider_;
    }
    if (!p)
        return;
    p->record(event_id, payload);
}

bool TelemetrySink::has_provider() const {
    QMutexLocker lock(&sink_mutex());
    return provider_ != nullptr;
}

} // namespace fincept::telemetry
