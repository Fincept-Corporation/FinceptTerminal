// src/screens/alt_investments/AltInvestmentsScreen.cpp
//
// Core: kStyle, static field-builder helpers, ctor, show/hide events,
// format_value, display_error, set_loading, save_state/restore_state.
// Other concerns:
//   - AltInvestmentsScreen_Layout.cpp   — setup_ui + create_* panels
//   - AltInvestmentsScreen_Analysis.cpp — form/analysis/verdict rendering
#include "screens/alt_investments/AltInvestmentsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/python_cli/PythonCliService.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>

// ── Stylesheet ───────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

static const QString kStyle =
    QStringLiteral("#altScreen { background: %1; }"

                   "#altHeader { background: %2; border-bottom: 2px solid %3; padding: 0 16px; }"
                   "#altHeaderTitle { color: %4; font-size: 13px; font-weight: 700; background: transparent; }"
                   "#altHeaderSub   { color: %5; font-size: 9px; letter-spacing: 0.8px; background: transparent; }"
                   "#altHeaderBadge { color: %6; font-size: 8px; font-weight: 700; letter-spacing: 0.5px;"
                   "  background: rgba(22,163,74,0.18); border: 1px solid rgba(22,163,74,0.4); padding: 2px 8px; }"

                   "#altLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#altLeftTitle  { color: %5; font-size: 9px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: %2; padding: 8px 12px; border-bottom: 1px solid %8; }"
                   "#altCatBtn { background: transparent; color: %5; border: none; border-bottom: 1px solid %8;"
                   "  text-align: left; font-size: 11px; padding: 9px 14px; }"
                   "#altCatBtn:hover { color: %4; background: %12; }"
                   "#altCatBtn[active=\"true\"] { color: %3; font-weight: 700;"
                   "  background: rgba(217,119,6,0.08); border-left: 3px solid %3; padding-left: 11px; }"

                   "#altCenterPanel { background: %1; }"
                   "#altCenterTitleBar { background: %2; border-bottom: 1px solid %8; }"
                   "#altCenterTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#altCenterDesc  { color: %5; font-size: 10px; background: transparent; }"
                   "#altComboLabel  { color: %5; font-size: 9px; font-weight: 700;"
                   "  letter-spacing: 0.5px; background: transparent; }"

                   "#altFormPanel  { background: %7; border: 1px solid %8; }"
                   "#altFormHeader { background: %2; border-bottom: 1px solid %8; padding: 0 12px; }"
                   "#altFormTitle  { color: %4; font-size: 10px; font-weight: 700; background: transparent; }"
                   "#altFieldLabel { color: %5; font-size: 9px; font-weight: 700;"
                   "  letter-spacing: 0.5px; background: transparent; }"

                   "QLineEdit { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: 11px; }"
                   "QLineEdit:focus { border-color: %9; }"
                   "QDoubleSpinBox { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: 11px; }"
                   "QDoubleSpinBox:focus { border-color: %9; }"
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: 11px; }"
                   "QComboBox::drop-down { border: none; width: 18px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8;"
                   "  selection-background-color: %3; selection-color: %1; outline: none; }"

                   "#altAnalyzeBtn { background: %3; color: %1; border: none; padding: 7px 20px;"
                   "  font-size: 10px; font-weight: 700; letter-spacing: 0.5px; }"
                   "#altAnalyzeBtn:hover    { background: #FF8800; }"
                   "#altAnalyzeBtn:disabled { background: %10; color: %11; }"

                   "#altRightPanel { background: %7; border-left: 1px solid %8; }"
                   "#altRightTitle { color: %5; font-size: 9px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: %2; padding: 8px 12px; border-bottom: 1px solid %8; }"
                   "#altVerdictBadge  { font-size: 11px; font-weight: 700; padding: 4px 14px;"
                   "  letter-spacing: 0.5px; }"
                   "#altVerdictRating { color: %13; font-size: 14px; font-weight: 700; background: transparent; }"
                   "#altVerdictRec    { color: %4; font-size: 10px; background: transparent; }"

                   "#altMetricRow     { background: transparent; border-bottom: 1px solid %8; }"
                   "#altMetricKey     { color: %5; font-size: 10px; background: transparent; }"
                   "#altMetricVal     { color: %13; font-size: 10px; font-weight: 700; background: transparent; }"
                   "#altMetricSection { color: %3; font-size: 9px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: %2; padding: 4px 10px; border-top: 1px solid %8; }"
                   "#altMetricBullet    { color: %5;  font-size: 10px; background: transparent; }"
                   "#altMetricBulletVal { color: %4;  font-size: 10px; background: transparent; }"
                   "#altMetricWarn      { color: #FFD700; font-size: 10px; background: transparent; }"
                   "#altMetricGood      { color: %6;  font-size: 10px; background: transparent; }"
                   "#altMetricBad       { color: %14; font-size: 10px; background: transparent; }"

                   "#altStatusBar  { background: %2; border-top: 1px solid %8; }"
                   "#altStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#altStatusHigh { color: %13; font-size: 9px; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollBar:vertical { background: %1; width: 5px; border: none; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar:horizontal { height: 0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::TEXT_DIM())       // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE())       // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Field factory helpers ────────────────────────────────────────────────────


