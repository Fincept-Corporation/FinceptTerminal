#pragma once
// Tile Map — OpenGL slippy-map renderer for ImGui
// Fetches CartoDB dark tiles, renders via ImDrawList::AddImage
// Supports pan/zoom, lat/lon projection, event dot overlays

#include <imgui.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <cmath>

namespace fincept::ui {

// Tile key for cache lookup
struct TileKey {
    int x, y, z;
    bool operator==(const TileKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct TileKeyHash {
    size_t operator()(const TileKey& k) const {
        return std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 10) ^ (std::hash<int>()(k.z) << 20);
    }
};

// A point to draw on the map
struct MapPoint {
    double lat, lon;
    ImU32 color;
    float radius;       // in pixels
    int id;             // user-defined ID for click detection
    std::string label;  // tooltip text
};

// Tile loading state
enum class TileState { Empty, Loading, Loaded, Failed };

struct TileEntry {
    TileState state = TileState::Empty;
    unsigned int texture_id = 0;  // OpenGL texture
    int width = 0, height = 0;
};

class TileMap {
public:
    TileMap();
    ~TileMap();

    // Set map center and zoom
    void set_view(double lat, double lon, int zoom);
    void set_zoom(int zoom);
    int zoom() const { return zoom_; }
    double center_lat() const { return center_lat_; }
    double center_lon() const { return center_lon_; }

    // Render the map into a given ImGui region
    // Returns the ID of the clicked point (-1 if none)
    int render(ImVec2 pos, ImVec2 size, const std::vector<MapPoint>& points);

    // Coordinate conversions
    static ImVec2 lat_lon_to_world(double lat, double lon, int zoom);
    static void world_to_lat_lon(ImVec2 world, int zoom, double& lat, double& lon);

private:
    // View state
    double center_lat_ = 30.0;
    double center_lon_ = 31.0;
    int zoom_ = 3;
    float zoom_smooth_ = 3.0f;   // smooth animated zoom for rendering
    int min_zoom_ = 2;
    int max_zoom_ = 12;

    // Pan state
    bool dragging_ = false;
    ImVec2 drag_start_{};
    double drag_start_lat_ = 0;
    double drag_start_lon_ = 0;

    // Scroll accumulator for smooth zoom
    float zoom_scroll_accum_ = 0;

    // Tile cache
    std::mutex tile_mutex_;
    std::unordered_map<TileKey, TileEntry, TileKeyHash> tile_cache_;
    std::atomic<int> pending_loads_{0};

    // Pending texture uploads (background thread -> main GL thread)
    struct PendingUpload {
        TileKey key;
        const unsigned char* pixels;
        int w, h;
    };
    std::mutex pending_uploads_mutex_;
    std::vector<PendingUpload> pending_uploads_;

    // Load a tile (async)
    void ensure_tile(const TileKey& key);
    void load_tile_async(TileKey key);
    void upload_texture(TileKey key, const unsigned char* data, int w, int h);

    // Rendering helpers
    void render_tiles(ImDrawList* dl, ImVec2 pos, ImVec2 size);
    int render_points(ImDrawList* dl, ImVec2 pos, ImVec2 size, const std::vector<MapPoint>& points);
    void handle_input(ImVec2 pos, ImVec2 size);

    // Projection helpers (integer zoom — for tile grid)
    ImVec2 world_to_screen(ImVec2 world_px, ImVec2 map_pos, ImVec2 map_size) const;
    ImVec2 screen_to_world(ImVec2 screen_px, ImVec2 map_pos, ImVec2 map_size) const;
    ImVec2 center_world_px() const;

    // Smooth zoom projection (float zoom — for points/rendering)
    static ImVec2 lat_lon_to_world_f(double lat, double lon, float zoom);
    ImVec2 center_world_px_smooth() const;
    ImVec2 world_to_screen_smooth(ImVec2 world_px, ImVec2 map_pos, ImVec2 map_size) const;

    // Animate smooth zoom toward target
    void update_smooth_zoom();

    static constexpr int TILE_SIZE = 256;
    static constexpr int MAX_CACHED_TILES = 512;

    void evict_old_tiles();
};

} // namespace fincept::ui
