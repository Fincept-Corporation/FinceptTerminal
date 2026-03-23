#include "screens/alt_investments/AltInvestmentsScreen.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

// ── Stylesheet ──────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

static const QString kStyle = QStringLiteral(
    "#altScreen { background: %1; }"

    "#altHeader { background: %2; border-bottom: 2px solid %3; }"
    "#altHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
    "#altHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
    "#altHeaderBadge { color: %6; font-size: 8px; font-weight: 700; "
    "  background: rgba(22,163,74,0.2); padding: 2px 6px; }"

    "#altLeftPanel { background: %7; border-right: 1px solid %8; }"
    "#altCatBtn { background: transparent; color: %5; border: none; text-align: left; "
    "  font-size: 11px; padding: 8px 12px; border-bottom: 1px solid %8; }"
    "#altCatBtn:hover { color: %4; background: %12; }"
    "#altCatBtn[active=\"true\"] { color: %3; background: rgba(217,119,6,0.1); "
    "  border-left: 3px solid %3; }"

    "#altCenterPanel { background: %1; }"
    "#altCenterTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"

    "#altPanel { background: %7; border: 1px solid %8; }"
    "#altPanelHeader { background: %2; border-bottom: 1px solid %8; }"
    "#altPanelTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
    "#altLabel { color: %5; font-size: 9px; font-weight: 700; "
    "  letter-spacing: 0.5px; background: transparent; }"

    "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 4px 8px; font-size: 11px; }"
    "QLineEdit:focus { border-color: %9; }"
    "QDoubleSpinBox { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 4px 8px; font-size: 11px; }"
    "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; }"
    "QComboBox { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 4px 8px; font-size: 11px; }"
    "QComboBox::drop-down { border: none; width: 16px; }"
    "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
    "  selection-background-color: %3; }"

    "#altAnalyzeBtn { background: %3; color: %1; border: none; padding: 6px 20px; "
    "  font-size: 10px; font-weight: 700; }"
    "#altAnalyzeBtn:hover { background: #FF8800; }"
    "#altAnalyzeBtn:disabled { background: %10; color: %11; }"

    "#altRightPanel { background: %7; border-left: 1px solid %8; }"
    "#altVerdictBadge { font-size: 12px; font-weight: 700; padding: 4px 12px; }"
    "#altVerdictRating { color: %13; font-size: 16px; font-weight: 700; background: transparent; }"
    "#altVerdictRec { color: %4; font-size: 11px; background: transparent; }"
    "QTextEdit { background: %1; color: %13; border: none; font-size: 11px; }"

    "#altStatusBar { background: %2; border-top: 1px solid %8; }"
    "#altStatusText { color: %5; font-size: 9px; background: transparent; }"
    "#altStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

    "QSplitter::handle { background: %8; }"
    "QScrollBar:vertical { background: %1; width: 6px; }"
    "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
)
    .arg(colors::BG_BASE)         // %1
    .arg(colors::BG_RAISED)       // %2
    .arg(colors::AMBER)           // %3
    .arg(colors::TEXT_PRIMARY)    // %4
    .arg(colors::TEXT_SECONDARY)  // %5
    .arg(colors::POSITIVE)        // %6
    .arg(colors::BG_SURFACE)      // %7
    .arg(colors::BORDER_DIM)      // %8
    .arg(colors::BORDER_BRIGHT)   // %9
    .arg(colors::AMBER_DIM)       // %10
    .arg(colors::TEXT_DIM)         // %11
    .arg(colors::BG_HOVER)        // %12
    .arg(colors::CYAN)             // %13
    .arg(colors::NEGATIVE)        // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Category definitions ────────────────────────────────────────────────────

