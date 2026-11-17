// InstrumentResolverSources.cpp — the production wiring of the symbol system's
// services: the resolver's remote sources (backend /symbols/search with the
// /market/search fallback through HttpClient — BackendSymbolSearch,
// Yahoo search / validation through the yfinance Python command), its recent-pick
// store (AppConfig, per profile), the user's home country (market region), and the
// production InstrumentSync. Registered for InstrumentResolver::instance() at static
// initialisation. File-scope names carry an isrs_ / kIsrs prefix (unity builds).

#include "core/config/AppConfig.h"
#include "core/config/ProfileManager.h"
#include "core/instrument/SymbolInput.h"
#include "core/market/MarketProfile.h"
#include "network/http/HttpClient.h"
#include "python/PythonRunner.h"
#include "services/instruments/BackendSymbolSearch.h"
#include "services/instruments/InstrumentIndex.h"
#include "services/instruments/InstrumentResolver.h"
#include "services/instruments/InstrumentSync.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QPointer>
#include <QUrl>

#include <algorithm>

namespace fincept::services {

namespace {

namespace ci = core::instrument;

QString isrs_recent_path(const QString& key) {
    QString profile = ProfileManager::instance().active().trimmed();
    if (profile.isEmpty())
        profile = QStringLiteral("default");
    return QStringLiteral("symbol_picker/recent/%1/%2").arg(profile, key);
}

QJsonObject isrs_python_json(const python::PythonResult& r) {
    const QJsonDocument doc = QJsonDocument::fromJson(python::extract_json(r.output).toUtf8());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

/// "could not ask" (network, rate limit, Python missing) vs "asked, nothing found".
bool isrs_python_unreachable(const QJsonObject& obj) {
    if (obj.isEmpty())
        return true;
    const QString code = obj.value(QStringLiteral("code")).toString().trimmed().toLower();
    return code == QLatin1String("network") || code == QLatin1String("rate_limited") ||
           code == QLatin1String("unknown") || code == QLatin1String("timeout");
}

/// The user's home country (market region); empty for the euro area or none.
QString isrs_home_country() {
    const auto& profile = core::market::MarketProfileStore::instance().current();
    const QString first = profile.countries.value(0).trimmed().toUpper();
    return first == QLatin1String("EU") ? QString() : first; // the euro area is not one country
}

/// The session's backend search source (/symbols/search, else /market/search) over HttpClient.
BackendSymbolSearch& isrs_backend() {
    static BackendSymbolSearch* s = new BackendSymbolSearch( // leaked: outlives every caller
        [](const QString& path, QObject* context, BackendSymbolSearch::HttpDone done) {
            HttpClient::instance().get(
                path,
                [done = std::move(done)](Result<QJsonDocument> r) {
                    BackendSymbolSearch::HttpReply rep;
                    if (r.is_ok()) {
                        rep.status = HttpClient::response_status();
                        if (rep.status == 0)
                            rep.status = 200;
                        rep.body = r.value();
                    } else {
                        rep.status = HttpClient::status_from_error(r.error());
                        rep.error = QString::fromStdString(r.error());
                    }
                    done(rep);
                },
                context);
        },
        &isrs_home_country);
    return *s;
}

ResolverSources isrs_production_sources() {
    ResolverSources s = index_resolver_sources();

    s.backend_search = [](const QString& query, int limit, QObject* context, ResolverSources::RemoteDone done) {
        isrs_backend().search(query, limit, context, std::move(done));
    };

    s.yahoo_search = [](const QString& query, int limit, QObject* context, ResolverSources::RemoteDone done) {
        QPointer<QObject> ctx(context);
        const QString upper = query.trimmed().toUpper();
        const QString isin = ci::is_valid_isin(upper) ? upper : QString();
        python::PythonRunner::instance().run(
            QStringLiteral("yfinance_data.py"), {QStringLiteral("search"), query, QString::number(limit)},
            [ctx, isin, done = std::move(done)](python::PythonResult r) {
                if (!ctx)
                    return;
                const QJsonObject obj = isrs_python_json(r);
                ResolverRemoteReply rep;
                if (obj.contains(QStringLiteral("results")) && !obj.contains(QStringLiteral("error"))) {
                    rep.ok = true;
                    rep.rows = instruments_from_yahoo_search(obj, isin);
                } else {
                    rep.unreachable = isrs_python_unreachable(obj);
                    rep.ok = !rep.unreachable; // the search ran and matched nothing
                    rep.error = obj.value(QStringLiteral("error")).toString();
                    if (rep.error.isEmpty())
                        rep.error = r.error;
                }
                done(rep);
            });
    };

    s.yahoo_validate = [](const QString& text, QObject* context, ResolverSources::RemoteDone done) {
        QPointer<QObject> ctx(context);
        python::PythonRunner::instance().run(
            QStringLiteral("yfinance_data.py"), {QStringLiteral("resolve_symbol"), text},
            [ctx, done = std::move(done)](python::PythonResult r) {
                if (!ctx)
                    return;
                const QJsonObject obj = isrs_python_json(r);
                ResolverRemoteReply rep;
                const bool answered =
                    obj.contains(QStringLiteral("found")) || obj.contains(QStringLiteral("resolved_symbol"));
                const bool found = obj.value(QStringLiteral("found")).toBool();
                if (!answered || (!found && isrs_python_unreachable(obj))) {
                    rep.unreachable = true;
                    rep.error = obj.value(QStringLiteral("error")).toString();
                    if (rep.error.isEmpty())
                        rep.error = r.error;
                    done(rep);
                    return;
                }
                rep.ok = true;
                const QJsonArray candidates = obj.value(QStringLiteral("candidates")).toArray();
                if (found) {
                    const QString sym = obj.value(QStringLiteral("resolved_symbol")).toString().trimmed();
                    // An ISIN is never a symbol (yfinance resolves ISINs internally and echoes them).
                    if (!sym.isEmpty() && !ci::is_valid_isin(sym.toUpper())) {
                        QString type;
                        for (const auto& c : candidates)
                            if (c.toObject().value(QStringLiteral("symbol")).toString().compare(sym, Qt::CaseInsensitive) == 0)
                                type = c.toObject().value(QStringLiteral("type")).toString();
                        const QJsonObject row{{QStringLiteral("symbol"), sym},
                                              {QStringLiteral("name"), obj.value(QStringLiteral("name"))},
                                              {QStringLiteral("exchange"), obj.value(QStringLiteral("exchange"))},
                                              {QStringLiteral("type"), type}};
                        const ci::Instrument inst = instrument_from_yahoo_row(row, 0);
                        if (inst.is_valid()) {
                            rep.rows.push_back(inst);
                            rep.first_confirmed = true;
                        }
                    }
                }
                int position = 1;
                for (const auto& c : candidates) {
                    const ci::Instrument inst = instrument_from_yahoo_row(c.toObject(), position++);
                    if (inst.is_valid())
                        rep.rows.push_back(inst);
                }
                done(rep);
            });
    };

    s.default_home_country = &isrs_home_country;

    s.load_recent = [](const QString& key) {
        return AppConfig::instance().get(isrs_recent_path(key)).toStringList();
    };
    s.save_recent = [](const QString& key, const QStringList& json) {
        AppConfig::instance().set(isrs_recent_path(key), json);
    };
    return s;
}

[[maybe_unused]] const bool kIsrsRegistered = InstrumentResolver::register_default_sources(&isrs_production_sources);

} // namespace

InstrumentSync& InstrumentSync::instance() {
    // Deliberately leaked, like the resolver: it must outlive the services it calls.
    static InstrumentSync* s = [] {
        InstrumentSync::Ops ops;
        ops.get = [](const QString& path, QObject* context, InstrumentSync::HttpDone done) {
            // A symbol-master page is up to page_limit (1000) instruments — several
            // hundred KB: parsed on HttpClient's worker, never on the GUI thread
            // (the delta itself is applied on the index worker). Delivered on
            // `context` (the sync), and not at all once it is gone.
            HttpClient::instance().get_large(
                path,
                [done = std::move(done)](Result<QJsonDocument> r) {
                    InstrumentSync::HttpReply rep;
                    if (r.is_ok()) {
                        rep.status = HttpClient::response_status();
                        if (rep.status == 0)
                            rep.status = 200;
                        rep.body = r.value();
                    } else {
                        rep.status = HttpClient::status_from_error(r.error());
                        rep.error = QString::fromStdString(r.error());
                    }
                    done(rep);
                },
                context);
        };
        ops.read_cursor = [](QObject* context, InstrumentSync::CursorDone done) {
            InstrumentIndex::instance().read_sync_cursor(context, std::move(done));
        };
        ops.apply_delta = [](const QJsonObject& delta, QObject* context, InstrumentSync::DeltaDone done) {
            InstrumentIndex::instance().apply_master_delta(delta, context, std::move(done));
        };
        ops.start_index = []() { InstrumentIndex::instance().start(); };
        auto* sync = new InstrumentSync(std::move(ops));
        if (QCoreApplication* app = QCoreApplication::instance(); app && sync->thread() != app->thread())
            sync->moveToThread(app->thread());
        return sync;
    }();
    return *s;
}

} // namespace fincept::services
