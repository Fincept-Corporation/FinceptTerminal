// src/screens/ai_quant_lab/QuantModulePanel_AdvancedDisplays_Models.cpp
//
// ADVANCED-category renderers for the model-training family: advanced_models
// (train_model), feature_engineering (per-indicator,
// select_features_by_ic, evaluate_expression), and portfolio_opt (BL, HRP,
// MinVar, MaxSharpe, EF).
//
// Part of the partial-class split of QuantModulePanel_AdvancedDisplays.cpp.

#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
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
using namespace fincept::screens::quant_charts;

void QuantModulePanel::display_advanced_models_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = ui::colors::legible(module_.color);
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "train_model") {
        const double final_loss = payload.value("final_loss").toDouble();
        const int epochs = payload.value("epochs_trained").toInt();
        const auto loss_hist = payload.value("loss_history").toArray();
        const double initial_loss = loss_hist.isEmpty() ? 0.0 : loss_hist[0].toDouble();
        const QJsonObject data = payload.value("data").toObject();
        const QJsonObject hold = payload.value("holdout_metrics").toObject();
        const QJsonObject base = payload.value("baseline").toObject();
        const QJsonValue ratio = payload.value("mse_vs_baseline");
        const auto period = [](const QJsonValue& v) {
            const QJsonArray a = v.toArray();
            return a.size() == 2 ? QStringLiteral("%1 → %2").arg(a[0].toString(), a[1].toString())
                                 : QString::fromUtf8("—");
        };

        QList<QWidget*> top = {
            gs_make_card(tr("MODEL"), payload.value("model_type").toString().toUpper(), this),
            gs_make_card(tr("TICKER"), data.value("ticker").toString(QStringLiteral("—")), this),
            gs_make_card(tr("EPOCHS"), QString::number(epochs), this),
            gs_make_card(tr("FINAL TRAIN LOSS"), QString::number(final_loss, 'g', 4), this,
                         initial_loss > 0 && final_loss < initial_loss ? ui::colors::SUCCESS() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Hold-out scores next to the naive "training-window mean return" forecast: a sequence
        // model that cannot beat it has learned nothing usable about the forward return.
        results_layout_->addWidget(gs_section_header(tr("HOLD-OUT  |  %1  |  %2 sequences  |  %3-day forward return")
                                                         .arg(period(data.value("test_period")))
                                                         .arg(data.value("test_sequences").toInt())
                                                         .arg(data.value("horizon").toInt()),
                                                     accent));
        const bool beats = payload.value("beats_baseline").toBool();
        QList<QWidget*> scores = {
            gs_make_card(tr("RMSE"), fmt_pct_safe(hold.value("rmse")), this),
            gs_make_card(tr("BASELINE RMSE"), fmt_pct_safe(base.value("rmse")), this),
            gs_make_card(tr("MSE / BASELINE"), fmt_num_safe(ratio, 3), this,
                         ratio.isDouble() ? (beats ? ui::colors::SUCCESS() : ui::colors::WARNING())
                                          : ui::colors::TEXT_SECONDARY()),
            gs_make_card(tr("VERDICT"), beats ? tr("BEATS BASELINE") : tr("NO EDGE"), this,
                         beats ? ui::colors::SUCCESS() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(scores, this));
        QList<QWidget*> direction = {
            gs_make_card(tr("DIRECTION HIT RATE"), fmt_pct_safe(hold.value("directional_accuracy"), 1), this),
            gs_make_card(tr("BASELINE HIT RATE"), fmt_pct_safe(base.value("directional_accuracy"), 1), this),
            gs_make_card(tr("IC (PRED vs ACTUAL)"), fmt_num_safe(hold.value("ic"), 3), this),
            gs_make_card(tr("R²"), fmt_num_safe(hold.value("r2_score"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(direction, this));

        // Predicted vs realised forward return over the hold-out window.
        const QJsonObject ho = payload.value("holdout").toObject();
        const QJsonArray ho_dates = ho.value("dates").toArray();
        if (!ho_dates.isEmpty()) {
            QcChartSpec spec;
            spec.title = tr("HOLD-OUT: FORWARD RETURN, PREDICTED vs ACTUAL");
            spec.x_time = true;
            for (const auto& d : ho_dates) {
                double ms = kQcNaN;
                qc_date_ms(d.toString(), ms);
                spec.xs.append(ms);
            }
            spec.fmt = [](double v) {
                return std::isfinite(v) ? QLocale().toString(v * 100.0, 'f', 2) + QLatin1Char('%')
                                        : QString::fromUtf8("—");
            };
            QcLine actual;
            actual.name = tr("Actual");
            actual.ys = qc_values(ho.value("actual").toArray());
            actual.color = ui::ChartColorRole::Muted;
            actual.has_baseline = true;
            actual.baseline = 0.0;
            spec.lines.append(actual);
            QcLine predicted;
            predicted.name = tr("Predicted");
            predicted.ys = qc_values(ho.value("predicted").toArray());
            predicted.color = ui::ChartColorRole::Accent;
            predicted.width = 2.0;
            spec.lines.append(predicted);
            results_layout_->addWidget(qc_series_chart(spec, this));
        }

        // Training-loss curve over every epoch, then the last epochs (newest first) — the
        // old 12-row sample never showed the final epoch the FINAL LOSS card quotes.
        if (!loss_hist.isEmpty()) {
            QcChartSpec spec;
            spec.title = tr("TRAINING LOSS BY EPOCH (standardised target)");
            spec.xs = qc_x_positions(loss_hist.size()).xs;
            spec.fmt = [](double v) { return QString::number(v, 'g', 4); };
            spec.x_label = [](double v) {
                return QCoreApplication::translate("QuantModulePanel", "epoch %1").arg(static_cast<qlonglong>(std::llround(v)));
            };
            QcLine l;
            l.name = tr("Loss");
            l.ys = qc_values(loss_hist);
            l.color = ui::ChartColorRole::Accent;
            l.width = 2.0;
            spec.lines.append(l);
            results_layout_->addWidget(qc_series_chart(spec, this));

            const QVector<int> latest = qc_latest_rows(loss_hist.size());
            const int rows = latest.size();
            results_layout_->addWidget(qc_caption(qc_latest_caption(rows, loss_hist.size())));
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({tr("Epoch"), tr("Loss")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * row_px(22) + row_px(32));
            for (int r = 0; r < rows; ++r) {
                const int i = latest.at(r);
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                auto* it = new QTableWidgetItem(QString::number(loss_hist[i].toDouble(), 'g', 6));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, it);
                table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        // Where the numbers come from.
        results_layout_->addWidget(qc_caption(
            tr("DATA  ·  %1  ·  %2 bars %3  ·  train %4  ·  test %5  ·  %6-bar windows  ·  %7 rows purged at the "
               "split  ·  %8")
                .arg(data.value("ticker").toString(), QString::number(data.value("bars").toInt()),
                     QStringLiteral("(%1 → %2)").arg(data.value("start_date").toString(),
                                                     data.value("end_date").toString()),
                     period(data.value("train_period")), period(data.value("test_period")),
                     QString::number(data.value("seq_length").toInt()),
                     QString::number(data.value("purged_rows").toInt()), data.value("source").toString())));
        status_label_->setText(tr("%1 on %2: hold-out MSE %3 × baseline")
                                   .arg(payload.value("model_type").toString().toUpper(),
                                        data.value("ticker").toString(), fmt_num_safe(ratio, 3)));
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

    const QString accent = ui::colors::legible(module_.color);
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── select_features_by_ic ────────────────────────────────────────────────
    if (command == "select_features_by_ic") {
        const auto sel = payload.value("selected_features").toArray();
        QList<QWidget*> top = {
            gs_make_card(tr("SELECTED"), QString::number(sel.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!sel.isEmpty()) {
            const auto ic_scores = payload.value("ic_scores").toObject(); // signed IC per selected feature
            auto* table = new QTableWidget(sel.size(), 3, this);
            table->setHorizontalHeaderLabels({tr("Rank"), tr("Feature"), tr("IC")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(sel.size() * row_px(24) + row_px(32), 320));
            for (int i = 0; i < sel.size(); ++i) {
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
                table->setItem(i, 1, new QTableWidgetItem(sel[i].toString()));
                if (ic_scores.contains(sel[i].toString())) {
                    const double ic = ic_scores.value(sel[i].toString()).toDouble();
                    auto* ic_item = new QTableWidgetItem(QString::number(ic, 'f', 4));
                    ic_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    ic_item->setForeground(QColor(gs_pos_neg_color(ic)));
                    table->setItem(i, 2, ic_item);
                }
                table->setRowHeight(i, row_px(24));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("Selected %1 feature(s) by IC").arg(sel.size()));
        return;
    }

    // ── Multi-series indicators (MACD, Bollinger) ────────────────────────────
    auto render_multi_series = [this](const QStringList& keys, const QJsonObject& payload) {
        QList<QWidget*> stats;
        QHash<QString, QJsonArray> series;
        for (const auto& k : keys) {
            const auto arr = payload.value(k).toArray();
            series.insert(k, arr);
            double sum = 0, mn = std::numeric_limits<double>::infinity(), mx = -std::numeric_limits<double>::infinity();
            int n = 0;
            for (const auto& v : arr) {
                if (v.isNull())
                    continue;
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

        // Every component over the whole series (warm-up NaNs are gaps), then the latest rows.
        int max_n = 0;
        // QJsonArray::size() returns qsizetype (qint64); std::max requires
        // matching arg types — explicit cast keeps both as int.
        for (const auto& a : series)
            max_n = std::max(max_n, static_cast<int>(a.size()));
        {
            QcChartSpec spec;
            QStringList names;
            for (const auto& k : keys)
                names << k.toUpper();
            spec.title = names.join(QStringLiteral(" / "));
            spec.xs = qc_x_positions(max_n).xs;
            spec.fmt = qc_fmt_auto();
            spec.x_label = [](double v) {
                return QStringLiteral("#%1").arg(static_cast<qlonglong>(std::llround(v)));
            };
            const bool macd = keys.contains(QStringLiteral("histogram"));
            for (int c = 0; c < keys.size(); ++c) {
                QcLine l;
                l.name = keys[c].toUpper();
                l.ys = qc_values(series[keys[c]]);
                if (macd && keys[c] == QLatin1String("histogram")) {
                    l.style = ui::TimeSeriesChart::Style::Bars;
                    l.color = ui::ChartColorRole::Muted;
                } else if (!macd && keys[c] == QLatin1String("middle")) {
                    l.color = ui::ChartColorRole::Accent;
                    l.width = 2.0;
                } else {
                    l.color = ui::ChartColorRole::Palette;
                    l.palette = c;
                    l.dashed = keys.contains(QStringLiteral("middle")); // Bollinger upper / lower bands
                }
                spec.lines.append(l);
            }
            results_layout_->addWidget(qc_series_chart(spec, this));
        }
        const QVector<int> latest = qc_latest_rows(max_n);
        const int rows = latest.size();
        results_layout_->addWidget(qc_caption(qc_latest_caption(rows, max_n)));
        auto* table = new QTableWidget(rows, keys.size() + 1, this);
        QStringList hdr = {tr("Index")};
        for (const auto& k : keys)
            hdr << k.toUpper();
        table->setHorizontalHeaderLabels(hdr);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(rows * row_px(22) + row_px(32));
        for (int r = 0; r < rows; ++r) {
            const int i = latest.at(r);
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
            table->setRowHeight(r, row_px(22));
        }
        results_layout_->addWidget(table);
    };

    if (command == "macd") {
        render_multi_series({"macd", "signal", "histogram"}, payload);
        status_label_->setText(tr("MACD computed"));
        return;
    }
    if (command == "bollinger_bands") {
        render_multi_series({"upper", "middle", "lower"}, payload);
        status_label_->setText(tr("Bollinger bands computed"));
        return;
    }
    // %K and %D, aligned to the bars where both are defined.
    if (command == "stochastic_oscillator") {
        render_multi_series({"k", "d"}, payload);
        status_label_->setText(tr("Stochastic oscillator: %1 bars").arg(payload.value("k").toArray().size()));
        return;
    }

    // ── Single-series indicators + evaluate_expression ───────────────────────
    if (payload.contains("data") && payload.value("data").isArray()) {
        const auto series = payload.value("data").toArray();
        double sum = 0, mn = std::numeric_limits<double>::infinity(), mx = -std::numeric_limits<double>::infinity();
        int n = 0;
        for (const auto& v : series) {
            if (v.isNull())
                continue;
            const double d = v.toDouble();
            sum += d;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
            ++n;
        }
        const double mean = n > 0 ? sum / n : 0.0;

        QList<QWidget*> top = {
            gs_make_card(tr("INDICATOR"), command.toUpper(), this),
            gs_make_card(tr("OBSERVATIONS"), QString::number(n), this),
            gs_make_card(tr("MEAN"), QString::number(mean, 'f', 4), this, gs_pos_neg_color(mean)),
            gs_make_card(tr("RANGE"), n > 0 ? QString("[%1, %2]").arg(mn, 0, 'f', 4).arg(mx, 0, 'f', 4) : "—", this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // The whole indicator series (warm-up NaNs are gaps), then the latest rows.
        {
            QcChartSpec spec;
            spec.title = command.toUpper();
            spec.xs = qc_x_positions(series.size()).xs;
            spec.fmt = qc_fmt_auto();
            spec.x_label = [](double v) {
                return QStringLiteral("#%1").arg(static_cast<qlonglong>(std::llround(v)));
            };
            QcLine l;
            l.name = command.toUpper();
            l.ys = qc_values(series);
            l.color = ui::ChartColorRole::Accent;
            spec.lines.append(l);
            auto* chart = qc_series_chart(spec, this);
            if (command == QLatin1String("rsi")) {
                chart->add_hline(70.0, QStringLiteral("70"), ui::ChartColorRole::Negative);
                chart->add_hline(30.0, QStringLiteral("30"), ui::ChartColorRole::Positive);
            }
            results_layout_->addWidget(chart);
        }
        const QVector<int> latest = qc_latest_rows(series.size());
        const int rows = latest.size();
        results_layout_->addWidget(qc_caption(qc_latest_caption(rows, series.size())));
        auto* table = new QTableWidget(rows, 2, this);
        table->setHorizontalHeaderLabels({tr("Index"), command.toUpper()});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(rows * row_px(22) + row_px(32));
        for (int r = 0; r < rows; ++r) {
            const int i = latest.at(r);
            table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
            if (!series[i].isNull()) {
                const double v = series[i].toDouble();
                auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, it);
            }
            table->setRowHeight(r, row_px(22));
        }
        results_layout_->addWidget(table);
        status_label_->setText(tr("%1: %2 obs").arg(command).arg(n));
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

    const QString accent = ui::colors::legible(module_.color);
    const QString method = payload.value("method").toString();
    QString header_text = (method.isEmpty() ? command : method).toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

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
            if (vol < min_vol) {
                min_vol = vol;
                min_vol_pt = p;
            }
            if (sh > max_sharpe) {
                max_sharpe = sh;
                max_sharpe_pt = p;
            }
        }

        QList<QWidget*> top = {
            gs_make_card(tr("PORTFOLIOS"), QString::number(payload.value("num_portfolios").toInt()), this,
                         ui::colors::POSITIVE()),
            gs_make_card(tr("MIN VOL"), fmt_pct_safe(QJsonValue(min_vol)), this, ui::colors::INFO()),
            gs_make_card(tr("MAX SHARPE"), QString::number(max_sharpe, 'f', 3), this,
                         max_sharpe > 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("RANGE"),
                         frontier.isEmpty()
                             ? "—"
                             : QString("%1% – %2%")
                                   .arg(frontier.first().toObject().value("return").toDouble() * 100, 0, 'f', 1)
                                   .arg(frontier.last().toObject().value("return").toDouble() * 100, 0, 'f', 1),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Risk / return picture of every frontier portfolio, the max-Sharpe and min-vol
        // points marked; the full table (sortable) below it.
        if (!frontier.isEmpty()) {
            QVector<double> vols;
            QVector<double> rets;
            int best_i = -1;
            int minv_i = -1;
            for (qsizetype i = 0; i < frontier.size(); ++i) {
                const auto p = frontier[i].toObject();
                vols << p.value("volatility").toDouble() * 100.0;
                rets << p.value("return").toDouble() * 100.0;
                if (p.value("sharpe").toDouble() == max_sharpe && best_i < 0)
                    best_i = static_cast<int>(i);
                if (p.value("volatility").toDouble() == min_vol && minv_i < 0)
                    minv_i = static_cast<int>(i);
            }
            results_layout_->addWidget(qc_caption(tr("EFFICIENT FRONTIER  ·  IN THE UNITS OF THE INPUT RETURNS")));
            results_layout_->addWidget(qc_frontier_chart(vols, rets, best_i, minv_i, this));

            const int rows = static_cast<int>(frontier.size());
            auto* table = new QTableWidget(rows, 3, this);
            table->setHorizontalHeaderLabels({tr("Return"), tr("Volatility"), tr("Sharpe")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows * row_px(24) + row_px(32), row_px(320)));
            for (int r = 0; r < rows; ++r) {
                const auto p = frontier[r].toObject();
                const double ret = p.value("return").toDouble();
                const double vol = p.value("volatility").toDouble();
                const double sh = p.value("sharpe").toDouble();
                table->setItem(r, 0, num_item(QString::number(ret * 100, 'f', 2) + "%", ret, gs_pos_neg_color(ret)));
                table->setItem(r, 1, num_item(QString::number(vol * 100, 'f', 2) + "%", vol));
                table->setItem(r, 2, num_item(QString::number(sh, 'f', 3), sh,
                                              sh >= 1.0 ? ui::colors::POSITIVE()
                                              : sh > 0  ? ui::colors::WARNING()
                                                        : ui::colors::NEGATIVE()));
                table->setRowHeight(r, row_px(24));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("EF: %1 portfolios  best Sharpe=%2")
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
        gs_make_card(tr("ASSETS"), QString::number(weights.size()), this, ui::colors::POSITIVE()),
        gs_make_card(tr("EXP. VOLATILITY"), fmt_pct_safe(QJsonValue(exp_vol)), this),
        gs_make_card(tr("EXP. RETURN"), has_ret ? fmt_pct_safe(QJsonValue(exp_ret)) : "—", this,
                     has_ret ? gs_pos_neg_color(exp_ret) : ui::colors::TEXT_SECONDARY()),
        gs_make_card(tr("SHARPE"), has_sharpe ? QString::number(sharpe, 'f', 3) : "—", this,
                     !has_sharpe     ? ui::colors::TEXT_SECONDARY()
                     : sharpe >= 1.0 ? ui::colors::POSITIVE()
                     : sharpe > 0    ? ui::colors::WARNING()
                                     : ui::colors::NEGATIVE()),
    };
    results_layout_->addWidget(gs_card_row(top, this));

    // Black-Litterman extras
    if (command == "black_litterman") {
        QList<QWidget*> bl = {
            gs_make_card(tr("VIEWS"), QString::number(payload.value("views_incorporated").toInt()), this,
                         ui::colors::INFO()),
            gs_make_card(tr("EQUILIBRIUM"), tr("%1 assets").arg(payload.value("equilibrium_returns").toObject().size()),
                         this),
            gs_make_card(tr("POSTERIOR"), tr("%1 assets").arg(payload.value("posterior_returns").toObject().size()),
                         this),
            gs_make_card(tr("METHOD"), "BL", this),
        };
        results_layout_->addWidget(gs_card_row(bl, this));
    }

    if (!weights.isEmpty()) {
        results_layout_->addWidget(gs_section_header(tr("WEIGHTS"), accent));
        results_layout_->addWidget(build_weights_view(weights, this));
    }

    status_label_->setText(tr("%1: %2 assets, vol=%3%")
                               .arg(method.isEmpty() ? command : method)
                               .arg(weights.size())
                               .arg(exp_vol * 100, 0, 'f', 2));
}

// ═════════════════════════════════════════════════════════════════════════════
// 6. FACTOR EVALUATION
// ═════════════════════════════════════════════════════════════════════════════
} // namespace fincept::screens
