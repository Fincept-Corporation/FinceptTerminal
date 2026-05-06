// src/screens/ai_quant_lab/QuantModulePanel_Misc.cpp
//
// Miscellaneous panel builders — Advanced Models, Feature Engineering,
// Portfolio Opt, Factor Evaluation, Strategy Builder, Data Processors, Factor
// Discovery, Model Library, Live Signals, Online Learning, Meta Learning, HFT,
// Rolling Retraining. Extracted from QuantModulePanel.cpp to keep that file
// maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QUrl>
#include <QDesktopServices>
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
// ADVANCED MODELS PANEL
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_advanced_models_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* model_type = new QComboBox(w);
    model_type->addItems(
        {"LSTM", "GRU", "Transformer", "Localformer", "HIST", "GAT", "LightGBM", "XGBoost", "CatBoost"});
    model_type->setStyleSheet(combo_ss());
    combo_inputs_["adv_model"] = model_type;
    vl->addWidget(build_input_row("Model Type", model_type, w));

    auto* hidden = new QSpinBox(w);
    hidden->setRange(16, 1024);
    hidden->setValue(64);
    hidden->setStyleSheet(spinbox_ss());
    int_inputs_["adv_hidden"] = hidden;
    vl->addWidget(build_input_row("Hidden Size", hidden, w));

    auto* layers = new QSpinBox(w);
    layers->setRange(1, 12);
    layers->setValue(2);
    layers->setStyleSheet(hidden->styleSheet());
    int_inputs_["adv_layers"] = layers;
    vl->addWidget(build_input_row("Num Layers", layers, w));

    auto* dropout = make_double_spin(0, 0.9, 0.1, 2, "", w);
    double_inputs_["adv_dropout"] = dropout;
    vl->addWidget(build_input_row("Dropout", dropout, w));

    auto* epochs = new QSpinBox(w);
    epochs->setRange(1, 1000);
    epochs->setValue(50);
    epochs->setStyleSheet(hidden->styleSheet());
    int_inputs_["adv_epochs"] = epochs;
    vl->addWidget(build_input_row("Epochs", epochs, w));

    auto* run = make_run_button("CREATE & TRAIN MODEL", w);
    connect(run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Training...");
        QJsonObject params;
        params["model_type"] = combo_inputs_["adv_model"]->currentText();
        QJsonObject config;
        config["hidden_size"] = int_inputs_["adv_hidden"]->value();
        config["num_layers"] = int_inputs_["adv_layers"]->value();
        config["dropout"] = double_inputs_["adv_dropout"]->value();
        config["epochs"] = int_inputs_["adv_epochs"]->value();
        params["model_config"] = config;
        AIQuantLabService::instance().train_advanced_model(params);
    });
    vl->addWidget(run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}
