// src/screens/alt_investments/AltInvestmentsScreen.cpp
//
// Core: alt_style(), the analyzer catalog, ctor / dtor, show / hide events,
// result_unit_for, plain_error, display_error, set_loading, save_state / restore_state.
// Other concerns:
//   - AltInvestmentsScreen_Layout.cpp   — setup_ui + create_* panels
//   - AltInvestmentsScreen_Forms.cpp    — field / method tables, form building, input storage
//   - AltInvestmentsScreen_Analysis.cpp — running the analysis, result display, copy / export
#include "screens/alt_investments/AltInvestmentsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/ResultView.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QStyle>

#include <cmath>

// ── Stylesheet ───────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

// Built on use (not a constant): it reads the live theme tokens and font sizes.
QString alt_style() {
    return
    QString("#altScreen { background: %1; }"

                   "#altHeader { background: %2; border-bottom: 2px solid %3; padding: 0 16px; }"
                   "#altHeaderTitle { color: %4; font-size: " + fincept::ui::fonts::px(1) + "px; font-weight: 700; background: transparent; }"
                   "#altHeaderSub   { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; letter-spacing: 0.8px; background: transparent; }"
                   "#altHeaderBadge { color: %6; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; letter-spacing: 0.5px;"
                   "  background: %15; border: 1px solid %16; padding: 2px 8px; }"

                   "#altModeStrip { background: %2; border-bottom: 1px solid %8; }"
                   "#altModeBtn { background: transparent; color: %5; border: none; border-bottom: 2px solid transparent;"
                   "  padding: 6px 14px; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; letter-spacing: 0.5px; }"
                   "#altModeBtn:hover { color: %4; }"
                   "#altModeBtn:checked { color: %3; border-bottom: 2px solid %3; }"

                   "#altLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#altLeftTitle  { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: %2; padding: 8px 12px; border-bottom: 1px solid %8; }"
                   "#altCatBtn { background: transparent; color: %5; border: none; border-bottom: 1px solid %8;"
                   "  text-align: left; font-size: " + fincept::ui::fonts::px(-1) + "px; padding: 9px 14px; }"
                   "#altCatBtn:hover { color: %4; background: %12; }"
                   "#altCatBtn[active=\"true\"] { color: %3; font-weight: 700;"
                   "  background: %17; border-left: 3px solid %3; padding-left: 11px; }"

                   "#altCenterPanel { background: %1; border: none; }"
                   "#altCenterContent { background: %1; }"
                   "#altCenterTitleBar { background: %2; border-bottom: 1px solid %8; }"
                   "#altCenterTitle { color: %4; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: 700; background: transparent; }"
                   "#altCenterDesc  { color: %5; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; }"
                   "#altComboLabel  { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700;"
                   "  letter-spacing: 0.5px; background: transparent; }"
                   "#altMethodNote  { color: %5; font-size: " + fincept::ui::fonts::px(-2) + "px; background: %10;"
                   "  border-left: 3px solid %13; padding: 6px 8px; }"

                   "#altFormPanel  { background: %7; border: 1px solid %8; }"
                   "#altFormHeader { background: %2; border-bottom: 1px solid %8; padding: 0 12px; }"
                   "#altFormTitle  { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; }"
                   "#altFieldLabel { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700;"
                   "  letter-spacing: 0.5px; background: transparent; }"
                   "#altFormProblems { color: %20; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; }"

                   "QLineEdit { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: " + fincept::ui::fonts::px(-1) + "px; }"
                   "QLineEdit:focus { border-color: %9; }"
                   "QDoubleSpinBox { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: " + fincept::ui::fonts::px(-1) + "px; }"
                   "QDoubleSpinBox:focus { border-color: %9; }"
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: " + fincept::ui::fonts::px(-1) + "px; }"
                   "QComboBox::drop-down { border: none; width: 18px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8;"
                   "  selection-background-color: %3; selection-color: %18; outline: none; }"

                   "#altAnalyzeBtn { background: %3; color: %18; border: none; padding: 7px 20px;"
                   "  font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; letter-spacing: 0.5px; }"
                   "#altAnalyzeBtn:hover    { background: %19; }"
                   "#altAnalyzeBtn:disabled { background: %10; color: %11; }"
                   "#altAnalyzeBtn[running=\"true\"] { background: %10; color: %14; border: 1px solid %14; }"
                   "#altSecondaryBtn { background: transparent; color: %5; border: 1px solid %8; padding: 6px 10px;"
                   "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
                   "#altSecondaryBtn:hover { color: %4; border-color: %9; background: %12; }"
                   "#altSecondaryBtn:disabled { color: %11; }"

                   "#altRightPanel { background: %7; border-left: 1px solid %8; }"
                   "#altRightBar { background: %2; border-bottom: 1px solid %8; }"
                   "#altRightTitle { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: transparent; }"
                   "#altResultTitle { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; }"
                   "#altStaleNote { color: %20; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
                   "#altVerdictBadge  { font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; padding: 4px 14px;"
                   "  letter-spacing: 0.5px; }"
                   "#altVerdictRating { color: %13; font-size: " + fincept::ui::fonts::px(2) + "px; font-weight: 700; background: transparent; }"
                   "#altVerdictRec    { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; }"
                   "#altDivider { background: %8; }"

                   "#altStatusBar  { background: %2; border-top: 1px solid %8; }"
                   "#altStatusText { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
                   "#altStatusHigh { color: %13; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollBar:vertical { background: %1; width: 6px; border: none; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar:horizontal { background: %1; height: 6px; border: none; }"
                   "QScrollBar::handle:horizontal { background: %8; min-width: 20px; }"
                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::SUCCESS())        // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::BG_RAISED())      // %10 (disabled button, note background)
        .arg(colors::TEXT_TERTIARY())  // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::DANGER())         // %14
        .arg(colors::alpha(colors::SUCCESS, 0.18)) // %15
        .arg(colors::alpha(colors::SUCCESS, 0.4))  // %16
        .arg(colors::alpha(colors::AMBER, 0.08))   // %17
        .arg(colors::TEXT_ON_ACCENT())             // %18
        .arg(colors::alpha(colors::AMBER, 0.85))   // %19
        .arg(colors::WARNING())                    // %20
        ;
}
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Category definitions ─────────────────────────────────────────────────────

