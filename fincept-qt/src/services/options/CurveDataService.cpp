#include "services/options/CurveDataService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QSaveFile>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

namespace fincept::services::options {

namespace {

constexpr int kTreasuryMaxAgeS = 6 * 3600;
constexpr int kFuturesMaxAgeS = 5 * 60;
constexpr int kScriptTimeoutMs = 60 * 1000;
const char* const kCurveScript = "derivatives/deriv_market.py";

QString cds_dir() {
    return AppPaths::cache() + QStringLiteral("/derivatives");
}

QString cds_treasury_path() {
    return cds_dir() + QStringLiteral("/treasury_curve.json");
}

QThreadPool* cds_pool() {
    static QThreadPool* pool = [] {
        auto* p = new QThreadPool(&CurveDataService::instance());
        p->setMaxThreadCount(1);
        p->setObjectName(QStringLiteral("DerivCurves"));
        return p;
    }();
    return pool;
}

void cds_save(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(bytes);
        f.commit();
    }
}

QString cds_tenor_text(double years) {
    if (years < 1.0)
        return QStringLiteral("%1M").arg(std::max(1, static_cast<int>(std::lround(years * 12))));
    return QStringLiteral("%1Y").arg(std::lround(years));
}

} // namespace

CurveDataService& CurveDataService::instance() {
    static CurveDataService s;
    return s;
}

CurveDataService::CurveDataService() : QObject(nullptr) {}

void CurveDataService::cancel(quint64 request_id) {
    const auto it = pending_.find(request_id);
    if (it == pending_.end())
        return;
    if (it->cancel)
        it->cancel->store(true);
    pending_.erase(it);
}

CurveDataService::TreasuryPtr CurveDataService::treasury_now() {
    if (treasury_)
        return treasury_;
    if (!disk_checked_) {
        disk_checked_ = true;
        QFile f(cds_treasury_path());
        if (f.size() < 256 * 1024 && f.open(QIODevice::ReadOnly)) { // ~1 KB: a synchronous read is fine
            auto c = std::make_shared<market::TreasuryCurve>(market::parse_treasury_curve(f.readAll()));
            if (c->ok())
                treasury_ = c; // loaded_at stays invalid: refreshed on the next request
        }
    }
    return treasury_;
}

rates::ZeroCurve CurveDataService::zero_curve_now() {
    const TreasuryPtr c = treasury_now();
    return c ? market::zero_curve(*c) : rates::ZeroCurve{};
}

double CurveDataService::rate_for(double years, double fallback, QString* label) {
    const TreasuryPtr c = treasury_now();
    if (!c) {
        if (label)
            *label = tr("assumed %1 % (Treasury curve not loaded)").arg(QString::number(fallback * 100, 'f', 2));
        return fallback;
    }
    const rates::ZeroCurve z = market::zero_curve(*c);
    if (z.empty())
        return fallback;
    const double r = z.rate_at(std::max(years, 1.0 / 365.0));
    if (label)
        *label = tr("UST %1 zero %2 % (curve of %3)")
                     .arg(cds_tenor_text(years), QString::number(r * 100, 'f', 2), c->date.toString(QStringLiteral("d MMM")));
    return r;
}

