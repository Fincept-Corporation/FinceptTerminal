#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

namespace fincept::screens::widgets {

/// Global indices widget — fetches 12 major indices via yfinance.
inline QuoteTableWidget* create_indices_widget(QWidget* parent = nullptr) {
    QMap<QString, QString> labels = {
        {"^GSPC", "S&P 500"},      {"^DJI", "DOW JONES"},
        {"^IXIC", "NASDAQ"},       {"^RUT", "RUSSELL 2000"},
        {"^FTSE", "FTSE 100"},     {"^GDAXI", "DAX"},
        {"^FCHI", "CAC 40"},       {"^N225", "NIKKEI 225"},
        {"^HSI", "HANG SENG"},     {"000001.SS", "SHANGHAI"},
        {"^BSESN", "SENSEX"},      {"^NSEI", "NIFTY 50"},
    };
    return new QuoteTableWidget(
        "GLOBAL INDICES",
        services::MarketDataService::indices_symbols(),
        labels, 2, {}, parent);
}

} // namespace fincept::screens::widgets
