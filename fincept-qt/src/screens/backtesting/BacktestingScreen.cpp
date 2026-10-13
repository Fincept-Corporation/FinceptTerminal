// src/screens/backtesting/BacktestingScreen.cpp
//
// Core lifecycle: ctor, show/hide events, service wiring, top-level layout
// assembly, IStatefulScreen/IGroupLinked overrides, and the on_error slot
// (kept here to keep the screen-level error path next to the lifecycle
// methods that consume it).
//
// This file is the entry point of a partial-class split. The rest of the
// implementation lives in:
//   - BacktestingScreen_Layout.cpp     — top/left/center/right/status bars
//   - BacktestingScreen_Commands.cpp   — provider/command selection, run,
//                                        gather_args, gather_strategy_params
//   - BacktestingScreen_Strategies.cpp — strategy combos + params rebuild
//   - BacktestingScreen_Results.cpp    — display_result/display_error and
//                                        the service-callback handlers
// Shared helpers live in BacktestingScreen_internal.h.

#include "screens/backtesting/BacktestingScreen.h"

#include "core/logging/Logger.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "screens/backtesting/BacktestingScreen_internal.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QJsonDocument>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::backtest;

// ── Constructor ──────────────────────────────────────────────────────────────

BacktestingScreen::BacktestingScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("btScreen");
    setAttribute(Qt::WA_StyledBackground, true);
    providers_ = all_providers();
    commands_ = all_commands();
    // strategies_ starts empty — populated dynamically per provider via load_strategies()
    build_ui();
    refresh_theme();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    connect_service();

    // Ctrl+Enter runs (Enter alone belongs to the focused input), Esc cancels.
    auto* run_sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    run_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(run_sc, &QShortcut::activated, this, [this]() {
        if (!is_running_)
            on_run();
    });
    auto* run_sc2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this);
    run_sc2->setContext(Qt::WidgetWithChildrenShortcut);
    connect(run_sc2, &QShortcut::activated, this, [this]() {
        if (!is_running_)
            on_run();
    });

    auto* wheel_guard = new backtesting_internal::WheelGuard(this);
    setProperty("_wheel_guard", QVariant::fromValue(static_cast<QObject*>(wheel_guard)));
    backtesting_internal::guard_all_inputs(this, wheel_guard);

    LOG_INFO("Backtesting", "Screen constructed");
}

void BacktestingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        // Use the provider/command restore_state() selected (it runs before the first
        // show); hard-coded 0/0 here discarded the restored selection every launch.
        on_provider_changed(active_provider_);
        on_command_changed(active_command_);
    }
    auto config = BacktestingService::instance().take_pending_portfolio_config();
    if (!config.isEmpty())
        apply_portfolio_config(config);
}

void BacktestingScreen::apply_portfolio_config(const QJsonObject& config) {
    auto symbols = config["symbols"].toArray();
    QStringList sym_list;
    for (const auto& s : symbols)
        sym_list.append(s.toString());
    if (symbols_edit_ && !sym_list.isEmpty())
        symbols_edit_->setText(sym_list.join(", "));
    if (capital_spin_ && config.contains("initialCapital"))
        capital_spin_->setValue(config["initialCapital"].toDouble());
    pending_weights_ = config["weights"].toArray();
    LOG_INFO("Backtesting", QString("Portfolio config applied: %1 symbols, $%2")
                                .arg(sym_list.size())
                                .arg(config["initialCapital"].toDouble(), 0, 'f', 0));

    // Auto-run: a caller (e.g. Equity Research's BACKTEST button) can request an
    // immediate basic backtest of the supplied symbol. Strategies load
    // asynchronously, so run now only if one is already selected; otherwise defer
    // until the get_strategies callback populates the combo (see on_result).
    if (config.value("autoRun").toBool()) {
        const bool strategies_ready =
            strategy_combo_ && strategy_combo_->count() > 0 && !strategy_combo_->currentData().toString().isEmpty();
        if (strategies_ready)
            trigger_auto_run();
        else
            pending_auto_run_ = true;
    }
}

void BacktestingScreen::trigger_auto_run() {
    // Defer to the next event-loop tick so the just-shown screen and any
    // freshly-rebuilt strategy params have settled before on_run() reads them.
    QTimer::singleShot(0, this, [this]() {
        if (!is_running_)
            on_run();
    });
}

void BacktestingScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // P3: never leave a 1s timer running for a screen the user can't see. The
    // run itself keeps going; the ticker resumes its count from run_elapsed_.
    if (run_timer_)
        run_timer_->stop();
}

// ── Run ticker (elapsed-time feedback for long commands) ─────────────────────

void BacktestingScreen::start_run_ticker() {
    if (!run_timer_) {
        run_timer_ = new QTimer(this);
        run_timer_->setInterval(1000);
        connect(run_timer_, &QTimer::timeout, this, [this]() {
            if (!is_running_) {
                run_timer_->stop();
                return;
            }
            const qint64 secs = run_elapsed_.elapsed() / 1000;
            set_status_state(tr("EXECUTING… %1s").arg(secs), QStringLiteral("running"));
        });
    }
    run_elapsed_.restart();
    run_timer_->start();
}

void BacktestingScreen::stop_run_ticker() {
    if (run_timer_)
        run_timer_->stop();
}

void BacktestingScreen::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void BacktestingScreen::retranslateUi() {
    // Top bar (provider/command buttons carry service-provided names — data).
    if (brand_label_)
        brand_label_->setText(tr("BACKTESTING"));
    if (run_button_)
        run_button_->setText(tr("RUN"));
    if (cancel_button_)
        cancel_button_->setText(tr("CANCEL"));
    if (export_csv_btn_)
        export_csv_btn_->setText(tr("EXPORT CSV"));
    if (allow_synthetic_check_)
        allow_synthetic_check_->setText(tr("Allow synthetic data (testing only)"));

    // Left panel section/field titles.
    if (commands_title_)
        commands_title_->setText(tr("COMMANDS"));
    if (strategy_title_)
        strategy_title_->setText(tr("STRATEGY"));
    if (strategy_cat_label_)
        strategy_cat_label_->setText(tr("CATEGORY"));
    if (strategy_pick_label_)
        strategy_pick_label_->setText(tr("STRATEGY"));
    if (params_title_)
        params_title_->setText(tr("PARAMETERS"));

    // Center panel.
    if (results_title_)
        results_title_->setText(tr("RESULTS"));
    if (export_json_btn_)
        export_json_btn_->setText(tr("EXPORT JSON"));
    if (summary_hint_)
        summary_hint_->setText(
            tr("Select a provider, command, and strategy, then click RUN to execute.\n\n"
               "Supported providers: VectorBT, Backtesting.py, FastTrade, Zipline, BT, Fincept\n"
               "Commands: Backtest, Optimize, Walk-Forward, Indicators, ML Labels, CV Splits, Returns"));
    if (equity_hint_)
        equity_hint_->setText(tr("Run a backtest to see the equity curve."));
    if (result_tabs_ && result_tabs_->count() >= 5) {
        result_tabs_->setTabText(0, tr("SUMMARY"));
        result_tabs_->setTabText(1, tr("EQUITY CURVE"));
        result_tabs_->setTabText(2, tr("METRICS"));
        result_tabs_->setTabText(3, tr("DETAILS"));
        result_tabs_->setTabText(4, tr("RAW JSON"));
    }
    if (metrics_table_)
        metrics_table_->setHorizontalHeaderLabels({tr("Metric"), tr("Value")});

    // Right panel section/field titles.
    if (market_data_title_)
        market_data_title_->setText(tr("MARKET DATA"));
    if (symbols_label_)
        symbols_label_->setText(tr("SYMBOLS"));
    if (start_label_)
        start_label_->setText(tr("START"));
    if (end_label_)
        end_label_->setText(tr("END"));
    if (interval_label_)
        interval_label_->setText(tr("INTERVAL"));
    if (execution_title_)
        execution_title_->setText(tr("EXECUTION"));
    if (commission_label_)
        commission_label_->setText(tr("COMMISSION (%)"));
    if (slippage_label_)
        slippage_label_->setText(tr("SLIPPAGE (%)"));
    if (risk_free_label_)
        risk_free_label_->setText(tr("RISK-FREE RATE (%)"));
    if (advanced_title_)
        advanced_title_->setText(tr("ADVANCED"));
    if (leverage_label_)
        leverage_label_->setText(tr("LEVERAGE"));
    if (stop_loss_label_)
        stop_loss_label_->setText(tr("STOP LOSS (%)"));
    if (take_profit_label_)
        take_profit_label_->setText(tr("TAKE PROFIT (%)"));
    if (pos_sizing_label_)
        pos_sizing_label_->setText(tr("POSITION SIZING"));
    if (benchmark_title_)
        benchmark_title_->setText(tr("BENCHMARK"));
    if (benchmark_label_)
        benchmark_label_->setText(tr("SYMBOL"));
    if (stop_loss_spin_)
        stop_loss_spin_->setSpecialValueText(tr("None"));
    if (take_profit_spin_)
        take_profit_spin_->setSpecialValueText(tr("None"));
    if (allow_short_check_)
        allow_short_check_->setText(tr("Allow Short Selling"));

    // Status bar.
    if (providers_caption_)
        providers_caption_->setText(tr("PROVIDERS:"));
    if (strategies_caption_)
        strategies_caption_->setText(tr("STRATEGIES:"));
    // status_label_ / status_dot_ carry transient run state — left as-is.
    // Command-config pages and dynamically rebuilt param rows pick up the new
    // language when the relevant combo/command rebuilds them.
}

