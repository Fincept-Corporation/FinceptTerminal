#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

class QLabel;

namespace fincept::ui {
class DataTable;
class Gauge;
} // namespace fincept::ui

namespace fincept::screens::widgets {

/// Risk-on / risk-off tile (type id "sentiment"; it replaces the old
/// breadth-of-24-large-caps "sentiment" score).
///
/// Shows services::RiskCompositeService: a 0–100 gauge built from five global
/// cross-asset signals (equity trend, VIX, credit, AUD/JPY, copper/gold), each
/// standardised against its own last year, and a row per signal with its
/// reading and z-score bar. The score appears only when every input is in; the
/// method is in the footer tooltip.
class MarketSentimentWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MarketSentimentWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void retranslateUi() override;

  private:
    void render();
    /// The table as tall as its rows, or as many whole rows as the tile has room for.
    void fit_table_height();

    ui::Gauge* gauge_ = nullptr;
    QLabel* regime_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QLabel* footer_ = nullptr;
};

} // namespace fincept::screens::widgets
