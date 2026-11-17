#pragma once
// Pure conversions between scripts/yfinance_data.py replies, the quote cache's
// JSON and the market data value types. Qt Core only (no service, no hub), so
// tests/tst_quote_parsing.cpp links just MarketDataParsing.cpp.

#include "services/markets/MarketDataTypes.h"

#include <QJsonObject>
#include <QString>

namespace fincept::services {

/// Yahoo marketState -> "PRE" / "REGULAR" / "POST" / "CLOSED" (case-insensitive;
/// PREPRE and POSTPOST -> "CLOSED"). Anything else -> "" (unknown).
QString normalize_market_state(const QString& raw);

/// One quote row of a yfinance_data.py reply (batch_all "quotes", batch_quotes,
/// quote). Keys an older script omits leave their field absent; the original
/// eight fields read a missing / null value as 0, as they always did. `name`
/// falls back to the symbol.
QuoteData quote_from_reply_row(const QJsonObject& row);

/// The quote cache's JSON shape (CacheManager "market:<SYM>") and back. Absent
/// optional fields are left out of the JSON and come back absent.
QJsonObject quote_to_cache_json(const QuoteData& q);
QuoteData quote_from_cache_json(const QJsonObject& o);

/// One row of yfinance_data.py batch_info's "info" map. Every numeric field
/// Yahoo did not report is NaN — the original InfoData fields included — and
/// `eps` mirrors revenue_per_share, as fetch_info() fills it. `symbol` is used
/// when the row carries none.
InfoData info_from_batch_row(const QJsonObject& row, const QString& symbol);

} // namespace fincept::services
