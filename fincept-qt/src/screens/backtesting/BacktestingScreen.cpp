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

QWidget* BacktestingScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(34);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* brand = new QLabel("BACKTESTING", bar);
    brand->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                 "padding:4px 12px; background:rgba(217,119,6,0.1);"
                                 "border:1px solid %4;")
                             .arg(ui::colors::AMBER())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::colors::AMBER_DIM()));
    hl->addWidget(brand);

    auto* div = new QWidget(bar);
    div->setFixedSize(1, 18);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    hl->addWidget(div);

    // Provider tabs
    for (int i = 0; i < providers_.size(); ++i) {
        const auto& p = providers_[i];
        auto* btn = new QPushButton(p.display_name, bar);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_provider_changed(i); });
        hl->addWidget(btn);
        provider_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Run button — accent amber style per DESIGN_SYSTEM 5.5
    run_button_ = new QPushButton("RUN", bar);
    run_button_->setCursor(Qt::PointingHandCursor);
    run_button_->setStyleSheet(
        QString("QPushButton { background:rgba(217,119,6,0.1); color:%1; font-family:%2; font-size:%3px;"
                "font-weight:700; border:1px solid %4; padding:0 12px;"
                "letter-spacing:1px; }"
                "QPushButton:hover { background:%1; color:%5; }"
                "QPushButton:disabled { background:%6; color:%7; border-color:%8; }")
            .arg(ui::colors::AMBER())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL)
            .arg(ui::colors::AMBER_DIM())
            .arg(ui::colors::BG_BASE())
            .arg(ui::colors::BG_RAISED())
            .arg(ui::colors::TEXT_DIM())
            .arg(ui::colors::BORDER_DIM()));
    connect(run_button_, &QPushButton::clicked, this, &BacktestingScreen::on_run);
    hl->addWidget(run_button_);

    // Status dot
    status_dot_ = new QLabel("READY", bar);
    status_dot_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                       "padding:3px 8px; background:rgba(22,163,74,0.08);"
                                       "border:1px solid rgba(22,163,74,0.25);")
                                   .arg(ui::colors::POSITIVE())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY));
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

    // ── Strategies section ──
    vl->addSpacing(8);
    auto* strat_title = new QLabel("STRATEGY", content);
    strat_title->setStyleSheet(section_style);
    vl->addWidget(strat_title);

    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                               "selection-background-color:rgba(217,119,6,0.15); }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    auto* cat_lbl = new QLabel("CATEGORY", content);
    cat_lbl->setStyleSheet(label_style);
    vl->addWidget(cat_lbl);
    strategy_category_combo_ = new QComboBox(content);
    strategy_category_combo_->setStyleSheet(combo_style);
    // Populated dynamically via on_result("get_strategies")
    connect(strategy_category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { populate_strategies(); });
    vl->addWidget(strategy_category_combo_);

    auto* strat_lbl = new QLabel("STRATEGY", content);
    strat_lbl->setStyleSheet(label_style);
    vl->addWidget(strat_lbl);
    strategy_combo_ = new QComboBox(content);
    strategy_combo_->setStyleSheet(combo_style);
    connect(strategy_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &BacktestingScreen::on_strategy_changed);
    vl->addWidget(strategy_combo_);

    // ── Strategy parameters ──
    vl->addSpacing(4);
    auto* params_title = new QLabel("PARAMETERS", content);
    params_title->setStyleSheet(label_style);
    vl->addWidget(params_title);

    strategy_params_container_ = new QWidget(content);
    strategy_params_layout_ = new QVBoxLayout(strategy_params_container_);
    strategy_params_layout_->setContentsMargins(0, 0, 0, 0);
    strategy_params_layout_->setSpacing(4);
    vl->addWidget(strategy_params_container_);

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
    result_tabs_->addTab(trades_table_, "TRADES");

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

    // ── MARKET DATA ──
    auto* mkt_title = new QLabel("MARKET DATA", content);
    mkt_title->setStyleSheet(section_style);
    vl->addWidget(mkt_title);

    auto* sym_lbl = new QLabel("SYMBOLS", content);
    sym_lbl->setStyleSheet(label_style);
    vl->addWidget(sym_lbl);
    symbols_edit_ = new QLineEdit("SPY", content);
    symbols_edit_->setPlaceholderText("SPY,AAPL,MSFT");
    symbols_edit_->setStyleSheet(input_style);
    vl->addWidget(symbols_edit_);
    // Publish the first symbol to the linked group when the user finishes editing.
    connect(symbols_edit_, &QLineEdit::editingFinished, this,
            [this]() { publish_first_symbol_to_group(); });

    auto* cap_lbl = new QLabel("INITIAL CAPITAL ($)", content);
    cap_lbl->setStyleSheet(label_style);
    vl->addWidget(cap_lbl);
    capital_spin_ = new QDoubleSpinBox(content);
    capital_spin_->setRange(1000, 1e12);
    capital_spin_->setValue(100000);
    capital_spin_->setDecimals(0);
    capital_spin_->setStyleSheet(input_style);
    vl->addWidget(capital_spin_);

    auto* dates = new QGridLayout;
    dates->setSpacing(8);
    auto* sd_lbl = new QLabel("START", content);
    sd_lbl->setStyleSheet(label_style);
    dates->addWidget(sd_lbl, 0, 0);
    start_date_ = new QDateEdit(QDate::currentDate().addYears(-1), content);
    start_date_->setDisplayFormat("yyyy-MM-dd");
    start_date_->setCalendarPopup(true);
    start_date_->setStyleSheet(input_style);
    dates->addWidget(start_date_, 1, 0);
    auto* ed_lbl = new QLabel("END", content);
    ed_lbl->setStyleSheet(label_style);
    dates->addWidget(ed_lbl, 0, 1);
    end_date_ = new QDateEdit(QDate::currentDate().addDays(-1), content);
    end_date_->setDisplayFormat("yyyy-MM-dd");
    end_date_->setCalendarPopup(true);
    end_date_->setStyleSheet(input_style);
    dates->addWidget(end_date_, 1, 1);
    vl->addLayout(dates);

    // ── EXECUTION SETTINGS ──
    vl->addSpacing(8);
    auto* exec_title = new QLabel("EXECUTION", content);
    exec_title->setStyleSheet(section_style);
    vl->addWidget(exec_title);

    auto* comm_lbl = new QLabel("COMMISSION (%)", content);
    comm_lbl->setStyleSheet(label_style);
    vl->addWidget(comm_lbl);
    commission_spin_ = new QDoubleSpinBox(content);
    commission_spin_->setRange(0, 10);
    commission_spin_->setValue(0.1);
    commission_spin_->setDecimals(3);
    commission_spin_->setSuffix("%");
    commission_spin_->setStyleSheet(input_style);
    vl->addWidget(commission_spin_);

    auto* slip_lbl = new QLabel("SLIPPAGE (%)", content);
    slip_lbl->setStyleSheet(label_style);
    vl->addWidget(slip_lbl);
    slippage_spin_ = new QDoubleSpinBox(content);
    slippage_spin_->setRange(0, 10);
    slippage_spin_->setValue(0.05);
    slippage_spin_->setDecimals(4);
    slippage_spin_->setSuffix("%");
    slippage_spin_->setStyleSheet(input_style);
    vl->addWidget(slippage_spin_);

    // ── ADVANCED ──
    vl->addSpacing(8);
    auto* adv_title = new QLabel("ADVANCED", content);
    adv_title->setStyleSheet(section_style);
    vl->addWidget(adv_title);

    auto* lev_lbl = new QLabel("LEVERAGE", content);
    lev_lbl->setStyleSheet(label_style);
    vl->addWidget(lev_lbl);
    leverage_spin_ = new QDoubleSpinBox(content);
    leverage_spin_->setRange(0.1, 10);
    leverage_spin_->setValue(1.0);
    leverage_spin_->setDecimals(1);
    leverage_spin_->setSuffix("x");
    leverage_spin_->setStyleSheet(input_style);
    vl->addWidget(leverage_spin_);

    auto* sl_lbl = new QLabel("STOP LOSS (%)", content);
    sl_lbl->setStyleSheet(label_style);
    vl->addWidget(sl_lbl);
    stop_loss_spin_ = new QDoubleSpinBox(content);
    stop_loss_spin_->setRange(0, 100);
    stop_loss_spin_->setValue(0);
    stop_loss_spin_->setDecimals(1);
    stop_loss_spin_->setSuffix("%");
    stop_loss_spin_->setSpecialValueText("None");
    stop_loss_spin_->setStyleSheet(input_style);
    vl->addWidget(stop_loss_spin_);

    auto* tp_lbl = new QLabel("TAKE PROFIT (%)", content);
    tp_lbl->setStyleSheet(label_style);
    vl->addWidget(tp_lbl);
    take_profit_spin_ = new QDoubleSpinBox(content);
    take_profit_spin_->setRange(0, 1000);
    take_profit_spin_->setValue(0);
    take_profit_spin_->setDecimals(1);
    take_profit_spin_->setSuffix("%");
    take_profit_spin_->setSpecialValueText("None");
    take_profit_spin_->setStyleSheet(input_style);
    vl->addWidget(take_profit_spin_);

    auto* ps_lbl = new QLabel("POSITION SIZING", content);
    ps_lbl->setStyleSheet(label_style);
    vl->addWidget(ps_lbl);
    pos_sizing_combo_ = new QComboBox(content);
    pos_sizing_combo_->setStyleSheet(combo_style);
    for (const auto& m : position_sizing_methods())
        pos_sizing_combo_->addItem(m);
    vl->addWidget(pos_sizing_combo_);

    allow_short_check_ = new QCheckBox("Allow Short Selling", content);
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
    vl->addWidget(allow_short_check_);

    auto* bm_lbl = new QLabel("BENCHMARK", content);
    bm_lbl->setStyleSheet(label_style);
    vl->addWidget(bm_lbl);
    benchmark_edit_ = new QLineEdit("SPY", content);
    benchmark_edit_->setStyleSheet(input_style);
    vl->addWidget(benchmark_edit_);

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

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 5: labels
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

        auto* hl = new QLabel("HORIZON", page);
        hl->setStyleSheet(label_style);
        pl->addWidget(hl);
        labels_horizon_spin_ = new QSpinBox(page);
        labels_horizon_spin_->setRange(1, 100);
        labels_horizon_spin_->setValue(5);
        labels_horizon_spin_->setStyleSheet(input_style);
        pl->addWidget(labels_horizon_spin_);

        auto* thl = new QLabel("THRESHOLD", page);
        thl->setStyleSheet(label_style);
        pl->addWidget(thl);
        labels_threshold_spin_ = new QDoubleSpinBox(page);
        labels_threshold_spin_->setRange(0.001, 1.0);
        labels_threshold_spin_->setValue(0.02);
        labels_threshold_spin_->setDecimals(3);
        labels_threshold_spin_->setSingleStep(0.005);
        labels_threshold_spin_->setStyleSheet(input_style);
        pl->addWidget(labels_threshold_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 6: splits
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

        auto* wl = new QLabel("WINDOW LENGTH", page);
        wl->setStyleSheet(label_style);
        pl->addWidget(wl);
        splitter_window_spin_ = new QSpinBox(page);
        splitter_window_spin_->setRange(10, 500);
        splitter_window_spin_->setValue(60);
        splitter_window_spin_->setStyleSheet(input_style);
        pl->addWidget(splitter_window_spin_);

        auto* stl = new QLabel("STEP SIZE", page);
        stl->setStyleSheet(label_style);
        pl->addWidget(stl);
        splitter_step_spin_ = new QSpinBox(page);
        splitter_step_spin_->setRange(1, 100);
        splitter_step_spin_->setValue(10);
        splitter_step_spin_->setStyleSheet(input_style);
        pl->addWidget(splitter_step_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 7: returns
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

        auto* wl = new QLabel("ROLLING WINDOW", page);
        wl->setStyleSheet(label_style);
        pl->addWidget(wl);
        returns_window_spin_ = new QSpinBox(page);
        returns_window_spin_->setRange(5, 252);
        returns_window_spin_->setValue(21);
        returns_window_spin_->setStyleSheet(input_style);
        pl->addWidget(returns_window_spin_);

        pl->addStretch();
        cmd_config_stack_->addWidget(page);
    }

    // Page 8: signals
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
        bool active = (i == active_provider_);
        provider_buttons_[i]->setStyleSheet(
            QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                    "padding:0 10px; border:1px solid %4;"
                    "background:%5; font-weight:%6; }"
                    "QPushButton:hover { background:rgba(%7,0.15); }")
                .arg(active ? p.color.name() : ui::colors::TEXT_TERTIARY())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY)
                .arg(active ? QString("rgba(%1,%2,%3,0.3)").arg(p.color.red()).arg(p.color.green()).arg(p.color.blue())
                            : ui::colors::BORDER_DIM())
                .arg(active ? QString("rgba(%1,%2,%3,0.12)").arg(p.color.red()).arg(p.color.green()).arg(p.color.blue())
                            : "transparent")
                .arg(active ? "700" : "400")
                .arg(QString("%1,%2,%3").arg(p.color.red()).arg(p.color.green()).arg(p.color.blue())));
    }
}

