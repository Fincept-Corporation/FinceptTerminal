#pragma once
// Surface Analytics Screen — 3D financial surface visualizations
// Volatility surfaces, correlation matrices, yield curves, PCA

#include "surface_types.h"
#include <imgui.h>

namespace fincept::surface {

class SurfaceScreen {
public:
    void render();

private:
    ChartType active_chart_ = ChartType::Volatility;
    bool data_loaded_ = false;
    bool show_table_ = false; // false=3D plot (default), true=table view

    // Camera for 3D rotation
    float cam_yaw_ = 0.65f;    // horizontal rotation
    float cam_pitch_ = 0.45f;  // vertical rotation
    float cam_zoom_ = 1.15f;
    bool dragging_ = false;
    float drag_start_x_ = 0, drag_start_y_ = 0;
    float drag_start_yaw_ = 0, drag_start_pitch_ = 0;

    // Data
    VolatilitySurfaceData vol_data_;
    CorrelationMatrixData corr_data_;
    YieldCurveData yield_data_;
    PCAData pca_data_;

    // Config
    int selected_symbol_ = 0;
    std::vector<std::string> corr_assets_ = {
        "SPY", "QQQ", "IWM", "DIA", "GLD", "TLT", "IEF", "HYG"
    };

    void load_demo_data();
    void render_control_bar();
    void render_metrics_panel();
    void render_chart_area();

    // 3D surface renderers
    void render_3d_surface(const std::vector<std::vector<float>>& z,
                           const char* x_label, const char* y_label, const char* z_label,
                           float min_val, float max_val, bool diverging = false,
                           const std::vector<std::string>* col_labels = nullptr,
                           const std::vector<std::string>* row_labels = nullptr);

    // Table renderers
    void render_vol_table();
    void render_corr_table();
    void render_yield_table();
    void render_pca_table();

    // 3D projection helper
    struct Vec3 { float x, y, z; };
    ImVec2 project(Vec3 p, ImVec2 center, float scale) const;
    ImVec4 surface_color(float t, bool diverging = false) const;
};

} // namespace fincept::surface
