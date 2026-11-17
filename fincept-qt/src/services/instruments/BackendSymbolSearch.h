#pragma once
// BackendSymbolSearch — the resolver's backend source (ResolverSources::backend_search):
// GET /symbols/search (the symbol master: Instrument JSON with real Yahoo symbols,
// renames and aliases) when the backend has it, else the older GET /market/search
// with the same normalised text (symbol-audit E4 source 2).
//
//   * The first /symbols/search answer decides for the session: 2xx = available;
//     404 = not deployed — every later search goes straight to /market/search.
//   * Any other failure of /symbols/search (5xx, rate limit, no connection) is not
//     taken as "missing": that one request is retried on /market/search, and the
//     next search asks /symbols/search again.
//   * The reply says `unreachable` only when the endpoint it ended on could not be
//     reached (network, 5xx, 429, 403) — "no match" is ok with no rows.
//
// HTTP is injected (production: HttpClient, InstrumentResolverSources.cpp; tests: a
// fake that answers asynchronously). The done callback runs exactly once, on the
// context's thread, never synchronously from search().

#include "services/instruments/InstrumentResolver.h"

#include <QJsonDocument>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace fincept::services {

/// GET /symbols/search body ({"results": [Instrument JSON + "match"], "did_you_mean", "version"}
/// or a bare array) -> instruments in the backend's order. Display-only list tails such as
/// "... 2 more" are dropped from aliases / former symbols; missing fields are filled by
/// complete_instrument().
QVector<fincept::core::instrument::Instrument> instruments_from_symbols_search(const QJsonDocument& body);

class BackendSymbolSearch {
  public:
    struct HttpReply {
        int status = 0; ///< HTTP status; 0 = no response (network, timeout)
        QJsonDocument body;
        QString error;
    };
    using HttpDone = std::function<void(const HttpReply&)>;
    using HttpGet = std::function<void(const QString& path, QObject* context, HttpDone done)>;

    enum class Endpoint : int {
        Unknown = 0,          ///< /symbols/search not asked yet this session
        SymbolsSearch = 1,    ///< /symbols/search answered
        MarketSearchOnly = 2, ///< /symbols/search returned 404: /market/search from now on
    };

    /// `home_country` (optional): ISO country sent as `country=` so the backend ranks the
    /// user's market first within the limit.
    explicit BackendSymbolSearch(HttpGet get, std::function<QString()> home_country = {});

    void search(const QString& query, int limit, QObject* context, ResolverSources::RemoteDone done);
    Endpoint endpoint() const;

    static QString symbols_search_path(const QString& query, int limit, const QString& country);
    static QString market_search_path(const QString& query, int limit);

  private:
    struct Shared;
    std::shared_ptr<Shared> d_;
};

} // namespace fincept::services
