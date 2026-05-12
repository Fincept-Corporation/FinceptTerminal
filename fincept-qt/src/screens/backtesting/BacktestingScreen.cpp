// src/screens/backtesting/BacktestingScreen.cpp
#include "screens/backtesting/BacktestingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "services/backtesting/BacktestingService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSplitter>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace fincept::services::backtest;

// ── Helpers ─────────────────────────────────────────────────────────────────

static QString fmt_metric(const QString& key, const QJsonValue& val) {
    if (val.isString())
        return val.toString();
    if (val.isBool())
        return val.toBool() ? "YES" : "NO";
    if (!val.isDouble())
        return QString::fromUtf8("\u2014");

    double v = val.toDouble();

    // Count metrics → integer
    if (count_metric_keys().contains(key))
        return QString::number(static_cast<int>(v));

    // Percentage metrics → show as "xx.xx%"
    if (pct_metric_keys().contains(key)) {
        // If value is in 0-1 range, multiply by 100
        if (std::abs(v) <= 1.0)
            return QString("%1%").arg(v * 100.0, 0, 'f', 2);
        return QString("%1%").arg(v, 0, 'f', 2);
    }

    // Ratio metrics → show as-is with 2-4 decimals
    if (ratio_metric_keys().contains(key))
        return QString::number(v, 'f', std::abs(v) >= 10.0 ? 2 : 4);

    // Currency-like large values
    if (std::abs(v) >= 1e9)
        return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
    if (std::abs(v) >= 1e6)
        return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
    if (std::abs(v) >= 1e3)
        return QString("$%1K").arg(v / 1e3, 0, 'f', 0);

    return QString::number(v, 'f', 4);
}

static void clear_layout(QLayout* layout) {
    if (!layout)
        return;
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (auto* w = item->widget()) {
            w->setParent(nullptr);
            w->deleteLater();
        } else if (auto* sub = item->layout()) {
            clear_layout(sub);
        }
        delete item;
    }
}

// ── Constructor ──────────────────────────────────────────────────────────────

BacktestingScreen::BacktestingScreen(QWidget* parent) : QWidget(parent) {
    providers_ = all_providers();
    commands_ = all_commands();
    // strategies_ starts empty — populated dynamically per provider via load_strategies()
    build_ui();
    connect_service();
    LOG_INFO("Backtesting", "Screen constructed");
}

void BacktestingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_provider_changed(0);
        on_command_changed(0);
    }
}

void BacktestingScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

void BacktestingScreen::connect_service() {
    auto& svc = BacktestingService::instance();
    connect(&svc, &BacktestingService::result_ready, this, &BacktestingScreen::on_result);
    connect(&svc, &BacktestingService::error_occurred, this, &BacktestingScreen::on_error);
    connect(&svc, &BacktestingService::command_options_loaded, this, &BacktestingScreen::on_command_options_loaded);
}

void BacktestingScreen::set_status_state(const QString& text, const QString& color, const QString& bg_rgba) {
    status_label_->setText(text);
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(color)
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
    status_dot_->setText(text);
    status_dot_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                       "padding:3px 8px; background:%4;"
                                       "border:1px solid %5;")
                                   .arg(color)
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(bg_rgba)
                                   .arg(bg_rgba.isEmpty() ? QString("transparent") : color));
}

void BacktestingScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));
    splitter->addWidget(build_left_panel());
    splitter->addWidget(build_center_panel());
    splitter->addWidget(build_right_panel());
    splitter->setSizes({220, 600, 280});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setCollapsible(2, false);

    root->addWidget(splitter, 1);
    root->addWidget(build_status_bar());

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
}

// ── Top Bar: Provider tabs + Run button ──────────────────────────────────────

namespace {

// Every chip on the top bar (brand label, provider tabs, RUN, status) shares
// the same outer geometry. The global QSS in ThemeManager applies a default
// `padding` to every QPushButton — we override that here with explicit
// min-height / max-height / padding so all chips render at exactly the same
// box size regardless of widget type or selection state.
constexpr int kPillHeight = 24;
constexpr int kPillPadH = 10;

QString pill_qss(const QString& selector,
                 const QString& fg,
                 const QString& bg,
                 const QString& border,
                 int font_px,
                 const QString& font_family,
                 const QString& weight = "700") {
    // Subtract 2px (the 1px top + bottom border) from min/max-height so the
    // total outer box equals kPillHeight in Qt's stylesheet box model.
    return QString("%1 {"
                   "  color:%2; background:%3; border:1px solid %4;"
                   "  font-family:%5; font-size:%6px; font-weight:%7;"
                   "  padding:0 %8px;"
                   "  min-height:%9px; max-height:%9px;"
                   "}")
        .arg(selector, fg, bg, border, font_family)
        .arg(font_px)
        .arg(weight)
        .arg(kPillPadH)
        .arg(kPillHeight - 2);
}

void apply_pill_geometry(QWidget* w) {
    w->setFixedHeight(kPillHeight);
    w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    w->setContentsMargins(0, 0, 0, 0);
}

} // namespace

QWidget* BacktestingScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(34);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    const QString font_family = ui::fonts::DATA_FAMILY;
    const int font_px = ui::fonts::TINY;

    // Brand chip
    auto* brand = new QLabel("BACKTESTING", bar);
    brand->setAlignment(Qt::AlignCenter);
    apply_pill_geometry(brand);
    brand->setStyleSheet(pill_qss("QLabel",
                                  ui::colors::AMBER(),
                                  "rgba(217,119,6,0.1)",
                                  ui::colors::AMBER_DIM(),
                                  font_px, font_family));
    hl->addWidget(brand);

    auto* div = new QWidget(bar);
    div->setFixedSize(1, 18);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    hl->addWidget(div);

    // Provider tabs — geometry pinned here, colour applied later in
    // update_provider_buttons() using the same pill_qss template.
    for (int i = 0; i < providers_.size(); ++i) {
        const auto& p = providers_[i];
        auto* btn = new QPushButton(p.display_name, bar);
        btn->setCursor(Qt::PointingHandCursor);
        apply_pill_geometry(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_provider_changed(i); });
        hl->addWidget(btn);
        provider_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Run button
    run_button_ = new QPushButton("RUN", bar);
    run_button_->setCursor(Qt::PointingHandCursor);
    apply_pill_geometry(run_button_);
    run_button_->setStyleSheet(
        pill_qss("QPushButton",
                 ui::colors::AMBER(),
                 "rgba(217,119,6,0.1)",
                 ui::colors::AMBER_DIM(),
                 font_px, font_family) +
        QString("QPushButton:hover { background:%1; color:%2; }"
                "QPushButton:disabled { background:%3; color:%4; border-color:%5; }")
            .arg(ui::colors::AMBER(), ui::colors::BG_BASE(),
                 ui::colors::BG_RAISED(), ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM()));
    connect(run_button_, &QPushButton::clicked, this, &BacktestingScreen::on_run);
    hl->addWidget(run_button_);

    // Status chip
    status_dot_ = new QLabel("READY", bar);
    status_dot_->setAlignment(Qt::AlignCenter);
    apply_pill_geometry(status_dot_);
    status_dot_->setStyleSheet(pill_qss("QLabel",
                                        ui::colors::POSITIVE(),
                                        "rgba(22,163,74,0.08)",
                                        "rgba(22,163,74,0.25)",
                                        font_px, font_family));
    hl->addWidget(status_dot_);

    return bar;
}

// ── Left Panel: Commands + Strategy Selector + Params ────────────────────────

QWidget* BacktestingScreen::build_left_panel() {
    auto* panel = new QWidget(this);
    panel->setMinimumWidth(180);
    panel->setMaximumWidth(320);
    panel->setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border:none; background:transparent; }"
                                  "QScrollBar:vertical { width:5px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%1; }"
                                  "QScrollBar::handle:vertical:hover { background:%2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                              .arg(ui::colors::BORDER_MED())
                              .arg(ui::colors::BORDER_BRIGHT()));

    auto* content = new QWidget(scroll);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(4);

    auto label_style = QString("color:%1; font-size:%2px; font-weight:700;"
                               "font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY);

    auto section_style = QString("color:%1; font-size:%2px; font-weight:700;"
                                 "font-family:%3; letter-spacing:1px; padding-bottom:4px;"
                                 "border-bottom:1px solid %4;")
                             .arg(ui::colors::AMBER())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::colors::BORDER_DIM());

    // ── Commands section ──
    auto* cmd_title = new QLabel("COMMANDS", content);
    cmd_title->setStyleSheet(section_style);
    vl->addWidget(cmd_title);

    const auto& cmds = commands_;
    for (int i = 0; i < cmds.size(); ++i) {
        const auto& cmd = cmds[i];
        auto* btn = new QPushButton(cmd.label, content);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_command_changed(i); });
        vl->addWidget(btn);
        command_buttons_.append(btn);
    }

    // ── Strategy block (only meaningful for backtest/optimize/walk_forward) ──
    // Wrapped in a named container so update_section_visibility() can hide it
    // when the active command doesn't run a strategy (ML labels, splits, etc.).
    strategy_section_ = new QWidget(content);
    auto* strat_outer = new QVBoxLayout(strategy_section_);
    strat_outer->setContentsMargins(0, 8, 0, 0);
    strat_outer->setSpacing(4);

    auto* strat_title = new QLabel("STRATEGY", strategy_section_);
    strat_title->setStyleSheet(section_style);
    strat_outer->addWidget(strat_title);

    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                               "selection-background-color:rgba(217,119,6,0.15); }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    auto* cat_lbl = new QLabel("CATEGORY", strategy_section_);
    cat_lbl->setStyleSheet(label_style);
    strat_outer->addWidget(cat_lbl);
    strategy_category_combo_ = new QComboBox(strategy_section_);
    strategy_category_combo_->setStyleSheet(combo_style);
    connect(strategy_category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { populate_strategies(); });
    strat_outer->addWidget(strategy_category_combo_);

    auto* strat_lbl = new QLabel("STRATEGY", strategy_section_);
    strat_lbl->setStyleSheet(label_style);
    strat_outer->addWidget(strat_lbl);
    strategy_combo_ = new QComboBox(strategy_section_);
    strategy_combo_->setStyleSheet(combo_style);
    connect(strategy_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &BacktestingScreen::on_strategy_changed);
    strat_outer->addWidget(strategy_combo_);

    auto* params_title = new QLabel("PARAMETERS", strategy_section_);
    params_title->setStyleSheet(label_style);
    strat_outer->addWidget(params_title);

    strategy_params_container_ = new QWidget(strategy_section_);
    strategy_params_layout_ = new QVBoxLayout(strategy_params_container_);
    strategy_params_layout_->setContentsMargins(0, 0, 0, 0);
    strategy_params_layout_->setSpacing(4);
    strat_outer->addWidget(strategy_params_container_);

    vl->addWidget(strategy_section_);

    vl->addStretch();
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(panel);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    return panel;
}

// ── Center: Tabbed Results Display ───────────────────────────────────────────

