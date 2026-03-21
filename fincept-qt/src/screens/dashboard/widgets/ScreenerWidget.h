#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"
#include <QLabel>
#include <QComboBox>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Stock Screener Widget — screens a broad basket by volume, change%, or price.
/// Fetches live data via yfinance batch_quotes and sorts/filters client-side.
class ScreenerWidget : public BaseWidget {
    Q_OBJECT
public:
    explicit ScreenerWidget(QWidget* parent = nullptr);

private:
    void refresh_data();
    void apply_filter();
    void render_rows(const QVector<services::QuoteData>& sorted);

    QComboBox*   filter_combo_  = nullptr;
    QVBoxLayout* list_layout_   = nullptr;
    QVector<services::QuoteData> all_quotes_;
};

} // namespace fincept::screens::widgets
