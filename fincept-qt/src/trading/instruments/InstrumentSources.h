#pragma once
// Aggregator for the broker instrument sources added by the unified cross-broker
// search work. Called once from InstrumentService alongside the original
// first-party (zerodha/angelone/groww/fyers/dhan/icicidirect) registrations.
namespace fincept::trading {
void register_extra_instrument_sources();
}
