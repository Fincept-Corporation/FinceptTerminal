#include "trading/instruments/SymbolResolver.h"

namespace fincept::trading {

SymbolResolver& SymbolResolver::instance() {
    static SymbolResolver s;
    return s;
}

void SymbolResolver::register_source(InstrumentSource source) {
    const QString id = source.broker_id;
    sources_.insert(id, std::move(source));
}

const InstrumentSource* SymbolResolver::find(const QString& broker_id) const {
    auto it = sources_.find(broker_id);
    return it == sources_.end() ? nullptr : &it.value();
}

QStringList SymbolResolver::registered_brokers() const {
    return sources_.keys();
}

} // namespace fincept::trading
