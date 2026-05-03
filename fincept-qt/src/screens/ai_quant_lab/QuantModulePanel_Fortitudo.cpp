// src/screens/ai_quant_lab/QuantModulePanel_Fortitudo.cpp
//
// Fortitudo panel — 6 risk-aware portfolio analyses (metrics, covariance, MV opt,
// CVaR opt, efficient frontier, exp-decay weights). Mirrors the GS Quant /
// Functime / Statsmodels pattern. Extracted from QuantModulePanel.cpp to keep
// that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QList>
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
// FORTITUDO PANEL — 6 risk-aware portfolio analyses (metrics, covariance, MV opt,
// CVaR opt, efficient frontier, exp-decay weights). Mirrors the GS Quant /
// Functime / Statsmodels pattern.
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

// Helper: build the asset-input row used by every Fortitudo sub-tab.
// Lets the user paste a tickers list ("AAPL,MSFT,GOOG") that the Python side
// fetches via yfinance — much simpler than typing thousands of return values.
struct FortAssetInputs {
    QLineEdit* tickers_edit;
    QComboBox* period_combo;
};

FortAssetInputs build_fort_asset_row(const QString& key_prefix, QWidget* parent,
                                      QHash<QString, QLineEdit*>& text_inputs,
                                      QHash<QString, QComboBox*>& combo_inputs,
                                      const QString& default_tickers = "AAPL,MSFT,GOOG,TSLA,AMZN") {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    auto* tk = new QLineEdit(row);
    tk->setPlaceholderText("Tickers (comma-separated, >= 2). Returns fetched via Yahoo Finance.");
    tk->setText(default_tickers);
    tk->setStyleSheet(input_ss());
    text_inputs[key_prefix + "_tickers"] = tk;

    auto* per = new QComboBox(row);
    per->addItems({"6mo", "1y", "2y", "3y", "5y", "10y", "max"});
    per->setCurrentText("1y");
    per->setStyleSheet(combo_ss());
    per->setFixedWidth(80);
    combo_inputs[key_prefix + "_period"] = per;

    hl->addWidget(tk, 1);
    hl->addWidget(per);
    return {tk, per};
}

