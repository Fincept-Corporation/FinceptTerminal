// src/screens/algo_trading/StrategyLibraryView.h
#pragma once
// StrategyLibraryView — the MY STRATEGIES sub-tab's workbench, a pure view (no service,
// engine, database or broker calls; StrategyListPanel is the controller):
//
//   command bar   ALGO [sub-tabs] … OPEN ↵ · BACKTEST · DEPLOY (PAPER) · DUPLICATE · EXPORT · DELETE
//   strip         SEARCH […]  CATEGORY [▾]  SHOW [▾]                         n of N
//   KPI ribbon    STRATEGIES · BACKTESTED · RUNNING · BEST LAST RETURN
//   panels        STRATEGIES (dense sortable table)  |  DETAIL (rules in plain language,
//                 risk, last backtest tiles with its equity sparkline, deployments, actions)
//   status bar    engine · source · counts · loaded at · state
//
// Every action names the strategy by id (action_requested); the controller opens the
// dialogs. The DELETE controls (bar action, detail button, the Delete key) carry the
// `fincept_agent_risk`="delete" marker.

#include "services/algo_trading/AlgoTradingTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

class QComboBox;
class QEvent;
class QHideEvent;
class QLabel;
class QShowEvent;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableView;

namespace fincept::ui {
class EmptyState;
class KeySortProxy;
class KpiStrip;
class LoadingSkeleton;
class PanelHeader;
class ScreenCommandBar;
class ScreenStatusBar;
template <typename Row>
class LiveTableModel;
} // namespace fincept::ui

namespace fincept::screens {

class StrategyLibraryView : public QWidget {
    Q_OBJECT
  public:
    /// Paper / live deployments of one strategy (from AlgoEngine::deployments_loaded).
    struct Deployments {
        int total = 0;
        int running = 0;      ///< running or starting
        int live_running = 0; ///< of which real money
        int trades = 0;
        QMap<QString, double> pnl_by_ccy; ///< net P&L per quote currency ("" = unknown)
    };
    struct Row; ///< one table row (defined in the .cpp)

    explicit StrategyLibraryView(QWidget* parent = nullptr);
    ~StrategyLibraryView() override;

    /// The loaded library; keeps the selection (by id) and the filters.
    void set_strategies(const QVector<fincept::services::algo::AlgoStrategy>& strategies);
    void set_deployments(const QHash<QString, Deployments>& by_strategy_id);
    /// A finished backtest updated one strategy's persisted summary.
    void set_last_backtest(const QString& strategy_id, const QJsonObject& summary);
    /// Before the first list arrives / when it could not be read.
    void show_loading();
    void show_error(const QString& message);
    /// Re-render every text (number / currency format or language changed).
    void refresh_text();

    QString selected_id() const;
    bool selected_strategy(fincept::services::algo::AlgoStrategy* out) const;
    void select_id(const QString& strategy_id);

    fincept::ui::ScreenStatusBar* status_bar() const { return status_; }

  signals:
    /// "open" | "backtest" | "deploy" | "duplicate" | "export" | "delete" on a strategy.
    void action_requested(const QString& action, const QString& strategy_id);
    void retry_requested();
    void builder_requested();

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void build_ui();
    void install_shortcuts();
    void retranslate();
    void rebuild_rows();
    void refilter();
    void update_ribbon();
    void update_detail();
    void update_actions();
    void update_page();
    void trigger(const QString& action);
    void fill_categories();

    fincept::ui::ScreenCommandBar* bar_ = nullptr;
    fincept::ui::ScreenStatusBar* status_ = nullptr;
    QLineEdit* search_ = nullptr;
    QComboBox* category_ = nullptr;
    QComboBox* show_ = nullptr;
    QLabel* count_ = nullptr;
    fincept::ui::KpiStrip* ribbon_ = nullptr;
    QStackedWidget* stack_ = nullptr; ///< 0 table, 1 state page, 2 skeleton
    QTableView* table_ = nullptr;
    fincept::ui::LiveTableModel<Row>* model_ = nullptr;
    fincept::ui::KeySortProxy* proxy_ = nullptr;
    fincept::ui::EmptyState* state_ = nullptr;
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    QString state_kind_; ///< "empty" | "nomatch" | "error"

    fincept::ui::PanelHeader* list_header_ = nullptr;
    fincept::ui::PanelHeader* detail_header_ = nullptr;

    // Detail pane
    QWidget* detail_body_ = nullptr;
    QLabel* detail_empty_ = nullptr;
    QWidget* detail_content_ = nullptr;
    QLabel* d_name_ = nullptr;
    QLabel* d_meta_ = nullptr;
    QLabel* d_entry_ = nullptr;
    QLabel* d_exit_ = nullptr;
    QLabel* d_risk_ = nullptr;
    QLabel* d_bt_caption_ = nullptr;
    QLabel* d_bt_note_ = nullptr;
    fincept::ui::KpiStrip* d_tiles_ = nullptr;
    QLabel* d_dep_ = nullptr;
    QLabel* d_entry_cap_ = nullptr;
    QLabel* d_exit_cap_ = nullptr;
    QLabel* d_risk_cap_ = nullptr;
    QLabel* d_dep_cap_ = nullptr;
    QPushButton* d_open_ = nullptr;
    QPushButton* d_backtest_ = nullptr;
    QPushButton* d_deploy_ = nullptr;
    QPushButton* d_duplicate_ = nullptr;
    QPushButton* d_export_ = nullptr;
    QPushButton* d_delete_ = nullptr;

    QVector<fincept::services::algo::AlgoStrategy> strategies_;
    QHash<QString, Deployments> deployments_;
    bool live_mixed_ = false; ///< the deployments' P&L spans currencies (every amount names its unit)
    bool loaded_ = false;
};

} // namespace fincept::screens
