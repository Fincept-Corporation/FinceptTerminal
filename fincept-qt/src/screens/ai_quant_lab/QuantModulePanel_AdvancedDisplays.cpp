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

// ─────────────────────────────────────────────────────────────────────────────
// Local helpers (anonymous namespace — file-local)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

QString fmt_pct_safe(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble() * 100.0, 'f', decimals) + "%";
}

QString fmt_num_safe(const QJsonValue& v, int decimals = 4) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals);
}

QString fmt_int_safe(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toInt());
}

QString fmt_bps(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals) + " bps";
}

QString verdict_color_for(const QString& v) {
    const QString u = v.toUpper();
    if (u == "STRONG" || u == "GOOD" || u == "EXCELLENT" || u == "CLEAN" || u == "NORMAL" || u == "BUY")
        return ui::colors::POSITIVE();
    if (u == "MODERATE" || u == "MAINTAIN" || u == "FAIR" || u == "MIXED")
        return ui::colors::TEXT_PRIMARY();
    if (u == "WEAK" || u == "WARNING" || u == "ELEVATED" || u == "NEUTRAL")
        return ui::colors::WARNING();
    if (u == "TOXIC" || u == "POOR" || u == "AVOID" || u == "SELL")
        return ui::colors::NEGATIVE();
    return ui::colors::TEXT_PRIMARY();
}

// Returns false if the payload was an error (already displayed via callback) — caller should bail.
bool check_success(const QJsonObject& payload,
                   const std::function<void(const QString&)>& display_error_fn) {
    if (!payload.value("success").toBool(false)) {
        const QString err = payload.value("error").toString("Unknown error");
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? "Input error: "
                              : kind == "runtime"    ? "Computation failed: "
                                                     : "";
        display_error_fn(prefix + err);
        return false;
    }
    return true;
}

// Render a {asset → weight} dict as a sorted-by-weight table.
QTableWidget* build_weights_table(const QJsonObject& weights, QWidget* parent, int max_height = 320) {
    QList<QPair<QString, double>> rows;
    for (auto it = weights.begin(); it != weights.end(); ++it)
        rows.append({it.key(), it.value().toDouble()});
    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return std::abs(a.second) > std::abs(b.second); });

    auto* table = new QTableWidget(rows.size(), 2, parent);
    table->setHorizontalHeaderLabels({"Asset", "Weight"});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setStyleSheet(table_ss());
    table->setMaximumHeight(qMin(rows.size() * 24 + 32, max_height));
    for (int r = 0; r < rows.size(); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(rows[r].first));
        const double w = rows[r].second;
        auto* wi = new QTableWidgetItem(QString::number(w * 100.0, 'f', 3) + "%");
        wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (w > 0.001) wi->setForeground(QColor(ui::colors::POSITIVE()));
        else if (w < -0.001) wi->setForeground(QColor(ui::colors::NEGATIVE()));
        table->setItem(r, 1, wi);
        table->setRowHeight(r, 24);
    }
    return table;
}

} // namespace

// NOTE: hft and rolling_retraining have bespoke per-tab UI handlers in
// QuantModulePanel.cpp::on_result() (in-tab card grid + book tables for HFT,
// progress bar + schedule cards for Rolling Retraining). The display_hft_result
// and display_rolling_retraining_result functions defined below are NOT
// dispatched by on_result() (the bespoke handlers take precedence). They exist
// as a fallback rendering style and as a starting point if a future round
// migrates those panels to the generic pattern.

