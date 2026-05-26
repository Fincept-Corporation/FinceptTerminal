// src/screens/quantlib/QuantLibScreen.cpp
//
// QuantLib API browser — modules + endpoints + execute/display. The large
// endpoint catalogs (MODULE_ENDPOINTS → module_endpoints(), endpoint_examples)
// live in QuantLibScreen_Data.cpp so editing them doesn't recompile the UI.
#include "screens/quantlib/QuantLibScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/quantlib/QuantLibClient.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
using namespace fincept::ui;

inline QString kStyle() {
    return QStringLiteral("#qlScreen { background: %1; }"

                          "#qlHeader { background: %2; border-bottom: 2px solid %3; }"
                          "#qlHeaderTitle { color: %4; font-weight: 700; background: transparent; }"
                          "#qlHeaderSub { color: %5; letter-spacing: 0.5px; background: transparent; }"
                          "#qlHeaderBadge { color: %6; font-weight: 700; "
                          "  background: rgba(22,163,74,0.2); padding: 2px 6px; }"

                          "#qlSidebar { background: %7; border-right: 1px solid %8; }"
                          "QTreeWidget { background: %1; color: %5; border: none; }"
                          "QTreeWidget::item { padding: 4px 8px; border-bottom: 1px solid %8; }"
                          "QTreeWidget::item:hover { color: %4; background: %12; }"
                          "QTreeWidget::item:selected { color: %3; background: rgba(217,119,6,0.1); "
                          "  border-left: 2px solid %3; }"

                          "#qlCenterPanel { background: %1; }"
                          "#qlCenterTitle { color: %4; font-weight: 700; background: transparent; }"

                          "#qlPanel { background: %7; border: 1px solid %8; }"
                          "#qlPanelHeader { background: %2; border-bottom: 1px solid %8; }"
                          "#qlPanelTitle { color: %4; font-weight: 700; background: transparent; }"
                          "#qlLabel { color: %5; font-weight: 700; "
                          "  letter-spacing: 0.5px; background: transparent; }"

                          "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
                          "  padding: 4px 8px; }"
                          "QLineEdit:focus { border-color: %9; }"
                          "QComboBox { background: %1; color: %4; border: 1px solid %8; "
                          "  padding: 4px 8px; }"
                          "QComboBox::drop-down { border: none; width: 16px; }"
                          "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                          "  selection-background-color: %3; }"

                          "#qlExecBtn { background: %3; color: %1; border: none; padding: 6px 20px; "
                          "  font-weight: 700; }"
                          "#qlExecBtn:hover { background: %10; }"
                          "#qlExecBtn:disabled { background: %10; color: %11; }"

                          "#qlResultPanel { background: %7; border-left: 1px solid %8; }"
                          "#qlResultStatus { color: %5; background: transparent; }"
                          "QTextEdit { background: %1; color: %13; border: none; }"
                          "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
                          "  }"
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
        ;
}
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Module definitions ──────────────────────────────────────────────────────