static AltField text_field(const QString& key, const QString& label, const QString& def = "") {
    AltField f;
    f.key = key;
    f.label = label;
    f.type = AltField::Text;
    f.combo_items = def;
    return f;
}

static AltField spin_field(const QString& key, const QString& label, double def, double min_v, double max_v,
                           int dec = 2, const QString& pfx = "", const QString& sfx = "", bool div100 = false) {
    AltField f;
    f.key = key;
    f.label = label;
    f.type = AltField::Spin;
    f.default_val = def;
    f.min_val = min_v;
    f.max_val = max_v;
    f.decimals = dec;
    f.prefix = pfx;
    f.suffix = sfx;
    f.divide_100 = div100;
    return f;
}

static AltField combo_field(const QString& key, const QString& label, const QString& items) {
    AltField f;
    f.key = key;
    f.label = label;
    f.type = AltField::Combo;
    f.combo_items = items;
    return f;
}

// ── Category definitions ─────────────────────────────────────────────────────

static QList<AltCategory> build_categories() {
    return {
        {"bonds",
         "Bonds & Fixed Income",
         "#3B82F6",
         "B",
         {
             {"high-yield", "High-Yield Bonds", "Spread analysis, default probability, equity-like behavior"},
             {"em-bonds", "Emerging Market Bonds", "Yield spread, sovereign default risk, currency risk"},
             {"convertible-bonds", "Convertible Bonds", "Conversion premium, bond floor, upside participation"},
             {"preferred-stocks", "Preferred Stocks", "Current yield, call risk, dividend safety analysis"},
         }},
        {"real_estate",
         "Real Estate",
         "#8B5CF6",
         "RE",
         {
             {"real-estate", "Direct Property / REIT", "NOI, cap rate, DCF valuation, financing analysis"},
         }},
        {"hedge_funds",
         "Hedge Funds",
         "#EAB308",
         "HF",
         {
             {"hedge-funds", "Long/Short Equity", "Strategy metrics, fee impact, liquidity risk"},
             {"managed-futures", "Managed Futures / CTA", "Fee drag, trend following, crisis alpha analysis"},
             {"market-neutral", "Market Neutral", "Beta neutrality, factor exposure, leverage risk"},
         }},
        {"commodities",
         "Commodities",
         "#F97316",
         "C",
         {
             {"natural-resources", "Natural Resources", "Futures basis, contango/backwardation, roll yield"},
             {"pme", "Precious Metals Equities", "Correlation to metals, inflation hedge, drawdowns"},
         }},
        {"private_capital",
         "Private Capital",
         "#EC4899",
         "PE",
         {
             {"private-capital", "Private Equity / Venture Capital", "IRR, MOIC, DPI, RVPI, J-curve, fee drag"},
         }},
        {"annuities",
         "Annuities",
         "#22C55E",
         "AN",
         {
             {"annuities", "Fixed Annuity", "Payout analysis, breakeven years, inflation erosion"},
             {"variable-annuities", "Variable Annuity", "Total fee drag, tax deferral myth, vs. alternatives"},
             {"eia", "Equity-Indexed Annuity", "Crediting rate, upside cap limitations, surrender charges"},
             {"inflation-annuity", "Inflation-Indexed Annuity",
              "vs. fixed annuity, vs. TIPS ladder, longevity break-even"},
         }},
        {"structured",
         "Structured Products",
         "#06B6D4",
         "SP",
         {
             {"structured-products", "Structured Notes", "Complexity score, hidden costs, issuer credit risk"},
             {"leveraged-funds", "Leveraged ETFs (2x/3x)", "Volatility decay, path-dependency, holding cost"},
         }},
        {"inflation",
         "Inflation Protected",
         "#FB923C",
         "IP",
         {
             {"tips", "TIPS — Inflation-Linked Treasuries", "Real yield, inflation scenarios, tax efficiency"},
             {"ibonds", "I-Bonds — Series I Savings Bonds", "Composite rate, early redemption penalty, vs. TIPS"},
             {"stable-value", "Stable Value Funds", "Market-to-book ratio, crediting rate, wrap contract risk"},
         }},
        {"strategies",
         "Strategies & Analysis",
         "#67E8F9",
         "ST",
         {
             {"covered-calls", "Covered Call Strategy", "Tax consequences, opportunity cost, premium income"},
             {"sri", "SRI / ESG Funds", "Performance vs. benchmark, screening costs, approaches"},
             {"asset-location", "Tax-Efficient Asset Location",
              "Optimal account placement, muni bonds, annual tax drag"},
         }},
        {"crypto",
         "Digital Assets",
         "#F87171",
         "DA",
         {
             {"digital-assets", "Cryptocurrency", "Fundamental metrics, NVT ratio, market cap analysis"},
         }},
    };
}

