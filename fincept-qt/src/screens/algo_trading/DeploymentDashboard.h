// src/screens/algo_trading/DeploymentDashboard.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Live deployment monitor — shows running strategies with real-time metrics.
class DeploymentDashboard : public QWidget {
    Q_OBJECT
  public:
    explicit DeploymentDashboard(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_deployments_loaded(QVector<fincept::services::algo::AlgoDeployment> deployments);
    void on_error(const QString& context, const QString& msg);

  private:
    void build_ui();
    void connect_service();
    QWidget* build_deployment_card(const fincept::services::algo::AlgoDeployment& d, QWidget* parent);
    void update_summary(const QVector<fincept::services::algo::AlgoDeployment>& deployments);
    void retranslateUi();

    // Summary stats (value labels captured by build_stat_card; captions cached for retranslate)
    QLabel* active_count_      = nullptr;
    QLabel* active_caption_    = nullptr;
    QLabel* total_pnl_         = nullptr;
    QLabel* total_pnl_caption_ = nullptr;
    QLabel* total_trades_      = nullptr;
    QLabel* total_trades_caption_ = nullptr;
    QLabel* avg_win_rate_      = nullptr;
    QLabel* avg_win_rate_caption_ = nullptr;

    // Equity curve placeholder
    QFrame* equity_placeholder_ = nullptr;
    QLabel* eq_title_           = nullptr;
    QLabel* eq_hint_            = nullptr;

    // Control bar + section
    QPushButton* refresh_btn_  = nullptr;
    QPushButton* stop_all_btn_ = nullptr;
    QLabel*      dep_title_     = nullptr;

    int deployment_count_ = 0; // for status_label_ retranslate

    QVBoxLayout* deployments_layout_ = nullptr;
    QLabel*      status_label_       = nullptr;
};

} // namespace fincept::screens
