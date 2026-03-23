#include "screens/quantlib/QuantLibScreen.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
using namespace fincept::ui;

static const QString kStyle = QStringLiteral(
    "#qlScreen { background: %1; }"

    "#qlHeader { background: %2; border-bottom: 2px solid %3; }"
    "#qlHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
    "#qlHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
    "#qlHeaderBadge { color: %6; font-size: 8px; font-weight: 700; "
    "  background: rgba(22,163,74,0.2); padding: 2px 6px; }"

    "#qlSidebar { background: %7; border-right: 1px solid %8; }"
    "QTreeWidget { background: %1; color: %5; border: none; font-size: 11px; }"
    "QTreeWidget::item { padding: 4px 8px; border-bottom: 1px solid %8; }"
    "QTreeWidget::item:hover { color: %4; background: %12; }"
    "QTreeWidget::item:selected { color: %3; background: rgba(217,119,6,0.1); "
    "  border-left: 2px solid %3; }"

    "#qlCenterPanel { background: %1; }"
    "#qlCenterTitle { color: %4; font-size: 13px; font-weight: 700; background: transparent; }"

    "#qlPanel { background: %7; border: 1px solid %8; }"
    "#qlPanelHeader { background: %2; border-bottom: 1px solid %8; }"
    "#qlPanelTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
    "#qlLabel { color: %5; font-size: 9px; font-weight: 700; "
    "  letter-spacing: 0.5px; background: transparent; }"

    "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 4px 8px; font-size: 11px; }"
    "QLineEdit:focus { border-color: %9; }"
    "QComboBox { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 4px 8px; font-size: 11px; }"
    "QComboBox::drop-down { border: none; width: 16px; }"
    "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
    "  selection-background-color: %3; }"

    "#qlExecBtn { background: %3; color: %1; border: none; padding: 6px 20px; "
    "  font-size: 10px; font-weight: 700; }"
    "#qlExecBtn:hover { background: #FF8800; }"
    "#qlExecBtn:disabled { background: %10; color: %11; }"

    "#qlResultPanel { background: %7; border-left: 1px solid %8; }"
    "#qlResultStatus { color: %5; font-size: 9px; background: transparent; }"
    "QTextEdit { background: %1; color: %13; border: none; font-size: 11px; }"
    "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
    "  font-size: 11px; }"
    "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
    "QHeaderView::section { background: %2; color: %5; border: none; "
    "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
    "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

    "#qlStatusBar { background: %2; border-top: 1px solid %8; }"
    "#qlStatusText { color: %5; font-size: 9px; background: transparent; }"
    "#qlStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

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
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Module definitions ──────────────────────────────────────────────────────

static QList<QuantModule> build_modules() {
    return {
        {"core", "Core", 51, {"Types", "Conventions", "AutoDiff", "Distributions", "Math", "Operations", "Legs", "Periods"}},
        {"analysis", "Analysis", 122, {"Fundamentals", "Profitability", "Liquidity", "Efficiency", "Growth", "Leverage", "Valuation Ratios", "DCF Valuation"}},
        {"curves", "Curves", 31, {"Build & Query", "Transforms", "NS/NSS Fitting", "Specialized"}},
        {"economics", "Economics", 25, {"Equilibrium", "Game Theory", "Auctions", "Utility Theory"}},
        {"instruments", "Instruments", 26, {"Bonds", "Swaps/FRA", "Markets", "Credit/Futures"}},
        {"ml", "Machine Learning", 48, {"Credit", "Regression", "Clustering", "Preprocessing", "Features", "Validation", "Time Series", "GP/Neural Net", "Factor/Covariance"}},
        {"models", "Models", 14, {"Short Rate", "Hull-White", "Heston", "Jump Diffusion", "Dupire/SVI"}},
        {"numerical", "Numerical", 28, {"Diff/FFT/Int", "Interp/LinAlg", "ODE/Roots/Opt"}},
        {"physics", "Physics", 24, {"Entropy", "Thermodynamics"}},
        {"portfolio", "Portfolio", 15, {"Optimization", "Risk Metrics"}},
        {"pricing", "Pricing", 29, {"Black-Scholes", "Black76", "Bachelier", "Numerical"}},
        {"regulatory", "Regulatory", 11, {"Basel III", "SA-CCR", "IFRS 9", "Liquidity", "Stress Test"}},
        {"risk", "Risk", 25, {"VaR/Stress", "EVT/XVA", "Sensitivities"}},
        {"scheduling", "Scheduling", 14, {"Calendars", "Day Count"}},
        {"solver", "Solver", 25, {"Bond Analytics", "Rates/IV", "Cashflows"}},
        {"statistics", "Statistics", 52, {"Continuous Dist", "Discrete Dist", "Time Series"}},
        {"stochastic", "Stochastic", 36, {"Processes", "Exact", "Simulation", "Sampling", "Theory"}},
        {"volatility", "Volatility", 14, {"Surface", "SABR", "Local Vol"}},
    };
}

// ── API base URL ────────────────────────────────────────────────────────────

static const QString API_BASE = QStringLiteral("https://api.fincept.in");

// ── Constructor ─────────────────────────────────────────────────────────────

QuantLibScreen::QuantLibScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("qlScreen");
    setStyleSheet(kStyle);
    modules_ = build_modules();
    setup_ui();
    LOG_INFO("QuantLib", "Screen constructed — 18 modules, 590+ endpoints");
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void QuantLibScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* sidebar = create_sidebar();
    sidebar->setMinimumWidth(200);
    sidebar->setMaximumWidth(260);

    auto* center = create_center_panel();
    auto* right = create_right_panel();

    splitter->addWidget(sidebar);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({220, 450, 450});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());

    // Select first module
    on_module_changed(0);
}

