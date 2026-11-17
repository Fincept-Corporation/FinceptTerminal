// QuantModulePanel_LiveLearning.cpp — live signals, online learning,
// meta learning, and rolling retraining panel builders.
// Method definitions split from QuantModulePanel_Misc.cpp.

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
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

// add_qlib_dataset(): QuantModulePanel_Common.h (shared with QuantModulePanel_Misc.cpp).

// ═══════════════════════════════════════════════════════════════════════════════
// LIVE SIGNALS PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_live_signals_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = make_view_tabs(w);
    // The two model views need a trained model: Heavy (a designed preview) until its id is known —
    // prefilled from the last model Model Library trained — then they run on first show.
    auto ls_model_spec = [](const QString& key, bool dataset, const QString& title, const QString& returns,
                                const QString& run_text) {
        ViewSpec s;
        s.key = key;
        s.cost = ViewCost::Heavy;
        s.loading = key == QLatin1String("model_signals") ? tr("Scoring the instruments with the model…")
                                                          : tr("Reading the model's feature importance…");
        s.shape = ui::LoadingSkeleton::Shape::Table;
        s.qlib_dataset = dataset;
        const QString input = key == QLatin1String("model_signals") ? QStringLiteral("ls_pr_model")
                                                                    : QStringLiteral("ls_fi_model");
        // No auto_when: measured 24 s (feature importance) and 57 s (model signals) — long even
        // with a model; the id is prefilled and the remembered result opens the view.
        Q_UNUSED(input)
        s.title = title;
        s.returns = returns + QLatin1Char(' ') +
                    tr("Train a model first in Model Library > Train Model: its id is filled in here.");
        s.run_text = run_text;
        s.cost_note = key == QLatin1String("model_signals") ? tr("Needs a trained model · about a minute, locally")
                                                            : tr("Needs a trained model · about 25 s, locally");
        return s;
    };
    // The last model Model Library trained (it stores the id when a training run succeeds).
    auto last_model = []() {
        const auto r = SettingsRepository::instance().get(QStringLiteral("ai_quant_lab/last_trained_model_id"));
        return r.is_ok() ? r.value().trimmed() : QString();
    };

    // ── Signal Data ──
    add_view_tab(tabs, {.key = QStringLiteral("signal_data"),
                  .cost = ViewCost::Heavy, // measured ~28 s per read: > ~10 s
                  .loading = tr("Reading the dataset's prices — about 30 s…"),
                  .shape = ui::LoadingSkeleton::Shape::Chart,
                  .qlib_dataset = true,
                  .title = tr("Signal data from the local dataset"),
                  .returns = tr("The instruments' prices and volumes over the recent window, read from the local "
                                "qlib dataset, as a chart and a table."),
                  .run_text = tr("FETCH SIGNALS"),
                  .cost_note = tr("Reads the local qlib dataset · about 30 s · no network")}, tr("Signal Data"),
                 [this](QWidget* sig_tab, QVBoxLayout* sigvl) {
        auto* sig_instr = new QLineEdit(sig_tab);
        sig_instr->setPlaceholderText(tr("Instruments (comma-separated, e.g. aapl,msft,goog)"));
        sig_instr->setStyleSheet(input_ss());
        text_inputs_["ls_instruments"] = sig_instr;
        sigvl->addWidget(build_input_row(tr("Instruments"), sig_instr, sig_tab));
        auto* sig_fields = new QLineEdit(sig_tab);
        sig_fields->setPlaceholderText(tr("Fields (e.g. $close,$open,$high,$low,$volume)"));
        sig_fields->setStyleSheet(input_ss());
        text_inputs_["ls_fields"] = sig_fields;
        sigvl->addWidget(build_input_row(tr("Fields"), sig_fields, sig_tab));
        auto* sig_start = new QLineEdit(sig_tab);
        sig_start->setPlaceholderText(tr("Start date (YYYY-MM-DD)"));
        sig_start->setStyleSheet(input_ss());
        text_inputs_["ls_start"] = sig_start;
        sigvl->addWidget(build_input_row(tr("Start Date"), sig_start, sig_tab));
        auto* sig_end = new QLineEdit(sig_tab);
        sig_end->setPlaceholderText(tr("End date (YYYY-MM-DD)"));
        sig_end->setStyleSheet(input_ss());
        text_inputs_["ls_end"] = sig_end;
        sigvl->addWidget(build_input_row(tr("End Date"), sig_end, sig_tab));
        auto* sig_run = make_run_button(tr("FETCH SIGNALS"), sig_tab);
        set_action(sig_run, [this]() {
            const QJsonArray instr = parse_csv_strings(text_inputs_["ls_instruments"]->text(), /*lowercase=*/true);
            if (instr.isEmpty()) {
                display_error(tr("Enter at least one instrument, e.g. aapl,msft."));
                return;
            }
            const QJsonArray fields = parse_csv_strings(text_inputs_["ls_fields"]->text());
            if (fields.isEmpty()) {
                display_error(tr("Enter at least one field, e.g. $close,$volume."));
                return;
            }
            QString iso_start, iso_end;
            if (const QString err =
                    check_iso_range(text_inputs_["ls_start"]->text(), text_inputs_["ls_end"]->text(), iso_start, iso_end);
                !err.isEmpty()) {
                display_error(err);
                return;
            }
            QJsonObject params;
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(tr("Fetching %1 field(s) for %2 instrument(s)...").arg(fields.size()).arg(instr.size()));
            params["instruments"] = instr;
            params["fields"] = fields;
            if (!iso_start.isEmpty())
                params["start_date"] = iso_start;
            if (!iso_end.isEmpty())
                params["end_date"] = iso_end;
            AIQuantLabService::instance().signals_get_data(params);
        });
        sigvl->addWidget(sig_run);
        sigvl->addStretch();
    });

    // ── Factor Analysis ──
    add_view_tab(tabs, {.key = QStringLiteral("factor_analysis"),
                  .cost = ViewCost::Heavy, // measured ~90 s on an 8-name pool
                  .loading = tr("Computing Alpha158 factors and their ICs — a minute or more…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .qlib_dataset = true,
                  .title = tr("Factor analysis"),
                  .returns = tr("The Alpha158 factors of the instruments over the window, ranked by their "
                                "cross-sectional IC with the next day's return."),
                  .run_text = tr("RUN FACTOR ANALYSIS"),
                  .cost_note = tr("Local · about 1-2 minutes of CPU for a small pool · needs at least 5 instruments")}, tr("Factor Analysis"),
                 [this](QWidget* fa_tab, QVBoxLayout* favl) {
        auto* fa_instr = new QLineEdit(fa_tab);
        fa_instr->setPlaceholderText(tr("Instruments (comma-separated)"));
        fa_instr->setStyleSheet(input_ss());
        text_inputs_["ls_fa_instr"] = fa_instr;
        favl->addWidget(build_input_row(tr("Instruments"), fa_instr, fa_tab));
        auto* fa_start = new QLineEdit(fa_tab);
        fa_start->setPlaceholderText(tr("Start date (YYYY-MM-DD)"));
        fa_start->setStyleSheet(input_ss());
        text_inputs_["ls_fa_start"] = fa_start;
        favl->addWidget(build_input_row(tr("Start Date"), fa_start, fa_tab));
        auto* fa_end = new QLineEdit(fa_tab);
        fa_end->setPlaceholderText(tr("End date (YYYY-MM-DD)"));
        fa_end->setStyleSheet(input_ss());
        text_inputs_["ls_fa_end"] = fa_end;
        favl->addWidget(build_input_row(tr("End Date"), fa_end, fa_tab));
        auto* fa_run = make_run_button(tr("RUN FACTOR ANALYSIS"), fa_tab);
        set_action(fa_run, [this]() {
            QJsonArray instr = parse_csv_strings(text_inputs_["ls_fa_instr"]->text(), /*lowercase=*/true);
            if (instr.isEmpty()) {
                // The dataset's pool is too large to run in seconds: its sample tickers instead.
                QStringList sample;
                for (const auto& v : qlib_dataset_.value(QStringLiteral("sample_instruments")).toArray())
                    sample << v.toString();
                // The script needs 5 names: fewer samples → the dataset's market pool.
                if (sample.size() < 5 && !qlib_dataset_.value(QStringLiteral("default_market")).toString().isEmpty())
                    sample = QStringList{qlib_dataset_.value(QStringLiteral("default_market")).toString()};
                if (!sample.isEmpty()) {
                    text_inputs_["ls_fa_instr"]->setText(sample.join(QLatin1Char(',')));
                    instr = parse_csv_strings(sample.join(QLatin1Char(',')), /*lowercase=*/true);
                }
            }
            if (instr.isEmpty()) {
                display_error(tr("Enter at least one instrument, e.g. aapl,msft."));
                return;
            }
            QString iso_start, iso_end;
            if (const QString err = check_iso_range(text_inputs_["ls_fa_start"]->text(), text_inputs_["ls_fa_end"]->text(),
                                                    iso_start, iso_end);
                !err.isEmpty()) {
                display_error(err);
                return;
            }
            QJsonObject params;
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(tr("Running factor analysis on %1 instrument(s)...").arg(instr.size()));
            params["instruments"] = instr;
            if (!iso_start.isEmpty())
                params["start_date"] = iso_start;
            if (!iso_end.isEmpty())
                params["end_date"] = iso_end;
            AIQuantLabService::instance().signals_get_factor_analysis(params);
        });
        favl->addWidget(fa_run);
        favl->addStretch();
    });

    // ── Feature Importance ──
    add_view_tab(tabs, ls_model_spec(QStringLiteral("feature_importance"), false, tr("Feature importance of a trained model"),
                               tr("Which features the model leans on (gain / split importance), ranked."),
                               tr("GET FEATURE IMPORTANCE")), tr("Feature Importance"),
                 [this, last_model](QWidget* fi_tab, QVBoxLayout* fivl) {
        // The saved models; with none picked, the newest tree model (only those have importances).
        auto* fi_model = make_saved_id_combo(SavedIds::TrainedModels, QStringLiteral("ls_fi_model"), last_model(),
                                             fi_tab, /*prefer_feature_importance=*/true);
        fivl->addWidget(build_input_row(tr("Model ID"), fi_model, fi_tab));
        auto* fi_run = make_run_button(tr("GET FEATURE IMPORTANCE"), fi_tab);
        set_action(fi_run, [this]() {
            const QString mid = saved_id(combo_inputs_.value(QStringLiteral("ls_fi_model")));
            if (mid.isEmpty()) {
                display_error(tr("Pick a trained model (Model Library > Train Model saves one), or type its ID."));
                return;
            }
            show_loading(tr("Loading feature importance for %1...").arg(mid));
            QJsonObject params;
            params["model_id"] = mid;
            AIQuantLabService::instance().signals_get_feature_importance(params);
        });
        fivl->addWidget(fi_run);
        fivl->addStretch();
    });

    // ── Model Signals ── (qlib_service.py predict: the trained model's scores per instrument)
    add_view_tab(tabs, ls_model_spec(QStringLiteral("model_signals"), true, tr("Signals of a trained model"),
                               tr("The trained model's score for each instrument over the window, ranked: what "
                                  "it would buy and sell."),
                               tr("GENERATE MODEL SIGNALS")), tr("Model Signals"),
                 [this, last_model](QWidget* pr_tab, QVBoxLayout* prvl) {
        auto* pr_model =
            make_saved_id_combo(SavedIds::TrainedModels, QStringLiteral("ls_pr_model"), last_model(), pr_tab);
        prvl->addWidget(build_input_row(tr("Model ID"), pr_model, pr_tab));
        auto* pr_instr = new QLineEdit(pr_tab);
        pr_instr->setPlaceholderText(tr("Instruments (optional; blank = the model's training universe)"));
        pr_instr->setStyleSheet(input_ss());
        text_inputs_["ls_pr_instr"] = pr_instr;
        prvl->addWidget(build_input_row(tr("Instruments"), pr_instr, pr_tab));
        auto* pr_start = new QLineEdit(pr_tab);
        pr_start->setPlaceholderText(tr("YYYY-MM-DD (blank: the last ~3 months of the dataset)"));
        pr_start->setStyleSheet(input_ss());
        text_inputs_["ls_pr_start"] = pr_start;
        prvl->addWidget(build_input_row(tr("Start Date"), pr_start, pr_tab));
        auto* pr_end = new QLineEdit(pr_tab);
        pr_end->setPlaceholderText(tr("YYYY-MM-DD (blank: the dataset's last day)"));
        pr_end->setStyleSheet(input_ss());
        text_inputs_["ls_pr_end"] = pr_end;
        prvl->addWidget(build_input_row(tr("End Date"), pr_end, pr_tab));
        auto* pr_run = make_run_button(tr("GENERATE MODEL SIGNALS"), pr_tab);
        set_action(pr_run, [this]() {
            const QString mid = saved_id(combo_inputs_.value(QStringLiteral("ls_pr_model")));
            if (mid.isEmpty()) {
                display_error(tr("Pick a trained model (Model Library > Train Model saves one), or type its ID."));
                return;
            }
            QString iso_start, iso_end;
            if (const QString err = check_iso_range(text_inputs_["ls_pr_start"]->text(), text_inputs_["ls_pr_end"]->text(),
                                                    iso_start, iso_end);
                !err.isEmpty()) {
                display_error(err);
                return;
            }
            QJsonObject params;
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(tr("Scoring instruments with %1...").arg(mid));
            params["model_id"] = mid;
            const QJsonArray instr = parse_csv_strings(text_inputs_["ls_pr_instr"]->text(), /*lowercase=*/true);
            if (!instr.isEmpty())
                params["instruments"] = instr;
            if (!iso_start.isEmpty())
                params["start_date"] = iso_start;
            if (!iso_end.isEmpty())
                params["end_date"] = iso_end;
            AIQuantLabService::instance().run_module(module_.id, QStringLiteral("predict"), params);
        });
        prvl->addWidget(pr_run);
        prvl->addStretch();
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);
    // A model the user can train / query: the one just created, else the newest listed. Fills the
    // Model ID of Incremental Train and Predict where it is still blank.
    auto ol_offer_model = [this](const QString& id, bool overwrite) {
        if (id.isEmpty())
            return;
        for (const char* key : {"train", "predict"})
            ensure_view_built(QLatin1String(key));
        for (const char* input : {"ol_train_id", "ol_pred_id"})
            if (QLineEdit* e = text_inputs_.value(QLatin1String(input)); e && (overwrite || e->text().trimmed().isEmpty()))
                e->setText(id);
    };
    auto ol_after = [this, ol_offer_model](bool ok, const QString& command, const QJsonObject& data) {
        if (!ok)
            return;
        if (command == QLatin1String("create_model")) {
            ol_offer_model(data.value("model_id").toString(), true);
            invalidate_view(QStringLiteral("models"));
        } else if (command == QLatin1String("list_models")) {
            const QJsonArray models = data.value("models").toArray();
            if (!models.isEmpty())
                ol_offer_model(models.last().toObject().value("model_id").toString(), false);
        }
    };
    // Train / Predict: Heavy (TRAIN mutates the model) until a model id is known; then the view opens
    // on that model's performance (read-only, seconds).
    auto ol_model_spec = [this, ol_after](const QString& key, const QString& input) {
        ViewSpec s;
        s.key = key;
        s.cost = ViewCost::Heavy;
        s.loading = tr("Reading the model's performance…");
        s.shape = ui::LoadingSkeleton::Shape::Cards;
        s.auto_when = [this, input]() {
            const QLineEdit* e = text_inputs_.value(input);
            return e && !e->text().trimmed().isEmpty();
        };
        s.after = ol_after;
        s.title = key == QLatin1String("train") ? tr("Train an online model one sample at a time")
                                                : tr("Predict with an online model");
        s.returns = tr("Needs an online model: create one in the Create Model tab (its id is filled in here). "
                       "With a model, this view opens on its running performance.");
        s.run_text = tr("SHOW PERFORMANCE");
        s.cost_note = tr("Local · river library");
        return s;
    };
    // The model's running performance (read-only): the default of Train and Predict.
    auto ol_performance = [this](const QString& input) {
        return [this, input]() {
            const QString mid = text_inputs_[input]->text().trimmed();
            if (mid.isEmpty()) {
                display_error(tr("Enter the ID of a model created in the Create Model tab."));
                return;
            }
            show_loading(tr("Loading performance for %1…").arg(mid));
            QJsonObject params;
            params["model_id"] = mid;
            AIQuantLabService::instance().online_performance(params);
        };
    };

    // ── Models ──
    add_view_tab(tabs, {.key = QStringLiteral("models"),
                  .cost = ViewCost::Fetch,
                  .loading = tr("Loading the online models…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .max_age_s = 600,
                  .after = ol_after}, tr("Models"),
                 [this](QWidget* models_tab, QVBoxLayout* mtvl) {
        auto* mt_info = new QLabel(tr("Incrementally trained models that update on each new data point."), models_tab);
        mt_info->setWordWrap(true);
        mt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-1))
                                   .arg(ui::fonts::DATA_FAMILY));
        mtvl->addWidget(mt_info);
        auto* mt_list = make_run_button(tr("LIST ALL MODELS"), models_tab);
        set_action(mt_list, [this]() {
            show_loading(tr("Loading online models..."));
            AIQuantLabService::instance().online_list_models();
        });
        mtvl->addWidget(mt_list);
        mtvl->addStretch();
    });

    // ── Create Model ──
    add_view_tab(tabs, {.key = QStringLiteral("create"),
                  .cost = ViewCost::Fetch,
                  .loading = tr("Loading the online models…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .max_age_s = 600,
                  .after = ol_after}, tr("Create Model"),
                 [this](QWidget* create_tab, QVBoxLayout* ctvl) {
        auto* ol_model_type = new QComboBox(create_tab);
        ol_model_type->setStyleSheet(combo_ss());
        ol_model_type->addItems({"linear", "bayesian_linear", "pa", "tree", "adaptive_tree", "bagging", "ewa", "srp"});
        combo_inputs_["ol_model_type"] = ol_model_type;
        ctvl->addWidget(build_input_row(tr("Model Type"), ol_model_type, create_tab));
        auto* ol_model_id = new QLineEdit(create_tab);
        ol_model_id->setPlaceholderText(tr("Model ID (optional, auto-generated if blank)"));
        ol_model_id->setStyleSheet(input_ss());
        text_inputs_["ol_model_id"] = ol_model_id;
        ctvl->addWidget(build_input_row(tr("Model ID"), ol_model_id, create_tab));
        set_default_action([this]() {
            show_loading(tr("Loading the online models…"));
            AIQuantLabService::instance().online_list_models();
        });
        auto* ol_create = make_run_button(tr("CREATE MODEL"), create_tab);
        set_action(ol_create, [this]() {
            show_loading(tr("Creating %1 online model...").arg(combo_inputs_["ol_model_type"]->currentText()));
            QJsonObject params;
            params["model_type"] = combo_inputs_["ol_model_type"]->currentText();
            auto mid = text_inputs_["ol_model_id"]->text().trimmed();
            if (!mid.isEmpty())
                params["model_id"] = mid;
            AIQuantLabService::instance().online_create_model(params);
        }, /*is_default=*/false);
        ctvl->addWidget(ol_create);
        ctvl->addStretch();
    });

    // ── Incremental Train ──
    add_view_tab(tabs, ol_model_spec(QStringLiteral("train"), QStringLiteral("ol_train_id")), tr("Incremental Train"),
                 [this, ol_performance](QWidget* train_tab, QVBoxLayout* ttvl) {
        set_default_action(ol_performance(QStringLiteral("ol_train_id")));
        auto* ol_tid = new QLineEdit(train_tab);
        ol_tid->setPlaceholderText(tr("Model ID"));
        ol_tid->setStyleSheet(input_ss());
        text_inputs_["ol_train_id"] = ol_tid;
        ttvl->addWidget(build_input_row(tr("Model ID"), ol_tid, train_tab));
        auto* ol_feats = new QLineEdit(train_tab);
        ol_feats->setPlaceholderText(R"({"close":94.0,"volume":1000000,"rsi":55.0})");
        ol_feats->setText(QStringLiteral(R"({"close":94.0,"volume":1000000,"rsi":55.0})"));
        ol_feats->setStyleSheet(input_ss());
        text_inputs_["ol_features"] = ol_feats;
        ttvl->addWidget(build_input_row(tr("Features (JSON)"), ol_feats, train_tab));
        auto* ol_target = new QLineEdit(train_tab);
        ol_target->setPlaceholderText(tr("Target value (e.g. 0.02)"));
        ol_target->setText(QStringLiteral("0.02"));
        ol_target->setStyleSheet(input_ss());
        text_inputs_["ol_target"] = ol_target;
        ttvl->addWidget(build_input_row(tr("Target"), ol_target, train_tab));
        auto* ol_train = make_run_button(tr("TRAIN ONE STEP"), train_tab);
        set_action(ol_train, [this]() {
            const QString mid = text_inputs_["ol_train_id"]->text().trimmed();
            if (mid.isEmpty()) {
                display_error(tr("Enter the ID of a model created in the Create Model tab."));
                return;
            }
            QJsonParseError perr{};
            const auto doc = QJsonDocument::fromJson(text_inputs_["ol_features"]->text().toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject() || doc.object().isEmpty()) {
                display_error(tr("Features must be a non-empty JSON object like "
                                 "{\"close\":94.0,\"rsi\":55.0} — %1")
                                  .arg(perr.errorString()));
                return;
            }
            // A blank / non-numeric target silently became 0.0 and corrupted the
            // model's running MAE with a fake observation.
            bool ok = false;
            const double target = text_inputs_["ol_target"]->text().trimmed().toDouble(&ok);
            if (!ok) {
                display_error(tr("Target must be a number (e.g. 0.02)."));
                return;
            }
            show_loading(tr("Feeding one sample to %1...").arg(mid));
            QJsonObject params;
            params["model_id"] = mid;
            params["features"] = doc.object();
            params["target"] = target;
            AIQuantLabService::instance().online_train(params);
        }, /*is_default=*/false);
        ttvl->addWidget(ol_train);
        ttvl->addStretch();
    });

    // ── Predict ──
    add_view_tab(tabs, ol_model_spec(QStringLiteral("predict"), QStringLiteral("ol_pred_id")), tr("Predict"),
                 [this, ol_performance](QWidget* pred_tab, QVBoxLayout* predvl) {
        set_default_action(ol_performance(QStringLiteral("ol_pred_id")));
        auto* ol_pid = new QLineEdit(pred_tab);
        ol_pid->setPlaceholderText(tr("Model ID"));
        ol_pid->setStyleSheet(input_ss());
        text_inputs_["ol_pred_id"] = ol_pid;
        predvl->addWidget(build_input_row(tr("Model ID"), ol_pid, pred_tab));
        auto* ol_pfeats = new QLineEdit(pred_tab);
        ol_pfeats->setPlaceholderText(R"({"close":95.0,"volume":1100000,"rsi":58.0})");
        ol_pfeats->setText(QStringLiteral(R"({"close":95.0,"volume":1100000,"rsi":58.0})"));
        ol_pfeats->setStyleSheet(input_ss());
        text_inputs_["ol_pred_feats"] = ol_pfeats;
        predvl->addWidget(build_input_row(tr("Features (JSON)"), ol_pfeats, pred_tab));
        auto* ol_pred = make_run_button(tr("PREDICT"), pred_tab);
        set_action(ol_pred, [this]() {
            const QString mid = text_inputs_["ol_pred_id"]->text().trimmed();
            if (mid.isEmpty()) {
                display_error(tr("Enter a model ID."));
                return;
            }
            QJsonParseError perr{};
            const auto doc = QJsonDocument::fromJson(text_inputs_["ol_pred_feats"]->text().toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject() || doc.object().isEmpty()) {
                display_error(tr("Features must be a non-empty JSON object — %1").arg(perr.errorString()));
                return;
            }
            show_loading(tr("Predicting with %1...").arg(mid));
            QJsonObject params;
            params["model_id"] = mid;
            params["features"] = doc.object();
            AIQuantLabService::instance().online_predict(params);
        }, /*is_default=*/false);
        predvl->addWidget(ol_pred);
        auto* ol_perf = make_run_button(tr("GET PERFORMANCE"), pred_tab);
        set_action(ol_perf, [this]() {
            const QString mid = text_inputs_["ol_pred_id"]->text().trimmed();
            if (mid.isEmpty()) {
                display_error(tr("Enter a model ID."));
                return;
            }
            show_loading(tr("Loading performance for %1...").arg(mid));
            QJsonObject params;
            params["model_id"] = mid;
            AIQuantLabService::instance().online_performance(params);
        }, /*is_default=*/false);
        predvl->addWidget(ol_perf);
        predvl->addStretch();
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    // ── Market data (shared by Model Selection and Hyperparameter Tuning) ──
    // qlib_meta_learning.py builds its dataset from these daily bars; the panel used to
    // send no data at all and every model was trained and scored on random numbers.
    auto* data_box = new QWidget(w);
    auto* dvl = new QVBoxLayout(data_box);
    dvl->setContentsMargins(0, 0, 0, 0);
    dvl->setSpacing(4);
    auto* data_info = new QLabel(tr("Models are trained on the ticker's daily bars: 16 scale-free features "
                                    "(returns, volatility, RSI, MACD, Bollinger position, ATR, volume, drawdown); "
                                    "target = forward return over the horizon (classification: up / down). "
                                    "The last ~20% of the window is held out and scored against a naive baseline."),
                                 data_box);
    data_info->setWordWrap(true);
    data_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::px(-1))
                                 .arg(ui::fonts::DATA_FAMILY));
    dvl->addWidget(data_info);
    auto* ml_ticker = new QLineEdit(series_default_symbol(), data_box); // the user's first watchlist symbol
    ml_ticker->setPlaceholderText(tr("Ticker (Yahoo Finance symbol, e.g. AAPL, MSFT, SPY)"));
    ml_ticker->setStyleSheet(input_ss());
    text_inputs_["ml_ticker"] = ml_ticker;
    dvl->addWidget(build_input_row(tr("Ticker"), ml_ticker, data_box));
    auto* ml_start = new QLineEdit(data_box);
    ml_start->setPlaceholderText(tr("YYYY-MM-DD — blank: 5 years before the end date"));
    ml_start->setStyleSheet(input_ss());
    text_inputs_["ml_start"] = ml_start;
    dvl->addWidget(build_input_row(tr("Start Date"), ml_start, data_box));
    auto* ml_end = new QLineEdit(data_box);
    ml_end->setPlaceholderText(tr("YYYY-MM-DD — blank: today"));
    ml_end->setStyleSheet(input_ss());
    text_inputs_["ml_end"] = ml_end;
    dvl->addWidget(build_input_row(tr("End Date"), ml_end, data_box));
    auto* ml_horizon = new QSpinBox(data_box);
    ml_horizon->setRange(1, 60);
    ml_horizon->setValue(5);
    ml_horizon->setSuffix(tr(" days"));
    ml_horizon->setStyleSheet(spinbox_ss());
    int_inputs_["ml_horizon"] = ml_horizon;
    dvl->addWidget(build_input_row(tr("Forecast Horizon"), ml_horizon, data_box));
    vl->addWidget(data_box);

    // Validates the market-data block and adds it to a request; false = error shown.
    auto add_data_params = [this](QJsonObject& params) -> bool {
        const QString ticker = text_inputs_["ml_ticker"]->text().trimmed().toUpper();
        if (ticker.isEmpty()) {
            display_error(tr("Enter a ticker — the models are trained on its daily bars."));
            return false;
        }
        QString iso_start, iso_end;
        if (const QString err =
                check_iso_range(text_inputs_["ml_start"]->text(), text_inputs_["ml_end"]->text(), iso_start, iso_end);
            !err.isEmpty()) {
            display_error(err);
            return false;
        }
        params["ticker"] = ticker;
        if (!iso_start.isEmpty())
            params["start_date"] = iso_start;
        if (!iso_end.isEmpty())
            params["end_date"] = iso_end;
        params["horizon"] = int_inputs_["ml_horizon"]->value();
        return true;
    };

    auto* tabs = make_view_tabs(w);
    // Training (selection, tuning, an ensemble) is minutes of CPU and only runs on its button; the
    // views open on what is already known: the candidate catalogue, the earlier results.
    auto ml_after = [this](bool ok, const QString& command, const QJsonObject& data) {
        if (!ok || command != QLatin1String("get_results"))
            return;
        // The trained model ids, for the Ensemble's Model Keys (when still blank).
        QStringList ids;
        const QJsonObject results = data.value("results").toObject();
        for (auto it = results.begin(); it != results.end(); ++it) {
            const QString id = it.value().toObject().value("model_id").toString();
            if (!id.isEmpty() && !ids.contains(id))
                ids << id;
        }
        if (QLineEdit* keys = text_inputs_.value(QStringLiteral("ml_ens_keys")); keys && keys->text().trimmed().isEmpty()
                                                                                && ids.size() >= 2)
            keys->setText(ids.join(QLatin1Char(',')));
    };

    // ── Model Selection ──
    add_view_tab(tabs, {.key = QStringLiteral("model_selection"),
                  .cost = ViewCost::Fetch,
                  .loading = tr("Loading the candidate models…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .max_age_s = 24 * 3600}, tr("Model Selection"),
                 [this, add_data_params](QWidget* sel_tab, QVBoxLayout* selvl) {
        auto* sel_info = new QLabel(tr("Train every candidate on the same market data and rank them on the hold-out "
                                       "window."),
                                    sel_tab);
        sel_info->setWordWrap(true);
        sel_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                    .arg(ui::colors::TEXT_SECONDARY())
                                    .arg(ui::fonts::px(-1))
                                    .arg(ui::fonts::DATA_FAMILY));
        selvl->addWidget(sel_info);
        auto* sel_list_btn = make_run_button(tr("LIST AVAILABLE MODELS"), sel_tab);
        set_action(sel_list_btn, [this]() {
            show_loading(tr("Loading candidate model catalog..."));
            AIQuantLabService::instance().meta_list_models();
        });
        selvl->addWidget(sel_list_btn);
        auto* sel_models = new QLineEdit(sel_tab);
        sel_models->setPlaceholderText(tr("Model IDs (comma-separated, e.g. lightgbm,xgboost,random_forest)"));
        sel_models->setText(QStringLiteral("lightgbm,xgboost,random_forest"));
        sel_models->setStyleSheet(input_ss());
        text_inputs_["ml_sel_models"] = sel_models;
        selvl->addWidget(build_input_row(tr("Models"), sel_models, sel_tab));
        auto* sel_task = new QComboBox(sel_tab);
        sel_task->setStyleSheet(combo_ss());
        sel_task->addItems({"regression", "classification"});
        combo_inputs_["ml_sel_task"] = sel_task;
        selvl->addWidget(build_input_row(tr("Task Type"), sel_task, sel_tab));
        auto* sel_run = make_run_button(tr("RUN MODEL SELECTION"), sel_tab);
        set_action(sel_run, [this, add_data_params]() {
            const QJsonArray model_ids = parse_csv_strings(text_inputs_["ml_sel_models"]->text(), /*lowercase=*/true);
            if (model_ids.size() < 2) {
                display_error(tr("Model selection needs at least two candidates, e.g. lightgbm,xgboost."));
                return;
            }
            QJsonObject params;
            if (!add_data_params(params))
                return;
            params["model_ids"] = model_ids;
            params["task_type"] = combo_inputs_["ml_sel_task"]->currentText();
            show_loading(tr("Training and ranking %1 candidate model(s) on %2...")
                             .arg(model_ids.size())
                             .arg(params["ticker"].toString()));
            AIQuantLabService::instance().meta_run_selection(params);
        }, /*is_default=*/false);
        selvl->addWidget(sel_run);
        selvl->addStretch();
    });

    // ── Ensemble ──
    add_view_tab(tabs, {.key = QStringLiteral("ensemble"),
                  .cost = ViewCost::Fetch,
                  .loading = tr("Loading the trained models…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .max_age_s = 600,
                  .after = ml_after}, tr("Ensemble"),
                 [this](QWidget* ens_tab, QVBoxLayout* ensvl) {
        auto* ens_info = new QLabel(tr("Fits the ensemble on the data its members were trained on and scores it on the "
                                       "same hold-out window. A model id (e.g. lightgbm) means its latest trained key; "
                                       "all members must come from one dataset (ticker, window, horizon, task)."),
                                    ens_tab);
        ens_info->setWordWrap(true);
        ens_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                    .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-1))
                                    .arg(ui::fonts::DATA_FAMILY));
        ensvl->addWidget(ens_info);
        auto* ens_keys = new QLineEdit(ens_tab);
        ens_keys->setPlaceholderText(tr("Model ids or keys, comma-separated (e.g. lightgbm,xgboost)"));
        ens_keys->setStyleSheet(input_ss());
        text_inputs_["ml_ens_keys"] = ens_keys;
        ensvl->addWidget(build_input_row(tr("Model Keys"), ens_keys, ens_tab));
        auto* ens_method = new QComboBox(ens_tab);
        ens_method->setStyleSheet(combo_ss());
        // qlib_meta_learning.py implements voting and stacking only ("averaging" always failed
        // with "Unknown ensemble method"; for regressors "voting" already averages).
        ens_method->addItems({"voting", "stacking"});
        combo_inputs_["ml_ens_method"] = ens_method;
        ensvl->addWidget(build_input_row(tr("Ensemble Method"), ens_method, ens_tab));
        set_default_action([this]() {
            show_loading(tr("Loading the trained models…"));
            AIQuantLabService::instance().meta_get_results();
        });
        auto* ens_run = make_run_button(tr("CREATE ENSEMBLE"), ens_tab);
        set_action(ens_run, [this]() {
            const QJsonArray keys = parse_csv_strings(text_inputs_["ml_ens_keys"]->text());
            if (keys.size() < 2) {
                display_error(tr("An ensemble needs at least two trained models (ids or keys from Model Selection)."));
                return;
            }
            show_loading(tr("Building a %1 ensemble from %2 model(s)...")
                             .arg(combo_inputs_["ml_ens_method"]->currentText())
                             .arg(keys.size()));
            QJsonObject params;
            params["model_keys"] = keys;
            params["method"] = combo_inputs_["ml_ens_method"]->currentText();
            AIQuantLabService::instance().meta_create_ensemble(params);
        }, /*is_default=*/false);
        ensvl->addWidget(ens_run);
        ensvl->addStretch();
    });

    // ── Hyperparameter Tuning ──
    add_view_tab(tabs, {.key = QStringLiteral("tuning"),
                  .cost = ViewCost::Fetch,
                  .loading = tr("Loading the earlier results…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .max_age_s = 600,
                  .after = ml_after}, tr("Hyperparameter Tuning"),
                 [this, add_data_params](QWidget* tune_tab, QVBoxLayout* tunevl) {
        auto* tune_model = new QComboBox(tune_tab);
        tune_model->setStyleSheet(combo_ss());
        tune_model->addItems({"lightgbm", "xgboost", "random_forest", "catboost"});
        combo_inputs_["ml_tune_model"] = tune_model;
        tunevl->addWidget(build_input_row(tr("Model"), tune_model, tune_tab));
        // The script defaulted to classification and the panel never said so.
        auto* tune_task = new QComboBox(tune_tab);
        tune_task->setStyleSheet(combo_ss());
        tune_task->addItems({"regression", "classification"});
        combo_inputs_["ml_tune_task"] = tune_task;
        tunevl->addWidget(build_input_row(tr("Task Type"), tune_task, tune_tab));
        auto* tune_grid = new QLineEdit(tune_tab);
        tune_grid->setPlaceholderText(R"({"n_estimators":[50,100,200],"max_depth":[3,5,7]})");
        tune_grid->setText(QStringLiteral(R"({"n_estimators":[50,100,200],"max_depth":[3,5,7]})"));
        tune_grid->setStyleSheet(input_ss());
        text_inputs_["ml_tune_grid"] = tune_grid;
        tunevl->addWidget(build_input_row(tr("Param Grid (JSON)"), tune_grid, tune_tab));
        auto* tune_method = new QComboBox(tune_tab);
        tune_method->setStyleSheet(combo_ss());
        tune_method->addItems({"grid", "random"});
        combo_inputs_["ml_tune_method"] = tune_method;
        tunevl->addWidget(build_input_row(tr("Search Method"), tune_method, tune_tab));
        auto* tune_run = make_run_button(tr("TUNE HYPERPARAMETERS"), tune_tab);
        set_action(tune_run, [this, add_data_params]() {
            QJsonParseError perr{};
            const auto doc = QJsonDocument::fromJson(text_inputs_["ml_tune_grid"]->text().toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject() || doc.object().isEmpty()) {
                display_error(tr("Param Grid must be a non-empty JSON object like "
                                 "{\"n_estimators\":[50,100],\"max_depth\":[3,5]} — %1")
                                  .arg(perr.errorString()));
                return;
            }
            QJsonObject params;
            if (!add_data_params(params))
                return;
            params["model_id"] = combo_inputs_["ml_tune_model"]->currentText();
            params["param_grid"] = doc.object();
            params["search_method"] = combo_inputs_["ml_tune_method"]->currentText();
            params["task_type"] = combo_inputs_["ml_tune_task"]->currentText();
            show_loading(tr("%1 search over %2 hyperparameter(s) on %3 — this can take a while...")
                             .arg(combo_inputs_["ml_tune_method"]->currentText())
                             .arg(doc.object().size())
                             .arg(params["ticker"].toString()));
            AIQuantLabService::instance().meta_tune_hyperparameters(params);
        }, /*is_default=*/false);
        tunevl->addWidget(tune_run);
        set_default_action([this]() {
            show_loading(tr("Loading meta-learning results…"));
            AIQuantLabService::instance().meta_get_results();
        });
        auto* results_btn = make_run_button(tr("GET ALL RESULTS"), tune_tab);
        set_action(results_btn, [this]() {
            show_loading(tr("Loading meta-learning results..."));
            AIQuantLabService::instance().meta_get_results();
        }, /*is_default=*/false);
        tunevl->addWidget(results_btn);
        tunevl->addStretch();
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    const QString accent = ui::colors::legible(module_.color);
    auto* tabs = make_view_tabs(w);

    // A config that exists: Execute Retrain's Model ID when still blank (or the one just created).
    auto rr_offer_config = [this](const QString& id, bool overwrite) {
        if (id.isEmpty())
            return;
        ensure_view_built(QStringLiteral("execute"));
        if (QLineEdit* e = text_inputs_.value(QStringLiteral("rr_exec_id"));
            e && (overwrite || e->text().trimmed().isEmpty()))
            e->setText(id);
    };

    // ── Tab 1: Retrain Configs ───────────────────────────────────────────────
    // The saved configs (<data root>/script-data/rolling_retraining/rolling_schedules.json),
    // listed on open as cards in the result
    // pane, each with Run Now / Delete; none yet → a designed empty state with CREATE CONFIG.
    ViewSpec configs_spec;
    configs_spec.key = QStringLiteral("configs");
    configs_spec.cost = ViewCost::Fetch;
    configs_spec.loading = tr("Loading the retrain configs…");
    configs_spec.shape = ui::LoadingSkeleton::Shape::Cards;
    configs_spec.max_age_s = 600;
    configs_spec.after = [rr_offer_config](bool ok, const QString& command, const QJsonObject& data) {
        if (!ok || command != QLatin1String("list"))
            return;
        const QJsonObject schedules = data.value("schedules").toObject();
        if (!schedules.isEmpty())
            rr_offer_config(schedules.keys().first(), false);
    };
    add_view_tab(tabs, configs_spec, tr("Retrain Configs"), [this](QWidget* list_tab, QVBoxLayout* ltvl) {
        // The real file: qlib_rolling_retraining.py keeps it under the data root the
        // terminal hands every script (FINCEPT_DATA_DIR = AppPaths::root()).
        const QString rr_file = QDir::toNativeSeparators(
            AppPaths::root() + QStringLiteral("/script-data/rolling_retraining/rolling_schedules.json"));
        // Zero-width spaces after the separators let the long path wrap at narrow widths.
        QString rr_file_wrapped = rr_file;
        rr_file_wrapped.replace(QDir::separator(), QString(QDir::separator()) + QChar(0x200B));
        auto* lt_info = new QLabel(tr("Retrain configs are saved in %1. They do not run on their own: start one "
                                      "with Run Now.")
                                       .arg(rr_file_wrapped),
                                   list_tab);
        lt_info->setToolTip(rr_file);
        lt_info->setWordWrap(true);
        lt_info->setStyleSheet(
            QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3)));
        ltvl->addWidget(lt_info);

        auto* lt_run = make_run_button(tr("REFRESH CONFIGS"), list_tab);
        set_action(lt_run, [this]() {
            show_loading(tr("Loading the retrain configs…"));
            AIQuantLabService::instance().rolling_list_schedules();
        });
        ltvl->addWidget(lt_run);
        ltvl->addStretch();
    });

    // ── Tab 2: Create Config ─────────────────────────────────────────────────
    // CREATE writes a config (a user action); the view opens on the PREVIEW of the windows it would
    // train (read-only, seconds), from a prefilled model id and step.
    ViewSpec create_spec;
    create_spec.key = QStringLiteral("create");
    create_spec.cost = ViewCost::Fetch;
    create_spec.loading = tr("Loading the retrain configs…");
    create_spec.shape = ui::LoadingSkeleton::Shape::Cards;
    create_spec.max_age_s = 600;
    create_spec.after = [rr_offer_config](bool ok, const QString& command, const QJsonObject& data) {
        if (!ok || command != QLatin1String("create"))
            return;
        rr_offer_config(data.value("model_id").toString(), true);
    };
    add_view_tab(tabs, create_spec, tr("Create Config"), [this, accent](QWidget* create_tab, QVBoxLayout* ctvl) {
        set_default_action([this]() {
            show_loading(tr("Loading the retrain configs…"));
            AIQuantLabService::instance().rolling_list_schedules();
        });
        auto* rr_model_id = new QLineEdit(create_tab);
        rr_model_id->setPlaceholderText(tr("Unique model ID (e.g. lgbm_sp500)"));
        rr_model_id->setText(QStringLiteral("lgbm_alpha158"));
        rr_model_id->setStyleSheet(input_ss());
        rr_model_id->setProperty("quantSignatureIgnore", true); // CREATE / PREVIEW inputs, not the list's
        text_inputs_["rr_model_id"] = rr_model_id;
        ctvl->addWidget(build_input_row(tr("Model ID *"), rr_model_id, create_tab));

        auto* rr_conf = new QLineEdit(create_tab);
        rr_conf->setPlaceholderText(tr("Optional: path to Qlib YAML config (leave blank for built-in LightGBM+Alpha158)"));
        rr_conf->setStyleSheet(input_ss());
        rr_conf->setProperty("quantSignatureIgnore", true); // CREATE / PREVIEW inputs, not the list's
        text_inputs_["rr_conf_path"] = rr_conf;
        ctvl->addWidget(build_input_row(tr("Config Path"), rr_conf, create_tab));

        // No Frequency input: nothing in the app runs a saved config on a timer — retrains
        // start only from Run Now / EXECUTE RETRAIN NOW, so a frequency and "next run"
        // promised runs that never happened.
        auto* rr_window = new QLineEdit(create_tab);
        rr_window->setPlaceholderText(tr("Trading days (blank: up to 5 years)"));
        rr_window->setToolTip(tr("Sets the training segment of the generated LightGBM+Alpha158 config, inside the "
                                 "dataset chosen in the bar above: this many trading days ending the day before "
                                 "validation (validation = the year before the dataset's last year, test = its last "
                                 "year). A config file you supply defines its own segments, so the window does not "
                                 "apply to it."));
        rr_window->setStyleSheet(input_ss());
        rr_window->setProperty("quantSignatureIgnore", true); // CREATE / PREVIEW inputs, not the list's
        text_inputs_["rr_window"] = rr_window;
        ctvl->addWidget(build_input_row(tr("Window (days)"), rr_window, create_tab));

        auto* rr_step = new QLineEdit(create_tab);
        rr_step->setPlaceholderText(tr("Step size between windows (default: 20)"));
        rr_step->setText(QStringLiteral("20"));
        rr_step->setStyleSheet(input_ss());
        rr_step->setProperty("quantSignatureIgnore", true); // CREATE / PREVIEW inputs, not the list's
        text_inputs_["rr_step"] = rr_step;
        ctvl->addWidget(build_input_row(tr("Step (days)"), rr_step, create_tab));

        // No region selector: the generated config reads the dataset chosen in the dataset bar
        // (its region, calendar and market pool come from that dataset).

        // Preview + Create buttons side by side
        auto* btn_row = new QWidget(create_tab);
        auto* btn_hl = new QHBoxLayout(btn_row);
        btn_hl->setContentsMargins(0, 4, 0, 0);
        btn_hl->setSpacing(8);

        auto* rr_preview = make_run_button(tr("PREVIEW WINDOWS"), btn_row);
        rr_preview->setProperty(kAccentShapeProp, QVariant()); // outlined, not module-filled
        rr_preview->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %2;border-radius:4px;"
                                          "font-size:%4px;font-weight:700;padding:6px 14px;}"
                                          "QPushButton:hover{background:%3;}")
                                      .arg(ui::colors::BG_RAISED(), accent, ui::colors::BG_HOVER(), ui::fonts::px(-3)));
        set_action(rr_preview, [this]() {
            const QString mid = text_inputs_["rr_model_id"]->text().trimmed();
            const QString cp = text_inputs_["rr_conf_path"]->text().trimmed();
            if (mid.isEmpty() && cp.isEmpty()) {
                display_error(tr("Enter a Model ID or Config Path to preview."));
                return;
            }
            // A non-numeric step used to silently become 0, which makes the window
            // generator produce zero (or infinitely many) windows.
            const QString step = text_inputs_["rr_step"]->text().trimmed();
            int step_days = 0;
            if (!step.isEmpty()) {
                bool ok = false;
                step_days = step.toInt(&ok);
                if (!ok || step_days <= 0) {
                    display_error(tr("Step must be a positive whole number of trading days (got '%1').").arg(step));
                    return;
                }
            }
            // A new id previews the config CREATE would write: the dataset chosen in the bar above
            // and the training window (without them the script used the freshest dataset and its
            // default window). A saved id previews its saved config, which names both itself.
            const QString win = text_inputs_["rr_window"]->text().trimmed();
            int win_days = 0;
            if (!win.isEmpty()) {
                bool ok = false;
                win_days = win.toInt(&ok);
                if (!ok || win_days <= 0) {
                    display_error(tr("Window must be a positive whole number of trading days (got '%1').").arg(win));
                    return;
                }
            }
            QJsonObject params;
            if (cp.isEmpty()) {
                // No dataset yet: still preview — a saved id does not need one, and for a new id
                // the script says what is missing.
                QString why;
                add_qlib_dataset(this, params, &why);
            }
            show_loading(tr("Generating the rolling windows…"));
            if (!mid.isEmpty())
                params["model_id"] = mid;
            if (!cp.isEmpty())
                params["conf_path"] = cp;
            if (win_days > 0)
                params["window"] = win_days;
            if (step_days > 0)
                params["step"] = step_days;
            AIQuantLabService::instance().rolling_preview_tasks(params);
        }, /*is_default=*/false);
        btn_hl->addWidget(rr_preview, 1);

        auto* rr_create = make_run_button(tr("CREATE CONFIG"), btn_row);
        rr_create->setProperty("rrCreateButton", true); // the empty list's CREATE CONFIG presses it
        set_action(rr_create, [this]() {
            const QString mid = text_inputs_["rr_model_id"]->text().trimmed();
            if (mid.isEmpty()) {
                display_error(tr("Model ID is required."));
                return;
            }
            // Window / step used to fall through as 0 on any non-numeric input.
            const QString win = text_inputs_["rr_window"]->text().trimmed();
            const QString step = text_inputs_["rr_step"]->text().trimmed();
            int win_days = 0, step_days = 0;
            if (!win.isEmpty()) {
                bool ok = false;
                win_days = win.toInt(&ok);
                if (!ok || win_days <= 0) {
                    display_error(tr("Window must be a positive whole number of trading days (got '%1').").arg(win));
                    return;
                }
            }
            if (!step.isEmpty()) {
                bool ok = false;
                step_days = step.toInt(&ok);
                if (!ok || step_days <= 0) {
                    display_error(tr("Step must be a positive whole number of trading days (got '%1').").arg(step));
                    return;
                }
            }
            // NOTE: step > window silently skips training data between windows.
            // Not blocked — gapped windows are a valid (if unusual) design — but it
            // is almost always a typo and deserves a UI warning eventually.
            QJsonObject params;
            const QString cp = text_inputs_["rr_conf_path"]->text().trimmed();
            // A config file names its own data; only the generated template needs a dataset.
            QString why;
            if (cp.isEmpty() && !add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(tr("Creating the retrain config %1…").arg(mid));
            params["model_id"] = mid;
            if (!cp.isEmpty())
                params["conf_path"] = cp;
            if (win_days > 0)
                params["window"] = win_days;
            if (step_days > 0)
                params["step"] = step_days;
            AIQuantLabService::instance().rolling_create_schedule(params);
        }, /*is_default=*/false);
        btn_hl->addWidget(rr_create, 1);
        ctvl->addWidget(btn_row);
        ctvl->addStretch();
    });

    // ── Tab 3: Execute Retrain ───────────────────────────────────────────────
    // Many minutes of CPU: never runs by itself. Its last outcome is remembered ("as of"); the
    // progress bar and per-window log stream live while it runs.
    ViewSpec exec_spec;
    exec_spec.key = QStringLiteral("execute");
    exec_spec.cost = ViewCost::Heavy;
    exec_spec.loading = tr("Retraining — progress and per-window log above…");
    exec_spec.shape = ui::LoadingSkeleton::Shape::Cards;
    exec_spec.qlib_dataset = true;
    exec_spec.after = [this](bool, const QString&, const QJsonObject&) {
        set_retrain_busy(this, false);
        invalidate_view(QStringLiteral("configs")); // last run / status changed
    };
    exec_spec.title = tr("Run a rolling retrain");
    exec_spec.returns = tr("Trains the config's model on each rolling window in turn and combines the results; "
                           "each window's progress streams into the log above. Create a config first (Create "
                           "Config tab) — its id is filled in here.");
    exec_spec.run_text = tr("EXECUTE RETRAIN NOW");
    exec_spec.cost_note = tr("Trains many models · can take tens of minutes of CPU · needs pyqlib + LightGBM");
    add_view_tab(tabs, exec_spec, tr("Execute Retrain"), [this](QWidget* retrain_tab, QVBoxLayout* retrainvl) {
        // AIQuantLabService::rolling_execute_retrain() forwards the script's JSON event lines as they
        // are printed, so the progress bar and per-window log below update while the run is going.
        auto* rt_info = new QLabel(tr("Executes a full rolling retrain for a saved config. Each window trains "
                                      "independently; the progress bar and per-window log below update as each "
                                      "window starts. Long runs can take many minutes."),
                                   retrain_tab);
        rt_info->setWordWrap(true);
        rt_info->setStyleSheet(
            QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3)));
        retrainvl->addWidget(rt_info);

        auto* rr_exec_id = new QLineEdit(retrain_tab);
        rr_exec_id->setPlaceholderText(tr("Model ID (must be in Retrain Configs)"));
        rr_exec_id->setStyleSheet(input_ss());
        text_inputs_["rr_exec_id"] = rr_exec_id;
        retrainvl->addWidget(build_input_row(tr("Model ID *"), rr_exec_id, retrain_tab));

        // Progress bar
        auto* rr_progress = new QProgressBar(retrain_tab);
        rr_progress->setObjectName("rr_progress");
        rr_progress->setRange(0, 100);
        rr_progress->setValue(0);
        rr_progress->setTextVisible(true);
        rr_progress->setFormat(tr("Idle"));
        rr_progress->setStyleSheet(
            QString("QProgressBar{background:%1;border:1px solid %2;border-radius:4px;"
                    "color:%3;font-size:%5px;font-weight:600;text-align:center;min-height:20px;}"
                    "QProgressBar::chunk{background:%4;border-radius:3px;}")
                // The percentage text sits on both the track and the chunk, so the chunk is a
                // tint of the module colour that TEXT_PRIMARY stays readable on in both themes.
                .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::TEXT_PRIMARY(),
                     ui::colors::tint(module_.color, 0.45), ui::fonts::px(-3)));
        retrainvl->addWidget(rr_progress);

        // Streaming log output
        auto* rr_log = new QTextEdit(retrain_tab);
        rr_log->setObjectName("rr_log");
        rr_log->setReadOnly(true);
        rr_log->setPlaceholderText(tr("Per-window training log appears here as the run progresses..."));
        rr_log->setStyleSheet(output_ss());
        rr_log->setMinimumHeight(140);
        retrainvl->addWidget(rr_log, 1);

        auto* rr_exec = make_run_button(tr("EXECUTE RETRAIN NOW"), retrain_tab);
        rr_exec->setObjectName(QStringLiteral("quantRunButton"));
        rr_exec->setProperty("rrRetrain", true);
        rr_exec->setProperty("rrExecButton", true);
        set_action(rr_exec, [this]() {
            const QString mid = text_inputs_["rr_exec_id"]->text().trimmed();
            if (mid.isEmpty()) {
                display_error(tr("Model ID is required."));
                return;
            }
            // Reset progress UI
            if (auto* pb = this->findChild<QProgressBar*>("rr_progress")) {
                pb->setValue(0);
                pb->setFormat(tr("Starting..."));
            }
            if (auto* log = this->findChild<QTextEdit*>("rr_log"))
                log->clear();
            show_loading(tr("Retraining %1 — progress and per-window log above…").arg(mid));
            set_retrain_busy(this, true); // re-enabled when the run ends (the view's `after`)
            QJsonObject params;
            params["model_id"] = mid;
            AIQuantLabService::instance().rolling_execute_retrain(params);
        });
        retrainvl->addWidget(rr_exec);
    });

    vl->addWidget(tabs);
    add_result_pane(vl);
    return w;
}

} // namespace fincept::screens
