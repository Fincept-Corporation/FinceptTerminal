// src/screens/algo_trading/StrategyOptimizeDialog.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QDialog>
#include <QJsonObject>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;

namespace fincept::ui {
class MatrixHeatmap;
}

namespace fincept::screens {

/// Stop-loss × take-profit sweep of a builder strategy (AlgoTradingService::
/// run_optimization): the grid of in-sample results as a heatmap (Sharpe, net
/// return, max drawdown or trades), the best cell by in-sample Sharpe and how
/// that cell did on the untouched out-of-sample tail. "Use these" hands the best
/// stop / target back to the builder.
class StrategyOptimizeDialog : public QDialog {
    Q_OBJECT
  public:
    StrategyOptimizeDialog(const fincept::services::algo::AlgoStrategy& strategy, const QString& symbol,
                           const QString& start_date, const QString& end_date, double capital,
                           QWidget* parent = nullptr);
    ~StrategyOptimizeDialog() override;

  signals:
    void apply_requested(double stop_loss_pct, double take_profit_pct);

  protected:
    /// Opening the dialog is the request: the sweep runs once with the default ranges.
    void showEvent(QShowEvent* event) override;

  private:
    void run();
    void show_metric();
    void on_result(const QJsonObject& result);
    static QVector<double> steps(double from, double to, double step);

    fincept::services::algo::AlgoStrategy strategy_;
    QString symbol_, start_, end_;
    double capital_ = 0;

    QDoubleSpinBox* sl_from_ = nullptr;
    QDoubleSpinBox* sl_to_ = nullptr;
    QDoubleSpinBox* sl_step_ = nullptr;
    QDoubleSpinBox* tp_from_ = nullptr;
    QDoubleSpinBox* tp_to_ = nullptr;
    QDoubleSpinBox* tp_step_ = nullptr;
    QDoubleSpinBox* oos_ = nullptr;
    QComboBox* metric_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* best_ = nullptr;
    fincept::ui::MatrixHeatmap* heat_ = nullptr;
    QJsonObject result_;
    bool running_ = false;
    bool auto_started_ = false; ///< the first show started the default sweep
};

} // namespace fincept::screens
