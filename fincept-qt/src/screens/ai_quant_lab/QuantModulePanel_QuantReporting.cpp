// src/screens/ai_quant_lab/QuantModulePanel_QuantReporting.cpp
//
// Quant Reporting panel — 5 report builders (IC analysis, cumulative returns,
// risk report, model performance, factor quantiles). Mirrors GS Quant /
// Functime / Statsmodels / Fortitudo pattern. Extracted from QuantModulePanel.cpp
// to keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/charts/BarChart.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;
using namespace fincept::screens::quant_charts;

namespace {

// The muted line under a predictor tab's inputs: what a ticker source scores (D1).
QLabel* rp_signal_hint(QWidget* parent) {
    auto* l = new QLabel(QCoreApplication::translate(
                             "QuantModulePanel",
                             "With a ticker as the source, the signal is the 20-day momentum (close / close 20 days "
                             "earlier − 1) and the return is the next day's return: the report scores momentum as "
                             "a one-day predictor."),
                         parent);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;")
                         .arg(ui::colors::TEXT_TERTIARY()));
    return l;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// QUANT REPORTING PANEL — 5 report builders (IC analysis, cumulative returns,
// risk report, model performance, factor quantiles). Mirrors GS Quant /
// Functime / Statsmodels / Fortitudo pattern. Each view opens on real data (D1:
// the daily prices of a symbol or a pair, 1 year) and runs on first show; the
// paste boxes are the "Custom series" source.
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_quant_reporting_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = make_view_tabs(w);

