// Tile Map — OpenGL slippy-map renderer implementation
// Uses CartoDB dark_all tiles, stb_image for PNG decode, OpenGL textures

#include "tile_map.h"
#include "core/logger.h"
#include <curl/curl.h>
#include <thread>
#include <cstring>
#include <algorithm>

// OpenGL
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

// GL constants not in Windows gl.h
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// stb_image for PNG decoding
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

namespace fincept::ui {

static constexpr double PI = 3.14159265358979323846;

// ============================================================================
// libcurl binary fetch
// ============================================================================
static size_t write_binary(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string fetch_tile_data(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string data;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_binary);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "FinceptTerminal/4.0.0");

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: image/png,*/*");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        LOG_WARN("TileMap", "Fetch failed: %s for %s", curl_easy_strerror(res), url.c_str());
        curl_easy_cleanup(curl);
        return {};
    }

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    if (code != 200) {
        LOG_WARN("TileMap", "HTTP %ld for %s", code, url.c_str());
        return {};
    }
    return data;
}

// ============================================================================
// Mercator projection helpers
// ============================================================================
ImVec2 TileMap::lat_lon_to_world(double lat, double lon, int zoom) {
    double n = std::pow(2.0, zoom);
    double x = (lon + 180.0) / 360.0 * n * TILE_SIZE;
    double lat_rad = lat * PI / 180.0;
    double y = (1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / PI) / 2.0 * n * TILE_SIZE;
    return ImVec2((float)x, (float)y);
}

void TileMap::world_to_lat_lon(ImVec2 world, int zoom, double& lat, double& lon) {
    double n = std::pow(2.0, zoom);
    lon = world.x / (n * TILE_SIZE) * 360.0 - 180.0;
    double lat_rad = std::atan(std::sinh(PI * (1.0 - 2.0 * world.y / (n * TILE_SIZE))));
    lat = lat_rad * 180.0 / PI;
}

ImVec2 TileMap::center_world_px() const {
    return lat_lon_to_world(center_lat_, center_lon_, zoom_);
}

ImVec2 TileMap::world_to_screen(ImVec2 world_px, ImVec2 map_pos, ImVec2 map_size) const {
    ImVec2 center = center_world_px();
    float sx = map_pos.x + map_size.x * 0.5f + (world_px.x - center.x);
    float sy = map_pos.y + map_size.y * 0.5f + (world_px.y - center.y);
    return ImVec2(sx, sy);
}

ImVec2 TileMap::screen_to_world(ImVec2 screen_px, ImVec2 map_pos, ImVec2 map_size) const {
    ImVec2 center = center_world_px();
    float wx = center.x + (screen_px.x - map_pos.x - map_size.x * 0.5f);
    float wy = center.y + (screen_px.y - map_pos.y - map_size.y * 0.5f);
    return ImVec2(wx, wy);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
TileMap::TileMap() = default;

TileMap::~TileMap() {
    std::lock_guard<std::mutex> lock(tile_mutex_);
    for (auto& [key, entry] : tile_cache_) {
        if (entry.texture_id) {
            glDeleteTextures(1, &entry.texture_id);
        }
    }
    tile_cache_.clear();
}

// ============================================================================
// View control
// ============================================================================
void TileMap::set_view(double lat, double lon, int zoom) {
    center_lat_ = lat;
    center_lon_ = lon;
    zoom_ = std::clamp(zoom, min_zoom_, max_zoom_);
    zoom_smooth_ = (float)zoom_;
}

void TileMap::set_zoom(int zoom) {
    zoom_ = std::clamp(zoom, min_zoom_, max_zoom_);
}

// Float-precision world projection for smooth zoom
ImVec2 TileMap::lat_lon_to_world_f(double lat, double lon, float zoom) {
    double n = std::pow(2.0, (double)zoom);
    double x = (lon + 180.0) / 360.0 * n * TILE_SIZE;
    double lat_rad = lat * PI / 180.0;
    double y = (1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / PI) / 2.0 * n * TILE_SIZE;
    return ImVec2((float)x, (float)y);
}

ImVec2 TileMap::center_world_px_smooth() const {
    return lat_lon_to_world_f(center_lat_, center_lon_, zoom_smooth_);
}

ImVec2 TileMap::world_to_screen_smooth(ImVec2 world_px, ImVec2 map_pos, ImVec2 map_size) const {
    ImVec2 center = center_world_px_smooth();
    float sx = map_pos.x + map_size.x * 0.5f + (world_px.x - center.x);
    float sy = map_pos.y + map_size.y * 0.5f + (world_px.y - center.y);
    return ImVec2(sx, sy);
}

void TileMap::update_smooth_zoom() {
    float target = (float)zoom_;
    float diff = target - zoom_smooth_;
    if (std::abs(diff) < 0.01f) {
        zoom_smooth_ = target;
    } else {
        // Smooth lerp — 12x per second feels responsive but smooth
        float speed = 12.0f * ImGui::GetIO().DeltaTime;
        zoom_smooth_ += diff * std::min(speed, 1.0f);
    }
}

// ============================================================================
// Tile loading
// ============================================================================
void TileMap::ensure_tile(const TileKey& key) {
    // NOTE: caller must NOT hold tile_mutex_
    std::lock_guard<std::mutex> lock(tile_mutex_);
    auto it = tile_cache_.find(key);
    if (it != tile_cache_.end()) {
        // Already loaded, loading, or failed — don't re-request
        return;
    }

    tile_cache_[key].state = TileState::Loading;
    load_tile_async(key);
}

void TileMap::load_tile_async(TileKey key) {
    pending_loads_++;
    std::thread([this, key]() {
        char url[256];
        std::snprintf(url, sizeof(url),
            "https://basemaps.cartocdn.com/dark_all/%d/%d/%d.png",
            key.z, key.x, key.y);

        std::string data = fetch_tile_data(url);

        if (data.empty()) {
            std::lock_guard<std::mutex> lock(tile_mutex_);
            tile_cache_[key].state = TileState::Failed;
            pending_loads_--;
            return;
        }

        // Decode PNG
        int w, h, channels;
        unsigned char* pixels = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(data.data()),
            static_cast<int>(data.size()),
            &w, &h, &channels, 4);  // Force RGBA

        if (!pixels) {
            LOG_WARN("TileMap", "PNG decode failed for tile %d/%d/%d", key.z, key.x, key.y);
            std::lock_guard<std::mutex> lock(tile_mutex_);
            tile_cache_[key].state = TileState::Failed;
            pending_loads_--;
            return;
        }

        // Queue for main-thread GL texture upload
        {
            std::lock_guard<std::mutex> lock(pending_uploads_mutex_);
            pending_uploads_.push_back({key, pixels, w, h});
        }

        pending_loads_--;
    }).detach();
}

void TileMap::upload_texture(TileKey key, const unsigned char* data, int w, int h) {
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Now update tile cache under lock
    std::lock_guard<std::mutex> lock(tile_mutex_);
    auto& entry = tile_cache_[key];
    entry.texture_id = tex;
    entry.width = w;
    entry.height = h;
    entry.state = TileState::Loaded;
}

void TileMap::evict_old_tiles() {
    std::lock_guard<std::mutex> lock(tile_mutex_);
    if ((int)tile_cache_.size() <= MAX_CACHED_TILES) return;

    std::vector<TileKey> to_remove;
    for (auto& [key, entry] : tile_cache_) {
        if (std::abs(key.z - zoom_) > 2) {
            to_remove.push_back(key);
        }
    }
    for (auto& key : to_remove) {
        auto& entry = tile_cache_[key];
        if (entry.texture_id) glDeleteTextures(1, &entry.texture_id);
        tile_cache_.erase(key);
    }
}

// ============================================================================
// Input handling (pan + zoom)
// ============================================================================
void TileMap::handle_input(ImVec2 pos, ImVec2 size) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;

    bool hovered = (mouse.x >= pos.x && mouse.x < pos.x + size.x &&
                    mouse.y >= pos.y && mouse.y < pos.y + size.y);

    if (!hovered) {
        dragging_ = false;
        return;
    }

    // Zoom with scroll wheel — accumulate fractional scroll, zoom on threshold
    if (io.MouseWheel != 0) {
        // Accumulate scroll (touchpads send small fractional values)
        zoom_scroll_accum_ += io.MouseWheel;

        // Only change zoom level when we've accumulated enough scroll
        float threshold = 1.0f;
        while (std::abs(zoom_scroll_accum_) >= threshold) {
            double old_lat, old_lon;
            ImVec2 world_at_mouse = screen_to_world(mouse, pos, size);
            world_to_lat_lon(world_at_mouse, zoom_, old_lat, old_lon);

            int step = (zoom_scroll_accum_ > 0) ? 1 : -1;
            int new_zoom = std::clamp(zoom_ + step, min_zoom_, max_zoom_);

            if (new_zoom != zoom_) {
                zoom_ = new_zoom;
                ImVec2 new_world = lat_lon_to_world(old_lat, old_lon, zoom_);
                ImVec2 mouse_offset(mouse.x - pos.x - size.x * 0.5f,
                                    mouse.y - pos.y - size.y * 0.5f);
                ImVec2 new_center_world(new_world.x - mouse_offset.x,
                                         new_world.y - mouse_offset.y);
                world_to_lat_lon(new_center_world, zoom_, center_lat_, center_lon_);
            }

            zoom_scroll_accum_ -= step * threshold;
        }

        // Decay accumulator if user stopped scrolling
    } else {
        zoom_scroll_accum_ *= 0.8f;
        if (std::abs(zoom_scroll_accum_) < 0.05f) zoom_scroll_accum_ = 0;
    }

    // Pan with left mouse drag
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered) {
        dragging_ = true;
        drag_start_ = mouse;
        drag_start_lat_ = center_lat_;
        drag_start_lon_ = center_lon_;
    }

    if (dragging_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float dx = mouse.x - drag_start_.x;
        float dy = mouse.y - drag_start_.y;

        ImVec2 start_world = lat_lon_to_world(drag_start_lat_, drag_start_lon_, zoom_);
        ImVec2 new_center_world(start_world.x - dx, start_world.y - dy);
        world_to_lat_lon(new_center_world, zoom_, center_lat_, center_lon_);

        center_lat_ = std::clamp(center_lat_, -85.0, 85.0);
        while (center_lon_ > 180.0) center_lon_ -= 360.0;
        while (center_lon_ < -180.0) center_lon_ += 360.0;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        dragging_ = false;
    }
}

// ============================================================================
// Tile rendering
// ============================================================================
void TileMap::render_tiles(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    // Process pending texture uploads on GL thread
    {
        std::lock_guard<std::mutex> lock(pending_uploads_mutex_);
        for (auto& upload : pending_uploads_) {
            upload_texture(upload.key, upload.pixels, upload.w, upload.h);
            stbi_image_free(const_cast<unsigned char*>(upload.pixels));
        }
        pending_uploads_.clear();
    }

    // Scale factor between smooth zoom and integer zoom for tile sizing
    float zoom_scale = std::pow(2.0f, zoom_smooth_ - (float)zoom_);
    float scaled_tile = TILE_SIZE * zoom_scale;

    ImVec2 center = center_world_px();
    // Scale center offset by smooth zoom ratio
    float center_x_smooth = center.x * zoom_scale;
    float center_y_smooth = center.y * zoom_scale;
    float half_w = size.x * 0.5f;
    float half_h = size.y * 0.5f;

    // Determine visible tile range (in integer-zoom tile coords)
    float world_left   = center.x - half_w / zoom_scale;
    float world_top    = center.y - half_h / zoom_scale;
    float world_right  = center.x + half_w / zoom_scale;
    float world_bottom = center.y + half_h / zoom_scale;

    int tile_x_min = (int)std::floor(world_left / TILE_SIZE);
    int tile_x_max = (int)std::floor(world_right / TILE_SIZE);
    int tile_y_min = std::max(0, (int)std::floor(world_top / TILE_SIZE));
    int tile_y_max = std::min((int)std::pow(2, zoom_) - 1, (int)std::floor(world_bottom / TILE_SIZE));

    int n = (int)std::pow(2, zoom_);

    dl->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);

    // Dark background
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(17, 17, 18, 255));

    // First pass: collect tiles to ensure
    struct TileRenderInfo {
        TileKey key;
        ImVec2 tile_min, tile_max;
    };
    std::vector<TileRenderInfo> visible_tiles;

    for (int ty = tile_y_min; ty <= tile_y_max; ty++) {
        for (int tx = tile_x_min; tx <= tile_x_max; tx++) {
            int wrapped_x = ((tx % n) + n) % n;
            TileKey key{wrapped_x, ty, zoom_};

            // Position tiles with smooth zoom scaling
            float screen_x = pos.x + half_w + ((float)(tx * TILE_SIZE) - center.x) * zoom_scale;
            float screen_y = pos.y + half_h + ((float)(ty * TILE_SIZE) - center.y) * zoom_scale;

            visible_tiles.push_back({key,
                ImVec2(screen_x, screen_y),
                ImVec2(screen_x + scaled_tile, screen_y + scaled_tile)});
        }
    }

    for (auto& info : visible_tiles) {
        ensure_tile(info.key);
    }

    // Second pass: render
    {
        std::lock_guard<std::mutex> lock(tile_mutex_);
        for (auto& info : visible_tiles) {
            auto it = tile_cache_.find(info.key);
            if (it != tile_cache_.end() && it->second.state == TileState::Loaded && it->second.texture_id) {
                dl->AddImage(
                    (ImTextureID)(intptr_t)it->second.texture_id,
                    info.tile_min, info.tile_max);
            } else {
                dl->AddRectFilled(info.tile_min, info.tile_max, IM_COL32(24, 24, 26, 255));
                dl->AddRect(info.tile_min, info.tile_max, IM_COL32(40, 40, 44, 100));
            }
        }
    }

    dl->PopClipRect();
}

// ============================================================================
// Point rendering
// ============================================================================
int TileMap::render_points(ImDrawList* dl, ImVec2 pos, ImVec2 size, const std::vector<MapPoint>& points) {
    ImGuiIO& io = ImGui::GetIO();
    int clicked_id = -1;
    int hovered_id = -1;
    ImVec2 hovered_screen{};
    std::string hovered_label;

    dl->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);

    for (auto& pt : points) {
        ImVec2 world_px = lat_lon_to_world_f(pt.lat, pt.lon, zoom_smooth_);
        ImVec2 screen = world_to_screen_smooth(world_px, pos, size);

        // Skip if off-screen
        if (screen.x < pos.x - pt.radius || screen.x > pos.x + size.x + pt.radius ||
            screen.y < pos.y - pt.radius || screen.y > pos.y + size.y + pt.radius) {
            continue;
        }

        // Draw glow + dot
        ImU32 glow_col = (pt.color & 0x00FFFFFF) | 0x40000000; // 25% alpha
        dl->AddCircleFilled(screen, pt.radius + 3.0f, glow_col);
        dl->AddCircleFilled(screen, pt.radius, pt.color);

        // Check hover/click
        float dx = io.MousePos.x - screen.x;
        float dy = io.MousePos.y - screen.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= pt.radius + 4.0f) {
            hovered_id = pt.id;
            hovered_screen = screen;
            hovered_label = pt.label;

            dl->AddCircle(screen, pt.radius + 2.0f, IM_COL32(255, 255, 255, 180), 0, 1.5f);

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragging_) {
                clicked_id = pt.id;
            }
        }
    }

    dl->PopClipRect();

    // Tooltip
    if (hovered_id >= 0 && !hovered_label.empty()) {
        ImGui::SetNextWindowPos(ImVec2(hovered_screen.x + 12, hovered_screen.y - 8));
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(hovered_label.c_str());
        ImGui::EndTooltip();
    }

    return clicked_id;
}

// ============================================================================
// Main render
// ============================================================================
int TileMap::render(ImVec2 pos, ImVec2 size, const std::vector<MapPoint>& points) {
    handle_input(pos, size);
    update_smooth_zoom();
    evict_old_tiles();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    render_tiles(dl, pos, size);
    int clicked = render_points(dl, pos, size, points);

    // Zoom indicator
    char zoom_text[32];
    std::snprintf(zoom_text, sizeof(zoom_text), "Zoom: %d", zoom_);
    ImVec2 text_size = ImGui::CalcTextSize(zoom_text);
    ImVec2 badge_pos(pos.x + size.x - text_size.x - 16, pos.y + size.y - text_size.y - 10);
    dl->AddRectFilled(ImVec2(badge_pos.x - 4, badge_pos.y - 2),
                       ImVec2(badge_pos.x + text_size.x + 4, badge_pos.y + text_size.y + 2),
                       IM_COL32(0, 0, 0, 180), 3.0f);
    dl->AddText(badge_pos, IM_COL32(200, 200, 200, 230), zoom_text);

    return clicked;
}

} // namespace fincept::ui
