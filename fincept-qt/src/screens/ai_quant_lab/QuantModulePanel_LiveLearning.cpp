// QuantModulePanel_LiveLearning.cpp — live signals, online learning,
// meta learning, and rolling retraining panel builders.
// Method definitions split from QuantModulePanel_Misc.cpp.

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
