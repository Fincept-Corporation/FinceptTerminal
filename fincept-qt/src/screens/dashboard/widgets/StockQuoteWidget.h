#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"
#include <QLabel>
#include <QGridLayout>

namespace fincept::screens::widgets {

/// Single stock quote card — fetches real-time data via yfinance.
/// Shows price, change, high/low/open/volume in a detail layout.
class StockQuoteWidget : public BaseWidget {
    Q_OBJECT
public:
    explicit StockQuoteWidget(const QString& symbol = "AAPL", QWidget* parent = nullptr);

    void set_symbol(const QString& symbol);

private:
    void refresh_data();
    void populate(const services::QuoteData& quote);

    QString symbol_;

    QLabel* price_label_  = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* arrow_label_  = nullptr;
    QLabel* open_val_     = nullptr;
    QLabel* high_val_     = nullptr;
    QLabel* low_val_      = nullptr;
    QLabel* volume_val_   = nullptr;
    QLabel* prev_val_     = nullptr;
};

} // namespace fincept::screens::widgets
