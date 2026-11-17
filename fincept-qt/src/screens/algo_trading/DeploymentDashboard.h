// src/screens/algo_trading/DeploymentDashboard.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "services/algo_trading/AlgoMonitorStore.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QEvent>
#include <QHash>
#include <QHideEvent>
#include <QSet>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QPushButton;

namespace fincept::screens {

class DeploymentsView;

/// DASHBOARD sub-tab controller — paper / live deployments with real-time metrics (live
/// snapshots updated in place), per-currency totals, the P&L history of every deployment
/// (persisted snapshots, read off the GUI thread), and the controls of the selected one (pause /
/// resume, stop with optional flatten, remove, the full log). The layout is DeploymentsView.
class DeploymentDashboard : public QWidget {
    Q_OBJECT
  public:
    explicit DeploymentDashboard(QWidget* parent = nullptr);

  signals:
    /// The "nothing deployed" state's actions (the screen switches tabs).
    void open_strategies_requested();
    void open_builder_requested();

  protected:
    void changeEvent(QEvent* event) override;
    // The 1 s age ticker and the P&L-history refresh never run while the tab is hidden.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_deployments_loaded(QVector<fincept::services::algo::AlgoDeployment> deployments);
    void on_live_update(const QString& deployment_id, const fincept::algo::AlgoLiveSnapshot& snap);
    void on_error(const QString& context, const QString& msg);

  private:
    void connect_service();
    /// Rows for the view from the last list + live snapshots + in-process states.
    void push_rows();
    void update_summary(const QVector<fincept::services::algo::AlgoDeployment>& deployments);
    void refresh_ages();
    // Currency a deployment's prices and P&L are in: its symbol's quote currency (pair quote
    // asset, else listing venue), else the broker's; empty when unknown (amounts then print
    // without a unit — never a guessed currency).
    QString currency_for(const fincept::services::algo::AlgoDeployment& d) const;
    // Error / unmanaged-position text of a deployment (empty = none), in its currency.
    QString alert_text(const fincept::services::algo::AlgoDeployment& d) const;
    // Reload the persisted P&L snapshots (worker thread) and redraw the chart and trends.
    void refresh_equity();
    void apply_equity(const QHash<QString, QVector<fincept::services::algo::AlgoEquityPoint>>& curves);
    /// Fills + log of the selected deployment (worker thread; debounced on selection changes).
    void load_history(const QString& deployment_id);
    void push_conditions(const QString& deployment_id);
    void on_selection(const QString& deployment_id);
    void toggle_pause(const QString& deployment_id);
    void request_stop(const QString& deployment_id);
    void request_remove(const QString& deployment_id);
    void request_stop_all();
    void open_log(const QString& deployment_id);
    const fincept::services::algo::AlgoDeployment* find(const QString& deployment_id) const;
    bool has_position(const QString& deployment_id) const;

    DeploymentsView* view_ = nullptr;
    QPushButton* stop_all_btn_ = nullptr;
    QVector<fincept::services::algo::AlgoDeployment> deployments_; // last structural load
    QHash<QString, fincept::algo::AlgoLiveSnapshot> live_;          // last snapshot per deployment
    QHash<QString, QStringList> activity_;                          // recent notes per deployment
    QHash<QString, QVector<double>> spark_;                         // P&L history per deployment
    // Last P&L history loaded, so a format change redraws without a reload.
    QHash<QString, QVector<fincept::services::algo::AlgoEquityPoint>> last_curves_;
    QSet<QString> paused_;        ///< paused in-process (the DB row stays 'running')
    QSet<QString> pause_pending_; ///< PAUSE / RESUME sent, state not confirmed yet
    // A confirmed STOP the engine has not reported "stopped" for yet: the row says STOPPING…
    // instead of RUNNING, and a second stop is blocked.
    QSet<QString> stop_pending_;
    QTimer* age_timer_ = nullptr;
    QTimer* equity_timer_ = nullptr;
    QTimer* history_timer_ = nullptr;
    QString history_for_; ///< deployment whose fills / log the next load reads
    int equity_seq_ = 0;
    int history_seq_ = 0;
    bool loaded_once_ = false; ///< the first deployments list arrived
};

} // namespace fincept::screens
