// Font loading implementation — primary font + color emoji merge
// Requires IMGUI_USE_WCHAR32 and IMGUI_ENABLE_FREETYPE

#include "core/font_loader.h"
#include "core/logger.h"
#include <filesystem>
#include <imgui_freetype.h>

namespace fincept::core {

static FontSizes s_fonts;

const FontSizes& get_fonts() { return s_fonts; }

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

    // ── Multi-size fonts for rich screens ──
    s_fonts.body = io.FontDefault;

    auto load_size = [&](float px) -> ImFont* {
        ImFontConfig cfg;
        cfg.OversampleH = 3; cfg.OversampleV = 3;
        cfg.PixelSnapH = true;
        cfg.RasterizerDensity = dpi_scale;
        for (const char* path : font_candidates) {
            if (std::filesystem::exists(path)) {
                ImFont* f = io.Fonts->AddFontFromFileTTF(path, px * dpi_scale, &cfg);
                if (f) return f;
            }
        }
        return io.FontDefault;
    };

    s_fonts.heading    = load_size(20.0f);
    s_fonts.subheading = load_size(15.0f);
    s_fonts.caption    = load_size(9.0f);

    // ── Merge color emoji font ──
    // With IMGUI_USE_WCHAR32, ImWchar is 32-bit so we can address emoji codepoints.
    // With IMGUI_ENABLE_FREETYPE, we can use LoadColor for COLR/CBLC color fonts.
    // Segoe UI Emoji (seguiemj.ttf) on Windows has excellent color emoji coverage.
    const char* emoji_font_candidates[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/seguiemj.ttf",    // Segoe UI Emoji — color emoji
#elif defined(__APPLE__)
        "/System/Library/Fonts/Apple Color Emoji.ttc",
#else
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/noto-cjk/NotoColorEmoji.ttf",
#endif
    };

    // Emoji ranges we need (32-bit codepoints):
    // Misc Symbols:           U+2600–U+26FF  (☀ ☁ ⚡ etc.)
    // Dingbats:               U+2700–U+27BF  (❄ etc.)
    // Misc Symbols & Arrows:  U+2B00–U+2BFF
    // Supplemental Symbols:   U+1F300–U+1F5FF (🌍 🌊 🐘 🐬 🍃 📄 📊 etc.)
    // Emoticons:              U+1F600–U+1F64F
    // Transport/Map:          U+1F680–U+1F6FF (🔌 🔒 etc.)
    // Supplemental:           U+1F900–U+1F9FF (🦂 🦎 🧠 🧱 🧼 🥑 🥬 etc.)
    // Symbols Extended-A:     U+1FA00–U+1FA6F
    // Symbols Extended-B:     U+1FA70–U+1FAFF (🪙 🪟 🪳 🪶 etc.)
    // Currency Symbols:       U+20A0–U+20CF  (₿ etc.)
    static const ImWchar emoji_ranges[] = {
        0x2000, 0x2BFF,    // General Punctuation through Misc Symbols & Arrows
        0x20A0, 0x20CF,    // Currency Symbols (₿)
        0x1F300, 0x1F9FF,  // Misc Symbols & Pictographs, Emoticons, Transport, Supplemental
        0x1FA00, 0x1FAFF,  // Symbols Extended-A & B (🪙 🪟 🪳 🪶 etc.)
        0,
    };

    for (const char* emoji_path : emoji_font_candidates) {
        if (std::filesystem::exists(emoji_path)) {
            ImFontConfig merge_cfg;
            merge_cfg.MergeMode = true;
            merge_cfg.OversampleH = 1;
            merge_cfg.OversampleV = 1;
            merge_cfg.PixelSnapH = true;
            merge_cfg.RasterizerDensity = dpi_scale;
            merge_cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

            ImFont* emoji = io.Fonts->AddFontFromFileTTF(
                emoji_path, font_size, &merge_cfg, emoji_ranges);
            if (emoji) {
                LOG_INFO("Font", "Merged emoji font: %s (color)", emoji_path);
            } else {
                LOG_WARN("Font", "Failed to merge emoji font: %s", emoji_path);
            }
            break;
        }
    }

    io.Fonts->Build();
}

} // namespace fincept::core
