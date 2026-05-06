#pragma once
// PaperVenue — in-memory simulator that mirrors Hyperliquid math.
//
// Consumed by the OrderRouter via IExchangeVenue. Mark prices are pushed in
// from outside (the engine subscribes to a real Hyperliquid public-WS feed in
// Phase 5c and forwards marks here, so paper PnL tracks reality). Funding
// payments are pushed in similarly — we accept the venue's funding-rate
// schedule rather than guessing.
//
// Models per Nof1 S1:
//  * Fees: 0.045% taker / 0.015% maker. Entries are market+IOC = taker.
//  * Slippage: 5bps each side.
//  * Sim latency: 200ms — paper P&L doesn't trivially beat live by HFT noise.
//  * Liquidation: deterministic when mark crosses liq_price.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §5b (PaperVenue).

#include "services/alpha_arena/IExchangeVenue.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

namespace fincept::services::alpha_arena {

class PaperVenue : public QObject, public IExchangeVenue {
    Q_OBJECT
  public:
    explicit PaperVenue(QObject* parent = nullptr);
    ~PaperVenue() override = default;

    // ── IExchangeVenue ──
    QString venue_kind() const override { return QStringLiteral("paper"); }
    ConnectionState connection_state() const override { return ConnectionState::Connected; }
    void place_order(const OrderRequest& req,
                     std::function<void(OrderAck)> ack_cb) override;
    void cancel_order(const QString& venue_order_id) override;
    double last_mark(const QString& coin) const override;
    void on_fill(std::function<void(FillEvent)> cb) override { fill_cb_ = std::move(cb); }
    void on_funding(std::function<void(FundingEvent)> cb) override { funding_cb_ = std::move(cb); }
    void on_liquidation(std::function<void(LiquidationEvent)> cb) override { liq_cb_ = std::move(cb); }
    void on_mark_update(std::function<void(QString, double, qint64)> cb) override {
        mark_update_cb_ = std::move(cb);
    }

    // ── Paper-specific external feed ──
    /// Engine pushes marks here from a real Hyperliquid public-WS subscription.
    /// We update internal marks and fire mark_update_cb_ + check liquidations.
    void push_mark(const QString& coin, double mark, qint64 utc_ms);

    /// Engine pushes funding payments here at the 8h boundary. Amount is
    /// signed: positive = received, negative = paid. Per-agent settlement.
    void push_funding(const QString& agent_id, const QString& coin,
                      double amount_usd, qint64 utc_ms);

    /// Engine queries this when reconciling: how much equity does this agent
    /// have right now (cash + sum(mark-to-market unrealized PnL))?
    double equity_for(const QString& agent_id) const;

    /// Set per-coin maintenance margin (Hyperliquid metadata). Default 5%.
    void set_maintenance_margin(const QString& coin, double frac);

  private:
    /// Internal model of an open paper position, separate from Position to
    /// avoid coupling the on-disk DTO to in-memory bookkeeping.
    struct PaperPosition {
        QString agent_id;
        QString coin;
        double qty = 0.0;       // signed
        double entry = 0.0;
        double mark = 0.0;
        int leverage = 1;
        double profit_target = 0.0;
        double stop_loss = 0.0;
        double maintenance_margin_frac = 0.05;
    };

    /// Per-agent ledger: cash balance + positions.
    struct AgentBook {
        double cash = 0.0;
        QHash<QString, PaperPosition> positions;  // key: coin
    };

    void seed_agent_if_needed(const QString& agent_id, double initial_capital);
    void check_liquidations(const QString& coin, double mark, qint64 utc_ms);
    static double liquidation_price(const PaperPosition& p);

    QHash<QString, AgentBook> books_;
    QHash<QString, double> marks_;          // coin -> last mark
    QHash<QString, double> mm_overrides_;   // coin -> maintenance margin frac

    std::function<void(FillEvent)> fill_cb_;
    std::function<void(FundingEvent)> funding_cb_;
    std::function<void(LiquidationEvent)> liq_cb_;
    std::function<void(QString, double, qint64)> mark_update_cb_;

    /// Sequential venue-order-id generator (paper only).
    quint64 next_order_seq_ = 1;

    friend void paper_seed_agent(PaperVenue& venue, const QString& agent_id, double initial_capital);
};

/// Convenience — the engine seeds each agent's paper book at agent-add time.
/// Public because PaperVenue itself doesn't know agent ids until placement.
void paper_seed_agent(PaperVenue& venue, const QString& agent_id, double initial_capital);

} // namespace fincept::services::alpha_arena