// ═══════════════════════════════════════════════════════════════════════════════
// FEATURE ENGINEERING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_feature_engineering_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Indicators tab ──
    auto* ind = new QWidget(this);
    auto* ivl = new QVBoxLayout(ind);
    ivl->setContentsMargins(12, 12, 12, 12);
    ivl->setSpacing(8);

    auto* ind_data = new QLineEdit(ind);
    ind_data->setPlaceholderText("Price data (comma-separated, e.g. 100,102,101,105,108)");
    ind_data->setStyleSheet(input_ss());
    text_inputs_["fe_data"] = ind_data;
    ivl->addWidget(build_input_row("Price Data", ind_data, ind));

    auto* ind_indicator = new QComboBox(ind);
    ind_indicator->addItems({"moving_average", "rsi", "macd", "bollinger_bands", "momentum", "volatility", "returns",
                             "log_returns", "drawdown"});
    ind_indicator->setStyleSheet(combo_ss());
    combo_inputs_["fe_indicator"] = ind_indicator;
    ivl->addWidget(build_input_row("Indicator", ind_indicator, ind));

    auto* ind_window = new QSpinBox(ind);
    ind_window->setRange(2, 200);
    ind_window->setValue(14);
    ind_window->setStyleSheet(spinbox_ss());
    int_inputs_["fe_window"] = ind_window;
    ivl->addWidget(build_input_row("Window", ind_window, ind));

    auto* ind_run = make_run_button("COMPUTE INDICATOR", ind);
    connect(ind_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Computing...");
        QJsonObject params;
        params["data"] = text_inputs_["fe_data"]->text();
        params["indicator"] = combo_inputs_["fe_indicator"]->currentText();
        params["window"] = int_inputs_["fe_window"]->value();
        AIQuantLabService::instance().feature_compute(params);
    });
    ivl->addWidget(ind_run);
    ivl->addStretch();
    tabs->addTab(ind, "Indicators");

    // ── Feature Selection tab ──
    auto* sel = new QWidget(this);
    auto* svl = new QVBoxLayout(sel);
    svl->setContentsMargins(12, 12, 12, 12);
    svl->setSpacing(8);

    auto* sel_features = new QLineEdit(sel);
    sel_features->setPlaceholderText("Feature values JSON: {\"rsi\":[...],\"macd\":[...]}");
    sel_features->setStyleSheet(input_ss());
    text_inputs_["fe_sel_features"] = sel_features;
    svl->addWidget(build_input_row("Features (JSON)", sel_features, sel));

    auto* sel_returns = new QLineEdit(sel);
    sel_returns->setPlaceholderText("Target returns (comma-separated)");
    sel_returns->setStyleSheet(input_ss());
    text_inputs_["fe_sel_returns"] = sel_returns;
    svl->addWidget(build_input_row("Returns", sel_returns, sel));

    auto* sel_topk = new QSpinBox(sel);
    sel_topk->setRange(1, 50);
    sel_topk->setValue(5);
    sel_topk->setStyleSheet(spinbox_ss());
    int_inputs_["fe_topk"] = sel_topk;
    svl->addWidget(build_input_row("Top-K Features", sel_topk, sel));

    auto* sel_run = make_run_button("SELECT FEATURES BY IC", sel);
    connect(sel_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Selecting...");
        QJsonObject params;
        auto feat_text = text_inputs_["fe_sel_features"]->text();
        auto doc = QJsonDocument::fromJson(feat_text.toUtf8());
        if (!doc.isNull())
            params["features"] = doc.object();
        params["returns"] = text_inputs_["fe_sel_returns"]->text();
        params["top_k"] = int_inputs_["fe_topk"]->value();
        AIQuantLabService::instance().feature_select_by_ic(params);
    });
    svl->addWidget(sel_run);
    svl->addStretch();
    tabs->addTab(sel, "Feature Selection");

    // ── Expression Engine tab ──
    auto* expr = new QWidget(this);
    auto* evl = new QVBoxLayout(expr);
    evl->setContentsMargins(12, 12, 12, 12);
    evl->setSpacing(8);

    auto* expr_data = new QLineEdit(expr);
    expr_data->setPlaceholderText("{\"close\":[100,102,...],\"volume\":[1000,1200,...]}");
    expr_data->setStyleSheet(input_ss());
    text_inputs_["fe_expr_data"] = expr_data;
    evl->addWidget(build_input_row("OHLCV Data (JSON)", expr_data, expr));

    auto* expr_expr = new QLineEdit(expr);
    expr_expr->setPlaceholderText("e.g. Mean(close, 5) / Std(close, 20)");
    expr_expr->setStyleSheet(input_ss());
    text_inputs_["fe_expression"] = expr_expr;
    evl->addWidget(build_input_row("Expression", expr_expr, expr));

    auto* expr_run = make_run_button("EVALUATE EXPRESSION", expr);
    connect(expr_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Evaluating...");
        QJsonObject params;
        auto data_text = text_inputs_["fe_expr_data"]->text();
        auto doc = QJsonDocument::fromJson(data_text.toUtf8());
        if (!doc.isNull())
            params["data"] = doc.object();
        params["expression"] = text_inputs_["fe_expression"]->text();
        AIQuantLabService::instance().feature_evaluate_expression(params);
    });
    evl->addWidget(expr_run);
    evl->addStretch();
    tabs->addTab(expr, "Expression Engine");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PORTFOLIO OPTIMIZATION PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_portfolio_opt_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Shared covariance / returns helper ──
    auto make_cov_tab = [&](const QString& method_id, const QString& btn_label, bool needs_returns) -> QWidget* {
        auto* t = new QWidget(this);
        auto* tvl = new QVBoxLayout(t);
        tvl->setContentsMargins(12, 12, 12, 12);
        tvl->setSpacing(8);

        auto* assets_in = new QLineEdit(t);
        assets_in->setPlaceholderText("Asset names (comma-separated, e.g. AAPL,GOOG,MSFT)");
        assets_in->setStyleSheet(input_ss());
        text_inputs_[method_id + "_assets"] = assets_in;
        tvl->addWidget(build_input_row("Assets", assets_in, t));

        auto* cov_in = new QLineEdit(t);
        cov_in->setPlaceholderText("Covariance matrix JSON: [[0.04,0.01],[0.01,0.09]]");
        cov_in->setStyleSheet(input_ss());
        text_inputs_[method_id + "_cov"] = cov_in;
        tvl->addWidget(build_input_row("Covariance Matrix", cov_in, t));

        if (needs_returns) {
            auto* ret_in = new QLineEdit(t);
            ret_in->setPlaceholderText("Expected returns (comma-separated, e.g. 0.10,0.15,0.12)");
            ret_in->setStyleSheet(input_ss());
            text_inputs_[method_id + "_returns"] = ret_in;
            tvl->addWidget(build_input_row("Expected Returns", ret_in, t));
        }

        auto* rf_spin = make_double_spin(0, 20, 2.0, 2, "%", t);
        double_inputs_[method_id + "_rf"] = rf_spin;
        tvl->addWidget(build_input_row("Risk-Free Rate", rf_spin, t));

        auto* run = make_run_button(btn_label, t);
        connect(run, &QPushButton::clicked, this, [this, method_id, needs_returns]() {
            status_label_->setText("Optimizing...");
            QJsonObject params;
            auto assets_str = text_inputs_[method_id + "_assets"]->text().split(',');
            QJsonArray assets_arr;
            for (auto& a : assets_str)
                assets_arr.append(a.trimmed());
            params["assets"] = assets_arr;
            auto cov_doc = QJsonDocument::fromJson(text_inputs_[method_id + "_cov"]->text().toUtf8());
            if (!cov_doc.isNull())
                params["cov_matrix"] = cov_doc.array();
            if (needs_returns) {
                QJsonArray ret_arr;
                for (auto& r : text_inputs_[method_id + "_returns"]->text().split(','))
                    ret_arr.append(r.trimmed().toDouble());
                params["expected_returns"] = ret_arr;
            }
            params["risk_free_rate"] = double_inputs_[method_id + "_rf"]->value() / 100.0;
            AIQuantLabService::instance().portopt_run(method_id, params);
        });
        tvl->addWidget(run);
        tvl->addStretch();
        return t;
    };

    tabs->addTab(make_cov_tab("hierarchical_risk_parity", "RUN HRP", false), "HRP");
    tabs->addTab(make_cov_tab("minimum_variance", "MIN VARIANCE", false), "Min Variance");
    tabs->addTab(make_cov_tab("maximum_sharpe", "MAX SHARPE", true), "Max Sharpe");
    tabs->addTab(make_cov_tab("efficient_frontier", "EFFICIENT FRONTIER", true), "Eff. Frontier");

    // ── Black-Litterman ──
    auto* bl = new QWidget(this);
    auto* blvl = new QVBoxLayout(bl);
    blvl->setContentsMargins(12, 12, 12, 12);
    blvl->setSpacing(8);
    auto* bl_assets = new QLineEdit(bl);
    bl_assets->setPlaceholderText("Asset names (comma-separated)");
    bl_assets->setStyleSheet(input_ss());
    text_inputs_["bl_assets"] = bl_assets;
    blvl->addWidget(build_input_row("Assets", bl_assets, bl));
    auto* bl_caps = new QLineEdit(bl);
    bl_caps->setPlaceholderText("Market caps (comma-separated, e.g. 2000,1500,800)");
    bl_caps->setStyleSheet(input_ss());
    text_inputs_["bl_caps"] = bl_caps;
    blvl->addWidget(build_input_row("Market Caps ($B)", bl_caps, bl));
    auto* bl_cov = new QLineEdit(bl);
    bl_cov->setPlaceholderText("Covariance matrix JSON: [[0.04,0.01],[0.01,0.09]]");
    bl_cov->setStyleSheet(input_ss());
    text_inputs_["bl_cov"] = bl_cov;
    blvl->addWidget(build_input_row("Covariance Matrix", bl_cov, bl));
    auto* bl_views = new QLineEdit(bl);
    bl_views->setPlaceholderText("Views (comma-separated, e.g. 0.05,0.10)");
    bl_views->setStyleSheet(input_ss());
    text_inputs_["bl_views"] = bl_views;
    blvl->addWidget(build_input_row("Views", bl_views, bl));
    auto* bl_conf = new QLineEdit(bl);
    bl_conf->setPlaceholderText("View confidences (e.g. 0.8,0.6)");
    bl_conf->setStyleSheet(input_ss());
    text_inputs_["bl_conf"] = bl_conf;
    blvl->addWidget(build_input_row("View Confidences", bl_conf, bl));
    auto* bl_run = make_run_button("RUN BLACK-LITTERMAN", bl);
    connect(bl_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running BL...");
        QJsonObject params;
        QJsonArray caps, views, confs, assets;
        for (auto& v : text_inputs_["bl_caps"]->text().split(','))
            caps.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["bl_views"]->text().split(','))
            views.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["bl_conf"]->text().split(','))
            confs.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["bl_assets"]->text().split(','))
            assets.append(v.trimmed());
        params["market_caps"] = caps;
        params["views"] = views;
        params["view_confidences"] = confs;
        params["assets"] = assets;
        auto cov_doc = QJsonDocument::fromJson(text_inputs_["bl_cov"]->text().toUtf8());
        if (!cov_doc.isNull())
            params["cov_matrix"] = cov_doc.array();
        AIQuantLabService::instance().portopt_run("black_litterman", params);
    });
    blvl->addWidget(bl_run);
    blvl->addStretch();
    tabs->addTab(bl, "Black-Litterman");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FACTOR EVALUATION PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_factor_evaluation_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── IC Metrics tab ──
    auto* ic = new QWidget(this);
    auto* icvl = new QVBoxLayout(ic);
    icvl->setContentsMargins(12, 12, 12, 12);
    icvl->setSpacing(8);
    auto* ic_preds = new QLineEdit(ic);
    ic_preds->setPlaceholderText("Predictions (comma-separated, e.g. 0.1,0.2,-0.1,0.3)");
    ic_preds->setStyleSheet(input_ss());
    text_inputs_["ev_predictions"] = ic_preds;
    icvl->addWidget(build_input_row("Predictions", ic_preds, ic));
    auto* ic_rets = new QLineEdit(ic);
    ic_rets->setPlaceholderText("Actual returns (comma-separated)");
    ic_rets->setStyleSheet(input_ss());
    text_inputs_["ev_returns"] = ic_rets;
    icvl->addWidget(build_input_row("Returns", ic_rets, ic));
    auto* ic_method = new QComboBox(ic);
    ic_method->addItems({"pearson", "spearman"});
    ic_method->setStyleSheet(combo_ss());
    combo_inputs_["ev_ic_method"] = ic_method;
    icvl->addWidget(build_input_row("Method", ic_method, ic));
    auto* ic_run = make_run_button("CALCULATE IC METRICS", ic);
    connect(ic_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        QJsonArray preds, rets;
        for (auto& v : text_inputs_["ev_predictions"]->text().split(','))
            preds.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["ev_returns"]->text().split(','))
            rets.append(v.trimmed().toDouble());
        params["predictions"] = preds;
        params["returns"] = rets;
        params["method"] = combo_inputs_["ev_ic_method"]->currentText();
        AIQuantLabService::instance().evaluation_ic(params);
    });
    icvl->addWidget(ic_run);
    icvl->addStretch();
    tabs->addTab(ic, "IC Metrics");

    // ── Full Report tab ──
    auto* rep = new QWidget(this);
    auto* repvl = new QVBoxLayout(rep);
    repvl->setContentsMargins(12, 12, 12, 12);
    repvl->setSpacing(8);
    auto* rep_name = new QLineEdit(rep);
    rep_name->setPlaceholderText("Factor name (e.g. momentum)");
    rep_name->setStyleSheet(input_ss());
    text_inputs_["ev_factor_name"] = rep_name;
    repvl->addWidget(build_input_row("Factor Name", rep_name, rep));
    auto* rep_preds = new QLineEdit(rep);
    rep_preds->setPlaceholderText("Predictions (comma-separated)");
    rep_preds->setStyleSheet(input_ss());
    text_inputs_["ev_rep_preds"] = rep_preds;
    repvl->addWidget(build_input_row("Predictions", rep_preds, rep));
    auto* rep_rets = new QLineEdit(rep);
    rep_rets->setPlaceholderText("Returns (comma-separated)");
    rep_rets->setStyleSheet(input_ss());
    text_inputs_["ev_rep_returns"] = rep_rets;
    repvl->addWidget(build_input_row("Returns", rep_rets, rep));
    auto* rep_run = make_run_button("GENERATE EVALUATION REPORT", rep);
    connect(rep_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Generating...");
        QJsonObject params;
        QJsonArray preds, rets;
        for (auto& v : text_inputs_["ev_rep_preds"]->text().split(','))
            preds.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["ev_rep_returns"]->text().split(','))
            rets.append(v.trimmed().toDouble());
        params["factor_name"] = text_inputs_["ev_factor_name"]->text();
        params["predictions"] = preds;
        params["returns"] = rets;
        AIQuantLabService::instance().evaluation_report(params);
    });
    repvl->addWidget(rep_run);
    repvl->addStretch();
    tabs->addTab(rep, "Full Report");

    // ── Risk Metrics tab ──
    auto* risk = new QWidget(this);
    auto* riskvl = new QVBoxLayout(risk);
    riskvl->setContentsMargins(12, 12, 12, 12);
    riskvl->setSpacing(8);
    auto* risk_rets = new QLineEdit(risk);
    risk_rets->setPlaceholderText("Daily returns (comma-separated)");
    risk_rets->setStyleSheet(input_ss());
    text_inputs_["ev_risk_returns"] = risk_rets;
    riskvl->addWidget(build_input_row("Returns", risk_rets, risk));
    auto* risk_bench = new QLineEdit(risk);
    risk_bench->setPlaceholderText("Benchmark returns (optional, comma-separated)");
    risk_bench->setStyleSheet(input_ss());
    text_inputs_["ev_risk_bench"] = risk_bench;
    riskvl->addWidget(build_input_row("Benchmark (opt.)", risk_bench, risk));
    auto* risk_conf = make_double_spin(0.8, 0.999, 0.95, 3, "", risk);
    double_inputs_["ev_risk_conf"] = risk_conf;
    riskvl->addWidget(build_input_row("Confidence Level", risk_conf, risk));
    auto* risk_run = make_run_button("CALCULATE RISK METRICS", risk);
    connect(risk_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        QJsonArray rets;
        for (auto& v : text_inputs_["ev_risk_returns"]->text().split(','))
            rets.append(v.trimmed().toDouble());
        params["returns"] = rets;
        params["confidence_level"] = double_inputs_["ev_risk_conf"]->value();
        auto bench_text = text_inputs_["ev_risk_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            for (auto& v : bench_text.split(','))
                bench.append(v.trimmed().toDouble());
            params["benchmark_returns"] = bench;
        }
        AIQuantLabService::instance().evaluation_risk_metrics(params);
    });
    riskvl->addWidget(risk_run);
    riskvl->addStretch();
    tabs->addTab(risk, "Risk Metrics");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRATEGY BUILDER PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_strategy_builder_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── TopK-Dropout ──
    auto* topk = new QWidget(this);
    auto* tkvl = new QVBoxLayout(topk);
    tkvl->setContentsMargins(12, 12, 12, 12);
    tkvl->setSpacing(8);
    auto* tk_signal = new QLineEdit(topk);
    tk_signal->setPlaceholderText("Signal values (comma-separated)");
    tk_signal->setStyleSheet(input_ss());
    text_inputs_["st_tk_signal"] = tk_signal;
    tkvl->addWidget(build_input_row("Signal", tk_signal, topk));
    auto* tk_topk = new QSpinBox(topk);
    tk_topk->setRange(1, 500);
    tk_topk->setValue(50);
    tk_topk->setStyleSheet(spinbox_ss());
    int_inputs_["st_topk"] = tk_topk;
    tkvl->addWidget(build_input_row("Top-K", tk_topk, topk));
    auto* tk_drop = new QSpinBox(topk);
    tk_drop->setRange(1, 100);
    tk_drop->setValue(5);
    tk_drop->setStyleSheet(tk_topk->styleSheet());
    int_inputs_["st_ndrop"] = tk_drop;
    tkvl->addWidget(build_input_row("N-Drop", tk_drop, topk));
    auto* tk_run = make_run_button("CREATE TOPK-DROPOUT STRATEGY", topk);
    connect(tk_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        QJsonArray signal;
        for (auto& v : text_inputs_["st_tk_signal"]->text().split(','))
            signal.append(v.trimmed().toDouble());
        params["signal"] = signal;
        params["topk"] = int_inputs_["st_topk"]->value();
        params["n_drop"] = int_inputs_["st_ndrop"]->value();
        AIQuantLabService::instance().strategy_create("create_topk_dropout", params);
    });
    tkvl->addWidget(tk_run);
    tkvl->addStretch();
    tabs->addTab(topk, "TopK-Dropout");

    // ── Risk Parity ──
    auto* rp = new QWidget(this);
    auto* rpvl = new QVBoxLayout(rp);
    rpvl->setContentsMargins(12, 12, 12, 12);
    rpvl->setSpacing(8);
    auto* rp_returns = new QLineEdit(rp);
    rp_returns->setPlaceholderText("Asset returns matrix JSON: [[0.01,-0.02,...],[...]]");
    rp_returns->setStyleSheet(input_ss());
    text_inputs_["st_rp_returns"] = rp_returns;
    rpvl->addWidget(build_input_row("Returns Matrix", rp_returns, rp));
    auto* rp_target = make_double_spin(0.01, 1.0, 0.10, 2, "", rp);
    double_inputs_["st_rp_target"] = rp_target;
    rpvl->addWidget(build_input_row("Target Risk", rp_target, rp));
    auto* rp_freq = new QComboBox(rp);
    rp_freq->addItems({"monthly", "weekly", "daily", "quarterly"});
    rp_freq->setStyleSheet(combo_ss());
    combo_inputs_["st_rp_freq"] = rp_freq;
    rpvl->addWidget(build_input_row("Rebalance", rp_freq, rp));
    auto* rp_run = make_run_button("CREATE RISK PARITY STRATEGY", rp);
    connect(rp_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        auto ret_doc = QJsonDocument::fromJson(text_inputs_["st_rp_returns"]->text().toUtf8());
        if (!ret_doc.isNull())
            params["returns"] = ret_doc.array();
        params["target_risk"] = double_inputs_["st_rp_target"]->value();
        params["rebalance_frequency"] = combo_inputs_["st_rp_freq"]->currentText();
        AIQuantLabService::instance().strategy_create("create_risk_parity", params);
    });
    rpvl->addWidget(rp_run);
    rpvl->addStretch();
    tabs->addTab(rp, "Risk Parity");

    // ── Portfolio Metrics ──
    auto* pm = new QWidget(this);
    auto* pmvl = new QVBoxLayout(pm);
    pmvl->setContentsMargins(12, 12, 12, 12);
    pmvl->setSpacing(8);
    auto* pm_rets = new QLineEdit(pm);
    pm_rets->setPlaceholderText("Portfolio returns (comma-separated)");
    pm_rets->setStyleSheet(input_ss());
    text_inputs_["st_pm_returns"] = pm_rets;
    pmvl->addWidget(build_input_row("Returns", pm_rets, pm));
    auto* pm_bench = new QLineEdit(pm);
    pm_bench->setPlaceholderText("Benchmark returns (optional)");
    pm_bench->setStyleSheet(input_ss());
    text_inputs_["st_pm_bench"] = pm_bench;
    pmvl->addWidget(build_input_row("Benchmark (opt.)", pm_bench, pm));
    auto* pm_rf = make_double_spin(0, 20, 2.0, 2, "%", pm);
    double_inputs_["st_pm_rf"] = pm_rf;
    pmvl->addWidget(build_input_row("Risk-Free Rate", pm_rf, pm));
    auto* pm_run = make_run_button("CALCULATE PORTFOLIO METRICS", pm);
    connect(pm_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        QJsonArray rets;
        for (auto& v : text_inputs_["st_pm_returns"]->text().split(','))
            rets.append(v.trimmed().toDouble());
        params["returns"] = rets;
        params["risk_free_rate"] = double_inputs_["st_pm_rf"]->value() / 100.0;
        auto bench_text = text_inputs_["st_pm_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            for (auto& v : bench_text.split(','))
                bench.append(v.trimmed().toDouble());
            params["benchmark_returns"] = bench;
        }
        AIQuantLabService::instance().strategy_portfolio_metrics(params);
    });
    pmvl->addWidget(pm_run);
    pmvl->addStretch();
    tabs->addTab(pm, "Portfolio Metrics");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATA PROCESSORS PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_data_processors_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── List Processors ──
    auto* list_tab = new QWidget(this);
    auto* ltvl = new QVBoxLayout(list_tab);
    ltvl->setContentsMargins(12, 12, 12, 12);
    ltvl->setSpacing(8);
    auto* lt_info = new QLabel("Browse all available data normalizers and transformation processors.", list_tab);
    lt_info->setWordWrap(true);
    lt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    ltvl->addWidget(lt_info);
    auto* lt_run = make_run_button("LIST ALL PROCESSORS", list_tab);
    connect(lt_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().dataproc_list_processors();
    });
    ltvl->addWidget(lt_run);
    ltvl->addStretch();
    tabs->addTab(list_tab, "Browse");

    // ── Create Pipeline ──
    auto* pipe_tab = new QWidget(this);
    auto* ptvl = new QVBoxLayout(pipe_tab);
    ptvl->setContentsMargins(12, 12, 12, 12);
    ptvl->setSpacing(8);
    auto* pipe_id = new QLineEdit(pipe_tab);
    pipe_id->setPlaceholderText("Pipeline ID (e.g. my_pipeline)");
    pipe_id->setStyleSheet(input_ss());
    text_inputs_["dp_pipeline_id"] = pipe_id;
    ptvl->addWidget(build_input_row("Pipeline ID", pipe_id, pipe_tab));
    auto* pipe_procs = new QLineEdit(pipe_tab);
    pipe_procs->setPlaceholderText(R"([{"type":"zscore"},{"type":"winsorize","lower":0.01,"upper":0.99}])");
    pipe_procs->setStyleSheet(input_ss());
    text_inputs_["dp_processors"] = pipe_procs;
    ptvl->addWidget(build_input_row("Processors (JSON)", pipe_procs, pipe_tab));
    auto* pipe_run = make_run_button("CREATE PIPELINE", pipe_tab);
    connect(pipe_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        params["pipeline_id"] = text_inputs_["dp_pipeline_id"]->text();
        auto doc = QJsonDocument::fromJson(text_inputs_["dp_processors"]->text().toUtf8());
        if (!doc.isNull())
            params["processors"] = doc.array();
        AIQuantLabService::instance().dataproc_create_pipeline(params);
    });
    ptvl->addWidget(pipe_run);
    ptvl->addStretch();
    tabs->addTab(pipe_tab, "Create Pipeline");

    // ── Process Data ──
    auto* proc_tab = new QWidget(this);
    auto* procvl = new QVBoxLayout(proc_tab);
    procvl->setContentsMargins(12, 12, 12, 12);
    procvl->setSpacing(8);
    auto* proc_pid = new QLineEdit(proc_tab);
    proc_pid->setPlaceholderText("Pipeline ID (must be created first)");
    proc_pid->setStyleSheet(input_ss());
    text_inputs_["dp_proc_pid"] = proc_pid;
    procvl->addWidget(build_input_row("Pipeline ID", proc_pid, proc_tab));
    auto* proc_data = new QLineEdit(proc_tab);
    proc_data->setPlaceholderText(R"({"feature_close":[100,102,...],"feature_volume":[1000,1200,...]})");
    proc_data->setStyleSheet(input_ss());
    text_inputs_["dp_proc_data"] = proc_data;
    procvl->addWidget(build_input_row("Data (JSON)", proc_data, proc_tab));
    auto* proc_run = make_run_button("PROCESS DATA", proc_tab);
    connect(proc_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Processing...");
        QJsonObject params;
        params["pipeline_id"] = text_inputs_["dp_proc_pid"]->text();
        auto doc = QJsonDocument::fromJson(text_inputs_["dp_proc_data"]->text().toUtf8());
        if (!doc.isNull())
            params["data"] = doc.object();
        AIQuantLabService::instance().dataproc_process_data(params);
    });
    procvl->addWidget(proc_run);
    procvl->addStretch();
    tabs->addTab(proc_tab, "Process Data");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FACTOR DISCOVERY PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_factor_discovery_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Factor Library ──
    auto* lib_tab = new QWidget(this);
    auto* libvl = new QVBoxLayout(lib_tab);
    libvl->setContentsMargins(12, 12, 12, 12);
    libvl->setSpacing(8);
    auto* lib_info = new QLabel("Browse all built-in Qlib alpha factors and expressions.", lib_tab);
    lib_info->setWordWrap(true);
    lib_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY));
    libvl->addWidget(lib_info);
    auto* lib_run = make_run_button("BROWSE FACTOR LIBRARY", lib_tab);
    connect(lib_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().factor_get_library();
    });
    libvl->addWidget(lib_run);
    auto* inst_run = make_run_button("LIST INSTRUMENTS", lib_tab);
    connect(inst_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().factor_get_instruments();
    });
    libvl->addWidget(inst_run);
    libvl->addStretch();
    tabs->addTab(lib_tab, "Factor Library");

    // ── Fetch Data ──
    auto* data_tab = new QWidget(this);
    auto* datavl = new QVBoxLayout(data_tab);
    datavl->setContentsMargins(12, 12, 12, 12);
    datavl->setSpacing(8);
    auto* fd_instr = new QLineEdit(data_tab);
    fd_instr->setPlaceholderText("Instruments (comma-separated, e.g. aapl,msft)");
    fd_instr->setStyleSheet(input_ss());
    text_inputs_["fd_instruments"] = fd_instr;
    datavl->addWidget(build_input_row("Instruments", fd_instr, data_tab));
    auto* fd_fields = new QLineEdit(data_tab);
    fd_fields->setPlaceholderText("Fields (comma-separated, e.g. $close,$volume,$open)");
    fd_fields->setStyleSheet(input_ss());
    text_inputs_["fd_fields"] = fd_fields;
    datavl->addWidget(build_input_row("Fields", fd_fields, data_tab));
    auto* fd_start = new QLineEdit(data_tab);
    fd_start->setPlaceholderText("Start date (YYYY-MM-DD, e.g. 2019-01-01)");
    fd_start->setStyleSheet(input_ss());
    text_inputs_["fd_start"] = fd_start;
    datavl->addWidget(build_input_row("Start Date", fd_start, data_tab));
    auto* fd_end = new QLineEdit(data_tab);
    fd_end->setPlaceholderText("End date (YYYY-MM-DD, e.g. 2020-11-10)");
    fd_end->setStyleSheet(input_ss());
    text_inputs_["fd_end"] = fd_end;
    datavl->addWidget(build_input_row("End Date", fd_end, data_tab));
    auto* fd_run = make_run_button("FETCH DATA", data_tab);
    connect(fd_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching...");
        QJsonObject params;
        QJsonArray instr;
        for (auto& s : text_inputs_["fd_instruments"]->text().split(','))
            if (!s.trimmed().isEmpty())
                instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        QJsonArray fields;
        for (auto& s : text_inputs_["fd_fields"]->text().split(','))
            if (!s.trimmed().isEmpty())
                fields.append(s.trimmed());
        params["fields"] = fields;
        if (!text_inputs_["fd_start"]->text().isEmpty())
            params["start_date"] = text_inputs_["fd_start"]->text().trimmed();
        if (!text_inputs_["fd_end"]->text().isEmpty())
            params["end_date"] = text_inputs_["fd_end"]->text().trimmed();
        AIQuantLabService::instance().factor_get_data(params);
    });
    datavl->addWidget(fd_run);
    datavl->addStretch();
    tabs->addTab(data_tab, "Fetch Data");

    // ── Calendar ──
    auto* cal_tab = new QWidget(this);
    auto* calvl = new QVBoxLayout(cal_tab);
    calvl->setContentsMargins(12, 12, 12, 12);
    calvl->setSpacing(8);
    auto* cal_start = new QLineEdit(cal_tab);
    cal_start->setPlaceholderText("Start date (YYYY-MM-DD)");
    cal_start->setStyleSheet(input_ss());
    text_inputs_["fd_cal_start"] = cal_start;
    calvl->addWidget(build_input_row("Start Date", cal_start, cal_tab));
    auto* cal_end = new QLineEdit(cal_tab);
    cal_end->setPlaceholderText("End date (YYYY-MM-DD)");
    cal_end->setStyleSheet(input_ss());
    text_inputs_["fd_cal_end"] = cal_end;
    calvl->addWidget(build_input_row("End Date", cal_end, cal_tab));
    auto* cal_run = make_run_button("GET TRADING CALENDAR", cal_tab);
    connect(cal_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        QJsonObject params;
        if (!text_inputs_["fd_cal_start"]->text().isEmpty())
            params["start_date"] = text_inputs_["fd_cal_start"]->text().trimmed();
        if (!text_inputs_["fd_cal_end"]->text().isEmpty())
            params["end_date"] = text_inputs_["fd_cal_end"]->text().trimmed();
        AIQuantLabService::instance().factor_get_calendar(params);
    });
    calvl->addWidget(cal_run);
    calvl->addStretch();
    tabs->addTab(cal_tab, "Calendar");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODEL LIBRARY PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_model_library_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Model Browser ──
    auto* browse_tab = new QWidget(this);
    auto* btvl = new QVBoxLayout(browse_tab);
    btvl->setContentsMargins(12, 12, 12, 12);
    btvl->setSpacing(8);
    auto* bt_info =
        new QLabel("List all available Qlib models (LightGBM, XGBoost, LSTM, Transformer, etc.).", browse_tab);
    bt_info->setWordWrap(true);
    bt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    btvl->addWidget(bt_info);
    auto* bt_list = make_run_button("LIST ALL MODELS", browse_tab);
    connect(bt_list, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().model_list();
    });
    btvl->addWidget(bt_list);
    auto* bt_status = make_run_button("CHECK QLIB STATUS", browse_tab);
    connect(bt_status, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Checking...");
        AIQuantLabService::instance().model_check_status();
    });
    btvl->addWidget(bt_status);
    btvl->addStretch();
    tabs->addTab(browse_tab, "Browse");

    // ── Train Model ──
    auto* train_tab = new QWidget(this);
    auto* ttvl = new QVBoxLayout(train_tab);
    ttvl->setContentsMargins(12, 12, 12, 12);
    ttvl->setSpacing(8);
    auto* ml_type = new QComboBox(train_tab);
    ml_type->setStyleSheet(combo_ss());
    ml_type->addItems({"lightgbm", "xgboost", "catboost", "linear", "lstm", "gru", "transformer"});
    combo_inputs_["ml_model_type"] = ml_type;
    ttvl->addWidget(build_input_row("Model Type", ml_type, train_tab));
    auto* ml_instr = new QLineEdit(train_tab);
    ml_instr->setPlaceholderText("Instruments (comma-separated, e.g. aapl,msft)");
    ml_instr->setStyleSheet(input_ss());
    text_inputs_["ml_instruments"] = ml_instr;
    ttvl->addWidget(build_input_row("Instruments", ml_instr, train_tab));
    auto* ml_start = new QLineEdit(train_tab);
    ml_start->setPlaceholderText("Train start (YYYY-MM-DD)");
    ml_start->setStyleSheet(input_ss());
    text_inputs_["ml_start"] = ml_start;
    ttvl->addWidget(build_input_row("Start Date", ml_start, train_tab));
    auto* ml_end = new QLineEdit(train_tab);
    ml_end->setPlaceholderText("Train end (YYYY-MM-DD)");
    ml_end->setStyleSheet(input_ss());
    text_inputs_["ml_end"] = ml_end;
    ttvl->addWidget(build_input_row("End Date", ml_end, train_tab));
    auto* ml_run = make_run_button("TRAIN MODEL", train_tab);
    connect(ml_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Training...");
        QJsonObject params;
        params["model_type"] = combo_inputs_["ml_model_type"]->currentText();
        QJsonArray instr;
        for (auto& s : text_inputs_["ml_instruments"]->text().split(','))
            if (!s.trimmed().isEmpty())
                instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        if (!text_inputs_["ml_start"]->text().isEmpty())
            params["start_date"] = text_inputs_["ml_start"]->text().trimmed();
        if (!text_inputs_["ml_end"]->text().isEmpty())
            params["end_date"] = text_inputs_["ml_end"]->text().trimmed();
        AIQuantLabService::instance().model_train(params);
    });
    ttvl->addWidget(ml_run);
    ttvl->addStretch();
    tabs->addTab(train_tab, "Train Model");

    // ── Backtest ──
    auto* bt_tab = new QWidget(this);
    auto* btvl2 = new QVBoxLayout(bt_tab);
    btvl2->setContentsMargins(12, 12, 12, 12);
    btvl2->setSpacing(8);
    auto* bt_model = new QLineEdit(bt_tab);
    bt_model->setPlaceholderText("Model ID (from training output)");
    bt_model->setStyleSheet(input_ss());
    text_inputs_["ml_bt_model"] = bt_model;
    btvl2->addWidget(build_input_row("Model ID", bt_model, bt_tab));
    auto* bt_instr = new QLineEdit(bt_tab);
    bt_instr->setPlaceholderText("Instruments (comma-separated)");
    bt_instr->setStyleSheet(input_ss());
    text_inputs_["ml_bt_instr"] = bt_instr;
    btvl2->addWidget(build_input_row("Instruments", bt_instr, bt_tab));
    auto* bt_start2 = new QLineEdit(bt_tab);
    bt_start2->setPlaceholderText("Backtest start (YYYY-MM-DD)");
    bt_start2->setStyleSheet(input_ss());
    text_inputs_["ml_bt_start"] = bt_start2;
    btvl2->addWidget(build_input_row("Start Date", bt_start2, bt_tab));
    auto* bt_end2 = new QLineEdit(bt_tab);
    bt_end2->setPlaceholderText("Backtest end (YYYY-MM-DD)");
    bt_end2->setStyleSheet(input_ss());
    text_inputs_["ml_bt_end"] = bt_end2;
    btvl2->addWidget(build_input_row("End Date", bt_end2, bt_tab));
    auto* bt_run2 = make_run_button("RUN BACKTEST", bt_tab);
    connect(bt_run2, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running backtest...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ml_bt_model"]->text().trimmed();
        QJsonArray instr;
        for (auto& s : text_inputs_["ml_bt_instr"]->text().split(','))
            if (!s.trimmed().isEmpty())
                instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        if (!text_inputs_["ml_bt_start"]->text().isEmpty())
            params["start_date"] = text_inputs_["ml_bt_start"]->text().trimmed();
        if (!text_inputs_["ml_bt_end"]->text().isEmpty())
            params["end_date"] = text_inputs_["ml_bt_end"]->text().trimmed();
        AIQuantLabService::instance().model_backtest(params);
    });
    btvl2->addWidget(bt_run2);
    btvl2->addStretch();
    tabs->addTab(bt_tab, "Backtest");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}


} // namespace fincept::screens
