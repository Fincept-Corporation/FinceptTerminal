// src/screens/quantlib/QuantLibScreen.cpp
//
// QuantLib suite — modules + endpoints + typed parameters + results. The
// endpoint catalogs (module_endpoints(), endpoint_examples()) live in
// QuantLibScreen_Data.cpp, the parameter form in QuantLibParamForm.cpp and the
// results pane in QuantLibResultView.cpp. Plan entitlements (which endpoints a
// Free account gets, at what cost) come from services/quantlib/QuantLibAccess.
#include "screens/quantlib/QuantLibScreen.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/quantlib/QuantLibParamForm.h"
#include "screens/quantlib/QuantLibResultView.h"
#include "services/quantlib/QuantLibClient.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EnterprisePromo.h"

#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QKeySequence>
#include <QPointer>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>

#include <utility>

namespace {
using namespace fincept::ui;

constexpr int kQlRememberedBodies = 40; // per-endpoint last inputs kept in the screen state

inline QString kStyle() {
    return QStringLiteral("#qlScreen { background: %1; }"

                          "#qlHeader { background: %2; border-bottom: 2px solid %3; }"
                          "#qlHeaderTitle { color: %4; font-weight: 700; background: transparent; }"
                          "#qlHeaderSub { color: %5; letter-spacing: 0.5px; background: transparent; }"
                          "#qlPlanBadge { color: %5; font-weight: 700; letter-spacing: 0.5px; padding: 2px 8px; "
                          "  border: 1px solid %8; background: transparent; }"
                          "#qlPlanBadge[plan=\"pro\"] { color: %6; border-color: %6; }"
                          "#qlPlanNote { color: %5; background: transparent; }"
                          "#qlUpgradeBtn { background: %3; color: %1; border: none; padding: 4px 12px; "
                          "  font-weight: 700; }"
                          "#qlUpgradeBtn:hover { background: %10; }"

                          "#qlSidebar { background: %7; border-right: 1px solid %8; }"
                          "#qlSidebarTitle { color: %5; font-weight: 700; letter-spacing: 0.5px; "
                          "  background: transparent; padding: 8px 12px; border-bottom: 1px solid %8; }"
                          "QTreeWidget { background: %1; color: %5; border: none; }"
                          "QTreeWidget::item { padding: 4px 8px; border-bottom: 1px solid %8; }"
                          "QTreeWidget::item:hover { color: %4; background: %12; }"
                          "QTreeWidget::item:selected { color: %3; background: rgba(217,119,6,0.1); }"

                          "#qlCenterPanel { background: %1; }"
                          "#qlCenterTitle { color: %4; font-weight: 700; background: transparent; }"

                          "#qlPanel { background: %7; border: 1px solid %8; }"
                          "#qlPanelHeader { background: %2; border-bottom: 1px solid %8; }"
                          "#qlPanelTitle { color: %4; font-weight: 700; background: transparent; }"
                          "#qlLabel { color: %5; font-weight: 700; letter-spacing: 0.5px; background: transparent; }"

                          "QLineEdit { background: %1; color: %4; border: 1px solid %8; padding: 4px 8px; }"
                          "QLineEdit:focus { border-color: %9; }"
                          "QLineEdit[invalid=\"true\"] { border-color: %14; }"
                          "QComboBox { background: %1; color: %4; border: 1px solid %8; padding: 4px 8px; }"
                          "QComboBox::drop-down { border: none; width: 16px; }"
                          "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                          "  selection-background-color: %3; }"
                          "QCheckBox { color: %4; background: transparent; }"

                          "#qlAccess { color: %5; background: transparent; }"
                          "#qlAccess[tier=\"free\"], #qlAccess[tier=\"included\"] { color: %6; }"
                          "#qlAccess[tier=\"credit\"] { color: %15; }"
                          "#qlAccess[tier=\"pro\"] { color: %3; }"
                          "#qlFieldLabel { color: %4; background: transparent; }"
                          "#qlFieldHint { color: %11; background: transparent; }"
                          "#qlFormWarn { color: %15; background: transparent; }"
                          "#qlFormBtn { background: %7; color: %5; border: 1px solid %8; padding: 2px 10px; "
                          "  font-weight: 700; }"
                          "#qlFormBtn:hover { color: %4; border-color: %9; }"
                          "#qlFormBtn:checked { color: %3; border-color: %3; }"
                          "#qlFormBtn:disabled { color: %11; }"
                          "#qlJsonEdit, #qlJsonView { background: %1; color: %13; border: 1px solid %8; }"

                          "#qlExecBtn { background: %3; color: %1; border: none; padding: 6px 20px; "
                          "  font-weight: 700; }"
                          "#qlExecBtn:hover { background: %10; }"
                          "#qlExecBtn:disabled { background: %10; color: %11; }"

                          "#qlResultPanel { background: %7; border-left: 1px solid %8; }"
                          "#qlResultBar { background: %2; border-bottom: 1px solid %8; }"
                          "#qlSectionBar { background: %7; border-bottom: 1px solid %8; }"
                          "#qlResultEndpoint { color: %13; background: transparent; }"
                          "#qlResultStatus { color: %5; background: transparent; }"
                          "#qlViewBtn { background: transparent; color: %5; border: 1px solid %8; "
                          "  padding: 2px 8px; font-weight: 700; }"
                          "#qlViewBtn:checked { background: %3; color: %1; border-color: %3; }"
                          "#qlViewBtn:disabled { color: %11; border-color: %8; }"
                          "#qlResultMessage { color: %5; background: %1; padding: 24px; }"
                          "#qlErrorPage { background: %1; }"
                          "#qlErrorTitle { color: %14; font-weight: 700; letter-spacing: 0.5px; "
                          "  background: transparent; }"
                          "#qlErrorTitle[tone=\"pro\"] { color: %3; }"
                          "#qlErrorTitle[tone=\"warn\"] { color: %15; }"
                          "#qlErrorText { color: %4; background: transparent; }"
                          "#qlErrorHint { color: %5; background: transparent; }"
                          "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; }"
                          "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                          "QHeaderView::section { background: %2; color: %5; border: none; "
                          "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                          "  padding: 4px 6px; font-weight: 700; }"

                          "#qlStatusBar { background: %2; border-top: 1px solid %8; }"
                          "#qlStatusText { color: %5; background: transparent; }"
                          "#qlStatusHighlight { color: %13; background: transparent; }"

                          "QSplitter::handle { background: %8; }"
                          "QScrollBar:vertical { background: %1; width: 6px; }"
                          "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
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
        .arg(colors::WARNING())        // %15
        ;
}

void ql_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

namespace fincept::screens {

using namespace fincept::ui;
using services::QuantLibCost;

// MODULE_ENDPOINTS catalog + endpoint_examples() live in
// QuantLibScreen_Data.cpp. Forward-declared here (not just above
// populate_panels) because build_modules() derives its per-module endpoint
// counts from the catalog rather than hard-coding them.
const QHash<QString, QStringList>& module_endpoints();

// ── Module definitions ──────────────────────────────────────────────────────
//
// The third field is a *fallback* endpoint count. The real count is taken from
// module_endpoints() in build_modules() — the hard-coded numbers had drifted
// away from the catalog (e.g. "analysis" advertised 122 endpoints while the
// catalog lists 46), so the sidebar was quoting figures the screen could not
// deliver.

static QList<QuantModule> build_modules() {
    QList<QuantModule> mods = {
        {"core",
         "Core",
         51,
         {"Types", "Conventions", "AutoDiff", "Distributions", "Math", "Operations", "Legs", "Periods"}},
        {"analysis",
         "Analysis",
         122,
         {"Fundamentals", "Profitability", "Liquidity", "Efficiency", "Growth", "Leverage", "Valuation Ratios",
          "Cash Flow", "Earnings Quality", "DCF Valuation", "Residual/SOTP", "Credit & Distress", "Startup/VC",
          "Industry"}},
        {"curves", "Curves", 31, {"Build & Query", "Transforms", "NS/NSS Fitting", "Specialized"}},
        {"economics", "Economics", 25, {"Equilibrium", "Game Theory", "Auctions", "Utility Theory"}},
        {"instruments", "Instruments", 26, {"Bonds", "Swaps/FRA", "Markets", "Credit/Futures"}},
        {"ml",
         "Machine Learning",
         48,
         {"Credit", "Regression", "Clustering", "Preprocessing", "Features", "Validation", "Time Series",
          "GP/Neural Net", "Factor/Covariance"}},
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

    // Replace the declared counts with what the catalog actually exposes.
    const auto& catalog = module_endpoints();
    for (auto& m : mods) {
        const auto it = catalog.find(m.id);
        m.endpoint_count = (it != catalog.end()) ? int(it.value().size()) : 0;
    }
    return mods;
}

// Map a sidebar sub-panel name onto the path segment that identifies it, so
// clicking a panel narrows the endpoint list instead of only relabelling the
// header. Panels with no entry (and any panel whose filter matches nothing)
// fall back to showing every endpoint in the module — never an empty combo.
static QStringList panel_path_tokens(const QString& panel) {
    static const QHash<QString, QStringList> kOverrides = {
        {"Operations", {"/ops/"}},
        {"Black-Scholes", {"/bs/"}},
        {"Black76", {"/black76"}},
        {"Bachelier", {"/bachelier"}},
        {"Build & Query",
         {"/build", "/query", "/discount", "/forward", "/zero", "/curve-points", "/interpolate",
          "/instantaneous-forward"}},
        {"NS/NSS Fitting", {"/ns", "/nss", "/fit", "/nelson-siegel/"}},
        {"Day Count", {"/day-count", "/daycount"}},
        {"Short Rate", {"/vasicek", "/cir", "/short-rate"}},
        {"Hull-White", {"/hull-white", "/hw"}},
        {"Jump Diffusion", {"/jump", "/merton", "/kou", "/variance-gamma"}},
        {"Dupire/SVI", {"/dupire", "/svi"}},
        {"Risk Metrics", {"/var", "/cvar", "/risk/", "/drawdown"}},
        {"Local Vol", {"/local-vol", "/localvol", "/dupire"}},
        {"Time Series", {"/timeseries", "/time-series", "/ts/", "/arima", "/garch", "/changepoint/", "/hmm/"}},
        // Panels whose name does not appear as a path segment. Without an entry
        // the default token matched nothing, so clicking the panel silently
        // showed the module's whole endpoint list instead of narrowing it.
        {"Valuation Ratios", {"/ratios/valuation/"}},
        {"DCF Valuation", {"/valuation/dcf/", "/valuation/comparable", "/valuation/screen", "/valuation/factor-models"}},
        {"Cash Flow", {"/ratios/cashflow/", "/fundamentals/cashflow"}},
        {"Earnings Quality", {"/ratios/quality/", "/fundamentals/quality"}},
        {"Residual/SOTP", {"/residual-income/", "/valuation/segment/", "/valuation/proforma/"}},
        {"Credit & Distress", {"/valuation/credit/", "/valuation/predictive/"}},
        {"Startup/VC", {"/valuation/startup/"}},
        {"Leverage", {"/leverage/", "/solvency", "/capital-structure"}},
        {"Transforms", {"/parallel-shift", "/twist", "/butterfly", "/key-rate-shift", "/roll", "/scale", "/time-shift"}},
        {"Specialized",
         {"/composite", "/proxy", "/real-rate", "/monotonicity-check", "/smoothness-penalty", "/constrained-fit",
          "/multicurve/", "/cross-currency-basis", "/inflation/"}},
        {"Equilibrium", {"/equilibrium/"}},
        {"Game Theory", {"/games/"}},
        {"Utility Theory", {"/utility/"}},
        {"Bonds", {"/bond/"}},
        {"Swaps/FRA", {"/swap/", "/fra/", "/ois/"}},
        {"Markets", {"/fx/", "/money-market/", "/commodity/", "/equity/"}},
        {"Credit/Futures", {"/cds/", "/futures/"}},
        {"Validation", {"/validation/", "/metrics/"}},
        {"GP/Neural Net", {"/gp/", "/nn/"}},
        {"Factor/Covariance", {"/factor/", "/covariance/"}},
        {"Diff/FFT/Int", {"/differentiation/", "/fft/", "/integration/"}},
        {"Interp/LinAlg", {"/interpolation/", "/linalg/"}},
        {"ODE/Roots/Opt", {"/ode/", "/roots/", "/optimize/", "/least-squares/"}},
        {"Entropy", {"/entropy/", "/divergence/", "/max-entropy"}},
        {"Thermodynamics", {"/thermodynamics/", "/boltzmann", "/ising"}},
        {"Optimization", {"/optimize/", "/black-litterman/", "/risk-parity"}},
        {"Numerical", {"/binomial/", "/kirk/", "/margrabe", "/basket-levy"}},
        {"Basel III", {"/basel/"}},
        {"SA-CCR", {"/saccr/"}},
        {"IFRS 9", {"/ifrs9/"}},
        {"Stress Test", {"/stress/"}},
        {"VaR/Stress", {"/var/", "/backtest", "/stress/", "/correlation-stress"}},
        {"EVT/XVA", {"/evt/", "/xva/", "/tail-risk/", "/copula/", "/portfolio-risk/"}},
        {"Calendars", {"/calendar/", "/adjustment/", "/schedule/"}},
        {"Bond Analytics",
         {"/bond-yield", "/duration", "/modified-duration", "/convexity", "/pv01", "/dv01", "-spread", "/oas"}},
        {"Rates/IV",
         {"/implied-vol", "/forward-rate", "/zero-rate", "/discount-factor", "/par-rate", "/bootstrap/",
          "/calibration/"}},
        {"Cashflows", {"/irr", "/xirr", "/basis", "/carry", "/implied-repo-rate", "/forward-futures-conversion"}},
        {"Continuous Dist",
         {"/normal/", "/lognormal/", "/student-t/", "/chi-squared/", "/f/", "/gamma/", "/beta/", "/exponential/"}},
        {"Discrete Dist",
         {"/poisson/", "/binomial/", "/geometric/", "/hypergeometric/", "/negative-binomial/", "/pgf"}},
        {"Processes",
         {"/gbm/", "/ou/", "/cir/", "/heston/", "/merton/", "/vasicek/", "/wiener/", "/poisson/", "/variance-gamma/",
          "/brownian-bridge/", "/correlated-bm/"}},
    };
    const auto ov = kOverrides.constFind(panel);
    if (ov != kOverrides.constEnd())
        return ov.value();

    // Default: the panel name lower-cased with spaces → hyphens, e.g.
    // "Risk Metrics" → "/risk-metrics/", "Distributions" → "/distributions/".
    QString token = panel.toLower();
    token.replace(' ', '-');
    token.replace('/', '-');
    return {"/" + token + "/", "/" + token};
}

// Badge text / colour for one cost on the Free plan.
static QString ql_cost_badge(QuantLibCost cost) {
    switch (cost) {
        case QuantLibCost::Included:
            return QuantLibScreen::tr("FREE");
        case QuantLibCost::OneCredit:
            return QuantLibScreen::tr("1 CR");
        case QuantLibCost::NeedsPro:
            break;
    }
    return QuantLibScreen::tr("PRO");
}

static QColor ql_cost_color(QuantLibCost cost) {
    switch (cost) {
        case QuantLibCost::Included:
            return QColor(colors::POSITIVE());
        case QuantLibCost::OneCredit:
            return QColor(colors::WARNING());
        case QuantLibCost::NeedsPro:
            break;
    }
    return QColor(colors::AMBER());
}

// ── Constructor ─────────────────────────────────────────────────────────────

QuantLibScreen::QuantLibScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("qlScreen");
    setStyleSheet(kStyle());
    modules_ = build_modules();
    paid_plan_ = services::quantlib_paid_plan();
    setup_ui();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { setStyleSheet(kStyle()); });

