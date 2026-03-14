// Geopolitics Screen — full implementation with interactive tile map
// Three-panel layout: filters | map+events feed | event detail

#include "geopolitics_screen.h"
#include "auth/auth_manager.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace fincept::geopolitics {

using namespace theme::colors;

// ============================================================================
// Auth event subscription
// ============================================================================
void GeopoliticsScreen::subscribe_auth_events() {
    if (auth_sub_id_ != 0) return; // Already subscribed
    auth_sub_id_ = core::EventBus::instance().subscribe("auth.session_changed",
        [this](const core::Event& e) {
            bool authed = e.get<bool>("authenticated", false);
            if (authed) {
                LOG_INFO("Geo", "Auth changed — will re-init on next frame");
                needs_reinit_ = true;
            }
        });
}

// ============================================================================
// Init & data refresh
// ============================================================================
void GeopoliticsScreen::init() {
    subscribe_auth_events();

    auto& auth = auth::AuthManager::instance();
    std::string api_key = auth.session().api_key;
    bool authed = auth.is_authenticated();
    std::string user_type = auth.session().user_type;
    std::string session_token = auth.session().session_token;

    LOG_INFO("Geo", "=== INIT: authenticated=%d, user_type='%s', api_key_len=%zu, session_token_len=%zu",
             (int)authed, user_type.c_str(), api_key.size(), session_token.size());

    if (api_key.empty()) {
        LOG_WARN("Geo", "No API key available, waiting for auth... (authenticated=%d)", (int)authed);
        return;
    }

    LOG_INFO("Geo", "Starting data fetch with api_key='%s...' country='%s'",
             api_key.substr(0, std::min((size_t)8, api_key.size())).c_str(),
             active_country_.c_str());

    data_.fetch_countries(api_key);
    data_.fetch_categories(api_key);
    data_.fetch_events(api_key, active_country_, active_category_, 1, 50);
    last_fetch_ = ImGui::GetTime();

    // Default map view: India
    tile_map_.set_view(22.0, 78.0, 4);

    initialized_ = true;
}

// ============================================================================
// Build map points from events
// ============================================================================
void GeopoliticsScreen::build_map_points() {
    map_points_.clear();
    int skipped_no_coords = 0;
    for (int i = 0; i < (int)events_snapshot_.size(); i++) {
        auto& ev = events_snapshot_[i];
        if (ev.latitude == 0 && ev.longitude == 0) {
            skipped_no_coords++;
            continue;
        }

        ui::MapPoint pt;
        pt.lat = ev.latitude;
        pt.lon = ev.longitude;
        pt.color = category_color(ev.event_category);
        pt.radius = 5.0f;
        pt.id = i;

        // Build tooltip
        pt.label = ev.event_category + " | " + ev.city;
        if (!ev.country.empty()) pt.label += " (" + ev.country + ")";
        if (!ev.matched_keywords.empty()) pt.label += "\n" + ev.matched_keywords;

        map_points_.push_back(pt);
    }

    // Log only when something changed
    static size_t last_points_count = 999;
    if (map_points_.size() != last_points_count) {
        LOG_INFO("Geo", "build_map_points: %zu points from %zu events (skipped %d with no coords)",
                 map_points_.size(), events_snapshot_.size(), skipped_no_coords);
        last_points_count = map_points_.size();
    }

    map_points_dirty_ = false;
}

