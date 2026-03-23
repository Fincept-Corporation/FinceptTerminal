// src/screens/algo_trading/StrategyListPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Strategy list — manage, deploy, delete saved strategies.
class StrategyListPanel : public QWidget {
    Q_OBJECT
  public:
    explicit StrategyListPanel(QWidget* parent = nullptr);

  private slots:
    void on_strategies_loaded(QVector<fincept::services::algo::AlgoStrategy> strategies);
    void on_error(const QString& context, const QString& msg);

  private:
    void build_ui();
    void connect_service();
    QWidget* build_strategy_card(const fincept::services::algo::AlgoStrategy& s, QWidget* parent);

    QLineEdit* search_edit_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QLabel* count_label_ = nullptr;
};

} // namespace fincept::screens
