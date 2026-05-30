#pragma once
// Upstox instrument source. Public gzip-compressed JSON master (all exchanges).
// Token is the alphanumeric instrument_key (kept in broker_token).
namespace fincept::trading {
void register_upstox_source();
}
