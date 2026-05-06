#pragma once
// HyperliquidClient — thin wrapper over Hyperliquid's public REST + WS surface.
//
// Public endpoints (no signing):
//   POST {base}/info          { "type": "<query>", … }
//   WS   {wsBase}             { "method": "subscribe", "subscription": {...} }
//
// Trading endpoints (signed via HyperliquidSigner — landing per Phase 5c):
//   POST {base}/exchange      { "action": {...}, "nonce": <ms>, "signature": <0x..> }
//
// Reference: Hyperliquid public docs (https://hyperliquid.gitbook.io/hyperliquid-docs/)
// and .grill-me/alpha-arena-production-refactor.md §Phase 5c.

#include "core/result/Result.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

namespace fincept::trading::hyperliquid {

class HyperliquidClient : public QObject {
    Q_OBJECT
  public:
    explicit HyperliquidClient(QObject* parent = nullptr);
    ~HyperliquidClient() override;

    /// Defaults: mainnet REST + WS. Use set_testnet(true) before any call to
    /// route at the testnet endpoints.
    void set_testnet(bool testnet);
    bool is_testnet() const { return testnet_; }

    using JsonCallback = std::function<void(Result<QJsonDocument>)>;

    /// POST /info with the given body. The `type` field selects the query;
    /// callers can pass any HL info query (`meta`, `clearinghouseState`, …).
    void info(const QJsonObject& body, JsonCallback cb);

    /// POST /exchange — signed action. The action JSON, nonce, and signature
    /// must be assembled by the caller (HyperliquidSigner). The client just
    /// posts and routes the response.
    void exchange(const QJsonObject& signed_envelope, JsonCallback cb);

    // ── WS ─────────────────────────────────────────────────────────────────

    /// Open the WebSocket. Caller subscribes to channels via subscribe().
    void connect_ws();
    void disconnect_ws();
    bool ws_connected() const;

    void subscribe(const QJsonObject& subscription);
    void unsubscribe(const QJsonObject& subscription);

  signals:
    /// Emitted on every parsed WS message. Consumers filter by `channel`.
    void ws_message(QJsonObject msg);
    void ws_state_changed(int state);   // QAbstractSocket::SocketState (int for portability)

  private:
    QString rest_base() const;
    QString ws_base() const;
    void post_json(const QString& url, const QJsonObject& body, JsonCallback cb);

    bool testnet_ = false;

    // ws holder is forward-declared in the cpp to keep the header light.
    class WsHolder;
    friend class WsHolder;
    WsHolder* ws_ = nullptr;
};

} // namespace fincept::trading::hyperliquid
