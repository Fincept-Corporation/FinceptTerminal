#include "services/data_normalization/DataMappingTestClient.h"

#include "network/http/HttpClient.h"

namespace fincept::services {

DataMappingTestClient& DataMappingTestClient::instance() {
    static DataMappingTestClient s;
    return s;
}

void DataMappingTestClient::test_api(Method method, const QString& url, const QJsonObject& body,
                                     const QObject* context, Callback callback) {
    auto& http = HttpClient::instance();
    switch (method) {
        case Method::Get:
            http.get(url, std::move(callback), context);
            return;
        case Method::Post:
            http.post(url, body, std::move(callback), context);
            return;
        case Method::Put:
            http.put(url, body, std::move(callback), context);
            return;
        case Method::Delete:
            http.del(url, body, std::move(callback), context);
            return;
    }
}

} // namespace fincept::services
