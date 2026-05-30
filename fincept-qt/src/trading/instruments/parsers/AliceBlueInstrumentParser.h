#pragma once
// AliceBlue instrument source. Public per-exchange contract_master CSVs (column
// layout differs per file); futures rebuilt canonically rather than copying
// AliceBlue's non-canonical trading-symbol forms.
namespace fincept::trading {
void register_aliceblue_source();
}
