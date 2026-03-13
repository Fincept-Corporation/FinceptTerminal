#pragma once
// Device ID generation matching the TypeScript implementation

#include <string>

namespace fincept::utils {

// Generate a unique device ID (10-255 chars) based on system info
std::string generate_device_id();

} // namespace fincept::utils
