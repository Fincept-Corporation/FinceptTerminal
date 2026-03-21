#include "SurfaceMetricsPanel.h"
#include <QFrame>
#include <QHBoxLayout>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

SurfaceMetricsPanel::SurfaceMetricsPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(200);
    setStyleSheet("background:#0d1117;");
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(8,8,8,4);
    layout_->setSpacing(2);
    layout_->addStretch();
}

void SurfaceMetricsPanel::clear_rows() {
    QLayoutItem* item;
    while ((item = layout_->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void SurfaceMetricsPanel::add_section(const QString& title, const QColor& color) {
    auto* lbl = new QLabel(title, this);
    lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:bold; "
        "border-bottom:1px solid %1; padding-bottom:2px; margin-top:6px;")
        .arg(color.name()));
    layout_->addWidget(lbl);
}

void SurfaceMetricsPanel::add_row(const QString& label, const QString& value, const QColor& value_color) {
    auto* row = new QWidget(this);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0,0,0,0);
    hl->setSpacing(4);

    auto* lbl = new QLabel(label, row);
    lbl->setStyleSheet("color:#8b949e; font-size:10px;");
    lbl->setWordWrap(false);

    auto* val = new QLabel(value, row);
    val->setStyleSheet(QString("color:%1; font-size:10px; font-weight:bold;").arg(value_color.name()));
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    hl->addWidget(lbl, 1);
    hl->addWidget(val, 1);
    layout_->addWidget(row);
}

