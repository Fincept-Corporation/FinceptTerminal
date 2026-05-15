// src/screens/backtesting/BacktestingScreen_Results.cpp
//
// Result rendering: summary cards, metrics table, details table, raw JSON
// pane, and the per-command formatting in display_result(). Also owns the
// service-callback handlers that route results into the UI.
//
// Part of the partial-class split of BacktestingScreen.cpp; helpers shared
// across split files live in BacktestingScreen_internal.h.

#include "screens/backtesting/BacktestingScreen.h"
#include "screens/backtesting/BacktestingScreen_internal.h"

#include "core/logging/Logger.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace fincept::screens {

using namespace fincept::services::backtest;
using fincept::screens::backtesting_internal::fmt_metric;
using fincept::screens::backtesting_internal::clear_layout;

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

} // namespace fincept::screens
