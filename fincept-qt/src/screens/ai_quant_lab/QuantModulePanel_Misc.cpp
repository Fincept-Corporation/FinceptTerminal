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

// ═══════════════════════════════════════════════════════════════════════════════
// LIVE SIGNALS PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_live_signals_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Signal Data ──
    auto* sig_tab = new QWidget(this);
    auto* sigvl = new QVBoxLayout(sig_tab);
    sigvl->setContentsMargins(12, 12, 12, 12);
    sigvl->setSpacing(8);
    auto* sig_instr = new QLineEdit(sig_tab);
    sig_instr->setPlaceholderText("Instruments (comma-separated, e.g. aapl,msft,goog)");
    sig_instr->setStyleSheet(input_ss());
    text_inputs_["ls_instruments"] = sig_instr;
    sigvl->addWidget(build_input_row("Instruments", sig_instr, sig_tab));
    auto* sig_fields = new QLineEdit(sig_tab);
    sig_fields->setPlaceholderText("Fields (e.g. $close,$open,$high,$low,$volume)");
    sig_fields->setStyleSheet(input_ss());
    text_inputs_["ls_fields"] = sig_fields;
    sigvl->addWidget(build_input_row("Fields", sig_fields, sig_tab));
    auto* sig_start = new QLineEdit(sig_tab);
    sig_start->setPlaceholderText("Start date (YYYY-MM-DD)");
    sig_start->setStyleSheet(input_ss());
    text_inputs_["ls_start"] = sig_start;
    sigvl->addWidget(build_input_row("Start Date", sig_start, sig_tab));
    auto* sig_end = new QLineEdit(sig_tab);
    sig_end->setPlaceholderText("End date (YYYY-MM-DD)");
    sig_end->setStyleSheet(input_ss());
    text_inputs_["ls_end"] = sig_end;
    sigvl->addWidget(build_input_row("End Date", sig_end, sig_tab));
    auto* sig_run = make_run_button("FETCH SIGNALS", sig_tab);
    connect(sig_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching...");
        QJsonObject params;
        QJsonArray instr;
        for (auto& s : text_inputs_["ls_instruments"]->text().split(','))
            if (!s.trimmed().isEmpty())
                instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        QJsonArray fields;
        for (auto& s : text_inputs_["ls_fields"]->text().split(','))
            if (!s.trimmed().isEmpty())
                fields.append(s.trimmed());
        params["fields"] = fields;
        if (!text_inputs_["ls_start"]->text().isEmpty())
            params["start_date"] = text_inputs_["ls_start"]->text().trimmed();
        if (!text_inputs_["ls_end"]->text().isEmpty())
            params["end_date"] = text_inputs_["ls_end"]->text().trimmed();
        AIQuantLabService::instance().signals_get_data(params);
    });
    sigvl->addWidget(sig_run);
    sigvl->addStretch();
    tabs->addTab(sig_tab, "Signal Data");

    // ── Factor Analysis ──
    auto* fa_tab = new QWidget(this);
    auto* favl = new QVBoxLayout(fa_tab);
    favl->setContentsMargins(12, 12, 12, 12);
    favl->setSpacing(8);
    auto* fa_instr = new QLineEdit(fa_tab);
    fa_instr->setPlaceholderText("Instruments (comma-separated)");
    fa_instr->setStyleSheet(input_ss());
    text_inputs_["ls_fa_instr"] = fa_instr;
    favl->addWidget(build_input_row("Instruments", fa_instr, fa_tab));
    auto* fa_start = new QLineEdit(fa_tab);
    fa_start->setPlaceholderText("Start date (YYYY-MM-DD)");
    fa_start->setStyleSheet(input_ss());
    text_inputs_["ls_fa_start"] = fa_start;
    favl->addWidget(build_input_row("Start Date", fa_start, fa_tab));
    auto* fa_end = new QLineEdit(fa_tab);
    fa_end->setPlaceholderText("End date (YYYY-MM-DD)");
    fa_end->setStyleSheet(input_ss());
    text_inputs_["ls_fa_end"] = fa_end;
    favl->addWidget(build_input_row("End Date", fa_end, fa_tab));
    auto* fa_run = make_run_button("RUN FACTOR ANALYSIS", fa_tab);
    connect(fa_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing...");
        QJsonObject params;
        QJsonArray instr;
        for (auto& s : text_inputs_["ls_fa_instr"]->text().split(','))
            if (!s.trimmed().isEmpty())
                instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        if (!text_inputs_["ls_fa_start"]->text().isEmpty())
            params["start_date"] = text_inputs_["ls_fa_start"]->text().trimmed();
        if (!text_inputs_["ls_fa_end"]->text().isEmpty())
            params["end_date"] = text_inputs_["ls_fa_end"]->text().trimmed();
        AIQuantLabService::instance().signals_get_factor_analysis(params);
    });
    favl->addWidget(fa_run);
    favl->addStretch();
    tabs->addTab(fa_tab, "Factor Analysis");

    // ── Feature Importance ──
    auto* fi_tab = new QWidget(this);
    auto* fivl = new QVBoxLayout(fi_tab);
    fivl->setContentsMargins(12, 12, 12, 12);
    fivl->setSpacing(8);
    auto* fi_model = new QLineEdit(fi_tab);
    fi_model->setPlaceholderText("Trained model ID");
    fi_model->setStyleSheet(input_ss());
    text_inputs_["ls_fi_model"] = fi_model;
    fivl->addWidget(build_input_row("Model ID", fi_model, fi_tab));
    auto* fi_run = make_run_button("GET FEATURE IMPORTANCE", fi_tab);
    connect(fi_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ls_fi_model"]->text().trimmed();
        AIQuantLabService::instance().signals_get_feature_importance(params);
    });
    fivl->addWidget(fi_run);
    fivl->addStretch();
    tabs->addTab(fi_tab, "Feature Importance");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ONLINE LEARNING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_online_learning_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Models ──
    auto* models_tab = new QWidget(this);
    auto* mtvl = new QVBoxLayout(models_tab);
    mtvl->setContentsMargins(12, 12, 12, 12);
    mtvl->setSpacing(8);
    auto* mt_info = new QLabel("Incrementally trained models that update on each new data point.", models_tab);
    mt_info->setWordWrap(true);
    mt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    mtvl->addWidget(mt_info);
    auto* mt_list = make_run_button("LIST ALL MODELS", models_tab);
    connect(mt_list, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().online_list_models();
    });
    mtvl->addWidget(mt_list);
    mtvl->addStretch();
    tabs->addTab(models_tab, "Models");

    // ── Create Model ──
    auto* create_tab = new QWidget(this);
    auto* ctvl = new QVBoxLayout(create_tab);
    ctvl->setContentsMargins(12, 12, 12, 12);
    ctvl->setSpacing(8);
    auto* ol_model_type = new QComboBox(create_tab);
    ol_model_type->setStyleSheet(combo_ss());
    ol_model_type->addItems({"linear", "bayesian_linear", "pa", "tree", "adaptive_tree", "bagging", "ewa", "srp"});
    combo_inputs_["ol_model_type"] = ol_model_type;
    ctvl->addWidget(build_input_row("Model Type", ol_model_type, create_tab));
    auto* ol_model_id = new QLineEdit(create_tab);
    ol_model_id->setPlaceholderText("Model ID (optional, auto-generated if blank)");
    ol_model_id->setStyleSheet(input_ss());
    text_inputs_["ol_model_id"] = ol_model_id;
    ctvl->addWidget(build_input_row("Model ID", ol_model_id, create_tab));
    auto* ol_create = make_run_button("CREATE MODEL", create_tab);
    connect(ol_create, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        params["model_type"] = combo_inputs_["ol_model_type"]->currentText();
        auto mid = text_inputs_["ol_model_id"]->text().trimmed();
        if (!mid.isEmpty())
            params["model_id"] = mid;
        AIQuantLabService::instance().online_create_model(params);
    });
    ctvl->addWidget(ol_create);
    ctvl->addStretch();
    tabs->addTab(create_tab, "Create Model");

    // ── Incremental Train ──
    auto* train_tab = new QWidget(this);
    auto* ttvl = new QVBoxLayout(train_tab);
    ttvl->setContentsMargins(12, 12, 12, 12);
    ttvl->setSpacing(8);
    auto* ol_tid = new QLineEdit(train_tab);
    ol_tid->setPlaceholderText("Model ID");
    ol_tid->setStyleSheet(input_ss());
    text_inputs_["ol_train_id"] = ol_tid;
    ttvl->addWidget(build_input_row("Model ID", ol_tid, train_tab));
    auto* ol_feats = new QLineEdit(train_tab);
    ol_feats->setPlaceholderText(R"({"close":94.0,"volume":1000000,"rsi":55.0})");
    ol_feats->setStyleSheet(input_ss());
    text_inputs_["ol_features"] = ol_feats;
    ttvl->addWidget(build_input_row("Features (JSON)", ol_feats, train_tab));
    auto* ol_target = new QLineEdit(train_tab);
    ol_target->setPlaceholderText("Target value (e.g. 0.02)");
    ol_target->setStyleSheet(input_ss());
    text_inputs_["ol_target"] = ol_target;
    ttvl->addWidget(build_input_row("Target", ol_target, train_tab));
    auto* ol_train = make_run_button("TRAIN ONE STEP", train_tab);
    connect(ol_train, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Training...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ol_train_id"]->text().trimmed();
        auto doc = QJsonDocument::fromJson(text_inputs_["ol_features"]->text().toUtf8());
        if (!doc.isNull())
            params["features"] = doc.object();
        params["target"] = text_inputs_["ol_target"]->text().trimmed().toDouble();
        AIQuantLabService::instance().online_train(params);
    });
    ttvl->addWidget(ol_train);
    ttvl->addStretch();
    tabs->addTab(train_tab, "Incremental Train");

    // ── Predict ──
    auto* pred_tab = new QWidget(this);
    auto* predvl = new QVBoxLayout(pred_tab);
    predvl->setContentsMargins(12, 12, 12, 12);
    predvl->setSpacing(8);
    auto* ol_pid = new QLineEdit(pred_tab);
    ol_pid->setPlaceholderText("Model ID");
    ol_pid->setStyleSheet(input_ss());
    text_inputs_["ol_pred_id"] = ol_pid;
    predvl->addWidget(build_input_row("Model ID", ol_pid, pred_tab));
    auto* ol_pfeats = new QLineEdit(pred_tab);
    ol_pfeats->setPlaceholderText(R"({"close":95.0,"volume":1100000,"rsi":58.0})");
    ol_pfeats->setStyleSheet(input_ss());
    text_inputs_["ol_pred_feats"] = ol_pfeats;
    predvl->addWidget(build_input_row("Features (JSON)", ol_pfeats, pred_tab));
    auto* ol_pred = make_run_button("PREDICT", pred_tab);
    connect(ol_pred, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Predicting...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ol_pred_id"]->text().trimmed();
        auto doc = QJsonDocument::fromJson(text_inputs_["ol_pred_feats"]->text().toUtf8());
        if (!doc.isNull())
            params["features"] = doc.object();
        AIQuantLabService::instance().online_predict(params);
    });
    predvl->addWidget(ol_pred);
    auto* ol_perf = make_run_button("GET PERFORMANCE", pred_tab);
    connect(ol_perf, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ol_pred_id"]->text().trimmed();
        AIQuantLabService::instance().online_performance(params);
    });
    predvl->addWidget(ol_perf);
    predvl->addStretch();
    tabs->addTab(pred_tab, "Predict");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// META LEARNING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_meta_learning_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // ── Model Selection ──
    auto* sel_tab = new QWidget(this);
    auto* selvl = new QVBoxLayout(sel_tab);
    selvl->setContentsMargins(12, 12, 12, 12);
    selvl->setSpacing(8);
    auto* sel_info = new QLabel("Automatically select the best model from a set of candidates.", sel_tab);
    sel_info->setWordWrap(true);
    sel_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY));
    selvl->addWidget(sel_info);
    auto* sel_list_btn = make_run_button("LIST AVAILABLE MODELS", sel_tab);
    connect(sel_list_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().meta_list_models();
    });
    selvl->addWidget(sel_list_btn);
    auto* sel_models = new QLineEdit(sel_tab);
    sel_models->setPlaceholderText("Model IDs (comma-separated, e.g. lightgbm,xgboost,random_forest)");
    sel_models->setStyleSheet(input_ss());
    text_inputs_["ml_sel_models"] = sel_models;
    selvl->addWidget(build_input_row("Models", sel_models, sel_tab));
    auto* sel_task = new QComboBox(sel_tab);
    sel_task->setStyleSheet(combo_ss());
    sel_task->addItems({"regression", "classification"});
    combo_inputs_["ml_sel_task"] = sel_task;
    selvl->addWidget(build_input_row("Task Type", sel_task, sel_tab));
    auto* sel_run = make_run_button("RUN MODEL SELECTION", sel_tab);
    connect(sel_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Selecting...");
        QJsonObject params;
        QJsonArray model_ids;
        for (auto& s : text_inputs_["ml_sel_models"]->text().split(','))
            if (!s.trimmed().isEmpty())
                model_ids.append(s.trimmed());
        params["model_ids"] = model_ids;
        params["task_type"] = combo_inputs_["ml_sel_task"]->currentText();
        AIQuantLabService::instance().meta_run_selection(params);
    });
    selvl->addWidget(sel_run);
    selvl->addStretch();
    tabs->addTab(sel_tab, "Model Selection");

    // ── Ensemble ──
    auto* ens_tab = new QWidget(this);
    auto* ensvl = new QVBoxLayout(ens_tab);
    ensvl->setContentsMargins(12, 12, 12, 12);
    ensvl->setSpacing(8);
    auto* ens_keys = new QLineEdit(ens_tab);
    ens_keys->setPlaceholderText("Model keys (from selection output, comma-separated)");
    ens_keys->setStyleSheet(input_ss());
    text_inputs_["ml_ens_keys"] = ens_keys;
    ensvl->addWidget(build_input_row("Model Keys", ens_keys, ens_tab));
    auto* ens_method = new QComboBox(ens_tab);
    ens_method->setStyleSheet(combo_ss());
    ens_method->addItems({"voting", "stacking", "averaging"});
    combo_inputs_["ml_ens_method"] = ens_method;
    ensvl->addWidget(build_input_row("Ensemble Method", ens_method, ens_tab));
    auto* ens_run = make_run_button("CREATE ENSEMBLE", ens_tab);
    connect(ens_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating ensemble...");
        QJsonObject params;
        QJsonArray keys;
        for (auto& s : text_inputs_["ml_ens_keys"]->text().split(','))
            if (!s.trimmed().isEmpty())
                keys.append(s.trimmed());
        params["model_keys"] = keys;
        params["method"] = combo_inputs_["ml_ens_method"]->currentText();
        AIQuantLabService::instance().meta_create_ensemble(params);
    });
    ensvl->addWidget(ens_run);
    ensvl->addStretch();
    tabs->addTab(ens_tab, "Ensemble");

    // ── Hyperparameter Tuning ──
    auto* tune_tab = new QWidget(this);
    auto* tunevl = new QVBoxLayout(tune_tab);
    tunevl->setContentsMargins(12, 12, 12, 12);
    tunevl->setSpacing(8);
    auto* tune_model = new QComboBox(tune_tab);
    tune_model->setStyleSheet(combo_ss());
    tune_model->addItems({"lightgbm", "xgboost", "random_forest", "catboost"});
    combo_inputs_["ml_tune_model"] = tune_model;
    tunevl->addWidget(build_input_row("Model", tune_model, tune_tab));
    auto* tune_grid = new QLineEdit(tune_tab);
    tune_grid->setPlaceholderText(R"({"n_estimators":[50,100,200],"max_depth":[3,5,7]})");
    tune_grid->setStyleSheet(input_ss());
    text_inputs_["ml_tune_grid"] = tune_grid;
    tunevl->addWidget(build_input_row("Param Grid (JSON)", tune_grid, tune_tab));
    auto* tune_method = new QComboBox(tune_tab);
    tune_method->setStyleSheet(combo_ss());
    tune_method->addItems({"grid", "random"});
    combo_inputs_["ml_tune_method"] = tune_method;
    tunevl->addWidget(build_input_row("Search Method", tune_method, tune_tab));
    auto* tune_run = make_run_button("TUNE HYPERPARAMETERS", tune_tab);
    connect(tune_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Tuning...");
        QJsonObject params;
        params["model_id"] = combo_inputs_["ml_tune_model"]->currentText();
        auto doc = QJsonDocument::fromJson(text_inputs_["ml_tune_grid"]->text().toUtf8());
        if (!doc.isNull())
            params["param_grid"] = doc.object();
        params["search_method"] = combo_inputs_["ml_tune_method"]->currentText();
        AIQuantLabService::instance().meta_tune_hyperparameters(params);
    });
    tunevl->addWidget(tune_run);
    auto* results_btn = make_run_button("GET ALL RESULTS", tune_tab);
    connect(results_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().meta_get_results();
    });
    tunevl->addWidget(results_btn);
    tunevl->addStretch();
    tabs->addTab(tune_tab, "Hyperparameter Tuning");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HFT PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_hft_panel() {
    const QString accent = module_.color.name();

    auto* w  = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Shared exchange/symbol bar ────────────────────────────────────────────
    auto* top_bar = new QWidget(w);
    top_bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* top_hl = new QHBoxLayout(top_bar);
    top_hl->setContentsMargins(12, 8, 12, 8);
    top_hl->setSpacing(8);

    auto* exch_lbl = new QLabel("EXCHANGE", top_bar);
    exch_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;")
                                .arg(ui::colors::TEXT_TERTIARY()));
    auto* hft_exchange = new QComboBox(top_bar);
    hft_exchange->addItems({"binance", "kraken", "coinbase", "bybit", "okx", "hyperliquid"});
    hft_exchange->setStyleSheet(combo_ss());
    hft_exchange->setFixedWidth(110);
    combo_inputs_["hft_exchange"] = hft_exchange;

    auto* sym_lbl = new QLabel("SYMBOL", top_bar);
    sym_lbl->setStyleSheet(exch_lbl->styleSheet());
    auto* hft_symbol = new QLineEdit(top_bar);
    hft_symbol->setText("BTC/USDT");
    hft_symbol->setFixedWidth(100);
    hft_symbol->setStyleSheet(input_ss());
    hft_symbol->setToolTip("e.g. BTC/USDT, ETH/USDT");
    text_inputs_["hft_symbol"] = hft_symbol;

    auto* depth_lbl = new QLabel("DEPTH", top_bar);
    depth_lbl->setStyleSheet(exch_lbl->styleSheet());
    auto* hft_depth = new QComboBox(top_bar);
    hft_depth->addItems({"10", "20", "50", "100"});
    hft_depth->setCurrentIndex(1);
    hft_depth->setFixedWidth(60);
    hft_depth->setStyleSheet(combo_ss());
    combo_inputs_["hft_depth"] = hft_depth;

    top_hl->addWidget(exch_lbl);
    top_hl->addWidget(hft_exchange);
    top_hl->addWidget(sym_lbl);
    top_hl->addWidget(hft_symbol);
    top_hl->addWidget(depth_lbl);
    top_hl->addWidget(hft_depth);
    top_hl->addStretch();

    // Live latency badge
    auto* latency_lbl = new QLabel("LATENCY —", top_bar);
    latency_lbl->setObjectName("hftLatency");
    latency_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-family:'Courier New'; background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    top_hl->addWidget(latency_lbl);
    vl->addWidget(top_bar);

    // ── Tab widget ────────────────────────────────────────────────────────────
    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(accent));

    // ════════════════════════════════════════════════════════
    // TAB 1 — LIVE ORDER BOOK
    // ════════════════════════════════════════════════════════
    auto* ob_tab  = new QWidget(this);
    auto* ob_root = new QVBoxLayout(ob_tab);
    ob_root->setContentsMargins(12, 10, 12, 10);
    ob_root->setSpacing(8);

    // Controls row
    auto* ob_ctrl = new QHBoxLayout;
    ob_ctrl->setSpacing(8);
    auto* ob_fetch = make_run_button("FETCH LIVE ORDER BOOK", ob_tab);
    connect(ob_fetch, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching live order book...");
        QJsonObject p;
        p["exchange"] = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]   = text_inputs_["hft_symbol"]->text().trimmed();
        p["depth"]    = combo_inputs_["hft_depth"]->currentText().toInt();
        AIQuantLabService::instance().hft_create_orderbook(p);
    });
    ob_ctrl->addWidget(ob_fetch);
    ob_ctrl->addStretch();
    ob_root->addLayout(ob_ctrl);

    // Metrics row — 5 cards in a grid
    auto* metrics_row = new QHBoxLayout;
    metrics_row->setSpacing(6);

    auto make_card = [&](const QString& label, QWidget* parent) -> QPair<QWidget*, QLabel*> {
        auto* card = new QWidget(parent);
        card->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(10, 6, 10, 6);
        cvl->setSpacing(2);
        auto* l = new QLabel(label, card);
        l->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                             .arg(ui::colors::TEXT_TERTIARY()));
        auto* v = new QLabel("—", card);
        v->setObjectName("hftCardVal");
        v->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; font-family:'Courier New'; background:transparent;")
                             .arg(ui::colors::TEXT_PRIMARY()));
        cvl->addWidget(l);
        cvl->addWidget(v);
        return {card, v};
    };

    auto [mid_card, mid_val]         = make_card("MID PRICE", ob_tab);
    auto [spread_card, spread_val]   = make_card("SPREAD BPS", ob_tab);
    auto [obi_card, obi_val]         = make_card("ORDER BOOK IMBALANCE", ob_tab);
    auto [pressure_card, pres_val]   = make_card("PRESSURE", ob_tab);
    auto [wmid_card, wmid_val]       = make_card("WEIGHTED MID", ob_tab);

    // Store for result update
    mid_val->setObjectName("hft_mid_val");
    spread_val->setObjectName("hft_spread_val");
    obi_val->setObjectName("hft_obi_val");
    pres_val->setObjectName("hft_pressure_val");
    wmid_val->setObjectName("hft_wmid_val");

    metrics_row->addWidget(mid_card, 1);
    metrics_row->addWidget(spread_card, 1);
    metrics_row->addWidget(obi_card, 1);
    metrics_row->addWidget(pressure_card, 1);
    metrics_row->addWidget(wmid_card, 1);
    ob_root->addLayout(metrics_row);

    // Bid / Ask tables side by side
    auto* books_row = new QHBoxLayout;
    books_row->setSpacing(8);

    // Bids
    auto* bids_frame = new QWidget(ob_tab);
    bids_frame->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* bids_vl = new QVBoxLayout(bids_frame);
    bids_vl->setContentsMargins(0, 0, 0, 0);
    bids_vl->setSpacing(0);
    auto* bids_hdr = new QLabel("  BIDS", bids_frame);
    bids_hdr->setFixedHeight(24);
    bids_hdr->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;"
                                    "background:%2; border-bottom:1px solid %3;")
                                .arg(ui::colors::POSITIVE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* bid_table = new QTableWidget(10, 3, bids_frame);
    bid_table->setObjectName("hft_bid_table");
    bid_table->setHorizontalHeaderLabels({"Price", "Size", "Cumulative"});
    bid_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bid_table->verticalHeader()->setVisible(false);
    bid_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bid_table->setSelectionMode(QAbstractItemView::NoSelection);
    bid_table->setShowGrid(false);
    bid_table->setStyleSheet(
        QString("QTableWidget { background:%1; border:none; }"
                "QHeaderView::section { background:%2; color:%3; font-size:9px; font-weight:700;"
                "  padding:3px 6px; border:none; border-bottom:1px solid %4; }"
                "QTableWidget::item { padding:2px 6px; font-family:'Courier New'; font-size:10px; border:none; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::BG_SURFACE(),
                 ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM()));
    bids_vl->addWidget(bids_hdr);
    bids_vl->addWidget(bid_table, 1);

    // Asks
    auto* asks_frame = new QWidget(ob_tab);
    asks_frame->setStyleSheet(bids_frame->styleSheet());
    auto* asks_vl = new QVBoxLayout(asks_frame);
    asks_vl->setContentsMargins(0, 0, 0, 0);
    asks_vl->setSpacing(0);
    auto* asks_hdr = new QLabel("  ASKS", asks_frame);
    asks_hdr->setFixedHeight(24);
    asks_hdr->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;"
                                    "background:%2; border-bottom:1px solid %3;")
                                .arg(ui::colors::NEGATIVE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* ask_table = new QTableWidget(10, 3, asks_frame);
    ask_table->setObjectName("hft_ask_table");
    ask_table->setHorizontalHeaderLabels({"Price", "Size", "Cumulative"});
    ask_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ask_table->verticalHeader()->setVisible(false);
    ask_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ask_table->setSelectionMode(QAbstractItemView::NoSelection);
    ask_table->setShowGrid(false);
    ask_table->setStyleSheet(bid_table->styleSheet());
    asks_vl->addWidget(asks_hdr);
    asks_vl->addWidget(ask_table, 1);

    books_row->addWidget(bids_frame, 1);
    books_row->addWidget(asks_frame, 1);
    ob_root->addLayout(books_row, 1);
    tabs->addTab(ob_tab, "Live Order Book");

    // ════════════════════════════════════════════════════════
    // TAB 2 — MICROSTRUCTURE (Market Making + Toxic Flow)
    // ════════════════════════════════════════════════════════
    auto* micro_tab  = new QWidget(this);
    auto* micro_root = new QVBoxLayout(micro_tab);
    micro_root->setContentsMargins(12, 10, 12, 10);
    micro_root->setSpacing(10);

    // ─ Market Making section ─
    auto* mm_section = new QWidget(micro_tab);
    mm_section->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* mm_vl = new QVBoxLayout(mm_section);
    mm_vl->setContentsMargins(12, 10, 12, 10);
    mm_vl->setSpacing(8);

    auto* mm_title = new QLabel("MARKET MAKING  —  Avellaneda-Stoikov Model", mm_section);
    mm_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                                .arg(accent));
    mm_vl->addWidget(mm_title);

    auto* mm_params = new QHBoxLayout;
    mm_params->setSpacing(8);
    auto* inv_spin = make_double_spin(-1000, 1000, 0.0, 4, " units", mm_section);
    int_inputs_["hft_inventory"] = nullptr;
    double_inputs_["hft_inventory_d"] = inv_spin;
    mm_params->addWidget(build_input_row("Inventory", inv_spin, mm_section));
    auto* spread_spin = make_double_spin(1.0, 10.0, 1.5, 2, "×", mm_section);
    double_inputs_["hft_spread_mult"] = spread_spin;
    mm_params->addWidget(build_input_row("Spread Mult", spread_spin, mm_section));
    auto* risk_spin = make_double_spin(0.001, 0.1, 0.01, 3, "", mm_section);
    double_inputs_["hft_risk_aversion"] = risk_spin;
    mm_params->addWidget(build_input_row("Risk Aversion", risk_spin, mm_section));
    mm_vl->addLayout(mm_params);

    auto* mm_run = make_run_button("CALCULATE OPTIMAL QUOTES", mm_section);
    connect(mm_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching live data + computing quotes...");
        QJsonObject p;
        p["exchange"]         = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]           = text_inputs_["hft_symbol"]->text().trimmed();
        p["inventory"]        = double_inputs_["hft_inventory_d"]->value();
        p["spread_multiplier"] = double_inputs_["hft_spread_mult"]->value();
        p["risk_aversion"]    = double_inputs_["hft_risk_aversion"]->value();
        AIQuantLabService::instance().hft_market_making_quotes(p);
    });
    mm_vl->addWidget(mm_run);

    // MM result cards
    auto* mm_results = new QHBoxLayout;
    mm_results->setSpacing(6);
    auto [bid_card, bid_val2] = make_card("BID QUOTE", mm_section);
    auto [ask_card, ask_val2] = make_card("ASK QUOTE", mm_section);
    auto [qs_card, qs_val]    = make_card("QUOTED SPREAD BPS", mm_section);
    auto [edge_card, edge_val] = make_card("EDGE/SIDE BPS", mm_section);
    auto [rec_card, rec_val]  = make_card("RECOMMENDATION", mm_section);
    bid_val2->setObjectName("hft_mm_bid");
    ask_val2->setObjectName("hft_mm_ask");
    qs_val->setObjectName("hft_mm_qspread");
    edge_val->setObjectName("hft_mm_edge");
    rec_val->setObjectName("hft_mm_rec");
    mm_results->addWidget(bid_card, 1);
    mm_results->addWidget(ask_card, 1);
    mm_results->addWidget(qs_card, 1);
    mm_results->addWidget(edge_card, 1);
    mm_results->addWidget(rec_card, 1);
    mm_vl->addLayout(mm_results);
    micro_root->addWidget(mm_section);

    // ─ Toxic Flow section ─
    auto* tox_section = new QWidget(micro_tab);
    tox_section->setStyleSheet(mm_section->styleSheet());
    auto* tox_vl = new QVBoxLayout(tox_section);
    tox_vl->setContentsMargins(12, 10, 12, 10);
    tox_vl->setSpacing(8);

    auto* tox_title = new QLabel("TOXIC FLOW DETECTION  —  PIN Score Model", tox_section);
    tox_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                                 .arg(accent));
    tox_vl->addWidget(tox_title);

    auto* tox_params = new QHBoxLayout;
    tox_params->setSpacing(8);
    auto* tox_limit = new QComboBox(tox_section);
    tox_limit->addItems({"50", "100", "200", "500"});
    tox_limit->setCurrentIndex(2);
    tox_limit->setStyleSheet(combo_ss());
    combo_inputs_["hft_tox_limit"] = tox_limit;
    tox_params->addWidget(build_input_row("Trade History", tox_limit, tox_section));
    tox_params->addStretch();
    tox_vl->addLayout(tox_params);

    auto* tox_run = make_run_button("DETECT TOXIC FLOW", tox_section);
    connect(tox_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching trades + analyzing flow...");
        QJsonObject p;
        p["exchange"] = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]   = text_inputs_["hft_symbol"]->text().trimmed();
        p["limit"]    = combo_inputs_["hft_tox_limit"]->currentText().toInt();
        AIQuantLabService::instance().hft_detect_toxic(p);
    });
    tox_vl->addWidget(tox_run);

    auto* tox_results = new QHBoxLayout;
    tox_results->setSpacing(6);
    auto [pin_card, pin_val]      = make_card("PIN SCORE (0-100)", tox_section);
    auto [vol_card, vol_val]      = make_card("VOLUME IMBALANCE", tox_section);
    auto [impact_card, impact_val] = make_card("PRICE IMPACT BPS", tox_section);
    auto [class_card, class_val]  = make_card("CLASSIFICATION", tox_section);
    auto [action_card, action_val] = make_card("RECOMMENDED ACTION", tox_section);
    pin_val->setObjectName("hft_tox_pin");
    vol_val->setObjectName("hft_tox_vol");
    impact_val->setObjectName("hft_tox_impact");
    class_val->setObjectName("hft_tox_class");
    action_val->setObjectName("hft_tox_action");
    tox_results->addWidget(pin_card, 1);
    tox_results->addWidget(vol_card, 1);
    tox_results->addWidget(impact_card, 1);
    tox_results->addWidget(class_card, 1);
    tox_results->addWidget(action_card, 1);
    tox_vl->addLayout(tox_results);
    micro_root->addWidget(tox_section);
    micro_root->addStretch();
    tabs->addTab(micro_tab, "Microstructure");

    // ════════════════════════════════════════════════════════
    // TAB 3 — SLIPPAGE ESTIMATOR
    // ════════════════════════════════════════════════════════
    auto* slip_tab  = new QWidget(this);
    auto* slip_root = new QVBoxLayout(slip_tab);
    slip_root->setContentsMargins(12, 10, 12, 10);
    slip_root->setSpacing(10);

    auto* slip_section = new QWidget(slip_tab);
    slip_section->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* slip_vl = new QVBoxLayout(slip_section);
    slip_vl->setContentsMargins(12, 10, 12, 10);
    slip_vl->setSpacing(8);

    auto* slip_title = new QLabel("SLIPPAGE ESTIMATOR  —  Real Order Book Walk", slip_section);
    slip_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                                  .arg(accent));
    slip_vl->addWidget(slip_title);

    auto* slip_desc = new QLabel(
        "Walks the live order book level-by-level to compute actual fill price and slippage for a given order size.", slip_section);
    slip_desc->setWordWrap(true);
    slip_desc->setStyleSheet(QString("color:%1; font-size:10px; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    slip_vl->addWidget(slip_desc);

    auto* slip_params = new QHBoxLayout;
    slip_params->setSpacing(8);
    auto* slip_side = new QComboBox(slip_section);
    slip_side->addItems({"buy", "sell"});
    slip_side->setStyleSheet(combo_ss());
    combo_inputs_["hft_slip_side"] = slip_side;
    slip_params->addWidget(build_input_row("Side", slip_side, slip_section));
    auto* slip_qty = make_double_spin(0.0001, 1'000'000, 1.0, 6, "", slip_section);
    double_inputs_["hft_slip_qty"] = slip_qty;
    slip_params->addWidget(build_input_row("Quantity", slip_qty, slip_section));
    slip_vl->addLayout(slip_params);

    auto* slip_run = make_run_button("ESTIMATE SLIPPAGE", slip_section);
    connect(slip_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Walking order book...");
        QJsonObject p;
        p["exchange"] = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]   = text_inputs_["hft_symbol"]->text().trimmed();
        p["side"]     = combo_inputs_["hft_slip_side"]->currentText();
        p["quantity"] = double_inputs_["hft_slip_qty"]->value();
        AIQuantLabService::instance().hft_execute_order(p);
    });
    slip_vl->addWidget(slip_run);

    // Slippage result cards
    auto* slip_results = new QHBoxLayout;
    slip_results->setSpacing(6);
    auto [avgp_card, avgp_val]     = make_card("AVG FILL PRICE", slip_section);
    auto [slbps_card, slbps_val]   = make_card("SLIPPAGE BPS", slip_section);
    auto [cost_card, cost_val]     = make_card("TOTAL COST", slip_section);
    auto [fills_card, fills_val]   = make_card("FILL LEVELS", slip_section);
    auto [viable_card, viable_val] = make_card("VIABILITY", slip_section);
    avgp_val->setObjectName("hft_slip_avgp");
    slbps_val->setObjectName("hft_slip_bps");
    cost_val->setObjectName("hft_slip_cost");
    fills_val->setObjectName("hft_slip_fills");
    viable_val->setObjectName("hft_slip_viable");
    slip_results->addWidget(avgp_card, 1);
    slip_results->addWidget(slbps_card, 1);
    slip_results->addWidget(cost_card, 1);
    slip_results->addWidget(fills_card, 1);
    slip_results->addWidget(viable_card, 1);
    slip_vl->addLayout(slip_results);

    // Fills table
    auto* fills_table = new QTableWidget(0, 3, slip_section);
    fills_table->setObjectName("hft_slip_table");
    fills_table->setHorizontalHeaderLabels({"Fill Price", "Quantity", "Cost"});
    fills_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    fills_table->verticalHeader()->setVisible(false);
    fills_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fills_table->setSelectionMode(QAbstractItemView::NoSelection);
    fills_table->setShowGrid(false);
    fills_table->setMaximumHeight(160);
    fills_table->setStyleSheet(
        QString("QTableWidget { background:%1; border:1px solid %2; }"
                "QHeaderView::section { background:%3; color:%4; font-size:9px; font-weight:700;"
                "  padding:3px 6px; border:none; border-bottom:1px solid %2; }"
                "QTableWidget::item { padding:2px 6px; font-family:'Courier New'; font-size:10px; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(),
                 ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY()));
    slip_vl->addWidget(fills_table);

    slip_root->addWidget(slip_section);
    slip_root->addStretch();
    tabs->addTab(slip_tab, "Slippage Estimator");

    // ── Full Analyze button (bottom bar) ─────────────────────────────────────
    auto* bottom_bar = new QWidget(w);
    bottom_bar->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                                  .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* bot_hl = new QHBoxLayout(bottom_bar);
    bot_hl->setContentsMargins(12, 6, 12, 6);
    auto* analyze_btn = make_run_button("⚡ FULL ANALYSIS — FETCH ALL & COMPUTE", bottom_bar);
    analyze_btn->setToolTip("Fetches live order book + trades, computes book metrics, market making quotes, toxic flow, and slippage in one call");
    connect(analyze_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running full microstructure analysis...");
        QJsonObject p;
        p["exchange"]          = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]            = text_inputs_["hft_symbol"]->text().trimmed();
        p["depth"]             = combo_inputs_["hft_depth"]->currentText().toInt();
        p["limit"]             = combo_inputs_["hft_tox_limit"]->currentText().toInt();
        p["inventory"]         = double_inputs_["hft_inventory_d"]->value();
        p["spread_multiplier"] = double_inputs_["hft_spread_mult"]->value();
        p["quantity"]          = double_inputs_["hft_slip_qty"]->value();
        AIQuantLabService::instance().hft_snapshot(p);
    });
    bot_hl->addWidget(analyze_btn, 1);

    vl->addWidget(tabs, 1);
    vl->addWidget(bottom_bar);

    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ROLLING RETRAINING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_rolling_retraining_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    const QString accent = module_.color.name();
    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(accent));

    // ── Tab 1: Schedules ─────────────────────────────────────────────────────
    auto* list_tab = new QWidget;
    auto* ltvl = new QVBoxLayout(list_tab);
    ltvl->setContentsMargins(12, 12, 12, 12);
    ltvl->setSpacing(10);

    auto* lt_info = new QLabel(
        "Schedules are persisted in ~/.fincept/rolling_schedules.json. "
        "Click Refresh to load the latest state.", list_tab);
    lt_info->setWordWrap(true);
    lt_info->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
    ltvl->addWidget(lt_info);

    // Schedule cards container (scrollable)
    auto* scroll = new QScrollArea(list_tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea{background:transparent;border:none;}"
                                  "QScrollBar:vertical{background:%1;width:6px;border-radius:3px;}"
                                  "QScrollBar::handle:vertical{background:%2;border-radius:3px;}")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));
    auto* cards_w = new QWidget;
    cards_w->setStyleSheet("background:transparent;");
    auto* cards_vl = new QVBoxLayout(cards_w);
    cards_vl->setContentsMargins(0, 0, 0, 0);
    cards_vl->setSpacing(8);
    cards_vl->addStretch();
    scroll->setWidget(cards_w);
    ltvl->addWidget(scroll, 1);

    // Store pointer so result handler can repopulate cards
    // We tag the cards container via objectName
    cards_w->setObjectName("rr_cards_container");

    auto* lt_run = make_run_button("REFRESH SCHEDULES", list_tab);
    connect(lt_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading schedules...");
        AIQuantLabService::instance().rolling_list_schedules();
    });
    ltvl->addWidget(lt_run);
    tabs->addTab(list_tab, "Schedules");

    // ── Tab 2: Create Schedule ───────────────────────────────────────────────
    auto* create_tab = new QWidget;
    auto* ctvl = new QVBoxLayout(create_tab);
    ctvl->setContentsMargins(12, 12, 12, 12);
    ctvl->setSpacing(8);

    auto* rr_model_id = new QLineEdit(create_tab);
    rr_model_id->setPlaceholderText("Unique model ID (e.g. lgbm_sp500)");
    rr_model_id->setStyleSheet(input_ss());
    text_inputs_["rr_model_id"] = rr_model_id;
    ctvl->addWidget(build_input_row("Model ID *", rr_model_id, create_tab));

    auto* rr_conf = new QLineEdit(create_tab);
    rr_conf->setPlaceholderText("Optional: path to Qlib YAML config (leave blank for built-in LightGBM+Alpha158)");
    rr_conf->setStyleSheet(input_ss());
    text_inputs_["rr_conf_path"] = rr_conf;
    ctvl->addWidget(build_input_row("Config Path", rr_conf, create_tab));

    auto* rr_freq = new QComboBox(create_tab);
    rr_freq->setStyleSheet(combo_ss());
    rr_freq->addItems({"daily", "weekly", "monthly"});
    combo_inputs_["rr_frequency"] = rr_freq;
    ctvl->addWidget(build_input_row("Frequency", rr_freq, create_tab));

    auto* rr_window = new QLineEdit(create_tab);
    rr_window->setPlaceholderText("Rolling window in trading days (default: 252)");
    rr_window->setStyleSheet(input_ss());
    text_inputs_["rr_window"] = rr_window;
    ctvl->addWidget(build_input_row("Window (days)", rr_window, create_tab));

    auto* rr_step = new QLineEdit(create_tab);
    rr_step->setPlaceholderText("Step size between windows (default: 20)");
    rr_step->setStyleSheet(input_ss());
    text_inputs_["rr_step"] = rr_step;
    ctvl->addWidget(build_input_row("Step (days)", rr_step, create_tab));

    // Region selector
    auto* rr_region = new QComboBox(create_tab);
    rr_region->setStyleSheet(combo_ss());
    rr_region->addItems({"us", "cn"});
    combo_inputs_["rr_region"] = rr_region;
    ctvl->addWidget(build_input_row("Data Region", rr_region, create_tab));

    // Preview + Create buttons side by side
    auto* btn_row = new QWidget(create_tab);
    auto* btn_hl = new QHBoxLayout(btn_row);
    btn_hl->setContentsMargins(0, 4, 0, 0);
    btn_hl->setSpacing(8);

    auto* rr_preview = make_run_button("PREVIEW WINDOWS", btn_row);
    rr_preview->setStyleSheet(QString(
        "QPushButton{background:%1;color:%2;border:1px solid %1;border-radius:4px;"
        "font-size:11px;font-weight:700;padding:6px 14px;}"
        "QPushButton:hover{background:%3;}")
        .arg(ui::colors::BG_RAISED(), accent, ui::colors::BG_HOVER()));
    connect(rr_preview, &QPushButton::clicked, this, [this]() {
        const QString mid = text_inputs_["rr_model_id"]->text().trimmed();
        const QString cp  = text_inputs_["rr_conf_path"]->text().trimmed();
        if (mid.isEmpty() && cp.isEmpty()) {
            status_label_->setText("Enter a Model ID or Config Path to preview.");
            return;
        }
        status_label_->setText("Generating preview...");
        QJsonObject params;
        if (!mid.isEmpty()) params["model_id"] = mid;
        if (!cp.isEmpty())  params["conf_path"] = cp;
        const QString step = text_inputs_["rr_step"]->text().trimmed();
        if (!step.isEmpty()) params["step"] = step.toInt();
        AIQuantLabService::instance().rolling_preview_tasks(params);
    });
    btn_hl->addWidget(rr_preview, 1);

    auto* rr_create = make_run_button("CREATE SCHEDULE", btn_row);
    connect(rr_create, &QPushButton::clicked, this, [this]() {
        const QString mid = text_inputs_["rr_model_id"]->text().trimmed();
        if (mid.isEmpty()) {
            status_label_->setText("Model ID is required.");
            return;
        }
        status_label_->setText("Creating schedule...");
        QJsonObject params;
        params["model_id"]  = mid;
        params["frequency"] = combo_inputs_["rr_frequency"]->currentText();
        params["region"]    = combo_inputs_["rr_region"]->currentText();
        const QString cp    = text_inputs_["rr_conf_path"]->text().trimmed();
        if (!cp.isEmpty()) params["conf_path"] = cp;
        const QString win   = text_inputs_["rr_window"]->text().trimmed();
        if (!win.isEmpty()) params["window"] = win.toInt();
        const QString step  = text_inputs_["rr_step"]->text().trimmed();
        if (!step.isEmpty()) params["step"] = step.toInt();
        AIQuantLabService::instance().rolling_create_schedule(params);
    });
    btn_hl->addWidget(rr_create, 1);
    ctvl->addWidget(btn_row);
    ctvl->addStretch();
    tabs->addTab(create_tab, "Create Schedule");

    // ── Tab 3: Execute Retrain ───────────────────────────────────────────────
    auto* retrain_tab = new QWidget;
    auto* retrainvl = new QVBoxLayout(retrain_tab);
    retrainvl->setContentsMargins(12, 12, 12, 12);
    retrainvl->setSpacing(10);

    auto* rt_info = new QLabel(
        "Executes a full rolling retrain for a scheduled model. Each window trains "
        "independently and progress is streamed live below.", retrain_tab);
    rt_info->setWordWrap(true);
    rt_info->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
    retrainvl->addWidget(rt_info);

    auto* rr_exec_id = new QLineEdit(retrain_tab);
    rr_exec_id->setPlaceholderText("Model ID (must be in Schedules)");
    rr_exec_id->setStyleSheet(input_ss());
    text_inputs_["rr_exec_id"] = rr_exec_id;
    retrainvl->addWidget(build_input_row("Model ID *", rr_exec_id, retrain_tab));

    // Progress bar
    auto* rr_progress = new QProgressBar(retrain_tab);
    rr_progress->setObjectName("rr_progress");
    rr_progress->setRange(0, 100);
    rr_progress->setValue(0);
    rr_progress->setTextVisible(true);
    rr_progress->setFormat("Idle");
    rr_progress->setStyleSheet(QString(
        "QProgressBar{background:%1;border:1px solid %2;border-radius:4px;"
        "color:%3;font-size:11px;font-weight:600;text-align:center;height:20px;}"
        "QProgressBar::chunk{background:%4;border-radius:3px;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(),
             ui::colors::TEXT_PRIMARY(), accent));
    retrainvl->addWidget(rr_progress);

    // Streaming log output
    auto* rr_log = new QTextEdit(retrain_tab);
    rr_log->setObjectName("rr_log");
    rr_log->setReadOnly(true);
    rr_log->setPlaceholderText("Training progress will stream here...");
    rr_log->setStyleSheet(output_ss());
    rr_log->setMinimumHeight(140);
    retrainvl->addWidget(rr_log, 1);

    auto* rr_exec = make_run_button("EXECUTE RETRAIN NOW", retrain_tab);
    connect(rr_exec, &QPushButton::clicked, this, [this]() {
        const QString mid = text_inputs_["rr_exec_id"]->text().trimmed();
        if (mid.isEmpty()) {
            status_label_->setText("Model ID is required.");
            return;
        }
        // Reset progress UI
        if (auto* pb = this->findChild<QProgressBar*>("rr_progress")) {
            pb->setValue(0);
            pb->setFormat("Starting...");
        }
        if (auto* log = this->findChild<QTextEdit*>("rr_log"))
            log->clear();
        status_label_->setText("Retraining...");
        QJsonObject params;
        params["model_id"] = mid;
        AIQuantLabService::instance().rolling_execute_retrain(params);
    });
    retrainvl->addWidget(rr_exec);
    tabs->addTab(retrain_tab, "Execute Retrain");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

} // namespace fincept::screens
