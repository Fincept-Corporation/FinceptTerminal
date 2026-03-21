#pragma once
#include "SurfaceTypes.h"
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::surface {

// Left-side metrics panel — shows key stats for the active surface
class SurfaceMetricsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SurfaceMetricsPanel(QWidget* parent = nullptr);

    void update_metrics(ChartType type,
        // All data pointers — panel selects the right one
        const VolatilitySurfaceData& vol,
        const GreeksSurfaceData& delta, const GreeksSurfaceData& gamma,
        const GreeksSurfaceData& vega, const GreeksSurfaceData& theta,
        const SkewSurfaceData& skew, const LocalVolData& local_vol,
        const YieldCurveData& yield, const SwaptionVolData& swaption,
        const CapFloorVolData& capfloor, const BondSpreadData& bond_spread,
        const OISBasisData& ois, const RealYieldData& real_yield,
        const ForwardRateData& fwd_rate,
        const FXVolData& fx_vol, const FXForwardPointsData& fx_fwd,
        const CrossCurrencyBasisData& xccy,
        const CDSSpreadData& cds, const CreditTransitionData& credit_trans,
        const RecoveryRateData& recovery,
        const CommodityForwardData& cmdty_fwd, const CommodityVolData& cmdty_vol,
        const CrackSpreadData& crack, const ContangoData& contango,
        const CorrelationMatrixData& corr, const PCAData& pca,
        const VaRData& var_data, const StressTestData& stress,
        const FactorExposureData& factor, const LiquidityData& liquidity,
        const DrawdownData& drawdown, const BetaData& beta,
        const ImpliedDividendData& impl_div,
        const InflationExpData& inflation, const MonetaryPolicyData& monetary);

private:
    QVBoxLayout* layout_;
    void clear_rows();
    void add_row(const QString& label, const QString& value,
                 const QColor& value_color = QColor(201,209,217));
    void add_section(const QString& title, const QColor& color);
};

} // namespace fincept::surface