void BacktestingScreen::on_command_changed(int index) {
    const auto& cmds = commands_;
    if (index < 0 || index >= cmds.size())
        return;
    active_command_ = index;
    update_command_buttons();

    // Map command index to config stack page
    // commands: backtest(0), optimize(1), walk_forward(2), indicator(3),
    //           indicator_signals(4), labels(5), splits(6), returns(7), signals(8)
    cmd_config_stack_->setCurrentIndex(index);
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

    auto label_style = QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY);

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
        args["optimizeMethod"] = opt_method_combo_->currentText();
        args["maxIterations"] = opt_iterations_spin_->value();

        // Build param ranges from strategy params
        QJsonObject ranges;
        for (auto* spin : param_spinboxes_) {
            auto name = spin->property("param_name").toString();
            double val = spin->value();
            QJsonObject range;
            range["min"] = qMax(spin->minimum(), val * 0.8);
            range["max"] = qMin(spin->maximum(), val * 1.2);
            range["step"] = spin->singleStep();
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
    }

    if (cmd_id == "indicator_signals") {
        args["indicator"] = ind_signal_indicator_combo_->currentData().toString();
        args["mode"] = ind_signal_mode_combo_->currentText();
    }

    if (cmd_id == "labels") {
        args["labelType"] = labels_type_combo_->currentText();
        QJsonObject params;
        params["horizon"] = labels_horizon_spin_->value();
        params["threshold"] = labels_threshold_spin_->value();
        args["params"] = params;
    }

    if (cmd_id == "splits") {
        args["splitterType"] = splitter_type_combo_->currentText();
        QJsonObject params;
        params["windowLength"] = splitter_window_spin_->value();
        params["step"] = splitter_step_spin_->value();
        args["params"] = params;
    }

    if (cmd_id == "returns") {
        args["analysisType"] = returns_type_combo_->currentText();
        args["rollingWindow"] = returns_window_spin_->value();
        if (returns_type_combo_->currentText() == "benchmark_comparison")
            args["benchmarkSymbol"] = benchmark_edit_->text().trimmed();
    }

    if (cmd_id == "signals") {
        args["generatorType"] = signal_gen_combo_->currentText();
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

void BacktestingScreen::display_result(const QJsonObject& payload) {
    clear_results();

    auto accent = providers_[active_provider_].color.name();

    // ── SUMMARY tab: metric cards ──
    auto* header = new QLabel("BACKTEST RESULTS", summary_container_);
    header->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                  "letter-spacing:1px;")
                              .arg(accent)
                              .arg(ui::fonts::TINY)
                              .arg(ui::fonts::DATA_FAMILY));
    summary_layout_->addWidget(header);

    // Extract performance metrics (handle nested structures)
    auto perf = payload.contains("performance") ? payload["performance"].toObject()
                : payload.contains("payload")      ? payload["payload"].toObject().value("performance").toObject()
                                             : payload;

    // Key metric cards in grid
    auto* cards = new QWidget(summary_container_);
    auto* gl = new QGridLayout(cards);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(8);

    QStringList key_metrics = {"total_return", "sharpe_ratio", "max_drawdown", "win_rate", "profit_factor",
                               "total_trades", "annualized_return", "sortino_ratio", "calmar_ratio", "volatility",
                               "alpha", "beta",
                               // camelCase variants
                               "totalReturn", "sharpeRatio", "maxDrawdown", "winRate", "profitFactor", "totalTrades",
                               "annualizedReturn", "sortinoRatio", "calmarRatio"};

    int col = 0, row = 0;
    QStringList shown;
    for (const auto& key : key_metrics) {
        if (!perf.contains(key))
            continue;
        // Skip duplicate camelCase if we already showed snake_case
        auto normalized = key;
        normalized.replace(QRegularExpression("([a-z])([A-Z])"), "\\1_\\2");
        normalized = normalized.toLower();
        if (shown.contains(normalized))
            continue;
        shown.append(normalized);

        auto display_label = key;
        display_label.replace('_', ' ');
        // Also handle camelCase
        display_label.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");

        auto* card = new QWidget(cards);
        card->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED()).arg(ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(10, 8, 10, 8);
        cvl->setSpacing(2);

        auto* lbl = new QLabel(display_label.toUpper(), card);
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY));
        auto* val = new QLabel(fmt_metric(key, perf[key]), card);
        val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                               .arg(accent)
                               .arg(ui::fonts::DATA)
                               .arg(ui::fonts::DATA_FAMILY));
        cvl->addWidget(lbl);
        cvl->addWidget(val);
        gl->addWidget(card, row, col);
        if (++col >= 3) {
            col = 0;
            row++;
        }
    }

    if (row > 0 || col > 0)
        summary_layout_->addWidget(cards);
    else
        cards->deleteLater();

    // If result has a status/message, show it
    if (payload.contains("status")) {
        auto status = payload["status"].toString();
        auto* status_lbl = new QLabel(QString("Status: %1").arg(status), summary_container_);
        status_lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:8px;")
                                      .arg(status == "success" ? ui::colors::POSITIVE() : ui::colors::WARNING())
                                      .arg(ui::fonts::SMALL)
                                      .arg(ui::fonts::DATA_FAMILY));
        summary_layout_->addWidget(status_lbl);
    }

    summary_layout_->addStretch();

    // ── METRICS tab: full metrics table ──
    QStringList all_keys;
    for (auto it = perf.begin(); it != perf.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            all_keys.append(it.key());

    if (!all_keys.isEmpty()) {
        metrics_table_->setRowCount(all_keys.size());
        for (int r = 0; r < all_keys.size(); ++r) {
            auto label = all_keys[r];
            label.replace('_', ' ');
            label.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
            auto* name_item = new QTableWidgetItem(label.toUpper());
            metrics_table_->setItem(r, 0, name_item);

            auto* val_item = new QTableWidgetItem(fmt_metric(all_keys[r], perf[all_keys[r]]));
            val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            metrics_table_->setItem(r, 1, val_item);
        }
        metrics_table_->resizeColumnsToContents();
    }

    // ── TRADES tab ──
    auto trades = payload.contains("trades") ? payload["trades"].toArray()
                  : payload.contains("payload") ? payload["payload"].toObject().value("trades").toArray()
                                          : QJsonArray();

    if (!trades.isEmpty() && trades[0].isObject()) {
        auto first = trades[0].toObject();
        QStringList cols;
        for (auto it = first.begin(); it != first.end(); ++it)
            if (!it.value().isObject() && !it.value().isArray())
                cols.append(it.key());

        int show = qMin(static_cast<int>(trades.size()), 100);
        trades_table_->setColumnCount(cols.size());
        trades_table_->setHorizontalHeaderLabels(cols);
        trades_table_->setRowCount(show);

        for (int r = 0; r < show; ++r) {
            auto trade = trades[r].toObject();
            for (int c = 0; c < cols.size(); ++c) {
                auto* item = new QTableWidgetItem(fmt_metric(cols[c], trade[cols[c]]));
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                trades_table_->setItem(r, c, item);
            }
        }
        trades_table_->resizeColumnsToContents();
    }

    // ── RAW JSON tab ──
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
        // Populate both indicator combos from {indicators:{Cat:[{id,name}]}}
        auto ind_obj = payload.value("indicators").toObject();
        indicator_combo_->clear();
        ind_signal_indicator_combo_->clear();
        for (const auto& cat : ind_obj.keys()) {
            for (const auto& iv : ind_obj.value(cat).toArray()) {
                auto o = iv.toObject();
                auto id = o.value("id").toString();
                auto name = o.value("name").toString(id);
                indicator_combo_->addItem(name, id);
                ind_signal_indicator_combo_->addItem(name, id);
            }
        }
        return;
    }

    // Regular command result — update run state and display
    is_running_ = false;
    run_button_->setEnabled(true);
    set_status_state("READY", ui::colors::POSITIVE, "rgba(22,163,74,0.08)");
    display_result(payload);
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

    repopulate(pos_sizing_combo_, options.value("position_sizing_methods").toArray());
    repopulate(opt_objective_combo_, options.value("optimize_objectives").toArray());
    repopulate(opt_method_combo_, options.value("optimize_methods").toArray());
    repopulate(labels_type_combo_, options.value("label_types").toArray());
    repopulate(splitter_type_combo_, options.value("splitter_types").toArray());
    repopulate(signal_gen_combo_, options.value("signal_generators").toArray());
    repopulate(ind_signal_mode_combo_, options.value("indicator_signal_modes").toArray());
    repopulate(returns_type_combo_, options.value("returns_analysis_types").toArray());

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
