// src/screens/backtesting/BacktestingScreen_Results.cpp
//
// Result rendering: summary cards, metrics table, details table, raw JSON
// pane, and the per-command formatting in display_result(). Also owns the
// service-callback handlers that route results into the UI.
//
// Part of the partial-class split of BacktestingScreen.cpp; helpers shared
// across split files live in BacktestingScreen_internal.h.

#include "core/logging/Logger.h"
#include "screens/backtesting/BacktestingScreen.h"
#include "screens/backtesting/BacktestingScreen_internal.h"
#include "services/backtesting/BacktestingService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QLocale>
#include <QMessageBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace fincept::screens {

using namespace fincept::services::backtest;
using fincept::screens::backtesting_internal::clear_layout;
using fincept::screens::backtesting_internal::fmt_metric;

// ── Result display ───────────────────────────────────────────────────────────

void BacktestingScreen::clear_results() {
    // The two hint labels live in the layouts torn down below and are deleted with them.
    // retranslateUi() still dereferences these cached pointers on a language switch, so a
    // run followed by a language change touched freed widgets — drop them here.
    summary_hint_ = nullptr;
    equity_hint_ = nullptr;
    clear_layout(summary_layout_);
    metrics_table_->setRowCount(0);
    set_detail_rows({}, {});
    raw_json_edit_->clear();
    if (result_tabs_ && result_tabs_->count() > 3) {
        result_tabs_->setTabText(0, tr("SUMMARY"));
        result_tabs_->setTabText(3, tr("DETAILS"));
    }
    if (equity_chart_tab_ && equity_chart_tab_->layout()) {
        auto* layout = equity_chart_tab_->layout();
        while (layout->count() > 0) {
            auto* item = layout->takeAt(0);
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }
}

void BacktestingScreen::set_detail_rows(const QJsonArray& rows, const QStringList& columns) {
    // A fresh reset every run: LiveTableModel only diffs in place when the key
    // sequence is unchanged, which for a new result would mean comparing every cell.
    details_model_->clear();
    details_cols_ = columns;
    details_model_->set_headers(columns);
    QVector<BtDetailRow> data;
    data.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i)
        data.push_back(BtDetailRow{i, rows[i].toObject()});
    details_model_->upsert(data);
    if (details_proxy_)
        details_proxy_->sort(-1); // natural (provider) order until the user clicks a header
    if (!columns.isEmpty())
        details_view_->resizeColumnsToContents(); // bounded by setResizeContentsPrecision(200)
}

