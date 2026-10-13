// src/screens/backtesting/BacktestingScreen_Layout.cpp
//
// UI construction for the BacktestingScreen — top bar, left panel (commands
// + strategy selector), center panel (tabbed results), right panel (config),
// and status bar. Pure layout: no signal handling or business logic.
//
// Part of the partial-class split of BacktestingScreen.cpp; helpers shared
// across split files live in BacktestingScreen_internal.h.

#include "core/logging/Logger.h"
#include "screens/backtesting/BacktestingScreen.h"
#include "screens/backtesting/BacktestingScreen_internal.h"
#include "services/backtesting/BacktestingService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>

namespace fincept::screens {

using namespace fincept::services::backtest;
using fincept::screens::backtesting_internal::apply_pill_geometry;

QWidget* BacktestingScreen::build_top_bar() {
    // Styling for everything built here lives in refresh_theme() (objectName /
    // dynamic-property selectors on the screen's root stylesheet).
    auto* bar = new QWidget(this);
    bar->setObjectName("btTopBar");
    bar->setFixedHeight(34);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    // Brand chip
    brand_label_ = new QLabel(tr("BACKTESTING"), bar);
    brand_label_->setObjectName("btBrand");
    brand_label_->setAlignment(Qt::AlignCenter);
    apply_pill_geometry(brand_label_);
    hl->addWidget(brand_label_);

    auto* div = new QWidget(bar);
    div->setObjectName("btDivider");
    div->setFixedSize(1, 18);
    hl->addWidget(div);

    // Provider tabs — colour comes from per-slug rules keyed on the "active" property
    // (see refresh_theme / update_provider_buttons).
    for (int i = 0; i < providers_.size(); ++i) {
        const auto& p = providers_[i];
        auto* btn = new QPushButton(p.display_name, bar);
        btn->setObjectName("btProviderTab");
        btn->setProperty("slug", p.slug);
        btn->setProperty("active", false);
        btn->setCursor(Qt::PointingHandCursor);
        apply_pill_geometry(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_provider_changed(i); });
        hl->addWidget(btn);
        provider_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Run button
    run_button_ = new QPushButton(tr("RUN"), bar);
    run_button_->setObjectName("btRunButton");
    run_button_->setCursor(Qt::PointingHandCursor);
    run_button_->setToolTip(tr("Run the selected command (Ctrl+Enter)"));
    apply_pill_geometry(run_button_);
    connect(run_button_, &QPushButton::clicked, this, &BacktestingScreen::on_run);
    hl->addWidget(run_button_);

    // Cancel button — only visible while a run is in flight.
    cancel_button_ = new QPushButton(tr("CANCEL"), bar);
    cancel_button_->setObjectName("btCancelButton");
    cancel_button_->setCursor(Qt::PointingHandCursor);
    cancel_button_->setToolTip(tr("Cancel the running command (Esc)"));
    apply_pill_geometry(cancel_button_);
    cancel_button_->setVisible(false);
    connect(cancel_button_, &QPushButton::clicked, this, &BacktestingScreen::on_cancel);
    hl->addWidget(cancel_button_);

    // Status chip
    status_dot_ = new QLabel(tr("READY"), bar);
    status_dot_->setObjectName("btStatusChip");
    status_dot_->setProperty("state", "ready");
    status_dot_->setAlignment(Qt::AlignCenter);
    apply_pill_geometry(status_dot_);
    hl->addWidget(status_dot_);

    return bar;
}

// ── Left Panel: Commands + Strategy Selector + Params ────────────────────────

QWidget* BacktestingScreen::build_left_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("btLeftPanel");
    panel->setMinimumWidth(180);
    panel->setMaximumWidth(320);

    auto* scroll = new QScrollArea(panel);
    scroll->setObjectName("btPanelScroll");
    scroll->setWidgetResizable(true);

    auto* content = new QWidget(scroll);
    content->setObjectName("btPanelContent");
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(4);

    // ── Commands section ──
    commands_title_ = new QLabel(tr("COMMANDS"), content);
    commands_title_->setObjectName("btSectionTitle");
    vl->addWidget(commands_title_);

