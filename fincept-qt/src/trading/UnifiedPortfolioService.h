#pragma once
// UnifiedPortfolioService — aggregates live positions & holdings across ALL
// active INR broker accounts into one merged model for the Portfolio Monitor
// screen (docs/superpowers/specs/2026-06-10-portfolio-monitor-design.md).
//
// Data flows in from each account's AccountDataStream (positions_updated /
// holdings_updated / quote_updated); actions flow out through UnifiedTrading
// (close_position / place_order) — this service adds only the merge layer.
//
// Scope (user-confirmed): INR brokers only (BrokerProfile.currency == "INR");
// live broker data only (paper portfolios are not aggregated).

#include "trading/TradingTypes.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

namespace fincept::trading {

/// One account's slice of an aggregated row.
struct AggChild {
    QString account_id;
    QString broker_label; // display: "Fyers — TILAK"
    QString product;      // broker product string ("MIS"/"CNC"/"INTRADAY"…)
    QString side;         // positions: "LONG"/"SHORT" (empty for holdings)
    double qty = 0;       // as delivered by the broker (may be signed)
    double avg_price = 0;
    double ltp = 0;
    double pnl = 0;
    double day_pnl = 0;    // positions: broker day_pnl; holdings: qty*(ltp-prev_close)
    double invested = 0;   // holdings only
    double current = 0;    // holdings only
    double prev_close = 0; // holdings only
};

/// One (symbol, exchange) group: parent sums + per-account children.
struct AggRow {
    QString symbol;
    QString exchange;
    double total_qty = 0;   // signed sum across children
    double w_avg_price = 0; // |qty|-weighted; 0 when no quantity
    double ltp = 0;
    double pnl = 0;
    double day_pnl = 0;
    double invested = 0; // holdings only
    double current = 0;  // holdings only
    QVector<AggChild> children;
};

struct PortfolioSummary {
    double positions_pnl = 0;
    double positions_day_pnl = 0;
    double holdings_pnl = 0;
    double holdings_day_pnl = 0;
    double invested = 0;
    double current = 0;
    double return_pct = 0; // holdings (current-invested)/invested
    int accounts_live = 0;
    int accounts_total = 0;
};

class UnifiedPortfolioService : public QObject {
    Q_OBJECT
  public:
    /// Paper aggregates each account's paper portfolio (pt_* engine, CNC rows →
    /// holdings, MIS/NRML → positions); Live aggregates real broker REST data.
    /// The two are NEVER blended — set_mode() swaps the whole model.
    enum class Mode { Paper, Live };

    static UnifiedPortfolioService& instance();

    /// Enumerate active INR accounts, ensure their streams run, connect signals.
    /// Idempotent — call on every screen show; new accounts get picked up.
    void activate();

    /// Force an immediate portfolio refetch on every tracked account.
    void refresh_all();

    void set_mode(Mode mode);
    Mode mode() const { return mode_; }

    // --- Merged model (main thread) ---
    QVector<AggRow> positions() const { return positions_; }
    QVector<AggRow> holdings() const { return holdings_; }
    PortfolioSummary summary() const { return summary_; }

    struct AccountInfo {
        QString account_id;
        QString label;
        QString broker_id;
        QString default_exchange;
        QString paper_portfolio_id;
        bool live = false;
    };
    QVector<AccountInfo> accounts() const;

    // --- Actions (async on a worker thread; action_finished on main thread) ---
    void exit_child(const QString& account_id, const QString& symbol, const QString& exchange,
                    const QString& product);
    /// Square off `symbol` across every child account holding it.
    /// `from_holdings` selects which merged model to read children from.
    void exit_symbol(const QString& symbol, const QString& exchange, bool from_holdings);
    /// Square off every non-delivery position in every tracked account.
    void square_off_all_positions();
    void place_new_order(const QString& account_id, const UnifiedOrder& order);

    // --- Ingestion (stream signal handlers; public as the selftest seam) ---
    void ingest_positions(const QString& account_id, const QVector<BrokerPosition>& positions);
    void ingest_holdings(const QString& account_id, const QVector<BrokerHolding>& holdings);
    void ingest_quote(const QString& account_id, const QString& symbol, const BrokerQuote& quote);

    // --- Test seams (--selftest-portfolio-monitor; bypass account enumeration) ---
    void test_register_account(const QString& account_id, const QString& label, const QString& currency);
    void test_clear();

  signals:
    void positions_changed();                    // structural rebuild needed
    void holdings_changed();                     // structural rebuild needed
    void position_patched(const QString& symbol); // in-place LTP/P&L update
    void holding_patched(const QString& symbol);  // in-place LTP/P&L update
    void summary_changed();                       // debounced
    void accounts_changed();                      // live-count / set changed
    void action_finished(bool ok, const QString& message);

  private:
    explicit UnifiedPortfolioService(QObject* parent = nullptr);

    void connect_stream(const QString& account_id);
    void rebuild_positions();
    void rebuild_holdings();
    void rebuild_paper(); // reads pt_* fresh; fills BOTH positions_ and holdings_
    void schedule_summary();
    void recompute_summary();
    void emit_all_changed();

    struct Acct {
        QString label;
        QString broker_id;
        QString default_exchange;
        QString paper_portfolio_id;
        bool live = false;
        bool wired = false; // stream signals connected
        QVector<BrokerPosition> positions;
        QVector<BrokerHolding> holdings;
    };

    QHash<QString, Acct> accts_; // account_id → state
    QVector<AggRow> positions_;
    QVector<AggRow> holdings_;
    PortfolioSummary summary_;
    QTimer* summary_debounce_ = nullptr;
    Mode mode_ = Mode::Paper; // user-requested default: paper first
};

/// Headless merge-math selftest (no GUI/broker/keys).
/// QT_QPA_PLATFORM=offscreen FinceptTerminal --selftest-portfolio-monitor
int run_portfolio_monitor_selftest();

} // namespace fincept::trading
