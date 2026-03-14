// Font loading implementation

#include "core/font_loader.h"
#include "core/logger.h"
#include <filesystem>

namespace fincept::core {

void load_fonts(float dpi_scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    float font_size = 16.0f * dpi_scale;

    const char* font_candidates[] = {
        "fonts/JetBrainsMono-Regular.ttf",
        "../fonts/JetBrainsMono-Regular.ttf",
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
#endif
    };

    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 3;
    config.PixelSnapH = true;
    config.RasterizerDensity = dpi_scale;

    bool loaded = false;
    for (const char* path : font_candidates) {
        if (std::filesystem::exists(path)) {
            io.FontDefault = io.Fonts->AddFontFromFileTTF(path, font_size, &config);
            if (io.FontDefault) {
                LOG_INFO("Font", "Loaded: %s at %.0fpx", path, font_size);
                loaded = true;
                break;
            }
        }
    }

    if (!loaded) {
        ImFontConfig fallback_config;
        fallback_config.SizePixels = font_size;
        io.FontDefault = io.Fonts->AddFontDefault(&fallback_config);
        LOG_WARN("Font", "Using ImGui default font at %.0fpx", font_size);
    }

    io.Fonts->Build();
}

} // namespace fincept::core
