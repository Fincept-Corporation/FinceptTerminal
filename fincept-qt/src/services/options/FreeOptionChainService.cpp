#include "services/options/FreeOptionChainService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/options/CurveDataService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

namespace fincept::services::options {

namespace {

constexpr int kFreshSeconds = 5 * 60;
constexpr int kKeepSymbols = 6;
constexpr int kTimeoutMs = 90 * 1000;
constexpr qint64 kMaxOutputBytes = 64LL * 1024 * 1024;
constexpr double kFallbackRate = 0.04; ///< used only when no Treasury curve was ever loaded
const char* const kChainScript = "derivatives/deriv_market.py";

QThreadPool* focs_pool() {
    static QThreadPool* pool = [] {
        auto* p = new QThreadPool(&FreeOptionChainService::instance());
        p->setMaxThreadCount(2);
        p->setObjectName(QStringLiteral("DerivChains"));
        return p;
    }();
    return pool;
}

QString focs_path(const QString& key) {
    QString safe = key;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Z0-9._-]")), QStringLiteral("_"));
    return AppPaths::cache() + QStringLiteral("/derivatives/chain_") + safe + QStringLiteral(".json");
}

/// Parse + analyse off the GUI thread.
std::shared_ptr<market::FreeChain> focs_build(const QByteArray& bytes, const rates::ZeroCurve& curve) {
    auto c = std::make_shared<market::FreeChain>(market::parse_free_chain(bytes));
    if (c->ok())
        market::analyse_chain(*c, curve, kFallbackRate);
    return c;
}

} // namespace

FreeOptionChainService& FreeOptionChainService::instance() {
    static FreeOptionChainService s;
    return s;
}

FreeOptionChainService::FreeOptionChainService() : QObject(nullptr) {}

QString FreeOptionChainService::key_of(const QString& symbol) {
    QString k = symbol.trimmed().toUpper();
    if (k.startsWith(QLatin1Char('^')))
        k = k.mid(1);
    if (k == QLatin1String("GSPC"))
        k = QStringLiteral("SPX");
    return k;
}

FreeOptionChainService::ChainPtr FreeOptionChainService::cached(const QString& symbol) const {
    const auto it = memory_.constFind(key_of(symbol));
    return it == memory_.constEnd() ? nullptr : it->chain;
}

void FreeOptionChainService::cancel(quint64 request_id) {
    const auto it = pending_.find(request_id);
    if (it == pending_.end())
        return;
    if (it->cancel)
        it->cancel->store(true);
    pending_.erase(it);
}

void FreeOptionChainService::store(const QString& key, const ChainPtr& c, bool fresh) {
    Entry& e = memory_[key];
    e.chain = c;
    if (fresh)
        e.fresh_at = QDateTime::currentDateTimeUtc();
    lru_.removeAll(key);
    lru_.append(key);
    while (lru_.size() > kKeepSymbols)
        memory_.remove(lru_.takeFirst());
    emit chain_updated(key);
}

void FreeOptionChainService::deliver(quint64 id, const Reply& r, bool final_reply) {
    const auto it = pending_.find(id);
    if (it == pending_.end())
        return; // cancelled
    const QPointer<QObject> ctx = it->ctx;
    const Callback cb = it->cb;
    if (final_reply)
        pending_.erase(it);
    if (ctx && cb)
        cb(r);
}

