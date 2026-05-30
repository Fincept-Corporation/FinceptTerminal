#pragma once
// Shoonya (Finvasia) instrument source. Downloads the 6 per-exchange
// "<EX>_symbols.txt.zip" masters from api.shoonya.com, unzips, and parses the
// comma-delimited symbol files (column layout differs per exchange).
namespace fincept::trading {
void register_shoonya_source();
}
