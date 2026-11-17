#include "services/data_normalization/DataMappingTestClient.h"

#include "network/http/HttpClient.h"

namespace fincept::services {

DataMappingTestClient& DataMappingTestClient::instance() {
    static DataMappingTestClient s;
    return s;
}

void DataMappingTestClient::test_api(Method method, const QString& url, const QJsonObject& body, const QObject* context,
                                     Callback callback, const QMap<QByteArray, QByteArray>& headers) {
    // A URL the user typed: the answer can be any size, so GET / POST bodies are
    // parsed on HttpClient's worker and delivered on `context` (never after it is
    // gone). PUT / PATCH / DELETE have no such form in HttpClient yet.
    auto& http = HttpClient::instance();
    switch (method) {
        case Method::Get:
            http.get_large(url, std::move(callback), context, headers);
            return;
        case Method::Post:
            http.post_large(url, body, /*timeout_ms=*/0, std::move(callback), context, headers);
            return;
        case Method::Put:
            http.put(url, body, std::move(callback), context, headers);
            return;
        case Method::Patch:
            http.patch(url, body, std::move(callback), context, headers);
            return;
        case Method::Delete:
            http.del(url, body, std::move(callback), context, headers);
            return;
    }
}

} // namespace fincept::services
