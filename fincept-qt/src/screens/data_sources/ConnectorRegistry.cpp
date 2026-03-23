// ConnectorRegistry.cpp — force-link all connector registration units
#include "screens/data_sources/ConnectorRegistry.h"

namespace fincept::screens::datasources {

// Each connector .cpp file uses a static-init bool to self-register.
// This function exists solely to ensure the linker includes all those
// translation units — call it once from DataSourcesScreen constructor.
void register_all_connectors() {
    // The static-init bools in each connector .cpp fire on first reference.
    // Just accessing the registry triggers them via the static initializers.
    // This function is intentionally empty — the linker will pull in the
    // connector .cpp files because they are listed in CMakeLists.txt.
    (void)ConnectorRegistry::instance().count();
}

} // namespace fincept::screens::datasources
