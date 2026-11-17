// src/screens/ai_quant_lab/QuantModulePanel_AIMLDisplays.cpp
//
// Card-based result renderers for AI/ML category panels that previously
// rendered plain QLabel text into the bottom results pane.
//
//   - rl_trading       — train (streaming progress + final result)
//   - online_learning  — list_models, create_model, train, predict, performance
//   - meta_learning    — list_models, run_selection, create_ensemble,
//                        tune_hyperparameters, get_results
//
// Skipped:
//   - deep_agent — has bespoke per-tab UI (LangGraph + RD-Agent text outputs,
//     task table widget). Handled by legacy on_result branch.
//   (pattern_intelligence was listed here as "declared but no panel/service
//    wired yet"; it has since been removed from all_quant_modules() entirely —
//    its target script is a library with no real command surface. See the
//    comment at its former registration site in AIQuantLabTypes.h.)
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"

#include <QAbstractItemView>
#include <QColor>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QPlainTextEdit>
#include <QProgressBar>
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
// check_success() / fmt_int_safe() / fmt_pct_safe() used to be duplicated in a
// file-local anonymous namespace here; they now come from the shared header.
using namespace fincept::screens::quant_advanced_helpers;
using namespace fincept::screens::quant_charts;

// ═════════════════════════════════════════════════════════════════════════════
// 1. RL TRADING
// ═════════════════════════════════════════════════════════════════════════════
//
// Streaming progress / log events are consumed by AIQuantLabService's
// training_progress / training_log signals (NOT result_ready) — they update
// the bespoke rl_progress_bar_ / rl_log_console_ widgets directly. By the time
// payloads reach this renderer they are the FINAL `event:"result"` payload of
// `train` (with the event field stripped by the service), or the one-line result
// of `evaluate` (run through run_module, so its "event" key is still present).
void QuantModulePanel::display_rl_trading_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = ui::colors::legible(module_.color);
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (payload.contains("algorithm"))
        header_text += "  |  " + payload.value("algorithm").toString().toUpper();
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // Percent / number cells: a value the script could not compute arrives as null — show "—".
    const auto pct_text = [](const QJsonValue& v) {
        return v.isDouble() ? QString::number(v.toDouble(), 'f', 2) + QLatin1Char('%') : QString::fromUtf8("—");
    };
    const auto pct_color = [](const QJsonValue& v) { return v.isDouble() ? gs_pos_neg_color(v.toDouble()) : QString(); };
    const auto num_text = [](const QJsonValue& v, int decimals) {
        return v.isDouble() ? QString::number(v.toDouble(), 'f', decimals) : QString::fromUtf8("—");
    };
    const auto excess = [](const QJsonValue& a, const QJsonValue& b) {
        return a.isDouble() && b.isDouble() ? QJsonValue(a.toDouble() - b.toDouble()) : QJsonValue();
    };

    // ── train: minimal final-result card row ─────────────────────────────────
    if (command == "train") {
        // "timesteps" is what was actually trained; on-policy algorithms train in whole
        // rollouts, so it can differ from the episodes x bars that were requested.
        QList<QWidget*> top = {
            gs_make_card(tr("ALGORITHM"), payload.value("algorithm").toString().toUpper(), this),
            gs_make_card(tr("TIMESTEPS"), fmt_int_safe(payload.value("timesteps")), this),
            gs_make_card(tr("REQUESTED"), fmt_int_safe(payload.value("requested_timesteps")), this),
            gs_make_card(tr("STATUS"), tr("TRAINED"), this, ui::colors::SUCCESS()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // The agent is trained on the first ~80% of the daily bars and evaluated on the held-out
        // rest, then compared with simply holding the stock over that same window.
        const auto eval = payload.value("evaluation").toObject();
        const auto data = payload.value("data").toObject();
        if (!eval.isEmpty()) {
            const QJsonValue agent_ret = eval.value("portfolio_return"); // already percent
            const QJsonValue hold_ret = eval.value("buy_hold_return_pct");
            const auto test_period = data.value("test_period").toArray();
            results_layout_->addWidget(gs_section_header(
                tr("OUT-OF-SAMPLE EVALUATION  |  %1  |  %2 → %3")
                    .arg(data.value("ticker").toString(),
                         test_period.size() > 0 ? test_period[0].toString() : QStringLiteral("—"),
                         test_period.size() > 1 ? test_period[1].toString() : QStringLiteral("—")),
                accent));
            QList<QWidget*> oos = {
                gs_make_card(tr("AGENT RETURN"), pct_text(agent_ret), this, pct_color(agent_ret)),
                gs_make_card(tr("BUY & HOLD"), pct_text(hold_ret), this, pct_color(hold_ret)),
                gs_make_card(tr("EXCESS VS HOLD"), pct_text(excess(agent_ret, hold_ret)), this,
                             pct_color(excess(agent_ret, hold_ret))),
                gs_make_card(tr("MEAN REWARD"), num_text(eval.value("mean_reward"), 4), this),
            };
            results_layout_->addWidget(gs_card_row(oos, this));
            QList<QWidget*> split = {
                gs_make_card(tr("TRAIN BARS"), fmt_int_safe(data.value("train_bars")), this),
                gs_make_card(tr("TEST BARS"), fmt_int_safe(data.value("test_bars")), this),
                gs_make_card(tr("EVAL EPISODES"), fmt_int_safe(eval.value("n_episodes")), this),
                gs_make_card(tr("REWARD STD"), num_text(eval.value("std_reward"), 4), this),
            };
            results_layout_->addWidget(gs_card_row(split, this));
        }
        const QString model_path = payload.value("model_path").toString();
        if (!model_path.isEmpty()) {
            auto* path_lbl = gs_section_header(tr("Model saved to %1").arg(model_path), accent);
            path_lbl->setWordWrap(true);
            path_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            results_layout_->addWidget(path_lbl);
            // (A fresh train hands the model to the Evaluate tab: the Train view's `after` hook,
            // QuantModulePanel_RL.cpp — not here, a remembered result is drawn again on every visit.)
        }
        if (payload.contains("model_save_error")) {
            auto* lbl = gs_section_header(
                tr("The model could not be saved: %1").arg(payload.value("model_save_error").toString()),
                ui::colors::WARNING());
            lbl->setWordWrap(true);
            results_layout_->addWidget(lbl);
        }

        const QString msg = payload.value("message").toString();
        if (!msg.isEmpty()) {
            auto* lbl = gs_section_header(msg, accent);
            lbl->setWordWrap(true);
            results_layout_->addWidget(lbl);
        }
        status_label_->setText(tr("Trained %1 — %2 steps")
                                   .arg(payload.value("algorithm").toString())
                                   .arg(payload.value("timesteps").toInt()));
        return;
    }

    // ── evaluate: replay of a saved model next to buy-and-hold ───────────────
    if (command == "evaluate") {
        const QJsonValue agent_ret = payload.value("portfolio_return"); // already percent
        const QJsonValue hold_ret = payload.value("buy_hold_return_pct");
        const auto period = payload.value("period").toArray();
        results_layout_->addWidget(gs_section_header(
            tr("REPLAY  |  %1  |  %2 → %3")
                .arg(payload.value("ticker").toString(),
                     period.size() > 0 ? period[0].toString() : QStringLiteral("—"),
                     period.size() > 1 ? period[1].toString() : QStringLiteral("—")),
            accent));
        QList<QWidget*> perf = {
            gs_make_card(tr("AGENT RETURN"), pct_text(agent_ret), this, pct_color(agent_ret)),
            gs_make_card(tr("BUY & HOLD"), pct_text(hold_ret), this, pct_color(hold_ret)),
            gs_make_card(tr("EXCESS VS HOLD"), pct_text(excess(agent_ret, hold_ret)), this,
                         pct_color(excess(agent_ret, hold_ret))),
            gs_make_card(tr("MEAN REWARD"), num_text(payload.value("mean_reward"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(perf, this));
        const auto train_period = payload.value("train_period").toArray();
        const QJsonValue final_value = payload.value("mean_portfolio_value");
        QList<QWidget*> info = {
            gs_make_card(tr("BARS"), fmt_int_safe(payload.value("bars")), this),
            gs_make_card(tr("FINAL VALUE"), final_value.isDouble() ? gs_fmt_money(final_value.toDouble())
                                                                   : QString::fromUtf8("—"),
                         this),
            gs_make_card(tr("EPISODES"), fmt_int_safe(payload.value("n_episodes")), this),
            gs_make_card(tr("TRAINED ON"),
                         train_period.size() > 1
                             ? QStringLiteral("%1 → %2").arg(train_period[0].toString(), train_period[1].toString())
                             : QString::fromUtf8("—"),
                         this),
        };
        results_layout_->addWidget(gs_card_row(info, this));
        const QString model_path = payload.value("model_path").toString();
        if (!model_path.isEmpty()) {
            auto* path_lbl = gs_section_header(tr("Model %1").arg(model_path), accent);
            path_lbl->setWordWrap(true);
            path_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            results_layout_->addWidget(path_lbl);
        }
        status_label_->setText(tr("Evaluated %1 on %2 — agent %3 vs buy & hold %4")
                                   .arg(payload.value("algorithm").toString(), payload.value("ticker").toString(),
                                        pct_text(agent_ret), pct_text(hold_ret)));
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 2. ONLINE LEARNING
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_online_learning_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = ui::colors::legible(module_.color);
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "list_models") {
        const auto models = payload.value("models").toArray();
        const bool river = payload.value("river_available").toBool();
        const bool qlib_online = payload.value("qlib_online_available").toBool();
        // The library and, separately, a local market dataset for it to read (qlib_datasets.py).
        const bool qlib_data = payload.value("qlib_dataset_available").toBool();
        const QJsonObject qlib_ds = payload.value("qlib_dataset").toObject();

        auto* qlib_card = gs_make_card(tr("QLIB ONLINE"),
                                       !qlib_online ? tr("MISSING") : qlib_data ? tr("READY") : tr("NO DATASET"), this,
                                       !qlib_online ? ui::colors::DANGER() : qlib_data ? ui::colors::SUCCESS()
                                                                                       : ui::colors::WARNING());
        qlib_card->setToolTip(!qlib_online ? tr("qlib (with its online module) is not installed")
                              : qlib_data
                                  ? tr("Dataset %1: %2 → %3")
                                        .arg(qlib_ds.value("id").toString(), qlib_ds.value("calendar_start").toString(),
                                             qlib_ds.value("calendar_end").toString())
                                  : tr("qlib is installed but there is no local market dataset; build one with "
                                       "BUILD FROM YAHOO (Model Library, Factor Discovery, Live Signals)"));
        QList<QWidget*> top = {
            gs_make_card(tr("MODELS"), QString::number(models.size()), this,
                         models.isEmpty() ? ui::colors::TEXT_SECONDARY() : ui::colors::POSITIVE()),
            gs_make_card(tr("RIVER"), river ? tr("AVAILABLE") : tr("MISSING"), this,
                         river ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            qlib_card,
            gs_make_card(tr("COUNT"), fmt_int_safe(payload.value("count")), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (models.isEmpty()) {
            // Nothing yet: what an online model is, and where to make one.
            const bool on_create = current_view() == QLatin1String("create");
            auto* es = ui::EmptyState::create(
                ui::EmptyState::no_data_spec(tr("No online models yet"),
                                             on_create ? tr("An online model learns one sample at a time. Create one "
                                                            "with the form above — it appears here.")
                                                       : tr("An online model learns one sample at a time. Create one "
                                                            "and it is listed here."),
                                             on_create ? QString() : tr("CREATE MODEL")),
                results_layout_->parentWidget());
            es->set_compact(true);
            es->set_alignment(Qt::AlignTop);
            connect(es, &ui::EmptyState::primary_clicked, this, [this]() { open_view(QStringLiteral("create")); });
            results_layout_->addWidget(es);
        }
        if (!models.isEmpty()) {
            auto* table = new QTableWidget(models.size(), 5, this);
            table->setHorizontalHeaderLabels(
                {tr("Model ID"), tr("Type"), tr("Samples"), tr("Created"), tr("Last Updated")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * row_px(26) + row_px(32), 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("model_id").toString()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("type").toString().toUpper()));
                const int samples = m.value("samples_trained").toInt();
                table->setItem(i, 2, num_item(QLocale().toString(samples), samples));
                table->setItem(i, 3, new QTableWidgetItem(m.value("created_at").toString()));
                const QJsonValue lu = m.value("last_updated");
                table->setItem(i, 4, new QTableWidgetItem(lu.isNull() ? tr("never") : lu.toString()));
                table->setRowHeight(i, row_px(26));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 online model(s)").arg(models.size()));
        return;
    }

    if (command == "create_model") {
        QList<QWidget*> top = {
            gs_make_card(tr("MODEL ID"), payload.value("model_id").toString(), this, ui::colors::POSITIVE()),
            gs_make_card(tr("TYPE"), payload.value("model_type").toString().toUpper(), this),
            gs_make_card(tr("STATUS"), tr("CREATED"), this, ui::colors::POSITIVE()),
            gs_make_card(tr("MESSAGE"), payload.value("message").toString(tr("OK")), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText(payload.value("message").toString(tr("Online model created")));
        return;
    }

    if (command == "train") {
        const QJsonValue pred = payload.value("prediction");
        const QJsonValue actual = payload.value("actual");
        // The script reports the per-sample error as "abs_error" (plus the signed
        // "residual" = actual - prediction). It used to be a top-level "error",
        // which the Python runner treats as a failed run.
        const double abs_err = payload.value("abs_error").toDouble();
        const double residual = payload.value("residual").toDouble(actual.toDouble() - pred.toDouble());
        const double mae = payload.value("current_mae").toDouble();
        const int samples = payload.value("samples_trained").toInt();
        const bool drift = payload.value("drift_detected").toBool();

        QList<QWidget*> top = {
            gs_make_card(tr("MODEL"), payload.value("model_id").toString(), this),
            gs_make_card(tr("SAMPLES"), QString::number(samples), this, ui::colors::POSITIVE()),
            gs_make_card(tr("CURRENT MAE"), QString::number(mae, 'f', 6), this,
                         mae < 0.01 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("DRIFT"), drift ? tr("DETECTED") : tr("NONE"), this,
                         drift ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!pred.isNull() && !actual.isNull()) {
            QList<QWidget*> pred_row = {
                gs_make_card(tr("PREDICTION"), QString::number(pred.toDouble(), 'f', 6), this,
                             gs_pos_neg_color(pred.toDouble())),
                gs_make_card(tr("ACTUAL"), QString::number(actual.toDouble(), 'f', 6), this,
                             gs_pos_neg_color(actual.toDouble())),
                gs_make_card(tr("RESIDUAL"), QString::number(residual, 'f', 6), this),
                gs_make_card(tr("ABS ERROR"), QString::number(abs_err, 'f', 6), this,
                             abs_err < std::abs(actual.toDouble()) * 0.1 ? ui::colors::SUCCESS()
                                                                         : ui::colors::WARNING()),
            };
            results_layout_->addWidget(gs_card_row(pred_row, this));
        }
        status_label_->setText(drift ? tr("⚠ Drift detected") : tr("MAE: %1").arg(mae, 0, 'f', 6));
        return;
    }

    if (command == "predict") {
        const QJsonValue pred = payload.value("prediction");
        const auto preds = payload.value("predictions").toArray();
        const int count = payload.value("count").toInt();

        if (preds.isEmpty() && !pred.isNull()) {
            // Single prediction
            QList<QWidget*> top = {
                gs_make_card(tr("MODEL"), payload.value("model_id").toString(), this),
                gs_make_card(tr("PREDICTION"), QString::number(pred.toDouble(), 'f', 6), this,
                             gs_pos_neg_color(pred.toDouble())),
            };
            results_layout_->addWidget(gs_card_row(top, this));
            status_label_->setText(tr("Prediction ready"));
            return;
        }

        // Batch predictions
        double sum = 0, mn = std::numeric_limits<double>::infinity(), mx = -std::numeric_limits<double>::infinity();
        for (const auto& v : preds) {
            const double d = v.toDouble();
            sum += d;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
        }
        const double mean = preds.isEmpty() ? 0.0 : sum / preds.size();

        QList<QWidget*> top = {
            gs_make_card(tr("MODEL"), payload.value("model_id").toString(), this),
            gs_make_card(tr("PREDICTIONS"), QString::number(count > 0 ? count : preds.size()), this,
                         ui::colors::POSITIVE()),
            gs_make_card(tr("MEAN"), QString::number(mean, 'f', 6), this, gs_pos_neg_color(mean)),
            gs_make_card(tr("RANGE"),
                         preds.isEmpty() ? QString("—") : QString("[%1, %2]").arg(mn, 0, 'f', 4).arg(mx, 0, 'f', 4),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!preds.isEmpty()) {
            // Every prediction in input order, then the last rows (newest first).
            QcChartSpec spec;
            spec.title = tr("PREDICTIONS BY INPUT ROW");
            spec.xs = qc_x_positions(preds.size()).xs;
            spec.fmt = qc_fmt_auto();
            spec.x_label = [](double v) {
                return QCoreApplication::translate("QuantModulePanel", "row %1").arg(static_cast<qlonglong>(std::llround(v)));
            };
            QcLine l;
            l.name = tr("Prediction");
            l.ys = qc_values(preds);
            l.style = ui::TimeSeriesChart::Style::Baseline;
            l.has_baseline = true;
            l.baseline = 0.0;
            spec.lines.append(l);
            results_layout_->addWidget(qc_series_chart(spec, this));

            const QVector<int> latest = qc_latest_rows(preds.size());
            const int rows = latest.size();
            results_layout_->addWidget(qc_caption(qc_latest_caption(rows, preds.size())));
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({tr("Index"), tr("Prediction")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * row_px(22) + row_px(32));
            for (int r = 0; r < rows; ++r) {
                const int i = latest.at(r);
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                const double v = preds[i].toDouble();
                auto* it = new QTableWidgetItem(QString::number(v, 'f', 6));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                it->setForeground(QColor(gs_pos_neg_color(v)));
                table->setItem(r, 1, it);
                table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 prediction(s)").arg(count > 0 ? count : preds.size()));
        return;
    }

    if (command == "performance") {
        const double mae = payload.value("current_mae").toDouble();
        const int samples = payload.value("samples_trained").toInt();
        const bool drift = payload.value("drift_detected").toBool();
        const QJsonValue lu = payload.value("last_updated");

        QList<QWidget*> top = {
            gs_make_card(tr("MODEL"), payload.value("model_id").toString(), this),
            gs_make_card(tr("TYPE"), payload.value("model_type").toString().toUpper(), this),
            gs_make_card(tr("SAMPLES"), QString::number(samples), this, ui::colors::POSITIVE()),
            gs_make_card(tr("CURRENT MAE"), QString::number(mae, 'f', 6), this,
                         mae < 0.01 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> meta = {
            gs_make_card(tr("DRIFT"), drift ? tr("DETECTED") : tr("NONE"), this,
                         drift ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()),
            gs_make_card(tr("LAST UPDATED"), lu.isNull() ? tr("never") : lu.toString(), this),
            gs_make_card(tr("CREATED"), payload.value("created_at").toString(), this),
            gs_make_card(tr("HEALTH"), drift ? tr("RETRAIN ADVISED") : tr("OK"), this,
                         drift ? ui::colors::WARNING() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(meta, this));
        status_label_->setText(tr("MAE: %1  samples: %2").arg(mae, 0, 'f', 6).arg(samples));
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 3. META LEARNING
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_meta_learning_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = ui::colors::legible(module_.color);
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "list_models") {
        const auto models = payload.value("models").toArray();
        const bool sk = payload.value("sklearn_available").toBool();
        const bool lgbm = payload.value("lightgbm_available").toBool();
        const bool xgb = payload.value("xgboost_available").toBool();
        const bool cat = payload.value("catboost_available").toBool();

        QList<QWidget*> top = {
            gs_make_card(tr("MODELS"), QString::number(models.size()), this, ui::colors::POSITIVE()),
            gs_make_card(tr("SKLEARN"), sk ? tr("OK") : tr("MISSING"), this,
                         sk ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card(tr("LIGHTGBM"), lgbm ? tr("OK") : tr("MISSING"), this,
                         lgbm ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("XGBOOST"), xgb ? tr("OK") : tr("MISSING"), this,
                         xgb ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> extras = {
            gs_make_card(tr("CATBOOST"), cat ? tr("OK") : tr("MISSING"), this,
                         cat ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("QLIB"), payload.value("qlib_available").toBool() ? tr("OK") : tr("MISSING"), this,
                         payload.value("qlib_available").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("COUNT"), fmt_int_safe(payload.value("count")), this),
            gs_make_card(tr("LIBRARIES"),
                         QString::number(int(sk) + int(lgbm) + int(xgb) + int(cat) +
                                         int(payload.value("qlib_available").toBool())),
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(extras, this));

        if (!models.isEmpty()) {
            auto* table = new QTableWidget(models.size(), 4, this);
            table->setHorizontalHeaderLabels({tr("ID"), tr("Name"), tr("Type"), tr("Library")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * row_px(26) + row_px(32), 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("id").toString()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("name").toString()));
                table->setItem(i, 2, new QTableWidgetItem(m.value("type").toString().toUpper()));
                table->setItem(i, 3, new QTableWidgetItem(m.value("library").toString()));
                table->setRowHeight(i, row_px(26));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 model(s) available").arg(models.size()));
        return;
    }

    // ── Shared pieces for the market-data results (selection / ensemble / tuning) ──
    // Hold-out metric columns per task; null renders as an em dash, never 0.0000.
    const auto metric_cols = [](bool is_classification) {
        return is_classification ? QList<QPair<QString, QString>>{{QStringLiteral("accuracy"), tr("Accuracy")},
                                                                 {QStringLiteral("f1_score"), tr("F1")},
                                                                 {QStringLiteral("auc_roc"), tr("AUC-ROC")}}
                                 : QList<QPair<QString, QString>>{{QStringLiteral("r2_score"), tr("R²")},
                                                                 {QStringLiteral("rmse"), tr("RMSE")},
                                                                 {QStringLiteral("directional_accuracy"),
                                                                  tr("Dir. Hit Rate")},
                                                                 {QStringLiteral("ic"), tr("IC")}};
    };
    const auto metric_cell = [](const QJsonValue& v) -> QTableWidgetItem* {
        if (!v.isDouble()) {
            auto* na = new QTableWidgetItem(QString::fromUtf8("—"));
            na->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return na;
        }
        return num_item(QString::number(v.toDouble(), 'f', 4), v.toDouble());
    };
    const auto score_text = [](const QJsonValue& v, bool is_classification) {
        if (!v.isDouble())
            return QString::fromUtf8("—");
        return is_classification ? QString::number(v.toDouble() * 100.0, 'f', 1) + QLatin1Char('%')
                                 : QString::number(v.toDouble(), 'f', 4);
    };
    const auto period_text = [](const QJsonValue& v) {
        const QJsonArray a = v.toArray();
        return a.size() == 2 ? QStringLiteral("%1 → %2").arg(a[0].toString(), a[1].toString())
                             : QString::fromUtf8("—");
    };
    // Where the numbers come from: ticker, windows and target, then the row counts.
    const auto add_data_block = [this, &accent, &period_text](const QJsonObject& data) {
        if (data.isEmpty())
            return;
        results_layout_->addWidget(gs_section_header(tr("DATA  |  %1  |  train %2  |  hold-out %3  |  target: %4")
                                                         .arg(data.value("ticker").toString(),
                                                              period_text(data.value("train_period")),
                                                              period_text(data.value("test_period")),
                                                              data.value("target").toString()),
                                                     accent));
        results_layout_->addWidget(
            qc_caption(tr("%1 daily bars (%2 → %3)  ·  %4 train / %5 hold-out rows  ·  %6 rows purged at the split so "
                          "no training label overlaps the hold-out  ·  %7")
                           .arg(QString::number(data.value("bars").toInt()), data.value("start_date").toString(),
                                data.value("end_date").toString(), QString::number(data.value("train_rows").toInt()),
                                QString::number(data.value("test_rows").toInt()),
                                QString::number(data.value("purged_rows").toInt()),
                                data.value("source").toString())));
    };
    // One table: a row per model plus the naive baseline as the last row.
    const auto add_score_table = [this, &metric_cols, &metric_cell](const QJsonArray& rows, bool is_classification,
                                                                    const QJsonObject& baseline,
                                                                    const QString& first_header,
                                                                    const QString& first_key) {
        const auto cols = metric_cols(is_classification);
        QStringList headers = {first_header, tr("Model")};
        for (const auto& c : cols)
            headers << c.second;
        const int n = static_cast<int>(rows.size()) + (baseline.isEmpty() ? 0 : 1);
        auto* table = new QTableWidget(n, headers.size(), this);
        table->setHorizontalHeaderLabels(headers);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(qMin(n * row_px(26) + row_px(32), 360));
        for (int i = 0; i < rows.size(); ++i) {
            const auto r = rows[i].toObject();
            const auto metrics = r.value("metrics").toObject();
            auto* first = new QTableWidgetItem(r.value(first_key).toString());
            first->setFlags(first->flags() | Qt::ItemIsSelectable); // model keys are copied into the Ensemble tab
            table->setItem(i, 0, first);
            table->setItem(i, 1, new QTableWidgetItem(r.value("model_id").toString()));
            for (int c = 0; c < cols.size(); ++c)
                table->setItem(i, 2 + c, metric_cell(metrics.value(cols[c].first)));
            table->setRowHeight(i, row_px(26));
        }
        if (!baseline.isEmpty()) {
            const int i = static_cast<int>(rows.size());
            table->setItem(i, 0, new QTableWidgetItem(tr("baseline")));
            table->setItem(i, 1, new QTableWidgetItem(baseline.value("name").toString()));
            for (int c = 0; c < cols.size(); ++c)
                table->setItem(i, 2 + c, metric_cell(baseline.value(cols[c].first)));
            table->setRowHeight(i, row_px(26));
        }
        finish_table(table);
        results_layout_->addWidget(table);
    };

    if (command == "run_selection") {
        const QString best = payload.value("best_model").toString();
        const QString task = payload.value("task_type").toString();
        const bool is_classification = task == QLatin1String("classification");
        const auto ranking = payload.value("ranking").toArray();
        const QJsonObject base = payload.value("baseline").toObject();
        const bool beats = payload.value("beats_baseline").toBool();
        const QJsonValue best_score = ranking.isEmpty() ? QJsonValue() : ranking.first().toObject().value("score");
        const QJsonValue base_score = base.value(is_classification ? "accuracy" : "r2_score");

        QList<QWidget*> top = {
            gs_make_card(tr("BEST MODEL"), best.isEmpty() ? QString::fromUtf8("—") : best, this),
            gs_make_card(is_classification ? tr("HOLD-OUT ACCURACY") : tr("HOLD-OUT R²"),
                         score_text(best_score, is_classification), this),
            gs_make_card(is_classification ? tr("BASELINE ACCURACY") : tr("BASELINE R²"),
                         score_text(base_score, is_classification), this),
            gs_make_card(tr("VERDICT"), beats ? tr("BEATS BASELINE") : tr("NO EDGE"), this,
                         beats ? ui::colors::SUCCESS() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        add_data_block(payload.value("data").toObject());

        if (!ranking.isEmpty()) {
            results_layout_->addWidget(gs_section_header(
                tr("RANKING  |  %1  |  by %2 on the hold-out window")
                    .arg(task.toUpper(), is_classification ? tr("accuracy") : tr("R²")),
                accent));
            add_score_table(ranking, is_classification, base, tr("Model Key"), QStringLiteral("model_key"));
        }
        // Per-model failures (a library missing, a fit error) are listed, not dropped.
        QStringList failed;
        const QJsonObject per_model = payload.value("results").toObject();
        for (auto it = per_model.begin(); it != per_model.end(); ++it)
            if (!it.value().toObject().value("trained").toBool())
                failed << QStringLiteral("%1: %2").arg(it.key(), it.value().toObject().value("error").toString());
        if (!failed.isEmpty())
            results_layout_->addWidget(qc_caption(tr("NOT TRAINED  ·  %1").arg(failed.join(QStringLiteral("  ·  ")))));
        status_label_->setText(tr("Best: %1 — %2 %3 vs baseline %4")
                                   .arg(best.isEmpty() ? QString::fromUtf8("—") : best,
                                        is_classification ? tr("accuracy") : tr("R²"),
                                        score_text(best_score, is_classification),
                                        score_text(base_score, is_classification)));
        return;
    }

    if (command == "create_ensemble") {
        const QString task = payload.value("task_type").toString();
        const bool is_classification = task == QLatin1String("classification");
        const QJsonObject base = payload.value("baseline").toObject();
        const QJsonValue score = payload.value("score");
        const QJsonValue best_member = payload.value("best_member_score");
        const QJsonValue base_score = base.value(is_classification ? "accuracy" : "r2_score");
        const bool beats_members = payload.value("beats_best_member").toBool();

        QList<QWidget*> top = {
            gs_make_card(tr("ENSEMBLE ID"), payload.value("ensemble_id").toString(), this),
            gs_make_card(tr("METHOD"), payload.value("method").toString().toUpper(), this),
            gs_make_card(tr("MEMBERS"), fmt_int_safe(payload.value("n_models")), this),
            gs_make_card(tr("TASK"), task.toUpper(), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        QList<QWidget*> scores = {
            gs_make_card(is_classification ? tr("ENSEMBLE ACCURACY") : tr("ENSEMBLE R²"),
                         score_text(score, is_classification), this),
            gs_make_card(tr("BEST MEMBER"), score_text(best_member, is_classification), this),
            gs_make_card(tr("BASELINE"), score_text(base_score, is_classification), this),
            gs_make_card(tr("VERDICT"), beats_members ? tr("BEATS ITS MEMBERS") : tr("NO GAIN OVER BEST MEMBER"),
                         this, beats_members ? ui::colors::SUCCESS() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(scores, this));
        add_data_block(payload.value("data").toObject());

        // Members (their own hold-out scores) and the fitted ensemble, on the same hold-out window.
        QJsonArray rows = payload.value("members").toArray();
        rows.append(QJsonObject{{"model_key", payload.value("ensemble_id")},
                                {"model_id", tr("%1 ensemble").arg(payload.value("method").toString())},
                                {"metrics", payload.value("metrics")}});
        results_layout_->addWidget(gs_section_header(tr("HOLD-OUT SCORES  |  members, ensemble, baseline"), accent));
        add_score_table(rows, is_classification, base, tr("Model Key"), QStringLiteral("model_key"));
        status_label_->setText(tr("%1 ensemble: %2 vs best member %3")
                                   .arg(payload.value("method").toString(), score_text(score, is_classification),
                                        score_text(best_member, is_classification)));
        return;
    }

    if (command == "tune_hyperparameters") {
        const auto best_params = payload.value("best_params").toObject();
        const QString method = payload.value("search_method").toString();
        const QString task = payload.value("task_type").toString();
        const bool is_classification = task == QLatin1String("classification");
        const QJsonObject hold = payload.value("holdout_metrics").toObject();
        const QJsonObject base = payload.value("baseline").toObject();
        const QJsonValue hold_score = hold.value(is_classification ? "accuracy" : "r2_score");
        const QJsonValue base_score = base.value(is_classification ? "accuracy" : "r2_score");
        const bool beats = hold_score.isDouble() && base_score.isDouble() && hold_score.toDouble() > base_score.toDouble();

        QList<QWidget*> top = {
            gs_make_card(tr("MODEL"), payload.value("model_id").toString(), this),
            gs_make_card(tr("CV SCORE (%1)").arg(payload.value("scoring").toString().toUpper()),
                         fmt_num_safe(payload.value("best_score"), 4), this),
            gs_make_card(tr("SEARCH"), tr("%1 · %2 candidate(s)")
                                           .arg(method.toUpper())
                                           .arg(payload.value("n_candidates").toInt()),
                         this),
            gs_make_card(tr("FOLDS"), payload.value("cv_method").toString(fmt_int_safe(payload.value("cv_folds"))),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        QList<QWidget*> holdout = {
            gs_make_card(is_classification ? tr("HOLD-OUT ACCURACY") : tr("HOLD-OUT R²"),
                         score_text(hold_score, is_classification), this),
            gs_make_card(tr("BASELINE"), score_text(base_score, is_classification), this),
            gs_make_card(tr("VERDICT"), beats ? tr("BEATS BASELINE") : tr("NO EDGE"), this,
                         beats ? ui::colors::SUCCESS() : ui::colors::WARNING()),
            gs_make_card(tr("MODEL KEY"), payload.value("model_key").toString(QStringLiteral("—")), this),
        };
        results_layout_->addWidget(gs_card_row(holdout, this));
        add_data_block(payload.value("data").toObject());

        // Tuned model vs the naive baseline on the hold-out window.
        add_score_table(QJsonArray{QJsonObject{{"model_key", payload.value("model_key")},
                                               {"model_id", tr("%1 (tuned)").arg(payload.value("model_id").toString())},
                                               {"metrics", hold}}},
                        is_classification, base, tr("Model Key"), QStringLiteral("model_key"));

        // Score of the chosen parameters in each chronological fold.
        const QJsonArray folds = payload.value("cv_scores").toArray();
        if (!folds.isEmpty()) {
            results_layout_->addWidget(gs_section_header(
                tr("WALK-FORWARD CV  |  best parameters, %1 per fold").arg(payload.value("scoring").toString()),
                accent));
            auto* ft = new QTableWidget(1, folds.size(), this);
            QStringList fh;
            for (int i = 0; i < folds.size(); ++i) {
                fh << tr("Fold %1").arg(i + 1);
                ft->setItem(0, i, metric_cell(folds[i]));
            }
            ft->setHorizontalHeaderLabels(fh);
            ft->verticalHeader()->setVisible(false);
            ft->setEditTriggers(QAbstractItemView::NoEditTriggers);
            ft->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            ft->setStyleSheet(table_ss());
            ft->setMaximumHeight(row_px(26) + row_px(32));
            ft->setRowHeight(0, row_px(26));
            results_layout_->addWidget(ft);
        }

        if (!best_params.isEmpty()) {
            results_layout_->addWidget(gs_section_header(tr("BEST PARAMETERS"), accent));
            auto* table = new QTableWidget(best_params.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Hyperparameter"), tr("Value")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(best_params.size() * row_px(24) + row_px(32), 280));
            int r = 0;
            for (auto it = best_params.begin(); it != best_params.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                auto* vi = new QTableWidgetItem(format_val(it.value()));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                table->setRowHeight(r, row_px(24));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 search: CV %2, hold-out %3 vs baseline %4")
                                   .arg(method, fmt_num_safe(payload.value("best_score"), 4),
                                        score_text(hold_score, is_classification),
                                        score_text(base_score, is_classification)));
        return;
    }

    if (command == "get_results") {
        const auto results = payload.value("results").toObject();
        const QString best = payload.value("best_model").toString();
        const int n_models = payload.value("n_models").toInt();

        QList<QWidget*> top = {
            gs_make_card(tr("RESULTS"), QString::number(results.size()), this),
            gs_make_card(tr("LAST BEST MODEL"), best.isEmpty() ? QString::fromUtf8("—") : best, this),
            gs_make_card(tr("STORED MODELS"), QString::number(n_models), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (results.isEmpty()) {
            auto* es = ui::EmptyState::create(
                ui::EmptyState::no_data_spec(tr("No trained models yet"),
                                             tr("Model Selection trains and ranks the candidates on one ticker; its "
                                                "results (and every tuning run) are listed here."),
                                             tr("RUN MODEL SELECTION")),
                results_layout_->parentWidget());
            es->set_compact(true);
            es->set_alignment(Qt::AlignTop);
            connect(es, &ui::EmptyState::primary_clicked, this,
                    [this]() { open_view(QStringLiteral("model_selection")); });
            results_layout_->addWidget(es);
        }
        if (!results.isEmpty()) {
            // Regression and classification results sit side by side now (latest per model and
            // task), so each row carries its own task and headline score.
            const QStringList headers = {tr("Model Key"), tr("Model ID"), tr("Task"),    tr("Ticker"),
                                         tr("Horizon"),   tr("Score"),    tr("Hold-out"), tr("Train N"),
                                         tr("Test N")};
            auto* table = new QTableWidget(results.size(), headers.size(), this);
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(results.size() * row_px(26) + row_px(32), 360));

            int r = 0;
            for (auto it = results.begin(); it != results.end(); ++it, ++r) {
                const auto obj = it.value().toObject();
                const auto metrics = obj.value("metrics").toObject();
                const auto data = obj.value("data").toObject();
                const bool cls = obj.value("task_type").toString() == QLatin1String("classification");
                const QJsonValue score = metrics.value(cls ? "accuracy" : "r2_score");
                const QString key = it.key();
                table->setItem(r, 0, new QTableWidgetItem(key == best ? tr("%1  (best)").arg(key) : key));
                table->setItem(r, 1, new QTableWidgetItem(obj.value("model_id").toString()));
                table->setItem(r, 2, new QTableWidgetItem(obj.value("task_type").toString()));
                table->setItem(r, 3, new QTableWidgetItem(data.value("ticker").toString(QStringLiteral("—"))));
                table->setItem(r, 4, new QTableWidgetItem(data.contains("horizon")
                                                              ? tr("%1 d").arg(data.value("horizon").toInt())
                                                              : QString::fromUtf8("—")));
                auto* si = score.isDouble()
                               ? num_item((cls ? tr("acc ") : tr("R² ")) + QString::number(score.toDouble(), 'f', 4),
                                          score.toDouble())
                               : new QTableWidgetItem(QString::fromUtf8("—"));
                si->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 5, si);
                table->setItem(r, 6, new QTableWidgetItem(period_text(data.value("test_period"))));
                table->setItem(r, 7, num_item(QString::number(obj.value("n_train_samples").toInt()),
                                              obj.value("n_train_samples").toInt()));
                table->setItem(r, 8, num_item(QString::number(obj.value("n_test_samples").toInt()),
                                              obj.value("n_test_samples").toInt()));
                table->setRowHeight(r, row_px(26));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 result(s)  best=%2").arg(results.size()).arg(best));
        return;
    }

    display_result(payload);
}

} // namespace fincept::screens
