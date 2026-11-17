// src/screens/ai_quant_lab/QuantModulePanel_Fortitudo.cpp
//
// Fortitudo panel — 6 risk-aware portfolio analyses (metrics, covariance, MV opt,
// CVaR opt, efficient frontier, exp-decay weights). Mirrors the GS Quant /
// Functime / Statsmodels pattern. Extracted from QuantModulePanel.cpp to keep
// that file maintainable.
//
// Conventions of the script's payload: `var` / `cvar` are the portfolio RETURN
// at the alpha tail, per period (negative = loss) in every op; `sharpe_ratio` is
// annualised (× √252) and `sharpe_ratio_daily` the per-period one; values that
// cannot be computed are null and shown as "—".
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/tables/BarCellDelegate.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHash>
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
#include <limits>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;
using namespace fincept::screens::quant_charts;
using namespace fincept::screens::quant_advanced_helpers;

// ═══════════════════════════════════════════════════════════════════════════════
// FORTITUDO PANEL — 6 risk-aware portfolio analyses (metrics, covariance, MV opt,
// CVaR opt, efficient frontier, exp-decay weights). Mirrors the GS Quant /
// Functime / Statsmodels pattern.
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

// Colour of a tail return (VaR / CVaR): negative = loss.
QString fort_tail_color(const QJsonValue& v) {
    if (!v.isDouble())
        return ui::colors::TEXT_SECONDARY();
    return v.toDouble() < 0.0 ? QString(ui::colors::NEGATIVE()) : QString(ui::colors::TEXT_PRIMARY());
}

// Threshold colour of an (annualised) Sharpe ratio that may be null.
QString fort_sharpe_color(const QJsonValue& v) {
    if (!v.isDouble())
        return ui::colors::TEXT_SECONDARY();
    const double s = v.toDouble();
    return s >= 1.0 ? QString(ui::colors::POSITIVE()) : s >= 0 ? QString(ui::colors::TEXT_PRIMARY())
                                                               : QString(ui::colors::NEGATIVE());
}

// "VaR / CVaR are tail returns" note under the risk cards.
QLabel* fort_tail_note(double alpha) {
    auto* l = new QLabel(QCoreApplication::translate("QuantModulePanel",
                                                     "VaR / CVaR: the portfolio return at the %1% tail, per day — "
                                                     "negative = loss.")
                             .arg(alpha * 100.0, 0, 'f', 1));
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; padding:0 0 0 2px;")
                         .arg(ui::colors::TEXT_TERTIARY()));
    return l;
}

// Helper: build the asset-input row used by every Fortitudo sub-tab.
// Lets the user paste a tickers list ("AAPL,MSFT,GOOG") that the Python side
// fetches via yfinance — much simpler than typing thousands of return values.
struct FortAssetInputs {
    QLineEdit* tickers_edit;
    QComboBox* period_combo;
};

FortAssetInputs build_fort_asset_row(const QString& key_prefix, QWidget* parent,
                                     QHash<QString, QLineEdit*>& text_inputs, QHash<QString, QComboBox*>& combo_inputs,
                                     const QString& default_tickers = "AAPL,MSFT,GOOG,TSLA,AMZN") {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    auto* tk = new QLineEdit(row);
    tk->setPlaceholderText(QCoreApplication::translate(
        "QuantModulePanel", "Tickers (comma-separated, >= 2). Returns fetched via Yahoo Finance."));
    tk->setText(default_tickers);
    tk->setStyleSheet(input_ss());
    text_inputs[key_prefix + "_tickers"] = tk;

    auto* per = new QComboBox(row);
    per->addItems({"6mo", "1y", "2y", "3y", "5y", "10y", "max"});
    per->setCurrentText("1y");
    per->setStyleSheet(combo_ss());
    set_text_width(per, 80);
    combo_inputs[key_prefix + "_period"] = per;

    hl->addWidget(tk, 1);
    hl->addWidget(per);
    return {tk, per};
}

