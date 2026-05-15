// src/screens/ai_quant_lab/QuantModulePanel_AdvancedDisplays_Factors.cpp
//
// ADVANCED-category renderers for factor + strategy analytics:
// factor_evaluation (IC / evaluation report / risk metrics / factor returns)
// and strategy_builder (create_*, portfolio_metrics).
//
// Part of the partial-class split of QuantModulePanel_AdvancedDisplays.cpp.

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
} // namespace fincept::screens