static QList<AltCategory> build_categories() {
    return {
        {"bonds", "Bonds & Fixed Income", "#0088FF", {
            {"high_yield_bond", "High-Yield Bonds"},
            {"emerging_market_bond", "Emerging Market Bonds"},
            {"convertible_bond", "Convertible Bonds"},
            {"preferred_stock", "Preferred Stocks"},
        }},
        {"real_estate", "Real Estate", "#9D4EDD", {
            {"real_estate", "Direct Property"},
            {"reit", "REITs"},
        }},
        {"hedge_funds", "Hedge Funds", "#FFD700", {
            {"hedge_fund", "Long/Short Equity"},
            {"managed_futures", "Managed Futures"},
            {"market_neutral", "Market Neutral"},
        }},
        {"commodities", "Commodities", "#FF8800", {
            {"precious_metals", "Precious Metals"},
            {"natural_resources", "Natural Resources"},
        }},
        {"private_capital", "Private Capital", "#E91E63", {
            {"private_equity", "Private Equity / VC"},
        }},
        {"annuities", "Annuities", "#4CAF50", {
            {"fixed_annuity", "Fixed Annuity"},
            {"variable_annuity", "Variable Annuity"},
            {"equity_indexed_annuity", "Equity-Indexed Annuity"},
            {"inflation_annuity", "Inflation Annuity"},
        }},
        {"structured", "Structured Products", "#00ACC1", {
            {"structured_product", "Structured Notes"},
            {"leveraged_fund", "Leveraged Funds"},
        }},
        {"inflation", "Inflation Protected", "#FFA726", {
            {"tips", "TIPS"},
            {"i_bond", "I-Bonds"},
            {"stable_value", "Stable Value Funds"},
        }},
        {"strategies", "Strategies", "#00E5FF", {
            {"covered_call", "Covered Calls"},
            {"sri_fund", "SRI Funds"},
            {"asset_location", "Asset Location"},
            {"performance", "Performance Analysis"},
            {"risk", "Risk Analysis"},
        }},
        {"crypto", "Digital Assets", "#FF5722", {
            {"digital_asset", "Cryptocurrency"},
        }},
    };
}

// ── Constructor ─────────────────────────────────────────────────────────────

AltInvestmentsScreen::AltInvestmentsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("altScreen");
    setStyleSheet(kStyle);
    categories_ = build_categories();
    setup_ui();
    LOG_INFO("AltInvestments", "Screen constructed");
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void AltInvestmentsScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* left = create_left_panel();
    left->setMinimumWidth(180);
    left->setMaximumWidth(220);

    auto* center = create_center_panel();

    auto* right = create_right_panel();
    right->setMinimumWidth(280);
    right->setMaximumWidth(360);

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({200, 600, 320});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());

    // Initialize first category
    on_category_changed(0);
}

QWidget* AltInvestmentsScreen::create_header() {
    auto* bar = new QWidget;
    bar->setObjectName("altHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    auto* title = new QLabel("ALTERNATIVE INVESTMENTS");
    title->setObjectName("altHeaderTitle");
    auto* sub = new QLabel("27 ANALYZERS ACROSS 10 ASSET CLASSES");
    sub->setObjectName("altHeaderSub");
    title_col->addWidget(title);
    title_col->addWidget(sub);
    hl->addLayout(title_col);
    hl->addStretch(1);

    auto* badge = new QLabel("PYTHON ANALYTICS");
    badge->setObjectName("altHeaderBadge");
    hl->addWidget(badge);

    return bar;
}

QWidget* AltInvestmentsScreen::create_left_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("altLeftPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* title = new QLabel("ASSET CLASSES");
    title->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                  "letter-spacing: 0.5px; background: transparent; "
                                  "padding: 8px 12px; border-bottom: 1px solid %2;")
                             .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM));
    vl->addWidget(title);

    for (int i = 0; i < categories_.size(); ++i) {
        auto* btn = new QPushButton(categories_[i].name);
        btn->setObjectName("altCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_changed(i); });
        vl->addWidget(btn);
        cat_btns_.append(btn);
    }

    vl->addStretch(1);
    return panel;
}