void BacktestingScreen::display_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    const QString slug = running_provider_.isEmpty() ? providers_[active_provider_].slug : running_provider_;
    const QColor accent_color = [&]() {
        for (const auto& pr : providers_)
            if (pr.slug == slug)
                return pr.color;
        return providers_[active_provider_].color;
    }();
    // Card values / section headers take the provider accent via a per-slug rule.
    backtesting_internal::set_style_prop(summary_container_, "provider", slug);

    // ── Local helpers (capture summary_container_ / metrics_table_) ──

    auto add_section_header = [&](const QString& text) {
        auto* lbl = new QLabel(text, summary_container_);
        lbl->setObjectName("btResultSection");
        summary_layout_->addWidget(lbl);
    };

    // tone: info | warn | error | synthetic (rules in refresh_theme)
    auto add_banner = [&](const QString& text, const char* tone) {
        auto* lbl = new QLabel(text, summary_container_);
        lbl->setObjectName("btBanner");
        lbl->setProperty("tone", QString::fromLatin1(tone));
        lbl->setWordWrap(true);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        summary_layout_->addWidget(lbl);
    };

    auto humanize = [](QString key) {
        key.replace('_', ' ');
        key.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
        return key.toUpper();
    };

    auto add_cards_from = [&](const QJsonObject& obj, const QStringList& ordered_keys, int cols_per_row = 3) {
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
            // An explicit null profit factor means "no losing trades" (see fmt_metric) — keep it.
            const bool null_pf = val.isNull() && (key == QLatin1String("profitFactor") ||
                                                  key == QLatin1String("profit_factor"));
            if (val.isObject() || val.isArray() || (val.isNull() && !null_pf))
                continue;
            auto canon = key;
            canon.replace(QRegularExpression("([a-z])([A-Z])"), "\\1_\\2");
            canon = canon.toLower();
            if (shown.contains(canon))
                continue;
            shown.append(canon);

            auto* card = new QWidget(cards);
            card->setObjectName("btCard");
            card->setAttribute(Qt::WA_StyledBackground, true);
            auto* cvl = new QVBoxLayout(card);
            cvl->setContentsMargins(10, 8, 10, 8);
            cvl->setSpacing(2);
            auto* lbl = new QLabel(humanize(key), card);
            lbl->setObjectName("btCardLabel");
            auto* vlbl = new QLabel(fmt_metric(key, val), card);
            vlbl->setObjectName("btCardValue");
            cvl->addWidget(lbl);
            cvl->addWidget(vlbl);
            gl->addWidget(card, row, col);
            if (++col >= cols_per_row) {
                col = 0;
                row++;
            }
        }
        if (row > 0 || col > 0)
            summary_layout_->addWidget(cards);
        else
            cards->deleteLater();
    };

    auto fill_kv_table = [&](const QJsonObject& obj) {
        QStringList keys;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const bool null_pf = it.value().isNull() && (it.key() == QLatin1String("profitFactor") ||
                                                         it.key() == QLatin1String("profit_factor"));
            if (!it.value().isObject() && !it.value().isArray() && (!it.value().isNull() || null_pf))
                keys.append(it.key());
        }
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

    // Every row goes to the model (no cap): the view formats only visible cells.
    int details_total = 0;
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
        details_total = static_cast<int>(arr.size());
        set_detail_rows(arr, cols);
    };

    // ── Header (per-command title) ──
    QString cmd_label = command.toUpper();
    for (const auto& c : commands_)
        if (c.id == command) {
            cmd_label = c.label.toUpper();
            break;
        }
    add_section_header(tr("%1 RESULTS").arg(cmd_label));

    // ── Synthetic data: impossible to miss (banner, tab title, status chip, exports) ──
    const bool synthetic = payload.value("usingSyntheticData").toBool();
    if (synthetic) {
        add_banner(tr("SYNTHETIC DATA — these prices were randomly generated because real market data could not "
                      "be loaded (you enabled \"Allow synthetic data\"). Every number below has NO market meaning. "
                      "Do not use it for any decision."),
                   "synthetic");
        if (result_tabs_)
            result_tabs_->setTabText(0, tr("SUMMARY · SYNTHETIC"));
    }

    const QJsonObject assumptions = payload.value("assumptions").toObject();

    // ── Per-command rendering ──

    if (command == "backtest") {
        auto perf = payload.value("performance").toObject();

        // ── Cost disclosure ──
        // Results are only achievable net of the costs that were actually sent
        // to the provider. State them next to the headline numbers so a reader
        // never mistakes a gross curve for a tradeable one. Providers now report
        // what they applied and ignored (assumptions); the hard-coded table below is
        // only the fallback for results that predate that block.
        if (!assumptions.isEmpty()) {
            const QString costs = assumptions.value("costs").toString();
            QStringList ignored;
            for (const auto& v : assumptions.value("ignoredSettings").toArray())
                ignored << v.toString();
            const bool gross = costs.startsWith(QLatin1String("none"));
            add_banner(tr("Costs: %1").arg(costs), gross ? "warn" : "info");
            if (!ignored.isEmpty())
                add_banner(tr("Not applied by this engine: %1.").arg(ignored.join(QStringLiteral("; "))), "warn");
        } else {
            const double comm = commission_spin_ ? commission_spin_->value() : 0.0;
            const double slip = slippage_spin_ ? slippage_spin_->value() : 0.0;
            const double sl_pct = stop_loss_spin_ ? stop_loss_spin_->value() : 0.0;
            const double tp_pct = take_profit_spin_ ? take_profit_spin_->value() : 0.0;

            // What each Python provider actually consumes (verified against the scripts):
            // vectorbt applies commission + slippage + SL/TP; zipline commission + slippage;
            // backtesting.py, BT and FastTrade commission only; the Fincept engine applies none
            // of them. Claiming "Costs applied" for a setting the provider drops would
            // overstate how conservative the run is.
            QString pname = slug;
            for (const auto& pr : providers_)
                if (pr.slug == slug)
                    pname = pr.display_name;
            const bool sup_comm = (slug == QLatin1String("vectorbt") || slug == QLatin1String("zipline") ||
                                   slug == QLatin1String("backtestingpy") || slug == QLatin1String("bt") ||
                                   slug == QLatin1String("fasttrade"));
            const bool sup_slip = (slug == QLatin1String("vectorbt") || slug == QLatin1String("zipline"));
            const bool sup_stops = (slug == QLatin1String("vectorbt"));

            const bool applied_comm = sup_comm && comm > 0.0;
            const bool applied_slip = sup_slip && slip > 0.0;
            const bool costless = !applied_comm && !applied_slip;

            QStringList ignored;
            if (!sup_comm && comm > 0.0)
                ignored << tr("commission");
            if (!sup_slip && slip > 0.0)
                ignored << tr("slippage");
            if (!sup_stops && (sl_pct > 0.0 || tp_pct > 0.0))
                ignored << tr("stop-loss/take-profit");
            const QString ignored_note = ignored.isEmpty()
                                             ? QString()
                                             : tr(" %1 does not apply your %2 setting.").arg(pname, ignored.join(", "));

            QString cost_text;
            if (costless) {
                cost_text = tr("NO COSTS MODELLED — no commission or slippage reached the engine. "
                               "These returns are gross and are not achievable.") +
                            ignored_note;
            } else {
                QStringList parts;
                if (applied_comm)
                    parts << tr("commission %1% per trade").arg(comm, 0, 'f', 3);
                if (applied_slip)
                    parts << tr("slippage %1% per fill").arg(slip, 0, 'f', 4);
                cost_text = tr("Costs applied: %1.").arg(parts.join(tr(", "))) + ignored_note;
            }
            add_banner(cost_text, (costless || !ignored.isEmpty()) ? "warn" : "info");
        }

        // ── No-trade guard ──
        // A run that never traded reports 0% drawdown / 0 volatility, which reads
        // like a flawless strategy on the summary cards. Say so explicitly.
        const int n_trades = perf.value("totalTrades").toInt(perf.value("total_trades").toInt(-1));
        if (n_trades == 0) {
            add_banner(tr("This strategy produced ZERO trades over the selected range. "
                          "Every performance figure below is meaningless — widen the date range, "
                          "loosen the entry rules, or check the symbol/interval."),
                       "error");
        }

        add_cards_from(perf, {"totalReturn", "annualizedReturn", "sharpeRatio", "sortinoRatio", "maxDrawdown",
                              "winRate", "profitFactor", "calmarRatio", "totalTrades", "volatility",
                              "positiveDayRate"});
        fill_kv_table(perf);
        auto trades = payload.value("trades").toArray();
        fill_details_table(trades, {"symbol", "entryDate", "exitDate", "side", "entryPrice", "exitPrice", "quantity",
                                    "pnl", "pnlPercent", "holdingPeriod", "exitReason", "commission"});
    } else if (command == "optimize") {
        QJsonObject summary{
            {"iterations", payload.value("iterations")},
            {"totalCombinations", payload.value("totalCombinations")},
            {"method", payload.value("method")},
            {"objective", payload.value("objective")},
            {"bestObjectiveValue", payload.value("bestObjectiveValue")},
        };
        add_cards_from(summary, {"bestObjectiveValue", "iterations", "totalCombinations", "method", "objective"});

        auto best_params = payload.value("bestParameters").toObject();
        if (!best_params.isEmpty()) {
            add_section_header(tr("BEST PARAMETERS"));
            add_cards_from(best_params, {}, 4);
        }
        auto best_perf = payload.value("bestPerformance").toObject();
        if (!best_perf.isEmpty()) {
            add_section_header(tr("BEST PERFORMANCE"));
            add_cards_from(best_perf,
                           {"totalReturn", "sharpeRatio", "maxDrawdown", "winRate", "profitFactor", "totalTrades"});
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
            // Providers emit camelCase keys (json_response); only the legacy shape is snake_case.
            row["objective"] = o.contains("objectiveValue") ? o.value("objectiveValue") : o.value("objective_value");
            auto perf = o.value("performance").toObject();
            for (const auto& k : QStringList{"totalReturn", "sharpeRatio", "maxDrawdown", "winRate"}) {
                if (perf.contains(k))
                    row[k] = perf.value(k);
            }
            flat.append(row);
        }
        fill_details_table(flat);
    } else if (command == "walk_forward") {
        QJsonObject summary{
            {"nWindows", payload.value("nWindows")},
            {"avgOosReturn", payload.value("avgOosReturn")},
            {"avgOosSharpe", payload.value("avgOosSharpe")},
            {"oosReturnStd", payload.value("oosReturnStd")},
            {"robustnessScore", payload.value("robustnessScore")},
            {"degradationRatio", payload.value("degradationRatio")},
        };
        add_cards_from(summary, {"nWindows", "avgOosReturn", "avgOosSharpe", "robustnessScore", "degradationRatio",
                                 "oosReturnStd"});
        fill_kv_table(summary);
        auto windows = payload.value("windows").toArray();
        fill_details_table(windows, {"window", "trainStart", "trainEnd", "testStart", "testEnd", "inSampleReturn",
                                     "inSampleSharpe", "outOfSampleReturn", "outOfSampleSharpe"});
    } else if (command == "indicator") {
        QJsonObject summary{
            {"indicator", payload.value("indicator")},
            {"symbol", payload.value("symbol")},
            {"period", payload.value("period")},
            {"totalBars", payload.value("totalBars")},
        };
        add_cards_from(summary, {"indicator", "symbol", "period", "totalBars"}, 4);
        auto stats = payload.value("stats").toObject();
        // Multi-output indicators have stats per component — flatten one level.
        if (!stats.isEmpty()) {
            QJsonObject flat;
            bool nested = false;
            for (auto it = stats.begin(); it != stats.end(); ++it) {
                if (it.value().isObject()) {
                    nested = true;
                    break;
                }
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
            add_section_header(tr("STATISTICS"));
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
    } else if (command == "indicator_signals") {
        add_cards_from(payload, {"totalSignals", "winRate", "profitFactor", "avgReturn", "medianReturn", "bestTrade",
                                 "worstTrade", "avgHoldingPeriod", "signalDensity", "entryCount", "exitCount",
                                 "originalEntryCount", "filteredEntryCount"});
        fill_kv_table(payload);
        // Combine entry & exit signals into a single details table with a Type column.
        auto entries = payload.value("entrySignals").toArray();
        auto exits = payload.value("exitSignals").toArray();
        QJsonArray combined;
        for (const auto& v : entries) {
            auto o = v.toObject();
            o["type"] = "ENTRY";
            combined.append(o);
        }
        for (const auto& v : exits) {
            auto o = v.toObject();
            o["type"] = "EXIT";
            combined.append(o);
        }
        if (combined.isEmpty()) {
            // Some modes only emit plain entries[]/exits[] string arrays
            QJsonArray plain;
            for (const auto& v : payload.value("entries").toArray()) {
                QJsonObject o;
                o["type"] = "ENTRY";
                o["date"] = v.toString();
                plain.append(o);
            }
            for (const auto& v : payload.value("exits").toArray()) {
                QJsonObject o;
                o["type"] = "EXIT";
                o["date"] = v.toString();
                plain.append(o);
            }
            combined = plain;
        }
        fill_details_table(combined);
    } else if (command == "labels") {
        QJsonObject summary{
            {"labelType", payload.value("labelType")},
            {"totalBars", payload.value("totalBars")},
            {"labeledBars", payload.value("labeledBars")},
        };
        add_cards_from(summary, {"labelType", "totalBars", "labeledBars"}, 3);

        auto dist = payload.value("distribution").toObject();
        if (!dist.isEmpty()) {
            add_section_header(tr("CLASS DISTRIBUTION"));
            // Re-key so cards show "Class -1 / Class 0 / Class 1"
            QJsonObject relabeled;
            QStringList ordered;
            QStringList raw_keys = dist.keys();
            std::sort(raw_keys.begin(), raw_keys.end(),
                      [](const QString& a, const QString& b) { return a.toInt() < b.toInt(); });
            for (const auto& k : raw_keys) {
                QString rk = tr("Class %1").arg(k);
                relabeled[rk] = dist.value(k);
                ordered.append(rk);
            }
            add_cards_from(relabeled, ordered, 5);
            fill_kv_table(relabeled);
        }
        fill_details_table(payload.value("sampleLabels").toArray(), {"date", "label"});
    } else if (command == "splits") {
        QJsonObject summary{
            {"splitterType", payload.value("splitterType")}, {"nSplits", payload.value("nSplits")},
            {"totalBars", payload.value("totalBars")},       {"indexStart", payload.value("indexStart")},
            {"indexEnd", payload.value("indexEnd")},
        };
        add_cards_from(summary, {"splitterType", "nSplits", "totalBars", "indexStart", "indexEnd"}, 3);
        fill_kv_table(summary);
        fill_details_table(payload.value("splits").toArray(),
                           {"fold", "trainStart", "trainEnd", "trainSize", "testStart", "testEnd", "testSize"});
    } else if (command == "returns") {
        QJsonObject summary{
            {"analysisType", payload.value("analysisType")},
            {"totalBars", payload.value("totalBars")},
            {"returnBars", payload.value("returnBars")},
        };
        add_cards_from(summary, {"analysisType", "totalBars", "returnBars"}, 3);

        // The interesting numbers live in payload.stats (returns_stats / drawdowns / ranges)
        // OR scattered at the top level for other shapes.
        auto stats = payload.value("stats").toObject();
        if (!stats.isEmpty()) {
            add_section_header(tr("STATISTICS"));
            add_cards_from(stats, {"Total Return", "Annualized Return", "Sharpe Ratio", "Sortino Ratio", "Calmar Ratio",
                                   "Max Drawdown", "Annualized Volatility", "Downside Risk"});
            fill_kv_table(stats);
        } else {
            // rolling-style payloads expose top-level fields
            QJsonObject topnum;
            for (const auto& k : QStringList{"metric", "window", "dataPoints", "maxDrawdown", "avgDrawdown",
                                             "totalDrawdowns", "activeDrawdown", "activeDuration", "totalRanges",
                                             "avgDuration", "maxDuration", "coverage"}) {
                if (payload.contains(k))
                    topnum[k] = payload.value(k);
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
                if (!a.isEmpty()) {
                    detail = a;
                    break;
                }
            }
        }
        fill_details_table(detail);
    } else if (command == "signals") {
        QJsonObject summary{
            {"generatorType", payload.value("generatorType")},
            {"totalBars", payload.value("totalBars")},
            {"entryCount", payload.value("entryCount")},
            {"exitCount", payload.value("exitCount")},
        };
        add_cards_from(summary, {"generatorType", "entryCount", "exitCount", "totalBars"}, 4);
        fill_kv_table(summary);

        QJsonArray combined;
        for (const auto& v : payload.value("entries").toArray()) {
            QJsonObject o;
            o["type"] = "ENTRY";
            o["date"] = v.toString();
            combined.append(o);
        }
        for (const auto& v : payload.value("exits").toArray()) {
            QJsonObject o;
            o["type"] = "EXIT";
            o["date"] = v.toString();
            combined.append(o);
        }
        fill_details_table(combined, {"type", "date"});
    } else if (command == "labels_to_signals") {
        QJsonObject summary{
            {"labelType", payload.value("labelType")}, {"entryLabel", payload.value("entryLabel")},
            {"exitLabel", payload.value("exitLabel")}, {"entryCount", payload.value("entryCount")},
            {"exitCount", payload.value("exitCount")}, {"totalBars", payload.value("totalBars")},
        };
        add_cards_from(summary, {"labelType", "entryCount", "exitCount", "totalBars", "entryLabel", "exitLabel"}, 3);
        fill_kv_table(summary);
        QJsonArray combined;
        for (const auto& v : payload.value("entries").toArray()) {
            QJsonObject o;
            o["type"] = "ENTRY";
            o["date"] = v.toString();
            combined.append(o);
        }
        for (const auto& v : payload.value("exits").toArray()) {
            QJsonObject o;
            o["type"] = "EXIT";
            o["date"] = v.toString();
            combined.append(o);
        }
        fill_details_table(combined, {"type", "date"});
    } else if (command == "indicator_sweep") {
        QJsonObject summary{
            {"indicator", payload.value("indicator")},
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
                if (o.contains(k))
                    row[k] = o.value(k);
            flat.append(row);
        }
        fill_details_table(flat);
    } else {
        // Unknown command — best-effort: show all scalar fields as cards.
        add_cards_from(payload, {});
        fill_kv_table(payload);
    }

    if (result_tabs_ && result_tabs_->count() > 3) {
        result_tabs_->setTabText(3, details_total > 0 ? tr("DETAILS (%1)").arg(QLocale().toString(details_total))
                                                      : tr("DETAILS"));
    }

    // ── How these numbers were produced (provider-reported) ──
    if (!assumptions.isEmpty()) {
        add_section_header(tr("ASSUMPTIONS"));
        QStringList lines;
        auto line = [&](const QString& label, const char* key) {
            const QJsonValue v = assumptions.value(QLatin1String(key));
            if (v.isString() && !v.toString().isEmpty())
                lines << QStringLiteral("%1: %2").arg(label, v.toString());
        };
        line(tr("ENGINE"), "engine");
        line(tr("EXECUTION"), "execution");
        line(tr("COSTS"), "costs");
        line(tr("POSITION SIZING"), "positionSizing");
        line(tr("DATA"), "dataSource");
        line(tr("ANNUALISATION"), "annualization");
        if (assumptions.contains("riskFreeRate"))
            lines << tr("RISK-FREE RATE: %1%").arg(assumptions.value("riskFreeRate").toDouble() * 100.0, 0, 'f', 2);
        line(tr("SURVIVORSHIP"), "survivorship");
        for (const auto& n : assumptions.value("notes").toArray())
            lines << tr("NOTE: %1").arg(n.toString());
        auto* box = new QLabel(lines.join(QStringLiteral("\n")), summary_container_);
        box->setObjectName("btAssumptions");
        box->setWordWrap(true);
        box->setTextInteractionFlags(Qt::TextSelectableByMouse);
        summary_layout_->addWidget(box);
    }

    summary_layout_->addStretch();

    // ── Equity Curve chart ──
    auto equity_arr = payload.value("equity").toArray();
    if (!equity_arr.isEmpty() && equity_chart_tab_ && equity_chart_tab_->layout()) {
        auto* eq_series = new QLineSeries;
        eq_series->setPen(QPen(accent_color, 1.5));
        auto* dd_series = new QLineSeries;
        dd_series->setPen(QPen(QColor(ui::colors::NEGATIVE()), 1.0));

        double y_min = 1e18, y_max = -1e18;
        double dd_min = 0;
        // Collect first and hand the series one list: append() per point re-emits
        // pointAdded for each of tens of thousands of bars.
        QList<QPointF> eq_points;
        QList<QPointF> dd_points;
        eq_points.reserve(equity_arr.size());
        dd_points.reserve(equity_arr.size());

        for (const auto& pt : equity_arr) {
            auto obj = pt.toObject();
            auto date_str = obj.value("date").toString();
            auto equity_val = obj.value("equity").toDouble();
            auto dd_val = obj.value("drawdown").toDouble();
            auto dt = QDateTime::fromString(date_str, "yyyy-MM-dd");
            if (!dt.isValid())
                dt = QDateTime::fromString(date_str, Qt::ISODate);
            // Intraday intervals emit "yyyy-MM-dd HH:mm:ss" (space separator, maybe a tz
            // suffix) — neither format above accepts that, so every point was dropped and the
            // equity chart stayed blank for 1h/5m runs.
            if (!dt.isValid())
                dt = QDateTime::fromString(date_str.left(19), "yyyy-MM-dd HH:mm:ss");
            if (!dt.isValid())
                dt = QDateTime::fromString(date_str.left(10), "yyyy-MM-dd");
            if (!dt.isValid())
                continue;
            const qreal ms = static_cast<qreal>(dt.toMSecsSinceEpoch());
            eq_points.append(QPointF(ms, equity_val));
            dd_points.append(QPointF(ms, dd_val * 100.0));
            y_min = qMin(y_min, equity_val);
            y_max = qMax(y_max, equity_val);
            dd_min = qMin(dd_min, dd_val * 100.0);
        }

        eq_series->replace(eq_points);
        dd_series->replace(dd_points);

        if (eq_series->count() > 1) {
            auto* chart = new QChart;
            chart->addSeries(eq_series);
            chart->legend()->setVisible(false);
            chart->setMargins(QMargins(0, 0, 0, 0));
            chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
            chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_SURFACE())));
            chart->setPlotAreaBackgroundVisible(true);

            auto* x_axis = new QDateTimeAxis;
            // A "MMM yyyy" label is useless on an intraday run (every tick reads the same).
            const qint64 span_ms = static_cast<qint64>(eq_series->at(eq_series->count() - 1).x() - eq_series->at(0).x());
            x_axis->setFormat(span_ms < 30LL * 86400000LL ? "dd MMM HH:mm" : "MMM yyyy");
            x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
            x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
            chart->addAxis(x_axis, Qt::AlignBottom);
            eq_series->attachAxis(x_axis);

            auto* y_axis = new QValueAxis;
            double margin = (y_max - y_min) * 0.05;
            y_axis->setRange(y_min - margin, y_max + margin);
            y_axis->setLabelFormat(cur::symbol() + "%.0f");
            y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
            y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
            chart->addAxis(y_axis, Qt::AlignLeft);
            eq_series->attachAxis(y_axis);

            // Drawdown on right axis
            if (dd_min < -0.01) {
                chart->addSeries(dd_series);
                auto* dd_axis = new QValueAxis;
                dd_axis->setRange(dd_min * 1.2, 0);
                dd_axis->setLabelFormat("%.1f%%");
                dd_axis->setLabelsColor(QColor(ui::colors::NEGATIVE()));
                dd_axis->setGridLineVisible(false);
                chart->addAxis(dd_axis, Qt::AlignRight);
                dd_series->attachAxis(x_axis);
                dd_series->attachAxis(dd_axis);
            } else {
                delete dd_series;
            }

            auto* view = new QChartView(chart, equity_chart_tab_);
            view->setRenderHint(QPainter::Antialiasing);
            equity_chart_tab_->layout()->addWidget(view);
        } else {
            delete eq_series;
            delete dd_series;
        }
    }

    // ── RAW JSON tab ──
    // A 100k-trade payload is tens of MB of indented text; laying that out in a
    // QTextEdit froze the UI. Long arrays are previewed (first 500 items, marked);
    // EXPORT JSON always writes the complete payload.
    {
        constexpr int kPreviewItems = 500;
        QJsonObject preview = payload;
        bool truncated = false;
        for (auto it = payload.begin(); it != payload.end(); ++it) {
            if (!it.value().isArray())
                continue;
            const QJsonArray arr = it.value().toArray();
            if (arr.size() <= kPreviewItems)
                continue;
            QJsonArray head;
            for (int i = 0; i < kPreviewItems; ++i)
                head.append(arr[i]);
            preview[it.key()] = head;
            preview[it.key() + QStringLiteral("__preview")] =
                tr("showing %1 of %2 items - EXPORT JSON writes all").arg(kPreviewItems).arg(arr.size());
            truncated = true;
        }
        raw_json_edit_->setPlainText(QString::fromUtf8(QJsonDocument(truncated ? preview : payload).toJson(
            QJsonDocument::Indented)));
    }

    // Switch to summary tab
    result_tabs_->setCurrentIndex(0);
}

