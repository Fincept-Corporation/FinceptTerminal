#include "SurfaceMetricsPanel.h"

#include <algorithm>
#include <cmath>

namespace fincept::surface {

static const char* MONO = "'Consolas','Courier New',monospace";

SurfaceMetricsPanel::SurfaceMetricsPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(190);
    setStyleSheet("QWidget { background:#0a0a0a; border-right:1px solid #1a1a1a; }");
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
    layout_->addStretch();
}

void SurfaceMetricsPanel::clear_rows() {
    QLayoutItem* item;
    while ((item = layout_->takeAt(0))) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void SurfaceMetricsPanel::add_section(const QString& title, const char* color) {
    auto* lbl = new QLabel(title, this);
    lbl->setFixedHeight(26);
    lbl->setStyleSheet(QString("color:%1; font-size:11px; font-weight:bold; letter-spacing:0.5px;"
                               " background:#111111; padding:0 10px;"
                               " border-bottom:1px solid #1a1a1a;"
                               " font-family:%2;")
                           .arg(color)
                           .arg(MONO));
    layout_->addWidget(lbl);
}

void SurfaceMetricsPanel::add_row(const QString& label, const QString& value, const char* value_color) {
    auto* row = new QWidget(this);
    row->setFixedHeight(22);
    row->setStyleSheet("QWidget { background:transparent; border-bottom:1px solid #111111; }");
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(4);

    auto* lbl = new QLabel(label, row);
    lbl->setStyleSheet(QString("color:#808080; font-size:11px; background:transparent;"
                               " font-family:%1; border:none;")
                           .arg(MONO));

    auto* val = new QLabel(value, row);
    val->setStyleSheet(QString("color:%1; font-size:11px; font-weight:bold; background:transparent;"
                               " font-family:%2; border:none;")
                           .arg(value_color)
                           .arg(MONO));
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    hl->addWidget(lbl, 1);
    hl->addWidget(val, 1);
    layout_->addWidget(row);
}

// ── update_metrics ────────────────────────────────────────────────────────────
void SurfaceMetricsPanel::update_metrics(
    ChartType type, const VolatilitySurfaceData& vol, const GreeksSurfaceData& delta, const GreeksSurfaceData& gamma,
    const GreeksSurfaceData& vega, const GreeksSurfaceData& theta, const SkewSurfaceData& skew,
    const LocalVolData& local_vol, const YieldCurveData& yield, const SwaptionVolData& swaption,
    const CapFloorVolData& capfloor, const BondSpreadData& bond_spread, const OISBasisData& ois,
    const RealYieldData& real_yield, const ForwardRateData& fwd_rate, const FXVolData& fx_vol,
    const FXForwardPointsData& fx_fwd, const CrossCurrencyBasisData& xccy, const CDSSpreadData& cds,
    const CreditTransitionData& credit_trans, const RecoveryRateData& recovery, const CommodityForwardData& cmdty_fwd,
    const CommodityVolData& cmdty_vol, const CrackSpreadData& crack, const ContangoData& contango,
    const CorrelationMatrixData& corr, const PCAData& pca, const VaRData& var_data, const StressTestData& stress,
    const FactorExposureData& factor, const LiquidityData& liquidity, const DrawdownData& drawdown,
    const BetaData& beta, const ImpliedDividendData& impl_div, const InflationExpData& inflation,
    const MonetaryPolicyData& monetary) {
    clear_rows();

    auto fmt = [](float v, int dec = 2) { return QString::number((double)v, 'f', dec); };
    auto minmax_z = [](const std::vector<std::vector<float>>& z, float& mn, float& mx) {
        mn = 9999;
        mx = -9999;
        for (const auto& r : z)
            for (float v : r) {
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
    };

    const char* POS = "#16a34a";
    const char* NEG = "#dc2626";
    const char* PRI = "#e5e5e5";
    const char* SEC = "#808080";
    const char* AMB = "#d97706";
    const char* CYN = "#0891b2";

    switch (type) {
        // ── EQUITY DERIVATIVES ────────────────────────────────────────────────────
        case ChartType::Volatility: {
            if (vol.z.empty())
                break;
            add_section("■ VOL SURFACE", AMB);
            add_row("SYMBOL", QString::fromStdString(vol.underlying), AMB);
            add_row("SPOT", "$" + fmt(vol.spot_price, 2), PRI);
            int me = (int)vol.z.size() / 2, ms = (int)vol.z[me].size() / 2;
            add_row("ATM IV", fmt(vol.z[me][ms], 1) + "%", POS);
            float mn, mx;
            minmax_z(vol.z, mn, mx);
            add_row("IV RANGE", fmt(mn, 1) + "-" + fmt(mx, 1) + "%", SEC);
            add_row("STRIKES", QString::number(vol.strikes.size()), SEC);
            add_row("EXPIRIES", QString::number(vol.expirations.size()), SEC);
            break;
        }
        case ChartType::DeltaSurface:
        case ChartType::GammaSurface:
        case ChartType::VegaSurface:
        case ChartType::ThetaSurface: {
            const GreeksSurfaceData* gd = (type == ChartType::DeltaSurface)   ? &delta
                                          : (type == ChartType::GammaSurface) ? &gamma
                                          : (type == ChartType::VegaSurface)  ? &vega
                                                                              : &theta;
            if (gd->z.empty())
                break;
            add_section(QString("■ %1").arg(QString::fromStdString(gd->greek_name).toUpper()), AMB);
            add_row("SYMBOL", QString::fromStdString(gd->underlying), AMB);
            add_row("SPOT", "$" + fmt(gd->spot_price, 2), PRI);
            int me = (int)gd->z.size() / 2, ms = (int)gd->z[me].size() / 2;
            add_row("ATM", fmt(gd->z[me][ms], 4), POS);
            add_row("RANGE", fmt(gd->strikes.front(), 0) + "-" + fmt(gd->strikes.back(), 0), SEC);
            add_row("GRID", QString::number(gd->strikes.size()) + "x" + QString::number(gd->expirations.size()), SEC);
            break;
        }
        case ChartType::SkewSurface: {
            if (skew.z.empty())
                break;
            add_section("■ SKEW SURFACE", AMB);
            add_row("SYMBOL", QString::fromStdString(skew.underlying), AMB);
            add_row("TYPE", "25D RISK REV", SEC);
            if (!skew.z.empty() && skew.z[0].size() > 2)
                add_row("1W 25D", fmt(skew.z[0][2], 2) + "%", skew.z[0][2] > 0 ? POS : NEG);
            add_row("TENORS", QString::number(skew.expirations.size()), SEC);
            break;
        }
        case ChartType::LocalVolSurface: {
            if (local_vol.z.empty())
                break;
            add_section("■ LOCAL VOL", AMB);
            add_row("SYMBOL", QString::fromStdString(local_vol.underlying), AMB);
            add_row("SPOT", "$" + fmt(local_vol.spot_price, 2), PRI);
            add_row("MODEL", "DUPIRE", SEC);
            int me = (int)local_vol.z.size() / 2, ms = (int)local_vol.z[me].size() / 2;
            add_row("ATM LV", fmt(local_vol.z[me][ms], 1) + "%", POS);
            break;
        }
        case ChartType::ImpliedDividend: {
            if (impl_div.z.empty())
                break;
            add_section("■ IMPLIED DIV", AMB);
            add_row("SYMBOL", QString::fromStdString(impl_div.underlying), AMB);
            add_row("EXPIRIES", QString::number(impl_div.expirations.size()), SEC);
            if (!impl_div.z.empty() && !impl_div.z[0].empty())
                add_row("NEAR DIV", "$" + fmt(impl_div.z[0][0], 2), PRI);
            break;
        }

        // ── FIXED INCOME ──────────────────────────────────────────────────────────
        case ChartType::YieldCurve: {
            if (yield.z.empty())
                break;
            add_section("■ YIELD CURVE", "#16a34a");
            const auto& last = yield.z.back();
            if (!last.empty()) {
                add_row("2Y", fmt(last[1], 2) + "%", PRI);
                add_row("5Y", fmt(last[3], 2) + "%", PRI);
                add_row("10Y", fmt(last[5], 2) + "%", PRI);
                float slope = last.back() - last[3];
                add_row("2s10s", fmt(slope, 1) + "bp", slope > 0 ? POS : NEG);
            }
            add_row("MATURITIES", QString::number(yield.maturities.size()), SEC);
            break;
        }
        case ChartType::SwaptionVol: {
            if (swaption.z.empty())
                break;
            add_section("■ SWAPTION VOL", "#16a34a");
            int me = (int)swaption.z.size() / 2, ms = (int)swaption.z[me].size() / 2;
            add_row("BELLY ATM", fmt(swaption.z[me][ms], 1) + "bp", CYN);
            float mn, mx;
            minmax_z(swaption.z, mn, mx);
            add_row("RANGE", fmt(mn, 0) + "-" + fmt(mx, 0) + "bp", SEC);
            add_row("GRID",
                    QString::number(swaption.option_expiries.size()) + "x" +
                        QString::number(swaption.swap_tenors.size()),
                    SEC);
            break;
        }
        case ChartType::CapFloorVol: {
            if (capfloor.z.empty())
                break;
            add_section("■ CAP/FLOOR VOL", "#16a34a");
            float mn, mx;
            minmax_z(capfloor.z, mn, mx);
            add_row("RANGE", fmt(mn, 0) + "-" + fmt(mx, 0) + "bp", SEC);
            add_row("MATURITIES", QString::number(capfloor.maturities.size()), SEC);
            break;
        }
        case ChartType::BondSpread: {
            if (bond_spread.z.empty())
                break;
            add_section("■ BOND SPREADS", "#16a34a");
            if (bond_spread.z.size() > 3)
                add_row("BBB 5Y", fmt(bond_spread.z[3][4], 0) + "bp", PRI);
            if (bond_spread.z.size() > 4)
                add_row("BB 5Y", fmt(bond_spread.z[4][4], 0) + "bp", NEG);
            add_row("RATINGS", QString::number(bond_spread.ratings.size()), SEC);
            break;
        }
        case ChartType::OISBasis: {
            if (ois.z.empty())
                break;
            add_section("■ OIS BASIS", "#16a34a");
            float mn, mx;
            minmax_z(ois.z, mn, mx);
            add_row("RANGE", fmt(mn, 1) + "-" + fmt(mx, 1) + "bp", SEC);
            add_row("TENORS", QString::number(ois.tenors.size()), SEC);
            break;
        }
        case ChartType::RealYield: {
            if (real_yield.z.empty())
                break;
            add_section("■ REAL YIELD", "#16a34a");
            const auto& last = real_yield.z.back();
            if (!last.empty()) {
                add_row("5Y REAL", fmt(last[2], 2) + "%", last[2] > 0 ? POS : NEG);
                add_row("10Y REAL", fmt(last[4], 2) + "%", last[4] > 0 ? POS : NEG);
            }
            break;
        }
        case ChartType::ForwardRate: {
            if (fwd_rate.z.empty())
                break;
            add_section("■ FORWARD RATE", "#16a34a");
            float mn, mx;
            minmax_z(fwd_rate.z, mn, mx);
            add_row("MIN", fmt(mn, 2) + "%", PRI);
            add_row("MAX", fmt(mx, 2) + "%", PRI);
            add_row("TENORS", QString::number(fwd_rate.start_tenors.size()), SEC);
            break;
        }

        // ── FX ────────────────────────────────────────────────────────────────────
        case ChartType::FXVol: {
            if (fx_vol.z.empty())
                break;
            add_section("■ FX VOL", "#ca8a04");
            add_row("PAIR", QString::fromStdString(fx_vol.pair), AMB);
            if (fx_vol.z.size() > 2)
                add_row("1M ATM", fmt(fx_vol.z[2][2], 2) + "%", PRI);
            float mn, mx;
            minmax_z(fx_vol.z, mn, mx);
            add_row("RANGE", fmt(mn, 1) + "-" + fmt(mx, 1) + "%", SEC);
            break;
        }
        case ChartType::FXForwardPoints: {
            if (fx_fwd.z.empty())
                break;
            add_section("■ FX FWD PTS", "#ca8a04");
            add_row("PAIRS", QString::number(fx_fwd.z.size()), SEC);
            add_row("TENORS", QString::number(fx_fwd.tenors.size()), SEC);
            break;
        }
        case ChartType::CrossCurrencyBasis: {
            if (xccy.z.empty())
                break;
            add_section("■ XCCY BASIS", "#ca8a04");
            float mn, mx;
            minmax_z(xccy.z, mn, mx);
            add_row("RANGE", fmt(mn, 1) + "-" + fmt(mx, 1) + "bp", SEC);
            add_row("PAIRS", QString::number(xccy.z.size()), SEC);
            break;
        }

        // ── CREDIT ────────────────────────────────────────────────────────────────
        case ChartType::CDSSpread: {
            if (cds.z.empty())
                break;
            add_section("■ CDS SPREADS", "#dc2626");
            if (!cds.entities.empty())
                add_row(QString::fromStdString(cds.entities[0]) + " 5Y", fmt(cds.z[0][4], 0) + "bp", NEG);
            if (cds.z.size() > 1)
                add_row(QString::fromStdString(cds.entities[1]) + " 5Y", fmt(cds.z[1][4], 0) + "bp", NEG);
            add_row("ENTITIES", QString::number(cds.entities.size()), SEC);
            break;
        }
        case ChartType::CreditTransition: {
            if (credit_trans.z.empty())
                break;
            add_section("■ TRANSITION", "#dc2626");
            add_row("AAA→AAA", fmt(credit_trans.z[0][0], 1) + "%", POS);
            if (credit_trans.z.size() > 6)
                add_row("CCC→D", fmt(credit_trans.z[6][7], 1) + "%", NEG);
            add_row("RATINGS", QString::number(credit_trans.ratings.size()), SEC);
            break;
        }
        case ChartType::RecoveryRate: {
            if (recovery.z.empty())
                break;
            add_section("■ RECOVERY RATE", "#dc2626");
            float mn, mx;
            minmax_z(recovery.z, mn, mx);
            add_row("RANGE", fmt(mn, 1) + "-" + fmt(mx, 1) + "%", SEC);
            break;
        }

        // ── COMMODITIES ───────────────────────────────────────────────────────────
        case ChartType::CommodityForward: {
            if (cmdty_fwd.z.empty())
                break;
            add_section("■ CMDTY FORWARD", "#9b72ff");
            if (!cmdty_fwd.commodities.empty())
                add_row(QString::fromStdString(cmdty_fwd.commodities[0]).toUpper() + " SPOT",
                        "$" + fmt(cmdty_fwd.z[0][0], 2), PRI);
            add_row("COMMODITIES", QString::number(cmdty_fwd.commodities.size()), SEC);
            add_row("CONTRACTS", QString::number(cmdty_fwd.contract_months.size()), SEC);
            break;
        }
        case ChartType::CommodityVol: {
            if (cmdty_vol.z.empty())
                break;
            add_section("■ CMDTY VOL", "#9b72ff");
            float mn, mx;
            minmax_z(cmdty_vol.z, mn, mx);
            add_row("RANGE", fmt(mn, 1) + "-" + fmt(mx, 1) + "%", SEC);
            add_row("COMMODITIES", QString::number(cmdty_vol.z.size()), SEC);
            break;
        }
        case ChartType::CrackSpread: {
            if (crack.z.empty())
                break;
            add_section("■ CRACK SPREAD", "#9b72ff");
            float mn, mx;
            minmax_z(crack.z, mn, mx);
            add_row("RANGE", fmt(mn, 1) + "-" + fmt(mx, 1) + " $/bbl", SEC);
            add_row("PRODUCTS", QString::number(crack.z.size()), SEC);
            break;
        }
        case ChartType::ContangoBackwardation: {
            if (contango.z.empty())
                break;
            add_section("■ CONTANGO", "#9b72ff");
            float mn, mx;
            minmax_z(contango.z, mn, mx);
            add_row("RANGE", fmt(mn, 2) + "-" + fmt(mx, 2) + "%", SEC);
            add_row("COMMODITIES", QString::number(contango.z.size()), SEC);
            break;
        }

        // ── RISK ──────────────────────────────────────────────────────────────────
        case ChartType::Correlation: {
            if (corr.z.empty())
                break;
            add_section("■ CORRELATION", AMB);
            add_row("ASSETS", QString::number(corr.assets.size()), SEC);
            add_row("WINDOW", QString::number(corr.window) + "D", SEC);
            float mn, mx;
            minmax_z(corr.z, mn, mx);
            add_row("RANGE", fmt(mn, 2) + " / " + fmt(mx, 2), SEC);
            break;
        }
        case ChartType::PCA: {
            if (pca.z.empty())
                break;
            add_section("■ PCA", AMB);
            if (!pca.variance_explained.empty()) {
                add_row("PC1 VAR%", fmt(pca.variance_explained[0] * 100, 1) + "%", CYN);
                if (pca.variance_explained.size() > 1)
                    add_row("PC2 VAR%", fmt(pca.variance_explained[1] * 100, 1) + "%", SEC);
            }
            add_row("FACTORS", QString::number(pca.factors.size()), SEC);
            add_row("ASSETS", QString::number(pca.assets.size()), SEC);
            break;
        }
        case ChartType::VaR: {
            if (var_data.z.empty())
                break;
            add_section("■ VALUE AT RISK", NEG);
            if (var_data.z.size() > 1)
                add_row("95% 1D", fmt(var_data.z[1][0], 2) + "%", NEG);
            if (var_data.z.size() > 2)
                add_row("99% 10D", fmt(var_data.z[2][2], 2) + "%", NEG);
            add_row("CONF LEVELS", QString::number(var_data.z.size()), SEC);
            break;
        }
        case ChartType::StressTestPnL: {
            if (stress.z.empty())
                break;
            add_section("■ STRESS TEST", NEG);
            float worst = 0;
            for (const auto& r : stress.z)
                for (float v : r)
                    worst = std::min(worst, v);
            add_row("WORST P&L", fmt(worst, 1) + "%", NEG);
            add_row("SCENARIOS", QString::number(stress.scenarios.size()), SEC);
            add_row("PORTFOLIOS", QString::number(stress.portfolios.size()), SEC);
            break;
        }
        case ChartType::FactorExposure: {
            if (factor.z.empty())
                break;
            add_section("■ FACTOR EXPOSURE", AMB);
            add_row("FACTORS", QString::number(factor.factors.size()), SEC);
            add_row("ASSETS", QString::number(factor.assets.size()), SEC);
            break;
        }
        case ChartType::LiquidityHeatmap: {
            if (liquidity.z.empty())
                break;
            add_section("■ LIQUIDITY", AMB);
            float mn, mx;
            minmax_z(liquidity.z, mn, mx);
            add_row("MIN BID-ASK", fmt(mn, 1) + "bp", POS);
            add_row("MAX BID-ASK", fmt(mx, 1) + "bp", NEG);
            break;
        }
        case ChartType::Drawdown: {
            if (drawdown.z.empty())
                break;
            add_section("■ DRAWDOWN", NEG);
            float mn, mx;
            minmax_z(drawdown.z, mn, mx);
            add_row("MAX DD", fmt(mn, 1) + "%", NEG);
            add_row("ASSETS", QString::number(drawdown.assets.size()), SEC);
            add_row("WINDOWS", QString::number(drawdown.windows.size()), SEC);
            break;
        }
        case ChartType::BetaSurface: {
            if (beta.z.empty())
                break;
            add_section("■ BETA SURFACE", AMB);
            float mn, mx;
            minmax_z(beta.z, mn, mx);
            add_row("RANGE", fmt(mn, 2) + " / " + fmt(mx, 2), SEC);
            add_row("ASSETS", QString::number(beta.assets.size()), SEC);
            add_row("HORIZONS", QString::number(beta.horizons.size()), SEC);
            break;
        }

        // ── MACRO ─────────────────────────────────────────────────────────────────
        case ChartType::InflationExpectations: {
            if (inflation.z.empty())
                break;
            add_section("■ INFLATION EXP", CYN);
            const auto& last = inflation.z.back();
            if (!last.empty())
                add_row("1Y BE", fmt(last[0], 2) + "%", PRI);
            if (last.size() > 3)
                add_row("5Y BE", fmt(last[3], 2) + "%", PRI);
            if (last.size() > 5)
                add_row("10Y BE", fmt(last[5], 2) + "%", PRI);
            add_row("HORIZONS", QString::number(inflation.horizons.size()), SEC);
            break;
        }
        case ChartType::MonetaryPolicyPath: {
            if (monetary.z.empty())
                break;
            add_section("■ POLICY PATH", CYN);
            if (!monetary.central_banks.empty())
                add_row(QString::fromStdString(monetary.central_banks[0]).toUpper(), fmt(monetary.z[0][0], 2) + "%",
                        PRI);
            if (monetary.z.size() > 1)
                add_row(QString::fromStdString(monetary.central_banks[1]).toUpper(), fmt(monetary.z[1][0], 2) + "%",
                        PRI);
            add_row("BANKS", QString::number(monetary.central_banks.size()), SEC);
            add_row("MEETINGS", QString::number(monetary.meetings_ahead.size()), SEC);
            break;
        }

        default:
            add_section("■ METRICS", "#525252");
            add_row("SELECT", "A SURFACE", "#525252");
            break;
    }

    layout_->addStretch();
}

} // namespace fincept::surface
