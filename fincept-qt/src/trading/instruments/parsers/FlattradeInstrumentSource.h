#pragma once
// Flattrade instrument source. Downloads the 8 per-segment scripmaster CSVs from
// the public Flattrade S3 bucket (uniform 9-column layout) and parses them via
// the shared NorenSymbolParser.
namespace fincept::trading {
void register_flattrade_source();
}