void BacktestingScreen::display_error(const QString& msg) {
    clear_results();
    last_payload_ = QJsonObject();

    auto* err = new QLabel(msg, summary_container_);
    err->setObjectName("btBanner");
    err->setProperty("tone", QStringLiteral("error"));
    err->setWordWrap(true);
    err->setTextInteractionFlags(Qt::TextSelectableByMouse);
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
        if (strategies_count_label_)
            strategies_count_label_->setText(QString::number(strategies_.size()));
        LOG_INFO("Backtesting", QString("[%1] Loaded %2 strategies").arg(provider).arg(strategies_.size()));
        // A backtest auto-run was requested (e.g. from Equity Research) before
        // strategies were ready — now that a strategy is selected, fire it.
        if (pending_auto_run_) {
            pending_auto_run_ = false;
            trigger_auto_run();
        }
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

    // Run results arrive via on_run_completed (correlated by run id).
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
    // Runs report through on_run_failed (correlated by run id). This slot only
    // handles the background catalogue loads: "load_strategies/<provider>" and
    // "<provider>/get_*". Anything else is another consumer's run.
    const bool catalogue_ctx = context.startsWith(QLatin1String("load_strategies/")) ||
                               context.endsWith(QLatin1String("/get_indicators")) ||
                               context.endsWith(QLatin1String("/get_strategies")) ||
                               context.endsWith(QLatin1String("/get_command_options"));
    if (!catalogue_ctx)
        return;
    if (is_running_) {
        // A catalogue load failing mid-run must not reset the run or wipe its result.
        LOG_WARN("Backtesting", QString("[%1] background load failed during a run: %2").arg(context, message));
        return;
    }
    set_status_state(tr("ERROR"), QStringLiteral("error"));
    display_error(tr("Could not load the provider catalogue [%1]: %2").arg(context, message));
}

