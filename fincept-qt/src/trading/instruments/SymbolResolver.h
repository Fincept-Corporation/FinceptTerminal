#pragma once

#include "trading/instruments/InstrumentSource.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace fincept::trading {

/// Registry of per-broker `InstrumentSource`s.
///
/// `InstrumentService` looks up by `broker_id` to drive download + parse during
/// catalog refresh. Brokers register their source once at startup; the resolver
/// is otherwise read-only.
///
/// Lifetime: process-global singleton, accessed only from the main thread for
/// registration and from the InstrumentService worker for lookup. Reads after
/// initialisation are safe without locking — the table is built once during
/// app bootstrap, before any refresh is triggered.
class SymbolResolver {
  public:
    static SymbolResolver& instance();

    /// Register (or replace) a source for `source.broker_id`.
    void register_source(InstrumentSource source);

    /// Look up by broker id. Returns nullptr if no source is registered.
    const InstrumentSource* find(const QString& broker_id) const;

    /// Brokers with a registered source.
    QStringList registered_brokers() const;

  private:
    SymbolResolver() = default;
    SymbolResolver(const SymbolResolver&) = delete;
    SymbolResolver& operator=(const SymbolResolver&) = delete;

    QHash<QString, InstrumentSource> sources_;
};

} // namespace fincept::trading