// Translation helper for the data-driven category/analyzer display strings.
// (These tables live in free static functions, so QObject::tr is unavailable.)
static QString alt_core_tr(const char* s) {
    return QCoreApplication::translate("AltInvestmentsScreen", s);
}

static QList<AltCategory> build_categories() {
    return {
        {"bonds",
         alt_core_tr("Bonds & Fixed Income"),
         colors::chart_color(0).name(), // category accent (theme chart palette)
         "B",
         {
             {"high-yield", alt_core_tr("High-Yield Bonds"),
              alt_core_tr("Yield and credit spread vs the rating's range, default probability by rating")},
             {"em-bonds", alt_core_tr("Emerging Market Bonds"),
              alt_core_tr("Yield spread, sovereign default risk, local-currency risk")},
             {"convertible-bonds", alt_core_tr("Convertible Bonds"),
              alt_core_tr("Conversion premium, bond floor, upside participation")},
             {"preferred-stocks", alt_core_tr("Preferred Stocks"),
              alt_core_tr("Current yield, yield to call and call risk; dividend-safety guide")},
         }},
        {"real_estate",
         alt_core_tr("Real Estate"),
         colors::chart_color(1).name(), // category accent (theme chart palette)
         "RE",
         {
             {"real-estate", alt_core_tr("Direct Property"),
              alt_core_tr("NOI and cap rate, financing (debt service, DSCR, LTV, levered IRR), DCF valuation")},
         }},
        {"hedge_funds",
         alt_core_tr("Hedge Funds"),
         colors::chart_color(2).name(), // category accent (theme chart palette)
         "HF",
         {
             {"hedge-funds", alt_core_tr("Hedge Fund (Monthly Returns)"),
              alt_core_tr("CAGR, volatility, Sharpe, drawdown - gross and net of fees, from your monthly returns")},
             {"managed-futures", alt_core_tr("Managed Futures / CTA"),
              alt_core_tr("Fee drag of management and performance fees over a horizon")},
             {"market-neutral", alt_core_tr("Market Neutral"),
              alt_core_tr("Net and gross exposure, leverage and its risk amplification")},
         }},
        {"commodities",
         alt_core_tr("Commodities"),
         colors::chart_color(3).name(), // category accent (theme chart palette)
         "C",
         {
             {"natural-resources", alt_core_tr("Natural Resources"),
              alt_core_tr("Futures basis: contango / backwardation, annualised basis, roll yield")},
         }},
        {"private_capital",
         alt_core_tr("Private Capital"),
         colors::chart_color(4).name(), // category accent (theme chart palette)
         "PE",
         {
             {"private-capital", alt_core_tr("Private Equity / Venture Capital"),
              alt_core_tr("IRR, TVPI, DPI, RVPI from your dated cash flows and NAV")},
         }},
        {"annuities",
         alt_core_tr("Annuities"),
         colors::chart_color(5).name(), // category accent (theme chart palette)
         "AN",
         {
             {"annuities", alt_core_tr("Fixed Annuity"),
              alt_core_tr("Payouts and their return, inflation erosion, vs a self-managed portfolio")},
             {"variable-annuities", alt_core_tr("Variable Annuity"),
              alt_core_tr("Total fee drag, tax deferral, surrender schedule, vs alternatives")},
             {"eia", alt_core_tr("Equity-Indexed Annuity"),
              alt_core_tr("Credited return, upside limits of cap and participation, surrender charges")},
             {"inflation-annuity", alt_core_tr("Inflation-Indexed Annuity"),
              alt_core_tr("vs fixed annuity, vs TIPS ladder, longevity break-even")},
         }},
        {"structured",
         alt_core_tr("Structured Products"),
         colors::chart_color(6).name(), // category accent (theme chart palette)
         "SP",
         {
             {"structured-products", alt_core_tr("Structured Notes (product guide)"),
              alt_core_tr("Product guide - general reading on structured notes, not an analysis of your inputs")},
             {"leveraged-funds", alt_core_tr("Leveraged ETFs (2x/3x)"),
              alt_core_tr("Volatility decay and the expected drag for your leverage and volatility")},
         }},
        {"inflation",
         alt_core_tr("Inflation Protected"),
         colors::chart_color(7).name(), // category accent (theme chart palette)
         "IP",
         {
             {"tips", alt_core_tr("TIPS - Inflation-Linked Treasuries"),
              alt_core_tr("Real yield, inflation scenarios, tax efficiency")},
             {"ibonds", alt_core_tr("I-Bonds - Series I Savings Bonds"),
              alt_core_tr("Composite rate, early redemption penalty, vs TIPS")},
             {"stable-value", alt_core_tr("Stable Value Funds"),
              alt_core_tr("Market-to-book ratio, crediting rate, suitability")},
         }},
        {"strategies",
         alt_core_tr("Strategies & Analysis"),
         colors::chart_color(8).name(), // category accent (theme chart palette)
         "ST",
         {
             {"covered-calls", alt_core_tr("Covered Call Strategy"),
              alt_core_tr("Tax consequences, opportunity cost, premium income")},
             {"sri", alt_core_tr("SRI / ESG Funds"), alt_core_tr("Performance vs benchmark, screening and expense costs")},
             {"asset-location", alt_core_tr("Tax-Efficient Asset Location"),
              alt_core_tr("Optimal account placement, value added, municipal bond decision")},
         }},
        {"crypto",
         alt_core_tr("Digital Assets"),
         colors::chart_color(9).name(), // category accent (theme chart palette)
         "DA",
         {
             {"digital-assets", alt_core_tr("Cryptocurrency"),
              alt_core_tr("Implied price, supply inflation, volume / market cap")},
         }},
    };
}

