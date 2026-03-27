#pragma once
#include <QObject>
#include <QTimer>

#ifdef HAS_QT_WEBSOCKETS
#    include <QWebSocket>
#endif

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
    void send_binary(const QByteArray& data);
    bool is_connected() const;

  signals:
    void connected();
    void disconnected();
    void message_received(const QString& message);
    void binary_message_received(const QByteArray& data);
    void error_occurred(const QString& error);

  private slots:
#ifdef HAS_QT_WEBSOCKETS
    void on_connected();
    void on_disconnected();
    void on_text_received(const QString& msg);
    void on_binary_received(const QByteArray& data);
    void on_error(QAbstractSocket::SocketError err);
    void attempt_reconnect();
#endif

  private:
#ifdef HAS_QT_WEBSOCKETS
    QWebSocket socket_;
#endif
    QTimer reconnect_timer_;
    QString url_;
    int reconnect_attempts_ = 0;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
};

} // namespace fincept
