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
// CFA Python returns a uniform shape:
//   { success, analysis_type,
//     result: { value, method, parameters, metadata, calculation_time, timestamp, calculator } }
//
// validate_data is the exception: result is a DataQualityReport.to_dict() with
// quality_score, issues, warnings, statistics, recommendations.
//
// We render flat scalars in result.value + result.metadata as a metrics table,
// surface the analysis method/calc-time as a header line, and keep the raw JSON
// + export button for inspection.

namespace {

void cfa_flatten_scalars(const QJsonObject& obj, const QString& prefix, QStringList& keys,
                         QHash<QString, QJsonValue>& out) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const auto& v = it.value();
        const QString key = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
        if (v.isObject()) {
            // Recurse one level deep so nested {slope, intercept} flattens nicely;
            // skip very deep / array-heavy fields (parameters.data, fitted_values).
            if (key.endsWith(".data") || key.endsWith(".fitted_values") ||
                key.endsWith(".residuals") || key.endsWith(".model_summary"))
                continue;
            cfa_flatten_scalars(v.toObject(), key, keys, out);
        } else if (!v.isArray()) {
            keys.append(key);
            out.insert(key, v);
        }
    }
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

    // ── Header ──────────────────────────────────────────────────────────────
    auto* header = new QLabel(QString("%1  •  %2  •  %3 ms")
                                  .arg(analysis_type.toUpper())
                                  .arg(method)
                                  .arg(QString::number(calc_time * 1000.0, 'f', 1)));
    header->setStyleSheet(QString("color:%1; font-weight:700; font-family:%2; letter-spacing:1px;")
                              .arg(module_.color.name())
                              .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // ── validate_data has its own report shape ──────────────────────────────
    if (command == "validate_data") {
        const double score = result.value("quality_score").toDouble();
        auto* score_lbl = new QLabel(QString("Quality Score: %1 / 100").arg(score, 0, 'f', 1));
        score_lbl->setStyleSheet(QString("color:%1; font-size:16px; font-weight:700;")
                                     .arg(score >= 80 ? ui::colors::POSITIVE()
                                          : score >= 50 ? ui::colors::WARNING()
                                                        : ui::colors::NEGATIVE()));
        results_layout_->addWidget(score_lbl);

        const auto issues = result.value("issues").toArray();
        const auto warnings = result.value("warnings").toArray();
        const auto recs = result.value("recommendations").toArray();

        auto add_section = [this](const QString& title, const QJsonArray& items, bool issue_shape) {
            if (items.isEmpty())
                return;
            auto* lbl = new QLabel(QString("%1 (%2)").arg(title).arg(items.size()));
            lbl->setStyleSheet(QString("color:%1; font-weight:700; margin-top:6px;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
            results_layout_->addWidget(lbl);
            for (const auto& v : items) {
                QString text;
                if (issue_shape) {
                    auto o = v.toObject();
                    text = QString("[%1] %2: %3")
                               .arg(o.value("severity").toString(),
                                    o.value("type").toString(),
                                    o.value("description").toString(o.value("message").toString()));
                } else {
                    text = "• " + v.toString();
                }
                auto* item = new QLabel(text);
                item->setWordWrap(true);
                item->setStyleSheet(QString("color:%1; padding:2px 0;").arg(ui::colors::TEXT_SECONDARY()));
                results_layout_->addWidget(item);
            }
        };
        add_section("Issues", issues, true);
        add_section("Warnings", warnings, true);
        add_section("Recommendations", recs, false);
    } else {
        // ── Standard analyses: flatten value + metadata into a metrics table ──
        QStringList keys;
        QHash<QString, QJsonValue> kv;
        const QJsonObject value_obj = result.value("value").toObject();
        const QJsonObject meta_obj = result.value("metadata").toObject();

        // value can also be a scalar (e.g. forecasting → array, but value can be number)
        const QJsonValue value_raw = result.value("value");
        if (value_obj.isEmpty() && !value_raw.isNull() && !value_raw.isObject() && !value_raw.isArray()) {
            keys.append("value");
            kv.insert("value", value_raw);
        } else {
            cfa_flatten_scalars(value_obj, "value", keys, kv);
        }
        cfa_flatten_scalars(meta_obj, "metadata", keys, kv);

        if (!keys.isEmpty()) {
            auto* table = new QTableWidget(keys.size(), 2);
            table->setHorizontalHeaderLabels({"Metric", "Value"});
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setAlternatingRowColors(true);
            table->horizontalHeader()->setStretchLastSection(true);
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
            table->resizeColumnsToContents();
            results_layout_->addWidget(table);
        }

        // For forecasting the value is an array of forecast values — show inline
        if (value_raw.isArray()) {
            const auto arr = value_raw.toArray();
            QStringList parts;
            parts.reserve(arr.size());
            for (const auto& v : arr)
                parts << QString::number(v.toDouble(), 'f', 4);
            auto* fcst = new QLabel(QString("Forecast (%1 steps): %2").arg(arr.size()).arg(parts.join(", ")));
            fcst->setWordWrap(true);
            fcst->setStyleSheet(QString("color:%1; padding:6px 0;").arg(ui::colors::TEXT_PRIMARY()));
            results_layout_->addWidget(fcst);
        }
    }

    // ── Raw JSON viewer ─────────────────────────────────────────────────────
    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(220);
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

    status_label_->setText("Done");
}

} // namespace fincept::screens