// ── Constructor ──────────────────────────────────────────────────────────────

AltInvestmentsScreen::AltInvestmentsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("altScreen");
    setStyleSheet(alt_style());
    categories_ = build_categories();
    setup_ui();
    // The screen's whole look is this one stylesheet, built from the theme tokens of the moment.
    // Rebuild it once the theme or font change has settled (never inside theme_changed: restyling a
    // whole screen there is what froze font changes for seconds); only when its text really changed.
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this, [this]() {
        const QString sheet = alt_style();
        if (sheet != styleSheet())
            setStyleSheet(sheet);
    });
    LOG_DEBUG("AltInvestments", "Screen constructed");
}

AltInvestmentsScreen::~AltInvestmentsScreen() {
    // A running analysis is stopped (its process killed) rather than left to finish for nobody.
    if (cancel_token_)
        cancel_token_->store(true);
    // The pane is destroyed after this object's members: it must not call back into them.
    if (pane_)
        pane_->set_cancel({});
}

void AltInvestmentsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
}
void AltInvestmentsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

// ── Live language switch ─────────────────────────────────────────────────────
void AltInvestmentsScreen::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void AltInvestmentsScreen::retranslateUi() {
    // Fixed chrome labels
    if (header_title_)
        header_title_->setText(tr("ALTERNATIVE INVESTMENTS"));
    if (header_sub_) {
        int analyzer_total = 0;
        for (const auto& cat : categories_)
            analyzer_total += static_cast<int>(cat.analyzers.size());
        header_sub_->setText(tr("%1 ANALYZERS  \xB7  %2 ASSET CLASSES  \xB7  MULTI-ASSET ANALYTICS")
                                 .arg(analyzer_total)
                                 .arg(categories_.size()));
    }
    if (method_label_)
        method_label_->setText(tr("METHOD"));
    if (header_badge_)
        header_badge_->setText(tr("LOCAL PYTHON ANALYTICS"));
    if (left_title_)
        left_title_->setText(tr("ASSET CLASSES"));
    if (combo_label_)
        combo_label_->setText(tr("ANALYZER"));
    if (form_title_)
        form_title_->setText(tr("INPUTS"));
    if (right_title_)
        right_title_->setText(tr("RESULTS"));
    if (status_left_)
        status_left_->setText(tr("ALTERNATIVE INVESTMENTS"));
    if (reset_btn_)
        reset_btn_->setText(tr("RESET TO EXAMPLE"));
    if (copy_btn_)
        copy_btn_->setText(tr("COPY"));
    if (csv_btn_)
        csv_btn_->setText(tr("CSV"));
    if (image_btn_)
        image_btn_->setText(tr("IMAGE"));
    if (analyze_btn_)
        analyze_btn_->setText(loading_ ? tr("CANCEL") : tr("ANALYZE"));

    // Rebuild the data-driven category/analyzer labels (they were translated at
    // construction); re-apply to the left buttons + combo + center via the
    // normal selection path so the active category/analyzer text refreshes too.
    categories_ = build_categories();
    for (int i = 0; i < cat_btns_.size() && i < categories_.size(); ++i)
        cat_btns_[i]->setText(QString(categories_[i].name).replace(QLatin1Char('&'), QLatin1String("&&")));
    if (active_category_ >= 0 && active_category_ < categories_.size()) {
        const int ana = active_analyzer_;
        on_category_changed(active_category_);
        if (ana > 0 && ana < categories_[active_category_].analyzers.size())
            on_analyzer_changed(ana);
    }
}

