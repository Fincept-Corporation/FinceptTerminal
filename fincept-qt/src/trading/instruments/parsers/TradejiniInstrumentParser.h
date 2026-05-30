#pragma once
// Tradejini instrument source. Public symbol-store REST API: a JSON group list,
// then one CSV per group whose underscore-delimited `id` encodes instrument /
// exchange / expiry / strike. `id` is stored as brsymbol (the order symId).
namespace fincept::trading {
void register_tradejini_source();
}
