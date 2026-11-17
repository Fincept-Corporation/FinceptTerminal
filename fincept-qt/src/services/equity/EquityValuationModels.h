// src/services/equity/EquityValuationModels.h
//
// Pure, synchronous valuation and scoring models for the Equity Research Valuation tab.
// Inputs are the annual statements EquityResearchService already loaded, so nothing here
// touches the network and every figure can be traced to a statement line.
//
// Why local: the tab used to POST these to the QuantLib API with request bodies that did
// not match the endpoints' models (DCF expects an `fcff` array + `wacc`; Piotroski/Beneish
// expect nested statements), so DCF, Piotroski and Beneish always failed — and the API's
// Beneish endpoint drops cash flow, which biases TATA towards "manipulator".
#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QPair>
#include <QString>
#include <QVector>

#include <cmath>

namespace fincept::services::equity::valuation {

/// One fiscal year of statement lines (NaN = the filing does not report the line).
struct FiscalYear {
    QString period; ///< fiscal year end, "YYYY-MM-DD"
    double revenue = std::nan("");
    double gross_profit = std::nan("");
    double ebit = std::nan("");
    double net_income = std::nan("");
    double sga = std::nan("");
    double depreciation = std::nan("");
    double operating_cash_flow = std::nan("");
    double total_assets = std::nan("");
    double total_liabilities = std::nan("");
    double current_assets = std::nan("");
    double current_liabilities = std::nan("");
    double retained_earnings = std::nan("");
    double long_term_debt = std::nan("");
    double receivables = std::nan("");
    double net_ppe = std::nan("");
    double shares = std::nan(""); ///< ordinary shares outstanding at year end
};

/// Fiscal years newest first, the three statements matched by period-end DATE (not by
/// column index — yfinance does not guarantee the statements carry the same periods).
QVector<FiscalYear> fiscal_years(const FinancialsData& fd);

// ── DCF ──────────────────────────────────────────────────────────────────────────
struct DcfInputs {
    double fcf = 0.0;             ///< base-year free cash flow (levered), absolute
    double growth = 0.0;          ///< annual growth over the projection, fraction
    double terminal_growth = 0.0; ///< perpetual growth after the projection, fraction
    double discount = 0.0;        ///< discount rate (cost of equity for levered FCF), fraction
    int years = 10;
    double shares = 0.0; ///< shares outstanding, absolute
    /// Fade growth linearly from `growth` (year 1) to `terminal_growth` (year n) instead of
    /// holding it flat then dropping to the terminal rate — the standard two-stage
    /// convergence; flat high growth for a decade overstates value.
    bool fade = false;
    /// Unlevered: `fcf` is free cash flow to the FIRM and `discount` the WACC, so the
    /// present value is the enterprise value; equity = EV − net debt − minority interest.
    bool unlevered = false;
    double net_debt = 0.0; ///< total debt − cash (unlevered only; negative = net cash)
    double minority = 0.0; ///< minority interest / non-controlling stakes (unlevered only)
};

struct DcfResult {
    bool ok = false;
    QString error;
    double pv_fcf = 0.0;            ///< present value of the projected cash flows
    double pv_terminal = 0.0;       ///< present value of the Gordon terminal value
    double enterprise_value = 0.0;  ///< unlevered: pv_fcf + pv_terminal; levered: = equity_value
    double equity_value = 0.0;      ///< levered: pv_fcf + pv_terminal; unlevered: EV − net debt − minority
    double per_share = 0.0;         ///< equity_value / shares
    double terminal_fraction = 0.0; ///< pv_terminal / (pv_fcf + pv_terminal)
    QVector<double> growth;         ///< growth applied in each projection year
    QVector<double> cash_flow;      ///< projected cash flow of each year
    QVector<double> discount_factor;
    QVector<double> present_value;
    double terminal_value = 0.0;    ///< undiscounted, at the end of the projection
};

/// Two-stage DCF: CF_t = CF_{t-1}·(1+g_t) for t = 1..n (g_t = g, or faded towards g_T when
/// `fade`), terminal value CF_n·(1+g_T)/(r − g_T), all discounted at r. Levered (FCFE at
/// the cost of equity) gives equity value directly; unlevered (FCFF at the WACC) gives the
/// enterprise value, bridged to equity by net debt and minority interest.
DcfResult dcf(const DcfInputs& in);

// ── Dividend discount / residual income (banks, insurers) ───────────────────────
struct DdmInputs {
    double dps = 0.0;             ///< current annual dividend per share
    double growth = 0.0;          ///< stage-1 dividend growth, fraction
    double terminal_growth = 0.0;
    double cost_of_equity = 0.0;
    int years = 5;
};
struct DdmResult {
    bool ok = false;
    QString error;
    double per_share = 0.0;
    double terminal_fraction = 0.0;
};
/// Two-stage dividend discount model (stage-1 growth, then Gordon growth).
DdmResult ddm(const DdmInputs& in);

struct ResidualIncomeInputs {
    double book_value_ps = 0.0;  ///< current book value per share
    double roe = 0.0;            ///< expected return on equity (fraction), faded to `long_run_roe`
    double long_run_roe = 0.0;   ///< ROE the fade converges to (≈ cost of equity = no excess return)
    double payout = 0.0;         ///< dividend payout ratio (fraction of earnings)
    double cost_of_equity = 0.0;
    int years = 10;
};
struct ResidualIncomeResult {
    bool ok = false;
    QString error;
    double per_share = 0.0;      ///< book value + PV of residual income
    double pv_residual = 0.0;    ///< PV of (ROE − ke) × book value over the horizon
};
/// Residual income: value = BV0 + Σ (ROE_t − ke)·BV_{t−1} / (1+ke)^t, ROE fading linearly
/// to `long_run_roe`, book value growing by retained earnings. No terminal value beyond
/// the horizon (excess returns are competed away).
ResidualIncomeResult residual_income(const ResidualIncomeInputs& in);

/// Reverse DCF: the projection growth rate (fraction) at which the DCF's per-share value
/// equals `price` with every other input unchanged — what the market price assumes.
/// NaN when no growth in [-50%, +100%] reaches the price (or the inputs are invalid).
double implied_growth(const DcfInputs& in, double price);

// ── Altman Z (1968, public manufacturers) ────────────────────────────────────────
struct AltmanResult {
    bool ok = false;
    QString missing; ///< which input was absent when !ok
    double z = 0.0;
    double x1 = 0.0, x2 = 0.0, x3 = 0.0, x4 = 0.0, x5 = 0.0;
    /// Z'' (Altman 1995, non-manufacturers / emerging markets): drops the sales-turnover
    /// term that penalises asset-light firms and uses BOOK equity, so it needs no price.
    bool z2_ok = false;
    double z2 = 0.0;
    double x4_book = 0.0; ///< book equity / total liabilities
};
/// Z = 1.2·WC/TA + 1.4·RE/TA + 3.3·EBIT/TA + 0.6·MVE/TL + 1.0·Sales/TA.
/// Z'' = 6.56·WC/TA + 3.26·RE/TA + 6.72·EBIT/TA + 1.05·BookEquity/TL.
/// `market_cap` must be in the statements' currency (Z only; Z'' is computed without it).
AltmanResult altman_z(const FiscalYear& fy, double market_cap);
/// Z'' zones: safe above 2.60, distress below 1.10.
constexpr double kAltmanZ2Safe = 2.60;
constexpr double kAltmanZ2Distress = 1.10;

// ── Piotroski F (2000) ──────────────────────────────────────────────────────────
struct PiotroskiResult {
    bool ok = false;
    QString missing;
    int score = 0;           ///< tests passed
    int tests_available = 0; ///< tests whose inputs were reported (max 9)
    QVector<QPair<QString, int>> tests; ///< test name → 1 pass, 0 fail, -1 not computable
};
PiotroskiResult piotroski_f(const FiscalYear& cur, const FiscalYear& prev);

// ── Beneish M (1999, 8 variables) ───────────────────────────────────────────────
struct BeneishResult {
    bool ok = false;
    QString missing;
    double m = 0.0;
    int imputed = 0; ///< indices set to the neutral 1.0 because an input was not reported
    QVector<QPair<QString, double>> components;
};
/// Beneish (1999) cut-off for the 8-variable model: M > −1.78 flags likely manipulation.
constexpr double kBeneishThreshold = -1.78;
BeneishResult beneish_m(const FiscalYear& cur, const FiscalYear& prev);

} // namespace fincept::services::equity::valuation
