// Maritime Intelligence — Global shipping & trade route visualization
// Port of MaritimeTab.tsx — 2D Mercator map with ocean/air routes,
// vessel tracking via Fincept Marine API, intelligence dashboard

#include "maritime_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "http/http_client.h"
#include "auth/auth_manager.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>

namespace fincept::maritime {

using namespace theme::colors;

// ============================================================================
// Constants
// ============================================================================

static constexpr float PI = 3.14159265358979323846f;

// Status colors
static ImVec4 status_color(const std::string& s) {
    if (s == "active")   return SUCCESS;
    if (s == "delayed")  return WARNING;
    if (s == "critical") return ERROR_RED;
    return TEXT_DIM;
}

static ImVec4 threat_color(const std::string& t) {
    if (t == "low")      return SUCCESS;
    if (t == "medium")   return WARNING;
    if (t == "high")     return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    if (t == "critical") return ERROR_RED;
    return TEXT_DIM;
}

// ============================================================================
// Static data catalogs
// ============================================================================

static std::vector<TradeRoute> build_ocean_routes() {
    return {
        {"Mumbai > Rotterdam",  "$45B",  "active",  23, {18.94f, 72.84f}, {51.96f, 4.14f},   {0.39f, 0.71f, 1.0f, 0.6f}},
        {"Mumbai > Shanghai",   "$156B", "active",  89, {18.94f, 72.84f}, {31.35f, 121.64f},  {0.47f, 0.78f, 1.0f, 0.7f}},
        {"Mumbai > Singapore",  "$89B",  "active",  45, {18.94f, 72.84f}, {1.26f, 103.82f},   {0.39f, 0.71f, 1.0f, 0.65f}},
        {"Chennai > Tokyo",     "$67B",  "delayed", 34, {13.08f, 80.27f}, {35.44f, 139.64f},  {0.59f, 0.86f, 1.0f, 0.6f}},
        {"Kolkata > Hong Kong", "$45B",  "active",  28, {22.57f, 88.36f}, {22.29f, 114.16f},  {0.39f, 0.71f, 1.0f, 0.55f}},
        {"Mumbai > Dubai",      "$78B",  "critical",12, {18.94f, 72.84f}, {24.99f, 55.03f},   {1.0f, 0.63f, 0.39f, 0.6f}},
        {"Mumbai > New York",   "$123B", "active",  56, {18.94f, 72.84f}, {40.67f, -74.08f},  {0.47f, 0.78f, 1.0f, 0.75f}},
        {"Chennai > Sydney",    "$54B",  "active",  31, {13.08f, 80.27f}, {-33.95f, 151.21f}, {0.51f, 0.75f, 1.0f, 0.6f}},
        {"Cochin > Klang",      "$34B",  "active",  19, {9.97f, 76.27f},  {3.00f, 101.40f},   {0.71f, 0.78f, 1.0f, 0.5f}},
        {"Mumbai > Cape Town",  "$28B",  "delayed",  8, {18.94f, 72.84f}, {-33.91f, 18.42f},  {0.63f, 0.71f, 1.0f, 0.5f}},
    };
}

static std::vector<AirRoute> build_air_routes() {
    return {
        {"Mumbai > London",       "$12B", 45,  {19.09f, 72.87f}, {51.47f, -0.45f},   {1.0f, 0.78f, 0.39f, 0.7f}},
        {"Delhi > New York",      "$18B", 67,  {28.56f, 77.10f}, {40.64f, -73.78f},  {1.0f, 0.78f, 0.39f, 0.75f}},
        {"Mumbai > Dubai",        "$22B", 123, {19.09f, 72.87f}, {25.25f, 55.37f},   {1.0f, 0.78f, 0.39f, 0.8f}},
        {"Bangalore > Singapore", "$8B",  89,  {13.20f, 77.71f}, {1.36f, 103.99f},   {1.0f, 0.78f, 0.39f, 0.65f}},
        {"Delhi > Frankfurt",     "$15B", 56,  {28.56f, 77.10f}, {50.04f, 8.56f},    {1.0f, 0.78f, 0.39f, 0.7f}},
        {"Mumbai > Hong Kong",    "$11B", 78,  {19.09f, 72.87f}, {22.31f, 113.92f},  {1.0f, 0.78f, 0.39f, 0.65f}},
        {"Chennai > Tokyo",       "$9B",  34,  {12.99f, 80.17f}, {35.77f, 140.39f},  {1.0f, 0.78f, 0.39f, 0.6f}},
        {"Delhi > Sydney",        "$7B",  28,  {28.56f, 77.10f}, {-33.94f, 151.18f}, {1.0f, 0.78f, 0.39f, 0.6f}},
    };
}

static std::vector<PresetLocation> build_presets() {
    return {
        {"Mumbai Port",    18.94f, 72.84f,  "Port"},
        {"Shanghai Port",  31.35f, 121.64f, "Port"},
        {"Singapore Port", 1.26f,  103.82f, "Port"},
        {"Hong Kong Port", 22.29f, 114.16f, "Port"},
        {"Rotterdam Port", 51.96f, 4.14f,   "Port"},
        {"Dubai Port",     24.99f, 55.03f,  "Port"},
    };
}

// ============================================================================
// Init
// ============================================================================

void MaritimeScreen::init() {
    ocean_routes_ = build_ocean_routes();
    air_routes_   = build_air_routes();
    presets_      = build_presets();
    intel_.monitored_routes = (int)ocean_routes_.size() + (int)air_routes_.size();
}

// ============================================================================
// Async helpers
// ============================================================================

bool MaritimeScreen::is_async_busy() const {
    return async_task_.valid() &&
           async_task_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

void MaritimeScreen::check_async() {
    if (async_task_.valid() &&
        async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try { async_task_.get(); } catch (...) {}
    }
}

// ============================================================================
// API calls
// ============================================================================

void MaritimeScreen::load_area_vessels() {
    if (is_async_busy()) return;
    loading_vessels_ = true;
    vessel_error_.clear();

    async_task_ = std::async(std::launch::async, [this]() {
        // Mumbai port region search area
        nlohmann::json body;
        body["min_lat"] = 18.5;
        body["max_lat"] = 19.5;
        body["min_lng"] = 72.0;
        body["max_lng"] = 73.5;

        std::string token = auth::AuthManager::instance().session().session_token;

        auto& client = http::HttpClient::instance();
        auto response = client.post("https://api.fincept.in/marine/vessel/area-search",
                                     body.dump(),
                                     {{"Content-Type", "application/json"},
                                      {"X-Session-Token", token}});

        if (!response.success || response.body.empty()) {
            vessel_error_ = response.error.empty() ? "Failed to load vessels" : response.error;
            loading_vessels_ = false;
            return;
        }

        try {
            auto j = nlohmann::json::parse(response.body);
            if (j.value("success", false) && j.contains("data")) {
                auto& data = j["data"];
                auto& vess = data.contains("vessels") ? data["vessels"] : data;
                vessels_.clear();
                if (vess.is_array()) {
                    for (auto& v : vess) {
                        VesselData vd;
                        vd.imo  = v.value("imo", "");
                        vd.name = v.value("name", "Unknown");
                        vd.lat  = std::stof(v.value("last_pos_latitude", "0"));
                        vd.lng  = std::stof(v.value("last_pos_longitude", "0"));
                        vd.speed = v.value("last_pos_speed", "0");
                        vd.angle = v.value("last_pos_angle", "0");
                        vd.from_port = v.value("route_from_port_name", "");
                        vd.to_port   = v.value("route_to_port_name", "");
                        vessels_.push_back(std::move(vd));
                    }
                }
                intel_.active_vessels = (int)vessels_.size();
            } else {
                vessel_error_ = j.value("error", "Unknown error");
            }
        } catch (const std::exception& ex) {
            vessel_error_ = std::string("Parse error: ") + ex.what();
        }
        loading_vessels_ = false;
    });
}

void MaritimeScreen::search_vessel_by_imo() {
    if (is_async_busy() || std::strlen(search_imo_) == 0) return;
    searching_ = true;
    has_search_result_ = false;
    search_error_.clear();

    std::string imo(search_imo_);
    async_task_ = std::async(std::launch::async, [this, imo]() {
        nlohmann::json body;
        body["imo"] = imo;

        std::string token = auth::AuthManager::instance().session().session_token;

        auto& client = http::HttpClient::instance();
        auto response = client.post("https://api.fincept.in/marine/vessel/position",
                                     body.dump(),
                                     {{"Content-Type", "application/json"},
                                      {"X-Session-Token", token}});

        if (!response.success || response.body.empty()) {
            search_error_ = response.error.empty() ? "Vessel not found" : response.error;
            searching_ = false;
            return;
        }

        try {
            auto j = nlohmann::json::parse(response.body);
            if (j.value("success", false) && j.contains("data")) {
                auto& v = j["data"].contains("vessel") ? j["data"]["vessel"] : j["data"];
                search_result_.imo  = v.value("imo", imo);
                search_result_.name = v.value("name", "Unknown");
                search_result_.lat  = std::stof(v.value("last_pos_latitude", "0"));
                search_result_.lng  = std::stof(v.value("last_pos_longitude", "0"));
                search_result_.speed = v.value("last_pos_speed", "0");
                search_result_.angle = v.value("last_pos_angle", "0");
                search_result_.from_port = v.value("route_from_port_name", "");
                search_result_.to_port   = v.value("route_to_port_name", "");
                has_search_result_ = true;
            } else {
                search_error_ = j.value("error", "Vessel not found");
            }
        } catch (const std::exception& ex) {
            search_error_ = std::string("Parse error: ") + ex.what();
        }
        searching_ = false;
    });
}

// ============================================================================
// Map projection: Mercator lat/lng -> screen coords
// ============================================================================

ImVec2 MaritimeScreen::geo_to_screen(float lat, float lng, ImVec2 center, float w, float h) const {
    // Simple equirectangular projection centered on map
    float x = center.x + (lng / 180.0f) * (w * 0.5f) * map_zoom_ + map_offset_.x;
    float y = center.y - (lat / 90.0f) * (h * 0.5f) * map_zoom_ + map_offset_.y;
    return {x, y};
}

// ============================================================================
// Main render
// ============================================================================

void MaritimeScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }
    check_async();

