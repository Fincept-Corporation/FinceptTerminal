#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

namespace fincept::screens::widgets {

/// Forex major pairs widget — fetches 8 pairs via yfinance.
inline QuoteTableWidget* create_forex_widget(QWidget* parent = nullptr) {
    QMap<QString, QString> labels = {
        {"EURUSD=X", "EUR/USD"}, {"GBPUSD=X", "GBP/USD"}, {"USDJPY=X", "USD/JPY"}, {"AUDUSD=X", "AUD/USD"},
        {"USDCAD=X", "USD/CAD"}, {"USDCHF=X", "USD/CHF"}, {"NZDUSD=X", "NZD/USD"}, {"EURCHF=X", "EUR/CHF"},
    };
    return new QuoteTableWidget("FOREX - MAJOR PAIRS", services::MarketDataService::forex_symbols(), labels, 4,
                                "#9D4EDD", parent);
}

} // namespace fincept::screens::widgets