// ============================================================================
// Main render
// ============================================================================
void GeopoliticsScreen::render() {
    // Subscribe even before init (so we catch auth events while waiting)
    subscribe_auth_events();

    // Re-init when auth state changes
    if (needs_reinit_.exchange(false)) {
        LOG_INFO("Geo", "Re-initializing after auth change");
        initialized_ = false;
    }

    if (!initialized_) init();

    // Auto-refresh every 5 minutes
    if (ImGui::GetTime() - last_fetch_ > 300.0 && !data_.is_loading()) {
        auto& auth = auth::AuthManager::instance();
        std::string api_key = auth.session().api_key;
        if (!api_key.empty()) {
            data_.fetch_events(api_key, active_country_, active_category_, current_page_, 50);
            last_fetch_ = ImGui::GetTime();
        }
    }

    // Snapshot data for this frame
    if (!data_.is_loading()) {
        auto new_events = data_.events();
        auto new_countries = data_.countries();
        auto new_categories = data_.categories();

        // Log when data changes
        if (new_events.size() != events_snapshot_.size() ||
            new_countries.size() != countries_snapshot_.size() ||
            new_categories.size() != categories_snapshot_.size()) {
            LOG_INFO("Geo", "SNAPSHOT UPDATE: events %zu->%zu, countries %zu->%zu, categories %zu->%zu",
                     events_snapshot_.size(), new_events.size(),
                     countries_snapshot_.size(), new_countries.size(),
                     categories_snapshot_.size(), new_categories.size());
        }

        events_snapshot_ = std::move(new_events);
        countries_snapshot_ = std::move(new_countries);
        categories_snapshot_ = std::move(new_categories);
        build_map_points();
    } else {
        // Log loading state periodically (every ~2 seconds)
        static double last_loading_log = 0;
        if (ImGui::GetTime() - last_loading_log > 2.0) {
            LOG_DEBUG("Geo", "Still loading... events=%zu, error='%s'",
                      events_snapshot_.size(), data_.error().c_str());
            last_loading_log = ImGui::GetTime();
        }
    }

    // Screen frame
    ui::ScreenFrame frame("##geopolitics_screen", ImVec2(4, 4), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    ImVec2 avail = frame.content_size();

    // Yoga vertical stack: toolbar(frame_h+8) + panels(flex) + stats(frame_h+4)
    float toolbar_h = ImGui::GetFrameHeight() + 8;
    float stats_h = ImGui::GetFrameHeight() + 4;
    auto vstack = ui::vstack_layout(avail.x, avail.y, {toolbar_h, -1, stats_h}, 4);

    // Toolbar
    render_toolbar(avail.x);

    float panels_h = vstack.heights[1];

    // Three-panel layout (responsive)
    if (frame.is_compact()) {
        // Stacked: map on top, events below — use Yoga for vertical split
        auto vsplit = ui::vstack_layout(avail.x, panels_h, {-1, -1});
        render_map_panel(avail.x, vsplit.heights[0]);
        render_events_feed(avail.x, vsplit.heights[1]);
    } else if (frame.is_medium()) {
        // Two panels: filters + map/events — use Yoga two_panel_layout
        auto med = ui::two_panel_layout(avail.x, panels_h, true, 22, 200, 260);

        ImGui::BeginChild("##geo_filters", ImVec2(med.side_w, med.side_h), ImGuiChildFlags_Borders);
        render_filter_panel(med.side_w, med.side_h);
        ImGui::EndChild();

        ImGui::SameLine(0, 4);

        ImGui::BeginChild("##geo_main", ImVec2(med.main_w, med.main_h), ImGuiChildFlags_Borders);
        auto inner_v = ui::vstack_layout(med.main_w, med.main_h, {-1.0f, -1.0f}, 4);
        render_map_panel(med.main_w, inner_v.heights[0]);
        ImGui::Spacing();
        render_events_feed(med.main_w, inner_v.heights[1]);
        ImGui::EndChild();
    } else {
        // Full three-panel: filters | map+events | detail
        // min_left=240 so country names + counts aren't truncated
        auto sizes = ui::three_panel_layout(avail.x, panels_h, 16, 22, 240, 240, 280, 4);

        ImGui::BeginChild("##geo_filters", ImVec2(sizes.left_w, panels_h), ImGuiChildFlags_Borders);
        render_filter_panel(sizes.left_w, panels_h);
        ImGui::EndChild();

        ImGui::SameLine(0, sizes.gap);

        ImGui::BeginChild("##geo_center", ImVec2(sizes.center_w, panels_h), ImGuiChildFlags_Borders);
        {
            float center_w = ImGui::GetContentRegionAvail().x;
            float center_h = ImGui::GetContentRegionAvail().y;

            if (view_mode_ == 0) {
                // Split: map top (60%), events bottom (40%) via Yoga
                auto cv = ui::vstack_layout(center_w, center_h, {-1.2f, -1.0f}, 4);
                render_map_panel(center_w, cv.heights[0]);
                render_events_feed(center_w, cv.heights[1]);
            } else if (view_mode_ == 1) {
                // Map only
                render_map_panel(center_w, center_h);
            } else {
                // Events only
                render_events_feed(center_w, center_h);
            }
        }
        ImGui::EndChild();

        if (sizes.right_w > 0) {
            ImGui::SameLine(0, sizes.gap);
            ImGui::BeginChild("##geo_detail", ImVec2(sizes.right_w, panels_h), ImGuiChildFlags_Borders);
            render_event_detail(sizes.right_w, panels_h);
            ImGui::EndChild();
        }
    }

    // Stats bar
    render_stats_bar(avail.x);

    frame.end();
}

// ============================================================================
// Toolbar
// ============================================================================
void GeopoliticsScreen::render_toolbar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##geo_toolbar", ImVec2(width, ImGui::GetFrameHeight() + 8), ImGuiChildFlags_Borders);

    ImGui::TextColored(ACCENT, "GEOPOLITICS");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "|");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_SECONDARY, "Global Intelligence Monitor");

    // View mode toggle buttons
    ImGui::SameLine(0, 20);
    {
        bool is_split = (view_mode_ == 0);
        if (is_split) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        if (ImGui::SmallButton("Map+List")) view_mode_ = 0;
        if (is_split) ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 4);
    {
        bool is_map = (view_mode_ == 1);
        if (is_map) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        if (ImGui::SmallButton("Map")) view_mode_ = 1;
        if (is_map) ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 4);
    {
        bool is_list = (view_mode_ == 2);
        if (is_list) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        if (ImGui::SmallButton("List")) view_mode_ = 2;
        if (is_list) ImGui::PopStyleColor();
    }

    ImGui::SameLine(width - 200);

    if (data_.is_loading()) {
        theme::LoadingSpinner("Fetching...");
    } else {
        if (theme::SecondaryButton("Refresh")) {
            auto& auth = auth::AuthManager::instance();
            std::string api_key = auth.session().api_key;
            if (!api_key.empty()) {
                data_.fetch_events(api_key, active_country_, active_category_, current_page_, 50);
                data_.fetch_countries(api_key);
                data_.fetch_categories(api_key);
                last_fetch_ = ImGui::GetTime();
                map_points_dirty_ = true;
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ============================================================================
// Map Panel
// ============================================================================
void GeopoliticsScreen::render_map_panel(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.067f, 0.067f, 0.071f, 1.0f));
    ImGui::BeginChild("##geo_map", ImVec2(width, height), ImGuiChildFlags_Borders);

    ImVec2 map_pos = ImGui::GetCursorScreenPos();
    ImVec2 map_size = ImGui::GetContentRegionAvail();

    // Render the tile map with event dots
    int clicked = tile_map_.render(map_pos, map_size, map_points_);

    if (clicked >= 0 && clicked < (int)events_snapshot_.size()) {
        selected_event_ = clicked;

        // Center map on clicked event
        auto& ev = events_snapshot_[clicked];
        if (ev.latitude != 0 || ev.longitude != 0) {
            tile_map_.set_view(ev.latitude, ev.longitude, std::max(tile_map_.zoom(), 5));
        }
    }

    // Reserve the space so ImGui knows the child is occupied
    ImGui::Dummy(map_size);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Filter Panel (Left)
// ============================================================================
void GeopoliticsScreen::render_filter_panel(float /*width*/, float /*height*/) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    // Country filter
    theme::SectionHeader("COUNTRIES");

    ImGui::PushItemWidth(-1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputTextWithHint("##country_search", "Search country...", country_filter_buf_, sizeof(country_filter_buf_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Country list
    std::string filter_lower;
    for (char c : std::string(country_filter_buf_))
        filter_lower += (char)std::tolower((unsigned char)c);

    for (auto& c : countries_snapshot_) {
        if (!filter_lower.empty()) {
            std::string name_lower;
            for (char ch : c.country) name_lower += (char)std::tolower((unsigned char)ch);
            if (name_lower.find(filter_lower) == std::string::npos) continue;
        }

        bool is_active = (c.country == active_country_);
        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        // Use available width to right-align count instead of fixed-width format
        char label[256];
        std::snprintf(label, sizeof(label), "%s  %d##c_%s", c.country.c_str(), c.event_count, c.country.c_str());
        if (ImGui::Button(label, ImVec2(-1, 0))) {
            active_country_ = c.country;
            selected_event_ = -1;
            current_page_ = 1;
            map_points_dirty_ = true;

            auto& auth = auth::AuthManager::instance();
            std::string api_key = auth.session().api_key;
            if (!api_key.empty()) {
                data_.fetch_events(api_key, active_country_, active_category_, 1, 50);
                last_fetch_ = ImGui::GetTime();
            }
        }

        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing(); ImGui::Spacing();

    // Category filter
    theme::SectionHeader("CATEGORIES");

    ImGui::PushItemWidth(-1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputTextWithHint("##cat_search", "Search category...", category_filter_buf_, sizeof(category_filter_buf_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // "All" button
    {
        bool is_all = active_category_.empty();
        if (is_all) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }
        if (ImGui::Button("All Categories", ImVec2(-1, 0))) {
            active_category_.clear();
            selected_event_ = -1;
            current_page_ = 1;
            map_points_dirty_ = true;

            auto& auth = auth::AuthManager::instance();
            std::string api_key = auth.session().api_key;
            if (!api_key.empty()) {
                data_.fetch_events(api_key, active_country_, active_category_, 1, 50);
                last_fetch_ = ImGui::GetTime();
            }
        }
        ImGui::PopStyleColor(2);
    }

    std::string cat_filter_lower;
    for (char c : std::string(category_filter_buf_))
        cat_filter_lower += (char)std::tolower((unsigned char)c);

    for (auto& cat : categories_snapshot_) {
        if (!cat_filter_lower.empty()) {
            std::string name_lower;
            for (char ch : cat.event_category) name_lower += (char)std::tolower((unsigned char)ch);
            if (name_lower.find(cat_filter_lower) == std::string::npos) continue;
        }

        bool is_active = (cat.event_category == active_category_);
        ImVec4 cat_col = category_color_vec(cat.event_category);

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(cat_col.x, cat_col.y, cat_col.z, 0.2f));
            ImGui::PushStyleColor(ImGuiCol_Text, cat_col);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        char label[256];
        std::snprintf(label, sizeof(label), "%s  (%d)", cat.event_category.c_str(), cat.event_count);
        if (ImGui::Button(label, ImVec2(-1, 0))) {
            active_category_ = cat.event_category;
            selected_event_ = -1;
            current_page_ = 1;
            map_points_dirty_ = true;

            auto& auth = auth::AuthManager::instance();
            std::string api_key = auth.session().api_key;
            if (!api_key.empty()) {
                data_.fetch_events(api_key, active_country_, active_category_, 1, 50);
                last_fetch_ = ImGui::GetTime();
            }
        }

        ImGui::PopStyleColor(2);
    }

    // Legend
    ImGui::Spacing(); ImGui::Spacing();
    theme::SectionHeader("LEGEND");
    const char* legend_cats[] = {
        "armed_conflict", "terrorism", "protests", "civilian_violence",
        "riots", "political_violence", "crisis", "explosions", "strategic"
    };
    for (auto cat_name : legend_cats) {
        ImVec4 col = category_color_vec(cat_name);
        ImGui::ColorButton(cat_name, col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(10, 10));
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "%s", cat_name);
    }

    ImGui::PopStyleColor();
}

// ============================================================================
// Events Feed (Center bottom or standalone)
// ============================================================================
void GeopoliticsScreen::render_events_feed(float width, float /*height*/) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);

    // Header
    ImGui::TextColored(ACCENT, "EVENTS");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "(%zu)", events_snapshot_.size());
    if (!active_country_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(TEXT_SECONDARY, "| %s", active_country_.c_str());
    }
    if (!active_category_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(category_color_vec(active_category_), "| %s", active_category_.c_str());
    }

    ImGui::Separator();

    // Error display
    std::string err = data_.error();
    if (!err.empty()) {
        theme::ErrorMessage(err.c_str());
    }

    // Events list
    if (events_snapshot_.empty() && !data_.is_loading()) {
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "  No events found for current filters.");
    }

    for (int i = 0; i < (int)events_snapshot_.size(); i++) {
        render_event_row(i, events_snapshot_[i], width);
    }

    // Pagination
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    int total_pg = data_.total_pages();
    if (total_pg > 1) {
        float content_w = ImGui::GetContentRegionAvail().x;
        float btn_w = 80.0f;
        float total_w = btn_w * 2 + 120;
        ImGui::SetCursorPosX((content_w - total_w) / 2.0f);

        if (current_page_ <= 1) ImGui::BeginDisabled();
        if (theme::SecondaryButton("< Prev", ImVec2(btn_w, 0))) {
            current_page_--;
            auto& auth = auth::AuthManager::instance();
            std::string api_key = auth.session().api_key;
            if (!api_key.empty()) {
                data_.fetch_events(api_key, active_country_, active_category_, current_page_, 50);
                last_fetch_ = ImGui::GetTime();
            }
            selected_event_ = -1;
            map_points_dirty_ = true;
        }
        if (current_page_ <= 1) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextColored(TEXT_SECONDARY, "Page %d / %d", current_page_, total_pg);
        ImGui::SameLine();

        if (current_page_ >= total_pg) ImGui::BeginDisabled();
        if (theme::SecondaryButton("Next >", ImVec2(btn_w, 0))) {
            current_page_++;
            auto& auth = auth::AuthManager::instance();
            std::string api_key = auth.session().api_key;
            if (!api_key.empty()) {
                data_.fetch_events(api_key, active_country_, active_category_, current_page_, 50);
                last_fetch_ = ImGui::GetTime();
            }
            selected_event_ = -1;
            map_points_dirty_ = true;
        }
        if (current_page_ >= total_pg) ImGui::EndDisabled();
    }

    ImGui::PopStyleColor();
}

// ============================================================================
// Event Row
// ============================================================================
void GeopoliticsScreen::render_event_row(int index, const GeoEvent& event, float /*width*/) {
    bool is_selected = (index == selected_event_);
    float row_h = 60.0f;

    ImGui::PushID(index);

    // Row background
    ImVec4 row_bg = is_selected ? ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.1f) : ImVec4(0, 0, 0, 0);
    ImGui::PushStyleColor(ImGuiCol_Header, row_bg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.06f));

    if (ImGui::Selectable("##event_row", is_selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, row_h))) {
        selected_event_ = index;
        if (event.latitude != 0 || event.longitude != 0) {
            tile_map_.set_view(event.latitude, event.longitude, std::max(tile_map_.zoom(), 5));
        }
    }

    // Draw all content via ImDrawList so it doesn't affect ImGui cursor
    ImVec2 row_min = ImGui::GetItemRectMin();
    ImVec2 row_max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float font_h = ImGui::GetFontSize();

    // Category color bar on left
    ImU32 cat_col = category_color(event.event_category);
    dl->AddRectFilled(row_min, ImVec2(row_min.x + 4, row_max.y), cat_col);

    float x0 = row_min.x + 12.0f;
    float y0 = row_min.y + 4.0f;

    // Line 1: category | city (country)
    {
        ImU32 cat_text_col = ImGui::ColorConvertFloat4ToU32(category_color_vec(event.event_category));
        ImU32 dim_col = ImGui::ColorConvertFloat4ToU32(TEXT_DIM);
        ImU32 sec_col = ImGui::ColorConvertFloat4ToU32(TEXT_SECONDARY);

        float cx = x0;
        dl->AddText(ImVec2(cx, y0), cat_text_col, event.event_category.c_str());
        cx += ImGui::CalcTextSize(event.event_category.c_str()).x + 6;

        dl->AddText(ImVec2(cx, y0), dim_col, "|");
        cx += ImGui::CalcTextSize("|").x + 6;

        dl->AddText(ImVec2(cx, y0), sec_col, event.city.c_str());
        cx += ImGui::CalcTextSize(event.city.c_str()).x;

        if (!event.country.empty()) {
            char country_buf[256];
            std::snprintf(country_buf, sizeof(country_buf), " (%s)", event.country.c_str());
            dl->AddText(ImVec2(cx, y0), dim_col, country_buf);
        }
    }

    // Line 2: domain
    float y1 = y0 + font_h + 2;
    dl->AddText(ImVec2(x0, y1), ImGui::ColorConvertFloat4ToU32(TEXT_DIM), event.domain.c_str());

    // Line 3: date
    float y2 = y1 + font_h + 2;
    dl->AddText(ImVec2(x0, y2), ImGui::ColorConvertFloat4ToU32(TEXT_DISABLED), event.extracted_date.c_str());

    // Keywords on right
    if (!event.matched_keywords.empty()) {
        ImU32 warn_col = ImGui::ColorConvertFloat4ToU32(WARNING);
        float kw_w = ImGui::CalcTextSize(event.matched_keywords.c_str()).x;
        float right_x = row_max.x - kw_w - 8;
        dl->AddText(ImVec2(right_x, y0), warn_col, event.matched_keywords.c_str());
    }

    ImGui::PopStyleColor(2);

    // Bottom separator line
    dl->AddLine(ImVec2(row_min.x, row_max.y), ImVec2(row_max.x, row_max.y),
                ImGui::ColorConvertFloat4ToU32(TEXT_DISABLED) & 0x40FFFFFF);

    ImGui::PopID();
}