QWidget* AltInvestmentsScreen::create_center_panel() {
    auto* scroll = new QScrollArea;
    scroll->setObjectName("altCenterPanel");
    scroll->setWidgetResizable(true);

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Analyzer selector
    auto* top_row = new QHBoxLayout;
    center_title_ = new QLabel("BONDS & FIXED INCOME");
    center_title_->setObjectName("altCenterTitle");
    top_row->addWidget(center_title_);
    top_row->addStretch(1);

    analyzer_combo_ = new QComboBox;
    analyzer_combo_->setFixedWidth(200);
    connect(analyzer_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AltInvestmentsScreen::on_analyzer_changed);
    top_row->addWidget(analyzer_combo_);
    vl->addLayout(top_row);

    // Input form panel
    auto* form_panel = new QWidget;
    form_panel->setObjectName("altPanel");
    auto* fpvl = new QVBoxLayout(form_panel);
    fpvl->setContentsMargins(0, 0, 0, 0);
    fpvl->setSpacing(0);

    auto* fhdr = new QWidget;
    fhdr->setObjectName("altPanelHeader");
    fhdr->setFixedHeight(34);
    auto* fhl = new QHBoxLayout(fhdr);
    fhl->setContentsMargins(12, 0, 12, 0);
    auto* ft = new QLabel("INPUT PARAMETERS");
    ft->setObjectName("altPanelTitle");
    fhl->addWidget(ft);
    fhl->addStretch(1);
    fpvl->addWidget(fhdr);

    auto* form_body = new QWidget;
    auto* fbl = new QVBoxLayout(form_body);
    fbl->setContentsMargins(12, 12, 12, 12);
    fbl->setSpacing(10);

    // Common inputs
    auto make_row = [&](const QString& label, QWidget* w) {
        auto* row = new QWidget;
        auto* rl = new QVBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(3);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("altLabel");
        rl->addWidget(lbl);
        rl->addWidget(w);
        return row;
    };

    auto make_spin = [](double val, double min_v, double max_v, int dec, const QString& prefix = "",
                        const QString& suffix = "") {
        auto* sp = new QDoubleSpinBox;
        sp->setRange(min_v, max_v);
        sp->setValue(val);
        sp->setDecimals(dec);
        sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
        if (!prefix.isEmpty()) sp->setPrefix(prefix);
        if (!suffix.isEmpty()) sp->setSuffix(suffix);
        return sp;
    };

    input_name_ = new QLineEdit("Sample Investment");
    fbl->addWidget(make_row("INVESTMENT NAME", input_name_));

    input_type_ = new QComboBox;
    fbl->addWidget(make_row("TYPE / SUBCLASS", input_type_));

    // Value fields (generic, labels updated per analyzer)
    auto* grid = new QWidget;
    auto* gl = new QHBoxLayout(grid);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(8);

    input_value1_ = make_spin(100000, 0, 1e12, 2, "$");
    input_value2_ = make_spin(5.0, -100, 100, 2, "", "%");
    input_value3_ = make_spin(10, 0, 100, 1);

    auto* v1w = make_row("VALUE / PRICE", input_value1_);
    auto* v2w = make_row("RATE / YIELD (%)", input_value2_);
    auto* v3w = make_row("TERM (years)", input_value3_);

    gl->addWidget(v1w, 1);
    gl->addWidget(v2w, 1);
    gl->addWidget(v3w, 1);
    fbl->addWidget(grid);

    auto* grid2 = new QWidget;
    auto* gl2 = new QHBoxLayout(grid2);
    gl2->setContentsMargins(0, 0, 0, 0);
    gl2->setSpacing(8);

    input_value4_ = make_spin(0, -100, 100, 2, "", "%");
    input_value5_ = make_spin(0, 0, 1e12, 2, "$");
    input_value6_ = make_spin(0, -100, 100, 4);

    auto* v4w = make_row("FEES / COSTS (%)", input_value4_);
    auto* v5w = make_row("CURRENT VALUE", input_value5_);
    auto* v6w = make_row("ADDITIONAL PARAM", input_value6_);

    gl2->addWidget(v4w, 1);
    gl2->addWidget(v5w, 1);
    gl2->addWidget(v6w, 1);
    fbl->addWidget(grid2);

    // Analyze button
    analyze_btn_ = new QPushButton("ANALYZE INVESTMENT");
    analyze_btn_->setObjectName("altAnalyzeBtn");
    analyze_btn_->setCursor(Qt::PointingHandCursor);
    analyze_btn_->setFixedHeight(34);
    connect(analyze_btn_, &QPushButton::clicked, this, &AltInvestmentsScreen::on_analyze);
    fbl->addWidget(analyze_btn_);

    fpvl->addWidget(form_body);
    vl->addWidget(form_panel);

    vl->addStretch(1);
    scroll->setWidget(content);
    return scroll;
}