    ui::ScreenFrame frame("##maritime_screen", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);

    float content_h = h - 56.0f; // header + status
    float left_w  = 220.0f;
    float right_w = 240.0f;
    float map_w   = w - left_w - right_w - 4.0f;

    ImGui::BeginGroup();
    {
        ImGui::BeginChild("##mt_left", ImVec2(left_w, content_h), false);
        render_left_panel(left_w, content_h);
        ImGui::EndChild();

        ImGui::SameLine(0, 2);

        ImGui::BeginChild("##mt_map", ImVec2(map_w, content_h), false);
        render_map_canvas(map_w, content_h);
        ImGui::EndChild();

        ImGui::SameLine(0, 2);

        ImGui::BeginChild("##mt_right", ImVec2(right_w, content_h), false);
        render_right_panel(right_w, content_h);
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    render_status_bar(w);
    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void MaritimeScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mt_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    ImGui::SmallButton("MARITIME INTELLIGENCE");
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Threat:");
    ImGui::SameLine();
    ImGui::TextColored(threat_color(intel_.threat_level), "%s",
        intel_.threat_level.c_str());

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Vessels:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "%d", intel_.active_vessels);

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Routes:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "%d", intel_.monitored_routes);

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Volume:");
    ImGui::SameLine();
    ImGui::TextColored(SUCCESS, "%s", intel_.trade_volume.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Left panel — intelligence, routes, layers
// ============================================================================

void MaritimeScreen::render_left_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    render_intel_stats(w);
    ImGui::Separator();
    render_layer_toggles(w);
    ImGui::Separator();
    render_route_list(w, h - 160.0f);

    ImGui::PopStyleColor();
}