// ============================================================================
// Event Detail (Right)
// ============================================================================
void GeopoliticsScreen::render_event_detail(float /*width*/, float /*height*/) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    theme::SectionHeader("EVENT DETAIL");

    if (selected_event_ < 0 || selected_event_ >= (int)events_snapshot_.size()) {
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "  Select an event to view details.");
        ImGui::PopStyleColor();
        return;
    }

    auto& ev = events_snapshot_[selected_event_];

    ImGui::Spacing();

    // Category badge
    ImVec4 cat_col = category_color_vec(ev.event_category);
    ImGui::TextColored(cat_col, "%s", ev.event_category.c_str());
    ImGui::Spacing();

    // Location
    theme::SectionHeader("LOCATION");
    ImGui::TextColored(TEXT_PRIMARY, "%s", ev.city.c_str());
    ImGui::TextColored(TEXT_SECONDARY, "%s", ev.country.c_str());
    if (ev.latitude != 0 || ev.longitude != 0) {
        ImGui::TextColored(TEXT_DIM, "%.4f, %.4f", ev.latitude, ev.longitude);

        // "Go to" button — centers map on this event
        if (theme::SecondaryButton("Center on Map")) {
            tile_map_.set_view(ev.latitude, ev.longitude, std::max(tile_map_.zoom(), 6));
        }
    }

    ImGui::Spacing();

    // Date
    theme::SectionHeader("DATE");
    ImGui::TextColored(TEXT_PRIMARY, "%s", ev.extracted_date.c_str());
    if (!ev.created_at.empty()) {
        ImGui::TextColored(TEXT_DIM, "Indexed: %s", ev.created_at.c_str());
    }

    ImGui::Spacing();

    // Keywords
    if (!ev.matched_keywords.empty()) {
        theme::SectionHeader("KEYWORDS");
        ImGui::TextWrapped("%s", ev.matched_keywords.c_str());
    }

    ImGui::Spacing();

    // Source
    theme::SectionHeader("SOURCE");
    ImGui::TextColored(TEXT_SECONDARY, "%s", ev.domain.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("%s", ev.url.c_str());

    ImGui::PopStyleColor();
}