void BacktestingScreen::on_run_completed(quint64 run_id, const QString& provider, const QString& command,
                                         const QJsonObject& payload) {
    if (run_id == 0 || run_id != current_run_id_)
        return; // another screen's run, a catalogue call, or a run we already cancelled
    end_run();
    last_payload_ = payload;
    last_command_ = command;
    if (payload.value("usingSyntheticData").toBool())
        set_status_state(tr("DONE · SYNTHETIC DATA"), QStringLiteral("synthetic"));
    else
        set_status_state(tr("READY"), QStringLiteral("done"));
    display_result(command, payload);
    LOG_INFO("Backtesting", QString("[%1/%2] Complete (run %3)").arg(provider, command).arg(run_id));
}

void BacktestingScreen::on_run_failed(quint64 run_id, const QString& context, const QString& message) {
    if (run_id == 0 || run_id != current_run_id_)
        return;
    end_run();
    set_status_state(tr("ERROR"), QStringLiteral("error"));
    display_error(QString("[%1] %2").arg(context, message));
}

void BacktestingScreen::on_run_cancelled(quint64 run_id, const QString& provider, const QString& command) {
    if (run_id == 0 || run_id != current_run_id_)
        return;
    const qint64 secs = run_elapsed_.isValid() ? run_elapsed_.elapsed() / 1000 : 0;
    end_run();
    set_status_state(tr("CANCELLED"), QStringLiteral("cancelled"));
    display_error(tr("%1 / %2 cancelled after %3 s. Nothing from this run will be shown.\n\n"
                     "Optimisation and walk-forward loops stop at their next iteration. A single long "
                     "backtest step cannot be interrupted: its Python process finishes in the background "
                     "(its result is discarded) and occupies one of the shared Python slots until then.")
                      .arg(provider, command)
                      .arg(secs));
    LOG_INFO("Backtesting", QString("[%1/%2] run %3 cancelled after %4s").arg(provider, command).arg(run_id).arg(secs));
}