// Build a weights table: asset / weight / weight % with color-coded bars.
QTableWidget* fort_weights_table(const QJsonArray& weights, QWidget* parent) {
    auto* table = new QTableWidget(weights.size(), 3, parent);
    table->setHorizontalHeaderLabels({"Asset", "Weight", "% of Portfolio"});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->setStyleSheet(table_ss());
    table->setMaximumHeight(qMin(weights.size() * 24 + 32, 320));
    for (int i = 0; i < weights.size(); ++i) {
        const auto w = weights[i].toObject();
        const double weight = w.value("weight").toDouble();
        const double pct = w.value("weight_pct").toDouble();
        table->setItem(i, 0, new QTableWidgetItem(w.value("asset").toString()));
        auto* wi = new QTableWidgetItem(QString::number(weight, 'f', 4));
        wi->setForeground(QColor(weight >= 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(i, 1, wi);
        auto* pi = new QTableWidgetItem(QString::number(pct, 'f', 2) + "%");
        pi->setForeground(QColor(pct >= 50.0 ? ui::colors::WARNING()
                                              : pct >= 20.0 ? ui::colors::TEXT_PRIMARY()
                                                            : ui::colors::TEXT_SECONDARY()));
        pi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(i, 2, pi);
        table->setRowHeight(i, 24);
    }
    return table;
}

} // namespace

QWidget* QuantModulePanel::build_fortitudo_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Portfolio Metrics ────────────────────────────────────────────────────
    auto* pm = new QWidget(this);
    auto* pml = new QVBoxLayout(pm);
    pml->setContentsMargins(12, 12, 12, 12);
    pml->setSpacing(8);

    auto pm_assets = build_fort_asset_row("ft_pm", pm, text_inputs_, combo_inputs_);
    pml->addWidget(build_input_row("Tickers", pm_assets.tickers_edit, pm));
    pml->addWidget(build_input_row("History Period", pm_assets.period_combo, pm));

    auto* pm_weights = new QLineEdit(pm);
    pm_weights->setPlaceholderText("Weights (comma-separated, will be normalized to 1.0). Equal-weight if blank.");
    pm_weights->setStyleSheet(input_ss());
    text_inputs_["ft_pm_weights"] = pm_weights;
    pml->addWidget(build_input_row("Portfolio Weights", pm_weights, pm));

    auto* pm_alpha = make_double_spin(0.001, 0.499, 0.05, 3, "", pm);
    double_inputs_["ft_pm_alpha"] = pm_alpha;
    pml->addWidget(build_input_row("CVaR α (tail probability)", pm_alpha, pm));

    auto* pm_run = make_run_button("COMPUTE PORTFOLIO METRICS", pm);
    connect(pm_run, &QPushButton::clicked, this, [this]() {
        const QString tk = text_inputs_["ft_pm_tickers"]->text().trimmed();
        if (tk.isEmpty()) {
            display_error("Enter at least 2 tickers (e.g. AAPL,MSFT,GOOG).");
            return;
        }
        QJsonObject params;
        params["tickers"] = tk;
        params["period"] = combo_inputs_["ft_pm_period"]->currentText();
        params["alpha"] = double_inputs_["ft_pm_alpha"]->value();
        const QString w_text = text_inputs_["ft_pm_weights"]->text().trimmed();
        if (!w_text.isEmpty()) {
            QJsonArray w;
            QString bad;
            if (!parse_doubles(w_text, w, &bad)) {
                display_error(QString("Weights: '%1' is not numeric.").arg(bad));
                return;
            }
            params["weights"] = w;
        }
        show_loading(QString("Fetching %1 from yfinance and computing metrics...").arg(tk));
        AIQuantLabService::instance().fort_portfolio_metrics(params);
    });
    pml->addWidget(pm_run);
    pml->addStretch();
    tabs->addTab(pm, "Portfolio Metrics");

    // ── Covariance / Correlation ─────────────────────────────────────────────
    auto* cv = new QWidget(this);
    auto* cvl = new QVBoxLayout(cv);
    cvl->setContentsMargins(12, 12, 12, 12);
    cvl->setSpacing(8);

    auto cv_assets = build_fort_asset_row("ft_cv", cv, text_inputs_, combo_inputs_);
    cvl->addWidget(build_input_row("Tickers", cv_assets.tickers_edit, cv));
    cvl->addWidget(build_input_row("History Period", cv_assets.period_combo, cv));

    auto* cv_hint = new QLabel(
        "Computes the full covariance matrix, correlation matrix, and per-asset moments "
        "(mean, vol, skew, kurtosis). Asset count is capped — keep it under 12 for readability.", cv);
    cv_hint->setWordWrap(true);
    cv_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    cvl->addWidget(cv_hint);

    auto* cv_run = make_run_button("COMPUTE COV + CORR + MOMENTS", cv);
    connect(cv_run, &QPushButton::clicked, this, [this]() {
        const QString tk = text_inputs_["ft_cv_tickers"]->text().trimmed();
        if (tk.isEmpty()) {
            display_error("Enter at least 2 tickers.");
            return;
        }
        QJsonObject params;
        params["tickers"] = tk;
        params["period"] = combo_inputs_["ft_cv_period"]->currentText();
        show_loading(QString("Fetching %1 and computing covariance...").arg(tk));
        AIQuantLabService::instance().fort_covariance_matrix(params);
    });
    cvl->addWidget(cv_run);
    cvl->addStretch();
    tabs->addTab(cv, "Covariance");

    // ── Mean-Variance Optimization ───────────────────────────────────────────
    auto* mv = new QWidget(this);
    auto* mvl = new QVBoxLayout(mv);
    mvl->setContentsMargins(12, 12, 12, 12);
    mvl->setSpacing(8);

    auto mv_assets = build_fort_asset_row("ft_mv", mv, text_inputs_, combo_inputs_);
    mvl->addWidget(build_input_row("Tickers", mv_assets.tickers_edit, mv));
    mvl->addWidget(build_input_row("History Period", mv_assets.period_combo, mv));

    auto* mv_obj = new QComboBox(mv);
    mv_obj->addItems({"min_variance", "max_sharpe", "target_return"});
    mv_obj->setStyleSheet(combo_ss());
    combo_inputs_["ft_mv_objective"] = mv_obj;
    mvl->addWidget(build_input_row("Objective", mv_obj, mv));

    auto* mv_target = make_double_spin(0, 100, 8.0, 2, "%", mv);
    double_inputs_["ft_mv_target"] = mv_target;
    mvl->addWidget(build_input_row("Target Annualized Return", mv_target, mv));

    auto* mv_long = new QComboBox(mv);
    mv_long->addItems({"true", "false"});
    mv_long->setStyleSheet(combo_ss());
    combo_inputs_["ft_mv_long_only"] = mv_long;
    mvl->addWidget(build_input_row("Long Only", mv_long, mv));

    auto* mv_max = make_double_spin(0.0, 1.0, 0.5, 2, "", mv);
    mv_max->setSpecialValueText("none");
    double_inputs_["ft_mv_max"] = mv_max;
    mvl->addWidget(build_input_row("Max Single-Asset Weight (0 = none)", mv_max, mv));

    auto* mv_rf = make_double_spin(0, 20, 4.5, 2, "%", mv);
    double_inputs_["ft_mv_rf"] = mv_rf;
    mvl->addWidget(build_input_row("Risk-Free Rate (annual)", mv_rf, mv));

    auto* mv_run = make_run_button("OPTIMIZE PORTFOLIO", mv);
    connect(mv_run, &QPushButton::clicked, this, [this]() {
        const QString tk = text_inputs_["ft_mv_tickers"]->text().trimmed();
        if (tk.isEmpty()) {
            display_error("Enter at least 2 tickers.");
            return;
        }
        const QString obj = combo_inputs_["ft_mv_objective"]->currentText();
        QJsonObject params;
        params["tickers"] = tk;
        params["period"] = combo_inputs_["ft_mv_period"]->currentText();
        params["objective"] = obj;
        params["long_only"] = combo_inputs_["ft_mv_long_only"]->currentText() == "true";
        const double maxw = double_inputs_["ft_mv_max"]->value();
        if (maxw > 0.0) params["max_weight"] = maxw;
        // Translate annual % rf rate to per-day decimal for daily returns.
        params["risk_free_rate"] = double_inputs_["ft_mv_rf"]->value() / 100.0 / 252.0;
        if (obj == "target_return") {
            // Translate annualized % to per-day decimal so the optimizer's
            // expected_return constraint matches.
            params["target_return"] = std::pow(1.0 + double_inputs_["ft_mv_target"]->value() / 100.0,
                                                1.0 / 252.0) - 1.0;
        }
        show_loading(QString("Fetching %1 and solving %2...").arg(tk, obj));
        AIQuantLabService::instance().fort_mean_variance_optimize(params);
    });
    mvl->addWidget(mv_run);
    mvl->addStretch();
    tabs->addTab(mv, "MV Optimize");

    // ── Mean-CVaR Optimization ───────────────────────────────────────────────
    auto* cvopt = new QWidget(this);
    auto* cvol = new QVBoxLayout(cvopt);
    cvol->setContentsMargins(12, 12, 12, 12);
    cvol->setSpacing(8);

    auto cv_opt_assets = build_fort_asset_row("ft_cvopt", cvopt, text_inputs_, combo_inputs_);
    cvol->addWidget(build_input_row("Tickers", cv_opt_assets.tickers_edit, cvopt));
    cvol->addWidget(build_input_row("History Period", cv_opt_assets.period_combo, cvopt));

    auto* cvopt_obj = new QComboBox(cvopt);
    cvopt_obj->addItems({"min_cvar", "target_return"});
    cvopt_obj->setStyleSheet(combo_ss());
    combo_inputs_["ft_cvopt_objective"] = cvopt_obj;
    cvol->addWidget(build_input_row("Objective", cvopt_obj, cvopt));

    auto* cvopt_alpha = make_double_spin(0.001, 0.499, 0.05, 3, "", cvopt);
    double_inputs_["ft_cvopt_alpha"] = cvopt_alpha;
    cvol->addWidget(build_input_row("CVaR α (tail probability)", cvopt_alpha, cvopt));

    auto* cvopt_target = make_double_spin(0, 100, 8.0, 2, "%", cvopt);
    double_inputs_["ft_cvopt_target"] = cvopt_target;
    cvol->addWidget(build_input_row("Target Annualized Return", cvopt_target, cvopt));

    auto* cvopt_long = new QComboBox(cvopt);
    cvopt_long->addItems({"true", "false"});
    cvopt_long->setStyleSheet(combo_ss());
    combo_inputs_["ft_cvopt_long_only"] = cvopt_long;
    cvol->addWidget(build_input_row("Long Only", cvopt_long, cvopt));

    auto* cvopt_max = make_double_spin(0.0, 1.0, 0.5, 2, "", cvopt);
    cvopt_max->setSpecialValueText("none");
    double_inputs_["ft_cvopt_max"] = cvopt_max;
    cvol->addWidget(build_input_row("Max Single-Asset Weight (0 = none)", cvopt_max, cvopt));

    auto* cvopt_run = make_run_button("MINIMIZE TAIL RISK", cvopt);
    connect(cvopt_run, &QPushButton::clicked, this, [this]() {
        const QString tk = text_inputs_["ft_cvopt_tickers"]->text().trimmed();
        if (tk.isEmpty()) {
            display_error("Enter at least 2 tickers.");
            return;
        }
        const QString obj = combo_inputs_["ft_cvopt_objective"]->currentText();
        QJsonObject params;
        params["tickers"] = tk;
        params["period"] = combo_inputs_["ft_cvopt_period"]->currentText();
        params["objective"] = obj;
        params["alpha"] = double_inputs_["ft_cvopt_alpha"]->value();
        params["long_only"] = combo_inputs_["ft_cvopt_long_only"]->currentText() == "true";
        const double maxw = double_inputs_["ft_cvopt_max"]->value();
        if (maxw > 0.0) params["max_weight"] = maxw;
        if (obj == "target_return") {
            params["target_return"] = std::pow(1.0 + double_inputs_["ft_cvopt_target"]->value() / 100.0,
                                                1.0 / 252.0) - 1.0;
        }
        show_loading(QString("Fetching %1 and solving %2...").arg(tk, obj));
        AIQuantLabService::instance().fort_mean_cvar_optimize(params);
    });
    cvol->addWidget(cvopt_run);
    cvol->addStretch();
    tabs->addTab(cvopt, "CVaR Optimize");

    // ── Efficient Frontier ───────────────────────────────────────────────────
    auto* ef = new QWidget(this);
    auto* efl = new QVBoxLayout(ef);
    efl->setContentsMargins(12, 12, 12, 12);
    efl->setSpacing(8);

    auto ef_assets = build_fort_asset_row("ft_ef", ef, text_inputs_, combo_inputs_);
    efl->addWidget(build_input_row("Tickers", ef_assets.tickers_edit, ef));
    efl->addWidget(build_input_row("History Period", ef_assets.period_combo, ef));

    auto* ef_n = new QSpinBox(ef);
    ef_n->setRange(5, 50);
    ef_n->setValue(20);
    ef_n->setStyleSheet(spinbox_ss());
    int_inputs_["ft_ef_n_points"] = ef_n;
    efl->addWidget(build_input_row("Frontier Points", ef_n, ef));

    auto* ef_long = new QComboBox(ef);
    ef_long->addItems({"true", "false"});
    ef_long->setStyleSheet(combo_ss());
    combo_inputs_["ft_ef_long_only"] = ef_long;
    efl->addWidget(build_input_row("Long Only", ef_long, ef));

    auto* ef_max = make_double_spin(0.0, 1.0, 0.5, 2, "", ef);
    ef_max->setSpecialValueText("none");
    double_inputs_["ft_ef_max"] = ef_max;
    efl->addWidget(build_input_row("Max Single-Asset Weight (0 = none)", ef_max, ef));

    auto* ef_rf = make_double_spin(0, 20, 4.5, 2, "%", ef);
    double_inputs_["ft_ef_rf"] = ef_rf;
    efl->addWidget(build_input_row("Risk-Free Rate (annual)", ef_rf, ef));

    auto* ef_run = make_run_button("BUILD EFFICIENT FRONTIER", ef);
    connect(ef_run, &QPushButton::clicked, this, [this]() {
        const QString tk = text_inputs_["ft_ef_tickers"]->text().trimmed();
        if (tk.isEmpty()) {
            display_error("Enter at least 2 tickers.");
            return;
        }
        QJsonObject params;
        params["tickers"] = tk;
        params["period"] = combo_inputs_["ft_ef_period"]->currentText();
        params["n_points"] = int_inputs_["ft_ef_n_points"]->value();
        params["long_only"] = combo_inputs_["ft_ef_long_only"]->currentText() == "true";
        const double maxw = double_inputs_["ft_ef_max"]->value();
        if (maxw > 0.0) params["max_weight"] = maxw;
        params["risk_free_rate"] = double_inputs_["ft_ef_rf"]->value() / 100.0 / 252.0;
        show_loading(QString("Fetching %1 and tracing %2-point frontier...")
                         .arg(tk).arg(int_inputs_["ft_ef_n_points"]->value()));
        AIQuantLabService::instance().fort_efficient_frontier(params);
    });
    efl->addWidget(ef_run);
    efl->addStretch();
    tabs->addTab(ef, "Efficient Frontier");

    // ── Exponential Decay Probabilities ──────────────────────────────────────
    auto* ed = new QWidget(this);
    auto* edl = new QVBoxLayout(ed);
    edl->setContentsMargins(12, 12, 12, 12);
    edl->setSpacing(8);

    auto ed_assets = build_fort_asset_row("ft_ed", ed, text_inputs_, combo_inputs_);
    edl->addWidget(build_input_row("Tickers", ed_assets.tickers_edit, ed));
    edl->addWidget(build_input_row("History Period", ed_assets.period_combo, ed));

    auto* ed_hl = new QSpinBox(ed);
    ed_hl->setRange(5, 1000);
    ed_hl->setValue(60);
    ed_hl->setSuffix(" obs");
    ed_hl->setStyleSheet(spinbox_ss());
    int_inputs_["ft_ed_half_life"] = ed_hl;
    edl->addWidget(build_input_row("Half-Life (observations)", ed_hl, ed));

    auto* ed_hint = new QLabel(
        "Builds exponentially-decayed scenario weights so recent observations dominate. "
        "ESS (Kish effective sample size) tells you how much of the history you're effectively using.", ed);
    ed_hint->setWordWrap(true);
    ed_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    edl->addWidget(ed_hint);

    auto* ed_run = make_run_button("COMPUTE DECAY WEIGHTS", ed);
    connect(ed_run, &QPushButton::clicked, this, [this]() {
        const QString tk = text_inputs_["ft_ed_tickers"]->text().trimmed();
        if (tk.isEmpty()) {
            display_error("Enter at least 2 tickers (used only for series length).");
            return;
        }
        QJsonObject params;
        params["tickers"] = tk;
        params["period"] = combo_inputs_["ft_ed_period"]->currentText();
        params["half_life"] = int_inputs_["ft_ed_half_life"]->value();
        show_loading(QString("Computing %1-day half-life decay on %2...")
                         .arg(int_inputs_["ft_ed_half_life"]->value()).arg(tk));
        AIQuantLabService::instance().fort_exp_decay_probabilities(params);
    });
    edl->addWidget(ed_run);
    edl->addStretch();
    tabs->addTab(ed, "Decay Weights");

    vl->addWidget(tabs);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ── FORTITUDO: rich card-based display per command ──────────────────────────

void QuantModulePanel::display_fortitudo_result(const QString& command, const QJsonObject& payload) {
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

    // Section header — same shape as other rich displays
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_assets"))
        header_text += QString("  |  %1 ASSETS").arg(d.value("n_assets").toInt());
    if (d.contains("n_scenarios"))
        header_text += QString("  |  %1 SCENARIOS").arg(d.value("n_scenarios").toInt());
    else if (d.contains("n_observations"))
        header_text += QString("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 1. CHECK STATUS ──────────────────────────────────────────────────────
    if (command == "check_status") {
        auto bool_card = [this](const QString& label, bool ok) {
            return gs_make_card(label, ok ? "AVAILABLE" : "MISSING", this,
                                ok ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        };
        QList<QWidget*> deps = {
            bool_card("OPTIMIZATION", d.value("wrappers_optimization").toBool()),
            bool_card("FUNCTIONS", d.value("wrappers_functions").toBool()),
            bool_card("NATIVE FORTITUDO", d.value("native_fortitudo_tech").toBool()),
            gs_make_card("MODE", d.value("mode").toString().toUpper(), this, ui::colors::INFO()),
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
        status_label_->setText("Fortitudo backend ready");
        return;
    }

    // ── 2. PORTFOLIO METRICS ─────────────────────────────────────────────────
    if (command == "portfolio_metrics") {
        const double exp_ret = d.value("expected_return").toDouble();
        const double vol = d.value("volatility").toDouble();
        const double sharpe = d.value("sharpe_ratio").toDouble();
        const double ann_ret = d.value("annualized_return").toDouble();
        const double ann_vol = d.value("annualized_volatility").toDouble();
        const double var_pct = d.value("var_pct").toDouble();
        const double cvar_pct = d.value("cvar_pct").toDouble();
        const double hhi = d.value("concentration_hhi").toDouble();

        QList<QWidget*> ann = {
            gs_make_card("ANN. RETURN", QString::number(ann_ret * 100, 'f', 2) + "%", this,
                         gs_pos_neg_color(ann_ret)),
            gs_make_card("ANN. VOLATILITY", QString::number(ann_vol * 100, 'f', 2) + "%", this),
            gs_make_card("SHARPE RATIO", gs_fmt_num(sharpe, 4), this,
                         sharpe >= 1.0 ? ui::colors::POSITIVE()
                                      : sharpe >= 0  ? ui::colors::TEXT_PRIMARY()
                                                     : ui::colors::NEGATIVE()),
            gs_make_card("ALPHA (CVaR)", QString::number(d.value("alpha").toDouble() * 100, 'f', 1) + "%",
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(ann, this));

        QList<QWidget*> daily = {
            gs_make_card("DAILY RETURN", QString::number(exp_ret * 100, 'f', 4) + "%", this,
                         gs_pos_neg_color(exp_ret)),
            gs_make_card("DAILY VOL", QString::number(vol * 100, 'f', 4) + "%", this),
            gs_make_card("VaR", QString::number(var_pct, 'f', 2) + "%", this,
                         var_pct < -2 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card("CVaR (Expected Shortfall)", QString::number(cvar_pct, 'f', 2) + "%", this,
                         cvar_pct < -2 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(daily, this));

        QList<QWidget*> conc = {
            gs_make_card("CONCENTRATION (HHI)", gs_fmt_num(hhi, 4), this,
                         hhi > 0.4 ? ui::colors::WARNING() : ui::colors::POSITIVE()),
            gs_make_card("MAX WEIGHT", QString::number(d.value("max_weight").toDouble() * 100, 'f', 2) + "%", this),
            gs_make_card("MIN WEIGHT", QString::number(d.value("min_weight").toDouble() * 100, 'f', 2) + "%", this),
            gs_make_card("EFFECTIVE ASSETS",
                         hhi > 0 ? gs_fmt_num(1.0 / hhi, 2) : QString("—"), this,
                         ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(conc, this));

        const QJsonArray weights = d.value("weights").toArray();
        if (!weights.isEmpty()) {
            results_layout_->addWidget(fort_weights_table(weights, this));
        }

        status_label_->setText(QString("Sharpe %1  |  Ann. Ret %2%  |  Ann. Vol %3%  |  HHI %4")
                                   .arg(sharpe, 0, 'f', 3)
                                   .arg(ann_ret * 100, 0, 'f', 2)
                                   .arg(ann_vol * 100, 0, 'f', 2)
                                   .arg(hhi, 0, 'f', 3));
        return;
    }

    // ── 3. COVARIANCE / CORRELATION / MOMENTS ───────────────────────────────
    if (command == "covariance_matrix") {
        const double avg_corr = d.value("avg_off_diag_correlation").toDouble();
        QList<QWidget*> top = {
            gs_make_card("ASSETS", QString::number(d.value("n_assets").toInt()), this),
            gs_make_card("OBSERVATIONS", QString::number(d.value("n_observations").toInt()), this),
            gs_make_card("AVG OFF-DIAG ρ", gs_fmt_num(avg_corr, 4), this,
                         std::abs(avg_corr) > 0.5 ? ui::colors::WARNING()
                                                  : ui::colors::TEXT_PRIMARY()),
            gs_make_card("MAX |ρ|", gs_fmt_num(std::max(std::abs(d.value("max_off_diag_correlation").toDouble()),
                                                         std::abs(d.value("min_off_diag_correlation").toDouble())), 4),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-asset moments table
        const QJsonArray moments = d.value("moments").toArray();
        if (!moments.isEmpty()) {
            const auto first = moments[0].toObject();
            QStringList headers = {"Asset"};
            QStringList keys;
            for (auto it = first.begin(); it != first.end(); ++it) {
                if (it.key() == "asset") continue;
                keys << it.key();
                QString h = it.key();
                if (!h.isEmpty()) h[0] = h[0].toUpper();
                headers << h;
            }
            auto* mt = new QTableWidget(moments.size(), headers.size(), this);
            mt->setHorizontalHeaderLabels(headers);
            mt->verticalHeader()->setVisible(false);
            mt->setEditTriggers(QAbstractItemView::NoEditTriggers);
            mt->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            mt->setStyleSheet(table_ss());
            mt->setMaximumHeight(qMin(moments.size() * 24 + 32, 280));
            for (int r = 0; r < moments.size(); ++r) {
                const auto row = moments[r].toObject();
                mt->setItem(r, 0, new QTableWidgetItem(row.value("asset").toString()));
                for (int c = 0; c < keys.size(); ++c) {
                    auto* it = new QTableWidgetItem(
                        QString::number(row.value(keys[c]).toDouble(), 'f', 6));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    mt->setItem(r, c + 1, it);
                }
                mt->setRowHeight(r, 22);
            }
            auto* mlbl = new QLabel("PER-ASSET MOMENTS");
            mlbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(mlbl);
            results_layout_->addWidget(mt);
        }

        // Correlation matrix
        const QJsonArray corr = d.value("correlation_matrix").toArray();
        if (!corr.isEmpty()) {
            const auto first = corr[0].toObject();
            QStringList headers = {"Asset"};
            QStringList keys;
            for (auto it = first.begin(); it != first.end(); ++it) {
                if (it.key() == "asset") continue;
                keys << it.key();
                headers << it.key();
            }
            auto* ct = new QTableWidget(corr.size(), headers.size(), this);
            ct->setHorizontalHeaderLabels(headers);
            ct->verticalHeader()->setVisible(false);
            ct->setEditTriggers(QAbstractItemView::NoEditTriggers);
            ct->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
            ct->setStyleSheet(table_ss());
            ct->setMaximumHeight(qMin(corr.size() * 24 + 32, 280));
            for (int r = 0; r < corr.size(); ++r) {
                const auto row = corr[r].toObject();
                ct->setItem(r, 0, new QTableWidgetItem(row.value("asset").toString()));
                for (int c = 0; c < keys.size(); ++c) {
                    const double cv = row.value(keys[c]).toDouble();
                    auto* it = new QTableWidgetItem(QString::number(cv, 'f', 3));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (r == c) {
                        it->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
                    } else if (cv > 0.5) {
                        it->setForeground(QColor(ui::colors::POSITIVE()));
                    } else if (cv < -0.5) {
                        it->setForeground(QColor(ui::colors::NEGATIVE()));
                    }
                    ct->setItem(r, c + 1, it);
                }
                ct->setRowHeight(r, 22);
            }
            auto* clbl = new QLabel("CORRELATION MATRIX");
            clbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(clbl);
            results_layout_->addWidget(ct);
        }

        status_label_->setText(QString("%1 assets  |  %2 obs  |  avg ρ = %3")
                                   .arg(d.value("n_assets").toInt())
                                   .arg(d.value("n_observations").toInt())
                                   .arg(avg_corr, 0, 'f', 4));
        return;
    }

    // ── 4. MV / CVaR OPTIMIZATION (shared display path) ─────────────────────
    if (command == "mean_variance_optimize" || command == "mean_cvar_optimize") {
        const QString objective = d.value("objective").toString();
        const double sharpe = d.value("sharpe_ratio").toDouble();
        const double ann_ret = d.value("annualized_return").toDouble();
        const double ann_vol = d.value("annualized_volatility").toDouble();
        const double var = d.value("var").toDouble();
        const double cvar = d.value("cvar").toDouble();
        const double hhi = d.value("concentration_hhi").toDouble();

        auto* hdr = new QLabel(QString("OBJECTIVE: %1   |   LONG-ONLY: %2")
                                   .arg(objective.toUpper().replace('_', ' '))
                                   .arg(d.value("long_only").toBool() ? "YES" : "NO"));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("ANN. RETURN", QString::number(ann_ret * 100, 'f', 2) + "%", this,
                         gs_pos_neg_color(ann_ret)),
            gs_make_card("ANN. VOLATILITY", QString::number(ann_vol * 100, 'f', 2) + "%", this),
            gs_make_card("SHARPE RATIO", gs_fmt_num(sharpe, 4), this,
                         sharpe >= 1.0 ? ui::colors::POSITIVE()
                                      : sharpe >= 0  ? ui::colors::TEXT_PRIMARY()
                                                     : ui::colors::NEGATIVE()),
            gs_make_card("CONCENTRATION (HHI)", gs_fmt_num(hhi, 4), this,
                         hhi > 0.4 ? ui::colors::WARNING() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (command == "mean_cvar_optimize") {
            QList<QWidget*> tail = {
                gs_make_card("CVaR @ α", QString::number(cvar * 100, 'f', 4) + "%", this,
                             ui::colors::NEGATIVE()),
                gs_make_card("VaR @ α", QString::number(var * 100, 'f', 4) + "%", this,
                             ui::colors::WARNING()),
                gs_make_card("ALPHA",
                             QString::number(d.value("alpha").toDouble() * 100, 'f', 1) + "%",
                             this, ui::colors::INFO()),
                gs_make_card("MAX WEIGHT",
                             QString::number(d.value("max_weight").toDouble() * 100, 'f', 2) + "%", this),
            };
            results_layout_->addWidget(gs_card_row(tail, this));
        } else {
            QList<QWidget*> mv_extra = {
                gs_make_card("VARIANCE", gs_fmt_num(d.value("variance").toDouble(), 6), this),
                gs_make_card("DAILY RETURN",
                             QString::number(d.value("expected_return").toDouble() * 100, 'f', 4) + "%",
                             this),
                gs_make_card("MAX WEIGHT",
                             QString::number(d.value("max_weight").toDouble() * 100, 'f', 2) + "%", this),
                gs_make_card("MIN WEIGHT",
                             QString::number(d.value("min_weight").toDouble() * 100, 'f', 2) + "%", this),
            };
            results_layout_->addWidget(gs_card_row(mv_extra, this));
        }

        const QJsonArray weights = d.value("weights").toArray();
        if (!weights.isEmpty()) {
            auto* wlbl = new QLabel("OPTIMAL WEIGHTS");
            wlbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(wlbl);
            results_layout_->addWidget(fort_weights_table(weights, this));
        }

        status_label_->setText(QString("%1 — Sharpe %2  |  Ann. Ret %3%  |  Ann. Vol %4%")
                                   .arg(objective)
                                   .arg(sharpe, 0, 'f', 3)
                                   .arg(ann_ret * 100, 0, 'f', 2)
                                   .arg(ann_vol * 100, 0, 'f', 2));
        return;
    }

    // ── 5. EFFICIENT FRONTIER ───────────────────────────────────────────────
    if (command == "efficient_frontier") {
        const int n = d.value("n_points").toInt();
        const int max_sharpe_idx = d.value("max_sharpe_index").toInt();
        const int min_var_idx = d.value("min_var_index").toInt();
        const double best_sharpe = d.value("best_sharpe").toDouble();

        auto* hdr = new QLabel(QString("FRONTIER: %1 POINTS  |  ASSETS: %2  |  LONG-ONLY: %3")
                                   .arg(n).arg(d.value("n_assets").toInt())
                                   .arg(d.value("long_only").toBool() ? "YES" : "NO"));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("BEST SHARPE", gs_fmt_num(best_sharpe, 4), this,
                         best_sharpe >= 1.0 ? ui::colors::POSITIVE()
                                            : best_sharpe >= 0 ? ui::colors::TEXT_PRIMARY()
                                                               : ui::colors::NEGATIVE()),
            gs_make_card("MAX SHARPE @", QString("Point #%1").arg(max_sharpe_idx + 1),
                         this, ui::colors::POSITIVE()),
            gs_make_card("MIN VARIANCE @", QString("Point #%1").arg(min_var_idx + 1),
                         this, ui::colors::INFO()),
            gs_make_card("RANGE", QString::number(d.value("min_volatility").toDouble() * 100, 'f', 2) +
                                       "%  →  " +
                                       QString::number(d.value("max_volatility").toDouble() * 100, 'f', 2) + "%",
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Frontier table — every point with annualized stats
        const QJsonArray frontier = d.value("frontier").toArray();
        if (!frontier.isEmpty()) {
            auto* table = new QTableWidget(frontier.size(), 5, this);
            table->setHorizontalHeaderLabels({"#", "Daily Vol", "Daily Ret", "Sharpe", "Ann. Vol → Ret"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(frontier.size() * 24 + 32, 380));
            for (int i = 0; i < frontier.size(); ++i) {
                const auto p = frontier[i].toObject();
                const double vol = p.value("volatility").toDouble();
                const double ret = p.value("expected_return").toDouble();
                const double sh = p.value("sharpe_ratio").toDouble();
                const double avol = p.value("annualized_volatility").toDouble();
                const double aret = p.value("annualized_return").toDouble();

                QString tag = QString::number(i + 1);
                if (i == max_sharpe_idx) tag += " ★";
                if (i == min_var_idx) tag += " ◆";
                auto* ti = new QTableWidgetItem(tag);
                if (i == max_sharpe_idx) ti->setForeground(QColor(ui::colors::POSITIVE()));
                else if (i == min_var_idx) ti->setForeground(QColor(ui::colors::INFO()));
                table->setItem(i, 0, ti);

                auto add_num = [&](int col, const QString& s, const QString& col_color = {}) {
                    auto* it = new QTableWidgetItem(s);
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (!col_color.isEmpty()) it->setForeground(QColor(col_color));
                    table->setItem(i, col, it);
                };
                add_num(1, QString::number(vol * 100, 'f', 4) + "%");
                add_num(2, QString::number(ret * 100, 'f', 4) + "%", gs_pos_neg_color(ret));
                add_num(3, QString::number(sh, 'f', 4),
                        sh >= 1.0 ? ui::colors::POSITIVE()
                                  : sh >= 0  ? ui::colors::TEXT_PRIMARY()
                                             : ui::colors::NEGATIVE());
                add_num(4, QString::number(avol * 100, 'f', 2) + "%  →  " +
                               QString::number(aret * 100, 'f', 2) + "%",
                        gs_pos_neg_color(aret));
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);

            // Show the max-sharpe portfolio's weights below the table
            if (max_sharpe_idx >= 0 && max_sharpe_idx < frontier.size()) {
                const auto best = frontier[max_sharpe_idx].toObject();
                const auto best_weights = best.value("weights").toArray();
                if (!best_weights.isEmpty()) {
                    QJsonArray with_pct;
                    for (const auto& v : best_weights) {
                        auto o = v.toObject();
                        o["weight_pct"] = o.value("weight").toDouble() * 100.0;
                        with_pct.append(o);
                    }
                    auto* lbl = new QLabel(QString("MAX-SHARPE PORTFOLIO WEIGHTS  (★ Point #%1)")
                                               .arg(max_sharpe_idx + 1));
                    lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:6px 0 0 2px;")
                                            .arg(ui::colors::POSITIVE()));
                    results_layout_->addWidget(lbl);
                    results_layout_->addWidget(fort_weights_table(with_pct, this));
                }
            }
        }

        status_label_->setText(QString("%1 frontier points  |  best Sharpe %2 @ point %3")
                                   .arg(n).arg(best_sharpe, 0, 'f', 3).arg(max_sharpe_idx + 1));
        return;
    }

    // ── 6. EXPONENTIAL DECAY PROBABILITIES ──────────────────────────────────
    if (command == "exp_decay_probabilities") {
        const int hl = d.value("half_life").toInt();
        const double ess = d.value("effective_sample_size").toDouble();
        const double ess_pct = d.value("ess_pct_of_n").toDouble();
        const double ratio = d.value("weight_ratio_last_to_first").toDouble();

        QList<QWidget*> top = {
            gs_make_card("HALF-LIFE", QString("%1 obs").arg(hl), this, ui::colors::INFO()),
            gs_make_card("EFFECTIVE SAMPLE", gs_fmt_num(ess, 1), this,
                         ess_pct > 50 ? ui::colors::POSITIVE()
                                      : ess_pct > 25 ? ui::colors::WARNING()
                                                     : ui::colors::NEGATIVE()),
            gs_make_card("ESS % OF N", QString::number(ess_pct, 'f', 1) + "%", this,
                         ess_pct > 50 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("LAST/FIRST RATIO", QString("%1×").arg(ratio, 0, 'f', 2),
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> bounds = {
            gs_make_card("FIRST WEIGHT", gs_fmt_num(d.value("first_weight").toDouble(), 6), this),
            gs_make_card("LAST WEIGHT", gs_fmt_num(d.value("last_weight").toDouble(), 6), this,
                         ui::colors::POSITIVE()),
            gs_make_card("MIN WEIGHT", gs_fmt_num(d.value("min_weight").toDouble(), 6), this),
            gs_make_card("MAX WEIGHT", gs_fmt_num(d.value("max_weight").toDouble(), 6), this),
        };
        results_layout_->addWidget(gs_card_row(bounds, this));

        // Sampled weights curve
        const QJsonArray weights = d.value("weights").toArray();
        if (!weights.isEmpty()) {
            auto* table = new QTableWidget(weights.size(), 2, this);
            table->setHorizontalHeaderLabels({"Date / Index", "Weight"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(weights.size() * 22 + 32, 360));
            for (int i = 0; i < weights.size(); ++i) {
                const auto o = weights[i].toObject();
                const QString label = o.contains("date") ? o.value("date").toString()
                                                          : QString::number(o.value("index").toInt());
                table->setItem(i, 0, new QTableWidgetItem(label));
                auto* wi = new QTableWidgetItem(
                    QString::number(o.value("weight").toDouble(), 'f', 6));
                wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                // Color gradient: bigger weight → more saturated
                const double w = o.value("weight").toDouble();
                if (w > d.value("max_weight").toDouble() * 0.7) {
                    wi->setForeground(QColor(ui::colors::POSITIVE()));
                } else if (w < d.value("max_weight").toDouble() * 0.1) {
                    wi->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
                }
                table->setItem(i, 1, wi);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("Half-life %1 obs  |  ESS %2 (%3%%)  |  last/first %4×")
                                   .arg(hl).arg(ess, 0, 'f', 1).arg(ess_pct, 0, 'f', 1)
                                   .arg(ratio, 0, 'f', 1));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