void MaritimeScreen::render_intel_stats(float w) {
    ImGui::TextColored(TEXT_DIM, "INTELLIGENCE SUMMARY");
    ImGui::Spacing();

    // Stats grid
    auto stat = [](const char* label, const char* val, ImVec4 color) {
        ImGui::TextColored(ImVec4(0.47f, 0.47f, 0.53f, 1), "%s", label);
        ImGui::SameLine(100);
        ImGui::TextColored(color, "%s", val);
    };

    stat("Threat Level", intel_.threat_level.c_str(), threat_color(intel_.threat_level));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", intel_.active_vessels);
    stat("Vessels", buf, ImVec4(0.0f, 0.9f, 1.0f, 1.0f));
    std::snprintf(buf, sizeof(buf), "%d", intel_.monitored_routes);
    stat("Routes", buf, ImVec4(0.0f, 0.9f, 1.0f, 1.0f));
    stat("Trade Vol", intel_.trade_volume.c_str(), SUCCESS);
}

void MaritimeScreen::render_layer_toggles(float w) {
    ImGui::TextColored(TEXT_DIM, "MAP LAYERS");
    ImGui::Checkbox("Ocean Routes", &show_ocean_routes_);
    ImGui::Checkbox("Air Routes",   &show_air_routes_);
    ImGui::Checkbox("Vessels",      &show_vessels_);
    ImGui::Checkbox("Ports",        &show_ports_);
    ImGui::Spacing();

    if (ImGui::SmallButton("Load Vessels") && !loading_vessels_) {
        load_area_vessels();
    }
    if (loading_vessels_) {
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "Loading...");
    }
    if (!vessel_error_.empty()) {
        ImGui::TextColored(ERROR_RED, "%s", vessel_error_.c_str());
    }
}