    // Plan changes (sign-in, purchase, pass expiry) re-badge the catalog live.
    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::auth_state_changed, this, &QuantLibScreen::refresh_plan_ui);
    connect(&auth, &auth::AuthManager::subscription_fetched, this, &QuantLibScreen::refresh_plan_ui);

    int total_endpoints = 0;
    for (const auto& m : modules_)
        total_endpoints += m.endpoint_count;
    LOG_INFO("QuantLib", QString("Screen constructed — %1 modules, %2 endpoints, %3 plan")
                             .arg(modules_.size())
                             .arg(total_endpoints)
                             .arg(paid_plan_ ? "paid" : "free"));
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
    sidebar->setMinimumWidth(220);
    sidebar->setMaximumWidth(300);

    auto* center = create_center_panel();
    result_view_ = new QuantLibResultView(this);
    connect(result_view_, &QuantLibResultView::retry_requested, this, &QuantLibScreen::on_execute);
    connect(result_view_, &QuantLibResultView::upgrade_requested, this, &QuantLibScreen::open_plans);

    splitter->addWidget(sidebar);
    splitter->addWidget(center);
    splitter->addWidget(result_view_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({240, 460, 520});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());

    // Keyboard path: modules → search → endpoint → parameter fields → execute
    // (creation order already runs the form's fields before EXECUTE). Done here
    // because setTabOrder only holds once the widgets sit in their final parents.
    module_tree_->setAccessibleName(tr("QuantLib modules"));
    setTabOrder(module_tree_, filter_edit_);
    setTabOrder(filter_edit_, endpoint_combo_);

