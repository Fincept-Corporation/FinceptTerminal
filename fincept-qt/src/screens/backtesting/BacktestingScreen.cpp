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
#include "screens/backtesting/BacktestingScreen_internal.h"

#include "core/logging/Logger.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/theme/Theme.h"

#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::backtest;

// ── Constructor ──────────────────────────────────────────────────────────────

BacktestingScreen::BacktestingScreen(QWidget* parent) : QWidget(parent) {
    providers_ = all_providers();
    commands_ = all_commands();
    // strategies_ starts empty — populated dynamically per provider via load_strategies()
    build_ui();
    connect_service();

    auto* wheel_guard = new backtesting_internal::WheelGuard(this);
    setProperty("_wheel_guard", QVariant::fromValue(static_cast<QObject*>(wheel_guard)));
    backtesting_internal::guard_all_inputs(this, wheel_guard);

    LOG_INFO("Backtesting", "Screen constructed");
}

void BacktestingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_provider_changed(0);
        on_command_changed(0);
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
        const bool strategies_ready = strategy_combo_ && strategy_combo_->count() > 0 &&
                                      !strategy_combo_->currentData().toString().isEmpty();
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
}

void BacktestingScreen::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void BacktestingScreen::retranslateUi() {
    // Top bar (provider/command buttons carry service-provided names — data).
    if (brand_label_) brand_label_->setText(tr("BACKTESTING"));
    if (run_button_)  run_button_->setText(tr("RUN"));

    // Left panel section/field titles.
    if (commands_title_)      commands_title_->setText(tr("COMMANDS"));
    if (strategy_title_)      strategy_title_->setText(tr("STRATEGY"));
    if (strategy_cat_label_)  strategy_cat_label_->setText(tr("CATEGORY"));
    if (strategy_pick_label_) strategy_pick_label_->setText(tr("STRATEGY"));
    if (params_title_)        params_title_->setText(tr("PARAMETERS"));

    // Center panel.
    if (results_title_)   results_title_->setText(tr("RESULTS"));
    if (export_json_btn_) export_json_btn_->setText(tr("EXPORT JSON"));
    if (summary_hint_)
        summary_hint_->setText(tr("Select a provider, command, and strategy, then click RUN to execute.\n\n"
                                  "Supported providers: VectorBT, Backtesting.py, FastTrade, Zipline, BT, Fincept\n"
                                  "Commands: Backtest, Optimize, Walk-Forward, Indicators, ML Labels, CV Splits, Returns"));
    if (equity_hint_)     equity_hint_->setText(tr("Run a backtest to see the equity curve."));
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
    if (market_data_title_) market_data_title_->setText(tr("MARKET DATA"));
    if (symbols_label_)     symbols_label_->setText(tr("SYMBOLS"));
    if (start_label_)       start_label_->setText(tr("START"));
    if (end_label_)         end_label_->setText(tr("END"));
    if (interval_label_)    interval_label_->setText(tr("INTERVAL"));
    if (execution_title_)   execution_title_->setText(tr("EXECUTION"));
    if (commission_label_)  commission_label_->setText(tr("COMMISSION (%)"));
    if (slippage_label_)    slippage_label_->setText(tr("SLIPPAGE (%)"));
    if (risk_free_label_)   risk_free_label_->setText(tr("RISK-FREE RATE (%)"));
    if (advanced_title_)    advanced_title_->setText(tr("ADVANCED"));
    if (leverage_label_)    leverage_label_->setText(tr("LEVERAGE"));
    if (stop_loss_label_)   stop_loss_label_->setText(tr("STOP LOSS (%)"));
    if (take_profit_label_) take_profit_label_->setText(tr("TAKE PROFIT (%)"));
    if (pos_sizing_label_)  pos_sizing_label_->setText(tr("POSITION SIZING"));
    if (benchmark_title_)   benchmark_title_->setText(tr("BENCHMARK"));
    if (benchmark_label_)   benchmark_label_->setText(tr("SYMBOL"));
    if (stop_loss_spin_)    stop_loss_spin_->setSpecialValueText(tr("None"));
    if (take_profit_spin_)  take_profit_spin_->setSpecialValueText(tr("None"));
    if (allow_short_check_) allow_short_check_->setText(tr("Allow Short Selling"));

    // Status bar.
    if (providers_caption_)  providers_caption_->setText(tr("PROVIDERS:"));
    if (strategies_caption_) strategies_caption_->setText(tr("STRATEGIES:"));
    // status_label_ / status_dot_ carry transient run state — left as-is.
    // Command-config pages and dynamically rebuilt param rows pick up the new
    // language when the relevant combo/command rebuilds them.
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

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap BacktestingScreen::save_state() const {
    QVariantMap state{
        {"provider", active_provider_},
        {"command", active_command_},
    };
    if (symbols_edit_) state["symbols"] = symbols_edit_->text();
    if (benchmark_edit_) state["benchmark"] = benchmark_edit_->text();
    if (raw_json_edit_ && !raw_json_edit_->toPlainText().isEmpty()) {
        const QString json_text = raw_json_edit_->toPlainText();
        if (json_text.size() < 500000)
            state["last_result"] = json_text;
    }
    return state;
}

void BacktestingScreen::restore_state(const QVariantMap& state) {
    const int prov = state.value("provider", 0).toInt();
    const int cmd = state.value("command", 0).toInt();
    if (prov != active_provider_)
        on_provider_changed(prov);
    if (cmd != active_command_)
        on_command_changed(cmd);
    if (symbols_edit_ && state.contains("symbols"))
        symbols_edit_->setText(state.value("symbols").toString());
    if (benchmark_edit_ && state.contains("benchmark"))
        benchmark_edit_->setText(state.value("benchmark").toString());
    if (raw_json_edit_ && state.contains("last_result"))
        raw_json_edit_->setPlainText(state.value("last_result").toString());
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
