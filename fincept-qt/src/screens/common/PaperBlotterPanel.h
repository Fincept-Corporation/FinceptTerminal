#pragma once
// PaperBlotterPanel — a reusable, screen-agnostic view of paper-trading state.
//
// Shows open positions, working orders, and executed trades for every ACTIVE
// paper account, with per-row and bulk square-off. It reads straight from the
// shared paper engine (pt_* / UnifiedTrading) and refreshes live off
// PaperMarkService's mark-to-market signals, so it is asset-class agnostic and
// stays correct no matter which screen hosts it. It is embedded as the F&O tab's
// "Positions" sub-tab and is intended to be the single blotter the Equity (and
// later Crypto) screens converge on, instead of each re-implementing the tables.

#include <QHash>
#include <QWidget>

class QLabel;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTimer;

namespace fincept::screens::common {

class PaperBlotterPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PaperBlotterPanel(QWidget* parent = nullptr);

  public slots:
    // Reload all tables from the paper engine. Cheap; safe to call often.
    void refresh();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void schedule_refresh(); // coalesce bursts of mark signals into one reload
    void square_off_all();
    void square_off_one(const QString& account_id, const QString& symbol, const QString& product);
    // Render the header summary; only touches the label stylesheet on a state flip.
    void update_summary(int open_count, double total_pnl, const QString& glyph);

    QTabWidget* tabs_ = nullptr;
    QTableWidget* positions_ = nullptr;
    QTableWidget* orders_ = nullptr;
    QTableWidget* trades_ = nullptr;
    QLabel* summary_ = nullptr;
    QPushButton* square_all_btn_ = nullptr;

    QTimer* refresh_timer_ = nullptr; // throttle live refreshes
    QTimer* poll_timer_ = nullptr;    // visibility-gated safety-net poll (P3)
    bool refresh_armed_ = false;

    // Cached summary-label state so a live blotter doesn't reparse the label's
    // CSS on every tick. -1 = never applied, 0 = neutral, 1 = gain, 2 = loss.
    int summary_state_ = -1;

    // Last row counts per table — resizeColumnsToContents() is O(rows x cols)
    // text measurement, so only re-run it when the shape actually changed.
    int last_positions_rows_ = -1;
    int last_orders_rows_ = -1;
    int last_trades_rows_ = -1;
};

} // namespace fincept::screens::common