QWidget* QuantLibScreen::create_header() {
    auto* bar = new QWidget;
    bar->setObjectName("qlHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    auto* title = new QLabel("QUANTLIB SUITE");
    title->setObjectName("qlHeaderTitle");
    auto* sub = new QLabel("18 MODULES | 590+ QUANTITATIVE ENDPOINTS");
    sub->setObjectName("qlHeaderSub");
    title_col->addWidget(title);
    title_col->addWidget(sub);
    hl->addLayout(title_col);
    hl->addStretch(1);

    auto* badge = new QLabel("API POWERED");
    badge->setObjectName("qlHeaderBadge");
    hl->addWidget(badge);

    return bar;
}

QWidget* QuantLibScreen::create_sidebar() {
    auto* panel = new QWidget;
    panel->setObjectName("qlSidebar");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* title = new QLabel("MODULES");
    title->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                  "letter-spacing: 0.5px; background: transparent; "
                                  "padding: 8px 12px; border-bottom: 1px solid %2;")
                             .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM));
    vl->addWidget(title);

    module_tree_ = new QTreeWidget;
    module_tree_->setHeaderHidden(true);
    module_tree_->setIndentation(16);

    for (int i = 0; i < modules_.size(); ++i) {
        const auto& m = modules_[i];
        auto* module_item = new QTreeWidgetItem(module_tree_);
        module_item->setText(0, m.name.toUpper() + " (" + QString::number(m.endpoint_count) + ")");
        module_item->setData(0, Qt::UserRole, i);
        module_item->setExpanded(i == 0);

        for (const auto& panel_name : m.panels) {
            auto* panel_item = new QTreeWidgetItem(module_item);
            panel_item->setText(0, panel_name);
            panel_item->setData(0, Qt::UserRole + 1, panel_name);
        }
    }

    connect(module_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (!item) return;
        // Check if it's a module (top level) or panel (child)
        if (!item->parent()) {
            int idx = item->data(0, Qt::UserRole).toInt();
            on_module_changed(idx);
        } else {
            // Panel clicked
            int mod_idx = item->parent()->data(0, Qt::UserRole).toInt();
            on_module_changed(mod_idx);
            active_panel_ = item->data(0, Qt::UserRole + 1).toString();
            status_panel_->setText(active_panel_);
            center_title_->setText(modules_[mod_idx].name.toUpper() + " / " + active_panel_.toUpper());
        }
    });

    vl->addWidget(module_tree_, 1);
    return panel;
}

