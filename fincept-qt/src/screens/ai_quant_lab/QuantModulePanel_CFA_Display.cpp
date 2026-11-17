// src/screens/ai_quant_lab/QuantModulePanel_CFA_Display.cpp
//
// CFA Quant — result rendering. Hosts the per-command display branches of
// display_cfa_result (13 commands) plus the file-local cfa_flatten_scalars
// helper and the fmt_* utilities used only inside this TU.
//
// Part of the partial-class split of QuantModulePanel_CFA.cpp.

// keep that file maintainable.
#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPair>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;
using namespace fincept::screens::quant_charts;

namespace {

// Fallback flattener — only used for unknown commands.
void cfa_flatten_scalars(const QJsonObject& obj, const QString& prefix, QStringList& keys,
                         QHash<QString, QJsonValue>& out) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const auto& v = it.value();
        const QString key = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
        if (v.isObject()) {
            if (key.endsWith(".data") || key.endsWith(".fitted_values") || key.endsWith(".residuals") ||
                key.endsWith(".model_summary") || key.endsWith(".predictions") || key.endsWith(".sample_means") ||
                key.endsWith(".resampled_statistics") || key.endsWith(".cluster_labels") ||
                key.endsWith(".principal_components") || key.endsWith(".cluster_centers"))
                continue;
            cfa_flatten_scalars(v.toObject(), key, keys, out);
        } else if (!v.isArray()) {
            keys.append(key);
            out.insert(key, v);
        }
    }
}

// The same helper as QuantModulePanel_AdvancedHelpers.h (one definition: in a unity build this
// file's own copy was ambiguous with that one in files using the helpers' namespace).
using fincept::screens::quant_advanced_helpers::fmt_num_safe;

[[maybe_unused]] QString fmt_pct(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    return QString::number(v.toDouble() * 100.0, 'f', decimals) + "%";
}

[[maybe_unused]] QString fmt_bool(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    return v.toBool() ? QStringLiteral("YES") : QStringLiteral("NO");
}

} // namespace

