// BackendSymbolSearch.cpp — /symbols/search with the /market/search fallback (see the header).
// File-scope names carry a bss_ prefix (unity builds).

#include "services/instruments/BackendSymbolSearch.h"

#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QUrl>

#include <algorithm>
#include <atomic>

namespace fincept::services {

namespace ci = core::instrument;

namespace {

constexpr const char* kBssTag = "SymbolSearch";

QString bss_encode(const QString& text) {
    return QString::fromUtf8(QUrl::toPercentEncoding(text));
}

/// Could not ask (as opposed to "asked, no match").
bool bss_unreachable(int status) {
    return status == 0 || status >= 500 || status == 429 || status == 403;
}

bool bss_ok(int status) {
    return status >= 200 && status < 300;
}

/// "... 2 more" / "… 2 more": a display tail the backend appends to long lists.
void bss_drop_display_tail(QStringList& list) {
    list.erase(std::remove_if(list.begin(), list.end(),
                              [](const QString& s) {
                                  const QString t = s.trimmed();
                                  return t.isEmpty() || t.startsWith(QLatin1String("...")) ||
                                         t.startsWith(QChar(0x2026));
                              }),
               list.end());
}

} // namespace

QVector<ci::Instrument> instruments_from_symbols_search(const QJsonDocument& body) {
    QJsonArray rows;
    if (body.isArray())
        rows = body.array();
    else if (body.isObject())
        rows = body.object().value(QStringLiteral("results")).toArray();
    QVector<ci::Instrument> out;
    out.reserve(rows.size());
    for (const auto& v : rows) {
        const QJsonObject o = v.toObject();
        if (o.isEmpty())
            continue;
        ci::Instrument inst = ci::Instrument::from_json(o);
        bss_drop_display_tail(inst.aliases);
        bss_drop_display_tail(inst.former_symbols);
        inst = complete_instrument(inst);
        if (inst.is_valid())
            out.push_back(std::move(inst));
    }
    return out;
}

struct BackendSymbolSearch::Shared {
    HttpGet get;
    std::function<QString()> home_country;
    std::atomic<int> endpoint{static_cast<int>(Endpoint::Unknown)};
};

BackendSymbolSearch::BackendSymbolSearch(HttpGet get, std::function<QString()> home_country)
    : d_(std::make_shared<Shared>()) {
    d_->get = std::move(get);
    d_->home_country = std::move(home_country);
}

BackendSymbolSearch::Endpoint BackendSymbolSearch::endpoint() const {
    return static_cast<Endpoint>(d_->endpoint.load());
}

QString BackendSymbolSearch::symbols_search_path(const QString& query, int limit, const QString& country) {
    QString path = QStringLiteral("/symbols/search?q=%1&limit=%2")
                       .arg(bss_encode(query.left(64)))
                       .arg(std::clamp(limit, 1, 50));
    const QString c = country.trimmed().toUpper();
    if (c.size() == 2)
        path += QStringLiteral("&country=") + c;
    return path;
}

QString BackendSymbolSearch::market_search_path(const QString& query, int limit) {
    return QStringLiteral("/market/search?q=%1&limit=%2").arg(bss_encode(query.left(50))).arg(std::clamp(limit, 1, 50));
}

void BackendSymbolSearch::search(const QString& query, int limit, QObject* context, ResolverSources::RemoteDone done) {
    const std::shared_ptr<Shared> d = d_;
    if (!d->get) {
        // Never synchronously: the resolver's contract.
        QMetaObject::invokeMethod(
            context ? context : QCoreApplication::instance(),
            [done = std::move(done)]() {
                ResolverRemoteReply rep;
                rep.unreachable = true;
                rep.error = QStringLiteral("no backend");
                done(rep);
            },
            Qt::QueuedConnection);
        return;
    }
    auto shared_done = std::make_shared<ResolverSources::RemoteDone>(std::move(done));

    // The older endpoint: rows mapped from TradingView codes (derived Yahoo forms).
    auto market = [d, query, limit, context, shared_done]() {
        d->get(market_search_path(query, limit), context, [shared_done](const HttpReply& r) {
            ResolverRemoteReply rep;
            if (bss_ok(r.status)) {
                rep.ok = true;
                rep.rows = instruments_from_market_search(r.body);
            } else {
                rep.unreachable = bss_unreachable(r.status);
                rep.error = r.error;
            }
            (*shared_done)(rep);
        });
    };

    if (d->endpoint.load() == static_cast<int>(Endpoint::MarketSearchOnly)) {
        market();
        return;
    }
    const QString country = d->home_country ? d->home_country() : QString();
    d->get(symbols_search_path(query, limit, country), context, [d, market, shared_done](const HttpReply& r) {
        if (bss_ok(r.status)) {
            d->endpoint.store(static_cast<int>(Endpoint::SymbolsSearch));
            ResolverRemoteReply rep;
            rep.ok = true;
            rep.rows = instruments_from_symbols_search(r.body);
            (*shared_done)(rep);
            return;
        }
        if (r.status == 404) {
            // Not deployed on this backend: decided for the session.
            if (d->endpoint.exchange(static_cast<int>(Endpoint::MarketSearchOnly)) !=
                static_cast<int>(Endpoint::MarketSearchOnly))
                LOG_INFO(kBssTag, "Backend has no /symbols/search; searching /market/search for this session");
        } else {
            LOG_DEBUG_RL(kBssTag, QStringLiteral("symbols_search_failed"), 60000,
                         logf("Symbol search failed, trying /market/search", {{"status", r.status}}));
        }
        market();
    });
}

} // namespace fincept::services
