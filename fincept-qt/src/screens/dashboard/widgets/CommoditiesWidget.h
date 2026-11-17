#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

namespace fincept::screens::widgets {

/// Commodities tile — energy / metals / agriculture futures grouped under
/// header rows (the region's set from MarketProfile) or the tile's own list.
inline QuoteTableWidget* create_commodities_widget(const QJsonObject& cfg = {}, QWidget* parent = nullptr) {
    return new QuoteTableWidget(QuoteListKind::Commodities, cfg, parent);
}

} // namespace fincept::screens::widgets
