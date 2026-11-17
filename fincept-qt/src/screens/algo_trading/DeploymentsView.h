// src/screens/algo_trading/DeploymentsView.h
#pragma once
// DeploymentsView — the DASHBOARD sub-tab's layout (no engine, broker or database calls;
// DeploymentDashboard is the controller that feeds it, confirms and acts on its signals):
//
//   [ALGO  1 BUILDER … 5 DASHBOARD …            REFRESH ⌃R · LOG · STRATEGIES ›]
//   [RUNNING | LIVE P&L | PAPER P&L | PAPER TODAY | OPEN POSITIONS | ORDERS TODAY | WIN | FEES | HALTED]
//   ┌ DEPLOYMENTS 4 ───────────── STOP ALL ┐┌ <STRATEGY · SYMBOL>  PAPER ─ PAUSE STOP LOG… ┐
//   │ state · strategy · symbol · mode ·   ││ ● RUNNING  PAPER  LONG  1d  ZERODHA ● CONNECTED │
//   │ net P&L · today · position · LTP ·   ││ LTP · NET P&L · TODAY · POSITION · TRADES …     │
//   │ trades · win · last signal · updated ││ EQUITY · FILLS · LOG · CONDITIONS · RISK        │
//   │ · uptime · trend                     ││ [chart / table / table / rule status / limits]  │
//   └──────────────────────────────────────┘└─────────────────────────────────────────────────┘
//   [ENGINE · data · info · as of · state · deployments]
//
// Before the first list: a skeleton; nothing deployed: the designed EmptyState (OPEN STRATEGIES
// / OPEN BUILDER). Styling: the Algo kit's root stylesheet (object names only).

#include "algo_engine/AlgoEngineTypes.h"
#include "screens/algo_trading/AlgoTabKit.h"
#include "services/algo_trading/AlgoMonitorStore.h"
#include "ui/tables/LiveTableModel.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTableView;

namespace fincept::ui {
class EmptyState;
class KeySortProxy;
class KpiStrip;
class LoadingSkeleton;
class PanelHeader;
class ScreenCommandBar;
class ScreenStatusBar;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

/// One deployment as the table and the detail pane show it (live snapshot merged in by the
/// controller).
struct DeploymentsViewRow {
    QString id;
    QString strategy;
    QString symbol; ///< symbol, or the F&O underlying
    QString timeframe;
    QString mode;   ///< paper | live
    QString side;   ///< BUY | SELL (entry side)
    QString status; ///< engine row: pending | starting | running | stopped | error | crashed
    QString currency; ///< quote currency; empty = unknown (no unit printed)
    bool paused = false;
    bool pause_pending = false;
    bool stop_pending = false;
    bool risk_halted = false;
    QString halt_reason;
    double ltp = 0;
    double net_pnl = 0; ///< realized + unrealized, net of fees
    double unrealized = 0;
    double today_pnl = 0;
    double fees = 0;
    double max_dd = 0;
    double win_rate = 0;
    int trades = 0;
    int trades_today = 0;
    double pos_qty = 0;
    QString pos_side;
    double pos_entry = 0;
    qint64 last_update_ms = 0; ///< last live snapshot (0 = none yet)
    qint64 last_signal_ms = 0;
    qint64 started_ms = 0;
    QVector<double> spark; ///< net P&L history (30 days)
    QString alert;         ///< error / unmanaged-position text; empty = none
    QString broker_id;
    bool broker_connected = false;
    double quantity = 0;
    double max_order_value = 0;
    double max_daily_loss = 0;
    QJsonObject risk_limits;
    bool active() const { return status == QLatin1String("running") || status == QLatin1String("starting"); }
};

/// One condition row of the detail's CONDITIONS page.
struct DeploymentsCondRow {
    QString key;
    QString section; ///< entry | exit
    QString label;
    QString op;
    double computed = 0;
    double target = 0;
    bool met = false;
    bool executed = false; ///< an entry rule of an open position (it fired)
};

class DeploymentsView : public QWidget {
    Q_OBJECT
  public:
    explicit DeploymentsView(QWidget* parent = nullptr);

    enum class Page { Loading, Empty, Table };

    ui::KpiStrip* ribbon() const { return ribbon_; }
    ui::ScreenStatusBar* status_bar() const { return status_bar_; }
    QPushButton* stop_all_button() const { return stop_all_btn_; }
    QPushButton* pause_button() const { return pause_btn_; }
    QPushButton* stop_button() const { return stop_btn_; }
    QPushButton* remove_button() const { return remove_btn_; }

