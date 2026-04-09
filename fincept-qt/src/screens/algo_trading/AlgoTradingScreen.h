// src/screens/algo_trading/AlgoTradingScreen.h
#pragma once
#include "screens/IStatefulScreen.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

class StrategyBuilderPanel;
class StrategyListPanel;
class ScannerPanel;
class DeploymentDashboard;

/// Algo Trading screen — 4-tab trading system builder.
/// Tabs: Builder, My Strategies, Scanner, Dashboard
class AlgoTradingScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AlgoTradingScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "algo_trading"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_tab_changed(int index);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_status_bar();
    void update_tab_buttons();

    QStackedWidget* content_stack_ = nullptr;
    StrategyBuilderPanel* builder_ = nullptr;
    StrategyListPanel* strategies_ = nullptr;
    ScannerPanel* scanner_ = nullptr;
    DeploymentDashboard* dashboard_ = nullptr;

    QVector<QPushButton*> tab_buttons_;
    int active_tab_ = 0;
    QLabel* deploy_count_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTimer* poll_timer_ = nullptr;
    bool first_show_ = true;
};

} // namespace fincept::screens