// Build a weights table: asset / weight / weight % with color-coded bars.
QTableWidget* fort_weights_table(const QJsonArray& weights, QWidget* parent) {
    auto* table = new QTableWidget(weights.size(), 3, parent);
    table->setHorizontalHeaderLabels({QCoreApplication::translate("QuantModulePanel", "Asset"),
                                      QCoreApplication::translate("QuantModulePanel", "Weight"),
                                      QCoreApplication::translate("QuantModulePanel", "% of Portfolio")});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    // Filled with fixed columns, sized once afterwards (never ResizeToContents while filling).
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    table->setStyleSheet(table_ss());
    table->setMaximumHeight(qMin(weights.size() * row_px(24) + row_px(32), 320));
    for (int i = 0; i < weights.size(); ++i) {
        const auto w = weights[i].toObject();
        const double weight = w.value("weight").toDouble();
        const double pct = w.value("weight_pct").toDouble();
        table->setItem(i, 0, new QTableWidgetItem(w.value("asset").toString()));
        table->setItem(i, 1, num_item(fmt_num_safe(w.value("weight"), 4), weight,
                                      weight >= 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        table->setItem(i, 2, num_item(fmt_pts_safe(w.value("weight_pct"), 2), pct,
                                      pct >= 50.0   ? ui::colors::WARNING()
                                      : pct >= 20.0 ? ui::colors::TEXT_PRIMARY()
                                                    : ui::colors::TEXT_SECONDARY()));
        table->setRowHeight(i, row_px(24));
    }
    // Share of the portfolio as a bar in its cell (signed when the book holds shorts).
    bool any_short = false;
    for (const auto& v : weights)
        any_short = any_short || v.toObject().value("weight").toDouble() < -1e-9;
    table->setItemDelegateForColumn(
        2, new ui::BarCellDelegate(any_short ? ui::BarCellDelegate::Mode::SignedBar : ui::BarCellDelegate::Mode::Bar,
                                   table));
    table->resizeColumnToContents(1);
    table->resizeColumnToContents(2);
    finish_table(table);
    return table;
}

// The weights table with a donut of the allocation beside it (long-only, <= 12 assets).
QWidget* fort_weights_view(const QJsonArray& weights, QWidget* parent) {
    QStringList labels;
    QVector<double> values;
    for (const auto& v : weights) {
        const auto w = v.toObject();
        labels << w.value("asset").toString();
        values << w.value("weight").toDouble();
    }
    return qc_side_by_side(fort_weights_table(weights, parent), qc_weights_donut(labels, values, parent), parent);
}

} // namespace

QWidget* QuantModulePanel::build_fortitudo_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = make_view_tabs(w);

    // ── Portfolio Metrics ────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("portfolio_metrics"), .cost = ViewCost::Fetch, .loading = tr("Downloading prices and computing portfolio metrics…")}, tr("Portfolio Metrics"),
                 [this](QWidget* pm, QVBoxLayout* pml) {

        auto pm_assets = build_fort_asset_row("ft_pm", pm, text_inputs_, combo_inputs_);
        pml->addWidget(build_input_row(tr("Tickers"), pm_assets.tickers_edit, pm));
        pml->addWidget(build_input_row(tr("History Period"), pm_assets.period_combo, pm));

        auto* pm_weights = new QLineEdit(pm);
        pm_weights->setPlaceholderText(tr("Weights (comma-separated, will be normalized to 1.0). Equal-weight if blank."));
        pm_weights->setStyleSheet(input_ss());
        text_inputs_["ft_pm_weights"] = pm_weights;
        pml->addWidget(build_input_row(tr("Portfolio Weights"), pm_weights, pm));

        auto* pm_alpha = make_double_spin(0.001, 0.499, 0.05, 3, "", pm);
        double_inputs_["ft_pm_alpha"] = pm_alpha;
        pml->addWidget(build_input_row(tr("CVaR α (tail probability)"), pm_alpha, pm));

        auto* pm_run = make_run_button(tr("COMPUTE PORTFOLIO METRICS"), pm);
        set_action(pm_run, [this]() {
            const QString tk = text_inputs_["ft_pm_tickers"]->text().trimmed();
            if (tk.isEmpty()) {
                display_error(tr("Enter at least 2 tickers (e.g. AAPL,MSFT,GOOG)."));
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
                    display_error(tr("Weights: '%1' is not numeric.").arg(bad));
                    return;
                }
                params["weights"] = w;
            }
            show_loading(tr("Fetching %1 from yfinance and computing metrics...").arg(tk));
            AIQuantLabService::instance().fort_portfolio_metrics(params);
        });
        pml->addWidget(pm_run);
        pml->addStretch();
    });

    // ── Covariance / Correlation ─────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("covariance"), .cost = ViewCost::Fetch, .loading = tr("Downloading prices and computing the covariance…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Covariance"),
                 [this](QWidget* cv, QVBoxLayout* cvl) {

        auto cv_assets = build_fort_asset_row("ft_cv", cv, text_inputs_, combo_inputs_);
        cvl->addWidget(build_input_row(tr("Tickers"), cv_assets.tickers_edit, cv));
        cvl->addWidget(build_input_row(tr("History Period"), cv_assets.period_combo, cv));

        auto* cv_hint =
            new QLabel(tr("Computes the full covariance matrix, correlation matrix, and per-asset moments "
                          "(mean, vol, skew, kurtosis). Asset count is capped — keep it under 12 for readability."),
                       cv);
        cv_hint->setWordWrap(true);
        cv_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        cvl->addWidget(cv_hint);

        auto* cv_run = make_run_button(tr("COMPUTE COV + CORR + MOMENTS"), cv);
        set_action(cv_run, [this]() {
            const QString tk = text_inputs_["ft_cv_tickers"]->text().trimmed();
            if (tk.isEmpty()) {
                display_error(tr("Enter at least 2 tickers."));
                return;
            }
            QJsonObject params;
            params["tickers"] = tk;
            params["period"] = combo_inputs_["ft_cv_period"]->currentText();
            show_loading(tr("Fetching %1 and computing covariance...").arg(tk));
            AIQuantLabService::instance().fort_covariance_matrix(params);
        });
        cvl->addWidget(cv_run);
        cvl->addStretch();
    });

    // ── Mean-Variance Optimization ───────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("mv_optimize"), .cost = ViewCost::Fetch, .loading = tr("Downloading prices and solving the mean-variance optimisation…")}, tr("MV Optimize"),
                 [this](QWidget* mv, QVBoxLayout* mvl) {

        auto mv_assets = build_fort_asset_row("ft_mv", mv, text_inputs_, combo_inputs_);
        mvl->addWidget(build_input_row(tr("Tickers"), mv_assets.tickers_edit, mv));
        mvl->addWidget(build_input_row(tr("History Period"), mv_assets.period_combo, mv));

        auto* mv_obj = new QComboBox(mv);
        mv_obj->addItems({"min_variance", "max_sharpe", "target_return"});
        mv_obj->setStyleSheet(combo_ss());
        combo_inputs_["ft_mv_objective"] = mv_obj;
        mvl->addWidget(build_input_row(tr("Objective"), mv_obj, mv));

        auto* mv_target = make_double_spin(0, 100, 8.0, 2, "%", mv);
        double_inputs_["ft_mv_target"] = mv_target;
        mvl->addWidget(build_input_row(tr("Target Annualized Return"), mv_target, mv));

        auto* mv_long = new QComboBox(mv);
        mv_long->addItems({"true", "false"});
        mv_long->setStyleSheet(combo_ss());
        combo_inputs_["ft_mv_long_only"] = mv_long;
        mvl->addWidget(build_input_row(tr("Long Only"), mv_long, mv));

        auto* mv_max = make_double_spin(0.0, 1.0, 0.5, 2, "", mv);
        mv_max->setSpecialValueText(tr("none"));
        double_inputs_["ft_mv_max"] = mv_max;
        mvl->addWidget(build_input_row(tr("Max Single-Asset Weight (0 = none)"), mv_max, mv));

        auto* mv_rf = make_double_spin(0, 20, 4.5, 2, "%", mv);
        double_inputs_["ft_mv_rf"] = mv_rf;
        mvl->addWidget(build_input_row(tr("Risk-Free Rate (annual)"), mv_rf, mv));

        auto* mv_run = make_run_button(tr("OPTIMIZE PORTFOLIO"), mv);
        set_action(mv_run, [this]() {
            const QString tk = text_inputs_["ft_mv_tickers"]->text().trimmed();
            if (tk.isEmpty()) {
                display_error(tr("Enter at least 2 tickers."));
                return;
            }
            const QString obj = combo_inputs_["ft_mv_objective"]->currentText();
            QJsonObject params;
            params["tickers"] = tk;
            params["period"] = combo_inputs_["ft_mv_period"]->currentText();
            params["objective"] = obj;
            params["long_only"] = combo_inputs_["ft_mv_long_only"]->currentText() == "true";
            const double maxw = double_inputs_["ft_mv_max"]->value();
            if (maxw > 0.0)
                params["max_weight"] = maxw;
            // Translate annual % rf rate to per-day decimal for daily returns.
            // Geometric (compounded) de-annualisation, matching the target_return
            // conversion below. These two used to disagree: rf was divided by 252
            // while the target was compounded, so the Sharpe objective compared a
            // simple-interest hurdle against compounded portfolio returns
            // (~2% relative bias in the rf term at 4.5% p.a.).
            params["risk_free_rate"] = std::pow(1.0 + double_inputs_["ft_mv_rf"]->value() / 100.0, 1.0 / 252.0) - 1.0;
            if (obj == "target_return") {
                // Translate annualized % to per-day decimal so the optimizer's
                // expected_return constraint matches.
                params["target_return"] =
                    std::pow(1.0 + double_inputs_["ft_mv_target"]->value() / 100.0, 1.0 / 252.0) - 1.0;
            }
            show_loading(tr("Fetching %1 and solving %2...").arg(tk, obj));
            AIQuantLabService::instance().fort_mean_variance_optimize(params);
        });
        mvl->addWidget(mv_run);
        mvl->addStretch();
    });

    // ── Mean-CVaR Optimization ───────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("cvar_optimize"), .cost = ViewCost::Fetch, .loading = tr("Downloading prices and minimising the tail risk…")}, tr("CVaR Optimize"),
                 [this](QWidget* cvopt, QVBoxLayout* cvol) {

        auto cv_opt_assets = build_fort_asset_row("ft_cvopt", cvopt, text_inputs_, combo_inputs_);
        cvol->addWidget(build_input_row(tr("Tickers"), cv_opt_assets.tickers_edit, cvopt));
        cvol->addWidget(build_input_row(tr("History Period"), cv_opt_assets.period_combo, cvopt));

        auto* cvopt_obj = new QComboBox(cvopt);
        cvopt_obj->addItems({"min_cvar", "target_return"});
        cvopt_obj->setStyleSheet(combo_ss());
        combo_inputs_["ft_cvopt_objective"] = cvopt_obj;
        cvol->addWidget(build_input_row(tr("Objective"), cvopt_obj, cvopt));

        auto* cvopt_alpha = make_double_spin(0.001, 0.499, 0.05, 3, "", cvopt);
        double_inputs_["ft_cvopt_alpha"] = cvopt_alpha;
        cvol->addWidget(build_input_row(tr("CVaR α (tail probability)"), cvopt_alpha, cvopt));

        auto* cvopt_target = make_double_spin(0, 100, 8.0, 2, "%", cvopt);
        double_inputs_["ft_cvopt_target"] = cvopt_target;
        cvol->addWidget(build_input_row(tr("Target Annualized Return"), cvopt_target, cvopt));

        auto* cvopt_long = new QComboBox(cvopt);
        cvopt_long->addItems({"true", "false"});
        cvopt_long->setStyleSheet(combo_ss());
        combo_inputs_["ft_cvopt_long_only"] = cvopt_long;
        cvol->addWidget(build_input_row(tr("Long Only"), cvopt_long, cvopt));

        auto* cvopt_max = make_double_spin(0.0, 1.0, 0.5, 2, "", cvopt);
        cvopt_max->setSpecialValueText(tr("none"));
        double_inputs_["ft_cvopt_max"] = cvopt_max;
        cvol->addWidget(build_input_row(tr("Max Single-Asset Weight (0 = none)"), cvopt_max, cvopt));

        auto* cvopt_run = make_run_button(tr("MINIMIZE TAIL RISK"), cvopt);
        set_action(cvopt_run, [this]() {
            const QString tk = text_inputs_["ft_cvopt_tickers"]->text().trimmed();
            if (tk.isEmpty()) {
                display_error(tr("Enter at least 2 tickers."));
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
            if (maxw > 0.0)
                params["max_weight"] = maxw;
            if (obj == "target_return") {
                params["target_return"] =
                    std::pow(1.0 + double_inputs_["ft_cvopt_target"]->value() / 100.0, 1.0 / 252.0) - 1.0;
            }
            show_loading(tr("Fetching %1 and solving %2...").arg(tk, obj));
            AIQuantLabService::instance().fort_mean_cvar_optimize(params);
        });
        cvol->addWidget(cvopt_run);
        cvol->addStretch();
    });

    // ── Efficient Frontier ───────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("efficient_frontier"), .cost = ViewCost::Fetch, .loading = tr("Downloading prices and tracing the frontier…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Efficient Frontier"),
                 [this](QWidget* ef, QVBoxLayout* efl) {

        auto ef_assets = build_fort_asset_row("ft_ef", ef, text_inputs_, combo_inputs_);
        efl->addWidget(build_input_row(tr("Tickers"), ef_assets.tickers_edit, ef));
        efl->addWidget(build_input_row(tr("History Period"), ef_assets.period_combo, ef));

        auto* ef_n = new QSpinBox(ef);
        ef_n->setRange(5, 50);
        ef_n->setValue(20);
        ef_n->setStyleSheet(spinbox_ss());
        int_inputs_["ft_ef_n_points"] = ef_n;
        efl->addWidget(build_input_row(tr("Frontier Points"), ef_n, ef));

        auto* ef_long = new QComboBox(ef);
        ef_long->addItems({"true", "false"});
        ef_long->setStyleSheet(combo_ss());
        combo_inputs_["ft_ef_long_only"] = ef_long;
        efl->addWidget(build_input_row(tr("Long Only"), ef_long, ef));

        auto* ef_max = make_double_spin(0.0, 1.0, 0.5, 2, "", ef);
        ef_max->setSpecialValueText(tr("none"));
        double_inputs_["ft_ef_max"] = ef_max;
        efl->addWidget(build_input_row(tr("Max Single-Asset Weight (0 = none)"), ef_max, ef));

        auto* ef_rf = make_double_spin(0, 20, 4.5, 2, "%", ef);
        double_inputs_["ft_ef_rf"] = ef_rf;
        efl->addWidget(build_input_row(tr("Risk-Free Rate (annual)"), ef_rf, ef));

        auto* ef_run = make_run_button(tr("BUILD EFFICIENT FRONTIER"), ef);
        set_action(ef_run, [this]() {
            const QString tk = text_inputs_["ft_ef_tickers"]->text().trimmed();
            if (tk.isEmpty()) {
                display_error(tr("Enter at least 2 tickers."));
                return;
            }
            QJsonObject params;
            params["tickers"] = tk;
            params["period"] = combo_inputs_["ft_ef_period"]->currentText();
            params["n_points"] = int_inputs_["ft_ef_n_points"]->value();
            params["long_only"] = combo_inputs_["ft_ef_long_only"]->currentText() == "true";
            const double maxw = double_inputs_["ft_ef_max"]->value();
            if (maxw > 0.0)
                params["max_weight"] = maxw;
            // Geometric de-annualisation — same convention as the MV/CVaR tabs.
            params["risk_free_rate"] = std::pow(1.0 + double_inputs_["ft_ef_rf"]->value() / 100.0, 1.0 / 252.0) - 1.0;
            show_loading(
                tr("Fetching %1 and tracing %2-point frontier...").arg(tk).arg(int_inputs_["ft_ef_n_points"]->value()));
            AIQuantLabService::instance().fort_efficient_frontier(params);
        });
        efl->addWidget(ef_run);
        efl->addStretch();
    });

    // ── Exponential Decay Probabilities ──────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("decay_weights"), .cost = ViewCost::Fetch, .loading = tr("Computing the decay weights…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Decay Weights"),
                 [this](QWidget* ed, QVBoxLayout* edl) {

        auto ed_assets = build_fort_asset_row("ft_ed", ed, text_inputs_, combo_inputs_);
        edl->addWidget(build_input_row(tr("Tickers"), ed_assets.tickers_edit, ed));
        edl->addWidget(build_input_row(tr("History Period"), ed_assets.period_combo, ed));

        auto* ed_hl = new QSpinBox(ed);
        ed_hl->setRange(5, 1000);
        ed_hl->setValue(60);
        ed_hl->setSuffix(tr(" obs"));
        ed_hl->setStyleSheet(spinbox_ss());
        int_inputs_["ft_ed_half_life"] = ed_hl;
        edl->addWidget(build_input_row(tr("Half-Life (observations)"), ed_hl, ed));

        auto* ed_hint =
            new QLabel(tr("Builds exponentially-decayed scenario weights so recent observations dominate. "
                          "ESS (Kish effective sample size) tells you how much of the history you're effectively using."),
                       ed);
        ed_hint->setWordWrap(true);
        ed_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        edl->addWidget(ed_hint);

        auto* ed_run = make_run_button(tr("COMPUTE DECAY WEIGHTS"), ed);
        set_action(ed_run, [this]() {
            const QString tk = text_inputs_["ft_ed_tickers"]->text().trimmed();
            if (tk.isEmpty()) {
                display_error(tr("Enter at least 2 tickers (used only for series length)."));
                return;
            }
            QJsonObject params;
            params["tickers"] = tk;
            params["period"] = combo_inputs_["ft_ed_period"]->currentText();
            params["half_life"] = int_inputs_["ft_ed_half_life"]->value();
            show_loading(
                tr("Computing %1-day half-life decay on %2...").arg(int_inputs_["ft_ed_half_life"]->value()).arg(tk));
            AIQuantLabService::instance().fort_exp_decay_probabilities(params);
        });
        edl->addWidget(ed_run);
        edl->addStretch();
    });

    vl->addWidget(tabs);

    add_result_pane(vl);

    return w;
}

