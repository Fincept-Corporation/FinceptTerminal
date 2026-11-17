#include "services/alt_investments/AltInvestmentsService.h"

#include "core/logging/Logger.h"
#include "services/python_cli/PythonCliService.h"
#include "storage/cache/CacheManager.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>

namespace fincept::services::alt {

namespace {

QByteArray alt_svc_compact(const QJsonObject& o) {
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QStringList alt_svc_args(const AltCall& c) {
    if (c.command.isEmpty())
        return c.args;
    QStringList a{c.command};
    a += c.args;
    a << QStringLiteral("--stdin");
    return a;
}

} // namespace

AltInvestmentsService& AltInvestmentsService::instance() {
    static AltInvestmentsService s;
    return s;
}

AltInvestmentsService::AltInvestmentsService(QObject* parent) : QObject(parent) {}

QString AltInvestmentsService::cache_key(const AltCall& call) {
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(call.script.toUtf8());
    h.addData(QByteArrayLiteral("\x1f"));
    h.addData(alt_svc_args(call).join(QChar(0x1f)).toUtf8());
    h.addData(QByteArrayLiteral("\x1f"));
    if (!call.command.isEmpty())
        h.addData(alt_svc_compact(call.input));
    return QStringLiteral("alt:svc:") + QString::fromLatin1(h.result().toHex());
}

bool AltInvestmentsService::cached(const AltCall& call, AltReply* out) const {
    const QVariant v = fincept::CacheManager::instance().get(cache_key(call));
    if (v.isNull())
        return false;
    const QJsonObject wrap = QJsonDocument::fromJson(v.toString().toUtf8()).object();
    if (wrap.isEmpty())
        return false;
    if (out) {
        out->ok = true;
        out->raw = wrap.value(QStringLiteral("raw")).toObject();
        const QJsonValue data = out->raw.value(QStringLiteral("data"));
        out->data = data.isObject() ? data.toObject() : out->raw;
        out->from_cache = true;
        out->as_of = QDateTime::fromMSecsSinceEpoch(qint64(wrap.value(QStringLiteral("as_of")).toDouble()));
        out->source = wrap.value(QStringLiteral("source")).toString();
        out->error.clear();
        out->cancelled = false;
    }
    return true;
}

void AltInvestmentsService::drop_cached(const AltCall& call) {
    fincept::CacheManager::instance().remove(cache_key(call));
}

quint64 AltInvestmentsService::call(const AltCall& c, QObject* ctx, Callback cb) {
    const quint64 id = ++next_id_;
    QPointer<QObject> guard = ctx;
    const bool has_ctx = ctx != nullptr;

    if (c.cache_ttl_sec > 0) {
        AltReply hit;
        if (cached(c, &hit)) {
            if (cb && (!has_ctx || guard))
                cb(hit);
            return id;
        }
    }

    python_cli::CliRunOptions opts;
    opts.timeout_ms = c.timeout_ms;
    opts.cancel_token = std::make_shared<std::atomic<bool>>(false);
    if (!c.command.isEmpty())
        opts.stdin_data = alt_svc_compact(c.input);
    opts.max_output_bytes = 32LL * 1024 * 1024;
    pending_.insert(id, Pending{opts.cancel_token});

    const QString script = c.script;
    const QString key = c.cache_ttl_sec > 0 ? cache_key(c) : QString();
    const int ttl = c.cache_ttl_sec;
    const QString source = c.source;
    const QString cmd = c.command.isEmpty() ? c.args.value(0) : c.command;
    QPointer<AltInvestmentsService> self = this;
    python_cli::PythonCliService::instance().run(
        script, alt_svc_args(c), opts,
        [self, id, guard, has_ctx, cb, key, ttl, source, script, cmd](const python_cli::CliResult& res) {
            if (!self)
                return;
            const auto it = self->pending_.find(id);
            const bool was_cancelled = it == self->pending_.end() || (it->cancel && it->cancel->load());
            if (it != self->pending_.end())
                self->pending_.erase(it);
            if (was_cancelled || res.cancelled)
                return; // cancel(): the caller no longer wants it
            if (has_ctx && !guard)
                return;
            AltReply r;
            r.as_of = QDateTime::currentDateTime();
            r.source = source;
            if (!res.success) {
                r.error = plain_error(res.error);
                LOG_WARN("AltInvestments", QStringLiteral("Script call failed script=%1 command=%2 reason=%3")
                                               .arg(script.section(QLatin1Char('/'), -1), cmd, res.error.left(160)));
                if (cb)
                    cb(r);
                return;
            }
            r.ok = true;
            r.raw = res.data;
            const QJsonValue data = res.data.value(QStringLiteral("data"));
            r.data = data.isObject() ? data.toObject() : res.data;
            if (!key.isEmpty() && ttl > 0) {
                QJsonObject wrap{{QStringLiteral("raw"), r.raw},
                                 {QStringLiteral("as_of"), double(r.as_of.toMSecsSinceEpoch())},
                                 {QStringLiteral("source"), source}};
                fincept::CacheManager::instance().put(key, QString::fromUtf8(alt_svc_compact(wrap)), ttl,
                                                      QStringLiteral("alt_investments"));
            }
            if (cb)
                cb(r);
        });
    return id;
}

void AltInvestmentsService::cancel(quint64 id) {
    const auto it = pending_.find(id);
    if (it == pending_.end())
        return;
    if (it->cancel)
        it->cancel->store(true);
    // Kept until the runner answers (it then drops the reply as cancelled).
}

QString AltInvestmentsService::plain_error(const QString& raw_in) {
    const QString raw = raw_in.trimmed();
    if (raw.isEmpty())
        return tr("The analysis failed without a message.");
    static const QRegularExpression kTimeout(QStringLiteral("timed out after (\\d+)"), QRegularExpression::CaseInsensitiveOption);
    const auto m = kTimeout.match(raw);
    if (m.hasMatch())
        return tr("The analysis took longer than %1 s and was stopped. Try again, or with fewer inputs.").arg(m.captured(1));
    if (raw.contains(QLatin1String("No module named"), Qt::CaseInsensitive))
        return tr("A Python package this analysis needs is missing. Open Settings > Python Environment to repair it.");
    if (raw.contains(QLatin1String("Python is not ready"), Qt::CaseInsensitive) ||
        raw.contains(QLatin1String("interpreter"), Qt::CaseInsensitive))
        return tr("The Python environment is not ready yet. It is set up in Settings > Python Environment.");
    if (raw.contains(QLatin1String("HTTP 429"), Qt::CaseInsensitive) ||
        raw.contains(QLatin1String("Too Many Requests"), Qt::CaseInsensitive))
        return tr("The free data source is busy (rate limit). Try again in a minute.");
    if (raw.contains(QLatin1String("Max retries"), Qt::CaseInsensitive) ||
        raw.contains(QLatin1String("Connection"), Qt::CaseInsensitive) ||
        raw.contains(QLatin1String("getaddrinfo"), Qt::CaseInsensitive))
        return tr("No reply from the data source (no connection?). Enter the values by hand or try again.");
    // Script messages are already sentences; keep the first line, never a traceback.
    const QString first = raw.section(QLatin1Char('\n'), 0, 0).trimmed();
    if (first.startsWith(QLatin1String("Traceback")))
        return tr("The analysis failed unexpectedly. Check the inputs and try again.");
    return first.left(400);
}

} // namespace fincept::services::alt