quint64 CurveDataService::treasury_curve(bool force, QObject* ctx, TreasuryCallback cb) {
    const quint64 id = ++next_id_;
    QPointer<QObject> guard(ctx);
    if (!force && treasury_ && treasury_loaded_at_.isValid() &&
        treasury_loaded_at_.secsTo(QDateTime::currentDateTimeUtc()) < kTreasuryMaxAgeS) {
        TreasuryReply r;
        r.curve = treasury_;
        QMetaObject::invokeMethod(
            this, [guard, cb, r]() { if (guard) cb(r); }, Qt::QueuedConnection);
        return id;
    }
    auto token = std::make_shared<std::atomic<bool>>(false);
    pending_.insert(id, {token});
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = kScriptTimeoutMs;
    opts.cancel_token = token;
    opts.max_output_bytes = 1024 * 1024;
    python::PythonRunner::instance().run_with_options(
        QString::fromLatin1(kCurveScript), {QStringLiteral("treasury_curve")}, opts,
        [this, id, guard, cb](python::PythonResult res) {
            if (!pending_.contains(id))
                return; // cancelled
            const QString out = res.output;
            const bool ran = res.success;
            const QString err = res.error;
            (void)QtConcurrent::run(cds_pool(), [this, id, guard, cb, out, ran, err]() {
                const QByteArray bytes = out.toUtf8();
                auto parsed = std::make_shared<market::TreasuryCurve>(
                    ran ? market::parse_treasury_curve(bytes) : market::TreasuryCurve{});
                if (!ran) {
                    parsed->status = QStringLiteral("feed_unavailable");
                    parsed->message = err.isEmpty() ? tr("The Treasury curve script did not run.") : err;
                }
                if (parsed->ok())
                    cds_save(cds_treasury_path(), bytes);
                QMetaObject::invokeMethod(
                    this,
                    [this, id, guard, cb, parsed]() {
                        if (!pending_.contains(id))
                            return;
                        pending_.remove(id);
                        TreasuryReply r;
                        if (parsed->ok()) {
                            treasury_ = parsed;
                            treasury_loaded_at_ = QDateTime::currentDateTimeUtc();
                            r.curve = treasury_;
                            emit treasury_updated();
                        } else {
                            LOG_WARN("Derivatives", "Treasury curve refresh failed: " + parsed->message);
                            r.message = parsed->message.isEmpty() ? tr("The Treasury curve could not be loaded.")
                                                                  : parsed->message;
                            r.curve = treasury_now();
                            r.stale = r.curve != nullptr;
                        }
                        if (guard)
                            cb(r);
                    },
                    Qt::QueuedConnection);
            });
        });
    return id;
}

quint64 CurveDataService::futures_curve(const QString& root_in, int months, bool force, QObject* ctx,
                                        FuturesCallback cb) {
    const quint64 id = ++next_id_;
    const QString root = root_in.trimmed().toUpper();
    QPointer<QObject> guard(ctx);
    const auto hit = futures_.constFind(root);
    if (!force && hit != futures_.constEnd() && hit->at.secsTo(QDateTime::currentDateTimeUtc()) < kFuturesMaxAgeS) {
        FuturesReply r;
        r.curve = hit->curve;
        QMetaObject::invokeMethod(
            this, [guard, cb, r]() { if (guard) cb(r); }, Qt::QueuedConnection);
        return id;
    }
    auto token = std::make_shared<std::atomic<bool>>(false);
    pending_.insert(id, {token});
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = kScriptTimeoutMs;
    opts.cancel_token = token;
    opts.max_output_bytes = 1024 * 1024;
    python::PythonRunner::instance().run_with_options(
        QString::fromLatin1(kCurveScript),
        {QStringLiteral("futures_curve"), root, QStringLiteral("--months"), QString::number(std::clamp(months, 2, 24))},
        opts, [this, id, guard, cb, root](python::PythonResult res) {
            if (!pending_.contains(id))
                return;
            const QString out = res.output;
            const bool ran = res.success;
            const QString err = res.error;
            (void)QtConcurrent::run(cds_pool(), [this, id, guard, cb, out, ran, err, root]() {
                auto parsed = std::make_shared<market::FuturesCurve>(
                    ran ? market::parse_futures_curve(out.toUtf8()) : market::FuturesCurve{});
                if (!ran) {
                    parsed->status = QStringLiteral("feed_unavailable");
                    parsed->message = err.isEmpty() ? tr("The futures curve script did not run.") : err;
                }
                QMetaObject::invokeMethod(
                    this,
                    [this, id, guard, cb, parsed, root]() {
                        if (!pending_.contains(id))
                            return;
                        pending_.remove(id);
                        FuturesReply r;
                        if (parsed->ok()) {
                            futures_.insert(root, {parsed, QDateTime::currentDateTimeUtc()});
                            r.curve = parsed;
                        } else {
                            r.message = parsed->message.isEmpty() ? tr("The futures curve could not be loaded.")
                                                                  : parsed->message;
                            const auto old = futures_.constFind(root);
                            if (old != futures_.constEnd())
                                r.curve = old->curve;
                        }
                        if (guard)
                            cb(r);
                    },
                    Qt::QueuedConnection);
            });
        });
    return id;
}

} // namespace fincept::services::options