QWidget* AltInvestmentsScreen::create_right_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("altRightPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* title = new QLabel("ANALYSIS VERDICT");
    title->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                  "letter-spacing: 0.5px; background: transparent; "
                                  "padding: 8px 12px; border-bottom: 1px solid %2;")
                             .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM));
    vl->addWidget(title);

    auto* verdict_area = new QWidget;
    auto* val = new QVBoxLayout(verdict_area);
    val->setContentsMargins(12, 12, 12, 12);
    val->setSpacing(10);

    // Badge
    verdict_badge_ = new QLabel("AWAITING ANALYSIS");
    verdict_badge_->setObjectName("altVerdictBadge");
    verdict_badge_->setAlignment(Qt::AlignCenter);
    verdict_badge_->setStyleSheet(QString("color: %1; background: %2; "
                                           "font-size: 12px; font-weight: 700; padding: 4px 12px;")
                                      .arg(colors::TEXT_DIM, colors::BG_RAISED));
    val->addWidget(verdict_badge_);

    // Rating
    verdict_rating_ = new QLabel;
    verdict_rating_->setObjectName("altVerdictRating");
    verdict_rating_->setAlignment(Qt::AlignCenter);
    val->addWidget(verdict_rating_);

    // Recommendation
    verdict_recommendation_ = new QLabel;
    verdict_recommendation_->setObjectName("altVerdictRec");
    verdict_recommendation_->setWordWrap(true);
    val->addWidget(verdict_recommendation_);

    // Detailed output
    verdict_details_ = new QTextEdit;
    verdict_details_->setReadOnly(true);
    verdict_details_->setPlaceholderText("Run an analysis to see detailed results...");
    val->addWidget(verdict_details_, 1);

    vl->addWidget(verdict_area, 1);
    return panel;
}

QWidget* AltInvestmentsScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("altStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("ALTERNATIVE INVESTMENTS");
    left->setObjectName("altStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_category_ = new QLabel("CATEGORY: BONDS & FIXED INCOME");
    status_category_->setObjectName("altStatusText");
    hl->addWidget(status_category_);

    hl->addSpacing(12);
    status_analyzer_ = new QLabel;
    status_analyzer_->setObjectName("altStatusHighlight");
    hl->addWidget(status_analyzer_);

    return bar;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AltInvestmentsScreen::on_category_changed(int index) {
    if (index < 0 || index >= categories_.size()) return;
    active_category_ = index;

    for (int i = 0; i < cat_btns_.size(); ++i) {
        cat_btns_[i]->setProperty("active", i == index);
        cat_btns_[i]->style()->unpolish(cat_btns_[i]);
        cat_btns_[i]->style()->polish(cat_btns_[i]);
    }

    // Update analyzer combo
    analyzer_combo_->clear();
    for (const auto& a : categories_[index].analyzers) {
        analyzer_combo_->addItem(a.name);
    }

    center_title_->setText(categories_[index].name.toUpper());
    status_category_->setText("CATEGORY: " + categories_[index].name.toUpper());

    // Update type dropdown based on category
    input_type_->clear();
    if (index == 0) { // Bonds
        input_type_->addItems({"BBB", "BB", "B", "CCC"});
    } else if (index == 1) { // Real Estate
        input_type_->addItems({"office", "retail", "industrial", "multifamily", "hotel"});
    } else if (index == 2) { // Hedge Funds
        input_type_->addItems({"equity_long_short", "event_driven", "global_macro", "relative_value"});
    } else if (index == 3) { // Commodities
        input_type_->addItems({"gold", "silver", "platinum", "palladium"});
    } else if (index == 5) { // Annuities
        input_type_->addItems({"fixed", "variable", "equity_indexed", "inflation"});
    } else if (index == 8) { // Strategies
        input_type_->addItems({"covered_call", "sri", "asset_location", "performance", "risk"});
    } else {
        input_type_->addItems({"default"});
    }

    if (analyzer_combo_->count() > 0) on_analyzer_changed(0);

    LOG_INFO("AltInvestments", "Category: " + categories_[index].name);
}

void AltInvestmentsScreen::on_analyzer_changed(int index) {
    if (index < 0 || index >= categories_[active_category_].analyzers.size()) return;
    active_analyzer_ = index;
    status_analyzer_->setText(categories_[active_category_].analyzers[index].name);
}

void AltInvestmentsScreen::on_analyze() {
    if (loading_) return;

    const auto& analyzer = categories_[active_category_].analyzers[active_analyzer_];

    QJsonObject data;
    data["name"] = input_name_->text();
    data["type"] = input_type_->currentText();
    data["value"] = input_value1_->value();
    data["rate"] = input_value2_->value() / 100.0;
    data["term"] = input_value3_->value();
    data["fee"] = input_value4_->value() / 100.0;
    data["current_value"] = input_value5_->value();
    data["additional"] = input_value6_->value();

    run_analysis(analyzer.id, data);
}

// ── Python integration ──────────────────────────────────────────────────────

void AltInvestmentsScreen::run_analysis(const QString& command, const QJsonObject& data) {
    set_loading(true);
    verdict_badge_->setText("ANALYZING...");
    verdict_badge_->setStyleSheet(QString("color: %1; background: rgba(217,119,6,0.15); "
                                           "font-size: 12px; font-weight: 700; padding: 4px 12px;")
                                      .arg(colors::AMBER));

    QString data_json = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));

    QPointer<AltInvestmentsScreen> self = this;

    python::PythonRunner::instance().run(
        "Analytics/alternateInvestment/cli.py",
        {command, "--data", data_json},
        [self, command](const python::PythonResult& result) {
            if (!self) return;
            self->set_loading(false);

            if (!result.success) {
                self->display_error(result.error.isEmpty() ? "Analysis failed" : result.error);
                return;
            }

            QString json_str = python::extract_json(result.output);
            if (json_str.isEmpty()) {
                self->display_error("No results from analysis");
                return;
            }

            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull() || !doc.isObject()) {
                self->display_error("Invalid response format");
                return;
            }

            auto obj = doc.object();
            if (obj.contains("error")) {
                self->display_error(obj["error"].toString());
                return;
            }

            self->display_verdict(obj);
            LOG_INFO("AltInvestments", "Analysis complete: " + command);
        });
}

