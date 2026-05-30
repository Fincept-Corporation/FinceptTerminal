#pragma once
// Samco (StockNote) instrument source. Public ScripMaster.csv; token is a
// "<num>_<EXCH>" composite string (kept in broker_token). Indices injected.
namespace fincept::trading {
void register_samco_source();
}
