#pragma once
// SurfaceAnalyticsScreen — 35 financial surface visualizations
// Equity derivatives, fixed income, FX, credit, commodities, risk, macro

#include "SurfaceDemoData.h"
#include "SurfaceTypes.h"
#include "screens/IStatefulScreen.h"
#include "services/databento/DatabentoService.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::surface {

class Surface3DWidget;
class SurfaceTableWidget;
class SurfaceMetricsPanel;
class SurfaceDatabentoPanel;

class SurfaceAnalyticsScreen : public QWidget, public fincept::screens::IStatefulScreen {
    Q_OBJECT
  public:
    explicit SurfaceAnalyticsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "surface_analytics"; }
    int state_version() const override { return 1; }

  private slots:
    void on_category_clicked(int index);
    void on_surface_clicked(int cat, int surf_index);
    void on_view_3d();
    void on_view_table();
    void on_import_csv();
    void on_refresh();
    void on_symbol_changed(int index);
    // Databento data slots
    void on_vol_surface_received(const fincept::DatabentoVolSurfaceResult& r);
    void on_ohlcv_received(const fincept::DatabentoOhlcvResult& r);
    void on_futures_received(const fincept::DatabentoFuturesResult& r);
    void on_surface_received(const fincept::DatabentoSurfaceResult& r);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void setup_ui();
    QWidget* build_control_bar();
    QWidget* build_category_bar();
    QWidget* build_surface_bar();
    void refresh_surface_bar();
    void load_demo_data();
    void update_chart();
    void update_metrics();
    void dispatch_csv(const QString& path);

    // Control bar widgets (kept for dynamic rebuild)
    QWidget* category_bar_ = nullptr;
    QWidget* surface_bar_ = nullptr;
    QComboBox* symbol_combo_ = nullptr;
    QPushButton* btn_3d_ = nullptr;
    QPushButton* btn_table_ = nullptr;

    // Main panels
    SurfaceDatabentoPanel* databento_panel_ = nullptr;
    SurfaceMetricsPanel* metrics_panel_ = nullptr;
    Surface3DWidget* surface_3d_ = nullptr;
    SurfaceTableWidget* surface_table_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;

    // State
    int active_category_ = 0;
    ChartType active_chart_ = ChartType::Volatility;
    bool show_table_ = false;

    // Equity symbol list
    static constexpr const char* VOL_SYMBOLS[] = {"SPY", "QQQ", "IWM", "AAPL", "TSLA", "NVDA", "AMZN"};
    static constexpr float VOL_SPOTS[] = {450, 380, 200, 175, 250, 120, 180};
    static constexpr int N_SYMBOLS = 7;
    int selected_symbol_ = 0;

    // Correlation assets
    std::vector<std::string> corr_assets_ = {"SPY", "QQQ", "IWM", "DIA", "GLD", "TLT", "IEF", "HYG"};

    // ---- Data for all 35 surfaces ----
    VolatilitySurfaceData vol_data_;
    GreeksSurfaceData delta_data_, gamma_data_, vega_data_, theta_data_;
    SkewSurfaceData skew_data_;
    LocalVolData local_vol_data_;
    YieldCurveData yield_data_;
    SwaptionVolData swaption_data_;
    CapFloorVolData capfloor_data_;
    BondSpreadData bond_spread_data_;
    OISBasisData ois_data_;
    RealYieldData real_yield_data_;
    ForwardRateData fwd_rate_data_;
    FXVolData fx_vol_data_;
    FXForwardPointsData fx_fwd_data_;
    CrossCurrencyBasisData xccy_data_;
    CDSSpreadData cds_data_;
    CreditTransitionData credit_trans_data_;
    RecoveryRateData recovery_data_;
    CommodityForwardData cmdty_fwd_data_;
    CommodityVolData cmdty_vol_data_;
    CrackSpreadData crack_data_;
    ContangoData contango_data_;
    CorrelationMatrixData corr_data_;
    PCAData pca_data_;
    VaRData var_data_;
    StressTestData stress_data_;
    FactorExposureData factor_data_;
    LiquidityData liquidity_data_;
    DrawdownData drawdown_data_;
    BetaData beta_data_;
    ImpliedDividendData impl_div_data_;
    InflationExpData inflation_data_;
    MonetaryPolicyData monetary_data_;

    // Category accent colors (R,G,B)
    static constexpr int CAT_COLORS[][3] = {
        {51, 153, 255},  // EQUITY DERIV — blue
        {102, 217, 140}, // FIXED INCOME — green
        {255, 191, 51},  // FX           — gold
        {230, 89, 89},   // CREDIT       — red
        {191, 140, 255}, // COMMODITIES  — purple
        {255, 140, 38},  // RISK         — orange
        {89, 217, 242},  // MACRO        — cyan
    };
};

} // namespace fincept::surface
