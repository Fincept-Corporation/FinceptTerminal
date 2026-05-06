#pragma once
// HyperliquidVenue — IExchangeVenue implementation against Hyperliquid.
//
// Phase-5c partial:
//   * Public surface — mark prices, funding rates, position reconcile from
//     `info` REST + WS subscriptions — fully wired.
//   * Trading surface — gated by HyperliquidSigner::is_wired(). Until the
//     signer lands, place_order() returns an OrderAck with status="rejected"
//     and error="hl_signer_not_wired".
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 5c.

#include "services/alpha_arena/IExchangeVenue.h"
#include "trading/exchanges/hyperliquid/HyperliquidClient.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

namespace fincept::trading::hyperliquid {

class HyperliquidVenue : public QObject,
                         public fincept::services::alpha_arena::IExchangeVenue {
    Q_OBJECT
  public:
    explicit HyperliquidVenue(QObject* parent = nullptr);
    ~HyperliquidVenue() override;

    /// Configure testnet vs mainnet before connect().
    void set_testnet(bool testnet);

    /// Bind a user wallet address — used as the queryable account on `info`
    /// queries. Distinct from the *agent* wallet that signs orders (the agent
    /// key is read out of SecureStorage at sign time inside
    /// HyperliquidSigner).
    void set_user_address(const QString& addr);

    /// Open WS, subscribe to public mark/trades feeds, start reconcile loop.
    void connect();

    // ── IExchangeVenue ─────────────────────────────────────────────────────
    QString venue_kind() const override { return QStringLiteral("hyperliquid"); }
    ConnectionState connection_state() const override { return state_; }
    void place_order(const fincept::services::alpha_arena::OrderRequest& req,
                     std::function<void(fincept::services::alpha_arena::OrderAck)> ack_cb) override;
    void cancel_order(const QString& venue_order_id) override;
    double last_mark(const QString& coin) const override;
    void on_fill(std::function<void(fincept::services::alpha_arena::FillEvent)> cb) override {
        fill_cb_ = std::move(cb);
    }
    void on_funding(std::function<void(fincept::services::alpha_arena::FundingEvent)> cb) override {
        funding_cb_ = std::move(cb);
    }
    void on_liquidation(std::function<void(fincept::services::alpha_arena::LiquidationEvent)> cb) override {
        liq_cb_ = std::move(cb);
    }
    void on_mark_update(std::function<void(QString, double, qint64)> cb) override {
        mark_update_cb_ = std::move(cb);
    }

  signals:
    /// Local-vs-remote drift detected by the 30-s reconcile loop.
    void position_drift(QString coin, double local_qty, double remote_qty);

  private slots:
    void on_ws_message(QJsonObject msg);
    void on_ws_state_changed(int state);
    void reconcile_tick();

  private:
    HyperliquidClient* client_;
    QTimer* reconcile_timer_;
    QString user_address_;
    ConnectionState state_ = ConnectionState::Disconnected;
    QHash<QString, double> marks_;

    std::function<void(fincept::services::alpha_arena::FillEvent)> fill_cb_;
    std::function<void(fincept::services::alpha_arena::FundingEvent)> funding_cb_;
    std::function<void(fincept::services::alpha_arena::LiquidationEvent)> liq_cb_;
    std::function<void(QString, double, qint64)> mark_update_cb_;
};

} // namespace fincept::trading::hyperliquid
