// src/screens/backtesting/BacktestingScreen_Commands.cpp
//
// Provider / command selection state, run dispatch, and arg gathering.
// Owns the UI-state transitions when the user picks a different provider
// or command (re-skinning pills, toggling section visibility, etc.) and
// translates the right-panel config widgets into a JSON args bundle for
// BacktestingService.
//
// Part of the partial-class split of BacktestingScreen.cpp; helpers shared
// across split files live in BacktestingScreen_internal.h.

#include "screens/backtesting/BacktestingScreen.h"
#include "screens/backtesting/BacktestingScreen_internal.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/theme/Theme.h"

#include <QJsonArray>
#include <QJsonObject>

namespace fincept::screens {

using namespace fincept::services::backtest;
using fincept::screens::backtesting_internal::pill_qss;

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

} // namespace fincept::screens
