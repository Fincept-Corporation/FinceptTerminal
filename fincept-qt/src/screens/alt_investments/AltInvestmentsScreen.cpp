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

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QSet>
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
                   "#altHeaderTitle { color: %4; font-size: 15px; font-weight: 700; background: transparent; }"
                   "#altHeaderSub   { color: %5; font-size: 11px; letter-spacing: 0.8px; background: transparent; }"
                   "#altHeaderBadge { color: %6; font-size: 10px; font-weight: 700; letter-spacing: 0.5px;"
                   "  background: rgba(22,163,74,0.18); border: 1px solid rgba(22,163,74,0.4); padding: 2px 8px; }"

                   "#altLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#altLeftTitle  { color: %5; font-size: 11px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: %2; padding: 8px 12px; border-bottom: 1px solid %8; }"
                   "#altCatBtn { background: transparent; color: %5; border: none; border-bottom: 1px solid %8;"
                   "  text-align: left; font-size: 13px; padding: 9px 14px; }"
                   "#altCatBtn:hover { color: %4; background: %12; }"
                   "#altCatBtn[active=\"true\"] { color: %3; font-weight: 700;"
                   "  background: rgba(217,119,6,0.08); border-left: 3px solid %3; padding-left: 11px; }"

                   "#altCenterPanel { background: %1; }"
                   "#altCenterTitleBar { background: %2; border-bottom: 1px solid %8; }"
                   "#altCenterTitle { color: %4; font-size: 14px; font-weight: 700; background: transparent; }"
                   "#altCenterDesc  { color: %5; font-size: 12px; background: transparent; }"
                   "#altComboLabel  { color: %5; font-size: 11px; font-weight: 700;"
                   "  letter-spacing: 0.5px; background: transparent; }"

                   "#altFormPanel  { background: %7; border: 1px solid %8; }"
                   "#altFormHeader { background: %2; border-bottom: 1px solid %8; padding: 0 12px; }"
                   "#altFormTitle  { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#altFieldLabel { color: %5; font-size: 11px; font-weight: 700;"
                   "  letter-spacing: 0.5px; background: transparent; }"

                   "QLineEdit { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: 13px; }"
                   "QLineEdit:focus { border-color: %9; }"
                   "QDoubleSpinBox { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: 13px; }"
                   "QDoubleSpinBox:focus { border-color: %9; }"
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8;"
                   "  padding: 5px 8px; font-size: 13px; }"
                   "QComboBox::drop-down { border: none; width: 18px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8;"
                   "  selection-background-color: %3; selection-color: %1; outline: none; }"

                   "#altAnalyzeBtn { background: %3; color: %1; border: none; padding: 7px 20px;"
                   "  font-size: 12px; font-weight: 700; letter-spacing: 0.5px; }"
                   "#altAnalyzeBtn:hover    { background: #FF8800; }"
                   "#altAnalyzeBtn:disabled { background: %10; color: %11; }"

                   "#altRightPanel { background: %7; border-left: 1px solid %8; }"
                   "#altRightTitle { color: %5; font-size: 11px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: %2; padding: 8px 12px; border-bottom: 1px solid %8; }"
                   "#altVerdictBadge  { font-size: 13px; font-weight: 700; padding: 4px 14px;"
                   "  letter-spacing: 0.5px; }"
                   "#altVerdictRating { color: %13; font-size: 16px; font-weight: 700; background: transparent; }"
                   "#altVerdictRec    { color: %4; font-size: 12px; background: transparent; }"

                   "#altMetricRow     { background: transparent; border-bottom: 1px solid %8; }"
                   "#altMetricKey     { color: %5; font-size: 12px; background: transparent; }"
                   "#altMetricVal     { color: %13; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#altMetricSection { color: %3; font-size: 11px; font-weight: 700; letter-spacing: 0.8px;"
                   "  background: %2; padding: 4px 10px; border-top: 1px solid %8; }"
                   "#altMetricBullet    { color: %5;  font-size: 12px; background: transparent; }"
                   "#altMetricBulletVal { color: %4;  font-size: 12px; background: transparent; }"
                   "#altMetricWarn      { color: #FFD700; font-size: 12px; background: transparent; }"
                   "#altMetricGood      { color: %6;  font-size: 12px; background: transparent; }"
                   "#altMetricBad       { color: %14; font-size: 12px; background: transparent; }"

                   "#altStatusBar  { background: %2; border-top: 1px solid %8; }"
                   "#altStatusText { color: %5; font-size: 11px; background: transparent; }"
                   "#altStatusHigh { color: %13; font-size: 11px; background: transparent; }"

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

// ── Category definitions ─────────────────────────────────────────────────────

// Translation helper for the data-driven category/analyzer display strings.
// (These tables live in free static functions, so QObject::tr is unavailable.)
static QString alt_tr(const char* s) {
    return QCoreApplication::translate("AltInvestmentsScreen", s);
}