    refresh_tree_badges();
    on_module_changed(0);

    // Populates the data-derived chrome (header module/endpoint counts, plan
    // note). Must run after the widgets exist.
    retranslateUi();
}

QWidget* QuantLibScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("qlHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(10);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    header_title_ = new QLabel(tr("QUANTLIB SUITE"));
    header_title_->setObjectName("qlHeaderTitle");
    // Derived, not hard-coded: the old "590+" was the sum of the stale
    // per-module counts and overstated the catalog by ~75 endpoints.
    header_sub_ = new QLabel;
    header_sub_->setObjectName("qlHeaderSub");
    title_col->addWidget(header_title_);
    title_col->addWidget(header_sub_);
    hl->addLayout(title_col);
    hl->addStretch(1);

    plan_note_ = new QLabel;
    plan_note_->setObjectName("qlPlanNote");
    hl->addWidget(plan_note_);

    upgrade_btn_ = new QPushButton(tr("UPGRADE"));
    upgrade_btn_->setObjectName("qlUpgradeBtn");
    upgrade_btn_->setCursor(Qt::PointingHandCursor);
    upgrade_btn_->setToolTip(tr("Open Plans & Pricing"));
    connect(upgrade_btn_, &QPushButton::clicked, this, &QuantLibScreen::open_plans);
    hl->addWidget(upgrade_btn_);

    header_badge_ = new QLabel;
    header_badge_->setObjectName("qlPlanBadge");
    hl->addWidget(header_badge_);

    return bar;
}