QWidget* BacktestingScreen::build_center_panel() {
    auto* panel = new QWidget(this);
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(panel);
    header->setFixedHeight(34);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("RESULTS", header);
    title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                             .arg(ui::colors::AMBER())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();

    auto* export_btn = new QPushButton("EXPORT JSON", header);
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setFixedHeight(22);
    export_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                                      "padding:0 10px; font-size:%3px; font-family:%4; }"
                                      "QPushButton:hover { color:%5; border-color:%5; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                                  .arg(ui::fonts::TINY)
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::AMBER()));
    connect(export_btn, &QPushButton::clicked, this, [this]() {
        QString text = raw_json_edit_ ? raw_json_edit_->toPlainText() : QString();
        if (text.trimmed().isEmpty())
            return;

        QString path = QFileDialog::getSaveFileName(this, "Export Backtest Results", "backtest_results.json",
                                                    "JSON Files (*.json);;All Files (*)");
        if (path.isEmpty())
            return;

        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(text.toUtf8());
            f.close();
            services::FileManagerService::instance().import_file(path, "backtesting");
            LOG_INFO("Backtesting", "Exported results to: " + path);
        }
    });
    hhl->addWidget(export_btn);

    vl->addWidget(header);

    // Tab widget for results
    result_tabs_ = new QTabWidget(panel);
    result_tabs_->setStyleSheet(
        QString("QTabWidget::pane { border:none; background:%1; }"
                "QTabBar::tab { background:transparent; color:%2; font-family:%3; font-size:%4px;"
                "font-weight:600; padding:6px 14px; border:none; letter-spacing:0.5px; }"
                "QTabBar::tab:selected { color:%5; background:%6; }"
                "QTabBar::tab:hover { color:%7; background:%8; }")
            .arg(ui::colors::BG_BASE())
            .arg(ui::colors::TEXT_TERTIARY())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL)
            .arg(ui::colors::TEXT_PRIMARY())
            .arg(ui::colors::ORANGE())
            .arg(ui::colors::TEXT_SECONDARY())
            .arg(ui::colors::BG_RAISED()));

    // SUMMARY tab
    auto* summary_scroll = new QScrollArea;
    summary_scroll->setWidgetResizable(true);
    summary_scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");
    summary_container_ = new QWidget(this);
    summary_layout_ = new QVBoxLayout(summary_container_);
    summary_layout_->setContentsMargins(16, 16, 16, 16);
    summary_layout_->setSpacing(12);

    // Initial hint
    auto* hint = new QLabel("Select a provider, command, and strategy, then click RUN to execute.\n\n"
                            "Supported providers: VectorBT, Backtesting.py, FastTrade, Zipline, BT, Fincept\n"
                            "Commands: Backtest, Optimize, Walk-Forward, Indicators, ML Labels, CV Splits, Returns");
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; line-height:1.6;"
                                "padding:20px; background:%4; border:1px solid %5;")
                            .arg(ui::colors::TEXT_SECONDARY())
                            .arg(ui::fonts::SMALL)
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::colors::BG_RAISED())
                            .arg(ui::colors::BORDER_DIM()));
    summary_layout_->addWidget(hint);
    summary_layout_->addStretch();
    summary_scroll->setWidget(summary_container_);
    result_tabs_->addTab(summary_scroll, "SUMMARY");

    // METRICS tab
    metrics_table_ = new QTableWidget(0, 2);
    metrics_table_->setHorizontalHeaderLabels({"Metric", "Value"});
    metrics_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    metrics_table_->setAlternatingRowColors(true);
    metrics_table_->horizontalHeader()->setStretchLastSection(true);
    metrics_table_->verticalHeader()->setVisible(false);
    metrics_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                          "font-family:%4; font-size:%5px; border:none; }"
                                          "QTableWidget::item { padding:3px 8px; }"
                                          "QHeaderView::section { background:%6; color:%7; font-weight:700;"
                                          "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                          "QTableWidget::item:alternate { background:%8; }")
                                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                                      .arg(ui::fonts::DATA_FAMILY)
                                      .arg(ui::fonts::SMALL)
                                      .arg(ui::colors::BG_RAISED())
                                      .arg(ui::colors::TEXT_SECONDARY())
                                      .arg(ui::colors::ROW_ALT()));
    result_tabs_->addTab(metrics_table_, "METRICS");

    // TRADES tab
    trades_table_ = new QTableWidget(0, 0);
    trades_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trades_table_->setAlternatingRowColors(true);
    trades_table_->horizontalHeader()->setStretchLastSection(true);
    trades_table_->verticalHeader()->setVisible(false);
    trades_table_->setStyleSheet(metrics_table_->styleSheet());
    result_tabs_->addTab(trades_table_, "DETAILS");

    // RAW JSON tab
    raw_json_edit_ = new QTextEdit;
    raw_json_edit_->setReadOnly(true);
    raw_json_edit_->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:none;"
                                          "font-family:%3; font-size:%4px; padding:12px; }")
                                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY())
                                      .arg(ui::fonts::DATA_FAMILY)
                                      .arg(ui::fonts::SMALL));
    result_tabs_->addTab(raw_json_edit_, "RAW JSON");

    vl->addWidget(result_tabs_, 1);
    return panel;
}

// ── Right Panel: Config ──────────────────────────────────────────────────────

