// src/services/equity/SecFilingsService.cpp
#include "services/equity/SecFilingsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>

namespace fincept::services::equity {

namespace {

constexpr const char* kSecFilingsServiceTag = "SecFilingsService";
constexpr const char* kSecSvcScript = "mcp/edgar/main.py";
constexpr int kSecSvcLkgTtlSec = 30 * 24 * 3600; // last-known-good copy shown while refreshing

QString sec_svc_section_tag(SecSection s) {
    switch (s) {
        case SecSection::Filings:
            return QStringLiteral("filings");
        case SecSection::Financials:
            return QStringLiteral("financials");
        case SecSection::Insiders:
            return QStringLiteral("insiders");
        case SecSection::Holders:
            return QStringLiteral("holders");
    }
    return QStringLiteral("filings");
}

// Fresh-cache lifetime per section. A new 8-K / Form 4 can land any business day, so those
// lists refresh well inside a session; statements and 13F holdings change quarterly.
int sec_svc_ttl(SecSection s) {
    switch (s) {
        case SecSection::Filings:
            return 15 * 60;
        case SecSection::Financials:
            return 6 * 3600;
        case SecSection::Insiders:
            return 60 * 60;
        case SecSection::Holders:
            return 6 * 3600;
    }
    return 15 * 60;
}

QStringList sec_svc_argv(SecSection s, const QString& sym) {
    switch (s) {
        case SecSection::Filings:
            return {QStringLiteral("research_filings"), sym, QStringLiteral("150")};
        case SecSection::Financials:
            return {QStringLiteral("research_financials"), sym};
        case SecSection::Insiders:
            return {QStringLiteral("research_insiders"), sym, QStringLiteral("25")};
        case SecSection::Holders:
            return {QStringLiteral("research_holders"), sym, QStringLiteral("10")};
    }
    return {};
}

// The edgar scripts report failures as {"error": "<text>"} (research_* commands) or
// {"error": {"command", "error", "details"}} (older commands); PythonRunner hands either
// back as text. Pull out the human sentence and drop tracebacks.
QString sec_svc_error_text(const QString& raw) {
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return SecFilingsService::tr("The SEC EDGAR request failed.");
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8());
    if (doc.isObject()) {
        const QJsonValue err = doc.object().value(QStringLiteral("error"));
        if (err.isString() && !err.toString().trimmed().isEmpty())
            return err.toString().trimmed();
        if (err.isObject()) {
            const QString inner = err.toObject().value(QStringLiteral("error")).toString().trimmed();
            if (!inner.isEmpty())
                return inner;
        }
    }
    if (trimmed.contains(QLatin1String("Traceback (most recent call last)")))
        return SecFilingsService::tr("The SEC EDGAR script failed (see the application log).");
    return trimmed.left(400);
}

bool sec_svc_unwrap(const QString& blob, SecSectionData& out) {
    const QJsonObject o = QJsonDocument::fromJson(blob.toUtf8()).object();
    if (!o.contains(QStringLiteral("data")))
        return false;
    out.data = o.value(QStringLiteral("data")).toObject();
    out.fetched_at_ms = static_cast<qint64>(o.value(QStringLiteral("fetched_at_ms")).toDouble());
    return true;
}

} // namespace

SecFilingsService& SecFilingsService::instance() {
    static SecFilingsService inst;
    return inst;
}

SecFilingsService::SecFilingsService(QObject* parent) : QObject(parent) {}

bool SecFilingsService::is_us_listing(const QString& symbol, QString* reason) {
    const QString s = symbol.trimmed().toUpper();
    auto fail = [reason](const QString& why) {
        if (reason)
            *reason = why;
        return false;
    };
    if (s.isEmpty())
        return fail(tr("No symbol selected."));
    if (s.startsWith(QLatin1Char('^')))
        return fail(tr("%1 is an index — SEC EDGAR filings exist only for registered issuers.").arg(s));
    if (s.contains(QLatin1Char('=')))
        return fail(tr("%1 is a currency or futures contract — SEC EDGAR has no filings for it.").arg(s));
    static const QRegularExpression kCryptoPair(QStringLiteral("-(USD|USDT|USDC|BUSD|EUR|GBP|JPY|INR|BTC|ETH)$"));
    if (kCryptoPair.match(s).hasMatch())
        return fail(tr("%1 is a crypto pair — SEC EDGAR covers US-registered issuers only.").arg(s));
    // yfinance writes share classes with '-' (BRK-B); a '.' is always a non-US exchange suffix.
    const qsizetype dot = s.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
        return fail(tr("%1 trades outside the US (exchange suffix .%2). SEC EDGAR covers US-registered issuers "
                       "only — open the company's US ticker or ADR instead.")
                        .arg(s, s.mid(dot + 1)));
    return true;
}

