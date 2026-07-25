// src/screens/ai_quant_lab/QuantModulePanel_Misc.cpp
//
// Miscellaneous panel builders — Advanced Models, Feature Engineering,
// Portfolio Opt, Factor Evaluation, Strategy Builder, Data Processors, Factor
// Discovery, Model Library. Extracted from QuantModulePanel.cpp to keep that
// file maintainable.
//
// Live Signals / Online Learning / Meta Learning / Rolling Retraining live in
// QuantModulePanel_LiveLearning.cpp; HFT in QuantModulePanel_HFT.cpp;
// RL Trading in QuantModulePanel_RL.cpp.
#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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
    vl->addWidget(build_input_row(tr("Model Type"), model_type, w));

    auto* hidden = new QSpinBox(w);
    hidden->setRange(16, 1024);
    hidden->setValue(64);
    hidden->setStyleSheet(spinbox_ss());
    int_inputs_["adv_hidden"] = hidden;
    vl->addWidget(build_input_row(tr("Hidden Size"), hidden, w));

    auto* layers = new QSpinBox(w);
    layers->setRange(1, 12);
    layers->setValue(2);
    layers->setStyleSheet(hidden->styleSheet());
    int_inputs_["adv_layers"] = layers;
    vl->addWidget(build_input_row(tr("Num Layers"), layers, w));

    auto* dropout = make_double_spin(0, 0.9, 0.1, 2, "", w);
    double_inputs_["adv_dropout"] = dropout;
    vl->addWidget(build_input_row(tr("Dropout"), dropout, w));

    auto* epochs = new QSpinBox(w);
    epochs->setRange(1, 1000);
    epochs->setValue(50);
    epochs->setStyleSheet(hidden->styleSheet());
    int_inputs_["adv_epochs"] = epochs;
    vl->addWidget(build_input_row(tr("Epochs"), epochs, w));

    auto* run = make_run_button(tr("CREATE & TRAIN MODEL"), w);
    connect(run, &QPushButton::clicked, this, [this]() {
        show_loading(tr("Creating and training %1 (%2 epochs) — this can take several minutes...")
                         .arg(combo_inputs_["adv_model"]->currentText())
                         .arg(int_inputs_["adv_epochs"]->value()));
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
    ind_data->setPlaceholderText(tr("Price data (comma-separated, e.g. 100,102,101,105,108)"));
    ind_data->setStyleSheet(input_ss());
    text_inputs_["fe_data"] = ind_data;
    ivl->addWidget(build_input_row(tr("Price Data"), ind_data, ind));

    auto* ind_indicator = new QComboBox(ind);
    ind_indicator->addItems({"moving_average", "rsi", "macd", "bollinger_bands", "momentum", "volatility", "returns",
                             "log_returns", "drawdown"});
    ind_indicator->setStyleSheet(combo_ss());
    combo_inputs_["fe_indicator"] = ind_indicator;
    ivl->addWidget(build_input_row(tr("Indicator"), ind_indicator, ind));

    auto* ind_window = new QSpinBox(ind);
    ind_window->setRange(2, 200);
    ind_window->setValue(14);
    ind_window->setStyleSheet(spinbox_ss());
    int_inputs_["fe_window"] = ind_window;
    ivl->addWidget(build_input_row(tr("Window"), ind_window, ind));

    auto* ind_run = make_run_button(tr("COMPUTE INDICATOR"), ind);
    connect(ind_run, &QPushButton::clicked, this, [this]() {
        QJsonArray series;
        QString bad;
        if (!parse_doubles(text_inputs_["fe_data"]->text(), series, &bad)) {
            display_error(tr("Price Data: '%1' is not numeric.").arg(bad));
            return;
        }
        const int window = int_inputs_["fe_window"]->value();
        if (series.size() < window) {
            display_error(
                tr("A %1-period window needs at least %1 prices; you provided %2.").arg(window).arg(series.size()));
            return;
        }
        show_loading(
            tr("Computing %1 over %2 prices...").arg(combo_inputs_["fe_indicator"]->currentText()).arg(series.size()));
        QJsonObject params;
        params["data"] = text_inputs_["fe_data"]->text();
        params["indicator"] = combo_inputs_["fe_indicator"]->currentText();
        params["window"] = window;
        AIQuantLabService::instance().feature_compute(params);
    });
    ivl->addWidget(ind_run);
    ivl->addStretch();
    tabs->addTab(ind, tr("Indicators"));

    // ── Feature Selection tab ──
    auto* sel = new QWidget(this);
    auto* svl = new QVBoxLayout(sel);
    svl->setContentsMargins(12, 12, 12, 12);
    svl->setSpacing(8);

    auto* sel_features = new QLineEdit(sel);
    sel_features->setPlaceholderText(tr("Feature values JSON: {\"rsi\":[...],\"macd\":[...]}"));
    sel_features->setStyleSheet(input_ss());
    text_inputs_["fe_sel_features"] = sel_features;
    svl->addWidget(build_input_row(tr("Features (JSON)"), sel_features, sel));

    auto* sel_returns = new QLineEdit(sel);
    sel_returns->setPlaceholderText(tr("Target returns (comma-separated)"));
    sel_returns->setStyleSheet(input_ss());
    text_inputs_["fe_sel_returns"] = sel_returns;
    svl->addWidget(build_input_row(tr("Returns"), sel_returns, sel));

    auto* sel_topk = new QSpinBox(sel);
    sel_topk->setRange(1, 50);
    sel_topk->setValue(5);
    sel_topk->setStyleSheet(spinbox_ss());
    int_inputs_["fe_topk"] = sel_topk;
    svl->addWidget(build_input_row(tr("Top-K Features"), sel_topk, sel));

    auto* sel_run = make_run_button(tr("SELECT FEATURES BY IC"), sel);
    connect(sel_run, &QPushButton::clicked, this, [this]() {
        // JSON parse failures used to be swallowed: params["features"] was simply
        // omitted and the run failed server-side with an unrelated message.
        QJsonParseError perr{};
        const auto doc = QJsonDocument::fromJson(text_inputs_["fe_sel_features"]->text().toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            display_error(
                tr("Features must be a JSON object like {\"rsi\":[…],\"macd\":[…]} — %1").arg(perr.errorString()));
            return;
        }
        QJsonArray rets;
        QString bad;
        if (!parse_doubles(text_inputs_["fe_sel_returns"]->text(), rets, &bad)) {
            display_error(tr("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (rets.isEmpty()) {
            display_error(tr("Enter the target return series."));
            return;
        }
        show_loading(tr("Ranking %1 feature(s) by IC...").arg(doc.object().size()));
        QJsonObject params;
        params["features"] = doc.object();
        params["returns"] = text_inputs_["fe_sel_returns"]->text();
        params["top_k"] = int_inputs_["fe_topk"]->value();
        AIQuantLabService::instance().feature_select_by_ic(params);
    });
    svl->addWidget(sel_run);
    svl->addStretch();
    tabs->addTab(sel, tr("Feature Selection"));

    // ── Expression Engine tab ──
    auto* expr = new QWidget(this);
    auto* evl = new QVBoxLayout(expr);
    evl->setContentsMargins(12, 12, 12, 12);
    evl->setSpacing(8);

    auto* expr_data = new QLineEdit(expr);
    expr_data->setPlaceholderText(tr("{\"close\":[100,102,...],\"volume\":[1000,1200,...]}"));
    expr_data->setStyleSheet(input_ss());
    text_inputs_["fe_expr_data"] = expr_data;
    evl->addWidget(build_input_row(tr("OHLCV Data (JSON)"), expr_data, expr));

    auto* expr_expr = new QLineEdit(expr);
    expr_expr->setPlaceholderText(tr("e.g. Mean(close, 5) / Std(close, 20)"));
    expr_expr->setStyleSheet(input_ss());
    text_inputs_["fe_expression"] = expr_expr;
    evl->addWidget(build_input_row(tr("Expression"), expr_expr, expr));

    auto* expr_run = make_run_button(tr("EVALUATE EXPRESSION"), expr);
    connect(expr_run, &QPushButton::clicked, this, [this]() {
        QJsonParseError perr{};
        const auto doc = QJsonDocument::fromJson(text_inputs_["fe_expr_data"]->text().toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            display_error(tr("OHLCV Data must be a JSON object like {\"close\":[…],\"volume\":[…]} — %1")
                              .arg(perr.errorString()));
            return;
        }
        const QString expression = text_inputs_["fe_expression"]->text().trimmed();
        if (expression.isEmpty()) {
            display_error(tr("Enter an expression, e.g. Mean(close, 5) / Std(close, 20)."));
            return;
        }
        show_loading(tr("Evaluating %1...").arg(expression));
        QJsonObject params;
        params["data"] = doc.object();
        params["expression"] = expression;
        AIQuantLabService::instance().feature_evaluate_expression(params);
    });
    evl->addWidget(expr_run);
    evl->addStretch();
    tabs->addTab(expr, tr("Expression Engine"));

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
        assets_in->setPlaceholderText(tr("Asset names (comma-separated, e.g. AAPL,GOOG,MSFT)"));
        assets_in->setStyleSheet(input_ss());
        text_inputs_[method_id + "_assets"] = assets_in;
        tvl->addWidget(build_input_row(tr("Assets"), assets_in, t));

        auto* cov_in = new QLineEdit(t);
        cov_in->setPlaceholderText(tr("Covariance matrix JSON: [[0.04,0.01],[0.01,0.09]]"));
        cov_in->setStyleSheet(input_ss());
        text_inputs_[method_id + "_cov"] = cov_in;
        tvl->addWidget(build_input_row(tr("Covariance Matrix"), cov_in, t));

        if (needs_returns) {
            auto* ret_in = new QLineEdit(t);
            ret_in->setPlaceholderText(tr("Expected returns (comma-separated, e.g. 0.10,0.15,0.12)"));
            ret_in->setStyleSheet(input_ss());
            text_inputs_[method_id + "_returns"] = ret_in;
            tvl->addWidget(build_input_row(tr("Expected Returns"), ret_in, t));
        }

        auto* rf_spin = make_double_spin(0, 20, 2.0, 2, "%", t);
        double_inputs_[method_id + "_rf"] = rf_spin;
        tvl->addWidget(build_input_row(tr("Risk-Free Rate"), rf_spin, t));

        auto* run = make_run_button(btn_label, t);
        connect(run, &QPushButton::clicked, this, [this, method_id, needs_returns]() {
            // Previously: a blank Assets field produced [""] and a malformed
            // covariance matrix was silently dropped, so the optimiser ran on
            // nonsense and returned an unrelated Python error.
            const QJsonArray assets_arr = parse_csv_strings(text_inputs_[method_id + "_assets"]->text());
            if (assets_arr.isEmpty()) {
                display_error(tr("Enter at least two asset names, e.g. AAPL,GOOG,MSFT."));
                return;
            }
            QJsonParseError perr{};
            const auto cov_doc = QJsonDocument::fromJson(text_inputs_[method_id + "_cov"]->text().toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !cov_doc.isArray()) {
                display_error(tr("Covariance Matrix must be a JSON 2-D array like [[0.04,0.01],[0.01,0.09]] — %1")
                                  .arg(perr.errorString()));
                return;
            }
            const QJsonArray cov = cov_doc.array();
            if (cov.size() != assets_arr.size()) {
                display_error(tr("Covariance matrix has %1 row(s) but you listed %2 assets - they must match.")
                                  .arg(cov.size())
                                  .arg(assets_arr.size()));
                return;
            }
            QJsonObject params;
            params["assets"] = assets_arr;
            params["cov_matrix"] = cov;
            if (needs_returns) {
                QJsonArray ret_arr;
                QString bad;
                if (!parse_doubles(text_inputs_[method_id + "_returns"]->text(), ret_arr, &bad)) {
                    display_error(tr("Expected Returns: '%1' is not numeric.").arg(bad));
                    return;
                }
                if (ret_arr.size() != assets_arr.size()) {
                    display_error(tr("Expected Returns has %1 entries but you listed %2 assets.")
                                      .arg(ret_arr.size())
                                      .arg(assets_arr.size()));
                    return;
                }
                params["expected_returns"] = ret_arr;
            }
            params["risk_free_rate"] = double_inputs_[method_id + "_rf"]->value() / 100.0;
            show_loading(tr("Optimising %1 assets...").arg(assets_arr.size()));
            AIQuantLabService::instance().portopt_run(method_id, params);
        });
        tvl->addWidget(run);
        tvl->addStretch();
        return t;
    };

    tabs->addTab(make_cov_tab("hierarchical_risk_parity", tr("RUN HRP"), false), tr("HRP"));
    tabs->addTab(make_cov_tab("minimum_variance", tr("MIN VARIANCE"), false), tr("Min Variance"));
    tabs->addTab(make_cov_tab("maximum_sharpe", tr("MAX SHARPE"), true), tr("Max Sharpe"));
    tabs->addTab(make_cov_tab("efficient_frontier", tr("EFFICIENT FRONTIER"), true), tr("Eff. Frontier"));

    // ── Black-Litterman ──
    auto* bl = new QWidget(this);
    auto* blvl = new QVBoxLayout(bl);
    blvl->setContentsMargins(12, 12, 12, 12);
    blvl->setSpacing(8);
    auto* bl_assets = new QLineEdit(bl);
    bl_assets->setPlaceholderText(tr("Asset names (comma-separated)"));
    bl_assets->setStyleSheet(input_ss());
    text_inputs_["bl_assets"] = bl_assets;
    blvl->addWidget(build_input_row(tr("Assets"), bl_assets, bl));
    auto* bl_caps = new QLineEdit(bl);
    bl_caps->setPlaceholderText(tr("Market caps (comma-separated, e.g. 2000,1500,800)"));
    bl_caps->setStyleSheet(input_ss());
    text_inputs_["bl_caps"] = bl_caps;
    blvl->addWidget(build_input_row(tr("Market Caps ($B)"), bl_caps, bl));
    auto* bl_cov = new QLineEdit(bl);
    bl_cov->setPlaceholderText(tr("Covariance matrix JSON: [[0.04,0.01],[0.01,0.09]]"));
    bl_cov->setStyleSheet(input_ss());
    text_inputs_["bl_cov"] = bl_cov;
    blvl->addWidget(build_input_row(tr("Covariance Matrix"), bl_cov, bl));
    auto* bl_views = new QLineEdit(bl);
    bl_views->setPlaceholderText(tr("Views (comma-separated, e.g. 0.05,0.10)"));
    bl_views->setStyleSheet(input_ss());
    text_inputs_["bl_views"] = bl_views;
    blvl->addWidget(build_input_row(tr("Views"), bl_views, bl));
    auto* bl_conf = new QLineEdit(bl);
    bl_conf->setPlaceholderText(tr("View confidences (e.g. 0.8,0.6)"));
    bl_conf->setStyleSheet(input_ss());
    text_inputs_["bl_conf"] = bl_conf;
    blvl->addWidget(build_input_row(tr("View Confidences"), bl_conf, bl));
    auto* bl_run = make_run_button(tr("RUN BLACK-LITTERMAN"), bl);
    connect(bl_run, &QPushButton::clicked, this, [this]() {
        const QJsonArray assets = parse_csv_strings(text_inputs_["bl_assets"]->text());
        if (assets.isEmpty()) {
            display_error(tr("Enter at least two asset names."));
            return;
        }
        QJsonArray caps, views, confs;
        QString bad;
        if (!parse_doubles(text_inputs_["bl_caps"]->text(), caps, &bad)) {
            display_error(tr("Market Caps: '%1' is not numeric.").arg(bad));
            return;
        }
        if (caps.size() != assets.size()) {
            display_error(
                tr("Market Caps has %1 entries but you listed %2 assets.").arg(caps.size()).arg(assets.size()));
            return;
        }
        if (!parse_doubles(text_inputs_["bl_views"]->text(), views, &bad)) {
            display_error(tr("Views: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["bl_conf"]->text(), confs, &bad)) {
            display_error(tr("View Confidences: '%1' is not numeric.").arg(bad));
            return;
        }
        if (views.size() != confs.size()) {
            display_error(tr("Each view needs exactly one confidence — got %1 view(s) and %2 confidence(s).")
                              .arg(views.size())
                              .arg(confs.size()));
            return;
        }
        QJsonParseError perr{};
        const auto cov_doc = QJsonDocument::fromJson(text_inputs_["bl_cov"]->text().toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !cov_doc.isArray()) {
            display_error(tr("Covariance Matrix must be a JSON 2-D array — %1").arg(perr.errorString()));
            return;
        }
        QJsonObject params;
        params["market_caps"] = caps;
        params["views"] = views;
        params["view_confidences"] = confs;
        params["assets"] = assets;
        params["cov_matrix"] = cov_doc.array();
        show_loading(tr("Blending %1 view(s) into the equilibrium prior...").arg(views.size()));
        AIQuantLabService::instance().portopt_run("black_litterman", params);
    });
    blvl->addWidget(bl_run);
    blvl->addStretch();
    tabs->addTab(bl, tr("Black-Litterman"));

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
    ic_preds->setPlaceholderText(tr("Predictions (comma-separated, e.g. 0.1,0.2,-0.1,0.3)"));
    ic_preds->setStyleSheet(input_ss());
    text_inputs_["ev_predictions"] = ic_preds;
    icvl->addWidget(build_input_row(tr("Predictions"), ic_preds, ic));
    auto* ic_rets = new QLineEdit(ic);
    ic_rets->setPlaceholderText(tr("Actual returns (comma-separated)"));
    ic_rets->setStyleSheet(input_ss());
    text_inputs_["ev_returns"] = ic_rets;
    icvl->addWidget(build_input_row(tr("Returns"), ic_rets, ic));
    auto* ic_method = new QComboBox(ic);
    ic_method->addItems({"pearson", "spearman"});
    ic_method->setStyleSheet(combo_ss());
    combo_inputs_["ev_ic_method"] = ic_method;
    icvl->addWidget(build_input_row(tr("Method"), ic_method, ic));
    auto* ic_run = make_run_button(tr("CALCULATE IC METRICS"), ic);
    connect(ic_run, &QPushButton::clicked, this, [this]() {
        QJsonArray preds, rets;
        QString bad;
        if (!parse_doubles(text_inputs_["ev_predictions"]->text(), preds, &bad)) {
            display_error(tr("Predictions: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["ev_returns"]->text(), rets, &bad)) {
            display_error(tr("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        // A correlation over fewer than 3 pairs is degenerate (|IC| == 1 by
        // construction at n == 2), so refuse rather than report a fake 1.0.
        if (preds.size() < 3) {
            display_error(tr("IC needs at least 3 paired observations; you provided %1.").arg(preds.size()));
            return;
        }
        if (preds.size() != rets.size()) {
            display_error(
                tr("Predictions (%1) and returns (%2) must have the same length.").arg(preds.size()).arg(rets.size()));
            return;
        }
        show_loading(tr("Computing %1 IC over %2 observations...")
                         .arg(combo_inputs_["ev_ic_method"]->currentText())
                         .arg(preds.size()));
        QJsonObject params;
        params["predictions"] = preds;
        params["returns"] = rets;
        params["method"] = combo_inputs_["ev_ic_method"]->currentText();
        AIQuantLabService::instance().evaluation_ic(params);
    });
    icvl->addWidget(ic_run);
    icvl->addStretch();
    tabs->addTab(ic, tr("IC Metrics"));

    // ── Full Report tab ──
    auto* rep = new QWidget(this);
    auto* repvl = new QVBoxLayout(rep);
    repvl->setContentsMargins(12, 12, 12, 12);
    repvl->setSpacing(8);
    auto* rep_name = new QLineEdit(rep);
    rep_name->setPlaceholderText(tr("Factor name (e.g. momentum)"));
    rep_name->setStyleSheet(input_ss());
    text_inputs_["ev_factor_name"] = rep_name;
    repvl->addWidget(build_input_row(tr("Factor Name"), rep_name, rep));
    auto* rep_preds = new QLineEdit(rep);
    rep_preds->setPlaceholderText(tr("Predictions (comma-separated)"));
    rep_preds->setStyleSheet(input_ss());
    text_inputs_["ev_rep_preds"] = rep_preds;
    repvl->addWidget(build_input_row(tr("Predictions"), rep_preds, rep));
    auto* rep_rets = new QLineEdit(rep);
    rep_rets->setPlaceholderText(tr("Returns (comma-separated)"));
    rep_rets->setStyleSheet(input_ss());
    text_inputs_["ev_rep_returns"] = rep_rets;
    repvl->addWidget(build_input_row(tr("Returns"), rep_rets, rep));
    auto* rep_run = make_run_button(tr("GENERATE EVALUATION REPORT"), rep);
    connect(rep_run, &QPushButton::clicked, this, [this]() {
        QJsonArray preds, rets;
        QString bad;
        if (!parse_doubles(text_inputs_["ev_rep_preds"]->text(), preds, &bad)) {
            display_error(tr("Predictions: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["ev_rep_returns"]->text(), rets, &bad)) {
            display_error(tr("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (preds.size() < 3) {
            display_error(tr("Need at least 3 paired observations; you provided %1.").arg(preds.size()));
            return;
        }
        if (preds.size() != rets.size()) {
            display_error(
                tr("Predictions (%1) and returns (%2) must have the same length.").arg(preds.size()).arg(rets.size()));
            return;
        }
        const QString fname = text_inputs_["ev_factor_name"]->text().trimmed();
        show_loading(tr("Scoring factor '%1' over %2 observations...")
                         .arg(fname.isEmpty() ? tr("unnamed") : fname)
                         .arg(preds.size()));
        QJsonObject params;
        params["factor_name"] = fname.isEmpty() ? QStringLiteral("factor") : fname;
        params["predictions"] = preds;
        params["returns"] = rets;
        AIQuantLabService::instance().evaluation_report(params);
    });
    repvl->addWidget(rep_run);
    repvl->addStretch();
    tabs->addTab(rep, tr("Full Report"));

    // ── Risk Metrics tab ──
    auto* risk = new QWidget(this);
    auto* riskvl = new QVBoxLayout(risk);
    riskvl->setContentsMargins(12, 12, 12, 12);
    riskvl->setSpacing(8);
    auto* risk_rets = new QLineEdit(risk);
    risk_rets->setPlaceholderText(tr("Daily returns (comma-separated)"));
    risk_rets->setStyleSheet(input_ss());
    text_inputs_["ev_risk_returns"] = risk_rets;
    riskvl->addWidget(build_input_row(tr("Returns"), risk_rets, risk));
    auto* risk_bench = new QLineEdit(risk);
    risk_bench->setPlaceholderText(tr("Benchmark returns (optional, comma-separated)"));
    risk_bench->setStyleSheet(input_ss());
    text_inputs_["ev_risk_bench"] = risk_bench;
    riskvl->addWidget(build_input_row(tr("Benchmark (opt.)"), risk_bench, risk));
    auto* risk_conf = make_double_spin(0.8, 0.999, 0.95, 3, "", risk);
    double_inputs_["ev_risk_conf"] = risk_conf;
    riskvl->addWidget(build_input_row(tr("Confidence Level"), risk_conf, risk));
    auto* risk_run = make_run_button(tr("CALCULATE RISK METRICS"), risk);
    connect(risk_run, &QPushButton::clicked, this, [this]() {
        QJsonArray rets;
        QString bad;
        if (!parse_doubles(text_inputs_["ev_risk_returns"]->text(), rets, &bad)) {
            display_error(tr("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        // Below 5 points volatility / VaR / drawdown are degenerate. (30+ is the
        // practical floor for the ANNUALISED figures, but blocking there would
        // reject legitimate exploratory runs.)
        if (rets.size() < 5) {
            display_error(tr("Risk metrics need at least 5 daily returns; you provided %1. "
                             "Annualised figures only become meaningful past ~30 observations.")
                              .arg(rets.size()));
            return;
        }
        QJsonObject params;
        params["returns"] = rets;
        params["confidence_level"] = double_inputs_["ev_risk_conf"]->value();
        const auto bench_text = text_inputs_["ev_risk_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            if (!parse_doubles(bench_text, bench, &bad)) {
                display_error(tr("Benchmark returns: '%1' is not numeric.").arg(bad));
                return;
            }
            // Beta / tracking error / capture ratios are only defined pairwise.
            if (bench.size() != rets.size()) {
                display_error(
                    tr("Benchmark (%1) must have the same length as returns (%2).").arg(bench.size()).arg(rets.size()));
                return;
            }
            params["benchmark_returns"] = bench;
        }
        show_loading(tr("Computing risk metrics over %1 observations...").arg(rets.size()));
        AIQuantLabService::instance().evaluation_risk_metrics(params);
    });
    riskvl->addWidget(risk_run);
    riskvl->addStretch();
    tabs->addTab(risk, tr("Risk Metrics"));

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
    tk_signal->setPlaceholderText(tr("Signal values (comma-separated)"));
    tk_signal->setStyleSheet(input_ss());
    text_inputs_["st_tk_signal"] = tk_signal;
    tkvl->addWidget(build_input_row(tr("Signal"), tk_signal, topk));
    auto* tk_topk = new QSpinBox(topk);
    tk_topk->setRange(1, 500);
    tk_topk->setValue(50);
    tk_topk->setStyleSheet(spinbox_ss());
    int_inputs_["st_topk"] = tk_topk;
    tkvl->addWidget(build_input_row(tr("Top-K"), tk_topk, topk));
    auto* tk_drop = new QSpinBox(topk);
    tk_drop->setRange(1, 100);
    tk_drop->setValue(5);
    tk_drop->setStyleSheet(tk_topk->styleSheet());
    int_inputs_["st_ndrop"] = tk_drop;
    tkvl->addWidget(build_input_row(tr("N-Drop"), tk_drop, topk));
    auto* tk_run = make_run_button(tr("CREATE TOPK-DROPOUT STRATEGY"), topk);
    connect(tk_run, &QPushButton::clicked, this, [this]() {
        QJsonArray signal;
        QString bad;
        if (!parse_doubles(text_inputs_["st_tk_signal"]->text(), signal, &bad)) {
            display_error(tr("Signal: '%1' is not numeric.").arg(bad));
            return;
        }
        const int topk = int_inputs_["st_topk"]->value();
        const int ndrop = int_inputs_["st_ndrop"]->value();
        if (signal.isEmpty()) {
            display_error(tr("Enter the cross-sectional signal values."));
            return;
        }
        // NOTE: topk > signal.size() is deliberately NOT blocked — a small test
        // universe is a legitimate way to try the panel and the shipped default
        // topk is 50. n_drop >= topk, however, replaces the whole book every
        // rebalance: that is not TopK-Dropout, it is 100% turnover.
        if (ndrop >= topk) {
            display_error(tr("N-Drop (%1) must be smaller than Top-K (%2).").arg(ndrop).arg(topk));
            return;
        }
        show_loading(tr("Building TopK-Dropout over %1 names...").arg(signal.size()));
        QJsonObject params;
        params["signal"] = signal;
        params["topk"] = topk;
        params["n_drop"] = ndrop;
        AIQuantLabService::instance().strategy_create("create_topk_dropout", params);
    });
    tkvl->addWidget(tk_run);
    tkvl->addStretch();
    tabs->addTab(topk, tr("TopK-Dropout"));

    // ── Risk Parity ──
    auto* rp = new QWidget(this);
    auto* rpvl = new QVBoxLayout(rp);
    rpvl->setContentsMargins(12, 12, 12, 12);
    rpvl->setSpacing(8);
    auto* rp_returns = new QLineEdit(rp);
    rp_returns->setPlaceholderText(tr("Asset returns matrix JSON: [[0.01,-0.02,...],[...]]"));
    rp_returns->setStyleSheet(input_ss());
    text_inputs_["st_rp_returns"] = rp_returns;
    rpvl->addWidget(build_input_row(tr("Returns Matrix"), rp_returns, rp));
    auto* rp_target = make_double_spin(0.01, 1.0, 0.10, 2, "", rp);
    double_inputs_["st_rp_target"] = rp_target;
    rpvl->addWidget(build_input_row(tr("Target Risk"), rp_target, rp));
    auto* rp_freq = new QComboBox(rp);
    rp_freq->addItems({"monthly", "weekly", "daily", "quarterly"});
    rp_freq->setStyleSheet(combo_ss());
    combo_inputs_["st_rp_freq"] = rp_freq;
    rpvl->addWidget(build_input_row(tr("Rebalance"), rp_freq, rp));
    auto* rp_run = make_run_button(tr("CREATE RISK PARITY STRATEGY"), rp);
    connect(rp_run, &QPushButton::clicked, this, [this]() {
        QJsonParseError perr{};
        const auto ret_doc = QJsonDocument::fromJson(text_inputs_["st_rp_returns"]->text().toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !ret_doc.isArray()) {
            display_error(tr("Returns Matrix must be a JSON 2-D array like [[0.01,-0.02],[0.00,0.01]] — %1")
                              .arg(perr.errorString()));
            return;
        }
        const QJsonArray rows = ret_doc.array();
        if (rows.isEmpty() || !rows.first().isArray()) {
            display_error(tr("Returns Matrix must contain at least one row of per-asset returns."));
            return;
        }
        show_loading(tr("Solving risk parity over %1 period(s)...").arg(rows.size()));
        QJsonObject params;
        params["returns"] = rows;
        params["target_risk"] = double_inputs_["st_rp_target"]->value();
        params["rebalance_frequency"] = combo_inputs_["st_rp_freq"]->currentText();
        AIQuantLabService::instance().strategy_create("create_risk_parity", params);
    });
    rpvl->addWidget(rp_run);
    rpvl->addStretch();
    tabs->addTab(rp, tr("Risk Parity"));

    // ── Portfolio Metrics ──
    auto* pm = new QWidget(this);
    auto* pmvl = new QVBoxLayout(pm);
    pmvl->setContentsMargins(12, 12, 12, 12);
    pmvl->setSpacing(8);
    auto* pm_rets = new QLineEdit(pm);
    pm_rets->setPlaceholderText(tr("Portfolio returns (comma-separated)"));
    pm_rets->setStyleSheet(input_ss());
    text_inputs_["st_pm_returns"] = pm_rets;
    pmvl->addWidget(build_input_row(tr("Returns"), pm_rets, pm));
    auto* pm_bench = new QLineEdit(pm);
    pm_bench->setPlaceholderText(tr("Benchmark returns (optional)"));
    pm_bench->setStyleSheet(input_ss());
    text_inputs_["st_pm_bench"] = pm_bench;
    pmvl->addWidget(build_input_row(tr("Benchmark (opt.)"), pm_bench, pm));
    auto* pm_rf = make_double_spin(0, 20, 2.0, 2, "%", pm);
    double_inputs_["st_pm_rf"] = pm_rf;
    pmvl->addWidget(build_input_row(tr("Risk-Free Rate"), pm_rf, pm));
    auto* pm_run = make_run_button(tr("CALCULATE PORTFOLIO METRICS"), pm);
    connect(pm_run, &QPushButton::clicked, this, [this]() {
        QJsonArray rets;
        QString bad;
        if (!parse_doubles(text_inputs_["st_pm_returns"]->text(), rets, &bad)) {
            display_error(tr("Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (rets.size() < 5) {
            display_error(tr("Portfolio metrics need at least 5 daily returns; you provided %1. "
                             "Annualised Sharpe/Sortino/Calmar only become meaningful past ~30 observations.")
                              .arg(rets.size()));
            return;
        }
        QJsonObject params;
        params["returns"] = rets;
        params["risk_free_rate"] = double_inputs_["st_pm_rf"]->value() / 100.0;
        const auto bench_text = text_inputs_["st_pm_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            if (!parse_doubles(bench_text, bench, &bad)) {
                display_error(tr("Benchmark returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (bench.size() != rets.size()) {
                display_error(
                    tr("Benchmark (%1) must have the same length as returns (%2).").arg(bench.size()).arg(rets.size()));
                return;
            }
            params["benchmark_returns"] = bench;
        }
        show_loading(tr("Computing portfolio metrics over %1 observations...").arg(rets.size()));
        AIQuantLabService::instance().strategy_portfolio_metrics(params);
    });
    pmvl->addWidget(pm_run);
    pmvl->addStretch();
    tabs->addTab(pm, tr("Portfolio Metrics"));

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
    auto* lt_info = new QLabel(tr("Browse all available data normalizers and transformation processors."), list_tab);
    lt_info->setWordWrap(true);
    lt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    ltvl->addWidget(lt_info);
    auto* lt_run = make_run_button(tr("LIST ALL PROCESSORS"), list_tab);
    connect(lt_run, &QPushButton::clicked, this, [this]() {
        show_loading(tr("Loading processor catalog..."));
        AIQuantLabService::instance().dataproc_list_processors();
    });
    ltvl->addWidget(lt_run);
    ltvl->addStretch();
    tabs->addTab(list_tab, tr("Browse"));

    // ── Create Pipeline ──
    auto* pipe_tab = new QWidget(this);
    auto* ptvl = new QVBoxLayout(pipe_tab);
    ptvl->setContentsMargins(12, 12, 12, 12);
    ptvl->setSpacing(8);
    auto* pipe_id = new QLineEdit(pipe_tab);
    pipe_id->setPlaceholderText(tr("Pipeline ID (e.g. my_pipeline)"));
    pipe_id->setStyleSheet(input_ss());
    text_inputs_["dp_pipeline_id"] = pipe_id;
    ptvl->addWidget(build_input_row(tr("Pipeline ID"), pipe_id, pipe_tab));
    auto* pipe_procs = new QLineEdit(pipe_tab);
    pipe_procs->setPlaceholderText(tr(R"([{"type":"zscore"},{"type":"winsorize","lower":0.01,"upper":0.99}])"));
    pipe_procs->setStyleSheet(input_ss());
    text_inputs_["dp_processors"] = pipe_procs;
    ptvl->addWidget(build_input_row(tr("Processors (JSON)"), pipe_procs, pipe_tab));
    auto* pipe_run = make_run_button(tr("CREATE PIPELINE"), pipe_tab);
    connect(pipe_run, &QPushButton::clicked, this, [this]() {
        const QString pid = text_inputs_["dp_pipeline_id"]->text().trimmed();
        if (pid.isEmpty()) {
            display_error(tr("Enter a pipeline ID — you need it to run PROCESS DATA later."));
            return;
        }
        QJsonParseError perr{};
        const auto doc = QJsonDocument::fromJson(text_inputs_["dp_processors"]->text().toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isArray() || doc.array().isEmpty()) {
            display_error(tr("Processors must be a non-empty JSON array like "
                             "[{\"type\":\"zscore\"}] — %1")
                              .arg(perr.errorString()));
            return;
        }
        show_loading(tr("Creating pipeline '%1' with %2 stage(s)...").arg(pid).arg(doc.array().size()));
        QJsonObject params;
        params["pipeline_id"] = pid;
        params["processors"] = doc.array();
        AIQuantLabService::instance().dataproc_create_pipeline(params);
    });
    ptvl->addWidget(pipe_run);
    ptvl->addStretch();
    tabs->addTab(pipe_tab, tr("Create Pipeline"));

    // ── Process Data ──
    auto* proc_tab = new QWidget(this);
    auto* procvl = new QVBoxLayout(proc_tab);
    procvl->setContentsMargins(12, 12, 12, 12);
    procvl->setSpacing(8);
    auto* proc_pid = new QLineEdit(proc_tab);
    proc_pid->setPlaceholderText(tr("Pipeline ID (must be created first)"));
    proc_pid->setStyleSheet(input_ss());
    text_inputs_["dp_proc_pid"] = proc_pid;
    procvl->addWidget(build_input_row(tr("Pipeline ID"), proc_pid, proc_tab));
    auto* proc_data = new QLineEdit(proc_tab);
    proc_data->setPlaceholderText(tr(R"({"feature_close":[100,102,...],"feature_volume":[1000,1200,...]})"));
    proc_data->setStyleSheet(input_ss());
    text_inputs_["dp_proc_data"] = proc_data;
    procvl->addWidget(build_input_row(tr("Data (JSON)"), proc_data, proc_tab));
    auto* proc_run = make_run_button(tr("PROCESS DATA"), proc_tab);
    connect(proc_run, &QPushButton::clicked, this, [this]() {
        const QString pid = text_inputs_["dp_proc_pid"]->text().trimmed();
        if (pid.isEmpty()) {
            display_error(tr("Enter the ID of a pipeline you created in the Create Pipeline tab."));
            return;
        }
        QJsonParseError perr{};
        const auto doc = QJsonDocument::fromJson(text_inputs_["dp_proc_data"]->text().toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject() || doc.object().isEmpty()) {
            display_error(tr("Data must be a non-empty JSON object of column → values — %1").arg(perr.errorString()));
            return;
        }
        show_loading(tr("Running pipeline '%1' over %2 column(s)...").arg(pid).arg(doc.object().size()));
        QJsonObject params;
        params["pipeline_id"] = pid;
        params["data"] = doc.object();
        AIQuantLabService::instance().dataproc_process_data(params);
    });
    procvl->addWidget(proc_run);
    procvl->addStretch();
    tabs->addTab(proc_tab, tr("Process Data"));

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
    auto* lib_info = new QLabel(tr("Browse all built-in Qlib alpha factors and expressions."), lib_tab);
    lib_info->setWordWrap(true);
    lib_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY));
    libvl->addWidget(lib_info);
    auto* lib_run = make_run_button(tr("BROWSE FACTOR LIBRARY"), lib_tab);
    connect(lib_run, &QPushButton::clicked, this, [this]() {
        show_loading(tr("Loading Qlib factor library..."));
        AIQuantLabService::instance().factor_get_library();
    });
    libvl->addWidget(lib_run);
    auto* inst_run = make_run_button(tr("LIST INSTRUMENTS"), lib_tab);
    connect(inst_run, &QPushButton::clicked, this, [this]() {
        show_loading(tr("Loading instrument universe..."));
        AIQuantLabService::instance().factor_get_instruments();
    });
    libvl->addWidget(inst_run);
    libvl->addStretch();
    tabs->addTab(lib_tab, tr("Factor Library"));

    // ── Fetch Data ──
    auto* data_tab = new QWidget(this);
    auto* datavl = new QVBoxLayout(data_tab);
    datavl->setContentsMargins(12, 12, 12, 12);
    datavl->setSpacing(8);
    auto* fd_instr = new QLineEdit(data_tab);
    fd_instr->setPlaceholderText(tr("Instruments (comma-separated, e.g. aapl,msft)"));
    fd_instr->setStyleSheet(input_ss());
    text_inputs_["fd_instruments"] = fd_instr;
    datavl->addWidget(build_input_row(tr("Instruments"), fd_instr, data_tab));
    auto* fd_fields = new QLineEdit(data_tab);
    fd_fields->setPlaceholderText(tr("Fields (comma-separated, e.g. $close,$volume,$open)"));
    fd_fields->setStyleSheet(input_ss());
    text_inputs_["fd_fields"] = fd_fields;
    datavl->addWidget(build_input_row(tr("Fields"), fd_fields, data_tab));
    const QDate fd_today = QDate::currentDate();
    auto* fd_start = new QDateEdit(data_tab);
    fd_start->setCalendarPopup(true);
    fd_start->setDisplayFormat("yyyy-MM-dd");
    fd_start->setMinimumDate(QDate(2000, 1, 1));
    fd_start->setMaximumDate(fd_today);
    fd_start->setDate(fd_today.addYears(-1));
    fd_start->setStyleSheet(dateedit_ss());
    date_inputs_["fd_start"] = fd_start;
    datavl->addWidget(build_input_row(tr("Start Date"), fd_start, data_tab));
    auto* fd_end = new QDateEdit(data_tab);
    fd_end->setCalendarPopup(true);
    fd_end->setDisplayFormat("yyyy-MM-dd");
    fd_end->setMinimumDate(QDate(2000, 1, 1));
    fd_end->setMaximumDate(fd_today);
    fd_end->setDate(fd_today);
    fd_end->setStyleSheet(dateedit_ss());
    date_inputs_["fd_end"] = fd_end;
    datavl->addWidget(build_input_row(tr("End Date"), fd_end, data_tab));
    auto* fd_run = make_run_button(tr("FETCH DATA"), data_tab);
    connect(fd_run, &QPushButton::clicked, this, [this]() {
        const QJsonArray instr = parse_csv_strings(text_inputs_["fd_instruments"]->text(), /*lowercase=*/true);
        if (instr.isEmpty()) {
            display_error(tr("Enter at least one instrument, e.g. aapl,msft."));
            return;
        }
        const QJsonArray fields = parse_csv_strings(text_inputs_["fd_fields"]->text());
        if (fields.isEmpty()) {
            display_error(tr("Enter at least one field, e.g. $close,$volume."));
            return;
        }
        const QDate start = date_inputs_["fd_start"]->date();
        const QDate end = date_inputs_["fd_end"]->date();
        if (start > end) {
            display_error(tr("Start date (%1) is after end date (%2).")
                              .arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")));
            return;
        }
        show_loading(tr("Fetching %1 field(s) for %2 instrument(s)...").arg(fields.size()).arg(instr.size()));
        QJsonObject params;
        params["instruments"] = instr;
        params["fields"] = fields;
        params["start_date"] = start.toString("yyyy-MM-dd");
        params["end_date"] = end.toString("yyyy-MM-dd");
        AIQuantLabService::instance().factor_get_data(params);
    });
    datavl->addWidget(fd_run);
    datavl->addStretch();
    tabs->addTab(data_tab, tr("Fetch Data"));

    // ── Calendar ──
    auto* cal_tab = new QWidget(this);
    auto* calvl = new QVBoxLayout(cal_tab);
    calvl->setContentsMargins(12, 12, 12, 12);
    calvl->setSpacing(8);
    const QDate cal_today = QDate::currentDate();
    auto* cal_start = new QDateEdit(cal_tab);
    cal_start->setCalendarPopup(true);
    cal_start->setDisplayFormat("yyyy-MM-dd");
    cal_start->setMinimumDate(QDate(2000, 1, 1));
    cal_start->setMaximumDate(cal_today);
    cal_start->setDate(cal_today.addYears(-1));
    cal_start->setStyleSheet(dateedit_ss());
    date_inputs_["fd_cal_start"] = cal_start;
    calvl->addWidget(build_input_row(tr("Start Date"), cal_start, cal_tab));
    auto* cal_end = new QDateEdit(cal_tab);
    cal_end->setCalendarPopup(true);
    cal_end->setDisplayFormat("yyyy-MM-dd");
    cal_end->setMinimumDate(QDate(2000, 1, 1));
    cal_end->setMaximumDate(cal_today);
    cal_end->setDate(cal_today);
    cal_end->setStyleSheet(dateedit_ss());
    date_inputs_["fd_cal_end"] = cal_end;
    calvl->addWidget(build_input_row(tr("End Date"), cal_end, cal_tab));
    auto* cal_run = make_run_button(tr("GET TRADING CALENDAR"), cal_tab);
    connect(cal_run, &QPushButton::clicked, this, [this]() {
        const QDate start = date_inputs_["fd_cal_start"]->date();
        const QDate end = date_inputs_["fd_cal_end"]->date();
        if (start > end) {
            display_error(tr("Start date (%1) is after end date (%2).")
                              .arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")));
            return;
        }
        show_loading(
            tr("Loading trading calendar %1 → %2...").arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")));
        QJsonObject params;
        params["start_date"] = start.toString("yyyy-MM-dd");
        params["end_date"] = end.toString("yyyy-MM-dd");
        AIQuantLabService::instance().factor_get_calendar(params);
    });
    calvl->addWidget(cal_run);
    calvl->addStretch();
    tabs->addTab(cal_tab, tr("Calendar"));

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
        new QLabel(tr("List all available Qlib models (LightGBM, XGBoost, LSTM, Transformer, etc.)."), browse_tab);
    bt_info->setWordWrap(true);
    bt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    btvl->addWidget(bt_info);
    auto* bt_list = make_run_button(tr("LIST ALL MODELS"), browse_tab);
    connect(bt_list, &QPushButton::clicked, this, [this]() {
        show_loading(tr("Loading Qlib model catalog..."));
        AIQuantLabService::instance().model_list();
    });
    btvl->addWidget(bt_list);
    auto* bt_status = make_run_button(tr("CHECK QLIB STATUS"), browse_tab);
    connect(bt_status, &QPushButton::clicked, this, [this]() {
        show_loading(tr("Checking Qlib runtime status..."));
        AIQuantLabService::instance().model_check_status();
    });
    btvl->addWidget(bt_status);
    btvl->addStretch();
    tabs->addTab(browse_tab, tr("Browse"));

    // ── Train Model ──
    auto* train_tab = new QWidget(this);
    auto* ttvl = new QVBoxLayout(train_tab);
    ttvl->setContentsMargins(12, 12, 12, 12);
    ttvl->setSpacing(8);
    auto* ml_type = new QComboBox(train_tab);
    ml_type->setStyleSheet(combo_ss());
    ml_type->addItems({"lightgbm", "xgboost", "catboost", "linear", "lstm", "gru", "transformer"});
    combo_inputs_["ml_model_type"] = ml_type;
    ttvl->addWidget(build_input_row(tr("Model Type"), ml_type, train_tab));
    auto* ml_instr = new QLineEdit(train_tab);
    ml_instr->setPlaceholderText(tr("Instruments (comma-separated, e.g. aapl,msft)"));
    ml_instr->setStyleSheet(input_ss());
    text_inputs_["ml_instruments"] = ml_instr;
    ttvl->addWidget(build_input_row(tr("Instruments"), ml_instr, train_tab));
    auto* ml_start = new QLineEdit(train_tab);
    ml_start->setPlaceholderText(tr("Train start (YYYY-MM-DD)"));
    ml_start->setStyleSheet(input_ss());
    text_inputs_["ml_start"] = ml_start;
    ttvl->addWidget(build_input_row(tr("Start Date"), ml_start, train_tab));
    auto* ml_end = new QLineEdit(train_tab);
    ml_end->setPlaceholderText(tr("Train end (YYYY-MM-DD)"));
    ml_end->setStyleSheet(input_ss());
    text_inputs_["ml_end"] = ml_end;
    ttvl->addWidget(build_input_row(tr("End Date"), ml_end, train_tab));
    auto* ml_run = make_run_button(tr("TRAIN MODEL"), train_tab);
    connect(ml_run, &QPushButton::clicked, this, [this]() {
        const QJsonArray instr = parse_csv_strings(text_inputs_["ml_instruments"]->text(), /*lowercase=*/true);
        if (instr.isEmpty()) {
            display_error(tr("Enter at least one instrument, e.g. aapl,msft."));
            return;
        }
        QString iso_start, iso_end;
        if (const QString err =
                check_iso_range(text_inputs_["ml_start"]->text(), text_inputs_["ml_end"]->text(), iso_start, iso_end);
            !err.isEmpty()) {
            display_error(err);
            return;
        }

        show_loading(tr("Training %1 on %2 instrument(s) — this can take several minutes...")
                         .arg(combo_inputs_["ml_model_type"]->currentText())
                         .arg(instr.size()));
        QJsonObject params;
        params["model_type"] = combo_inputs_["ml_model_type"]->currentText();
        params["instruments"] = instr;
        if (!iso_start.isEmpty())
            params["start_date"] = iso_start;
        if (!iso_end.isEmpty())
            params["end_date"] = iso_end;
        AIQuantLabService::instance().model_train(params);
    });
    ttvl->addWidget(ml_run);
    ttvl->addStretch();
    tabs->addTab(train_tab, tr("Train Model"));

    // ── Backtest ──
    auto* bt_tab = new QWidget(this);
    auto* btvl2 = new QVBoxLayout(bt_tab);
    btvl2->setContentsMargins(12, 12, 12, 12);
    btvl2->setSpacing(8);
    auto* bt_model = new QLineEdit(bt_tab);
    bt_model->setPlaceholderText(tr("Model ID (from training output)"));
    bt_model->setStyleSheet(input_ss());
    text_inputs_["ml_bt_model"] = bt_model;
    btvl2->addWidget(build_input_row(tr("Model ID"), bt_model, bt_tab));
    auto* bt_instr = new QLineEdit(bt_tab);
    bt_instr->setPlaceholderText(tr("Instruments (comma-separated)"));
    bt_instr->setStyleSheet(input_ss());
    text_inputs_["ml_bt_instr"] = bt_instr;
    btvl2->addWidget(build_input_row(tr("Instruments"), bt_instr, bt_tab));
    auto* bt_start2 = new QLineEdit(bt_tab);
    bt_start2->setPlaceholderText(tr("Backtest start (YYYY-MM-DD)"));
    bt_start2->setStyleSheet(input_ss());
    text_inputs_["ml_bt_start"] = bt_start2;
    btvl2->addWidget(build_input_row(tr("Start Date"), bt_start2, bt_tab));
    auto* bt_end2 = new QLineEdit(bt_tab);
    bt_end2->setPlaceholderText(tr("Backtest end (YYYY-MM-DD)"));
    bt_end2->setStyleSheet(input_ss());
    text_inputs_["ml_bt_end"] = bt_end2;
    btvl2->addWidget(build_input_row(tr("End Date"), bt_end2, bt_tab));
    auto* bt_run2 = make_run_button(tr("RUN BACKTEST"), bt_tab);
    connect(bt_run2, &QPushButton::clicked, this, [this]() {
        const QString model_id = text_inputs_["ml_bt_model"]->text().trimmed();
        if (model_id.isEmpty()) {
            display_error(tr("Enter the Model ID printed by TRAIN MODEL."));
            return;
        }
        const QJsonArray instr = parse_csv_strings(text_inputs_["ml_bt_instr"]->text(), /*lowercase=*/true);
        if (instr.isEmpty()) {
            display_error(tr("Enter at least one instrument, e.g. aapl,msft."));
            return;
        }
        QString iso_start, iso_end;
        if (const QString err = check_iso_range(text_inputs_["ml_bt_start"]->text(), text_inputs_["ml_bt_end"]->text(),
                                                iso_start, iso_end);
            !err.isEmpty()) {
            display_error(err);
            return;
        }
        show_loading(tr("Backtesting %1 on %2 instrument(s)...").arg(model_id).arg(instr.size()));
        QJsonObject params;
        params["model_id"] = model_id;
        params["instruments"] = instr;
        if (!iso_start.isEmpty())
            params["start_date"] = iso_start;
        if (!iso_end.isEmpty())
            params["end_date"] = iso_end;
        AIQuantLabService::instance().model_backtest(params);
    });
    btvl2->addWidget(bt_run2);
    btvl2->addStretch();
    tabs->addTab(bt_tab, tr("Backtest"));

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

} // namespace fincept::screens