QWidget* BacktestingScreen::build_right_panel() {
    auto* panel = new QWidget(this);
    panel->setMinimumWidth(200);
    panel->setMaximumWidth(380);
    panel->setStyleSheet(
        QString("background:%1; border-left:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border:none; background:transparent; }"
                                  "QScrollBar:vertical { width:5px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%1; }"
                                  "QScrollBar::handle:vertical:hover { background:%2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                              .arg(ui::colors::BORDER_MED())
                              .arg(ui::colors::BORDER_BRIGHT()));

    auto* content = new QWidget(scroll);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(4);

    auto label_style = QString("color:%1; font-size:%2px; font-weight:700;"
                               "font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY);

    auto section_style = QString("color:%1; font-size:%2px; font-weight:700;"
                                 "font-family:%3; letter-spacing:1px; padding-bottom:4px;"
                                 "border-bottom:1px solid %4;")
                             .arg(ui::colors::AMBER())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::colors::BORDER_DIM());

    auto input_style = QString("QLineEdit, QDoubleSpinBox, QSpinBox, QDateEdit { background:%1; color:%2;"
                               "border:1px solid %3; font-family:%4; font-size:%5px; padding:4px 6px; }"
                               "QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QDateEdit:focus"
                               "{ border-color:%6; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::colors::BORDER_BRIGHT());

    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                               "selection-background-color:rgba(217,119,6,0.15); }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    // Each section is a named QWidget so update_section_visibility() can
    // toggle whole blocks at once depending on the active command.

    // ── MARKET DATA (always shown) ──
    market_data_section_ = new QWidget(content);
    auto* mkt_layout = new QVBoxLayout(market_data_section_);
    mkt_layout->setContentsMargins(0, 0, 0, 0);
    mkt_layout->setSpacing(4);

    auto* mkt_title = new QLabel("MARKET DATA", market_data_section_);
    mkt_title->setStyleSheet(section_style);
    mkt_layout->addWidget(mkt_title);

    auto* sym_lbl = new QLabel("SYMBOLS", market_data_section_);
    sym_lbl->setStyleSheet(label_style);
    mkt_layout->addWidget(sym_lbl);
    symbols_edit_ = new QLineEdit("SPY", market_data_section_);
    symbols_edit_->setPlaceholderText("SPY,AAPL,MSFT");
    symbols_edit_->setStyleSheet(input_style);
    mkt_layout->addWidget(symbols_edit_);
    connect(symbols_edit_, &QLineEdit::editingFinished, this,
            [this]() { publish_first_symbol_to_group(); });

    auto* dates = new QGridLayout;
    dates->setSpacing(8);
    auto* sd_lbl = new QLabel("START", market_data_section_);
    sd_lbl->setStyleSheet(label_style);
    dates->addWidget(sd_lbl, 0, 0);
    start_date_ = new QDateEdit(QDate::currentDate().addYears(-1), market_data_section_);
    start_date_->setDisplayFormat("yyyy-MM-dd");
    start_date_->setCalendarPopup(true);
    start_date_->setStyleSheet(input_style);
    dates->addWidget(start_date_, 1, 0);
    auto* ed_lbl = new QLabel("END", market_data_section_);
    ed_lbl->setStyleSheet(label_style);
    dates->addWidget(ed_lbl, 0, 1);
    end_date_ = new QDateEdit(QDate::currentDate().addDays(-1), market_data_section_);
    end_date_->setDisplayFormat("yyyy-MM-dd");
    end_date_->setCalendarPopup(true);
    end_date_->setStyleSheet(input_style);
    dates->addWidget(end_date_, 1, 1);
    mkt_layout->addLayout(dates);

    vl->addWidget(market_data_section_);

    // ── EXECUTION (backtest-family only) ──
    // Holds the cost/cash-flow knobs: initial capital, commission, slippage.
    execution_section_ = new QWidget(content);
    auto* exec_layout = new QVBoxLayout(execution_section_);
    exec_layout->setContentsMargins(0, 8, 0, 0);
    exec_layout->setSpacing(4);

    auto* exec_title = new QLabel("EXECUTION", execution_section_);
    exec_title->setStyleSheet(section_style);
    exec_layout->addWidget(exec_title);

    auto* cap_lbl = new QLabel("INITIAL CAPITAL ($)", execution_section_);
    cap_lbl->setStyleSheet(label_style);
    exec_layout->addWidget(cap_lbl);
    capital_spin_ = new QDoubleSpinBox(execution_section_);
    capital_spin_->setRange(1000, 1e12);
    capital_spin_->setValue(100000);
    capital_spin_->setDecimals(0);
    capital_spin_->setStyleSheet(input_style);
    exec_layout->addWidget(capital_spin_);

    auto* comm_lbl = new QLabel("COMMISSION (%)", execution_section_);
    comm_lbl->setStyleSheet(label_style);
    exec_layout->addWidget(comm_lbl);
    commission_spin_ = new QDoubleSpinBox(execution_section_);
    commission_spin_->setRange(0, 10);
    commission_spin_->setValue(0.1);
    commission_spin_->setDecimals(3);
    commission_spin_->setSuffix("%");
    commission_spin_->setStyleSheet(input_style);
    exec_layout->addWidget(commission_spin_);

    auto* slip_lbl = new QLabel("SLIPPAGE (%)", execution_section_);
    slip_lbl->setStyleSheet(label_style);
    exec_layout->addWidget(slip_lbl);
    slippage_spin_ = new QDoubleSpinBox(execution_section_);
    slippage_spin_->setRange(0, 10);
    slippage_spin_->setValue(0.05);
    slippage_spin_->setDecimals(4);
    slippage_spin_->setSuffix("%");
    slippage_spin_->setStyleSheet(input_style);
    exec_layout->addWidget(slippage_spin_);

    vl->addWidget(execution_section_);

    // ── ADVANCED (backtest-family only) ──
    advanced_section_ = new QWidget(content);
    auto* adv_layout = new QVBoxLayout(advanced_section_);
    adv_layout->setContentsMargins(0, 8, 0, 0);
    adv_layout->setSpacing(4);

    auto* adv_title = new QLabel("ADVANCED", advanced_section_);
    adv_title->setStyleSheet(section_style);
    adv_layout->addWidget(adv_title);

    auto* lev_lbl = new QLabel("LEVERAGE", advanced_section_);
    lev_lbl->setStyleSheet(label_style);
    adv_layout->addWidget(lev_lbl);
    leverage_spin_ = new QDoubleSpinBox(advanced_section_);
    leverage_spin_->setRange(0.1, 10);
    leverage_spin_->setValue(1.0);
    leverage_spin_->setDecimals(1);
    leverage_spin_->setSuffix("x");
    leverage_spin_->setStyleSheet(input_style);
    adv_layout->addWidget(leverage_spin_);

    auto* sl_lbl = new QLabel("STOP LOSS (%)", advanced_section_);
    sl_lbl->setStyleSheet(label_style);
    adv_layout->addWidget(sl_lbl);
    stop_loss_spin_ = new QDoubleSpinBox(advanced_section_);
    stop_loss_spin_->setRange(0, 100);
    stop_loss_spin_->setValue(0);
    stop_loss_spin_->setDecimals(1);
    stop_loss_spin_->setSuffix("%");
    stop_loss_spin_->setSpecialValueText("None");
    stop_loss_spin_->setStyleSheet(input_style);
    adv_layout->addWidget(stop_loss_spin_);

    auto* tp_lbl = new QLabel("TAKE PROFIT (%)", advanced_section_);
    tp_lbl->setStyleSheet(label_style);
    adv_layout->addWidget(tp_lbl);
    take_profit_spin_ = new QDoubleSpinBox(advanced_section_);
    take_profit_spin_->setRange(0, 1000);
    take_profit_spin_->setValue(0);
    take_profit_spin_->setDecimals(1);
    take_profit_spin_->setSuffix("%");
    take_profit_spin_->setSpecialValueText("None");
    take_profit_spin_->setStyleSheet(input_style);
    adv_layout->addWidget(take_profit_spin_);

    auto* ps_lbl = new QLabel("POSITION SIZING", advanced_section_);
    ps_lbl->setStyleSheet(label_style);
    adv_layout->addWidget(ps_lbl);
    pos_sizing_combo_ = new QComboBox(advanced_section_);
    pos_sizing_combo_->setStyleSheet(combo_style);
    for (const auto& m : position_sizing_methods())
        pos_sizing_combo_->addItem(m);
    adv_layout->addWidget(pos_sizing_combo_);

    allow_short_check_ = new QCheckBox("Allow Short Selling", advanced_section_);
    allow_short_check_->setStyleSheet(
        QString("QCheckBox { color:%1; font-family:%2; font-size:%3px; spacing:6px; }"
                "QCheckBox::indicator { width:14px; height:14px; border:1px solid %4; background:%5; }"
                "QCheckBox::indicator:checked { background:%6; border-color:%6; }")
            .arg(ui::colors::TEXT_SECONDARY())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL)
            .arg(ui::colors::BORDER_MED())
            .arg(ui::colors::BG_RAISED())
            .arg(ui::colors::AMBER()));
    adv_layout->addWidget(allow_short_check_);

    vl->addWidget(advanced_section_);

    // ── BENCHMARK (backtest-family + returns) ──
    // Used as the comparison baseline for performance metrics and for the
    // benchmark-aware returns_stats analysis (alpha/beta/info-ratio).
    benchmark_section_ = new QWidget(content);
    auto* bm_layout = new QVBoxLayout(benchmark_section_);
    bm_layout->setContentsMargins(0, 8, 0, 0);
    bm_layout->setSpacing(4);

    auto* bm_title = new QLabel("BENCHMARK", benchmark_section_);
    bm_title->setStyleSheet(section_style);
    bm_layout->addWidget(bm_title);

    auto* bm_lbl = new QLabel("SYMBOL", benchmark_section_);
    bm_lbl->setStyleSheet(label_style);
    bm_layout->addWidget(bm_lbl);
    benchmark_edit_ = new QLineEdit("SPY", benchmark_section_);
    benchmark_edit_->setStyleSheet(input_style);
    bm_layout->addWidget(benchmark_edit_);

    vl->addWidget(benchmark_section_);

    // ── COMMAND-SPECIFIC CONFIG ──
    vl->addSpacing(8);
    cmd_config_stack_ = new QStackedWidget(content);

    // Page 0: backtest (no extra config needed)
    cmd_config_stack_->addWidget(new QWidget);

    // Page 1: optimize
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("OPTIMIZATION", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* ol = new QLabel("OBJECTIVE", page);
        ol->setStyleSheet(label_style);
        pl->addWidget(ol);
        opt_objective_combo_ = new QComboBox(page);
        opt_objective_combo_->setStyleSheet(combo_style);
        for (const auto& o : optimize_objectives())
            opt_objective_combo_->addItem(o);
        pl->addWidget(opt_objective_combo_);

        auto* ml = new QLabel("METHOD", page);
        ml->setStyleSheet(label_style);
        pl->addWidget(ml);
        opt_method_combo_ = new QComboBox(page);
        opt_method_combo_->setStyleSheet(combo_style);
        for (const auto& m : optimize_methods())
            opt_method_combo_->addItem(m);
        pl->addWidget(opt_method_combo_);

        auto* il = new QLabel("MAX ITERATIONS", page);
        il->setStyleSheet(label_style);
        pl->addWidget(il);
        opt_iterations_spin_ = new QSpinBox(page);
        opt_iterations_spin_->setRange(10, 10000);
        opt_iterations_spin_->setValue(100);
        opt_iterations_spin_->setStyleSheet(input_style);
        pl->addWidget(opt_iterations_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 2: walk_forward
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("WALK-FORWARD", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* sl = new QLabel("NUMBER OF SPLITS", page);
        sl->setStyleSheet(label_style);
        pl->addWidget(sl);
        wf_splits_spin_ = new QSpinBox(page);
        wf_splits_spin_->setRange(2, 20);
        wf_splits_spin_->setValue(5);
        wf_splits_spin_->setStyleSheet(input_style);
        pl->addWidget(wf_splits_spin_);

        auto* tl = new QLabel("TRAIN RATIO", page);
        tl->setStyleSheet(label_style);
        pl->addWidget(tl);
        wf_train_ratio_spin_ = new QDoubleSpinBox(page);
        wf_train_ratio_spin_->setRange(0.5, 0.95);
        wf_train_ratio_spin_->setValue(0.7);
        wf_train_ratio_spin_->setDecimals(2);
        wf_train_ratio_spin_->setSingleStep(0.05);
        wf_train_ratio_spin_->setStyleSheet(input_style);
        pl->addWidget(wf_train_ratio_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 3: indicator
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("INDICATOR", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* il = new QLabel("INDICATOR TYPE", page);
        il->setStyleSheet(label_style);
        pl->addWidget(il);
        indicator_combo_ = new QComboBox(page);
        indicator_combo_->setStyleSheet(combo_style);
        // Populated dynamically via on_result("get_indicators")
        pl->addWidget(indicator_combo_);

        auto* pl_period = new QLabel("PERIOD", page);
        pl_period->setStyleSheet(label_style);
        pl->addWidget(pl_period);
        indicator_period_spin_ = new QSpinBox(page);
        indicator_period_spin_->setRange(2, 500);
        indicator_period_spin_->setValue(14);
        indicator_period_spin_->setStyleSheet(input_style);
        pl->addWidget(indicator_period_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 4: indicator_signals
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("INDICATOR SIGNALS", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* isl = new QLabel("INDICATOR", page);
        isl->setStyleSheet(label_style);
        pl->addWidget(isl);
        ind_signal_indicator_combo_ = new QComboBox(page);
        ind_signal_indicator_combo_->setStyleSheet(combo_style);
        // Populated dynamically via on_result("get_indicators")
        pl->addWidget(ind_signal_indicator_combo_);

        auto* ml = new QLabel("SIGNAL MODE", page);
        ml->setStyleSheet(label_style);
        pl->addWidget(ml);
        ind_signal_mode_combo_ = new QComboBox(page);
        ind_signal_mode_combo_->setStyleSheet(combo_style);
        for (const auto& m : indicator_signal_modes())
            ind_signal_mode_combo_->addItem(m);
        pl->addWidget(ind_signal_mode_combo_);

        // Helpers
        auto make_int_row = [&](const QString& text, QSpinBox*& spin, int mn, int mx, int dv) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0); rl->setSpacing(4);
            auto* lab = new QLabel(text, row); lab->setStyleSheet(label_style);
            rl->addWidget(lab);
            spin = new QSpinBox(row);
            spin->setRange(mn, mx); spin->setValue(dv); spin->setStyleSheet(input_style);
            rl->addWidget(spin);
            return row;
        };
        auto make_dbl_row = [&](const QString& text, QDoubleSpinBox*& spin,
                                double mn, double mx, double dv, int decimals = 2) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0); rl->setSpacing(4);
            auto* lab = new QLabel(text, row); lab->setStyleSheet(label_style);
            rl->addWidget(lab);
            spin = new QDoubleSpinBox(row);
            spin->setRange(mn, mx); spin->setValue(dv);
            spin->setDecimals(decimals); spin->setStyleSheet(input_style);
            rl->addWidget(spin);
            return row;
        };
        auto make_combo_row = [&](const QString& text, QComboBox*& combo,
                                  const QStringList& items) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0); rl->setSpacing(4);
            auto* lab = new QLabel(text, row); lab->setStyleSheet(label_style);
            rl->addWidget(lab);
            combo = new QComboBox(row);
            combo->setStyleSheet(combo_style);
            for (const auto& it : items) combo->addItem(it);
            rl->addWidget(combo);
            return row;
        };

        // ── crossover_signals: fast period + slow period + ma type ──
        is_fast_period_row_ = make_int_row("FAST PERIOD", is_fast_period_spin_, 2, 200, 10);
        is_slow_period_row_ = make_int_row("SLOW PERIOD", is_slow_period_spin_, 2, 500, 20);
        is_ma_type_row_     = make_combo_row("MA TYPE", is_ma_type_combo_, {"ma", "ema"});
        pl->addWidget(is_fast_period_row_);
        pl->addWidget(is_slow_period_row_);
        pl->addWidget(is_ma_type_row_);

        // ── threshold_signals: period + lower + upper ──
        is_period_row_ = make_int_row("PERIOD", is_period_spin_, 2, 200, 14);
        is_lower_row_  = make_dbl_row("LOWER THRESHOLD", is_lower_spin_, -200, 200, 30);
        is_upper_row_  = make_dbl_row("UPPER THRESHOLD", is_upper_spin_, -200, 200, 70);
        pl->addWidget(is_period_row_);
        pl->addWidget(is_lower_row_);
        pl->addWidget(is_upper_row_);

        // ── breakout_signals: channel + period (shares is_period) ──
        is_channel_row_ = make_combo_row("CHANNEL", is_channel_combo_, {"donchian", "bbands", "keltner"});
        pl->addWidget(is_channel_row_);

        // ── mean_reversion_signals: period + z_entry + z_exit ──
        is_z_entry_row_ = make_dbl_row("Z ENTRY (sigma)", is_z_entry_spin_, 0.1, 5.0, 2.0, 2);
        is_z_exit_row_  = make_dbl_row("Z EXIT (sigma)",  is_z_exit_spin_,  0.0, 5.0, 0.0, 2);
        pl->addWidget(is_z_entry_row_);
        pl->addWidget(is_z_exit_row_);

        // ── signal_filter: filter_indicator + filter_period + filter_threshold + filter_type ──
        is_filter_indicator_row_ = make_combo_row("FILTER INDICATOR", is_filter_indicator_combo_,
                                                   {"adx", "atr", "mstd", "zscore", "rsi"});
        is_filter_period_row_    = make_int_row("FILTER PERIOD", is_filter_period_spin_, 2, 200, 14);
        is_filter_threshold_row_ = make_dbl_row("FILTER THRESHOLD", is_filter_threshold_spin_,
                                                 -100, 100, 25, 3);
        is_filter_type_row_      = make_combo_row("FILTER TYPE", is_filter_type_combo_, {"above", "below"});
        pl->addWidget(is_filter_indicator_row_);
        pl->addWidget(is_filter_period_row_);
        pl->addWidget(is_filter_threshold_row_);
        pl->addWidget(is_filter_type_row_);

        auto update_is_rows = [this]() {
            const QString m = ind_signal_mode_combo_->currentText();
            const bool is_cross  = (m == "crossover_signals" || m == "crossover");
            const bool is_thresh = (m == "threshold_signals" || m == "threshold");
            const bool is_break  = (m == "breakout_signals"  || m == "breakout");
            const bool is_mr     = (m == "mean_reversion_signals" || m == "mean_reversion");
            const bool is_filt   = (m == "signal_filter" || m == "filter");
            is_fast_period_row_->setVisible(is_cross);
            is_slow_period_row_->setVisible(is_cross);
            is_ma_type_row_->setVisible(is_cross);
            // PERIOD reused by threshold + breakout + mean_reversion (+ filter base period inferred).
            is_period_row_->setVisible(is_thresh || is_break || is_mr);
            is_lower_row_->setVisible(is_thresh);
            is_upper_row_->setVisible(is_thresh);
            is_channel_row_->setVisible(is_break);
            is_z_entry_row_->setVisible(is_mr);
            is_z_exit_row_->setVisible(is_mr);
            is_filter_indicator_row_->setVisible(is_filt);
            is_filter_period_row_->setVisible(is_filt);
            is_filter_threshold_row_->setVisible(is_filt);
            is_filter_type_row_->setVisible(is_filt);
        };
        connect(ind_signal_mode_combo_, &QComboBox::currentTextChanged, this,
                [update_is_rows](const QString&) { update_is_rows(); });
        update_is_rows();

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 5: labels — params shown depend on the selected label generator.
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("ML LABELS", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* tl = new QLabel("LABEL TYPE", page);
        tl->setStyleSheet(label_style);
        pl->addWidget(tl);
        labels_type_combo_ = new QComboBox(page);
        labels_type_combo_->setStyleSheet(combo_style);
        for (const auto& lt : label_types())
            labels_type_combo_->addItem(lt);
        pl->addWidget(labels_type_combo_);

        // ── HORIZON row (FIXLB only) ──
        labels_horizon_row_ = new QWidget(page);
        auto* hr = new QVBoxLayout(labels_horizon_row_);
        hr->setContentsMargins(0, 0, 0, 0);
        hr->setSpacing(4);
        auto* hl = new QLabel("HORIZON (bars)", labels_horizon_row_);
        hl->setStyleSheet(label_style);
        hr->addWidget(hl);
        labels_horizon_spin_ = new QSpinBox(labels_horizon_row_);
        labels_horizon_spin_->setRange(1, 100);
        labels_horizon_spin_->setValue(5);
        labels_horizon_spin_->setStyleSheet(input_style);
        hr->addWidget(labels_horizon_spin_);
        pl->addWidget(labels_horizon_row_);

        // ── WINDOW row (MEANLB / LEXLB / TRENDLB / BOLB) ──
        labels_window_row_ = new QWidget(page);
        auto* wr = new QVBoxLayout(labels_window_row_);
        wr->setContentsMargins(0, 0, 0, 0);
        wr->setSpacing(4);
        auto* wl = new QLabel("WINDOW", labels_window_row_);
        wl->setStyleSheet(label_style);
        wr->addWidget(wl);
        labels_window_spin_ = new QSpinBox(labels_window_row_);
        labels_window_spin_->setRange(2, 500);
        labels_window_spin_->setValue(20);
        labels_window_spin_->setStyleSheet(input_style);
        wr->addWidget(labels_window_spin_);
        pl->addWidget(labels_window_row_);

        // ── THRESHOLD row (FIXLB / MEANLB / TRENDLB) ──
        labels_threshold_row_ = new QWidget(page);
        auto* tr = new QVBoxLayout(labels_threshold_row_);
        tr->setContentsMargins(0, 0, 0, 0);
        tr->setSpacing(4);
        auto* thl = new QLabel("THRESHOLD", labels_threshold_row_);
        thl->setStyleSheet(label_style);
        tr->addWidget(thl);
        labels_threshold_spin_ = new QDoubleSpinBox(labels_threshold_row_);
        labels_threshold_spin_->setRange(0.0, 5.0);
        labels_threshold_spin_->setValue(0.02);
        labels_threshold_spin_->setDecimals(3);
        labels_threshold_spin_->setSingleStep(0.005);
        labels_threshold_spin_->setStyleSheet(input_style);
        tr->addWidget(labels_threshold_spin_);
        pl->addWidget(labels_threshold_row_);

        // ── ALPHA row (BOLB only — Bollinger Band sigma multiplier) ──
        labels_alpha_row_ = new QWidget(page);
        auto* ar = new QVBoxLayout(labels_alpha_row_);
        ar->setContentsMargins(0, 0, 0, 0);
        ar->setSpacing(4);
        auto* al = new QLabel("ALPHA (sigma)", labels_alpha_row_);
        al->setStyleSheet(label_style);
        ar->addWidget(al);
        labels_alpha_spin_ = new QDoubleSpinBox(labels_alpha_row_);
        labels_alpha_spin_->setRange(0.1, 5.0);
        labels_alpha_spin_->setValue(2.0);
        labels_alpha_spin_->setDecimals(2);
        labels_alpha_spin_->setSingleStep(0.1);
        labels_alpha_spin_->setStyleSheet(input_style);
        ar->addWidget(labels_alpha_spin_);
        pl->addWidget(labels_alpha_row_);

        // Toggle row visibility based on the chosen label generator.
        auto update_label_rows = [this]() {
            const QString lt = labels_type_combo_->currentText();
            labels_horizon_row_->setVisible(lt == "FIXLB");
            labels_window_row_->setVisible(lt == "MEANLB" || lt == "LEXLB"
                                            || lt == "TRENDLB" || lt == "BOLB");
            labels_threshold_row_->setVisible(lt == "FIXLB" || lt == "MEANLB" || lt == "TRENDLB");
            labels_alpha_row_->setVisible(lt == "BOLB");
        };
        connect(labels_type_combo_, &QComboBox::currentTextChanged, this,
                [update_label_rows](const QString&) { update_label_rows(); });
        update_label_rows();

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 6: splits — fields shown depend on the chosen splitter.
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("CV SPLITS", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* tl = new QLabel("SPLITTER TYPE", page);
        tl->setStyleSheet(label_style);
        pl->addWidget(tl);
        splitter_type_combo_ = new QComboBox(page);
        splitter_type_combo_->setStyleSheet(combo_style);
        for (const auto& st : splitter_types())
            splitter_type_combo_->addItem(st);
        pl->addWidget(splitter_type_combo_);

        // Helper to build a labeled spinbox row inside its own QWidget container.
        auto make_row = [&](const QString& label_text, QSpinBox*& spin_out,
                            int min_v, int max_v, int default_v) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            auto* lab = new QLabel(label_text, row);
            lab->setStyleSheet(label_style);
            rl->addWidget(lab);
            spin_out = new QSpinBox(row);
            spin_out->setRange(min_v, max_v);
            spin_out->setValue(default_v);
            spin_out->setStyleSheet(input_style);
            rl->addWidget(spin_out);
            return row;
        };

        // Rolling-specific: training window length (in bars).
        splitter_window_row_ = make_row("WINDOW LENGTH", splitter_window_spin_, 10, 2000, 252);
        pl->addWidget(splitter_window_row_);

        // Expanding-specific: minimum training-set length before first split.
        splitter_min_row_ = make_row("MIN TRAIN LENGTH", splitter_min_spin_, 10, 2000, 252);
        pl->addWidget(splitter_min_row_);

        // Rolling + Expanding: out-of-sample window per fold.
        splitter_test_row_ = make_row("TEST LENGTH", splitter_test_spin_, 5, 1000, 63);
        pl->addWidget(splitter_test_row_);

        // Rolling + Expanding: bars to advance between folds.
        splitter_step_row_ = make_row("STEP SIZE", splitter_step_spin_, 1, 500, 21);
        pl->addWidget(splitter_step_row_);

        // PurgedKFold-specific: number of folds.
        splitter_n_splits_row_ = make_row("N SPLITS", splitter_n_splits_spin_, 2, 20, 5);
        pl->addWidget(splitter_n_splits_row_);

        // PurgedKFold-specific: bars to drop between train end and test start
        // (purge) and between test end and next train (embargo) — guards against
        // information leakage in financial-ML CV (López de Prado).
        splitter_purge_row_ = make_row("PURGE LENGTH", splitter_purge_spin_, 0, 100, 5);
        pl->addWidget(splitter_purge_row_);

        splitter_embargo_row_ = make_row("EMBARGO LENGTH", splitter_embargo_spin_, 0, 100, 5);
        pl->addWidget(splitter_embargo_row_);

        // Toggle row visibility based on the chosen splitter.
        auto update_split_rows = [this]() {
            const QString st = splitter_type_combo_->currentText();
            const bool is_rolling   = (st == "RollingSplitter");
            const bool is_expanding = (st == "ExpandingSplitter");
            const bool is_purged    = (st == "PurgedKFoldSplitter" || st == "PurgedKFold");
            splitter_window_row_->setVisible(is_rolling);
            splitter_min_row_->setVisible(is_expanding);
            splitter_test_row_->setVisible(is_rolling || is_expanding);
            splitter_step_row_->setVisible(is_rolling || is_expanding);
            splitter_n_splits_row_->setVisible(is_purged);
            splitter_purge_row_->setVisible(is_purged);
            splitter_embargo_row_->setVisible(is_purged);
        };
        connect(splitter_type_combo_, &QComboBox::currentTextChanged, this,
                [update_split_rows](const QString&) { update_split_rows(); });
        update_split_rows();

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 7: returns — extra inputs only appear for the analysis types that use them.
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("RETURNS ANALYSIS", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* tl = new QLabel("ANALYSIS TYPE", page);
        tl->setStyleSheet(label_style);
        pl->addWidget(tl);
        returns_type_combo_ = new QComboBox(page);
        returns_type_combo_->setStyleSheet(combo_style);
        for (const auto& rt : returns_analysis_types())
            returns_type_combo_->addItem(rt);
        pl->addWidget(returns_type_combo_);

        // ── ROLLING WINDOW row (rolling analysis only) ──
        returns_window_row_ = new QWidget(page);
        auto* wr = new QVBoxLayout(returns_window_row_);
        wr->setContentsMargins(0, 0, 0, 0);
        wr->setSpacing(4);
        auto* wl = new QLabel("ROLLING WINDOW", returns_window_row_);
        wl->setStyleSheet(label_style);
        wr->addWidget(wl);
        returns_window_spin_ = new QSpinBox(returns_window_row_);
        returns_window_spin_->setRange(5, 504);
        returns_window_spin_->setValue(63);
        returns_window_spin_->setStyleSheet(input_style);
        wr->addWidget(returns_window_spin_);
        pl->addWidget(returns_window_row_);

        // ── METRIC row (rolling analysis only) ──
        // Names must match the vbt_returns / zl_returns rolling_map dispatch keys.
        returns_metric_row_ = new QWidget(page);
        auto* mr = new QVBoxLayout(returns_metric_row_);
        mr->setContentsMargins(0, 0, 0, 0);
        mr->setSpacing(4);
        auto* ml = new QLabel("METRIC", returns_metric_row_);
        ml->setStyleSheet(label_style);
        mr->addWidget(ml);
        returns_metric_combo_ = new QComboBox(returns_metric_row_);
        returns_metric_combo_->setStyleSheet(combo_style);
        for (const auto& m : {"sharpe", "sortino", "calmar", "omega",
                              "info_ratio", "downside_risk",
                              "total", "annualized", "volatility"})
            returns_metric_combo_->addItem(m);
        mr->addWidget(returns_metric_combo_);
        pl->addWidget(returns_metric_row_);

        // ── THRESHOLD row (ranges analysis only — defines what counts as a "range") ──
        returns_threshold_row_ = new QWidget(page);
        auto* thr = new QVBoxLayout(returns_threshold_row_);
        thr->setContentsMargins(0, 0, 0, 0);
        thr->setSpacing(4);
        auto* thl = new QLabel("THRESHOLD", returns_threshold_row_);
        thl->setStyleSheet(label_style);
        thr->addWidget(thl);
        returns_threshold_spin_ = new QDoubleSpinBox(returns_threshold_row_);
        returns_threshold_spin_->setRange(-1.0, 1.0);
        returns_threshold_spin_->setValue(0.0);
        returns_threshold_spin_->setDecimals(4);
        returns_threshold_spin_->setSingleStep(0.001);
        returns_threshold_spin_->setStyleSheet(input_style);
        thr->addWidget(returns_threshold_spin_);
        pl->addWidget(returns_threshold_row_);

        auto update_returns_rows = [this]() {
            const QString at = returns_type_combo_->currentText();
            returns_window_row_->setVisible(at == "rolling");
            returns_metric_row_->setVisible(at == "rolling");
            returns_threshold_row_->setVisible(at == "ranges");
        };
        connect(returns_type_combo_, &QComboBox::currentTextChanged, this,
                [update_returns_rows](const QString&) { update_returns_rows(); });
        update_returns_rows();

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 8: signals — generator-aware param rows.
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("SIGNAL GENERATORS", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* gl = new QLabel("GENERATOR TYPE", page);
        gl->setStyleSheet(label_style);
        pl->addWidget(gl);
        signal_gen_combo_ = new QComboBox(page);
        signal_gen_combo_->setStyleSheet(combo_style);
        for (const auto& sg : signal_generators())
            signal_gen_combo_->addItem(sg);
        pl->addWidget(signal_gen_combo_);

        // Helpers to build a row inside its own container so we can toggle visibility.
        auto make_int_row = [&](const QString& text, QSpinBox*& spin, int mn, int mx, int dv) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0); rl->setSpacing(4);
            auto* lab = new QLabel(text, row); lab->setStyleSheet(label_style);
            rl->addWidget(lab);
            spin = new QSpinBox(row);
            spin->setRange(mn, mx); spin->setValue(dv); spin->setStyleSheet(input_style);
            rl->addWidget(spin);
            return row;
        };
        auto make_prob_row = [&](const QString& text, QDoubleSpinBox*& spin, double dv) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0); rl->setSpacing(4);
            auto* lab = new QLabel(text, row); lab->setStyleSheet(label_style);
            rl->addWidget(lab);
            spin = new QDoubleSpinBox(row);
            spin->setRange(0.001, 1.0); spin->setValue(dv);
            spin->setDecimals(3); spin->setSingleStep(0.01);
            spin->setStyleSheet(input_style);
            rl->addWidget(spin);
            return row;
        };

        signal_count_row_      = make_int_row("COUNT (n)",       signal_count_spin_,     1, 1000, 10);
        signal_entry_prob_row_ = make_prob_row("ENTRY PROB",     signal_entry_prob_spin_, 0.05);
        signal_exit_prob_row_  = make_prob_row("EXIT PROB",      signal_exit_prob_spin_,  0.10);
        signal_min_hold_row_   = make_int_row("MIN HOLD (bars)", signal_min_hold_spin_,   1, 500, 5);
        signal_max_hold_row_   = make_int_row("MAX HOLD (bars)", signal_max_hold_spin_,   1, 500, 20);

        pl->addWidget(signal_count_row_);
        pl->addWidget(signal_entry_prob_row_);
        pl->addWidget(signal_exit_prob_row_);
        pl->addWidget(signal_min_hold_row_);
        pl->addWidget(signal_max_hold_row_);

        auto update_signal_rows = [this]() {
            const QString g = signal_gen_combo_->currentText();
            const bool is_rand    = (g == "RAND");
            const bool is_randx   = (g == "RANDX");
            const bool is_randnx  = (g == "RANDNX");
            const bool is_rprob   = (g == "RPROB");
            const bool is_rprobx  = (g == "RPROBX");
            signal_count_row_->setVisible(is_rand || is_randx || is_randnx);
            signal_entry_prob_row_->setVisible(is_rprob || is_rprobx);
            signal_exit_prob_row_->setVisible(is_rprobx);
            signal_min_hold_row_->setVisible(is_randnx);
            signal_max_hold_row_->setVisible(is_randnx);
        };
        connect(signal_gen_combo_, &QComboBox::currentTextChanged, this,
                [update_signal_rows](const QString&) { update_signal_rows(); });
        update_signal_rows();

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 9: labels_to_signals — converts ML labels into trading signals.
    // Reuses the same horizon/threshold spinboxes from the labels page (they
    // describe the underlying label generation) and only adds a label-type
    // selector here.
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("LABELS -> SIGNALS", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* tl = new QLabel("LABEL TYPE", page);
        tl->setStyleSheet(label_style);
        pl->addWidget(tl);
        l2s_label_type_combo_ = new QComboBox(page);
        l2s_label_type_combo_->setStyleSheet(combo_style);
        for (const auto& lt : label_types())
            l2s_label_type_combo_->addItem(lt);
        pl->addWidget(l2s_label_type_combo_);

        // Entry/exit label override — defaults match vectorbt + zipline (entry=1, exit=-1)
        // but users can flip the mapping (e.g. enter on -1 for mean-reversion).
        auto* elbl = new QLabel("ENTRY LABEL", page);
        elbl->setStyleSheet(label_style);
        pl->addWidget(elbl);
        l2s_entry_label_spin_ = new QSpinBox(page);
        l2s_entry_label_spin_->setRange(-2, 2);
        l2s_entry_label_spin_->setValue(1);
        l2s_entry_label_spin_->setStyleSheet(input_style);
        pl->addWidget(l2s_entry_label_spin_);

        auto* xlbl = new QLabel("EXIT LABEL", page);
        xlbl->setStyleSheet(label_style);
        pl->addWidget(xlbl);
        l2s_exit_label_spin_ = new QSpinBox(page);
        l2s_exit_label_spin_->setRange(-2, 2);
        l2s_exit_label_spin_->setValue(-1);
        l2s_exit_label_spin_->setStyleSheet(input_style);
        pl->addWidget(l2s_exit_label_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 10: indicator_sweep — runs an indicator across a parameter grid
    // and returns aggregated stats per parameter value. Indicator combo is
    // populated dynamically via on_result("get_indicators").
    {
        auto* page = new QWidget(this);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(4);
        auto* t = new QLabel("INDICATOR SWEEP", page);
        t->setStyleSheet(section_style);
        pl->addWidget(t);

        auto* il = new QLabel("INDICATOR", page);
        il->setStyleSheet(label_style);
        pl->addWidget(il);
        sweep_indicator_combo_ = new QComboBox(page);
        sweep_indicator_combo_->setStyleSheet(combo_style);
        pl->addWidget(sweep_indicator_combo_);

        auto* mnl = new QLabel("MIN", page);
        mnl->setStyleSheet(label_style);
        pl->addWidget(mnl);
        sweep_min_spin_ = new QSpinBox(page);
        sweep_min_spin_->setRange(1, 1000);
        sweep_min_spin_->setValue(5);
        sweep_min_spin_->setStyleSheet(input_style);
        pl->addWidget(sweep_min_spin_);

        auto* mxl = new QLabel("MAX", page);
        mxl->setStyleSheet(label_style);
        pl->addWidget(mxl);
        sweep_max_spin_ = new QSpinBox(page);
        sweep_max_spin_->setRange(1, 1000);
        sweep_max_spin_->setValue(50);
        sweep_max_spin_->setStyleSheet(input_style);
        pl->addWidget(sweep_max_spin_);

        auto* sl = new QLabel("STEP", page);
        sl->setStyleSheet(label_style);
        pl->addWidget(sl);
        sweep_step_spin_ = new QSpinBox(page);
        sweep_step_spin_->setRange(1, 100);
        sweep_step_spin_->setValue(1);
        sweep_step_spin_->setStyleSheet(input_style);
        pl->addWidget(sweep_step_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    cmd_config_stack_->setCurrentIndex(0);
    vl->addWidget(cmd_config_stack_);

    vl->addStretch();
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(panel);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    return panel;
}

// ── Status Bar ───────────────────────────────────────────────────────────────

QWidget* BacktestingScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(26);
    bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);
    auto s = QString("color:%1; font-size:%2px; font-family:%3;")
                 .arg(ui::colors::TEXT_TERTIARY())
                 .arg(ui::fonts::TINY)
                 .arg(ui::fonts::DATA_FAMILY);
    auto bold = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                    .arg(ui::colors::TEXT_PRIMARY())
                    .arg(ui::fonts::TINY)
                    .arg(ui::fonts::DATA_FAMILY);

    auto* l1 = new QLabel("PROVIDERS:", bar);
    l1->setStyleSheet(s);
    auto* v1 = new QLabel("6", bar);
    v1->setStyleSheet(bold);
    hl->addWidget(l1);
    hl->addWidget(v1);

    auto* l2 = new QLabel("STRATEGIES:", bar);
    l2->setStyleSheet(s);
    auto* v2 = new QLabel(QString::number(strategies_.size()), bar);
    v2->setStyleSheet(bold);
    hl->addWidget(l2);
    hl->addWidget(v2);

    hl->addStretch();
    status_label_ = new QLabel("READY", bar);
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(ui::colors::POSITIVE())
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(status_label_);
    return bar;
}

// ── Provider/Command/Strategy selection ──────────────────────────────────────

void BacktestingScreen::on_provider_changed(int index) {
    if (index < 0 || index >= providers_.size())
        return;
    active_provider_ = index;
    update_provider_buttons();
    update_command_buttons();

    // Clear stale strategies and show loading state
    strategies_.clear();
    strategy_category_combo_->clear();
    strategy_category_combo_->addItem("Loading...");
    strategy_combo_->clear();

    const auto& slug = providers_[index].slug;
    auto& svc = BacktestingService::instance();
    svc.load_strategies(slug);
    svc.load_command_options(slug);
    svc.execute(slug, "get_indicators", {});
    ScreenStateManager::instance().notify_changed(this);
}

void BacktestingScreen::update_provider_buttons() {
    for (int i = 0; i < provider_buttons_.size(); ++i) {
        const auto& p = providers_[i];
        const bool active = (i == active_provider_);
        const QString rgb = QString("%1,%2,%3").arg(p.color.red()).arg(p.color.green()).arg(p.color.blue());
        const QString fg = active ? p.color.name() : ui::colors::TEXT_TERTIARY();
        const QString bg = active ? QString("rgba(%1,0.12)").arg(rgb) : QString("transparent");
        const QString border = active ? QString("rgba(%1,0.3)").arg(rgb) : ui::colors::BORDER_DIM();
        const QString weight = active ? "700" : "400";

        // Same pill template as the brand / RUN / status chips so geometry is
        // identical regardless of selection state.
        provider_buttons_[i]->setStyleSheet(
            pill_qss("QPushButton", fg, bg, border, ui::fonts::TINY, ui::fonts::DATA_FAMILY, weight) +
            QString("QPushButton:hover { background:rgba(%1,0.15); }").arg(rgb));
    }
}

void BacktestingScreen::on_command_changed(int index) {
    const auto& cmds = commands_;
    if (index < 0 || index >= cmds.size())
        return;
    active_command_ = index;
    update_command_buttons();

    // Map command index to config stack page. The page order in
    // build_left_panel must match the command order in
    // BacktestingTypes::all_commands() — adding a new command requires
    // appending a matching cmd_config_stack_ page in the same position.
    // commands: backtest(0), optimize(1), walk_forward(2), indicator(3),
    //           indicator_signals(4), labels(5), splits(6), returns(7),
    //           signals(8), labels_to_signals(9), indicator_sweep(10)
    cmd_config_stack_->setCurrentIndex(index);
    update_section_visibility();
}

void BacktestingScreen::update_section_visibility() {
    // Show/hide whole right + left panel sections based on the active command.
    //
    // Sections map to the args each command actually consumes (see gather_args):
    //   - backtest/optimize/walk_forward    : execution, advanced, benchmark, strategy
    //   - returns                            : benchmark (only — for benchmark-relative analysis)
    //   - everything else (labels, splits,   : MARKET DATA + per-command page only
    //     signals, labels_to_signals,
    //     indicator, indicator_signals,
    //     indicator_sweep)
    //
    // MARKET DATA is always visible — every command reads symbols + dates.
    if (active_command_ < 0 || active_command_ >= commands_.size())
        return;
    const QString cmd = commands_[active_command_].id;
    const bool is_backtest_family = (cmd == "backtest" || cmd == "optimize" || cmd == "walk_forward");
    const bool needs_benchmark = is_backtest_family || cmd == "returns";

    if (execution_section_) execution_section_->setVisible(is_backtest_family);
    if (advanced_section_)  advanced_section_->setVisible(is_backtest_family);
    if (benchmark_section_) benchmark_section_->setVisible(needs_benchmark);
    if (strategy_section_)  strategy_section_->setVisible(is_backtest_family);

    // Show explicit MIN/MAX/STEP rows under each strategy param only for `optimize`.
    const bool is_optimize = (cmd == "optimize");
    for (auto* row : param_range_rows_)
        if (row) row->setVisible(is_optimize);
}

void BacktestingScreen::update_command_buttons() {
    const auto& cmds = commands_;
    const auto& supported = providers_[active_provider_].commands;

    for (int i = 0; i < command_buttons_.size() && i < cmds.size(); ++i) {
        const auto& cmd = cmds[i];
        bool active = (i == active_command_);
        bool enabled = supported.contains(cmd.id);
        command_buttons_[i]->setEnabled(enabled);
        command_buttons_[i]->setStyleSheet(
            QString("QPushButton { text-align:left; padding:6px 10px; border:none;"
                    "border-left:1px solid %1; color:%2;"
                    "font-size:%3px; font-family:%4; font-weight:%5; background:%6; }"
                    "QPushButton:hover { background:rgba(%7,0.1); color:%8; }"
                    "QPushButton:disabled { color:%9; border-left-color:transparent; }")
                .arg(active ? cmd.color.name() : "transparent")
                .arg(active ? cmd.color.name() : ui::colors::TEXT_SECONDARY())
                .arg(ui::fonts::SMALL)
                .arg(ui::fonts::DATA_FAMILY)
                .arg(active ? "700" : "400")
                .arg(active ? QString("rgba(%1,%2,%3,0.08)")
                                  .arg(cmd.color.red())
                                  .arg(cmd.color.green())
                                  .arg(cmd.color.blue())
                            : "transparent")
                .arg(QString("%1,%2,%3").arg(cmd.color.red()).arg(cmd.color.green()).arg(cmd.color.blue()))
                .arg(cmd.color.name())
                .arg(ui::colors::TEXT_DIM()));
    }
}

void BacktestingScreen::on_strategy_changed(int index) {
    Q_UNUSED(index)
    rebuild_strategy_params();
}

void BacktestingScreen::populate_strategies() {
    strategy_combo_->blockSignals(true);
    strategy_combo_->clear();
    auto cat = strategy_category_combo_->currentText();
    for (const auto& s : strategies_) {
        if (s.category == cat)
            strategy_combo_->addItem(s.name, s.id);
    }
    strategy_combo_->blockSignals(false);

    if (strategy_combo_->count() > 0) {
        strategy_combo_->setCurrentIndex(0);
        rebuild_strategy_params();
    } else {
        clear_layout(strategy_params_layout_);
        param_spinboxes_.clear();
    }
}

void BacktestingScreen::rebuild_strategy_params() {
    clear_layout(strategy_params_layout_);
    param_spinboxes_.clear();
    param_min_spinboxes_.clear();
    param_max_spinboxes_.clear();
    param_step_spinboxes_.clear();
    param_range_rows_.clear();

    auto strategy_id = strategy_combo_->currentData().toString();
    if (strategy_id.isEmpty())
        return;

    const Strategy* strat = nullptr;
    for (const auto& s : strategies_) {
        if (s.id == strategy_id) {
            strat = &s;
            break;
        }
    }
    if (!strat || strat->params.isEmpty())
        return;

    auto input_style = QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 4px; }"
                               "QDoubleSpinBox:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::colors::BORDER_BRIGHT());

    auto sublabel_style = QString("color:%1; font-size:%2px; font-family:%3;")
                              .arg(ui::colors::TEXT_DIM())
                              .arg(ui::fonts::TINY)
                              .arg(ui::fonts::DATA_FAMILY);

    auto label_style = QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY);

    const bool is_optimize = (active_command_ >= 0
                              && active_command_ < commands_.size()
                              && commands_[active_command_].id == "optimize");

    for (const auto& p : strat->params) {
        auto* lbl = new QLabel(p.label.toUpper(), strategy_params_container_);
        lbl->setStyleSheet(label_style);
        strategy_params_layout_->addWidget(lbl);

        auto* spin = new QDoubleSpinBox(strategy_params_container_);
        spin->setRange(p.min_val, p.max_val);
        spin->setValue(p.default_val);
        spin->setSingleStep(p.step);
        spin->setDecimals(p.step < 1.0 ? 2 : 0);
        spin->setStyleSheet(input_style);
        spin->setProperty("param_name", p.name);
        strategy_params_layout_->addWidget(spin);
        param_spinboxes_.append(spin);

        // ── Optimize-only: explicit MIN / MAX / STEP range editor ──
        // Built up front so we can flip visibility in update_section_visibility()
        // without having to rebuild the whole strategy form on command change.
        auto* range_row = new QWidget(strategy_params_container_);
        auto* hl = new QGridLayout(range_row);
        hl->setContentsMargins(0, 0, 0, 4);
        hl->setHorizontalSpacing(4);
        hl->setVerticalSpacing(2);

        auto build_subspin = [&](const QString& sub_label, double default_val, int col) -> QDoubleSpinBox* {
            auto* sl = new QLabel(sub_label, range_row);
            sl->setStyleSheet(sublabel_style);
            hl->addWidget(sl, 0, col);
            auto* s = new QDoubleSpinBox(range_row);
            s->setRange(p.min_val, p.max_val);
            s->setValue(default_val);
            s->setSingleStep(p.step);
            s->setDecimals(p.step < 1.0 ? 2 : 0);
            s->setStyleSheet(input_style);
            hl->addWidget(s, 1, col);
            return s;
        };

        // Default range: seed value ± a couple of steps, snapped to param bounds.
        const double span = qMax(p.step, (p.max_val - p.min_val) * 0.1);
        const double min_default = qMax(p.min_val, p.default_val - span);
        const double max_default = qMin(p.max_val, p.default_val + span);
        auto* min_spin  = build_subspin("MIN",  min_default,    0);
        auto* max_spin  = build_subspin("MAX",  max_default,    1);
        auto* step_spin = build_subspin("STEP", p.step,         2);

        strategy_params_layout_->addWidget(range_row);
        range_row->setVisible(is_optimize);

        param_min_spinboxes_.append(min_spin);
        param_max_spinboxes_.append(max_spin);
        param_step_spinboxes_.append(step_spin);
        param_range_rows_.append(range_row);
    }
}

// ── Gather args ──────────────────────────────────────────────────────────────

QJsonObject BacktestingScreen::gather_strategy_params() {
    QJsonObject params;
    for (auto* spin : param_spinboxes_) {
        auto name = spin->property("param_name").toString();
        if (!name.isEmpty())
            params[name] = spin->value();
    }
    return params;
}

QJsonObject BacktestingScreen::gather_args() {
    const auto& cmds = commands_;
    auto cmd_id = cmds[active_command_].id;

    // Common fields
    auto sym_text = symbols_edit_->text().trimmed();
    QJsonArray symbols;
    for (const auto& s : sym_text.split(',', Qt::SkipEmptyParts))
        symbols.append(s.trimmed());

    auto start = start_date_->date().toString("yyyy-MM-dd");
    auto end = end_date_->date().toString("yyyy-MM-dd");

    QJsonObject args;
    args["symbols"] = symbols;
    args["startDate"] = start;
    args["endDate"] = end;

    // ── Command-specific args ──
    if (cmd_id == "backtest" || cmd_id == "optimize" || cmd_id == "walk_forward") {
        args["initialCapital"] = capital_spin_->value();
        args["commission"] = commission_spin_->value() / 100.0;
        args["slippage"] = slippage_spin_->value() / 100.0;
        args["leverage"] = leverage_spin_->value();
        args["positionSizing"] = pos_sizing_combo_->currentText();
        args["allowShort"] = allow_short_check_->isChecked();
        args["benchmarkSymbol"] = benchmark_edit_->text().trimmed();

        if (stop_loss_spin_->value() > 0)
            args["stopLoss"] = stop_loss_spin_->value() / 100.0;
        if (take_profit_spin_->value() > 0)
            args["takeProfit"] = take_profit_spin_->value() / 100.0;

        // Strategy — type = ID (used by Python), name = display name
        QJsonObject strategy;
        strategy["type"] = strategy_combo_->currentData().toString(); // strategy ID
        strategy["name"] = strategy_combo_->currentText();            // display name
        strategy["category"] = strategy_category_combo_->currentText();
        strategy["params"] = gather_strategy_params();
        args["strategy"] = strategy;
    }

    if (cmd_id == "optimize") {
        args["optimizeObjective"] = opt_objective_combo_->currentText();
        args["optimizeMethod"]    = opt_method_combo_->currentText();
        args["maxIterations"]     = opt_iterations_spin_->value();

        // Build paramRanges from the explicit per-param MIN/MAX/STEP editor.
        // rebuild_strategy_params keeps param_spinboxes_ and the three range
        // vectors index-aligned, so we can zip them.
        QJsonObject ranges;
        for (int i = 0; i < param_spinboxes_.size(); ++i) {
            auto name = param_spinboxes_[i]->property("param_name").toString();
            if (name.isEmpty()) continue;
            if (i >= param_min_spinboxes_.size() || i >= param_max_spinboxes_.size()
                || i >= param_step_spinboxes_.size()) continue;
            double mn = param_min_spinboxes_[i]->value();
            double mx = param_max_spinboxes_[i]->value();
            double st = qMax(0.0001, param_step_spinboxes_[i]->value());
            if (mx < mn) std::swap(mn, mx);
            QJsonObject range;
            range["min"]  = mn;
            range["max"]  = mx;
            range["step"] = st;
            ranges[name] = range;
        }
        args["paramRanges"] = ranges;
    }

    if (cmd_id == "walk_forward") {
        args["wfSplits"] = wf_splits_spin_->value();
        args["wfTrainRatio"] = wf_train_ratio_spin_->value();
    }

    if (cmd_id == "indicator") {
        args["indicator"] = indicator_combo_->currentData().toString();
        QJsonObject p;
        p["period"] = indicator_period_spin_->value();
        args["params"] = p;
    }

    if (cmd_id == "indicator_signals") {
        args["indicator"] = ind_signal_indicator_combo_->currentData().toString();
        const QString m = ind_signal_mode_combo_->currentText();
        args["mode"] = m;
        QJsonObject params;
        // vectorbt uses long mode names ('crossover_signals'); zipline uses both
        // long and short forms. The params keys are the same across providers.
        if (m == "crossover_signals" || m == "crossover") {
            params["fast_period"]   = is_fast_period_spin_->value();
            params["slow_period"]   = is_slow_period_spin_->value();
            params["fastPeriod"]    = is_fast_period_spin_->value(); // zipline alias
            params["slowPeriod"]    = is_slow_period_spin_->value();
            params["ma_type"]       = is_ma_type_combo_->currentText();
            params["maType"]        = is_ma_type_combo_->currentText();
            params["fast_indicator"] = is_ma_type_combo_->currentText(); // vectorbt key
            params["slow_indicator"] = is_ma_type_combo_->currentText();
        } else if (m == "threshold_signals" || m == "threshold") {
            params["period"] = is_period_spin_->value();
            params["lower"]  = is_lower_spin_->value();
            params["upper"]  = is_upper_spin_->value();
        } else if (m == "breakout_signals" || m == "breakout") {
            params["period"]  = is_period_spin_->value();
            params["channel"] = is_channel_combo_->currentText();
        } else if (m == "mean_reversion_signals" || m == "mean_reversion") {
            params["period"]  = is_period_spin_->value();
            params["z_entry"] = is_z_entry_spin_->value();
            params["z_exit"]  = is_z_exit_spin_->value();
        } else if (m == "signal_filter" || m == "filter") {
            params["base_indicator"]   = ind_signal_indicator_combo_->currentData().toString();
            params["base_period"]      = is_period_spin_->value();
            params["filter_indicator"] = is_filter_indicator_combo_->currentText();
            params["filter_period"]    = is_filter_period_spin_->value();
            params["filter_threshold"] = is_filter_threshold_spin_->value();
            params["filter_type"]      = is_filter_type_combo_->currentText();
        }
        args["params"] = params;
    }

    if (cmd_id == "labels") {
        const QString lt = labels_type_combo_->currentText();
        args["labelType"] = lt;
        QJsonObject params;
        // Send only the params the chosen generator consumes so the JSON stays
        // unambiguous; Python providers use defaults for anything missing.
        if (lt == "FIXLB") {
            params["horizon"]   = labels_horizon_spin_->value();
            params["threshold"] = labels_threshold_spin_->value();
        } else if (lt == "MEANLB" || lt == "TRENDLB") {
            params["window"]    = labels_window_spin_->value();
            params["threshold"] = labels_threshold_spin_->value();
        } else if (lt == "LEXLB") {
            params["window"]    = labels_window_spin_->value();
        } else if (lt == "BOLB") {
            params["window"]    = labels_window_spin_->value();
            params["alpha"]     = labels_alpha_spin_->value();
        }
        args["params"] = params;
    }

    if (cmd_id == "splits") {
        const QString st = splitter_type_combo_->currentText();
        args["splitterType"] = st;
        QJsonObject params;
        // vectorbt reads snake_case (window_len/test_len/min_len/n_splits/purge_len/embargo_len);
        // zipline reads camelCase (windowLen/testLen/minLen/nSplits/purgeLen/embargoLen).
        // Sending both keeps the JSON contract provider-agnostic.
        if (st == "RollingSplitter") {
            params["window_len"] = splitter_window_spin_->value();
            params["windowLen"]  = splitter_window_spin_->value();
            params["test_len"]   = splitter_test_spin_->value();
            params["testLen"]    = splitter_test_spin_->value();
            params["step"]       = splitter_step_spin_->value();
        } else if (st == "ExpandingSplitter") {
            params["min_len"]  = splitter_min_spin_->value();
            params["minLen"]   = splitter_min_spin_->value();
            params["test_len"] = splitter_test_spin_->value();
            params["testLen"]  = splitter_test_spin_->value();
            params["step"]     = splitter_step_spin_->value();
        } else if (st == "PurgedKFoldSplitter" || st == "PurgedKFold") {
            params["n_splits"]    = splitter_n_splits_spin_->value();
            params["nSplits"]     = splitter_n_splits_spin_->value();
            params["purge_len"]   = splitter_purge_spin_->value();
            params["purgeLen"]    = splitter_purge_spin_->value();
            params["embargo_len"] = splitter_embargo_spin_->value();
            params["embargoLen"]  = splitter_embargo_spin_->value();
        }
        args["params"] = params;
    }

    if (cmd_id == "returns") {
        const QString at = returns_type_combo_->currentText();
        args["analysisType"] = at;
        // Benchmark is consumed by returns_stats for alpha/beta/info-ratio.
        if (!benchmark_edit_->text().trimmed().isEmpty())
            args["benchmarkSymbol"] = benchmark_edit_->text().trimmed();

        QJsonObject params;
        if (at == "rolling") {
            params["window"] = returns_window_spin_->value();
            // vectorbt reads `metric` (singular string); zipline reads
            // `metrics` (list, used as a set-membership check). Send both.
            const QString m = returns_metric_combo_->currentText();
            params["metric"]  = m;
            QJsonArray ml; ml.append(m);
            params["metrics"] = ml;
        } else if (at == "ranges") {
            params["threshold"] = returns_threshold_spin_->value();
        }
        if (!params.isEmpty())
            args["params"] = params;
    }

    if (cmd_id == "signals") {
        const QString g = signal_gen_combo_->currentText();
        args["generatorType"] = g;
        // vectorbt reads snake_case (n / entry_prob / exit_prob / min_hold / max_hold);
        // zipline reads camelCase (n / entryProb / exitProb / minHold / maxHold).
        // Send both so every provider gets what it needs.
        QJsonObject params;
        if (g == "RAND" || g == "RANDX" || g == "RANDNX") {
            params["n"] = signal_count_spin_->value();
        }
        if (g == "RANDNX") {
            params["min_hold"] = signal_min_hold_spin_->value();
            params["max_hold"] = signal_max_hold_spin_->value();
            params["minHold"]  = signal_min_hold_spin_->value();
            params["maxHold"]  = signal_max_hold_spin_->value();
        }
        if (g == "RPROB" || g == "RPROBX") {
            params["entry_prob"] = signal_entry_prob_spin_->value();
            params["entryProb"]  = signal_entry_prob_spin_->value();
        }
        if (g == "RPROBX") {
            params["exit_prob"] = signal_exit_prob_spin_->value();
            params["exitProb"]  = signal_exit_prob_spin_->value();
        }
        args["params"] = params;
    }

    if (cmd_id == "labels_to_signals") {
        // Shares the labels page's widgets (the labels_to_signals page only
        // re-picks the generator). Apply the same type→params mapping so we
        // don't send irrelevant params and so non-FIXLB types still get window.
        const QString lt = l2s_label_type_combo_->currentText();
        args["labelType"] = lt;
        args["entryLabel"] = l2s_entry_label_spin_->value();
        args["exitLabel"]  = l2s_exit_label_spin_->value();
        QJsonObject params;
        if (lt == "FIXLB") {
            params["horizon"]   = labels_horizon_spin_->value();
            params["threshold"] = labels_threshold_spin_->value();
        } else if (lt == "MEANLB" || lt == "TRENDLB") {
            params["window"]    = labels_window_spin_->value();
            params["threshold"] = labels_threshold_spin_->value();
        } else if (lt == "LEXLB") {
            params["window"]    = labels_window_spin_->value();
        } else if (lt == "BOLB") {
            params["window"]    = labels_window_spin_->value();
            params["alpha"]     = labels_alpha_spin_->value();
        }
        args["params"] = params;
    }

    if (cmd_id == "indicator_sweep") {
        args["indicator"] = sweep_indicator_combo_->currentData().toString();
        QJsonObject range;
        range["min"] = sweep_min_spin_->value();
        range["max"] = sweep_max_spin_->value();
        range["step"] = sweep_step_spin_->value();
        args["paramRange"] = range;
    }

    return args;
}

// ── Run ──────────────────────────────────────────────────────────────────────

void BacktestingScreen::on_run() {
    if (is_running_)
        return;
    if (active_provider_ < 0 || active_provider_ >= providers_.size())
        return;
    if (active_command_ < 0 || active_command_ >= commands_.size())
        return;

    const auto& provider_info = providers_[active_provider_];
    const auto& command_id = commands_[active_command_].id;

    // Validate command is supported by current provider
    if (!provider_info.commands.contains(command_id)) {
        display_error(
            QString("Command '%1' is not supported by provider '%2'").arg(command_id, provider_info.display_name));
        return;
    }

    // Validate symbols not empty
    if (symbols_edit_->text().trimmed().isEmpty()) {
        display_error("Please enter at least one symbol (e.g. SPY, AAPL)");
        return;
    }

    is_running_ = true;
    run_button_->setEnabled(false);
    set_status_state("EXECUTING...", ui::colors::WARNING, "rgba(217,119,6,0.08)");

    auto args = gather_args();

    LOG_INFO("Backtesting", QString("Executing %1/%2").arg(provider_info.slug, command_id));
    BacktestingService::instance().execute(provider_info.slug, command_id, args);
}

// ── Result display ───────────────────────────────────────────────────────────

void BacktestingScreen::clear_results() {
    clear_layout(summary_layout_);
    metrics_table_->setRowCount(0);
    trades_table_->setRowCount(0);
    trades_table_->setColumnCount(0);
    raw_json_edit_->clear();
}

void BacktestingScreen::display_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    const auto accent = providers_[active_provider_].color.name();

    // ── Local helpers (capture summary_container_ / metrics_table_ / trades_table_) ──

    auto add_section_header = [&](const QString& text, const QString& color = QString()) {
        auto* lbl = new QLabel(text, summary_container_);
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; "
                                   "letter-spacing:1px; padding-top:4px;")
                               .arg(color.isEmpty() ? accent : color)
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY));
        summary_layout_->addWidget(lbl);
    };

    auto humanize = [](QString key) {
        key.replace('_', ' ');
        key.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
        return key.toUpper();
    };

    auto add_cards_from = [&](const QJsonObject& obj, const QStringList& ordered_keys,
                              int cols_per_row = 3) {
        if (obj.isEmpty())
            return;
        auto* cards = new QWidget(summary_container_);
        auto* gl = new QGridLayout(cards);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setSpacing(8);

        // Build the display order: explicit keys first, then any remaining scalars.
        QStringList order = ordered_keys;
        if (order.isEmpty()) {
            for (auto it = obj.begin(); it != obj.end(); ++it)
                if (!it.value().isObject() && !it.value().isArray())
                    order.append(it.key());
        }

        int col = 0, row = 0;
        QStringList shown;
        for (const auto& key : order) {
            if (!obj.contains(key))
                continue;
            const auto& val = obj[key];
            if (val.isObject() || val.isArray() || val.isNull())
                continue;
            auto canon = key;
            canon.replace(QRegularExpression("([a-z])([A-Z])"), "\\1_\\2");
            canon = canon.toLower();
            if (shown.contains(canon))
                continue;
            shown.append(canon);

            auto* card = new QWidget(cards);
            card->setStyleSheet(QString("background:%1; border:1px solid %2;")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* cvl = new QVBoxLayout(card);
            cvl->setContentsMargins(10, 8, 10, 8);
            cvl->setSpacing(2);
            auto* lbl = new QLabel(humanize(key), card);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY));
            auto* vlbl = new QLabel(fmt_metric(key, val), card);
            vlbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                    .arg(accent)
                                    .arg(ui::fonts::DATA)
                                    .arg(ui::fonts::DATA_FAMILY));
            cvl->addWidget(lbl);
            cvl->addWidget(vlbl);
            gl->addWidget(card, row, col);
            if (++col >= cols_per_row) { col = 0; row++; }
        }
        if (row > 0 || col > 0)
            summary_layout_->addWidget(cards);
        else
            cards->deleteLater();
    };

    auto fill_kv_table = [&](const QJsonObject& obj) {
        QStringList keys;
        for (auto it = obj.begin(); it != obj.end(); ++it)
            if (!it.value().isObject() && !it.value().isArray() && !it.value().isNull())
                keys.append(it.key());
        metrics_table_->setRowCount(keys.size());
        for (int r = 0; r < keys.size(); ++r) {
            auto* name = new QTableWidgetItem(humanize(keys[r]));
            metrics_table_->setItem(r, 0, name);
            auto* val = new QTableWidgetItem(fmt_metric(keys[r], obj[keys[r]]));
            val->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            metrics_table_->setItem(r, 1, val);
        }
        metrics_table_->resizeColumnsToContents();
    };

    auto fill_details_table = [&](const QJsonArray& arr, const QStringList& preferred_cols = {}) {
        if (arr.isEmpty() || !arr[0].isObject())
            return;
        auto first = arr[0].toObject();
        QStringList cols = preferred_cols;
        if (cols.isEmpty()) {
            for (auto it = first.begin(); it != first.end(); ++it)
                if (!it.value().isObject() && !it.value().isArray())
                    cols.append(it.key());
        }
        const int rows = qMin(static_cast<int>(arr.size()), 200);
        trades_table_->setColumnCount(cols.size());
        trades_table_->setHorizontalHeaderLabels(cols);
        trades_table_->setRowCount(rows);
        for (int r = 0; r < rows; ++r) {
            auto rec = arr[r].toObject();
            for (int c = 0; c < cols.size(); ++c) {
                auto* item = new QTableWidgetItem(fmt_metric(cols[c], rec[cols[c]]));
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                trades_table_->setItem(r, c, item);
            }
        }
        trades_table_->resizeColumnsToContents();
    };

    // ── Header (per-command title) ──
    QString cmd_label = command.toUpper();
    for (const auto& c : commands_) if (c.id == command) { cmd_label = c.label.toUpper(); break; }
    add_section_header(cmd_label + " RESULTS");

    // ── Synthetic-data warning (Python sets this when yfinance is unavailable) ──
    if (payload.value("usingSyntheticData").toBool()) {
        auto* warn = new QLabel(
            "Using synthetic price data (yfinance unavailable). Results do not reflect real markets.",
            summary_container_);
        warn->setWordWrap(true);
        warn->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:8px 10px; "
                                    "background:rgba(245,158,11,0.10); border:1px solid %4;")
                                .arg(ui::colors::WARNING())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::colors::WARNING()));
        summary_layout_->addWidget(warn);
    }

    // ── Per-command rendering ──

    if (command == "backtest") {
        auto perf = payload.value("performance").toObject();
        add_cards_from(perf, {"totalReturn", "annualizedReturn", "sharpeRatio", "sortinoRatio",
                              "maxDrawdown", "winRate", "profitFactor", "calmarRatio",
                              "totalTrades", "volatility"});
        if (payload.contains("status")) {
            auto status = payload["status"].toString();
            auto* sl = new QLabel(QString("Status: %1").arg(status), summary_container_);
            sl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:8px;")
                                  .arg(status == "success" ? ui::colors::POSITIVE() : ui::colors::WARNING())
                                  .arg(ui::fonts::SMALL)
                                  .arg(ui::fonts::DATA_FAMILY));
            summary_layout_->addWidget(sl);
        }
        fill_kv_table(perf);
        auto trades = payload.value("trades").toArray();
        fill_details_table(trades);
    }
    else if (command == "optimize") {
        QJsonObject summary{
            {"iterations",          payload.value("iterations")},
            {"totalCombinations",   payload.value("totalCombinations")},
            {"method",              payload.value("method")},
            {"objective",           payload.value("objective")},
            {"bestObjectiveValue",  payload.value("bestObjectiveValue")},
        };
        add_cards_from(summary, {"bestObjectiveValue", "iterations", "totalCombinations",
                                 "method", "objective"});

        auto best_params = payload.value("bestParameters").toObject();
        if (!best_params.isEmpty()) {
            add_section_header("BEST PARAMETERS");
            add_cards_from(best_params, {}, 4);
        }
        auto best_perf = payload.value("bestPerformance").toObject();
        if (!best_perf.isEmpty()) {
            add_section_header("BEST PERFORMANCE");
            add_cards_from(best_perf, {"totalReturn", "sharpeRatio", "maxDrawdown",
                                       "winRate", "profitFactor", "totalTrades"});
            fill_kv_table(best_perf);
        }
        // Details: top results — flatten params into the row
        auto all_results = payload.value("allResults").toArray();
        QJsonArray flat;
        for (const auto& v : all_results) {
            auto o = v.toObject();
            QJsonObject row;
            auto params = o.value("parameters").toObject();
            for (auto it = params.begin(); it != params.end(); ++it)
                row[it.key()] = it.value();
            row["objective"] = o.value("objective_value");
            auto perf = o.value("performance").toObject();
            for (const auto& k : QStringList{"totalReturn", "sharpeRatio", "maxDrawdown", "winRate"}) {
                if (perf.contains(k)) row[k] = perf.value(k);
            }
            flat.append(row);
        }
        fill_details_table(flat);
    }
    else if (command == "walk_forward") {
        QJsonObject summary{
            {"nWindows",          payload.value("nWindows")},
            {"avgOosReturn",      payload.value("avgOosReturn")},
            {"avgOosSharpe",      payload.value("avgOosSharpe")},
            {"oosReturnStd",      payload.value("oosReturnStd")},
            {"robustnessScore",   payload.value("robustnessScore")},
            {"degradationRatio",  payload.value("degradationRatio")},
        };
        add_cards_from(summary, {"nWindows", "avgOosReturn", "avgOosSharpe",
                                 "robustnessScore", "degradationRatio", "oosReturnStd"});
        fill_kv_table(summary);
        auto windows = payload.value("windows").toArray();
        fill_details_table(windows, {"window", "trainStart", "trainEnd", "testStart", "testEnd",
                                     "inSampleReturn", "inSampleSharpe",
                                     "outOfSampleReturn", "outOfSampleSharpe"});
    }
    else if (command == "indicator") {
        QJsonObject summary{
            {"indicator",  payload.value("indicator")},
            {"symbol",     payload.value("symbol")},
            {"period",     payload.value("period")},
            {"totalBars",  payload.value("totalBars")},
        };
        add_cards_from(summary, {"indicator", "symbol", "period", "totalBars"}, 4);
        auto stats = payload.value("stats").toObject();
        // Multi-output indicators have stats per component — flatten one level.
        if (!stats.isEmpty()) {
            QJsonObject flat;
            bool nested = false;
            for (auto it = stats.begin(); it != stats.end(); ++it) {
                if (it.value().isObject()) { nested = true; break; }
            }
            if (nested) {
                for (auto it = stats.begin(); it != stats.end(); ++it) {
                    auto sub = it.value().toObject();
                    for (auto sit = sub.begin(); sit != sub.end(); ++sit)
                        flat[it.key() + "." + sit.key()] = sit.value();
                }
            } else {
                flat = stats;
            }
            add_section_header("STATISTICS");
            add_cards_from(flat, {"last", "mean", "std", "min", "max"});
            fill_kv_table(flat);
        }
        // Details: series sample. For multi-output use the first non-empty component.
        auto series_v = payload.value("series");
        QJsonArray sample;
        if (series_v.isArray()) {
            sample = series_v.toArray();
        } else if (series_v.isObject()) {
            auto so = series_v.toObject();
            for (auto it = so.begin(); it != so.end(); ++it) {
                if (it.value().isArray() && !it.value().toArray().isEmpty()) {
                    sample = it.value().toArray();
                    break;
                }
            }
        }
        fill_details_table(sample, {"date", "value"});
    }
    else if (command == "indicator_signals") {
        add_cards_from(payload, {"totalSignals", "winRate", "profitFactor", "avgReturn",
                                 "medianReturn", "bestTrade", "worstTrade",
                                 "avgHoldingPeriod", "signalDensity",
                                 "entryCount", "exitCount", "originalEntryCount", "filteredEntryCount"});
        fill_kv_table(payload);
        // Combine entry & exit signals into a single details table with a Type column.
        auto entries = payload.value("entrySignals").toArray();
        auto exits = payload.value("exitSignals").toArray();
        QJsonArray combined;
        for (const auto& v : entries) { auto o = v.toObject(); o["type"] = "ENTRY"; combined.append(o); }
        for (const auto& v : exits)   { auto o = v.toObject(); o["type"] = "EXIT";  combined.append(o); }
        if (combined.isEmpty()) {
            // Some modes only emit plain entries[]/exits[] string arrays
            QJsonArray plain;
            for (const auto& v : payload.value("entries").toArray()) {
                QJsonObject o; o["type"] = "ENTRY"; o["date"] = v.toString(); plain.append(o);
            }
            for (const auto& v : payload.value("exits").toArray()) {
                QJsonObject o; o["type"] = "EXIT"; o["date"] = v.toString(); plain.append(o);
            }
            combined = plain;
        }
        fill_details_table(combined);
    }
    else if (command == "labels") {
        QJsonObject summary{
            {"labelType",   payload.value("labelType")},
            {"totalBars",   payload.value("totalBars")},
            {"labeledBars", payload.value("labeledBars")},
        };
        add_cards_from(summary, {"labelType", "totalBars", "labeledBars"}, 3);

        auto dist = payload.value("distribution").toObject();
        if (!dist.isEmpty()) {
            add_section_header("CLASS DISTRIBUTION");
            // Re-key so cards show "Class -1 / Class 0 / Class 1"
            QJsonObject relabeled;
            QStringList ordered;
            QStringList raw_keys = dist.keys();
            std::sort(raw_keys.begin(), raw_keys.end(), [](const QString& a, const QString& b) {
                return a.toInt() < b.toInt();
            });
            for (const auto& k : raw_keys) {
                QString rk = "Class " + k;
                relabeled[rk] = dist.value(k);
                ordered.append(rk);
            }
            add_cards_from(relabeled, ordered, 5);
            fill_kv_table(relabeled);
        }
        fill_details_table(payload.value("sampleLabels").toArray(), {"date", "label"});
    }
    else if (command == "splits") {
        QJsonObject summary{
            {"splitterType", payload.value("splitterType")},
            {"nSplits",      payload.value("nSplits")},
            {"totalBars",    payload.value("totalBars")},
            {"indexStart",   payload.value("indexStart")},
            {"indexEnd",     payload.value("indexEnd")},
        };
        add_cards_from(summary, {"splitterType", "nSplits", "totalBars", "indexStart", "indexEnd"}, 3);
        fill_kv_table(summary);
        fill_details_table(payload.value("splits").toArray(),
                           {"fold", "trainStart", "trainEnd", "trainSize",
                            "testStart", "testEnd", "testSize"});
    }
    else if (command == "returns") {
        QJsonObject summary{
            {"analysisType", payload.value("analysisType")},
            {"totalBars",    payload.value("totalBars")},
            {"returnBars",   payload.value("returnBars")},
        };
        add_cards_from(summary, {"analysisType", "totalBars", "returnBars"}, 3);

        // The interesting numbers live in payload.stats (returns_stats / drawdowns / ranges)
        // OR scattered at the top level for other shapes.
        auto stats = payload.value("stats").toObject();
        if (!stats.isEmpty()) {
            add_section_header("STATISTICS");
            add_cards_from(stats, {"Total Return", "Annualized Return", "Sharpe Ratio",
                                   "Sortino Ratio", "Calmar Ratio", "Max Drawdown",
                                   "Annualized Volatility", "Downside Risk"});
            fill_kv_table(stats);
        } else {
            // rolling-style payloads expose top-level fields
            QJsonObject topnum;
            for (const auto& k : QStringList{"metric", "window", "dataPoints", "maxDrawdown",
                                             "avgDrawdown", "totalDrawdowns", "activeDrawdown",
                                             "activeDuration", "totalRanges", "avgDuration",
                                             "maxDuration", "coverage"}) {
                if (payload.contains(k)) topnum[k] = payload.value(k);
            }
            if (!topnum.isEmpty()) {
                add_cards_from(topnum, {});
                fill_kv_table(topnum);
            }
        }

        // Details: the series / records that ship with the chosen analysis type
        QJsonArray detail;
        for (const auto& k : QStringList{"records", "cumulativeReturns", "drawdownSeries", "series"}) {
            if (payload.value(k).isArray()) {
                auto a = payload.value(k).toArray();
                if (!a.isEmpty()) { detail = a; break; }
            }
        }
        fill_details_table(detail);
    }
    else if (command == "signals") {
        QJsonObject summary{
            {"generatorType", payload.value("generatorType")},
            {"totalBars",     payload.value("totalBars")},
            {"entryCount",    payload.value("entryCount")},
            {"exitCount",     payload.value("exitCount")},
        };
        add_cards_from(summary, {"generatorType", "entryCount", "exitCount", "totalBars"}, 4);
        fill_kv_table(summary);

        QJsonArray combined;
        for (const auto& v : payload.value("entries").toArray()) {
            QJsonObject o; o["type"] = "ENTRY"; o["date"] = v.toString(); combined.append(o);
        }
        for (const auto& v : payload.value("exits").toArray()) {
            QJsonObject o; o["type"] = "EXIT"; o["date"] = v.toString(); combined.append(o);
        }
        fill_details_table(combined, {"type", "date"});
    }
    else if (command == "labels_to_signals") {
        QJsonObject summary{
            {"labelType",  payload.value("labelType")},
            {"entryLabel", payload.value("entryLabel")},
            {"exitLabel",  payload.value("exitLabel")},
            {"entryCount", payload.value("entryCount")},
            {"exitCount",  payload.value("exitCount")},
            {"totalBars",  payload.value("totalBars")},
        };
        add_cards_from(summary, {"labelType", "entryCount", "exitCount", "totalBars",
                                 "entryLabel", "exitLabel"}, 3);
        fill_kv_table(summary);
        QJsonArray combined;
        for (const auto& v : payload.value("entries").toArray()) {
            QJsonObject o; o["type"] = "ENTRY"; o["date"] = v.toString(); combined.append(o);
        }
        for (const auto& v : payload.value("exits").toArray()) {
            QJsonObject o; o["type"] = "EXIT"; o["date"] = v.toString(); combined.append(o);
        }
        fill_details_table(combined, {"type", "date"});
    }
    else if (command == "indicator_sweep") {
        QJsonObject summary{
            {"indicator",         payload.value("indicator")},
            {"totalCombinations", payload.value("totalCombinations")},
        };
        add_cards_from(summary, {"indicator", "totalCombinations"}, 2);
        // Flatten each result row: params + scalar stats
        auto results = payload.value("results").toArray();
        QJsonArray flat;
        for (const auto& v : results) {
            auto o = v.toObject();
            QJsonObject row;
            auto params = o.value("params").toObject();
            for (auto it = params.begin(); it != params.end(); ++it)
                row[it.key()] = it.value();
            for (const auto& k : QStringList{"mean", "std", "min", "max", "last"})
                if (o.contains(k)) row[k] = o.value(k);
            flat.append(row);
        }
        fill_details_table(flat);
    }
    else {
        // Unknown command — best-effort: show all scalar fields as cards.
        add_cards_from(payload, {});
        fill_kv_table(payload);
    }

    summary_layout_->addStretch();

    // ── RAW JSON tab (always populated) ──
    raw_json_edit_->setPlainText(QJsonDocument(payload).toJson(QJsonDocument::Indented));

    // Switch to summary tab
    result_tabs_->setCurrentIndex(0);
}

