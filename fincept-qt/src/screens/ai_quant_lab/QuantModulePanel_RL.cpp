// src/screens/ai_quant_lab/QuantModulePanel_RL.cpp
//
// RL Trading panel — train RL agents (PPO/DQN/A2C/SAC/TD3) on yfinance data
// with live progress + log console. Extracted from QuantModulePanel.cpp to keep
// that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

// ═══════════════════════════════════════════════════════════════════════════════
// RL TRADING PANEL
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_rl_trading_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* algo = new QComboBox(w);
    algo->addItems({"PPO", "DQN", "A2C", "SAC", "TD3"});
    algo->setStyleSheet(combo_ss());
    combo_inputs_["rl_algo"] = algo;
    vl->addWidget(build_input_row("RL Algorithm", algo, w));

    auto* ticker = new QLineEdit(w);
    ticker->setPlaceholderText("AAPL");
    ticker->setStyleSheet(input_ss());
    text_inputs_["rl_ticker"] = ticker;
    vl->addWidget(build_input_row("Ticker", ticker, w));

    auto* episodes = new QSpinBox(w);
    episodes->setRange(10, 10000);
    episodes->setValue(100);
    episodes->setStyleSheet(spinbox_ss());
    int_inputs_["rl_episodes"] = episodes;
    vl->addWidget(build_input_row("Training Episodes", episodes, w));

    auto* lr = make_double_spin(0.00001, 1.0, 0.0003, 5, "", w);
    double_inputs_["rl_learning_rate"] = lr;
    vl->addWidget(build_input_row("Learning Rate", lr, w));

    auto* capital = make_double_spin(1000, 1e12, 100000, 0, "", w);
    double_inputs_["rl_capital"] = capital;
    vl->addWidget(build_input_row("Initial Capital ($)", capital, w));

    rl_train_button_ = make_run_button("TRAIN RL AGENT", w);
    connect(rl_train_button_, &QPushButton::clicked, this, [this]() {
        if (!rl_train_button_ || !rl_log_console_) return;
        rl_log_console_->clear();
        rl_progress_bar_->setValue(0);
        rl_progress_bar_->setVisible(true);
        rl_progress_stats_->setText("step 0 / — · reward — · loss —");
        rl_progress_stats_->setVisible(true);
        rl_log_console_->setVisible(true);
        rl_train_button_->setEnabled(false);
        status_label_->setText("Training RL Agent...");
        QJsonObject params;
        params["algorithm"] = combo_inputs_["rl_algo"]->currentText();
        params["ticker"] = text_inputs_["rl_ticker"]->text();
        params["episodes"] = int_inputs_["rl_episodes"]->value();
        params["learning_rate"] = double_inputs_["rl_learning_rate"]->value();
        params["initial_capital"] = double_inputs_["rl_capital"]->value();
        AIQuantLabService::instance().train_rl_agent(params);
    });
    vl->addWidget(rl_train_button_);

    // Progress bar — hidden until training starts
    rl_progress_bar_ = new QProgressBar(w);
    rl_progress_bar_->setRange(0, 100);
    rl_progress_bar_->setValue(0);
    rl_progress_bar_->setTextVisible(true);
    rl_progress_bar_->setFixedHeight(18);
    rl_progress_bar_->setVisible(false);
    rl_progress_bar_->setStyleSheet(
        QString("QProgressBar { background:%1; border:1px solid %2; border-radius:2px; text-align:center; color:%3; }"
                "QProgressBar::chunk { background:%4; }")
            .arg(ui::colors::BG_RAISED())
            .arg(ui::colors::BORDER_MED())
            .arg(ui::colors::TEXT_PRIMARY())
            .arg(module_.color.name()));
    vl->addWidget(rl_progress_bar_);

    // Stats label — hidden until training starts
    rl_progress_stats_ = new QLabel(w);
    rl_progress_stats_->setText("");
    rl_progress_stats_->setVisible(false);
    rl_progress_stats_->setStyleSheet(
        QString("color:%1; background:transparent; font-family:%2; font-size:%3px;")
            .arg(ui::colors::TEXT_SECONDARY())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL));
    vl->addWidget(rl_progress_stats_);

    // Log console — hidden until training starts
    rl_log_console_ = new QPlainTextEdit(w);
    rl_log_console_->setReadOnly(true);
    rl_log_console_->setMaximumBlockCount(5000);
    rl_log_console_->setMinimumHeight(200);
    rl_log_console_->setVisible(false);
    rl_log_console_->setStyleSheet(
        QString("QPlainTextEdit { background:%1; color:%2; border:1px solid %3; border-radius:2px;"
                "font-family:%4; font-size:%5px; padding:6px; }")
            .arg(ui::colors::BG_BASE())
            .arg(ui::colors::TEXT_PRIMARY())
            .arg(ui::colors::BORDER_MED())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL));
    vl->addWidget(rl_log_console_);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

} // namespace fincept::screens
