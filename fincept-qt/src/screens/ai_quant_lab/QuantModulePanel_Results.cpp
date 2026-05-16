// src/screens/ai_quant_lab/QuantModulePanel_Results.cpp
//
// Result rendering — the generic display_result() and the big on_result()
// dispatcher (rl_trading / online_learning / meta_learning / factor_discovery
// / model_library / live_signals / cfa_quant / gs_quant / functime / statsmodels
// / fortitudo / quant_reporting / gluonts / advanced_* / langgraph / rd_agent
// / backtesting / hft / rolling_retraining).
//
// Part of the partial-class split of QuantModulePanel.cpp.

#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QDesktopServices>
#include <QLineSeries>
#include <QValueAxis>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QFrame>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>

#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

void QuantModulePanel::display_result(const QJsonObject& payload) {
    clear_results();

    auto* header = new QLabel("RESULTS");
    header->setStyleSheet(QString("color:%1; font-weight:700; font-family:%2; letter-spacing:1px;")
                              .arg(module_.color.name())
                              .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // Key-value table for scalar fields
    QStringList keys;
    for (auto it = payload.begin(); it != payload.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            keys.append(it.key());

    if (!keys.isEmpty()) {
        auto* table = new QTableWidget(keys.size(), 2);
        table->setHorizontalHeaderLabels({"Metric", "Value"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setMaximumHeight(qMin(keys.size() * 30 + 30, 400));
        table->setStyleSheet(table_ss());

        for (int r = 0; r < keys.size(); ++r) {
            auto label = keys[r];
            label.replace('_', ' ');
            table->setItem(r, 0, new QTableWidgetItem(label.toUpper()));
            auto* vi = new QTableWidgetItem(format_val(payload[keys[r]]));
            vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, 1, vi);
        }
        table->resizeColumnsToContents();
        results_layout_->addWidget(table);
    }

    // Raw JSON
    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(200);
    raw->setPlainText(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    raw->setStyleSheet(output_ss());
    results_layout_->addWidget(raw);

    // Export button — saves raw JSON to File Manager
    auto* export_btn = new QPushButton("EXPORT RESULTS");
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setFixedHeight(28);
    export_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                                      "font-size:%3px; font-family:%4; padding:0 12px; }"
                                      "QPushButton:hover { color:%1; background:rgba(255,255,255,0.05); }")
                                  .arg(module_.color.name(), ui::colors::BORDER_DIM())
                                  .arg(ui::fonts::SMALL)
                                  .arg(ui::fonts::DATA_FAMILY));
    QString json_str = QJsonDocument(payload).toJson(QJsonDocument::Indented);
    connect(export_btn, &QPushButton::clicked, this, [this, json_str]() {
        QString safe_name = module_.label;
        safe_name.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString stored_name = safe_name + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json";
        QString dest = services::FileManagerService::instance().storage_dir() + "/" + stored_name;

        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(json_str.toUtf8());
            f.close();
            services::FileManagerService::instance().register_file(stored_name, module_.label + "_result.json",
                                                                   QFileInfo(dest).size(), "application/json",
                                                                   "ai_quant_lab");
            LOG_INFO("AIQuantLab", "Exported result: " + stored_name);
        }
    });
    results_layout_->addWidget(export_btn);

    status_label_->setText("Done");
}

// ── Signal handlers ──────────────────────────────────────────────────────────

void QuantModulePanel::on_result(const QString& module_id, const QString& command, const QJsonObject& payload) {
    if (module_id != module_.id)
        return;

    // RL Trading: training finished → re-enable button, keep progress/logs visible so
    // the user can inspect the run. The card-based renderer below paints into
    // the bottom results pane (in addition to the bespoke log/progress widgets).
    if (module_id == "rl_trading" && command == "train" && rl_train_button_) {
        rl_train_button_->setEnabled(true);
        if (rl_progress_bar_)
            rl_progress_bar_->setValue(100);
    }
    if (module_id == "rl_trading") {
        display_rl_trading_result(command, payload);
        return;
    }
    if (module_id == "online_learning") {
        display_online_learning_result(command, payload);
        return;
    }
    if (module_id == "meta_learning") {
        display_meta_learning_result(command, payload);
        return;
    }
    // CORE-category renderers (backtesting handled later via display_backtest_result)
    if (module_id == "factor_discovery") {
        display_factor_discovery_result(command, payload);
        return;
    }
    if (module_id == "model_library") {
        display_model_library_result(command, payload);
        return;
    }
    if (module_id == "live_signals") {
        display_live_signals_result(command, payload);
        return;
    }

    // ── CFA Quant: uniform result shape across all analyses ──────────────────
    if (module_id == "cfa_quant") {
        display_cfa_result(command, payload);
        return;
    }

    // ── GS Quant: rich card-based display per sub-tab ────────────────────────
    if (module_id == "gs_quant") {
        display_gs_result(command, payload);
        return;
    }

    // ── Functime: rich card-based display per sub-tab ────────────────────────
    if (module_id == "functime") {
        display_functime_result(command, payload);
        return;
    }

    // ── Statsmodels: rich card-based display per sub-tab ─────────────────────
    if (module_id == "statsmodels") {
        display_statsmodels_result(command, payload);
        return;
    }

    // ── Fortitudo: rich card-based display per sub-tab ───────────────────────
    if (module_id == "fortitudo") {
        display_fortitudo_result(command, payload);
        return;
    }

    // ── Quant Reporting: rich card-based display per sub-tab ─────────────────
    if (module_id == "quant_reporting") {
        display_quant_reporting_result(command, payload);
        return;
    }

    // ── GluonTS: rich card-based display per sub-tab ─────────────────────────
    if (module_id == "gluonts") {
        display_gluonts_result(command, payload);
        return;
    }

    // ── ADVANCED-category panels: card-based displays ────────────────────────
    // hft and rolling_retraining are intentionally NOT dispatched here — they
    // have bespoke per-tab UI (in-tab card grid + order-book tables for HFT;
    // progress bar + schedule cards for Rolling Retraining) handled later in
    // this function. Re-routing them to the generic results pane would create
    // a duplicate, disconnected display.
    if (module_id == "advanced_models") {
        display_advanced_models_result(command, payload);
        return;
    }
    if (module_id == "feature_engineering") {
        display_feature_engineering_result(command, payload);
        return;
    }
    if (module_id == "portfolio_opt") {
        display_portfolio_opt_result(command, payload);
        return;
    }
    if (module_id == "factor_evaluation") {
        display_factor_evaluation_result(command, payload);
        return;
    }
    if (module_id == "strategy_builder") {
        display_strategy_builder_result(command, payload);
        return;
    }
    if (module_id == "data_processors") {
        display_data_processors_result(command, payload);
        return;
    }

    // ── LangGraph deep analysis result ───────────────────────────────────────
    if (command == "execute_task") {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString("Unknown error"));
            return;
        }
        auto text = payload["result"].toString();
        if (text.isEmpty())
            text = QJsonDocument(payload).toJson(QJsonDocument::Indented);
        if (agent_output_)
            agent_output_->setPlainText(text);
        auto tid = payload["thread_id"].toString();
        status_label_->setText(tid.isEmpty() ? "Done" : QString("Done — thread: %1").arg(tid));
        return;
    }

    // ── RD-Agent: check_status ────────────────────────────────────────────────
    if (command == "check_status") {
        auto ver = payload["rdagent_version"].toString("?");
        auto avail = payload["availability"].toObject();
        QStringList flags;
        for (auto it = avail.begin(); it != avail.end(); ++it)
            if (it.value().toBool())
                flags << it.key();
        status_label_->setText(QString("rdagent %1 — %2").arg(ver, flags.join(", ")));
        return;
    }

    // ── RD-Agent: task started (factor_mining / model_optimization / quant_research) ──
    if (command == "start_factor_mining" || command == "start_model_optimization" ||
        command == "start_quant_research") {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString("Unknown error"));
            return;
        }
        auto task_id = payload["task_id"].toString();
        auto est = payload["estimated_time"].toString();
        status_label_->setText(QString("Task %1 started").arg(task_id));
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(
                QString("Task started: %1\nEstimated time: %2\n\nUse Task Monitor → REFRESH to track progress.")
                    .arg(task_id, est));
        return;
    }

    // ── RD-Agent: list_tasks ──────────────────────────────────────────────────
    if (command == "list_tasks" && rd_task_table_) {
        auto tasks = payload["tasks"].toArray();
        rd_task_table_->setRowCount(0);
        for (const auto& t : tasks) {
            auto obj = t.toObject();
            int row = rd_task_table_->rowCount();
            rd_task_table_->insertRow(row);
            rd_task_table_->setItem(row, 0, new QTableWidgetItem(obj["task_id"].toString()));
            rd_task_table_->setItem(row, 1, new QTableWidgetItem(obj["type"].toString()));
            auto status_str = obj["status"].toString();
            auto* status_item = new QTableWidgetItem(status_str);
            if (status_str == "running")
                status_item->setForeground(QColor("" + QString(ui::colors::WARNING()) + ""));
            else if (status_str == "completed")
                status_item->setForeground(QColor("" + QString(ui::colors::POSITIVE()) + ""));
            else if (status_str == "failed" || status_str == "stopped")
                status_item->setForeground(QColor("" + QString(ui::colors::NEGATIVE()) + ""));
            rd_task_table_->setItem(row, 2, status_item);
            rd_task_table_->setItem(
                row, 3, new QTableWidgetItem(QString::number(obj["progress"].toDouble() * 100, 'f', 0) + "%"));
            auto ic = obj["best_ic"];
            rd_task_table_->setItem(row, 4,
                                    new QTableWidgetItem(ic.isNull() ? "-" : QString::number(ic.toDouble(), 'f', 4)));
            rd_task_table_->setItem(row, 5, new QTableWidgetItem(obj["elapsed_time"].toString("-")));
        }
        status_label_->setText(QString("%1 task(s)").arg(tasks.size()));
        return;
    }

    // ── RD-Agent: get_task_status ─────────────────────────────────────────────
    if (command == "get_task_status") {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString());
            return;
        }
        auto task_id = payload["task_id"].toString();
        auto progress = payload["progress"].toDouble() * 100;
        auto step = payload["current_step"].toString();
        auto ic = payload["best_ic"];
        status_label_->setText(QString("Task %1 — %2% — %3").arg(task_id).arg(progress, 0, 'f', 0).arg(step));
        if (rd_agent_output_) {
            rd_agent_output_->setPlainText(
                QString(
                    "Task ID:    %1\nStatus:     %2\nProgress:   %3%\nStep:       %4\nBest IC:    %5\nElapsed:    %6")
                    .arg(task_id, payload["status"].toString())
                    .arg(progress, 0, 'f', 0)
                    .arg(step)
                    .arg(ic.isNull() ? "N/A" : QString::number(ic.toDouble(), 'f', 4))
                    .arg(payload["elapsed_time"].toString("-")));
        }
        return;
    }

    // ── RD-Agent: get_discovered_factors ──────────────────────────────────────
    if (command == "get_discovered_factors" && rd_agent_output_) {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString());
            return;
        }
        auto factors = payload["factors"].toArray();
        auto best_ic = payload["best_ic"];
        QString out = QString("Discovered Factors: %1  |  Best IC: %2\n\n")
                          .arg(factors.size())
                          .arg(best_ic.isNull() ? "N/A" : QString::number(best_ic.toDouble(), 'f', 4));
        int idx = 1;
        for (const auto& f : factors) {
            auto obj = f.toObject();
            out += QString("[%1] %2\n  IC: %3  Sharpe: %4\n  %5\n\n")
                       .arg(idx++)
                       .arg(obj["name"].toString("Factor"))
                       .arg(obj["ic"].isNull() ? "N/A" : QString::number(obj["ic"].toDouble(), 'f', 4))
                       .arg(obj["sharpe"].isNull() ? "N/A" : QString::number(obj["sharpe"].toDouble(), 'f', 3))
                       .arg(obj["description"].toString());
        }
        rd_agent_output_->setPlainText(out);
        status_label_->setText(QString("Found %1 factor(s)").arg(factors.size()));
        return;
    }

    // ── RD-Agent: get_optimized_model ─────────────────────────────────────────
    if (command == "get_optimized_model" && rd_agent_output_) {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString());
            return;
        }
        auto models = payload["models"].toArray();
        QString out = QString("Optimized Models: %1\n\n").arg(models.size());
        int idx = 1;
        for (const auto& m : models) {
            auto obj = m.toObject();
            out += QString("[%1] %2\n  Sharpe: %3  IC: %4\n")
                       .arg(idx++)
                       .arg(obj["model_id"].toString("Model"))
                       .arg(obj["sharpe"].isNull() ? "N/A" : QString::number(obj["sharpe"].toDouble(), 'f', 3))
                       .arg(obj["ic"].isNull() ? "N/A" : QString::number(obj["ic"].toDouble(), 'f', 4));
        }
        rd_agent_output_->setPlainText(out);
        return;
    }

    // ── RD-Agent: stop_task / resume_task ────────────────────────────────────
    if (command == "stop_task" || command == "resume_task") {
        status_label_->setText(payload["message"].toString(command));
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(payload["message"].toString());
        return;
    }

    // ── RD-Agent: start_ui ───────────────────────────────────────────────────
    if (command == "start_ui") {
        auto url = payload["url"].toString();
        if (!url.isEmpty()) {
            status_label_->setText(QString("Log viewer: %1").arg(url));
            QDesktopServices::openUrl(QUrl(url));
        }
        return;
    }

    // ── RD-Agent: start_mcp_server ────────────────────────────────────────────
    if (command == "start_mcp_server") {
        if (!payload["success"].toBool()) {
            status_label_->setText("MCP server failed");
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(
                    QString("MCP server failed to start:\n%1\n\nInstall: %2")
                        .arg(payload["error"].toString())
                        .arg(payload["install"].toString("pip install 'mcp[cli]' 'pydantic-ai[mcp]' yfinance")));
            return;
        }
        auto url = payload["url"].toString();
        auto port = payload["port"].toInt();
        auto tools = payload["tools"].toArray();
        QStringList tool_names;
        for (const auto& t : tools)
            tool_names << t.toString();
        status_label_->setText(QString("MCP ready on port %1").arg(port));
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(QString("MCP tool server running at %1\n\nAvailable tools:\n  %2\n\n"
                                                   "Enable 'enable_mcp: true' in factor/model/quant research params\n"
                                                   "to give RD-Agent loops access to these tools.")
                                               .arg(url, tool_names.join("\n  ")));
        return;
    }

    // ── RD-Agent: mcp_status ─────────────────────────────────────────────────
    if (command == "mcp_status") {
        auto avail = payload["mcp_server_available"].toBool();
        auto pai_mcp = payload["pydantic_ai_mcp"].toBool();
        auto ports = payload["running_ports"].toArray();
        QString info = QString("MCP server available: %1\npydantic-ai MCP: %2\nRunning ports: %3")
                           .arg(avail ? "yes" : "no")
                           .arg(pai_mcp ? "yes" : "no")
                           .arg(ports.isEmpty() ? "none" : [&]() {
                               QStringList ps;
                               for (const auto& p : ports)
                                   ps << QString::number(p.toInt());
                               return ps.join(", ");
                           }());
        status_label_->setText(avail ? "MCP available" : "MCP not installed");
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(info);
        return;
    }

    // ── Backtesting ───────────────────────────────────────────────────────────
    if (module_id == "backtesting") {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString("Unknown error"));
            return;
        }
        if (command == "run_backtest") {
            display_backtest_result(payload);
            return;
        }
        display_result(payload);
        return;
    }

    // factor_discovery, model_library, live_signals are now rendered by the
    // card-based dispatch at the top of this function (display_*_result in
    // QuantModulePanel_CoreDisplays.cpp).
    //
    // online_learning and meta_learning are also rendered by card-based
    // dispatches at the top (display_*_result in
    // QuantModulePanel_AIMLDisplays.cpp).

    // ── HFT ──────────────────────────────────────────────────────────────────
    if (module_id == "hft") {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString());
            return;
        }

        // Helper: find a named child label in this panel and set its text
        auto set_card = [this](const QString& name, const QString& text, const QString& color = {}) {
            auto* lbl = this->findChild<QLabel*>(name);
            if (!lbl) return;
            lbl->setText(text);
            if (!color.isEmpty())
                lbl->setStyleSheet(
                    QString("color:%1; font-size:13px; font-weight:700; font-family:'Courier New'; background:transparent;")
                        .arg(color));
        };

        // Helper: populate a bid or ask QTableWidget from a JSON array [[price,size],...]
        auto fill_book_table = [this](const QString& name, const QJsonArray& levels, bool is_bid) {
            auto* tbl = this->findChild<QTableWidget*>(name);
            if (!tbl) return;
            const int n = qMin(levels.size(), 10);
            tbl->setRowCount(n);
            double cumulative = 0.0;
            const QString price_col = is_bid ? QString(ui::colors::POSITIVE()) : QString(ui::colors::NEGATIVE());
            for (int i = 0; i < n; ++i) {
                const auto row = levels[i].toArray();
                const double price = row.size() > 0 ? row[0].toDouble() : 0.0;
                const double size  = row.size() > 1 ? row[1].toDouble() : 0.0;
                cumulative += size;
                auto* pi = new QTableWidgetItem(QString::number(price, 'f', 4));
                pi->setForeground(QColor(price_col));
                auto* si = new QTableWidgetItem(QString::number(size, 'g', 5));
                si->setForeground(QColor(QString(ui::colors::TEXT_PRIMARY())));
                auto* ci = new QTableWidgetItem(QString::number(cumulative, 'g', 6));
                ci->setForeground(QColor(QString(ui::colors::TEXT_TERTIARY())));
                tbl->setItem(i, 0, pi);
                tbl->setItem(i, 1, si);
                tbl->setItem(i, 2, ci);
                tbl->setRowHeight(i, 22);
            }
        };

        // ── fetch_orderbook ────────────────────────────────────────────────
        if (command == "fetch_orderbook") {
            const auto bids      = payload["bids"].toArray();
            const auto asks      = payload["asks"].toArray();
            const double mid     = payload["mid_price"].toDouble();
            const double spread_bps = payload["spread_bps"].toDouble();
            const double obi     = payload["obi"].toDouble();
            const QString pres   = payload["pressure"].toString();
            const double wmid    = payload["weighted_mid"].toDouble();
            const double lat     = payload["latency_ms"].toDouble();

            // Update latency badge
            if (auto* lbl = this->findChild<QLabel*>("hftLatency"))
                lbl->setText(QString("LATENCY  %1 ms").arg(lat, 0, 'f', 1));

            // Metric cards
            set_card("hft_mid_val",      QString::number(mid, 'f', 4));
            set_card("hft_spread_val",   QString::number(spread_bps, 'f', 3) + " bps");
            const QString obi_color = obi > 0.1 ? QString(ui::colors::POSITIVE())
                                    : obi < -0.1 ? QString(ui::colors::NEGATIVE())
                                    : QString(ui::colors::TEXT_PRIMARY());
            set_card("hft_obi_val",      QString::number(obi, 'f', 4), obi_color);
            const QString pres_color = pres == "BUY" ? QString(ui::colors::POSITIVE())
                                     : pres == "SELL" ? QString(ui::colors::NEGATIVE())
                                     : QString(ui::colors::TEXT_PRIMARY());
            set_card("hft_pressure_val", pres, pres_color);
            set_card("hft_wmid_val",     QString::number(wmid, 'f', 4));

            fill_book_table("hft_bid_table", bids, true);
            fill_book_table("hft_ask_table", asks, false);

            status_label_->setText(
                QString("%1  mid: %2  spread: %3 bps  OBI: %4")
                    .arg(payload["symbol"].toString())
                    .arg(mid, 0, 'f', 4)
                    .arg(spread_bps, 0, 'f', 3)
                    .arg(obi, 0, 'f', 4));
            return;
        }

        // ── market_making ─────────────────────────────────────────────────
        if (command == "market_making") {
            const auto mm = payload["market_making"].toObject();
            // Also update book metrics if present
            if (payload.contains("book_metrics")) {
                const auto bm = payload["book_metrics"].toObject();
                set_card("hft_mid_val",    QString::number(bm["mid_price"].toDouble(), 'f', 4));
                set_card("hft_spread_val", QString::number(bm["spread_bps"].toDouble(), 'f', 3) + " bps");
            }
            set_card("hft_mm_bid",     QString::number(mm["bid_price"].toDouble(), 'f', 4),
                     QString(ui::colors::POSITIVE()));
            set_card("hft_mm_ask",     QString::number(mm["ask_price"].toDouble(), 'f', 4),
                     QString(ui::colors::NEGATIVE()));
            set_card("hft_mm_qspread", QString::number(mm["quoted_spread_bps"].toDouble(), 'f', 3) + " bps");
            set_card("hft_mm_edge",    QString::number(mm["edge_per_side_bps"].toDouble(), 'f', 3) + " bps");
            const QString rec   = mm["recommendation"].toString();
            const QString r_col = rec == "WIDEN" ? QString(ui::colors::WARNING())
                                : rec == "TIGHTEN" ? QString(ui::colors::POSITIVE())
                                : QString(ui::colors::TEXT_PRIMARY());
            set_card("hft_mm_rec", rec, r_col);
            status_label_->setText(
                QString("Bid: %1  Ask: %2  Edge: %3 bps/side")
                    .arg(mm["bid_price"].toDouble(), 0, 'f', 4)
                    .arg(mm["ask_price"].toDouble(), 0, 'f', 4)
                    .arg(mm["edge_per_side_bps"].toDouble(), 0, 'f', 3));
            return;
        }

        // ── toxic_flow ────────────────────────────────────────────────────
        if (command == "toxic_flow") {
            const auto tf = payload["toxic_flow"].toObject();
            const bool   is_toxic = tf["is_toxic"].toBool();
            const double score    = tf["toxicity_score"].toDouble();
            const QString cls     = tf["classification"].toString();
            const QString action  = tf["action"].toString();

            const QString score_col = score > 60 ? QString(ui::colors::NEGATIVE())
                                    : score > 30 ? QString(ui::colors::WARNING())
                                    : QString(ui::colors::POSITIVE());
            set_card("hft_tox_pin",    QString::number(score, 'f', 1), score_col);
            set_card("hft_tox_vol",    QString::number(tf["vol_imbalance"].toDouble(), 'f', 4));
            set_card("hft_tox_impact", QString::number(tf["price_impact_bps"].toDouble(), 'f', 2) + " bps");
            const QString cls_col = cls == "HIGH_RISK" ? QString(ui::colors::NEGATIVE())
                                  : cls == "ELEVATED"  ? QString(ui::colors::WARNING())
                                  : QString(ui::colors::POSITIVE());
            set_card("hft_tox_class",  cls, cls_col);
            const QString act_col = action == "WIDEN_SPREADS" || action == "STEP_BACK"
                                        ? QString(ui::colors::WARNING())
                                        : QString(ui::colors::POSITIVE());
            set_card("hft_tox_action", QString(action).replace('_', ' '), act_col);
            status_label_->setText(
                is_toxic ? QString("TOXIC FLOW DETECTED — score: %1 — %2").arg(score, 0, 'f', 1).arg(cls)
                         : QString("Flow clean — score: %1 — %2").arg(score, 0, 'f', 1).arg(cls));
            return;
        }

        // ── slippage ──────────────────────────────────────────────────────
        if (command == "slippage") {
            const auto sl = payload;  // flat: average_price, slippage_bps, fills, etc at top level
            const double avg_p   = sl["average_price"].toDouble();
            const double sl_bps  = sl["slippage_bps"].toDouble();
            const double cost    = sl["total_cost"].toDouble();
            const int    n_fills = sl["fill_count"].toInt();
            const bool   viable  = sl["viable"].toBool();

            set_card("hft_slip_avgp",  QString::number(avg_p, 'f', 4));
            const QString bps_col = sl_bps < 5  ? QString(ui::colors::POSITIVE())
                                  : sl_bps < 20 ? QString(ui::colors::WARNING())
                                  : QString(ui::colors::NEGATIVE());
            set_card("hft_slip_bps",   QString::number(sl_bps, 'f', 4) + " bps", bps_col);
            set_card("hft_slip_cost",  QString::number(cost, 'f', 4));
            set_card("hft_slip_fills", QString::number(n_fills));
            set_card("hft_slip_viable",
                     viable ? "VIABLE" : "HIGH SLIP",
                     viable ? QString(ui::colors::POSITIVE()) : QString(ui::colors::NEGATIVE()));

            // Populate fills table
            if (auto* tbl = this->findChild<QTableWidget*>("hft_slip_table")) {
                const auto fills = sl["fills"].toArray();
                tbl->setRowCount(fills.size());
                for (int i = 0; i < fills.size(); ++i) {
                    const auto f = fills[i].toObject();
                    tbl->setItem(i, 0, new QTableWidgetItem(QString::number(f["price"].toDouble(), 'f', 4)));
                    tbl->setItem(i, 1, new QTableWidgetItem(QString::number(f["quantity"].toDouble(), 'g', 6)));
                    tbl->setItem(i, 2, new QTableWidgetItem(QString::number(f["cost"].toDouble(), 'f', 4)));
                    tbl->setRowHeight(i, 20);
                }
            }
            status_label_->setText(
                QString("Avg fill: %1  |  Slippage: %2 bps  |  %3")
                    .arg(avg_p, 0, 'f', 4)
                    .arg(sl_bps, 0, 'f', 4)
                    .arg(viable ? "VIABLE" : "HIGH SLIPPAGE"));
            return;
        }

        // ── analyze (full analysis) ───────────────────────────────────────
        if (command == "analyze") {
            // Dispatch sub-results to each section
            if (payload.contains("bids")) {
                const auto bids = payload["bids"].toArray();
                const auto asks = payload["asks"].toArray();
                fill_book_table("hft_bid_table", bids, true);
                fill_book_table("hft_ask_table", asks, false);
            }
            if (payload.contains("book_metrics")) {
                const auto bm = payload["book_metrics"].toObject();
                set_card("hft_mid_val",    QString::number(bm["mid_price"].toDouble(), 'f', 4));
                set_card("hft_spread_val", QString::number(bm["spread_bps"].toDouble(), 'f', 3) + " bps");
                const double obi = bm["obi"].toDouble();
                const QString obi_col = obi > 0.1 ? QString(ui::colors::POSITIVE())
                                      : obi < -0.1 ? QString(ui::colors::NEGATIVE())
                                      : QString(ui::colors::TEXT_PRIMARY());
                set_card("hft_obi_val",      QString::number(obi, 'f', 4), obi_col);
                const QString pres = bm["pressure"].toString();
                const QString p_col = pres == "BUY" ? QString(ui::colors::POSITIVE())
                                    : pres == "SELL" ? QString(ui::colors::NEGATIVE())
                                    : QString(ui::colors::TEXT_PRIMARY());
                set_card("hft_pressure_val", pres, p_col);
                set_card("hft_wmid_val",     QString::number(bm["weighted_mid"].toDouble(), 'f', 4));
            }
            if (payload.contains("market_making")) {
                const auto mm = payload["market_making"].toObject();
                set_card("hft_mm_bid", QString::number(mm["bid_price"].toDouble(), 'f', 4),
                         QString(ui::colors::POSITIVE()));
                set_card("hft_mm_ask", QString::number(mm["ask_price"].toDouble(), 'f', 4),
                         QString(ui::colors::NEGATIVE()));
                set_card("hft_mm_qspread", QString::number(mm["quoted_spread_bps"].toDouble(), 'f', 3) + " bps");
                set_card("hft_mm_edge",    QString::number(mm["edge_per_side_bps"].toDouble(), 'f', 3) + " bps");
                set_card("hft_mm_rec",     mm["recommendation"].toString());
            }
            if (payload.contains("toxic_flow")) {
                const auto tf    = payload["toxic_flow"].toObject();
                const double sc  = tf["toxicity_score"].toDouble();
                const QString col = sc > 60 ? QString(ui::colors::NEGATIVE())
                                  : sc > 30 ? QString(ui::colors::WARNING())
                                  : QString(ui::colors::POSITIVE());
                set_card("hft_tox_pin",    QString::number(sc, 'f', 1), col);
                set_card("hft_tox_vol",    QString::number(tf["vol_imbalance"].toDouble(), 'f', 4));
                set_card("hft_tox_impact", QString::number(tf["price_impact_bps"].toDouble(), 'f', 2) + " bps");
                set_card("hft_tox_class",  tf["classification"].toString());
                set_card("hft_tox_action", QString(tf["action"].toString()).replace('_', ' '));
            }
            if (payload.contains("slippage")) {
                const auto sl = payload["slippage"].toObject();
                set_card("hft_slip_avgp",  QString::number(sl["average_price"].toDouble(), 'f', 4));
                set_card("hft_slip_bps",   QString::number(sl["slippage_bps"].toDouble(), 'f', 4) + " bps");
                set_card("hft_slip_cost",  QString::number(sl["total_cost"].toDouble(), 'f', 4));
                set_card("hft_slip_fills", QString::number(sl["fill_count"].toInt()));
                const bool v = sl["viable"].toBool();
                set_card("hft_slip_viable", v ? "VIABLE" : "HIGH SLIP",
                         v ? QString(ui::colors::POSITIVE()) : QString(ui::colors::NEGATIVE()));
            }
            if (auto* lat_lbl = this->findChild<QLabel*>("hftLatency"))
                lat_lbl->setText(QString("LATENCY  %1 ms").arg(payload["latency_ms"].toDouble(), 0, 'f', 1));

            status_label_->setText(
                QString("Full analysis complete — %1 @ %2")
                    .arg(payload["symbol"].toString(), payload["timestamp"].toString().left(19)));
            return;
        }

        display_result(payload);
        return;
    }

    // ── Rolling Retraining ───────────────────────────────────────────────────
    if (module_id == "rolling_retraining") {
        // ── Streaming events from retrain command ────────────────────────────
        const QString event = payload["event"].toString();
        if (!event.isEmpty()) {
            auto* pb  = this->findChild<QProgressBar*>("rr_progress");
            auto* log = this->findChild<QTextEdit*>("rr_log");

            if (event == "start") {
                const int total = payload["total_windows"].toInt();
                if (pb) { pb->setRange(0, total); pb->setValue(0); pb->setFormat("0 / %v windows"); }
                if (log) log->append(QString("Starting retrain: %1  |  %2 windows")
                                         .arg(payload["model_id"].toString()).arg(total));
                status_label_->setText(QString("Retraining %1 — 0/%2 windows")
                                           .arg(payload["model_id"].toString()).arg(total));
            } else if (event == "window") {
                const int idx   = payload["index"].toInt();
                const int total = payload["total"].toInt();
                if (pb) { pb->setValue(idx); pb->setFormat(QString("%1 / %2 windows").arg(idx).arg(total)); }
                if (log) log->append(QString("  Window %1/%2  train→%3  test %4→%5")
                                         .arg(idx).arg(total)
                                         .arg(payload["train_end"].toString().left(10))
                                         .arg(payload["test_start"].toString().left(10))
                                         .arg(payload["test_end"].toString().left(10)));
                status_label_->setText(QString("Window %1/%2").arg(idx).arg(total));
            } else if (event == "ensemble") {
                if (log) log->append(QString("  %1").arg(payload["message"].toString()));
                status_label_->setText("Combining rolling results...");
            } else if (event == "done") {
                const int total = payload["windows_trained"].toInt();
                if (pb) { pb->setValue(total); pb->setFormat("Complete"); }
                if (log) log->append(QString("\nDone — %1 windows in %2s  |  Experiment: %3")
                                         .arg(total)
                                         .arg(payload["elapsed_sec"].toDouble(), 0, 'f', 1)
                                         .arg(payload["exp_name"].toString()));
                status_label_->setText(QString("Retrain complete — %1 windows in %2s")
                                           .arg(total)
                                           .arg(payload["elapsed_sec"].toDouble(), 0, 'f', 1));
            } else if (event == "error") {
                if (pb) { pb->setFormat("Failed"); }
                if (log) log->append(QString("\nError: %1").arg(payload["error"].toString()));
                display_error(payload["error"].toString());
            }
            return;
        }

        // ── Non-streaming responses ──────────────────────────────────────────
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString());
            return;
        }
        clear_results();

        if (command == "list") {
            auto schedules = payload["schedules"].toObject();
            // Find the cards container and repopulate it
            auto* cards_w = this->findChild<QWidget*>("rr_cards_container");
            if (cards_w) {
                auto* cards_vl = qobject_cast<QVBoxLayout*>(cards_w->layout());
                // Remove old cards (keep the trailing stretch)
                while (cards_vl->count() > 1)
                    delete cards_vl->takeAt(0)->widget();

                if (schedules.isEmpty()) {
                    auto* empty = new QLabel("No schedules configured yet.\nUse the Create Schedule tab to add one.",
                                             cards_w);
                    empty->setAlignment(Qt::AlignCenter);
                    empty->setStyleSheet(QString("color:%1; font-size:12px; padding:24px;")
                                             .arg(ui::colors::TEXT_SECONDARY()));
                    cards_vl->insertWidget(0, empty);
                } else {
                    int i = 0;
                    for (auto it = schedules.begin(); it != schedules.end(); ++it, ++i) {
                        const QString mid = it.key();
                        auto obj = it.value().toObject();

                        // Card frame
                        auto* card = new QWidget(cards_w);
                        card->setStyleSheet(QString(
                            "QWidget{background:%1;border:1px solid %2;border-radius:6px;}"
                            "QWidget:hover{border-color:%3;}")
                            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(),
                                 module_.color.name()));

                        auto* cvl = new QVBoxLayout(card);
                        cvl->setContentsMargins(12, 10, 12, 10);
                        cvl->setSpacing(4);

                        // Top row: model ID + status badge
                        auto* top = new QWidget(card);
                        top->setStyleSheet("QWidget{background:transparent;border:none;}");
                        auto* thl = new QHBoxLayout(top);
                        thl->setContentsMargins(0, 0, 0, 0);

                        auto* id_lbl = new QLabel(mid, top);
                        id_lbl->setStyleSheet(QString("color:%1;font-weight:700;font-size:13px;"
                                                       "background:transparent;border:none;")
                                                  .arg(ui::colors::TEXT_PRIMARY()));
                        thl->addWidget(id_lbl);
                        thl->addStretch();

                        const QString status = obj["last_status"].toString("pending");
                        const QString badge_color = (status == "completed") ? "#22c55e"
                                                  : (status == "failed")    ? "#ef4444"
                                                                            : "#f59e0b";
                        auto* badge = new QLabel(status.toUpper(), top);
                        badge->setStyleSheet(QString("color:%1;background:transparent;border:none;"
                                                      "font-size:10px;font-weight:700;")
                                                 .arg(badge_color));
                        thl->addWidget(badge);
                        cvl->addWidget(top);

                        // Details row
                        const QString freq   = obj["frequency"].toString(obj["freq"].toString());
                        const QString window = QString::number(obj["window"].toInt());
                        const QString next   = obj["next_run"].toString("-").left(16).replace("T", "  ");
                        const QString last   = obj["last_run"].isNull()
                                                   ? "Never"
                                                   : obj["last_run"].toString().left(16).replace("T", "  ");
                        auto* detail = new QLabel(
                            QString("Freq: %1  |  Window: %2 days  |  Next: %3  |  Last: %4")
                                .arg(freq, window, next, last), card);
                        detail->setStyleSheet(QString("color:%1;font-size:11px;"
                                                       "background:transparent;border:none;")
                                                  .arg(ui::colors::TEXT_SECONDARY()));
                        cvl->addWidget(detail);

                        // Action buttons row
                        auto* acts = new QWidget(card);
                        acts->setStyleSheet("QWidget{background:transparent;border:none;}");
                        auto* ahl = new QHBoxLayout(acts);
                        ahl->setContentsMargins(0, 4, 0, 0);
                        ahl->setSpacing(6);

                        auto* run_btn = new QPushButton("Run Now", acts);
                        run_btn->setStyleSheet(QString(
                            "QPushButton{background:%1;color:%2;border:1px solid %1;"
                            "border-radius:3px;font-size:10px;font-weight:700;padding:3px 10px;}"
                            "QPushButton:hover{background:%3;}")
                            .arg(module_.color.name(), "#000000", module_.color.lighter(115).name()));
                        connect(run_btn, &QPushButton::clicked, this, [this, mid]() {
                            if (auto* pb = this->findChild<QProgressBar*>("rr_progress"))
                                { pb->setValue(0); pb->setFormat("Starting..."); }
                            if (auto* log = this->findChild<QTextEdit*>("rr_log")) log->clear();
                            status_label_->setText(QString("Retraining %1...").arg(mid));
                            QJsonObject p; p["model_id"] = mid;
                            AIQuantLabService::instance().rolling_execute_retrain(p);
                        });
                        ahl->addWidget(run_btn);

                        auto* del_btn = new QPushButton("Delete", acts);
                        del_btn->setStyleSheet(QString(
                            "QPushButton{background:transparent;color:#ef4444;border:1px solid #ef4444;"
                            "border-radius:3px;font-size:10px;font-weight:700;padding:3px 10px;}"
                            "QPushButton:hover{background:#7f1d1d;}"));
                        connect(del_btn, &QPushButton::clicked, this, [this, mid]() {
                            status_label_->setText(QString("Deleting %1...").arg(mid));
                            QJsonObject p; p["model_id"] = mid;
                            AIQuantLabService::instance().rolling_delete_schedule(p);
                        });
                        ahl->addWidget(del_btn);
                        ahl->addStretch();
                        cvl->addWidget(acts);

                        cards_vl->insertWidget(i, card);
                    }
                }
            }
            status_label_->setText(QString("%1 schedule(s)").arg(schedules.size()));
            return;
        }

        if (command == "create") {
            auto sched = payload["schedule"].toObject();
            const QString tmpl = payload["template_generated"].toBool()
                                     ? QString("\nBuilt-in LightGBM+Alpha158 template generated at:\n%1")
                                           .arg(payload["conf_path"].toString())
                                     : "";
            clear_results();
            auto* lbl = new QLabel(
                QString("Schedule created: %1\nFreq: %2  |  Window: %3 days  |  Next: %4%5")
                    .arg(payload["model_id"].toString(),
                         sched["frequency"].toString(),
                         QString::number(sched["window"].toInt()),
                         sched["next_run"].toString().left(16).replace("T", "  "),
                         tmpl),
                this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_PRIMARY()));
            results_layout_->addWidget(lbl);
            status_label_->setText("Schedule created — switch to Schedules tab to view");
            return;
        }

        if (command == "preview") {
            clear_results();
            const int total = payload["total_windows"].toInt();
            auto windows = payload["windows"].toArray();

            auto* hdr = new QLabel(
                QString("Preview: %1 rolling windows  (step=%2, horizon=%3)")
                    .arg(total).arg(payload["step"].toInt()).arg(payload["horizon"].toInt()),
                this);
            hdr->setStyleSheet(QString("color:%1;font-weight:700;font-size:13px;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
            results_layout_->addWidget(hdr);

            // Table of windows
            auto* tbl = new QTableWidget(total, 4, this);
            tbl->setStyleSheet(table_ss());
            tbl->setHorizontalHeaderLabels({"#", "Train Start", "Train End", "Test Period"});
            tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            tbl->verticalHeader()->setVisible(false);
            tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
            tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
            tbl->setAlternatingRowColors(true);
            int row = 0;
            for (const auto& v : windows) {
                auto obj = v.toObject();
                auto item = [&](const QString& s) {
                    auto* it = new QTableWidgetItem(s.left(10));
                    it->setTextAlignment(Qt::AlignCenter);
                    return it;
                };
                tbl->setItem(row, 0, item(QString::number(row + 1)));
                tbl->setItem(row, 1, item(obj["train_start"].toString()));
                tbl->setItem(row, 2, item(obj["train_end"].toString()));
                tbl->setItem(row, 3, item(QString("%1 → %2")
                    .arg(obj["test_start"].toString().left(10),
                         obj["test_end"].toString().left(10))));
                ++row;
            }
            tbl->setMaximumHeight(220);
            results_layout_->addWidget(tbl);
            status_label_->setText(QString("Preview: %1 windows").arg(total));
            return;
        }

        if (command == "delete") {
            status_label_->setText(QString("Deleted: %1").arg(payload["model_id"].toString()));
            // Auto-refresh the schedule list
            AIQuantLabService::instance().rolling_list_schedules();
            return;
        }

        display_result(payload);
        return;
    }

    display_result(payload);
}

} // namespace fincept::screens