void BacktestingScreen::display_error(const QString& msg) {
    clear_results();

    auto* err = new QLabel(msg, summary_container_);
    err->setWordWrap(true);
    err->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:12px;"
                               "background:rgba(220,38,38,0.08); border:1px solid rgba(220,38,38,0.3);")
                           .arg(ui::colors::NEGATIVE())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY));
    summary_layout_->addWidget(err);
    summary_layout_->addStretch();

    // Also put error in raw JSON tab for debugging
    raw_json_edit_->setPlainText(msg);

    result_tabs_->setCurrentIndex(0);
}

// ── Signal handlers ──────────────────────────────────────────────────────────

void BacktestingScreen::on_result(const QString& provider, const QString& command, const QJsonObject& payload) {
    // Route background metadata commands — don't touch run state or results UI
    if (command == "get_strategies") {
        // Only apply if result is for the currently-active provider
        if (provider != providers_[active_provider_].slug)
            return;
        strategies_ = services::backtest::strategies_from_json(payload);
        auto cats = services::backtest::categories_from_strategies(strategies_);
        strategy_category_combo_->blockSignals(true);
        strategy_category_combo_->clear();
        for (const auto& cat : cats)
            strategy_category_combo_->addItem(cat);
        strategy_category_combo_->blockSignals(false);
        populate_strategies();
        LOG_INFO("Backtesting", QString("[%1] Loaded %2 strategies").arg(provider).arg(strategies_.size()));
        return;
    }
    if (command == "get_indicators") {
        if (provider != providers_[active_provider_].slug)
            return;
        // Populate all three indicator combos from {indicators:{Cat:[{id,name}]}}.
        // sweep_indicator_combo_ may be null if a future refactor strips the
        // sweep page — guard so we don't crash older builds during migration.
        auto ind_obj = payload.value("indicators").toObject();
        indicator_combo_->clear();
        ind_signal_indicator_combo_->clear();
        if (sweep_indicator_combo_)
            sweep_indicator_combo_->clear();
        for (const auto& cat : ind_obj.keys()) {
            for (const auto& iv : ind_obj.value(cat).toArray()) {
                auto o = iv.toObject();
                auto id = o.value("id").toString();
                auto name = o.value("name").toString(id);
                indicator_combo_->addItem(name, id);
                ind_signal_indicator_combo_->addItem(name, id);
                if (sweep_indicator_combo_)
                    sweep_indicator_combo_->addItem(name, id);
            }
        }
        return;
    }

    // Regular command result — update run state and display
    is_running_ = false;
    run_button_->setEnabled(true);
    set_status_state("READY", ui::colors::POSITIVE, "rgba(22,163,74,0.08)");
    display_result(command, payload);
    LOG_INFO("Backtesting", QString("[%1/%2] Complete").arg(provider, command));
}

