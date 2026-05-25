#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace fincept::multiuser {

struct PhaseOneBackendResponse {
    int status_code = 0;
    QByteArray body;
    QString transport_error;
};

class PhaseOneRequestBackend {
  public:
    using Callback = std::function<void(const PhaseOneBackendResponse&)>;

    virtual ~PhaseOneRequestBackend() = default;
    virtual void send(const QString& method, const QString& url, const QJsonObject& body,
                      const QMap<QString, QString>& headers, Callback cb) = 0;
};

struct PhaseOneTransportResponse {
    bool transport_success = false;
    int status_code = 0;
    QJsonObject body;
    QString error_code;
    QString error_message;
};

class PhaseOneClientTransport : public QObject {
  public:
    using Callback = std::function<void(const PhaseOneTransportResponse&)>;

    static PhaseOneClientTransport& instance();

    void get(const QString& path, const QString& session_id, Callback cb);
    void post(const QString& path, const QJsonObject& body, const QString& session_id, Callback cb);

    void set_session_id(const QString& session_id);
    QString session_id() const;

    static void set_backend_for_tests(const std::shared_ptr<PhaseOneRequestBackend>& backend);
    static void reset_backend_for_tests();

  private:
    PhaseOneClientTransport();

    void send(const QString& method, const QString& path, const QJsonObject& body, const QString& session_id, Callback cb);
    std::shared_ptr<PhaseOneRequestBackend> backend();

    std::shared_ptr<PhaseOneRequestBackend> backend_;
    QString session_id_;

    static std::shared_ptr<PhaseOneRequestBackend> test_backend_;
};

} // namespace fincept::multiuser