static QList<QuantModule> build_modules() {
    return {
        {"core",
         "Core",
         51,
         {"Types", "Conventions", "AutoDiff", "Distributions", "Math", "Operations", "Legs", "Periods"}},
        {"analysis",
         "Analysis",
         122,
         {"Fundamentals", "Profitability", "Liquidity", "Efficiency", "Growth", "Leverage", "Valuation Ratios",
          "DCF Valuation"}},
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
}

// ── Constructor ─────────────────────────────────────────────────────────────

QuantLibScreen::QuantLibScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("qlScreen");
    setStyleSheet(kStyle());
    modules_ = build_modules();
    setup_ui();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { setStyleSheet(kStyle()); });
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
    auto* bar = new QWidget(this);
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
    auto* panel = new QWidget(this);
    panel->setObjectName("qlSidebar");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* title = new QLabel("MODULES");
    title->setStyleSheet(QString("color: %1; font-weight: 700; "
                                 "letter-spacing: 0.5px; background: transparent; "
                                 "padding: 8px 12px; border-bottom: 1px solid %2;")
                             .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM()));
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
        if (!item)
            return;
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

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    center_title_ = new QLabel("CORE");
    center_title_->setObjectName("qlCenterTitle");
    vl->addWidget(center_title_);

    // Endpoint selector
    auto* ep_panel = new QWidget(this);
    ep_panel->setObjectName("qlPanel");
    auto* epvl = new QVBoxLayout(ep_panel);
    epvl->setContentsMargins(0, 0, 0, 0);
    epvl->setSpacing(0);

    auto* ephdr = new QWidget(this);
    ephdr->setObjectName("qlPanelHeader");
    ephdr->setFixedHeight(34);
    auto* ephl = new QHBoxLayout(ephdr);
    ephl->setContentsMargins(12, 0, 12, 0);
    auto* ept = new QLabel("ENDPOINT");
    ept->setObjectName("qlPanelTitle");
    ephl->addWidget(ept);
    ephl->addStretch(1);
    epvl->addWidget(ephdr);

    auto* ep_body = new QWidget(this);
    auto* ebl = new QVBoxLayout(ep_body);
    ebl->setContentsMargins(12, 12, 12, 12);
    ebl->setSpacing(10);

    endpoint_combo_ = new QComboBox;
    ebl->addWidget(endpoint_combo_);

    // JSON body input — users enter the exact fields the API expects
    auto* json_label = new QLabel("REQUEST BODY (JSON)");
    json_label->setObjectName("qlLabel");
    ebl->addWidget(json_label);

    param_input1_ = new QLineEdit;
    param_input1_->setPlaceholderText(
        "{\"spot\": 100, \"strike\": 105, \"r\": 0.05, \"sigma\": 0.2, \"T\": 1.0, \"option_type\": \"call\"}");
    param_input1_->setMinimumHeight(28);
    ebl->addWidget(param_input1_);

    // Quick-fill helpers
    auto* helpers = new QHBoxLayout;
    helpers->setSpacing(4);

    auto add_helper = [&](const QString& label, const QString& json_text) {
        auto* btn = new QPushButton(label);
        btn->setObjectName("qlLabel");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                    "padding: 2px 8px; font-weight: 700; } "
                    "QPushButton:hover { color: %4; }")
                .arg(colors::BG_SURFACE(), colors::TEXT_SECONDARY(), colors::BORDER_DIM(), colors::TEXT_PRIMARY()));
        connect(btn, &QPushButton::clicked, param_input1_, [this, json_text]() { param_input1_->setText(json_text); });
        helpers->addWidget(btn);
    };

    add_helper("BS Price", "{\"spot\":100,\"strike\":105,\"risk_free_rate\":0.05,\"volatility\":0.2,\"time_to_"
                           "maturity\":1.0,\"option_type\":\"call\"}");
    add_helper("GBM Sim", "{\"S0\":100,\"mu\":0.05,\"sigma\":0.2,\"T\":1.0,\"n_steps\":52,\"n_paths\":5}");
    add_helper("VaR", "{\"portfolio_value\":1000000,\"volatility\":0.02,\"confidence\":0.99,\"horizon\":1}");
    add_helper("Heston", "{\"spot\":100,\"strike\":105,\"r\":0.05,\"T\":1.0,\"v0\":0.04,\"kappa\":1.5,"
                         "\"theta\":0.04,\"sigma_v\":0.3,\"rho\":-0.7,\"option_type\":\"call\"}");
    helpers->addStretch(1);
    ebl->addLayout(helpers);

    // Hide unused inputs
    param_input2_ = new QLineEdit;
    param_input2_->hide();
    param_input3_ = new QLineEdit;
    param_input3_->hide();
    param_input4_ = new QLineEdit;
    param_input4_->hide();

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
    auto* panel = new QWidget(this);
    panel->setObjectName("qlResultPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget(this);
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
    auto* bar = new QWidget(this);
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
    if (index < 0 || index >= modules_.size())
        return;
    active_module_ = index;

    const auto& m = modules_[index];
    status_module_->setText("MODULE: " + m.name.toUpper());
    center_title_->setText(m.name.toUpper());

    populate_panels(index);
    LOG_INFO("QuantLib", "Module: " + m.name);
    ScreenStateManager::instance().notify_changed(this);
}