void BacktestingScreen::on_command_options_loaded(const QString& provider, const QJsonObject& options) {
    if (provider != providers_[active_provider_].slug)
        return;

    auto repopulate = [](QComboBox* combo, const QJsonArray& arr) {
        if (arr.isEmpty())
            return;
        combo->clear();
        for (const auto& v : arr)
            combo->addItem(v.toString());
        combo->setCurrentIndex(0);
    };

    // Providers using json_response() emit camelCase keys; fincept_provider
    // uses raw json.dumps so its keys stay snake_case. Try camelCase first
    // (the dominant form across vectorbt/bt/backtestingpy/fasttrade/zipline),
    // then fall back to snake_case for fincept.
    auto pick = [&](const char* camel, const char* snake) -> QJsonArray {
        auto a = options.value(camel).toArray();
        return a.isEmpty() ? options.value(snake).toArray() : a;
    };

    repopulate(pos_sizing_combo_, pick("positionSizingMethods", "position_sizing_methods"));
    repopulate(opt_objective_combo_, pick("optimizeObjectives", "optimize_objectives"));
    repopulate(opt_method_combo_, pick("optimizeMethods", "optimize_methods"));
    repopulate(labels_type_combo_, pick("labelTypes", "label_types"));
    repopulate(splitter_type_combo_, pick("splitterTypes", "splitter_types"));
    repopulate(signal_gen_combo_, pick("signalGenerators", "signal_generators"));
    repopulate(ind_signal_mode_combo_, pick("indicatorSignalModes", "indicator_signal_modes"));
    repopulate(returns_type_combo_, pick("returnsAnalysisTypes", "returns_analysis_types"));

    LOG_INFO("Backtesting", QString("[%1] Command options loaded").arg(provider));
}