void MaritimeScreen::render_route_list(float w, float h) {
    ImGui::TextColored(TEXT_DIM, "TRADE ROUTES (%d)", (int)ocean_routes_.size());
    ImGui::BeginChild("##mt_routes", ImVec2(0, h), false);

    for (int i = 0; i < (int)ocean_routes_.size(); i++) {
        auto& r = ocean_routes_[i];
        bool sel = (i == selected_route_);

        ImGui::PushID(i);
        if (ImGui::Selectable(("##route_" + std::to_string(i)).c_str(), sel, 0, ImVec2(0, 36))) {
            selected_route_ = sel ? -1 : i;
        }

        ImVec2 p = ImGui::GetItemRectMin();
        // Status indicator
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(p.x, p.y), ImVec2(p.x + 3, p.y + 36),
            ImGui::ColorConvertFloat4ToU32(status_color(r.status)));

        ImGui::SetCursorScreenPos(ImVec2(p.x + 8, p.y + 2));
        ImGui::TextColored(TEXT_PRIMARY, "%s", r.name.c_str());
        ImGui::SetCursorScreenPos(ImVec2(p.x + 8, p.y + 18));
        ImGui::TextColored(SUCCESS, "%s", r.value.c_str());
        ImGui::SameLine(130);
        ImGui::TextColored(TEXT_DIM, "%d ships", r.vessels);

        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 40));
        ImGui::PopID();
    }

    ImGui::EndChild();
}

// ============================================================================
// Map canvas — 2D equirectangular with routes/vessels/ports
// ============================================================================

