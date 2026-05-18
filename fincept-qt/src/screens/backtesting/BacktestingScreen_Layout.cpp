// src/screens/backtesting/BacktestingScreen_Layout.cpp
//
// UI construction for the BacktestingScreen — top bar, left panel (commands
// + strategy selector), center panel (tabbed results), right panel (config),
// and status bar. Pure layout: no signal handling or business logic.
//
// Part of the partial-class split of BacktestingScreen.cpp; helpers shared
// across split files live in BacktestingScreen_internal.h.

#include "screens/backtesting/BacktestingScreen.h"
#include "screens/backtesting/BacktestingScreen_internal.h"

#include "core/logging/Logger.h"
#include "services/backtesting/BacktestingService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QScrollArea>
#include <QSplitter>

namespace fincept::screens {

using namespace fincept::services::backtest;
using fincept::screens::backtesting_internal::pill_qss;
using fincept::screens::backtesting_internal::apply_pill_geometry;

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

} // namespace fincept::screens
