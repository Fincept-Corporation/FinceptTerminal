// src/screens/ai_quant_lab/QuantModulePanel_AdvancedDisplays.cpp
//
// Card-based result renderers for the 8 ADVANCED-category panels.
// Mirrors the analytics pattern (gs_make_card / gs_card_row / gs_section_header).
// Replaces the generic display_result() flat-table fallback for these modules.
//
//   - hft (3 commands: fetch_orderbook, market_making, toxic_flow, slippage, analyze)
//   - rolling_retraining (list, create, preview, retrain events, delete)
//   - advanced_models (list_models, list_available, create, train, predict, info)
//   - feature_engineering (per-indicator, select_features_by_ic, evaluate_expression)
//   - portfolio_opt (BL, HRP, MinVar, MaxSharpe, EF)
//   - factor_evaluation (calculate_ic, generate_evaluation_report, calculate_risk_metrics, analyze_factor_returns)
//   - strategy_builder (create_*, portfolio_metrics)
//   - data_processors (list_processors, create_pipeline, process_data)
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>


namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;
using namespace fincept::screens::quant_advanced_helpers;

void QuantModulePanel::display_hft_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    const QString symbol = payload.value("symbol").toString();
    const QString exchange = payload.value("exchange").toString();
    const double latency = payload.value("latency_ms").toDouble();

    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (!symbol.isEmpty()) header_text += "  |  " + symbol.toUpper();
    if (!exchange.isEmpty()) header_text += "  |  " + exchange.toUpper();
    if (latency > 0.0) header_text += QString("  |  %1 ms").arg(latency, 0, 'f', 1);
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── Helper: book-metrics card row ────────────────────────────────────────
    auto book_row = [this](const QJsonObject& bm) {
        const double mid = bm.value("mid_price").toDouble();
        const double spread_bps = bm.value("spread_bps").toDouble();
        const double obi = bm.value("obi").toDouble();
        const QString pressure = bm.value("pressure").toString();
        QList<QWidget*> top = {
            gs_make_card("MID PRICE", QString::number(mid, 'f', 4), this),
            gs_make_card("SPREAD", fmt_bps(spread_bps), this,
                         spread_bps > 20 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card("OBI", QString::number(obi, 'f', 3), this, gs_pos_neg_color(obi)),
            gs_make_card("PRESSURE", pressure.isEmpty() ? "—" : pressure.toUpper(), this,
                         verdict_color_for(pressure)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        const double bb = bm.value("best_bid").toDouble();
        const double ba = bm.value("best_ask").toDouble();
        const double bvol = bm.value("bid_volume").toDouble();
        const double avol = bm.value("ask_volume").toDouble();
        QList<QWidget*> book = {
            gs_make_card("BEST BID", QString::number(bb, 'f', 4), this, ui::colors::POSITIVE()),
            gs_make_card("BEST ASK", QString::number(ba, 'f', 4), this, ui::colors::NEGATIVE()),
            gs_make_card("BID VOLUME", QString::number(bvol, 'f', 2), this),
            gs_make_card("ASK VOLUME", QString::number(avol, 'f', 2), this),
        };
        results_layout_->addWidget(gs_card_row(book, this));
    };

    // ── Helper: bids/asks side-by-side mini tables ───────────────────────────
    auto book_tables = [this](const QJsonArray& bids, const QJsonArray& asks) {
        if (bids.isEmpty() && asks.isEmpty()) return;
        const int rows = std::min<int>(10, std::max(bids.size(), asks.size()));
        auto* table = new QTableWidget(rows, 4, this);
        table->setHorizontalHeaderLabels({"Bid Size", "Bid Px", "Ask Px", "Ask Size"});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(rows * 22 + 32);
        auto pair_double = [](const QJsonValue& v, int idx) -> double {
            const auto a = v.toArray();
            return a.size() > idx ? a[idx].toDouble() : 0.0;
        };
        for (int i = 0; i < rows; ++i) {
            if (i < bids.size()) {
                auto* sz = new QTableWidgetItem(QString::number(pair_double(bids[i], 1), 'f', 4));
                sz->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 0, sz);
                auto* px = new QTableWidgetItem(QString::number(pair_double(bids[i], 0), 'f', 4));
                px->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                px->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(i, 1, px);
            }
            if (i < asks.size()) {
                auto* px = new QTableWidgetItem(QString::number(pair_double(asks[i], 0), 'f', 4));
                px->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                px->setForeground(QColor(ui::colors::NEGATIVE()));
                table->setItem(i, 2, px);
                auto* sz = new QTableWidgetItem(QString::number(pair_double(asks[i], 1), 'f', 4));
                sz->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 3, sz);
            }
            table->setRowHeight(i, 22);
        }
        results_layout_->addWidget(table);
    };

    // ── Helper: market-making card row ───────────────────────────────────────
    auto mm_row = [this, accent](const QJsonObject& mm) {
        results_layout_->addWidget(gs_section_header("MARKET MAKING", accent));
        const QString rec = mm.value("recommendation").toString();
        QList<QWidget*> top = {
            gs_make_card("BID PRICE", fmt_num_safe(mm.value("bid_price"), 4), this, ui::colors::POSITIVE()),
            gs_make_card("ASK PRICE", fmt_num_safe(mm.value("ask_price"), 4), this, ui::colors::NEGATIVE()),
            gs_make_card("QUOTED SPREAD", fmt_bps(mm.value("quoted_spread_bps")), this),
            gs_make_card("EDGE PER SIDE", fmt_bps(mm.value("edge_per_side_bps")), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> sizes = {
            gs_make_card("BID SIZE", fmt_num_safe(mm.value("bid_size"), 0), this),
            gs_make_card("ASK SIZE", fmt_num_safe(mm.value("ask_size"), 0), this),
            gs_make_card("INVENTORY", fmt_num_safe(mm.value("inventory"), 4), this,
                         gs_pos_neg_color(mm.value("inventory").toDouble())),
            gs_make_card("RECOMMENDATION", rec.isEmpty() ? "—" : rec.toUpper(), this, verdict_color_for(rec)),
        };
        results_layout_->addWidget(gs_card_row(sizes, this));
    };

    // ── Helper: toxic-flow card row ──────────────────────────────────────────
    auto tox_row = [this, accent](const QJsonObject& tx) {
        results_layout_->addWidget(gs_section_header("TOXIC FLOW", accent));
        const double score = tx.value("toxicity_score").toDouble();
        const bool is_toxic = tx.value("is_toxic").toBool();
        const QString action = tx.value("action").toString();
        const QString classification = tx.value("classification").toString();
        QList<QWidget*> top = {
            gs_make_card("TOXICITY SCORE", QString::number(score, 'f', 2), this,
                         score >= 70 ? ui::colors::NEGATIVE()
                                     : score >= 40 ? ui::colors::WARNING()
                                                   : ui::colors::POSITIVE()),
            gs_make_card("IS TOXIC", is_toxic ? "YES" : "NO", this,
                         is_toxic ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()),
            gs_make_card("ACTION", action.isEmpty() ? "—" : action.toUpper(), this, verdict_color_for(action)),
            gs_make_card("CLASS", classification.isEmpty() ? "—" : classification.toUpper(), this,
                         verdict_color_for(classification)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> stats = {
            gs_make_card("VOL IMBALANCE", fmt_num_safe(tx.value("vol_imbalance"), 4), this,
                         gs_pos_neg_color(tx.value("vol_imbalance").toDouble())),
            gs_make_card("PRICE IMPACT", fmt_bps(tx.value("price_impact_bps")), this),
            gs_make_card("BUY VOLUME", fmt_num_safe(tx.value("buy_volume"), 2), this, ui::colors::POSITIVE()),
            gs_make_card("SELL VOLUME", fmt_num_safe(tx.value("sell_volume"), 2), this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(stats, this));

        QList<QWidget*> trade_stats = {
            gs_make_card("AVG TRADE", fmt_num_safe(tx.value("avg_trade_size"), 4), this),
            gs_make_card("MAX TRADE", fmt_num_safe(tx.value("max_trade_size"), 4), this),
            gs_make_card("SIZE CONC.", fmt_num_safe(tx.value("size_concentration"), 2), this),
            gs_make_card("TRADE COUNT", fmt_int_safe(tx.value("trade_count")), this),
        };
        results_layout_->addWidget(gs_card_row(trade_stats, this));
    };

    // ── Helper: slippage card row + fills table ──────────────────────────────
    auto slip_row = [this, accent](const QJsonObject& sl) {
        results_layout_->addWidget(gs_section_header("SLIPPAGE ESTIMATE", accent));
        const double slip_bps = sl.value("slippage_bps").toDouble();
        const bool viable = sl.value("viable").toBool();
        QList<QWidget*> top = {
            gs_make_card("SIDE", sl.value("side").toString().toUpper(), this,
                         sl.value("side").toString() == "buy" ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("QUANTITY", fmt_num_safe(sl.value("quantity"), 4), this),
            gs_make_card("FILLED", fmt_num_safe(sl.value("filled_quantity"), 4), this),
            gs_make_card("VIABLE", viable ? "YES" : "NO", this,
                         viable ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> px = {
            gs_make_card("AVG PRICE", fmt_num_safe(sl.value("average_price"), 4), this),
            gs_make_card("MID PRICE", fmt_num_safe(sl.value("mid_price"), 4), this),
            gs_make_card("SLIPPAGE", fmt_bps(slip_bps), this,
                         std::abs(slip_bps) > 10 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card("TOTAL COST", fmt_num_safe(sl.value("total_cost"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(px, this));

        // Fills table
        const auto fills = sl.value("fills").toArray();
        if (!fills.isEmpty()) {
            auto* table = new QTableWidget(fills.size(), 3, this);
            table->setHorizontalHeaderLabels({"Price", "Quantity", "Cost"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(fills.size() * 22 + 32, 220));
            for (int i = 0; i < fills.size(); ++i) {
                const auto f = fills[i].toObject();
                auto add = [&](int col, double v, int dec) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, col, it);
                };
                add(0, f.value("price").toDouble(), 4);
                add(1, f.value("quantity").toDouble(), 4);
                add(2, f.value("cost").toDouble(), 4);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }
    };

    // ── Dispatch by command ──────────────────────────────────────────────────
    if (command == "fetch_orderbook") {
        book_row(payload);
        book_tables(payload.value("bids").toArray(), payload.value("asks").toArray());
        status_label_->setText(QString("OB %1: mid=%2 spread=%3bps")
                                   .arg(symbol)
                                   .arg(payload.value("mid_price").toDouble(), 0, 'f', 4)
                                   .arg(payload.value("spread_bps").toDouble(), 0, 'f', 2));
    } else if (command == "market_making") {
        book_row(payload.value("book_metrics").toObject());
        mm_row(payload.value("market_making").toObject());
        status_label_->setText("Market making quote ready");
    } else if (command == "toxic_flow") {
        book_row(payload.value("book_metrics").toObject());
        tox_row(payload.value("toxic_flow").toObject());
        status_label_->setText(QString("Toxicity: %1")
                                   .arg(payload.value("toxic_flow").toObject().value("toxicity_score").toDouble(), 0, 'f', 2));
    } else if (command == "slippage") {
        slip_row(payload);
        status_label_->setText(QString("Slippage: %1 bps").arg(payload.value("slippage_bps").toDouble(), 0, 'f', 2));
    } else if (command == "analyze") {
        // Full snapshot: book + market_making + toxic_flow + slippage
        book_row(payload.value("book_metrics").toObject());
        book_tables(payload.value("bids").toArray(), payload.value("asks").toArray());
        if (payload.contains("market_making"))
            mm_row(payload.value("market_making").toObject());
        if (payload.contains("toxic_flow"))
            tox_row(payload.value("toxic_flow").toObject());
        if (payload.contains("slippage"))
            slip_row(payload.value("slippage").toObject());
        status_label_->setText(QString("Full snapshot: %1 trades").arg(payload.value("trade_count").toInt()));
    } else {
        // Unknown command — fall through to generic flat display
        display_result(payload);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// 2. ROLLING RETRAINING
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_rolling_retraining_result(const QString& command, const QJsonObject& payload) {
    // Streaming events keep state across multiple calls — accumulate progress
    if (command == "retrain") {
        const QString event = payload.value("event").toString();
        if (event == "start") {
            clear_results();
            results_layout_->addWidget(gs_section_header(
                QString("RETRAIN START: %1  |  %2 windows")
                    .arg(payload.value("model_id").toString())
                    .arg(payload.value("total_windows").toInt()),
                module_.color.name()));
            status_label_->setText("Retraining started…");
            return;
        }
        if (event == "window") {
            auto* lbl = new QLabel(QString("Window %1/%2: train→%3, test %4–%5")
                                       .arg(payload.value("index").toInt())
                                       .arg(payload.value("total").toInt())
                                       .arg(payload.value("train_end").toString())
                                       .arg(payload.value("test_start").toString())
                                       .arg(payload.value("test_end").toString()));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                       "padding:4px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                        module_.color.name()));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("Window %1/%2")
                                       .arg(payload.value("index").toInt())
                                       .arg(payload.value("total").toInt()));
            return;
        }
        if (event == "ensemble") {
            auto* lbl = new QLabel(QString("Combining: %1").arg(payload.value("message").toString()));
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                       "padding:4px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::INFO()));
            results_layout_->addWidget(lbl);
            status_label_->setText("Ensembling rolling results…");
            return;
        }
        if (event == "done") {
            QList<QWidget*> top = {
                gs_make_card("MODEL", payload.value("model_id").toString(), this),
                gs_make_card("WINDOWS TRAINED", fmt_int_safe(payload.value("windows_trained")), this,
                             ui::colors::POSITIVE()),
                gs_make_card("ELAPSED",
                             QString::number(payload.value("elapsed_sec").toDouble(), 'f', 1) + " s", this),
                gs_make_card("EXPERIMENT", payload.value("exp_name").toString(), this, ui::colors::INFO()),
            };
            results_layout_->addWidget(gs_card_row(top, this));
            status_label_->setText("Retrain complete");
            return;
        }
        return;
    }

    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "list") {
        const QJsonObject schedules = payload.value("schedules").toObject();
        const int count = payload.value("count").toInt();
        const bool qlib = payload.value("qlib_available").toBool();

        QList<QWidget*> top = {
            gs_make_card("SCHEDULES", QString::number(count), this,
                         count > 0 ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()),
            gs_make_card("QLIB", qlib ? "AVAILABLE" : "MISSING", this,
                         qlib ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (count > 0) {
            auto* table = new QTableWidget(count, 6, this);
            table->setHorizontalHeaderLabels({"Model ID", "Frequency", "Window", "Step", "Last Status", "Next Run"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(count * 26 + 32, 360));
            int r = 0;
            for (auto it = schedules.begin(); it != schedules.end(); ++it, ++r) {
                const auto sch = it.value().toObject();
                table->setItem(r, 0, new QTableWidgetItem(it.key()));
                table->setItem(r, 1, new QTableWidgetItem(sch.value("frequency").toString()));
                table->setItem(r, 2, new QTableWidgetItem(QString::number(sch.value("window").toInt())));
                table->setItem(r, 3, new QTableWidgetItem(QString::number(sch.value("step").toInt())));
                const QString status = sch.value("last_status").toString();
                auto* st = new QTableWidgetItem(status);
                if (status == "completed") st->setForeground(QColor(ui::colors::POSITIVE()));
                else if (status == "failed") st->setForeground(QColor(ui::colors::NEGATIVE()));
                else if (status == "running") st->setForeground(QColor(ui::colors::WARNING()));
                table->setItem(r, 4, st);
                table->setItem(r, 5, new QTableWidgetItem(sch.value("next_run").toString()));
                table->setRowHeight(r, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 schedule(s)").arg(count));
        return;
    }

    if (command == "create") {
        const auto sch = payload.value("schedule").toObject();
        QList<QWidget*> top = {
            gs_make_card("MODEL", payload.value("model_id").toString(), this, ui::colors::POSITIVE()),
            gs_make_card("FREQUENCY", sch.value("frequency").toString(), this),
            gs_make_card("WINDOW", QString::number(sch.value("window").toInt()), this),
            gs_make_card("HORIZON", QString::number(sch.value("horizon").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        QList<QWidget*> next = {
            gs_make_card("STEP", QString::number(sch.value("step").toInt()), this),
            gs_make_card("NEXT RUN", sch.value("next_run").toString(), this, ui::colors::INFO()),
            gs_make_card("TEMPLATE", sch.value("template_generated").toBool() ? "YES" : "NO", this,
                         sch.value("template_generated").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("CREATED", sch.value("created_at").toString(), this),
        };
        results_layout_->addWidget(gs_card_row(next, this));
        status_label_->setText("Schedule created");
        return;
    }

    if (command == "preview") {
        const int total = payload.value("total_windows").toInt();
        const auto windows = payload.value("windows").toArray();
        QList<QWidget*> top = {
            gs_make_card("TOTAL WINDOWS", QString::number(total), this, ui::colors::POSITIVE()),
            gs_make_card("STEP", QString::number(payload.value("step").toInt()), this),
            gs_make_card("HORIZON", QString::number(payload.value("horizon").toInt()), this),
            gs_make_card("CONFIG", QFileInfo(payload.value("conf_path").toString()).fileName(), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!windows.isEmpty()) {
            auto* table = new QTableWidget(windows.size(), 4, this);
            table->setHorizontalHeaderLabels({"Train Start", "Train End", "Test Start", "Test End"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(windows.size() * 24 + 32, 360));
            for (int i = 0; i < windows.size(); ++i) {
                const auto w = windows[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(w.value("train_start").toString()));
                table->setItem(i, 1, new QTableWidgetItem(w.value("train_end").toString()));
                table->setItem(i, 2, new QTableWidgetItem(w.value("test_start").toString()));
                table->setItem(i, 3, new QTableWidgetItem(w.value("test_end").toString()));
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Preview: %1 rolling windows").arg(total));
        return;
    }

    if (command == "delete") {
        QList<QWidget*> top = {
            gs_make_card("DELETED", payload.value("model_id").toString(), this, ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText("Schedule removed");
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 3. ADVANCED MODELS
// ═════════════════════════════════════════════════════════════════════════════

void QuantModulePanel::display_data_processors_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "check_status") {
        QList<QWidget*> top = {
            gs_make_card("PANDAS", payload.value("pandas_available").toBool() ? "AVAILABLE" : "MISSING", this,
                         payload.value("pandas_available").toBool() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("PROCESSORS", QString::number(payload.value("processors_available").toInt()), this,
                         ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText("Data processors ready");
        return;
    }

    if (command == "list_processors") {
        const auto procs = payload.value("processors").toArray();
        const auto descs = payload.value("descriptions").toObject();

        QList<QWidget*> top = {
            gs_make_card("AVAILABLE", QString::number(procs.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!procs.isEmpty()) {
            auto* table = new QTableWidget(procs.size(), 2, this);
            table->setHorizontalHeaderLabels({"Processor", "Description"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(procs.size() * 26 + 32, 360));
            for (int i = 0; i < procs.size(); ++i) {
                const QString name = procs[i].toString();
                table->setItem(i, 0, new QTableWidgetItem(name.toUpper()));
                table->setItem(i, 1, new QTableWidgetItem(descs.value(name).toString()));
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 processor(s)").arg(procs.size()));
        return;
    }

    if (command == "create_pipeline") {
        const auto procs = payload.value("processors").toArray();
        QStringList plist;
        for (const auto& v : procs) plist << v.toString();

        QList<QWidget*> top = {
            gs_make_card("PIPELINE ID", payload.value("pipeline_id").toString(), this, ui::colors::POSITIVE()),
            gs_make_card("PROCESSORS", QString::number(payload.value("num_processors").toInt()), this),
            gs_make_card("STATUS", "CREATED", this, ui::colors::POSITIVE()),
            gs_make_card("STAGES", QString::number(plist.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!plist.isEmpty()) {
            auto* lbl = new QLabel("Pipeline order: " + plist.join("  →  "));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:11px;"
                                       "padding:8px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText("Pipeline created");
        return;
    }

    if (command == "process_data") {
        const auto stats = payload.value("statistics").toObject();
        const auto in_shape = stats.value("input_shape").toArray();
        const auto out_shape = stats.value("output_shape").toArray();
        auto shape_str = [](const QJsonArray& a) {
            QStringList s;
            for (const auto& v : a) s << QString::number(v.toInt());
            return "(" + s.join(", ") + ")";
        };

        QList<QWidget*> top = {
            gs_make_card("PIPELINE", payload.value("pipeline_id").toString(), this),
            gs_make_card("INPUT SHAPE", shape_str(in_shape), this),
            gs_make_card("OUTPUT SHAPE", shape_str(out_shape), this, ui::colors::POSITIVE()),
            gs_make_card("ROWS DROPPED",
                         in_shape.size() > 0 && out_shape.size() > 0
                             ? QString::number(in_shape[0].toInt() - out_shape[0].toInt())
                             : "—",
                         this, ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> nulls = {
            gs_make_card("INPUT NULLS", QString::number(stats.value("input_nulls").toInt()), this,
                         ui::colors::WARNING()),
            gs_make_card("OUTPUT NULLS", QString::number(stats.value("output_nulls").toInt()), this,
                         stats.value("output_nulls").toInt() == 0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("INPUT MEAN", QString::number(stats.value("input_mean").toDouble(), 'f', 4), this),
            gs_make_card("OUTPUT MEAN", QString::number(stats.value("output_mean").toDouble(), 'f', 4), this),
        };
        results_layout_->addWidget(gs_card_row(nulls, this));

        QList<QWidget*> std_row = {
            gs_make_card("INPUT STD", QString::number(stats.value("input_std").toDouble(), 'f', 4), this),
            gs_make_card("OUTPUT STD", QString::number(stats.value("output_std").toDouble(), 'f', 4), this,
                         std::abs(stats.value("output_std").toDouble() - 1.0) < 0.1 ? ui::colors::POSITIVE()
                                                                                     : ui::colors::TEXT_PRIMARY()),
            gs_make_card("STD RATIO",
                         stats.value("input_std").toDouble() > 0
                             ? QString::number(stats.value("output_std").toDouble() / stats.value("input_std").toDouble(), 'f', 3)
                             : "—",
                         this),
            gs_make_card("DROPPED %",
                         in_shape.size() > 0 && in_shape[0].toInt() > 0
                             ? QString::number((1.0 - double(out_shape[0].toInt()) / in_shape[0].toInt()) * 100, 'f', 2) + "%"
                             : "—",
                         this),
        };
        results_layout_->addWidget(gs_card_row(std_row, this));
        status_label_->setText(QString("Processed: %1 → %2").arg(shape_str(in_shape)).arg(shape_str(out_shape)));
        return;
    }

    display_result(payload);
}


} // namespace fincept::screens