QWidget* QuantLibScreen::create_center_panel() {
    auto* scroll = new QScrollArea;
    scroll->setObjectName("qlCenterPanel");
    scroll->setWidgetResizable(true);

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    center_title_ = new QLabel("CORE");
    center_title_->setObjectName("qlCenterTitle");
    vl->addWidget(center_title_);

    // Endpoint selector
    auto* ep_panel = new QWidget;
    ep_panel->setObjectName("qlPanel");
    auto* epvl = new QVBoxLayout(ep_panel);
    epvl->setContentsMargins(0, 0, 0, 0);
    epvl->setSpacing(0);

    auto* ephdr = new QWidget;
    ephdr->setObjectName("qlPanelHeader");
    ephdr->setFixedHeight(34);
    auto* ephl = new QHBoxLayout(ephdr);
    ephl->setContentsMargins(12, 0, 12, 0);
    auto* ept = new QLabel("ENDPOINT");
    ept->setObjectName("qlPanelTitle");
    ephl->addWidget(ept);
    ephl->addStretch(1);
    epvl->addWidget(ephdr);

    auto* ep_body = new QWidget;
    auto* ebl = new QVBoxLayout(ep_body);
    ebl->setContentsMargins(12, 12, 12, 12);
    ebl->setSpacing(10);

    endpoint_combo_ = new QComboBox;
    ebl->addWidget(endpoint_combo_);

    // Parameters
    auto make_param = [&](const QString& label, QLineEdit*& input) {
        auto* row = new QWidget;
        auto* rl = new QVBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(3);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("qlLabel");
        input = new QLineEdit;
        rl->addWidget(lbl);
        rl->addWidget(input);
        ebl->addWidget(row);
    };

    // JSON body input — users enter the exact fields the API expects
    auto* json_label = new QLabel("REQUEST BODY (JSON)");
    json_label->setObjectName("qlLabel");
    ebl->addWidget(json_label);

    param_input1_ = new QLineEdit;
    param_input1_->setPlaceholderText("{\"spot\": 100, \"strike\": 105, \"risk_free_rate\": 0.05, \"volatility\": 0.2, \"time_to_maturity\": 1.0}");
    param_input1_->setMinimumHeight(28);
    ebl->addWidget(param_input1_);

    // Quick-fill helpers
    auto* helpers = new QHBoxLayout;
    helpers->setSpacing(4);

    auto add_helper = [&](const QString& label, const QString& json_text) {
        auto* btn = new QPushButton(label);
        btn->setObjectName("qlLabel");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                    "padding: 2px 8px; font-size: 8px; font-weight: 700; } "
                                    "QPushButton:hover { color: %4; }")
                               .arg(colors::BG_SURFACE, colors::TEXT_SECONDARY,
                                    colors::BORDER_DIM, colors::TEXT_PRIMARY));
        connect(btn, &QPushButton::clicked, param_input1_, [this, json_text]() {
            param_input1_->setText(json_text);
        });
        helpers->addWidget(btn);
    };

    add_helper("BS Price", "{\"spot\":100,\"strike\":105,\"risk_free_rate\":0.05,\"volatility\":0.2,\"time_to_maturity\":1.0,\"option_type\":\"call\"}");
    add_helper("GBM Sim", "{\"S0\":100,\"mu\":0.05,\"sigma\":0.2,\"T\":1.0,\"n_steps\":252,\"n_paths\":5}");
    add_helper("VaR", "{\"portfolio_value\":1000000,\"volatility\":0.02,\"confidence_level\":0.99,\"horizon\":1}");
    add_helper("Bond Yield", "{\"face_value\":1000,\"coupon_rate\":0.05,\"price\":950,\"periods\":10,\"frequency\":2}");
    helpers->addStretch(1);
    ebl->addLayout(helpers);

    // Hide unused inputs
    param_input2_ = new QLineEdit; param_input2_->hide();
    param_input3_ = new QLineEdit; param_input3_->hide();
    param_input4_ = new QLineEdit; param_input4_->hide();

    exec_btn_ = new QPushButton("EXECUTE COMPUTATION");
    exec_btn_->setObjectName("qlExecBtn");
    exec_btn_->setCursor(Qt::PointingHandCursor);
    exec_btn_->setFixedHeight(34);
    connect(exec_btn_, &QPushButton::clicked, this, &QuantLibScreen::on_execute);
    ebl->addWidget(exec_btn_);

    epvl->addWidget(ep_body);
    vl->addWidget(ep_panel);

    vl->addStretch(1);
    scroll->setWidget(content);
    return scroll;
}

QWidget* QuantLibScreen::create_right_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("qlResultPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget;
    toolbar->setFixedHeight(32);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);
    auto* rt = new QLabel("RESULTS");
    rt->setObjectName("qlPanelTitle");
    tbl->addWidget(rt);
    tbl->addStretch(1);
    result_status_ = new QLabel;
    result_status_->setObjectName("qlResultStatus");
    tbl->addWidget(result_status_);
    vl->addWidget(toolbar);

    // Result stack: JSON | table
    result_stack_ = new QStackedWidget;

    result_view_ = new QTextEdit;
    result_view_->setReadOnly(true);
    result_view_->setPlaceholderText("Select a module and endpoint, then execute to see results...");
    result_stack_->addWidget(result_view_);

    result_table_ = new QTableWidget;
    result_table_->verticalHeader()->setVisible(false);
    result_table_->horizontalHeader()->setStretchLastSection(true);
    result_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    result_stack_->addWidget(result_table_);

    vl->addWidget(result_stack_, 1);
    return panel;
}

