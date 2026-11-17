// src/screens/alt_investments/AltInvestmentsScreen_Forms.cpp
//
// What each analyzer asks for: the field table (with the methods each field feeds), the method
// table (computed methods and product guides), building the form for the selected analyzer +
// method, keeping every analyzer's inputs (persisted through save_state), and turning the form into
// the JSON cli.py reads. cli.py maps these display-oriented keys to its analyzers' own names.
//
// Every field shown for a method changes that method's result: checked by running every
// (analyzer, method) with each shown field perturbed (scratchpad altA12/perturb2.py parses the
// tables below, so keep one alt_* call per line).
//
// Part of the partial-class split of AltInvestmentsScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/alt_investments/AltInvestmentsScreen.h"
#include "ui/formatting/AmountInput.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/DataGridEditor.h"
#include "ui/widgets/ResultView.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

using namespace fincept::ui;

// Translation helper for the data-driven field labels below.
static QString alt_tr(const char* s) {
    return QCoreApplication::translate("AltInvestmentsScreen", s);
}

// ── Field builders (one per line in the tables: the check script parses them) ──

static QStringList alt_method_list(const char* methods) {
    return QString::fromLatin1(methods).split(QLatin1Char(','), Qt::SkipEmptyParts);
}

static AltField alt_text(const char* key, const QString& label, const char* def, const char* methods = "") {
    AltField f;
    f.key = QString::fromLatin1(key);
    f.label = label;
    f.type = AltField::Text;
    f.combo_items = QString::fromUtf8(def);
    f.methods = alt_method_list(methods);
    return f;
}

static AltField alt_num(const char* key, const QString& label, double def, double min_v, double max_v, int dec,
                        const char* pfx, const char* sfx, bool div100, const char* methods = "") {
    AltField f;
    f.key = QString::fromLatin1(key);
    f.label = label;
    f.type = AltField::Spin;
    f.default_val = def;
    f.min_val = min_v;
    f.max_val = max_v;
    f.decimals = dec;
    f.prefix = QString::fromLatin1(pfx);
    f.suffix = QString::fromLatin1(sfx);
    f.divide_100 = div100;
    f.methods = alt_method_list(methods);
    return f;
}

static AltField alt_combo(const char* key, const QString& label, const char* items, const char* methods = "") {
    AltField f;
    f.key = QString::fromLatin1(key);
    f.label = label;
    f.type = AltField::Combo;
    f.combo_items = QString::fromLatin1(items);
    f.methods = alt_method_list(methods);
    return f;
}

// Dated cash flows (date | call / distribution / nav | amount | note). `rows`: ';'-separated rows.
static AltField alt_flows(const char* key, const QString& label, const char* rows, const char* methods = "") {
    AltField f;
    f.key = QString::fromLatin1(key);
    f.label = label;
    f.type = AltField::CashFlows;
    f.combo_items = QString::fromLatin1(rows);
    f.methods = alt_method_list(methods);
    return f;
}

// Returns (optional date | value), entered as percent or decimals.
static AltField alt_series(const char* key, const QString& label, const char* rows, const char* methods = "") {
    AltField f;
    f.key = QString::fromLatin1(key);
    f.label = label;
    f.type = AltField::Series;
    f.combo_items = QString::fromLatin1(rows);
    f.methods = alt_method_list(methods);
    return f;
}

// ── Per-analyzer form fields ─────────────────────────────────────────────────
// The last argument lists the methods a field feeds (none = every method). A method shows only its
// fields; values carry over between methods because they are stored per analyzer.