// ── Exports ──────────────────────────────────────────────────────────────────

namespace {

QString bt_csv_cell(const QJsonValue& v) {
    QString s;
    if (v.isDouble())
        s = QString::number(v.toDouble(), 'g', 15);
    else if (v.isBool())
        s = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    else if (v.isString())
        s = v.toString();
    else if (v.isNull() || v.isUndefined())
        return {};
    else
        s = QString::fromUtf8((v.isArray() ? QJsonDocument(v.toArray()) : QJsonDocument(v.toObject()))
                                  .toJson(QJsonDocument::Compact));
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
        s.replace(QLatin1String("\""), QLatin1String("\"\""));
        s = QLatin1Char('"') + s + QLatin1Char('"');
    }
    return s;
}

} // namespace

QStringList BacktestingScreen::export_header_lines() const {
    QStringList h;
    const bool synthetic = last_payload_.value("usingSyntheticData").toBool();
    if (synthetic)
        h << QStringLiteral("# WARNING: SYNTHETIC DATA - prices were randomly generated; these results have NO market "
                            "meaning");
    h << QStringLiteral("# Fincept Terminal backtest export, generated %1")
             .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    QStringList syms;
    for (const auto& v : last_run_meta_.value("symbols").toArray())
        syms << v.toString();
    if (!last_run_meta_.isEmpty()) {
        h << QStringLiteral("# provider: %1 | command: %2 | strategy: %3")
                 .arg(last_run_meta_.value("provider").toString(), last_run_meta_.value("command").toString(),
                      last_run_meta_.value("strategy").toString());
        h << QStringLiteral("# symbols: %1 | period: %2 to %3 | interval: %4")
                 .arg(syms.join(QLatin1Char(' ')), last_run_meta_.value("startDate").toString(),
                      last_run_meta_.value("endDate").toString(), last_run_meta_.value("interval").toString());
    }
    const QJsonObject a = last_payload_.value("assumptions").toObject();
    for (const char* key : {"engine", "execution", "costs", "positionSizing", "dataSource", "annualization"}) {
        const QString v = a.value(QLatin1String(key)).toString();
        if (!v.isEmpty())
            h << QStringLiteral("# %1: %2").arg(QLatin1String(key), QString(v).replace(QLatin1Char('\n'), QLatin1Char(' ')));
    }
    return h;
}