static QList<AltCategory> build_categories() {
    return {
        {"bonds",
         alt_tr("Bonds & Fixed Income"),
         "#3B82F6",
         "B",
         {
             {"high-yield", alt_tr("High-Yield Bonds"),
              alt_tr("Spread analysis, default probability, equity-like behavior")},
             {"em-bonds", alt_tr("Emerging Market Bonds"),
              alt_tr("Yield spread, sovereign default risk, currency risk")},
             {"convertible-bonds", alt_tr("Convertible Bonds"),
              alt_tr("Conversion premium, bond floor, upside participation")},
             {"preferred-stocks", alt_tr("Preferred Stocks"),
              alt_tr("Current yield, call risk, dividend safety analysis")},
         }},
        {"real_estate",
         alt_tr("Real Estate"),
         "#8B5CF6",
         "RE",
         {
             {"real-estate", alt_tr("Direct Property / REIT"),
              alt_tr("NOI, cap rate, DCF valuation, financing analysis")},
         }},
        {"hedge_funds",
         alt_tr("Hedge Funds"),
         "#EAB308",
         "HF",
         {
             {"hedge-funds", alt_tr("Hedge Fund (Monthly Returns)"),
              alt_tr("CAGR, volatility, Sharpe, drawdown — gross and net of 2/20 fees")},
             {"managed-futures", alt_tr("Managed Futures / CTA"), alt_tr("Fee drag of 2/20 on a gross return")},
             {"market-neutral", alt_tr("Market Neutral"), alt_tr("Gross/net exposure and leverage risk")},
         }},
        {"commodities",
         alt_tr("Commodities"),
         "#F97316",
         "C",
         {
             {"natural-resources", alt_tr("Natural Resources"),
              alt_tr("Futures basis: contango / backwardation, annualised basis")},
         }},
        {"private_capital",
         alt_tr("Private Capital"),
         "#EC4899",
         "PE",
         {
             {"private-capital", alt_tr("Private Equity / Venture Capital"),
              alt_tr("IRR, TVPI, DPI, RVPI from your cash flows and NAV")},
         }},
        {"annuities",
         alt_tr("Annuities"),
         "#22C55E",
         "AN",
         {
             {"annuities", alt_tr("Fixed Annuity"), alt_tr("Payout analysis, breakeven years, inflation erosion")},
             {"variable-annuities", alt_tr("Variable Annuity"),
              alt_tr("Total fee drag, tax deferral myth, vs. alternatives")},
             {"eia", alt_tr("Equity-Indexed Annuity"),
              alt_tr("Crediting rate, upside cap limitations, surrender charges")},
             {"inflation-annuity", alt_tr("Inflation-Indexed Annuity"),
              alt_tr("vs. fixed annuity, vs. TIPS ladder, longevity break-even")},
         }},
        {"structured",
         alt_tr("Structured Products"),
         "#06B6D4",
         "SP",
         {
             {"structured-products", alt_tr("Structured Notes"),
              alt_tr("Qualitative verdict — complexity, hidden costs, issuer credit risk")},
             {"leveraged-funds", alt_tr("Leveraged ETFs (2x/3x)"),
              alt_tr("Volatility decay, path-dependency, holding cost")},
         }},
        {"inflation",
         alt_tr("Inflation Protected"),
         "#FB923C",
         "IP",
         {
             {"tips", alt_tr("TIPS — Inflation-Linked Treasuries"),
              alt_tr("Real yield, inflation scenarios, tax efficiency")},
             {"ibonds", alt_tr("I-Bonds — Series I Savings Bonds"),
              alt_tr("Composite rate, early redemption penalty, vs. TIPS")},
             {"stable-value", alt_tr("Stable Value Funds"),
              alt_tr("Market-to-book ratio, crediting rate, wrap contract risk")},
         }},
        {"strategies",
         alt_tr("Strategies & Analysis"),
         "#67E8F9",
         "ST",
         {
             {"covered-calls", alt_tr("Covered Call Strategy"),
              alt_tr("Tax consequences, opportunity cost, premium income")},
             {"sri", alt_tr("SRI / ESG Funds"), alt_tr("Performance vs. benchmark, screening costs, approaches")},
             {"asset-location", alt_tr("Tax-Efficient Asset Location"),
              alt_tr("Optimal account placement, muni bonds, annual tax drag")},
         }},
        {"crypto",
         alt_tr("Digital Assets"),
         "#F87171",
         "DA",
         {
             {"digital-assets", alt_tr("Cryptocurrency"),
              alt_tr("Implied price, supply inflation, volume / market cap")},
         }},
    };
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
        header_badge_->setText(tr("PYTHON ANALYTICS ENGINE"));
    if (left_title_)
        left_title_->setText(tr("ASSET CLASSES"));
    if (combo_label_)
        combo_label_->setText(tr("ANALYZER"));
    if (form_title_)
        form_title_->setText(tr("INPUT PARAMETERS"));
    if (right_title_)
        right_title_->setText(tr("ANALYSIS RESULTS"));
    if (status_left_)
        status_left_->setText(tr("ALTERNATIVE INVESTMENTS"));
    if (analyze_btn_ && !loading_)
        analyze_btn_->setText(tr("ANALYZE"));

    // Rebuild the data-driven category/analyzer labels (they were translated at
    // construction); re-apply to the left buttons + combo + center via the
    // normal selection path so the active category/analyzer text refreshes too.
    categories_ = build_categories();
    for (int i = 0; i < cat_btns_.size() && i < categories_.size(); ++i)
        cat_btns_[i]->setText(categories_[i].name);
    if (active_category_ >= 0 && active_category_ < categories_.size())
        on_category_changed(active_category_);
}