// ── Result units ─────────────────────────────────────────────────────────────

// Unit of a metric, decided by its NAME — and, between an amount and a rate, by
// magnitude: the analyzers report rates as fractions (0.02 = 2%), so a value
// above 1.5 under a rate-sounding name is an amount ("initial_premium" 100000,
// "inflation_indexed_initial" 4000, "after_tax_premium" 315 used to print as
// 10,000,000.00% / 400,000.00% / 31,500.00%). Fees, costs, credits, cushions and
// "vs" differences below 1 are rates too ("management_fee" 0.02 -> 2.00%).
fincept::ui::result::Unit AltInvestmentsScreen::result_unit_for(const QString& key, double value) {
    using U = fincept::ui::result::Unit;
    // Keys whose words would mislead the rules below (checked first).
    static const QHash<QString, U> kExact = {
        {"loan_to_value", U::Fraction},          {"cash_on_cash_year1", U::Fraction},
        {"annual_debt_service", U::Money},       {"debt_service", U::Money},
        {"equity_required", U::Money},           {"bond_floor", U::Money},
        {"cumulative_distributed", U::Money},    {"annual_dividend", U::Money},
        {"indexed_minus_fixed_over_term", U::Money}, {"gross_value", U::Plain},
        {"net_value", U::Plain},                 {"dscr", U::Plain},
        {"dscr_year1", U::Plain},                {"min_dscr", U::Plain},
        {"gross_rent_multiplier", U::Plain},     {"months", U::Integer},
    };
    const auto exact = kExact.constFind(key);
    if (exact != kExact.constEnd())
        return exact.value();
    const QStringList tokens = key.toLower().split(QLatin1Char('_'), Qt::SkipEmptyParts);
    if (tokens.isEmpty())
        return U::Plain;
    if (tokens.contains(QStringLiteral("bps")) || key.contains(QLatin1String("basis_points")))
        return U::Bps;
    static const QSet<QString> kPercent = {
        "rate",       "pct",        "percent",    "percentage", "yield",    "return",   "returns",
        "spread",     "probability", "prob",      "cagr",       "drag",     "margin",   "irr",
        "volatility", "drawdown",   "capture",    "captured",   "participation", "loss", "lost",
        "increase",   "inflation",  "growth",     "premium",    "decay",    "underperformance",
        "impact",     "excluded",   "participant", "alpha",     "annualized"};
    static const QSet<QString> kSmallRates = {"fee", "fees", "cost", "costs", "credit", "cushion", "given", "vs",
                                              "smoothing", "charge"};
    static const QSet<QString> kMultiple = {"multiple", "moic", "tvpi", "dpi", "rvpi"};
    static const QSet<QString> kPlain = {"ratio",    "sharpe",   "sortino", "calmar", "beta",  "correlation",
                                         "years",    "year",     "age",     "months", "days",  "count",
                                         "score",    "duration", "leverage", "exposure", "shares", "periods",
                                         "holdings"};
    static const QSet<QString> kMoney = {"value",  "price",   "amount", "income",   "dollar",  "dollars",
                                         "wealth", "paid",    "payout", "payouts",  "nav",     "principal",
                                         "noi",    "invested", "capital", "commitment", "gain", "adjustment",
                                         "initial", "payment", "benefit", "withdrawal", "balance", "investment",
                                         "cap",    "proceeds", "advantage", "shortfall", "profit", "basis",
                                         "tax",    "penalty", "cash",    "flows",    "pv",      "bonds"};
    const bool finite = std::isfinite(value);
    const bool amount_sized = finite && std::fabs(value) > 1.5;
    auto any = [&tokens](const QSet<QString>& words) {
        for (const QString& t : tokens)
            if (words.contains(t))
                return true;
        return false;
    };
    if (key.contains(QLatin1String("expense_ratio")) && !amount_sized)
        return U::Fraction; // an expense ratio is a rate (0.008 = 0.80%)
    // A ratio of two quantities ("volume_to_market_cap") — unless it names a rate
    // ("yield_to_maturity", "percent_lost_to_fees").
    if (key.contains(QLatin1String("_to_")) && !any(kPercent) && !any(kSmallRates))
        return U::Plain;
    if (tokens.last() == QLatin1String("year") && finite && value >= 1800 && value <= 2300)
        return U::Year; // "vintage_year" 2020, not "2,020"
    if (any(kMultiple))
        return U::Multiple;
    // An amount under a rate word: no fraction-rate is above 150%.
    if (amount_sized && (any(kPercent) || any(kSmallRates)))
        return any(kMoney) || any(kSmallRates) || tokens.contains(QStringLiteral("premium")) ? U::Money : U::Plain;
    // A rate-like LAST word wins ("annual_payout_rate", "upside_capture_pct")...
    if (kPercent.contains(tokens.last()))
        return U::Fraction;
    // ...and below 150% any rate word does, even beside an amount word
    // ("initial_surrender_charge" 0.10, "penalty_rate_of_gain").
    if (finite && (any(kSmallRates) || any(kPercent)) && !any(kPlain))
        return U::Fraction;
    // ...then explicit non-percent words ("leverage_ratio", "conversion_premium_dollar")...
    if (any(kPlain))
        return U::Plain;
    if (any(kMoney))
        return U::Money;
    // ...then a rate word anywhere ("spread_over_treasuries", "yield_to_maturity").
    if (any(kPercent))
        return U::Fraction;
    return U::Plain;
}

