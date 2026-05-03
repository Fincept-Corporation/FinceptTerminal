// src/screens/ai_quant_lab/QuantModulePanel_Statsmodels.cpp
//
// Statsmodels panel — 6 econometric analyses (OLS, ARIMA, stationarity,
// ACF/PACF, Granger causality, descriptive). Mirrors the GS Quant / Functime
// pattern. Extracted from QuantModulePanel.cpp to keep that file maintainable.
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
#include <QJsonValue>
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
// STATSMODELS PANEL — 6 econometric analyses (OLS, ARIMA, stationarity,
// ACF/PACF, Granger causality, descriptive). Mirrors the GS Quant / Functime
// pattern.
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_statsmodels_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // Same LOAD SAMPLE helper shape as functime
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed,
                                  const QString& tip, bool walk = false) -> QPushButton* {
        auto* btn = new QPushButton("LOAD SAMPLE", parent);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(22);
        btn->setToolTip(tip);
        btn->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2;"
                                   "padding:0 8px; font-size:9px; font-weight:700; letter-spacing:0.5px; border-radius:2px; }"
                                   "QPushButton:hover { color:%3; border-color:%3; background:rgba(255,255,255,0.04); }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), module_.color.name()));
        QPointer<QLineEdit> guard(edit);
        connect(btn, &QPushButton::clicked, this, [guard, seed, walk]() {
            if (guard) guard->setText(walk ? sample_random_walk(seed) : sample_series(seed));
        });
        return btn;
    };

    // ── OLS Regression ───────────────────────────────────────────────────────
    auto* ols = new QWidget(this);
    auto* oll = new QVBoxLayout(ols);
    oll->setContentsMargins(12, 12, 12, 12);
    oll->setSpacing(8);

    auto* ols_y = new QLineEdit(ols);
    ols_y->setPlaceholderText("Dependent variable y (>= 10 values)");
    ols_y->setStyleSheet(input_ss());
    text_inputs_["sm_ols_y"] = ols_y;
    oll->addWidget(build_input_row("y (Dependent)", ols_y, ols));
    oll->addWidget(add_sample_btn(ols_y, ols, 71,
                                   "100-pt synthetic series (level ~50)"));

    auto* ols_x = new QLineEdit(ols);
    ols_x->setPlaceholderText("Regressor x (single column, same length as y). For multi-feature use the JSON 2D form.");
    ols_x->setStyleSheet(input_ss());
    text_inputs_["sm_ols_x"] = ols_x;
    oll->addWidget(build_input_row("x (Regressor)", ols_x, ols));
    oll->addWidget(add_sample_btn(ols_x, ols, 72,
                                   "100-pt synthetic regressor"));

    auto* ols_const = new QComboBox(ols);
    ols_const->addItems({"true", "false"});
    ols_const->setStyleSheet(combo_ss());
    combo_inputs_["sm_ols_const"] = ols_const;
    oll->addWidget(build_input_row("Add Constant", ols_const, ols));

    auto* ols_run = make_run_button("RUN OLS REGRESSION", ols);
    connect(ols_run, &QPushButton::clicked, this, [this]() {
        QJsonArray y, x;
        QString bad;
        if (!parse_doubles(text_inputs_["sm_ols_y"]->text(), y, &bad)) {
            display_error(QString("y: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["sm_ols_x"]->text(), x, &bad)) {
            display_error(QString("x: '%1' is not numeric.").arg(bad));
            return;
        }
        if (y.size() < 10) {
            display_error(QString("OLS needs at least 10 observations; you provided %1.").arg(y.size()));
            return;
        }
        if (y.size() != x.size()) {
            display_error(QString("y (%1) and x (%2) must have the same length.")
                              .arg(y.size()).arg(x.size()));
            return;
        }
        show_loading(QString("Fitting OLS on %1 obs...").arg(y.size()));
        QJsonObject params;
        params["y"] = y;
        params["x"] = x;
        params["add_constant"] = combo_inputs_["sm_ols_const"]->currentText() == "true";
        AIQuantLabService::instance().sm_ols(params);
    });
    oll->addWidget(ols_run);
    oll->addStretch();
    tabs->addTab(ols, "OLS Regression");

    // ── ARIMA ────────────────────────────────────────────────────────────────
    auto* ar = new QWidget(this);
    auto* arl = new QVBoxLayout(ar);
    arl->setContentsMargins(12, 12, 12, 12);
    arl->setSpacing(8);

    auto* ar_vals = new QLineEdit(ar);
    ar_vals->setPlaceholderText("Time series values (>= 30)");
    ar_vals->setStyleSheet(input_ss());
    text_inputs_["sm_ar_values"] = ar_vals;
    arl->addWidget(build_input_row("Series Values", ar_vals, ar));
    arl->addWidget(add_sample_btn(ar_vals, ar, 81,
                                   "200-pt synthetic series with seasonality"));

    auto* ar_p = new QSpinBox(ar);
    ar_p->setRange(0, 10);
    ar_p->setValue(1);
    ar_p->setStyleSheet(spinbox_ss());
    int_inputs_["sm_ar_p"] = ar_p;
    arl->addWidget(build_input_row("AR Order p", ar_p, ar));

    auto* ar_d = new QSpinBox(ar);
    ar_d->setRange(0, 3);
    ar_d->setValue(1);
    ar_d->setStyleSheet(spinbox_ss());
    int_inputs_["sm_ar_d"] = ar_d;
    arl->addWidget(build_input_row("Differencing d", ar_d, ar));

    auto* ar_q = new QSpinBox(ar);
    ar_q->setRange(0, 10);
    ar_q->setValue(1);
    ar_q->setStyleSheet(spinbox_ss());
    int_inputs_["sm_ar_q"] = ar_q;
    arl->addWidget(build_input_row("MA Order q", ar_q, ar));

    auto* ar_h = new QSpinBox(ar);
    ar_h->setRange(1, 365);
    ar_h->setValue(14);
    ar_h->setStyleSheet(spinbox_ss());
    int_inputs_["sm_ar_horizon"] = ar_h;
    arl->addWidget(build_input_row("Forecast Horizon", ar_h, ar));

    auto* ar_run = make_run_button("FIT ARIMA + FORECAST", ar);
    connect(ar_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["sm_ar_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("ARIMA needs at least 30 observations; you provided %1.").arg(vals.size()));
            return;
        }
        const int p = int_inputs_["sm_ar_p"]->value();
        const int d = int_inputs_["sm_ar_d"]->value();
        const int q = int_inputs_["sm_ar_q"]->value();
        show_loading(QString("Fitting ARIMA(%1,%2,%3) on %4 obs and projecting %5 steps...")
                         .arg(p).arg(d).arg(q).arg(vals.size()).arg(int_inputs_["sm_ar_horizon"]->value()));
        QJsonObject params;
        params["values"] = vals;
        params["p"] = p;
        params["d"] = d;
        params["q"] = q;
        params["horizon"] = int_inputs_["sm_ar_horizon"]->value();
        AIQuantLabService::instance().sm_arima(params);
    });
    arl->addWidget(ar_run);
    arl->addStretch();
    tabs->addTab(ar, "ARIMA");

    // ── Stationarity Tests (single d, full ADF + KPSS detail) ────────────────
    auto* st = new QWidget(this);
    auto* stl = new QVBoxLayout(st);
    stl->setContentsMargins(12, 12, 12, 12);
    stl->setSpacing(8);

    auto* st_vals = new QLineEdit(st);
    st_vals->setPlaceholderText("Time series values (>= 30)");
    st_vals->setStyleSheet(input_ss());
    text_inputs_["sm_st_values"] = st_vals;
    stl->addWidget(build_input_row("Series Values", st_vals, st));
    stl->addWidget(add_sample_btn(st_vals, st, 91,
                                   "150-pt random walk with drift", true));

    auto* st_d = new QSpinBox(st);
    st_d->setRange(0, 3);
    st_d->setValue(0);
    st_d->setStyleSheet(spinbox_ss());
    int_inputs_["sm_st_d"] = st_d;
    stl->addWidget(build_input_row("Differencing Order d", st_d, st));

    auto* st_adf = new QComboBox(st);
    st_adf->addItems({"c", "ct", "ctt", "n"});
    st_adf->setToolTip("c = constant, ct = constant + trend, ctt = constant + trend + quadratic trend, n = no constant");
    st_adf->setStyleSheet(combo_ss());
    combo_inputs_["sm_st_adf_reg"] = st_adf;
    stl->addWidget(build_input_row("ADF Regression Type", st_adf, st));

    auto* st_kpss = new QComboBox(st);
    st_kpss->addItems({"c", "ct"});
    st_kpss->setToolTip("c = constant only, ct = constant + trend");
    st_kpss->setStyleSheet(combo_ss());
    combo_inputs_["sm_st_kpss_reg"] = st_kpss;
    stl->addWidget(build_input_row("KPSS Regression Type", st_kpss, st));

    auto* st_run = make_run_button("RUN ADF + KPSS", st);
    connect(st_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["sm_st_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("Stationarity tests need at least 30 observations; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Running ADF + KPSS at d=%1...").arg(int_inputs_["sm_st_d"]->value()));
        QJsonObject params;
        params["values"] = vals;
        params["d"] = int_inputs_["sm_st_d"]->value();
        params["adf_regression"] = combo_inputs_["sm_st_adf_reg"]->currentText();
        params["kpss_regression"] = combo_inputs_["sm_st_kpss_reg"]->currentText();
        AIQuantLabService::instance().sm_stationarity_tests(params);
    });
    stl->addWidget(st_run);
    stl->addStretch();
    tabs->addTab(st, "Stationarity");

    // ── ACF / PACF ───────────────────────────────────────────────────────────
    auto* ap = new QWidget(this);
    auto* apl = new QVBoxLayout(ap);
    apl->setContentsMargins(12, 12, 12, 12);
    apl->setSpacing(8);

    auto* ap_vals = new QLineEdit(ap);
    ap_vals->setPlaceholderText("Time series values (>= 20). Used for ARIMA(p,q) order selection.");
    ap_vals->setStyleSheet(input_ss());
    text_inputs_["sm_ap_values"] = ap_vals;
    apl->addWidget(build_input_row("Series Values", ap_vals, ap));
    apl->addWidget(add_sample_btn(ap_vals, ap, 101,
                                   "200-pt synthetic series with seasonality"));

    auto* ap_lags = new QSpinBox(ap);
    ap_lags->setRange(2, 100);
    ap_lags->setValue(20);
    ap_lags->setStyleSheet(spinbox_ss());
    int_inputs_["sm_ap_nlags"] = ap_lags;
    apl->addWidget(build_input_row("Number of Lags", ap_lags, ap));

    auto* ap_run = make_run_button("COMPUTE ACF + PACF", ap);
    connect(ap_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["sm_ap_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 20) {
            display_error(QString("ACF/PACF needs at least 20 observations; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Computing ACF + PACF up to lag %1...").arg(int_inputs_["sm_ap_nlags"]->value()));
        QJsonObject params;
        params["values"] = vals;
        params["nlags"] = int_inputs_["sm_ap_nlags"]->value();
        AIQuantLabService::instance().sm_acf_pacf(params);
    });
    apl->addWidget(ap_run);
    apl->addStretch();
    tabs->addTab(ap, "ACF / PACF");

    // ── Granger Causality ────────────────────────────────────────────────────
    auto* gc = new QWidget(this);
    auto* gcl = new QVBoxLayout(gc);
    gcl->setContentsMargins(12, 12, 12, 12);
    gcl->setSpacing(8);

    auto* gc_y = new QLineEdit(gc);
    gc_y->setPlaceholderText("Effect series y (the one we ask: 'is this caused by x?')");
    gc_y->setStyleSheet(input_ss());
    text_inputs_["sm_gc_y"] = gc_y;
    gcl->addWidget(build_input_row("y (Effect)", gc_y, gc));
    gcl->addWidget(add_sample_btn(gc_y, gc, 111,
                                   "200-pt synthetic effect series"));

    auto* gc_x = new QLineEdit(gc);
    gc_x->setPlaceholderText("Potential cause series x (same length as y)");
    gc_x->setStyleSheet(input_ss());
    text_inputs_["sm_gc_x"] = gc_x;
    gcl->addWidget(build_input_row("x (Potential Cause)", gc_x, gc));
    gcl->addWidget(add_sample_btn(gc_x, gc, 112,
                                   "200-pt synthetic candidate cause"));

    auto* gc_lag = new QSpinBox(gc);
    gc_lag->setRange(1, 20);
    gc_lag->setValue(4);
    gc_lag->setStyleSheet(spinbox_ss());
    int_inputs_["sm_gc_max_lag"] = gc_lag;
    gcl->addWidget(build_input_row("Max Lag to Test", gc_lag, gc));

    auto* gc_hint = new QLabel(
        "H₀: x does NOT Granger-cause y. p < 0.05 ⇒ x carries predictive information about y at that lag.",
        gc);
    gc_hint->setWordWrap(true);
    gc_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    gcl->addWidget(gc_hint);

    auto* gc_run = make_run_button("TEST GRANGER CAUSALITY", gc);
    connect(gc_run, &QPushButton::clicked, this, [this]() {
        QJsonArray y, x;
        QString bad;
        if (!parse_doubles(text_inputs_["sm_gc_y"]->text(), y, &bad)) {
            display_error(QString("y: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["sm_gc_x"]->text(), x, &bad)) {
            display_error(QString("x: '%1' is not numeric.").arg(bad));
            return;
        }
        if (y.size() < 30 || x.size() < 30) {
            display_error("Granger test needs at least 30 observations in both series.");
            return;
        }
        if (y.size() != x.size()) {
            display_error(QString("y (%1) and x (%2) must have the same length.")
                              .arg(y.size()).arg(x.size()));
            return;
        }
        show_loading(QString("Testing Granger causality up to lag %1...")
                         .arg(int_inputs_["sm_gc_max_lag"]->value()));
        QJsonObject params;
        params["y"] = y;
        params["x"] = x;
        params["max_lag"] = int_inputs_["sm_gc_max_lag"]->value();
        AIQuantLabService::instance().sm_granger_causality(params);
    });
    gcl->addWidget(gc_run);
    gcl->addStretch();
    tabs->addTab(gc, "Granger Causality");

    // ── Descriptive + Normality ──────────────────────────────────────────────
    auto* de = new QWidget(this);
    auto* del = new QVBoxLayout(de);
    del->setContentsMargins(12, 12, 12, 12);
    del->setSpacing(8);

    auto* de_vals = new QLineEdit(de);
    de_vals->setPlaceholderText("Numeric values (>= 8). Includes Jarque-Bera + Shapiro-Wilk normality tests.");
    de_vals->setStyleSheet(input_ss());
    text_inputs_["sm_de_values"] = de_vals;
    del->addWidget(build_input_row("Values", de_vals, de));
    del->addWidget(add_sample_btn(de_vals, de, 121,
                                   "200-pt synthetic series"));

    auto* de_run = make_run_button("DESCRIBE + TEST NORMALITY", de);
    connect(de_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["sm_de_values"]->text(), vals, &bad)) {
            display_error(QString("Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 8) {
            display_error(QString("Need at least 8 values; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Computing descriptive statistics + normality on %1 values...").arg(vals.size()));
        QJsonObject params;
        params["values"] = vals;
        AIQuantLabService::instance().sm_descriptive(params);
    });
    del->addWidget(de_run);
    del->addStretch();
    tabs->addTab(de, "Descriptive");

    vl->addWidget(tabs);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ── STATSMODELS: rich card-based display per command ────────────────────────

void QuantModulePanel::display_statsmodels_result(const QString& command, const QJsonObject& payload) {
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

    // Header strip — same shape as gs_quant / functime
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
            bool_card("STATSMODELS", d.value("statsmodels").toBool()),
            bool_card("SCIPY", d.value("scipy").toBool()),
            gs_make_card("BACKEND", d.value("backend").toString().toUpper(), this),
            gs_make_card("OPS", QString::number(d.value("ops_available").toArray().size()),
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(deps, this));
        const auto ops = d.value("ops_available").toArray();
        QStringList names;
        for (const auto& v : ops) names << v.toString();
        auto* lbl = new QLabel(QString("Operations: %1").arg(names.join(", ")));
        lbl->setWordWrap(true);
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(lbl);
        status_label_->setText("Statsmodels backend ready");
        return;
    }

    // ── 2. OLS ───────────────────────────────────────────────────────────────
    if (command == "ols") {
        const double r2 = d.value("r_squared").toDouble();
        const double adj = d.value("adj_r_squared").toDouble();
        const double f_stat = d.value("f_statistic").toDouble();
        const double f_p = d.value("f_p_value").toDouble();
        const double dw = d.value("durbin_watson").toDouble();
        const double bp_p = d.value("breusch_pagan_p").toDouble();

        auto* hdr = new QLabel(QString("OLS  |  %1 OBS  |  %2 FEATURES%3")
                                   .arg(d.value("n_observations").toInt())
                                   .arg(d.value("n_features").toInt())
                                   .arg(d.value("has_constant").toBool() ? "  +  CONSTANT" : ""));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> fit = {
            gs_make_card("R²", gs_fmt_num(r2, 4), this,
                         r2 >= 0.7 ? ui::colors::POSITIVE()
                                  : r2 >= 0.3 ? ui::colors::WARNING()
                                              : ui::colors::NEGATIVE()),
            gs_make_card("ADJ R²", gs_fmt_num(adj, 4), this),
            gs_make_card("F STATISTIC", gs_fmt_num(f_stat, 2), this),
            gs_make_card("F p-VALUE", gs_fmt_num(f_p, 4), this,
                         f_p < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(fit, this));

        QList<QWidget*> ic = {
            gs_make_card("AIC", gs_fmt_num(d.value("aic").toDouble(), 2), this),
            gs_make_card("BIC", gs_fmt_num(d.value("bic").toDouble(), 2), this),
            gs_make_card("LOG-LIK", gs_fmt_num(d.value("log_likelihood").toDouble(), 2), this),
            gs_make_card("RESID SE", gs_fmt_num(d.value("residual_std_error").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(ic, this));

        const QString dw_col = (dw >= 1.5 && dw <= 2.5) ? ui::colors::POSITIVE()
                                                         : ui::colors::WARNING();
        QList<QWidget*> diag = {
            gs_make_card("DURBIN-WATSON", gs_fmt_num(dw, 3), this, dw_col),
            gs_make_card("AUTOCORR.", d.value("dw_interpretation").toString().toUpper(), this, dw_col),
            gs_make_card("BREUSCH-PAGAN p", gs_fmt_num(bp_p, 4), this),
            gs_make_card("HOMOSCEDASTIC", d.value("homoscedastic_5pct").toBool() ? "YES" : "NO", this,
                         d.value("homoscedastic_5pct").toBool() ? ui::colors::POSITIVE()
                                                                  : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(diag, this));

        // Coefficient table
        const QJsonArray coefs = d.value("coefficients").toArray();
        if (!coefs.isEmpty()) {
            auto* table = new QTableWidget(coefs.size(), 7, this);
            table->setHorizontalHeaderLabels({"Term", "Estimate", "Std. Error", "t", "p", "95% CI Lower", "95% CI Upper"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(coefs.size() * 26 + 40, 320));
            for (int i = 0; i < coefs.size(); ++i) {
                const auto c = coefs[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(c.value("name").toString()));
                auto* est = new QTableWidgetItem(QString::number(c.value("estimate").toDouble(), 'f', 4));
                est->setForeground(QColor(gs_pos_neg_color(c.value("estimate").toDouble())));
                est->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 1, est);
                auto add_num = [&](int col, double v, int dec, const QString& col_color = {}) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (!col_color.isEmpty()) it->setForeground(QColor(col_color));
                    table->setItem(i, col, it);
                };
                add_num(2, c.value("std_error").toDouble(), 4);
                add_num(3, c.value("t_stat").toDouble(), 3);
                const double pv = c.value("p_value").toDouble();
                add_num(4, pv, 4, c.value("significant_5pct").toBool() ? ui::colors::POSITIVE()
                                                                        : ui::colors::TEXT_TERTIARY());
                add_num(5, c.value("ci_lower").toDouble(), 4);
                add_num(6, c.value("ci_upper").toDouble(), 4);
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("R²=%1  Adj=%2  F=%3 (p=%4)")
                                   .arg(r2, 0, 'f', 4).arg(adj, 0, 'f', 4)
                                   .arg(f_stat, 0, 'f', 2).arg(f_p, 0, 'g', 3));
        return;
    }

    // ── 3. ARIMA ─────────────────────────────────────────────────────────────
    if (command == "arima") {
        const QJsonArray order = d.value("order").toArray();
        const int p = order.size() > 0 ? order[0].toInt() : 0;
        const int d_ord = order.size() > 1 ? order[1].toInt() : 0;
        const int q = order.size() > 2 ? order[2].toInt() : 0;
        const int horizon = d.value("horizon").toInt();
        const double aic = d.value("aic").toDouble();
        const double bic = d.value("bic").toDouble();
        const double r2 = d.value("in_sample_r2").toDouble();
        const double lb_p = d.value("ljung_box_lag10_p").toDouble();
        const bool whitenoise = d.value("ljung_box_residuals_white_noise").toBool();
        const double last_actual = d.value("last_actual").toDouble();
        const double first_fc = d.value("first_forecast").toDouble();
        const double last_fc = d.value("last_forecast").toDouble();

        auto* hdr = new QLabel(QString("ARIMA(%1,%2,%3)  |  HORIZON %4 STEPS  |  %5 OBS")
                                   .arg(p).arg(d_ord).arg(q).arg(horizon)
                                   .arg(d.value("n_observations").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> fit = {
            gs_make_card("AIC", gs_fmt_num(aic, 2), this),
            gs_make_card("BIC", gs_fmt_num(bic, 2), this),
            gs_make_card("HQIC", gs_fmt_num(d.value("hqic").toDouble(), 2), this),
            gs_make_card("IN-SAMPLE R²", gs_fmt_num(r2, 4), this,
                         r2 >= 0.7 ? ui::colors::POSITIVE()
                                  : r2 >= 0.3 ? ui::colors::WARNING()
                                              : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(fit, this));

        QList<QWidget*> resid = {
            gs_make_card("LB(10) p-VALUE", gs_fmt_num(lb_p, 4), this,
                         whitenoise ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("RESID WHITE NOISE", whitenoise ? "YES" : "NO", this,
                         whitenoise ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("RESID MEAN", gs_fmt_num(d.value("residual_mean").toDouble(), 4), this),
            gs_make_card("RESID σ", gs_fmt_num(d.value("residual_std").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(resid, this));

        const double horizon_drift = last_fc - last_actual;
        QList<QWidget*> proj = {
            gs_make_card("LAST ACTUAL", gs_fmt_num(last_actual, 4), this),
            gs_make_card("FIRST FORECAST", gs_fmt_num(first_fc, 4), this,
                         gs_pos_neg_color(first_fc - last_actual)),
            gs_make_card("LAST FORECAST", gs_fmt_num(last_fc, 4), this,
                         gs_pos_neg_color(horizon_drift)),
            gs_make_card("HORIZON Δ", gs_fmt_num(horizon_drift, 4), this,
                         gs_pos_neg_color(horizon_drift)),
        };
        results_layout_->addWidget(gs_card_row(proj, this));

        // Forecast table with 95% bands
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            auto* table = new QTableWidget(forecast.size(), 4, this);
            table->setHorizontalHeaderLabels({"Step", "Lower 95%", "Point", "Upper 95%"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * 22 + 32, 280));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                auto* lo = new QTableWidgetItem(QString::number(o.value("lower_95").toDouble(), 'f', 4));
                lo->setForeground(QColor(ui::colors::NEGATIVE()));
                lo->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 1, lo);
                auto* pt = new QTableWidgetItem(QString::number(o.value("point").toDouble(), 'f', 4));
                pt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 2, pt);
                auto* up = new QTableWidgetItem(QString::number(o.value("upper_95").toDouble(), 'f', 4));
                up->setForeground(QColor(ui::colors::POSITIVE()));
                up->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 3, up);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("ARIMA(%1,%2,%3) — AIC=%4  R²=%5  LB(10) p=%6")
                                   .arg(p).arg(d_ord).arg(q)
                                   .arg(aic, 0, 'f', 1).arg(r2, 0, 'f', 3).arg(lb_p, 0, 'f', 3));
        return;
    }

    // ── 4. STATIONARITY TESTS ────────────────────────────────────────────────
    if (command == "stationarity_tests") {
        const QString verdict = d.value("verdict").toString();
        const int d_ord = d.value("differencing_order").toInt();

        auto* hdr = new QLabel(QString("VERDICT:  %1   |   DIFFERENCING d = %2")
                                   .arg(verdict).arg(d_ord));
        const bool stationary = verdict.contains("STATIONARY") && !verdict.contains("NON-");
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    stationary ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        results_layout_->addWidget(hdr);

        const QJsonObject adf = d.value("adf").toObject();
        const QJsonObject kp = d.value("kpss").toObject();

        // ADF row
        auto* adf_lbl = new QLabel("ADF (H₀: unit root → non-stationary)");
        adf_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(adf_lbl);
        QList<QWidget*> adf_row = {
            gs_make_card("STATISTIC", gs_fmt_num(adf.value("statistic").toDouble(), 4), this),
            gs_make_card("p-VALUE", gs_fmt_num(adf.value("p_value").toDouble(), 4), this,
                         adf.value("stationary_5pct").toBool() ? ui::colors::POSITIVE()
                                                                : ui::colors::WARNING()),
            gs_make_card("VERDICT @ 5%", adf.value("stationary_5pct").toBool() ? "STATIONARY"
                                                                                  : "NON-STATIONARY",
                         this, adf.value("stationary_5pct").toBool() ? ui::colors::POSITIVE()
                                                                       : ui::colors::NEGATIVE()),
            gs_make_card("LAGS USED", QString::number(adf.value("lags_used").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(adf_row, this));
        QList<QWidget*> adf_crit = {
            gs_make_card("ADF CRIT 1%", gs_fmt_num(adf.value("critical_1pct").toDouble(), 4), this),
            gs_make_card("ADF CRIT 5%", gs_fmt_num(adf.value("critical_5pct").toDouble(), 4), this),
            gs_make_card("ADF CRIT 10%", gs_fmt_num(adf.value("critical_10pct").toDouble(), 4), this),
            gs_make_card("REGRESSION", adf.value("regression").toString().toUpper(), this),
        };
        results_layout_->addWidget(gs_card_row(adf_crit, this));

        // KPSS row
        auto* kpss_lbl = new QLabel("KPSS (H₀: stationary)");
        kpss_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(kpss_lbl);
        QList<QWidget*> kp_row = {
            gs_make_card("STATISTIC", gs_fmt_num(kp.value("statistic").toDouble(), 4), this),
            gs_make_card("p-VALUE", gs_fmt_num(kp.value("p_value").toDouble(), 4), this,
                         kp.value("stationary_5pct").toBool() ? ui::colors::POSITIVE()
                                                               : ui::colors::WARNING()),
            gs_make_card("VERDICT @ 5%", kp.value("stationary_5pct").toBool() ? "STATIONARY"
                                                                                : "NON-STATIONARY",
                         this, kp.value("stationary_5pct").toBool() ? ui::colors::POSITIVE()
                                                                      : ui::colors::NEGATIVE()),
            gs_make_card("LAGS USED", QString::number(kp.value("lags_used").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(kp_row, this));
        QList<QWidget*> kp_crit = {
            gs_make_card("KPSS CRIT 1%", gs_fmt_num(kp.value("critical_1pct").toDouble(), 4), this),
            gs_make_card("KPSS CRIT 5%", gs_fmt_num(kp.value("critical_5pct").toDouble(), 4), this),
            gs_make_card("KPSS CRIT 10%", gs_fmt_num(kp.value("critical_10pct").toDouble(), 4), this),
            gs_make_card("REGRESSION", kp.value("regression").toString().toUpper(), this),
        };
        results_layout_->addWidget(gs_card_row(kp_crit, this));

        status_label_->setText(verdict);
        return;
    }

    // ── 5. ACF / PACF ───────────────────────────────────────────────────────
    if (command == "acf_pacf") {
        const int p_sug = d.value("suggested_arima_p").toInt();
        const int q_sug = d.value("suggested_arima_q").toInt();
        const double band = d.value("significance_band_95pct").toDouble();

        auto* hdr = new QLabel(QString("LAGS %1  |  SIGNIFICANCE BAND ±%2  |  SUGGESTED ARIMA(p=%3, q=%4)")
                                   .arg(d.value("nlags").toInt())
                                   .arg(band, 0, 'f', 4).arg(p_sug).arg(q_sug));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> sum = {
            gs_make_card("SIG. ACF LAGS", QString::number(d.value("n_significant_acf_lags").toInt()), this),
            gs_make_card("SIG. PACF LAGS", QString::number(d.value("n_significant_pacf_lags").toInt()), this),
            gs_make_card("SUGGESTED p", QString::number(p_sug), this, ui::colors::INFO()),
            gs_make_card("SUGGESTED q", QString::number(q_sug), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(sum, this));

        // Lag table
        const QJsonArray lags = d.value("lags").toArray();
        if (!lags.isEmpty()) {
            auto* table = new QTableWidget(lags.size(), 5, this);
            table->setHorizontalHeaderLabels({"Lag", "ACF", "ACF Sig.", "PACF", "PACF Sig."});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(lags.size() * 22 + 32, 360));
            for (int i = 0; i < lags.size(); ++i) {
                const auto o = lags[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("lag").toInt())));
                auto* a = new QTableWidgetItem(QString::number(o.value("acf").toDouble(), 'f', 4));
                a->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                a->setForeground(QColor(o.value("acf_significant").toBool() ? ui::colors::POSITIVE()
                                                                              : ui::colors::TEXT_PRIMARY()));
                table->setItem(i, 1, a);
                table->setItem(i, 2, new QTableWidgetItem(o.value("acf_significant").toBool() ? "✓" : "—"));
                auto* p = new QTableWidgetItem(QString::number(o.value("pacf").toDouble(), 'f', 4));
                p->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                p->setForeground(QColor(o.value("pacf_significant").toBool() ? ui::colors::POSITIVE()
                                                                                : ui::colors::TEXT_PRIMARY()));
                table->setItem(i, 3, p);
                table->setItem(i, 4, new QTableWidgetItem(o.value("pacf_significant").toBool() ? "✓" : "—"));
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Suggested ARIMA(p=%1, q=%2)").arg(p_sug).arg(q_sug));
        return;
    }

    // ── 6. GRANGER CAUSALITY ────────────────────────────────────────────────
    if (command == "granger_causality") {
        const bool any_causal = d.value("x_causes_y_at_any_lag").toBool();
        const QJsonArray causal_lags = d.value("causal_lags").toArray();

        auto* hdr = new QLabel(QString("MAX LAG TESTED: %1   |   %2")
                                   .arg(d.value("max_lag").toInt())
                                   .arg(d.value("interpretation").toString()));
        hdr->setWordWrap(true);
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    any_causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(hdr);

        QStringList causal_strs;
        for (const auto& v : causal_lags) causal_strs << QString::number(v.toInt());

        QList<QWidget*> sum = {
            gs_make_card("ANY CAUSAL LAG", any_causal ? "YES" : "NO", this,
                         any_causal ? ui::colors::POSITIVE() : ui::colors::TEXT_PRIMARY()),
            gs_make_card("CAUSAL LAGS", causal_strs.isEmpty() ? "—" : causal_strs.join(", "), this,
                         any_causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()),
            gs_make_card("LAGS TESTED", QString::number(d.value("max_lag").toInt()), this),
            gs_make_card("OBS", QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(sum, this));

        const QJsonArray tests = d.value("tests").toArray();
        if (!tests.isEmpty()) {
            auto* table = new QTableWidget(tests.size(), 6, this);
            table->setHorizontalHeaderLabels({"Lag", "F-stat", "F p-value", "χ² stat", "χ² p-value", "x ⇒ y @ 5%"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(tests.size() * 26 + 32, 280));
            for (int i = 0; i < tests.size(); ++i) {
                const auto t = tests[i].toObject();
                const bool causal = t.value("x_causes_y_5pct").toBool();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(t.value("lag").toInt())));
                auto add_num = [&](int col, double v, int dec) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, col, it);
                };
                add_num(1, t.value("f_statistic").toDouble(), 3);
                auto* fp = new QTableWidgetItem(QString::number(t.value("f_p_value").toDouble(), 'g', 4));
                fp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                fp->setForeground(QColor(causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
                table->setItem(i, 2, fp);
                add_num(3, t.value("chi2_statistic").toDouble(), 3);
                auto* cp = new QTableWidgetItem(QString::number(t.value("chi2_p_value").toDouble(), 'g', 4));
                cp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                cp->setForeground(QColor(causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
                table->setItem(i, 4, cp);
                auto* yn = new QTableWidgetItem(causal ? "YES" : "NO");
                yn->setForeground(QColor(causal ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
                yn->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 5, yn);
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(d.value("interpretation").toString());
        return;
    }

    // ── 7. DESCRIPTIVE + NORMALITY ───────────────────────────────────────────
    if (command == "descriptive") {
        QList<QWidget*> central = {
            gs_make_card("MEAN", gs_fmt_num(d.value("mean").toDouble(), 4), this),
            gs_make_card("MEDIAN", gs_fmt_num(d.value("median").toDouble(), 4), this),
            gs_make_card("STD DEV", gs_fmt_num(d.value("std").toDouble(), 4), this),
            gs_make_card("VARIANCE", gs_fmt_num(d.value("variance").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(central, this));

        QList<QWidget*> spread = {
            gs_make_card("MIN", gs_fmt_num(d.value("min").toDouble(), 4), this, ui::colors::NEGATIVE()),
            gs_make_card("MAX", gs_fmt_num(d.value("max").toDouble(), 4), this, ui::colors::POSITIVE()),
            gs_make_card("RANGE", gs_fmt_num(d.value("range").toDouble(), 4), this),
            gs_make_card("IQR", gs_fmt_num(d.value("iqr").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(spread, this));

        QList<QWidget*> qrt = {
            gs_make_card("p25", gs_fmt_num(d.value("p25").toDouble(), 4), this),
            gs_make_card("p50", gs_fmt_num(d.value("p50").toDouble(), 4), this),
            gs_make_card("p75", gs_fmt_num(d.value("p75").toDouble(), 4), this),
            gs_make_card("CV",
                         d.value("coefficient_of_variation").isNull()
                             ? QString("—")
                             : gs_fmt_num(d.value("coefficient_of_variation").toDouble(), 4),
                         this),
        };
        results_layout_->addWidget(gs_card_row(qrt, this));

        QList<QWidget*> shape = {
            gs_make_card("SKEWNESS", gs_fmt_num(d.value("skewness").toDouble(), 3), this,
                         gs_pos_neg_color(d.value("skewness").toDouble())),
            gs_make_card("KURTOSIS (excess)", gs_fmt_num(d.value("kurtosis_excess").toDouble(), 3), this),
            gs_make_card("SUM", gs_fmt_num(d.value("sum").toDouble(), 2), this),
            gs_make_card("OBS", QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(shape, this));

        // Normality
        auto* nh = new QLabel("NORMALITY  (H₀: data is normally distributed; p > 0.05 ⇒ cannot reject normal)");
        nh->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                              .arg(ui::colors::TEXT_TERTIARY()));
        results_layout_->addWidget(nh);

        const QJsonValue jb_normal = d.value("jarque_bera_normal_5pct");
        const QJsonValue sw_normal = d.value("shapiro_wilk_normal_5pct");
        QList<QWidget*> norm = {
            gs_make_card("JARQUE-BERA STAT", gs_fmt_num(d.value("jarque_bera_stat").toDouble(), 3), this),
            gs_make_card("JARQUE-BERA p", gs_fmt_num(d.value("jarque_bera_p").toDouble(), 4), this,
                         jb_normal.isBool()
                             ? (jb_normal.toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING())
                             : ui::colors::TEXT_PRIMARY()),
            gs_make_card("SHAPIRO-WILK p", gs_fmt_num(d.value("shapiro_wilk_p").toDouble(), 4), this,
                         sw_normal.isBool()
                             ? (sw_normal.toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING())
                             : ui::colors::TEXT_PRIMARY()),
            gs_make_card("NORMAL (BOTH)",
                         (jb_normal.isBool() && sw_normal.isBool() &&
                              jb_normal.toBool() && sw_normal.toBool())
                             ? "YES" : "NO",
                         this,
                         (jb_normal.isBool() && sw_normal.isBool() &&
                              jb_normal.toBool() && sw_normal.toBool())
                             ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(norm, this));

        status_label_->setText(QString("μ=%1  σ=%2  n=%3  JB p=%4")
                                   .arg(d.value("mean").toDouble(), 0, 'f', 3)
                                   .arg(d.value("std").toDouble(), 0, 'f', 3)
                                   .arg(d.value("n_observations").toInt())
                                   .arg(d.value("jarque_bera_p").toDouble(), 0, 'f', 3));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