// ── UI Setup ─────────────────────────────────────────────────────────────────

// Unit of a metric, decided by its NAME. The old rule — "any value strictly
// between 0 and 1 is a percentage" — showed a 0.25-year expiry as 25%, a 0.45
// Sharpe as 45% and a beta of 0.8 as 80%, while a 1.27 participation rate in the
// same list stayed 1.2667. The analyzers report rates as fractions.
enum class AltUnit { Percent, Bps, Plain };

static AltUnit alt_unit_for_key(const QString& key) {
    const QStringList tokens = key.toLower().split(QLatin1Char('_'), Qt::SkipEmptyParts);
    if (tokens.isEmpty())
        return AltUnit::Plain;
    if (tokens.contains(QStringLiteral("bps")) || key.contains(QLatin1String("basis_points")))
        return AltUnit::Bps;
    static const QSet<QString> kPercent = {
        "rate",    "pct",      "percent",       "percentage", "yield",    "return",    "returns",
        "spread",  "probability", "prob",       "cagr",       "drag",     "margin",    "irr",
        "volatility", "drawdown", "capture",    "captured",   "participation", "loss", "lost",
        "increase", "inflation", "growth",      "premium"};
    static const QSet<QString> kPlain = {"ratio",    "sharpe",   "sortino", "calmar", "beta",  "correlation",
                                         "multiple", "moic",     "tvpi",    "dpi",    "rvpi",  "years",
                                         "year",     "age",      "months",  "days",   "count", "score",
                                         "duration", "leverage", "exposure", "shares", "periods"};
    static const QSet<QString> kMoney = {"value",  "price",   "amount", "income",   "dollar",  "dollars",
                                         "wealth", "paid",    "payout", "payouts",  "nav",     "principal",
                                         "fees",   "charge",  "noi",    "invested", "capital", "commitment",
                                         "gain",   "adjustment"};
    // A rate-like LAST word wins ("annual_payout_rate", "upside_capture_pct")...
    if (kPercent.contains(tokens.last()))
        return AltUnit::Percent;
    // ...then explicit non-percent words ("leverage_ratio", "conversion_premium_dollar")...
    for (const QString& t : tokens) {
        if (kPlain.contains(t) || kMoney.contains(t))
            return AltUnit::Plain;
    }
    // ...then a rate word anywhere ("spread_over_treasuries", "yield_to_maturity").
    for (const QString& t : tokens) {
        if (kPercent.contains(t))
            return AltUnit::Percent;
    }
    return AltUnit::Plain;
}

QString AltInvestmentsScreen::format_value(const QString& key, const QJsonValue& v) const {
    if (v.isBool())
        return v.toBool() ? tr("Yes") : tr("No");
    if (v.isNull())
        return "\u2014";
    if (v.isString())
        return v.toString();
    if (v.isDouble()) {
        const double d = v.toDouble();
        switch (alt_unit_for_key(key)) {
            case AltUnit::Percent:
                return QLocale().toString(d * 100.0, 'f', 2) + "%";
            case AltUnit::Bps:
                return QLocale().toString(d, 'f', 1) + " bps";
            case AltUnit::Plain:
                break;
        }
        if (d == static_cast<double>(static_cast<long long>(d)) && qAbs(d) < 1e15)
            return QLocale().toString(static_cast<qlonglong>(d));
        return QLocale().toString(d, 'f', qAbs(d) >= 1000.0 ? 2 : 4);
    }
    if (v.isArray())
        return QString("[%1 items]").arg(v.toArray().size());
    return "[object]";
}

void AltInvestmentsScreen::display_error(const QString& error) {
    verdict_badge_->setText(tr("ERROR"));
    verdict_badge_->setStyleSheet(QString("color:%1; background:rgba(220,38,38,0.15);"
                                          " font-size:13px; font-weight:700; padding:4px 14px;")
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
    analyze_btn_->setText(loading ? tr("ANALYZING...") : tr("ANALYZE"));
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