// ============================================================================
// Stats Bar (Bottom)
// ============================================================================
void GeopoliticsScreen::render_stats_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##geo_stats", ImVec2(width, ImGui::GetFrameHeight() + 4), ImGuiChildFlags_Borders);

    ImGui::TextColored(TEXT_DIM, "Events: %d", data_.total_events());
    ImGui::SameLine(0, 20);
    ImGui::TextColored(TEXT_DIM, "Countries: %zu", countries_snapshot_.size());
    ImGui::SameLine(0, 20);
    ImGui::TextColored(TEXT_DIM, "Categories: %zu", categories_snapshot_.size());
    ImGui::SameLine(0, 20);
    ImGui::TextColored(TEXT_DIM, "Zoom: %d", tile_map_.zoom());
    ImGui::SameLine(0, 20);

    if (!active_country_.empty()) {
        ImGui::TextColored(ACCENT, "Filter: %s", active_country_.c_str());
        ImGui::SameLine(0, 8);
    }
    if (!active_category_.empty()) {
        ImGui::TextColored(category_color_vec(active_category_), "| %s", active_category_.c_str());
        ImGui::SameLine(0, 8);
    }

    if (data_.is_loading()) {
        ImGui::SameLine(width - 100);
        theme::LoadingSpinner("Loading...");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::geopolitics
