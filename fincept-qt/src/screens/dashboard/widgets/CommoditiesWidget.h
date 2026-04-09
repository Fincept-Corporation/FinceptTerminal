#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"
#include "ui/theme/Theme.h"

namespace fincept::screens::widgets {

/// Commodities widget — fetches 8 major commodities via yfinance.
inline QuoteTableWidget* create_commodities_widget(QWidget* parent = nullptr) {
    QMap<QString, QString> labels = {
        {"GC=F", "Gold"},    {"SI=F", "Silver"}, {"CL=F", "Crude WTI"}, {"BZ=F", "Brent"},
        {"NG=F", "Nat Gas"}, {"HG=F", "Copper"}, {"PL=F", "Platinum"},  {"PA=F", "Palladium"},
    };
    return new QuoteTableWidget("COMMODITIES", services::MarketDataService::commodity_symbols(), labels, 2,
                                ui::colors::WARNING(), parent);
}

} // namespace fincept::screens::widgets
