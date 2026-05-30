#pragma once
// IIFL (XTS / Symphony Fintech) instrument source. Authenticated market-data
// API: POST /instruments/master per exchange segment (pipe-delimited text) plus
// /indexlist. The market-data feed token is unpacked from the account's
// :::-packed access_token (trade_token:::feed_token).
namespace fincept::trading {
void register_iifl_source();
}