void SurfaceMetricsPanel::update_metrics(ChartType type,
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
    const InflationExpData& inflation, const MonetaryPolicyData& monetary)
{
    clear_rows();

    auto fmt = [](float v, int dec=2) { return QString::number((double)v, 'f', dec); };
    auto green = QColor(63,185,80);
    auto red   = QColor(248,81,73);
    auto blue  = QColor(88,166,255);
    auto dim   = QColor(139,148,158);

    switch (type) {
    case ChartType::Volatility: {
        if (vol.z.empty()) break;
        add_section("VOL SURFACE", blue);
        add_row("Symbol", QString::fromStdString(vol.underlying), blue);
        add_row("Spot",   "$" + fmt(vol.spot_price), QColor(201,209,217));
        int me = (int)vol.z.size()/2, ms = (int)vol.z[me].size()/2;
        add_row("ATM IV",  fmt(vol.z[me][ms], 1) + "%", green);
        float mn=999, mx=-999;
        for (const auto& r:vol.z) for (float v:r){mn=std::min(mn,v);mx=std::max(mx,v);}
        add_row("IV Range", fmt(mn,1)+"-"+fmt(mx,1)+"%", dim);
        add_row("Strikes", QString::number(vol.strikes.size()), dim);
        add_row("Expiries", QString::number(vol.expirations.size()), dim);
        break;
    }
    case ChartType::DeltaSurface: case ChartType::GammaSurface:
    case ChartType::VegaSurface:  case ChartType::ThetaSurface: {
        const GreeksSurfaceData* gd = nullptr;
        if (type==ChartType::DeltaSurface) gd=&delta;
        else if (type==ChartType::GammaSurface) gd=&gamma;
        else if (type==ChartType::VegaSurface) gd=&vega;
        else gd=&theta;
        if (gd->z.empty()) break;
        add_section(QString::fromStdString(gd->greek_name)+" SURFACE", blue);
        add_row("Symbol", QString::fromStdString(gd->underlying), blue);
        add_row("Spot", "$"+fmt(gd->spot_price), QColor(201,209,217));
        int me=(int)gd->z.size()/2, ms=(int)gd->z[me].size()/2;
        add_row("ATM Value", fmt(gd->z[me][ms],4), green);
        add_row("Points", QString::number(gd->strikes.size()*gd->expirations.size()), dim);
        add_row("Strikes", fmt(gd->strikes.front(),0)+"-"+fmt(gd->strikes.back(),0), dim);
        break;
    }
    case ChartType::SkewSurface: {
        if (skew.z.empty()) break;
        add_section("SKEW", QColor(255,170,51));
        add_row("Symbol", QString::fromStdString(skew.underlying), blue);
        add_row("Type", "25D Risk Rev.", dim);
        if (skew.z.size() > 0 && skew.z[0].size() > 2)
            add_row("1W 25D Skew", fmt(skew.z[0][2],2)+"%",
                skew.z[0][2] > 0 ? green : red);
        add_row("Tenors", QString::number(skew.expirations.size()), dim);
        break;
    }
    case ChartType::LocalVolSurface: {
        if (local_vol.z.empty()) break;
        add_section("LOCAL VOL", blue);
        add_row("Symbol", QString::fromStdString(local_vol.underlying), blue);
        add_row("Spot", "$"+fmt(local_vol.spot_price), QColor(201,209,217));
        add_row("Model", "Dupire", dim);
        int me=(int)local_vol.z.size()/2, ms=(int)local_vol.z[me].size()/2;
        add_row("ATM LV", fmt(local_vol.z[me][ms],1)+"%", green);
        break;
    }
    case ChartType::YieldCurve: {
        if (yield.z.empty()) break;
        add_section("YIELD CURVE", QColor(63,185,80));
        const auto& last = yield.z.back();
        if (!last.empty()) {
            add_row("2s10s", fmt(last.back()-last[3],1)+"bp",
                last.back()>last[3] ? green : red);
            add_row("Front", fmt(last.front(),2)+"%", QColor(201,209,217));
            add_row("Long End", fmt(last.back(),2)+"%", QColor(201,209,217));
        }
        add_row("Maturities", QString::number(yield.maturities.size()), dim);
        break;
    }
    case ChartType::SwaptionVol: {
        if (swaption.z.empty()) break;
        add_section("SWAPTION VOL", QColor(63,185,80));
        int me=(int)swaption.z.size()/2, ms=(int)swaption.z[me].size()/2;
        add_row("Belly ATM", fmt(swaption.z[me][ms],1)+"bp", blue);
        float mn=999,mx=-999;
        for (const auto& r:swaption.z) for(float v:r){mn=std::min(mn,v);mx=std::max(mx,v);}
        add_row("Range", fmt(mn,0)+"-"+fmt(mx,0)+"bp", dim);
        break;
    }
    case ChartType::BondSpread: {
        if (bond_spread.z.empty()) break;
        add_section("BOND SPREADS", QColor(63,185,80));
        // IG (BBB) 5Y
        if (bond_spread.z.size() > 3) add_row("BBB 5Y", fmt(bond_spread.z[3][4],0)+"bp", QColor(201,209,217));
        // HY (BB) 5Y
        if (bond_spread.z.size() > 4) add_row("BB 5Y", fmt(bond_spread.z[4][4],0)+"bp", red);
        break;
    }
    case ChartType::FXVol: {
        if (fx_vol.z.empty()) break;
        add_section("FX VOL", QColor(255,194,20));
        add_row("Pair", QString::fromStdString(fx_vol.pair), blue);
        int atm_d = 2;
        if (fx_vol.z.size() > 2)
            add_row("1M ATM", fmt(fx_vol.z[2][atm_d],2)+"%", QColor(201,209,217));
        break;
    }
    case ChartType::CDSSpread: {
        if (cds.z.empty()) break;
        add_section("CDS SPREADS", red);
        if (!cds.entities.empty()) add_row(QString::fromStdString(cds.entities[0])+" 5Y",
            fmt(cds.z[0][4],0)+"bp", red);
        break;
    }
    case ChartType::CreditTransition: {
        if (credit_trans.z.empty()) break;
        add_section("TRANSITION", red);
        // AAA stay probability
        add_row("AAA→AAA", fmt(credit_trans.z[0][0],1)+"%", green);
        // CCC default
        if (credit_trans.z.size() > 6) add_row("CCC→D", fmt(credit_trans.z[6][7],1)+"%", red);
        break;
    }
    case ChartType::Correlation: {
        if (corr.z.empty()) break;
        add_section("CORRELATION", QColor(255,112,41));
        add_row("Assets", QString::number(corr.assets.size()), dim);
        add_row("Window", QString::number(corr.window)+"D", dim);
        break;
    }
    case ChartType::PCA: {
        if (pca.z.empty()) break;
        add_section("PCA", QColor(255,112,41));
        if (!pca.variance_explained.empty()) {
            add_row("PC1 Var%", fmt(pca.variance_explained[0]*100,1)+"%", blue);
            if (pca.variance_explained.size()>1)
                add_row("PC2 Var%", fmt(pca.variance_explained[1]*100,1)+"%", dim);
        }
        add_row("Factors", QString::number(pca.factors.size()), dim);
        break;
    }
    case ChartType::VaR: {
        if (var_data.z.empty()) break;
        add_section("VALUE AT RISK", red);
        // 95% 1D
        if (var_data.z.size()>1)
            add_row("95% 1D VaR", fmt(var_data.z[1][0],2)+"%", red);
        // 99% 10D
        if (var_data.z.size()>2)
            add_row("99% 10D VaR", fmt(var_data.z[2][2],2)+"%", red);
        break;
    }
    case ChartType::StressTestPnL: {
        if (stress.z.empty()) break;
        add_section("STRESS TEST", red);
        // Worst scenario
        float worst = 0;
        for (const auto& r : stress.z) for (float v : r) worst = std::min(worst, v);
        add_row("Worst P&L", fmt(worst,1)+"%", red);
        add_row("Scenarios", QString::number(stress.scenarios.size()), dim);
        break;
    }
    case ChartType::CommodityForward: {
        if (cmdty_fwd.z.empty()) break;
        add_section("CMDTY FORWARD", QColor(189,140,255));
        if (!cmdty_fwd.commodities.empty())
            add_row(QString::fromStdString(cmdty_fwd.commodities[0])+" Spot",
                "$"+fmt(cmdty_fwd.z[0][0],2), QColor(201,209,217));
        break;
    }
    case ChartType::InflationExpectations: {
        if (inflation.z.empty()) break;
        add_section("INFLATION EXP", QColor(102,217,242));
        const auto& last = inflation.z.back();
        if (!last.empty()) add_row("1Y BE", fmt(last[0],2)+"%", QColor(201,209,217));
        if (last.size()>3) add_row("5Y BE", fmt(last[3],2)+"%", QColor(201,209,217));
        if (last.size()>5) add_row("10Y BE", fmt(last[5],2)+"%", QColor(201,209,217));
        break;
    }
    case ChartType::MonetaryPolicyPath: {
        if (monetary.z.empty()) break;
        add_section("POLICY PATH", QColor(102,217,242));
        if (!monetary.central_banks.empty())
            add_row(QString::fromStdString(monetary.central_banks[0])+" Now",
                fmt(monetary.z[0][0],2)+"%", QColor(201,209,217));
        break;
    }
    default:
        add_section("METRICS", dim);
        add_row("Select a", "surface", dim);
        break;
    }

    layout_->addStretch();
}

} // namespace fincept::surface
