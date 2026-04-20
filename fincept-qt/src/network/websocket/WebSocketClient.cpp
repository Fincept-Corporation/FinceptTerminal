#include "network/websocket/WebSocketClient.h"

#include "core/logging/Logger.h"

namespace fincept {

#ifdef HAS_QT_WEBSOCKETS

WebSocketClient::WebSocketClient(QObject* parent) : QObject(parent) {
    connect(&socket_, &QWebSocket::connected, this, &WebSocketClient::on_connected);
    connect(&socket_, &QWebSocket::disconnected, this, &WebSocketClient::on_disconnected);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &WebSocketClient::on_text_received);
    connect(&socket_, &QWebSocket::binaryMessageReceived, this, &WebSocketClient::on_binary_received);
    connect(&socket_, &QWebSocket::errorOccurred, this, &WebSocketClient::on_error);
    connect(&reconnect_timer_, &QTimer::timeout, this, &WebSocketClient::attempt_reconnect);
    reconnect_timer_.setSingleShot(true);
}

void WebSocketClient::connect_to(const QString& url) {
    url_ = url;
    reconnect_attempts_ = 0;
    LOG_INFO("WS", "Connecting to " + url);
    socket_.open(QUrl(url));
}

void WebSocketClient::disconnect() {
    reconnect_timer_.stop();
    socket_.close();
}

void WebSocketClient::send(const QString& message) {
    socket_.sendTextMessage(message);
}

void WebSocketClient::send_binary(const QByteArray& data) {
    socket_.sendBinaryMessage(data);
}

bool WebSocketClient::is_connected() const {
    return socket_.state() == QAbstractSocket::ConnectedState;
}

void WebSocketClient::on_connected() {
    LOG_INFO("WS", "Connected to " + url_);
    reconnect_attempts_ = 0;
    emit connected();
}

void WebSocketClient::on_disconnected() {
    LOG_WARN("WS", "Disconnected from " + url_);
    emit disconnected();
    if (reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
        int delay = std::min(1000 * (1 << reconnect_attempts_), 30000);
        reconnect_timer_.start(delay);
    }
}

void WebSocketClient::on_text_received(const QString& msg) {
    emit message_received(msg);
}

void WebSocketClient::on_binary_received(const QByteArray& data) {
    emit binary_message_received(data);
}

void WebSocketClient::on_error(QAbstractSocket::SocketError err) {
    Q_UNUSED(err);
    LOG_ERROR("WS", "Error: " + socket_.errorString());
    emit error_occurred(socket_.errorString());
}

void WebSocketClient::attempt_reconnect() {
    reconnect_attempts_++;
    LOG_INFO("WS", QString("Reconnect attempt %1/%2").arg(reconnect_attempts_).arg(MAX_RECONNECT_ATTEMPTS));
    socket_.open(QUrl(url_));
}

#else // No Qt WebSockets — stub implementations

WebSocketClient::WebSocketClient(QObject* parent) : QObject(parent) {}
void WebSocketClient::connect_to(const QString& /*url*/) {
    LOG_WARN("WS", "WebSocket not available — Qt6::WebSockets not installed");
}
void WebSocketClient::disconnect() {}
void WebSocketClient::send(const QString& /*message*/) {}
void WebSocketClient::send_binary(const QByteArray& /*data*/) {}
bool WebSocketClient::is_connected() const {
    return false;
}

#endif

} // namespace fincept