QWidget* QuantLibScreen::create_sidebar() {
    auto* panel = new QWidget(this);
    panel->setObjectName("qlSidebar");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    sidebar_title_ = new QLabel(tr("MODULES"));
    sidebar_title_->setObjectName("qlSidebarTitle");
    vl->addWidget(sidebar_title_);

    module_tree_ = new QTreeWidget;
    module_tree_->setHeaderHidden(true);
    module_tree_->setIndentation(16);
    module_tree_->setColumnCount(2); // name | access badge
    module_tree_->header()->setStretchLastSection(false);
    module_tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    module_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    for (int i = 0; i < modules_.size(); ++i) {
        const auto& m = modules_[i];
        auto* module_item = new QTreeWidgetItem(module_tree_);
        module_item->setText(0, m.name.toUpper() + " (" + QString::number(m.endpoint_count) + ")");
        module_item->setData(0, Qt::UserRole, i);
        module_item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        module_item->setExpanded(i == 0);

        for (const auto& panel_name : m.panels) {
            auto* panel_item = new QTreeWidgetItem(module_item);
            panel_item->setText(0, panel_name);
            panel_item->setData(0, Qt::UserRole + 1, panel_name);
            panel_item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    connect(module_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (!item)
            return;
        // A module/panel click is a browse action: drop any search filter.
        if (filter_edit_ && !filter_edit_->text().isEmpty()) {
            const QSignalBlocker block(filter_edit_);
            filter_edit_->clear();
        }
        if (!item->parent()) {
            // Module header — clear any sub-panel filter carried over from the
            // previously selected module.
            active_panel_.clear();
            status_panel_->clear();
            on_module_changed(item->data(0, Qt::UserRole).toInt());
        } else {
            const int mod_idx = item->parent()->data(0, Qt::UserRole).toInt();
            if (mod_idx < 0 || mod_idx >= modules_.size())
                return;
            // Set the panel BEFORE on_module_changed() so populate_panels()
            // applies the filter — previously the panel was only a label and the
            // endpoint list never narrowed.
            active_panel_ = item->data(0, Qt::UserRole + 1).toString();
            on_module_changed(mod_idx);
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
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    center_title_ = new QLabel(QStringLiteral("CORE"));
    center_title_->setObjectName("qlCenterTitle");
    vl->addWidget(center_title_);

    auto make_panel = [this](QLabel** title_out, const QString& title) {
        auto* panel = new QWidget(this);
        panel->setObjectName("qlPanel");
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);
        auto* hdr = new QWidget(panel);
        hdr->setObjectName("qlPanelHeader");
        hdr->setFixedHeight(34);
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        *title_out = new QLabel(title, hdr);
        (*title_out)->setObjectName("qlPanelTitle");
        hhl->addWidget(*title_out);
        hhl->addStretch(1);
        pvl->addWidget(hdr);
        auto* body = new QWidget(panel);
        auto* bl = new QVBoxLayout(body);
        bl->setContentsMargins(12, 12, 12, 12);
        bl->setSpacing(8);
        pvl->addWidget(body);
        return std::make_pair(panel, bl);
    };

    // Endpoint picker
    auto [ep_panel, ebl] = make_panel(&endpoint_panel_title_, tr("ENDPOINT"));
    filter_edit_ = new QLineEdit;
    filter_edit_->setClearButtonEnabled(true);
    filter_edit_->setAccessibleName(tr("Search endpoints"));
    connect(filter_edit_, &QLineEdit::textChanged, this, [this]() { populate_panels(active_module_); });
    ebl->addWidget(filter_edit_);

    endpoint_combo_ = new QComboBox;
    endpoint_combo_->setAccessibleName(tr("Endpoint"));
    endpoint_combo_->setMaxVisibleItems(24);
    connect(endpoint_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &QuantLibScreen::on_endpoint_changed);
    ebl->addWidget(endpoint_combo_);

    access_label_ = new QLabel;
    access_label_->setObjectName("qlAccess");
    access_label_->setWordWrap(true);
    ebl->addWidget(access_label_);
    vl->addWidget(ep_panel);

    // Parameters
    auto [param_panel, pbl] = make_panel(&params_title_, tr("REQUEST"));
    param_form_ = new QuantLibParamForm(this);
    connect(param_form_, &QuantLibParamForm::run_requested, this, &QuantLibScreen::on_execute);
    pbl->addWidget(param_form_);

    exec_btn_ = new QPushButton(tr("EXECUTE"));
    exec_btn_->setObjectName("qlExecBtn");
    exec_btn_->setCursor(Qt::PointingHandCursor);
    exec_btn_->setFixedHeight(34);
    exec_btn_->setAccessibleName(tr("Execute computation"));
    connect(exec_btn_, &QPushButton::clicked, this, &QuantLibScreen::on_execute);
    pbl->addWidget(exec_btn_);
    vl->addWidget(param_panel);

    // Ctrl+Enter executes from anywhere in the screen (Enter alone runs from a
    // parameter field; in the raw-JSON editor Enter inserts a newline).
    auto* exec_sc = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    exec_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(exec_sc, &QShortcut::activated, this, &QuantLibScreen::on_execute);
    auto* exec_sc2 = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Enter")), this);
    exec_sc2->setContext(Qt::WidgetWithChildrenShortcut);
    connect(exec_sc2, &QShortcut::activated, this, &QuantLibScreen::on_execute);
    // Ctrl+F jumps to the endpoint search.
    auto* find_sc = new QShortcut(QKeySequence::Find, this);
    find_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(find_sc, &QShortcut::activated, this, [this]() {
        filter_edit_->setFocus();
        filter_edit_->selectAll();
    });

    vl->addStretch(1);
    scroll->setWidget(content);
    return scroll;
}

QWidget* QuantLibScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("qlStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    status_left_ = new QLabel(tr("QUANTLIB SUITE"));
    status_left_->setObjectName("qlStatusText");
    hl->addWidget(status_left_);
    hl->addStretch(1);

    status_module_ = new QLabel(tr("MODULE: %1").arg(QStringLiteral("CORE")));
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

// ── Live language switch ─────────────────────────────────────────────────────

void QuantLibScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void QuantLibScreen::retranslateUi() {
    // Fixed chrome. Module/panel/endpoint names are API-catalog data and are
    // intentionally not translated.
    if (header_title_)
        header_title_->setText(tr("QUANTLIB SUITE"));
    if (header_sub_) {
        int total = 0;
        for (const auto& m : modules_)
            total += m.endpoint_count;
        header_sub_->setText(tr("%1 MODULES | %2 QUANTITATIVE ENDPOINTS").arg(modules_.size()).arg(total));
    }
    if (upgrade_btn_) {
        upgrade_btn_->setText(tr("UPGRADE"));
        upgrade_btn_->setToolTip(tr("Open Plans & Pricing"));
    }
    if (sidebar_title_)
        sidebar_title_->setText(tr("MODULES"));
    if (filter_edit_)
        filter_edit_->setPlaceholderText(tr("Search all endpoints (Ctrl+F) — e.g. garch, implied-vol, swap"));
    if (params_title_)
        params_title_->setText(tr("REQUEST"));
    if (param_form_)
        param_form_->retranslate();
    if (result_view_)
        result_view_->retranslate();
    if (status_left_)
        status_left_->setText(tr("QUANTLIB SUITE"));
    if (exec_btn_) {
        exec_btn_->setToolTip(tr("Run the selected endpoint (Enter in a field, or Ctrl+Enter)"));
        if (!loading_)
            exec_btn_->setText(tr("EXECUTE"));
    }
    // endpoint_panel_title_ carries the live "(n of m)" counts and the combo
    // items carry translated badges — repopulating rebuilds both and keeps the
    // current selection (and the form's unsent edits).
    if (endpoint_panel_title_)
        populate_panels(active_module_);
    refresh_plan_ui();

    // Status module prefix (module name itself is data)
    if (status_module_ && active_module_ >= 0 && active_module_ < modules_.size())
        status_module_->setText(tr("MODULE: %1").arg(modules_[active_module_].name.toUpper()));
}

// ── Plan / access chrome ─────────────────────────────────────────────────────

QuantLibCost QuantLibScreen::cost_of(const QString& endpoint) const {
    if (!paid_plan_ && pro_overrides_.contains(endpoint))
        return QuantLibCost::NeedsPro;
    return services::quantlib_cost(services::quantlib_endpoint_tier(endpoint), paid_plan_);
}

void QuantLibScreen::refresh_plan_ui() {
    paid_plan_ = services::quantlib_paid_plan();
    const bool signed_in = auth::AuthManager::instance().is_authenticated();

    int total = 0;
    for (const auto& m : modules_)
        total += m.endpoint_count;

    if (header_badge_) {
        header_badge_->setText(paid_plan_ ? tr("PRO PLAN") : signed_in ? tr("FREE PLAN") : tr("SIGNED OUT"));
        header_badge_->setProperty("plan", paid_plan_ ? "pro" : "free");
        ql_repolish(header_badge_);
    }
    if (plan_note_) {
        plan_note_->setText(paid_plan_ ? tr("All %1 endpoints included · 0 credits per call").arg(total)
                                       : signed_in ? tr("Unlock every module with Pro — $10/month")
                                                   : tr("Sign in to run QuantLib computations"));
    }
    if (upgrade_btn_)
        upgrade_btn_->setVisible(!paid_plan_);

    refresh_tree_badges();
    decorate_endpoint_items();
    update_access_label();
}

QStringList QuantLibScreen::endpoints_for(int module_index, const QString& panel, bool* filtered) const {
    if (filtered)
        *filtered = false;
    if (module_index < 0 || module_index >= modules_.size())
        return {};
    const auto it = module_endpoints().constFind(modules_[module_index].id);
    const QStringList all = (it != module_endpoints().constEnd()) ? it.value() : QStringList{};
    if (panel.isEmpty())
        return all;
    const QStringList tokens = panel_path_tokens(panel);
    QStringList hits;
    for (const auto& ep : all) {
        for (const auto& t : tokens) {
            if (ep.contains(t, Qt::CaseInsensitive)) {
                hits << ep;
                break;
            }
        }
    }
    if (hits.isEmpty())
        return all;
    if (filtered)
        *filtered = true;
    return hits;
}

void QuantLibScreen::refresh_tree_badges() {
    if (!module_tree_)
        return;
    // One badge per module / panel: FREE, 1 CR or PRO when every endpoint in
    // it costs the same on the current plan, MIXED otherwise (the tooltip has
    // the split). Pro accounts pay 0 credits everywhere: no badges.
    auto badge_for = [this](const QStringList& eps, QTreeWidgetItem* item) {
        if (paid_plan_ || eps.isEmpty()) {
            item->setText(1, QString());
            item->setToolTip(1, QString());
            return;
        }
        int counts[3] = {0, 0, 0};
        for (const auto& ep : eps)
            counts[static_cast<int>(cost_of(ep))] += 1;
        const int kinds = (counts[0] > 0) + (counts[1] > 0) + (counts[2] > 0);
        if (kinds == 1) {
            const auto cost = counts[0] ? QuantLibCost::Included : counts[1] ? QuantLibCost::OneCredit
                                                                              : QuantLibCost::NeedsPro;
            item->setText(1, ql_cost_badge(cost));
            item->setForeground(1, ql_cost_color(cost));
        } else {
            item->setText(1, tr("MIXED"));
            item->setForeground(1, QColor(colors::TEXT_SECONDARY()));
        }
        item->setToolTip(1, tr("Free plan: %1 free · %2 at 1 credit per call · %3 need Pro")
                                .arg(counts[0])
                                .arg(counts[1])
                                .arg(counts[2]));
    };
    for (int i = 0; i < module_tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* mod_item = module_tree_->topLevelItem(i);
        const int mod_idx = mod_item->data(0, Qt::UserRole).toInt();
        badge_for(endpoints_for(mod_idx, QString()), mod_item);
        for (int c = 0; c < mod_item->childCount(); ++c) {
            QTreeWidgetItem* panel_item = mod_item->child(c);
            badge_for(endpoints_for(mod_idx, panel_item->data(0, Qt::UserRole + 1).toString()), panel_item);
        }
    }
}

void QuantLibScreen::decorate_endpoint_items() {
    if (!endpoint_combo_)
        return;
    for (int i = 0; i < endpoint_combo_->count(); ++i) {
        const QString ep = endpoint_combo_->itemData(i).toString();
        if (paid_plan_) {
            endpoint_combo_->setItemText(i, ep);
            endpoint_combo_->setItemData(i, QVariant(), Qt::ForegroundRole);
            endpoint_combo_->setItemData(i, QVariant(), Qt::ToolTipRole);
            continue;
        }
        const auto cost = cost_of(ep);
        endpoint_combo_->setItemText(i, ep + QStringLiteral("    ") + ql_cost_badge(cost));
        endpoint_combo_->setItemData(i, ql_cost_color(cost), Qt::ForegroundRole);
        endpoint_combo_->setItemData(i,
                                     cost == QuantLibCost::Included    ? tr("Free on your plan")
                                     : cost == QuantLibCost::OneCredit ? tr("1 credit per call on the Free plan")
                                                                       : tr("Needs Pro ($10/month)"),
                                     Qt::ToolTipRole);
    }
}

void QuantLibScreen::update_access_label() {
    if (!access_label_)
        return;
    const QString ep = current_endpoint();
    if (ep.isEmpty()) {
        access_label_->clear();
        return;
    }
    QString text;
    QString tier;
    if (paid_plan_) {
        text = tr("INCLUDED IN YOUR PRO PLAN · 0 credits per call");
        tier = QStringLiteral("included");
    } else {
        switch (cost_of(ep)) {
            case QuantLibCost::Included:
                text = tr("FREE · 0 credits on the Free plan");
                tier = QStringLiteral("free");
                break;
            case QuantLibCost::OneCredit:
                text = tr("1 CREDIT PER CALL on the Free plan · included in Pro at 0 credits");
                tier = QStringLiteral("credit");
                break;
            case QuantLibCost::NeedsPro:
                text = tr("PRO · this endpoint needs Pro ($10/month) — on the Free plan the server answers with an "
                          "upgrade notice");
                tier = QStringLiteral("pro");
                break;
        }
    }
    access_label_->setText(text);
    access_label_->setProperty("tier", tier);
    ql_repolish(access_label_);
}

void QuantLibScreen::open_plans() {
    ui::billing::open_plans(this);
}

// ── Slots ───────────────────────────────────────────────────────────────────

QString QuantLibScreen::current_endpoint() const {
    return endpoint_combo_ ? endpoint_combo_->currentData().toString() : QString();
}

void QuantLibScreen::on_module_changed(int index) {
    if (index < 0 || index >= modules_.size())
        return;
    active_module_ = index;

    const auto& m = modules_[index];
    status_module_->setText(tr("MODULE: %1").arg(m.name.toUpper()));
    center_title_->setText(m.name.toUpper());

    populate_panels(index);
    LOG_INFO("QuantLib", "Module: " + m.name);
    ScreenStateManager::instance().notify_changed(this);
}

void QuantLibScreen::on_endpoint_changed(int index) {
    if (index < 0)
        return;
    const QString ep = current_endpoint();
    if (ep.isEmpty())
        return;
    status_endpoint_->setText(ep);

    // A search result can live in another module: follow it in the chrome.
    const QString module_id = ep.section(QLatin1Char('/'), 0, 0);
    if (module_id != modules_.value(active_module_).id) {
        for (int i = 0; i < modules_.size(); ++i) {
            if (modules_[i].id == module_id) {
                active_module_ = i;
                active_panel_.clear();
                status_panel_->clear();
                status_module_->setText(tr("MODULE: %1").arg(modules_[i].name.toUpper()));
                center_title_->setText(modules_[i].name.toUpper());
                break;
            }
        }
    }

    // The form shows the user's last inputs for this endpoint when there are
    // any, else the example body. Switching endpoints never carries one
    // endpoint's payload over to another.
    if (ep != form_endpoint_) {
        form_endpoint_ = ep;
        param_form_->set_endpoint(ep, endpoint_examples().value(ep), last_bodies_.value(ep),
                                  !services::QuantLibClient::is_get_endpoint(ep));
    }
    update_access_label();
    ScreenStateManager::instance().notify_changed(this);
}

void QuantLibScreen::on_execute() {
    if (loading_)
        return;

    const QString endpoint = current_endpoint();
    if (endpoint.isEmpty()) {
        // Used to return silently — the button appeared dead.
        result_view_->show_input_error(tr("Select an endpoint first."));
        return;
    }

    QJsonObject params;
    QString error;
    if (!param_form_->body(&params, &error)) {
        result_view_->show_input_error(error);
        return;
    }

    remember_body(endpoint, params);
    execute_api(endpoint, params);
}

// ── Data ────────────────────────────────────────────────────────────────────
//
// module_endpoints() (the real api.fincept.in endpoint paths) and
// endpoint_examples() live in QuantLibScreen_Data.cpp; module_endpoints() is
// forward-declared near build_modules() above.

void QuantLibScreen::populate_panels(int module_index) {
    if (module_index < 0 || module_index >= modules_.size())
        return;

    const QString search = filter_edit_ ? filter_edit_->text().trimmed() : QString();
    QStringList shown;
    QString title;
    if (!search.isEmpty()) {
        // Search spans every module — 590 endpoints are too many to browse.
        int total = 0;
        for (const auto& m : modules_) {
            const QStringList eps = module_endpoints().value(m.id);
            total += int(eps.size());
            for (const auto& ep : eps) {
                if (ep.contains(search, Qt::CaseInsensitive))
                    shown << ep;
            }
        }
        title = tr("ENDPOINT — SEARCH (%1 of %2)").arg(shown.size()).arg(total);
    } else {
        // Narrow to the active sub-panel when it maps onto a path segment. Falls
        // back to the full list when the panel has no match, so a filter miss
        // can never leave the user staring at an empty endpoint combo.
        bool filtered = false;
        shown = endpoints_for(module_index, active_panel_, &filtered);
        const int all = int(endpoints_for(module_index, QString()).size());
        title = filtered ? tr("ENDPOINT — %1 (%2 of %3)").arg(active_panel_.toUpper()).arg(shown.size()).arg(all)
                         : tr("ENDPOINT (%1)").arg(all);
    }

    // Rebuild without firing on_endpoint_changed() for every intermediate state,
    // and keep the user's endpoint selected when it survives the new list.
    const QString previous = current_endpoint();
    {
        const QSignalBlocker block(endpoint_combo_);
        endpoint_combo_->clear();
        for (const auto& ep : shown)
            endpoint_combo_->addItem(ep, ep);
        const int keep = previous.isEmpty() ? -1 : endpoint_combo_->findData(previous);
        if (keep >= 0)
            endpoint_combo_->setCurrentIndex(keep);
    }
    decorate_endpoint_items();

    if (endpoint_panel_title_)
        endpoint_panel_title_->setText(title);

    // The signal blocker suppressed the index change, so drive the selection
    // explicitly — on_endpoint_changed() only rebuilds the form when the
    // endpoint actually changed, so unsent edits survive a refill.
    if (endpoint_combo_->count() > 0) {
        on_endpoint_changed(endpoint_combo_->currentIndex());
    } else {
        if (status_endpoint_)
            status_endpoint_->clear();
        update_access_label();
    }
}

void QuantLibScreen::remember_body(const QString& endpoint, const QJsonObject& body) {
    last_bodies_.insert(endpoint, body);
    body_order_.removeAll(endpoint);
    body_order_.append(endpoint);
    while (body_order_.size() > kQlRememberedBodies)
        last_bodies_.remove(body_order_.takeFirst());
    ScreenStateManager::instance().notify_changed(this);
}

void QuantLibScreen::execute_api(const QString& endpoint, const QJsonObject& params) {
    set_loading(true);
    const QuantLibCost cost = cost_of(endpoint);
    result_view_->show_loading(endpoint, cost == QuantLibCost::OneCredit ? tr("1 credit") : QString());

    QElapsedTimer timer;
    timer.start();
    QPointer<QuantLibScreen> self = this;
    services::QuantLibClient::instance().call(endpoint, params, [self, endpoint, params,
                                                                 timer](mcp::ToolResult result) {
        if (!self)
            return;
        self->set_loading(false);
        const qint64 elapsed = timer.elapsed();

        if (!result.success) {
            const services::QuantLibFailure failure = services::classify_quantlib_failure(result);
            if (failure.kind == services::QuantLibFailureKind::NeedsPro && !self->pro_overrides_.contains(endpoint)) {
                // The server is authoritative: badge it PRO even if the local table disagreed.
                self->pro_overrides_.insert(endpoint);
                self->refresh_tree_badges();
                self->decorate_endpoint_items();
                self->update_access_label();
            }
            LOG_WARN("QuantLib", QString("%1 failed (HTTP %2): %3")
                                     .arg(endpoint)
                                     .arg(failure.http_status)
                                     .arg(failure.message.left(300)));
            self->result_view_->show_failure(endpoint, failure);
            return;
        }

        // A success envelope can still carry the handler's own error text.
        const QString inline_error = services::quantlib_inline_error(result.data);
        if (!inline_error.isEmpty()) {
            services::QuantLibFailure failure;
            failure.kind = services::QuantLibFailureKind::Computation;
            failure.http_status = 200;
            failure.message = inline_error;
            self->result_view_->show_failure(endpoint, failure);
            return;
        }

        self->result_view_->show_result(endpoint, params, result.data, elapsed);
        LOG_INFO("QuantLib", QString("Executed %1 in %2 ms").arg(endpoint).arg(elapsed));
    });
}

void QuantLibScreen::set_loading(bool loading) {
    loading_ = loading;
    exec_btn_->setEnabled(!loading);
    exec_btn_->setText(loading ? tr("COMPUTING...") : tr("EXECUTE"));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap QuantLibScreen::save_state() const {
    QVariantMap bodies;
    for (const QString& ep : body_order_) {
        const auto it = last_bodies_.constFind(ep);
        if (it != last_bodies_.constEnd())
            bodies.insert(ep, QString::fromUtf8(QJsonDocument(it.value()).toJson(QJsonDocument::Compact)));
    }
    return {
        {"module", active_module_},
        {"panel", active_panel_},
        {"endpoint", current_endpoint()},
        {"bodies", bodies},
        {"body_order", body_order_},
    };
}

void QuantLibScreen::restore_state(const QVariantMap& state) {
    // Remembered inputs first, so the form shows them for the restored endpoint.
    const QVariantMap bodies = state.value("bodies").toMap();
    const QStringList order = state.value("body_order").toStringList();
    for (const QString& ep : order) {
        const QJsonDocument doc = QJsonDocument::fromJson(bodies.value(ep).toString().toUtf8());
        if (doc.isObject() && body_order_.size() < kQlRememberedBodies) {
            last_bodies_.insert(ep, doc.object());
            body_order_.removeAll(ep);
            body_order_.append(ep);
        }
    }

    const int mod = state.value("module", 0).toInt();
    form_endpoint_.clear(); // re-seed the form from the restored inputs
    on_module_changed(mod);

    const QString panel = state.value("panel").toString();
    if (!panel.isEmpty() && module_tree_) {
        // Select the panel under the restored module — panel names repeat across
        // modules ("Time Series" is in both ML and Statistics).
        for (int i = 0; i < module_tree_->topLevelItemCount(); ++i) {
            QTreeWidgetItem* mod_item = module_tree_->topLevelItem(i);
            if (mod_item->data(0, Qt::UserRole).toInt() != active_module_)
                continue;
            for (int c = 0; c < mod_item->childCount(); ++c) {
                if (mod_item->child(c)->text(0) != panel)
                    continue;
                mod_item->setExpanded(true);
                module_tree_->setCurrentItem(mod_item->child(c));
                active_panel_ = panel;
                status_panel_->setText(active_panel_);
                center_title_->setText(modules_[active_module_].name.toUpper() + " / " + active_panel_.toUpper());
                // Re-apply the endpoint filter for the restored panel.
                populate_panels(active_module_);
                break;
            }
            break;
        }
    }

    const QString endpoint = state.value("endpoint").toString();
    if (!endpoint.isEmpty()) {
        const int idx = endpoint_combo_->findData(endpoint);
        if (idx >= 0)
            endpoint_combo_->setCurrentIndex(idx); // emits → on_endpoint_changed
    }
    ScreenStateManager::instance().notify_changed(this);
}

} // namespace fincept::screens
