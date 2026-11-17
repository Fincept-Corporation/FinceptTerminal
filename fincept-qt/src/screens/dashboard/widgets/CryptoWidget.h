#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

namespace fincept::screens::widgets {

/// Crypto tile — major coins (plus BTC / ETH in the region's currency) or the tile's own list.
inline QuoteTableWidget* create_crypto_widget(const QJsonObject& cfg = {}, QWidget* parent = nullptr) {
    return new QuoteTableWidget(QuoteListKind::Crypto, cfg, parent);
}

} // namespace fincept::screens::widgets
