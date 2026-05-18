#pragma once
// Phase 5 scaffolding — shared helper for "GET URL → parse JSON → emit" services.
//
// ~13 services today follow this near-identical pattern:
//   1. Build URL from inputs
//   2. Hit HttpClient with a context-scoped callback
//   3. Stale-request gate (request_id / pending_query)
//   4. Cache via CacheManager with a per-service TTL
//   5. Parse JSON to a typed result
//   6. Emit `*_ready(request_id, result)` or `*_failed(request_id, reason)`
//
// `RestServiceBase` collapses steps 2–6. A subclass provides the URL builder
// and the parser; the base does the rest.

#include "core/result/Result.h"
#include "network/http/HttpClient.h"

#include <QJsonDocument>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

namespace fincept::services {

/// Base class for thin REST-and-emit services.
///
/// Subclasses inherit `QObject` through this base, declare their own typed
/// signal, and call `fetch_and_emit(...)` from public methods. The base owns
/// the network call, the QPointer-context lifetime guard, and the JSON parsing
/// dispatch.
///
/// Caching is opt-in — pass a non-empty `cache_key` and a TTL and the helper
/// will consult `CacheManager` before issuing the request.
///
/// This is **not** a `Producer` — for topic-based fan-out, use a DataHub
/// `Producer`. `RestServiceBase` is for one-shot request/response services
/// like `MarketSearchService`.
class RestServiceBase : public QObject {
    Q_OBJECT
  public:
    explicit RestServiceBase(QObject* parent = nullptr) : QObject(parent) {}

  protected:
    /// Issue `GET url`. On success, hand the parsed JSON to `on_response`.
    /// `on_response` is run on the Qt event loop only if `this` is still alive.
    void fetch_json(const QString& url, std::function<void(Result<QJsonDocument>)> on_response) {
        QPointer<RestServiceBase> self = this;
        HttpClient::instance().get(
            url,
            [self, cb = std::move(on_response)](Result<QJsonDocument> result) {
                if (!self)
                    return;
                cb(std::move(result));
            },
            this);
    }
};

} // namespace fincept::services
