#include "terminal_appearance_section.h"
#include "screens/settings/settings_types.h"
#include "storage/database.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>

namespace fincept::settings {

// ============================================================================
// Default theme presets
// ============================================================================
struct ThemePreset {
    const char* name;
    ImVec4 colors[12];  // primary, secondary, success, alert, warning, info,
                        // accent, purple, text, textMuted, background, panel
};

static const ThemePreset THEME_PRESETS[] = {
    {"Fincept Classic", {
        {1.0f, 0.53f, 0.0f, 1.0f},   // primary (orange)
        {0.0f, 0.84f, 0.44f, 1.0f},   // secondary (green)
        {0.0f, 0.84f, 0.44f, 1.0f},   // success
        {1.0f, 0.23f, 0.23f, 1.0f},   // alert
        {1.0f, 0.78f, 0.08f, 1.0f},   // warning
        {0.30f, 0.60f, 0.96f, 1.0f},  // info
        {1.0f, 0.45f, 0.05f, 1.0f},   // accent
        {0.67f, 0.33f, 1.0f, 1.0f},   // purple
        {0.95f, 0.95f, 0.96f, 1.0f},  // text
        {0.53f, 0.53f, 0.57f, 1.0f},  // textMuted
        {0.07f, 0.07f, 0.07f, 1.0f},  // background
        {0.12f, 0.12f, 0.13f, 1.0f},  // panel
    }},
    {"Matrix Green", {
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.8f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.6f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.5f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.5f, 0.0f, 1.0f},
        {0.0f, 0.02f, 0.0f, 1.0f},
        {0.0f, 0.06f, 0.02f, 1.0f},
    }},
    {"Blue Terminal", {
        {0.2f, 0.5f, 1.0f, 1.0f},
        {0.1f, 0.4f, 0.8f, 1.0f},
        {0.0f, 0.8f, 0.4f, 1.0f},
        {1.0f, 0.3f, 0.3f, 1.0f},
        {1.0f, 0.7f, 0.1f, 1.0f},
        {0.3f, 0.7f, 1.0f, 1.0f},
        {0.2f, 0.5f, 1.0f, 1.0f},
        {0.6f, 0.3f, 1.0f, 1.0f},
        {0.9f, 0.93f, 0.97f, 1.0f},
        {0.5f, 0.55f, 0.65f, 1.0f},
        {0.04f, 0.05f, 0.1f, 1.0f},
        {0.07f, 0.08f, 0.14f, 1.0f},
    }},
    {"Amber Retro", {
        {1.0f, 0.7f, 0.0f, 1.0f},
        {0.9f, 0.6f, 0.0f, 1.0f},
        {0.0f, 0.8f, 0.3f, 1.0f},
        {1.0f, 0.2f, 0.2f, 1.0f},
        {1.0f, 0.8f, 0.0f, 1.0f},
        {0.3f, 0.6f, 0.9f, 1.0f},
        {1.0f, 0.7f, 0.0f, 1.0f},
        {0.7f, 0.3f, 1.0f, 1.0f},
        {1.0f, 0.85f, 0.3f, 1.0f},
        {0.6f, 0.45f, 0.1f, 1.0f},
        {0.05f, 0.04f, 0.0f, 1.0f},
        {0.1f, 0.08f, 0.02f, 1.0f},
    }},
    {"Purple Neon", {
        {0.7f, 0.2f, 1.0f, 1.0f},
        {0.9f, 0.1f, 0.6f, 1.0f},
        {0.0f, 1.0f, 0.6f, 1.0f},
        {1.0f, 0.1f, 0.3f, 1.0f},
        {1.0f, 0.8f, 0.0f, 1.0f},
        {0.1f, 0.6f, 1.0f, 1.0f},
        {0.7f, 0.2f, 1.0f, 1.0f},
        {0.9f, 0.1f, 0.9f, 1.0f},
        {0.9f, 0.85f, 0.95f, 1.0f},
        {0.55f, 0.45f, 0.6f, 1.0f},
        {0.06f, 0.03f, 0.08f, 1.0f},
        {0.1f, 0.05f, 0.13f, 1.0f},
    }},
};

static constexpr int NUM_PRESETS = sizeof(THEME_PRESETS) / sizeof(THEME_PRESETS[0]);

static const char* COLOR_LABELS[] = {
    "Primary", "Secondary", "Success", "Alert", "Warning", "Info",
    "Accent", "Purple", "Text", "Text Muted", "Background", "Panel"
};

