#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Stock Screener Widget — screens a broad basket by volume, change%, or price.
/// Fetches live data via yfinance batch_quotes and sorts/filters client-side.
class ScreenerWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit ScreenerWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;

  private:
    void apply_styles();
    void refresh_data();
    void apply_filter();
    void render_rows(const QVector<services::QuoteData>& sorted);

    QComboBox* filter_combo_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QVector<services::QuoteData> all_quotes_;

    // Widgets needing theme-aware restyling
    QWidget* filter_bar_ = nullptr;
    QLabel* filter_lbl_ = nullptr;
    QLabel* count_lbl_ = nullptr;
    QWidget* header_ = nullptr;
    QVector<QLabel*> header_labels_;
    QScrollArea* scroll_ = nullptr;
};

} // namespace fincept::screens::widgets