    const auto& cmds = commands_;
    for (int i = 0; i < cmds.size(); ++i) {
        const auto& cmd = cmds[i];
        auto* btn = new QPushButton(cmd.label, content);
        btn->setObjectName("btCommandButton");
        btn->setProperty("cmd", cmd.id);
        btn->setProperty("active", false);
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

    strategy_title_ = new QLabel(tr("STRATEGY"), strategy_section_);
    strategy_title_->setObjectName("btSectionTitle");
    strat_outer->addWidget(strategy_title_);


    strategy_cat_label_ = new QLabel(tr("CATEGORY"), strategy_section_);
    strategy_cat_label_->setObjectName("btFieldLabel");
    strat_outer->addWidget(strategy_cat_label_);
    strategy_category_combo_ = new QComboBox(strategy_section_);
    connect(strategy_category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { populate_strategies(); });
    strat_outer->addWidget(strategy_category_combo_);

    strategy_pick_label_ = new QLabel(tr("STRATEGY"), strategy_section_);
    strategy_pick_label_->setObjectName("btFieldLabel");
    strat_outer->addWidget(strategy_pick_label_);
    strategy_combo_ = new QComboBox(strategy_section_);
    connect(strategy_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &BacktestingScreen::on_strategy_changed);
    strat_outer->addWidget(strategy_combo_);

    params_title_ = new QLabel(tr("PARAMETERS"), strategy_section_);
    params_title_->setObjectName("btFieldLabel");
    strat_outer->addWidget(params_title_);

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
    panel->setObjectName("btCenterPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(panel);
    header->setObjectName("btResultsHeader");
    header->setFixedHeight(34);
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    hhl->setSpacing(8);
    results_title_ = new QLabel(tr("RESULTS"), header);
    results_title_->setObjectName("btResultsTitle");
    hhl->addWidget(results_title_);
    hhl->addStretch();

    // CSV export: the DETAILS rows (trades / runs / windows) and the equity curve,
    // with full precision and every row (the views may be sorted or formatted).
    export_csv_btn_ = new QPushButton(tr("EXPORT CSV"), header);
    export_csv_btn_->setObjectName("btHeaderButton");
    export_csv_btn_->setCursor(Qt::PointingHandCursor);
    export_csv_btn_->setFixedHeight(22);
    export_csv_btn_->setToolTip(tr("Export the DETAILS rows or the equity curve as CSV"));
    auto* csv_menu = new QMenu(export_csv_btn_);
    QAction* csv_details = csv_menu->addAction(tr("Details / trades (CSV)…"));
    QAction* csv_equity = csv_menu->addAction(tr("Equity curve (CSV)…"));
    connect(csv_details, &QAction::triggered, this, &BacktestingScreen::export_details_csv);
    connect(csv_equity, &QAction::triggered, this, &BacktestingScreen::export_equity_csv);
    export_csv_btn_->setMenu(csv_menu);
    hhl->addWidget(export_csv_btn_);

    export_json_btn_ = new QPushButton(tr("EXPORT JSON"), header);
    export_json_btn_->setObjectName("btHeaderButton");
    export_json_btn_->setCursor(Qt::PointingHandCursor);
    export_json_btn_->setFixedHeight(22);
    connect(export_json_btn_, &QPushButton::clicked, this, [this]() {
        // Export the FULL payload: the RAW JSON tab may show a capped preview.
        QString text;
        if (!last_payload_.isEmpty())
            text = QString::fromUtf8(QJsonDocument(last_payload_).toJson(QJsonDocument::Indented));
        else if (raw_json_edit_)
            text = raw_json_edit_->toPlainText();
        if (text.trimmed().isEmpty()) {
            QMessageBox::information(this, tr("Export Backtest Results"), tr("There is no result to export yet."));
            return;
        }
        const bool synthetic = last_payload_.value("usingSyntheticData").toBool();
        const QString suggested =
            synthetic ? QStringLiteral("backtest_results_SYNTHETIC.json") : QStringLiteral("backtest_results.json");
        const QString path = QFileDialog::getSaveFileName(this, tr("Export Backtest Results"), suggested,
                                                          tr("JSON Files (*.json);;All Files (*)"));
        if (path.isEmpty())
            return;
        write_text_file(path, text, tr("Export Backtest Results"));
    });
    hhl->addWidget(export_json_btn_);

    vl->addWidget(header);

    // Tab widget for results
    result_tabs_ = new QTabWidget(panel);
    result_tabs_->setObjectName("btResultTabs");

    // SUMMARY tab
    auto* summary_scroll = new QScrollArea;
    summary_scroll->setObjectName("btSummaryScroll");
    summary_scroll->setWidgetResizable(true);
    summary_container_ = new QWidget(this);
    summary_container_->setObjectName("btSummary");
    summary_layout_ = new QVBoxLayout(summary_container_);
    summary_layout_->setContentsMargins(16, 16, 16, 16);
    summary_layout_->setSpacing(12);

    summary_hint_ =
        new QLabel(tr("Select a provider, command, and strategy, then click RUN to execute.\n\n"
                      "Supported providers: VectorBT, Backtesting.py, FastTrade, Zipline, BT, Fincept\n"
                      "Commands: Backtest, Optimize, Walk-Forward, Indicators, ML Labels, CV Splits, Returns"));
    summary_hint_->setObjectName("btHint");
    summary_hint_->setWordWrap(true);
    summary_layout_->addWidget(summary_hint_);
    summary_layout_->addStretch();
    summary_scroll->setWidget(summary_container_);
    result_tabs_->addTab(summary_scroll, tr("SUMMARY"));

    // EQUITY CURVE tab
    equity_chart_tab_ = new QWidget(this);
    equity_chart_tab_->setObjectName("btEquityTab");
    auto* eq_layout = new QVBoxLayout(equity_chart_tab_);
    eq_layout->setContentsMargins(0, 0, 0, 0);
    equity_hint_ = new QLabel(tr("Run a backtest to see the equity curve."), equity_chart_tab_);
    equity_hint_->setObjectName("btEquityHint");
    equity_hint_->setAlignment(Qt::AlignCenter);
    eq_layout->addWidget(equity_hint_);
    result_tabs_->addTab(equity_chart_tab_, tr("EQUITY CURVE"));

    // METRICS tab
    metrics_table_ = new QTableWidget(0, 2);
    metrics_table_->setObjectName("btMetricsTable");
    metrics_table_->setHorizontalHeaderLabels({tr("Metric"), tr("Value")});
    metrics_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    metrics_table_->setAlternatingRowColors(true);
    metrics_table_->horizontalHeader()->setStretchLastSection(true);
    metrics_table_->verticalHeader()->setVisible(false);
    result_tabs_->addTab(metrics_table_, tr("METRICS"));

    // DETAILS tab — model/view. Cells are formatted lazily (only visible rows),
    // so a 100k-trade result renders instantly; the proxy sorts numerically on
    // LiveTableModel::SortRole.
    details_model_ = new ui::LiveTableModel<BtDetailRow>(this);
    details_model_->set_key([](const BtDetailRow& r) { return QString::number(r.index); });
    details_model_->set_cell([this](const BtDetailRow& r, int col, int role) -> QVariant {
        if (col < 0 || col >= details_cols_.size())
            return {};
        const QString& key = details_cols_.at(col);
        const QJsonValue v = r.rec.value(key);
        switch (role) {
            case Qt::DisplayRole:
                return backtesting_internal::fmt_metric(key, v);
            case Qt::TextAlignmentRole:
                return QVariant::fromValue(Qt::Alignment(
                    (v.isDouble() ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter));
            case ui::LiveTableModel<BtDetailRow>::SortRole:
                if (v.isDouble())
                    return v.toDouble();
                return v.toVariant().toString();
            default:
                return {};
        }
    });
    details_proxy_ = new QSortFilterProxyModel(this);
    details_proxy_->setSourceModel(details_model_);
    details_proxy_->setSortRole(ui::LiveTableModel<BtDetailRow>::SortRole);
    details_view_ = new QTableView;
    details_view_->setObjectName("btDetailsTable");
    details_view_->setModel(details_proxy_);
    details_view_->setSortingEnabled(true);
    details_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    details_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    details_view_->setAlternatingRowColors(true);
    details_view_->setWordWrap(false);
    details_view_->horizontalHeader()->setStretchLastSection(true);
    // Measure only the first rows when auto-sizing columns: measuring all of a
    // 100k-row model would stall the UI thread.
    details_view_->horizontalHeader()->setResizeContentsPrecision(200);
    details_view_->verticalHeader()->setVisible(false);
    details_view_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    details_view_->verticalHeader()->setDefaultSectionSize(22);
    result_tabs_->addTab(details_view_, tr("DETAILS"));

    // RAW JSON tab
    raw_json_edit_ = new QTextEdit;
    raw_json_edit_->setObjectName("btRawJson");
    raw_json_edit_->setReadOnly(true);
    result_tabs_->addTab(raw_json_edit_, tr("RAW JSON"));

    vl->addWidget(result_tabs_, 1);
    return panel;
}

// ── Right Panel: Config ──────────────────────────────────────────────────────

QWidget* BacktestingScreen::build_right_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("btRightPanel");
    panel->setMinimumWidth(200);
    panel->setMaximumWidth(380);

    auto* scroll = new QScrollArea(panel);
    scroll->setObjectName("btPanelScroll");
    scroll->setWidgetResizable(true);

    auto* content = new QWidget(scroll);
    content->setObjectName("btPanelContent");
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(4);

    // Each section is a named QWidget so update_section_visibility() can
    // toggle whole blocks at once depending on the active command.

    // ── MARKET DATA (always shown) ──
    market_data_section_ = new QWidget(content);
    auto* mkt_layout = new QVBoxLayout(market_data_section_);
    mkt_layout->setContentsMargins(0, 0, 0, 0);
    mkt_layout->setSpacing(4);

    market_data_title_ = new QLabel(tr("MARKET DATA"), market_data_section_);
    market_data_title_->setObjectName("btSectionTitle");
    mkt_layout->addWidget(market_data_title_);

    symbols_label_ = new QLabel(tr("SYMBOLS"), market_data_section_);
    symbols_label_->setObjectName("btFieldLabel");
    mkt_layout->addWidget(symbols_label_);
    symbols_edit_ = new QLineEdit("SPY", market_data_section_);
    symbols_edit_->setPlaceholderText(tr("SPY,AAPL,MSFT"));
    mkt_layout->addWidget(symbols_edit_);
    connect(symbols_edit_, &QLineEdit::editingFinished, this, [this]() { publish_first_symbol_to_group(); });

    auto* dates = new QGridLayout;
    dates->setSpacing(8);
    start_label_ = new QLabel(tr("START"), market_data_section_);
    start_label_->setObjectName("btFieldLabel");
    dates->addWidget(start_label_, 0, 0);
    start_date_ = new QDateEdit(QDate::currentDate().addYears(-1), market_data_section_);
    start_date_->setDisplayFormat("yyyy-MM-dd");
    start_date_->setCalendarPopup(true);
    dates->addWidget(start_date_, 1, 0);
    end_label_ = new QLabel(tr("END"), market_data_section_);
    end_label_->setObjectName("btFieldLabel");
    dates->addWidget(end_label_, 0, 1);
    end_date_ = new QDateEdit(QDate::currentDate().addDays(-1), market_data_section_);
    end_date_->setDisplayFormat("yyyy-MM-dd");
    end_date_->setCalendarPopup(true);
    dates->addWidget(end_date_, 1, 1);
    mkt_layout->addLayout(dates);

    interval_label_ = new QLabel(tr("INTERVAL"), market_data_section_);
    interval_label_->setObjectName("btFieldLabel");
    mkt_layout->addWidget(interval_label_);
    interval_combo_ = new QComboBox(market_data_section_);
    for (const auto& i : {"1d", "1wk", "1h", "4h", "15m", "5m", "1m"})
        interval_combo_->addItem(i);
    mkt_layout->addWidget(interval_combo_);

    // Providers refuse to run when real prices cannot be loaded; this opt-in lets a
    // user test strategy mechanics on random-walk prices. Off on every launch.
    allow_synthetic_check_ = new QCheckBox(tr("Allow synthetic data (testing only)"), market_data_section_);
    allow_synthetic_check_->setObjectName("btSyntheticCheck");
    allow_synthetic_check_->setToolTip(
        tr("If real market data cannot be loaded, run on randomly generated prices instead of failing.\n"
           "Results are then clearly marked SYNTHETIC and have no market meaning."));
    mkt_layout->addWidget(allow_synthetic_check_);

    vl->addWidget(market_data_section_);

    // ── EXECUTION (backtest-family only) ──
    // Holds the cost/cash-flow knobs: initial capital, commission, slippage.
    execution_section_ = new QWidget(content);
    auto* exec_layout = new QVBoxLayout(execution_section_);
    exec_layout->setContentsMargins(0, 8, 0, 0);
    exec_layout->setSpacing(4);

    execution_title_ = new QLabel(tr("EXECUTION"), execution_section_);
    execution_title_->setObjectName("btSectionTitle");
    exec_layout->addWidget(execution_title_);

    // Capital label is currency-bound via cur::bindLabel — the "%1" is the
    // currency symbol token. Wrapping the format string keeps it translatable.
    auto* cap_lbl = new QLabel(execution_section_);
    cur::bindLabel(cap_lbl, tr("INITIAL CAPITAL (%1)"));
    cap_lbl->setObjectName("btFieldLabel");
    exec_layout->addWidget(cap_lbl);
    capital_spin_ = new QDoubleSpinBox(execution_section_);
    capital_spin_->setRange(1000, 1e12);
    capital_spin_->setValue(100000);
    capital_spin_->setDecimals(0);
    exec_layout->addWidget(capital_spin_);

    commission_label_ = new QLabel(tr("COMMISSION (%)"), execution_section_);
    commission_label_->setObjectName("btFieldLabel");
    exec_layout->addWidget(commission_label_);
    commission_spin_ = new QDoubleSpinBox(execution_section_);
    commission_spin_->setRange(0, 10);
    commission_spin_->setValue(0.1);
    commission_spin_->setDecimals(3);
    commission_spin_->setSuffix("%");
    exec_layout->addWidget(commission_spin_);

    slippage_label_ = new QLabel(tr("SLIPPAGE (%)"), execution_section_);
    slippage_label_->setObjectName("btFieldLabel");
    exec_layout->addWidget(slippage_label_);
    slippage_spin_ = new QDoubleSpinBox(execution_section_);
    slippage_spin_->setRange(0, 10);
    slippage_spin_->setValue(0.05);
    slippage_spin_->setDecimals(4);
    slippage_spin_->setSuffix("%");
    exec_layout->addWidget(slippage_spin_);

    risk_free_label_ = new QLabel(tr("RISK-FREE RATE (%)"), execution_section_);
    risk_free_label_->setObjectName("btFieldLabel");
    exec_layout->addWidget(risk_free_label_);
    risk_free_spin_ = new QDoubleSpinBox(execution_section_);
    risk_free_spin_->setRange(0, 20);
    risk_free_spin_->setValue(4.0);
    risk_free_spin_->setDecimals(2);
    risk_free_spin_->setSingleStep(0.25);
    risk_free_spin_->setSuffix("%");
    exec_layout->addWidget(risk_free_spin_);

    vl->addWidget(execution_section_);

    // ── ADVANCED (backtest-family only) ──
    advanced_section_ = new QWidget(content);
    auto* adv_layout = new QVBoxLayout(advanced_section_);
    adv_layout->setContentsMargins(0, 8, 0, 0);
    adv_layout->setSpacing(4);

    advanced_title_ = new QLabel(tr("ADVANCED"), advanced_section_);
    advanced_title_->setObjectName("btSectionTitle");
    adv_layout->addWidget(advanced_title_);

    leverage_label_ = new QLabel(tr("LEVERAGE"), advanced_section_);
    leverage_label_->setObjectName("btFieldLabel");
    adv_layout->addWidget(leverage_label_);
    leverage_spin_ = new QDoubleSpinBox(advanced_section_);
    leverage_spin_->setRange(0.1, 10);
    leverage_spin_->setValue(1.0);
    leverage_spin_->setDecimals(1);
    leverage_spin_->setSuffix("x");
    adv_layout->addWidget(leverage_spin_);

    stop_loss_label_ = new QLabel(tr("STOP LOSS (%)"), advanced_section_);
    stop_loss_label_->setObjectName("btFieldLabel");
    adv_layout->addWidget(stop_loss_label_);
    stop_loss_spin_ = new QDoubleSpinBox(advanced_section_);
    stop_loss_spin_->setRange(0, 100);
    stop_loss_spin_->setValue(0);
    stop_loss_spin_->setDecimals(1);
    stop_loss_spin_->setSuffix("%");
    stop_loss_spin_->setSpecialValueText(tr("None"));
    adv_layout->addWidget(stop_loss_spin_);

    take_profit_label_ = new QLabel(tr("TAKE PROFIT (%)"), advanced_section_);
    take_profit_label_->setObjectName("btFieldLabel");
    adv_layout->addWidget(take_profit_label_);
    take_profit_spin_ = new QDoubleSpinBox(advanced_section_);
    take_profit_spin_->setRange(0, 1000);
    take_profit_spin_->setValue(0);
    take_profit_spin_->setDecimals(1);
    take_profit_spin_->setSuffix("%");
    take_profit_spin_->setSpecialValueText(tr("None"));
    adv_layout->addWidget(take_profit_spin_);

    pos_sizing_label_ = new QLabel(tr("POSITION SIZING"), advanced_section_);
    pos_sizing_label_->setObjectName("btFieldLabel");
    adv_layout->addWidget(pos_sizing_label_);
    pos_sizing_combo_ = new QComboBox(advanced_section_);
    for (const auto& m : position_sizing_methods())
        pos_sizing_combo_->addItem(m);
    adv_layout->addWidget(pos_sizing_combo_);

    pos_sizing_value_label_ = new QLabel(tr("SIZE (%)"), advanced_section_);
    pos_sizing_value_label_->setObjectName("btFieldLabel");
    adv_layout->addWidget(pos_sizing_value_label_);
    pos_sizing_value_spin_ = new QDoubleSpinBox(advanced_section_);
    pos_sizing_value_spin_->setRange(1, 100);
    pos_sizing_value_spin_->setValue(100);
    pos_sizing_value_spin_->setDecimals(2);
    pos_sizing_value_spin_->setSuffix("%");
    adv_layout->addWidget(pos_sizing_value_spin_);
    connect(pos_sizing_combo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (text == "percent") {
            pos_sizing_value_label_->setText(tr("SIZE (%)"));
            pos_sizing_value_spin_->setSuffix("%");
            pos_sizing_value_spin_->setRange(1, 100);
            pos_sizing_value_spin_->setValue(100);
        } else if (text == "fixed") {
            pos_sizing_value_label_->setText(tr("SIZE (%1)").arg(cur::symbol()));
            pos_sizing_value_spin_->setSuffix("");
            pos_sizing_value_spin_->setRange(1, 1e9);
            pos_sizing_value_spin_->setValue(10000);
        } else if (text == "risk") {
            pos_sizing_value_label_->setText(tr("RISK PER TRADE (%)"));
            pos_sizing_value_spin_->setSuffix("%");
            pos_sizing_value_spin_->setRange(0.1, 100);
            pos_sizing_value_spin_->setValue(1);
        } else {
            pos_sizing_value_label_->setText(tr("TARGET (%)"));
            pos_sizing_value_spin_->setSuffix("%");
            pos_sizing_value_spin_->setRange(1, 100);
            pos_sizing_value_spin_->setValue(15);
        }
    });

    allow_short_check_ = new QCheckBox(tr("Allow Short Selling"), advanced_section_);
    adv_layout->addWidget(allow_short_check_);

    vl->addWidget(advanced_section_);

    // ── BENCHMARK (backtest-family + returns) ──
    // Used as the comparison baseline for performance metrics and for the
    // benchmark-aware returns_stats analysis (alpha/beta/info-ratio).
    benchmark_section_ = new QWidget(content);
    auto* bm_layout = new QVBoxLayout(benchmark_section_);
    bm_layout->setContentsMargins(0, 8, 0, 0);
    bm_layout->setSpacing(4);

    benchmark_title_ = new QLabel(tr("BENCHMARK"), benchmark_section_);
    benchmark_title_->setObjectName("btSectionTitle");
    bm_layout->addWidget(benchmark_title_);

    benchmark_label_ = new QLabel(tr("SYMBOL"), benchmark_section_);
    benchmark_label_->setObjectName("btFieldLabel");
    bm_layout->addWidget(benchmark_label_);
    benchmark_edit_ = new QLineEdit("SPY", benchmark_section_);
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
        auto* t = new QLabel(tr("OPTIMIZATION"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* ol = new QLabel(tr("OBJECTIVE"), page);
        ol->setObjectName("btFieldLabel");
        pl->addWidget(ol);
        opt_objective_combo_ = new QComboBox(page);
        for (const auto& o : optimize_objectives())
            opt_objective_combo_->addItem(o);
        pl->addWidget(opt_objective_combo_);

        auto* ml = new QLabel(tr("METHOD"), page);
        ml->setObjectName("btFieldLabel");
        pl->addWidget(ml);
        opt_method_combo_ = new QComboBox(page);
        for (const auto& m : optimize_methods())
            opt_method_combo_->addItem(m);
        pl->addWidget(opt_method_combo_);

        auto* il = new QLabel(tr("MAX ITERATIONS"), page);
        il->setObjectName("btFieldLabel");
        pl->addWidget(il);
        opt_iterations_spin_ = new QSpinBox(page);
        opt_iterations_spin_->setRange(10, 10000);
        opt_iterations_spin_->setValue(100);
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
        auto* t = new QLabel(tr("WALK-FORWARD"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* sl = new QLabel(tr("NUMBER OF SPLITS"), page);
        sl->setObjectName("btFieldLabel");
        pl->addWidget(sl);
        wf_splits_spin_ = new QSpinBox(page);
        wf_splits_spin_->setRange(2, 20);
        wf_splits_spin_->setValue(5);
        pl->addWidget(wf_splits_spin_);

        auto* tl = new QLabel(tr("TRAIN RATIO"), page);
        tl->setObjectName("btFieldLabel");
        pl->addWidget(tl);
        wf_train_ratio_spin_ = new QDoubleSpinBox(page);
        wf_train_ratio_spin_->setRange(0.5, 0.95);
        wf_train_ratio_spin_->setValue(0.7);
        wf_train_ratio_spin_->setDecimals(2);
        wf_train_ratio_spin_->setSingleStep(0.05);
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
        auto* t = new QLabel(tr("INDICATOR"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* il = new QLabel(tr("INDICATOR TYPE"), page);
        il->setObjectName("btFieldLabel");
        pl->addWidget(il);
        indicator_combo_ = new QComboBox(page);
        // Populated dynamically via on_result("get_indicators")
        pl->addWidget(indicator_combo_);

        auto* pl_period = new QLabel(tr("PERIOD"), page);
        pl_period->setObjectName("btFieldLabel");
        pl->addWidget(pl_period);
        indicator_period_spin_ = new QSpinBox(page);
        indicator_period_spin_->setRange(2, 500);
        indicator_period_spin_->setValue(14);
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
        auto* t = new QLabel(tr("INDICATOR SIGNALS"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* isl = new QLabel(tr("INDICATOR"), page);
        isl->setObjectName("btFieldLabel");
        pl->addWidget(isl);
        ind_signal_indicator_combo_ = new QComboBox(page);
        // Populated dynamically via on_result("get_indicators")
        pl->addWidget(ind_signal_indicator_combo_);

        auto* ml = new QLabel(tr("SIGNAL MODE"), page);
        ml->setObjectName("btFieldLabel");
        pl->addWidget(ml);
        ind_signal_mode_combo_ = new QComboBox(page);
        for (const auto& m : indicator_signal_modes())
            ind_signal_mode_combo_->addItem(m);
        pl->addWidget(ind_signal_mode_combo_);

        // Helpers
        auto make_int_row = [&](const QString& text, QSpinBox*& spin, int mn, int mx, int dv) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            auto* lab = new QLabel(text, row);
            lab->setObjectName("btFieldLabel");
            rl->addWidget(lab);
            spin = new QSpinBox(row);
            spin->setRange(mn, mx);
            spin->setValue(dv);
            rl->addWidget(spin);
            return row;
        };
        auto make_dbl_row = [&](const QString& text, QDoubleSpinBox*& spin, double mn, double mx, double dv,
                                int decimals = 2) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            auto* lab = new QLabel(text, row);
            lab->setObjectName("btFieldLabel");
            rl->addWidget(lab);
            spin = new QDoubleSpinBox(row);
            spin->setRange(mn, mx);
            spin->setValue(dv);
            spin->setDecimals(decimals);
            rl->addWidget(spin);
            return row;
        };
        auto make_combo_row = [&](const QString& text, QComboBox*& combo, const QStringList& items) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            auto* lab = new QLabel(text, row);
            lab->setObjectName("btFieldLabel");
            rl->addWidget(lab);
            combo = new QComboBox(row);
            for (const auto& it : items)
                combo->addItem(it);
            rl->addWidget(combo);
            return row;
        };

        // ── crossover_signals: fast period + slow period + ma type ──
        // Combo items ("ma"/"ema" etc.) are engine keys — not translated.
        is_fast_period_row_ = make_int_row(tr("FAST PERIOD"), is_fast_period_spin_, 2, 200, 10);
        is_slow_period_row_ = make_int_row(tr("SLOW PERIOD"), is_slow_period_spin_, 2, 500, 20);
        is_ma_type_row_ = make_combo_row(tr("MA TYPE"), is_ma_type_combo_, {"ma", "ema"});
        pl->addWidget(is_fast_period_row_);
        pl->addWidget(is_slow_period_row_);
        pl->addWidget(is_ma_type_row_);

        // ── threshold_signals: period + lower + upper ──
        is_period_row_ = make_int_row(tr("PERIOD"), is_period_spin_, 2, 200, 14);
        is_lower_row_ = make_dbl_row(tr("LOWER THRESHOLD"), is_lower_spin_, -200, 200, 30);
        is_upper_row_ = make_dbl_row(tr("UPPER THRESHOLD"), is_upper_spin_, -200, 200, 70);
        pl->addWidget(is_period_row_);
        pl->addWidget(is_lower_row_);
        pl->addWidget(is_upper_row_);

        // ── breakout_signals: channel + period (shares is_period) ──
        is_channel_row_ = make_combo_row(tr("CHANNEL"), is_channel_combo_, {"donchian", "bbands", "keltner"});
        pl->addWidget(is_channel_row_);

        // ── mean_reversion_signals: period + z_entry + z_exit ──
        is_z_entry_row_ = make_dbl_row(tr("Z ENTRY (sigma)"), is_z_entry_spin_, 0.1, 5.0, 2.0, 2);
        is_z_exit_row_ = make_dbl_row(tr("Z EXIT (sigma)"), is_z_exit_spin_, 0.0, 5.0, 0.0, 2);
        pl->addWidget(is_z_entry_row_);
        pl->addWidget(is_z_exit_row_);

        // ── signal_filter: filter_indicator + filter_period + filter_threshold + filter_type ──
        is_filter_indicator_row_ =
            make_combo_row(tr("FILTER INDICATOR"), is_filter_indicator_combo_, {"adx", "atr", "mstd", "zscore", "rsi"});
        is_filter_period_row_ = make_int_row(tr("FILTER PERIOD"), is_filter_period_spin_, 2, 200, 14);
        is_filter_threshold_row_ = make_dbl_row(tr("FILTER THRESHOLD"), is_filter_threshold_spin_, -100, 100, 25, 3);
        is_filter_type_row_ = make_combo_row(tr("FILTER TYPE"), is_filter_type_combo_, {"above", "below"});
        pl->addWidget(is_filter_indicator_row_);
        pl->addWidget(is_filter_period_row_);
        pl->addWidget(is_filter_threshold_row_);
        pl->addWidget(is_filter_type_row_);

        auto update_is_rows = [this]() {
            const QString m = ind_signal_mode_combo_->currentText();
            const bool is_cross = (m == "crossover_signals" || m == "crossover");
            const bool is_thresh = (m == "threshold_signals" || m == "threshold");
            const bool is_break = (m == "breakout_signals" || m == "breakout");
            const bool is_mr = (m == "mean_reversion_signals" || m == "mean_reversion");
            const bool is_filt = (m == "signal_filter" || m == "filter");
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
        auto* t = new QLabel(tr("ML LABELS"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* tl = new QLabel(tr("LABEL TYPE"), page);
        tl->setObjectName("btFieldLabel");
        pl->addWidget(tl);
        labels_type_combo_ = new QComboBox(page);
        for (const auto& lt : label_types())
            labels_type_combo_->addItem(lt);
        pl->addWidget(labels_type_combo_);

        // ── HORIZON row (FIXLB only) ──
        labels_horizon_row_ = new QWidget(page);
        auto* hr = new QVBoxLayout(labels_horizon_row_);
        hr->setContentsMargins(0, 0, 0, 0);
        hr->setSpacing(4);
        auto* hl = new QLabel(tr("HORIZON (bars)"), labels_horizon_row_);
        hl->setObjectName("btFieldLabel");
        hr->addWidget(hl);
        labels_horizon_spin_ = new QSpinBox(labels_horizon_row_);
        labels_horizon_spin_->setRange(1, 100);
        labels_horizon_spin_->setValue(5);
        hr->addWidget(labels_horizon_spin_);
        pl->addWidget(labels_horizon_row_);

        // ── WINDOW row (MEANLB / LEXLB / TRENDLB / BOLB) ──
        labels_window_row_ = new QWidget(page);
        auto* wr = new QVBoxLayout(labels_window_row_);
        wr->setContentsMargins(0, 0, 0, 0);
        wr->setSpacing(4);
        auto* wl = new QLabel(tr("WINDOW"), labels_window_row_);
        wl->setObjectName("btFieldLabel");
        wr->addWidget(wl);
        labels_window_spin_ = new QSpinBox(labels_window_row_);
        labels_window_spin_->setRange(2, 500);
        labels_window_spin_->setValue(20);
        wr->addWidget(labels_window_spin_);
        pl->addWidget(labels_window_row_);

        // ── THRESHOLD row (FIXLB / MEANLB / TRENDLB) ──
        labels_threshold_row_ = new QWidget(page);
        auto* trow = new QVBoxLayout(labels_threshold_row_);
        trow->setContentsMargins(0, 0, 0, 0);
        trow->setSpacing(4);
        auto* thl = new QLabel(tr("THRESHOLD"), labels_threshold_row_);
        thl->setObjectName("btFieldLabel");
        trow->addWidget(thl);
        labels_threshold_spin_ = new QDoubleSpinBox(labels_threshold_row_);
        labels_threshold_spin_->setRange(0.0, 5.0);
        labels_threshold_spin_->setValue(0.02);
        labels_threshold_spin_->setDecimals(3);
        labels_threshold_spin_->setSingleStep(0.005);
        trow->addWidget(labels_threshold_spin_);
        pl->addWidget(labels_threshold_row_);

        // ── ALPHA row (BOLB only — Bollinger Band sigma multiplier) ──
        labels_alpha_row_ = new QWidget(page);
        auto* ar = new QVBoxLayout(labels_alpha_row_);
        ar->setContentsMargins(0, 0, 0, 0);
        ar->setSpacing(4);
        auto* al = new QLabel(tr("ALPHA (sigma)"), labels_alpha_row_);
        al->setObjectName("btFieldLabel");
        ar->addWidget(al);
        labels_alpha_spin_ = new QDoubleSpinBox(labels_alpha_row_);
        labels_alpha_spin_->setRange(0.1, 5.0);
        labels_alpha_spin_->setValue(2.0);
        labels_alpha_spin_->setDecimals(2);
        labels_alpha_spin_->setSingleStep(0.1);
        ar->addWidget(labels_alpha_spin_);
        pl->addWidget(labels_alpha_row_);

        // Toggle row visibility based on the chosen label generator.
        auto update_label_rows = [this]() {
            const QString lt = labels_type_combo_->currentText();
            labels_horizon_row_->setVisible(lt == "FIXLB");
            labels_window_row_->setVisible(lt == "MEANLB" || lt == "LEXLB" || lt == "TRENDLB" || lt == "BOLB");
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
        auto* t = new QLabel(tr("CV SPLITS"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* tl = new QLabel(tr("SPLITTER TYPE"), page);
        tl->setObjectName("btFieldLabel");
        pl->addWidget(tl);
        splitter_type_combo_ = new QComboBox(page);
        for (const auto& st : splitter_types())
            splitter_type_combo_->addItem(st);
        pl->addWidget(splitter_type_combo_);

        // Helper to build a labeled spinbox row inside its own QWidget container.
        auto make_row = [&](const QString& label_text, QSpinBox*& spin_out, int min_v, int max_v,
                            int default_v) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            auto* lab = new QLabel(label_text, row);
            lab->setObjectName("btFieldLabel");
            rl->addWidget(lab);
            spin_out = new QSpinBox(row);
            spin_out->setRange(min_v, max_v);
            spin_out->setValue(default_v);
            rl->addWidget(spin_out);
            return row;
        };

        // Rolling-specific: training window length (in bars).
        splitter_window_row_ = make_row(tr("WINDOW LENGTH"), splitter_window_spin_, 10, 2000, 252);
        pl->addWidget(splitter_window_row_);

        // Expanding-specific: minimum training-set length before first split.
        splitter_min_row_ = make_row(tr("MIN TRAIN LENGTH"), splitter_min_spin_, 10, 2000, 252);
        pl->addWidget(splitter_min_row_);

        // Rolling + Expanding: out-of-sample window per fold.
        splitter_test_row_ = make_row(tr("TEST LENGTH"), splitter_test_spin_, 5, 1000, 63);
        pl->addWidget(splitter_test_row_);

        // Rolling + Expanding: bars to advance between folds.
        splitter_step_row_ = make_row(tr("STEP SIZE"), splitter_step_spin_, 1, 500, 21);
        pl->addWidget(splitter_step_row_);

        // PurgedKFold-specific: number of folds.
        splitter_n_splits_row_ = make_row(tr("N SPLITS"), splitter_n_splits_spin_, 2, 20, 5);
        pl->addWidget(splitter_n_splits_row_);

        // PurgedKFold-specific: bars to drop between train end and test start
        // (purge) and between test end and next train (embargo) — guards against
        // information leakage in financial-ML CV (López de Prado).
        splitter_purge_row_ = make_row(tr("PURGE LENGTH"), splitter_purge_spin_, 0, 100, 5);
        pl->addWidget(splitter_purge_row_);

        splitter_embargo_row_ = make_row(tr("EMBARGO LENGTH"), splitter_embargo_spin_, 0, 100, 5);
        pl->addWidget(splitter_embargo_row_);

        // Toggle row visibility based on the chosen splitter.
        auto update_split_rows = [this]() {
            const QString st = splitter_type_combo_->currentText();
            const bool is_rolling = (st == "RollingSplitter");
            const bool is_expanding = (st == "ExpandingSplitter");
            const bool is_purged = (st == "PurgedKFoldSplitter" || st == "PurgedKFold");
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
        auto* t = new QLabel(tr("RETURNS ANALYSIS"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* tl = new QLabel(tr("ANALYSIS TYPE"), page);
        tl->setObjectName("btFieldLabel");
        pl->addWidget(tl);
        returns_type_combo_ = new QComboBox(page);
        for (const auto& rt : returns_analysis_types())
            returns_type_combo_->addItem(rt);
        pl->addWidget(returns_type_combo_);

        // ── ROLLING WINDOW row (rolling analysis only) ──
        returns_window_row_ = new QWidget(page);
        auto* wr = new QVBoxLayout(returns_window_row_);
        wr->setContentsMargins(0, 0, 0, 0);
        wr->setSpacing(4);
        auto* wl = new QLabel(tr("ROLLING WINDOW"), returns_window_row_);
        wl->setObjectName("btFieldLabel");
        wr->addWidget(wl);
        returns_window_spin_ = new QSpinBox(returns_window_row_);
        returns_window_spin_->setRange(5, 504);
        returns_window_spin_->setValue(63);
        wr->addWidget(returns_window_spin_);
        pl->addWidget(returns_window_row_);

        // ── METRIC row (rolling analysis only) ──
        // Names must match the vbt_returns / zl_returns rolling_map dispatch keys.
        returns_metric_row_ = new QWidget(page);
        auto* mr = new QVBoxLayout(returns_metric_row_);
        mr->setContentsMargins(0, 0, 0, 0);
        mr->setSpacing(4);
        auto* ml = new QLabel(tr("METRIC"), returns_metric_row_);
        ml->setObjectName("btFieldLabel");
        mr->addWidget(ml);
        returns_metric_combo_ = new QComboBox(returns_metric_row_);
        for (const auto& m : {"sharpe", "sortino", "calmar", "omega", "info_ratio", "downside_risk", "total",
                              "annualized", "volatility"})
            returns_metric_combo_->addItem(m);
        mr->addWidget(returns_metric_combo_);
        pl->addWidget(returns_metric_row_);

        // ── THRESHOLD row (ranges analysis only — defines what counts as a "range") ──
        returns_threshold_row_ = new QWidget(page);
        auto* thr = new QVBoxLayout(returns_threshold_row_);
        thr->setContentsMargins(0, 0, 0, 0);
        thr->setSpacing(4);
        auto* thl = new QLabel(tr("THRESHOLD"), returns_threshold_row_);
        thl->setObjectName("btFieldLabel");
        thr->addWidget(thl);
        returns_threshold_spin_ = new QDoubleSpinBox(returns_threshold_row_);
        returns_threshold_spin_->setRange(-1.0, 1.0);
        returns_threshold_spin_->setValue(0.0);
        returns_threshold_spin_->setDecimals(4);
        returns_threshold_spin_->setSingleStep(0.001);
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
        auto* t = new QLabel(tr("SIGNAL GENERATORS"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* gl = new QLabel(tr("GENERATOR TYPE"), page);
        gl->setObjectName("btFieldLabel");
        pl->addWidget(gl);
        signal_gen_combo_ = new QComboBox(page);
        for (const auto& sg : signal_generators())
            signal_gen_combo_->addItem(sg);
        pl->addWidget(signal_gen_combo_);

        // Helpers to build a row inside its own container so we can toggle visibility.
        auto make_int_row = [&](const QString& text, QSpinBox*& spin, int mn, int mx, int dv) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            auto* lab = new QLabel(text, row);
            lab->setObjectName("btFieldLabel");
            rl->addWidget(lab);
            spin = new QSpinBox(row);
            spin->setRange(mn, mx);
            spin->setValue(dv);
            rl->addWidget(spin);
            return row;
        };
        auto make_prob_row = [&](const QString& text, QDoubleSpinBox*& spin, double dv) -> QWidget* {
            auto* row = new QWidget(page);
            auto* rl = new QVBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            auto* lab = new QLabel(text, row);
            lab->setObjectName("btFieldLabel");
            rl->addWidget(lab);
            spin = new QDoubleSpinBox(row);
            spin->setRange(0.001, 1.0);
            spin->setValue(dv);
            spin->setDecimals(3);
            spin->setSingleStep(0.01);
            rl->addWidget(spin);
            return row;
        };

        signal_count_row_ = make_int_row(tr("COUNT (n)"), signal_count_spin_, 1, 1000, 10);
        signal_entry_prob_row_ = make_prob_row(tr("ENTRY PROB"), signal_entry_prob_spin_, 0.05);
        signal_exit_prob_row_ = make_prob_row(tr("EXIT PROB"), signal_exit_prob_spin_, 0.10);
        signal_min_hold_row_ = make_int_row(tr("MIN HOLD (bars)"), signal_min_hold_spin_, 1, 500, 5);
        signal_max_hold_row_ = make_int_row(tr("MAX HOLD (bars)"), signal_max_hold_spin_, 1, 500, 20);

        pl->addWidget(signal_count_row_);
        pl->addWidget(signal_entry_prob_row_);
        pl->addWidget(signal_exit_prob_row_);
        pl->addWidget(signal_min_hold_row_);
        pl->addWidget(signal_max_hold_row_);

        auto update_signal_rows = [this]() {
            const QString g = signal_gen_combo_->currentText();
            const bool is_rand = (g == "RAND");
            const bool is_randx = (g == "RANDX");
            const bool is_randnx = (g == "RANDNX");
            const bool is_rprob = (g == "RPROB");
            const bool is_rprobx = (g == "RPROBX");
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
        auto* t = new QLabel(tr("LABELS -> SIGNALS"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* tl = new QLabel(tr("LABEL TYPE"), page);
        tl->setObjectName("btFieldLabel");
        pl->addWidget(tl);
        l2s_label_type_combo_ = new QComboBox(page);
        for (const auto& lt : label_types())
            l2s_label_type_combo_->addItem(lt);
        pl->addWidget(l2s_label_type_combo_);

        // Entry/exit label override — defaults match vectorbt + zipline (entry=1, exit=-1)
        // but users can flip the mapping (e.g. enter on -1 for mean-reversion).
        auto* elbl = new QLabel(tr("ENTRY LABEL"), page);
        elbl->setObjectName("btFieldLabel");
        pl->addWidget(elbl);
        l2s_entry_label_spin_ = new QSpinBox(page);
        l2s_entry_label_spin_->setRange(-2, 2);
        l2s_entry_label_spin_->setValue(1);
        pl->addWidget(l2s_entry_label_spin_);

        auto* xlbl = new QLabel(tr("EXIT LABEL"), page);
        xlbl->setObjectName("btFieldLabel");
        pl->addWidget(xlbl);
        l2s_exit_label_spin_ = new QSpinBox(page);
        l2s_exit_label_spin_->setRange(-2, 2);
        l2s_exit_label_spin_->setValue(-1);
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
        auto* t = new QLabel(tr("INDICATOR SWEEP"), page);
        t->setObjectName("btSectionTitle");
        pl->addWidget(t);

        auto* il = new QLabel(tr("INDICATOR"), page);
        il->setObjectName("btFieldLabel");
        pl->addWidget(il);
        sweep_indicator_combo_ = new QComboBox(page);
        pl->addWidget(sweep_indicator_combo_);

        auto* mnl = new QLabel(tr("MIN"), page);
        mnl->setObjectName("btFieldLabel");
        pl->addWidget(mnl);
        sweep_min_spin_ = new QSpinBox(page);
        sweep_min_spin_->setRange(1, 1000);
        sweep_min_spin_->setValue(5);
        pl->addWidget(sweep_min_spin_);

        auto* mxl = new QLabel(tr("MAX"), page);
        mxl->setObjectName("btFieldLabel");
        pl->addWidget(mxl);
        sweep_max_spin_ = new QSpinBox(page);
        sweep_max_spin_->setRange(1, 1000);
        sweep_max_spin_->setValue(50);
        pl->addWidget(sweep_max_spin_);

        auto* sl = new QLabel(tr("STEP"), page);
        sl->setObjectName("btFieldLabel");
        pl->addWidget(sl);
        sweep_step_spin_ = new QSpinBox(page);
        sweep_step_spin_->setRange(1, 100);
        sweep_step_spin_->setValue(1);
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
    bar->setObjectName("btStatusBar");
    bar->setFixedHeight(26);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    providers_caption_ = new QLabel(tr("PROVIDERS:"), bar);
    providers_caption_->setObjectName("btStatusCaption");
    providers_count_label_ = new QLabel(QString::number(providers_.size()), bar);
    providers_count_label_->setObjectName("btStatusValue");
    hl->addWidget(providers_caption_);
    hl->addWidget(providers_count_label_);

    // Updated whenever a provider's strategy catalogue arrives (it was a frozen "0").
    strategies_caption_ = new QLabel(tr("STRATEGIES:"), bar);
    strategies_caption_->setObjectName("btStatusCaption");
    strategies_count_label_ = new QLabel(QStringLiteral("—"), bar);
    strategies_count_label_->setObjectName("btStatusValue");
    hl->addWidget(strategies_caption_);
    hl->addWidget(strategies_count_label_);

    hl->addStretch();
    status_label_ = new QLabel(tr("READY"), bar);
    status_label_->setObjectName("btStatusText");
    status_label_->setProperty("state", "ready");
    hl->addWidget(status_label_);
    return bar;
}

} // namespace fincept::screens