// ── Plain-language errors ────────────────────────────────────────────────────

QString AltInvestmentsScreen::plain_error(const QString& raw_in) {
    const QString raw = raw_in.trimmed();
    if (raw.isEmpty())
        return tr("The analysis failed without saying why. Check the inputs and try again.");
    if (raw == QLatin1String("Cancelled"))
        return tr("Analysis cancelled.");
    static const QRegularExpression kTimeout(QStringLiteral("timed out after (\\d+) s"));
    const auto t = kTimeout.match(raw);
    if (t.hasMatch())
        return tr("The analysis took longer than %1 seconds and was stopped. Press ANALYZE to try again; "
                  "if it keeps happening, restart the terminal.")
            .arg(t.captured(1));
    if (raw.contains(QLatin1String("Python is not set up"), Qt::CaseInsensitive) ||
        raw.contains(QLatin1String("still installing"), Qt::CaseInsensitive) ||
        raw.contains(QLatin1String("Analytics libraries"), Qt::CaseInsensitive))
        return tr("The Python analytics are not ready yet. Finish the Python setup (the bar at the top of the "
                  "window), then press ANALYZE again.\n\n%1")
            .arg(raw);
    static const QRegularExpression kMonths(QStringLiteral("at least 12 monthly returns.*got (\\d+)"));
    const auto mo = kMonths.match(raw);
    if (mo.hasMatch())
        return tr("Enter at least 12 monthly returns, oldest first - %1 usable rows were entered.")
            .arg(mo.captured(1));
    if (raw.contains(QLatin1String("does not match format")) || raw.contains(QLatin1String("is not a date")))
        return tr("A date could not be read. Use YYYY-MM-DD, for example 2024-03-31.\n\n%1").arg(raw);
    if (raw.contains(QLatin1String("division by zero"), Qt::CaseInsensitive) ||
        raw.contains(QLatin1String("DivisionByZero")) || raw.contains(QLatin1String("DivisionUndefined")) ||
        raw.contains(QLatin1String("InvalidOperation")))
        return tr("An input of 0 (or a negative value) made the calculation impossible. Check the inputs that "
                  "must be above 0 - price, income, rates, years.");
    if (raw.startsWith(QLatin1String("Insufficient"), Qt::CaseInsensitive))
        return tr("This method needs more data than the form provides: %1").arg(raw);
    if (raw.startsWith(QLatin1String("No JSON output")) || raw.startsWith(QLatin1String("Invalid JSON")))
        return tr("The analysis did not return a readable result. Try again; details: %1").arg(raw);
    return raw;
}