QWidget* QuantLibScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("qlStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("QUANTLIB SUITE");
    left->setObjectName("qlStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_module_ = new QLabel("MODULE: CORE");
    status_module_->setObjectName("qlStatusText");
    hl->addWidget(status_module_);

    hl->addSpacing(12);
    status_panel_ = new QLabel;
    status_panel_->setObjectName("qlStatusText");
    hl->addWidget(status_panel_);

    hl->addSpacing(12);
    status_endpoint_ = new QLabel;
    status_endpoint_->setObjectName("qlStatusHighlight");
    hl->addWidget(status_endpoint_);

    return bar;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void QuantLibScreen::on_module_changed(int index) {
    if (index < 0 || index >= modules_.size()) return;
    active_module_ = index;

    const auto& m = modules_[index];
    status_module_->setText("MODULE: " + m.name.toUpper());
    center_title_->setText(m.name.toUpper());

    populate_panels(index);
    LOG_INFO("QuantLib", "Module: " + m.name);
}

void QuantLibScreen::on_panel_changed(QListWidgetItem* item) {
    if (!item) return;
    active_panel_ = item->text();
    status_panel_->setText(active_panel_);
}

void QuantLibScreen::on_endpoint_changed(int index) {
    if (index >= 0) {
        status_endpoint_->setText(endpoint_combo_->currentText());
    }
}

void QuantLibScreen::on_execute() {
    if (loading_) return;

    QString endpoint = endpoint_combo_->currentText();
    if (endpoint.isEmpty()) return;

    // Parse JSON body from input
    QJsonObject params;
    QString body_text = param_input1_->text().trimmed();
    if (!body_text.isEmpty()) {
        auto doc = QJsonDocument::fromJson(body_text.toUtf8());
        if (doc.isObject()) {
            params = doc.object();
        } else {
            result_view_->setPlainText("ERROR: Invalid JSON in request body.\n\n"
                                        "Expected format: {\"key\": value, ...}\n"
                                        "Example: {\"spot\": 100, \"strike\": 105, \"risk_free_rate\": 0.05}");
            result_stack_->setCurrentIndex(0);
            return;
        }
    }

    execute_api(endpoint, params);
}

// ── Data ────────────────────────────────────────────────────────────────────

// Real API endpoint paths per module (from api.fincept.in OpenAPI spec)
static const QHash<QString, QStringList> MODULE_ENDPOINTS = {
    {"core", {"core/types/currencies", "core/types/frequencies", "core/types/money/create", "core/types/money/convert",
              "core/types/rate/convert", "core/types/spread/from-bps", "core/types/tenor/add-to-date", "core/types/notional-schedule",
              "core/conventions/parse-date", "core/conventions/format-date", "core/conventions/days-to-years", "core/conventions/years-to-days",
              "core/conventions/normalize-rate", "core/conventions/normalize-volatility",
              "core/autodiff/dual-eval", "core/autodiff/gradient", "core/autodiff/taylor-expand",
              "core/distributions/normal/cdf", "core/distributions/normal/pdf", "core/distributions/normal/ppf",
              "core/distributions/t/cdf", "core/distributions/t/pdf", "core/distributions/t/ppf",
              "core/distributions/chi2/cdf", "core/distributions/chi2/pdf",
              "core/distributions/gamma/cdf", "core/distributions/gamma/pdf",
              "core/distributions/exponential/cdf", "core/distributions/exponential/pdf", "core/distributions/exponential/ppf",
              "core/distributions/bivariate-normal/cdf",
              "core/math/eval", "core/math/two-arg",
              "core/ops/black-scholes", "core/ops/black76", "core/ops/forward-rate", "core/ops/discount-cashflows",
              "core/ops/interpolate", "core/ops/statistics", "core/ops/var", "core/ops/percentile",
              "core/ops/covariance-matrix", "core/ops/cholesky", "core/ops/gbm-paths", "core/ops/zero-rate-convert",
              "core/legs/fixed", "core/legs/float", "core/legs/zero-coupon",
              "core/periods/day-count-fraction", "core/periods/fixed-coupon", "core/periods/float-coupon"}},
    {"pricing", {"pricing/bs/price", "pricing/bs/greeks", "pricing/bs/greeks-full", "pricing/bs/implied-vol",
                 "pricing/bs/digital-call", "pricing/bs/digital-put", "pricing/bs/asset-or-nothing-call", "pricing/bs/asset-or-nothing-put",
                 "pricing/black76/price", "pricing/black76/greeks", "pricing/black76/greeks-full", "pricing/black76/implied-vol",
                 "pricing/black76/caplet", "pricing/black76/floorlet", "pricing/black76/swaption",
                 "pricing/bachelier/price", "pricing/bachelier/greeks", "pricing/bachelier/greeks-full", "pricing/bachelier/implied-vol",
                 "pricing/bachelier/shifted-lognormal", "pricing/bachelier/vol-conversion",
                 "pricing/binomial/european", "pricing/binomial/american", "pricing/binomial/bermudan", "pricing/binomial/barrier",
                 "pricing/kirk/spread-price", "pricing/kirk/spread-greeks", "pricing/margrabe", "pricing/basket-levy"}},
    {"curves", {"curves/build", "curves/zero-rate", "curves/forward-rate", "curves/discount-factor", "curves/curve-points",
                "curves/interpolate", "curves/interpolate-derivative", "curves/instantaneous-forward",
                "curves/parallel-shift", "curves/twist", "curves/butterfly", "curves/key-rate-shift",
                "curves/nelson-siegel/fit", "curves/nelson-siegel/evaluate", "curves/nss/fit", "curves/nss/evaluate",
                "curves/roll", "curves/scale", "curves/time-shift", "curves/composite", "curves/proxy",
                "curves/real-rate", "curves/monotonicity-check", "curves/smoothness-penalty", "curves/constrained-fit",
                "curves/multicurve/setup", "curves/multicurve/basis-spread",
                "curves/cross-currency-basis", "curves/inflation/build", "curves/inflation/bootstrap", "curves/inflation/seasonality"}},
    {"volatility", {"volatility/surface/flat", "volatility/surface/from-points", "volatility/surface/grid",
                    "volatility/surface/smile", "volatility/surface/term-structure", "volatility/surface/total-variance",
                    "volatility/sabr/implied-vol", "volatility/sabr/calibrate", "volatility/sabr/smile",
                    "volatility/sabr/normal-vol", "volatility/sabr/density", "volatility/sabr/dynamics",
                    "volatility/local-vol/constant", "volatility/local-vol/implied-to-local"}},
    {"models", {"models/short-rate/bond-price", "models/short-rate/bond-option", "models/short-rate/yield-curve", "models/short-rate/monte-carlo",
                "models/hull-white/calibrate",
                "models/heston/price", "models/heston/monte-carlo", "models/heston/implied-vol",
                "models/merton/price", "models/merton/fft", "models/kou/price",
                "models/dupire/price", "models/svi/calibrate", "models/variance-gamma/price"}},
    {"stochastic", {"stochastic/gbm/simulate", "stochastic/gbm/properties", "stochastic/ou/simulate", "stochastic/cir/simulate", "stochastic/cir/bond-price",
                    "stochastic/heston/simulate", "stochastic/merton/simulate", "stochastic/vasicek/simulate", "stochastic/vasicek/bond-price",
                    "stochastic/wiener/simulate", "stochastic/poisson/simulate", "stochastic/variance-gamma/simulate",
                    "stochastic/brownian-bridge/simulate", "stochastic/correlated-bm/simulate",
                    "stochastic/exact/gbm", "stochastic/exact/ou", "stochastic/exact/cir", "stochastic/exact/heston",
                    "stochastic/simulation/euler-maruyama", "stochastic/simulation/milstein",
                    "stochastic/simulation/euler-maruyama-nd", "stochastic/simulation/milstein-nd", "stochastic/simulation/multilevel-mc",
                    "stochastic/sampling/sobol", "stochastic/sampling/antithetic", "stochastic/sampling/correlated-normals",
                    "stochastic/sampling/multivariate-normal", "stochastic/sampling/distribution", "stochastic/sampling/jump",
                    "stochastic/theory/ito-lemma", "stochastic/theory/ito-product-rule", "stochastic/theory/quadratic-variation",
                    "stochastic/theory/covariation", "stochastic/theory/martingale-test",
                    "stochastic/theory/girsanov/measure-change", "stochastic/theory/girsanov/risk-neutral-drift"}},
    {"risk", {"risk/var/parametric", "risk/var/historical", "risk/var/component", "risk/var/incremental", "risk/var/marginal", "risk/var/es-optimization",
              "risk/backtest", "risk/stress/scenario", "risk/correlation-stress", "risk/tail-risk/comprehensive", "risk/tail-risk/tail-dependence",
              "risk/evt/gpd", "risk/evt/gev", "risk/evt/hill",
              "risk/xva/cva", "risk/xva/pfe", "risk/copula/sample",
              "risk/portfolio-risk/exposure-profile", "risk/portfolio-risk/optimal-hedge",
              "risk/sensitivities/greeks", "risk/sensitivities/key-rate-duration", "risk/sensitivities/parallel-shift",
              "risk/sensitivities/twist", "risk/sensitivities/cross-gamma", "risk/sensitivities/bucket-delta"}},
    {"portfolio", {"portfolio/optimize/min-variance", "portfolio/optimize/max-sharpe", "portfolio/optimize/efficient-frontier", "portfolio/optimize/target-return",
                   "portfolio/black-litterman/equilibrium", "portfolio/black-litterman/posterior",
                   "portfolio/risk-parity", "portfolio/risk/inverse-volatility",
                   "portfolio/risk/var", "portfolio/risk/cvar", "portfolio/risk/incremental-var", "portfolio/risk/contribution",
                   "portfolio/risk/ratios", "portfolio/risk/comprehensive", "portfolio/risk/portfolio-comprehensive"}},
    {"instruments", {"instruments/bond/fixed/price", "instruments/bond/fixed/yield", "instruments/bond/fixed/analytics", "instruments/bond/fixed/cashflows",
                     "instruments/bond/zero-coupon/price", "instruments/bond/inflation-linked",
                     "instruments/swap/irs/value", "instruments/swap/irs/par-rate", "instruments/swap/irs/dv01",
                     "instruments/fra/value", "instruments/fra/break-even",
                     "instruments/ois/value", "instruments/ois/build-curve",
                     "instruments/cds/value", "instruments/cds/hazard-rate", "instruments/cds/survival-probability",
                     "instruments/fx/forward", "instruments/fx/garman-kohlhagen",
                     "instruments/money-market/deposit", "instruments/money-market/repo", "instruments/money-market/tbill",
                     "instruments/commodity/future", "instruments/futures/stir", "instruments/futures/bond/ctd",
                     "instruments/equity/variance-swap", "instruments/equity/volatility-swap"}},
    {"solver", {"solver/finance/bond-yield", "solver/finance/duration", "solver/finance/modified-duration", "solver/finance/convexity",
                "solver/finance/convexity-adjustment", "solver/finance/pv01", "solver/finance/dv01",
                "solver/finance/implied-vol", "solver/finance/implied-vol-black76",
                "solver/finance/forward-rate", "solver/finance/zero-rate", "solver/finance/discount-factor",
                "solver/finance/par-rate", "solver/finance/z-spread", "solver/finance/i-spread", "solver/finance/g-spread",
                "solver/finance/oas", "solver/finance/asw-spread", "solver/finance/basis",
                "solver/finance/carry", "solver/finance/implied-repo-rate", "solver/finance/forward-futures-conversion",
                "solver/finance/irr", "solver/finance/xirr",
                "solver/bootstrap/curve", "solver/calibration/vasicek"}},
    {"economics", {"economics/equilibrium/cobb-douglas", "economics/equilibrium/ces", "economics/equilibrium/walrasian", "economics/equilibrium/exchange-economy",
                   "economics/games/create", "economics/games/classic", "economics/games/mixed-nash", "economics/games/nash-check",
                   "economics/games/best-response", "economics/games/eliminate-dominated", "economics/games/fictitious-play",
                   "economics/auctions/run", "economics/auctions/simulate", "economics/auctions/equilibrium-bid", "economics/auctions/expected-revenue",
                   "economics/utility/cara", "economics/utility/crra", "economics/utility/log", "economics/utility/quadratic",
                   "economics/utility/expected-utility", "economics/utility/certainty-equivalent", "economics/utility/certainty-equivalent-approximation",
                   "economics/utility/risk-premium", "economics/utility/prospect-theory", "economics/utility/stochastic-dominance"}},
    {"regulatory", {"regulatory/basel/capital-ratios", "regulatory/basel/credit-rwa", "regulatory/basel/operational-rwa",
                    "regulatory/saccr/ead",
                    "regulatory/ifrs9/stage-assessment", "regulatory/ifrs9/sicr", "regulatory/ifrs9/ecl-12m", "regulatory/ifrs9/ecl-lifetime",
                    "regulatory/liquidity/lcr", "regulatory/liquidity/nsfr",
                    "regulatory/stress/capital-projection"}},
    {"scheduling", {"scheduling/calendar/list", "scheduling/calendar/is-business-day", "scheduling/calendar/next-business-day",
                    "scheduling/calendar/previous-business-day", "scheduling/calendar/business-days-between", "scheduling/calendar/add-business-days",
                    "scheduling/daycount/conventions", "scheduling/daycount/year-fraction", "scheduling/daycount/day-count", "scheduling/daycount/batch-year-fraction",
                    "scheduling/adjustment/adjust-date", "scheduling/adjustment/batch-adjust", "scheduling/adjustment/methods",
                    "scheduling/schedule/generate"}},
    {"numerical", {"numerical/differentiation/derivative", "numerical/differentiation/gradient", "numerical/differentiation/hessian",
                   "numerical/fft/forward", "numerical/fft/inverse", "numerical/fft/convolve",
                   "numerical/integration/quadrature", "numerical/integration/monte-carlo", "numerical/integration/stratified",
                   "numerical/interpolation/evaluate", "numerical/interpolation/spline-curve", "numerical/interpolation/spline-derivative",
                   "numerical/linalg/matmul", "numerical/linalg/matvec", "numerical/linalg/solve", "numerical/linalg/inverse",
                   "numerical/linalg/decompose", "numerical/linalg/dot", "numerical/linalg/outer", "numerical/linalg/norm",
                   "numerical/linalg/transpose", "numerical/linalg/lstsq",
                   "numerical/least-squares/fit", "numerical/ode/solve",
                   "numerical/roots/find-1d", "numerical/roots/find-nd", "numerical/roots/newton",
                   "numerical/optimize/minimize"}},
    {"physics", {"physics/entropy/shannon", "physics/entropy/renyi", "physics/entropy/tsallis", "physics/entropy/cross",
                 "physics/entropy/conditional", "physics/entropy/joint", "physics/entropy/differential",
                 "physics/entropy/mutual-information", "physics/entropy/transfer", "physics/entropy/fisher-information", "physics/entropy/markov-rate",
                 "physics/divergence/kl", "physics/divergence/js",
                 "physics/boltzmann", "physics/max-entropy", "physics/ising", "physics/ising/critical-temperature",
                 "physics/thermodynamics/ideal-gas", "physics/thermodynamics/van-der-waals", "physics/thermodynamics/carnot",
                 "physics/thermodynamics/free-energy", "physics/thermodynamics/clausius-clapeyron",
                 "physics/thermodynamics/maxwell-relations", "physics/thermodynamics/joule-thomson"}},
    {"statistics", {"statistics/distributions/normal/cdf", "statistics/distributions/normal/pdf", "statistics/distributions/normal/ppf", "statistics/distributions/normal/properties",
                    "statistics/distributions/lognormal/cdf", "statistics/distributions/lognormal/pdf", "statistics/distributions/lognormal/ppf", "statistics/distributions/lognormal/properties",
                    "statistics/distributions/student-t/cdf", "statistics/distributions/student-t/pdf", "statistics/distributions/student-t/properties",
                    "statistics/distributions/chi-squared/cdf", "statistics/distributions/chi-squared/pdf", "statistics/distributions/chi-squared/properties",
                    "statistics/distributions/f/pdf", "statistics/distributions/f/properties",
                    "statistics/distributions/gamma/cdf", "statistics/distributions/gamma/pdf", "statistics/distributions/gamma/properties",
                    "statistics/distributions/beta/cdf", "statistics/distributions/beta/pdf", "statistics/distributions/beta/properties",
                    "statistics/distributions/exponential/cdf", "statistics/distributions/exponential/pdf", "statistics/distributions/exponential/ppf", "statistics/distributions/exponential/properties",
                    "statistics/distributions/poisson/cdf", "statistics/distributions/poisson/pmf", "statistics/distributions/poisson/properties",
                    "statistics/distributions/binomial/cdf", "statistics/distributions/binomial/pmf", "statistics/distributions/binomial/properties",
                    "statistics/distributions/geometric/cdf", "statistics/distributions/geometric/pmf", "statistics/distributions/geometric/ppf", "statistics/distributions/geometric/properties",
                    "statistics/distributions/hypergeometric/cdf", "statistics/distributions/hypergeometric/pmf", "statistics/distributions/hypergeometric/properties",
                    "statistics/distributions/negative-binomial/cdf", "statistics/distributions/negative-binomial/pmf", "statistics/distributions/negative-binomial/properties",
                    "statistics/distributions/pgf",
                    "statistics/timeseries/ar/fit", "statistics/timeseries/ar/forecast",
                    "statistics/timeseries/arima/fit", "statistics/timeseries/arima/forecast",
                    "statistics/timeseries/ma/fit",
                    "statistics/timeseries/garch/fit", "statistics/timeseries/garch/forecast",
                    "statistics/timeseries/egarch/fit", "statistics/timeseries/gjr-garch/fit"}},
    {"ml", {"ml/credit/scorecard", "ml/credit/logistic-regression", "ml/credit/woe-binning", "ml/credit/calibration",
            "ml/credit/discrimination", "ml/credit/performance", "ml/credit/migration", "ml/credit/beta-lgd", "ml/credit/two-stage-lgd",
            "ml/regression/fit", "ml/regression/ensemble", "ml/regression/tree", "ml/regression/lgd", "ml/regression/ead",
            "ml/clustering/kmeans", "ml/clustering/hierarchical", "ml/clustering/dbscan", "ml/clustering/pca", "ml/clustering/isolation-forest",
            "ml/preprocessing/scale", "ml/preprocessing/transform", "ml/preprocessing/outliers", "ml/preprocessing/winsorize", "ml/preprocessing/stationarity",
            "ml/features/lags", "ml/features/rolling", "ml/features/calendar", "ml/features/technical", "ml/features/financial-ratios", "ml/features/cross-sectional",
            "ml/validation/stability", "ml/validation/discrimination-report", "ml/validation/calibration-report", "ml/validation/interpretability",
            "ml/timeseries/feature-importance", "ml/changepoint/detect",
            "ml/gp/curve", "ml/gp/vol-surface", "ml/nn/curve", "ml/nn/vol-surface", "ml/nn/portfolio",
            "ml/hmm/fit", "ml/garch-hybrid", "ml/metrics/regression", "ml/metrics/classification",
            "ml/factor/statistical", "ml/factor/cross-sectional", "ml/covariance/estimate"}},
    {"analysis", {"analysis/fundamentals/profitability", "analysis/fundamentals/liquidity", "analysis/fundamentals/efficiency",
                  "analysis/fundamentals/growth", "analysis/fundamentals/solvency", "analysis/fundamentals/cashflow",
                  "analysis/fundamentals/comprehensive", "analysis/fundamentals/dupont", "analysis/fundamentals/quality",
                  "analysis/fundamentals/capital-structure/wacc", "analysis/fundamentals/capital-structure/optimal",
                  "analysis/ratios/profitability/roa", "analysis/ratios/profitability/roe", "analysis/ratios/profitability/roic",
                  "analysis/ratios/profitability/gross-margin", "analysis/ratios/profitability/net-margin", "analysis/ratios/profitability/ebitda-margin",
                  "analysis/ratios/liquidity/current-ratio", "analysis/ratios/liquidity/quick-ratio", "analysis/ratios/liquidity/cash-ratio",
                  "analysis/ratios/leverage/debt-to-equity", "analysis/ratios/leverage/interest-coverage",
                  "analysis/ratios/valuation/pe", "analysis/ratios/valuation/pb", "analysis/ratios/valuation/ps",
                  "analysis/ratios/valuation/ev-ebitda", "analysis/ratios/valuation/dividend-yield",
                  "analysis/valuation/dcf/fcff", "analysis/valuation/dcf/ddm", "analysis/valuation/dcf/gordon-growth",
                  "analysis/valuation/dcf/two-stage", "analysis/valuation/dcf/wacc", "analysis/valuation/dcf/terminal-value", "analysis/valuation/dcf/cost-of-equity",
                  "analysis/valuation/comparable", "analysis/valuation/screen", "analysis/valuation/factor-models",
                  "analysis/valuation/credit/merton-model", "analysis/valuation/credit/distance-to-default",
                  "analysis/valuation/predictive/altman-z", "analysis/valuation/predictive/piotroski-f", "analysis/valuation/predictive/beneish-m",
                  "analysis/industry/banking", "analysis/industry/insurance", "analysis/industry/reits", "analysis/industry/utilities"}},
};

void QuantLibScreen::populate_panels(int module_index) {
    endpoint_combo_->clear();

    const auto& m = modules_[module_index];
    auto it = MODULE_ENDPOINTS.find(m.id);
    if (it != MODULE_ENDPOINTS.end()) {
        for (const auto& ep : it.value()) {
            endpoint_combo_->addItem(ep);
        }
    }

    connect(endpoint_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QuantLibScreen::on_endpoint_changed, Qt::UniqueConnection);
}

void QuantLibScreen::execute_api(const QString& endpoint, const QJsonObject& params) {
    set_loading(true);
    result_status_->setText("Computing...");

    QString url = API_BASE + "/quantlib/" + endpoint;

    // 5 GET-only endpoints, everything else is POST
    static const QStringList GET_ENDPOINTS = {
        "core/types/currencies", "core/types/frequencies",
        "scheduling/calendar/list", "scheduling/daycount/conventions",
        "scheduling/adjustment/methods",
    };
    bool is_get = GET_ENDPOINTS.contains(endpoint);

    auto* nam = new QNetworkAccessManager(this);
    QUrl request_url(url);
    QNetworkRequest req{request_url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "FinceptTerminal/4.0.0");

    // Add API key from auth session
    auto& auth_mgr = auth::AuthManager::instance();
    if (auth_mgr.is_authenticated()) {
        req.setRawHeader("X-API-Key", auth_mgr.session().api_key.toUtf8());
    }

    QPointer<QuantLibScreen> self = this;
    QNetworkReply* reply = nullptr;
    if (is_get) {
        reply = nam->get(req);
    } else {
        QByteArray body = QJsonDocument(params).toJson(QJsonDocument::Compact);
        reply = nam->post(req, body);
    }

    connect(reply, &QNetworkReply::finished, this, [self, reply, nam, endpoint]() {
        reply->deleteLater();
        nam->deleteLater();
        if (!self) return;

        self->set_loading(false);

        if (reply->error() != QNetworkReply::NoError) {
            self->display_error(reply->errorString());
            return;
        }

        auto data = reply->readAll();
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(data, &err);
        if (doc.isNull()) {
            self->display_error("JSON parse error: " + err.errorString());
            return;
        }

        if (doc.isObject()) {
            auto obj = doc.object();
            if (obj.contains("error")) {
                self->display_error(obj["error"].toString());
                return;
            }
            self->display_result(obj);
        } else {
            // Display raw JSON
            self->result_view_->setPlainText(doc.toJson(QJsonDocument::Indented));
            self->result_stack_->setCurrentIndex(0);
        }

        self->status_endpoint_->setText(endpoint);
        LOG_INFO("QuantLib", "Executed: " + endpoint);
    });
}

// ── Display ─────────────────────────────────────────────────────────────────

void QuantLibScreen::display_result(const QJsonObject& result) {
    // Try to display as table if result has data array
    if (result.contains("data") && result["data"].isArray()) {
        auto data = result["data"].toArray();
        if (!data.isEmpty() && data[0].isObject()) {
            // Table view
            auto first = data[0].toObject();
            QStringList cols;
            for (auto it = first.begin(); it != first.end(); ++it) {
                cols << it.key();
            }

            result_table_->setSortingEnabled(false);
            result_table_->setColumnCount(cols.size());
            result_table_->setHorizontalHeaderLabels(cols);

            int rows = qMin(data.size(), 1000);
            result_table_->setRowCount(rows);

            for (int r = 0; r < rows; ++r) {
                auto obj = data[r].toObject();
                for (int c = 0; c < cols.size(); ++c) {
                    auto val = obj.value(cols[c]);
                    QString text = val.isDouble() ? QString::number(val.toDouble(), 'g', 10)
                                 : val.isNull() ? "--"
                                 : val.toVariant().toString();
                    auto* item = new QTableWidgetItem(text);
                    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (val.isDouble()) {
                        item->setForeground(QColor(val.toDouble() < 0 ? colors::NEGATIVE : colors::CYAN));
                    }
                    result_table_->setItem(r, c, item);
                }
            }

            result_table_->resizeColumnsToContents();
            result_table_->setSortingEnabled(true);
            result_stack_->setCurrentIndex(1);
            result_status_->setText(QString::number(rows) + " results");
            return;
        }
    }

    // JSON view fallback
    result_view_->setPlainText(QJsonDocument(result).toJson(QJsonDocument::Indented));
    result_stack_->setCurrentIndex(0);
    result_status_->setText("OK");
}

void QuantLibScreen::display_error(const QString& error) {
    result_view_->setPlainText("ERROR: " + error);
    result_stack_->setCurrentIndex(0);
    result_status_->setText("Error");
    LOG_ERROR("QuantLib", error);
}

void QuantLibScreen::set_loading(bool loading) {
    loading_ = loading;
    exec_btn_->setEnabled(!loading);
    exec_btn_->setText(loading ? "COMPUTING..." : "EXECUTE COMPUTATION");
}

} // namespace fincept::screens