// ── Per-analyzer form fields ─────────────────────────────────────────────────

static QList<AltField> fields_for(const QString& id) {
    if (id == "high-yield")
        return {
            text_field("name", "BOND NAME", "HY Corp Bond"),
            spin_field("par_value", "PAR VALUE ($)", 1000, 100, 1e9, 0, "$"),
            spin_field("price", "MARKET PRICE ($)", 950, 100, 1e9, 2, "$"),
            spin_field("coupon_rate", "COUPON RATE (%)", 8.5, 0, 30, 2, "", "%"),
            spin_field("maturity_years", "MATURITY (years)", 5, 0.5, 30, 1),
            combo_field("credit_rating", "CREDIT RATING", "BB|B|CCC|BB+|BB-|B+|B-|CCC+"),
        };
    if (id == "em-bonds")
        return {
            text_field("name", "BOND NAME", "Brazil 2030"),
            spin_field("face_value", "FACE VALUE ($)", 1000, 100, 1e9, 0, "$"),
            spin_field("current_market_value", "MARKET PRICE ($)", 950, 100, 1e9, 2, "$"),
            spin_field("coupon_rate", "COUPON RATE (%)", 6.0, 0, 25, 2, "", "%"),
            spin_field("maturity_years", "MATURITY (years)", 10, 0.5, 30, 1),
            spin_field("credit_spread", "CREDIT SPREAD (%)", 3.0, 0, 20, 2, "", "%", true),
        };
    if (id == "convertible-bonds")
        return {
            text_field("name", "BOND NAME", "Tesla Conv 2028"),
            spin_field("par_value", "PAR VALUE ($)", 1000, 100, 1e9, 0, "$"),
            spin_field("current_price", "MARKET PRICE ($)", 1100, 100, 1e9, 2, "$"),
            spin_field("coupon_rate", "COUPON RATE (%)", 2.0, 0, 15, 2, "", "%"),
            spin_field("maturity_years", "MATURITY (years)", 5, 0.5, 30, 1),
            spin_field("stock_price", "STOCK PRICE ($)", 55, 1, 1e6, 2, "$"),
            spin_field("conversion_ratio", "CONVERSION RATIO", 18, 1, 1e4, 2),
        };
    if (id == "preferred-stocks")
        return {
            text_field("name", "SECURITY NAME", "JPM Series J Preferred"),
            spin_field("par_value", "PAR VALUE ($)", 25, 1, 1e4, 2, "$"),
            spin_field("current_price", "MARKET PRICE ($)", 24.5, 1, 1e4, 2, "$"),
            spin_field("dividend_rate", "DIVIDEND RATE (%)", 5.5, 0, 20, 2, "", "%", true),
        };
    if (id == "real-estate")
        return {
            text_field("name", "PROPERTY NAME", "Class A Office"),
            spin_field("acquisition_price", "PURCHASE PRICE ($)", 1000000, 1000, 1e12, 0, "$"),
            spin_field("gross_income", "GROSS INCOME ($/yr)", 120000, 0, 1e9, 0, "$"),
            spin_field("vacancy_rate", "VACANCY RATE (%)", 5.0, 0, 100, 1, "", "%", true),
            spin_field("operating_expenses", "OPER. EXPENSES ($/yr)", 40000, 0, 1e9, 0, "$"),
            spin_field("loan_amount", "LOAN AMOUNT ($)", 700000, 0, 1e12, 0, "$"),
            spin_field("interest_rate", "MORTGAGE RATE (%)", 6.5, 0, 30, 2, "", "%", true),
            spin_field("loan_term", "LOAN TERM (years)", 30, 5, 50, 0),
        };
    if (id == "hedge-funds")
        return {
            text_field("name", "FUND NAME", "Global Macro Fund"),
            spin_field("management_fee", "MGMT FEE (%)", 2.0, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "PERF FEE (%)", 20.0, 0, 50, 1, "", "%", true),
            spin_field("hurdle_rate", "HURDLE RATE (%)", 6.0, 0, 20, 2, "", "%", true),
            combo_field("strategy_type", "STRATEGY",
                        "equity_long_short|global_macro|event_driven|relative_value|multi_strategy"),
        };
    if (id == "managed-futures")
        return {
            text_field("name", "FUND NAME", "CTA Trend Fund"),
            spin_field("management_fee", "MGMT FEE (%)", 2.0, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "PERF FEE (%)", 20.0, 0, 50, 1, "", "%", true),
            spin_field("gross_return", "GROSS RETURN (%)", 12.0, -50, 100, 2, "", "%", true),
        };
    if (id == "market-neutral")
        return {
            text_field("name", "FUND NAME", "Market Neutral Fund"),
            spin_field("gross_leverage", "GROSS LEVERAGE", 2.0, 0.5, 10, 1),
            spin_field("management_fee", "MGMT FEE (%)", 1.5, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "PERF FEE (%)", 20.0, 0, 50, 1, "", "%", true),
        };
    if (id == "natural-resources")
        return {
            text_field("name", "COMMODITY NAME", "WTI Crude Oil"),
            spin_field("spot_price", "SPOT PRICE ($)", 80, 0, 1e6, 2, "$"),
            spin_field("three_month_futures", "3M FUTURES ($)", 82, 0, 1e6, 2, "$"),
            spin_field("six_month_futures", "6M FUTURES ($)", 84, 0, 1e6, 2, "$"),
            spin_field("twelve_month_futures", "12M FUTURES ($)", 87, 0, 1e6, 2, "$"),
            combo_field("sector", "SECTOR", "energy|metals|agriculture|livestock"),
        };
    if (id == "pme")
        return {
            text_field("name", "FUND NAME", "Gold Miners ETF"),
        };
    if (id == "private-capital")
        return {
            text_field("name", "FUND NAME", "Buyout Fund III"),
            spin_field("management_fee", "MGMT FEE (%)", 2.0, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "CARRIED INT (%)", 20.0, 0, 40, 1, "", "%", true),
            spin_field("hurdle_rate", "HURDLE RATE (%)", 8.0, 0, 20, 2, "", "%", true),
        };
    if (id == "annuities")
        return {
            text_field("name", "ANNUITY NAME", "Fixed Annuity"),
            spin_field("premium", "PREMIUM ($)", 100000, 1000, 1e9, 0, "$"),
            spin_field("annual_payout_rate", "ANNUAL PAYOUT RATE (%)", 5.0, 0.1, 20, 2, "", "%", true),
            spin_field("payout_years", "PAYOUT TERM (years)", 20, 5, 50, 0),
            spin_field("guaranteed_rate", "GUARANTEED RATE (%)", 4.0, 0, 15, 2, "", "%", true),
        };
    if (id == "variable-annuities")
        return {
            text_field("name", "ANNUITY NAME", "Variable Annuity"),
            spin_field("premium", "PREMIUM ($)", 100000, 1000, 1e9, 0, "$"),
            spin_field("me_fee", "M&E FEE (%)", 1.25, 0, 5, 2, "", "%", true),
            spin_field("investment_fee", "INVESTMENT FEE (%)", 0.75, 0, 5, 2, "", "%", true),
            spin_field("admin_fee", "ADMIN FEE (%)", 0.15, 0, 3, 2, "", "%", true),
            spin_field("surrender_period", "SURRENDER PERIOD (yrs)", 7, 0, 20, 0),
        };
    if (id == "eia")
        return {
            text_field("name", "PRODUCT NAME", "EIA Product"),
            spin_field("index_return", "INDEX RETURN (%)", 10.0, -50, 100, 2, "", "%", true),
            spin_field("participation_rate", "PARTICIPATION (%)", 80.0, 10, 100, 1, "", "%", true),
            spin_field("cap_rate", "CAP RATE (%)", 6.0, 0, 50, 2, "", "%", true),
            spin_field("floor_rate", "FLOOR RATE (%)", 0.0, -10, 10, 2, "", "%", true),
        };
    if (id == "inflation-annuity")
        return {
            text_field("name", "ANNUITY NAME", "Inflation-Indexed Annuity"),
            spin_field("real_payout_rate", "REAL PAYOUT RATE (%)", 4.0, 0, 15, 2, "", "%", true),
            spin_field("inflation_rate", "ASSUMED INFLATION (%)", 3.0, 0, 15, 2, "", "%", true),
            spin_field("payout_years", "PAYOUT TERM (years)", 20, 5, 50, 0),
            spin_field("fixed_payout_rate", "FIXED ALT RATE (%)", 5.5, 0, 15, 2, "", "%", true),
        };
    if (id == "structured-products")
        return {
            text_field("name", "PRODUCT NAME", "Principal Protected Note"),
            spin_field("principal", "PRINCIPAL ($)", 50000, 1000, 1e9, 0, "$"),
            spin_field("participation_rate", "PARTICIPATION (%)", 80.0, 10, 100, 1, "", "%", true),
            spin_field("cap_rate", "UPSIDE CAP (%)", 20.0, 0, 100, 1, "", "%", true),
            spin_field("maturity_years", "MATURITY (years)", 5, 1, 30, 0),
        };
    if (id == "leveraged-funds")
        return {
            text_field("name", "FUND NAME", "3x S&P 500 ETF"),
            spin_field("leverage_ratio", "LEVERAGE RATIO", 3.0, 2.0, 4.0, 1),
        };
    if (id == "tips")
        return {
            text_field("name", "SECURITY NAME", "TIPS 2031"),
            spin_field("face_value", "FACE VALUE ($)", 10000, 1000, 1e9, 0, "$"),
            spin_field("current_market_value", "MARKET PRICE ($)", 10200, 1000, 1e9, 2, "$"),
            spin_field("coupon_rate", "REAL COUPON (%)", 2.5, 0, 10, 2, "", "%", true),
            spin_field("maturity_years", "MATURITY (years)", 7, 1, 30, 1),
        };
    if (id == "ibonds")
        return {
            text_field("name", "BOND NAME", "I-Bond 2025"),
            spin_field("purchase_price", "PURCHASE PRICE ($)", 10000, 25, 10000, 0, "$"),
            spin_field("fixed_rate", "FIXED RATE (%)", 0.4, 0, 5, 2, "", "%", true),
            spin_field("inflation_rate", "CPI-U INFLATION (%)", 3.4, 0, 20, 2, "", "%", true),
        };
    if (id == "stable-value")
        return {
            text_field("name", "FUND NAME", "Stable Value Fund"),
            spin_field("book_value", "BOOK VALUE ($)", 100000, 1000, 1e9, 0, "$"),
            spin_field("market_value", "MARKET VALUE ($)", 98000, 1000, 1e9, 0, "$"),
            spin_field("crediting_rate", "CREDITING RATE (%)", 3.2, 0, 15, 2, "", "%", true),
        };
    if (id == "covered-calls")
        return {
            text_field("name", "POSITION NAME", "AAPL Covered Call"),
            spin_field("stock_price", "STOCK PRICE ($)", 150, 1, 1e6, 2, "$"),
            spin_field("strike_price", "STRIKE PRICE ($)", 160, 1, 1e6, 2, "$"),
            spin_field("premium", "PREMIUM / SHARE ($)", 5, 0, 1e4, 2, "$"),
            spin_field("shares", "SHARES", 100, 1, 1e6, 0),
            spin_field("cost_basis", "COST BASIS ($)", 125, 1, 1e6, 2, "$"),
            spin_field("holding_period_days", "HOLDING DAYS", 400, 1, 3650, 0),
        };
    if (id == "sri")
        return {
            text_field("name", "FUND NAME", "ESG Equity Fund"),
            spin_field("sri_return", "SRI RETURN (%)", 9.0, -30, 100, 2, "", "%", true),
            spin_field("benchmark_return", "BENCHMARK RETURN (%)", 10.0, -30, 100, 2, "", "%", true),
            spin_field("expense_ratio", "EXPENSE RATIO (%)", 0.8, 0, 5, 2, "", "%", true),
            spin_field("time_period_years", "TIME PERIOD (years)", 10, 1, 50, 0),
        };
    if (id == "asset-location")
        return {
            text_field("name", "ASSET NAME", "REITs / High-Yield"),
            combo_field("asset_class", "ASSET CLASS",
                        "reits|high_yield_bonds|stocks|municipal_bonds|tips|commodities|cash"),
            spin_field("tax_bracket", "TAX BRACKET (%)", 24.0, 10, 45, 1, "", "%", true),
        };
    if (id == "digital-assets")
        return {
            text_field("name", "ASSET NAME", "Bitcoin"),
            spin_field("price", "PRICE ($)", 45000, 0, 1e9, 2, "$"),
            spin_field("market_cap", "MARKET CAP ($B)", 850, 0, 1e6, 1, "$", "B"),
            spin_field("volume_24h", "24H VOLUME ($B)", 25, 0, 1e6, 1, "$", "B"),
            spin_field("circulating_supply", "CIRC. SUPPLY (M)", 19.5, 0, 1e6, 1, "", "M"),
            spin_field("max_supply", "MAX SUPPLY (M)", 21.0, 0, 1e6, 1, "", "M"),
        };
    return {text_field("name", "NAME", "")};
}

