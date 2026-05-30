#pragma once
// Motilal Oswal (OpenAPI) instrument source. Public per-exchange scrip-master
// and index CSVs; no auth. Download combines all files into one payload with
// "#SCRIP:<EX>" / "#INDEX:<EX>" section markers (each file has its own header).
namespace fincept::trading {
void register_motilal_source();
}