bool BacktestingScreen::write_text_file(const QString& path, const QString& text, const QString& title) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR("Backtesting", "Export failed for " + path + ": " + f.errorString());
        QMessageBox::warning(this, title, tr("Could not write %1:\n%2").arg(path, f.errorString()));
        return false;
    }
    f.write(text.toUtf8());
    f.close();
    services::FileManagerService::instance().import_file(path, "backtesting");
    LOG_INFO("Backtesting", "Exported results to: " + path);
    return true;
}

void BacktestingScreen::export_details_csv() {
    const auto& rows = details_model_->rows();
    if (rows.isEmpty() || details_cols_.isEmpty()) {
        QMessageBox::information(this, tr("Export CSV"), tr("There are no detail rows to export."));
        return;
    }
    const bool synthetic = last_payload_.value("usingSyntheticData").toBool();
    const QString base = last_command_.isEmpty() ? QStringLiteral("backtest") : last_command_;
    const QString suggested =
        QStringLiteral("%1_details%2.csv").arg(base, synthetic ? QStringLiteral("_SYNTHETIC") : QString());
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Export Details CSV"), suggested, tr("CSV Files (*.csv);;All Files (*)"));
    if (path.isEmpty())
        return;

    QString out = export_header_lines().join(QLatin1Char('\n')) + QLatin1Char('\n');
    out.reserve(out.size() + rows.size() * details_cols_.size() * 12);
    QStringList header;
    for (const auto& c : details_cols_)
        header << bt_csv_cell(QJsonValue(c));
    out += header.join(QLatin1Char(',')) + QLatin1Char('\n');
    // Provider order and raw values (full precision) - not the view's sort or formatting.
    for (const auto& r : rows) {
        QStringList cells;
        cells.reserve(details_cols_.size());
        for (const auto& c : details_cols_)
            cells << bt_csv_cell(r.rec.value(c));
        out += cells.join(QLatin1Char(',')) + QLatin1Char('\n');
    }
    write_text_file(path, out, tr("Export Details CSV"));
}

