#include "multiuser/client/PhaseOneClientTransport.h"

#include "multiuser/client/PhaseOneEndpointProvider.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace fincept::multiuser {

namespace {

class NetworkBackend final : public PhaseOneRequestBackend {
  public:
    void send(const QString& method, const QString& url, const QJsonObject& body, const QMap<QString, QString>& headers,
              Callback cb) override {
        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/4.0");
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

        QNetworkReply* reply = nullptr;
        const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        if (method == "GET")
            reply = nam_.get(request);
        else
            reply = nam_.post(request, payload);

        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, cb = std::move(cb)]() {
            PhaseOneBackendResponse response;
            response.status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            response.body = reply->readAll();
            if (reply->error() != QNetworkReply::NoError && response.body.isEmpty())
                response.transport_error = reply->errorString();
            reply->deleteLater();
            if (cb)
                cb(response);
        });
    }

  private:
    QNetworkAccessManager nam_;
};

QMap<QString, QString> build_headers(const QString& session_id) {
    QMap<QString, QString> headers;
    if (!session_id.isEmpty())
        headers.insert("Authorization", QStringLiteral("Bearer %1").arg(session_id));
    return headers;
}

PhaseOneTransportResponse parse_backend_response(const PhaseOneBackendResponse& response) {
    PhaseOneTransportResponse parsed;
    parsed.status_code = response.status_code;

    if (!response.transport_error.isEmpty()) {
        parsed.error_message = response.transport_error;
        return parsed;
    }

    parsed.transport_success = true;
    if (response.body.isEmpty())
        return parsed;

    const QJsonDocument document = QJsonDocument::fromJson(response.body);
    if (!document.isObject()) {
        parsed.transport_success = false;
        parsed.error_message = QStringLiteral("Phase one response was not a JSON object.");
        return parsed;
    }

    parsed.body = document.object();
    parsed.error_code = parsed.body.value("error_code").toString();
    parsed.error_message = parsed.body.value("message").toString();
    return parsed;
}

} // namespace

std::shared_ptr<PhaseOneRequestBackend> PhaseOneClientTransport::test_backend_;

PhaseOneClientTransport& PhaseOneClientTransport::instance() {
    static PhaseOneClientTransport transport;
    return transport;
}

PhaseOneClientTransport::PhaseOneClientTransport() : backend_(std::make_shared<NetworkBackend>()) {}

void PhaseOneClientTransport::get(const QString& path, const QString& session_id, Callback cb) {
    send("GET", path, {}, session_id, std::move(cb));
}

void PhaseOneClientTransport::post(const QString& path, const QJsonObject& body, const QString& session_id, Callback cb) {
    send("POST", path, body, session_id, std::move(cb));
}

void PhaseOneClientTransport::set_session_id(const QString& session_id) {
    session_id_ = session_id;
}

QString PhaseOneClientTransport::session_id() const {
    return session_id_;
}

void PhaseOneClientTransport::set_backend_for_tests(const std::shared_ptr<PhaseOneRequestBackend>& backend) {
    test_backend_ = backend;
}

void PhaseOneClientTransport::reset_backend_for_tests() {
    test_backend_.reset();
}

void PhaseOneClientTransport::send(const QString& method, const QString& path, const QJsonObject& body,
                                   const QString& session_id, Callback cb) {
    if (test_backend_) {
        backend()->send(method, path, body, build_headers(session_id), [cb = std::move(cb)](const PhaseOneBackendResponse& response) {
            if (cb)
                cb(parse_backend_response(response));
        });
        return;
    }

    const PhaseOneEndpointResolution endpoint = PhaseOneEndpointProvider::instance().resolve();
    if (!endpoint.ok) {
        if (cb)
            cb({false, 0, {}, endpoint.error_code, endpoint.message});
        return;
    }

    const QString absolute_url = path.startsWith('/') ? endpoint.base_url + path : endpoint.base_url + '/' + path;
    backend()->send(method, absolute_url, body, build_headers(session_id), [cb = std::move(cb)](const PhaseOneBackendResponse& response) {
        if (cb)
            cb(parse_backend_response(response));
    });
}

std::shared_ptr<PhaseOneRequestBackend> PhaseOneClientTransport::backend() {
    return test_backend_ ? test_backend_ : backend_;
}

} // namespace fincept::multiuser
