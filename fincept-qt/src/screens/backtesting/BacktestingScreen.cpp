// src/screens/backtesting/BacktestingScreen.cpp
#include "screens/backtesting/BacktestingScreen.h"
#include "services/backtesting/BacktestingService.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollArea>
#include <QTabWidget>

#include <cmath>

namespace fincept::screens {

using namespace fincept::services::backtest;

// ── Constructor ──────────────────────────────────────────────────────────────
BacktestingScreen::BacktestingScreen(QWidget* parent) : QWidget(parent) {
    providers_ = all_providers();
    strategies_ = default_strategies();
    build_ui();
    connect_service();
    LOG_INFO("Backtesting", "Screen constructed");
}

void BacktestingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_provider_changed(0);
    }
}

void BacktestingScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

void BacktestingScreen::connect_service() {
    auto& svc = BacktestingService::instance();
    connect(&svc, &BacktestingService::result_ready, this, &BacktestingScreen::on_result);
    connect(&svc, &BacktestingService::error_occurred, this, &BacktestingScreen::on_error);
}

void BacktestingScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    body->addWidget(build_left_panel());
    body->addWidget(build_center_panel(), 1);
    body->addWidget(build_right_panel());

    auto* body_w = new QWidget(this);
    body_w->setLayout(body);
    root->addWidget(body_w, 1);
    root->addWidget(build_status_bar());

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
}

// ── Top Bar: Provider tabs + Run button ──────────────────────────────────────
QWidget* BacktestingScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);
    bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* brand = new QLabel("BACKTESTING", bar);
    brand->setStyleSheet(QString(
        "color:#FF6B35; font-size:%1px; font-weight:700; font-family:%2;"
        "padding:4px 12px; background:rgba(255,107,53,0.08);"
        "border:1px solid rgba(255,107,53,0.25); border-radius:2px;")
        .arg(ui::fonts::TINY).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(brand);

    auto* div = new QWidget(bar);
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    hl->addWidget(div);

    // Provider tabs
    for (int i = 0; i < providers_.size(); ++i) {
        const auto& p = providers_[i];
        auto* btn = new QPushButton(p.display_name, bar);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { color:%1; font-size:9px; font-family:%2;"
            "padding:4px 10px; border:1px solid transparent; border-radius:2px;"
            "background:transparent; }"
            "QPushButton:hover { background:rgba(%3,0.1); color:%4; }")
            .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY)
            .arg(QString("%1,%2,%3").arg(p.color.red()).arg(p.color.green()).arg(p.color.blue()))
            .arg(p.color.name()));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_provider_changed(i); });
        hl->addWidget(btn);
        provider_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Run button
    run_button_ = new QPushButton("RUN", bar);
    run_button_->setCursor(Qt::PointingHandCursor);
    run_button_->setStyleSheet(QString(
        "QPushButton { background:#FF6B35; color:#000; font-family:%1; font-size:%2px;"
        "font-weight:700; border:none; padding:6px 20px; border-radius:2px;"
        "letter-spacing:1px; }"
        "QPushButton:hover { background:#E55A2B; }"
        "QPushButton:disabled { background:#444; color:#888; }")
        .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    connect(run_button_, &QPushButton::clicked, this, &BacktestingScreen::on_run);
    hl->addWidget(run_button_);

    // Status dot
    auto* ready = new QLabel("READY", bar);
    ready->setObjectName("bt_status_dot");
    ready->setStyleSheet(QString(
        "color:%1; font-size:9px; font-weight:700; font-family:%2;"
        "padding:3px 8px; background:rgba(22,163,74,0.08);"
        "border:1px solid rgba(22,163,74,0.25); border-radius:2px;")
        .arg(ui::colors::POSITIVE).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(ready);

    return bar;
}

// ── Left Panel: Commands + Strategy Selector ─────────────────────────────────
QWidget* BacktestingScreen::build_left_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(240);
    panel->setStyleSheet(QString("background:%1; border-right:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto label_style = QString("color:%1; font-size:8px; font-weight:700;"
        "font-family:%2; letter-spacing:1px;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY);

    // Commands section
    auto* cmd_title = new QLabel("COMMANDS", panel);
    cmd_title->setStyleSheet(QString("color:#FF6B35; font-size:9px; font-weight:700;"
        "font-family:%1; letter-spacing:1px; padding-bottom:4px;"
        "border-bottom:1px solid %2;")
        .arg(ui::fonts::DATA_FAMILY).arg(ui::colors::BORDER_DIM));
    vl->addWidget(cmd_title);

    auto cmds = all_commands();
    for (int i = 0; i < cmds.size(); ++i) {
        const auto& cmd = cmds[i];
        auto* btn = new QPushButton(cmd.label, panel);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { text-align:left; padding:6px 10px; border:none;"
            "border-left:2px solid transparent; color:%1;"
            "font-size:10px; font-family:%2; background:transparent; }"
            "QPushButton:hover { background:rgba(%3,0.08); color:%4; }")
            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
            .arg(QString("%1,%2,%3").arg(cmd.color.red()).arg(cmd.color.green()).arg(cmd.color.blue()))
            .arg(cmd.color.name()));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_command_changed(i); });
        vl->addWidget(btn);
        command_buttons_.append(btn);
    }

    // Strategies section
    vl->addSpacing(8);
    auto* strat_title = new QLabel("STRATEGY", panel);
    strat_title->setStyleSheet(cmd_title->styleSheet());
    vl->addWidget(strat_title);

    auto combo_style = QString(
        "QComboBox { background:%1; color:%2; border:1px solid %3;"
        "font-family:%4; font-size:%5px; padding:4px 6px; }")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
        .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto* cat_lbl = new QLabel("CATEGORY", panel);
    cat_lbl->setStyleSheet(label_style);
    vl->addWidget(cat_lbl);
    strategy_category_combo_ = new QComboBox(panel);
    strategy_category_combo_->setStyleSheet(combo_style);
    for (const auto& cat : strategy_categories())
        strategy_category_combo_->addItem(cat);
    connect(strategy_category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { populate_strategies(); });
    vl->addWidget(strategy_category_combo_);

    auto* strat_lbl = new QLabel("STRATEGY", panel);
    strat_lbl->setStyleSheet(label_style);
    vl->addWidget(strat_lbl);
    strategy_combo_ = new QComboBox(panel);
    strategy_combo_->setStyleSheet(combo_style);
    vl->addWidget(strategy_combo_);

    vl->addStretch();
    return panel;
}