    void set_page(Page page);
    /// Every deployment (upsert: selection, scroll and sort survive live ticks); the first row
    /// is selected when nothing is.
    void set_rows(const QVector<DeploymentsViewRow>& rows);
    QString selected_id() const;
    const DeploymentsViewRow* row(const QString& id) const;
    /// Once a second while shown: ages ("updated 3 s ago", NO DATA) repaint.
    void tick();

    /// P&L histories (30 days) of every deployment: the EQUITY page draws the selected one or all.
    void set_curves(const QHash<QString, QVector<services::algo::AlgoEquityPoint>>& curves);
    /// Fills and the signal / order / risk log of `id` (loading = a read is under way).
    void set_history(const QString& id, const QVector<services::algo::AlgoTradeRow>& trades,
                     const QVector<services::algo::AlgoEventRow>& events, bool loading);
    /// Live rule status of `id` and its last activity lines.
    void set_conditions(const QString& id, const QVector<DeploymentsCondRow>& rows, qint64 as_of_ms,
                        const QStringList& activity);

  signals:
    void selection_changed(const QString& id);
    void refresh_requested();
    void log_requested(const QString& id);
    void pause_toggle_requested(const QString& id);
    void stop_requested(const QString& id);
    void remove_requested(const QString& id);
    void stop_all_requested();
    void open_strategies_requested();
    void open_builder_requested();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void build_ui();
    QWidget* build_detail(QWidget* parent);
    void retranslate();
    void render_detail();
    void render_equity();
    void set_kv(const QString& key, const QString& value, algo_kit::Tone tone = algo_kit::Tone::None,
                const QString& tip = {});

    ui::ScreenCommandBar* bar_ = nullptr;
    ui::ScreenStatusBar* status_bar_ = nullptr;
    ui::KpiStrip* ribbon_ = nullptr;
    QStackedWidget* pages_ = nullptr; ///< table + detail | loading | empty
    QWidget* work_ = nullptr;
    ui::LoadingSkeleton* loading_ = nullptr;
    ui::EmptyState* empty_ = nullptr;

    // Deployments table
    ui::PanelHeader* list_header_ = nullptr;
    QTableView* table_ = nullptr;
    ui::LiveTableModel<DeploymentsViewRow>* model_ = nullptr;
    ui::KeySortProxy* proxy_ = nullptr;
    QPushButton* stop_all_btn_ = nullptr;
    bool mixed_ = false; ///< the money columns mix currencies (ISO codes in the cells)

    // Detail
    ui::PanelHeader* detail_header_ = nullptr;
    QStackedWidget* detail_stack_ = nullptr; ///< 0 nothing selected, 1 the deployment
    QLabel* detail_none_ = nullptr;
    QWidget* chips_ = nullptr;
    QLabel* state_chip_ = nullptr;
    QLabel* mode_chip_ = nullptr;
    QLabel* side_chip_ = nullptr;
    QLabel* tf_chip_ = nullptr;
    QLabel* broker_chip_ = nullptr;
    QLabel* alert_ = nullptr;
    QLabel* halt_ = nullptr;
    QHash<QString, QLabel*> kv_keys_;
    QHash<QString, QLabel*> kv_values_;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QPushButton* log_btn_ = nullptr;
    QTabBar* tabs_ = nullptr;
    QStackedWidget* tab_pages_ = nullptr;
    // EQUITY
    QWidget* equity_mode_ = nullptr; ///< SELECTED | ALL segments
    bool equity_all_ = false;
    ui::TimeSeriesChart* chart_ = nullptr;
    int dd_pane_ = 1;
    QHash<QString, QVector<services::algo::AlgoEquityPoint>> curves_;
    // FILLS / LOG
    QLabel* fills_note_ = nullptr;
    QTableView* fills_table_ = nullptr;
    ui::LiveTableModel<services::algo::AlgoTradeRow>* fills_model_ = nullptr;
    ui::KeySortProxy* fills_proxy_ = nullptr;
    QLabel* log_note_ = nullptr;
    QTableView* log_table_ = nullptr;
    ui::LiveTableModel<services::algo::AlgoEventRow>* log_model_ = nullptr;
    ui::KeySortProxy* log_proxy_ = nullptr;
    QString history_id_; ///< whose fills / log are shown
    // CONDITIONS
    QLabel* cond_note_ = nullptr;
    QTableView* cond_table_ = nullptr;
    ui::LiveTableModel<DeploymentsCondRow>* cond_model_ = nullptr;
    QPlainTextEdit* activity_ = nullptr;
    QString cond_id_;
    // RISK
    QHash<QString, QLabel*> risk_values_;
    QHash<QString, QLabel*> risk_keys_;

    QString detail_ccy_;    ///< currency of the deployment the detail shows
    QString detail_symbol_; ///< its symbol (price decimals)
};

} // namespace fincept::screens
