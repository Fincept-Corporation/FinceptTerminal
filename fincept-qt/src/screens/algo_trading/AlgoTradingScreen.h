// src/screens/algo_trading/AlgoTradingScreen.h
#pragma once
#include "screens/algo_trading/AlertsPanel.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QEvent>
#include <QHideEvent>
#include <QPointer>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QLabel;

namespace fincept::ui {
class ScreenCommandBar;
class ScreenStatusBar;
} // namespace fincept::ui

namespace fincept::screens {

class StrategyBuilderPanel;
class StrategyListPanel;
class ScannerPanel;
class DeploymentDashboard;
class UniverseScannerPanel;

/// Algo Trading screen — six sub-tabs (Builder, Strategies, Scanner, Alerts, Dashboard,
/// Universe), each a dense workbench with its own command bar (title, the sub-tab segments,
/// its actions) and status bar, built by AlgoTabKit. The screen holds the lazily built
/// panels, the one root stylesheet, Alt+1..6, and the running-deployment badge it keeps on
/// every sub-tab's bars.
class AlgoTradingScreen : public QWidget, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit AlgoTradingScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "algo_trading"; }
    int state_version() const override { return 1; }

    /// ISectionLinked — a sub-tab (builder | strategies (also my_strategies) | scanner | alerts |
    /// dashboard (also deployments) | universe; "tab:<id>" too), "strategy:<id>" (MY STRATEGIES
    /// with that row selected) or "deployment:<id>" (the DASHBOARD). The sub-tab's own switch;
    /// nothing is opened, backtested or deployed by a link.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_tab_changed(int index);

  private:
    void build_ui();
    void install_shortcuts();
    /// The panel of tab `index`, built (and wired) on first use.
    QWidget* ensure_panel(int index);
    /// Connects a freshly built panel's command bar (sub-tab segments) and status bar.
    void adopt_panel(QWidget* panel, int index);
    /// The Builder, built if needed.
    StrategyBuilderPanel* builder();
    static constexpr int kAlgoPanelCount = 6;
    QVariantMap pending_draft_; ///< builder draft restored before the first show
    /// Re-render the running-deployment badge on every adopted panel's bars: danger "LIVE"
    /// only when real-money deployments run, ok "PAPER" otherwise; hidden when none run.
    void update_deploy_badge();
    void retranslateUi();

    QStackedWidget* content_stack_ = nullptr;
    StrategyBuilderPanel* builder_ = nullptr;
    StrategyListPanel* strategies_ = nullptr;
    ScannerPanel* scanner_ = nullptr;
    AlertsPanel* alerts_ = nullptr;
    DeploymentDashboard* dashboard_ = nullptr;
    UniverseScannerPanel* universe_ = nullptr;

    struct AdoptedBars {
        QPointer<ui::ScreenCommandBar> bar;
        QPointer<ui::ScreenStatusBar> status;
        QPointer<QLabel> badge;
        int tab = 0;
    };
    QVector<AdoptedBars> adopted_;

    int active_tab_ = 0;
    int active_deployments_ = 0;
    int live_deployments_ = 0;
    QTimer* poll_timer_ = nullptr;
    bool first_show_ = true;
};

} // namespace fincept::screens
