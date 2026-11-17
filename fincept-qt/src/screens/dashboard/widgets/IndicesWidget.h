#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

namespace fincept::screens::widgets {

/// Market indices tile — the headline indices of the user's MarketProfile
/// region (or the tile's own list), configurable via the gear icon.
inline QuoteTableWidget* create_indices_widget(const QJsonObject& cfg = {}, QWidget* parent = nullptr) {
    return new QuoteTableWidget(QuoteListKind::Indices, cfg, parent);
}

} // namespace fincept::screens::widgets
