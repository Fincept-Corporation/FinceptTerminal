#pragma once
// PaperExchange — isolated-margin perp simulator per agent, marked from live
// mids, persisted via ArenaStore (crash-safe). HL-style taker fee + funding.
// Contract: execute()/close_all() must only run after mark_all() at the current
// round's marks — the liquidation sweep is what keeps close credits non-negative.
#include "core/result/Result.h"
#include "services/alpha_arena/ArenaStore.h"
#include "services/alpha_arena/ArenaTypes.h"

namespace fincept::arena {

struct AccountView {
    double balance = 0;        // free collateral
    double equity = 0;         // balance + Σ(position margin + uPnL − funding)
    double margin_used = 0;
    QVector<PositionRow> positions;
    QHash<QString, double> upnl;   // per coin (net of funding)
};

class PaperExchange {
  public:
    static constexpr double kTakerFee = 0.00045;   // 4.5 bps
    static constexpr double kSlippage = 0.0002;    // 2 bps
    static constexpr double kMaintMarginRate = 0.025;

    PaperExchange(ArenaStore& store, QString competition_id);
    virtual ~PaperExchange() = default;   // engine deletes HyperliquidLiveVenue through this base
    void load();                                       // agents' accounts+positions from store
    void set_marks(const QHash<QString, double>& mids);
    /// Rates are PER-HOUR fractional (Hyperliquid convention); longs pay positive funding.
    void set_funding(const QHash<QString, double>& rates);
    /// Funding + liquidation sweep. Returns liquidation order rows (persisted).
    /// elapsed_ms = ms since the PREVIOUS mark_all; funding double-accrues if misused.
    QVector<OrderRow> mark_all(qint64 elapsed_ms, int round_seq);
    /// Execute one parsed+risk-approved action at current mark. Persists order+position+account.
    /// Virtual: HyperliquidLiveVenue overrides to mirror paper fills to the live venue.
    virtual Result<OrderRow> execute(const QString& agent_id, const AgentAction& a, int round_seq);
    QVector<OrderRow> close_all(const QString& agent_id, int round_seq);
    AccountView account(const QString& agent_id) const;
    QHash<QString, double> marks() const { return marks_; }   // current mids (ticker/UI)

  private:
    double mark(const QString& coin) const { return marks_.value(coin, 0.0); }
    double position_upnl(const PositionRow& p, double mk) const;
    OrderRow close_position(const QString& agent_id, PositionRow& p, double fraction,
                            int round_seq, const QString& status, const QString& reason);
    void persist(const QString& agent_id);

    ArenaStore& store_;
    QString competition_id_;
    QHash<QString, double> marks_;
    QHash<QString, double> funding_rates_;
    QHash<QString, double> balances_;                       // agent_id → free balance
    QHash<QString, QVector<PositionRow>> positions_;        // agent_id → open positions
};

} // namespace fincept::arena
