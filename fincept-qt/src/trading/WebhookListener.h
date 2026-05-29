#pragma once
// WebhookListener — local loopback HTTP server that receives alert webhooks
// from external charting platforms (TradingView, ChartInk) and turns them into
// orders routed through UnifiedTrading.
//
// Desktop app: there is no public server, so this binds 127.0.0.1:<port>. To
// receive alerts from a cloud platform behind NAT, the user tunnels this port
// (ngrok / cloudflared) — that is out of scope here; we only parse and route.
//
// TradingView alert JSON (line alert)    → placeorder
// TradingView alert JSON (strategy)      → smartorder (has "position_size")
// ChartInk scanner alert JSON            → placeorder per scanned symbol
//
// The listener does NOT place orders itself. It validates + parses the payload
// and emits a signal; the owner (a screen or a service) decides what to do —
// route to UnifiedTrading directly, or queue via the Action Center (Phase 3).

#include <QObject>
#include <QTcpServer>
#include <QJsonObject>

namespace fincept::trading {

class WebhookListener : public QObject {
    Q_OBJECT
  public:
    explicit WebhookListener(QObject* parent = nullptr);
    ~WebhookListener() override;

    // Start listening on 127.0.0.1:preferred_port. Falls back to an ephemeral
    // port if busy; read the actual port via port(). Returns true on success.
    bool start(quint16 preferred_port = 8125);
    void stop();
    bool is_listening() const;
    quint16 port() const { return port_; }

  signals:
    // Emitted for a TradingView line/strategy alert. `is_smart` is true when the
    // payload carried "position_size" (strategy alert → smart order).
    void tradingview_alert(const QJsonObject& alert, bool is_smart);
    // Emitted once per scanned symbol from a ChartInk scanner alert.
    void chartink_alert(const QJsonObject& alert);
    void error_occurred(const QString& message);

  private slots:
    void handle_new_connection();

  private:
    // Parse a raw HTTP request: extract method, path, and JSON body.
    // Returns true and fills `body` if a JSON body was found.
    static bool parse_http_request(const QByteArray& raw, QString& path, QJsonObject& body,
                                   QJsonArray& body_array, bool& is_array);

    void route_payload(const QString& path, const QJsonObject& body,
                       const QJsonArray& body_array, bool is_array);
    void route_chartink(const QJsonObject& body);

    static QByteArray http_response(int status, const QString& message);

    QTcpServer* server_ = nullptr;
    quint16 port_ = 0;
};

} // namespace fincept::trading
