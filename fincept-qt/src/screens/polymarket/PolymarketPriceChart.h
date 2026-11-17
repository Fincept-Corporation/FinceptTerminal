#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "screens/polymarket/PolymarketViewState.h"
#include "services/prediction/PredictionTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Price chart with interval selector and outcome toggle.
/// Outcome names come from prediction::Outcome::name — no hardcoded YES/NO.
class PolymarketPriceChart : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketPriceChart(QWidget* parent = nullptr);

    void set_price_history(const fincept::services::prediction::PriceHistory& history);
    /// History was requested (new market, outcome or interval): a chart skeleton until it lands.
    void set_loading();
    /// The request failed: the reason with RETRY (re-emits interval_changed for the current interval).
    void set_error(const QString& message);
    /// This exchange has no price history for its markets (e.g. a preview): say so plainly.
    void set_unavailable(const QString& message);
    void set_current_price(double price);
    void set_outcome_labels(const QStringList& labels);

    /// Update the presentation profile. Re-renders the chart with the new
    /// y-axis label and price formatter. Safe to call before any data has
    /// been set.
    void set_presentation(const ExchangePresentation& p);

  signals:
    void interval_changed(const QString& interval);
    void outcome_changed(int index);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QStackedWidget* chart_stack_ = nullptr;
    QWidget* chart_container_ = nullptr;
    PmViewState chart_state_;
    QComboBox* interval_combo_ = nullptr;
    QComboBox* outcome_combo_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* interval_lbl_ = nullptr;
    QLabel* outcome_lbl_ = nullptr;

    // Cached last history so set_presentation() can re-render.
    fincept::services::prediction::PriceHistory last_history_;
    bool has_last_history_ = false;

    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();
};

} // namespace fincept::screens::polymarket
