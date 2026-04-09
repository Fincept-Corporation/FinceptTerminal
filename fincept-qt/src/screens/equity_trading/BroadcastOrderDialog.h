#pragma once
// BroadcastOrderDialog — select accounts and place the same order across all of them.
// Shows checkboxes per account, "Select All", and a results summary after placement.

#include "trading/TradingTypes.h"
#include "trading/UnifiedTrading.h"

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens::equity {

class BroadcastOrderDialog : public QDialog {
    Q_OBJECT
  public:
    explicit BroadcastOrderDialog(const trading::UnifiedOrder& order, QWidget* parent = nullptr);

  signals:
    void broadcast_completed(const QVector<trading::UnifiedTrading::BroadcastResult>& results);

  private:
    void setup_ui();
    void on_select_all(bool checked);
    void on_place_order();
    void show_results(const QVector<trading::UnifiedTrading::BroadcastResult>& results);

    trading::UnifiedOrder order_;

    QCheckBox* select_all_cb_ = nullptr;
    QVector<QCheckBox*> account_cbs_;
    QVector<QString> account_ids_;
    QPushButton* place_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QWidget* results_widget_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
};

} // namespace fincept::screens::equity
