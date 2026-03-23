#pragma once
#include <QObject>
#include <QTimer>
#include <QWebSocket>

#include <functional>

namespace fincept {

/// WebSocket client with auto-reconnect for real-time market data.
class WebSocketClient : public QObject {
    Q_OBJECT
  public:
    explicit WebSocketClient(QObject* parent = nullptr);

    void connect_to(const QString& url);
    void disconnect();
    void send(const QString& message);
    bool is_connected() const;

  signals:
    void connected();
    void disconnected();
    void message_received(const QString& message);
    void error_occurred(const QString& error);

  private slots:
    void on_connected();
    void on_disconnected();
    void on_text_received(const QString& msg);
    void on_error(QAbstractSocket::SocketError err);
    void attempt_reconnect();

  private:
    QWebSocket socket_;
    QTimer reconnect_timer_;
    QString url_;
    int reconnect_attempts_ = 0;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
};

} // namespace fincept
