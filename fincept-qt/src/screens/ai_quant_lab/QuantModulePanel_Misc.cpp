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
#include "ui/widgets/AutoLoadPane.h"

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

namespace {

// add_qlib_dataset(): QuantModulePanel_Common.h (shared with QuantModulePanel_LiveLearning.cpp).

/// A short note under a form (what a ticker source computes, what a field means).
QLabel* misc_hint(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_TERTIARY(), ui::fonts::px(-3)));
    return label;
}

/// The info line above a catalogue view's buttons.
QLabel* misc_info(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                             .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-1))
                             .arg(ui::fonts::DATA_FAMILY));
    return label;
}

/// Settings keys shared with other panels (Live Signals reads the last trained model id).
constexpr const char* kLastTrainedModelKey = "ai_quant_lab/last_trained_model_id";
constexpr const char* kLastPipelineKey = "ai_quant_lab/last_pipeline_id";

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// ADVANCED MODELS PANEL
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_advanced_models_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // One view. Training is class C (Heavy): it never runs by itself. The pane under the form
    // opens on the last result ("as of") or on a preview of what a run returns, with one button.
    // Measured: SPY, LSTM 64x2, 20 epochs over 5 years of bars = 27 s (download + torch import ~22 s).
    begin_view({.key = QStringLiteral("train"),
                .cost = ViewCost::Heavy,
                .loading = tr("Downloading the bars and training the network…"),
                .shape = ui::LoadingSkeleton::Shape::Chart,
                .title = tr("Train a sequence model"),
                .returns = tr("Trains an LSTM, GRU or Transformer on the ticker's daily bars and scores it on a "
                              "held-out window against the naive mean-return forecast: loss curve, hold-out RMSE, "
                              "direction hit rate and IC."),
                .run_text = tr("CREATE & TRAIN MODEL"),
                .cost_note = tr("Trains a neural network · about 30 s of CPU, longer with more epochs")});

    // Market data: the network is trained on this ticker's daily bars (the panel used to
    // send none, and the script fit the network to random numbers).
    vl->addWidget(misc_info(tr("Trains on the ticker's daily bars: windows of 20 bars of 16 standardised "
                               "features, target = forward return over the horizon. The last ~20% of the "
                               "window is held out and compared with the naive mean-return forecast."),
                            w));
    // The user's context first (first watchlist symbol, else SPY), as the series views.
    auto* adv_ticker = new QLineEdit(series_default_symbol(), w);
    adv_ticker->setPlaceholderText(tr("Ticker (Yahoo Finance symbol, e.g. AAPL, MSFT, SPY)"));
    adv_ticker->setStyleSheet(input_ss());
    text_inputs_["adv_ticker"] = adv_ticker;
    vl->addWidget(build_input_row(tr("Ticker"), adv_ticker, w));
    auto* adv_start = new QLineEdit(w);
    adv_start->setPlaceholderText(tr("YYYY-MM-DD — blank: 5 years before the end date"));
    adv_start->setStyleSheet(input_ss());
    text_inputs_["adv_start"] = adv_start;
    vl->addWidget(build_input_row(tr("Start Date"), adv_start, w));
    auto* adv_end = new QLineEdit(w);
    adv_end->setPlaceholderText(tr("YYYY-MM-DD — blank: today"));
    adv_end->setStyleSheet(input_ss());
    text_inputs_["adv_end"] = adv_end;
    vl->addWidget(build_input_row(tr("End Date"), adv_end, w));
    auto* adv_horizon = new QSpinBox(w);
    adv_horizon->setRange(1, 60);
    adv_horizon->setValue(5);
    adv_horizon->setSuffix(tr(" days"));
    adv_horizon->setStyleSheet(spinbox_ss());
    int_inputs_["adv_horizon"] = adv_horizon;
    vl->addWidget(build_input_row(tr("Forecast Horizon"), adv_horizon, w));

    auto* model_type = new QComboBox(w);
    // Only the architectures qlib_advanced_models.py implements. The combo also offered
    // Localformer / HIST / GAT / LightGBM / XGBoost / CatBoost, every one of which the
    // script rejected as an unknown model type.
    model_type->addItems({"LSTM", "GRU", "Transformer"});
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
    epochs->setValue(20);
    epochs->setStyleSheet(hidden->styleSheet());
    int_inputs_["adv_epochs"] = epochs;
    vl->addWidget(build_input_row(tr("Epochs"), epochs, w));

    auto* run = make_run_button(tr("CREATE && TRAIN MODEL"), w);
    set_action(run, [this]() {
        const QString ticker = text_inputs_["adv_ticker"]->text().trimmed().toUpper();
        if (ticker.isEmpty()) {
            display_error(tr("Enter a ticker — the model is trained on its daily bars."));
            return;
        }
        QString iso_start, iso_end;
        if (const QString err =
                check_iso_range(text_inputs_["adv_start"]->text(), text_inputs_["adv_end"]->text(), iso_start, iso_end);
            !err.isEmpty()) {
            display_error(err);
            return;
        }
        // nn.TransformerEncoder splits the hidden size over 4 attention heads.
        if (combo_inputs_["adv_model"]->currentText() == QLatin1String("Transformer") &&
            int_inputs_["adv_hidden"]->value() % 4 != 0) {
            display_error(tr("For a Transformer the hidden size must be a multiple of 4 (4 attention heads)."));
            return;
        }
        show_loading(tr("Training %1 on %2 (%3 epochs) — this can take a few minutes...")
                         .arg(combo_inputs_["adv_model"]->currentText(), ticker)
                         .arg(int_inputs_["adv_epochs"]->value()));
        QJsonObject params;
        params["model_type"] = combo_inputs_["adv_model"]->currentText();
        params["ticker"] = ticker;
        if (!iso_start.isEmpty())
            params["start_date"] = iso_start;
        if (!iso_end.isEmpty())
            params["end_date"] = iso_end;
        params["horizon"] = int_inputs_["adv_horizon"]->value();
        QJsonObject config;
        config["hidden_size"] = int_inputs_["adv_hidden"]->value();
        config["num_layers"] = int_inputs_["adv_layers"]->value();
        config["dropout"] = double_inputs_["adv_dropout"]->value();
        config["epochs"] = int_inputs_["adv_epochs"]->value();
        params["model_config"] = config;
        AIQuantLabService::instance().train_advanced_model(params);
    });
    vl->addWidget(run);
    end_view();

    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);

    // ── Indicators tab ──
    add_view_tab(tabs, {.key = QStringLiteral("indicators"), .cost = ViewCost::Fetch, .loading = tr("Computing the indicator…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Indicators"),
                 [this](QWidget* ind, QVBoxLayout* ivl) {
        // Real data by default (D1): the symbol's daily closes / highs / lows / volumes; the boxes
        // are the Custom series.
        add_series_source(ind, ivl, SeriesShape::Single);
        auto* ind_data = new QLineEdit(ind);
        ind_data->setPlaceholderText(tr("Close prices (comma-separated, e.g. 100,102,101,105,108)"));
        ind_data->setStyleSheet(input_ss());
        text_inputs_["fe_data"] = ind_data;
        mark_series_input(ind_data, SeriesKind::Closes);
        ivl->addWidget(build_input_row(tr("Price Data"), ind_data, ind));

        // High / Low / Volume series for the range- and volume-based indicators (ATR,
        // stochastic, OBV, VWAP). Same length as the close series; enabled only when the
        // selected indicator reads them.
        auto* ind_high = new QLineEdit(ind);
        ind_high->setPlaceholderText(tr("High prices, one per close (comma-separated)"));
        ind_high->setStyleSheet(input_ss());
        text_inputs_["fe_high"] = ind_high;
        mark_series_input(ind_high, SeriesKind::High);
        ivl->addWidget(build_input_row(tr("High"), ind_high, ind));
        auto* ind_low = new QLineEdit(ind);
        ind_low->setPlaceholderText(tr("Low prices, one per close (comma-separated)"));
        ind_low->setStyleSheet(input_ss());
        text_inputs_["fe_low"] = ind_low;
        mark_series_input(ind_low, SeriesKind::Low);
        ivl->addWidget(build_input_row(tr("Low"), ind_low, ind));
        auto* ind_volume = new QLineEdit(ind);
        ind_volume->setPlaceholderText(tr("Volume, one per close (comma-separated)"));
        ind_volume->setStyleSheet(input_ss());
        text_inputs_["fe_volume"] = ind_volume;
        mark_series_input(ind_volume, SeriesKind::Volume);
        ivl->addWidget(build_input_row(tr("Volume"), ind_volume, ind));

        auto* ind_indicator = new QComboBox(ind);
        ind_indicator->addItems({"moving_average", "rsi", "macd", "bollinger_bands", "momentum", "volatility", "returns",
                                 "log_returns", "drawdown", "atr", "stochastic_oscillator", "obv", "vwap"});
        ind_indicator->setStyleSheet(combo_ss());
        combo_inputs_["fe_indicator"] = ind_indicator;
        ivl->addWidget(build_input_row(tr("Indicator"), ind_indicator, ind));

        auto* ind_window = new QSpinBox(ind);
        ind_window->setRange(2, 200);
        ind_window->setValue(14);
        ind_window->setStyleSheet(spinbox_ss());
        int_inputs_["fe_window"] = ind_window;
        ivl->addWidget(build_input_row(tr("Window"), ind_window, ind));

        // Indicators that read the Window (qlib_feature_engineering.py): MACD uses its own
        // 12/26/9 periods, returns / log returns a 1-bar change and drawdown, OBV and VWAP
        // no parameter; for the stochastic it is the %K lookback (%D is its 3-bar mean).
        const QStringList windowed = {"moving_average", "rsi",   "bollinger_bands",      "momentum",
                                      "volatility",     "atr",   "stochastic_oscillator"};
        // Which extra series each indicator needs, in the order the script reads them.
        const QHash<QString, QStringList> extra_series = {
            {QStringLiteral("atr"), {QStringLiteral("high"), QStringLiteral("low")}},
            {QStringLiteral("stochastic_oscillator"), {QStringLiteral("high"), QStringLiteral("low")}},
            {QStringLiteral("obv"), {QStringLiteral("volume")}},
            {QStringLiteral("vwap"), {QStringLiteral("high"), QStringLiteral("low"), QStringLiteral("volume")}},
        };
        auto sync_window = [ind_window, ind_high, ind_low, ind_volume, windowed, extra_series](const QString& indicator) {
            const bool uses = windowed.contains(indicator);
            ind_window->setEnabled(uses);
            ind_window->setToolTip(uses ? QString() : tr("%1 does not use a window.").arg(indicator));
            const QStringList needs = extra_series.value(indicator);
            ind_high->setEnabled(needs.contains(QStringLiteral("high")));
            ind_low->setEnabled(needs.contains(QStringLiteral("low")));
            ind_volume->setEnabled(needs.contains(QStringLiteral("volume")));
        };
        connect(ind_indicator, &QComboBox::currentTextChanged, ind_window, sync_window);
        sync_window(ind_indicator->currentText());

        auto* ind_run = make_run_button(tr("COMPUTE INDICATOR"), ind);
        set_action(ind_run, [this, windowed, extra_series]() {
            QJsonArray series;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("fe_data")), series, &bad)) {
                display_error(tr("Price Data: '%1' is not numeric.").arg(bad));
                return;
            }
            const QString indicator = combo_inputs_["fe_indicator"]->currentText();
            const bool uses_window = windowed.contains(indicator);
            const int window = int_inputs_["fe_window"]->value();
            if (uses_window && series.size() < window) {
                display_error(
                    tr("A %1-period window needs at least %1 prices; you provided %2.").arg(window).arg(series.size()));
                return;
            }
            if (series.size() < 2) {
                display_error(tr("Enter at least two prices."));
                return;
            }
            QJsonObject params;
            // High / Low / Volume: required by this indicator, numeric, one value per close.
            for (const QString& key : extra_series.value(indicator)) {
                QJsonArray extra;
                QString bad_extra;
                const QString label = key == QLatin1String("high")  ? tr("High")
                                      : key == QLatin1String("low") ? tr("Low")
                                                                    : tr("Volume");
                if (!parse_doubles(series_text(QStringLiteral("fe_") + key), extra, &bad_extra)) {
                    display_error(tr("%1: '%2' is not numeric.").arg(label, bad_extra));
                    return;
                }
                if (extra.size() != series.size()) {
                    display_error(tr("%1 needs %2 values (one per close); you provided %3.")
                                      .arg(label)
                                      .arg(series.size())
                                      .arg(extra.size()));
                    return;
                }
                params[key] = extra;
            }
            show_loading(tr("Computing %1 over %2 prices...").arg(indicator).arg(series.size()));
            params["data"] = series; // the validated numeric array, not the raw text field
            params["indicator"] = indicator;
            if (indicator == QLatin1String("stochastic_oscillator"))
                params["k_window"] = window;
            else if (uses_window)
                params["window"] = window;
            AIQuantLabService::instance().feature_compute(params);
        });
        ivl->addWidget(ind_run);
        ivl->addStretch();
    });

    // ── Feature Selection tab ──
    add_view_tab(tabs, {.key = QStringLiteral("feature_selection"), .cost = ViewCost::Fetch, .loading = tr("Ranking the features by IC…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Feature Selection"),
                 [this](QWidget* sel, QVBoxLayout* svl) {
        // Real data by default (D1): five classic single-name factors of the symbol, each known at
        // the close of day t, ranked by their IC with the next day's return.
        add_series_source(sel, svl, SeriesShape::Single);
        svl->addWidget(misc_hint(tr("Ticker source: momentum (20d), reversal (5d), volatility (20d), volume ratio "
                                    "(20d) and the day's range, scored against the next day's return."),
                                 sel));
        auto* sel_features = new QLineEdit(sel);
        sel_features->setPlaceholderText(tr("Feature values JSON: {\"rsi\":[...],\"macd\":[...]}"));
        sel_features->setStyleSheet(input_ss());
        text_inputs_["fe_sel_features"] = sel_features;
        mark_series_input(sel_features, SeriesKind::FactorsJson);
        svl->addWidget(build_input_row(tr("Features (JSON)"), sel_features, sel));

        auto* sel_returns = new QLineEdit(sel);
        sel_returns->setPlaceholderText(tr("Target returns (comma-separated)"));
        sel_returns->setStyleSheet(input_ss());
        text_inputs_["fe_sel_returns"] = sel_returns;
        mark_series_input(sel_returns, SeriesKind::FwdReturns);
        svl->addWidget(build_input_row(tr("Returns"), sel_returns, sel));

        auto* sel_topk = new QSpinBox(sel);
        sel_topk->setRange(1, 50);
        sel_topk->setValue(5);
        sel_topk->setStyleSheet(spinbox_ss());
        int_inputs_["fe_topk"] = sel_topk;
        svl->addWidget(build_input_row(tr("Top-K Features"), sel_topk, sel));

        auto* sel_run = make_run_button(tr("SELECT FEATURES BY IC"), sel);
        set_action(sel_run, [this]() {
            // JSON parse failures used to be swallowed: params["features"] was simply
            // omitted and the run failed server-side with an unrelated message.
            QJsonParseError perr{};
            const auto doc = QJsonDocument::fromJson(series_text(QStringLiteral("fe_sel_features")).toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                display_error(
                    tr("Features must be a JSON object like {\"rsi\":[…],\"macd\":[…]} — %1").arg(perr.errorString()));
                return;
            }
            QJsonArray rets;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("fe_sel_returns")), rets, &bad)) {
                display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (rets.isEmpty()) {
                display_error(tr("Enter the target return series."));
                return;
            }
            if (rets.size() < 10) {
                display_error(tr("IC ranking needs at least 10 paired observations; you provided %1 returns.")
                                  .arg(rets.size()));
                return;
            }
            show_loading(tr("Ranking %1 feature(s) by IC...").arg(doc.object().size()));
            QJsonObject params;
            params["features"] = doc.object();
            params["returns"] = rets; // the validated numeric array, not the raw text field
            params["top_k"] = int_inputs_["fe_topk"]->value();
            AIQuantLabService::instance().feature_select_by_ic(params);
        });
        svl->addWidget(sel_run);
        svl->addStretch();
    });

    // ── Expression Engine tab ──
    add_view_tab(tabs, {.key = QStringLiteral("expression"), .cost = ViewCost::Fetch, .loading = tr("Evaluating the expression…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Expression Engine"),
                 [this](QWidget* expr, QVBoxLayout* evl) {
        // Real data by default (D1): the symbol's daily open / high / low / close / volume.
        add_series_source(expr, evl, SeriesShape::Single);
        auto* expr_data = new QLineEdit(expr);
        expr_data->setPlaceholderText(tr("{\"close\":[100,102,...],\"volume\":[1000,1200,...]}"));
        expr_data->setStyleSheet(input_ss());
        text_inputs_["fe_expr_data"] = expr_data;
        mark_series_input(expr_data, SeriesKind::OhlcvJson);
        evl->addWidget(build_input_row(tr("OHLCV Data (JSON)"), expr_data, expr));

        auto* expr_expr = new QLineEdit(expr);
        expr_expr->setPlaceholderText(tr("e.g. Mean(close, 5) / Std(close, 20)"));
        // A 20-day z-score of the close: how far the price stands from its moving average.
        expr_expr->setText(QStringLiteral("(close - Mean(close, 20)) / Std(close, 20)"));
        expr_expr->setToolTip(tr("Fields: open, high, low, close, volume. Functions: Ref, Mean, Std, Sum, Max, Min, "
                                 "Rank, Log, Abs, Sign, Greater, Less, Delta. Operators: + - * / **."));
        expr_expr->setStyleSheet(input_ss());
        text_inputs_["fe_expression"] = expr_expr;
        evl->addWidget(build_input_row(tr("Expression"), expr_expr, expr));

        auto* expr_run = make_run_button(tr("EVALUATE EXPRESSION"), expr);
        set_action(expr_run, [this]() {
            QJsonParseError perr{};
            const auto doc = QJsonDocument::fromJson(series_text(QStringLiteral("fe_expr_data")).toUtf8(), &perr);
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
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);

    // ── The covariance-based optimisers: HRP, Min Variance, Max Sharpe, Efficient Frontier ──
    // One form for the four. Real data by default (D1): the symbols' annualised covariance of daily
    // returns (and their annualised mean daily return as the expected return) over the period; the
    // boxes are the Custom series. Every capture is by value: the build and the action run later.
    auto add_cov_tab = [this, tabs](const QString& method_id, const QString& title, const QString& btn_label,
                                    bool needs_returns, const QString& loading, ui::LoadingSkeleton::Shape shape) {
        add_view_tab(tabs, {.key = method_id, .cost = ViewCost::Fetch, .loading = loading, .shape = shape}, title,
                     [this, method_id, btn_label, needs_returns](QWidget* t, QVBoxLayout* tvl) {
        add_series_source(t, tvl, SeriesShape::List);
        auto* assets_in = new QLineEdit(t);
        assets_in->setPlaceholderText(tr("Asset names (comma-separated, e.g. AAPL,GOOG,MSFT)"));
        assets_in->setStyleSheet(input_ss());
        text_inputs_[method_id + "_assets"] = assets_in;
        mark_series_input(assets_in, SeriesKind::Assets);
        tvl->addWidget(build_input_row(tr("Assets"), assets_in, t));

        auto* cov_in = new QLineEdit(t);
        cov_in->setPlaceholderText(tr("Annualised covariance matrix (decimals), JSON: [[0.04,0.01],[0.01,0.09]]"));
        cov_in->setStyleSheet(input_ss());
        text_inputs_[method_id + "_cov"] = cov_in;
        mark_series_input(cov_in, SeriesKind::CovarianceJson);
        tvl->addWidget(build_input_row(tr("Covariance Matrix"), cov_in, t));

        QLineEdit* ret_edit = nullptr;
        if (needs_returns) {
            ret_edit = new QLineEdit(t);
            ret_edit->setPlaceholderText(tr("Annual expected returns as decimals (e.g. 0.10,0.15,0.12)"));
            ret_edit->setStyleSheet(input_ss());
            text_inputs_[method_id + "_returns"] = ret_edit;
            mark_series_input(ret_edit, SeriesKind::ExpReturns);
            tvl->addWidget(build_input_row(tr("Expected Returns"), ret_edit, t));
            tvl->addWidget(misc_hint(tr("Ticker source: the expected return of each symbol is its annualised mean "
                                        "daily return over the period — a backward-looking estimate."),
                                     t));
        }

        // One synthetic 3-asset case (names, annualised covariance, expected
        // returns) so the optimiser can be tried without hand-typing a matrix.
        // make_sample_button switches the source to Custom series and fills the matrix; this fills
        // the names and the returns.
        {
            auto* sb = make_sample_button(
                cov_in, t, tr("3 synthetic assets: annualised vols 20% / 15% / 8%, mild correlations."),
                []() { return QStringLiteral("[[0.04,0.006,-0.002],[0.006,0.0225,0.001],[-0.002,0.001,0.0064]]"); });
            QPointer<QLineEdit> assets_guard(assets_in);
            QPointer<QLineEdit> ret_guard(ret_edit);
            connect(sb, &QPushButton::clicked, this, [assets_guard, ret_guard]() {
                const auto fill = [](QLineEdit* e, const QString& text) {
                    e->setText(text);
                    e->setProperty("quantSampleText", text);
                };
                if (assets_guard)
                    fill(assets_guard, QStringLiteral("ASSET_A,ASSET_B,ASSET_C"));
                if (ret_guard)
                    fill(ret_guard, QStringLiteral("0.08,0.10,0.04"));
            });
            tvl->addWidget(sb);
        }

        // Only the return-based methods use a risk-free rate (Max Sharpe objective, the
        // frontier's Sharpe column); HRP and Min Variance work on the covariance alone.
        if (needs_returns) {
            auto* rf_spin = make_double_spin(0, 20, 2.0, 2, "%", t);
            double_inputs_[method_id + "_rf"] = rf_spin;
            tvl->addWidget(build_input_row(tr("Risk-Free Rate"), rf_spin, t));
        }

        auto* run = make_run_button(btn_label, t);
        set_action(run, [this, method_id, needs_returns]() {
            // Previously: a blank Assets field produced [""] and a malformed
            // covariance matrix was silently dropped, so the optimiser ran on
            // nonsense and returned an unrelated Python error.
            const QJsonArray assets_arr = parse_csv_strings(series_text(method_id + "_assets"));
            if (assets_arr.isEmpty()) {
                display_error(tr("Enter at least two asset names, e.g. AAPL,GOOG,MSFT."));
                return;
            }
            QJsonParseError perr{};
            const auto cov_doc = QJsonDocument::fromJson(series_text(method_id + "_cov").toUtf8(), &perr);
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
            if (const QString err = check_numeric_matrix(cov, static_cast<int>(assets_arr.size())); !err.isEmpty()) {
                display_error(tr("Covariance Matrix must be square (N×N for N assets): %1").arg(err));
                return;
            }
            QJsonObject params;
            params["assets"] = assets_arr;
            params["cov_matrix"] = cov;
            if (needs_returns) {
                QJsonArray ret_arr;
                QString bad;
                if (!parse_doubles(series_text(method_id + "_returns"), ret_arr, &bad)) {
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
                params["risk_free_rate"] = double_inputs_[method_id + "_rf"]->value() / 100.0;
            }
            show_loading(tr("Optimising %1 assets...").arg(assets_arr.size()));
            AIQuantLabService::instance().portopt_run(method_id, params);
        });
        tvl->addWidget(run);
        tvl->addStretch();
        });
    };

    // View keys are the script's method ids (stable; also the text_inputs_ prefixes).
    add_cov_tab(QStringLiteral("hierarchical_risk_parity"), tr("HRP"), tr("RUN HRP"), false,
                tr("Clustering the assets and allocating risk…"), ui::LoadingSkeleton::Shape::Result);
    add_cov_tab(QStringLiteral("minimum_variance"), tr("Min Variance"), tr("MIN VARIANCE"), false,
                tr("Solving the minimum-variance portfolio…"), ui::LoadingSkeleton::Shape::Result);
    add_cov_tab(QStringLiteral("maximum_sharpe"), tr("Max Sharpe"), tr("MAX SHARPE"), true,
                tr("Solving the maximum-Sharpe portfolio…"), ui::LoadingSkeleton::Shape::Result);
    add_cov_tab(QStringLiteral("efficient_frontier"), tr("Eff. Frontier"), tr("EFFICIENT FRONTIER"), true,
                tr("Tracing the efficient frontier…"), ui::LoadingSkeleton::Shape::Chart);

    // ── Black-Litterman ──
    add_view_tab(tabs, {.key = QStringLiteral("black_litterman"), .cost = ViewCost::Fetch, .loading = tr("Blending the views into the prior…"), .shape = ui::LoadingSkeleton::Shape::Result}, tr("Black-Litterman"),
                 [this](QWidget* bl, QVBoxLayout* blvl) {
        // Real data by default (D1): the symbols, the annualised covariance of their daily returns and
        // EQUAL market caps (an equal-weight equilibrium prior); the views stay ordinary inputs.
        add_series_source(bl, blvl, SeriesShape::List);
        blvl->addWidget(misc_hint(tr("Ticker source: equal market caps (an equal-weight prior) and the covariance of "
                                     "the symbols' daily returns. View i is an absolute annual return for symbol i, "
                                     "in the order listed. Custom series: enter real caps."),
                                  bl));
        auto* bl_assets = new QLineEdit(bl);
        bl_assets->setPlaceholderText(tr("Asset names (comma-separated)"));
        bl_assets->setStyleSheet(input_ss());
        text_inputs_["bl_assets"] = bl_assets;
        mark_series_input(bl_assets, SeriesKind::Assets);
        blvl->addWidget(build_input_row(tr("Assets"), bl_assets, bl));
        auto* bl_caps = new QLineEdit(bl);
        bl_caps->setPlaceholderText(tr("Market caps (comma-separated, e.g. 2000,1500,800)"));
        bl_caps->setStyleSheet(input_ss());
        text_inputs_["bl_caps"] = bl_caps;
        bl_caps->setToolTip(tr("Only relative size matters — they set the equilibrium (market) weights."));
        mark_series_input(bl_caps, SeriesKind::EqualWeights);
        blvl->addWidget(build_input_row(tr("Market Caps ($B)"), bl_caps, bl));
        auto* bl_cov = new QLineEdit(bl);
        bl_cov->setPlaceholderText(tr("Covariance matrix JSON: [[0.04,0.01],[0.01,0.09]]"));
        bl_cov->setStyleSheet(input_ss());
        text_inputs_["bl_cov"] = bl_cov;
        mark_series_input(bl_cov, SeriesKind::CovarianceJson);
        blvl->addWidget(build_input_row(tr("Covariance Matrix"), bl_cov, bl));
        auto* bl_views = new QLineEdit(bl);
        bl_views->setPlaceholderText(tr("Annual expected return per asset as decimals, in the order listed (e.g. 0.05,0.10)"));
        // Two views on the first two symbols (SPY 8 %, QQQ 10 % with the default list).
        bl_views->setText(QStringLiteral("0.08,0.10"));
        bl_views->setStyleSheet(input_ss());
        text_inputs_["bl_views"] = bl_views;
        blvl->addWidget(build_input_row(tr("Views"), bl_views, bl));
        auto* bl_conf = new QLineEdit(bl);
        bl_conf->setPlaceholderText(tr("Confidence per view, 0–1 (e.g. 0.8,0.6)"));
        bl_conf->setText(QStringLiteral("0.8,0.6"));
        bl_conf->setStyleSheet(input_ss());
        text_inputs_["bl_conf"] = bl_conf;
        blvl->addWidget(build_input_row(tr("View Confidences"), bl_conf, bl));
        auto* bl_run = make_run_button(tr("RUN BLACK-LITTERMAN"), bl);
        set_action(bl_run, [this]() {
            const QJsonArray assets = parse_csv_strings(series_text(QStringLiteral("bl_assets")));
            if (assets.isEmpty()) {
                display_error(tr("Enter at least two asset names."));
                return;
            }
            QJsonArray caps, views, confs;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("bl_caps")), caps, &bad)) {
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
            // View i is an absolute view on asset i (in the order listed above).
            if (views.size() > assets.size()) {
                display_error(tr("You gave %1 view(s) but only %2 asset(s); views map to assets in order.")
                                  .arg(views.size())
                                  .arg(assets.size()));
                return;
            }
            QJsonParseError perr{};
            const auto cov_doc = QJsonDocument::fromJson(series_text(QStringLiteral("bl_cov")).toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !cov_doc.isArray()) {
                display_error(tr("Covariance Matrix must be a JSON 2-D array — %1").arg(perr.errorString()));
                return;
            }
            if (cov_doc.array().size() != assets.size()) {
                display_error(tr("Covariance matrix has %1 row(s) but you listed %2 assets - they must match.")
                                  .arg(cov_doc.array().size())
                                  .arg(assets.size()));
                return;
            }
            if (const QString err = check_numeric_matrix(cov_doc.array(), static_cast<int>(assets.size()));
                !err.isEmpty()) {
                display_error(tr("Covariance Matrix must be square (N×N for N assets): %1").arg(err));
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
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);

    // ── IC Metrics tab ──
    add_view_tab(tabs, {.key = QStringLiteral("ic_metrics"), .cost = ViewCost::Fetch, .loading = tr("Computing the information coefficient…"), .shape = ui::LoadingSkeleton::Shape::Result}, tr("IC Metrics"),
                 [this](QWidget* ic, QVBoxLayout* icvl) {
        // Real data by default (D1): the symbol's 20-day momentum as the prediction, scored against
        // the next day's return; the boxes are the Custom series.
        add_series_source(ic, icvl, SeriesShape::Single);
        icvl->addWidget(misc_hint(tr("Ticker source: the prediction is the 20-day momentum (close / close 20 days "
                                     "earlier − 1), scored against the next day's return."),
                                  ic));
        auto* ic_preds = new QLineEdit(ic);
        ic_preds->setPlaceholderText(tr("Predictions (comma-separated, e.g. 0.1,0.2,-0.1,0.3)"));
        ic_preds->setStyleSheet(input_ss());
        text_inputs_["ev_predictions"] = ic_preds;
        mark_series_input(ic_preds, SeriesKind::Signal, 0);
        icvl->addWidget(build_input_row(tr("Predictions"), ic_preds, ic));
        auto* ic_rets = new QLineEdit(ic);
        ic_rets->setPlaceholderText(tr("Actual returns (comma-separated)"));
        ic_rets->setStyleSheet(input_ss());
        text_inputs_["ev_returns"] = ic_rets;
        mark_series_input(ic_rets, SeriesKind::FwdReturns, 0);
        icvl->addWidget(build_input_row(tr("Returns"), ic_rets, ic));
        icvl->addWidget(make_sample_button(ic_preds, ic, tr("252 synthetic predictions"),
                                           []() { return sample_returns(311); }));
        icvl->addWidget(make_sample_button(ic_rets, ic, tr("252 synthetic realised returns (independent of the "
                                                           "predictions, so IC ≈ 0)"),
                                           []() { return sample_returns(312); }));
        auto* ic_method = new QComboBox(ic);
        ic_method->addItems({"pearson", "spearman"});
        ic_method->setStyleSheet(combo_ss());
        combo_inputs_["ev_ic_method"] = ic_method;
        icvl->addWidget(build_input_row(tr("Method"), ic_method, ic));
        auto* ic_run = make_run_button(tr("CALCULATE IC METRICS"), ic);
        set_action(ic_run, [this]() {
            QJsonArray preds, rets;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("ev_predictions")), preds, &bad)) {
                display_error(tr("Predictions: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text(QStringLiteral("ev_returns")), rets, &bad)) {
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
    });

    // ── Full Report tab ──
    add_view_tab(tabs, {.key = QStringLiteral("full_report"), .cost = ViewCost::Fetch, .loading = tr("Scoring the factor…"), .shape = ui::LoadingSkeleton::Shape::Result}, tr("Full Report"),
                 [this](QWidget* rep, QVBoxLayout* repvl) {
        // Real data by default (D1): as IC Metrics — 20-day momentum against the next day's return.
        add_series_source(rep, repvl, SeriesShape::Single);
        repvl->addWidget(misc_hint(tr("Ticker source: the factor is the 20-day momentum (close / close 20 days "
                                      "earlier − 1), scored against the next day's return."),
                                   rep));
        auto* rep_name = new QLineEdit(rep);
        rep_name->setPlaceholderText(tr("Factor name (e.g. momentum)"));
        rep_name->setText(QStringLiteral("momentum_20d"));
        rep_name->setStyleSheet(input_ss());
        text_inputs_["ev_factor_name"] = rep_name;
        repvl->addWidget(build_input_row(tr("Factor Name"), rep_name, rep));
        auto* rep_preds = new QLineEdit(rep);
        rep_preds->setPlaceholderText(tr("Predictions (comma-separated)"));
        rep_preds->setStyleSheet(input_ss());
        text_inputs_["ev_rep_preds"] = rep_preds;
        mark_series_input(rep_preds, SeriesKind::Signal, 0);
        repvl->addWidget(build_input_row(tr("Predictions"), rep_preds, rep));
        auto* rep_rets = new QLineEdit(rep);
        rep_rets->setPlaceholderText(tr("Returns (comma-separated)"));
        rep_rets->setStyleSheet(input_ss());
        text_inputs_["ev_rep_returns"] = rep_rets;
        mark_series_input(rep_rets, SeriesKind::FwdReturns, 0);
        repvl->addWidget(build_input_row(tr("Returns"), rep_rets, rep));
        repvl->addWidget(make_sample_button(rep_preds, rep, tr("252 synthetic predictions"),
                                            []() { return sample_returns(321); }));
        repvl->addWidget(make_sample_button(rep_rets, rep, tr("252 synthetic realised returns"),
                                            []() { return sample_returns(322); }));
        auto* rep_run = make_run_button(tr("GENERATE EVALUATION REPORT"), rep);
        set_action(rep_run, [this]() {
            QJsonArray preds, rets;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("ev_rep_preds")), preds, &bad)) {
                display_error(tr("Predictions: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text(QStringLiteral("ev_rep_returns")), rets, &bad)) {
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
    });

    // ── Risk Metrics tab ──
    add_view_tab(tabs, {.key = QStringLiteral("risk_metrics"), .cost = ViewCost::Fetch, .loading = tr("Computing risk metrics…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Risk Metrics"),
                 [this](QWidget* risk, QVBoxLayout* riskvl) {
        // Real data by default (D1): the symbol's daily returns against the benchmark's, on their
        // common days; the boxes are the Custom series (the benchmark is optional there).
        add_series_source(risk, riskvl, SeriesShape::Pair, tr("Benchmark"));
        auto* risk_rets = new QLineEdit(risk);
        risk_rets->setPlaceholderText(tr("Daily returns (comma-separated)"));
        risk_rets->setStyleSheet(input_ss());
        text_inputs_["ev_risk_returns"] = risk_rets;
        mark_series_input(risk_rets, SeriesKind::Returns, 0);
        riskvl->addWidget(build_input_row(tr("Returns"), risk_rets, risk));
        riskvl->addWidget(make_sample_button(risk_rets, risk, tr("252 synthetic daily returns (mu=0.05%, sigma=1.2%)"),
                                             []() { return sample_returns(331); }));
        auto* risk_bench = new QLineEdit(risk);
        risk_bench->setPlaceholderText(tr("Benchmark returns (optional, comma-separated)"));
        risk_bench->setStyleSheet(input_ss());
        text_inputs_["ev_risk_bench"] = risk_bench;
        mark_series_input(risk_bench, SeriesKind::Returns, 1);
        riskvl->addWidget(build_input_row(tr("Benchmark (opt.)"), risk_bench, risk));
        auto* risk_conf = make_double_spin(0.8, 0.999, 0.95, 3, "", risk);
        double_inputs_["ev_risk_conf"] = risk_conf;
        riskvl->addWidget(build_input_row(tr("Confidence Level"), risk_conf, risk));
        auto* risk_run = make_run_button(tr("CALCULATE RISK METRICS"), risk);
        set_action(risk_run, [this]() {
            QJsonArray rets;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("ev_risk_returns")), rets, &bad)) {
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
            const auto bench_text = series_text(QStringLiteral("ev_risk_bench")).trimmed();
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
    });

    // ── Combine Factors tab ── (qlib_evaluation.py combine_factors)
    // z-scores each factor, then weights them equally, by the caller's weights, by
    // their IC with the forward returns, or for the maximum composite IC.
    add_view_tab(tabs, {.key = QStringLiteral("combine_factors"), .cost = ViewCost::Fetch, .loading = tr("Combining the factors…"), .shape = ui::LoadingSkeleton::Shape::Result}, tr("Combine Factors"),
                 [this](QWidget* comb, QVBoxLayout* combvl) {
        // Real data by default (D1): five single-name factors of the symbol and the next day's return.
        add_series_source(comb, combvl, SeriesShape::Single);
        combvl->addWidget(misc_hint(tr("Ticker source: momentum (20d), reversal (5d), volatility (20d), volume ratio "
                                       "(20d) and the day's range, with the next day's return."),
                                    comb));
        auto* comb_factors = new QLineEdit(comb);
        comb_factors->setPlaceholderText(tr("Factors JSON, equal lengths: {\"momentum\":[0.1,0.3,...],\"value\":[10,12,...]}"));
        comb_factors->setStyleSheet(input_ss());
        text_inputs_["ev_comb_factors"] = comb_factors;
        mark_series_input(comb_factors, SeriesKind::FactorsJson);
        combvl->addWidget(build_input_row(tr("Factors (JSON)"), comb_factors, comb));
        auto* comb_rets = new QLineEdit(comb);
        comb_rets->setPlaceholderText(tr("Forward returns, one per observation (required for ic_weight / max_ic)"));
        comb_rets->setStyleSheet(input_ss());
        text_inputs_["ev_comb_returns"] = comb_rets;
        mark_series_input(comb_rets, SeriesKind::FwdReturns);
        combvl->addWidget(build_input_row(tr("Returns (opt.)"), comb_rets, comb));
        auto* comb_method = new QComboBox(comb);
        comb_method->addItems({"equal_weight", "ic_weight", "max_ic", "custom"});
        comb_method->setStyleSheet(combo_ss());
        comb_method->setToolTip(tr("equal_weight: 1/n each · ic_weight: weights ∝ each factor's IC · max_ic: weights that "
                                   "maximise the composite IC (C⁻¹·IC) · custom: your weights. ic_weight / max_ic are "
                                   "estimated in-sample on the returns given."));
        combo_inputs_["ev_comb_method"] = comb_method;
        combvl->addWidget(build_input_row(tr("Method"), comb_method, comb));
        auto* comb_weights = new QLineEdit(comb);
        comb_weights->setPlaceholderText(tr("Custom weights JSON, one per factor: {\"momentum\":0.6,\"value\":0.4}"));
        comb_weights->setStyleSheet(input_ss());
        text_inputs_["ev_comb_weights"] = comb_weights;
        combvl->addWidget(build_input_row(tr("Weights (custom)"), comb_weights, comb));
        auto sync_comb = [comb_weights](const QString& method) {
            comb_weights->setEnabled(method == QLatin1String("custom"));
        };
        connect(comb_method, &QComboBox::currentTextChanged, comb_weights, sync_comb);
        sync_comb(comb_method->currentText());
        auto* comb_run = make_run_button(tr("COMBINE FACTORS"), comb);
        set_action(comb_run, [this]() {
            QJsonParseError perr{};
            const auto doc = QJsonDocument::fromJson(series_text(QStringLiteral("ev_comb_factors")).toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                display_error(tr("Factors must be a JSON object like {\"momentum\":[…],\"value\":[…]} — %1")
                                  .arg(perr.errorString()));
                return;
            }
            const QJsonObject factors = doc.object();
            if (factors.size() < 2) {
                display_error(tr("Combining needs at least two factors."));
                return;
            }
            qsizetype n_obs = -1;
            for (auto it = factors.begin(); it != factors.end(); ++it) {
                if (!it.value().isArray()) {
                    display_error(tr("Factor '%1' must be an array of numbers.").arg(it.key()));
                    return;
                }
                const QJsonArray values = it.value().toArray();
                for (const auto& v : values) {
                    if (!v.isDouble()) {
                        display_error(tr("Factor '%1' contains a non-numeric value.").arg(it.key()));
                        return;
                    }
                }
                if (n_obs >= 0 && values.size() != n_obs) {
                    display_error(tr("Every factor needs the same number of values ('%1' has %2, expected %3).")
                                      .arg(it.key())
                                      .arg(values.size())
                                      .arg(n_obs));
                    return;
                }
                n_obs = values.size();
            }
            if (n_obs < 3) {
                display_error(tr("Each factor needs at least 3 observations."));
                return;
            }
            const QString method = combo_inputs_["ev_comb_method"]->currentText();
            QJsonObject params;
            params["factors"] = factors;
            params["method"] = method;
            const QString rets_text = series_text(QStringLiteral("ev_comb_returns")).trimmed();
            if (!rets_text.isEmpty()) {
                QJsonArray rets;
                QString bad;
                if (!parse_doubles(rets_text, rets, &bad)) {
                    display_error(tr("Returns: '%1' is not numeric.").arg(bad));
                    return;
                }
                if (rets.size() != n_obs) {
                    display_error(tr("Returns (%1) must have one value per observation (%2).").arg(rets.size()).arg(n_obs));
                    return;
                }
                params["returns"] = rets;
            } else if (method == QLatin1String("ic_weight") || method == QLatin1String("max_ic")) {
                display_error(tr("%1 estimates the weights from the forward returns — enter the returns.").arg(method));
                return;
            }
            if (method == QLatin1String("custom")) {
                QJsonParseError werr{};
                const auto wdoc = QJsonDocument::fromJson(text_inputs_["ev_comb_weights"]->text().toUtf8(), &werr);
                if (werr.error != QJsonParseError::NoError || !wdoc.isObject()) {
                    display_error(tr("Custom weights must be a JSON object like {\"momentum\":0.6,\"value\":0.4} — %1")
                                      .arg(werr.errorString()));
                    return;
                }
                params["weights"] = wdoc.object();
            }
            show_loading(tr("Combining %1 factors (%2) over %3 observations...").arg(factors.size()).arg(method).arg(n_obs));
            AIQuantLabService::instance().run_module(module_.id, QStringLiteral("combine_factors"), params);
        });
        combvl->addWidget(comb_run);
        combvl->addStretch();
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);

    // ── TopK-Dropout ──
    add_view_tab(tabs, {.key = QStringLiteral("topk_dropout"), .cost = ViewCost::Fetch, .loading = tr("Ranking the names…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("TopK-Dropout"),
                 [this](QWidget* topk, QVBoxLayout* tkvl) {
        // Real data by default (D1): one cross-section — each symbol's 20-day momentum — ranked into
        // the book; the boxes are the Custom series.
        add_series_source(topk, tkvl, SeriesShape::List);
        tkvl->addWidget(misc_hint(tr("Ticker source: the signal is each symbol's 20-day momentum; the book holds "
                                     "the Top-K by score, equally weighted, and N-Drop names rotate per rebalance."),
                                  topk));
        auto* tk_signal = new QLineEdit(topk);
        tk_signal->setPlaceholderText(tr("Signal values (comma-separated)"));
        tk_signal->setStyleSheet(input_ss());
        text_inputs_["st_tk_signal"] = tk_signal;
        mark_series_input(tk_signal, SeriesKind::CrossMomentum);
        tkvl->addWidget(build_input_row(tr("Signal"), tk_signal, topk));
        // The names of the scores, in the same order (shown in the book instead of "Asset 1…").
        auto* tk_assets = new QLineEdit(topk);
        tk_assets->setPlaceholderText(tr("Optional names, one per signal value (e.g. AAPL,MSFT,GOOG)"));
        tk_assets->setStyleSheet(input_ss());
        text_inputs_["st_tk_assets"] = tk_assets;
        mark_series_input(tk_assets, SeriesKind::Assets);
        tkvl->addWidget(build_input_row(tr("Names (opt.)"), tk_assets, topk));
        // Sized for the default five-symbol universe: hold 3, rotate 1 (Top-K above the number of
        // names would hold everything and leave nothing to rotate into).
        auto* tk_topk = new QSpinBox(topk);
        tk_topk->setRange(1, 500);
        tk_topk->setValue(3);
        tk_topk->setStyleSheet(spinbox_ss());
        int_inputs_["st_topk"] = tk_topk;
        tkvl->addWidget(build_input_row(tr("Top-K"), tk_topk, topk));
        auto* tk_drop = new QSpinBox(topk);
        tk_drop->setRange(1, 100);
        tk_drop->setValue(1);
        tk_drop->setStyleSheet(tk_topk->styleSheet());
        int_inputs_["st_ndrop"] = tk_drop;
        tkvl->addWidget(build_input_row(tr("N-Drop"), tk_drop, topk));
        auto* tk_run = make_run_button(tr("CREATE TOPK-DROPOUT STRATEGY"), topk);
        set_action(tk_run, [this]() {
            QJsonArray signal;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("st_tk_signal")), signal, &bad)) {
                display_error(tr("Signal: '%1' is not numeric.").arg(bad));
                return;
            }
            const QJsonArray names = parse_csv_strings(series_text(QStringLiteral("st_tk_assets")));
            if (!names.isEmpty() && names.size() != signal.size()) {
                display_error(tr("Names has %1 entries but the signal has %2 values — one name per value, or none.")
                                  .arg(names.size())
                                  .arg(signal.size()));
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
            if (!names.isEmpty())
                params["assets"] = names;
            AIQuantLabService::instance().strategy_create("create_topk_dropout", params);
        });
        tkvl->addWidget(tk_run);
        tkvl->addStretch();
    });

    // ── Risk Parity ──
    add_view_tab(tabs, {.key = QStringLiteral("risk_parity"), .cost = ViewCost::Fetch, .loading = tr("Solving risk parity…"), .shape = ui::LoadingSkeleton::Shape::Result}, tr("Risk Parity"),
                 [this](QWidget* rp, QVBoxLayout* rpvl) {
        // Real data by default (D1): the symbols' daily returns, one row per common day; the boxes
        // are the Custom series.
        add_series_source(rp, rpvl, SeriesShape::List);
        auto* rp_returns = new QLineEdit(rp);
        rp_returns->setPlaceholderText(
            tr("DAILY returns as decimals, one row per day, one column per asset: [[0.01,-0.02],[0.00,0.01]]"));
        rp_returns->setStyleSheet(input_ss());
        text_inputs_["st_rp_returns"] = rp_returns;
        mark_series_input(rp_returns, SeriesKind::ReturnsMatrixJson);
        rpvl->addWidget(build_input_row(tr("Returns Matrix"), rp_returns, rp));
        auto* rp_assets = new QLineEdit(rp);
        rp_assets->setPlaceholderText(tr("Optional names, one per column (e.g. SPY,TLT)"));
        rp_assets->setStyleSheet(input_ss());
        text_inputs_["st_rp_assets"] = rp_assets;
        mark_series_input(rp_assets, SeriesKind::Assets);
        rpvl->addWidget(build_input_row(tr("Names (opt.)"), rp_assets, rp));
        // Annualised target volatility. Entered in percent (10 = 10 %/yr); the
        // script takes a fraction. The bare 0.10 spin gave no hint of either unit.
        auto* rp_target = make_double_spin(1.0, 100.0, 10.0, 1, "%", rp);
        rp_target->setToolTip(tr("Annualised portfolio volatility the risk-parity weights are scaled to."));
        double_inputs_["st_rp_target"] = rp_target;
        rpvl->addWidget(build_input_row(tr("Target Vol (ann.)"), rp_target, rp));
        auto* rp_freq = new QComboBox(rp);
        rp_freq->addItems({"monthly", "weekly", "daily", "quarterly"});
        rp_freq->setStyleSheet(combo_ss());
        combo_inputs_["st_rp_freq"] = rp_freq;
        rpvl->addWidget(build_input_row(tr("Rebalance"), rp_freq, rp));
        auto* rp_run = make_run_button(tr("CREATE RISK PARITY STRATEGY"), rp);
        set_action(rp_run, [this]() {
            QJsonParseError perr{};
            const auto ret_doc = QJsonDocument::fromJson(series_text(QStringLiteral("st_rp_returns")).toUtf8(), &perr);
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
            if (const QString err = check_numeric_matrix(rows); !err.isEmpty()) {
                display_error(tr("Returns Matrix: %1 Every row needs one return per asset.").arg(err));
                return;
            }
            // A covariance needs at least two assets and more periods than one. The
            // script annualises with sqrt(252), so rows must be DAILY returns.
            if (rows.first().toArray().size() < 2 || rows.size() < 3) {
                display_error(tr("Risk parity needs at least 2 assets (columns) and 3 periods (rows); got %1 × %2.")
                                  .arg(rows.size())
                                  .arg(rows.first().toArray().size()));
                return;
            }
            const QJsonArray names = parse_csv_strings(series_text(QStringLiteral("st_rp_assets")));
            if (!names.isEmpty() && names.size() != rows.first().toArray().size()) {
                display_error(tr("Names has %1 entries but the matrix has %2 columns — one name per column, or none.")
                                  .arg(names.size())
                                  .arg(rows.first().toArray().size()));
                return;
            }
            show_loading(tr("Solving risk parity over %1 period(s)...").arg(rows.size()));
            QJsonObject params;
            params["returns"] = rows;
            if (!names.isEmpty())
                params["assets"] = names;
            params["target_risk"] = double_inputs_["st_rp_target"]->value() / 100.0;
            params["rebalance_frequency"] = combo_inputs_["st_rp_freq"]->currentText();
            AIQuantLabService::instance().strategy_create("create_risk_parity", params);
        });
        rpvl->addWidget(rp_run);
        rpvl->addStretch();
    });

    // ── Portfolio Metrics ──
    add_view_tab(tabs, {.key = QStringLiteral("portfolio_metrics"), .cost = ViewCost::Fetch, .loading = tr("Computing portfolio metrics…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Portfolio Metrics"),
                 [this](QWidget* pm, QVBoxLayout* pmvl) {
        // Real data by default (D1): the symbol's daily returns against the benchmark's, on their
        // common days; the boxes are the Custom series (the benchmark is optional there).
        add_series_source(pm, pmvl, SeriesShape::Pair, tr("Benchmark"));
        auto* pm_rets = new QLineEdit(pm);
        pm_rets->setPlaceholderText(tr("Portfolio returns (comma-separated)"));
        pm_rets->setStyleSheet(input_ss());
        text_inputs_["st_pm_returns"] = pm_rets;
        mark_series_input(pm_rets, SeriesKind::Returns, 0);
        pmvl->addWidget(build_input_row(tr("Returns"), pm_rets, pm));
        pmvl->addWidget(make_sample_button(pm_rets, pm, tr("252 synthetic daily portfolio returns"),
                                           []() { return sample_returns(341); }));
        auto* pm_bench = new QLineEdit(pm);
        pm_bench->setPlaceholderText(tr("Benchmark returns (optional)"));
        pm_bench->setStyleSheet(input_ss());
        text_inputs_["st_pm_bench"] = pm_bench;
        mark_series_input(pm_bench, SeriesKind::Returns, 1);
        pmvl->addWidget(build_input_row(tr("Benchmark (opt.)"), pm_bench, pm));
        auto* pm_rf = make_double_spin(0, 20, 2.0, 2, "%", pm);
        double_inputs_["st_pm_rf"] = pm_rf;
        pmvl->addWidget(build_input_row(tr("Risk-Free Rate"), pm_rf, pm));
        auto* pm_run = make_run_button(tr("CALCULATE PORTFOLIO METRICS"), pm);
        set_action(pm_run, [this]() {
            QJsonArray rets;
            QString bad;
            if (!parse_doubles(series_text(QStringLiteral("st_pm_returns")), rets, &bad)) {
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
            const auto bench_text = series_text(QStringLiteral("st_pm_bench")).trimmed();
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
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);

    // ── List Processors ──
    add_view_tab(tabs, {.key = QStringLiteral("browse"), .cost = ViewCost::Fetch, .loading = tr("Loading the processor catalogue…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Browse"),
                 [this](QWidget* list_tab, QVBoxLayout* ltvl) {
        ltvl->addWidget(
            misc_info(tr("Browse all available data normalizers and transformation processors."), list_tab));
        auto* lt_run = make_run_button(tr("LIST ALL PROCESSORS"), list_tab);
        set_action(lt_run, [this]() {
            show_loading(tr("Loading processor catalog..."));
            AIQuantLabService::instance().dataproc_list_processors();
        });
        ltvl->addWidget(lt_run);
        ltvl->addStretch();
    });

    // ── Create Pipeline ──
    // CREATE writes <data root>/script-data/data_processors/pipelines.json, so it never runs by itself. Under the
    // form, the view shows the processor catalogue (its default: the cached list, free) — the types
    // a pipeline can use. A created pipeline's id is filled into Process Data, which then runs.
    add_view_tab(tabs,
                 {.key = QStringLiteral("create_pipeline"),
                  .cost = ViewCost::Fetch,
                  .loading = tr("Loading the processor catalogue…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .after =
                      [this](bool ok, const QString& command, const QJsonObject& data) {
                          if (!ok || command != QLatin1String("create_pipeline"))
                              return;
                          const QString id = data.value(QStringLiteral("pipeline_id")).toString();
                          if (id.isEmpty())
                              return;
                          SettingsRepository::instance().set(QString::fromLatin1(kLastPipelineKey), id,
                                                             QStringLiteral("ai_quant_lab"));
                          ensure_view_built(QStringLiteral("process_data"));
                          // The saved pipelines are read again; the new one becomes Process Data's id.
                          refresh_saved_ids(SavedIds::Pipelines, id);
                      }},
                 tr("Create Pipeline"), [this](QWidget* pipe_tab, QVBoxLayout* ptvl) {
        auto* pipe_id = new QLineEdit(pipe_tab);
        pipe_id->setPlaceholderText(tr("Pipeline ID (e.g. my_pipeline)"));
        pipe_id->setText(QStringLiteral("my_pipeline"));
        pipe_id->setStyleSheet(input_ss());
        pipe_id->setProperty("quantSignatureIgnore", true); // CREATE's input, not the list's
        text_inputs_["dp_pipeline_id"] = pipe_id;
        ptvl->addWidget(build_input_row(tr("Pipeline ID"), pipe_id, pipe_tab));
        auto* pipe_procs = new QLineEdit(pipe_tab);
        pipe_procs->setPlaceholderText(tr(R"([{"type":"zscore"},{"type":"winsorize","lower":0.01,"upper":0.99}])"));
        // Clip the outliers at the 5th / 95th percentile, then standardise each column.
        pipe_procs->setText(QStringLiteral(
            "[{\"type\":\"winsorize\",\"lower\":0.05,\"upper\":0.95},{\"type\":\"zscore\"}]"));
        pipe_procs->setStyleSheet(input_ss());
        pipe_procs->setProperty("quantSignatureIgnore", true);
        text_inputs_["dp_processors"] = pipe_procs;
        ptvl->addWidget(build_input_row(tr("Processors (JSON)"), pipe_procs, pipe_tab));
        ptvl->addWidget(misc_hint(tr("Stages run in order. Parameters: winsorize lower / upper, robust_zscore clip, "
                                     "fillna fill_value or method (ffill / bfill), process_inf replace_value, tanh "
                                     "scale, dropna axis / thresh. The catalogue below lists every type."),
                                  pipe_tab));
        // The catalogue under the form: what the view shows before anything is created.
        set_default_action([this]() {
            show_loading(tr("Loading processor catalog..."));
            AIQuantLabService::instance().dataproc_list_processors();
        });
        auto* pipe_run = make_run_button(tr("CREATE PIPELINE"), pipe_tab);
        set_action(pipe_run, [this]() {
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
        }, /*is_default=*/false);
        ptvl->addWidget(pipe_run);
        ptvl->addStretch();
    });

    // ── Process Data ──
    // Needs a saved pipeline: a preview until one exists (no id, or the saved list lacks it), then it
    // runs by itself on the pipeline picked (the last one created by default) and the 2-column sample
    // below. The saved list is read now, so it is known by the time this tab opens.
    refresh_saved_ids(SavedIds::Pipelines);
    add_view_tab(tabs,
                 {.key = QStringLiteral("process_data"),
                  .cost = ViewCost::Heavy,
                  .loading = tr("Running the pipeline…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .auto_when =
                      [this]() {
                          const QString pid = saved_id(combo_inputs_.value(QStringLiteral("dp_proc_pid")));
                          return !pid.isEmpty() && saved_id_listed(SavedIds::Pipelines, pid);
                      },
                  .title = tr("Process data through a pipeline"),
                  .returns = tr("Fits the pipeline's stages on your data and transforms it: the processed table and "
                                "the before / after statistics. Create a pipeline first — its id is filled in here."),
                  .run_text = tr("PROCESS DATA"),
                  .cost_note = tr("Needs a pipeline from Create Pipeline")},
                 tr("Process Data"), [this](QWidget* proc_tab, QVBoxLayout* procvl) {
        // The saved pipelines (most recent first); until the list is read, the last pipeline created
        // (here or in an earlier session). Editable: an id can still be typed.
        QString last_pipeline;
        if (const auto last = SettingsRepository::instance().get(QString::fromLatin1(kLastPipelineKey)); last.is_ok())
            last_pipeline = last.value().trimmed();
        auto* proc_pid =
            make_saved_id_combo(SavedIds::Pipelines, QStringLiteral("dp_proc_pid"), last_pipeline, proc_tab);
        procvl->addWidget(build_input_row(tr("Pipeline ID"), proc_pid, proc_tab));
        auto* proc_data = new QLineEdit(proc_tab);
        proc_data->setPlaceholderText(tr(R"({"feature_close":[100,102,...],"feature_volume":[1000,1200,...]})"));
        // A small 2-column sample with one outlier day (row 4) and one reversal (row 10), so a
        // winsorize / z-score pipeline visibly changes it.
        proc_data->setText(QStringLiteral(
            "{\"momentum\":[0.012,-0.004,0.021,0.35,-0.008,0.015,0.003,-0.019,0.007,-0.28,0.011,0.004],"
            "\"volume_ratio\":[1.02,0.95,1.10,3.8,0.88,1.05,0.97,1.21,0.92,0.15,1.08,0.99]}"));
        proc_data->setStyleSheet(input_ss());
        text_inputs_["dp_proc_data"] = proc_data;
        procvl->addWidget(build_input_row(tr("Data (JSON)"), proc_data, proc_tab));
        auto* proc_run = make_run_button(tr("PROCESS DATA"), proc_tab);
        set_action(proc_run, [this]() {
            const QString pid = saved_id(combo_inputs_.value(QStringLiteral("dp_proc_pid")));
            if (pid.isEmpty()) {
                display_error(tr("Pick a pipeline you created in the Create Pipeline tab, or type its ID."));
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
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);

    // ── Factor Library ──
    // The library (Alpha158 / Alpha360 expressions) needs no dataset: it loads on open (cached).
    // LIST INSTRUMENTS reads the chosen dataset and stays a button.
    add_view_tab(tabs, {.key = QStringLiteral("factor_library"), .cost = ViewCost::Fetch, .loading = tr("Loading the factor library…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Factor Library"),
                 [this](QWidget* lib_tab, QVBoxLayout* libvl) {
        libvl->addWidget(misc_info(tr("Browse all built-in Qlib alpha factors and expressions."), lib_tab));
        auto* lib_run = make_run_button(tr("BROWSE FACTOR LIBRARY"), lib_tab);
        set_action(lib_run, [this]() {
            show_loading(tr("Loading Qlib factor library..."));
            AIQuantLabService::instance().factor_get_library();
        });
        libvl->addWidget(lib_run);
        auto* inst_run = make_run_button(tr("LIST INSTRUMENTS"), lib_tab);
        set_action(inst_run, [this]() {
            // Not the cached factor_get_instruments(): the list depends on the chosen dataset.
            QJsonObject params{{"market", QStringLiteral("all")}};
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(tr("Loading instrument universe..."));
            AIQuantLabService::instance().run_module(module_.id, QStringLiteral("get_instruments"), params);
        }, /*is_default=*/false);
        libvl->addWidget(inst_run);
        libvl->addStretch();
    });

    // ── Fetch Data ──
    // Reads the dataset chosen in the bar above: the run waits for the bar's probe; the defaults
    // (sample instruments, $close,$volume, the dataset's last year) come from the bar.
    add_view_tab(tabs,
                 {.key = QStringLiteral("fetch_data"),
                  .cost = ViewCost::Heavy, // measured ~28 s per read (qlib start-up + local read): > ~10 s
                  .loading = tr("Reading the dataset — about 30 s…"),
                  .shape = ui::LoadingSkeleton::Shape::Cards,
                  .qlib_dataset = true,
                  .title = tr("Prices and volumes from the local dataset"),
                  .returns = tr("The chosen fields of the instruments over the window, read from the local qlib "
                                "dataset, as a chart and a table."),
                  .run_text = tr("FETCH DATA"),
                  .cost_note = tr("Reads the local qlib dataset · about 30 s · no network")},
                 tr("Fetch Data"), [this](QWidget* data_tab, QVBoxLayout* datavl) {
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
        set_action(fd_run, [this]() {
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
            QJsonObject params;
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(tr("Fetching %1 field(s) for %2 instrument(s)...").arg(fields.size()).arg(instr.size()));
            params["instruments"] = instr;
            params["fields"] = fields;
            params["start_date"] = start.toString("yyyy-MM-dd");
            params["end_date"] = end.toString("yyyy-MM-dd");
            AIQuantLabService::instance().factor_get_data(params);
        });
        datavl->addWidget(fd_run);
        datavl->addStretch();
    });

    // ── Calendar ──
    // The dataset's trading days over its "recent" window (dates from the bar).
    add_view_tab(tabs,
                 {.key = QStringLiteral("calendar"),
                  .cost = ViewCost::Fetch,
                  .loading = tr("Reading the trading calendar…"),
                  .shape = ui::LoadingSkeleton::Shape::Table,
                  .qlib_dataset = true},
                 tr("Calendar"), [this](QWidget* cal_tab, QVBoxLayout* calvl) {
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
        set_action(cal_run, [this]() {
            const QDate start = date_inputs_["fd_cal_start"]->date();
            const QDate end = date_inputs_["fd_cal_end"]->date();
            if (start > end) {
                display_error(tr("Start date (%1) is after end date (%2).")
                                  .arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")));
                return;
            }
            QJsonObject params;
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(
                tr("Loading trading calendar %1 → %2...").arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")));
            params["start_date"] = start.toString("yyyy-MM-dd");
            params["end_date"] = end.toString("yyyy-MM-dd");
            AIQuantLabService::instance().factor_get_calendar(params);
        });
        calvl->addWidget(cal_run);
        calvl->addStretch();
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
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

    auto* tabs = make_view_tabs(w);

    // ── Model Browser ──
    // The catalogue (static list + which libraries are installed, cached) loads on open. CHECK QLIB
    // STATUS and DOWNLOAD QLIB DATASET stay buttons.
    add_view_tab(tabs, {.key = QStringLiteral("browse"), .cost = ViewCost::Fetch, .loading = tr("Loading the model catalogue…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Browse"),
                 [this](QWidget* browse_tab, QVBoxLayout* btvl) {
        btvl->addWidget(misc_info(tr("List all available Qlib models (LightGBM, XGBoost, LSTM, Transformer, etc.)."),
                                  browse_tab));
        auto* bt_list = make_run_button(tr("LIST ALL MODELS"), browse_tab);
        set_action(bt_list, [this]() {
            show_loading(tr("Loading Qlib model catalog..."));
            AIQuantLabService::instance().model_list();
        });
        btvl->addWidget(bt_list);
        auto* bt_status = make_run_button(tr("CHECK QLIB STATUS"), browse_tab);
        set_action(bt_status, [this]() {
            show_loading(tr("Checking Qlib runtime status..."));
            AIQuantLabService::instance().model_check_status();
        }, /*is_default=*/false);
        btvl->addWidget(bt_status);
        // Training, backtests, Factor Discovery data and Live Signals all read the local qlib
        // dataset, which nothing installed: every one of them failed on a fresh install.
        auto* bt_download = make_run_button(tr("DOWNLOAD QLIB DATASET (US, ~450 MB)"), browse_tab);
        // The real folder: qlib_datasets.public_download_dir() under the data root the terminal
        // hands every script (FINCEPT_DATA_DIR = AppPaths::root()).
        bt_download->setToolTip(tr("Downloads the public qlib US daily dataset (Yahoo Finance–sourced) into "
                                   "%1. Skipped when it is already there (or in qlib's own folder %2 from an "
                                   "earlier release). Takes several minutes; other Python tasks queue behind it.")
                                    .arg(QDir::toNativeSeparators(AppPaths::root() + QStringLiteral("/qlib_data/us_data")),
                                         QDir::toNativeSeparators(QDir::homePath() + QStringLiteral("/.qlib/qlib_data"))));
        // The download takes minutes: no time-out for this button's run (the view's is 4 min).
        set_action(bt_download, [this]() {
            show_loading(tr("Downloading the qlib US dataset (~450 MB) and unpacking it — this takes several minutes..."));
            AIQuantLabService::instance().run_module(module_.id, QStringLiteral("download_data"),
                                                     QJsonObject{{"region", QStringLiteral("us")}});
        }, /*is_default=*/false, /*timeout_s=*/0);
        btvl->addWidget(bt_download);
        btvl->addStretch();
    });

    // ── Train Model ──
    // Training is class C: it never runs by itself (last result "as of", else a preview). A trained
    // model's id is filled into Backtest and kept for Live Signals.
    add_view_tab(tabs,
                 {.key = QStringLiteral("train"),
                  .cost = ViewCost::Heavy,
                  .loading = tr("Training the model…"),
                  .shape = ui::LoadingSkeleton::Shape::Cards,
                  .qlib_dataset = true,
                  .after =
                      [this](bool ok, const QString& command, const QJsonObject& data) {
                          if (!ok || command != QLatin1String("train_model"))
                              return;
                          const QString id = data.value(QStringLiteral("model_id")).toString();
                          if (id.isEmpty())
                              return;
                          SettingsRepository::instance().set(QString::fromLatin1(kLastTrainedModelKey), id,
                                                             QStringLiteral("ai_quant_lab"));
                          ensure_view_built(QStringLiteral("backtest"));
                          // The saved models are read again; the new one becomes Backtest's Model ID.
                          refresh_saved_ids(SavedIds::TrainedModels, id);
                          // Backtest the names it was trained on unless the user chose others.
                          QLineEdit* bt_instr = text_inputs_.value(QStringLiteral("ml_bt_instr"));
                          const QLineEdit* train_instr = text_inputs_.value(QStringLiteral("ml_instruments"));
                          if (bt_instr && train_instr && bt_instr->text().trimmed().isEmpty())
                              bt_instr->setText(train_instr->text().trimmed());
                      },
                  .title = tr("Train a Qlib model"),
                  .returns = tr("Trains the model on the dataset's Alpha158 features for the instruments and split "
                                "above: the model id, the predictions and the configuration. The id is filled into "
                                "Backtest and Live Signals."),
                  .run_text = tr("TRAIN MODEL"),
                  .cost_note = tr("Trains a model · seconds for a few names, minutes for a pool")},
                 tr("Train Model"), [this](QWidget* train_tab, QVBoxLayout* ttvl) {
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
        // Train / validation split. The dataset bar fills it from the chosen dataset (validation =
        // the year before its last year, training = up to five years before that); with Train End
        // and Valid Start blank the script splits Train Start -> Valid End 80/20.
        const QList<QPair<QString, QString>> split_rows = {{QStringLiteral("ml_start"), tr("Train Start")},
                                                           {QStringLiteral("ml_train_end"), tr("Train End")},
                                                           {QStringLiteral("ml_valid_start"), tr("Valid Start")},
                                                           {QStringLiteral("ml_end"), tr("Valid End")}};
        for (const auto& [key, label] : split_rows) {
            auto* edit = new QLineEdit(train_tab);
            edit->setPlaceholderText(tr("YYYY-MM-DD"));
            edit->setStyleSheet(input_ss());
            text_inputs_[key] = edit;
            ttvl->addWidget(build_input_row(label, edit, train_tab));
        }
        auto* ml_run = make_run_button(tr("TRAIN MODEL"), train_tab);
        set_action(ml_run, [this]() {
            const QJsonArray instr = parse_csv_strings(text_inputs_["ml_instruments"]->text(), /*lowercase=*/true);
            if (instr.isEmpty()) {
                display_error(tr("Enter at least one instrument, e.g. aapl,msft."));
                return;
            }
            QString train_start, train_end, valid_start, valid_end;
            if (const QString err = check_iso_range(text_inputs_["ml_start"]->text(), text_inputs_["ml_train_end"]->text(),
                                                    train_start, train_end);
                !err.isEmpty()) {
                display_error(tr("Training: %1").arg(err));
                return;
            }
            if (const QString err = check_iso_range(text_inputs_["ml_valid_start"]->text(), text_inputs_["ml_end"]->text(),
                                                    valid_start, valid_end);
                !err.isEmpty()) {
                display_error(tr("Validation: %1").arg(err));
                return;
            }
            const bool split = !train_end.isEmpty() || !valid_start.isEmpty();
            if (split && (train_start.isEmpty() || train_end.isEmpty() || valid_start.isEmpty() || valid_end.isEmpty())) {
                display_error(tr("Fill all four dates, or leave Train End and Valid Start blank for an 80/20 split of "
                                 "Train Start to Valid End."));
                return;
            }
            if (split && train_end >= valid_start) {
                display_error(tr("Training (to %1) must end before validation starts (%2).").arg(train_end, valid_start));
                return;
            }
            QJsonObject params;
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }

            show_loading(tr("Training %1 on %2 instrument(s) — this can take several minutes...")
                             .arg(combo_inputs_["ml_model_type"]->currentText())
                             .arg(instr.size()));
            params["model_type"] = combo_inputs_["ml_model_type"]->currentText();
            params["instruments"] = instr;
            if (split) {
                params["train_start"] = train_start;
                params["train_end"] = train_end;
                params["valid_start"] = valid_start;
                params["valid_end"] = valid_end;
            } else {
                if (!train_start.isEmpty())
                    params["start_date"] = train_start;
                if (!valid_end.isEmpty())
                    params["end_date"] = valid_end;
            }
            AIQuantLabService::instance().model_train(params);
        });
        ttvl->addWidget(ml_run);
        ttvl->addStretch();
    });

    // ── Backtest ──
    // Needs a trained model: class C. The Model ID is the last model trained (Train Model fills it).
    add_view_tab(tabs,
                 {.key = QStringLiteral("backtest"),
                  .cost = ViewCost::Heavy,
                  .loading = tr("Backtesting the model…"),
                  .shape = ui::LoadingSkeleton::Shape::Chart,
                  .qlib_dataset = true,
                  .title = tr("Backtest a trained model"),
                  .returns = tr("Trades the model's predictions with TopK-Dropout over the period: total and "
                                "annualised return, Sharpe, drawdown, turnover and the equity curve. Train a model "
                                "first — its id is filled in here."),
                  .run_text = tr("RUN BACKTEST"),
                  .cost_note = tr("Reads the dataset · tens of seconds for a pool")},
                 tr("Backtest"), [this](QWidget* bt_tab, QVBoxLayout* btvl2) {
        // The saved models (newest first), until the list is read the last model trained (here or in an
        // earlier session). Editable: an id can still be typed.
        QString last_trained;
        if (const auto last = SettingsRepository::instance().get(QString::fromLatin1(kLastTrainedModelKey));
            last.is_ok())
            last_trained = last.value().trimmed();
        auto* bt_model =
            make_saved_id_combo(SavedIds::TrainedModels, QStringLiteral("ml_bt_model"), last_trained, bt_tab);
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
        set_action(bt_run2, [this]() {
            const QString model_id = saved_id(combo_inputs_.value(QStringLiteral("ml_bt_model")));
            if (model_id.isEmpty()) {
                display_error(tr("Pick a trained model (Train Model saves one), or type its Model ID."));
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
            QJsonObject params;
            QString why;
            if (!add_qlib_dataset(this, params, &why)) {
                display_error(why);
                return;
            }
            show_loading(tr("Backtesting %1 on %2 instrument(s)...").arg(model_id).arg(instr.size()));
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
    });

    // No stretch factor on the tabs: their height is capped (Maximum policy), and a capped
    // widget in a stretched cell is centred in it, which left a blank band above the form.
    vl->addWidget(tabs);
    add_result_pane(vl);
    return w;
}

} // namespace fincept::screens