    // Shared helper: synthetic-sample button styled by the panel sheet; runs
    // fed with its data are flagged SAMPLE INPUT on the results.
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed, const QString& tip) -> QPushButton* {
        return make_sample_button(edit, parent, tip, [seed]() { return sample_returns(seed); });
    };

    // ── IC Analysis ──────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("ic_analysis"), .cost = ViewCost::Fetch, .loading = tr("Computing the information coefficient…")}, tr("IC Analysis"),
                 [this, add_sample_btn](QWidget* ic_tab, QVBoxLayout* icvl) {
        add_series_source(ic_tab, icvl, SeriesShape::Single); // momentum vs next-day return (D1)

        auto* ic_preds = new QLineEdit(ic_tab);
        ic_preds->setPlaceholderText(tr("Model predictions (decimals, >= 10 values)"));
        ic_preds->setStyleSheet(input_ss());
        text_inputs_["rp_ic_preds"] = ic_preds;
        mark_series_input(ic_preds, SeriesKind::Signal);
        icvl->addWidget(build_input_row(tr("Predictions"), ic_preds, ic_tab));
        icvl->addWidget(add_sample_btn(ic_preds, ic_tab, 211, tr("252 synthetic predictions (mu=0.05%, sigma=1.2%)")));

        auto* ic_rets = new QLineEdit(ic_tab);
        ic_rets->setPlaceholderText(tr("Realized returns (decimals, same length as predictions)"));
        ic_rets->setStyleSheet(input_ss());
        text_inputs_["rp_ic_rets"] = ic_rets;
        mark_series_input(ic_rets, SeriesKind::FwdReturns);
        icvl->addWidget(build_input_row(tr("Returns"), ic_rets, ic_tab));
        icvl->addWidget(add_sample_btn(ic_rets, ic_tab, 212, tr("252 synthetic realized returns")));
        {
            auto* hint = rp_signal_hint(ic_tab);
            mark_series_widget(hint, /*ticker_only=*/true);
            icvl->addWidget(hint);
        }

        auto* ic_method = new QComboBox(ic_tab);
        ic_method->addItems({"both", "pearson", "spearman"});
        ic_method->setStyleSheet(combo_ss());
        combo_inputs_["rp_ic_method"] = ic_method;
        icvl->addWidget(build_input_row(tr("IC Method"), ic_method, ic_tab));

        auto* ic_window = new QSpinBox(ic_tab);
        ic_window->setRange(5, 100);
        ic_window->setValue(20);
        ic_window->setStyleSheet(spinbox_ss());
        int_inputs_["rp_ic_window"] = ic_window;
        icvl->addWidget(build_input_row(tr("Rolling Window"), ic_window, ic_tab));

        auto* ic_run = make_run_button(tr("RUN IC ANALYSIS"), ic_tab);
        set_action(ic_run, [this]() {
            QJsonArray preds, rets;
            QString bad;
            if (!parse_doubles(series_text("rp_ic_preds"), preds, &bad)) {
                display_error(tr("Predictions: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("rp_ic_rets"), rets, &bad)) {
                display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (preds.size() < 10) {
                display_error(tr("IC analysis needs at least 10 predictions; you provided %1.").arg(preds.size()));
                return;
            }
            if (preds.size() != rets.size()) {
                display_error(
                    tr("Predictions (%1) and returns (%2) must have the same length.").arg(preds.size()).arg(rets.size()));
                return;
            }
            show_loading(tr("Computing IC + rolling IC on %1 observations...").arg(preds.size()));
            QJsonObject params;
            params["predictions"] = preds;
            params["returns"] = rets;
            params["method"] = combo_inputs_["rp_ic_method"]->currentText();
            params["window"] = int_inputs_["rp_ic_window"]->value();
            AIQuantLabService::instance().reporting_ic_analysis(params);
        });
        icvl->addWidget(ic_run);
        icvl->addStretch();
    });

    // ── Cumulative Returns ───────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("cumulative_returns"), .cost = ViewCost::Fetch, .loading = tr("Compounding the returns…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Cumulative Returns"),
                 [this, add_sample_btn](QWidget* cr_tab, QVBoxLayout* crvl) {
        // The symbol's daily returns against the benchmark's (D1).
        add_series_source(cr_tab, crvl, SeriesShape::Pair, tr("Benchmark"));

        auto* cr_rets = new QLineEdit(cr_tab);
        cr_rets->setPlaceholderText(tr("Portfolio daily returns (decimals)"));
        cr_rets->setStyleSheet(input_ss());
        text_inputs_["rp_cr_rets"] = cr_rets;
        mark_series_input(cr_rets, SeriesKind::Returns, 0);
        crvl->addWidget(build_input_row(tr("Returns"), cr_rets, cr_tab));
        crvl->addWidget(add_sample_btn(cr_rets, cr_tab, 221, tr("252 synthetic portfolio returns")));

        auto* cr_bench = new QLineEdit(cr_tab);
        cr_bench->setPlaceholderText(tr("Benchmark returns (optional; same length as portfolio if provided)"));
        cr_bench->setStyleSheet(input_ss());
        text_inputs_["rp_cr_bench"] = cr_bench;
        mark_series_input(cr_bench, SeriesKind::Returns, 1);
        crvl->addWidget(build_input_row(tr("Benchmark Returns (optional)"), cr_bench, cr_tab));
        crvl->addWidget(add_sample_btn(cr_bench, cr_tab, 222, tr("252 synthetic benchmark returns")));

        auto* cr_title = new QLineEdit(cr_tab);
        cr_title->setPlaceholderText(tr("Chart title (e.g. Strategy vs S&P 500)"));
        cr_title->setText(tr("Cumulative Returns"));
        cr_title->setStyleSheet(input_ss());
        text_inputs_["rp_cr_title"] = cr_title;
        crvl->addWidget(build_input_row(tr("Title"), cr_title, cr_tab));

        auto* cr_run = make_run_button(tr("BUILD CUMULATIVE RETURNS REPORT"), cr_tab);
        set_action(cr_run, [this]() {
            QJsonArray rets;
            QString bad;
            if (!parse_doubles(series_text("rp_cr_rets"), rets, &bad)) {
                display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (rets.size() < 5) {
                display_error(tr("Need at least 5 returns; you provided %1.").arg(rets.size()));
                return;
            }
            QJsonObject params;
            params["returns"] = rets;
            const QString bench_text = series_text("rp_cr_bench").trimmed();
            if (!bench_text.isEmpty()) {
                QJsonArray bench;
                if (!parse_doubles(bench_text, bench, &bad)) {
                    display_error(tr("Benchmark: '%1' is not numeric.").arg(bad));
                    return;
                }
                if (bench.size() != rets.size()) {
                    display_error(tr("Benchmark (%1) and returns (%2) must have the same length.")
                                      .arg(bench.size())
                                      .arg(rets.size()));
                    return;
                }
                params["benchmark_returns"] = bench;
            }
            const QString title = text_inputs_["rp_cr_title"]->text().trimmed();
            params["title"] = title.isEmpty() ? tr("Cumulative Returns") : title;
            show_loading(tr("Building cumulative returns report on %1 obs...").arg(rets.size()));
            AIQuantLabService::instance().reporting_cumulative_returns(params);
        });
        crvl->addWidget(cr_run);
        crvl->addStretch();
    });

    // ── Risk Report ──────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("risk_report"), .cost = ViewCost::Fetch, .loading = tr("Computing drawdowns, VaR and volatility…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Risk Report"),
                 [this, add_sample_btn](QWidget* rk_tab, QVBoxLayout* rkvl) {
        add_series_source(rk_tab, rkvl, SeriesShape::Single); // daily returns of the symbol (D1)

        auto* rk_rets = new QLineEdit(rk_tab);
        rk_rets->setPlaceholderText(tr("Daily returns (>= 30 values)"));
        rk_rets->setStyleSheet(input_ss());
        text_inputs_["rp_rk_rets"] = rk_rets;
        mark_series_input(rk_rets, SeriesKind::Returns);
        rkvl->addWidget(build_input_row(tr("Returns"), rk_rets, rk_tab));
        rkvl->addWidget(add_sample_btn(rk_rets, rk_tab, 231, tr("252 synthetic daily returns")));

        auto* rk_window = new QSpinBox(rk_tab);
        rk_window->setRange(5, 100);
        rk_window->setValue(20);
        rk_window->setSuffix(tr(" days"));
        rk_window->setToolTip(tr("Trailing window of the rolling annualized volatility."));
        rk_window->setStyleSheet(spinbox_ss());
        int_inputs_["rp_rk_window"] = rk_window;
        rkvl->addWidget(build_input_row(tr("Volatility Window"), rk_window, rk_tab));

        auto* rk_hint =
            new QLabel(tr("Computes max drawdown with peak/trough/recovery markers, daily VaR + CVaR at 5% and 1%, "
                          "and rolling annualized volatility."),
                       rk_tab);
        rk_hint->setWordWrap(true);
        rk_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        rkvl->addWidget(rk_hint);

        auto* rk_run = make_run_button(tr("BUILD RISK REPORT"), rk_tab);
        set_action(rk_run, [this]() {
            QJsonArray rets;
            QString bad;
            if (!parse_doubles(series_text("rp_rk_rets"), rets, &bad)) {
                display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (rets.size() < 30) {
                display_error(tr("Risk report needs at least 30 obs; you provided %1.").arg(rets.size()));
                return;
            }
            QJsonObject params;
            params["returns"] = rets;
            params["rolling_window"] = int_inputs_["rp_rk_window"]->value();
            show_loading(tr("Computing drawdown + rolling vol on %1 obs...").arg(rets.size()));
            AIQuantLabService::instance().reporting_risk_report(params);
        });
        rkvl->addWidget(rk_run);
        rkvl->addStretch();
    });

    // ── Model Performance ────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("model_performance"), .cost = ViewCost::Fetch, .loading = tr("Scoring the model's predictions…")}, tr("Model Performance"),
                 [this, add_sample_btn](QWidget* mp_tab, QVBoxLayout* mpvl) {
        add_series_source(mp_tab, mpvl, SeriesShape::Single); // momentum vs next-day return (D1)

        auto* mp_preds = new QLineEdit(mp_tab);
        mp_preds->setPlaceholderText(tr("Model predictions (decimals, >= 20 values)"));
        mp_preds->setStyleSheet(input_ss());
        text_inputs_["rp_mp_preds"] = mp_preds;
        mark_series_input(mp_preds, SeriesKind::Signal);
        mpvl->addWidget(build_input_row(tr("Predictions"), mp_preds, mp_tab));
        mpvl->addWidget(add_sample_btn(mp_preds, mp_tab, 241, tr("252 synthetic model predictions")));

        auto* mp_rets = new QLineEdit(mp_tab);
        mp_rets->setPlaceholderText(tr("Realized returns (same length as predictions)"));
        mp_rets->setStyleSheet(input_ss());
        text_inputs_["rp_mp_rets"] = mp_rets;
        mark_series_input(mp_rets, SeriesKind::FwdReturns);
        mpvl->addWidget(build_input_row(tr("Returns"), mp_rets, mp_tab));
        mpvl->addWidget(add_sample_btn(mp_rets, mp_tab, 242, tr("252 synthetic realized returns")));
        {
            auto* hint = rp_signal_hint(mp_tab);
            mark_series_widget(hint, /*ticker_only=*/true);
            mpvl->addWidget(hint);
        }

        auto* mp_name = new QLineEdit(mp_tab);
        mp_name->setPlaceholderText(tr("Model label (e.g. LightGBM, LSTM)"));
        mp_name->setText(tr("Model"));
        mp_name->setStyleSheet(input_ss());
        text_inputs_["rp_mp_name"] = mp_name;
        mpvl->addWidget(build_input_row(tr("Model Name"), mp_name, mp_tab));

        auto* mp_q = new QSpinBox(mp_tab);
        mp_q->setRange(2, 10);
        mp_q->setValue(5);
        mp_q->setStyleSheet(spinbox_ss());
        int_inputs_["rp_mp_n_quantiles"] = mp_q;
        mpvl->addWidget(build_input_row(tr("Quantile Buckets"), mp_q, mp_tab));

        auto* mp_run = make_run_button(tr("EVALUATE MODEL"), mp_tab);
        set_action(mp_run, [this]() {
            QJsonArray preds, rets;
            QString bad;
            if (!parse_doubles(series_text("rp_mp_preds"), preds, &bad)) {
                display_error(tr("Predictions: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("rp_mp_rets"), rets, &bad)) {
                display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (preds.size() < 20) {
                display_error(tr("Model evaluation needs at least 20 obs; you provided %1.").arg(preds.size()));
                return;
            }
            if (preds.size() != rets.size()) {
                display_error(
                    tr("Predictions (%1) and returns (%2) must have the same length.").arg(preds.size()).arg(rets.size()));
                return;
            }
            QJsonObject params;
            params["predictions"] = preds;
            params["returns"] = rets;
            QString name = text_inputs_["rp_mp_name"]->text().trimmed();
            // A ticker source scores the 20-day momentum: say so unless the user named the model.
            if (series_ticker_mode(QStringLiteral("model_performance")) && (name.isEmpty() || name == tr("Model")))
                name = tr("20-day momentum");
            params["model_name"] = name.isEmpty() ? tr("Model") : name;
            params["n_quantiles"] = int_inputs_["rp_mp_n_quantiles"]->value();
            show_loading(tr("Evaluating %1 on %2 obs...").arg(name.isEmpty() ? tr("model") : name).arg(preds.size()));
            AIQuantLabService::instance().reporting_model_performance(params);
        });
        mpvl->addWidget(mp_run);
        mpvl->addStretch();
    });

    // ── Factor Quantiles ─────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("factor_quantiles"), .cost = ViewCost::Fetch, .loading = tr("Bucketing the factor into quantiles…")}, tr("Factor Quantiles"),
                 [this, add_sample_btn](QWidget* fq_tab, QVBoxLayout* fqvl) {
        add_series_source(fq_tab, fqvl, SeriesShape::Single); // momentum vs next-day return (D1)

        auto* fq_preds = new QLineEdit(fq_tab);
        fq_preds->setPlaceholderText(tr("Factor / signal values (decimals, >= 20)"));
        fq_preds->setStyleSheet(input_ss());
        text_inputs_["rp_fq_preds"] = fq_preds;
        mark_series_input(fq_preds, SeriesKind::Signal);
        fqvl->addWidget(build_input_row(tr("Factor Signal"), fq_preds, fq_tab));
        fqvl->addWidget(add_sample_btn(fq_preds, fq_tab, 251, tr("252 synthetic factor values")));

        auto* fq_rets = new QLineEdit(fq_tab);
        fq_rets->setPlaceholderText(tr("Realized returns (same length as factor)"));
        fq_rets->setStyleSheet(input_ss());
        text_inputs_["rp_fq_rets"] = fq_rets;
        mark_series_input(fq_rets, SeriesKind::FwdReturns);
        fqvl->addWidget(build_input_row(tr("Realized Returns"), fq_rets, fq_tab));
        fqvl->addWidget(add_sample_btn(fq_rets, fq_tab, 252, tr("252 synthetic realized returns")));
        {
            auto* hint = rp_signal_hint(fq_tab);
            mark_series_widget(hint, /*ticker_only=*/true);
            fqvl->addWidget(hint);
        }

        auto* fq_q = new QSpinBox(fq_tab);
        fq_q->setRange(2, 10);
        fq_q->setValue(5);
        fq_q->setStyleSheet(spinbox_ss());
        int_inputs_["rp_fq_n_quantiles"] = fq_q;
        fqvl->addWidget(build_input_row(tr("Quantile Buckets"), fq_q, fq_tab));

        auto* fq_hint =
            new QLabel(tr("Sorts observations into N buckets by signal strength and reports realized return per bucket. "
                          "A monotone Q1→Q5 progression indicates a clean factor; non-monotone hints at noise."),
                       fq_tab);
        fq_hint->setWordWrap(true);
        fq_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        fqvl->addWidget(fq_hint);

        auto* fq_run = make_run_button(tr("ANALYZE FACTOR QUANTILES"), fq_tab);
        set_action(fq_run, [this]() {
            QJsonArray preds, rets;
            QString bad;
            if (!parse_doubles(series_text("rp_fq_preds"), preds, &bad)) {
                display_error(tr("Factor: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("rp_fq_rets"), rets, &bad)) {
                display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (preds.size() < 20) {
                display_error(tr("Quantile analysis needs at least 20 obs; you provided %1.").arg(preds.size()));
                return;
            }
            if (preds.size() != rets.size()) {
                display_error(
                    tr("Factor (%1) and returns (%2) must have the same length.").arg(preds.size()).arg(rets.size()));
                return;
            }
            QJsonObject params;
            params["predictions"] = preds;
            params["returns"] = rets;
            params["n_quantiles"] = int_inputs_["rp_fq_n_quantiles"]->value();
            show_loading(tr("Bucketing %1 obs into %2 quantiles...")
                             .arg(preds.size())
                             .arg(int_inputs_["rp_fq_n_quantiles"]->value()));
            AIQuantLabService::instance().reporting_factor_quantiles(params);
        });
        fqvl->addWidget(fq_run);
        fqvl->addStretch();
    });

    vl->addWidget(tabs);

    add_result_pane(vl);

    return w;
}

// ── QUANT REPORTING: rich card-based display per command ────────────────────

void QuantModulePanel::display_quant_reporting_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    if (!payload.value("success").toBool(false)) {
        const QString err = payload.value("error").toString(tr("Unknown error"));
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? tr("Input error: ")
                               : kind == "runtime"  ? tr("Computation failed: ")
                                                    : QString();
        display_error(prefix + err);
        return;
    }

    const QJsonObject d = payload.value("data").toObject();
    const QString accent = ui::colors::legible(module_.color);

    // Section header
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += tr("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 2. IC ANALYSIS ───────────────────────────────────────────────────────
    if (command == "ic_analysis") {
        const double pearson = d.value("overall_pearson_ic").toDouble();
        const double pearson_p = d.value("overall_pearson_p").toDouble();
        const double spearman = d.value("overall_spearman_ic").toDouble();
        const double spearman_p = d.value("overall_spearman_p").toDouble();
        const double icir = d.value("icir").toDouble();
        const double rank_icir = d.value("rank_icir").toDouble();
        const QString ic_verdict = d.value("ic_verdict").toString();
        const QString rank_verdict = d.value("rank_ic_verdict").toString();

        auto* hdr = new QLabel(tr("METHOD: %1   |   ROLLING WINDOW: %2   |   %3 ROLLING POINTS")
                                   .arg(d.value("method").toString().toUpper())
                                   .arg(d.value("window").toInt())
                                   .arg(d.value("n_rolling_points").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        auto verdict_color = [](const QString& v) {
            if (v == "STRONG")
                return ui::colors::POSITIVE();
            if (v == "MODERATE")
                return ui::colors::POSITIVE();
            if (v == "WEAK")
                return ui::colors::WARNING();
            if (v == "VERY WEAK")
                return ui::colors::WARNING();
            return ui::colors::NEGATIVE();
        };

        QList<QWidget*> overall = {
            gs_make_card(tr("PEARSON IC"), gs_fmt_num(pearson, 4), this, gs_pos_neg_color(pearson)),
            gs_make_card(tr("PEARSON p"), gs_fmt_num(pearson_p, 4), this,
                         pearson_p < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("SPEARMAN IC"), gs_fmt_num(spearman, 4), this, gs_pos_neg_color(spearman)),
            gs_make_card(tr("SPEARMAN p"), gs_fmt_num(spearman_p, 4), this,
                         spearman_p < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(overall, this));

        QList<QWidget*> ic_card = {
            gs_make_card(tr("ROLLING IC MEAN"), gs_fmt_num(d.value("ic_mean").toDouble(), 4), this,
                         gs_pos_neg_color(d.value("ic_mean").toDouble())),
            gs_make_card(tr("ROLLING IC σ"), gs_fmt_num(d.value("ic_std").toDouble(), 4), this),
            gs_make_card(tr("ICIR"), gs_fmt_num(icir, 3), this,
                         icir >= 0.5 ? ui::colors::POSITIVE()
                         : icir >= 0 ? ui::colors::WARNING()
                                     : ui::colors::NEGATIVE()),
            gs_make_card(tr("IC VERDICT"), ic_verdict, this, verdict_color(ic_verdict)),
        };
        results_layout_->addWidget(gs_card_row(ic_card, this));

        QList<QWidget*> rank_card = {
            gs_make_card(tr("RANK IC MEAN"), gs_fmt_num(d.value("rank_ic_mean").toDouble(), 4), this,
                         gs_pos_neg_color(d.value("rank_ic_mean").toDouble())),
            gs_make_card(tr("RANK IC σ"), gs_fmt_num(d.value("rank_ic_std").toDouble(), 4), this),
            gs_make_card(tr("RANK ICIR"), gs_fmt_num(rank_icir, 3), this,
                         rank_icir >= 0.5 ? ui::colors::POSITIVE()
                         : rank_icir >= 0 ? ui::colors::WARNING()
                                          : ui::colors::NEGATIVE()),
            gs_make_card(tr("RANK VERDICT"), rank_verdict, this, verdict_color(rank_verdict)),
        };
        results_layout_->addWidget(gs_card_row(rank_card, this));

        QList<QWidget*> hit = {
            gs_make_card(tr("IC POSITIVE %"), QString::number(d.value("ic_positive_pct").toDouble(), 'f', 1) + "%",
                         this,
                         d.value("ic_positive_pct").toDouble() > 60 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(
                tr("RANK IC POS %"), QString::number(d.value("rank_ic_positive_pct").toDouble(), 'f', 1) + "%", this,
                d.value("rank_ic_positive_pct").toDouble() > 60 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
            gs_make_card(tr("WINDOW SIZE"), tr("%1 obs").arg(d.value("window").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(hit, this));

        // Rolling IC: every window as a chart (x = the window's last observation; only the
        // methods that were computed are drawn), then the latest windows, newest first.
        const QJsonArray rolling = d.value("rolling_series").toArray();
        if (!rolling.isEmpty()) {
            const bool has_ic = rolling.first().toObject().contains("ic");
            const bool has_rank = rolling.first().toObject().contains("rank_ic");
            QcChartSpec spec;
            spec.title = tr("ROLLING IC  ·  %1-OBS WINDOW").arg(d.value("window").toInt());
            const XAxis x = qc_x_axis(rolling, {QStringLiteral("end_index")});
            spec.xs = x.xs;
            spec.fmt = qc_fmt_num(3);
            spec.x_label = [](double v) {
                return QCoreApplication::translate("QuantModulePanel", "window ending obs %1")
                    .arg(static_cast<qlonglong>(std::llround(v)));
            };
            if (has_ic) {
                QcLine l;
                l.name = tr("IC (Pearson)");
                l.ys = qc_column(rolling, QStringLiteral("ic"));
                l.color = ui::ChartColorRole::Accent;
                spec.lines.append(l);
            }
            if (has_rank) {
                QcLine l;
                l.name = tr("Rank IC (Spearman)");
                l.ys = qc_column(rolling, QStringLiteral("rank_ic"));
                l.color = ui::ChartColorRole::Info;
                spec.lines.append(l);
            }
            auto* chart = qc_series_chart(spec, this);
            chart->add_hline(0.0, QString(), ui::ChartColorRole::Muted);
            results_layout_->addWidget(chart);

            const QVector<int> latest = qc_latest_rows(rolling.size());
            const int rows = latest.size();
            results_layout_->addWidget(qc_caption(qc_latest_caption(rows, rolling.size())));
            auto* table = new QTableWidget(rows, 3, this);
            table->setHorizontalHeaderLabels({tr("End Index"), tr("IC"), tr("Rank IC")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * row_px(22) + row_px(32));
            for (int r = 0; r < rows; ++r) {
                const auto o = rolling[latest.at(r)].toObject();
                table->setItem(r, 0, new QTableWidgetItem(QString::number(o.value("end_index").toInt())));
                auto add_v = [&](int col, const char* key) {
                    if (!o.contains(QLatin1String(key))) {
                        table->setItem(r, col, new QTableWidgetItem(QString::fromUtf8("—")));
                        return;
                    }
                    const double v = o.value(QLatin1String(key)).toDouble();
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setForeground(QColor(gs_pos_neg_color(v)));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, col, it);
                };
                add_v(1, "ic");
                add_v(2, "rank_ic");
                table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("IC: pearson=%1 spearman=%2  ICIR=%3 → %4")
                                   .arg(pearson, 0, 'f', 3)
                                   .arg(spearman, 0, 'f', 3)
                                   .arg(icir, 0, 'f', 3)
                                   .arg(ic_verdict));
        return;
    }

    // ── 3. CUMULATIVE RETURNS ────────────────────────────────────────────────
    if (command == "cumulative_returns") {
        const double total_ret_pct = d.value("total_return_pct").toDouble();
        const double ann_ret_pct = d.value("annualized_return_pct").toDouble();
        const double ann_vol_pct = d.value("annualized_volatility_pct").toDouble();
        const double sharpe = d.value("sharpe_ratio").toDouble();
        const double mdd_pct = d.value("max_drawdown_pct").toDouble();
        const double win_pct = d.value("win_rate_pct").toDouble();
        const bool has_bench = d.value("has_benchmark").toBool();

        auto* hdr =
            new QLabel(tr("%1   |   %2 OBS").arg(d.value("title").toString()).arg(d.value("n_observations").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        // Sharpe and the information ratio are null when their denominator is zero (a constant
        // series, a portfolio identical to its benchmark): "—", not 0.
        const auto ratio_text = [](const QJsonValue& v) {
            return v.isDouble() ? gs_fmt_num(v.toDouble(), 3) : QString::fromUtf8("—");
        };
        const bool has_sharpe = d.value("sharpe_ratio").isDouble();
        QList<QWidget*> top = {
            gs_make_card(tr("TOTAL RETURN"), QString::number(total_ret_pct, 'f', 2) + "%", this,
                         gs_pos_neg_color(total_ret_pct)),
            gs_make_card(tr("ANN. RETURN"), QString::number(ann_ret_pct, 'f', 2) + "%", this,
                         gs_pos_neg_color(ann_ret_pct)),
            gs_make_card(tr("ANN. VOLATILITY"), QString::number(ann_vol_pct, 'f', 2) + "%", this),
            gs_make_card(tr("SHARPE RATIO"), ratio_text(d.value("sharpe_ratio")), this,
                         !has_sharpe     ? ui::colors::TEXT_SECONDARY()
                         : sharpe >= 1.0 ? ui::colors::POSITIVE()
                         : sharpe >= 0   ? ui::colors::TEXT_PRIMARY()
                                         : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> risk = {
            gs_make_card(tr("MAX DRAWDOWN"), QString::number(mdd_pct, 'f', 2) + "%", this,
                         mdd_pct <= -20.0 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card(tr("WIN RATE"), QString::number(win_pct, 'f', 1) + "%", this,
                         win_pct > 55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("BEST DAY"), QString::number(d.value("best_day").toDouble() * 100, 'f', 2) + "%", this,
                         ui::colors::POSITIVE()),
            gs_make_card(tr("WORST DAY"), QString::number(d.value("worst_day").toDouble() * 100, 'f', 2) + "%", this,
                         ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(risk, this));

        if (has_bench) {
            QList<QWidget*> bench = {
                gs_make_card(tr("BENCH TOTAL"),
                             QString::number(d.value("benchmark_total_return_pct").toDouble(), 'f', 2) + "%", this,
                             gs_pos_neg_color(d.value("benchmark_total_return_pct").toDouble())),
                gs_make_card(tr("ALPHA"), QString::number(d.value("alpha_pct").toDouble(), 'f', 2) + "%", this,
                             gs_pos_neg_color(d.value("alpha_pct").toDouble())),
                gs_make_card(tr("TRACKING ERROR"),
                             QString::number(d.value("tracking_error_pct").toDouble(), 'f', 2) + "%", this),
                gs_make_card(tr("INFORMATION RATIO"), ratio_text(d.value("information_ratio")), this,
                             !d.value("information_ratio").isDouble()      ? ui::colors::TEXT_SECONDARY()
                             : d.value("information_ratio").toDouble() > 0.5 ? ui::colors::POSITIVE()
                                                                             : ui::colors::WARNING()),
            };
            results_layout_->addWidget(gs_card_row(bench, this));
        }

        // Cumulative return of the whole curve (growth of 1 -> percent, exact per point), the
        // benchmark dashed beside it, then the latest points newest first. Both curves are
        // sampled by the script on the same observation index and always end on the last one.
        const QJsonArray curve = d.value("portfolio_curve").toArray();
        const QJsonArray bench_curve = d.value("benchmark_curve").toArray();
        if (!curve.isEmpty()) {
            const auto to_pct = [](const QJsonArray& rows) {
                QVector<double> out = qc_column(rows, QStringLiteral("value"));
                for (double& v : out)
                    v = std::isfinite(v) ? (v - 1.0) * 100.0 : v;
                return out;
            };
            QcChartSpec spec;
            spec.title = tr("CUMULATIVE RETURN");
            const XAxis x = qc_x_axis(curve, {QStringLiteral("index")});
            spec.xs = x.xs;
            spec.fmt = qc_fmt_pct(2);
            spec.x_label = [](double v) {
                return QStringLiteral("obs %1").arg(static_cast<qlonglong>(std::llround(v)));
            };
            QcLine pl;
            pl.name = tr("Portfolio");
            pl.ys = to_pct(curve);
            pl.color = ui::ChartColorRole::Accent;
            pl.width = 2.0;
            spec.lines.append(pl);
            if (has_bench && !bench_curve.isEmpty()) {
                QcLine bl;
                bl.name = tr("Benchmark");
                bl.xs = qc_x_axis(bench_curve, {QStringLiteral("index")}).xs;
                bl.ys = to_pct(bench_curve);
                bl.color = ui::ChartColorRole::Muted;
                bl.dashed = true;
                spec.lines.append(bl);
            }
            auto* chart = qc_series_chart(spec, this);
            chart->add_hline(0.0, QString(), ui::ChartColorRole::Muted);
            results_layout_->addWidget(chart);

            const QVector<int> latest = qc_latest_rows(curve.size());
            const int rows = latest.size();
            results_layout_->addWidget(qc_caption(qc_latest_caption(rows, curve.size())));
            auto* table = new QTableWidget(rows, has_bench ? 4 : 3, this);
            QStringList headers = {tr("Index"), tr("Portfolio"), tr("Cum Return")};
            if (has_bench)
                headers << tr("Benchmark");
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * row_px(22) + row_px(32));
            for (int r = 0; r < rows; ++r) {
                const int i = latest.at(r);
                const auto o = curve[i].toObject();
                const double v = o.value("value").toDouble();
                table->setItem(r, 0, new QTableWidgetItem(QString::number(o.value("index").toInt())));
                auto* vi = new QTableWidgetItem(QString::number(v, 'f', 4));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                auto* ri = new QTableWidgetItem(QString::number((v - 1.0) * 100, 'f', 2) + "%");
                ri->setForeground(QColor(gs_pos_neg_color(v - 1.0)));
                ri->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 2, ri);
                if (has_bench && i < bench_curve.size()) {
                    const auto bo = bench_curve[i].toObject();
                    auto* bi = new QTableWidgetItem(QString::number(bo.value("value").toDouble(), 'f', 4));
                    bi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, 3, bi);
                }
                table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("%1: total %2%  Sharpe %3  MaxDD %4%")
                                   .arg(d.value("title").toString())
                                   .arg(total_ret_pct, 0, 'f', 2)
                                   .arg(sharpe, 0, 'f', 2)
                                   .arg(mdd_pct, 0, 'f', 2));
        return;
    }

    // ── 4. RISK REPORT ───────────────────────────────────────────────────────
    if (command == "risk_report") {
        const double mdd_pct = d.value("max_drawdown_pct").toDouble();
        const bool recovered = d.value("max_drawdown_recovered").toBool();
        const int duration = d.value("drawdown_duration_days").toInt();
        const double cur_dd = d.value("current_drawdown_pct").toDouble();
        const double cur_vol = d.value("current_rolling_vol").toDouble();
        const double avg_vol = d.value("avg_rolling_vol").toDouble();

        auto* hdr = new QLabel(tr("ROLLING WINDOW: %1 DAYS").arg(d.value("rolling_window").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> dd = {
            gs_make_card(tr("MAX DRAWDOWN"), QString::number(mdd_pct, 'f', 2) + "%", this,
                         mdd_pct <= -20.0 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card(tr("CURRENT DD"), QString::number(cur_dd, 'f', 2) + "%", this,
                         cur_dd < -5.0 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("DURATION"), tr("%1 days").arg(duration), this),
            gs_make_card(tr("RECOVERED"), recovered ? tr("YES") : tr("NOT YET"), this,
                         recovered ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(dd, this));

        QList<QWidget*> vol = {
            gs_make_card(tr("CURRENT VOL"), QString::number(cur_vol * 100, 'f', 2) + "%", this,
                         cur_vol > avg_vol * 1.3 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("AVG ROLLING VOL"), QString::number(avg_vol * 100, 'f', 2) + "%", this),
            gs_make_card(tr("MAX ROLLING VOL"),
                         QString::number(d.value("max_rolling_vol").toDouble() * 100, 'f', 2) + "%", this,
                         ui::colors::WARNING()),
            gs_make_card(tr("MIN ROLLING VOL"),
                         QString::number(d.value("min_rolling_vol").toDouble() * 100, 'f', 2) + "%", this),
        };
        results_layout_->addWidget(gs_card_row(vol, this));

        QList<QWidget*> tail = {
            gs_make_card(tr("VaR 5%"), QString::number(d.value("var_5pct_daily").toDouble() * 100, 'f', 3) + "%", this,
                         ui::colors::WARNING()),
            gs_make_card(tr("CVaR 5%"), QString::number(d.value("cvar_5pct_daily").toDouble() * 100, 'f', 3) + "%",
                         this, ui::colors::NEGATIVE()),
            gs_make_card(tr("VaR 1%"), QString::number(d.value("var_1pct_daily").toDouble() * 100, 'f', 3) + "%", this,
                         ui::colors::WARNING()),
            gs_make_card(tr("CVaR 1%"), QString::number(d.value("cvar_1pct_daily").toDouble() * 100, 'f', 3) + "%",
                         this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(tail, this));

        // Drawdown timeline strip
        const int peak_i = d.value("max_drawdown_peak_index").toInt();
        const int trough_i = d.value("max_drawdown_trough_index").toInt();
        const int recov_i = d.value("max_drawdown_recovery_index").toInt();
        auto* timeline =
            new QLabel(tr("Drawdown timeline:  peak @ obs %1  →  trough @ obs %2 (Δ %3 days)  →  %4")
                           .arg(peak_i)
                           .arg(trough_i)
                           .arg(trough_i - peak_i)
                           .arg(recov_i >= 0 ? tr("recovered @ obs %1").arg(recov_i) : tr("not yet recovered")));
        timeline->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                        "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                    .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                         recovered ? ui::colors::POSITIVE() : ui::colors::WARNING()));
        results_layout_->addWidget(timeline);

        // The curves the script returns (sampled to ~300 points, x = observation index): the
        // drawdown over the whole series with the max-drawdown episode shaded, and under it the
        // rolling annualised volatility with its average.
        const QJsonArray dd_curve = d.value("drawdown_curve").toArray();
        const QJsonArray vol_curve = d.value("rolling_vol_curve").toArray();
        if (!dd_curve.isEmpty() || !vol_curve.isEmpty()) {
            // "label" is the observation index; "index" is the position in the sampled list (for
            // the rolling volatility it starts at the first full window, not at observation 0).
            const QStringList x_keys = {QStringLiteral("label"), QStringLiteral("index")};
            const auto to_pct = [](const QJsonArray& rows) {
                QVector<double> out = qc_column(rows, QStringLiteral("value"));
                for (double& v : out)
                    v = std::isfinite(v) ? v * 100.0 : v;
                return out;
            };
            QcChartSpec spec;
            spec.title = tr("DRAWDOWN  ·  ROLLING %1-DAY VOLATILITY").arg(d.value("rolling_window").toInt());
            spec.fmt = qc_fmt_pct(2);
            spec.x_label = [](double v) {
                return QCoreApplication::translate("QuantModulePanel", "obs %1")
                    .arg(static_cast<qlonglong>(std::llround(v)));
            };
            int vol_pane = 0;
            if (!dd_curve.isEmpty()) {
                QcLine dl;
                dl.name = tr("Drawdown");
                dl.xs = qc_x_axis(dd_curve, x_keys).xs;
                dl.ys = to_pct(dd_curve);
                dl.style = ui::TimeSeriesChart::Style::Area;
                dl.color = ui::ChartColorRole::Negative;
                dl.has_baseline = true;
                dl.baseline = 0.0;
                spec.lines.append(dl);
                vol_pane = 1;
            }
            if (!vol_curve.isEmpty()) {
                QcLine vl_line;
                vl_line.name = tr("Rolling volatility (annualised)");
                vl_line.xs = qc_x_axis(vol_curve, x_keys).xs;
                vl_line.ys = to_pct(vol_curve);
                vl_line.color = ui::ChartColorRole::Info;
                vl_line.pane = vol_pane;
                spec.lines.append(vl_line);
                if (vol_pane == 1) {
                    spec.pane_titles.append(tr("ROLLING VOLATILITY (ANNUALISED)"));
                    spec.pane_fmts.append(qc_fmt_pct(1));
                }
            }
            auto* chart = qc_series_chart(spec, this);
            // The episode the DURATION card measures: peak to recovery (or to the last observation).
            const int episode_end = recov_i >= 0 ? recov_i : d.value("n_observations").toInt() - 1;
            if (!dd_curve.isEmpty() && mdd_pct < 0.0 && episode_end > peak_i)
                chart->add_band(peak_i, episode_end, ui::ChartColorRole::Warning, tr("Max drawdown"));
            if (!vol_curve.isEmpty() && avg_vol > 0.0)
                chart->add_hline(avg_vol * 100.0, tr("Average"), ui::ChartColorRole::Muted, vol_pane);
            results_layout_->addWidget(chart);
        }

        status_label_->setText(tr("MaxDD %1%  Cur %2%  Vol %3%  Recovered: %4")
                                   .arg(mdd_pct, 0, 'f', 2)
                                   .arg(cur_dd, 0, 'f', 2)
                                   .arg(cur_vol * 100, 0, 'f', 2)
                                   .arg(recovered ? tr("YES") : tr("NO")));
        return;
    }

    // ── 5. MODEL PERFORMANCE ────────────────────────────────────────────────
    if (command == "model_performance") {
        const QString model = d.value("model_name").toString();
        const double pearson = d.value("pearson_ic").toDouble();
        const double spearman = d.value("spearman_ic").toDouble();
        const double dir_acc = d.value("direction_accuracy_pct").toDouble();
        const double hit_rate = d.value("hit_rate_pct").toDouble();
        const double ls_bps = d.value("long_short_spread_bps").toDouble();
        const bool monotonic = d.value("monotonic").toBool();
        const QString verdict = d.value("verdict").toString();

        auto* hdr = new QLabel(tr("MODEL: %1   |   %2 QUANTILES   |   %3")
                                   .arg(model.toUpper())
                                   .arg(d.value("n_quantiles").toInt())
                                   .arg(verdict));
        const bool good = verdict.contains("STRONG") || verdict.contains("MODERATE");
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    good ? ui::colors::POSITIVE() : ui::colors::WARNING()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> ic = {
            gs_make_card(tr("PEARSON IC"), gs_fmt_num(pearson, 4), this, gs_pos_neg_color(pearson)),
            gs_make_card(tr("SPEARMAN IC"), gs_fmt_num(spearman, 4), this, gs_pos_neg_color(spearman)),
            gs_make_card(tr("PEARSON p"), gs_fmt_num(d.value("pearson_p_value").toDouble(), 4), this),
            gs_make_card(tr("SPEARMAN p"), gs_fmt_num(d.value("spearman_p_value").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(ic, this));

        QList<QWidget*> hit = {
            gs_make_card(tr("DIR. ACCURACY"), QString::number(dir_acc, 'f', 1) + "%", this,
                         dir_acc > 55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("HIT RATE"), QString::number(hit_rate, 'f', 1) + "%", this,
                         hit_rate > 55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("LONG-SHORT SPREAD"), QString::number(ls_bps, 'f', 1) + tr(" bps"), this,
                         gs_pos_neg_color(ls_bps)),
            gs_make_card(tr("MONOTONIC"), monotonic ? tr("YES") : tr("NO"), this,
                         monotonic ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(hit, this));

        // Quantile table, with the mean return per prediction quantile as bars above it —
        // a monotone staircase is what a useful model looks like.
        const QJsonArray quants = d.value("quantile_table").toArray();
        if (!quants.isEmpty()) {
            QStringList cats;
            QVector<double> means;
            for (const auto& v : quants) {
                const auto q = v.toObject();
                cats << QStringLiteral("Q%1").arg(q.value("quantile").toInt());
                means << q.value("mean_return").toDouble() * 100.0;
            }
            results_layout_->addWidget(qc_caption(tr("MEAN RETURN BY PREDICTION QUANTILE")));
            auto* bars = new ui::BarChart(this);
            bars->set_data(cats, means, tr("Mean return"));
            bars->set_signed_colors(true);
            bars->set_show_values(true);
            bars->set_value_formatter(qc_fmt_pct(3));
            bars->setFixedHeight(row_px(200));
            results_layout_->addWidget(bars);

            auto* table = new QTableWidget(quants.size(), 6, this);
            table->setHorizontalHeaderLabels(
                {tr("Q"), tr("N"), tr("Pred Range"), tr("Mean Return"), tr("Median Return"), tr("Win Rate")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(quants.size() * row_px(24) + row_px(32), 280));
            for (int i = 0; i < quants.size(); ++i) {
                const auto q = quants[i].toObject();
                const double mr = q.value("mean_return").toDouble();
                const int qn = q.value("quantile").toInt();
                const int n = q.value("n").toInt();
                table->setItem(i, 0, num_item(QString::number(qn), qn));
                table->setItem(i, 1, num_item(QLocale().toString(n), n));
                table->setItem(i, 2,
                               num_item(QString("[%1, %2]")
                                            .arg(q.value("pred_min").toDouble(), 0, 'f', 4)
                                            .arg(q.value("pred_max").toDouble(), 0, 'f', 4),
                                        q.value("pred_min").toDouble()));
                table->setItem(i, 3, num_item(QString::number(mr * 100, 'f', 4) + "%", mr, gs_pos_neg_color(mr)));
                const double med = q.value("median_return").toDouble();
                table->setItem(i, 4, num_item(QString::number(med * 100, 'f', 4) + "%", med));
                const double win = q.value("win_rate_pct").toDouble();
                table->setItem(i, 5, num_item(QString::number(win, 'f', 1) + "%", win));
                table->setRowHeight(i, row_px(24));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("%1: IC=%2  Spread=%3 bps  → %4")
                                   .arg(model)
                                   .arg(spearman, 0, 'f', 3)
                                   .arg(ls_bps, 0, 'f', 1)
                                   .arg(verdict));
        return;
    }

    // ── 6. FACTOR QUANTILES ─────────────────────────────────────────────────
    if (command == "factor_quantiles") {
        const double spread_bps = d.value("long_short_spread_bps").toDouble();
        const double ls_sharpe = d.value("long_short_sharpe_annualized").toDouble();
        const bool monotone = d.value("monotonic").toBool();
        const QString verdict = d.value("verdict").toString();
        const int n_q = d.value("n_quantiles").toInt();

        auto* hdr = new QLabel(tr("%1 QUANTILES   |   %2").arg(n_q).arg(verdict));
        const bool good = verdict.contains("STRONG");
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    good ? ui::colors::POSITIVE() : ui::colors::WARNING()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card(tr("L/S SPREAD"), QString::number(spread_bps, 'f', 1) + tr(" bps"), this,
                         gs_pos_neg_color(spread_bps)),
            gs_make_card(tr("L/S SHARPE (ANN)"), gs_fmt_num(ls_sharpe, 3), this,
                         ls_sharpe >= 1.0 ? ui::colors::POSITIVE()
                         : ls_sharpe > 0  ? ui::colors::WARNING()
                                          : ui::colors::NEGATIVE()),
            gs_make_card(tr("MONOTONIC"), monotone ? tr("YES") : tr("NO"), this,
                         monotone ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Mean forward return per factor quantile as bars (low → high), then the detail table.
        const QJsonArray quants = d.value("quantiles").toArray();
        if (!quants.isEmpty()) {
            QStringList cats;
            QVector<double> means;
            for (const auto& v : quants) {
                const auto q = v.toObject();
                cats << QStringLiteral("Q%1").arg(q.value("quantile").toInt());
                means << q.value("ret_mean").toDouble() * 100.0;
            }
            results_layout_->addWidget(qc_caption(tr("MEAN RETURN BY FACTOR QUANTILE  (Q1 = LOW)")));
            auto* bars = new ui::BarChart(this);
            bars->set_data(cats, means, tr("Mean return"));
            bars->set_signed_colors(true);
            bars->set_show_values(true);
            bars->set_value_formatter(qc_fmt_pct(3));
            bars->setFixedHeight(row_px(200));
            results_layout_->addWidget(bars);

            auto* table = new QTableWidget(quants.size(), 7, this);
            table->setHorizontalHeaderLabels(
                {tr("Q"), tr("N"), tr("Pred Mean"), tr("Ret Mean"), tr("Ret Median"), tr("Ret σ"), tr("Win Rate")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(quants.size() * row_px(26) + row_px(32), 320));
            for (int i = 0; i < quants.size(); ++i) {
                const auto q = quants[i].toObject();
                const int qid = q.value("quantile").toInt();
                const double rm = q.value("ret_mean").toDouble();
                QString tag = QString::number(qid);
                if (qid == 1)
                    tag += tr(" (low)");
                if (qid == n_q)
                    tag += tr(" (high)");
                auto* tag_it = num_item(tag, qid);
                tag_it->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                table->setItem(i, 0, tag_it);
                const int n = q.value("n").toInt();
                table->setItem(i, 1, num_item(QLocale().toString(n), n));
                auto add_n = [&](int col, double v, int dec, bool color = false) {
                    table->setItem(i, col, num_item(QString::number(v, 'f', dec), v, color ? gs_pos_neg_color(v) : QString()));
                };
                add_n(2, q.value("pred_mean").toDouble(), 4);
                table->setItem(i, 3, num_item(QString::number(rm * 100, 'f', 4) + "%", rm, gs_pos_neg_color(rm)));
                const double rmed = q.value("ret_median").toDouble();
                table->setItem(i, 4, num_item(QString::number(rmed * 100, 'f', 4) + "%", rmed));
                add_n(5, q.value("ret_std").toDouble() * 100, 4);
                const double win = q.value("win_rate_pct").toDouble();
                table->setItem(i, 6, num_item(QString::number(win, 'f', 1) + "%", win));
                table->setRowHeight(i, row_px(26));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }

        status_label_->setText(
            tr("Spread %1 bps  Sharpe %2  → %3").arg(spread_bps, 0, 'f', 1).arg(ls_sharpe, 0, 'f', 2).arg(verdict));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