void MaritimeScreen::render_map_canvas(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.04f, 0.08f, 1.0f));
    ImGui::BeginChild("##mt_canvas", ImVec2(w, h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 center(canvas_pos.x + w * 0.5f, canvas_pos.y + h * 0.5f);

    // Handle panning & zooming
    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            map_offset_.x += delta.x;
            map_offset_.y += delta.y;
        }
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
            map_zoom_ = std::clamp(map_zoom_ + wheel * 0.15f, 0.3f, 5.0f);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Grid lines (latitude/longitude)
    ImU32 grid_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.12f, 0.16f, 0.5f));
    for (int lat = -60; lat <= 80; lat += 30) {
        ImVec2 l = geo_to_screen((float)lat, -180.0f, center, w, h);
        ImVec2 r = geo_to_screen((float)lat, 180.0f, center, w, h);
        dl->AddLine(l, r, grid_col, 0.5f);
    }
    for (int lng = -180; lng <= 180; lng += 30) {
        ImVec2 t = geo_to_screen(80.0f, (float)lng, center, w, h);
        ImVec2 b = geo_to_screen(-60.0f, (float)lng, center, w, h);
        dl->AddLine(t, b, grid_col, 0.5f);
    }

    // Draw ocean routes
    if (show_ocean_routes_) {
        for (int i = 0; i < (int)ocean_routes_.size(); i++) {
            auto& r = ocean_routes_[i];
            ImVec2 p1 = geo_to_screen(r.start.lat, r.start.lng, center, w, h);
            ImVec2 p2 = geo_to_screen(r.end.lat, r.end.lng, center, w, h);
            float alpha = (i == selected_route_) ? 1.0f : r.color.w;
            float thick = (i == selected_route_) ? 3.0f : 1.5f;
            ImU32 col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(r.color.x, r.color.y, r.color.z, alpha));
            dl->AddLine(p1, p2, col, thick * map_zoom_);

            // Route label at midpoint
            if (map_zoom_ >= 0.8f) {
                ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
                dl->AddText(mid, ImGui::ColorConvertFloat4ToU32(TEXT_DIM), r.value.c_str());
            }
        }
    }

    // Draw air routes
    if (show_air_routes_) {
        for (auto& ar : air_routes_) {
            ImVec2 p1 = geo_to_screen(ar.start.lat, ar.start.lng, center, w, h);
            ImVec2 p2 = geo_to_screen(ar.end.lat, ar.end.lng, center, w, h);
            ImU32 col = ImGui::ColorConvertFloat4ToU32(ar.color);
            // Dashed line for air routes (draw short segments)
            float dx = p2.x - p1.x, dy = p2.y - p1.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0f) continue;
            float dash = 6.0f;
            for (float t = 0; t < len; t += dash * 2) {
                float t1 = t / len;
                float t2 = std::min((t + dash) / len, 1.0f);
                ImVec2 a(p1.x + dx * t1, p1.y + dy * t1);
                ImVec2 b(p1.x + dx * t2, p1.y + dy * t2);
                dl->AddLine(a, b, col, 1.0f * map_zoom_);
            }
        }
    }

    // Draw port markers
    if (show_ports_) {
        for (auto& loc : presets_) {
            ImVec2 p = geo_to_screen(loc.lat, loc.lng, center, w, h);
            dl->AddCircleFilled(p, 5.0f * map_zoom_,
                ImGui::ColorConvertFloat4ToU32(ACCENT), 8);
            dl->AddCircle(p, 6.0f * map_zoom_,
                ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), 8);
            if (map_zoom_ >= 1.0f) {
                dl->AddText(ImVec2(p.x + 8, p.y - 6),
                    ImGui::ColorConvertFloat4ToU32(TEXT_SECONDARY), loc.name.c_str());
            }
        }
    }

    // Draw vessels
    if (show_vessels_ && !vessels_.empty()) {
        ImU32 vessel_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 0.8f));
        for (auto& v : vessels_) {
            ImVec2 p = geo_to_screen(v.lat, v.lng, center, w, h);
            dl->AddTriangleFilled(
                ImVec2(p.x, p.y - 4 * map_zoom_),
                ImVec2(p.x - 3 * map_zoom_, p.y + 3 * map_zoom_),
                ImVec2(p.x + 3 * map_zoom_, p.y + 3 * map_zoom_),
                vessel_col);
        }
    }

    // Draw search result marker
    if (has_search_result_) {
        ImVec2 p = geo_to_screen(search_result_.lat, search_result_.lng, center, w, h);
        dl->AddCircleFilled(p, 8.0f * map_zoom_,
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.0f, 0.0f, 0.8f)), 12);
        dl->AddText(ImVec2(p.x + 10, p.y - 6),
            ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY),
            search_result_.name.c_str());
    }

    // Zoom controls overlay
    ImGui::SetCursorPos(ImVec2(w - 80, 8));
    if (ImGui::SmallButton("-##mz")) map_zoom_ = std::max(0.3f, map_zoom_ - 0.2f);
    ImGui::SameLine();
    ImGui::Text("%.0f%%", map_zoom_ * 100);
    ImGui::SameLine();
    if (ImGui::SmallButton("+##mz")) map_zoom_ = std::min(5.0f, map_zoom_ + 0.2f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Right panel — vessel search, presets, markers
// ============================================================================

void MaritimeScreen::render_right_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    render_vessel_search(w);
    ImGui::Separator();
    render_search_result(w);
    ImGui::Separator();
    render_preset_locations(w);

    ImGui::PopStyleColor();
}

