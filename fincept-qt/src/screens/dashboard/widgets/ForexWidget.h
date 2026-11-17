#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

namespace fincept::screens::widgets {

/// FX tile — the region's FX pairs (MarketProfile) or the tile's own list.
inline QuoteTableWidget* create_forex_widget(const QJsonObject& cfg = {}, QWidget* parent = nullptr) {
    return new QuoteTableWidget(QuoteListKind::Forex, cfg, parent);
}

} // namespace fincept::screens::widgets