static QList<AltField> alt_fields_for(const QString& id) {
    if (id == "high-yield")
        return {
            alt_text("name", alt_tr("BOND NAME"), "HY Corp Bond"),
            alt_num("par_value", alt_tr("PAR VALUE ($)"), 1000, 1, 1e9, 0, "$", "", false, "credit_analysis"),
            alt_num("price", alt_tr("MARKET PRICE ($)"), 950, 1, 1e9, 2, "$", "", false, "credit_analysis"),
            alt_num("coupon_rate", alt_tr("COUPON RATE (%)"), 8.5, 0, 30, 2, "", "%", true, "credit_analysis"),
            alt_num("maturity_years", alt_tr("MATURITY (years)"), 5, 0.5, 30, 1, "", "", false, "credit_analysis"),
            alt_num("treasury_yield", alt_tr("TREASURY YIELD, SAME TENOR (%)"), 4.0, 0, 20, 2, "", "%", true, "credit_analysis"),
            alt_combo("credit_rating", alt_tr("CREDIT RATING"), "BB|BB+|BB-|B|B+|B-|CCC|CCC+|BBB"),
        };
    if (id == "em-bonds")
        return {
            alt_text("name", alt_tr("BOND NAME"), "Brazil 2030"),
            alt_num("face_value", alt_tr("FACE VALUE ($)"), 1000, 1, 1e9, 0, "$", "", false, "yield_spread"),
            alt_num("current_market_value", alt_tr("MARKET PRICE ($)"), 950, 1, 1e9, 2, "$", "", false, "yield_spread"),
            alt_num("coupon_rate", alt_tr("COUPON RATE (%)"), 6.0, 0, 25, 2, "", "%", true, "yield_spread"),
            alt_num("maturity_years", alt_tr("MATURITY (years)"), 10, 0.5, 30, 1, "", "", false, "yield_spread,default_risk"),
            alt_num("treasury_yield", alt_tr("TREASURY YIELD, SAME TENOR (%)"), 4.0, 0, 20, 2, "", "%", true, "yield_spread"),
            alt_num("credit_spread", alt_tr("CREDIT SPREAD (%)"), 3.0, 0, 20, 2, "", "%", true, "default_risk"),
            alt_num("recovery_rate", alt_tr("RECOVERY IN DEFAULT (%)"), 30, 0, 100, 1, "", "%", true, "default_risk"),
            alt_combo("sovereign_rating", alt_tr("SOVEREIGN RATING"), "BB|BBB|B|CCC|BB+|BB-|B+|B-", "default_risk"),
            alt_num("local_currency_volatility", alt_tr("LOCAL CURRENCY VOLATILITY VS USD (%)"), 15, 0, 100, 1, "", "%", true, "currency_risk"),
            alt_num("fx_correlation", alt_tr("FX / BOND RETURN CORRELATION"), -0.30, -0.95, 0.95, 2, "", "", false, "currency_risk"),
        };
    if (id == "convertible-bonds")
        return {
            alt_text("name", alt_tr("BOND NAME"), "Tesla Conv 2028"),
            alt_num("par_value", alt_tr("PAR VALUE ($)"), 1000, 1, 1e9, 0, "$", "", false, "bond_floor,upside_participation"),
            alt_num("current_price", alt_tr("MARKET PRICE ($)"), 1100, 1, 1e9, 2, "$", "", false),
            alt_num("stock_price", alt_tr("STOCK PRICE ($)"), 55, 0.01, 1e6, 2, "$", "", false),
            alt_num("conversion_ratio", alt_tr("CONVERSION RATIO (shares per bond)"), 18, 0.01, 1e5, 2, "", "", false),
            alt_num("coupon_rate", alt_tr("COUPON RATE (%)"), 2.0, 0, 15, 2, "", "%", true, "bond_floor,upside_participation"),
            alt_num("maturity_years", alt_tr("MATURITY (years)"), 5, 0.5, 30, 1, "", "", false, "bond_floor,upside_participation"),
            alt_num("treasury_yield", alt_tr("TREASURY YIELD, SAME TENOR (%)"), 4.0, 0, 20, 2, "", "%", true, "bond_floor,upside_participation"),
            alt_num("credit_spread", alt_tr("ISSUER CREDIT SPREAD (%)"), 2.0, 0, 30, 2, "", "%", true, "bond_floor,upside_participation"),
        };
    if (id == "preferred-stocks")
        return {
            alt_text("name", alt_tr("SECURITY NAME"), "JPM Series J Preferred"),
            alt_num("par_value", alt_tr("PAR VALUE ($)"), 25, 1, 1e4, 2, "$", "", false, "yield_analysis,call_risk"),
            alt_num("current_price", alt_tr("MARKET PRICE ($)"), 24.5, 0.01, 1e4, 2, "$", "", false, "yield_analysis,call_risk"),
            alt_num("dividend_rate", alt_tr("DIVIDEND RATE (% of par)"), 5.5, 0, 20, 2, "", "%", true, "yield_analysis,call_risk"),
            alt_num("call_price", alt_tr("CALL PRICE ($)"), 25, 0.01, 1e4, 2, "$", "", false, "yield_analysis,call_risk"),
            alt_num("years_to_call", alt_tr("YEARS TO FIRST CALL (0 = not callable)"), 3, 0, 50, 1, "", "", false, "yield_analysis,call_risk"),
        };
    if (id == "real-estate")
        return {
            alt_text("name", alt_tr("PROPERTY NAME"), "Class A Office"),
            alt_num("acquisition_price", alt_tr("PURCHASE PRICE ($)"), 1000000, 1000, 1e12, 0, "$", "", false),
            alt_num("gross_income", alt_tr("GROSS RENT ($/yr)"), 120000, 0, 1e10, 0, "$", "", false),
            alt_num("vacancy_rate", alt_tr("VACANCY (%)"), 5.0, 0, 100, 1, "", "%", true),
            alt_num("operating_expenses", alt_tr("OPERATING EXPENSES ($/yr)"), 40000, 0, 1e10, 0, "$", "", false),
            alt_num("loan_amount", alt_tr("LOAN AMOUNT ($)"), 700000, 0, 1e12, 0, "$", "", false, "financing"),
            alt_num("interest_rate", alt_tr("MORTGAGE RATE (%)"), 6.5, 0, 30, 2, "", "%", true, "financing"),
            alt_num("loan_term", alt_tr("AMORTISATION (years)"), 30, 1, 50, 0, "", "", false, "financing"),
            alt_num("hold_period_years", alt_tr("HOLD PERIOD (years)"), 10, 1, 50, 0, "", "", false, "financing,dcf"),
            alt_num("rent_growth", alt_tr("NOI GROWTH (%/yr)"), 3.0, -20, 30, 2, "", "%", true, "financing,dcf"),
            alt_num("exit_cap_rate", alt_tr("EXIT CAP RATE (%)"), 6.0, 0.1, 30, 2, "", "%", true, "financing,dcf"),
            alt_num("discount_rate", alt_tr("DISCOUNT RATE (%)"), 8.0, -50, 50, 2, "", "%", true, "dcf"),
        };
    if (id == "hedge-funds")
        return {
            alt_text("name", alt_tr("FUND NAME"), "Global Macro Fund"),
            // The fund's own track record: nothing is computed without it.
            alt_series("monthly_returns", alt_tr("MONTHLY GROSS RETURNS (oldest first, 12 or more)"), "2025-01-31|1.2;2025-02-28|-0.4;2025-03-31|1.8;2025-04-30|0.6;2025-05-31|-1.1;2025-06-30|0.9;2025-07-31|1.4;2025-08-31|-0.2;2025-09-30|0.7;2025-10-31|1.0;2025-11-30|-0.6;2025-12-31|1.3"),
            alt_num("management_fee", alt_tr("MANAGEMENT FEE (%/yr)"), 2.0, 0, 10, 2, "", "%", true),
            alt_num("performance_fee", alt_tr("PERFORMANCE FEE (%)"), 20.0, 0, 50, 1, "", "%", true),
            alt_num("hurdle_rate", alt_tr("HURDLE RATE (%/yr)"), 0.0, 0, 20, 2, "", "%", true),
            alt_num("risk_free_rate", alt_tr("RISK-FREE RATE (%/yr)"), 4.0, 0, 20, 2, "", "%", true),
        };
    if (id == "managed-futures")
        return {
            alt_text("name", alt_tr("FUND NAME"), "CTA Trend Fund"),
            alt_num("gross_return", alt_tr("GROSS RETURN (%/yr)"), 12.0, -50, 100, 2, "", "%", true),
            alt_num("management_fee", alt_tr("MANAGEMENT FEE (%/yr)"), 2.0, 0, 10, 2, "", "%", true),
            alt_num("performance_fee", alt_tr("PERFORMANCE FEE (%)"), 20.0, 0, 50, 1, "", "%", true),
            alt_num("years", alt_tr("HORIZON (years)"), 10, 1, 50, 0, "", "", false),
        };
    if (id == "market-neutral")
        return {
            alt_text("name", alt_tr("FUND NAME"), "Market Neutral Fund"),
            alt_num("long_exposure", alt_tr("LONG EXPOSURE (x capital)"), 1.0, 0, 10, 2, "", "x", false),
            alt_num("short_exposure", alt_tr("SHORT EXPOSURE (x capital)"), 1.0, 0, 10, 2, "", "x", false),
        };
    if (id == "natural-resources")
        return {
            alt_text("name", alt_tr("COMMODITY NAME"), "WTI Crude Oil"),
            alt_num("spot_price", alt_tr("SPOT PRICE ($)"), 80, 0.01, 1e6, 2, "$", "", false),
            alt_num("three_month_futures", alt_tr("3-MONTH FUTURES ($)"), 82, 0.01, 1e6, 2, "$", "", false),
            alt_num("six_month_futures", alt_tr("6-MONTH FUTURES ($)"), 84, 0.01, 1e6, 2, "$", "", false, "contango"),
            alt_num("twelve_month_futures", alt_tr("12-MONTH FUTURES ($)"), 87, 0.01, 1e6, 2, "$", "", false, "contango"),
        };
    if (id == "private-capital")
        return {
            // The LP's own cash flows drive every metric; the latest NAV row is the residual value.
            alt_text("name", alt_tr("FUND NAME"), "Buyout Fund III"),
            alt_flows("cash_flows", alt_tr("CASH FLOWS AND NAV"), "2020-03-31|call|1000000|First close;2021-06-30|call|500000|;2023-12-31|distribution|600000|Partial exit;2026-06-30|nav|1400000|Q2 statement"),
            alt_num("commitment", alt_tr("COMMITMENT ($)"), 2000000, 0, 1e12, 0, "$", "", false),
        };
    if (id == "annuities")
        return {
            alt_text("name", alt_tr("ANNUITY NAME"), "Fixed Annuity"),
            alt_num("premium", alt_tr("PREMIUM ($)"), 100000, 1000, 1e9, 0, "$", "", false),
            alt_num("annual_payout_rate", alt_tr("ANNUAL PAYOUT RATE (%)"), 5.0, 0.1, 20, 2, "", "%", true),
            alt_num("payout_years", alt_tr("PAYOUT TERM (years)"), 20, 1, 50, 0, "", "", false),
            alt_num("inflation_rate", alt_tr("ASSUMED INFLATION (%/yr)"), 3.0, 0, 20, 2, "", "%", true, "inflation_erosion"),
            alt_num("alternative_return", alt_tr("SELF-MANAGED PORTFOLIO RETURN (%/yr)"), 4.0, -10, 20, 2, "", "%", true, "self_insurance"),
            alt_num("withdrawal_rate", alt_tr("SELF-MANAGED WITHDRAWAL (% of premium/yr)"), 4.0, 0, 20, 2, "", "%", true, "self_insurance"),
        };
    if (id == "variable-annuities")
        return {
            alt_text("name", alt_tr("ANNUITY NAME"), "Variable Annuity"),
            alt_num("premium", alt_tr("PREMIUM ($)"), 100000, 1000, 1e9, 0, "$", "", false, "fees,tax,surrender"),
            alt_num("me_fee", alt_tr("M&E FEE (%/yr)"), 1.25, 0, 5, 2, "", "%", true, "fees,tax,alternatives"),
            alt_num("investment_fee", alt_tr("INVESTMENT FEE (%/yr)"), 0.75, 0, 5, 2, "", "%", true, "fees,tax,alternatives"),
            alt_num("admin_fee", alt_tr("ADMIN FEE (%/yr)"), 0.15, 0, 3, 2, "", "%", true, "fees,tax,alternatives"),
            alt_num("surrender_period", alt_tr("SURRENDER PERIOD (years)"), 7, 0, 20, 0, "", "", false, "surrender,alternatives"),
            alt_num("initial_surrender_charge", alt_tr("FIRST-YEAR SURRENDER CHARGE (%)"), 7.0, 0, 20, 2, "", "%", true, "surrender"),
            alt_num("years", alt_tr("HORIZON (years)"), 20, 1, 50, 0, "", "", false, "tax"),
            alt_num("gross_return", alt_tr("GROSS RETURN (%/yr)"), 8.0, -20, 30, 2, "", "%", true, "tax"),
            alt_num("ordinary_tax_rate", alt_tr("ORDINARY INCOME TAX RATE (%)"), 37.0, 0, 60, 1, "", "%", true, "tax"),
        };
    if (id == "eia")
        return {
            alt_text("name", alt_tr("PRODUCT NAME"), "EIA Product"),
            alt_num("index_return", alt_tr("INDEX RETURN (%)"), 10.0, -50, 100, 2, "", "%", true, "crediting"),
            alt_num("participation_rate", alt_tr("PARTICIPATION (%)"), 80.0, 1, 200, 1, "", "%", true, "crediting,upside_limitation"),
            alt_num("cap_rate", alt_tr("CAP RATE (%)"), 6.0, 0, 50, 2, "", "%", true, "crediting,upside_limitation"),
            alt_num("floor_rate", alt_tr("FLOOR RATE (%)"), 0.0, -10, 10, 2, "", "%", true, "crediting,upside_limitation"),
            alt_num("premium", alt_tr("PREMIUM ($)"), 100000, 1000, 1e9, 0, "$", "", false, "surrender_charges"),
            alt_num("surrender_period", alt_tr("SURRENDER PERIOD (years)"), 7, 1, 20, 0, "", "", false, "surrender_charges"),
            alt_num("initial_surrender_charge", alt_tr("FIRST-YEAR SURRENDER CHARGE (%)"), 10.0, 0, 30, 2, "", "%", true, "surrender_charges"),
        };
    if (id == "inflation-annuity")
        return {
            alt_text("name", alt_tr("ANNUITY NAME"), "Inflation-Indexed Annuity"),
            alt_num("acquisition_price", alt_tr("PREMIUM ($)"), 100000, 1000, 1e9, 0, "$", "", false, "compare_fixed,compare_tips,inflation_value"),
            alt_num("real_payout_rate", alt_tr("REAL PAYOUT RATE (%)"), 4.0, 0.1, 15, 2, "", "%", true),
            alt_num("age", alt_tr("PURCHASE AGE (years)"), 65, 40, 90, 0, "", "", false, "compare_fixed,longevity"),
            alt_num("inflation_rate", alt_tr("ASSUMED INFLATION (%/yr)"), 3.0, 0, 15, 2, "", "%", true, "compare_fixed,inflation_value"),
            alt_num("payout_years", alt_tr("PAYOUT TERM (years)"), 20, 1, 50, 0, "", "", false, "compare_fixed,inflation_value"),
            alt_num("fixed_payout_rate", alt_tr("FIXED ANNUITY PAYOUT RATE (%)"), 5.5, 0, 15, 2, "", "%", true, "compare_fixed"),
            alt_num("tips_real_yield", alt_tr("TIPS REAL YIELD (%)"), 2.0, 0.01, 10, 2, "", "%", true, "compare_tips"),
        };
    if (id == "structured-products")
        return {
            // Only a product guide: nothing is computed from inputs.
            alt_text("name", alt_tr("PRODUCT NAME"), "Principal Protected Note"),
        };
    if (id == "leveraged-funds")
        return {
            alt_text("name", alt_tr("FUND NAME"), "3x S&P 500 ETF"),
            alt_num("leverage_ratio", alt_tr("LEVERAGE (x)"), 3.0, 1.25, 5.0, 2, "", "x", false, "decay"),
            alt_num("index_return", alt_tr("INDEX RETURN (%/yr)"), 8.0, -50, 50, 2, "", "%", true, "decay"),
            alt_num("index_volatility", alt_tr("INDEX VOLATILITY (%/yr)"), 18.0, 1, 150, 1, "", "%", true, "decay"),
            alt_num("holding_years", alt_tr("HOLDING PERIOD (years)"), 1, 1, 30, 1, "", "", false, "decay"),
            alt_num("expense_ratio", alt_tr("EXPENSE RATIO (%/yr)"), 0.95, 0, 5, 2, "", "%", true, "decay"),
        };
    if (id == "tips")
        return {
            alt_text("name", alt_tr("SECURITY NAME"), "TIPS 2031"),
            alt_num("face_value", alt_tr("FACE VALUE ($)"), 10000, 100, 1e9, 0, "$", "", false),
            alt_num("current_market_value", alt_tr("MARKET PRICE ($)"), 10200, 100, 1e9, 2, "$", "", false, "real_yield"),
            alt_num("coupon_rate", alt_tr("REAL COUPON (%)"), 2.5, 0, 10, 2, "", "%", true),
            alt_num("maturity_years", alt_tr("MATURITY (years)"), 7, 1, 30, 1, "", "", false, "real_yield,inflation_scenarios"),
            alt_num("tax_rate_ordinary", alt_tr("ORDINARY INCOME TAX RATE (%)"), 30.0, 0, 60, 1, "", "%", true, "tax_efficiency"),
        };
    if (id == "ibonds")
        return {
            alt_text("name", alt_tr("BOND NAME"), "I-Bond 2025"),
            alt_num("purchase_price", alt_tr("PURCHASE PRICE ($)"), 10000, 25, 10000, 0, "$", "", false, "composite_rate,penalty"),
            alt_num("fixed_rate", alt_tr("FIXED RATE (%)"), 0.4, 0, 5, 2, "", "%", true),
            // Treasury's composite formula takes the SEMIANNUAL rate; entering an annual
            // CPI figure here doubled the inflation component.
            alt_num("inflation_rate", alt_tr("SEMIANNUAL CPI-U INFLATION (%)"), 1.5, -10, 20, 2, "", "%", true),
            alt_num("years_held", alt_tr("YEARS HELD"), 1, 0, 30, 0, "", "", false, "composite_rate,penalty"),
            alt_num("tips_yield", alt_tr("TIPS REAL YIELD (%)"), 2.0, -2, 10, 2, "", "%", true, "compare_tips"),
        };
    if (id == "stable-value")
        return {
            alt_text("name", alt_tr("FUND NAME"), "Stable Value Fund"),
            alt_num("book_value", alt_tr("BOOK VALUE ($)"), 100000, 1000, 1e9, 0, "$", "", false, "market_to_book"),
            alt_num("market_value", alt_tr("MARKET VALUE ($)"), 98000, 1000, 1e9, 0, "$", "", false, "market_to_book"),
            alt_num("crediting_rate", alt_tr("CREDITING RATE (%)"), 3.2, 0, 15, 2, "", "%", true, "crediting_rate"),
            alt_num("portfolio_yield", alt_tr("UNDERLYING PORTFOLIO YIELD (%)"), 4.0, 0, 20, 2, "", "%", true, "crediting_rate"),
            alt_num("wrap_fee", alt_tr("WRAP FEE (%)"), 0.25, 0, 2, 2, "", "%", true, "crediting_rate"),
            alt_num("treasury_yield", alt_tr("TREASURY YIELD (%)"), 4.0, 0, 20, 2, "", "%", true, "crediting_rate"),
            alt_num("credit_spread", alt_tr("CREDIT SPREAD (%)"), 1.0, 0, 10, 2, "", "%", true, "crediting_rate"),
            alt_num("investor_age", alt_tr("INVESTOR AGE"), 50, 18, 100, 0, "", "", false, "suitability"),
            alt_num("retirement_age", alt_tr("RETIREMENT AGE"), 65, 40, 100, 0, "", "", false, "suitability"),
            alt_combo("risk_tolerance", alt_tr("RISK TOLERANCE"), "moderate|conservative|aggressive", "suitability"),
        };
    if (id == "covered-calls")
        return {
            alt_text("name", alt_tr("POSITION NAME"), "AAPL Covered Call"),
            alt_num("stock_price", alt_tr("STOCK PRICE ($)"), 150, 0.01, 1e6, 2, "$", "", false, "opportunity_cost,alternative"),
            alt_num("strike_price", alt_tr("STRIKE PRICE ($)"), 160, 0.01, 1e6, 2, "$", "", false, "tax,opportunity_cost"),
            alt_num("premium", alt_tr("PREMIUM PER SHARE ($)"), 5, 0, 1e4, 2, "$", "", false, "tax,opportunity_cost,alternative"),
            alt_num("shares", alt_tr("SHARES"), 100, 1, 1e7, 0, "", "", false, "tax,alternative"),
            alt_num("cost_basis", alt_tr("COST BASIS PER SHARE ($)"), 125, 0.01, 1e6, 2, "$", "", false, "tax"),
            alt_num("holding_period_days", alt_tr("DAYS HELD"), 400, 1, 3650, 0, "", "", false, "tax"),
        };
    if (id == "sri")
        return {
            alt_text("name", alt_tr("FUND NAME"), "ESG Equity Fund"),
            alt_num("sri_return", alt_tr("FUND RETURN (%/yr)"), 9.0, -30, 100, 2, "", "%", true, "performance"),
            alt_num("benchmark_return", alt_tr("BENCHMARK RETURN (%/yr)"), 10.0, -30, 100, 2, "", "%", true, "performance"),
            alt_num("time_period_years", alt_tr("PERIOD (years)"), 10, 1, 50, 0, "", "", false, "performance"),
            alt_num("expense_ratio", alt_tr("FUND EXPENSE RATIO (%)"), 0.8, 0, 5, 2, "", "%", true, "performance,expenses"),
            alt_num("benchmark_expense", alt_tr("INDEX FUND EXPENSE RATIO (%)"), 0.15, 0, 5, 2, "", "%", true, "performance,expenses"),
            alt_num("num_holdings", alt_tr("FUND HOLDINGS"), 100, 1, 20000, 0, "", "", false, "screening"),
            alt_num("benchmark_holdings", alt_tr("BENCHMARK HOLDINGS"), 500, 1, 20000, 0, "", "", false, "screening"),
        };
    if (id == "asset-location")
        return {
            alt_text("name", alt_tr("ASSET NAME"), "REITs / High-Yield"),
            alt_combo("asset_class", alt_tr("ASSET CLASS"), "reits|high_yield_bonds|stocks|municipal_bonds|tips|commodities|cash", "optimal,value_added"),
            alt_num("tax_bracket", alt_tr("MARGINAL TAX BRACKET (%)"), 24.0, 0, 60, 1, "", "%", true, "value_added,muni_bond"),
            alt_num("value", alt_tr("ASSET VALUE ($)"), 100000, 1, 1e12, 0, "$", "", false, "value_added"),
            alt_num("years", alt_tr("YEARS"), 30, 1, 60, 0, "", "", false, "value_added"),
            alt_num("municipal_yield", alt_tr("MUNICIPAL BOND YIELD (%)"), 3.0, 0, 20, 2, "", "%", true, "muni_bond"),
            alt_num("taxable_yield", alt_tr("TAXABLE BOND YIELD (%)"), 4.0, 0, 20, 2, "", "%", true, "muni_bond"),
        };
    if (id == "digital-assets")
        return {
            // No PRICE field: the analyzer derives price as market cap / circulating supply
            // and ignored an entered price (two different prices on one screen).
            alt_text("name", alt_tr("ASSET NAME"), "Bitcoin"),
            alt_num("market_cap", alt_tr("MARKET CAP ($B)"), 850, 0, 1e6, 1, "$", "B", false),
            alt_num("volume_24h", alt_tr("24H VOLUME ($B)"), 25, 0, 1e6, 1, "$", "B", false),
            alt_num("circulating_supply", alt_tr("CIRCULATING SUPPLY (M)"), 19.5, 0, 1e9, 2, "", "M", false),
            alt_num("max_supply", alt_tr("MAX SUPPLY (M)"), 21.0, 0, 1e9, 2, "", "M", false),
        };
    return {alt_text("name", alt_tr("NAME"), "")};
}