void QuantLibScreen::on_panel_changed(QListWidgetItem* item) {
    if (!item)
        return;
    active_panel_ = item->text();
    status_panel_->setText(active_panel_);
    ScreenStateManager::instance().notify_changed(this);
}

void QuantLibScreen::on_endpoint_changed(int index) {
    if (index < 0)
        return;
    const QString ep = endpoint_combo_->currentText();
    status_endpoint_->setText(ep);

    // Auto-fill example body if user hasn't typed anything
    if (param_input1_->text().trimmed().isEmpty()) {
        const auto& examples = endpoint_examples();
        auto it = examples.find(ep);
        if (it != examples.end())
            param_input1_->setText(it.value());
        else
            param_input1_->clear();
    }
}

void QuantLibScreen::on_execute() {
    if (loading_)
        return;

    QString endpoint = endpoint_combo_->currentText();
    if (endpoint.isEmpty())
        return;

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

// MODULE_ENDPOINTS catalog + endpoint_examples() definition live in
// QuantLibScreen_Data.cpp. module_endpoints() is the free-function accessor
// used by populate_panels(); endpoint_examples() is a static member.
// Forward declaration — we're already inside `namespace fincept::screens` (opened
// at line 100), so no extra namespace wrapping.
const QHash<QString, QStringList>& module_endpoints();

void QuantLibScreen::populate_panels(int module_index) {
    endpoint_combo_->clear();

    const auto& m = modules_[module_index];
    auto it = module_endpoints().find(m.id);
    if (it != module_endpoints().end()) {
        for (const auto& ep : it.value()) {
            endpoint_combo_->addItem(ep);
        }
    }

    connect(endpoint_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &QuantLibScreen::on_endpoint_changed, Qt::UniqueConnection);
}

void QuantLibScreen::execute_api(const QString& endpoint, const QJsonObject& params) {
    set_loading(true);
    result_status_->setText("Computing...");

    QPointer<QuantLibScreen> self = this;
    services::QuantLibClient::instance().call(endpoint, params, [self, endpoint](mcp::ToolResult result) {
        if (!self)
            return;

        self->set_loading(false);

        if (!result.success) {
            self->display_error(result.error);
            return;
        }

        auto payload = result.data;
        if (payload.isArray())
            self->display_result_array(payload.toArray());
        else if (payload.isObject())
            self->display_result(payload.toObject());
        else
            self->display_result(QJsonObject{}); // empty / null payload

        self->status_endpoint_->setText(endpoint);
        LOG_INFO("QuantLib", "Executed: " + endpoint);
    });
}

// ── Display ─────────────────────────────────────────────────────────────────

void QuantLibScreen::display_result_array(const QJsonArray& arr) {
    if (arr.isEmpty()) {
        result_view_->setPlainText("[]");
        result_stack_->setCurrentIndex(0);
        result_status_->setText("Empty");
        return;
    }

    if (arr[0].isObject()) {
        // Array of objects → table
        auto first = arr[0].toObject();
        QStringList cols;
        for (auto it = first.begin(); it != first.end(); ++it)
            cols << it.key();

        result_table_->setSortingEnabled(false);
        result_table_->setColumnCount(cols.size());
        result_table_->setHorizontalHeaderLabels(cols);

        int rows = qMin(arr.size(), 1000);
        result_table_->setRowCount(rows);

        for (int r = 0; r < rows; ++r) {
            auto obj = arr[r].toObject();
            for (int c = 0; c < cols.size(); ++c) {
                auto val = obj.value(cols[c]);
                QString text = val.isDouble() ? QString::number(val.toDouble(), 'g', 10)
                               : val.isNull() ? "--"
                                              : val.toVariant().toString();
                auto* item = new QTableWidgetItem(text);
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                if (val.isDouble())
                    item->setForeground(QColor(val.toDouble() < 0 ? colors::NEGATIVE() : colors::CYAN()));
                result_table_->setItem(r, c, item);
            }
        }
        result_table_->resizeColumnsToContents();
        result_table_->setSortingEnabled(true);
        result_stack_->setCurrentIndex(1);
        result_status_->setText(QString::number(rows) + " results");
    } else {
        // Array of scalars (e.g. currency list, calendar list) → two-column table: Index | Value
        result_table_->setSortingEnabled(false);
        result_table_->setColumnCount(2);
        result_table_->setHorizontalHeaderLabels({"#", "Value"});
        int rows = qMin(arr.size(), 1000);
        result_table_->setRowCount(rows);
        for (int r = 0; r < rows; ++r) {
            result_table_->setItem(r, 0, new QTableWidgetItem(QString::number(r + 1)));
            result_table_->setItem(r, 1, new QTableWidgetItem(arr[r].toVariant().toString()));
        }
        result_table_->resizeColumnsToContents();
        result_table_->setSortingEnabled(true);
        result_stack_->setCurrentIndex(1);
        result_status_->setText(QString::number(rows) + " items");
    }
}

void QuantLibScreen::display_result(const QJsonObject& result) {
    // result is already the unwrapped payload from QuantLibClient
    if (result.isEmpty()) {
        result_view_->setPlainText("(empty response)");
        result_stack_->setCurrentIndex(0);
        result_status_->setText("OK");
        return;
    }

    { // flat object → Key/Value table
        // Flat object result (e.g. {"price": 8.02, "delta": 0.45, ...}) → two-column table: Key | Value
        auto obj = result;
        if (!obj.isEmpty()) {
            result_table_->setSortingEnabled(false);
            result_table_->setColumnCount(2);
            result_table_->setHorizontalHeaderLabels({"Field", "Value"});
            result_table_->setRowCount(obj.size());
            int r = 0;
            for (auto it = obj.begin(); it != obj.end(); ++it, ++r) {
                auto* key_item = new QTableWidgetItem(it.key());
                key_item->setForeground(QColor(colors::TEXT_SECONDARY()));
                result_table_->setItem(r, 0, key_item);

                QString text;
                auto val = it.value();
                if (val.isDouble())
                    text = QString::number(val.toDouble(), 'g', 10);
                else if (val.isBool())
                    text = val.toBool() ? "true" : "false";
                else if (val.isNull() || val.isUndefined())
                    text = "--";
                else if (val.isArray() || val.isObject())
                    text = QJsonDocument(val.isArray() ? QJsonDocument(val.toArray()) : QJsonDocument(val.toObject()))
                               .toJson(QJsonDocument::Compact);
                else
                    text = val.toString();

                auto* val_item = new QTableWidgetItem(text);
                val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                if (val.isDouble())
                    val_item->setForeground(QColor(val.toDouble() < 0 ? colors::NEGATIVE() : colors::CYAN()));
                result_table_->setItem(r, 1, val_item);
            }
            result_table_->resizeColumnsToContents();
            result_table_->setSortingEnabled(true);
            result_stack_->setCurrentIndex(1);
            result_status_->setText(QString::number(obj.size()) + " fields");
            return;
        }
    }

    // Fallback: raw JSON
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

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap QuantLibScreen::save_state() const {
    return {
        {"module", active_module_},
        {"panel", active_panel_},
    };
}

void QuantLibScreen::restore_state(const QVariantMap& state) {
    const int mod = state.value("module", 0).toInt();
    if (mod != active_module_)
        on_module_changed(mod);

    const QString panel = state.value("panel").toString();
    if (!panel.isEmpty() && module_tree_) {
        // Find matching item in the tree and select it
        auto items = module_tree_->findItems(panel, Qt::MatchExactly | Qt::MatchRecursive);
        if (!items.isEmpty()) {
            module_tree_->setCurrentItem(items.first());
            // on_panel_changed expects QListWidgetItem* — extract text directly
            active_panel_ = items.first()->text(0);
            status_panel_->setText(active_panel_);
            ScreenStateManager::instance().notify_changed(this);
        }
    }
}

} // namespace fincept::screens