// ═════════════════════════════════════════════════════════════════════════════
// 1. HFT
// ═════════════════════════════════════════════════════════════════════════════
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
void QuantModulePanel::display_advanced_models_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "list_available") {
        const auto models = payload.value("models").toArray();
        const bool torch = payload.value("torch_available").toBool();
        const bool qlib = payload.value("qlib_available").toBool();
        QSet<QString> categories;
        for (const auto& m : models)
            categories.insert(m.toObject().value("category").toString());
        QList<QWidget*> top = {
            gs_make_card("MODELS", QString::number(models.size()), this, ui::colors::POSITIVE()),
            gs_make_card("PYTORCH", torch ? "AVAILABLE" : "MISSING", this,
                         torch ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("QLIB", qlib ? "AVAILABLE" : "MISSING", this,
                         qlib ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("CATEGORIES", QString::number(categories.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!models.isEmpty()) {
            auto* table = new QTableWidget(models.size(), 4, this);
            table->setHorizontalHeaderLabels({"ID", "Name", "Category", "Available"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * 26 + 32, 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("id").toString().toUpper()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("name").toString()));
                table->setItem(i, 2, new QTableWidgetItem(m.value("category").toString()));
                const bool avail = m.value("available").toBool();
                auto* a = new QTableWidgetItem(avail ? "YES" : "NO");
                a->setForeground(QColor(avail ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
                a->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 3, a);
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 advanced model(s) available").arg(models.size()));
        return;
    }

    if (command == "create_model") {
        const auto cfg = payload.value("config").toObject();
        QList<QWidget*> top = {
            gs_make_card("MODEL ID", payload.value("model_id").toString(), this, ui::colors::POSITIVE()),
            gs_make_card("TYPE", payload.value("model_type").toString().toUpper(), this),
            gs_make_card("CONFIG KEYS", QString::number(cfg.size()), this),
            gs_make_card("STATUS", "CREATED", this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!cfg.isEmpty()) {
            auto* table = new QTableWidget(cfg.size(), 2, this);
            table->setHorizontalHeaderLabels({"Hyperparameter", "Value"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(cfg.size() * 24 + 32, 240));
            int r = 0;
            for (auto it = cfg.begin(); it != cfg.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                auto* vi = new QTableWidgetItem(format_val(it.value()));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(payload.value("message").toString("Model created"));
        return;
    }

    if (command == "train_model") {
        const double final_loss = payload.value("final_loss").toDouble();
        const int epochs = payload.value("epochs_trained").toInt();
        const int total_epochs = payload.value("total_epochs").toInt(epochs);
        const auto loss_hist = payload.value("loss_history").toArray();
        double initial_loss = loss_hist.isEmpty() ? 0.0 : loss_hist[0].toDouble();

        QList<QWidget*> top = {
            gs_make_card("MODEL ID", payload.value("model_id").toString(), this),
            gs_make_card("EPOCHS",
                         total_epochs > 0 && total_epochs != epochs
                             ? QString("%1 / %2").arg(epochs).arg(total_epochs)
                             : QString::number(epochs),
                         this, ui::colors::POSITIVE()),
            gs_make_card("FINAL LOSS", QString::number(final_loss, 'g', 4), this,
                         final_loss < initial_loss * 0.5 ? ui::colors::POSITIVE()
                                                          : ui::colors::WARNING()),
            gs_make_card("LOSS REDUCTION",
                         initial_loss > 0
                             ? QString::number((1.0 - final_loss / initial_loss) * 100, 'f', 1) + "%"
                             : "—",
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Loss history table — sample to first/middle/last 12 epochs
        if (!loss_hist.isEmpty()) {
            const int rows = std::min<int>(12, loss_hist.size());
            const int step = std::max<int>(1, loss_hist.size() / rows);
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({"Epoch", "Loss"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < loss_hist.size() && r < rows; i += step, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                auto* it = new QTableWidgetItem(QString::number(loss_hist[i].toDouble(), 'g', 6));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, it);
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Trained %1 epochs  final loss=%2")
                                   .arg(epochs).arg(final_loss, 0, 'g', 4));
        return;
    }

    if (command == "predict") {
        const auto preds = payload.value("predictions").toArray();
        double mean = 0.0, mn = std::numeric_limits<double>::infinity(), mx = -std::numeric_limits<double>::infinity();
        for (const auto& v : preds) {
            const double d = v.toDouble();
            mean += d;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
        }
        if (!preds.isEmpty()) mean /= preds.size();
        QList<QWidget*> top = {
            gs_make_card("MODEL ID", payload.value("model_id").toString(), this),
            gs_make_card("PREDICTIONS", QString::number(preds.size()), this, ui::colors::POSITIVE()),
            gs_make_card("MEAN", QString::number(mean, 'f', 6), this, gs_pos_neg_color(mean)),
            gs_make_card("RANGE",
                         preds.isEmpty() ? "—"
                                         : QString("[%1, %2]").arg(mn, 0, 'f', 4).arg(mx, 0, 'f', 4),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Show first/middle/last 12 predictions
        if (!preds.isEmpty()) {
            const int rows = std::min<int>(12, preds.size());
            const int step = std::max<int>(1, preds.size() / rows);
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({"Index", "Prediction"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < preds.size() && r < rows; i += step, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                const double v = preds[i].toDouble();
                auto* it = new QTableWidgetItem(QString::number(v, 'f', 6));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                it->setForeground(QColor(gs_pos_neg_color(v)));
                table->setItem(r, 1, it);
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 predictions").arg(preds.size()));
        return;
    }

    if (command == "info" || command == "list_models") {
        // info: single model; list_models: array of summaries
        if (command == "info") {
            QList<QWidget*> top = {
                gs_make_card("MODEL ID", payload.value("model_id").toString(), this),
                gs_make_card("TYPE", payload.value("model_type").toString().toUpper(), this),
                gs_make_card("TRAINED", payload.value("trained").toBool() ? "YES" : "NO", this,
                             payload.value("trained").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card("EPOCHS", fmt_int_safe(payload.value("epochs_trained")), this),
            };
            results_layout_->addWidget(gs_card_row(top, this));
            status_label_->setText("Model info loaded");
        } else {
            const auto models = payload.value("models").toArray();
            QList<QWidget*> top = {
                gs_make_card("REGISTERED", QString::number(models.size()), this, ui::colors::POSITIVE()),
            };
            results_layout_->addWidget(gs_card_row(top, this));
            if (!models.isEmpty()) {
                auto* table = new QTableWidget(models.size(), 5, this);
                table->setHorizontalHeaderLabels({"Model ID", "Type", "Trained", "Epochs", "Created"});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(qMin(models.size() * 26 + 32, 360));
                for (int i = 0; i < models.size(); ++i) {
                    const auto m = models[i].toObject();
                    table->setItem(i, 0, new QTableWidgetItem(m.value("model_id").toString()));
                    table->setItem(i, 1, new QTableWidgetItem(m.value("type").toString().toUpper()));
                    const bool tr = m.value("trained").toBool();
                    auto* t = new QTableWidgetItem(tr ? "YES" : "NO");
                    t->setForeground(QColor(tr ? ui::colors::POSITIVE() : ui::colors::WARNING()));
                    t->setTextAlignment(Qt::AlignCenter);
                    table->setItem(i, 2, t);
                    table->setItem(i, 3, new QTableWidgetItem(QString::number(m.value("epochs_trained").toInt())));
                    table->setItem(i, 4, new QTableWidgetItem(m.value("created_at").toString()));
                    table->setRowHeight(i, 26);
                }
                results_layout_->addWidget(table);
            }
            status_label_->setText(QString("%1 trained model(s)").arg(models.size()));
        }
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 4. FEATURE ENGINEERING
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_feature_engineering_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── check_status ─────────────────────────────────────────────────────────
    if (command == "check_status") {
        const bool pd = payload.value("pandas_available").toBool();
        const auto inds = payload.value("indicators_available").toArray();
        QList<QWidget*> top = {
            gs_make_card("PANDAS", pd ? "AVAILABLE" : "MISSING", this,
                         pd ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("INDICATORS", QString::number(inds.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        if (!inds.isEmpty()) {
            QStringList lst;
            for (const auto& v : inds) lst << v.toString();
            auto* lbl = new QLabel("Available: " + lst.join(", "));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText("Feature engineering ready");
        return;
    }

    // ── select_features_by_ic ────────────────────────────────────────────────
    if (command == "select_features_by_ic") {
        const auto sel = payload.value("selected_features").toArray();
        QList<QWidget*> top = {
            gs_make_card("SELECTED", QString::number(sel.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!sel.isEmpty()) {
            auto* table = new QTableWidget(sel.size(), 2, this);
            table->setHorizontalHeaderLabels({"Rank", "Feature"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(sel.size() * 24 + 32, 320));
            for (int i = 0; i < sel.size(); ++i) {
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
                table->setItem(i, 1, new QTableWidgetItem(sel[i].toString()));
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Selected %1 feature(s) by IC").arg(sel.size()));
        return;
    }

    // ── Multi-series indicators (MACD, Bollinger, Stochastic) ────────────────
    auto render_multi_series = [this](const QStringList& keys, const QJsonObject& payload) {
        QList<QWidget*> stats;
        QHash<QString, QJsonArray> series;
        for (const auto& k : keys) {
            const auto arr = payload.value(k).toArray();
            series.insert(k, arr);
            double sum = 0, mn = std::numeric_limits<double>::infinity(),
                   mx = -std::numeric_limits<double>::infinity();
            int n = 0;
            for (const auto& v : arr) {
                if (v.isNull()) continue;
                const double d = v.toDouble();
                sum += d;
                mn = std::min(mn, d);
                mx = std::max(mx, d);
                ++n;
            }
            const double mean = n > 0 ? sum / n : 0.0;
            stats << gs_make_card(k.toUpper(), QString::number(mean, 'f', 4), this, gs_pos_neg_color(mean));
        }
        results_layout_->addWidget(gs_card_row(stats, this));

        // Multi-column table sample
        int max_n = 0;
        // QJsonArray::size() returns qsizetype (qint64); std::max requires
        // matching arg types — explicit cast keeps both as int.
        for (const auto& a : series) max_n = std::max(max_n, static_cast<int>(a.size()));
        const int rows = std::min<int>(15, max_n);
        const int step = std::max<int>(1, max_n / rows);
        auto* table = new QTableWidget(rows, keys.size() + 1, this);
        QStringList hdr = {"Index"};
        for (const auto& k : keys) hdr << k.toUpper();
        table->setHorizontalHeaderLabels(hdr);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(rows * 22 + 32);
        for (int i = 0, r = 0; i < max_n && r < rows; i += step, ++r) {
            table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
            for (int c = 0; c < keys.size(); ++c) {
                const auto& a = series[keys[c]];
                if (i < a.size() && !a[i].isNull()) {
                    const double v = a[i].toDouble();
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, c + 1, it);
                }
            }
            table->setRowHeight(r, 22);
        }
        results_layout_->addWidget(table);
    };

    if (command == "macd") {
        render_multi_series({"macd", "signal", "histogram"}, payload);
        status_label_->setText("MACD computed");
        return;
    }
    if (command == "bollinger_bands") {
        render_multi_series({"upper", "middle", "lower"}, payload);
        status_label_->setText("Bollinger bands computed");
        return;
    }
    if (command == "stochastic_oscillator") {
        render_multi_series({"k", "d"}, payload);
        status_label_->setText("Stochastic oscillator computed");
        return;
    }

    // ── Single-series indicators + evaluate_expression ───────────────────────
    if (payload.contains("data") && payload.value("data").isArray()) {
        const auto series = payload.value("data").toArray();
        double sum = 0, mn = std::numeric_limits<double>::infinity(),
               mx = -std::numeric_limits<double>::infinity();
        int n = 0;
        for (const auto& v : series) {
            if (v.isNull()) continue;
            const double d = v.toDouble();
            sum += d;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
            ++n;
        }
        const double mean = n > 0 ? sum / n : 0.0;

        QList<QWidget*> top = {
            gs_make_card("INDICATOR", command.toUpper(), this),
            gs_make_card("OBSERVATIONS", QString::number(n), this),
            gs_make_card("MEAN", QString::number(mean, 'f', 4), this, gs_pos_neg_color(mean)),
            gs_make_card("RANGE",
                         n > 0 ? QString("[%1, %2]").arg(mn, 0, 'f', 4).arg(mx, 0, 'f', 4) : "—",
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Sampled table
        const int rows = std::min<int>(15, series.size());
        const int step = std::max<int>(1, series.size() / rows);
        auto* table = new QTableWidget(rows, 2, this);
        table->setHorizontalHeaderLabels({"Index", command.toUpper()});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(rows * 22 + 32);
        for (int i = 0, r = 0; i < series.size() && r < rows; i += step, ++r) {
            table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
            if (!series[i].isNull()) {
                const double v = series[i].toDouble();
                auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, it);
            }
            table->setRowHeight(r, 22);
        }
        results_layout_->addWidget(table);
        status_label_->setText(QString("%1: %2 obs").arg(command).arg(n));
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 5. PORTFOLIO OPTIMIZATION
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_portfolio_opt_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    const QString method = payload.value("method").toString();
    QString header_text = (method.isEmpty() ? command : method).toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "check_status") {
        const bool sp = payload.value("scipy_available").toBool();
        const auto methods = payload.value("methods_available").toArray();
        QList<QWidget*> top = {
            gs_make_card("SCIPY", sp ? "AVAILABLE" : "MISSING", this,
                         sp ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("METHODS", QString::number(methods.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText("Portfolio optimization ready");
        return;
    }

    // Efficient frontier — special case: array of {return, volatility, sharpe, weights}
    if (command == "efficient_frontier") {
        const auto frontier = payload.value("frontier").toArray();
        // Find min-vol and max-sharpe portfolios
        double min_vol = std::numeric_limits<double>::infinity();
        double max_sharpe = -std::numeric_limits<double>::infinity();
        QJsonObject min_vol_pt, max_sharpe_pt;
        for (const auto& v : frontier) {
            const auto p = v.toObject();
            const double vol = p.value("volatility").toDouble();
            const double sh = p.value("sharpe").toDouble();
            if (vol < min_vol) { min_vol = vol; min_vol_pt = p; }
            if (sh > max_sharpe) { max_sharpe = sh; max_sharpe_pt = p; }
        }

        QList<QWidget*> top = {
            gs_make_card("PORTFOLIOS", QString::number(payload.value("num_portfolios").toInt()), this,
                         ui::colors::POSITIVE()),
            gs_make_card("MIN VOL", fmt_pct_safe(QJsonValue(min_vol)), this, ui::colors::INFO()),
            gs_make_card("MAX SHARPE", QString::number(max_sharpe, 'f', 3), this,
                         max_sharpe > 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("RANGE",
                         frontier.isEmpty() ? "—"
                                            : QString("%1% – %2%")
                                                  .arg(frontier.first().toObject().value("return").toDouble() * 100, 0, 'f', 1)
                                                  .arg(frontier.last().toObject().value("return").toDouble() * 100, 0, 'f', 1),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Sample frontier
        if (!frontier.isEmpty()) {
            const int rows = std::min<int>(15, frontier.size());
            const int step = std::max<int>(1, frontier.size() / rows);
            auto* table = new QTableWidget(rows, 3, this);
            table->setHorizontalHeaderLabels({"Return", "Volatility", "Sharpe"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 24 + 32);
            for (int i = 0, r = 0; i < frontier.size() && r < rows; i += step, ++r) {
                const auto p = frontier[i].toObject();
                const double ret = p.value("return").toDouble();
                const double vol = p.value("volatility").toDouble();
                const double sh = p.value("sharpe").toDouble();
                auto add = [&](int col, const QString& s, const QString& fg = {}) {
                    auto* it = new QTableWidgetItem(s);
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (!fg.isEmpty()) it->setForeground(QColor(fg));
                    table->setItem(r, col, it);
                };
                add(0, QString::number(ret * 100, 'f', 2) + "%", gs_pos_neg_color(ret));
                add(1, QString::number(vol * 100, 'f', 2) + "%");
                add(2, QString::number(sh, 'f', 3),
                    sh >= 1.0 ? ui::colors::POSITIVE() : sh > 0 ? ui::colors::WARNING() : ui::colors::NEGATIVE());
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("EF: %1 portfolios  best Sharpe=%2")
                                   .arg(payload.value("num_portfolios").toInt())
                                   .arg(max_sharpe, 0, 'f', 3));
        return;
    }

    // Standard optimization methods (BL, HRP, MinVar, MaxSharpe)
    const QJsonObject weights = payload.value("weights").toObject();
    const double exp_ret = payload.value("expected_return").toDouble();
    const double exp_vol = payload.value("expected_volatility").toDouble();
    const double sharpe = payload.value("sharpe_ratio").toDouble();
    const bool has_ret = payload.contains("expected_return");
    const bool has_sharpe = payload.contains("sharpe_ratio");

    QList<QWidget*> top = {
        gs_make_card("ASSETS", QString::number(weights.size()), this, ui::colors::POSITIVE()),
        gs_make_card("EXP. VOLATILITY", fmt_pct_safe(QJsonValue(exp_vol)), this),
        gs_make_card("EXP. RETURN", has_ret ? fmt_pct_safe(QJsonValue(exp_ret)) : "—", this,
                     has_ret ? gs_pos_neg_color(exp_ret) : ui::colors::TEXT_TERTIARY()),
        gs_make_card("SHARPE", has_sharpe ? QString::number(sharpe, 'f', 3) : "—", this,
                     !has_sharpe ? ui::colors::TEXT_TERTIARY()
                                 : sharpe >= 1.0 ? ui::colors::POSITIVE()
                                                  : sharpe > 0 ? ui::colors::WARNING()
                                                                : ui::colors::NEGATIVE()),
    };
    results_layout_->addWidget(gs_card_row(top, this));

    // Black-Litterman extras
    if (command == "black_litterman") {
        QList<QWidget*> bl = {
            gs_make_card("VIEWS", QString::number(payload.value("views_incorporated").toInt()), this, ui::colors::INFO()),
            gs_make_card("EQUILIBRIUM",
                         QString::number(payload.value("equilibrium_returns").toObject().size()) + " assets", this),
            gs_make_card("POSTERIOR",
                         QString::number(payload.value("posterior_returns").toObject().size()) + " assets", this),
            gs_make_card("METHOD", "BL", this),
        };
        results_layout_->addWidget(gs_card_row(bl, this));
    }

    if (!weights.isEmpty()) {
        results_layout_->addWidget(gs_section_header("WEIGHTS", accent));
        results_layout_->addWidget(build_weights_table(weights, this));
    }

    status_label_->setText(QString("%1: %2 assets, vol=%3%")
                               .arg(method.isEmpty() ? command : method)
                               .arg(weights.size())
                               .arg(exp_vol * 100, 0, 'f', 2));
}

// ═════════════════════════════════════════════════════════════════════════════
// 6. FACTOR EVALUATION
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_factor_evaluation_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "check_status") {
        const bool pd = payload.value("pandas_available").toBool();
        const bool ql = payload.value("qlib_available").toBool();
        QList<QWidget*> top = {
            gs_make_card("PANDAS", pd ? "AVAILABLE" : "MISSING", this,
                         pd ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("QLIB", ql ? "AVAILABLE" : "MISSING", this,
                         ql ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText("Factor evaluation ready");
        return;
    }

    // ── calculate_ic ─────────────────────────────────────────────────────────
    if (command == "calculate_ic") {
        const double ic_mean = payload.value("IC_mean").toDouble();
        const double ic_std = payload.value("IC_std").toDouble();
        const double icir = payload.value("ICIR").toDouble();
        const double ic_pos = payload.value("IC_positive_rate").toDouble();
        const double pval = payload.value("p_value").toDouble();
        const bool sig = payload.value("is_significant").toBool();

        QList<QWidget*> top = {
            gs_make_card("IC MEAN", QString::number(ic_mean, 'f', 4), this, gs_pos_neg_color(ic_mean)),
            gs_make_card("IC STD", QString::number(ic_std, 'f', 4), this),
            gs_make_card("ICIR", QString::number(icir, 'f', 3), this,
                         icir >= 0.5 ? ui::colors::POSITIVE()
                                     : icir > 0 ? ui::colors::WARNING()
                                                : ui::colors::NEGATIVE()),
            gs_make_card("POS RATE",
                         QString::number(ic_pos * 100, 'f', 1) + "%", this,
                         ic_pos > 0.55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> stats = {
            gs_make_card("IC MAX", QString::number(payload.value("IC_max").toDouble(), 'f', 4), this,
                         ui::colors::POSITIVE()),
            gs_make_card("IC MIN", QString::number(payload.value("IC_min").toDouble(), 'f', 4), this,
                         ui::colors::NEGATIVE()),
            gs_make_card("IC MEDIAN", QString::number(payload.value("IC_median").toDouble(), 'f', 4), this),
            gs_make_card("OBSERVATIONS", QString::number(payload.value("observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(stats, this));

        QList<QWidget*> moments = {
            gs_make_card("SKEWNESS", QString::number(payload.value("IC_skewness").toDouble(), 'f', 4), this),
            gs_make_card("KURTOSIS", QString::number(payload.value("IC_kurtosis").toDouble(), 'f', 4), this),
            gs_make_card("p-VALUE", QString::number(pval, 'f', 4), this,
                         pval < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("SIGNIFICANT", sig ? "YES" : "NO", this,
                         sig ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(moments, this));

        // Sample IC series
        const auto series = payload.value("ic_series").toArray();
        if (!series.isEmpty()) {
            const int rows = std::min<int>(15, series.size());
            const int step = std::max<int>(1, series.size() / rows);
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({"Date", "IC"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < series.size() && r < rows; i += step, ++r) {
                const auto o = series[i].toObject();
                table->setItem(r, 0, new QTableWidgetItem(o.value("date").toString()));
                const double ic = o.value("ic").toDouble();
                auto* it = new QTableWidgetItem(QString::number(ic, 'f', 4));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                it->setForeground(QColor(gs_pos_neg_color(ic)));
                table->setItem(r, 1, it);
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("IC=%1 ICIR=%2 p=%3")
                                   .arg(ic_mean, 0, 'f', 4).arg(icir, 0, 'f', 3).arg(pval, 0, 'f', 4));
        return;
    }

    // ── analyze_factor_returns ───────────────────────────────────────────────
    if (command == "analyze_factor_returns") {
        const double ls_sharpe = payload.value("long_short_sharpe").toDouble();
        const double ls_ret = payload.value("long_short_mean_return").toDouble();
        const double mono = payload.value("monotonicity").toDouble();
        const double spread = payload.value("spread").toDouble();

        QList<QWidget*> top = {
            gs_make_card("L/S SHARPE", QString::number(ls_sharpe, 'f', 3), this,
                         ls_sharpe >= 1.0 ? ui::colors::POSITIVE()
                                          : ls_sharpe > 0 ? ui::colors::WARNING()
                                                           : ui::colors::NEGATIVE()),
            gs_make_card("L/S MEAN RET", fmt_pct_safe(QJsonValue(ls_ret), 4), this, gs_pos_neg_color(ls_ret)),
            gs_make_card("SPREAD", fmt_pct_safe(QJsonValue(spread), 4), this, gs_pos_neg_color(spread)),
            gs_make_card("MONOTONICITY",
                         QString::number(mono * 100, 'f', 1) + "%", this,
                         mono >= 0.7 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        const auto qs = payload.value("quantile_stats").toObject();
        if (!qs.isEmpty()) {
            QList<QPair<QString, QJsonObject>> rows;
            for (auto it = qs.begin(); it != qs.end(); ++it)
                rows.append({it.key(), it.value().toObject()});
            std::sort(rows.begin(), rows.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });

            auto* table = new QTableWidget(rows.size(), 5, this);
            table->setHorizontalHeaderLabels({"Quantile", "Mean Return", "Std", "Sharpe", "Win Rate"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows.size() * 26 + 32, 280));
            for (int r = 0; r < rows.size(); ++r) {
                const auto& q = rows[r].second;
                table->setItem(r, 0, new QTableWidgetItem(rows[r].first));
                const double mr = q.value("mean_return").toDouble();
                auto* mri = new QTableWidgetItem(QString::number(mr * 100, 'f', 4) + "%");
                mri->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                mri->setForeground(QColor(gs_pos_neg_color(mr)));
                table->setItem(r, 1, mri);
                auto* sd = new QTableWidgetItem(QString::number(q.value("std_return").toDouble() * 100, 'f', 4) + "%");
                sd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 2, sd);
                const double sh = q.value("sharpe").toDouble();
                auto* shi = new QTableWidgetItem(QString::number(sh, 'f', 3));
                shi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                shi->setForeground(QColor(gs_pos_neg_color(sh)));
                table->setItem(r, 3, shi);
                auto* wi = new QTableWidgetItem(QString::number(q.value("win_rate").toDouble() * 100, 'f', 1) + "%");
                wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 4, wi);
                table->setRowHeight(r, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("L/S Sharpe=%1  spread=%2%")
                                   .arg(ls_sharpe, 0, 'f', 3).arg(spread * 100, 0, 'f', 4));
        return;
    }

    // ── calculate_risk_metrics ───────────────────────────────────────────────
    if (command == "calculate_risk_metrics") {
        const double vol = payload.value("volatility").toDouble();
        const double mdd = payload.value("max_drawdown").toDouble();
        const double beta = payload.value("beta").toDouble();
        const double te = payload.value("tracking_error").toDouble();

        QList<QWidget*> top = {
            gs_make_card("VOLATILITY", fmt_pct_safe(QJsonValue(vol)), this),
            gs_make_card("MAX DRAWDOWN", fmt_pct_safe(QJsonValue(mdd)), this,
                         mdd <= -0.20 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card("BETA", QString::number(beta, 'f', 3), this,
                         beta > 1.2 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card("TRACKING ERROR", fmt_pct_safe(QJsonValue(te)), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> tail = {
            gs_make_card("DOWNSIDE DEV", fmt_pct_safe(payload.value("downside_deviation")), this,
                         ui::colors::WARNING()),
            gs_make_card("VaR 95%", fmt_pct_safe(payload.value("var_95")), this, ui::colors::WARNING()),
            gs_make_card("CVaR 95%", fmt_pct_safe(payload.value("cvar_95")), this, ui::colors::NEGATIVE()),
            gs_make_card("DD DURATION",
                         QString::number(payload.value("max_drawdown_duration").toInt()) + " days", this),
        };
        results_layout_->addWidget(gs_card_row(tail, this));

        QList<QWidget*> capture = {
            gs_make_card("UP CAPTURE",
                         QString::number(payload.value("up_capture").toDouble() * 100, 'f', 1) + "%", this,
                         payload.value("up_capture").toDouble() > 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("DOWN CAPTURE",
                         QString::number(payload.value("down_capture").toDouble() * 100, 'f', 1) + "%", this,
                         payload.value("down_capture").toDouble() < 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("SKEWNESS", QString::number(payload.value("skewness").toDouble(), 'f', 3), this,
                         gs_pos_neg_color(payload.value("skewness").toDouble())),
            gs_make_card("KURTOSIS", QString::number(payload.value("kurtosis").toDouble(), 'f', 3), this),
        };
        results_layout_->addWidget(gs_card_row(capture, this));
        status_label_->setText(QString("Vol=%1%  MaxDD=%2%  β=%3")
                                   .arg(vol * 100, 0, 'f', 2).arg(mdd * 100, 0, 'f', 2).arg(beta, 0, 'f', 3));
        return;
    }

    // ── generate_evaluation_report ───────────────────────────────────────────
    if (command == "generate_evaluation_report") {
        const auto rep = payload.value("report").toObject();
        const QString rating = rep.value("rating").toString();
        const double score = rep.value("overall_score").toDouble();
        const auto ic = rep.value("ic_analysis").toObject();
        const auto qa = rep.value("quantile_analysis").toObject();
        const auto turn = rep.value("turnover_analysis").toObject();

        QList<QWidget*> top = {
            gs_make_card("FACTOR", rep.value("factor_name").toString(), this),
            gs_make_card("OVERALL SCORE", QString::number(score, 'f', 1) + " / 100", this,
                         score >= 70 ? ui::colors::POSITIVE()
                                     : score >= 40 ? ui::colors::WARNING()
                                                   : ui::colors::NEGATIVE()),
            gs_make_card("RATING", rating.toUpper(), this, verdict_color_for(rating)),
            gs_make_card("PERIODS",
                         QString::number(rep.value("data_range").toObject().value("periods").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        results_layout_->addWidget(gs_section_header("IC ANALYSIS", accent));
        QList<QWidget*> ic_cards = {
            gs_make_card("IC MEAN", QString::number(ic.value("IC_mean").toDouble(), 'f', 4), this,
                         gs_pos_neg_color(ic.value("IC_mean").toDouble())),
            gs_make_card("ICIR", QString::number(ic.value("ICIR").toDouble(), 'f', 3), this,
                         ic.value("ICIR").toDouble() >= 0.5 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("POS RATE",
                         QString::number(ic.value("IC_positive_rate").toDouble() * 100, 'f', 1) + "%", this),
            gs_make_card("p-VALUE", QString::number(ic.value("p_value").toDouble(), 'f', 4), this,
                         ic.value("p_value").toDouble() < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(ic_cards, this));

        results_layout_->addWidget(gs_section_header("QUANTILE ANALYSIS", accent));
        QList<QWidget*> qa_cards = {
            gs_make_card("L/S SHARPE", QString::number(qa.value("long_short_sharpe").toDouble(), 'f', 3), this,
                         qa.value("long_short_sharpe").toDouble() >= 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("SPREAD", fmt_pct_safe(qa.value("spread"), 4), this,
                         gs_pos_neg_color(qa.value("spread").toDouble())),
            gs_make_card("MONOTONICITY",
                         QString::number(qa.value("monotonicity").toDouble() * 100, 'f', 1) + "%", this,
                         qa.value("monotonicity").toDouble() >= 0.7 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("L/S MEAN RET",
                         fmt_pct_safe(qa.value("long_short_mean_return"), 4), this,
                         gs_pos_neg_color(qa.value("long_short_mean_return").toDouble())),
        };
        results_layout_->addWidget(gs_card_row(qa_cards, this));

        if (!turn.isEmpty()) {
            results_layout_->addWidget(gs_section_header("TURNOVER", accent));
            QList<QWidget*> tu_cards = {
                gs_make_card("MEAN", fmt_pct_safe(turn.value("mean_turnover")), this),
                gs_make_card("MEDIAN", fmt_pct_safe(turn.value("median_turnover")), this),
                gs_make_card("MAX", fmt_pct_safe(turn.value("max_turnover")), this, ui::colors::WARNING()),
                gs_make_card("OBS", QString::number(turn.value("observations").toInt()), this),
            };
            results_layout_->addWidget(gs_card_row(tu_cards, this));
        }
        status_label_->setText(QString("%1: %2/100 (%3)")
                                   .arg(rep.value("factor_name").toString())
                                   .arg(score, 0, 'f', 1).arg(rating));
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 7. STRATEGY BUILDER
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_strategy_builder_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    const QString stype = payload.value("strategy_type").toString();
    QString header_text = (stype.isEmpty() ? command : stype).toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "check_status") {
        QList<QWidget*> top = {
            gs_make_card("QLIB", payload.value("qlib_available").toBool() ? "AVAILABLE" : "MISSING", this,
                         payload.value("qlib_available").toBool() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("PANDAS", payload.value("pandas_available").toBool() ? "AVAILABLE" : "MISSING", this,
                         payload.value("pandas_available").toBool() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("NUMPY", payload.value("numpy_available").toBool() ? "AVAILABLE" : "MISSING", this,
                         payload.value("numpy_available").toBool() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("SCIPY", payload.value("scipy_available").toBool() ? "AVAILABLE" : "MISSING", this,
                         payload.value("scipy_available").toBool() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText("Strategy builder ready");
        return;
    }

    // portfolio_metrics — a flat dict of stats
    if (command == "portfolio_metrics") {
        const double sh = payload.value("sharpe_ratio").toDouble();
        const double ann = payload.value("annual_return").toDouble();
        const double mdd = payload.value("max_drawdown").toDouble();
        const double cal = payload.value("calmar_ratio").toDouble();

        QList<QWidget*> top = {
            gs_make_card("ANN. RETURN", fmt_pct_safe(QJsonValue(ann)), this, gs_pos_neg_color(ann)),
            gs_make_card("VOLATILITY", fmt_pct_safe(payload.value("volatility")), this),
            gs_make_card("SHARPE", QString::number(sh, 'f', 3), this,
                         sh >= 1.0 ? ui::colors::POSITIVE() : sh > 0 ? ui::colors::WARNING() : ui::colors::NEGATIVE()),
            gs_make_card("MAX DD", fmt_pct_safe(QJsonValue(mdd)), this,
                         mdd <= -0.20 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> ratios = {
            gs_make_card("SORTINO", QString::number(payload.value("sortino_ratio").toDouble(), 'f', 3), this,
                         payload.value("sortino_ratio").toDouble() >= 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("CALMAR", QString::number(cal, 'f', 3), this,
                         cal >= 0.5 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("INFO RATIO", QString::number(payload.value("information_ratio").toDouble(), 'f', 3), this,
                         payload.value("information_ratio").toDouble() > 0.5 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("BETA", QString::number(payload.value("beta").toDouble(), 'f', 3), this),
        };
        results_layout_->addWidget(gs_card_row(ratios, this));

        QList<QWidget*> day = {
            gs_make_card("WIN RATE",
                         QString::number(payload.value("win_rate").toDouble() * 100, 'f', 1) + "%", this,
                         payload.value("win_rate").toDouble() > 0.55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("BEST DAY", fmt_pct_safe(payload.value("best_day"), 3), this, ui::colors::POSITIVE()),
            gs_make_card("WORST DAY", fmt_pct_safe(payload.value("worst_day"), 3), this, ui::colors::NEGATIVE()),
            gs_make_card("ALPHA", fmt_pct_safe(payload.value("alpha"), 3), this,
                         gs_pos_neg_color(payload.value("alpha").toDouble())),
        };
        results_layout_->addWidget(gs_card_row(day, this));
        status_label_->setText(QString("Annual %1%  Sharpe %2  MaxDD %3%")
                                   .arg(ann * 100, 0, 'f', 2).arg(sh, 0, 'f', 3).arg(mdd * 100, 0, 'f', 2));
        return;
    }

    // Strategy creation commands — common fields: strategy_id, strategy_type, config?, weights?, suitable_for[]
    QList<QWidget*> top = {
        gs_make_card("STRATEGY ID", payload.value("strategy_id").toString(), this, ui::colors::POSITIVE()),
        gs_make_card("TYPE", stype, this),
        gs_make_card("STATUS", "CREATED", this, ui::colors::POSITIVE()),
        gs_make_card("DESCRIPTION", "↓ see below", this, ui::colors::INFO()),
    };
    results_layout_->addWidget(gs_card_row(top, this));

    // Risk parity / mean variance: weights present
    if (payload.contains("weights")) {
        const auto weights = payload.value("weights").toObject();
        if (payload.contains("expected_return")) {
            QList<QWidget*> mv = {
                gs_make_card("EXP. RETURN", fmt_pct_safe(payload.value("expected_return")), this,
                             gs_pos_neg_color(payload.value("expected_return").toDouble())),
                gs_make_card("EXP. VOL", fmt_pct_safe(payload.value("expected_volatility")), this),
                gs_make_card("SHARPE",
                             QString::number(payload.value("sharpe_ratio").toDouble(), 'f', 3), this,
                             payload.value("sharpe_ratio").toDouble() >= 1.0 ? ui::colors::POSITIVE()
                                                                              : ui::colors::WARNING()),
                gs_make_card("RISK-FREE",
                             fmt_pct_safe(payload.value("risk_free_rate")), this, ui::colors::INFO()),
            };
            results_layout_->addWidget(gs_card_row(mv, this));
        } else if (payload.contains("target_risk")) {
            QList<QWidget*> rp = {
                gs_make_card("TARGET RISK", fmt_pct_safe(payload.value("target_risk")), this),
                gs_make_card("EXP. VOL",
                             payload.value("expected_volatility").isString()
                                 ? payload.value("expected_volatility").toString()
                                 : fmt_pct_safe(payload.value("expected_volatility")),
                             this),
                gs_make_card("REBALANCE", payload.value("rebalance_frequency").toString().toUpper(), this),
                gs_make_card("ASSETS", QString::number(weights.size()), this),
            };
            results_layout_->addWidget(gs_card_row(rp, this));
        }
        results_layout_->addWidget(gs_section_header("WEIGHTS", accent));
        results_layout_->addWidget(build_weights_table(weights, this));
    } else if (payload.contains("config")) {
        // TopK-Dropout / Enhanced Indexing / Weight Strategy: render config dict
        const auto cfg = payload.value("config").toObject();
        if (!cfg.isEmpty()) {
            results_layout_->addWidget(gs_section_header("CONFIG", accent));
            auto* table = new QTableWidget(cfg.size(), 2, this);
            table->setHorizontalHeaderLabels({"Parameter", "Value"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(cfg.size() * 24 + 32, 240));
            int r = 0;
            for (auto it = cfg.begin(); it != cfg.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                auto* vi = new QTableWidgetItem(format_val(it.value()));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }

        // Show extra Enhanced-Indexing fields
        if (payload.contains("expected_tracking_error") || payload.contains("expected_alpha")) {
            QList<QWidget*> ei = {
                gs_make_card("TRACKING ERROR", payload.value("expected_tracking_error").toString(), this),
                gs_make_card("ALPHA TARGET", payload.value("expected_alpha").toString(), this, ui::colors::POSITIVE()),
                gs_make_card("BENCHMARK",
                             payload.value("config").toObject().value("benchmark").toString(), this, ui::colors::INFO()),
                gs_make_card("ACTIVE SHARE",
                             QString::number(payload.value("config").toObject().value("active_share").toDouble() * 100, 'f', 1) + "%",
                             this),
            };
            results_layout_->addWidget(gs_card_row(ei, this));
        } else if (payload.contains("expected_turnover")) {
            QList<QWidget*> tk = {
                gs_make_card("EXPECTED TURNOVER", payload.value("expected_turnover").toString(), this, ui::colors::WARNING()),
            };
            results_layout_->addWidget(gs_card_row(tk, this));
        }
    }

    // Description + suitable_for badges
    const QString desc = payload.value("description").toString();
    if (!desc.isEmpty()) {
        auto* lbl = new QLabel(desc);
        lbl->setWordWrap(true);
        lbl->setStyleSheet(QString("color:%1; font-size:11px; padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(lbl);
    }
    const auto sf = payload.value("suitable_for").toArray();
    if (!sf.isEmpty()) {
        QStringList sf_list;
        for (const auto& v : sf) sf_list << "• " + v.toString();
        auto* sf_lbl = new QLabel("Suitable for:\n" + sf_list.join("\n"));
        sf_lbl->setWordWrap(true);
        sf_lbl->setStyleSheet(QString("color:%1; font-size:10px; padding:6px 10px;")
                                  .arg(ui::colors::TEXT_SECONDARY()));
        results_layout_->addWidget(sf_lbl);
    }

    status_label_->setText(QString("%1 strategy created").arg(stype));
}

// ═════════════════════════════════════════════════════════════════════════════
// 8. DATA PROCESSORS
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
