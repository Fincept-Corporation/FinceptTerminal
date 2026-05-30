#pragma once
// Paytm Money instrument source. Public fully-quoted security_master.csv; token
// is the numeric security_id. instrument_type field drives canonical type/exchange.
namespace fincept::trading {
void register_paytm_source();
}
