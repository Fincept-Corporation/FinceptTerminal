#include "trading/instruments/parsers/NorenSymbolParser.h"

#include "trading/instruments/InstrumentNormalize.h"

namespace fincept::trading {

Instrument noren_build(const QString& broker_id, const NorenRow& r) {
    Instrument inst;
    inst.broker_id = broker_id;
    inst.brexchange = r.brexchange.trimmed().toUpper();
    inst.brsymbol = r.brsymbol.trimmed();
    inst.name = r.name.trimmed().toUpper();
    inst.instrument_token = r.token.toLongLong();
    inst.exchange_token = inst.instrument_token;
    inst.strike = r.strike;
    inst.lot_size = r.lot_size > 0 ? r.lot_size : 1;
    inst.tick_size = r.tick_size > 0 ? r.tick_size : 0.05;

    const QString ot = r.option_type.trimmed().toUpper();
    const QString rawi = r.raw_instrument.trimmed().toUpper();

    InstrumentType t;
    if (r.is_bse_index || rawi == "INDEX" || rawi == "UNDIND")
        t = InstrumentType::INDEX;
    else if (ot == "XX")
        t = InstrumentType::FUT;
    else if (ot == "CE")
        t = InstrumentType::CE;
    else if (ot == "PE")
        t = InstrumentType::PE;
    else
        t = InstrumentType::EQ;
    inst.instrument_type = t;

    const QString exch = r.is_bse_index ? QStringLiteral("BSE_INDEX") : inst.brexchange;
    inst.exchange = norm::normalise_exchange(exch, t);

    const QString exp_nd = norm::expiry_to_nodash(r.expiry_raw);
    inst.expiry = norm::nodash_to_display(exp_nd);

    // EQ → strip suffix off the native trading symbol; INDEX → map the underlying.
    const QString fallback = (t == InstrumentType::EQ) ? inst.brsymbol : inst.name;
    inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, r.strike, fallback);
    return inst;
}

} // namespace fincept::trading
