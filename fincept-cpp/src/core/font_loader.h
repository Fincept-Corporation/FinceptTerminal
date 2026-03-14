#pragma once
// Font loading — platform-aware font discovery and loading

#include <imgui.h>

namespace fincept::core {

// Load fonts with DPI scaling. Call after ImGui backend init.
void load_fonts(float dpi_scale);

} // namespace fincept::core
