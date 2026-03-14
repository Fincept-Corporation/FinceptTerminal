#pragma once
// Terminal Appearance Section — theme, font, layout, timezone, function keys
// Mirrors TerminalAppearanceSection.tsx from the Tauri project

#include <imgui.h>
#include <string>

namespace fincept::settings {

class TerminalAppearanceSection {
public:
    void render();

private:
    bool initialized_ = false;

    // Theme
    int selected_theme_ = 0;
    ImVec4 custom_colors_[12] = {};  // primary through panel

    // Font
    int font_family_ = 0;     // index into font_families()
    int font_size_ = 14;
    int font_weight_ = 0;     // 0=Normal, 1=Semi-bold, 2=Bold
    bool font_italic_ = false;

    // Layout
    int content_density_ = 1;  // 0=Compact, 1=Default, 2=Comfortable
    float border_radius_ = 1.0f;
    int border_style_ = 2;     // 0=None, 1=Subtle, 2=Normal, 3=Prominent

    // Effects
    float widget_opacity_ = 1.0f;
    bool glow_effects_ = false;
    float glow_intensity_ = 0.5f;

    // Timezone
    int selected_timezone_ = 0;

    // Function key mappings
    struct FKeyMapping {
        char tab_name[64] = {};
    };
    FKeyMapping fkey_mappings_[12] = {};

    // Status
    std::string status_;
    double status_time_ = 0;

    void init();
    void load_settings();
    void save_settings();

    void render_theme_section();
    void render_font_section();
    void render_layout_section();
    void render_effects_section();
    void render_timezone_section();
    void render_fkey_section();
};

} // namespace fincept::settings