void QuantModulePanel::display_cfa_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    if (!payload.value("success").toBool()) {
        display_error(payload.value("error").toString(tr("Unknown error")));
        return;
    }

    const QJsonObject result = payload.value("result").toObject();
    const QString method = result.value("method").toString();
    const double calc_time = result.value("calculation_time").toDouble();
    const QString analysis_type = payload.value("analysis_type").toString(command);
    const QString accent = ui::colors::legible(module_.color);

    // ── Section header ──────────────────────────────────────────────────────
    QString header_text = analysis_type.toUpper();
    header_text.replace('_', ' ');
    if (!method.isEmpty())
        header_text += QString("  |  %1").arg(method.toUpper());
    if (calc_time > 0.0)
        header_text += tr("  |  %1 ms").arg(QString::number(calc_time * 1000.0, 'f', 1));
    results_layout_->addWidget(gs_section_header(header_text, accent));

    const QJsonValue value_raw = result.value("value");
    const QJsonObject value_obj = value_raw.toObject();
    const QJsonObject meta = result.value("metadata").toObject();
    bool rendered = false;

    // ── 1. TREND ANALYSIS ───────────────────────────────────────────────────
    if (command == "trend_analysis") {
        const double slope = value_obj.value("slope").toDouble();
        const double intercept = value_obj.value("intercept").toDouble();
        const double r2 = meta.value("r_squared").toDouble();
        const double tstat = meta.value("slope_t_statistic").toDouble();
        const double pval = meta.value("slope_p_value").toDouble();
        const QString direction = meta.value("trend_direction").toString().toUpper();
        const bool significant = meta.value("trend_significant").toBool();

        auto dir_color = [&]() {
            if (direction == "UPWARD")
                return ui::colors::POSITIVE();
            if (direction == "DOWNWARD")
                return ui::colors::NEGATIVE();
            return ui::colors::TEXT_PRIMARY();
        };

        QList<QWidget*> top = {
            gs_make_card(tr("DIRECTION"), direction.isEmpty() ? QStringLiteral("—") : direction, this, dir_color()),
            gs_make_card(tr("SLOPE"), gs_fmt_num(slope, 6), this, gs_pos_neg_color(slope)),
            gs_make_card(tr("INTERCEPT"), gs_fmt_num(intercept, 4), this),
            gs_make_card(tr("R²"), gs_fmt_num(r2, 4), this,
                         r2 >= 0.7   ? ui::colors::POSITIVE()
                         : r2 >= 0.3 ? ui::colors::WARNING()
                                     : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> stats = {
            gs_make_card(tr("t-STATISTIC"), gs_fmt_num(tstat, 3), this),
            gs_make_card(tr("p-VALUE"), gs_fmt_num(pval, 4), this,
                         pval < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("SIGNIFICANT"), significant ? tr("YES") : tr("NO"), this,
                         significant ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("OBSERVATIONS"),
                         QString::number(result.value("parameters").toObject().value("data").toArray().size()), this),
        };
        results_layout_->addWidget(gs_card_row(stats, this));
        status_label_->setText(tr("Trend %1  slope=%2  R²=%3  p=%4")
                                   .arg(direction)
                                   .arg(slope, 0, 'g', 4)
                                   .arg(r2, 0, 'f', 3)
                                   .arg(pval, 0, 'f', 4));
        rendered = true;
    }

    // ── 2. STATIONARITY TEST ────────────────────────────────────────────────
    else if (command == "stationarity_test") {
        const QString test = result.value("parameters").toObject().value("test_type").toString().toUpper();
        const double tstat = value_obj.value("test_statistic").toDouble();
        const double pval = value_obj.value("p_value").toDouble();
        const bool stationary = value_obj.value("is_stationary").toBool();
        const QString h0 = value_obj.value("null_hypothesis").toString();

        QList<QWidget*> top = {
            gs_make_card(tr("TEST"), test.isEmpty() ? QStringLiteral("ADF") : test, this),
            gs_make_card(tr("TEST STAT"), gs_fmt_num(tstat, 4), this),
            gs_make_card(tr("p-VALUE"), gs_fmt_num(pval, 4), this,
                         pval < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("STATIONARY"), stationary ? tr("YES") : tr("NO"), this,
                         stationary ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Critical values row
        const QJsonObject crit = value_obj.value("critical_values").toObject();
        if (!crit.isEmpty()) {
            QList<QWidget*> crit_cards;
            for (auto it = crit.begin(); it != crit.end(); ++it) {
                crit_cards << gs_make_card(tr("CRIT %1").arg(it.key()), gs_fmt_num(it.value().toDouble(), 4), this);
            }
            results_layout_->addWidget(gs_card_row(crit_cards, this));
        }

        if (!h0.isEmpty()) {
            auto* note = new QLabel(tr("H₀: %1").arg(h0));
            note->setStyleSheet(
                QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; padding:6px 10px; background:%2; border-left:3px solid %3;")
                    .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            note->setWordWrap(true);
            results_layout_->addWidget(note);
        }
        status_label_->setText(tr("%1: stat=%2  p=%3  → %4")
                                   .arg(test)
                                   .arg(tstat, 0, 'f', 3)
                                   .arg(pval, 0, 'f', 4)
                                   .arg(stationary ? tr("stationary") : tr("non-stationary")));
        rendered = true;
    }

    // ── 3. ARIMA MODEL ──────────────────────────────────────────────────────
    else if (command == "arima_model") {
        const QJsonArray order = result.value("parameters").toObject().value("order").toArray();
        const QString order_str =
            order.size() == 3
                ? QString("(%1, %2, %3)").arg(order[0].toInt()).arg(order[1].toInt()).arg(order[2].toInt())
                : QStringLiteral("—");
        const double aic = meta.value("aic").toDouble();
        const double bic = meta.value("bic").toDouble();
        const double llf = meta.value("log_likelihood").toDouble();
        const double lb_p = meta.value("ljung_box_p_value").toDouble();
        const bool autocorr = meta.value("residuals_autocorrelated").toBool();

        QList<QWidget*> top = {
            gs_make_card(tr("ORDER (p,d,q)"), order_str, this),
            gs_make_card(tr("AIC"), gs_fmt_num(aic, 2), this),
            gs_make_card(tr("BIC"), gs_fmt_num(bic, 2), this),
            gs_make_card(tr("LOG-LIKELIHOOD"), gs_fmt_num(llf, 2), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> diag = {
            gs_make_card(tr("LJUNG-BOX p"), gs_fmt_num(lb_p, 4), this,
                         lb_p > 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("RESID. AUTOCORR."), autocorr ? tr("YES") : tr("NO"), this,
                         autocorr ? ui::colors::WARNING() : ui::colors::POSITIVE()),
            gs_make_card(tr("FITTED POINTS"), QString::number(value_obj.value("fitted_values").toArray().size()), this),
            gs_make_card(tr("RESIDUALS"), QString::number(value_obj.value("residuals").toArray().size()), this),
        };
        results_layout_->addWidget(gs_card_row(diag, this));

        // Coefficient list (value.parameters can be an array OR a {const, ar.L1, ...} object)
        const QJsonValue coef_raw = value_obj.value("parameters");
        QStringList coef_lines;
        if (coef_raw.isArray()) {
            const auto arr = coef_raw.toArray();
            for (int i = 0; i < arr.size(); ++i)
                coef_lines << QString("β%1 = %2").arg(i).arg(arr[i].toDouble(), 0, 'f', 6);
        } else if (coef_raw.isObject()) {
            const auto o = coef_raw.toObject();
            for (auto it = o.begin(); it != o.end(); ++it)
                coef_lines << QString("%1 = %2").arg(it.key()).arg(it.value().toDouble(), 0, 'f', 6);
        }
        if (!coef_lines.isEmpty()) {
            auto* coef_lbl = new QLabel(tr("Coefficients: %1").arg(coef_lines.join("   ")));
            coef_lbl->setWordWrap(true);
            coef_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                            "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(coef_lbl);
        }
        status_label_->setText(
            tr("ARIMA%1  AIC=%2  Ljung-Box p=%3").arg(order_str).arg(aic, 0, 'f', 2).arg(lb_p, 0, 'f', 3));
        rendered = true;
    }

    // ── 4. FORECASTING ──────────────────────────────────────────────────────
    else if (command == "forecasting") {
        const QJsonObject params = result.value("parameters").toObject();
        const QString fmethod = params.value("method").toString().toUpper();
        const int horizon = meta.value("forecast_horizon").toInt();
        const int train_n = meta.value("train_data_size").toInt();
        const int test_n = meta.value("test_data_size").toInt();
        const QJsonValue mae = meta.value("mae");
        const QJsonValue rmse = meta.value("rmse");
        const QJsonValue mape = meta.value("mape");
        // The forecast beyond the last observation (same method refitted on the whole series),
        // or the reason that refit failed — never both.
        const QJsonArray oos = meta.value("forecast_out_of_sample").toArray();
        const QString oos_error = meta.value("forecast_out_of_sample_error").toString().trimmed();

        QList<QWidget*> top = {
            gs_make_card(tr("METHOD"), fmethod.isEmpty() ? QStringLiteral("—") : fmethod, this),
            gs_make_card(tr("HORIZON"), QString::number(horizon), this),
            gs_make_card(tr("TRAIN"), tr("%1 obs").arg(train_n), this),
            gs_make_card(tr("TEST"), tr("%1 obs").arg(test_n), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> err = {
            gs_make_card(tr("MAE"), fmt_num_safe(mae, 4), this,
                         mae.isNull() ? ui::colors::TEXT_SECONDARY() : ui::colors::WARNING()),
            gs_make_card(tr("RMSE"), fmt_num_safe(rmse, 4), this,
                         rmse.isNull() ? ui::colors::TEXT_SECONDARY() : ui::colors::WARNING()),
            gs_make_card(tr("MAPE"),
                         mape.isNull() ? QStringLiteral("—") : QString::number(mape.toDouble(), 'f', 2) + "%", this),
            gs_make_card(tr("FCST POINTS"), QString::number(value_raw.toArray().size()), this),
        };
        results_layout_->addWidget(gs_card_row(err, this));

        if (!oos_error.isEmpty()) {
            auto* note = new QLabel(
                tr("No forecast beyond the last observation: refitting %1 on the whole series failed — %2")
                    .arg(fmethod, oos_error));
            note->setWordWrap(true);
            note->setTextInteractionFlags(Qt::TextSelectableByMouse);
            note->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) +
                                        "px; padding:6px 10px; background:%2; border-left:3px solid %1;")
                                    .arg(ui::colors::WARNING(), ui::colors::BG_SURFACE()));
            results_layout_->addWidget(note);
        }

        // Two forecasts against the series:
        //  * hold-out — fitted on the first TRAIN observations, so its step t+1 is observation
        //    TRAIN, inside the test window where the actual values continue (the MAE / RMSE /
        //    MAPE above are measured on that overlap);
        //  * forward — the same method refitted on the whole series, continuing from the last
        //    observation (metadata.forecast_out_of_sample).
        // With no test window (train == all data) the script's value already IS the forward
        // forecast, so it is drawn once, as the forward line.
        if (value_raw.isArray()) {
            const auto arr = value_raw.toArray();
            const QVector<double> data = qc_values(params.value("data").toArray());
            const qsizetype n = data.size();
            const bool has_holdout = test_n > 0 && !arr.isEmpty();
            const QJsonArray fwd = !oos.isEmpty() ? oos : (test_n > 0 ? QJsonArray() : arr);
            const int train = (train_n > 0 && train_n <= n) ? train_n : static_cast<int>(n);
            if ((has_holdout || !fwd.isEmpty()) && n > 0) {
                QcChartSpec spec;
                if (has_holdout && !fwd.isEmpty())
                    spec.title = tr("SERIES  ·  HOLD-OUT FORECAST FROM OBS %1  ·  %2-STEP FORECAST BEYOND THE LAST "
                                    "OBSERVATION")
                                     .arg(train - 1)
                                     .arg(fwd.size());
                else if (has_holdout)
                    spec.title = tr("SERIES + %1-STEP FORECAST FROM THE END OF TRAINING (OBS %2)")
                                     .arg(arr.size())
                                     .arg(train - 1);
                else
                    spec.title = tr("SERIES + %1-STEP FORECAST BEYOND THE LAST OBSERVATION").arg(fwd.size());
                spec.fmt = qc_fmt_auto();
                spec.height = 280;
                // Observations are "obs i"; steps past the last one read t+1, t+2, ...
                spec.x_label = [n](double x) {
                    const qlonglong i = std::llround(x);
                    return i >= n ? QStringLiteral("t+%1").arg(i - n + 1) : QStringLiteral("obs %1").arg(i);
                };

                QcLine actual;
                actual.name = tr("Actual");
                actual.xs = qc_x_positions(n).xs;
                actual.ys = data;
                actual.color = ui::ChartColorRole::Text;
                actual.width = 1.25;
                spec.lines.append(actual);

                // A forecast line starting at the observation it continues from.
                const auto anchored = [&data](int anchor, const QJsonArray& values) {
                    QPair<QVector<double>, QVector<double>> out;
                    out.first.append(static_cast<double>(anchor));
                    out.second.append(data.at(anchor));
                    for (qsizetype i = 0; i < values.size(); ++i) {
                        out.first.append(static_cast<double>(anchor + 1 + i));
                        out.second.append(values.at(i).isDouble() ? values.at(i).toDouble() : kQcNaN);
                    }
                    return out;
                };
                if (has_holdout) {
                    const auto pts = anchored(train - 1, arr);
                    QcLine hold;
                    hold.name = tr("Hold-out forecast (from obs %1)").arg(train - 1);
                    hold.xs = pts.first;
                    hold.ys = pts.second;
                    hold.color = ui::ChartColorRole::Accent;
                    hold.width = 2.0;
                    spec.lines.append(hold);
                }
                if (!fwd.isEmpty()) {
                    const auto pts = anchored(static_cast<int>(n - 1), fwd);
                    QcLine ahead;
                    ahead.name = tr("Forecast beyond last obs");
                    ahead.xs = pts.first;
                    ahead.ys = pts.second;
                    ahead.color = ui::ChartColorRole::Info;
                    ahead.dashed = true;
                    ahead.width = 2.0;
                    spec.lines.append(ahead);
                }
                auto* chart = qc_series_chart(spec, this);
                if (has_holdout) {
                    // Shade the hold-out forecast span, up to where the forward span begins.
                    const double from = train - 1;
                    double to = static_cast<double>(train - 1 + arr.size());
                    if (!fwd.isEmpty())
                        to = std::min(to, static_cast<double>(n - 1));
                    if (to > from)
                        chart->add_band(from, to, ui::ChartColorRole::Muted, tr("Hold-out"));
                }
                if (!fwd.isEmpty())
                    chart->add_band(static_cast<double>(n - 1), static_cast<double>(n - 1 + fwd.size()),
                                    ui::ChartColorRole::Info, tr("Out of sample"));
                results_layout_->addWidget(chart);
            }

            // Step table: hold-out forecast next to the actual it is scored on, and the forward
            // forecast for the same step count.
            const int rows = static_cast<int>(std::max(has_holdout ? arr.size() : qsizetype(0), fwd.size()));
            if (rows > 0) {
                QStringList headers{tr("Step")};
                if (has_holdout)
                    headers << tr("Hold-out forecast") << tr("Actual");
                if (!fwd.isEmpty())
                    headers << tr("Beyond last obs");
                auto* table = new QTableWidget(rows, headers.size(), this);
                table->setHorizontalHeaderLabels(headers);
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(qMin(rows * row_px(24) + row_px(32), 280));
                const auto num_cell = [](const QJsonValue& v) {
                    auto* it = new QTableWidgetItem(fmt_num_safe(v, 6));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    return it;
                };
                for (int i = 0; i < rows; ++i) {
                    int col = 0;
                    table->setItem(i, col++, new QTableWidgetItem(QString("t+%1").arg(i + 1)));
                    if (has_holdout) {
                        table->setItem(i, col++, num_cell(i < arr.size() ? arr.at(i) : QJsonValue()));
                        const int obs = train + i;
                        table->setItem(i, col++,
                                       num_cell(obs < n && std::isfinite(data.at(obs)) ? QJsonValue(data.at(obs))
                                                                                       : QJsonValue()));
                    }
                    if (!fwd.isEmpty())
                        table->setItem(i, col++, num_cell(i < fwd.size() ? fwd.at(i) : QJsonValue()));
                    table->setRowHeight(i, row_px(24));
                }
                results_layout_->addWidget(table);
            }
        }
        QString status = tr("%1 forecast (%2 steps)  MAE=%3").arg(fmethod).arg(horizon).arg(fmt_num_safe(mae, 4));
        if (!oos.isEmpty())
            status += tr("  ·  t+%1 beyond last obs: %2").arg(oos.size()).arg(fmt_num_safe(oos.last(), 4));
        else if (!oos_error.isEmpty())
            status += tr("  ·  no forecast beyond the last observation");
        status_label_->setText(status);
        rendered = true;
    }

    // ── 5. SUPERVISED LEARNING ──────────────────────────────────────────────
    else if (command == "supervised_learning") {
        const QJsonObject params = result.value("parameters").toObject();
        const QString problem = params.value("problem_type").toString();
        const bool is_reg = (problem == "regression");
        const QString best = meta.value("best_algorithm").toString().toUpper();
        const int train_n = meta.value("train_size").toInt();
        const int test_n = meta.value("test_size").toInt();
        const int n_feat = meta.value("n_features").toInt();
        // Classification: accuracy of always predicting the test window's most common class.
        const QJsonValue baseline = meta.value("majority_class_accuracy");

        QList<QWidget*> top = {
            gs_make_card(tr("PROBLEM"), problem.toUpper(), this),
            gs_make_card(tr("BEST ALGORITHM"), best.isEmpty() ? QStringLiteral("—") : best, this,
                         ui::colors::POSITIVE()),
            gs_make_card(tr("FEATURES"), QString::number(n_feat), this),
            gs_make_card(tr("TRAIN / TEST"), QString("%1 / %2").arg(train_n).arg(test_n), this),
        };
        if (!is_reg) {
            auto* card = gs_make_card(tr("BASELINE"), fmt_num_safe(baseline, 4), this);
            card->setToolTip(tr("Majority-class accuracy: the accuracy of always predicting the most common class "
                                "of the test window. A classifier has learned something only if its accuracy "
                                "beats this."));
            top.append(card);
        }
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-algorithm metrics table
        QStringList headers = is_reg ? QStringList{tr("Algorithm"), tr("MSE"), tr("RMSE"), tr("R²")}
                                     : QStringList{tr("Algorithm"), tr("Accuracy"), tr("Precision"), tr("Recall")};
        QStringList score_keys =
            is_reg ? QStringList{"mse", "rmse", "r2_score"} : QStringList{"accuracy", "precision", "recall"};

        QList<QPair<QString, QJsonObject>> rows;
        for (auto it = value_obj.begin(); it != value_obj.end(); ++it) {
            if (it.value().isObject())
                rows.append({it.key(), it.value().toObject()});
        }

        if (!rows.isEmpty()) {
            auto* table = new QTableWidget(rows.size(), headers.size(), this);
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows.size() * row_px(26) + row_px(32), 320));
            for (int r = 0; r < rows.size(); ++r) {
                const auto& algo = rows[r].first;
                const auto& metrics = rows[r].second;
                auto* name_it = new QTableWidgetItem(algo.toUpper());
                if (algo.toUpper() == best)
                    name_it->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(r, 0, name_it);

                if (metrics.contains("error")) {
                    auto* err_it = new QTableWidgetItem(tr("ERR: %1").arg(metrics.value("error").toString()));
                    err_it->setForeground(QColor(ui::colors::NEGATIVE()));
                    table->setItem(r, 1, err_it);
                    table->setSpan(r, 1, 1, headers.size() - 1);
                } else {
                    for (int c = 0; c < score_keys.size(); ++c) {
                        const QJsonValue raw = metrics.value(score_keys[c]);
                        const double v = raw.toDouble();
                        auto* it = new QTableWidgetItem(fmt_num_safe(raw, 4));
                        it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        // R² and accuracy: green if good. A null (non-finite in the script) reads
                        // "—" and is not coloured.
                        const bool scored = raw.isDouble() && (score_keys[c] == "r2_score" || score_keys[c] == "accuracy");
                        if (scored && v > 0.5)
                            it->setForeground(QColor(ui::colors::POSITIVE()));
                        else if (scored && v < 0)
                            it->setForeground(QColor(ui::colors::NEGATIVE()));
                        table->setItem(r, c + 1, it);
                    }
                }
                table->setRowHeight(r, row_px(26));
            }
            results_layout_->addWidget(table);
        }
        QString status = tr("Supervised %1: best=%2 (%3 algos)").arg(problem).arg(best).arg(rows.size());
        if (!is_reg && baseline.isDouble())
            status += tr("  ·  baseline %1").arg(fmt_num_safe(baseline, 4));
        status_label_->setText(status);
        rendered = true;
    }

    // ── 6. UNSUPERVISED LEARNING ────────────────────────────────────────────
    else if (command == "unsupervised_learning") {
        const int n_samples = meta.value("n_samples").toInt();
        const int n_feat = meta.value("n_features").toInt();

        QList<QWidget*> top = {
            gs_make_card(tr("SAMPLES"), QString::number(n_samples), this),
            gs_make_card(tr("FEATURES"), QString::number(n_feat), this),
            gs_make_card(tr("METHODS"), QString::number(value_obj.size()), this),
            gs_make_card(tr("DATA SCALED"), meta.value("data_scaled").toBool() ? tr("YES") : tr("NO"), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // PCA section
        if (value_obj.contains("pca") && value_obj.value("pca").isObject()) {
            const QJsonObject pca = value_obj.value("pca").toObject();
            results_layout_->addWidget(gs_section_header(tr("PCA"), accent));
            const QJsonArray evr = pca.value("explained_variance_ratio").toArray();
            const QJsonArray cum = pca.value("cumulative_variance").toArray();
            const int n95 = pca.value("components_for_95_variance").toInt();
            const double total = evr.isEmpty() ? 0.0 : cum[cum.size() - 1].toDouble();

            QList<QWidget*> pca_cards = {
                gs_make_card(tr("COMPONENTS"), QString::number(evr.size()), this),
                gs_make_card(tr("FOR 95% VAR"), QString::number(n95), this, ui::colors::POSITIVE()),
                gs_make_card(
                    tr("TOP COMPONENT"),
                    evr.isEmpty() ? QStringLiteral("—") : QString::number(evr[0].toDouble() * 100, 'f', 2) + "%", this),
                gs_make_card(tr("CUM. EXPLAINED"), QString::number(total * 100, 'f', 2) + "%", this),
            };
            results_layout_->addWidget(gs_card_row(pca_cards, this));

            if (!evr.isEmpty()) {
                const int rows = std::min<int>(10, evr.size());
                auto* table = new QTableWidget(rows, 3, this);
                table->setHorizontalHeaderLabels({tr("PC"), tr("Explained Var"), tr("Cumulative")});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(rows * row_px(24) + row_px(32));
                for (int i = 0; i < rows; ++i) {
                    table->setItem(i, 0, new QTableWidgetItem(QString("PC%1").arg(i + 1)));
                    auto* e = new QTableWidgetItem(QString::number(evr[i].toDouble() * 100, 'f', 3) + "%");
                    e->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 1, e);
                    auto* c = new QTableWidgetItem(QString::number(cum[i].toDouble() * 100, 'f', 3) + "%");
                    c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 2, c);
                    table->setRowHeight(i, row_px(24));
                }
                results_layout_->addWidget(table);
            }
        }

        // K-Means section
        if (value_obj.contains("kmeans") && value_obj.value("kmeans").isObject()) {
            const QJsonObject km = value_obj.value("kmeans").toObject();
            results_layout_->addWidget(gs_section_header(tr("K-MEANS"), accent));
            const int opt_k = km.value("optimal_k").toInt();
            const QJsonArray sil = km.value("silhouette_by_k").toArray();
            const QJsonArray ine = km.value("inertia_by_k").toArray();
            double best_sil = 0.0;
            for (const auto& v : sil) {
                const auto pair = v.toArray();
                if (pair.size() >= 2)
                    best_sil = std::max(best_sil, pair[1].toDouble());
            }
            const QJsonArray labels = km.value("cluster_labels").toArray();

            QList<QWidget*> km_cards = {
                gs_make_card(tr("OPTIMAL k"), QString::number(opt_k), this, ui::colors::POSITIVE()),
                gs_make_card(tr("BEST SILHOUETTE"), gs_fmt_num(best_sil, 4), this,
                             best_sil >= 0.5 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card(tr("k RANGE"), QString::number(sil.size()), this),
                gs_make_card(tr("OBSERVATIONS"), QString::number(labels.size()), this),
            };
            results_layout_->addWidget(gs_card_row(km_cards, this));

            if (!sil.isEmpty()) {
                auto* table = new QTableWidget(sil.size(), 3, this);
                table->setHorizontalHeaderLabels({tr("k"), tr("Silhouette"), tr("Inertia")});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(qMin(sil.size() * row_px(24) + row_px(32), 280));
                for (int i = 0; i < sil.size(); ++i) {
                    const auto sp = sil[i].toArray();
                    const auto ip = i < ine.size() ? ine[i].toArray() : QJsonArray();
                    const int k = sp.size() > 0 ? sp[0].toInt() : 0;
                    const double s = sp.size() > 1 ? sp[1].toDouble() : 0.0;
                    const double in_v = ip.size() > 1 ? ip[1].toDouble() : 0.0;

                    auto* k_it = new QTableWidgetItem(QString::number(k));
                    if (k == opt_k)
                        k_it->setForeground(QColor(ui::colors::POSITIVE()));
                    table->setItem(i, 0, k_it);
                    auto* s_it = new QTableWidgetItem(QString::number(s, 'f', 4));
                    s_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (k == opt_k)
                        s_it->setForeground(QColor(ui::colors::POSITIVE()));
                    table->setItem(i, 1, s_it);
                    auto* in_it = new QTableWidgetItem(QString::number(in_v, 'f', 2));
                    in_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 2, in_it);
                    table->setRowHeight(i, row_px(24));
                }
                results_layout_->addWidget(table);
            }
        }

        // Hierarchical (simple)
        if (value_obj.contains("hierarchical") && value_obj.value("hierarchical").isObject()) {
            const QJsonObject h = value_obj.value("hierarchical").toObject();
            QList<QWidget*> h_cards = {
                gs_make_card(tr("HIERARCHICAL k"), QString::number(h.value("n_clusters").toInt()), this),
                gs_make_card(tr("LABELS"), QString::number(h.value("cluster_labels").toArray().size()), this),
            };
            results_layout_->addWidget(gs_card_row(h_cards, this));
        }
        status_label_->setText(
            tr("Unsupervised: %1 method(s) on %2×%3").arg(value_obj.size()).arg(n_samples).arg(n_feat));
        rendered = true;
    }

    // ── 7. MODEL EVALUATION ─────────────────────────────────────────────────
    else if (command == "model_evaluation") {
        const QJsonObject params = result.value("parameters").toObject();
        const QString requested = params.value("model_type").toString();
        // The estimator that actually ran — e.g. "svr" on 0/1 data runs an SVC classifier.
        const QString model_run = meta.value("model").toString();
        const QString model = !model_run.isEmpty() ? model_run
                              : requested.isEmpty() ? QStringLiteral("—")
                                                    : requested.toUpper();
        const QString problem = params.value("problem_type").toString().toUpper();
        const QString scoring = meta.value("scoring").toString(); // r2 | accuracy
        const QString score_name = scoring == QLatin1String("r2")         ? QStringLiteral("R²")
                                   : scoring == QLatin1String("accuracy") ? tr("ACCURACY")
                                                                          : scoring.toUpper();
        const int folds = params.value("cv_folds").toInt();
        // Any score can be null (a fold / learning-curve point the model could not fit, e.g. a
        // classification slice holding a single class): rendered "—", never 0.0000.
        const QJsonValue mean_cv = meta.value("mean_cv_score");
        const QJsonValue std_cv = meta.value("std_cv_score");
        const QJsonValue overfit = meta.value("overfitting_score");
        const bool is_overfit = meta.value("is_overfitting").toBool();
        const double threshold = meta.value("overfitting_threshold").toDouble(0.1);
        const QJsonValue final_train = meta.value("final_train_score");
        const QJsonValue final_val = meta.value("final_val_score");
        const int n_train = meta.value("train_size").toInt();
        const int n_holdout = meta.value("holdout_size").toInt();

        auto* model_card = gs_make_card(tr("MODEL"), model, this);
        if (!requested.isEmpty() && !model_run.isEmpty())
            model_card->setToolTip(tr("Requested: %1 — ran: %2").arg(requested, model_run));
        QList<QWidget*> top = {
            model_card,
            gs_make_card(tr("PROBLEM"), score_name.isEmpty() ? problem : QString("%1 · %2").arg(problem, score_name),
                         this),
            gs_make_card(tr("CV FOLDS"), QString::number(folds), this),
            gs_make_card(tr("MEAN CV ± σ"), QString("%1 ± %2").arg(fmt_num_safe(mean_cv, 4), fmt_num_safe(std_cv, 4)),
                         this,
                         !mean_cv.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : mean_cv.toDouble() > 0.5 ? ui::colors::SUCCESS()
                         : mean_cv.toDouble() > 0   ? ui::colors::WARNING()
                                                    : ui::colors::DANGER()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> overfit_cards = {
            gs_make_card(tr("FINAL TRAIN"), fmt_num_safe(final_train, 4), this),
            gs_make_card(tr("FINAL HOLD-OUT"), fmt_num_safe(final_val, 4), this,
                         !final_val.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : final_val.toDouble() > 0.5 ? ui::colors::SUCCESS()
                                                      : ui::colors::WARNING()),
            gs_make_card(tr("OVERFIT GAP"), fmt_num_safe(overfit, 4), this,
                         !overfit.isDouble()                ? ui::colors::TEXT_SECONDARY()
                         : overfit.toDouble() > threshold ? ui::colors::DANGER()
                                                          : ui::colors::SUCCESS()),
            gs_make_card(tr("OVERFITTING"), is_overfit ? tr("YES") : tr("NO"), this,
                         is_overfit ? ui::colors::DANGER() : ui::colors::SUCCESS()),
        };
        results_layout_->addWidget(gs_card_row(overfit_cards, this));

        auto score_cell = [](const QJsonValue& v) {
            auto* it = new QTableWidgetItem(fmt_num_safe(v, 4));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (!v.isDouble())
                it->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            return it;
        };
        auto make_table = [this](int rows, const QStringList& headers) {
            auto* table = new QTableWidget(rows, headers.size(), this);
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows * row_px(24) + row_px(32), 280));
            return table;
        };

        // Learning curve: fits on growing shares of the training part, each scored on its own
        // training rows (Train) and on the fixed hold-out at the end of the sample (Hold-out).
        const QJsonArray sizes = value_obj.value("train_sizes").toArray();
        const QJsonArray train_s = value_obj.value("train_scores").toArray();
        const QJsonArray val_s = value_obj.value("val_scores").toArray();
        if (!sizes.isEmpty()) {
            results_layout_->addWidget(gs_section_header(
                tr("LEARNING CURVE  |  %1  |  TRAIN PART %2 OBS, HOLD-OUT = LAST %3 OBS")
                    .arg(score_name)
                    .arg(n_train)
                    .arg(n_holdout),
                accent));
            const int rows = static_cast<int>(sizes.size());
            auto* table = make_table(rows, {tr("Train size"), tr("Train"), tr("Hold-out")});
            for (int i = 0; i < rows; ++i) {
                auto* size_it = new QTableWidgetItem(sizes.at(i).isDouble()
                                                         ? QString::number(sizes.at(i).toInt())
                                                         : QStringLiteral("—"));
                size_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 0, size_it);
                table->setItem(i, 1, score_cell(i < train_s.size() ? train_s.at(i) : QJsonValue()));
                table->setItem(i, 2, score_cell(i < val_s.size() ? val_s.at(i) : QJsonValue()));
                table->setRowHeight(i, row_px(24));
            }
            results_layout_->addWidget(table);
        }

        // Walk-forward CV: TimeSeriesSplit over the whole sample — each fold is scored on the
        // observations right after the ones it was trained on.
        const QJsonArray cv = value_obj.value("cv_scores").toArray();
        if (!cv.isEmpty()) {
            results_layout_->addWidget(gs_section_header(
                tr("WALK-FORWARD CV  |  %1  |  %2 FOLDS  |  MEAN %3 ± %4")
                    .arg(score_name)
                    .arg(cv.size())
                    .arg(fmt_num_safe(mean_cv, 4), fmt_num_safe(std_cv, 4)),
                accent));
            const int rows = static_cast<int>(cv.size());
            auto* table = make_table(rows, {tr("Fold"), tr("Score")});
            for (int i = 0; i < rows; ++i) {
                auto* fold_it = new QTableWidgetItem(QString::number(i + 1));
                fold_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 0, fold_it);
                table->setItem(i, 1, score_cell(cv.at(i)));
                table->setRowHeight(i, row_px(24));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1: CV %2±%3  Overfit: %4")
                                   .arg(model)
                                   .arg(fmt_num_safe(mean_cv, 3), fmt_num_safe(std_cv, 3))
                                   .arg(is_overfit ? tr("YES") : tr("NO")));
        rendered = true;
    }

    // ── 8. RESAMPLING METHODS ───────────────────────────────────────────────
    else if (command == "resampling_methods") {
        const double orig = meta.value("original_statistic").toDouble();
        const int data_n = meta.value("data_size").toInt();
        const QString stat_name = meta.value("statistic_name").toString();

        QList<QWidget*> top = {
            gs_make_card(tr("STATISTIC"), stat_name.isEmpty() ? tr("mean") : stat_name, this),
            gs_make_card(tr("ORIGINAL VALUE"), gs_fmt_num(orig, 6), this, gs_pos_neg_color(orig)),
            gs_make_card(tr("DATA SIZE"), QString::number(data_n), this),
            gs_make_card(tr("METHODS"), QString::number(value_obj.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Bootstrap
        if (value_obj.contains("bootstrap") && value_obj.value("bootstrap").isObject()) {
            const QJsonObject b = value_obj.value("bootstrap").toObject();
            results_layout_->addWidget(gs_section_header(tr("BOOTSTRAP"), accent));
            const auto ci = b.value("confidence_interval").toArray();
            const QString ci_str =
                ci.size() == 2 ? QString("[%1, %2]").arg(ci[0].toDouble(), 0, 'f', 4).arg(ci[1].toDouble(), 0, 'f', 4)
                               : QStringLiteral("—");
            QList<QWidget*> b_cards = {
                gs_make_card(tr("MEAN"), gs_fmt_num(b.value("mean").toDouble(), 6), this),
                gs_make_card(tr("STD"), gs_fmt_num(b.value("std").toDouble(), 6), this),
                gs_make_card(tr("BIAS"), gs_fmt_num(b.value("bias").toDouble(), 6), this,
                             gs_pos_neg_color(b.value("bias").toDouble())),
                gs_make_card(tr("95% CI"), ci_str, this, ui::colors::INFO()),
            };
            results_layout_->addWidget(gs_card_row(b_cards, this));
        }

        // Jackknife
        if (value_obj.contains("jackknife") && value_obj.value("jackknife").isObject()) {
            const QJsonObject j = value_obj.value("jackknife").toObject();
            results_layout_->addWidget(gs_section_header(tr("JACKKNIFE"), accent));
            QList<QWidget*> j_cards = {
                gs_make_card(tr("BIAS"), gs_fmt_num(j.value("bias").toDouble(), 6), this,
                             gs_pos_neg_color(j.value("bias").toDouble())),
                gs_make_card(tr("BIAS-CORRECTED"), gs_fmt_num(j.value("bias_corrected_estimate").toDouble(), 6), this),
                gs_make_card(tr("VARIANCE"), gs_fmt_num(j.value("variance_estimate").toDouble(), 6), this),
                gs_make_card(tr("STD ERROR"), gs_fmt_num(j.value("std_error").toDouble(), 6), this),
            };
            results_layout_->addWidget(gs_card_row(j_cards, this));
        }

        // Permutation
        if (value_obj.contains("permutation") && value_obj.value("permutation").isObject()) {
            const QJsonObject p = value_obj.value("permutation").toObject();
            results_layout_->addWidget(gs_section_header(tr("PERMUTATION"), accent));
            const double pv = p.value("p_value").toDouble();
            const bool sig = p.value("significant").toBool();
            QList<QWidget*> p_cards = {
                gs_make_card(tr("p-VALUE"), gs_fmt_num(pv, 4), this,
                             pv < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card(tr("SIGNIFICANT"), sig ? tr("YES") : tr("NO"), this,
                             sig ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card(tr("RESAMPLES"), QString::number(p.value("resampled_statistics").toArray().size()), this),
                gs_make_card(tr("ORIGINAL"), gs_fmt_num(orig, 6), this),
            };
            results_layout_->addWidget(gs_card_row(p_cards, this));
        }
        status_label_->setText(
            tr("Resampling: original=%1  (%2 method(s))").arg(orig, 0, 'g', 4).arg(value_obj.size()));
        rendered = true;
    }

    // ── 9. SAMPLING TECHNIQUES ──────────────────────────────────────────────
    else if (command == "sampling_techniques") {
        const QJsonObject params = result.value("parameters").toObject();
        const int pop_n = params.value("population_size").toInt();
        const int sample_n = params.value("sample_size").toInt();
        const double frac = meta.value("sampling_fraction").toDouble();

        QList<QWidget*> top = {
            gs_make_card(tr("POPULATION"), QString::number(pop_n), this),
            gs_make_card(tr("SAMPLE SIZE"), QString::number(sample_n), this),
            gs_make_card(tr("SAMPLING FRAC"), QString::number(frac * 100, 'f', 2) + "%", this),
            gs_make_card(tr("METHODS"), QString::number(value_obj.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-method comparison table — pick the first numeric column from each method's stats
        if (!value_obj.isEmpty()) {
            QList<QPair<QString, QJsonObject>> methods;
            for (auto it = value_obj.begin(); it != value_obj.end(); ++it)
                if (it.value().isObject())
                    methods.append({it.key(), it.value().toObject()});

            auto* table = new QTableWidget(methods.size(), 5, this);
            table->setHorizontalHeaderLabels(
                {tr("Method"), tr("Sample N"), tr("Sample Mean"), tr("Pop Mean"), tr("Bias")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(methods.size() * row_px(26) + row_px(32), 240));
            for (int r = 0; r < methods.size(); ++r) {
                const auto& name = methods[r].first;
                const auto& m = methods[r].second;
                table->setItem(r, 0, new QTableWidgetItem(name.toUpper()));
                if (m.contains("error")) {
                    auto* e = new QTableWidgetItem(tr("ERR: %1").arg(m.value("error").toString()));
                    e->setForeground(QColor(ui::colors::NEGATIVE()));
                    table->setItem(r, 1, e);
                    table->setSpan(r, 1, 1, 4);
                } else {
                    table->setItem(r, 1, new QTableWidgetItem(QString::number(m.value("sample_size").toInt())));
                    auto first_val = [](const QJsonObject& o) -> double {
                        if (o.isEmpty())
                            return 0.0;
                        return o.begin().value().toDouble();
                    };
                    const double sm = first_val(m.value("sample_mean").toObject());
                    const double pm = first_val(m.value("population_mean").toObject());
                    const double bias = first_val(m.value("bias").toObject());
                    auto add_n = [&](int col, double v, bool pos_neg = false) {
                        auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                        it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        if (pos_neg)
                            it->setForeground(QColor(gs_pos_neg_color(v)));
                        table->setItem(r, col, it);
                    };
                    add_n(2, sm);
                    add_n(3, pm);
                    add_n(4, bias, true);
                }
                table->setRowHeight(r, row_px(26));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("Sampling: %1 method(s)  pop=%2  n=%3 (%4%)")
                                   .arg(value_obj.size())
                                   .arg(pop_n)
                                   .arg(sample_n)
                                   .arg(frac * 100, 0, 'f', 1));
        rendered = true;
    }

    // ── 10. CENTRAL LIMIT THEOREM ───────────────────────────────────────────
    else if (command == "central_limit_theorem") {
        const QJsonObject params = result.value("parameters").toObject();
        const QString dist = params.value("population_dist").toString().toUpper();
        const double pmean = meta.value("population_mean").toDouble();
        const double pstd = meta.value("population_std").toDouble();
        const int pop_n = meta.value("population_size").toInt();

        QList<QWidget*> top = {
            gs_make_card(tr("DISTRIBUTION"), dist.isEmpty() ? QStringLiteral("—") : dist, this),
            gs_make_card(tr("POP MEAN"), gs_fmt_num(pmean, 4), this),
            gs_make_card(tr("POP STD"), gs_fmt_num(pstd, 4), this),
            gs_make_card(tr("POP SIZE"), QString::number(pop_n), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-sample-size table
        QList<QPair<int, QJsonObject>> rows;
        for (auto it = value_obj.begin(); it != value_obj.end(); ++it) {
            if (!it.value().isObject())
                continue;
            const QString k = it.key(); // "n_5", "n_30", etc
            const int n = k.startsWith("n_") ? k.mid(2).toInt() : 0;
            rows.append({n, it.value().toObject()});
        }
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        if (!rows.isEmpty()) {
            auto* table = new QTableWidget(rows.size(), 7, this);
            table->setHorizontalHeaderLabels({tr("n"), tr("Theor. Mean"), tr("Theor. SE"), tr("Empir. Mean"),
                                              tr("Empir. SE"), tr("Mean Bias"), tr("Normal?")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows.size() * row_px(26) + row_px(32), 320));
            for (int r = 0; r < rows.size(); ++r) {
                const auto& o = rows[r].second;
                table->setItem(r, 0, new QTableWidgetItem(QString::number(rows[r].first)));
                auto add = [&](int col, double v, int dec) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, col, it);
                };
                add(1, o.value("theoretical_mean").toDouble(), 4);
                add(2, o.value("theoretical_std").toDouble(), 4);
                add(3, o.value("empirical_mean").toDouble(), 4);
                add(4, o.value("empirical_std").toDouble(), 4);
                const double mb = o.value("mean_bias").toDouble();
                auto* mbi = new QTableWidgetItem(QString::number(mb, 'f', 4));
                mbi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                mbi->setForeground(QColor(gs_pos_neg_color(mb)));
                table->setItem(r, 5, mbi);
                const QJsonValue norm = o.value("appears_normal");
                QString norm_str = norm.isNull() ? QStringLiteral("—") : (norm.toBool() ? tr("YES") : tr("NO"));
                auto* ni = new QTableWidgetItem(norm_str);
                ni->setTextAlignment(Qt::AlignCenter);
                if (!norm.isNull())
                    ni->setForeground(QColor(norm.toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()));
                table->setItem(r, 6, ni);
                table->setRowHeight(r, row_px(26));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("CLT %1: μ=%2  σ=%3  (%4 sample sizes)")
                                   .arg(dist)
                                   .arg(pmean, 0, 'f', 3)
                                   .arg(pstd, 0, 'f', 3)
                                   .arg(rows.size()));
        rendered = true;
    }

    // ── 11. SAMPLING ERROR ANALYSIS ─────────────────────────────────────────
    else if (command == "sampling_error_analysis") {
        const int rec_n = meta.value("recommended_sample_size").toInt();
        const double tgt_moe = meta.value("target_margin_of_error").toDouble();
        const double zc = meta.value("z_critical").toDouble();
        const double conf = meta.value("coverage_target").toDouble();

        QList<QWidget*> top = {
            gs_make_card(tr("RECOMMENDED n"), QString::number(rec_n), this, ui::colors::POSITIVE()),
            gs_make_card(tr("TARGET MoE"), gs_fmt_num(tgt_moe, 4), this),
            gs_make_card(tr("Z CRITICAL"), gs_fmt_num(zc, 4), this),
            gs_make_card(tr("CONFIDENCE"), QString::number(conf * 100, 'f', 1) + "%", this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QPair<int, QJsonObject>> rows;
        for (auto it = value_obj.begin(); it != value_obj.end(); ++it) {
            if (!it.value().isObject())
                continue;
            const auto o = it.value().toObject();
            rows.append({o.value("sample_size").toInt(), o});
        }
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        if (!rows.isEmpty()) {
            auto* table = new QTableWidget(rows.size(), 6, this);
            table->setHorizontalHeaderLabels(
                {tr("n"), tr("Theor. SE"), tr("Empir. SE"), tr("MoE"), tr("Coverage"), tr("MSE")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows.size() * row_px(26) + row_px(32), 320));
            for (int r = 0; r < rows.size(); ++r) {
                const auto& o = rows[r].second;
                table->setItem(r, 0, new QTableWidgetItem(QString::number(rows[r].first)));
                auto add = [&](int col, double v, int dec) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, col, it);
                };
                add(1, o.value("theoretical_se").toDouble(), 6);
                add(2, o.value("empirical_se").toDouble(), 6);
                add(3, o.value("theoretical_moe").toDouble(), 6);
                const double cov = o.value("coverage_probability").toDouble();
                auto* ci = new QTableWidgetItem(QString::number(cov * 100, 'f', 2) + "%");
                ci->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ci->setForeground(QColor(cov >= conf - 0.02 ? ui::colors::POSITIVE() : ui::colors::WARNING()));
                table->setItem(r, 4, ci);
                add(5, o.value("mean_squared_error").toDouble(), 6);
                table->setRowHeight(r, row_px(26));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(
            tr("Sampling error: recommended n=%1  target MoE=%2").arg(rec_n).arg(tgt_moe, 0, 'f', 4));
        rendered = true;
    }

    // ── 12. VALIDATE DATA ───────────────────────────────────────────────────
    else if (command == "validate_data") {
        const double score = result.value("quality_score").toDouble();
        const QString name = result.value("data_name").toString();
        const auto issues = result.value("issues").toArray();
        const auto warnings = result.value("warnings").toArray();
        const auto recs = result.value("recommendations").toArray();
        const auto stats_obj = result.value("statistics").toObject();

        QList<QWidget*> top = {
            gs_make_card(tr("QUALITY SCORE"), tr("%1 / 100").arg(QString::number(score, 'f', 1)), this,
                         score >= 80   ? ui::colors::POSITIVE()
                         : score >= 50 ? ui::colors::WARNING()
                                       : ui::colors::NEGATIVE()),
            gs_make_card(tr("DATA NAME"), name.isEmpty() ? tr("data") : name, this),
            gs_make_card(tr("ISSUES"), QString::number(issues.size()), this,
                         issues.isEmpty() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card(tr("WARNINGS"), QString::number(warnings.size()), this,
                         warnings.isEmpty() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Statistics table (if present)
        if (!stats_obj.isEmpty()) {
            QStringList sk;
            QHash<QString, QJsonValue> sv;
            for (auto it = stats_obj.begin(); it != stats_obj.end(); ++it) {
                if (!it.value().isObject() && !it.value().isArray()) {
                    sk << it.key();
                    sv.insert(it.key(), it.value());
                }
            }
            if (!sk.isEmpty()) {
                results_layout_->addWidget(gs_section_header(tr("STATISTICS"), accent));
                auto* table = new QTableWidget(sk.size(), 2, this);
                table->setHorizontalHeaderLabels({tr("Statistic"), tr("Value")});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(qMin(sk.size() * row_px(24) + row_px(32), 280));
                for (int r = 0; r < sk.size(); ++r) {
                    QString lbl = sk[r];
                    lbl.replace('_', ' ');
                    table->setItem(r, 0, new QTableWidgetItem(lbl.toUpper()));
                    auto* vi = new QTableWidgetItem(format_val(sv.value(sk[r])));
                    vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, 1, vi);
                    table->setRowHeight(r, row_px(24));
                }
                results_layout_->addWidget(table);
            }
        }

        // Issues / warnings / recommendations sections
        auto add_section = [this, accent](const QString& title, const QJsonArray& items, bool issue_shape,
                                          const QString& bar_color) {
            if (items.isEmpty())
                return;
            auto* hdr = new QLabel(tr("%1  (%2)").arg(title).arg(items.size()));
            hdr->setStyleSheet(QString("color:%1; font-weight:700; font-size:" + fincept::ui::fonts::px(-4) + "px; letter-spacing:1px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), bar_color));
            results_layout_->addWidget(hdr);
            for (const auto& v : items) {
                QString text;
                QString fg = ui::colors::TEXT_SECONDARY();
                if (issue_shape && v.isObject()) {
                    const auto o = v.toObject();
                    const QString sev = o.value("severity").toString().toUpper();
                    if (sev == "CRITICAL" || sev == "HIGH")
                        fg = ui::colors::NEGATIVE();
                    else if (sev == "MEDIUM")
                        fg = ui::colors::WARNING();
                    text = tr("[%1] %2: %3")
                               .arg(sev.isEmpty() ? QStringLiteral("—") : sev, o.value("type").toString(),
                                    o.value("description").toString(o.value("message").toString()));
                } else {
                    text = QStringLiteral("• ") + v.toString();
                }
                auto* item = new QLabel(text);
                item->setWordWrap(true);
                item->setStyleSheet(QString("color:%1; padding:2px 12px;").arg(fg));
                results_layout_->addWidget(item);
            }
        };
        add_section(tr("ISSUES"), issues, true, ui::colors::NEGATIVE());
        add_section(tr("WARNINGS"), warnings, true, ui::colors::WARNING());
        add_section(tr("RECOMMENDATIONS"), recs, false, ui::colors::INFO());

        status_label_->setText(tr("Validate %1: score=%2  issues=%3  warnings=%4")
                                   .arg(name)
                                   .arg(score, 0, 'f', 1)
                                   .arg(issues.size())
                                   .arg(warnings.size()));
        rendered = true;
    }

    // ── Fallback for unknown commands: flat metrics table ───────────────────
    if (!rendered) {
        QStringList keys;
        QHash<QString, QJsonValue> kv;
        if (value_obj.isEmpty() && !value_raw.isNull() && !value_raw.isObject() && !value_raw.isArray()) {
            keys.append("value");
            kv.insert("value", value_raw);
        } else {
            cfa_flatten_scalars(value_obj, "value", keys, kv);
        }
        cfa_flatten_scalars(meta, "metadata", keys, kv);
        if (!keys.isEmpty()) {
            auto* table = new QTableWidget(keys.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Metric"), tr("Value")});
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->verticalHeader()->setVisible(false);
            table->setMaximumHeight(qMin(keys.size() * row_px(28) + row_px(32), 480));
            table->setStyleSheet(table_ss());
            for (int r = 0; r < keys.size(); ++r) {
                QString label = keys[r];
                label.replace('_', ' ');
                table->setItem(r, 0, new QTableWidgetItem(label.toUpper()));
                auto* vi = new QTableWidgetItem(format_val(kv.value(keys[r])));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
            }
            results_layout_->addWidget(table);
        }
    }

    // ── Raw JSON viewer (collapsed by default — capped height) ──────────────
    auto* raw_hdr = new QLabel(tr("RAW JSON"));
    raw_hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:1px; margin-top:4px;")
                               .arg(ui::colors::TEXT_TERTIARY()));
    results_layout_->addWidget(raw_hdr);

    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(180);
    raw->setPlainText(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    raw->setStyleSheet(output_ss());
    results_layout_->addWidget(raw);

    // ── Export button ───────────────────────────────────────────────────────
    auto* export_btn = new QPushButton(tr("EXPORT RESULTS"));
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setMinimumHeight(28);
    export_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                                      "font-size:%3px; font-family:%4; padding:0 12px; }"
                                      "QPushButton:hover { color:%1; background:%5; }")
                                  .arg(ui::colors::legible(module_.color), ui::colors::BORDER_DIM())
                                  .arg(ui::fonts::px(-1))
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::overlay(0.05)));
    const QString json_str = QJsonDocument(payload).toJson(QJsonDocument::Indented);
    connect(export_btn, &QPushButton::clicked, this, [this, json_str, command]() {
        QString safe_name = module_.label + "_" + command;
        safe_name.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        const QString stored_name = safe_name + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json";
        const QString dest = services::FileManagerService::instance().storage_dir() + "/" + stored_name;
        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(json_str.toUtf8());
            f.close();
            services::FileManagerService::instance().register_file(stored_name, module_.label + "_" + command + ".json",
                                                                   QFileInfo(dest).size(), "application/json",
                                                                   "ai_quant_lab");
            LOG_INFO("AIQuantLab", "Exported result: " + stored_name);
        } else {
            LOG_WARN("AIQuantLab", fincept::logf("Result export could not be written",
                                                 {{"path", fincept::log_path(dest)}, {"error", f.errorString()}}));
            if (status_label_)
                status_label_->setText(tr("Export failed: %1").arg(f.errorString()));
        }
    });
    results_layout_->addWidget(export_btn);

    if (status_label_->text() == tr("Running..."))
        status_label_->setText(tr("Done"));
}

} // namespace fincept::screens