// ── Center: Results Display ──────────────────────────────────────────────────
QWidget* BacktestingScreen::build_center_panel() {
    auto* panel = new QWidget(this);
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("RESULTS", header);
    title->setStyleSheet(QString(
        "color:#FF6B35; font-size:%1px; font-weight:700; font-family:%2; letter-spacing:1px;")
        .arg(ui::fonts::TINY).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();
    vl->addWidget(header);

    // Results scroll area
    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    results_container_ = new QWidget(scroll);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(16, 16, 16, 16);
    results_layout_->setSpacing(12);

    // Initial hint
    auto* hint = new QLabel(
        "Select a provider, command, and strategy, then click RUN to execute.\n\n"
        "Supported providers: VectorBT, Backtesting.py, FastTrade, Zipline, BT, Fincept\n"
        "Commands: Backtest, Optimize, Walk-Forward, Indicators, ML Labels, CV Splits, Returns Analysis");
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; line-height:1.6;"
        "padding:20px; background:%4; border:1px solid %5; border-radius:2px;")
        .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY)
        .arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
    results_layout_->addWidget(hint);
    results_layout_->addStretch();

    scroll->setWidget(results_container_);
    vl->addWidget(scroll, 1);

    return panel;
}

// ── Right Panel: Config ──────────────────────────────────────────────────────
QWidget* BacktestingScreen::build_right_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(300);
    panel->setStyleSheet(QString("background:%1; border-left:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget(scroll);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto label_style = QString("color:%1; font-size:8px; font-weight:700;"
        "font-family:%2; letter-spacing:1px;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY);
    auto input_style = QString(
        "QLineEdit, QDoubleSpinBox, QSpinBox, QDateEdit { background:%1; color:%2;"
        "border:1px solid %3; font-family:%4; font-size:%5px; padding:4px 6px;"
        "border-radius:2px; }"
        "QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QDateEdit:focus"
        "{ border-color:#FF6B35; }")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
        .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    // ── MARKET DATA ──
    auto* mkt_title = new QLabel("MARKET DATA", content);
    mkt_title->setStyleSheet(QString("color:#FF6B35; font-size:9px; font-weight:700;"
        "font-family:%1; letter-spacing:1px; padding-bottom:4px;"
        "border-bottom:1px solid %2;")
        .arg(ui::fonts::DATA_FAMILY).arg(ui::colors::BORDER_DIM));
    vl->addWidget(mkt_title);

    auto* sym_lbl = new QLabel("SYMBOLS", content);
    sym_lbl->setStyleSheet(label_style);
    vl->addWidget(sym_lbl);
    symbols_edit_ = new QLineEdit("SPY", content);
    symbols_edit_->setPlaceholderText("SPY,AAPL,MSFT");
    symbols_edit_->setStyleSheet(input_style);
    vl->addWidget(symbols_edit_);

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
    exec_title->setStyleSheet(mkt_title->styleSheet());
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
    adv_title->setStyleSheet(mkt_title->styleSheet());
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

    auto combo_style = QString(
        "QComboBox { background:%1; color:%2; border:1px solid %3;"
        "font-family:%4; font-size:%5px; padding:4px 6px; }")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
        .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto* ps_lbl = new QLabel("POSITION SIZING", content);
    ps_lbl->setStyleSheet(label_style);
    vl->addWidget(ps_lbl);
    pos_sizing_combo_ = new QComboBox(content);
    pos_sizing_combo_->setStyleSheet(combo_style);
    for (const auto& m : position_sizing_methods())
        pos_sizing_combo_->addItem(m);
    vl->addWidget(pos_sizing_combo_);

    auto* bm_lbl = new QLabel("BENCHMARK", content);
    bm_lbl->setStyleSheet(label_style);
    vl->addWidget(bm_lbl);
    benchmark_edit_ = new QLineEdit("SPY", content);
    benchmark_edit_->setStyleSheet(input_style);
    vl->addWidget(benchmark_edit_);

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
    bar->setFixedHeight(24);
    bar->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);
    auto s = QString("color:%1; font-size:8px; font-family:%2;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY);
    auto* l1 = new QLabel("PROVIDERS:", bar); l1->setStyleSheet(s);
    auto* v1 = new QLabel("6", bar);
    v1->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(l1); hl->addWidget(v1);
    auto* l2 = new QLabel("STRATEGIES:", bar); l2->setStyleSheet(s);
    auto* v2 = new QLabel("50+", bar); v2->setStyleSheet(v1->styleSheet());
    hl->addWidget(l2); hl->addWidget(v2);
    hl->addStretch();
    status_label_ = new QLabel("READY", bar);
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::POSITIVE).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(status_label_);
    return bar;
}