// ── FORTITUDO: rich card-based display per command ──────────────────────────

void QuantModulePanel::display_fortitudo_result(const QString& command, const QJsonObject& payload) {
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

    // Section header — same shape as other rich displays
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_assets"))
        header_text += tr("  |  %1 ASSETS").arg(d.value("n_assets").toInt());
    if (d.contains("n_scenarios"))
        header_text += tr("  |  %1 SCENARIOS").arg(d.value("n_scenarios").toInt());
    else if (d.contains("n_observations"))
        header_text += tr("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 2. PORTFOLIO METRICS ─────────────────────────────────────────────────
    if (command == "portfolio_metrics") {
        const QJsonValue exp_ret = d.value("expected_return");
        const QJsonValue sharpe = d.value("sharpe_ratio"); // annualised; null when the volatility is 0
        const QJsonValue ann_ret = d.value("annualized_return");
        const QJsonValue var_pct = d.value("var_pct");   // tail return in %, negative = loss
        const QJsonValue cvar_pct = d.value("cvar_pct");
        const QJsonValue hhi_v = d.value("concentration_hhi");
        const double hhi = hhi_v.toDouble();

        QList<QWidget*> ann = {
            gs_make_card(tr("ANN. RETURN"), fmt_pct_safe(ann_ret, 2), this, gs_pos_neg_color(ann_ret.toDouble())),
            gs_make_card(tr("ANN. VOLATILITY"), fmt_pct_safe(d.value("annualized_volatility"), 2), this),
            gs_make_card(tr("SHARPE (ANN.)"), fmt_num_safe(sharpe, 3), this, fort_sharpe_color(sharpe)),
            gs_make_card(tr("ALPHA (TAIL)"), fmt_pct_safe(d.value("alpha"), 1), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(ann, this));

        QList<QWidget*> daily = {
            gs_make_card(tr("DAILY RETURN"), fmt_pct_safe(exp_ret, 4), this, gs_pos_neg_color(exp_ret.toDouble())),
            gs_make_card(tr("DAILY VOL"), fmt_pct_safe(d.value("volatility"), 4), this),
            gs_make_card(tr("SHARPE (DAILY)"), fmt_num_safe(d.value("sharpe_ratio_daily"), 4), this),
            gs_make_card(tr("VaR (TAIL RETURN)"), fmt_pts_safe(var_pct, 2), this, fort_tail_color(var_pct)),
            gs_make_card(tr("CVaR (TAIL RETURN)"), fmt_pts_safe(cvar_pct, 2), this, fort_tail_color(cvar_pct)),
        };
        results_layout_->addWidget(gs_card_row(daily, this));
        results_layout_->addWidget(fort_tail_note(d.value("alpha").toDouble()));

        QList<QWidget*> conc = {
            gs_make_card(tr("CONCENTRATION (HHI)"), fmt_num_safe(hhi_v, 4), this,
                         !hhi_v.isDouble() ? ui::colors::TEXT_SECONDARY()
                         : hhi > 0.4       ? ui::colors::WARNING()
                                           : ui::colors::POSITIVE()),
            gs_make_card(tr("MAX WEIGHT"), fmt_pct_safe(d.value("max_weight"), 2), this),
            gs_make_card(tr("MIN WEIGHT"), fmt_pct_safe(d.value("min_weight"), 2), this),
            gs_make_card(tr("EFFECTIVE ASSETS"), hhi > 0 ? gs_fmt_num(1.0 / hhi, 2) : QString::fromUtf8("—"), this,
                         ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(conc, this));

        const QJsonArray weights = d.value("weights").toArray();
        if (!weights.isEmpty()) {
            results_layout_->addWidget(fort_weights_view(weights, this));
        }

        status_label_->setText(tr("Sharpe (ann.) %1  |  Ann. Ret %2  |  Ann. Vol %3  |  CVaR %4  |  HHI %5")
                                   .arg(fmt_num_safe(sharpe, 3), fmt_pct_safe(ann_ret, 2),
                                        fmt_pct_safe(d.value("annualized_volatility"), 2), fmt_pts_safe(cvar_pct, 2),
                                        fmt_num_safe(hhi_v, 3)));
        return;
    }

    // ── 3. COVARIANCE / CORRELATION / MOMENTS ───────────────────────────────
    if (command == "covariance_matrix") {
        const QJsonValue avg_corr_v = d.value("avg_off_diag_correlation");
        const double avg_corr = avg_corr_v.toDouble();
        const QJsonValue max_c = d.value("max_off_diag_correlation");
        const QJsonValue min_c = d.value("min_off_diag_correlation");
        QList<QWidget*> top = {
            gs_make_card(tr("ASSETS"), QString::number(d.value("n_assets").toInt()), this),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
            gs_make_card(tr("AVG OFF-DIAG ρ"), fmt_num_safe(avg_corr_v, 4), this,
                         std::abs(avg_corr) > 0.5 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("MAX |ρ|"),
                         max_c.isDouble() && min_c.isDouble()
                             ? gs_fmt_num(std::max(std::abs(max_c.toDouble()), std::abs(min_c.toDouble())), 4)
                             : QString::fromUtf8("—"),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-asset moments table
        const QJsonArray moments = d.value("moments").toArray();
        if (!moments.isEmpty()) {
            const auto first = moments[0].toObject();
            QStringList headers = {tr("Asset")};
            QStringList keys;
            for (auto it = first.begin(); it != first.end(); ++it) {
                if (it.key() == "asset")
                    continue;
                keys << it.key();
                QString h = it.key();
                if (!h.isEmpty())
                    h[0] = h[0].toUpper();
                headers << h;
            }
            auto* mt = new QTableWidget(moments.size(), headers.size(), this);
            mt->setHorizontalHeaderLabels(headers);
            mt->verticalHeader()->setVisible(false);
            mt->setEditTriggers(QAbstractItemView::NoEditTriggers);
            mt->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            mt->setStyleSheet(table_ss());
            mt->setMaximumHeight(qMin(moments.size() * row_px(24) + row_px(32), 280));
            for (int r = 0; r < moments.size(); ++r) {
                const auto row = moments[r].toObject();
                mt->setItem(r, 0, new QTableWidgetItem(row.value("asset").toString()));
                for (int c = 0; c < keys.size(); ++c) {
                    auto* it = new QTableWidgetItem(fmt_num_safe(row.value(keys[c]), 6));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    mt->setItem(r, c + 1, it);
                }
                mt->setRowHeight(r, row_px(22));
            }
            auto* mlbl = new QLabel(tr("PER-ASSET MOMENTS"));
            mlbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 0 0 2px;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(mlbl);
            results_layout_->addWidget(mt);
        }

        // Correlation matrix
        const QJsonArray corr = d.value("correlation_matrix").toArray();
        if (!corr.isEmpty()) {
            const auto first = corr[0].toObject();
            QStringList keys;
            for (auto it = first.begin(); it != first.end(); ++it) {
                if (it.key() == "asset")
                    continue;
                keys << it.key();
            }
            // Every pair on the -1..+1 diverging scale with the value in the cell; rows and
            // columns clustered so correlated groups sit together. Hover reads the pair.
            QStringList row_labels;
            for (int r = 0; r < corr.size(); ++r)
                row_labels << corr[r].toObject().value("asset").toString();
            // A JSON object's keys come back alphabetically; put the columns in the row
            // order so the diagonal is the diagonal (and clustering sees a symmetric matrix).
            QStringList col_order = row_labels;
            bool same_assets = row_labels.size() == keys.size();
            for (const auto& a : row_labels)
                same_assets = same_assets && keys.contains(a);
            if (same_assets)
                keys = col_order;
            QVector<QVector<double>> values;
            for (int r = 0; r < corr.size(); ++r) {
                const auto row = corr[r].toObject();
                QVector<double> vals;
                for (const auto& k : keys) {
                    const QJsonValue cv = row.value(k);
                    vals << (cv.isDouble() ? cv.toDouble() : kQcNaN);
                }
                values << vals;
            }
            auto* heat = new ui::MatrixHeatmap(this);
            heat->set_matrix(row_labels, keys, values);
            heat->set_diverging_scale(-1.0, 1.0, 0.0);
            heat->set_show_values(true);
            heat->set_cluster(row_labels.size() == keys.size() && row_labels.size() > 2);
            heat->set_value_formatter(qc_fmt_num(2));
            heat->setMinimumHeight(row_px(std::clamp(60 + 34 * static_cast<int>(keys.size()), 200, 520)));
            results_layout_->addWidget(qc_caption(tr("CORRELATION MATRIX  ·  %1 ASSETS").arg(keys.size())));
            results_layout_->addWidget(heat);
        }

        status_label_->setText(tr("%1 assets  |  %2 obs  |  avg ρ = %3")
                                   .arg(d.value("n_assets").toInt())
                                   .arg(d.value("n_observations").toInt())
                                   .arg(fmt_num_safe(avg_corr_v, 4)));
        return;
    }

    // ── 4. MV / CVaR OPTIMIZATION (shared display path) ─────────────────────
    if (command == "mean_variance_optimize" || command == "mean_cvar_optimize") {
        const QString objective = d.value("objective").toString();
        const QJsonValue sharpe = d.value("sharpe_ratio"); // annualised
        const QJsonValue ann_ret = d.value("annualized_return");
        const QJsonValue var = d.value("var");   // tail return, negative = loss
        const QJsonValue cvar = d.value("cvar");
        const QJsonValue hhi_v = d.value("concentration_hhi");
        const double hhi = hhi_v.toDouble();

        auto* hdr = new QLabel(tr("OBJECTIVE: %1   |   LONG-ONLY: %2")
                                   .arg(objective.toUpper().replace('_', ' '))
                                   .arg(d.value("long_only").toBool() ? tr("YES") : tr("NO")));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card(tr("ANN. RETURN"), fmt_pct_safe(ann_ret, 2), this, gs_pos_neg_color(ann_ret.toDouble())),
            gs_make_card(tr("ANN. VOLATILITY"), fmt_pct_safe(d.value("annualized_volatility"), 2), this),
            gs_make_card(tr("SHARPE (ANN.)"), fmt_num_safe(sharpe, 3), this, fort_sharpe_color(sharpe)),
            gs_make_card(tr("CONCENTRATION (HHI)"), fmt_num_safe(hhi_v, 4), this,
                         !hhi_v.isDouble() ? ui::colors::TEXT_SECONDARY()
                         : hhi > 0.4       ? ui::colors::WARNING()
                                           : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Both optimisers report the tail of the chosen weights (the CVaR one minimises it).
        QList<QWidget*> tail = {
            gs_make_card(tr("CVaR @ α (TAIL RETURN)"), fmt_pct_safe(cvar, 3), this, fort_tail_color(cvar)),
            gs_make_card(tr("VaR @ α (TAIL RETURN)"), fmt_pct_safe(var, 3), this, fort_tail_color(var)),
            gs_make_card(tr("ALPHA"), fmt_pct_safe(d.value("alpha"), 1), this, ui::colors::INFO()),
            gs_make_card(tr("SHARPE (DAILY)"), fmt_num_safe(d.value("sharpe_ratio_daily"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(tail, this));
        results_layout_->addWidget(fort_tail_note(d.value("alpha").toDouble()));

        QList<QWidget*> extra = {
            gs_make_card(tr("VARIANCE (DAILY)"), fmt_num_safe(d.value("variance"), 6), this),
            gs_make_card(tr("DAILY RETURN"), fmt_pct_safe(d.value("expected_return"), 4), this),
            gs_make_card(tr("MAX WEIGHT"), fmt_pct_safe(d.value("max_weight"), 2), this),
            gs_make_card(tr("MIN WEIGHT"), fmt_pct_safe(d.value("min_weight"), 2), this),
        };
        results_layout_->addWidget(gs_card_row(extra, this));

        const QJsonArray weights = d.value("weights").toArray();
        if (!weights.isEmpty()) {
            auto* wlbl = new QLabel(tr("OPTIMAL WEIGHTS"));
            wlbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 0 0 2px;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(wlbl);
            results_layout_->addWidget(fort_weights_view(weights, this));
        }

        status_label_->setText(tr("%1 — Sharpe (ann.) %2  |  Ann. Ret %3  |  Ann. Vol %4  |  CVaR %5")
                                   .arg(objective)
                                   .arg(fmt_num_safe(sharpe, 3), fmt_pct_safe(ann_ret, 2),
                                        fmt_pct_safe(d.value("annualized_volatility"), 2), fmt_pct_safe(cvar, 3)));
        return;
    }

    // ── 5. EFFICIENT FRONTIER ───────────────────────────────────────────────
    if (command == "efficient_frontier") {
        const int n = d.value("n_points").toInt();
        // max_sharpe_index / best_sharpe are null when no point has a Sharpe ratio
        // (zero volatility everywhere): no point is marked then (-1), not point #1.
        const int max_sharpe_idx = d.value("max_sharpe_index").isDouble() ? d.value("max_sharpe_index").toInt() : -1;
        const int min_var_idx = d.value("min_var_index").isDouble() ? d.value("min_var_index").toInt() : -1;
        const QJsonValue best_sharpe = d.value("best_sharpe"); // annualised

        auto* hdr = new QLabel(tr("FRONTIER: %1 POINTS  |  ASSETS: %2  |  LONG-ONLY: %3")
                                   .arg(n)
                                   .arg(d.value("n_assets").toInt())
                                   .arg(d.value("long_only").toBool() ? tr("YES") : tr("NO")));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        const auto point_label = [](int idx) {
            return idx >= 0 ? tr("Point #%1").arg(idx + 1) : QString::fromUtf8("—");
        };
        QList<QWidget*> top = {
            gs_make_card(tr("BEST SHARPE (ANN.)"), fmt_num_safe(best_sharpe, 3), this, fort_sharpe_color(best_sharpe)),
            gs_make_card(tr("MAX SHARPE @"), point_label(max_sharpe_idx), this, ui::colors::POSITIVE()),
            gs_make_card(tr("MIN VARIANCE @"), point_label(min_var_idx), this, ui::colors::INFO()),
            gs_make_card(tr("DAILY VOL RANGE"),
                         fmt_pct_safe(d.value("min_volatility"), 2) + "  →  " +
                             fmt_pct_safe(d.value("max_volatility"), 2),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Frontier picture (annualised volatility vs annualised return, max-Sharpe and
        // min-variance points marked), then every point in the table.
        const QJsonArray frontier = d.value("frontier").toArray();
        if (!frontier.isEmpty()) {
            QVector<double> vols;
            QVector<double> rets;
            for (const auto& v : frontier) {
                const auto p = v.toObject();
                // NaN (skipped by the chart) where the script could not annualise.
                vols << p.value("annualized_volatility").toDouble(kQcNaN) * 100.0;
                rets << p.value("annualized_return").toDouble(kQcNaN) * 100.0;
            }
            results_layout_->addWidget(qc_caption(tr("EFFICIENT FRONTIER  ·  ANNUALISED  ·  ★ MAX SHARPE  ◆ MIN VARIANCE")));
            results_layout_->addWidget(qc_frontier_chart(vols, rets, max_sharpe_idx, min_var_idx, this,
                                                         tr("Annualised volatility (%)"), tr("Annualised return (%)")));

            auto* table = new QTableWidget(frontier.size(), 5, this);
            table->setHorizontalHeaderLabels(
                {tr("#"), tr("Daily Vol"), tr("Daily Ret"), tr("Sharpe (ann.)"), tr("Ann. Vol → Ret")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            // Filled with fixed columns, sized once afterwards (never ResizeToContents while filling).
            for (int c = 0; c < 4; ++c)
                table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Interactive);
            table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(frontier.size() * row_px(24) + row_px(32), 380));
            for (int i = 0; i < frontier.size(); ++i) {
                const auto p = frontier[i].toObject();
                const double vol = p.value("volatility").toDouble();
                const double ret = p.value("expected_return").toDouble();
                const QJsonValue sh_v = p.value("sharpe_ratio");
                const double sh = sh_v.toDouble();
                const QJsonValue aret_v = p.value("annualized_return");
                const double aret = aret_v.toDouble();

                QString tag = QString::number(i + 1);
                if (i == max_sharpe_idx)
                    tag += " ★";
                if (i == min_var_idx)
                    tag += " ◆";
                auto* ti = num_item(tag, i + 1,
                                    i == max_sharpe_idx  ? QString(ui::colors::POSITIVE())
                                    : i == min_var_idx ? QString(ui::colors::INFO())
                                                       : QString());
                ti->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                table->setItem(i, 0, ti);

                auto add_num = [&](int col, const QString& s, double v, const QString& col_color = {}) {
                    table->setItem(i, col, num_item(s, v, col_color));
                };
                add_num(1, fmt_pct_safe(p.value("volatility"), 4), vol);
                add_num(2, fmt_pct_safe(p.value("expected_return"), 4), ret, gs_pos_neg_color(ret));
                add_num(3, fmt_num_safe(sh_v, 3), sh_v.isDouble() ? sh : -std::numeric_limits<double>::infinity(),
                        fort_sharpe_color(sh_v));
                add_num(4,
                        fmt_pct_safe(p.value("annualized_volatility"), 2) + "  →  " + fmt_pct_safe(aret_v, 2),
                        aret_v.isDouble() ? aret : -std::numeric_limits<double>::infinity(), gs_pos_neg_color(aret));
                table->setRowHeight(i, row_px(24));
            }
            for (int c = 0; c < 4; ++c)
                table->resizeColumnToContents(c);
            finish_table(table);
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
                    auto* lbl = new QLabel(tr("MAX-SHARPE PORTFOLIO WEIGHTS  (★ Point #%1)").arg(max_sharpe_idx + 1));
                    lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:6px 0 0 2px;")
                                           .arg(ui::colors::POSITIVE()));
                    results_layout_->addWidget(lbl);
                    results_layout_->addWidget(fort_weights_view(with_pct, this));
                }
            }
        }

        status_label_->setText(tr("%1 frontier points  |  best Sharpe (ann.) %2 @ %3")
                                   .arg(n)
                                   .arg(fmt_num_safe(best_sharpe, 3), point_label(max_sharpe_idx)));
        return;
    }

    // ── 6. EXPONENTIAL DECAY PROBABILITIES ──────────────────────────────────
    if (command == "exp_decay_probabilities") {
        const int hl = d.value("half_life").toInt();
        const QJsonValue ess = d.value("effective_sample_size");
        const QJsonValue ess_pct_v = d.value("ess_pct_of_n");
        const double ess_pct = ess_pct_v.toDouble();
        const QJsonValue ratio = d.value("weight_ratio_last_to_first"); // null when the first weight is 0

        QList<QWidget*> top = {
            gs_make_card(tr("HALF-LIFE"), tr("%1 obs").arg(hl), this, ui::colors::INFO()),
            gs_make_card(tr("EFFECTIVE SAMPLE"), fmt_num_safe(ess, 1), this,
                         !ess_pct_v.isDouble() ? ui::colors::TEXT_SECONDARY()
                         : ess_pct > 50        ? ui::colors::POSITIVE()
                         : ess_pct > 25        ? ui::colors::WARNING()
                                               : ui::colors::NEGATIVE()),
            gs_make_card(tr("ESS % OF N"), fmt_pts_safe(ess_pct_v, 1), this,
                         !ess_pct_v.isDouble() ? ui::colors::TEXT_SECONDARY()
                         : ess_pct > 50        ? ui::colors::POSITIVE()
                                               : ui::colors::WARNING()),
            gs_make_card(tr("LAST/FIRST RATIO"),
                         ratio.isDouble() ? QString("%1×").arg(ratio.toDouble(), 0, 'f', 2) : QString::fromUtf8("—"),
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> bounds = {
            gs_make_card(tr("FIRST WEIGHT"), fmt_num_safe(d.value("first_weight"), 6), this),
            gs_make_card(tr("LAST WEIGHT"), fmt_num_safe(d.value("last_weight"), 6), this, ui::colors::POSITIVE()),
            gs_make_card(tr("MIN WEIGHT"), fmt_num_safe(d.value("min_weight"), 6), this),
            gs_make_card(tr("MAX WEIGHT"), fmt_num_safe(d.value("max_weight"), 6), this),
        };
        results_layout_->addWidget(gs_card_row(bounds, this));

        // Sampled weights curve
        const QJsonArray weights = d.value("weights").toArray();
        if (!weights.isEmpty()) {
            auto* table = new QTableWidget(weights.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Date / Index"), tr("Weight")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(weights.size() * row_px(22) + row_px(32), 360));
            for (int i = 0; i < weights.size(); ++i) {
                const auto o = weights[i].toObject();
                const QString label =
                    o.contains("date") ? o.value("date").toString() : QString::number(o.value("index").toInt());
                table->setItem(i, 0, new QTableWidgetItem(label));
                auto* wi = new QTableWidgetItem(fmt_num_safe(o.value("weight"), 6));
                wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                // Color gradient: bigger weight → more saturated
                const double w = o.value("weight").toDouble();
                if (w > d.value("max_weight").toDouble() * 0.7) {
                    wi->setForeground(QColor(ui::colors::POSITIVE()));
                } else if (w < d.value("max_weight").toDouble() * 0.1) {
                    wi->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
                }
                table->setItem(i, 1, wi);
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        // (The format used to read "(%3%%)", which QString::arg leaves as a doubled "%%".)
        status_label_->setText(tr("Half-life %1 obs  |  ESS %2 (%3 of n)  |  last/first %4")
                                   .arg(hl)
                                   .arg(fmt_num_safe(ess, 1), fmt_pts_safe(ess_pct_v, 1),
                                        ratio.isDouble() ? QString("%1×").arg(ratio.toDouble(), 0, 'f', 1)
                                                         : QString::fromUtf8("—")));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
