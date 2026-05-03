// src/screens/ai_quant_lab/QuantModulePanel_QuantReporting.cpp
//
// Quant Reporting panel — 5 report builders (IC analysis, cumulative returns,
// risk report, model performance, factor quantiles). Mirrors GS Quant /
// Functime / Statsmodels / Fortitudo pattern. Extracted from QuantModulePanel.cpp
// to keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
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

// ═══════════════════════════════════════════════════════════════════════════════
// QUANT REPORTING PANEL — 5 report builders (IC analysis, cumulative returns,
// risk report, model performance, factor quantiles). Mirrors GS Quant /
// Functime / Statsmodels / Fortitudo pattern.
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_quant_reporting_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // Local LOAD SAMPLE helper — same shape as the other panels.
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed,
                                  const QString& tip) -> QPushButton* {
        auto* btn = new QPushButton("LOAD SAMPLE", parent);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(22);
        btn->setToolTip(tip);
        btn->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2;"
                                   "padding:0 8px; font-size:9px; font-weight:700; letter-spacing:0.5px; border-radius:2px; }"
                                   "QPushButton:hover { color:%3; border-color:%3; background:rgba(255,255,255,0.04); }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), module_.color.name()));
        QPointer<QLineEdit> guard(edit);
        connect(btn, &QPushButton::clicked, this, [guard, seed]() {
            if (guard) guard->setText(sample_returns(seed));
        });
        return btn;
    };

    // ── IC Analysis ──────────────────────────────────────────────────────────
    auto* ic_tab = new QWidget(this);
    auto* icvl = new QVBoxLayout(ic_tab);
    icvl->setContentsMargins(12, 12, 12, 12);
    icvl->setSpacing(8);

    auto* ic_preds = new QLineEdit(ic_tab);
    ic_preds->setPlaceholderText("Model predictions (decimals, >= 10 values)");
    ic_preds->setStyleSheet(input_ss());
    text_inputs_["rp_ic_preds"] = ic_preds;
    icvl->addWidget(build_input_row("Predictions", ic_preds, ic_tab));
    icvl->addWidget(add_sample_btn(ic_preds, ic_tab, 211,
                                    "252 synthetic predictions (mu=0.05%, sigma=1.2%)"));

    auto* ic_rets = new QLineEdit(ic_tab);
    ic_rets->setPlaceholderText("Realized returns (decimals, same length as predictions)");
    ic_rets->setStyleSheet(input_ss());
    text_inputs_["rp_ic_rets"] = ic_rets;
    icvl->addWidget(build_input_row("Returns", ic_rets, ic_tab));
    icvl->addWidget(add_sample_btn(ic_rets, ic_tab, 212,
                                    "252 synthetic realized returns"));

    auto* ic_method = new QComboBox(ic_tab);
    ic_method->addItems({"both", "pearson", "spearman"});
    ic_method->setStyleSheet(combo_ss());
    combo_inputs_["rp_ic_method"] = ic_method;
    icvl->addWidget(build_input_row("IC Method", ic_method, ic_tab));

    auto* ic_window = new QSpinBox(ic_tab);
    ic_window->setRange(5, 100);
    ic_window->setValue(20);
    ic_window->setStyleSheet(spinbox_ss());
    int_inputs_["rp_ic_window"] = ic_window;
    icvl->addWidget(build_input_row("Rolling Window", ic_window, ic_tab));

    auto* ic_run = make_run_button("RUN IC ANALYSIS", ic_tab);
    connect(ic_run, &QPushButton::clicked, this, [this]() {
        QJsonArray preds, rets;
        QString bad;
        if (!parse_doubles(text_inputs_["rp_ic_preds"]->text(), preds, &bad)) {
            display_error(QString("Predictions: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["rp_ic_rets"]->text(), rets, &bad)) {
            display_error(QString("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (preds.size() < 10) {
            display_error(QString("IC analysis needs at least 10 predictions; you provided %1.").arg(preds.size()));
            return;
        }
        if (preds.size() != rets.size()) {
            display_error(QString("Predictions (%1) and returns (%2) must have the same length.")
                              .arg(preds.size()).arg(rets.size()));
            return;
        }
        show_loading(QString("Computing IC + rolling IC on %1 observations...").arg(preds.size()));
        QJsonObject params;
        params["predictions"] = preds;
        params["returns"] = rets;
        params["method"] = combo_inputs_["rp_ic_method"]->currentText();
        params["window"] = int_inputs_["rp_ic_window"]->value();
        AIQuantLabService::instance().reporting_ic_analysis(params);
    });
    icvl->addWidget(ic_run);
    icvl->addStretch();
    tabs->addTab(ic_tab, "IC Analysis");

    // ── Cumulative Returns ───────────────────────────────────────────────────
    auto* cr_tab = new QWidget(this);
    auto* crvl = new QVBoxLayout(cr_tab);
    crvl->setContentsMargins(12, 12, 12, 12);
    crvl->setSpacing(8);

    auto* cr_rets = new QLineEdit(cr_tab);
    cr_rets->setPlaceholderText("Portfolio daily returns (decimals)");
    cr_rets->setStyleSheet(input_ss());
    text_inputs_["rp_cr_rets"] = cr_rets;
    crvl->addWidget(build_input_row("Returns", cr_rets, cr_tab));
    crvl->addWidget(add_sample_btn(cr_rets, cr_tab, 221,
                                    "252 synthetic portfolio returns"));

    auto* cr_bench = new QLineEdit(cr_tab);
    cr_bench->setPlaceholderText("Benchmark returns (optional; same length as portfolio if provided)");
    cr_bench->setStyleSheet(input_ss());
    text_inputs_["rp_cr_bench"] = cr_bench;
    crvl->addWidget(build_input_row("Benchmark Returns (optional)", cr_bench, cr_tab));
    crvl->addWidget(add_sample_btn(cr_bench, cr_tab, 222,
                                    "252 synthetic benchmark returns"));

    auto* cr_title = new QLineEdit(cr_tab);
    cr_title->setPlaceholderText("Chart title (e.g. Strategy vs S&P 500)");
    cr_title->setText("Cumulative Returns");
    cr_title->setStyleSheet(input_ss());
    text_inputs_["rp_cr_title"] = cr_title;
    crvl->addWidget(build_input_row("Title", cr_title, cr_tab));

    auto* cr_run = make_run_button("BUILD CUMULATIVE RETURNS REPORT", cr_tab);
    connect(cr_run, &QPushButton::clicked, this, [this]() {
        QJsonArray rets;
        QString bad;
        if (!parse_doubles(text_inputs_["rp_cr_rets"]->text(), rets, &bad)) {
            display_error(QString("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (rets.size() < 5) {
            display_error(QString("Need at least 5 returns; you provided %1.").arg(rets.size()));
            return;
        }
        QJsonObject params;
        params["returns"] = rets;
        const QString bench_text = text_inputs_["rp_cr_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            if (!parse_doubles(bench_text, bench, &bad)) {
                display_error(QString("Benchmark: '%1' is not numeric.").arg(bad));
                return;
            }
            if (bench.size() != rets.size()) {
                display_error(QString("Benchmark (%1) and returns (%2) must have the same length.")
                                  .arg(bench.size()).arg(rets.size()));
                return;
            }
            params["benchmark_returns"] = bench;
        }
        const QString title = text_inputs_["rp_cr_title"]->text().trimmed();
        params["title"] = title.isEmpty() ? QString("Cumulative Returns") : title;
        show_loading(QString("Building cumulative returns report on %1 obs...").arg(rets.size()));
        AIQuantLabService::instance().reporting_cumulative_returns(params);
    });
    crvl->addWidget(cr_run);
    crvl->addStretch();
    tabs->addTab(cr_tab, "Cumulative Returns");

    // ── Risk Report ──────────────────────────────────────────────────────────
    auto* rk_tab = new QWidget(this);
    auto* rkvl = new QVBoxLayout(rk_tab);
    rkvl->setContentsMargins(12, 12, 12, 12);
    rkvl->setSpacing(8);

    auto* rk_rets = new QLineEdit(rk_tab);
    rk_rets->setPlaceholderText("Daily returns (>= 30 values)");
    rk_rets->setStyleSheet(input_ss());
    text_inputs_["rp_rk_rets"] = rk_rets;
    rkvl->addWidget(build_input_row("Returns", rk_rets, rk_tab));
    rkvl->addWidget(add_sample_btn(rk_rets, rk_tab, 231,
                                    "252 synthetic daily returns"));

    auto* rk_window = new QSpinBox(rk_tab);
    rk_window->setRange(5, 100);
    rk_window->setValue(20);
    rk_window->setStyleSheet(spinbox_ss());
    int_inputs_["rp_rk_window"] = rk_window;
    rkvl->addWidget(build_input_row("Rolling Volatility Window (days)", rk_window, rk_tab));

    auto* rk_hint = new QLabel(
        "Computes max drawdown with peak/trough/recovery markers, daily VaR + CVaR at 5% and 1%, "
        "and rolling annualized volatility.", rk_tab);
    rk_hint->setWordWrap(true);
    rk_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    rkvl->addWidget(rk_hint);

    auto* rk_run = make_run_button("BUILD RISK REPORT", rk_tab);
    connect(rk_run, &QPushButton::clicked, this, [this]() {
        QJsonArray rets;
        QString bad;
        if (!parse_doubles(text_inputs_["rp_rk_rets"]->text(), rets, &bad)) {
            display_error(QString("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (rets.size() < 30) {
            display_error(QString("Risk report needs at least 30 obs; you provided %1.").arg(rets.size()));
            return;
        }
        QJsonObject params;
        params["returns"] = rets;
        params["rolling_window"] = int_inputs_["rp_rk_window"]->value();
        show_loading(QString("Computing drawdown + rolling vol on %1 obs...").arg(rets.size()));
        AIQuantLabService::instance().reporting_risk_report(params);
    });
    rkvl->addWidget(rk_run);
    rkvl->addStretch();
    tabs->addTab(rk_tab, "Risk Report");

    // ── Model Performance ────────────────────────────────────────────────────
    auto* mp_tab = new QWidget(this);
    auto* mpvl = new QVBoxLayout(mp_tab);
    mpvl->setContentsMargins(12, 12, 12, 12);
    mpvl->setSpacing(8);

    auto* mp_preds = new QLineEdit(mp_tab);
    mp_preds->setPlaceholderText("Model predictions (decimals, >= 20 values)");
    mp_preds->setStyleSheet(input_ss());
    text_inputs_["rp_mp_preds"] = mp_preds;
    mpvl->addWidget(build_input_row("Predictions", mp_preds, mp_tab));
    mpvl->addWidget(add_sample_btn(mp_preds, mp_tab, 241,
                                    "252 synthetic model predictions"));

    auto* mp_rets = new QLineEdit(mp_tab);
    mp_rets->setPlaceholderText("Realized returns (same length as predictions)");
    mp_rets->setStyleSheet(input_ss());
    text_inputs_["rp_mp_rets"] = mp_rets;
    mpvl->addWidget(build_input_row("Returns", mp_rets, mp_tab));
    mpvl->addWidget(add_sample_btn(mp_rets, mp_tab, 242,
                                    "252 synthetic realized returns"));

    auto* mp_name = new QLineEdit(mp_tab);
    mp_name->setPlaceholderText("Model label (e.g. LightGBM, LSTM)");
    mp_name->setText("Model");
    mp_name->setStyleSheet(input_ss());
    text_inputs_["rp_mp_name"] = mp_name;
    mpvl->addWidget(build_input_row("Model Name", mp_name, mp_tab));

    auto* mp_q = new QSpinBox(mp_tab);
    mp_q->setRange(2, 10);
    mp_q->setValue(5);
    mp_q->setStyleSheet(spinbox_ss());
    int_inputs_["rp_mp_n_quantiles"] = mp_q;
    mpvl->addWidget(build_input_row("Quantile Buckets", mp_q, mp_tab));

    auto* mp_run = make_run_button("EVALUATE MODEL", mp_tab);
    connect(mp_run, &QPushButton::clicked, this, [this]() {
        QJsonArray preds, rets;
        QString bad;
        if (!parse_doubles(text_inputs_["rp_mp_preds"]->text(), preds, &bad)) {
            display_error(QString("Predictions: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["rp_mp_rets"]->text(), rets, &bad)) {
            display_error(QString("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (preds.size() < 20) {
            display_error(QString("Model evaluation needs at least 20 obs; you provided %1.").arg(preds.size()));
            return;
        }
        if (preds.size() != rets.size()) {
            display_error(QString("Predictions (%1) and returns (%2) must have the same length.")
                              .arg(preds.size()).arg(rets.size()));
            return;
        }
        QJsonObject params;
        params["predictions"] = preds;
        params["returns"] = rets;
        const QString name = text_inputs_["rp_mp_name"]->text().trimmed();
        params["model_name"] = name.isEmpty() ? QString("Model") : name;
        params["n_quantiles"] = int_inputs_["rp_mp_n_quantiles"]->value();
        show_loading(QString("Evaluating %1 on %2 obs...").arg(name.isEmpty() ? "model" : name).arg(preds.size()));
        AIQuantLabService::instance().reporting_model_performance(params);
    });
    mpvl->addWidget(mp_run);
    mpvl->addStretch();
    tabs->addTab(mp_tab, "Model Performance");

    // ── Factor Quantiles ─────────────────────────────────────────────────────
    auto* fq_tab = new QWidget(this);
    auto* fqvl = new QVBoxLayout(fq_tab);
    fqvl->setContentsMargins(12, 12, 12, 12);
    fqvl->setSpacing(8);

    auto* fq_preds = new QLineEdit(fq_tab);
    fq_preds->setPlaceholderText("Factor / signal values (decimals, >= 20)");
    fq_preds->setStyleSheet(input_ss());
    text_inputs_["rp_fq_preds"] = fq_preds;
    fqvl->addWidget(build_input_row("Factor Signal", fq_preds, fq_tab));
    fqvl->addWidget(add_sample_btn(fq_preds, fq_tab, 251,
                                    "252 synthetic factor values"));

    auto* fq_rets = new QLineEdit(fq_tab);
    fq_rets->setPlaceholderText("Realized returns (same length as factor)");
    fq_rets->setStyleSheet(input_ss());
    text_inputs_["rp_fq_rets"] = fq_rets;
    fqvl->addWidget(build_input_row("Realized Returns", fq_rets, fq_tab));
    fqvl->addWidget(add_sample_btn(fq_rets, fq_tab, 252,
                                    "252 synthetic realized returns"));

    auto* fq_q = new QSpinBox(fq_tab);
    fq_q->setRange(2, 10);
    fq_q->setValue(5);
    fq_q->setStyleSheet(spinbox_ss());
    int_inputs_["rp_fq_n_quantiles"] = fq_q;
    fqvl->addWidget(build_input_row("Quantile Buckets", fq_q, fq_tab));

    auto* fq_hint = new QLabel(
        "Sorts observations into N buckets by signal strength and reports realized return per bucket. "
        "A monotone Q1→Q5 progression indicates a clean factor; non-monotone hints at noise.", fq_tab);
    fq_hint->setWordWrap(true);
    fq_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    fqvl->addWidget(fq_hint);

    auto* fq_run = make_run_button("ANALYZE FACTOR QUANTILES", fq_tab);
    connect(fq_run, &QPushButton::clicked, this, [this]() {
        QJsonArray preds, rets;
        QString bad;
        if (!parse_doubles(text_inputs_["rp_fq_preds"]->text(), preds, &bad)) {
            display_error(QString("Factor: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["rp_fq_rets"]->text(), rets, &bad)) {
            display_error(QString("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (preds.size() < 20) {
            display_error(QString("Quantile analysis needs at least 20 obs; you provided %1.").arg(preds.size()));
            return;
        }
        if (preds.size() != rets.size()) {
            display_error(QString("Factor (%1) and returns (%2) must have the same length.")
                              .arg(preds.size()).arg(rets.size()));
            return;
        }
        QJsonObject params;
        params["predictions"] = preds;
        params["returns"] = rets;
        params["n_quantiles"] = int_inputs_["rp_fq_n_quantiles"]->value();
        show_loading(QString("Bucketing %1 obs into %2 quantiles...")
                         .arg(preds.size()).arg(int_inputs_["rp_fq_n_quantiles"]->value()));
        AIQuantLabService::instance().reporting_factor_quantiles(params);
    });
    fqvl->addWidget(fq_run);
    fqvl->addStretch();
    tabs->addTab(fq_tab, "Factor Quantiles");

    vl->addWidget(tabs);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ── QUANT REPORTING: rich card-based display per command ────────────────────

void QuantModulePanel::display_quant_reporting_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    if (!payload.value("success").toBool(false)) {
        const QString err = payload.value("error").toString("Unknown error");
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? "Input error: "
                              : kind == "runtime"    ? "Computation failed: "
                                                     : "";
        display_error(prefix + err);
        return;
    }

    const QJsonObject d = payload.value("data").toObject();
    const QString accent = module_.color.name();

    // Section header
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += QString("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 1. CHECK STATUS ──────────────────────────────────────────────────────
    if (command == "check_status") {
        auto bool_card = [this](const QString& label, bool ok) {
            return gs_make_card(label, ok ? "AVAILABLE" : "MISSING", this,
                                ok ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        };
        QList<QWidget*> deps = {
            bool_card("SCIPY", d.value("scipy").toBool()),
            bool_card("PANDAS", d.value("pandas").toBool()),
            gs_make_card("BACKEND", d.value("backend").toString().toUpper(), this),
            gs_make_card("OPS", QString::number(d.value("ops_available").toArray().size()),
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(deps, this));
        status_label_->setText("Quant Reporting backend ready");
        return;
    }

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

        auto* hdr = new QLabel(QString("METHOD: %1   |   ROLLING WINDOW: %2   |   %3 ROLLING POINTS")
                                   .arg(d.value("method").toString().toUpper())
                                   .arg(d.value("window").toInt())
                                   .arg(d.value("n_rolling_points").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        auto verdict_color = [](const QString& v) {
            if (v == "STRONG") return ui::colors::POSITIVE();
            if (v == "MODERATE") return ui::colors::POSITIVE();
            if (v == "WEAK") return ui::colors::WARNING();
            if (v == "VERY WEAK") return ui::colors::WARNING();
            return ui::colors::NEGATIVE();
        };

        QList<QWidget*> overall = {
            gs_make_card("PEARSON IC", gs_fmt_num(pearson, 4), this, gs_pos_neg_color(pearson)),
            gs_make_card("PEARSON p", gs_fmt_num(pearson_p, 4), this,
                         pearson_p < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("SPEARMAN IC", gs_fmt_num(spearman, 4), this, gs_pos_neg_color(spearman)),
            gs_make_card("SPEARMAN p", gs_fmt_num(spearman_p, 4), this,
                         spearman_p < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(overall, this));

        QList<QWidget*> ic_card = {
            gs_make_card("ROLLING IC MEAN", gs_fmt_num(d.value("ic_mean").toDouble(), 4), this,
                         gs_pos_neg_color(d.value("ic_mean").toDouble())),
            gs_make_card("ROLLING IC σ", gs_fmt_num(d.value("ic_std").toDouble(), 4), this),
            gs_make_card("ICIR", gs_fmt_num(icir, 3), this,
                         icir >= 0.5 ? ui::colors::POSITIVE()
                                    : icir >= 0  ? ui::colors::WARNING()
                                                 : ui::colors::NEGATIVE()),
            gs_make_card("IC VERDICT", ic_verdict, this, verdict_color(ic_verdict)),
        };
        results_layout_->addWidget(gs_card_row(ic_card, this));

        QList<QWidget*> rank_card = {
            gs_make_card("RANK IC MEAN", gs_fmt_num(d.value("rank_ic_mean").toDouble(), 4), this,
                         gs_pos_neg_color(d.value("rank_ic_mean").toDouble())),
            gs_make_card("RANK IC σ", gs_fmt_num(d.value("rank_ic_std").toDouble(), 4), this),
            gs_make_card("RANK ICIR", gs_fmt_num(rank_icir, 3), this,
                         rank_icir >= 0.5 ? ui::colors::POSITIVE()
                                          : rank_icir >= 0  ? ui::colors::WARNING()
                                                            : ui::colors::NEGATIVE()),
            gs_make_card("RANK VERDICT", rank_verdict, this, verdict_color(rank_verdict)),
        };
        results_layout_->addWidget(gs_card_row(rank_card, this));

        QList<QWidget*> hit = {
            gs_make_card("IC POSITIVE %", QString::number(d.value("ic_positive_pct").toDouble(), 'f', 1) + "%", this,
                         d.value("ic_positive_pct").toDouble() > 60 ? ui::colors::POSITIVE()
                                                                       : ui::colors::WARNING()),
            gs_make_card("RANK IC POS %",
                         QString::number(d.value("rank_ic_positive_pct").toDouble(), 'f', 1) + "%", this,
                         d.value("rank_ic_positive_pct").toDouble() > 60 ? ui::colors::POSITIVE()
                                                                           : ui::colors::WARNING()),
            gs_make_card("OBSERVATIONS", QString::number(d.value("n_observations").toInt()), this),
            gs_make_card("WINDOW SIZE", QString::number(d.value("window").toInt()) + " obs", this),
        };
        results_layout_->addWidget(gs_card_row(hit, this));

        // Rolling IC table (sample first/middle/last 15 rows)
        const QJsonArray rolling = d.value("rolling_series").toArray();
        if (!rolling.isEmpty()) {
            const int rows = std::min<int>(15, rolling.size());
            const int step = std::max<int>(1, rolling.size() / rows);
            auto* table = new QTableWidget(rows, 3, this);
            table->setHorizontalHeaderLabels({"End Index", "IC", "Rank IC"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < rolling.size() && r < rows; i += step, ++r) {
                const auto o = rolling[i].toObject();
                table->setItem(r, 0, new QTableWidgetItem(QString::number(o.value("end_index").toInt())));
                auto add_v = [&](int col, double v) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setForeground(QColor(gs_pos_neg_color(v)));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, col, it);
                };
                add_v(1, o.value("ic").toDouble());
                add_v(2, o.value("rank_ic").toDouble());
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("IC: pearson=%1 spearman=%2  ICIR=%3 → %4")
                                   .arg(pearson, 0, 'f', 3).arg(spearman, 0, 'f', 3)
                                   .arg(icir, 0, 'f', 3).arg(ic_verdict));
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

        auto* hdr = new QLabel(QString("%1   |   %2 OBS").arg(d.value("title").toString())
                                   .arg(d.value("n_observations").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("TOTAL RETURN", QString::number(total_ret_pct, 'f', 2) + "%", this,
                         gs_pos_neg_color(total_ret_pct)),
            gs_make_card("ANN. RETURN", QString::number(ann_ret_pct, 'f', 2) + "%", this,
                         gs_pos_neg_color(ann_ret_pct)),
            gs_make_card("ANN. VOLATILITY", QString::number(ann_vol_pct, 'f', 2) + "%", this),
            gs_make_card("SHARPE RATIO", gs_fmt_num(sharpe, 3), this,
                         sharpe >= 1.0 ? ui::colors::POSITIVE()
                                      : sharpe >= 0  ? ui::colors::TEXT_PRIMARY()
                                                     : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> risk = {
            gs_make_card("MAX DRAWDOWN", QString::number(mdd_pct, 'f', 2) + "%", this,
                         mdd_pct <= -20.0 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card("WIN RATE", QString::number(win_pct, 'f', 1) + "%", this,
                         win_pct > 55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("BEST DAY", QString::number(d.value("best_day").toDouble() * 100, 'f', 2) + "%", this,
                         ui::colors::POSITIVE()),
            gs_make_card("WORST DAY", QString::number(d.value("worst_day").toDouble() * 100, 'f', 2) + "%", this,
                         ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(risk, this));

        if (has_bench) {
            QList<QWidget*> bench = {
                gs_make_card("BENCH TOTAL",
                             QString::number(d.value("benchmark_total_return_pct").toDouble(), 'f', 2) + "%",
                             this, gs_pos_neg_color(d.value("benchmark_total_return_pct").toDouble())),
                gs_make_card("ALPHA", QString::number(d.value("alpha_pct").toDouble(), 'f', 2) + "%", this,
                             gs_pos_neg_color(d.value("alpha_pct").toDouble())),
                gs_make_card("TRACKING ERROR",
                             QString::number(d.value("tracking_error_pct").toDouble(), 'f', 2) + "%", this),
                gs_make_card("INFORMATION RATIO",
                             gs_fmt_num(d.value("information_ratio").toDouble(), 3), this,
                             d.value("information_ratio").toDouble() > 0.5 ? ui::colors::POSITIVE()
                                                                              : ui::colors::WARNING()),
            };
            results_layout_->addWidget(gs_card_row(bench, this));
        }

        // Equity curve preview — table sampled to ~12 rows
        const QJsonArray curve = d.value("portfolio_curve").toArray();
        const QJsonArray bench_curve = d.value("benchmark_curve").toArray();
        if (!curve.isEmpty()) {
            const int rows = std::min<int>(12, curve.size());
            const int step = std::max<int>(1, curve.size() / rows);
            auto* table = new QTableWidget(rows, has_bench ? 4 : 3, this);
            QStringList headers = {"Index", "Portfolio", "Cum Return"};
            if (has_bench) headers << "Benchmark";
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < curve.size() && r < rows; i += step, ++r) {
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
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("%1: total %2%  Sharpe %3  MaxDD %4%")
                                   .arg(d.value("title").toString())
                                   .arg(total_ret_pct, 0, 'f', 2).arg(sharpe, 0, 'f', 2)
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

        auto* hdr = new QLabel(QString("ROLLING WINDOW: %1 DAYS").arg(d.value("rolling_window").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> dd = {
            gs_make_card("MAX DRAWDOWN", QString::number(mdd_pct, 'f', 2) + "%", this,
                         mdd_pct <= -20.0 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card("CURRENT DD", QString::number(cur_dd, 'f', 2) + "%", this,
                         cur_dd < -5.0 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card("DURATION", QString("%1 days").arg(duration), this),
            gs_make_card("RECOVERED",
                         recovered ? "YES" : "NOT YET",
                         this, recovered ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(dd, this));

        QList<QWidget*> vol = {
            gs_make_card("CURRENT VOL", QString::number(cur_vol * 100, 'f', 2) + "%", this,
                         cur_vol > avg_vol * 1.3 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card("AVG ROLLING VOL", QString::number(avg_vol * 100, 'f', 2) + "%", this),
            gs_make_card("MAX ROLLING VOL",
                         QString::number(d.value("max_rolling_vol").toDouble() * 100, 'f', 2) + "%", this,
                         ui::colors::WARNING()),
            gs_make_card("MIN ROLLING VOL",
                         QString::number(d.value("min_rolling_vol").toDouble() * 100, 'f', 2) + "%", this),
        };
        results_layout_->addWidget(gs_card_row(vol, this));

        QList<QWidget*> tail = {
            gs_make_card("VaR 5%",
                         QString::number(d.value("var_5pct_daily").toDouble() * 100, 'f', 3) + "%", this,
                         ui::colors::WARNING()),
            gs_make_card("CVaR 5%",
                         QString::number(d.value("cvar_5pct_daily").toDouble() * 100, 'f', 3) + "%", this,
                         ui::colors::NEGATIVE()),
            gs_make_card("VaR 1%",
                         QString::number(d.value("var_1pct_daily").toDouble() * 100, 'f', 3) + "%", this,
                         ui::colors::WARNING()),
            gs_make_card("CVaR 1%",
                         QString::number(d.value("cvar_1pct_daily").toDouble() * 100, 'f', 3) + "%", this,
                         ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(tail, this));

        // Drawdown timeline strip
        const int peak_i = d.value("max_drawdown_peak_index").toInt();
        const int trough_i = d.value("max_drawdown_trough_index").toInt();
        const int recov_i = d.value("max_drawdown_recovery_index").toInt();
        auto* timeline = new QLabel(QString(
            "Drawdown timeline:  peak @ obs %1  →  trough @ obs %2 (Δ %3 days)  →  %4")
                                        .arg(peak_i).arg(trough_i).arg(trough_i - peak_i)
                                        .arg(recov_i >= 0
                                                 ? QString("recovered @ obs %1").arg(recov_i)
                                                 : QString("not yet recovered")));
        timeline->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                        "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                    .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                         recovered ? ui::colors::POSITIVE() : ui::colors::WARNING()));
        results_layout_->addWidget(timeline);

        status_label_->setText(QString("MaxDD %1%  Cur %2%  Vol %3%  Recovered: %4")
                                   .arg(mdd_pct, 0, 'f', 2).arg(cur_dd, 0, 'f', 2)
                                   .arg(cur_vol * 100, 0, 'f', 2).arg(recovered ? "YES" : "NO"));
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

        auto* hdr = new QLabel(QString("MODEL: %1   |   %2 QUANTILES   |   %3")
                                   .arg(model.toUpper()).arg(d.value("n_quantiles").toInt())
                                   .arg(verdict));
        const bool good = verdict.contains("STRONG") || verdict.contains("MODERATE");
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    good ? ui::colors::POSITIVE() : ui::colors::WARNING()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> ic = {
            gs_make_card("PEARSON IC", gs_fmt_num(pearson, 4), this, gs_pos_neg_color(pearson)),
            gs_make_card("SPEARMAN IC", gs_fmt_num(spearman, 4), this, gs_pos_neg_color(spearman)),
            gs_make_card("PEARSON p", gs_fmt_num(d.value("pearson_p_value").toDouble(), 4), this),
            gs_make_card("SPEARMAN p", gs_fmt_num(d.value("spearman_p_value").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(ic, this));

        QList<QWidget*> hit = {
            gs_make_card("DIR. ACCURACY", QString::number(dir_acc, 'f', 1) + "%", this,
                         dir_acc > 55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("HIT RATE", QString::number(hit_rate, 'f', 1) + "%", this,
                         hit_rate > 55 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("LONG-SHORT SPREAD",
                         QString::number(ls_bps, 'f', 1) + " bps", this,
                         gs_pos_neg_color(ls_bps)),
            gs_make_card("MONOTONIC", monotonic ? "YES" : "NO", this,
                         monotonic ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(hit, this));

        // Quantile table
        const QJsonArray quants = d.value("quantile_table").toArray();
        if (!quants.isEmpty()) {
            auto* table = new QTableWidget(quants.size(), 6, this);
            table->setHorizontalHeaderLabels({"Q", "N", "Pred Range", "Mean Return", "Median Return", "Win Rate"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(quants.size() * 24 + 32, 280));
            for (int i = 0; i < quants.size(); ++i) {
                const auto q = quants[i].toObject();
                const double mr = q.value("mean_return").toDouble();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(q.value("quantile").toInt())));
                table->setItem(i, 1, new QTableWidgetItem(QString::number(q.value("n").toInt())));
                table->setItem(i, 2, new QTableWidgetItem(
                    QString("[%1, %2]").arg(q.value("pred_min").toDouble(), 0, 'f', 4)
                                       .arg(q.value("pred_max").toDouble(), 0, 'f', 4)));
                auto* mri = new QTableWidgetItem(QString::number(mr * 100, 'f', 4) + "%");
                mri->setForeground(QColor(gs_pos_neg_color(mr)));
                mri->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 3, mri);
                auto* mei = new QTableWidgetItem(
                    QString::number(q.value("median_return").toDouble() * 100, 'f', 4) + "%");
                mei->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 4, mei);
                auto* wi = new QTableWidgetItem(
                    QString::number(q.value("win_rate_pct").toDouble(), 'f', 1) + "%");
                wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 5, wi);
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("%1: IC=%2  Spread=%3 bps  → %4")
                                   .arg(model).arg(spearman, 0, 'f', 3)
                                   .arg(ls_bps, 0, 'f', 1).arg(verdict));
        return;
    }

    // ── 6. FACTOR QUANTILES ─────────────────────────────────────────────────
    if (command == "factor_quantiles") {
        const double spread_bps = d.value("long_short_spread_bps").toDouble();
        const double ls_sharpe = d.value("long_short_sharpe_annualized").toDouble();
        const bool monotone = d.value("monotonic").toBool();
        const QString verdict = d.value("verdict").toString();
        const int n_q = d.value("n_quantiles").toInt();

        auto* hdr = new QLabel(QString("%1 QUANTILES   |   %2").arg(n_q).arg(verdict));
        const bool good = verdict.contains("STRONG");
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    good ? ui::colors::POSITIVE() : ui::colors::WARNING()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("L/S SPREAD", QString::number(spread_bps, 'f', 1) + " bps", this,
                         gs_pos_neg_color(spread_bps)),
            gs_make_card("L/S SHARPE (ANN)", gs_fmt_num(ls_sharpe, 3), this,
                         ls_sharpe >= 1.0 ? ui::colors::POSITIVE()
                                          : ls_sharpe > 0 ? ui::colors::WARNING()
                                                          : ui::colors::NEGATIVE()),
            gs_make_card("MONOTONIC", monotone ? "YES" : "NO", this,
                         monotone ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("OBSERVATIONS", QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Quantile detail table
        const QJsonArray quants = d.value("quantiles").toArray();
        if (!quants.isEmpty()) {
            auto* table = new QTableWidget(quants.size(), 7, this);
            table->setHorizontalHeaderLabels({"Q", "N", "Pred Mean", "Ret Mean", "Ret Median", "Ret σ", "Win Rate"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(quants.size() * 26 + 32, 320));
            for (int i = 0; i < quants.size(); ++i) {
                const auto q = quants[i].toObject();
                const int qid = q.value("quantile").toInt();
                const double rm = q.value("ret_mean").toDouble();
                QString tag = QString::number(qid);
                if (qid == 1) tag += " (low)";
                if (qid == n_q) tag += " (high)";
                table->setItem(i, 0, new QTableWidgetItem(tag));
                table->setItem(i, 1, new QTableWidgetItem(QString::number(q.value("n").toInt())));
                auto add_n = [&](int col, double v, int dec, bool color = false) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (color) it->setForeground(QColor(gs_pos_neg_color(v)));
                    table->setItem(i, col, it);
                };
                add_n(2, q.value("pred_mean").toDouble(), 4);
                auto* rmi = new QTableWidgetItem(QString::number(rm * 100, 'f', 4) + "%");
                rmi->setForeground(QColor(gs_pos_neg_color(rm)));
                rmi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 3, rmi);
                auto* rmd = new QTableWidgetItem(
                    QString::number(q.value("ret_median").toDouble() * 100, 'f', 4) + "%");
                rmd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 4, rmd);
                add_n(5, q.value("ret_std").toDouble() * 100, 4);
                auto* wi = new QTableWidgetItem(
                    QString::number(q.value("win_rate_pct").toDouble(), 'f', 1) + "%");
                wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 6, wi);
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("Spread %1 bps  Sharpe %2  → %3")
                                   .arg(spread_bps, 0, 'f', 1).arg(ls_sharpe, 0, 'f', 2).arg(verdict));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