// ── Provider/Command/Strategy selection ──────────────────────────────────────
void BacktestingScreen::on_provider_changed(int index) {
    if (index < 0 || index >= providers_.size()) return;
    active_provider_ = index;
    update_provider_buttons();
    populate_strategies();

    // Enable/disable command buttons based on provider support
    auto cmds = all_commands();
    const auto& supported = providers_[index].commands;
    for (int i = 0; i < command_buttons_.size() && i < cmds.size(); ++i)
        command_buttons_[i]->setEnabled(supported.contains(cmds[i].id));
}

void BacktestingScreen::update_provider_buttons() {
    for (int i = 0; i < provider_buttons_.size(); ++i) {
        const auto& p = providers_[i];
        bool active = (i == active_provider_);
        provider_buttons_[i]->setStyleSheet(QString(
            "QPushButton { color:%1; font-size:9px; font-family:%2;"
            "padding:4px 10px; border:1px solid %3; border-radius:2px;"
            "background:%4; font-weight:%5; }"
            "QPushButton:hover { background:rgba(%6,0.1); }")
            .arg(active ? p.color.name() : ui::colors::TEXT_TERTIARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(active ? QString("rgba(%1,%2,%3,0.3)")
                .arg(p.color.red()).arg(p.color.green()).arg(p.color.blue()) : "transparent")
            .arg(active ? QString("rgba(%1,%2,%3,0.12)")
                .arg(p.color.red()).arg(p.color.green()).arg(p.color.blue()) : "transparent")
            .arg(active ? "700" : "400")
            .arg(QString("%1,%2,%3").arg(p.color.red()).arg(p.color.green()).arg(p.color.blue())));
    }
}

void BacktestingScreen::on_command_changed(int index) {
    auto cmds = all_commands();
    if (index < 0 || index >= cmds.size()) return;
    active_command_ = index;

    for (int i = 0; i < command_buttons_.size(); ++i) {
        const auto& cmd = cmds[i];
        bool active = (i == index);
        command_buttons_[i]->setStyleSheet(QString(
            "QPushButton { text-align:left; padding:6px 10px; border:none;"
            "border-left:2px solid %1; color:%2;"
            "font-size:10px; font-family:%3; font-weight:%4; background:%5; }"
            "QPushButton:hover { background:rgba(%6,0.1); color:%7; }"
            "QPushButton:disabled { color:%8; }")
            .arg(active ? cmd.color.name() : "transparent")
            .arg(active ? cmd.color.name() : ui::colors::TEXT_SECONDARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(active ? "700" : "400")
            .arg(active ? QString("rgba(%1,%2,%3,0.08)")
                .arg(cmd.color.red()).arg(cmd.color.green()).arg(cmd.color.blue()) : "transparent")
            .arg(QString("%1,%2,%3").arg(cmd.color.red()).arg(cmd.color.green()).arg(cmd.color.blue()))
            .arg(cmd.color.name())
            .arg(ui::colors::TEXT_DIM));
    }
}

void BacktestingScreen::on_strategy_changed(int) {
    // Strategy changed — no immediate action needed
}

void BacktestingScreen::populate_strategies() {
    strategy_combo_->clear();
    auto cat = strategy_category_combo_->currentText();
    for (const auto& s : strategies_) {
        if (s.category == cat)
            strategy_combo_->addItem(s.name, s.id);
    }
}

// ── Gather args ──────────────────────────────────────────────────────────────
QJsonObject BacktestingScreen::gather_args() {
    QJsonObject args;

    // Symbols
    auto sym_text = symbols_edit_->text().trimmed();
    QJsonArray symbols;
    for (const auto& s : sym_text.split(',', Qt::SkipEmptyParts))
        symbols.append(s.trimmed());
    args["symbols"] = symbols;

    args["startDate"] = start_date_->date().toString("yyyy-MM-dd");
    args["endDate"] = end_date_->date().toString("yyyy-MM-dd");
    args["initialCapital"] = capital_spin_->value();
    args["commission"] = commission_spin_->value() / 100.0;
    args["slippage"] = slippage_spin_->value() / 100.0;
    args["leverage"] = leverage_spin_->value();

    if (stop_loss_spin_->value() > 0)
        args["stopLoss"] = stop_loss_spin_->value() / 100.0;
    if (take_profit_spin_->value() > 0)
        args["takeProfit"] = take_profit_spin_->value() / 100.0;

    args["positionSizing"] = pos_sizing_combo_->currentText();
    args["benchmarkSymbol"] = benchmark_edit_->text().trimmed();

    // Strategy
    QJsonObject strategy;
    strategy["name"] = strategy_combo_->currentData().toString();
    strategy["type"] = strategy_category_combo_->currentText();
    args["strategy"] = strategy;

    return args;
}

// ── Run ──────────────────────────────────────────────────────────────────────
void BacktestingScreen::on_run() {
    if (is_running_) return;
    is_running_ = true;
    run_button_->setEnabled(false);
    status_label_->setText("EXECUTING...");
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::WARNING).arg(ui::fonts::DATA_FAMILY));

    auto provider = providers_[active_provider_].slug;
    auto cmds = all_commands();
    auto command = cmds[active_command_].id;
    auto args = gather_args();

    BacktestingService::instance().execute(provider, command, args);
}