void BacktestingScreen::connect_service() {
    auto& svc = BacktestingService::instance();
    connect(&svc, &BacktestingService::result_ready, this, &BacktestingScreen::on_result);
    connect(&svc, &BacktestingService::error_occurred, this, &BacktestingScreen::on_error);
    connect(&svc, &BacktestingService::run_completed, this, &BacktestingScreen::on_run_completed);
    connect(&svc, &BacktestingService::run_failed, this, &BacktestingScreen::on_run_failed);
    connect(&svc, &BacktestingService::run_cancelled, this, &BacktestingScreen::on_run_cancelled);
    connect(&svc, &BacktestingService::command_options_loaded, this, &BacktestingScreen::on_command_options_loaded);
}

void BacktestingScreen::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape && is_running_) {
        on_cancel();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void BacktestingScreen::set_status_state(const QString& text, const QString& state) {
    status_label_->setText(text);
    status_dot_->setText(text);
    backtesting_internal::set_style_prop(status_label_, "state", state);
    backtesting_internal::set_style_prop(status_dot_, "state", state);
}

// ── Theme: one root stylesheet for the whole screen ─────────────────────────
//
// Every widget in the screen is styled from here through objectName and
// dynamic-property selectors. The screen used ~170 inline style-sheet calls
// (several inside loops — one CSS reparse per metric card / parameter row per
// run); selector-less panel sheets also cascaded stray borders onto every child.

void BacktestingScreen::refresh_theme() {
    using namespace ui::colors;
    const QString ff = QString(ui::fonts::DATA_FAMILY);
    const int tiny = ui::fonts::TINY;
    const int small = ui::fonts::SMALL;
    const int data = ui::fonts::DATA;
    const int pill_h = backtesting_internal::kPillHeight - 2; // minus 1px top + bottom border
    const int pill_pad = backtesting_internal::kPillPadH;

    auto pill = [&](const QString& selector, const QString& fg, const QString& bg, const QString& border,
                    const QString& weight) {
        return QString("%1 { color:%2; background:%3; border:1px solid %4; font-family:%5; font-size:%6px;"
                       " font-weight:%7; padding:0 %8px; min-height:%9px; max-height:%9px; }\n")
            .arg(selector, fg, bg, border, ff)
            .arg(tiny)
            .arg(weight)
            .arg(pill_pad)
            .arg(pill_h);
    };
    auto rgb = [](const QColor& c) { return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue()); };

    QString ss;
    ss.reserve(16000);

    // Root + panels. The global app sheet paints every QWidget with the base
    // colour, so each panel re-asserts its background for its descendants.
    ss += QString("QWidget#btScreen { background:%1; }\n").arg(BG_BASE());
    ss += QString("QSplitter#btMainSplitter::handle { background:%1; }\n").arg(BORDER_DIM());
    ss += QString("QWidget#btTopBar { background:%1; border-bottom:1px solid %2; }\n"
                  "QWidget#btTopBar QWidget { background:transparent; }\n")
              .arg(BG_RAISED(), BORDER_DIM());
    ss += QString("QWidget#btDivider { background:%1; }\n").arg(BORDER_DIM());
    ss += QString("QWidget#btLeftPanel { background:%1; border-right:1px solid %2; }\n"
                  "QWidget#btRightPanel { background:%1; border-left:1px solid %2; }\n"
                  "QWidget#btLeftPanel QWidget, QWidget#btRightPanel QWidget { background:%1; }\n")
              .arg(BG_SURFACE(), BORDER_DIM());
    ss += QString("QScrollArea#btPanelScroll, QScrollArea#btSummaryScroll { border:none; }\n"
                  "QScrollArea#btPanelScroll QScrollBar:vertical { width:5px; background:transparent; }\n"
                  "QScrollArea#btPanelScroll QScrollBar::handle:vertical { background:%1; }\n"
                  "QScrollArea#btPanelScroll QScrollBar::handle:vertical:hover { background:%2; }\n"
                  "QScrollArea#btPanelScroll QScrollBar::add-line:vertical,"
                  " QScrollArea#btPanelScroll QScrollBar::sub-line:vertical { height:0; }\n")
              .arg(BORDER_MED(), BORDER_BRIGHT());

    // Top bar chips
    ss += pill("QLabel#btBrand", AMBER(), "rgba(217,119,6,0.1)", AMBER_DIM(), "700");
    ss += pill("QPushButton#btRunButton", AMBER(), "rgba(217,119,6,0.1)", AMBER_DIM(), "700");
    ss += QString("QPushButton#btRunButton:hover { background:%1; color:%2; }\n"
                  "QPushButton#btRunButton:disabled { background:%3; color:%4; border-color:%5; }\n")
              .arg(AMBER(), BG_BASE(), BG_RAISED(), TEXT_DIM(), BORDER_DIM());
    ss += pill("QPushButton#btCancelButton", NEGATIVE(), "rgba(220,38,38,0.10)", NEGATIVE(), "700");
    ss += QString("QPushButton#btCancelButton:hover { background:%1; color:%2; }\n").arg(NEGATIVE(), TEXT_PRIMARY());
    ss += pill("QPushButton#btProviderTab", TEXT_TERTIARY(), "transparent", BORDER_DIM(), "400");
    for (const auto& p : providers_) {
        const QString c = rgb(p.color);
        ss += QString("QPushButton#btProviderTab[slug=\"%1\"]:hover { background:rgba(%2,0.15); }\n"
                      "QPushButton#btProviderTab[slug=\"%1\"][active=\"true\"] { color:%3; background:rgba(%2,0.12);"
                      " border-color:rgba(%2,0.3); font-weight:700; }\n")
                  .arg(p.slug, c, p.color.name());
    }

    // Status chip + status-bar text share one state vocabulary.
    struct StateColors {
        const char* state;
        QString fg;
        QString bg;
        QString border;
    };
    const StateColors states[] = {
        {"ready", POSITIVE(), "rgba(22,163,74,0.08)", "rgba(22,163,74,0.25)"},
        {"done", POSITIVE(), "rgba(22,163,74,0.08)", "rgba(22,163,74,0.25)"},
        {"running", WARNING(), "rgba(217,119,6,0.08)", WARNING()},
        {"error", NEGATIVE(), "rgba(220,38,38,0.08)", NEGATIVE()},
        {"cancelled", TEXT_SECONDARY(), BG_RAISED(), BORDER_MED()},
        {"synthetic", NEGATIVE(), "rgba(220,38,38,0.15)", NEGATIVE()},
    };
    ss += pill("QLabel#btStatusChip", POSITIVE(), "rgba(22,163,74,0.08)", "rgba(22,163,74,0.25)", "700");
    ss += QString("QLabel#btStatusText { font-family:%1; font-size:%2px; font-weight:700; }\n").arg(ff).arg(tiny);
    for (const auto& st : states) {
        ss += QString("QLabel#btStatusChip[state=\"%1\"] { color:%2; background:%3; border-color:%4; }\n"
                      "QLabel#btStatusText[state=\"%1\"] { color:%2; }\n")
                  .arg(QLatin1String(st.state), st.fg, st.bg, st.border);
    }

    // Section titles, field labels, parameter labels
    ss += QString("QLabel#btSectionTitle { color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                  " letter-spacing:1px; padding-bottom:4px; border-bottom:1px solid %4; }\n")
              .arg(AMBER())
              .arg(tiny)
              .arg(ff, BORDER_DIM());
    ss += QString("QLabel#btFieldLabel { color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                  " letter-spacing:1px; }\n")
              .arg(TEXT_TERTIARY())
              .arg(tiny)
              .arg(ff);
    ss += QString("QLabel#btParamLabel { color:%1; font-size:%2px; font-family:%3; }\n"
                  "QLabel#btSubLabel { color:%4; font-size:%2px; font-family:%3; }\n")
              .arg(TEXT_TERTIARY())
              .arg(tiny)
              .arg(ff, TEXT_DIM());

    // Command list (per-command accent colour keyed on the "cmd" property)
    ss += QString("QPushButton#btCommandButton { text-align:left; padding:6px 10px; border:none;"
                  " border-left:1px solid transparent; color:%1; font-size:%2px; font-family:%3;"
                  " font-weight:400; background:transparent; }\n"
                  "QPushButton#btCommandButton:disabled { color:%4; border-left-color:transparent; }\n")
              .arg(TEXT_SECONDARY())
              .arg(small)
              .arg(ff, TEXT_DIM());
    for (const auto& c : commands_) {
        const QString crgb = rgb(c.color);
        ss += QString("QPushButton#btCommandButton[cmd=\"%1\"]:hover { background:rgba(%2,0.1); color:%3; }\n"
                      "QPushButton#btCommandButton[cmd=\"%1\"][active=\"true\"] { border-left-color:%3; color:%3;"
                      " font-weight:700; background:rgba(%2,0.08); }\n")
                  .arg(c.id, crgb, c.color.name());
    }

    // Inputs inside the side panels
    const QString panels_in = QStringLiteral("QWidget#btLeftPanel %1, QWidget#btRightPanel %1");
    ss += QString("%1 { background:%2; color:%3; border:1px solid %4; font-family:%5; font-size:%6px;"
                  " padding:4px 6px; }\n")
              .arg(panels_in.arg("QComboBox"), BG_RAISED(), TEXT_PRIMARY(), BORDER_MED(), ff)
              .arg(small);
    ss += QString("%1 { border:none; }\n").arg(panels_in.arg("QComboBox::drop-down"));
    ss += QString("%1 { background:%2; color:%3; border:1px solid %4; selection-background-color:rgba(217,119,6,0.15); }\n")
              .arg(panels_in.arg("QComboBox QAbstractItemView"), BG_RAISED(), TEXT_PRIMARY(), BORDER_MED());
    for (const char* type : {"QLineEdit", "QDoubleSpinBox", "QSpinBox", "QDateEdit"}) {
        ss += QString("%1 { background:%2; color:%3; border:1px solid %4; font-family:%5; font-size:%6px;"
                      " padding:4px 6px; }\n")
                  .arg(panels_in.arg(QLatin1String(type)), BG_RAISED(), TEXT_PRIMARY(), BORDER_MED(), ff)
                  .arg(small);
        ss += QString("%1 { border-color:%2; }\n")
                  .arg(panels_in.arg(QString::fromLatin1(type) + QStringLiteral(":focus")), BORDER_BRIGHT());
    }
    ss += QString("%1 { color:%2; font-family:%3; font-size:%4px; spacing:6px; }\n")
              .arg(panels_in.arg("QCheckBox"), TEXT_SECONDARY(), ff)
              .arg(small);
    ss += QString("%1 { width:14px; height:14px; border:1px solid %2; background:%3; }\n")
              .arg(panels_in.arg("QCheckBox::indicator"), BORDER_MED(), BG_RAISED());
    ss += QString("%1 { background:%2; border-color:%2; }\n").arg(panels_in.arg("QCheckBox::indicator:checked"), AMBER());
    // The synthetic opt-in reads as a warning when ticked.
    ss += QString("QCheckBox#btSyntheticCheck:checked { color:%1; font-weight:700; }\n"
                  "QCheckBox#btSyntheticCheck::indicator:checked { background:%1; border-color:%1; }\n")
              .arg(NEGATIVE());

    // Center: header, tabs, tables
    ss += QString("QWidget#btResultsHeader { background:%1; border-bottom:1px solid %2; }\n"
                  "QWidget#btResultsHeader QWidget { background:transparent; }\n")
              .arg(BG_RAISED(), BORDER_DIM());
    ss += QString("QLabel#btResultsTitle { color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                  " letter-spacing:1px; }\n")
              .arg(AMBER())
              .arg(tiny)
              .arg(ff);
    ss += QString("QPushButton#btHeaderButton { background:transparent; color:%1; border:1px solid %2;"
                  " padding:0 10px; font-size:%3px; font-family:%4; }\n"
                  "QPushButton#btHeaderButton:hover { color:%5; border-color:%5; }\n"
                  "QPushButton#btHeaderButton::menu-indicator { width:0; }\n")
              .arg(TEXT_SECONDARY(), BORDER_DIM())
              .arg(tiny)
              .arg(ff, AMBER());
    ss += QString("QTabWidget#btResultTabs::pane { border:none; background:%1; }\n"
                  "QTabWidget#btResultTabs QTabBar::tab { background:transparent; color:%2; font-family:%3;"
                  " font-size:%4px; font-weight:600; padding:6px 14px; border:none; letter-spacing:0.5px; }\n"
                  "QTabWidget#btResultTabs QTabBar::tab:selected { color:%5; background:%6; }\n"
                  "QTabWidget#btResultTabs QTabBar::tab:hover { color:%7; background:%8; }\n")
              .arg(BG_BASE(), TEXT_TERTIARY(), ff)
              .arg(small)
              .arg(TEXT_PRIMARY(), ORANGE(), TEXT_SECONDARY(), BG_RAISED());
    ss += QString("QWidget#btSummary, QWidget#btEquityTab { background:%1; }\n").arg(BG_BASE());
    ss += QString("QLabel#btHint { color:%1; font-size:%2px; font-family:%3; padding:20px; background:%4;"
                  " border:1px solid %5; }\n"
                  "QLabel#btEquityHint { color:%6; font-size:%2px; font-family:%3; }\n")
              .arg(TEXT_SECONDARY())
              .arg(small)
              .arg(ff, BG_RAISED(), BORDER_DIM(), TEXT_TERTIARY());
    for (const char* table : {"QTableWidget#btMetricsTable", "QTableView#btDetailsTable"}) {
        ss += QString("%1 { background:%2; color:%3; gridline-color:%4; font-family:%5; font-size:%6px; border:none;"
                      " alternate-background-color:%7; }\n"
                      "%1::item { padding:3px 8px; }\n"
                      "%1 QHeaderView::section { background:%8; color:%9; font-weight:700; padding:4px 8px;"
                      " border:1px solid %4; font-family:%5; font-size:%6px; }\n")
                  .arg(QLatin1String(table), BG_SURFACE(), TEXT_PRIMARY(), BORDER_DIM(), ff)
                  .arg(small)
                  .arg(ROW_ALT(), BG_RAISED(), TEXT_SECONDARY());
    }
    ss += QString("QTextEdit#btRawJson { background:%1; color:%2; border:none; font-family:%3; font-size:%4px;"
                  " padding:12px; }\n")
              .arg(BG_SURFACE(), TEXT_PRIMARY(), ff)
              .arg(small);

    // Result summary widgets (built per run in display_result)
    ss += QString("QLabel#btResultSection { color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                  " letter-spacing:1px; padding-top:4px; }\n")
              .arg(AMBER())
              .arg(tiny)
              .arg(ff);
    ss += QString("QWidget#btCard { background:%1; border:1px solid %2; }\n"
                  "QWidget#btCard QLabel { background:transparent; border:none; }\n"
                  "QLabel#btCardLabel { color:%3; font-size:%4px; font-family:%5; }\n"
                  "QLabel#btCardValue { color:%6; font-size:%7px; font-weight:700; font-family:%5; }\n")
              .arg(BG_RAISED(), BORDER_DIM(), TEXT_TERTIARY())
              .arg(tiny)
              .arg(ff, AMBER())
              .arg(data);
    for (const auto& p : providers_) {
        ss += QString("QWidget#btSummary[provider=\"%1\"] QLabel#btCardValue,"
                      " QWidget#btSummary[provider=\"%1\"] QLabel#btResultSection { color:%2; }\n")
                  .arg(p.slug, p.color.name());
    }
    ss += QString("QLabel#btBanner { font-family:%1; font-size:%2px; padding:6px 10px; }\n"
                  "QLabel#btBanner[tone=\"info\"] { color:%3; background:%4; border:1px solid %5; }\n"
                  "QLabel#btBanner[tone=\"warn\"] { color:%6; background:rgba(245,158,11,0.10); border:1px solid %6; }\n"
                  "QLabel#btBanner[tone=\"error\"] { color:%7; background:rgba(220,38,38,0.08); border:1px solid %7; }\n"
                  "QLabel#btBanner[tone=\"synthetic\"] { color:%8; background:%7; border:1px solid %7;"
                  " font-weight:700; font-size:%9px; padding:10px 12px; }\n")
              .arg(ff)
              .arg(tiny)
              .arg(TEXT_TERTIARY(), BG_RAISED(), BORDER_DIM(), WARNING(), NEGATIVE(), TEXT_PRIMARY())
              .arg(small);
    ss += QString("QLabel#btAssumptions { color:%1; font-family:%2; font-size:%3px; padding:8px 10px;"
                  " background:%4; border:1px solid %5; }\n")
              .arg(TEXT_SECONDARY(), ff)
              .arg(tiny)
              .arg(BG_SURFACE(), BORDER_DIM());

    // Status bar
    ss += QString("QWidget#btStatusBar { background:%1; border-top:1px solid %2; }\n"
                  "QWidget#btStatusBar QWidget { background:transparent; }\n"
                  "QLabel#btStatusCaption { color:%3; font-size:%4px; font-family:%5; }\n"
                  "QLabel#btStatusValue { color:%6; font-size:%4px; font-weight:700; font-family:%5; }\n")
              .arg(BG_RAISED(), BORDER_DIM(), TEXT_TERTIARY())
              .arg(tiny)
              .arg(ff, TEXT_PRIMARY());

    setStyleSheet(ss);
}

void BacktestingScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("btMainSplitter");
    splitter->setHandleWidth(1);
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
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap BacktestingScreen::save_state() const {
    QVariantMap state{
        {"provider", active_provider_},
        {"command", active_command_},
    };
    if (symbols_edit_)
        state["symbols"] = symbols_edit_->text();
    if (benchmark_edit_)
        state["benchmark"] = benchmark_edit_->text();
    if (!last_payload_.isEmpty()) {
        const QString json_text = QString::fromUtf8(QJsonDocument(last_payload_).toJson(QJsonDocument::Compact));
        if (json_text.size() < 500000)
            state["last_result"] = json_text;
    }
    return state;
}

void BacktestingScreen::restore_state(const QVariantMap& state) {
    const int prov = state.value("provider", 0).toInt();
    const int cmd = state.value("command", 0).toInt();
    if (first_show_) {
        // Not shown yet: just record the selection — showEvent() applies it once, instead of
        // firing a provider reload here and a second one at first show.
        if (prov >= 0 && prov < providers_.size())
            active_provider_ = prov;
        if (cmd >= 0 && cmd < commands_.size())
            active_command_ = cmd;
    } else {
        if (prov != active_provider_)
            on_provider_changed(prov);
        if (cmd != active_command_)
            on_command_changed(cmd);
    }
    if (symbols_edit_ && state.contains("symbols"))
        symbols_edit_->setText(state.value("symbols").toString());
    if (benchmark_edit_ && state.contains("benchmark"))
        benchmark_edit_->setText(state.value("benchmark").toString());
    if (raw_json_edit_ && state.contains("last_result")) {
        const QString text = state.value("last_result").toString();
        raw_json_edit_->setPlainText(text);
        // Keep exports working after a restart: the restored text is the full payload
        // (save_state only persists results under 500 KB).
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
        if (doc.isObject())
            last_payload_ = doc.object();
    }
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
