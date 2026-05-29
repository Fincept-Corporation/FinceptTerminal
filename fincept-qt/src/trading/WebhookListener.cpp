#include "trading/WebhookListener.h"

#include "core/logging/Logger.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpSocket>

namespace fincept::trading {

namespace {
constexpr const char* WH_TAG = "WebhookListener";
}

WebhookListener::WebhookListener(QObject* parent) : QObject(parent) {
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &WebhookListener::handle_new_connection);
}

WebhookListener::~WebhookListener() {
    stop();
}

bool WebhookListener::start(quint16 preferred_port) {
    if (server_->isListening())
        return true;

    if (!server_->listen(QHostAddress::LocalHost, preferred_port)) {
        // Preferred port busy — fall back to an ephemeral port.
        if (!server_->listen(QHostAddress::LocalHost, 0)) {
            const QString err = server_->errorString();
            LOG_ERROR(WH_TAG, QString("Failed to bind webhook listener: %1").arg(err));
            emit error_occurred(err);
            return false;
        }
    }
    port_ = server_->serverPort();
    LOG_INFO(WH_TAG, QString("Webhook listener on 127.0.0.1:%1").arg(port_));
    return true;
}

void WebhookListener::stop() {
    if (server_ && server_->isListening())
        server_->close();
    port_ = 0;
}

bool WebhookListener::is_listening() const {
    return server_ && server_->isListening();
}

void WebhookListener::handle_new_connection() {
    while (server_->hasPendingConnections()) {
        QTcpSocket* sock = server_->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            const QByteArray raw = sock->readAll();
            if (raw.isEmpty())
                return;

            QString path;
            QJsonObject body;
            QJsonArray body_array;
            bool is_array = false;
            const bool ok = parse_http_request(raw, path, body, body_array, is_array);

            QByteArray response;
            if (!ok) {
                response = http_response(400, "Bad Request: expected JSON body");
            } else {
                route_payload(path, body, body_array, is_array);
                response = http_response(200, "OK");
            }
            sock->write(response);
            sock->flush();
            sock->disconnectFromHost();
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
}

bool WebhookListener::parse_http_request(const QByteArray& raw, QString& path, QJsonObject& body,
                                         QJsonArray& body_array, bool& is_array) {
    // Split headers from body at the blank line.
    const int sep = raw.indexOf("\r\n\r\n");
    if (sep < 0)
        return false;

    const QByteArray header_block = raw.left(sep);
    const QByteArray body_block = raw.mid(sep + 4);

    // Request line: "POST /tradingview HTTP/1.1"
    const int line_end = header_block.indexOf("\r\n");
    const QByteArray request_line = line_end < 0 ? header_block : header_block.left(line_end);
    const QList<QByteArray> parts = request_line.split(' ');
    if (parts.size() >= 2)
        path = QString::fromUtf8(parts[1]);

    if (body_block.trimmed().isEmpty())
        return false;

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(body_block, &perr);
    if (perr.error != QJsonParseError::NoError)
        return false;

    if (doc.isArray()) {
        is_array = true;
        body_array = doc.array();
        return true;
    }
    if (doc.isObject()) {
        is_array = false;
        body = doc.object();
        return true;
    }
    return false;
}

void WebhookListener::route_payload(const QString& path, const QJsonObject& body,
                                    const QJsonArray& body_array, bool is_array) {
    const QString p = path.toLower();

    // ChartInk scanner alert — carries scan_name + comma-separated "stocks".
    if (p.contains("chartink") || body.contains("stocks") || body.contains("scan_name")) {
        route_chartink(body);
        return;
    }

    // TradingView alert(s). Array payloads are treated as multiple alerts.
    auto emit_tv = [this](const QJsonObject& obj) {
        const bool is_smart = obj.contains("position_size");
        emit tradingview_alert(obj, is_smart);
    };

    if (is_array) {
        for (const auto& v : body_array) {
            if (v.isObject())
                emit_tv(v.toObject());
        }
    } else {
        emit_tv(body);
    }
}

void WebhookListener::route_chartink(const QJsonObject& body) {
    // ChartInk webhook format:
    //   { "stocks": "SBIN,INFY", "trigger_prices": "500,1450",
    //     "scan_name": "Bullish Breakout BUY", "alert_name": "...", "triggered_at": "..." }
    // The action is inferred from a keyword in scan_name.
    const QString scan_name = body.value("scan_name").toString().toUpper();
    QString action;
    if (scan_name.contains("SHORT"))
        action = "SELL";
    else if (scan_name.contains("COVER"))
        action = "BUY";
    else if (scan_name.contains("SELL"))
        action = "SELL";
    else if (scan_name.contains("BUY"))
        action = "BUY";

    if (action.isEmpty()) {
        LOG_WARN(WH_TAG, QString("ChartInk alert has no BUY/SELL/SHORT/COVER keyword in scan_name: %1")
                             .arg(scan_name));
        emit error_occurred("ChartInk: no action keyword in scan_name");
        return;
    }

    const QStringList stocks = body.value("stocks").toString().split(',', Qt::SkipEmptyParts);
    const QStringList prices = body.value("trigger_prices").toString().split(',', Qt::SkipEmptyParts);

    for (int i = 0; i < stocks.size(); ++i) {
        QJsonObject alert;
        alert["symbol"] = stocks[i].trimmed();
        alert["exchange"] = "NSE"; // ChartInk is NSE/BSE; default NSE, consumer can remap
        alert["action"] = action;
        if (i < prices.size())
            alert["trigger_price"] = prices[i].trimmed().toDouble();
        alert["scan_name"] = scan_name;
        alert["source"] = "chartink";
        emit chartink_alert(alert);
    }
}

QByteArray WebhookListener::http_response(int status, const QString& message) {
    const QJsonObject obj{{"status", status == 200 ? "success" : "error"}, {"message", message}};
    const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const QString reason = status == 200 ? "OK" : "Bad Request";
    QByteArray resp;
    resp += QString("HTTP/1.1 %1 %2\r\n").arg(status).arg(reason).toUtf8();
    resp += "Content-Type: application/json\r\n";
    resp += QString("Content-Length: %1\r\n").arg(json.size()).toUtf8();
    resp += "Connection: close\r\n\r\n";
    resp += json;
    return resp;
}

} // namespace fincept::trading