// ── Analysis methods (cli.py --method) ───────────────────────────────────────
// Per analyzer, in display order: the methods that run end to end in cli.py (every one checked by
// running it). A guide (last column true) is general product text, the same for every input: it is
// listed after the computed methods under "product guides", shows no inputs and no score.
// Methods cli.py lists but that need data the form cannot supply (trend_following, beta_analysis,
// equity_behavior, volatility / onchain ...) are not offered.

struct AltMethodRow {
    const char* analyzer;
    const char* id;
    const char* label;
    bool guide;
};

static const AltMethodRow kAltMethodRows[] = {
    {"high-yield", "credit_analysis", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Yield and credit spread"), false},
    {"high-yield", "default_prob", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Default probability (by rating)"), false},
    {"em-bonds", "yield_spread", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Yield spread"), false},
    {"em-bonds", "default_risk", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Sovereign default risk"), false},
    {"em-bonds", "currency_risk", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Local-currency risk"), false},
    {"convertible-bonds", "conversion_premium", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Conversion premium"), false},
    {"convertible-bonds", "bond_floor", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Bond floor"), false},
    {"convertible-bonds", "upside_participation", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Upside participation"), false},
    {"preferred-stocks", "yield_analysis", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Current yield and yield to call"), false},
    {"preferred-stocks", "call_risk", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Call risk"), false},
    {"preferred-stocks", "dividend_safety", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Product guide: dividend safety"), true},
    {"real-estate", "summary", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Income summary (NOI, cap rate)"), false},
    {"real-estate", "financing", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Financing (DSCR, LTV, levered IRR)"), false},
    {"real-estate", "dcf", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "DCF valuation"), false},
    {"hedge-funds", "metrics", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Gross vs net performance"), false},
    {"managed-futures", "fee_impact", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Fee impact"), false},
    {"market-neutral", "leverage_risk", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Exposure and leverage risk"), false},
    {"natural-resources", "basis", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Futures basis (3-month)"), false},
    {"natural-resources", "contango", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Term structure and roll yield"), false},
    {"private-capital", "metrics", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Key metrics"), false},
    {"annuities", "payouts", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Payouts and return"), false},
    {"annuities", "inflation_erosion", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Inflation erosion"), false},
    {"annuities", "self_insurance", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "vs a self-managed portfolio"), false},
    {"variable-annuities", "fees", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Total annual cost"), false},
    {"variable-annuities", "tax", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Tax deferral vs index fund"), false},
    {"variable-annuities", "surrender", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Surrender schedule"), false},
    {"variable-annuities", "alternatives", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Alternatives"), false},
    {"variable-annuities", "verdict", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Product guide: variable annuities"), true},
    {"eia", "crediting", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Credited return"), false},
    {"eia", "upside_limitation", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Upside limitation"), false},
    {"eia", "surrender_charges", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Surrender charges"), false},
    {"inflation-annuity", "compare_fixed", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Compare to fixed annuity"), false},
    {"inflation-annuity", "compare_tips", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Compare to TIPS ladder"), false},
    {"inflation-annuity", "longevity", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Longevity break-even"), false},
    {"inflation-annuity", "inflation_value", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Inflation protection value"), false},
    {"structured-products", "complexity", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Product guide: structured notes"), true},
    {"leveraged-funds", "decay", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Volatility decay"), false},
    {"leveraged-funds", "verdict", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Product guide: leveraged ETFs"), true},
    {"tips", "real_yield", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Real yield"), false},
    {"tips", "inflation_scenarios", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Inflation scenarios"), false},
    {"tips", "tax_efficiency", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Tax efficiency"), false},
    {"ibonds", "composite_rate", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Composite rate"), false},
    {"ibonds", "penalty", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Early redemption penalty"), false},
    {"ibonds", "compare_tips", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Compare to TIPS"), false},
    {"ibonds", "tax_efficiency", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Product guide: I-bond taxes"), true},
    {"stable-value", "market_to_book", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Market-to-book"), false},
    {"stable-value", "crediting_rate", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Crediting rate"), false},
    {"stable-value", "suitability", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Suitability"), false},
    {"covered-calls", "tax", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Tax consequences"), false},
    {"covered-calls", "opportunity_cost", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Opportunity cost"), false},
    {"covered-calls", "alternative", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Alternative strategy"), false},
    {"covered-calls", "verdict", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Product guide: covered calls"), true},
    {"sri", "performance", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Performance vs benchmark"), false},
    {"sri", "screening", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Screening impact"), false},
    {"sri", "expenses", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Expenses"), false},
    {"sri", "approaches", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Product guide: SRI approaches"), true},
    {"asset-location", "optimal", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Optimal location"), false},
    {"asset-location", "value_added", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Value added"), false},
    {"asset-location", "muni_bond", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Municipal bond decision"), false},
    {"digital-assets", "fundamental", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Fundamental metrics"), false},
};

static QList<AltMethod> alt_methods_for(const QString& id) {
    QList<AltMethod> out;
    for (const AltMethodRow& r : kAltMethodRows)
        if (id == QLatin1String(r.analyzer))
            out.append({QString::fromLatin1(r.id), QCoreApplication::translate("AltInvestmentsScreen", r.label), r.guide});
    return out;
}

// ── Stored values ────────────────────────────────────────────────────────────
// Per analyzer, the DISPLAY value of every field (what the user sees: 5.0 for 5%, 850 for $850B):
//   Spin -> number, Text / Combo -> string, CashFlows -> [raw rows], Series -> {unit, rows}.

static QJsonArray alt_rows_from_spec(const AltField& f) {
    static const QStringList kFlowKeys = {"date", "type", "amount", "note"};
    static const QStringList kSeriesKeys = {"date", "value"};
    const QStringList& keys = f.type == AltField::CashFlows ? kFlowKeys : kSeriesKeys;
    QJsonArray rows;
    for (const QString& row : f.combo_items.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QStringList cells = row.split(QLatin1Char('|'));
        QJsonObject o;
        for (int i = 0; i < keys.size() && i < cells.size(); ++i)
            if (!cells.at(i).trimmed().isEmpty())
                o.insert(keys.at(i), cells.at(i).trimmed());
        rows.append(o);
    }
    return rows;
}

static QJsonValue alt_default_value(const AltField& f) {
    switch (f.type) {
    case AltField::Spin:
        return f.default_val;
    case AltField::Text:
        return f.combo_items;
    case AltField::Combo:
        return f.combo_items.section(QLatin1Char('|'), 0, 0);
    case AltField::CashFlows:
        return alt_rows_from_spec(f);
    case AltField::Series:
        return QJsonObject{{"unit", "percent"}, {"rows", alt_rows_from_spec(f)}};
    }
    return {};
}

static bool alt_value_ok(const AltField& f, const QJsonValue& v) {
    switch (f.type) {
    case AltField::Spin:
        return v.isDouble();
    case AltField::Text:
        return v.isString();
    case AltField::Combo:
        return v.isString() && f.combo_items.split(QLatin1Char('|')).contains(v.toString());
    case AltField::CashFlows:
        return v.isArray();
    case AltField::Series:
        return v.isObject() && v.toObject().value("rows").isArray();
    }
    return false;
}

QJsonObject& AltInvestmentsScreen::inputs_for(const QString& analyzer_id) {
    QJsonObject& stored = inputs_[analyzer_id];
    for (const AltField& f : alt_fields_for(analyzer_id))
        if (!alt_value_ok(f, stored.value(f.key)))
            stored.insert(f.key, alt_default_value(f));
    return stored;
}

void AltInvestmentsScreen::store_input(const QString& key, const QJsonValue& value) {
    const QString id = current_analyzer_id();
    if (id.isEmpty())
        return;
    inputs_[id].insert(key, value);
    mark_result_stale();
    if (!restoring_)
        fincept::ScreenStateManager::instance().notify_changed(this);
}

void AltInvestmentsScreen::reset_inputs_to_example() {
    if (loading_)
        return;
    const QString id = current_analyzer_id();
    if (id.isEmpty())
        return;
    inputs_.remove(id);
    rebuild_form();
    mark_result_stale();
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void AltInvestmentsScreen::mark_result_stale() {
    // An input changed: the pane gets the new signature and, when a result for other inputs is on
    // screen, shows its "Inputs changed — RUN AGAIN" notice (the screen's own stale line is retired).
    update_signature();
    if (last_result_.isEmpty() || loading_ || result_stale_)
        return;
    result_stale_ = true;
}

// ── Current selection ────────────────────────────────────────────────────────

QString AltInvestmentsScreen::current_analyzer_id() const {
    if (active_category_ < 0 || active_category_ >= categories_.size())
        return {};
    const auto& ans = categories_[active_category_].analyzers;
    return (active_analyzer_ >= 0 && active_analyzer_ < ans.size()) ? ans[active_analyzer_].id : QString();
}

QString AltInvestmentsScreen::current_analyzer_name() const {
    if (active_category_ < 0 || active_category_ >= categories_.size())
        return {};
    const auto& ans = categories_[active_category_].analyzers;
    return (active_analyzer_ >= 0 && active_analyzer_ < ans.size()) ? ans[active_analyzer_].name : QString();
}

AltMethod AltInvestmentsScreen::current_method() const {
    const QList<AltMethod> methods = alt_methods_for(current_analyzer_id());
    if (methods.isEmpty())
        return {};
    const QString id = method_combo_ ? method_combo_->currentData().toString() : QString();
    for (const AltMethod& m : methods)
        if (m.id == id)
            return m;
    return methods.first();
}

// ── Methods combo ────────────────────────────────────────────────────────────

void AltInvestmentsScreen::populate_methods(const QString& analyzer_id) {
    if (!method_combo_)
        return;
    const QList<AltMethod> methods = alt_methods_for(analyzer_id);
    {
        const QSignalBlocker block(method_combo_);
        method_combo_->clear();
        bool any_computed = false;
        for (const AltMethod& m : methods)
            if (!m.guide) {
                method_combo_->addItem(m.label, m.id);
                any_computed = true;
            }
        bool separated = false;
        for (const AltMethod& m : methods)
            if (m.guide) {
                // Product guides are grouped apart so they are never read as a computed result.
                if (any_computed && !separated) {
                    method_combo_->insertSeparator(method_combo_->count());
                    separated = true;
                }
                method_combo_->addItem(m.label, m.id);
                method_combo_->setItemData(method_combo_->count() - 1,
                                           tr("General product guide - the same for every input, not an analysis "
                                              "of your numbers"),
                                           Qt::ToolTipRole);
            }
        int idx = method_combo_->findData(methods_.value(analyzer_id));
        if (idx < 0)
            idx = method_combo_->findData(methods.isEmpty() ? QString() : methods.first().id);
        method_combo_->setCurrentIndex(std::max(0, idx));
    }
    // Nothing to choose between when only one method is available.
    const bool choosable = methods.size() > 1;
    method_combo_->setVisible(choosable);
    if (method_label_)
        method_label_->setVisible(choosable);
}

// ── Selection slots ──────────────────────────────────────────────────────────

void AltInvestmentsScreen::on_category_changed(int index) {
    if (index < 0 || index >= categories_.size())
        return;
    active_category_ = index;

    for (int i = 0; i < cat_btns_.size(); ++i) {
        cat_btns_[i]->setProperty("active", i == index);
        cat_btns_[i]->style()->unpolish(cat_btns_[i]);
        cat_btns_[i]->style()->polish(cat_btns_[i]);
    }

    const auto& cat = categories_[index];
    center_title_->setText(cat.name.toUpper());
    status_category_->setText(tr("CATEGORY: %1").arg(cat.name.toUpper()));

    {
        const QSignalBlocker block(analyzer_combo_);
        analyzer_combo_->clear();
        for (const auto& a : cat.analyzers)
            analyzer_combo_->addItem(a.name);
    }
    // One analyzer: the combo would only repeat the title.
    analyzer_combo_->setVisible(cat.analyzers.size() > 1);
    if (combo_label_)
        combo_label_->setVisible(cat.analyzers.size() > 1);

    on_analyzer_changed(0);
    LOG_DEBUG("AltInvestments", "Category: " + cat.name);
}

void AltInvestmentsScreen::on_analyzer_changed(int index) {
    if (index < 0 || index >= categories_[active_category_].analyzers.size())
        return;
    // A reply for the analyzer being left is never drawn under the new one: the pane's view
    // changes below (leaving it cancels a default run; a run the user started finishes for it).
    active_analyzer_ = index;

    const auto& ana = categories_[active_category_].analyzers[index];
    center_desc_->setText(ana.description);
    status_analyzer_->setText(ana.name.toUpper());
    {
        const QSignalBlocker block(analyzer_combo_);
        analyzer_combo_->setCurrentIndex(index);
    }
    populate_methods(ana.id);
    rebuild_form();
    select_view(); // its remembered result at once, and its analysis with the inputs on screen

    if (!restoring_)
        fincept::ScreenStateManager::instance().notify_changed(this);
}

void AltInvestmentsScreen::on_method_changed(int index) {
    if (index < 0 || !method_combo_)
        return;
    const QString id = method_combo_->itemData(index).toString();
    if (id.isEmpty())
        return;
    methods_.insert(current_analyzer_id(), id);
    rebuild_form();
    select_view();
    if (!restoring_)
        fincept::ScreenStateManager::instance().notify_changed(this);
}

// ── Form building ────────────────────────────────────────────────────────────

// Currency-aware field helpers: an amount field is in the user's display currency (a calculator the
// user denominates; never converted when the setting changes). Labels ending in "($)", "($/yr)" or
// "($B)" and spin prefixes of "$" follow the display currency and the symbol style live.
static void set_alt_field_label(QLabel* lbl, const QString& label) {
    namespace fmt = fincept::ui::formatting;
    if (label.endsWith("($)")) {
        QString base = label;
        base.chop(3); // strip "($)"
        fmt::bind_currency_label(lbl, base + "(%1)");
    } else if (label.contains(QLatin1String("($/yr)")) || label.contains(QLatin1String("($B)"))) {
        QString tmpl = label;
        tmpl.replace(QLatin1String("($/yr)"), QLatin1String("(%1/yr)"));
        tmpl.replace(QLatin1String("($B)"), QLatin1String("(%1 B)"));
        fmt::bind_currency_label(lbl, tmpl);
    } else {
        lbl->setText(label);
    }
}

static void apply_alt_field_prefix(QDoubleSpinBox* sp, const QString& prefix) {
    if (prefix == QLatin1String("$"))
        fincept::ui::formatting::bind_currency_affix(sp); // prefix or suffix per the user's settings
    else if (!prefix.isEmpty())
        sp->setPrefix(prefix);
}

// Enter in a field runs the analysis (the data grids keep Enter for moving between rows).
static void alt_enter_runs(QWidget* w, AltInvestmentsScreen* screen, void (AltInvestmentsScreen::*slot)()) {
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto* sc = new QShortcut(QKeySequence(key), w);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(sc, &QShortcut::activated, screen, slot);
    }
}

// Cash-flow grid: date | type | amount | note. Pasted two-column data (date, signed amount) gets its
// type from the sign; three columns may be (date, type, amount) or (date, amount, note).
static QString alt_flow_type(const QString& raw) {
    const QString v = raw.trimmed().toLower();
    static const QHash<QString, QString> kTypes = {
        {"call", "call"},
        {"capital call", "call"},
        {"contribution", "call"},
        {"drawdown", "call"},
        {"paid-in", "call"},
        {"paid in", "call"},
        {"investment", "call"},
        {"distribution", "distribution"},
        {"dist", "distribution"},
        {"dist.", "distribution"},
        {"return of capital", "distribution"},
        {"proceeds", "distribution"},
        {"nav", "nav"},
        {"net asset value", "nav"},
        {"valuation", "nav"},
        {"value", "nav"},
        {"residual value", "nav"},
        {"fair value", "nav"},
    };
    return kTypes.value(v);
}

static ui::DataGridEditor* alt_make_flow_grid(QWidget* parent) {
    using ui::DataGridColumn;
    auto type_col = DataGridColumn::choice(
        "type", alt_tr("TYPE"), {"call", "distribution", "nav"},
        {alt_tr("Call"), alt_tr("Distribution"), alt_tr("NAV")});
    type_col.synonyms = {{"capital call", "call"},    {"contribution", "call"},   {"drawdown", "call"},
                         {"paid-in", "call"},         {"paid in", "call"},        {"investment", "call"},
                         {"dist", "distribution"},    {"dist.", "distribution"},  {"return of capital", "distribution"},
                         {"proceeds", "distribution"}, {"net asset value", "nav"}, {"valuation", "nav"},
                         {"value", "nav"},            {"residual value", "nav"},  {"fair value", "nav"}};
    type_col.tooltip = alt_tr("Call = capital paid in, Distribution = cash returned, NAV = the fund's value on "
                              "that date (the latest NAV is used)");
    // Read like every money field ("2.5M", "1,250,000", "(500,000)"), in the user's number format.
    auto amount_col = DataGridColumn::amount("amount", alt_tr("AMOUNT"), QString(), 0);
    amount_col.tooltip = alt_tr("Positive amounts; the type gives the direction (a negative amount pasted without a "
                                "type is read as a call)");
    QVector<DataGridColumn> cols = {DataGridColumn::date("date", alt_tr("DATE")), type_col, amount_col,
                                    DataGridColumn::text("note", alt_tr("NOTE"))};
    auto* grid = new ui::DataGridEditor(cols, parent);
    grid->set_paste_adapter([](const QVector<QStringList>& cells) {
        QVector<QStringList> out;
        out.reserve(cells.size());
        for (const QStringList& c : cells) {
            auto by_sign = [](const QString& amount) {
                const ui::formatting::AmountInput v = ui::formatting::read_amount_input(amount, QString());
                if (!v.ok())
                    return QString();
                return *v.value < 0 ? QStringLiteral("call") : QStringLiteral("distribution");
            };
            const QString c0 = c.value(0), c1 = c.value(1), c2 = c.value(2);
            if (c.size() <= 1) {
                out << QStringList{c0, QString(), QString(), QString()};
            } else if (c.size() == 2) {
                out << QStringList{c0, by_sign(c1), c1, QString()};
            } else if (!alt_flow_type(c1).isEmpty()) {
                out << QStringList{c0, alt_flow_type(c1), c2, c.mid(3).join(QLatin1Char(' '))};
            } else if (!alt_flow_type(c2).isEmpty()) {
                out << QStringList{c0, alt_flow_type(c2), c1, c.mid(3).join(QLatin1Char(' '))};
            } else {
                out << QStringList{c0, by_sign(c1), c1, c.mid(2).join(QLatin1Char(' '))};
            }
        }
        return out;
    });
    grid->set_hint(alt_tr("paste from a spreadsheet: date, amount (calls negative) or date, type, amount"));
    return grid;
}

// Return series: optional date | value.
static ui::DataGridEditor* alt_make_series_grid(QWidget* parent) {
    using ui::DataGridColumn;
    auto value_col = DataGridColumn::number("value", alt_tr("RETURN"), 2);
    QVector<DataGridColumn> cols = {DataGridColumn::date("date", alt_tr("DATE (optional)"), /*required=*/false),
                                    value_col};
    auto* grid = new ui::DataGridEditor(cols, parent);
    grid->set_paste_adapter([](const QVector<QStringList>& cells) {
        QVector<QStringList> out;
        out.reserve(cells.size());
        for (const QStringList& c : cells) {
            if (c.size() <= 1) {
                out << QStringList{QString(), c.value(0)};
            } else if (ui::DataGridEditor::parse_date(c.value(0), nullptr)) {
                out << QStringList{c.value(0), c.value(1)};
            } else if (ui::DataGridEditor::parse_date(c.value(1), nullptr)) {
                out << QStringList{c.value(1), c.value(0)};
            } else {
                out << QStringList{c.value(0), c.value(1)};
            }
        }
        return out;
    });
    grid->set_hint(alt_tr("paste a column of returns, or dates and returns"));
    grid->set_visible_rows(10);
    return grid;
}

void AltInvestmentsScreen::rebuild_form() {
    field_widgets_.clear();
    current_fields_.clear();
    // A live-data fetch for the old form is stopped; its reply would have nothing to fill.
    cancel_live();
    live_note_.clear();
    live_input_.clear();
    live_buttons_.clear();
    while (form_layout_->count() > 0) {
        auto* item = form_layout_->takeAt(0);
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
    if (form_problems_) {
        form_problems_->clear();
        form_problems_->setVisible(false);
    }

    const QString ana_id = current_analyzer_id();
    if (ana_id.isEmpty())
        return;
    const AltMethod method = current_method();

    if (method.guide) {
        method_note_->setText(tr("Product guide: general reading about this kind of product. It is the same text "
                                 "whatever you enter, so there is nothing to fill in, and it is not a rating of "
                                 "any product you hold."));
        method_note_->setVisible(true);
        auto* note = new QLabel(tr("No inputs - press ANALYZE to open the guide."), form_container_);
        note->setObjectName("altFieldLabel");
        note->setWordWrap(true);
        form_layout_->addWidget(note);
        form_layout_->addStretch(1);
        return;
    }
    method_note_->setVisible(false);

    const QJsonObject vals = inputs_for(ana_id); // a copy: store_input() writes the hash
    for (const AltField& f : alt_fields_for(ana_id))
        if (f.applies_to(method.id))
            current_fields_.append(f);

    // A4: free market data for the fields that have a source (on request; AltInvestmentsScreen_Live.cpp).
    if (QWidget* live = build_live_strip(ana_id, method, form_container_))
        form_layout_->addWidget(live);

    auto make_label = [](const AltField& field, QWidget* parent) {
        auto* lbl = new QLabel(parent);
        set_alt_field_label(lbl, field.label);
        lbl->setObjectName("altFieldLabel");
        lbl->setWordWrap(true);
        if (!field.tooltip.isEmpty())
            lbl->setToolTip(field.tooltip);
        return lbl;
    };
    auto make_spin = [this, &vals](const AltField& field, QWidget* parent) {
        auto* sp = new QDoubleSpinBox(parent);
        sp->setRange(field.min_val, field.max_val);
        sp->setDecimals(field.decimals);
        sp->setValue(vals.value(field.key).toDouble(field.default_val));
        sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sp->setAccessibleName(field.label);
        apply_alt_field_prefix(sp, field.prefix);
        if (!field.suffix.isEmpty())
            sp->setSuffix(field.suffix == QLatin1String("x") ? QStringLiteral("x") : field.suffix);
        const QString key = field.key;
        connect(sp, &QDoubleSpinBox::valueChanged, this, [this, key](double v) { store_input(key, v); });
        alt_enter_runs(sp, this, &AltInvestmentsScreen::on_analyze);
        field_widgets_.insert(key, sp);
        return sp;
    };

    int i = 0;
    while (i < current_fields_.size()) {
        const AltField& f = current_fields_[i];
        const bool pair = (f.type == AltField::Spin && i + 1 < current_fields_.size() &&
                           current_fields_[i + 1].type == AltField::Spin);

        if (pair) {
            auto* row = new QWidget(form_container_);
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(8);
            for (int k = 0; k < 2; ++k) {
                const AltField& field = current_fields_[i + k];
                auto* col = new QWidget(row);
                auto* cl = new QVBoxLayout(col);
                cl->setContentsMargins(0, 0, 0, 0);
                cl->setSpacing(3);
                cl->addWidget(make_label(field, col));
                cl->addStretch(1);
                cl->addWidget(make_spin(field, col));
                rl->addWidget(col, 1);
            }
            form_layout_->addWidget(row);
            i += 2;
            continue;
        }

        auto* col = new QWidget(form_container_);
        auto* cl = new QVBoxLayout(col);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(3);
        cl->addWidget(make_label(f, col));
        const QString key = f.key;

        if (f.type == AltField::Text) {
            auto* le = new QLineEdit(vals.value(key).toString(f.combo_items), col);
            le->setAccessibleName(f.label);
            connect(le, &QLineEdit::textChanged, this, [this, key](const QString& t) { store_input(key, t); });
            connect(le, &QLineEdit::returnPressed, this, &AltInvestmentsScreen::on_analyze);
            cl->addWidget(le);
            field_widgets_.insert(key, le);
        } else if (f.type == AltField::Combo) {
            auto* cb = new QComboBox(col);
            for (const auto& item : f.combo_items.split('|'))
                cb->addItem(item.trimmed());
            cb->setCurrentText(vals.value(key).toString());
            cb->setAccessibleName(f.label);
            connect(cb, &QComboBox::currentTextChanged, this, [this, key](const QString& t) { store_input(key, t); });
            alt_enter_runs(cb, this, &AltInvestmentsScreen::on_analyze);
            cl->addWidget(cb);
            field_widgets_.insert(key, cb);
        } else if (f.type == AltField::CashFlows) {
            auto* grid = alt_make_flow_grid(col);
            grid->from_json(vals.value(key).toArray());
            connect(grid, &ui::DataGridEditor::changed, this,
                    [this, key, grid]() { store_input(key, grid->raw_json()); });
            cl->addWidget(grid);
            field_widgets_.insert(key, grid);
        } else if (f.type == AltField::Series) {
            const QJsonObject series = vals.value(key).toObject();
            auto* unit_row = new QWidget(col);
            auto* ul = new QHBoxLayout(unit_row);
            ul->setContentsMargins(0, 0, 0, 0);
            ul->setSpacing(6);
            auto* unit_lbl = new QLabel(tr("VALUES ARE"), unit_row);
            unit_lbl->setObjectName("altFieldLabel");
            auto* unit = new QComboBox(unit_row);
            unit->addItem(tr("Percent (1.2 = 1.2%)"), QStringLiteral("percent"));
            unit->addItem(tr("Decimals (0.012 = 1.2%)"), QStringLiteral("decimal"));
            unit->setCurrentIndex(series.value("unit").toString() == QLatin1String("decimal") ? 1 : 0);
            unit->setAccessibleName(tr("Return units"));
            ul->addWidget(unit_lbl);
            ul->addWidget(unit, 1);
            cl->addWidget(unit_row);
            auto* grid = alt_make_series_grid(col);
            grid->from_json(series.value("rows").toArray());
            auto store = [this, key, grid, unit]() {
                store_input(key, QJsonObject{{"unit", unit->currentData().toString()}, {"rows", grid->raw_json()}});
            };
            connect(grid, &ui::DataGridEditor::changed, this, store);
            connect(unit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, store);
            cl->addWidget(grid);
            field_widgets_.insert(key, grid);
            field_widgets_.insert(key + QStringLiteral("#unit"), unit);
        } else {
            cl->addWidget(make_spin(f, col));
        }
        form_layout_->addWidget(col);
        ++i;
    }
    form_layout_->addStretch(1);
}

// ── Form data collection ─────────────────────────────────────────────────────

QJsonObject AltInvestmentsScreen::collect_form_data(QStringList* problems) const {
    QJsonObject form;
    const QJsonObject vals = inputs_.value(current_analyzer_id());
    for (const AltField& f : current_fields_) {
        const QJsonValue v = vals.value(f.key);
        if (f.type == AltField::Text) {
            const QString t = v.toString(f.combo_items).trimmed();
            if (!t.isEmpty() || f.key == QLatin1String("name"))
                form[f.key] = t;
        } else if (f.type == AltField::Combo) {
            form[f.key] = v.toString();
        } else if (f.type == AltField::CashFlows) {
            const auto* grid = qobject_cast<const ui::DataGridEditor*>(field_widgets_.value(f.key).data());
            QJsonArray flows;
            if (grid) {
                for (const QJsonValue& row : grid->to_json()) {
                    QJsonObject o = row.toObject();
                    o.remove(QStringLiteral("note")); // for the user only
                    flows.append(o);
                }
                if (grid->invalid_row_count() > 0 && problems)
                    *problems << grid->problems_text();
            }
            form[f.key] = flows;
        } else if (f.type == AltField::Series) {
            const auto* grid = qobject_cast<const ui::DataGridEditor*>(field_widgets_.value(f.key).data());
            const bool percent = v.toObject().value("unit").toString() != QLatin1String("decimal");
            QJsonArray rows = grid ? grid->to_json() : QJsonArray();
            if (grid && grid->invalid_row_count() > 0 && problems)
                *problems << grid->problems_text();
            // Dated series are put in date order; dates are sent only when every row has one.
            int dated = 0;
            for (const QJsonValue& r : rows)
                if (r.toObject().contains(QStringLiteral("date")))
                    ++dated;
            const bool all_dated = dated > 0 && dated == rows.size();
            if (dated > 0 && !all_dated && problems)
                *problems << tr("Dates were ignored: only %1 of %2 rows have one.").arg(dated).arg(rows.size());
            QVector<QJsonObject> ordered;
            for (const QJsonValue& r : rows)
                ordered << r.toObject();
            if (all_dated)
                std::stable_sort(ordered.begin(), ordered.end(), [](const QJsonObject& a, const QJsonObject& b) {
                    return a.value("date").toString() < b.value("date").toString();
                });
            QJsonArray values, dates;
            for (const QJsonObject& r : ordered) {
                const double x = r.value("value").toDouble();
                values.append(percent ? x / 100.0 : x);
                if (all_dated)
                    dates.append(r.value("date").toString());
            }
            form[f.key] = values;
            if (all_dated)
                form[QStringLiteral("dates")] = dates;
        } else {
            double val = v.toDouble(f.default_val);
            // Scale and percent-conversion are independent concerns: a field
            // can legitimately be entered in billions *and* be a rate.
            if (f.suffix == "B")
                val *= 1e9;
            else if (f.suffix == "M")
                val *= 1e6;
            if (f.divide_100)
                val /= 100.0;
            form[f.key] = val;
        }
    }
    return form;
}

QJsonObject AltInvestmentsScreen::input_echo() const {
    // Label -> value as shown, in form order (keys prefixed with their index to keep the order).
    QJsonObject echo;
    const QJsonObject vals = inputs_.value(current_analyzer_id());
    int n = 0;
    for (const AltField& f : current_fields_) {
        const QJsonValue v = vals.value(f.key);
        QString shown;
        switch (f.type) {
        case AltField::Spin: {
            shown = QLocale().toString(v.toDouble(f.default_val), 'f', f.decimals);
            // "$" marks a money field: an amount in the user's display currency (not converted), the
            // same setting as the field's label (fmt::bind_currency_label).
            if (f.prefix == QLatin1String("$"))
                shown = fincept::ui::formatting::money(
                    v.toDouble(f.default_val) * (f.suffix == "B" ? 1e9 : f.suffix == "M" ? 1e6 : 1.0),
                    fincept::ui::formatting::display_currency());
            else if (!f.suffix.isEmpty())
                shown += f.suffix;
            break;
        }
        case AltField::Text:
        case AltField::Combo:
            shown = v.toString();
            break;
        case AltField::CashFlows:
        case AltField::Series: {
            const auto* grid = qobject_cast<const ui::DataGridEditor*>(field_widgets_.value(f.key).data());
            shown = grid ? tr("%1 rows").arg(grid->valid_row_count()) : QString();
            break;
        }
        }
        QString label = f.label;
        // The unit of the value above, in the user's symbol style.
        label.replace(QLatin1String("($)"),
                      QStringLiteral("(%1)").arg(fincept::ui::formatting::currency_unit(
                          fincept::ui::formatting::display_currency())));
        echo.insert(QStringLiteral("%1\x1f%2").arg(n++, 3, 10, QLatin1Char('0')).arg(label), shown);
    }
    return echo;
}

} // namespace fincept::screens
