#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Price chart with interval selector and outcome toggle.
class PolymarketPriceChart : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketPriceChart(QWidget* parent = nullptr);

    void set_price_history(const services::polymarket::PriceHistory& history);
    void set_current_price(double price);
    void set_token_labels(const QStringList& labels);

  signals:
    void interval_changed(const QString& interval);
    void outcome_changed(int index);

  private:
    QWidget* chart_container_ = nullptr;
    QComboBox* interval_combo_ = nullptr;
    QComboBox* outcome_combo_ = nullptr;
    QLabel* price_label_ = nullptr;
};

} // namespace fincept::screens::polymarket