void BacktestingScreen::export_equity_csv() {
    const QJsonArray eq = last_payload_.value("equity").toArray();
    if (eq.isEmpty()) {
        QMessageBox::information(this, tr("Export CSV"), tr("This result has no equity curve to export."));
        return;
    }
    const bool synthetic = last_payload_.value("usingSyntheticData").toBool();
    const QString suggested =
        QStringLiteral("equity_curve%1.csv").arg(synthetic ? QStringLiteral("_SYNTHETIC") : QString());
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Export Equity Curve CSV"), suggested, tr("CSV Files (*.csv);;All Files (*)"));
    if (path.isEmpty())
        return;

    QString out = export_header_lines().join(QLatin1Char('\n')) + QLatin1Char('\n');
    out += QStringLiteral("date,equity,drawdown,returns,benchmark\n");
    for (const auto& v : eq) {
        const QJsonObject p = v.toObject();
        out += QStringList{bt_csv_cell(p.value("date")), bt_csv_cell(p.value("equity")),
                           bt_csv_cell(p.value("drawdown")), bt_csv_cell(p.value("returns")),
                           bt_csv_cell(p.value("benchmark"))}
                   .join(QLatin1Char(',')) +
               QLatin1Char('\n');
    }
    write_text_file(path, out, tr("Export Equity Curve CSV"));
}

} // namespace fincept::screens
