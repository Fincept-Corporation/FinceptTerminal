// src/screens/ai_quant_lab/QuantModulePanel.cpp
//
// Core: widget-factory helpers, refresh_theme, connect_service, build_ui,
// build_generic_panel, clear_results, display_error, show_loading, on_error.
// Result rendering (display_result + the huge on_result dispatch) lives in
// QuantModulePanel_Results.cpp.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QDesktopServices>
#include <QLineSeries>
#include <QValueAxis>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QFrame>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>

#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

QDoubleSpinBox* QuantModulePanel::make_double_spin(double min, double max, double val, int decimals,
                                                   const QString& suffix, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setDecimals(decimals);
    if (!suffix.isEmpty())
        spin->setSuffix(suffix);
    spin->setStyleSheet(spinbox_ss());
    return spin;
}

QPushButton* QuantModulePanel::make_run_button(const QString& text, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-weight:700; border:none;"
                               "padding:0 20px; border-radius:2px; letter-spacing:0.8px; }"
                               "QPushButton:hover { background:%3; }"
                               "QPushButton:pressed { background:%4; }")
                           .arg(module_.color.name(), ui::colors::BG_BASE(), module_.color.lighter(115).name(),
                                module_.color.darker(110).name()));
    return btn;
}

QWidget* QuantModulePanel::build_input_row(const QString& label, QWidget* input, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 2, 0, 2);
    hl->setSpacing(8);
    auto* lbl = new QLabel(label, row);
    lbl->setFixedWidth(160);
    lbl->setStyleSheet(QString("color:%1; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(lbl);
    hl->addWidget(input, 1);
    return row;
}

// ── LLM picker helpers ────────────────────────────────────────────────────────

QWidget* QuantModulePanel::build_llm_picker(QWidget* parent, QComboBox** out_combo) {
    auto* combo = new QComboBox(parent);
    combo->setStyleSheet(combo_ss());

    // Populate from saved profiles
    auto profiles_result = LlmProfileRepository::instance().list_profiles();
    if (profiles_result.is_ok()) {
        for (const auto& p : profiles_result.value()) {
            combo->addItem(QString("%1  [%2 / %3]").arg(p.name, p.provider, p.model_id), p.id);
        }
    }
    if (combo->count() == 0)
        combo->addItem("No LLM profiles — configure in Settings → LLM Config", QVariant());

    // Pre-select the global default if present
    auto resolved = LlmProfileRepository::instance().resolve_for_context("ai_quant_lab");
    if (!resolved.profile_id.isEmpty()) {
        int idx = combo->findData(resolved.profile_id);
        if (idx >= 0)
            combo->setCurrentIndex(idx);
    }

    if (out_combo)
        *out_combo = combo;
    return build_input_row("LLM Profile", combo, parent);
}

QJsonObject QuantModulePanel::llm_config_from_combo(QComboBox* combo) const {
    if (!combo)
        return {};
    QString profile_id = combo->currentData().toString();
    if (profile_id.isEmpty())
        return {};

    auto resolved = LlmProfileRepository::instance().resolve_for_context("ai_quant_lab", profile_id);
    // resolve_for_context uses context_id as a profile_id hint — use get_profile directly
    auto result = LlmProfileRepository::instance().get_profile(profile_id);
    if (!result.is_ok())
        return {};

    const auto& p = result.value();
    QJsonObject cfg;
    cfg["llm_provider"] = p.provider;
    cfg["llm_api_key"] = p.api_key;
    cfg["llm_model"] = p.model_id;
    cfg["llm_base_url"] = p.base_url;
    return cfg;
}

// ── Constructor ──────────────────────────────────────────────────────────────

QuantModulePanel::QuantModulePanel(const QuantModule& mod, QWidget* parent) : QWidget(parent), module_(mod) {
    build_ui();
    connect_service();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
}

void QuantModulePanel::refresh_theme() {
    setStyleSheet(QString("background:%1; color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));

    if (panel_header_)
        panel_header_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                         .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    if (header_title_)
        header_title_->setStyleSheet(QString("color:%1; font-weight:700; letter-spacing:1px; background:transparent;")
                                         .arg(module_.color.name()));

    if (header_cat_)
        header_cat_->setStyleSheet(QString("color:%1; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));

    if (status_label_)
        status_label_->setStyleSheet(QString("color:%1; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
}

void QuantModulePanel::connect_service() {
    auto& svc = AIQuantLabService::instance();
    connect(&svc, &AIQuantLabService::result_ready, this, &QuantModulePanel::on_result);
    connect(&svc, &AIQuantLabService::error_occurred, this, &QuantModulePanel::on_error);

    // RL Trading live-streaming signals. Filtered by module_id so only the RL panel
    // updates its widgets; other panels ignore these.
    connect(&svc, &AIQuantLabService::training_progress, this,
            [this](const QString& module_id, int step, int total, double reward_mean, double loss) {
                if (module_id != "rl_trading" || !rl_progress_bar_ || !rl_progress_stats_)
                    return;
                const int pct = (total > 0) ? qBound(0, int(100.0 * step / total), 100) : 0;
                rl_progress_bar_->setValue(pct);
                rl_progress_stats_->setText(QString("step %1 / %2 · reward %3 · loss %4")
                                                .arg(step)
                                                .arg(total)
                                                .arg(reward_mean, 0, 'f', 3)
                                                .arg(loss, 0, 'f', 4));
            });

    connect(&svc, &AIQuantLabService::training_log, this,
            [this](const QString& module_id, const QString& line, bool is_stderr) {
                if (module_id != "rl_trading" || !rl_log_console_)
                    return;
                const QString prefix = is_stderr ? QStringLiteral("[stderr] ") : QString();
                rl_log_console_->appendPlainText(prefix + line);
            });
}

void QuantModulePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Module header — stored for refresh_theme()
    panel_header_ = new QWidget(this);
    panel_header_->setFixedHeight(36);
    auto* hhl = new QHBoxLayout(panel_header_);
    hhl->setContentsMargins(16, 0, 16, 0);
    hhl->setSpacing(8);

    header_title_ = new QLabel(module_.label.toUpper(), panel_header_);
    hhl->addWidget(header_title_);

    auto* div = new QWidget(panel_header_);
    div->setObjectName("panelHeaderDiv");
    div->setFixedSize(1, 14);
    hhl->addWidget(div);

    QString cat_label = module_.category;
    cat_label.replace('_', '/');
    header_cat_ = new QLabel(cat_label, panel_header_);
    hhl->addWidget(header_cat_);

    hhl->addStretch();

    status_label_ = new QLabel(panel_header_);
    hhl->addWidget(status_label_);

    root->addWidget(panel_header_);

    // Content
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    QWidget* content = nullptr;
    if (module_.id == "gs_quant")
        content = build_gs_quant_panel();
    else if (module_.id == "functime")
        content = build_functime_panel();
    else if (module_.id == "statsmodels")
        content = build_statsmodels_panel();
    else if (module_.id == "fortitudo")
        content = build_fortitudo_panel();
    else if (module_.id == "gluonts")
        content = build_gluonts_panel();
    else if (module_.id == "cfa_quant")
        content = build_cfa_quant_panel();
    else if (module_.id == "backtesting")
        content = build_backtesting_panel();
    else if (module_.id == "rl_trading")
        content = build_rl_trading_panel();
    else if (module_.id == "advanced_models")
        content = build_advanced_models_panel();
    else if (module_.id == "deep_agent")
        content = build_deep_agent_panel();
    else if (module_.id == "factor_discovery")
        content = build_factor_discovery_panel();
    else if (module_.id == "model_library")
        content = build_model_library_panel();
    else if (module_.id == "live_signals")
        content = build_live_signals_panel();
    else if (module_.id == "online_learning")
        content = build_online_learning_panel();
    else if (module_.id == "meta_learning")
        content = build_meta_learning_panel();
    else if (module_.id == "hft")
        content = build_hft_panel();
    else if (module_.id == "rolling_retraining")
        content = build_rolling_retraining_panel();
    else if (module_.id == "feature_engineering")
        content = build_feature_engineering_panel();
    else if (module_.id == "portfolio_opt")
        content = build_portfolio_opt_panel();
    else if (module_.id == "factor_evaluation")
        content = build_factor_evaluation_panel();
    else if (module_.id == "strategy_builder")
        content = build_strategy_builder_panel();
    else if (module_.id == "data_processors")
        content = build_data_processors_panel();
    else if (module_.id == "quant_reporting")
        content = build_quant_reporting_panel();
    else
        content = build_generic_panel();

    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// GENERIC PANEL — for modules without specialized UI
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_generic_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* desc = new QLabel(module_.description, w);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; line-height:1.5;")
                            .arg(ui::colors::TEXT_PRIMARY())
                            .arg(ui::fonts::SMALL)
                            .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(desc);

    auto* script_lbl = new QLabel(QString("Python script: %1").arg(module_.script), w);
    script_lbl->setStyleSheet(QString("color:%1; font-family:%2;"
                                      "padding:6px; background:%3; border:1px solid %4; border-radius:2px;")
                                  .arg(ui::colors::TEXT_TERTIARY())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::BG_RAISED())
                                  .arg(ui::colors::BORDER_DIM()));
    vl->addWidget(script_lbl);

    // Command input
    auto* cmd = new QLineEdit(w);
    cmd->setPlaceholderText("Command (e.g. analyze, train, list_models)");
    cmd->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:6px 8px; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL));
    text_inputs_["gen_command"] = cmd;
    vl->addWidget(build_input_row("Command", cmd, w));

    // JSON params
    auto* params_edit = new QTextEdit(w);
    params_edit->setPlaceholderText("JSON parameters (optional)\ne.g. {\"ticker\":\"AAPL\"}");
    params_edit->setMaximumHeight(100);
    params_edit->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                       "font-family:%4; font-size:%5px; padding:6px; }")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::SMALL));
    vl->addWidget(params_edit);

    auto* run = make_run_button("EXECUTE", w);
    connect(run, &QPushButton::clicked, this, [this, params_edit]() {
        status_label_->setText("Running...");
        auto cmd_text = text_inputs_["gen_command"]->text().trimmed();
        if (cmd_text.isEmpty())
            cmd_text = "analyze";
        auto json_text = params_edit->toPlainText().trimmed();
        QJsonObject params;
        if (!json_text.isEmpty()) {
            auto doc = QJsonDocument::fromJson(json_text.toUtf8());
            if (!doc.isNull())
                params = doc.object();
        }
        AIQuantLabService::instance().run_module(module_.id, cmd_text, params);
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
// RESULT DISPLAY
// ═══════════════════════════════════════════════════════════════════════════════

void QuantModulePanel::clear_results() {
    if (!results_layout_)
        return;
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void QuantModulePanel::display_error(const QString& msg) {
    clear_results();
    auto* err = new QLabel(msg);
    err->setWordWrap(true);
    err->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:12px;"
                               "background:rgba(220,38,38,0.08); border:1px solid rgba(220,38,38,0.3);"
                               "border-radius:2px;")
                           .arg(ui::colors::NEGATIVE())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(err);
    status_label_->setText("Error");
}


// ── Loading spinner shown while a Python op is in flight ─────────────────────

void QuantModulePanel::show_loading(const QString& message) {
    clear_results();
    status_label_->setText(message);

    // Container card
    auto* box = new QWidget(this);
    box->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                           .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(box);
    hl->setContentsMargins(16, 14, 16, 14);
    hl->setSpacing(14);

    // Animated ASCII-style spinner (avoids needing a QMovie/gif resource).
    // Updates 5 frames/sec via a single QTimer parented to the box, so it's
    // garbage-collected automatically when the box is replaced by results.
    auto* spinner = new QLabel(box);
    spinner->setFixedSize(20, 20);
    spinner->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700; "
                                   "font-family:'Courier New'; background:transparent;")
                               .arg(module_.color.name()));
    static const QStringList kFrames = {"|", "/", "-", "\\"};
    spinner->setText(kFrames[0]);

    auto* timer = new QTimer(box);
    timer->setInterval(120);
    int frame_idx = 0;
    QPointer<QLabel> spin_guard(spinner);
    connect(timer, &QTimer::timeout, box, [spin_guard, frame_idx]() mutable {
        if (!spin_guard) return;
        frame_idx = (frame_idx + 1) % kFrames.size();
        spin_guard->setText(kFrames[frame_idx]);
    });
    timer->start();

    auto* msg = new QLabel(message, box);
    msg->setWordWrap(true);
    msg->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; background:transparent;")
                           .arg(ui::colors::TEXT_PRIMARY()));

    auto* hint = new QLabel(QString::fromUtf8("Running on the embedded Python runtime — "
                                              "first invocation per session takes longer (cold start)."),
                            box);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1; font-size:9px; background:transparent;")
                            .arg(ui::colors::TEXT_TERTIARY()));

    auto* text_col = new QVBoxLayout;
    text_col->setContentsMargins(0, 0, 0, 0);
    text_col->setSpacing(2);
    text_col->addWidget(msg);
    text_col->addWidget(hint);

    hl->addWidget(spinner);
    hl->addLayout(text_col, 1);

    results_layout_->addWidget(box);
}


void QuantModulePanel::on_error(const QString& module_id, const QString& message) {
    if (module_id != module_.id)
        return;
    if (module_id == "rl_trading" && rl_train_button_) {
        rl_train_button_->setEnabled(true);
    }
    display_error(message);
}

} // namespace fincept::screens
