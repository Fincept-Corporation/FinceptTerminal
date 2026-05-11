// src/screens/ai_quant_lab/QuantModulePanel_AIMLDisplays.cpp
//
// Card-based result renderers for AI/ML category panels that previously
// rendered plain QLabel text into the bottom results pane.
//
//   - rl_trading       — train (streaming progress + final result), evaluate
//   - online_learning  — list_models, create_model, train, predict, performance
//   - meta_learning    — list_models, run_selection, create_ensemble,
//                        tune_hyperparameters, get_results
//
// Skipped:
//   - deep_agent — has bespoke per-tab UI (LangGraph + RD-Agent text outputs,
//     task table widget). Handled by legacy on_result branch.
//   - pattern_intelligence — module declared but no panel/service wired yet.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
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

namespace {

[[maybe_unused]] QString fmt_num_safe(const QJsonValue& v, int decimals = 4) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals);
}

QString fmt_int_safe(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toInt());
}

QString fmt_pct_safe(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble() * 100.0, 'f', decimals) + "%";
}

// Returns false if the payload was an error (already displayed via callback).
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

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// 1. RL TRADING
// ═════════════════════════════════════════════════════════════════════════════
//
// Streaming progress / log events are consumed by AIQuantLabService's
// training_progress / training_log signals (NOT result_ready) — they update
// the bespoke rl_progress_bar_ / rl_log_console_ widgets directly. By the time
// payloads reach this renderer they are the FINAL `event:"result"` payload of
// `train` (with the event field stripped by the service) or the single-shot
// `evaluate` result.
void QuantModulePanel::display_rl_trading_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (payload.contains("algorithm"))
        header_text += "  |  " + payload.value("algorithm").toString().toUpper();
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── train: minimal final-result card row ─────────────────────────────────
    if (command == "train") {
        QList<QWidget*> top = {
            gs_make_card("ALGORITHM", payload.value("algorithm").toString().toUpper(), this, ui::colors::POSITIVE()),
            gs_make_card("TIMESTEPS", fmt_int_safe(payload.value("timesteps")), this),
            gs_make_card("STATUS", "TRAINED", this, ui::colors::POSITIVE()),
            gs_make_card("MESSAGE", payload.value("message").toString().left(40), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        const QString msg = payload.value("message").toString();
        if (!msg.isEmpty()) {
            auto* lbl = new QLabel(msg);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:11px; padding:8px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText(QString("Trained %1 — %2 steps")
                                   .arg(payload.value("algorithm").toString())
                                   .arg(payload.value("timesteps").toInt()));
        return;
    }

    // ── evaluate: rich card view of episode metrics ──────────────────────────
    if (command == "evaluate") {
        const double mean_reward = payload.value("mean_reward").toDouble();
        const double std_reward = payload.value("std_reward").toDouble();
        const double mean_pv = payload.value("mean_portfolio_value").toDouble();
        const double port_ret = payload.value("portfolio_return").toDouble();
        const int n_eps = payload.value("n_episodes").toInt();
        const double mean_len = payload.value("mean_length").toDouble();

        QList<QWidget*> top = {
            gs_make_card("EPISODES", QString::number(n_eps), this, ui::colors::POSITIVE()),
            gs_make_card("MEAN REWARD",
                         QString::number(mean_reward, 'f', 4) + " ± " + QString::number(std_reward, 'f', 4),
                         this, gs_pos_neg_color(mean_reward)),
            gs_make_card("PORTFOLIO RETURN", fmt_pct_safe(QJsonValue(port_ret), 2), this, gs_pos_neg_color(port_ret)),
            gs_make_card("MEAN PORTFOLIO VAL", QString::number(mean_pv, 'f', 2), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Compute best/worst episode rewards once
        const auto rewards = payload.value("all_rewards").toArray();
        double best_ep = -std::numeric_limits<double>::infinity();
        double worst_ep = std::numeric_limits<double>::infinity();
        for (const auto& v : rewards) {
            const double d = v.toDouble();
            best_ep = std::max(best_ep, d);
            worst_ep = std::min(worst_ep, d);
        }
        const bool has_eps = !rewards.isEmpty();

        QList<QWidget*> stats = {
            gs_make_card("MEAN EP LENGTH", QString::number(mean_len, 'f', 1) + " steps", this),
            gs_make_card("REWARD STD", QString::number(std_reward, 'f', 4), this),
            gs_make_card("BEST EP",
                         has_eps ? QString::number(best_ep, 'f', 4) : QString("—"),
                         this, ui::colors::POSITIVE()),
            gs_make_card("WORST EP",
                         has_eps ? QString::number(worst_ep, 'f', 4) : QString("—"),
                         this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(stats, this));

        // Per-episode rewards table (sampled if many episodes)
        if (!rewards.isEmpty()) {
            const int rows = std::min<int>(15, rewards.size());
            const int step = std::max<int>(1, rewards.size() / rows);
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({"Episode", "Reward"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < rewards.size() && r < rows; i += step, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i + 1)));
                const double v = rewards[i].toDouble();
                auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                it->setForeground(QColor(gs_pos_neg_color(v)));
                table->setItem(r, 1, it);
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Eval %1 eps  reward=%2  return=%3%")
                                   .arg(n_eps).arg(mean_reward, 0, 'f', 4).arg(port_ret * 100, 0, 'f', 2));
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

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "list_models") {
        const auto models = payload.value("models").toArray();
        const bool river = payload.value("river_available").toBool();
        const bool qlib_online = payload.value("qlib_online_available").toBool();

        QList<QWidget*> top = {
            gs_make_card("MODELS", QString::number(models.size()), this,
                         models.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::POSITIVE()),
            gs_make_card("RIVER", river ? "AVAILABLE" : "MISSING", this,
                         river ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("QLIB ONLINE", qlib_online ? "AVAILABLE" : "MISSING", this,
                         qlib_online ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("COUNT", fmt_int_safe(payload.value("count")), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!models.isEmpty()) {
            auto* table = new QTableWidget(models.size(), 5, this);
            table->setHorizontalHeaderLabels({"Model ID", "Type", "Samples", "Created", "Last Updated"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * 26 + 32, 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("model_id").toString()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("type").toString().toUpper()));
                auto* s = new QTableWidgetItem(QString::number(m.value("samples_trained").toInt()));
                s->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 2, s);
                table->setItem(i, 3, new QTableWidgetItem(m.value("created_at").toString()));
                const QJsonValue lu = m.value("last_updated");
                table->setItem(i, 4, new QTableWidgetItem(lu.isNull() ? "never" : lu.toString()));
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 online model(s)").arg(models.size()));
        return;
    }

    if (command == "create_model") {
        QList<QWidget*> top = {
            gs_make_card("MODEL ID", payload.value("model_id").toString(), this, ui::colors::POSITIVE()),
            gs_make_card("TYPE", payload.value("model_type").toString().toUpper(), this),
            gs_make_card("STATUS", "CREATED", this, ui::colors::POSITIVE()),
            gs_make_card("MESSAGE", payload.value("message").toString("OK"), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText(payload.value("message").toString("Online model created"));
        return;
    }

    if (command == "train") {
        const QJsonValue pred = payload.value("prediction");
        const QJsonValue actual = payload.value("actual");
        const double err = payload.value("error").toDouble();
        const double mae = payload.value("current_mae").toDouble();
        const int samples = payload.value("samples_trained").toInt();
        const bool drift = payload.value("drift_detected").toBool();

        QList<QWidget*> top = {
            gs_make_card("MODEL", payload.value("model_id").toString(), this),
            gs_make_card("SAMPLES", QString::number(samples), this, ui::colors::POSITIVE()),
            gs_make_card("CURRENT MAE", QString::number(mae, 'f', 6), this,
                         mae < 0.01 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("DRIFT", drift ? "DETECTED" : "NONE", this,
                         drift ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!pred.isNull() && !actual.isNull()) {
            QList<QWidget*> pred_row = {
                gs_make_card("PREDICTION", QString::number(pred.toDouble(), 'f', 6), this,
                             gs_pos_neg_color(pred.toDouble())),
                gs_make_card("ACTUAL", QString::number(actual.toDouble(), 'f', 6), this,
                             gs_pos_neg_color(actual.toDouble())),
                gs_make_card("ERROR", QString::number(err, 'f', 6), this,
                             std::abs(err) < std::abs(actual.toDouble()) * 0.1
                                 ? ui::colors::POSITIVE()
                                 : ui::colors::WARNING()),
                gs_make_card("ABS ERROR", QString::number(std::abs(err), 'f', 6), this),
            };
            results_layout_->addWidget(gs_card_row(pred_row, this));
        }
        status_label_->setText(drift ? "⚠ Drift detected"
                                     : QString("MAE: %1").arg(mae, 0, 'f', 6));
        return;
    }

    if (command == "predict") {
        const QJsonValue pred = payload.value("prediction");
        const auto preds = payload.value("predictions").toArray();
        const int count = payload.value("count").toInt();

        if (preds.isEmpty() && !pred.isNull()) {
            // Single prediction
            QList<QWidget*> top = {
                gs_make_card("MODEL", payload.value("model_id").toString(), this),
                gs_make_card("PREDICTION", QString::number(pred.toDouble(), 'f', 6), this,
                             gs_pos_neg_color(pred.toDouble())),
            };
            results_layout_->addWidget(gs_card_row(top, this));
            status_label_->setText("Prediction ready");
            return;
        }

        // Batch predictions
        double sum = 0, mn = std::numeric_limits<double>::infinity(),
               mx = -std::numeric_limits<double>::infinity();
        for (const auto& v : preds) {
            const double d = v.toDouble();
            sum += d;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
        }
        const double mean = preds.isEmpty() ? 0.0 : sum / preds.size();

        QList<QWidget*> top = {
            gs_make_card("MODEL", payload.value("model_id").toString(), this),
            gs_make_card("PREDICTIONS", QString::number(count > 0 ? count : preds.size()), this,
                         ui::colors::POSITIVE()),
            gs_make_card("MEAN", QString::number(mean, 'f', 6), this, gs_pos_neg_color(mean)),
            gs_make_card("RANGE",
                         preds.isEmpty() ? "—"
                             : QString("[%1, %2]").arg(mn, 0, 'f', 4).arg(mx, 0, 'f', 4),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

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
        status_label_->setText(QString("%1 prediction(s)").arg(count > 0 ? count : preds.size()));
        return;
    }

    if (command == "performance") {
        const double mae = payload.value("current_mae").toDouble();
        const int samples = payload.value("samples_trained").toInt();
        const bool drift = payload.value("drift_detected").toBool();
        const QJsonValue lu = payload.value("last_updated");

        QList<QWidget*> top = {
            gs_make_card("MODEL", payload.value("model_id").toString(), this),
            gs_make_card("TYPE", payload.value("model_type").toString().toUpper(), this),
            gs_make_card("SAMPLES", QString::number(samples), this, ui::colors::POSITIVE()),
            gs_make_card("CURRENT MAE", QString::number(mae, 'f', 6), this,
                         mae < 0.01 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> meta = {
            gs_make_card("DRIFT", drift ? "DETECTED" : "NONE", this,
                         drift ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()),
            gs_make_card("LAST UPDATED", lu.isNull() ? "never" : lu.toString(), this),
            gs_make_card("CREATED", payload.value("created_at").toString(), this),
            gs_make_card("HEALTH", drift ? "RETRAIN ADVISED" : "OK", this,
                         drift ? ui::colors::WARNING() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(meta, this));
        status_label_->setText(QString("MAE: %1  samples: %2").arg(mae, 0, 'f', 6).arg(samples));
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

    const QString accent = module_.color.name();
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
            gs_make_card("MODELS", QString::number(models.size()), this, ui::colors::POSITIVE()),
            gs_make_card("SKLEARN", sk ? "OK" : "MISSING", this,
                         sk ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("LIGHTGBM", lgbm ? "OK" : "MISSING", this,
                         lgbm ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("XGBOOST", xgb ? "OK" : "MISSING", this,
                         xgb ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> extras = {
            gs_make_card("CATBOOST", cat ? "OK" : "MISSING", this,
                         cat ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("QLIB", payload.value("qlib_available").toBool() ? "OK" : "MISSING", this,
                         payload.value("qlib_available").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("COUNT", fmt_int_safe(payload.value("count")), this),
            gs_make_card("LIBRARIES",
                         QString::number(int(sk) + int(lgbm) + int(xgb) + int(cat)
                                         + int(payload.value("qlib_available").toBool())),
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(extras, this));

        if (!models.isEmpty()) {
            auto* table = new QTableWidget(models.size(), 4, this);
            table->setHorizontalHeaderLabels({"ID", "Name", "Type", "Library"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * 26 + 32, 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("id").toString()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("name").toString()));
                table->setItem(i, 2, new QTableWidgetItem(m.value("type").toString().toUpper()));
                table->setItem(i, 3, new QTableWidgetItem(m.value("library").toString()));
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 model(s) available").arg(models.size()));
        return;
    }

    if (command == "run_selection") {
        const QString best = payload.value("best_model").toString();
        const int trained = payload.value("trained_count").toInt();
        const QString task = payload.value("task_type").toString();
        const auto ranking = payload.value("ranking").toArray();

        QList<QWidget*> top = {
            gs_make_card("BEST MODEL", best.isEmpty() ? "—" : best, this, ui::colors::POSITIVE()),
            gs_make_card("TRAINED", QString::number(trained), this),
            gs_make_card("TASK TYPE", task.toUpper(), this, ui::colors::INFO()),
            gs_make_card("RANKED", QString::number(ranking.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Determine task type from ranking metrics
        bool is_classification = false;
        if (!ranking.isEmpty()) {
            const auto first_metrics = ranking.first().toObject().value("metrics").toObject();
            is_classification = first_metrics.contains("accuracy");
        }

        if (!ranking.isEmpty()) {
            QStringList headers = is_classification
                ? QStringList{"Rank", "Model ID", "Score", "Accuracy", "F1", "AUC-ROC"}
                : QStringList{"Rank", "Model ID", "Score", "R²", "RMSE", "MSE"};
            QStringList metric_keys = is_classification
                ? QStringList{"accuracy", "f1_score", "auc_roc"}
                : QStringList{"r2_score", "rmse", "mse"};

            auto* table = new QTableWidget(ranking.size(), headers.size(), this);
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(ranking.size() * 26 + 32, 360));

            for (int i = 0; i < ranking.size(); ++i) {
                const auto r = ranking[i].toObject();
                const QString mid = r.value("model_id").toString();
                const auto metrics = r.value("metrics").toObject();
                const double score = r.value("score").toDouble();

                auto* rank_it = new QTableWidgetItem(QString::number(i + 1));
                rank_it->setTextAlignment(Qt::AlignCenter);
                if (i == 0) rank_it->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(i, 0, rank_it);

                auto* mid_it = new QTableWidgetItem(mid);
                if (mid == best) mid_it->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(i, 1, mid_it);

                auto* score_it = new QTableWidgetItem(QString::number(score, 'f', 4));
                score_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                score_it->setForeground(QColor(gs_pos_neg_color(score)));
                table->setItem(i, 2, score_it);

                for (int c = 0; c < metric_keys.size(); ++c) {
                    const double v = metrics.value(metric_keys[c]).toDouble();
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (metric_keys[c] == "accuracy" || metric_keys[c] == "r2_score") {
                        if (v > 0.7) it->setForeground(QColor(ui::colors::POSITIVE()));
                        else if (v < 0) it->setForeground(QColor(ui::colors::NEGATIVE()));
                    }
                    table->setItem(i, c + 3, it);
                }
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Best: %1 (%2 model(s) trained)").arg(best).arg(trained));
        return;
    }

    if (command == "create_ensemble") {
        const auto keys = payload.value("model_keys").toArray();
        QStringList key_list;
        for (const auto& v : keys) key_list << v.toString();

        QList<QWidget*> top = {
            gs_make_card("ENSEMBLE ID", payload.value("ensemble_id").toString(), this, ui::colors::POSITIVE()),
            gs_make_card("METHOD", payload.value("method").toString().toUpper(), this, ui::colors::INFO()),
            gs_make_card("MODELS", fmt_int_safe(payload.value("n_models")), this, ui::colors::POSITIVE()),
            gs_make_card("STATUS", "CREATED", this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!key_list.isEmpty()) {
            auto* lbl = new QLabel("Composition: " + key_list.join("  +  "));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:11px;"
                                       "padding:8px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText("Ensemble created");
        return;
    }

    if (command == "tune_hyperparameters") {
        const double best_score = payload.value("best_score").toDouble();
        const auto best_params = payload.value("best_params").toObject();
        const QString method = payload.value("search_method").toString();

        QList<QWidget*> top = {
            gs_make_card("MODEL", payload.value("model_id").toString(), this),
            gs_make_card("BEST SCORE", QString::number(best_score, 'f', 4), this,
                         best_score > 0.7 ? ui::colors::POSITIVE()
                                          : best_score > 0 ? ui::colors::WARNING()
                                                           : ui::colors::NEGATIVE()),
            gs_make_card("SEARCH METHOD", method.toUpper(), this, ui::colors::INFO()),
            gs_make_card("CV FOLDS", fmt_int_safe(payload.value("cv_folds")), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!best_params.isEmpty()) {
            results_layout_->addWidget(gs_section_header("BEST PARAMETERS", accent));
            auto* table = new QTableWidget(best_params.size(), 2, this);
            table->setHorizontalHeaderLabels({"Hyperparameter", "Value"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(best_params.size() * 24 + 32, 280));
            int r = 0;
            for (auto it = best_params.begin(); it != best_params.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                auto* vi = new QTableWidgetItem(format_val(it.value()));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1: best score %2")
                                   .arg(method).arg(best_score, 0, 'f', 4));
        return;
    }

    if (command == "get_results") {
        const auto results = payload.value("results").toObject();
        const QString best = payload.value("best_model").toString();
        const int n_models = payload.value("n_models").toInt();

        QList<QWidget*> top = {
            gs_make_card("RESULTS", QString::number(results.size()), this, ui::colors::POSITIVE()),
            gs_make_card("BEST MODEL", best.isEmpty() ? "—" : best, this, ui::colors::POSITIVE()),
            gs_make_card("N MODELS", QString::number(n_models), this),
            gs_make_card("KEYS", QString::number(results.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!results.isEmpty()) {
            // Determine task type from first result's metrics
            bool is_classification = false;
            const auto first = results.begin().value().toObject();
            const auto first_metrics = first.value("metrics").toObject();
            if (first_metrics.contains("accuracy")) is_classification = true;

            QStringList headers = is_classification
                ? QStringList{"Model Key", "Model ID", "Accuracy", "F1", "Train N", "Test N"}
                : QStringList{"Model Key", "Model ID", "R²", "RMSE", "Train N", "Test N"};
            QStringList metric_keys = is_classification
                ? QStringList{"accuracy", "f1_score"}
                : QStringList{"r2_score", "rmse"};

            auto* table = new QTableWidget(results.size(), headers.size(), this);
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(results.size() * 26 + 32, 360));

            int r = 0;
            for (auto it = results.begin(); it != results.end(); ++it, ++r) {
                const QString key = it.key();
                const auto obj = it.value().toObject();
                const auto metrics = obj.value("metrics").toObject();

                auto* k = new QTableWidgetItem(key);
                if (key == best || obj.value("model_id").toString() == best)
                    k->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(r, 0, k);
                table->setItem(r, 1, new QTableWidgetItem(obj.value("model_id").toString()));

                for (int c = 0; c < metric_keys.size(); ++c) {
                    const double v = metrics.value(metric_keys[c]).toDouble();
                    auto* mi = new QTableWidgetItem(QString::number(v, 'f', 4));
                    mi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if ((metric_keys[c] == "accuracy" || metric_keys[c] == "r2_score") && v > 0.7)
                        mi->setForeground(QColor(ui::colors::POSITIVE()));
                    table->setItem(r, c + 2, mi);
                }
                auto* tn = new QTableWidgetItem(QString::number(obj.value("n_train_samples").toInt()));
                tn->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, headers.size() - 2, tn);
                auto* en = new QTableWidgetItem(QString::number(obj.value("n_test_samples").toInt()));
                en->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, headers.size() - 1, en);
                table->setRowHeight(r, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 result(s)  best=%2").arg(results.size()).arg(best));
        return;
    }

    display_result(payload);
}

} // namespace fincept::screens