// ── Result display ───────────────────────────────────────────────────────────
static QString fmt_val(const QJsonValue& val) {
    if (val.isDouble()) {
        double v = val.toDouble();
        if (std::abs(v) >= 1e9)  return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
        if (std::abs(v) >= 1e6)  return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
        if (std::abs(v) >= 1e3)  return QString("$%1K").arg(v / 1e3, 0, 'f', 0);
        if (std::abs(v) < 1.0 && std::abs(v) > 0.0001)
            return QString("%1%").arg(v * 100, 0, 'f', 2);
        return QString::number(v, 'f', 4);
    }
    if (val.isBool()) return val.toBool() ? "YES" : "NO";
    if (val.isString()) return val.toString();
    return QString::fromUtf8("\u2014");
}

void BacktestingScreen::display_result(const QJsonObject& data) {
    // Clear
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto accent = providers_[active_provider_].color.name();

    // Header
    auto* header = new QLabel("BACKTEST RESULTS");
    header->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
        "letter-spacing:1px;").arg(accent).arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // Performance metrics cards (if nested in "performance" or "data.performance")
    auto perf = data.contains("performance") ? data["performance"].toObject()
              : data.contains("data") ? data["data"].toObject()["performance"].toObject()
              : data;

    // Key metric cards
    auto* cards = new QWidget;
    auto* gl = new QGridLayout(cards);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(8);

    QStringList key_metrics = {"total_return", "sharpe_ratio", "max_drawdown",
                                "win_rate", "profit_factor", "total_trades",
                                "annualized_return", "sortino_ratio", "calmar_ratio",
                                "volatility", "alpha", "beta"};

    int col = 0, row = 0;
    for (const auto& key : key_metrics) {
        if (!perf.contains(key)) continue;
        auto label = key;
        label.replace('_', ' ');
        auto* card = new QWidget(cards);
        card->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:2px;")
            .arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(10, 8, 10, 8);
        cvl->setSpacing(2);
        auto* lbl = new QLabel(label.toUpper(), card);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2;")
            .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
        auto* val = new QLabel(fmt_val(perf[key]), card);
        val->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700; font-family:%2;")
            .arg(accent).arg(ui::fonts::DATA_FAMILY));
        cvl->addWidget(lbl);
        cvl->addWidget(val);
        gl->addWidget(card, row, col);
        if (++col >= 3) { col = 0; row++; }
    }
    results_layout_->addWidget(cards);

    // Shared table stylesheet (used for both metrics and trades tables)
    const QString table_style = QString(
        "QTableWidget { background:%1; color:%2; gridline-color:%3;"
        "font-family:%4; font-size:%5px; border:1px solid %3; }"
        "QTableWidget::item { padding:3px 8px; }"
        "QHeaderView::section { background:%6; color:%7; font-weight:700;"
        "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
        "QTableWidget::item:alternate { background:%8; }")
        .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
        .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL)
        .arg(ui::colors::BG_RAISED).arg(ui::colors::TEXT_SECONDARY)
        .arg(ui::colors::ROW_ALT);

    // Full metrics table
    QStringList all_keys;
    for (auto it = perf.begin(); it != perf.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            all_keys.append(it.key());

    if (!all_keys.isEmpty()) {
        auto* metrics_title = new QLabel("ALL METRICS");
        metrics_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;"
            "font-family:%2; letter-spacing:1px; padding-top:8px;")
            .arg(accent).arg(ui::fonts::DATA_FAMILY));
        results_layout_->addWidget(metrics_title);

        auto* table = new QTableWidget(all_keys.size(), 2);
        table->setHorizontalHeaderLabels({"Metric", "Value"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setMaximumHeight(qMin(all_keys.size() * 28 + 30, 500));
        table->setStyleSheet(table_style);

        for (int r = 0; r < all_keys.size(); ++r) {
            auto label = all_keys[r]; label.replace('_', ' ');
            table->setItem(r, 0, new QTableWidgetItem(label.toUpper()));
            auto* vi = new QTableWidgetItem(fmt_val(perf[all_keys[r]]));
            vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, 1, vi);
        }
        table->resizeColumnsToContents();
        results_layout_->addWidget(table);
    }

    // Trades table (if present)
    auto trades = data.contains("trades") ? data["trades"].toArray()
                : data.contains("data") ? data["data"].toObject()["trades"].toArray()
                : QJsonArray();

    if (!trades.isEmpty()) {
        auto* trades_title = new QLabel(QString("TRADES (%1)").arg(trades.size()));
        trades_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;"
            "font-family:%2; letter-spacing:1px; padding-top:8px;")
            .arg(accent).arg(ui::fonts::DATA_FAMILY));
        results_layout_->addWidget(trades_title);

        // Build table from first trade's keys
        if (trades[0].isObject()) {
            auto first = trades[0].toObject();
            QStringList cols;
            for (auto it = first.begin(); it != first.end(); ++it)
                if (!it.value().isObject() && !it.value().isArray())
                    cols.append(it.key());

            int show = qMin(trades.size(), 50);
            auto* tt = new QTableWidget(show, cols.size());
            tt->setHorizontalHeaderLabels(cols);
            tt->setEditTriggers(QAbstractItemView::NoEditTriggers);
            tt->setAlternatingRowColors(true);
            tt->horizontalHeader()->setStretchLastSection(true);
            tt->verticalHeader()->setVisible(false);
            tt->setMaximumHeight(400);
            tt->setStyleSheet(table_style);

            for (int r = 0; r < show; ++r) {
                auto trade = trades[r].toObject();
                for (int c = 0; c < cols.size(); ++c) {
                    auto* item = new QTableWidgetItem(fmt_val(trade[cols[c]]));
                    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    tt->setItem(r, c, item);
                }
            }
            tt->resizeColumnsToContents();
            results_layout_->addWidget(tt);
        }
    }

    // Raw JSON
    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(200);
    raw->setPlainText(QJsonDocument(data).toJson(QJsonDocument::Indented));
    raw->setStyleSheet(QString(
        "QTextEdit { background:%1; color:%2; border:1px solid %3;"
        "font-family:%4; font-size:%5px; padding:8px; }")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
        .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    results_layout_->addWidget(raw);
}

void BacktestingScreen::display_error(const QString& msg) {
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    auto* err = new QLabel(msg);
    err->setWordWrap(true);
    err->setStyleSheet(QString(
        "color:%1; font-size:%2px; font-family:%3; padding:12px;"
        "background:rgba(220,38,38,0.08); border:1px solid rgba(220,38,38,0.3);"
        "border-radius:2px;")
        .arg(ui::colors::NEGATIVE).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(err);
}

// ── Signal handlers ──────────────────────────────────────────────────────────
void BacktestingScreen::on_result(const QString& provider, const QString& command,
                                   const QJsonObject& data) {
    is_running_ = false;
    run_button_->setEnabled(true);
    status_label_->setText("READY");
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::POSITIVE).arg(ui::fonts::DATA_FAMILY));
    display_result(data);
    LOG_INFO("Backtesting", QString("[%1/%2] Complete").arg(provider, command));
}

void BacktestingScreen::on_error(const QString& context, const QString& message) {
    is_running_ = false;
    run_button_->setEnabled(true);
    status_label_->setText("ERROR");
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
        .arg(ui::colors::NEGATIVE).arg(ui::fonts::DATA_FAMILY));
    display_error(QString("[%1] %2").arg(context, message));
}

} // namespace fincept::screens
