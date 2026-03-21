#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

namespace fincept::screens::widgets {

/// Cryptocurrency widget — fetches top 9 cryptos via yfinance.
inline QuoteTableWidget* create_crypto_widget(QWidget* parent = nullptr) {
    QMap<QString, QString> labels = {
        {"BTC-USD", "BTC"},   {"ETH-USD", "ETH"},   {"BNB-USD", "BNB"},
        {"SOL-USD", "SOL"},   {"XRP-USD", "XRP"},    {"ADA-USD", "ADA"},
        {"DOGE-USD", "DOGE"}, {"DOT-USD", "DOT"},    {"LTC-USD", "LTC"},
    };
    return new QuoteTableWidget(
        "CRYPTOCURRENCY",
        services::MarketDataService::crypto_symbols(),
        labels, 2, "#d97706", parent);
}

} // namespace fincept::screens::widgets
