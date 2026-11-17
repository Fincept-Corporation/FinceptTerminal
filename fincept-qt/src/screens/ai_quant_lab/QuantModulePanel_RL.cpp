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
#include <QTabWidget>
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

    // Two views: TRAIN (Heavy — minutes of CPU, never runs by itself) and EVALUATE (a replay of a
    // saved model: Heavy until a model exists, then it runs on first show).
    auto* tabs = make_view_tabs(w);

    ViewSpec train_spec;
    train_spec.key = QStringLiteral("train");
    train_spec.cost = ViewCost::Heavy;
    train_spec.loading = tr("Training the agent — progress and log above…");
    train_spec.shape = ui::LoadingSkeleton::Shape::Chart;
    train_spec.title = tr("Train a reinforcement-learning trading agent");
    train_spec.returns = tr("Trains the chosen algorithm on the ticker's daily bars, then compares its out-of-sample "
                            "replay with buy-and-hold (return, Sharpe, drawdown, trades) and saves the model for "
                            "EVALUATE.");
    train_spec.run_text = tr("TRAIN RL AGENT");
    train_spec.cost_note = tr("Runs locally · several minutes of CPU · needs torch + stable-baselines3");
    // A fresh train: progress complete, and the saved model goes to the Evaluate tab (remembered).
    train_spec.after = [this](bool ok, const QString&, const QJsonObject& data) {
        if (!ok) {
            if (rl_progress_bar_)
                rl_progress_bar_->setValue(0);
            if (rl_progress_stats_)
                rl_progress_stats_->setText(tr("Training failed — see the log above."));
            return;
        }
        if (rl_progress_bar_)
            rl_progress_bar_->setValue(100);
        const QString model_path = data.value("model_path").toString();
        if (model_path.isEmpty())
            return;
        ensure_view_built(QStringLiteral("evaluate"));
        if (QLineEdit* eval_path = text_inputs_.value(QStringLiteral("rl_eval_path")))
            eval_path->setText(model_path);
    };
    add_view_tab(tabs, train_spec, tr("Train Agent"), [this](QWidget* page, QVBoxLayout* tl) {
        auto* algo = new QComboBox(page);
        algo->addItems({"PPO", "DQN", "A2C", "SAC", "TD3"});
        algo->setStyleSheet(combo_ss());
        combo_inputs_["rl_algo"] = algo;
        tl->addWidget(build_input_row(tr("RL Algorithm"), algo, page));

        auto* ticker = new QLineEdit(page);
        ticker->setPlaceholderText(tr("e.g. AAPL — daily bars fetched from Yahoo Finance"));
        ticker->setText(series_default_symbol()); // the user's first watchlist symbol, else SPY
        ticker->setStyleSheet(input_ss());
        text_inputs_["rl_ticker"] = ticker;
        tl->addWidget(build_input_row(tr("Ticker"), ticker, page));

        auto* episodes = new QSpinBox(page);
        episodes->setRange(10, 10000);
        episodes->setValue(100);
        episodes->setStyleSheet(spinbox_ss());
        int_inputs_["rl_episodes"] = episodes;
        tl->addWidget(build_input_row(tr("Training Episodes"), episodes, page));

        auto* lr = make_double_spin(0.00001, 1.0, 0.0003, 5, "", page);
        double_inputs_["rl_learning_rate"] = lr;
        tl->addWidget(build_input_row(tr("Learning Rate"), lr, page));

        auto* capital = make_double_spin(1000, 1e12, 100000, 0, "", page);
        double_inputs_["rl_capital"] = capital;
        tl->addWidget(build_input_row(tr("Initial Capital ($)"), capital, page));

        rl_train_button_ = make_run_button(tr("TRAIN RL AGENT"), page);
        set_action(rl_train_button_, [this]() {
            // A blank ticker used to be forwarded verbatim, so the run failed deep
            // inside yfinance with an opaque traceback instead of here.
            const QString tk = text_inputs_["rl_ticker"]->text().trimmed().toUpper();
            if (tk.isEmpty()) {
                display_error(tr("Enter a ticker (e.g. AAPL, SPY, MSFT) before training."));
                return;
            }
            if (rl_log_console_ && rl_progress_bar_ && rl_progress_stats_) {
                rl_log_console_->clear();
                rl_progress_bar_->setValue(0);
                rl_progress_bar_->setVisible(true);
                rl_progress_stats_->setText(tr("step 0 / — · reward — · loss —"));
                rl_progress_stats_->setVisible(true);
                rl_log_console_->setVisible(true);
            }
            show_loading(tr("Training %1 on %2 — progress and log above; this can take several minutes…")
                             .arg(combo_inputs_["rl_algo"]->currentText(), tk));
            QJsonObject params;
            params["algorithm"] = combo_inputs_["rl_algo"]->currentText();
            params["ticker"] = tk;
            params["episodes"] = int_inputs_["rl_episodes"]->value();
            params["learning_rate"] = double_inputs_["rl_learning_rate"]->value();
            params["initial_capital"] = double_inputs_["rl_capital"]->value();
            AIQuantLabService::instance().train_rl_agent(params);
        });
        tl->addWidget(rl_train_button_);

        // Progress bar — hidden until training starts
        rl_progress_bar_ = new QProgressBar(page);
        rl_progress_bar_->setRange(0, 100);
        rl_progress_bar_->setValue(0);
        rl_progress_bar_->setTextVisible(true);
        rl_progress_bar_->setFixedHeight(row_px(18));
        rl_progress_bar_->setVisible(false);
        rl_progress_bar_->setStyleSheet(
            QString("QProgressBar { background:%1; border:1px solid %2; border-radius:2px; text-align:center; color:%3; }"
                    "QProgressBar::chunk { background:%4; }")
                .arg(ui::colors::BG_RAISED())
                .arg(ui::colors::BORDER_MED())
                .arg(ui::colors::TEXT_PRIMARY())
                // The percentage text sits on both the track and the chunk, so the chunk is a
                // tint of the module colour that TEXT_PRIMARY stays readable on in both themes.
                .arg(ui::colors::tint(module_.color, 0.45)));
        tl->addWidget(rl_progress_bar_);

        // Stats label — hidden until training starts
        rl_progress_stats_ = new QLabel(page);
        rl_progress_stats_->setVisible(false);
        rl_progress_stats_->setStyleSheet(QString("color:%1; background:transparent; font-family:%2; font-size:%3px;")
                                              .arg(ui::colors::TEXT_SECONDARY())
                                              .arg(ui::fonts::DATA_FAMILY)
                                              .arg(ui::fonts::px(-1)));
        tl->addWidget(rl_progress_stats_);

        // Log console — hidden until training starts
        rl_log_console_ = new QPlainTextEdit(page);
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
                .arg(ui::fonts::px(-1)));
        tl->addWidget(rl_log_console_);
        tl->addStretch();
    });

    // ── Evaluate a saved model ────────────────────────────────────────────────
    // Every TRAIN run saves its model (<data root>/script-data/rl_models); this replays one over a window of
    // daily bars next to buy-and-hold (qlib_rl.py evaluate). The path is filled in by the last
    // successful TRAIN and remembered; with a path the replay (deterministic, seconds) runs on
    // first show.
    ViewSpec eval_spec;
    eval_spec.key = QStringLiteral("evaluate");
    eval_spec.cost = ViewCost::Heavy;
    eval_spec.loading = tr("Replaying the saved model over daily bars…");
    eval_spec.shape = ui::LoadingSkeleton::Shape::Chart;
    eval_spec.auto_when = [this]() {
        const QLineEdit* path = text_inputs_.value(QStringLiteral("rl_eval_path"));
        return path && !path->text().trimmed().isEmpty();
    };
    eval_spec.title = tr("Evaluate a saved RL agent");
    eval_spec.returns = tr("Replays a model saved by TRAIN over daily bars and compares it with buy-and-hold. Train "
                           "an agent first (Train Agent tab) — its model is filled in here.");
    eval_spec.run_text = tr("EVALUATE SAVED MODEL");
    eval_spec.cost_note = tr("Needs a saved model · runs locally in seconds");
    add_view_tab(tabs, eval_spec, tr("Evaluate Saved Model"), [this](QWidget* page, QVBoxLayout* el) {
        auto* eval_info = new QLabel(tr("Replays a model saved by TRAIN over daily bars and compares it with "
                                        "buy-and-hold. Leave the dates blank to use the bars after the model's "
                                        "training data (out of sample) up to today."),
                                     page);
        eval_info->setWordWrap(true);
        eval_info->setStyleSheet(QString("color:%1; background:transparent; font-family:%2; font-size:%3px;")
                                     .arg(ui::colors::TEXT_SECONDARY())
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::px(-1)));
        el->addWidget(eval_info);

        auto* eval_path = new QLineEdit(page);
        eval_path->setObjectName(QStringLiteral("rlEvalModelPath"));
        eval_path->setPlaceholderText(tr("Model file (.zip) — filled in by the last TRAIN run"));
        eval_path->setStyleSheet(input_ss());
        text_inputs_["rl_eval_path"] = eval_path;
        el->addWidget(build_input_row(tr("Model Path"), eval_path, page));

        auto* eval_ticker = new QLineEdit(page);
        eval_ticker->setPlaceholderText(tr("Optional — defaults to the ticker the model was trained on"));
        eval_ticker->setStyleSheet(input_ss());
        text_inputs_["rl_eval_ticker"] = eval_ticker;
        el->addWidget(build_input_row(tr("Ticker"), eval_ticker, page));

        auto* eval_start = new QLineEdit(page);
        eval_start->setPlaceholderText(tr("Optional YYYY-MM-DD — default: the day after the training data"));
        eval_start->setStyleSheet(input_ss());
        text_inputs_["rl_eval_start"] = eval_start;
        el->addWidget(build_input_row(tr("Start Date"), eval_start, page));

        auto* eval_end = new QLineEdit(page);
        eval_end->setPlaceholderText(tr("Optional YYYY-MM-DD — default: today"));
        eval_end->setStyleSheet(input_ss());
        text_inputs_["rl_eval_end"] = eval_end;
        el->addWidget(build_input_row(tr("End Date"), eval_end, page));

        auto* eval_capital = make_double_spin(1000, 1e12, 100000, 0, "", page);
        double_inputs_["rl_eval_capital"] = eval_capital;
        el->addWidget(build_input_row(tr("Initial Capital ($)"), eval_capital, page));

        auto* eval_run = make_run_button(tr("EVALUATE SAVED MODEL"), page);
        set_action(eval_run, [this]() {
            const QString path = text_inputs_["rl_eval_path"]->text().trimmed();
            if (path.isEmpty()) {
                display_error(tr("Enter the path of a saved model — TRAIN RL AGENT fills it in when a run finishes."));
                return;
            }
            QString iso_start, iso_end;
            if (const QString err = check_iso_range(text_inputs_["rl_eval_start"]->text(),
                                                    text_inputs_["rl_eval_end"]->text(), iso_start, iso_end);
                !err.isEmpty()) {
                display_error(err);
                return;
            }
            QJsonObject params;
            params["model_path"] = path;
            const QString tk = text_inputs_["rl_eval_ticker"]->text().trimmed().toUpper();
            if (!tk.isEmpty())
                params["ticker"] = tk;
            if (!iso_start.isEmpty())
                params["start_date"] = iso_start;
            if (!iso_end.isEmpty())
                params["end_date"] = iso_end;
            // The policy is evaluated deterministically on a fixed bar series, so one
            // replay is the whole result (more episodes would repeat it exactly).
            params["n_episodes"] = 1;
            params["initial_capital"] = double_inputs_["rl_eval_capital"]->value();
            show_loading(tr("Replaying the saved model over daily bars…"));
            AIQuantLabService::instance().run_module(QStringLiteral("rl_trading"), QStringLiteral("evaluate"), params);
        });
        el->addWidget(eval_run);
        el->addStretch();
    });

    vl->addWidget(tabs);
    add_result_pane(vl);
    return w;
}

} // namespace fincept::screens