// ============================================================================
// Init / Load / Save
// ============================================================================
void TerminalAppearanceSection::init() {
    if (initialized_) return;
    load_settings();

    // Copy default theme colors
    for (int i = 0; i < 12; i++) {
        custom_colors_[i] = THEME_PRESETS[0].colors[i];
    }

    // Default F-key mappings
    const char* defaults[] = {
        "Dashboard", "Markets", "News", "Portfolio", "Analytics", "Watchlist",
        "Research", "Screener", "Trading", "Chat", "Fullscreen", "Profile"
    };
    for (int i = 0; i < 12; i++) {
        std::strncpy(fkey_mappings_[i].tab_name, defaults[i],
                     sizeof(fkey_mappings_[i].tab_name) - 1);
    }

    initialized_ = true;
}

void TerminalAppearanceSection::load_settings() {
    auto font_val = db::ops::get_setting("font_family");
    if (font_val) {
        const auto& fonts = font_families();
        for (int i = 0; i < (int)fonts.size(); i++) {
            if (*font_val == fonts[i]) { font_family_ = i; break; }
        }
    }

    auto size_val = db::ops::get_setting("font_size");
    if (size_val) font_size_ = std::atoi(size_val->c_str());

    auto density_val = db::ops::get_setting("content_density");
    if (density_val) content_density_ = std::atoi(density_val->c_str());

    auto tz_val = db::ops::get_setting("timezone");
    if (tz_val) {
        const auto& zones = timezone_list();
        for (int i = 0; i < (int)zones.size(); i++) {
            if (*tz_val == zones[i].value) { selected_timezone_ = i; break; }
        }
    }

    auto theme_val = db::ops::get_setting("color_theme");
    if (theme_val) {
        for (int i = 0; i < NUM_PRESETS; i++) {
            if (*theme_val == THEME_PRESETS[i].name) {
                selected_theme_ = i;
                for (int j = 0; j < 12; j++)
                    custom_colors_[j] = THEME_PRESETS[i].colors[j];
                break;
            }
        }
    }
}

void TerminalAppearanceSection::save_settings() {
    const auto& fonts = font_families();
    if (font_family_ >= 0 && font_family_ < (int)fonts.size())
        db::ops::save_setting("font_family", fonts[font_family_], "appearance");

    db::ops::save_setting("font_size", std::to_string(font_size_), "appearance");
    db::ops::save_setting("content_density", std::to_string(content_density_), "appearance");

    const auto& zones = timezone_list();
    if (selected_timezone_ >= 0 && selected_timezone_ < (int)zones.size())
        db::ops::save_setting("timezone", zones[selected_timezone_].value, "appearance");

    if (selected_theme_ >= 0 && selected_theme_ < NUM_PRESETS)
        db::ops::save_setting("color_theme", THEME_PRESETS[selected_theme_].name, "appearance");

    status_ = "Appearance settings saved";
    status_time_ = ImGui::GetTime();
}

