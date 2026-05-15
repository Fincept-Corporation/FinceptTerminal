#include "services/markets/MarketSearchService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <algorithm>

namespace fincept::services {

namespace {

constexpr int kMinLimit = 1;
constexpr int kMaxLimit = 100;

QList<MarketSearchService::Item> parse_items(const QJsonDocument& doc) {
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        const auto obj = doc.object();
        if (obj.contains("results"))
            arr = obj["results"].toArray();
        else if (obj.contains("data"))
            arr = obj["data"].toArray();
    }

    QList<MarketSearchService::Item> out;
    out.reserve(arr.size());
    for (const auto& v : arr) {
        const auto obj = v.toObject();
        const QString sym = obj["symbol"].toString();
        if (sym.isEmpty())
            continue;
        out.push_back({sym, obj["name"].toString(), obj["exchange"].toString(),
                       obj["type"].toString(), obj["country"].toString()});
    }
    return out;
}

} // namespace

MarketSearchService& MarketSearchService::instance() {
    static MarketSearchService s;
    return s;
}

MarketSearchService::MarketSearchService() = default;

void MarketSearchService::search(const QString& query, const QString& type, int limit,
                                 const QString& request_id) {
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        emit results_ready(request_id, query, {});
        return;
    }
    const int clamped = std::clamp(limit, kMinLimit, kMaxLimit);

    QString url = QString("/market/search?q=%1&limit=%2").arg(q).arg(clamped);
    if (!type.isEmpty())
        url += "&type=" + type;

    QPointer<MarketSearchService> self = this;
    HttpClient::instance().get(
        url,
        [self, request_id, query](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                const QString reason = QString::fromStdString(result.error());
                LOG_WARN("MarketSearch", "search failed: " + reason);
                emit self->search_failed(request_id, query, reason);
                return;
            }
            emit self->results_ready(request_id, query, parse_items(result.value()));
        },
        this);
}

} // namespace fincept::services