// ── Display ─────────────────────────────────────────────────────────────────

void AltInvestmentsScreen::display_verdict(const QJsonObject& result) {
    auto metrics = result.contains("metrics") ? result["metrics"].toObject() : result;

    // Verdict badge
    QString category = metrics.value("analysis_category").toString("MIXED");
    QString badge_color;
    QString badge_bg;

    if (category.contains("GOOD", Qt::CaseInsensitive)) {
        badge_color = colors::POSITIVE;
        badge_bg = "rgba(22,163,74,0.15)";
    } else if (category.contains("BAD", Qt::CaseInsensitive) || category.contains("UGLY", Qt::CaseInsensitive)) {
        badge_color = colors::NEGATIVE;
        badge_bg = "rgba(220,38,38,0.15)";
    } else if (category.contains("FLAWED", Qt::CaseInsensitive)) {
        badge_color = "#FFD700";
        badge_bg = "rgba(255,215,0,0.15)";
    } else {
        badge_color = colors::CYAN;
        badge_bg = "rgba(8,145,178,0.15)";
    }

    verdict_badge_->setText(category.toUpper());
    verdict_badge_->setStyleSheet(QString("color: %1; background: %2; "
                                           "font-size: 12px; font-weight: 700; padding: 4px 12px;")
                                      .arg(badge_color, badge_bg));

    // Rating
    QString rating = metrics.value("rating").toString();
    verdict_rating_->setText(rating.isEmpty() ? "" : rating);

    // Recommendation
    QString rec = metrics.value("analysis_recommendation").toString();
    verdict_recommendation_->setText(rec);

    // Detailed output
    QStringList details;

    // Key problems
    auto problems = metrics["key_problems"].toArray();
    if (!problems.isEmpty()) {
        details << "KEY FINDINGS:";
        for (int i = 0; i < problems.size(); ++i) {
            details << QString("  %1. %2").arg(i + 1).arg(problems[i].toString());
        }
        details << "";
    }

    // When acceptable
    QString when = metrics.value("when_acceptable").toString();
    if (!when.isEmpty()) {
        details << "WHEN ACCEPTABLE:";
        details << "  " + when;
        details << "";
    }

    // All numeric metrics
    details << "METRICS:";
    for (auto it = metrics.begin(); it != metrics.end(); ++it) {
        if (it.value().isDouble()) {
            details << QString("  %1: %2").arg(it.key()).arg(it.value().toDouble(), 0, 'f', 4);
        }
    }

    verdict_details_->setPlainText(details.join("\n"));
}

void AltInvestmentsScreen::display_error(const QString& error) {
    verdict_badge_->setText("ERROR");
    verdict_badge_->setStyleSheet(QString("color: %1; background: rgba(220,38,38,0.15); "
                                           "font-size: 12px; font-weight: 700; padding: 4px 12px;")
                                      .arg(colors::NEGATIVE));
    verdict_rating_->clear();
    verdict_recommendation_->setText(error);
    verdict_details_->clear();
    LOG_ERROR("AltInvestments", error);
}

void AltInvestmentsScreen::set_loading(bool loading) {
    loading_ = loading;
    analyze_btn_->setEnabled(!loading);
    analyze_btn_->setText(loading ? "ANALYZING..." : "ANALYZE INVESTMENT");
}

} // namespace fincept::screens
