#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>

namespace fincept::screens::widgets {

/// Forex major pairs widget — fetches 8 pairs via yfinance.
inline QuoteTableWidget* create_forex_widget(QWidget* parent = nullptr) {
    QMap<QString, QString> labels = {
        {"EURUSD=X", "EUR/USD"}, {"GBPUSD=X", "GBP/USD"}, {"USDJPY=X", "USD/JPY"}, {"AUDUSD=X", "AUD/USD"},
        {"USDCAD=X", "USD/CAD"}, {"USDCHF=X", "USD/CHF"}, {"NZDUSD=X", "NZD/USD"}, {"EURCHF=X", "EUR/CHF"},
    };
    const QString title =
        QCoreApplication::translate("fincept::screens::widgets::QuoteTableWidget", "FOREX - MAJOR PAIRS");
    // Accent must come from the theme (DESIGN_SYSTEM §2) — this was a
    // hardcoded purple that no palette defines and that does not follow a
    // theme switch.
    return new QuoteTableWidget(title, services::MarketDataService::forex_symbols(), labels, 4, ui::colors::INFO(),
                                parent);
}

} // namespace fincept::screens::widgets
