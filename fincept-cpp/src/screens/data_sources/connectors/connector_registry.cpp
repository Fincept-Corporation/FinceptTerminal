// Connector Registry — delegates to AdapterRegistry for all definitions
// Implements get_all_data_source_defs() declared in data_sources_types.h

#include "../data_sources_types.h"
#include "../adapters/adapter_registry.h"

namespace fincept::data_sources {

std::vector<DataSourceDef> get_all_data_source_defs() {
    return adapters::AdapterRegistry::instance().get_all_definitions();
}

} // namespace fincept::data_sources
