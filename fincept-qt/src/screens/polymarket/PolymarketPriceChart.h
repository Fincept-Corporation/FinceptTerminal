#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/prediction/PredictionTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Price chart with interval selector and outcome toggle.
/// Outcome names come from prediction::Outcome::name — no hardcoded YES/NO.
class PolymarketPriceChart : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketPriceChart(QWidget* parent = nullptr);

    void set_price_history(const fincept::services::prediction::PriceHistory& history);
    void set_current_price(double price);
    void set_outcome_labels(const QStringList& labels);

    /// Update the presentation profile. Re-renders the chart with the new
    /// y-axis label and price formatter. Safe to call before any data has
    /// been set.
    void set_presentation(const ExchangePresentation& p);

  signals:
    void interval_changed(const QString& interval);
    void outcome_changed(int index);

  private:
    QWidget* chart_container_ = nullptr;
    QComboBox* interval_combo_ = nullptr;
    QComboBox* outcome_combo_ = nullptr;
    QLabel* price_label_ = nullptr;

    // Cached last history so set_presentation() can re-render.
    fincept::services::prediction::PriceHistory last_history_;
    bool has_last_history_ = false;

    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();
};

} // namespace fincept::screens::polymarket