void MaritimeScreen::render_vessel_search(float w) {
    ImGui::TextColored(TEXT_DIM, "VESSEL SEARCH");
    ImGui::Spacing();

    ImGui::Text("IMO Number:");
    ImGui::SetNextItemWidth(w - 16);
    bool enter = ImGui::InputText("##mt_imo", search_imo_, sizeof(search_imo_),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    if ((ImGui::SmallButton("Search") || enter) && !searching_ && !is_async_busy()) {
        search_vessel_by_imo();
    }
    if (searching_) {
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "Searching...");
    }
    if (!search_error_.empty()) {
        ImGui::TextColored(ERROR_RED, "%s", search_error_.c_str());
    }
}

void MaritimeScreen::render_search_result(float w) {
    if (!has_search_result_) return;

    ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "VESSEL FOUND");
    ImGui::Spacing();

    auto row = [](const char* label, const char* val) {
        ImGui::TextColored(ImVec4(0.47f, 0.47f, 0.53f, 1), "%s", label);
        ImGui::SameLine(80);
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1), "%s", val);
    };

    row("Name",  search_result_.name.c_str());
    row("IMO",   search_result_.imo.c_str());

    char pos_buf[64];
    std::snprintf(pos_buf, sizeof(pos_buf), "%.4f, %.4f", search_result_.lat, search_result_.lng);
    row("Position", pos_buf);
    row("Speed",  (search_result_.speed + " kn").c_str());
    row("From",   search_result_.from_port.empty() ? "N/A" : search_result_.from_port.c_str());
    row("To",     search_result_.to_port.empty() ? "N/A" : search_result_.to_port.c_str());

    ImGui::Spacing();
    if (ImGui::SmallButton("Zoom to Vessel")) {
        // Center map on vessel
        map_offset_ = {0, 0};
        map_zoom_ = 3.0f;
    }
}

void MaritimeScreen::render_preset_locations(float w) {
    ImGui::TextColored(TEXT_DIM, "PRESET LOCATIONS");
    ImGui::Spacing();

    for (auto& loc : presets_) {
        if (ImGui::SmallButton(loc.name.c_str())) {
            // Center map on location
            map_offset_ = {0, 0};
            map_zoom_ = 2.5f;
        }
    }
}

// ============================================================================
// Status bar
// ============================================================================

void MaritimeScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.04f, 1.0f));
    ImGui::BeginChild("##mt_status", ImVec2(w, 24), false);
    ImGui::SetCursorPos(ImVec2(8, 4));

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "AIS Feed: ACTIVE");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Satellite Link: NOMINAL");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Data Latency: 2.3ms");

    ImGui::SameLine(w - 280);
    ImGui::TextColored(TEXT_DIM, "Drag: Pan | Scroll: Zoom");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "CLEARANCE: TOP SECRET // SCI");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::maritime