quint64 FreeOptionChainService::fetch(const QString& symbol, const QString& source, bool force, QObject* ctx,
                                      Callback cb) {
    const quint64 id = ++next_id_;
    const QString key = key_of(symbol);
    auto token = std::make_shared<std::atomic<bool>>(false);
    pending_.insert(id, {token, QPointer<QObject>(ctx), std::move(cb)});
    if (key.isEmpty()) {
        Reply r;
        r.status = QStringLiteral("bad_symbol");
        r.message = tr("Choose a symbol first.");
        QMetaObject::invokeMethod(this, [this, id, r]() { deliver(id, r, true); }, Qt::QueuedConnection);
        return id;
    }
    const auto hit = memory_.constFind(key);
    const bool fresh = hit != memory_.constEnd() && hit->fresh_at.isValid() &&
                       hit->fresh_at.secsTo(QDateTime::currentDateTimeUtc()) < kFreshSeconds;
    if (!force && fresh) {
        Reply r;
        r.stage = Stage::Memory;
        r.chain = hit->chain;
        r.status = r.chain->status;
        QMetaObject::invokeMethod(this, [this, id, r]() { deliver(id, r, true); }, Qt::QueuedConnection);
        return id;
    }
    const rates::ZeroCurve curve = CurveDataService::instance().zero_curve_now();
    if (hit == memory_.constEnd()) {
        // Nothing in memory: the saved copy first (read and analysed off the GUI thread).
        const QString path = focs_path(key);
        (void)QtConcurrent::run(focs_pool(), [this, id, key, path, curve, token]() {
            QFile f(path);
            if (token->load() || !f.exists() || !f.open(QIODevice::ReadOnly))
                return;
            auto c = focs_build(f.readAll(), curve);
            if (!c->ok())
                return;
            QMetaObject::invokeMethod(
                this,
                [this, id, key, c]() {
                    if (!pending_.contains(id) || memory_.contains(key))
                        return; // cancelled, or the fresh chain won the race
                    store(key, c, false);
                    Reply r;
                    r.stage = Stage::Saved;
                    r.chain = c;
                    r.status = c->status;
                    deliver(id, r, false);
                },
                Qt::QueuedConnection);
        });
    }
    run_script(id, symbol, source);
    return id;
}

void FreeOptionChainService::run_script(quint64 id, const QString& symbol, const QString& source) {
    const auto it = pending_.find(id);
    if (it == pending_.end())
        return;
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = kTimeoutMs;
    opts.cancel_token = it->cancel;
    opts.max_output_bytes = kMaxOutputBytes;
    const QString key = key_of(symbol);
    const QString src = source.isEmpty() ? QStringLiteral("auto") : source;
    QStringList args = {QStringLiteral("chain"), symbol.trimmed(), QStringLiteral("--source"), src,
                        QStringLiteral("--max-expiries"), QStringLiteral("12")};
    LOG_INFO("Derivatives", QStringLiteral("Fetching delayed option chain %1 (%2)").arg(key, src));
    python::PythonRunner::instance().run_with_options(
        QString::fromLatin1(kChainScript), args, opts, [this, id, key](python::PythonResult res) {
            if (!pending_.contains(id))
                return;
            if (!res.success) {
                Reply r;
                r.stage = Stage::Failed;
                const bool cancelled = res.error == QLatin1String("Cancelled");
                r.status = cancelled ? QStringLiteral("cancelled") : QStringLiteral("feed_unavailable");
                r.message = res.error.isEmpty() ? tr("The option chain could not be loaded.") : res.error;
                LOG_WARN("Derivatives", "Option chain fetch failed: " + r.message);
                deliver(id, r, true);
                return;
            }
            const QString out = res.output;
            const rates::ZeroCurve curve = CurveDataService::instance().zero_curve_now();
            (void)QtConcurrent::run(focs_pool(), [this, id, key, out, curve]() {
                const QByteArray bytes = out.toUtf8();
                auto c = focs_build(bytes, curve);
                if (c->ok()) {
                    const QString path = focs_path(key);
                    QDir().mkpath(QFileInfo(path).absolutePath());
                    QSaveFile f(path);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(bytes);
                        f.commit();
                    }
                }
                QMetaObject::invokeMethod(
                    this,
                    [this, id, key, c]() {
                        if (!pending_.contains(id))
                            return;
                        Reply r;
                        r.status = c->status;
                        if (c->ok()) {
                            store(key, c, true);
                            r.stage = Stage::Fresh;
                            r.chain = c;
                        } else {
                            r.stage = Stage::Failed;
                            r.message = c->message.isEmpty() ? tr("The option chain could not be loaded.") : c->message;
                        }
                        deliver(id, r, true);
                    },
                    Qt::QueuedConnection);
            });
        });
}

} // namespace fincept::services::options
