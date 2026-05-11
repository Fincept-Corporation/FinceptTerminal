// src/screens/ai_quant_lab/QuantModulePanel_CFA.cpp
//
// CFA Quant panel — 8 analysis types. Extracted from QuantModulePanel.cpp to
// keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QHash>
#include <QHBoxLayout>
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

// ═══════════════════════════════════════════════════════════════════════════════
// CFA QUANT PANEL — 8 analysis types
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_cfa_quant_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // ── Section header ──────────────────────────────────────────────────────
    auto* data_lbl = new QLabel("DATA INPUT", w);
    data_lbl->setStyleSheet(QString("color:%1; font-weight:700; font-family:%2;"
                                    "letter-spacing:1px;")
                                .arg(module_.color.name())
                                .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(data_lbl);

    // ── Symbol / data input ─────────────────────────────────────────────────
    auto* symbol = new QLineEdit(w);
    symbol->setPlaceholderText("Ticker (AAPL, ^GSPC, BTC-USD) or comma-separated values");
    symbol->setStyleSheet(input_ss());
    symbol->setText("AAPL");
    text_inputs_["cfa_symbol"] = symbol;
    vl->addWidget(build_input_row("Symbol / Data", symbol, w));

    // Period / interval / column for ticker fetches
    auto* period = new QComboBox(w);
    period->addItems({"1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "max"});
    period->setCurrentText("1y");
    period->setStyleSheet(combo_ss());
    combo_inputs_["cfa_period"] = period;
    vl->addWidget(build_input_row("Period (ticker only)", period, w));

    auto* interval = new QComboBox(w);
    interval->addItems({"1d", "1wk", "1mo"});
    interval->setStyleSheet(combo_ss());
    combo_inputs_["cfa_interval"] = interval;
    vl->addWidget(build_input_row("Interval (ticker only)", interval, w));

    // ── Analysis selector ───────────────────────────────────────────────────
    auto* analysis_type = new QComboBox(w);
    const QStringList analyses = {
        "trend_analysis", "stationarity_test", "arima_model", "forecasting",
        "supervised_learning", "unsupervised_learning", "model_evaluation",
        "resampling_methods", "sampling_techniques", "central_limit_theorem",
        "sampling_error_analysis", "validate_data",
    };
    analysis_type->addItems(analyses);
    analysis_type->setStyleSheet(combo_ss());
    combo_inputs_["cfa_analysis"] = analysis_type;
    vl->addWidget(build_input_row("Analysis Type", analysis_type, w));

    // ── Per-analysis parameter stack ────────────────────────────────────────
    auto* params_lbl = new QLabel("ANALYSIS PARAMETERS", w);
    params_lbl->setStyleSheet(QString("color:%1; font-weight:700; font-family:%2;"
                                      "letter-spacing:1px; margin-top:8px;")
                                  .arg(module_.color.name())
                                  .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(params_lbl);

    auto* stack = new QStackedWidget(w);
    stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto add_page = [&](std::initializer_list<QWidget*> rows) {
        auto* page = new QWidget(stack);
        auto* pv = new QVBoxLayout(page);
        pv->setContentsMargins(0, 0, 0, 0);
        pv->setSpacing(6);
        for (auto* r : rows)
            pv->addWidget(r);
        pv->addStretch();
        stack->addWidget(page);
        return page;
    };

    // 0: trend_analysis — trend_type
    {
        auto* t = new QComboBox(stack);
        t->addItems({"linear", "log_linear"});
        t->setStyleSheet(combo_ss());
        combo_inputs_["cfa_trend_type"] = t;
        add_page({build_input_row("Trend Type", t, stack)});
    }
    // 1: stationarity_test — test_type
    {
        auto* t = new QComboBox(stack);
        t->addItems({"adf", "kpss"});
        t->setStyleSheet(combo_ss());
        combo_inputs_["cfa_stationarity_test"] = t;
        add_page({build_input_row("Test Type", t, stack)});
    }
    // 2: arima_model — order p,d,q + optional seasonal
    {
        auto* p = new QSpinBox(stack);
        p->setRange(0, 10);
        p->setValue(1);
        p->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_arima_p"] = p;
        auto* d = new QSpinBox(stack);
        d->setRange(0, 3);
        d->setValue(0);
        d->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_arima_d"] = d;
        auto* q = new QSpinBox(stack);
        q->setRange(0, 10);
        q->setValue(1);
        q->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_arima_q"] = q;
        add_page({
            build_input_row("AR order (p)", p, stack),
            build_input_row("Differencing (d)", d, stack),
            build_input_row("MA order (q)", q, stack),
        });
    }
    // 3: forecasting — horizon, method, train_size
    {
        auto* h = new QSpinBox(stack);
        h->setRange(1, 252);
        h->setValue(5);
        h->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_forecast_horizon"] = h;
        auto* m = new QComboBox(stack);
        m->addItems({"simple_exponential", "holt", "holt_winters", "arima", "naive", "drift"});
        m->setStyleSheet(combo_ss());
        combo_inputs_["cfa_forecast_method"] = m;
        auto* ts = make_double_spin(0.5, 0.95, 0.8, 2, "", stack);
        double_inputs_["cfa_forecast_train_size"] = ts;
        add_page({
            build_input_row("Horizon (steps)", h, stack),
            build_input_row("Method", m, stack),
            build_input_row("Train Size", ts, stack),
        });
    }
    // 4: supervised_learning — n_lags, problem_type, algorithms
    {
        auto* lags = new QSpinBox(stack);
        lags->setRange(1, 50);
        lags->setValue(5);
        lags->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_sup_lags"] = lags;
        auto* pt = new QComboBox(stack);
        pt->addItems({"regression", "classification"});
        pt->setStyleSheet(combo_ss());
        combo_inputs_["cfa_sup_problem"] = pt;
        auto* algos = new QLineEdit(stack);
        algos->setText("ridge,random_forest");
        algos->setPlaceholderText("comma-separated: ridge,lasso,random_forest,svr,knn");
        algos->setStyleSheet(input_ss());
        text_inputs_["cfa_sup_algorithms"] = algos;
        add_page({
            build_input_row("Lag features (n_lags)", lags, stack),
            build_input_row("Problem Type", pt, stack),
            build_input_row("Algorithms", algos, stack),
        });
    }
    // 5: unsupervised_learning — n_lags, methods
    {
        auto* lags = new QSpinBox(stack);
        lags->setRange(1, 50);
        lags->setValue(5);
        lags->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_unsup_lags"] = lags;
        auto* methods = new QLineEdit(stack);
        methods->setText("pca,kmeans");
        methods->setPlaceholderText("comma-separated: pca,kmeans,agglomerative");
        methods->setStyleSheet(input_ss());
        text_inputs_["cfa_unsup_methods"] = methods;
        add_page({
            build_input_row("Lag features (n_lags)", lags, stack),
            build_input_row("Methods", methods, stack),
        });
    }
    // 6: model_evaluation — n_lags, model_type, cv_folds
    {
        auto* lags = new QSpinBox(stack);
        lags->setRange(1, 50);
        lags->setValue(5);
        lags->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_eval_lags"] = lags;
        auto* mt = new QComboBox(stack);
        mt->addItems({"random_forest", "ridge", "lasso", "svr", "knn", "decision_tree"});
        mt->setStyleSheet(combo_ss());
        combo_inputs_["cfa_eval_model"] = mt;
        auto* cv = new QSpinBox(stack);
        cv->setRange(2, 20);
        cv->setValue(5);
        cv->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_eval_cv"] = cv;
        add_page({
            build_input_row("Lag features (n_lags)", lags, stack),
            build_input_row("Model Type", mt, stack),
            build_input_row("CV Folds", cv, stack),
        });
    }
    // 7: resampling_methods — methods, n_resamples
    {
        auto* m = new QLineEdit(stack);
        m->setText("bootstrap,jackknife");
        m->setPlaceholderText("comma-separated: bootstrap,jackknife,permutation");
        m->setStyleSheet(input_ss());
        text_inputs_["cfa_resample_methods"] = m;
        auto* n = new QSpinBox(stack);
        n->setRange(50, 100000);
        n->setValue(1000);
        n->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_resample_n"] = n;
        add_page({
            build_input_row("Methods", m, stack),
            build_input_row("Resamples", n, stack),
        });
    }
    // 8: sampling_techniques — sample_size, methods (population uses Symbol/Data)
    {
        auto* size = new QSpinBox(stack);
        size->setRange(10, 100000);
        size->setValue(100);
        size->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_sampling_size"] = size;
        auto* m = new QLineEdit(stack);
        m->setText("simple,stratified,systematic");
        m->setStyleSheet(input_ss());
        text_inputs_["cfa_sampling_methods"] = m;
        add_page({
            build_input_row("Sample Size", size, stack),
            build_input_row("Methods", m, stack),
        });
    }
    // 9: central_limit_theorem — distribution, sample_sizes, n_samples
    {
        auto* dist = new QComboBox(stack);
        dist->addItems({"exponential", "uniform", "normal", "poisson", "binomial"});
        dist->setStyleSheet(combo_ss());
        combo_inputs_["cfa_clt_dist"] = dist;
        auto* sizes = new QLineEdit(stack);
        sizes->setText("5,10,30,100");
        sizes->setStyleSheet(input_ss());
        text_inputs_["cfa_clt_sizes"] = sizes;
        auto* n = new QSpinBox(stack);
        n->setRange(100, 100000);
        n->setValue(1000);
        n->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_clt_n"] = n;
        add_page({
            build_input_row("Distribution", dist, stack),
            build_input_row("Sample Sizes", sizes, stack),
            build_input_row("Samples per size", n, stack),
        });
    }
    // 10: sampling_error_analysis — pop_mean, pop_std, sample_sizes, conf, n_sims
    {
        auto* mean = make_double_spin(-1e6, 1e6, 0.0, 4, "", stack);
        double_inputs_["cfa_serr_mean"] = mean;
        auto* sd = make_double_spin(0.0001, 1e6, 1.0, 4, "", stack);
        double_inputs_["cfa_serr_std"] = sd;
        auto* sizes = new QLineEdit(stack);
        sizes->setText("10,30,50,100,500");
        sizes->setStyleSheet(input_ss());
        text_inputs_["cfa_serr_sizes"] = sizes;
        auto* conf = make_double_spin(0.5, 0.999, 0.95, 3, "", stack);
        double_inputs_["cfa_serr_conf"] = conf;
        auto* sims = new QSpinBox(stack);
        sims->setRange(100, 100000);
        sims->setValue(1000);
        sims->setStyleSheet(spinbox_ss());
        int_inputs_["cfa_serr_sims"] = sims;
        add_page({
            build_input_row("Population Mean", mean, stack),
            build_input_row("Population Std", sd, stack),
            build_input_row("Sample Sizes", sizes, stack),
            build_input_row("Confidence Level", conf, stack),
            build_input_row("Simulations", sims, stack),
        });
    }
    // 11: validate_data — data_type, data_name
    {
        auto* dt = new QComboBox(stack);
        dt->addItems({"general", "returns", "prices", "rates"});
        dt->setStyleSheet(combo_ss());
        combo_inputs_["cfa_validate_type"] = dt;
        auto* dn = new QLineEdit(stack);
        dn->setText("data");
        dn->setStyleSheet(input_ss());
        text_inputs_["cfa_validate_name"] = dn;
        add_page({
            build_input_row("Data Type", dt, stack),
            build_input_row("Data Name", dn, stack),
        });
    }

    vl->addWidget(stack);

    // Switch stack page when analysis combo changes
    connect(analysis_type, qOverload<int>(&QComboBox::currentIndexChanged), stack,
            &QStackedWidget::setCurrentIndex);

    // ── Run button ──────────────────────────────────────────────────────────
    auto* run = make_run_button("RUN ANALYSIS", w);
    connect(run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running...");
        const QString cmd = combo_inputs_["cfa_analysis"]->currentText();
        const QString raw = text_inputs_["cfa_symbol"]->text().trimmed();

        QJsonObject params;
        params["period"] = combo_inputs_["cfa_period"]->currentText();
        params["interval"] = combo_inputs_["cfa_interval"]->currentText();

        // Sample-error analysis is the only command that doesn't read from "data".
        // Everything else reads from "data" (or "population" for sampling_techniques).
        const bool needs_data = (cmd != "sampling_error_analysis" &&
                                 cmd != "central_limit_theorem");
        if (needs_data && raw.isEmpty()) {
            display_error("Please enter a ticker symbol or comma-separated values");
            status_label_->setText("Error");
            return;
        }
        if (cmd == "sampling_techniques")
            params["population"] = raw;
        else if (needs_data)
            params["data"] = raw;

        auto split_csv = [](const QString& s) {
            QJsonArray arr;
            for (const auto& part : s.split(',', Qt::SkipEmptyParts))
                arr.append(part.trimmed());
            return arr;
        };
        auto split_csv_int = [](const QString& s) {
            QJsonArray arr;
            for (const auto& part : s.split(',', Qt::SkipEmptyParts)) {
                bool ok = false;
                int v = part.trimmed().toInt(&ok);
                if (ok) arr.append(v);
            }
            return arr;
        };

        if (cmd == "trend_analysis") {
            params["trend_type"] = combo_inputs_["cfa_trend_type"]->currentText();
        } else if (cmd == "stationarity_test") {
            params["test_type"] = combo_inputs_["cfa_stationarity_test"]->currentText();
        } else if (cmd == "arima_model") {
            QJsonArray order;
            order.append(int_inputs_["cfa_arima_p"]->value());
            order.append(int_inputs_["cfa_arima_d"]->value());
            order.append(int_inputs_["cfa_arima_q"]->value());
            params["order"] = order;
        } else if (cmd == "forecasting") {
            params["horizon"] = int_inputs_["cfa_forecast_horizon"]->value();
            params["method"] = combo_inputs_["cfa_forecast_method"]->currentText();
            params["train_size"] = double_inputs_["cfa_forecast_train_size"]->value();
        } else if (cmd == "supervised_learning") {
            params["n_lags"] = int_inputs_["cfa_sup_lags"]->value();
            params["problem_type"] = combo_inputs_["cfa_sup_problem"]->currentText();
            params["algorithms"] = split_csv(text_inputs_["cfa_sup_algorithms"]->text());
        } else if (cmd == "unsupervised_learning") {
            params["n_lags"] = int_inputs_["cfa_unsup_lags"]->value();
            params["methods"] = split_csv(text_inputs_["cfa_unsup_methods"]->text());
        } else if (cmd == "model_evaluation") {
            params["n_lags"] = int_inputs_["cfa_eval_lags"]->value();
            params["model_type"] = combo_inputs_["cfa_eval_model"]->currentText();
            params["cv_folds"] = int_inputs_["cfa_eval_cv"]->value();
        } else if (cmd == "resampling_methods") {
            params["methods"] = split_csv(text_inputs_["cfa_resample_methods"]->text());
            params["n_resamples"] = int_inputs_["cfa_resample_n"]->value();
        } else if (cmd == "sampling_techniques") {
            params["sample_size"] = int_inputs_["cfa_sampling_size"]->value();
            params["sampling_methods"] = split_csv(text_inputs_["cfa_sampling_methods"]->text());
        } else if (cmd == "central_limit_theorem") {
            params["population_dist"] = combo_inputs_["cfa_clt_dist"]->currentText();
            params["sample_sizes"] = split_csv_int(text_inputs_["cfa_clt_sizes"]->text());
            params["n_samples"] = int_inputs_["cfa_clt_n"]->value();
        } else if (cmd == "sampling_error_analysis") {
            params["population_mean"] = double_inputs_["cfa_serr_mean"]->value();
            params["population_std"] = double_inputs_["cfa_serr_std"]->value();
            params["sample_sizes"] = split_csv_int(text_inputs_["cfa_serr_sizes"]->text());
            params["confidence_level"] = double_inputs_["cfa_serr_conf"]->value();
            params["n_simulations"] = int_inputs_["cfa_serr_sims"]->value();
        } else if (cmd == "validate_data") {
            params["data_type"] = combo_inputs_["cfa_validate_type"]->currentText();
            params["data_name"] = text_inputs_["cfa_validate_name"]->text();
        }

        AIQuantLabService::instance().run_module("cfa_quant", cmd, params);
    });
    vl->addWidget(run);

    // ── Results container ───────────────────────────────────────────────────
    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ── CFA Quant Result Display ───────────────────────────────────────────────
//
// CFA Python returns a uniform envelope:
//   { success, analysis_type,
//     result: { value, method, parameters, metadata, calculation_time, timestamp, calculator } }
//
// validate_data is the exception: result is a DataQualityReport.to_dict()
// (quality_score, issues, warnings, statistics, recommendations).
//
// Each command has its own card layout — same visual language as Quant
// Reporting (gs_make_card / gs_card_row / gs_section_header). The flattened
// 2-col fallback only runs when the shape doesn't match a known command.

namespace {

// Fallback flattener — only used for unknown commands.
void cfa_flatten_scalars(const QJsonObject& obj, const QString& prefix, QStringList& keys,
                         QHash<QString, QJsonValue>& out) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const auto& v = it.value();
        const QString key = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
        if (v.isObject()) {
            if (key.endsWith(".data") || key.endsWith(".fitted_values") ||
                key.endsWith(".residuals") || key.endsWith(".model_summary") ||
                key.endsWith(".predictions") || key.endsWith(".sample_means") ||
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

QString fmt_num_safe(const QJsonValue& v, int decimals = 4) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals);
}

[[maybe_unused]] QString fmt_pct(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble() * 100.0, 'f', decimals) + "%";
}

[[maybe_unused]] QString fmt_bool(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return v.toBool() ? QStringLiteral("YES") : QStringLiteral("NO");
}

} // namespace

void QuantModulePanel::display_cfa_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    if (!payload.value("success").toBool()) {
        display_error(payload.value("error").toString("Unknown error"));
        return;
    }

    const QJsonObject result = payload.value("result").toObject();
    const QString method = result.value("method").toString();
    const double calc_time = result.value("calculation_time").toDouble();
    const QString analysis_type = payload.value("analysis_type").toString(command);
    const QString accent = module_.color.name();

    // ── Section header ──────────────────────────────────────────────────────
    QString header_text = analysis_type.toUpper();
    header_text.replace('_', ' ');
    if (!method.isEmpty())
        header_text += QString("  |  %1").arg(method.toUpper());
    if (calc_time > 0.0)
        header_text += QString("  |  %1 ms").arg(QString::number(calc_time * 1000.0, 'f', 1));
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
            if (direction == "UPWARD") return ui::colors::POSITIVE();
            if (direction == "DOWNWARD") return ui::colors::NEGATIVE();
            return ui::colors::TEXT_PRIMARY();
        };

        QList<QWidget*> top = {
            gs_make_card("DIRECTION", direction.isEmpty() ? "—" : direction, this, dir_color()),
            gs_make_card("SLOPE", gs_fmt_num(slope, 6), this, gs_pos_neg_color(slope)),
            gs_make_card("INTERCEPT", gs_fmt_num(intercept, 4), this),
            gs_make_card("R²", gs_fmt_num(r2, 4), this,
                         r2 >= 0.7 ? ui::colors::POSITIVE()
                                   : r2 >= 0.3 ? ui::colors::WARNING()
                                               : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> stats = {
            gs_make_card("t-STATISTIC", gs_fmt_num(tstat, 3), this),
            gs_make_card("p-VALUE", gs_fmt_num(pval, 4), this,
                         pval < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("SIGNIFICANT", significant ? "YES" : "NO", this,
                         significant ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("OBSERVATIONS",
                         QString::number(result.value("parameters").toObject()
                                             .value("data").toArray().size()),
                         this),
        };
        results_layout_->addWidget(gs_card_row(stats, this));
        status_label_->setText(QString("Trend %1  slope=%2  R²=%3  p=%4")
                                   .arg(direction).arg(slope, 0, 'g', 4)
                                   .arg(r2, 0, 'f', 3).arg(pval, 0, 'f', 4));
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
            gs_make_card("TEST", test.isEmpty() ? "ADF" : test, this),
            gs_make_card("TEST STAT", gs_fmt_num(tstat, 4), this),
            gs_make_card("p-VALUE", gs_fmt_num(pval, 4), this,
                         pval < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("STATIONARY", stationary ? "YES" : "NO", this,
                         stationary ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Critical values row
        const QJsonObject crit = value_obj.value("critical_values").toObject();
        if (!crit.isEmpty()) {
            QList<QWidget*> crit_cards;
            for (auto it = crit.begin(); it != crit.end(); ++it) {
                crit_cards << gs_make_card("CRIT " + it.key(), gs_fmt_num(it.value().toDouble(), 4), this);
            }
            results_layout_->addWidget(gs_card_row(crit_cards, this));
        }

        if (!h0.isEmpty()) {
            auto* note = new QLabel("H₀: " + h0);
            note->setStyleSheet(QString("color:%1; font-size:10px; padding:6px 10px; background:%2; border-left:3px solid %3;")
                                    .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            note->setWordWrap(true);
            results_layout_->addWidget(note);
        }
        status_label_->setText(QString("%1: stat=%2  p=%3  → %4")
                                   .arg(test).arg(tstat, 0, 'f', 3).arg(pval, 0, 'f', 4)
                                   .arg(stationary ? "stationary" : "non-stationary"));
        rendered = true;
    }

    // ── 3. ARIMA MODEL ──────────────────────────────────────────────────────
    else if (command == "arima_model") {
        const QJsonArray order = result.value("parameters").toObject().value("order").toArray();
        const QString order_str = order.size() == 3
            ? QString("(%1, %2, %3)").arg(order[0].toInt()).arg(order[1].toInt()).arg(order[2].toInt())
            : QStringLiteral("—");
        const double aic = meta.value("aic").toDouble();
        const double bic = meta.value("bic").toDouble();
        const double llf = meta.value("log_likelihood").toDouble();
        const double lb_p = meta.value("ljung_box_p_value").toDouble();
        const bool autocorr = meta.value("residuals_autocorrelated").toBool();

        QList<QWidget*> top = {
            gs_make_card("ORDER (p,d,q)", order_str, this),
            gs_make_card("AIC", gs_fmt_num(aic, 2), this),
            gs_make_card("BIC", gs_fmt_num(bic, 2), this),
            gs_make_card("LOG-LIKELIHOOD", gs_fmt_num(llf, 2), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> diag = {
            gs_make_card("LJUNG-BOX p", gs_fmt_num(lb_p, 4), this,
                         lb_p > 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("RESID. AUTOCORR.", autocorr ? "YES" : "NO", this,
                         autocorr ? ui::colors::WARNING() : ui::colors::POSITIVE()),
            gs_make_card("FITTED POINTS",
                         QString::number(value_obj.value("fitted_values").toArray().size()), this),
            gs_make_card("RESIDUALS",
                         QString::number(value_obj.value("residuals").toArray().size()), this),
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
            auto* coef_lbl = new QLabel("Coefficients: " + coef_lines.join("   "));
            coef_lbl->setWordWrap(true);
            coef_lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                            "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(coef_lbl);
        }
        status_label_->setText(QString("ARIMA%1  AIC=%2  Ljung-Box p=%3")
                                   .arg(order_str).arg(aic, 0, 'f', 2).arg(lb_p, 0, 'f', 3));
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

        QList<QWidget*> top = {
            gs_make_card("METHOD", fmethod.isEmpty() ? "—" : fmethod, this),
            gs_make_card("HORIZON", QString::number(horizon), this),
            gs_make_card("TRAIN", QString::number(train_n) + " obs", this),
            gs_make_card("TEST", QString::number(test_n) + " obs", this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> err = {
            gs_make_card("MAE", fmt_num_safe(mae, 4), this,
                         mae.isNull() ? ui::colors::TEXT_TERTIARY() : ui::colors::WARNING()),
            gs_make_card("RMSE", fmt_num_safe(rmse, 4), this,
                         rmse.isNull() ? ui::colors::TEXT_TERTIARY() : ui::colors::WARNING()),
            gs_make_card("MAPE", mape.isNull() ? QStringLiteral("—")
                                               : QString::number(mape.toDouble(), 'f', 2) + "%", this),
            gs_make_card("FCST POINTS",
                         QString::number(value_raw.toArray().size()), this),
        };
        results_layout_->addWidget(gs_card_row(err, this));

        // Forecast values table
        if (value_raw.isArray()) {
            const auto arr = value_raw.toArray();
            auto* table = new QTableWidget(arr.size(), 2, this);
            table->setHorizontalHeaderLabels({"Step", "Forecast"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(arr.size() * 24 + 32, 280));
            for (int i = 0; i < arr.size(); ++i) {
                table->setItem(i, 0, new QTableWidgetItem(QString("t+%1").arg(i + 1)));
                auto* it = new QTableWidgetItem(QString::number(arr[i].toDouble(), 'f', 6));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 1, it);
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 forecast (%2 steps)  MAE=%3")
                                   .arg(fmethod).arg(horizon).arg(fmt_num_safe(mae, 4)));
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

        QList<QWidget*> top = {
            gs_make_card("PROBLEM", problem.toUpper(), this),
            gs_make_card("BEST ALGORITHM", best.isEmpty() ? "—" : best, this, ui::colors::POSITIVE()),
            gs_make_card("FEATURES", QString::number(n_feat), this),
            gs_make_card("TRAIN / TEST",
                         QString("%1 / %2").arg(train_n).arg(test_n), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-algorithm metrics table
        QStringList headers = is_reg
            ? QStringList{"Algorithm", "MSE", "RMSE", "R²"}
            : QStringList{"Algorithm", "Accuracy", "Precision", "Recall"};
        QStringList score_keys = is_reg
            ? QStringList{"mse", "rmse", "r2_score"}
            : QStringList{"accuracy", "precision", "recall"};

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
            table->setMaximumHeight(qMin(rows.size() * 26 + 32, 320));
            for (int r = 0; r < rows.size(); ++r) {
                const auto& algo = rows[r].first;
                const auto& metrics = rows[r].second;
                auto* name_it = new QTableWidgetItem(algo.toUpper());
                if (algo.toUpper() == best)
                    name_it->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(r, 0, name_it);

                if (metrics.contains("error")) {
                    auto* err_it = new QTableWidgetItem("ERR: " + metrics.value("error").toString());
                    err_it->setForeground(QColor(ui::colors::NEGATIVE()));
                    table->setItem(r, 1, err_it);
                    table->setSpan(r, 1, 1, headers.size() - 1);
                } else {
                    for (int c = 0; c < score_keys.size(); ++c) {
                        const double v = metrics.value(score_keys[c]).toDouble();
                        auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                        it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        // R² and accuracy: green if good
                        if ((score_keys[c] == "r2_score" || score_keys[c] == "accuracy") && v > 0.5)
                            it->setForeground(QColor(ui::colors::POSITIVE()));
                        else if ((score_keys[c] == "r2_score" || score_keys[c] == "accuracy") && v < 0)
                            it->setForeground(QColor(ui::colors::NEGATIVE()));
                        table->setItem(r, c + 1, it);
                    }
                }
                table->setRowHeight(r, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Supervised %1: best=%2 (%3 algos)")
                                   .arg(problem).arg(best).arg(rows.size()));
        rendered = true;
    }

    // ── 6. UNSUPERVISED LEARNING ────────────────────────────────────────────
    else if (command == "unsupervised_learning") {
        const int n_samples = meta.value("n_samples").toInt();
        const int n_feat = meta.value("n_features").toInt();

        QList<QWidget*> top = {
            gs_make_card("SAMPLES", QString::number(n_samples), this),
            gs_make_card("FEATURES", QString::number(n_feat), this),
            gs_make_card("METHODS", QString::number(value_obj.size()), this),
            gs_make_card("DATA SCALED", meta.value("data_scaled").toBool() ? "YES" : "NO", this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // PCA section
        if (value_obj.contains("pca") && value_obj.value("pca").isObject()) {
            const QJsonObject pca = value_obj.value("pca").toObject();
            results_layout_->addWidget(gs_section_header("PCA", accent));
            const QJsonArray evr = pca.value("explained_variance_ratio").toArray();
            const QJsonArray cum = pca.value("cumulative_variance").toArray();
            const int n95 = pca.value("components_for_95_variance").toInt();
            const double total = evr.isEmpty() ? 0.0 : cum[cum.size() - 1].toDouble();

            QList<QWidget*> pca_cards = {
                gs_make_card("COMPONENTS", QString::number(evr.size()), this),
                gs_make_card("FOR 95% VAR", QString::number(n95), this, ui::colors::POSITIVE()),
                gs_make_card("TOP COMPONENT", evr.isEmpty() ? "—"
                                                : QString::number(evr[0].toDouble() * 100, 'f', 2) + "%", this),
                gs_make_card("CUM. EXPLAINED", QString::number(total * 100, 'f', 2) + "%", this),
            };
            results_layout_->addWidget(gs_card_row(pca_cards, this));

            if (!evr.isEmpty()) {
                const int rows = std::min<int>(10, evr.size());
                auto* table = new QTableWidget(rows, 3, this);
                table->setHorizontalHeaderLabels({"PC", "Explained Var", "Cumulative"});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(rows * 24 + 32);
                for (int i = 0; i < rows; ++i) {
                    table->setItem(i, 0, new QTableWidgetItem(QString("PC%1").arg(i + 1)));
                    auto* e = new QTableWidgetItem(QString::number(evr[i].toDouble() * 100, 'f', 3) + "%");
                    e->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 1, e);
                    auto* c = new QTableWidgetItem(QString::number(cum[i].toDouble() * 100, 'f', 3) + "%");
                    c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 2, c);
                    table->setRowHeight(i, 24);
                }
                results_layout_->addWidget(table);
            }
        }

        // K-Means section
        if (value_obj.contains("kmeans") && value_obj.value("kmeans").isObject()) {
            const QJsonObject km = value_obj.value("kmeans").toObject();
            results_layout_->addWidget(gs_section_header("K-MEANS", accent));
            const int opt_k = km.value("optimal_k").toInt();
            const QJsonArray sil = km.value("silhouette_by_k").toArray();
            const QJsonArray ine = km.value("inertia_by_k").toArray();
            double best_sil = 0.0;
            for (const auto& v : sil) {
                const auto pair = v.toArray();
                if (pair.size() >= 2) best_sil = std::max(best_sil, pair[1].toDouble());
            }
            const QJsonArray labels = km.value("cluster_labels").toArray();

            QList<QWidget*> km_cards = {
                gs_make_card("OPTIMAL k", QString::number(opt_k), this, ui::colors::POSITIVE()),
                gs_make_card("BEST SILHOUETTE", gs_fmt_num(best_sil, 4), this,
                             best_sil >= 0.5 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card("k RANGE", QString::number(sil.size()), this),
                gs_make_card("OBSERVATIONS", QString::number(labels.size()), this),
            };
            results_layout_->addWidget(gs_card_row(km_cards, this));

            if (!sil.isEmpty()) {
                auto* table = new QTableWidget(sil.size(), 3, this);
                table->setHorizontalHeaderLabels({"k", "Silhouette", "Inertia"});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(qMin(sil.size() * 24 + 32, 280));
                for (int i = 0; i < sil.size(); ++i) {
                    const auto sp = sil[i].toArray();
                    const auto ip = i < ine.size() ? ine[i].toArray() : QJsonArray();
                    const int k = sp.size() > 0 ? sp[0].toInt() : 0;
                    const double s = sp.size() > 1 ? sp[1].toDouble() : 0.0;
                    const double in_v = ip.size() > 1 ? ip[1].toDouble() : 0.0;

                    auto* k_it = new QTableWidgetItem(QString::number(k));
                    if (k == opt_k) k_it->setForeground(QColor(ui::colors::POSITIVE()));
                    table->setItem(i, 0, k_it);
                    auto* s_it = new QTableWidgetItem(QString::number(s, 'f', 4));
                    s_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (k == opt_k) s_it->setForeground(QColor(ui::colors::POSITIVE()));
                    table->setItem(i, 1, s_it);
                    auto* in_it = new QTableWidgetItem(QString::number(in_v, 'f', 2));
                    in_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 2, in_it);
                    table->setRowHeight(i, 24);
                }
                results_layout_->addWidget(table);
            }
        }

        // Hierarchical (simple)
        if (value_obj.contains("hierarchical") && value_obj.value("hierarchical").isObject()) {
            const QJsonObject h = value_obj.value("hierarchical").toObject();
            QList<QWidget*> h_cards = {
                gs_make_card("HIERARCHICAL k", QString::number(h.value("n_clusters").toInt()), this),
                gs_make_card("LABELS",
                             QString::number(h.value("cluster_labels").toArray().size()), this),
            };
            results_layout_->addWidget(gs_card_row(h_cards, this));
        }
        status_label_->setText(QString("Unsupervised: %1 method(s) on %2×%3")
                                   .arg(value_obj.size()).arg(n_samples).arg(n_feat));
        rendered = true;
    }

    // ── 7. MODEL EVALUATION ─────────────────────────────────────────────────
    else if (command == "model_evaluation") {
        const QJsonObject params = result.value("parameters").toObject();
        const QString model = params.value("model_type").toString().toUpper();
        const QString problem = params.value("problem_type").toString().toUpper();
        const int folds = params.value("cv_folds").toInt();
        const double mean_cv = meta.value("mean_cv_score").toDouble();
        const double std_cv = meta.value("std_cv_score").toDouble();
        const double overfit = meta.value("overfitting_score").toDouble();
        const bool is_overfit = meta.value("is_overfitting").toBool();
        const double final_train = meta.value("final_train_score").toDouble();
        const double final_val = meta.value("final_val_score").toDouble();

        QList<QWidget*> top = {
            gs_make_card("MODEL", model.isEmpty() ? "—" : model, this),
            gs_make_card("PROBLEM", problem, this),
            gs_make_card("CV FOLDS", QString::number(folds), this),
            gs_make_card("MEAN CV ± σ",
                         QString("%1 ± %2").arg(mean_cv, 0, 'f', 4).arg(std_cv, 0, 'f', 4), this,
                         mean_cv > 0.5 ? ui::colors::POSITIVE()
                                       : mean_cv > 0  ? ui::colors::WARNING()
                                                      : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> overfit_cards = {
            gs_make_card("FINAL TRAIN", gs_fmt_num(final_train, 4), this),
            gs_make_card("FINAL VAL", gs_fmt_num(final_val, 4), this,
                         final_val > 0.5 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("OVERFIT GAP", gs_fmt_num(overfit, 4), this,
                         overfit > 0.1 ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()),
            gs_make_card("OVERFITTING", is_overfit ? "YES" : "NO", this,
                         is_overfit ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(overfit_cards, this));

        // CV scores + learning curves table
        const QJsonArray cv = value_obj.value("cv_scores").toArray();
        const QJsonArray train_s = value_obj.value("train_scores").toArray();
        const QJsonArray val_s = value_obj.value("val_scores").toArray();
        if (!cv.isEmpty() || !train_s.isEmpty()) {
            auto* table = new QTableWidget(std::max(cv.size(), train_s.size()), 3, this);
            table->setHorizontalHeaderLabels({"Idx", "CV / Train Score", "Val Score"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(table->rowCount() * 24 + 32, 280));
            const int rows = table->rowCount();
            for (int i = 0; i < rows; ++i) {
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
                if (i < cv.size()) {
                    auto* it = new QTableWidgetItem(QString::number(cv[i].toDouble(), 'f', 4) + " (cv)");
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 1, it);
                } else if (i < train_s.size()) {
                    auto* it = new QTableWidgetItem(QString::number(train_s[i].toDouble(), 'f', 4) + " (tr)");
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 1, it);
                }
                if (i < val_s.size()) {
                    const double v = val_s[i].toDouble();
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (v > 0.5) it->setForeground(QColor(ui::colors::POSITIVE()));
                    else if (v < 0) it->setForeground(QColor(ui::colors::NEGATIVE()));
                    table->setItem(i, 2, it);
                }
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1: CV %2±%3  Overfit: %4")
                                   .arg(model).arg(mean_cv, 0, 'f', 3).arg(std_cv, 0, 'f', 3)
                                   .arg(is_overfit ? "YES" : "NO"));
        rendered = true;
    }

    // ── 8. RESAMPLING METHODS ───────────────────────────────────────────────
    else if (command == "resampling_methods") {
        const double orig = meta.value("original_statistic").toDouble();
        const int data_n = meta.value("data_size").toInt();
        const QString stat_name = meta.value("statistic_name").toString();

        QList<QWidget*> top = {
            gs_make_card("STATISTIC", stat_name.isEmpty() ? "mean" : stat_name, this),
            gs_make_card("ORIGINAL VALUE", gs_fmt_num(orig, 6), this, gs_pos_neg_color(orig)),
            gs_make_card("DATA SIZE", QString::number(data_n), this),
            gs_make_card("METHODS", QString::number(value_obj.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Bootstrap
        if (value_obj.contains("bootstrap") && value_obj.value("bootstrap").isObject()) {
            const QJsonObject b = value_obj.value("bootstrap").toObject();
            results_layout_->addWidget(gs_section_header("BOOTSTRAP", accent));
            const auto ci = b.value("confidence_interval").toArray();
            const QString ci_str = ci.size() == 2
                ? QString("[%1, %2]").arg(ci[0].toDouble(), 0, 'f', 4).arg(ci[1].toDouble(), 0, 'f', 4)
                : QStringLiteral("—");
            QList<QWidget*> b_cards = {
                gs_make_card("MEAN", gs_fmt_num(b.value("mean").toDouble(), 6), this),
                gs_make_card("STD", gs_fmt_num(b.value("std").toDouble(), 6), this),
                gs_make_card("BIAS", gs_fmt_num(b.value("bias").toDouble(), 6), this,
                             gs_pos_neg_color(b.value("bias").toDouble())),
                gs_make_card("95% CI", ci_str, this, ui::colors::INFO()),
            };
            results_layout_->addWidget(gs_card_row(b_cards, this));
        }

        // Jackknife
        if (value_obj.contains("jackknife") && value_obj.value("jackknife").isObject()) {
            const QJsonObject j = value_obj.value("jackknife").toObject();
            results_layout_->addWidget(gs_section_header("JACKKNIFE", accent));
            QList<QWidget*> j_cards = {
                gs_make_card("BIAS", gs_fmt_num(j.value("bias").toDouble(), 6), this,
                             gs_pos_neg_color(j.value("bias").toDouble())),
                gs_make_card("BIAS-CORRECTED", gs_fmt_num(j.value("bias_corrected_estimate").toDouble(), 6), this),
                gs_make_card("VARIANCE", gs_fmt_num(j.value("variance_estimate").toDouble(), 6), this),
                gs_make_card("STD ERROR", gs_fmt_num(j.value("std_error").toDouble(), 6), this),
            };
            results_layout_->addWidget(gs_card_row(j_cards, this));
        }

        // Permutation
        if (value_obj.contains("permutation") && value_obj.value("permutation").isObject()) {
            const QJsonObject p = value_obj.value("permutation").toObject();
            results_layout_->addWidget(gs_section_header("PERMUTATION", accent));
            const double pv = p.value("p_value").toDouble();
            const bool sig = p.value("significant").toBool();
            QList<QWidget*> p_cards = {
                gs_make_card("p-VALUE", gs_fmt_num(pv, 4), this,
                             pv < 0.05 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card("SIGNIFICANT", sig ? "YES" : "NO", this,
                             sig ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card("RESAMPLES",
                             QString::number(p.value("resampled_statistics").toArray().size()), this),
                gs_make_card("ORIGINAL", gs_fmt_num(orig, 6), this),
            };
            results_layout_->addWidget(gs_card_row(p_cards, this));
        }
        status_label_->setText(QString("Resampling: original=%1  (%2 method(s))")
                                   .arg(orig, 0, 'g', 4).arg(value_obj.size()));
        rendered = true;
    }

    // ── 9. SAMPLING TECHNIQUES ──────────────────────────────────────────────
    else if (command == "sampling_techniques") {
        const QJsonObject params = result.value("parameters").toObject();
        const int pop_n = params.value("population_size").toInt();
        const int sample_n = params.value("sample_size").toInt();
        const double frac = meta.value("sampling_fraction").toDouble();

        QList<QWidget*> top = {
            gs_make_card("POPULATION", QString::number(pop_n), this),
            gs_make_card("SAMPLE SIZE", QString::number(sample_n), this),
            gs_make_card("SAMPLING FRAC", QString::number(frac * 100, 'f', 2) + "%", this),
            gs_make_card("METHODS", QString::number(value_obj.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-method comparison table — pick the first numeric column from each method's stats
        if (!value_obj.isEmpty()) {
            QList<QPair<QString, QJsonObject>> methods;
            for (auto it = value_obj.begin(); it != value_obj.end(); ++it)
                if (it.value().isObject()) methods.append({it.key(), it.value().toObject()});

            auto* table = new QTableWidget(methods.size(), 5, this);
            table->setHorizontalHeaderLabels({"Method", "Sample N", "Sample Mean", "Pop Mean", "Bias"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(methods.size() * 26 + 32, 240));
            for (int r = 0; r < methods.size(); ++r) {
                const auto& name = methods[r].first;
                const auto& m = methods[r].second;
                table->setItem(r, 0, new QTableWidgetItem(name.toUpper()));
                if (m.contains("error")) {
                    auto* e = new QTableWidgetItem("ERR: " + m.value("error").toString());
                    e->setForeground(QColor(ui::colors::NEGATIVE()));
                    table->setItem(r, 1, e);
                    table->setSpan(r, 1, 1, 4);
                } else {
                    table->setItem(r, 1, new QTableWidgetItem(QString::number(m.value("sample_size").toInt())));
                    auto first_val = [](const QJsonObject& o) -> double {
                        if (o.isEmpty()) return 0.0;
                        return o.begin().value().toDouble();
                    };
                    const double sm = first_val(m.value("sample_mean").toObject());
                    const double pm = first_val(m.value("population_mean").toObject());
                    const double bias = first_val(m.value("bias").toObject());
                    auto add_n = [&](int col, double v, bool pos_neg = false) {
                        auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                        it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        if (pos_neg) it->setForeground(QColor(gs_pos_neg_color(v)));
                        table->setItem(r, col, it);
                    };
                    add_n(2, sm);
                    add_n(3, pm);
                    add_n(4, bias, true);
                }
                table->setRowHeight(r, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Sampling: %1 method(s)  pop=%2  n=%3 (%4%)")
                                   .arg(value_obj.size()).arg(pop_n).arg(sample_n)
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
            gs_make_card("DISTRIBUTION", dist.isEmpty() ? "—" : dist, this),
            gs_make_card("POP MEAN", gs_fmt_num(pmean, 4), this),
            gs_make_card("POP STD", gs_fmt_num(pstd, 4), this),
            gs_make_card("POP SIZE", QString::number(pop_n), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-sample-size table
        QList<QPair<int, QJsonObject>> rows;
        for (auto it = value_obj.begin(); it != value_obj.end(); ++it) {
            if (!it.value().isObject()) continue;
            const QString k = it.key();  // "n_5", "n_30", etc
            const int n = k.startsWith("n_") ? k.mid(2).toInt() : 0;
            rows.append({n, it.value().toObject()});
        }
        std::sort(rows.begin(), rows.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        if (!rows.isEmpty()) {
            auto* table = new QTableWidget(rows.size(), 7, this);
            table->setHorizontalHeaderLabels(
                {"n", "Theor. Mean", "Theor. SE", "Empir. Mean", "Empir. SE", "Mean Bias", "Normal?"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows.size() * 26 + 32, 320));
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
                QString norm_str = norm.isNull() ? "—" : (norm.toBool() ? "YES" : "NO");
                auto* ni = new QTableWidgetItem(norm_str);
                ni->setTextAlignment(Qt::AlignCenter);
                if (!norm.isNull())
                    ni->setForeground(QColor(norm.toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()));
                table->setItem(r, 6, ni);
                table->setRowHeight(r, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("CLT %1: μ=%2  σ=%3  (%4 sample sizes)")
                                   .arg(dist).arg(pmean, 0, 'f', 3).arg(pstd, 0, 'f', 3).arg(rows.size()));
        rendered = true;
    }

    // ── 11. SAMPLING ERROR ANALYSIS ─────────────────────────────────────────
    else if (command == "sampling_error_analysis") {
        const int rec_n = meta.value("recommended_sample_size").toInt();
        const double tgt_moe = meta.value("target_margin_of_error").toDouble();
        const double zc = meta.value("z_critical").toDouble();
        const double conf = meta.value("coverage_target").toDouble();

        QList<QWidget*> top = {
            gs_make_card("RECOMMENDED n", QString::number(rec_n), this, ui::colors::POSITIVE()),
            gs_make_card("TARGET MoE", gs_fmt_num(tgt_moe, 4), this),
            gs_make_card("Z CRITICAL", gs_fmt_num(zc, 4), this),
            gs_make_card("CONFIDENCE", QString::number(conf * 100, 'f', 1) + "%", this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QPair<int, QJsonObject>> rows;
        for (auto it = value_obj.begin(); it != value_obj.end(); ++it) {
            if (!it.value().isObject()) continue;
            const auto o = it.value().toObject();
            rows.append({o.value("sample_size").toInt(), o});
        }
        std::sort(rows.begin(), rows.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        if (!rows.isEmpty()) {
            auto* table = new QTableWidget(rows.size(), 6, this);
            table->setHorizontalHeaderLabels(
                {"n", "Theor. SE", "Empir. SE", "MoE", "Coverage", "MSE"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(rows.size() * 26 + 32, 320));
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
                table->setRowHeight(r, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Sampling error: recommended n=%1  target MoE=%2")
                                   .arg(rec_n).arg(tgt_moe, 0, 'f', 4));
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
            gs_make_card("QUALITY SCORE", QString::number(score, 'f', 1) + " / 100", this,
                         score >= 80 ? ui::colors::POSITIVE()
                                    : score >= 50 ? ui::colors::WARNING()
                                                  : ui::colors::NEGATIVE()),
            gs_make_card("DATA NAME", name.isEmpty() ? "data" : name, this),
            gs_make_card("ISSUES", QString::number(issues.size()), this,
                         issues.isEmpty() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("WARNINGS", QString::number(warnings.size()), this,
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
                results_layout_->addWidget(gs_section_header("STATISTICS", accent));
                auto* table = new QTableWidget(sk.size(), 2, this);
                table->setHorizontalHeaderLabels({"Statistic", "Value"});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(qMin(sk.size() * 24 + 32, 280));
                for (int r = 0; r < sk.size(); ++r) {
                    QString lbl = sk[r];
                    lbl.replace('_', ' ');
                    table->setItem(r, 0, new QTableWidgetItem(lbl.toUpper()));
                    auto* vi = new QTableWidgetItem(format_val(sv.value(sk[r])));
                    vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, 1, vi);
                    table->setRowHeight(r, 24);
                }
                results_layout_->addWidget(table);
            }
        }

        // Issues / warnings / recommendations sections
        auto add_section = [this, accent](const QString& title, const QJsonArray& items, bool issue_shape,
                                          const QString& bar_color) {
            if (items.isEmpty()) return;
            auto* hdr = new QLabel(QString("%1  (%2)").arg(title).arg(items.size()));
            hdr->setStyleSheet(QString("color:%1; font-weight:700; font-size:10px; letter-spacing:1px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), bar_color));
            results_layout_->addWidget(hdr);
            for (const auto& v : items) {
                QString text;
                QString fg = ui::colors::TEXT_SECONDARY();
                if (issue_shape && v.isObject()) {
                    const auto o = v.toObject();
                    const QString sev = o.value("severity").toString().toUpper();
                    if (sev == "CRITICAL" || sev == "HIGH") fg = ui::colors::NEGATIVE();
                    else if (sev == "MEDIUM") fg = ui::colors::WARNING();
                    text = QString("[%1] %2: %3")
                               .arg(sev.isEmpty() ? "—" : sev,
                                    o.value("type").toString(),
                                    o.value("description").toString(o.value("message").toString()));
                } else {
                    text = "• " + v.toString();
                }
                auto* item = new QLabel(text);
                item->setWordWrap(true);
                item->setStyleSheet(QString("color:%1; padding:2px 12px;").arg(fg));
                results_layout_->addWidget(item);
            }
        };
        add_section("ISSUES", issues, true, ui::colors::NEGATIVE());
        add_section("WARNINGS", warnings, true, ui::colors::WARNING());
        add_section("RECOMMENDATIONS", recs, false, ui::colors::INFO());

        status_label_->setText(QString("Validate %1: score=%2  issues=%3  warnings=%4")
                                   .arg(name).arg(score, 0, 'f', 1).arg(issues.size()).arg(warnings.size()));
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
            table->setHorizontalHeaderLabels({"Metric", "Value"});
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->verticalHeader()->setVisible(false);
            table->setMaximumHeight(qMin(keys.size() * 28 + 32, 480));
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
    auto* raw_hdr = new QLabel("RAW JSON");
    raw_hdr->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px; margin-top:4px;")
                               .arg(ui::colors::TEXT_TERTIARY()));
    results_layout_->addWidget(raw_hdr);

    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(180);
    raw->setPlainText(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    raw->setStyleSheet(output_ss());
    results_layout_->addWidget(raw);

    // ── Export button ───────────────────────────────────────────────────────
    auto* export_btn = new QPushButton("EXPORT RESULTS");
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setFixedHeight(28);
    export_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                                      "font-size:%3px; font-family:%4; padding:0 12px; }"
                                      "QPushButton:hover { color:%1; background:rgba(255,255,255,0.05); }")
                                  .arg(module_.color.name(), ui::colors::BORDER_DIM())
                                  .arg(ui::fonts::SMALL)
                                  .arg(ui::fonts::DATA_FAMILY));
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
        }
    });
    results_layout_->addWidget(export_btn);

    if (status_label_->text() == "Running...")
        status_label_->setText("Done");
}

} // namespace fincept::screens