// ============================================================================
// Sub-section renderers
// ============================================================================
void TerminalAppearanceSection::render_theme_section() {
    using namespace theme::colors;

    theme::SectionHeader("COLOR THEME");

    // Theme preset selector
    ImGui::TextColored(TEXT_SECONDARY, "Preset");
    ImGui::SameLine(100);
    ImGui::SetNextItemWidth(200);

    if (ImGui::BeginCombo("##theme_preset", THEME_PRESETS[selected_theme_].name)) {
        for (int i = 0; i < NUM_PRESETS; i++) {
            bool selected = (i == selected_theme_);
            if (ImGui::Selectable(THEME_PRESETS[i].name, selected)) {
                selected_theme_ = i;
                for (int j = 0; j < 12; j++)
                    custom_colors_[j] = THEME_PRESETS[i].colors[j];
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // Color editors in 2-column grid
    if (ImGui::BeginTable("##colors", 2, ImGuiTableFlags_None)) {
        for (int i = 0; i < 12; i++) {
            ImGui::TableNextColumn();
            char label[64];
            std::snprintf(label, sizeof(label), "%s##color_%d", COLOR_LABELS[i], i);
            ImGui::ColorEdit4(label, &custom_colors_[i].x,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        }
        ImGui::EndTable();
    }

    // Preview swatch
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Preview:");
    float swatch_w = 30.0f;
    for (int i = 0; i < 12; i++) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + swatch_w, p.y + 20), ImGui::ColorConvertFloat4ToU32(custom_colors_[i]));
        ImGui::Dummy(ImVec2(swatch_w, 20));
        if (i < 11) ImGui::SameLine(0, 2);
    }
}

void TerminalAppearanceSection::render_font_section() {
    using namespace theme::colors;

    theme::SectionHeader("FONT");

    float label_w = 100.0f;

    // Font family
    ImGui::TextColored(TEXT_SECONDARY, "Family");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    const auto& fonts = font_families();
    if (ImGui::BeginCombo("##font_family",
            font_family_ >= 0 && font_family_ < (int)fonts.size()
                ? fonts[font_family_] : "Unknown")) {
        for (int i = 0; i < (int)fonts.size(); i++) {
            if (ImGui::Selectable(fonts[i], i == font_family_))
                font_family_ = i;
        }
        ImGui::EndCombo();
    }

    // Font size
    ImGui::TextColored(TEXT_SECONDARY, "Size");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    ImGui::SliderInt("##font_size", &font_size_, 9, 18, "%d px");

    // Font weight
    ImGui::TextColored(TEXT_SECONDARY, "Weight");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    const char* weights[] = {"Normal", "Semi-bold", "Bold"};
    ImGui::Combo("##font_weight", &font_weight_, weights, 3);

    // Italic
    ImGui::TextColored(TEXT_SECONDARY, "Italic");
    ImGui::SameLine(label_w);
    ImGui::Checkbox("##font_italic", &font_italic_);
}

void TerminalAppearanceSection::render_layout_section() {
    using namespace theme::colors;

    theme::SectionHeader("LAYOUT");

    float label_w = 120.0f;

    // Content density
    ImGui::TextColored(TEXT_SECONDARY, "Content Density");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    const char* densities[] = {"Compact", "Default", "Comfortable"};
    ImGui::Combo("##density", &content_density_, densities, 3);

    // Border radius
    ImGui::TextColored(TEXT_SECONDARY, "Border Radius");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("##border_radius", &border_radius_, 0.0f, 8.0f, "%.1f px");

    // Border style
    ImGui::TextColored(TEXT_SECONDARY, "Border Style");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    const char* border_styles[] = {"None", "Subtle", "Normal", "Prominent"};
    ImGui::Combo("##border_style", &border_style_, border_styles, 4);
}

void TerminalAppearanceSection::render_effects_section() {
    using namespace theme::colors;

    theme::SectionHeader("EFFECTS");

    float label_w = 120.0f;

    // Widget opacity
    ImGui::TextColored(TEXT_SECONDARY, "Widget Opacity");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("##widget_opacity", &widget_opacity_, 0.3f, 1.0f, "%.0f%%");

    // Glow effects
    ImGui::TextColored(TEXT_SECONDARY, "Glow Effects");
    ImGui::SameLine(label_w);
    ImGui::Checkbox("##glow", &glow_effects_);

    if (glow_effects_) {
        ImGui::TextColored(TEXT_SECONDARY, "Glow Intensity");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##glow_intensity", &glow_intensity_, 0.1f, 1.0f, "%.1f");
    }
}

void TerminalAppearanceSection::render_timezone_section() {
    using namespace theme::colors;

    theme::SectionHeader("TIMEZONE");

    const auto& zones = timezone_list();

    ImGui::TextColored(TEXT_SECONDARY, "Timezone");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(300);

    if (ImGui::BeginCombo("##timezone",
            selected_timezone_ >= 0 && selected_timezone_ < (int)zones.size()
                ? zones[selected_timezone_].label : "UTC")) {
        for (int i = 0; i < (int)zones.size(); i++) {
            if (ImGui::Selectable(zones[i].label, i == selected_timezone_))
                selected_timezone_ = i;
        }
        ImGui::EndCombo();
    }
}

void TerminalAppearanceSection::render_fkey_section() {
    using namespace theme::colors;

    theme::SectionHeader("FUNCTION KEY MAPPINGS");

    ImGui::TextColored(TEXT_DIM, "Map F1-F12 keys to tab shortcuts.");
    ImGui::Spacing();

    if (ImGui::BeginTable("##fkeys", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Tab", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < 12; i++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char key_label[8];
            std::snprintf(key_label, sizeof(key_label), "F%d", i + 1);
            ImGui::TextColored(ACCENT, "%s", key_label);

            ImGui::TableNextColumn();
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##fkey", fkey_mappings_[i].tab_name,
                             sizeof(fkey_mappings_[i].tab_name));
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Main render
// ============================================================================
void TerminalAppearanceSection::render() {
    init();

    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "TERMINAL APPEARANCE");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
    if (theme::AccentButton("Save All")) {
        save_settings();
    }
    ImGui::Separator();

    // Status message
    if (!status_.empty()) {
        double elapsed = ImGui::GetTime() - status_time_;
        if (elapsed < 5.0) {
            ImGui::TextColored(SUCCESS, "%s", status_.c_str());
        } else {
            status_.clear();
        }
    }

    ImGui::Spacing();

    // Scrollable content with all sub-sections
    ImGui::BeginChild("##appearance_content", ImVec2(0, 0));

    render_theme_section();
    ImGui::Spacing();
    render_font_section();
    ImGui::Spacing();
    render_layout_section();
    ImGui::Spacing();
    render_effects_section();
    ImGui::Spacing();
    render_timezone_section();
    ImGui::Spacing();
    render_fkey_section();

    ImGui::EndChild();
}

} // namespace fincept::settings
