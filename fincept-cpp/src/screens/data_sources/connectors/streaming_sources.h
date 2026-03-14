#pragma once
#include "../data_sources_types.h"
#include <vector>

namespace fincept::data_sources::connectors {
std::vector<DataSourceDef> get_streaming_source_defs();
} // namespace fincept::data_sources::connectors