void BacktestingScreen::on_error(const QString& context, const QString& message) {
    is_running_ = false;
    run_button_->setEnabled(true);
    set_status_state("ERROR", ui::colors::NEGATIVE, "rgba(220,38,38,0.08)");
    display_error(QString("[%1] %2").arg(context, message));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap BacktestingScreen::save_state() const {
    return {
        {"provider", active_provider_},
        {"command", active_command_},
    };
}

void BacktestingScreen::restore_state(const QVariantMap& state) {
    const int prov = state.value("provider", 0).toInt();
    const int cmd = state.value("command", 0).toInt();
    if (prov != active_provider_)
        on_provider_changed(prov);
    if (cmd != active_command_)
        on_command_changed(cmd);
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

SymbolRef BacktestingScreen::current_symbol() const {
    if (!symbols_edit_)
        return {};
    const QString text = symbols_edit_->text().trimmed();
    if (text.isEmpty())
        return {};
    // First comma-separated token is the "active" symbol for group linking.
    const QString first = text.section(',', 0, 0).trimmed();
    if (first.isEmpty())
        return {};
    return SymbolRef::equity(first);
}

void BacktestingScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid() || !symbols_edit_)
        return;
    // Replace the entire field — adding to a comma list silently would
    // surprise users who have a multi-symbol backtest configured.
    if (symbols_edit_->text().trimmed() == ref.symbol)
        return;
    QSignalBlocker block(symbols_edit_); // avoid bouncing the publish back
    symbols_edit_->setText(ref.symbol);
}

void BacktestingScreen::publish_first_symbol_to_group() {
    if (link_group_ == SymbolGroup::None)
        return;
    const SymbolRef ref = current_symbol();
    if (!ref.is_valid())
        return;
    SymbolContext::instance().set_group_symbol(link_group_, ref, this);
}

} // namespace fincept::screens
