#include "trading/instruments/InstrumentSources.h"

#include "trading/instruments/parsers/AliceBlueInstrumentParser.h"
#include "trading/instruments/parsers/FivePaisaInstrumentParser.h"
#include "trading/instruments/parsers/FlattradeInstrumentSource.h"
#include "trading/instruments/parsers/KotakInstrumentParser.h"
#include "trading/instruments/parsers/MotilalInstrumentParser.h"
#include "trading/instruments/parsers/PaytmInstrumentParser.h"
#include "trading/instruments/parsers/SamcoInstrumentParser.h"
#include "trading/instruments/parsers/ShoonyaInstrumentSource.h"
#include "trading/instruments/parsers/TradejiniInstrumentParser.h"
#include "trading/instruments/parsers/UpstoxInstrumentParser.h"
#include "trading/instruments/parsers/XtsInstrumentParser.h"

namespace fincept::trading {

void register_extra_instrument_sources() {
    static bool done = false;
    if (done)
        return;
    done = true;

    register_upstox_source();
    register_fivepaisa_source();
    register_kotak_source();
    register_motilal_source();
    register_paytm_source();
    register_aliceblue_source();
    register_shoonya_source();
    register_flattrade_source();
    register_samco_source();
    register_tradejini_source();
    register_iifl_source();
}

} // namespace fincept::trading
