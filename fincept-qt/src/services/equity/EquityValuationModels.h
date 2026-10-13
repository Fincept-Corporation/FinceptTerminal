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
};

struct DcfResult {
    bool ok = false;
    QString error;
    double pv_fcf = 0.0;            ///< present value of the projected cash flows
    double pv_terminal = 0.0;       ///< present value of the Gordon terminal value
    double equity_value = 0.0;      ///< pv_fcf + pv_terminal
    double per_share = 0.0;         ///< equity_value / shares
    double terminal_fraction = 0.0; ///< pv_terminal / equity_value
};

/// Levered-FCF DCF: FCF_t = FCF_0·(1+g)^t for t = 1..n, terminal value
/// FCF_n·(1+g_T)/(r − g_T), all discounted at r. Equity value directly (levered FCF is
/// already after interest), so no net-debt bridge.
DcfResult dcf(const DcfInputs& in);

// ── Altman Z (1968, public manufacturers) ────────────────────────────────────────
struct AltmanResult {
    bool ok = false;
    QString missing; ///< which input was absent when !ok
    double z = 0.0;
    double x1 = 0.0, x2 = 0.0, x3 = 0.0, x4 = 0.0, x5 = 0.0;
};
/// Z = 1.2·WC/TA + 1.4·RE/TA + 3.3·EBIT/TA + 0.6·MVE/TL + 1.0·Sales/TA.
/// `market_cap` must be in the statements' currency.
AltmanResult altman_z(const FiscalYear& fy, double market_cap);

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