void SecFilingsService::fetch(const QString& symbol, SecSection section, bool force) {
    const QString sym = symbol.trimmed().toUpper();
    QString reason;
    if (!is_us_listing(sym, &reason)) {
        emit section_failed(sym, section, reason);
        return;
    }
    const QString tag = sec_svc_section_tag(section);
    const QString fresh_key = QStringLiteral("sec:%1:%2").arg(tag, sym);
    const QString lkg_key = QStringLiteral("sec:lkg:%1:%2").arg(tag, sym);
    auto& cache = fincept::CacheManager::instance();

    if (!force) {
        if (const auto hit = cache.try_get(fresh_key)) {
            SecSectionData d;
            if (sec_svc_unwrap(*hit, d)) {
                d.symbol = sym;
                d.section = section;
                emit section_loaded(d);
                return;
            }
        }
        // Show the last copy we had at once (flagged stale) while the refresh runs.
        if (const auto lkg = cache.try_get(lkg_key)) {
            SecSectionData d;
            if (sec_svc_unwrap(*lkg, d)) {
                d.symbol = sym;
                d.section = section;
                d.stale = true;
                emit section_loaded(d);
            }
        }
    }

    run_shared(fresh_key, sec_svc_argv(section, sym),
               [this, sym, section, fresh_key, lkg_key](bool ok, const QJsonObject& payload, const QString& error) {
                   if (!ok) {
                       LOG_WARN(kSecFilingsServiceTag,
                                QString("%1 %2 failed: %3").arg(sec_svc_section_tag(section), sym, error));
                       emit section_failed(sym, section, error);
                       return;
                   }
                   const qint64 now = QDateTime::currentMSecsSinceEpoch();
                   const QJsonObject wrapped{{"fetched_at_ms", static_cast<double>(now)},
                                             {"data", payload.value(QStringLiteral("data")).toObject()}};
                   const QVariant blob(QString::fromUtf8(QJsonDocument(wrapped).toJson(QJsonDocument::Compact)));
                   auto& c = fincept::CacheManager::instance();
                   c.put(fresh_key, blob, sec_svc_ttl(section), QStringLiteral("edgar"));
                   c.put(lkg_key, blob, kSecSvcLkgTtlSec, QStringLiteral("edgar"));

                   SecSectionData d;
                   d.symbol = sym;
                   d.section = section;
                   d.data = wrapped.value(QStringLiteral("data")).toObject();
                   d.fetched_at_ms = now;
                   emit section_loaded(d);
               });
}

void SecFilingsService::run_command(const QStringList& argv, int ttl_sec, CommandCallback done) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this, argv, ttl_sec, done]() { run_command(argv, ttl_sec, done); }, Qt::QueuedConnection);
        return;
    }
    if (!done)
        return;
    // Same key format the MCP EDGAR tools always used, so their existing cache entries still hit.
    const QString key = QStringLiteral("edgar:") + argv.join(QLatin1Char(':'));
    if (const auto hit = fincept::CacheManager::instance().try_get(key)) {
        const QJsonDocument doc = QJsonDocument::fromJson(hit->toUtf8());
        if (doc.isObject()) {
            done(true, doc.object(), {});
            return;
        }
    }
    run_shared(key, argv, [key, ttl_sec, done](bool ok, const QJsonObject& payload, const QString& error) {
        if (ok && ttl_sec > 0)
            fincept::CacheManager::instance().put(
                key, QVariant(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact))), ttl_sec,
                QStringLiteral("edgar"));
        done(ok, payload, error);
    });
}

void SecFilingsService::run_shared(const QString& key, const QStringList& argv, CommandCallback done) {
    auto it = waiters_.find(key);
    if (it != waiters_.end()) {
        it->append(std::move(done)); // identical request already running — share its result
        return;
    }
    waiters_.insert(key, {std::move(done)});

    QPointer<SecFilingsService> self = this;
    python::PythonRunner::instance().run(
        QString::fromLatin1(kSecSvcScript), argv, [self, key](python::PythonResult r) {
            if (!self)
                return;
            bool ok = r.success;
            QJsonObject payload;
            QString error;
            if (ok) {
                const QJsonDocument doc = QJsonDocument::fromJson(python::extract_json(r.output).toUtf8());
                if (!doc.isObject()) {
                    ok = false;
                    error = tr("SEC EDGAR returned an unreadable response.");
                } else {
                    payload = doc.object();
                    if (!payload.value(QStringLiteral("success")).toBool(true)) {
                        ok = false;
                        error = sec_svc_error_text(QString::fromUtf8(QJsonDocument(payload).toJson()));
                    }
                }
            } else {
                error = sec_svc_error_text(r.error.trimmed().isEmpty() ? r.output : r.error);
            }
            const QVector<CommandCallback> callbacks = self->waiters_.take(key);
            for (const auto& cb : callbacks) {
                if (cb)
                    cb(ok, payload, error);
            }
        });
}

} // namespace fincept::services::equity