// ── Constructor ──────────────────────────────────────────────────────────────

AltInvestmentsScreen::AltInvestmentsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("altScreen");
    setStyleSheet(kStyle);
    categories_ = build_categories();
    setup_ui();
    LOG_INFO("AltInvestments", "Screen constructed");
}

void AltInvestmentsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
}
void AltInvestmentsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

// ── UI Setup ─────────────────────────────────────────────────────────────────


QString AltInvestmentsScreen::format_value(const QJsonValue& v) const {
    if (v.isBool())
        return v.toBool() ? "Yes" : "No";
    if (v.isNull())
        return "\u2014";
    if (v.isString())
        return v.toString();
    if (v.isDouble()) {
        const double d = v.toDouble();
        // Looks like a 0–1 rate/fraction (not an integer)
        if (d > 0.0001 && d < 1.0 && d != static_cast<double>(static_cast<long long>(d)))
            return QString::number(d * 100.0, 'f', 2) + "%";
        if (qAbs(d) >= 1e12)
            return QString::number(d / 1e12, 'f', 2) + "T";
        if (qAbs(d) >= 1e9)
            return QString::number(d / 1e9, 'f', 2) + "B";
        if (qAbs(d) >= 1e6)
            return QString::number(d / 1e6, 'f', 2) + "M";
        if (d == static_cast<double>(static_cast<long long>(d)) && qAbs(d) < 1e15)
            return QString::number(static_cast<long long>(d));
        return QString::number(d, 'f', 4);
    }
    if (v.isArray())
        return QString("[%1 items]").arg(v.toArray().size());
    return "[object]";
}

