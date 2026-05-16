#pragma once

#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentTypes.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::trading {

/// One broker's contribution to the instrument catalog.
///
/// A source is a pair of functions:
///   1. `download` — fetches the broker's master instruments payload (CSV, JSON, …)
///       given the broker's credentials. May block; called on a worker thread.
///   2. `parse`    — normalises that payload into `Instrument` rows whose `broker_id`
///       field is already set. Called on the same worker thread.
///
/// Sources are registered with `SymbolResolver` at startup. The `InstrumentService`
/// dispatches refresh requests through the resolver instead of an `if`-chain so new
/// brokers plug in without editing core code.
///
/// A `download` returning an empty `QByteArray` is treated as "fetch failed".
/// A `parse` returning an empty vector is treated as "parse failed".
struct InstrumentSource {
    using Downloader = std::function<QByteArray(const BrokerCredentials&)>;
    using Parser     = std::function<QVector<Instrument>(const QByteArray&)>;

    QString    broker_id;   ///< Canonical broker id ("zerodha", "angelone", …).
    Downloader download;    ///< Fetch the raw master payload.
    Parser     parse;       ///< Normalise into Instrument rows.
};

} // namespace fincept::trading
