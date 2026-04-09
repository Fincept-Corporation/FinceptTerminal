#pragma once
// BrokerHttp — synchronous HTTP helper for broker API calls
// Uses QNetworkAccessManager with a local event loop for blocking calls.
// This keeps broker implementations simple (no callback chains).

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QString>

namespace fincept::trading {

struct BrokerHttpResponse {
    bool success = false;
    int status_code = 0;
    QJsonObject json;
    QString raw_body;
    QString error;
};

class BrokerHttp {
  public:
    static BrokerHttp& instance();

    BrokerHttpResponse get(const QString& url, const QMap<QString, QString>& headers = {});

    BrokerHttpResponse post_json(const QString& url, const QJsonObject& payload,
                                 const QMap<QString, QString>& headers = {});

    BrokerHttpResponse post_json_array(const QString& url, const QJsonArray& payload,
                                       const QMap<QString, QString>& headers = {});

    BrokerHttpResponse put_json(const QString& url, const QJsonObject& payload,
                                const QMap<QString, QString>& headers = {});

    BrokerHttpResponse patch_json(const QString& url, const QJsonObject& payload,
                                  const QMap<QString, QString>& headers = {});

    BrokerHttpResponse del(const QString& url, const QMap<QString, QString>& headers = {},
                           const QJsonObject& payload = {});

    // Form-urlencoded POST (Zerodha, some Indian brokers)
    BrokerHttpResponse post_form(const QString& url, const QMap<QString, QString>& params,
                                 const QMap<QString, QString>& headers = {});

    // Form-urlencoded PUT (Zerodha modify_order)
    BrokerHttpResponse put_form(const QString& url, const QMap<QString, QString>& params,
                                const QMap<QString, QString>& headers = {});

    // Raw body POST — caller provides pre-encoded body + headers (Shoonya jData= protocol)
    BrokerHttpResponse post_raw(const QString& url, const QByteArray& body, const QMap<QString, QString>& headers = {});

    // Raw body PUT — caller provides pre-encoded body + headers (Tradier modify_order)
    BrokerHttpResponse put_raw(const QString& url, const QByteArray& body, const QMap<QString, QString>& headers = {});

    // Timeout in ms (default 15s)
    void set_timeout(int ms) { timeout_ms_ = ms; }

  private:
    BrokerHttp() = default;
    BrokerHttpResponse execute(const QString& method, const QString& url, const QByteArray& body,
                               const QString& content_type, const QMap<QString, QString>& headers);
    int timeout_ms_ = 15000;
    QMutex mutex_; // guards concurrent calls from QtConcurrent worker threads
};

} // namespace fincept::trading