void AltInvestmentsScreen::display_error(const QString& error) {
    verdict_badge_->setText("ERROR");
    verdict_badge_->setStyleSheet(QString("color:%1; background:rgba(220,38,38,0.15);"
                                          " font-size:11px; font-weight:700; padding:4px 14px;")
                                      .arg(colors::NEGATIVE()));
    verdict_rating_->clear();
    verdict_rec_->setText(error);
    while (metrics_layout_->count() > 0) {
        auto* item = metrics_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    metrics_layout_->addStretch(1);
    LOG_ERROR("AltInvestments", error);
}

void AltInvestmentsScreen::set_loading(bool loading) {
    loading_ = loading;
    analyze_btn_->setEnabled(!loading);
    analyze_btn_->setText(loading ? "ANALYZING..." : "ANALYZE");
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap AltInvestmentsScreen::save_state() const {
    return {
        {"category", active_category_},
        {"analyzer", active_analyzer_},
    };
}

void AltInvestmentsScreen::restore_state(const QVariantMap& state) {
    const int cat = state.value("category", -1).toInt();
    const int ana = state.value("analyzer", -1).toInt();
    if (cat < 0 || cat >= categories_.size())
        return;
    on_category_changed(cat);
    if (ana >= 0 && ana < categories_[cat].analyzers.size())
        on_analyzer_changed(ana);
}

} // namespace fincept::screens
