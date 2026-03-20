#pragma once
// Font loading — platform-aware font discovery and loading

#include <imgui.h>

namespace fincept::core {

// Load fonts with DPI scaling. Call after ImGui backend init.
void load_fonts(float dpi_scale);

// Multi-size font set for rich screens (Report Builder, etc.)
struct FontSizes {
    ImFont* heading    = nullptr;  // ~20px — H1
    ImFont* subheading = nullptr;  // ~15px — H2
    ImFont* body       = nullptr;  // ~11px — body text (default)
    ImFont* caption    = nullptr;  //  ~9px — labels, badges
};

// Returns loaded fonts after load_fonts() has been called.
const FontSizes& get_fonts();

} // namespace fincept::core