// ── States ───────────────────────────────────────────────────────────────────

void AltInvestmentsScreen::display_error(const QString& error) {
    verdict_badge_->setText(tr("COULD NOT ANALYZE"));
    verdict_badge_->setStyleSheet(QString("color:%1; background:%2;"
                                          " font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; padding:4px 14px;")
                                      .arg(colors::DANGER(), colors::alpha(colors::DANGER, 0.15)));
    verdict_rating_->clear();
    verdict_rec_->clear();
    last_result_ = QJsonObject();
    if (result_title_)
        result_title_->clear();
    if (result_view_)
        result_view_->show_error(error);
    update_actions();
    LOG_WARN("AltInvestments", error.left(300));
}

void AltInvestmentsScreen::set_loading(bool loading) {
    loading_ = loading;
    // While it runs the button stops it (the analysis can be cancelled at any time).
    analyze_btn_->setText(loading ? tr("CANCEL") : tr("ANALYZE"));
    analyze_btn_->setToolTip(loading ? tr("Stop the running analysis (Esc)") : tr("Run the analyzer (Enter)"));
    analyze_btn_->setProperty("running", loading);
    analyze_btn_->style()->unpolish(analyze_btn_);
    analyze_btn_->style()->polish(analyze_btn_);
    if (reset_btn_)
        reset_btn_->setEnabled(!loading);
    if (auto* esc = findChild<QShortcut*>(QStringLiteral("altEscCancel"), Qt::FindDirectChildrenOnly))
        esc->setEnabled(loading);
    update_actions();
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap AltInvestmentsScreen::save_state() const {
    QVariantMap methods;
    for (auto it = methods_.constBegin(); it != methods_.constEnd(); ++it)
        methods.insert(it.key(), it.value());
    // Each analyzer's inputs as compact JSON text (grids included). A runaway paste is not
    // allowed to bloat the session row: an analyzer above 256 KB keeps its previous state.
    QVariantMap inputs;
    constexpr int kMaxAnalyzerBytes = 256 * 1024;
    for (auto it = inputs_.constBegin(); it != inputs_.constEnd(); ++it) {
        const QByteArray json = QJsonDocument(it.value()).toJson(QJsonDocument::Compact);
        if (json.size() > kMaxAnalyzerBytes)
            continue;
        inputs.insert(it.key(), QString::fromUtf8(json));
    }
    return {
        {"category", active_category_},
        {"analyzer", active_analyzer_},
        {"methods", methods},
        {"inputs", inputs},
        {"mode", mode_id(active_mode_)},
    };
}

void AltInvestmentsScreen::restore_state(const QVariantMap& state) {
    restoring_ = true;
    const QVariantMap methods = state.value("methods").toMap();
    for (auto it = methods.constBegin(); it != methods.constEnd(); ++it)
        methods_.insert(it.key(), it.value().toString());
    const QVariantMap inputs = state.value("inputs").toMap();
    for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
        const QJsonDocument doc = QJsonDocument::fromJson(it.value().toString().toUtf8());
        if (doc.isObject())
            inputs_.insert(it.key(), doc.object()); // merged over the defaults by inputs_for()
    }

    const int cat = state.value("category", -1).toInt();
    const int ana = state.value("analyzer", -1).toInt();
    if (cat >= 0 && cat < categories_.size()) {
        on_category_changed(cat);
        if (ana > 0 && ana < categories_[cat].analyzers.size())
            on_analyzer_changed(ana);
    } else {
        rebuild_form(); // the current analyzer with the restored inputs
    }
    // The workbench mode last used (its panel is built now, as it is the one on screen).
    const int mode = mode_index(state.value("mode").toString());
    if (mode > 0)
        set_mode(mode);
    restoring_ = false;
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

bool AltInvestmentsScreen::open_section(const QString& section, const QString& focus) {
    // The mode strip's own path (set_mode) and the ANALYZERS pickers' (on_category_changed /
    // on_analyzer_changed). Validated before anything changes.
    QString key = section.trimmed().toLower();
    if (key.startsWith(QLatin1String("mode:")))
        key = key.mid(5).trimmed();
    int mode = -1;
    if (!key.isEmpty()) {
        mode = mode_index(key); // 0 for an unknown id: checked against its key
        if (mode_id(mode) != key)
            return false;
    }
    const QString want = focus.trimmed().toLower();
    int cat = -1;
    int ana = -1;
    for (int c = 0; !want.isEmpty() && cat < 0 && c < categories_.size(); ++c) {
        if (categories_[c].id.toLower() == want) {
            cat = c;
            ana = 0;
            break;
        }
        for (int a = 0; a < categories_[c].analyzers.size(); ++a)
            if (categories_[c].analyzers[a].id.toLower() == want) {
                cat = c;
                ana = a;
                break;
            }
    }
    // An analyzer lives in the ANALYZERS mode (0): with another mode named, the focus is ignored.
    if (cat >= 0 && mode <= 0) {
        set_mode(0);
        if (cat != active_category_)
            on_category_changed(cat);
        if (ana != active_analyzer_)
            on_analyzer_changed(ana);
        return true;
    }
    if (mode >= 0)
        set_mode(mode);
    return true;
}

} // namespace fincept::screens
