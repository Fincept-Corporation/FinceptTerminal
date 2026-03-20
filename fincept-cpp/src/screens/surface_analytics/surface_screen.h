#pragma once
// Surface Analytics Screen — 35 financial surface visualizations
// Equity derivatives, fixed income, FX, credit, commodities, risk, macro

#include "surface_types.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstdio>
#include <string>

namespace fincept::surface {

class SurfaceScreen {
public:
    void render();

private:
    ChartType active_chart_ = ChartType::Volatility;
    int active_category_ = 0;
    bool data_loaded_ = false;
    bool show_table_ = false;

    // Camera for 3D rotation
    float cam_yaw_ = 0.65f;
    float cam_pitch_ = 0.45f;
    float cam_zoom_ = 1.15f;
    bool dragging_ = false;
    float drag_start_x_ = 0, drag_start_y_ = 0;
    float drag_start_yaw_ = 0, drag_start_pitch_ = 0;

    // Config
    int selected_symbol_ = 0;
    std::vector<std::string> corr_assets_ = {
        "SPY", "QQQ", "IWM", "DIA", "GLD", "TLT", "IEF", "HYG"
    };

    // ---- Data for all 35 surfaces ----

    // Equity Derivatives
    VolatilitySurfaceData vol_data_;
    GreeksSurfaceData delta_data_, gamma_data_, vega_data_, theta_data_;
    SkewSurfaceData skew_data_;
    LocalVolData local_vol_data_;

    // Fixed Income
    YieldCurveData yield_data_;
    SwaptionVolData swaption_data_;
    CapFloorVolData capfloor_data_;
    BondSpreadData bond_spread_data_;
    OISBasisData ois_data_;
    RealYieldData real_yield_data_;
    ForwardRateData fwd_rate_data_;

    // FX
    FXVolData fx_vol_data_;
    FXForwardPointsData fx_fwd_data_;
    CrossCurrencyBasisData xccy_data_;

    // Credit
    CDSSpreadData cds_data_;
    CreditTransitionData credit_trans_data_;
    RecoveryRateData recovery_data_;

    // Commodities
    CommodityForwardData cmdty_fwd_data_;
    CommodityVolData cmdty_vol_data_;
    CrackSpreadData crack_data_;
    ContangoData contango_data_;

    // Risk & Portfolio
    CorrelationMatrixData corr_data_;
    PCAData pca_data_;
    VaRData var_data_;
    StressTestData stress_data_;
    FactorExposureData factor_data_;
    LiquidityData liquidity_data_;
    DrawdownData drawdown_data_;
    BetaData beta_data_;
    ImpliedDividendData impl_div_data_;

    // Macro
    InflationExpData inflation_data_;
    MonetaryPolicyData monetary_data_;

    // ---- Core render methods ----
    void load_demo_data();
    void render_control_bar();
    void render_metrics_panel();
    void render_chart_area();

    // ---- 3D surface renderer (in surface_3d_renderer.cpp) ----
    void render_3d_surface(const std::vector<std::vector<float>>& z,
                           const char* x_label, const char* y_label, const char* z_label,
                           float min_val, float max_val, bool diverging = false,
                           const std::vector<std::string>* col_labels = nullptr,
                           const std::vector<std::string>* row_labels = nullptr);

    // ---- Original table renderers (in surface_tables.cpp) ----
    void render_vol_table();
    void render_corr_table();
    void render_yield_table();
    void render_pca_table();

    // ---- Category panel renderers (each in panels_*.cpp) ----
    void render_metrics_derivatives();
    void render_metrics_fixed_income();
    void render_metrics_fx();
    void render_metrics_credit();
    void render_metrics_commodities();
    void render_metrics_risk();
    void render_metrics_macro();

    void render_chart_derivatives();
    void render_chart_fixed_income();
    void render_chart_fx();
    void render_chart_credit();
    void render_chart_commodities();
    void render_chart_risk();
    void render_chart_macro();

    // ---- Extended table renderer (in tables_extended.cpp) ----
    void render_table_for(ChartType type);

    // ---- CSV Import state ----
    bool show_import_dialog_ = false;
    char import_path_buf_[512] = {};
    std::string import_error_;
    bool import_success_ = false;

    void render_csv_import_dialog();
    bool dispatch_csv_load(const std::string& path, std::string& err);

    // ---- 3D projection helpers ----
    struct Vec3 { float x, y, z; };
    ImVec2 project(Vec3 p, ImVec2 center, float scale) const;
    ImVec4 surface_color(float t, bool diverging = false) const;
};

} // namespace fincept::surface
