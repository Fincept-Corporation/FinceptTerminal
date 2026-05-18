// src/screens/ai_quant_lab/QuantModulePanel_AdvancedDisplays_Models.cpp
//
// ADVANCED-category renderers for the model-training family: advanced_models
// (list/create/train/predict/info), feature_engineering (per-indicator,
// select_features_by_ic, evaluate_expression), and portfolio_opt (BL, HRP,
// MinVar, MaxSharpe, EF).
//
// Part of the partial-class split of QuantModulePanel_AdvancedDisplays.cpp.

#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;
using namespace fincept::screens::quant_advanced_helpers;

void QuantModulePanel::display_advanced_models_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "list_available") {
        const auto models = payload.value("models").toArray();
        const bool torch = payload.value("torch_available").toBool();
        const bool qlib = payload.value("qlib_available").toBool();
        QSet<QString> categories;
        for (const auto& m : models)
            categories.insert(m.toObject().value("category").toString());
        QList<QWidget*> top = {
            gs_make_card("MODELS", QString::number(models.size()), this, ui::colors::POSITIVE()),
            gs_make_card("PYTORCH", torch ? "AVAILABLE" : "MISSING", this,
                         torch ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("QLIB", qlib ? "AVAILABLE" : "MISSING", this,
                         qlib ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("CATEGORIES", QString::number(categories.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!models.isEmpty()) {
            auto* table = new QTableWidget(models.size(), 4, this);
            table->setHorizontalHeaderLabels({"ID", "Name", "Category", "Available"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * 26 + 32, 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("id").toString().toUpper()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("name").toString()));
                table->setItem(i, 2, new QTableWidgetItem(m.value("category").toString()));
                const bool avail = m.value("available").toBool();
                auto* a = new QTableWidgetItem(avail ? "YES" : "NO");
                a->setForeground(QColor(avail ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
                a->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 3, a);
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 advanced model(s) available").arg(models.size()));
        return;
    }

    if (command == "create_model") {
        const auto cfg = payload.value("config").toObject();
        QList<QWidget*> top = {
            gs_make_card("MODEL ID", payload.value("model_id").toString(), this, ui::colors::POSITIVE()),
            gs_make_card("TYPE", payload.value("model_type").toString().toUpper(), this),
            gs_make_card("CONFIG KEYS", QString::number(cfg.size()), this),
            gs_make_card("STATUS", "CREATED", this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!cfg.isEmpty()) {
            auto* table = new QTableWidget(cfg.size(), 2, this);
            table->setHorizontalHeaderLabels({"Hyperparameter", "Value"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(cfg.size() * 24 + 32, 240));
            int r = 0;
            for (auto it = cfg.begin(); it != cfg.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                auto* vi = new QTableWidgetItem(format_val(it.value()));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(payload.value("message").toString("Model created"));
        return;
    }

    if (command == "train_model") {
        const double final_loss = payload.value("final_loss").toDouble();
        const int epochs = payload.value("epochs_trained").toInt();
        const int total_epochs = payload.value("total_epochs").toInt(epochs);
        const auto loss_hist = payload.value("loss_history").toArray();
        double initial_loss = loss_hist.isEmpty() ? 0.0 : loss_hist[0].toDouble();

        QList<QWidget*> top = {
            gs_make_card("MODEL ID", payload.value("model_id").toString(), this),
            gs_make_card("EPOCHS",
                         total_epochs > 0 && total_epochs != epochs
                             ? QString("%1 / %2").arg(epochs).arg(total_epochs)
                             : QString::number(epochs),
                         this, ui::colors::POSITIVE()),
            gs_make_card("FINAL LOSS", QString::number(final_loss, 'g', 4), this,
                         final_loss < initial_loss * 0.5 ? ui::colors::POSITIVE()
                                                          : ui::colors::WARNING()),
            gs_make_card("LOSS REDUCTION",
                         initial_loss > 0
                             ? QString::number((1.0 - final_loss / initial_loss) * 100, 'f', 1) + "%"
                             : "—",
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Loss history table — sample to first/middle/last 12 epochs
        if (!loss_hist.isEmpty()) {
            const int rows = std::min<int>(12, loss_hist.size());
            const int step = std::max<int>(1, loss_hist.size() / rows);
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({"Epoch", "Loss"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < loss_hist.size() && r < rows; i += step, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                auto* it = new QTableWidgetItem(QString::number(loss_hist[i].toDouble(), 'g', 6));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, it);
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Trained %1 epochs  final loss=%2")
                                   .arg(epochs).arg(final_loss, 0, 'g', 4));
        return;
    }

    if (command == "predict") {
        const auto preds = payload.value("predictions").toArray();
        double mean = 0.0, mn = std::numeric_limits<double>::infinity(), mx = -std::numeric_limits<double>::infinity();
        for (const auto& v : preds) {
            const double d = v.toDouble();
            mean += d;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
        }
        if (!preds.isEmpty()) mean /= preds.size();
        QList<QWidget*> top = {
            gs_make_card("MODEL ID", payload.value("model_id").toString(), this),
            gs_make_card("PREDICTIONS", QString::number(preds.size()), this, ui::colors::POSITIVE()),
            gs_make_card("MEAN", QString::number(mean, 'f', 6), this, gs_pos_neg_color(mean)),
            gs_make_card("RANGE",
                         preds.isEmpty() ? "—"
                                         : QString("[%1, %2]").arg(mn, 0, 'f', 4).arg(mx, 0, 'f', 4),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Show first/middle/last 12 predictions
        if (!preds.isEmpty()) {
            const int rows = std::min<int>(12, preds.size());
            const int step = std::max<int>(1, preds.size() / rows);
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({"Index", "Prediction"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < preds.size() && r < rows; i += step, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                const double v = preds[i].toDouble();
                auto* it = new QTableWidgetItem(QString::number(v, 'f', 6));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                it->setForeground(QColor(gs_pos_neg_color(v)));
                table->setItem(r, 1, it);
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 predictions").arg(preds.size()));
        return;
    }

    if (command == "info" || command == "list_models") {
        // info: single model; list_models: array of summaries
        if (command == "info") {
            QList<QWidget*> top = {
                gs_make_card("MODEL ID", payload.value("model_id").toString(), this),
                gs_make_card("TYPE", payload.value("model_type").toString().toUpper(), this),
                gs_make_card("TRAINED", payload.value("trained").toBool() ? "YES" : "NO", this,
                             payload.value("trained").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card("EPOCHS", fmt_int_safe(payload.value("epochs_trained")), this),
            };
            results_layout_->addWidget(gs_card_row(top, this));
            status_label_->setText("Model info loaded");
        } else {
            const auto models = payload.value("models").toArray();
            QList<QWidget*> top = {
                gs_make_card("REGISTERED", QString::number(models.size()), this, ui::colors::POSITIVE()),
            };
            results_layout_->addWidget(gs_card_row(top, this));
            if (!models.isEmpty()) {
                auto* table = new QTableWidget(models.size(), 5, this);
                table->setHorizontalHeaderLabels({"Model ID", "Type", "Trained", "Epochs", "Created"});
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setStyleSheet(table_ss());
                table->setMaximumHeight(qMin(models.size() * 26 + 32, 360));
                for (int i = 0; i < models.size(); ++i) {
                    const auto m = models[i].toObject();
                    table->setItem(i, 0, new QTableWidgetItem(m.value("model_id").toString()));
                    table->setItem(i, 1, new QTableWidgetItem(m.value("type").toString().toUpper()));
                    const bool tr = m.value("trained").toBool();
                    auto* t = new QTableWidgetItem(tr ? "YES" : "NO");
                    t->setForeground(QColor(tr ? ui::colors::POSITIVE() : ui::colors::WARNING()));
                    t->setTextAlignment(Qt::AlignCenter);
                    table->setItem(i, 2, t);
                    table->setItem(i, 3, new QTableWidgetItem(QString::number(m.value("epochs_trained").toInt())));
                    table->setItem(i, 4, new QTableWidgetItem(m.value("created_at").toString()));
                    table->setRowHeight(i, 26);
                }
                results_layout_->addWidget(table);
            }
            status_label_->setText(QString("%1 trained model(s)").arg(models.size()));
        }
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 4. FEATURE ENGINEERING
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_feature_engineering_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── check_status ─────────────────────────────────────────────────────────
    if (command == "check_status") {
        const bool pd = payload.value("pandas_available").toBool();
        const auto inds = payload.value("indicators_available").toArray();
        QList<QWidget*> top = {
            gs_make_card("PANDAS", pd ? "AVAILABLE" : "MISSING", this,
                         pd ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("INDICATORS", QString::number(inds.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        if (!inds.isEmpty()) {
            QStringList lst;
            for (const auto& v : inds) lst << v.toString();
            auto* lbl = new QLabel("Available: " + lst.join(", "));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText("Feature engineering ready");
        return;
    }

    // ── select_features_by_ic ────────────────────────────────────────────────
    if (command == "select_features_by_ic") {
        const auto sel = payload.value("selected_features").toArray();
        QList<QWidget*> top = {
            gs_make_card("SELECTED", QString::number(sel.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!sel.isEmpty()) {
            auto* table = new QTableWidget(sel.size(), 2, this);
            table->setHorizontalHeaderLabels({"Rank", "Feature"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(sel.size() * 24 + 32, 320));
            for (int i = 0; i < sel.size(); ++i) {
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
                table->setItem(i, 1, new QTableWidgetItem(sel[i].toString()));
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("Selected %1 feature(s) by IC").arg(sel.size()));
        return;
    }

    // ── Multi-series indicators (MACD, Bollinger, Stochastic) ────────────────
    auto render_multi_series = [this](const QStringList& keys, const QJsonObject& payload) {
        QList<QWidget*> stats;
        QHash<QString, QJsonArray> series;
        for (const auto& k : keys) {
            const auto arr = payload.value(k).toArray();
            series.insert(k, arr);
            double sum = 0, mn = std::numeric_limits<double>::infinity(),
                   mx = -std::numeric_limits<double>::infinity();
            int n = 0;
            for (const auto& v : arr) {
                if (v.isNull()) continue;
                const double d = v.toDouble();
                sum += d;
                mn = std::min(mn, d);
                mx = std::max(mx, d);
                ++n;
            }
            const double mean = n > 0 ? sum / n : 0.0;
            stats << gs_make_card(k.toUpper(), QString::number(mean, 'f', 4), this, gs_pos_neg_color(mean));
        }
        results_layout_->addWidget(gs_card_row(stats, this));

        // Multi-column table sample
        int max_n = 0;
        // QJsonArray::size() returns qsizetype (qint64); std::max requires
        // matching arg types — explicit cast keeps both as int.
        for (const auto& a : series) max_n = std::max(max_n, static_cast<int>(a.size()));
        const int rows = std::min<int>(15, max_n);
        const int step = std::max<int>(1, max_n / rows);
        auto* table = new QTableWidget(rows, keys.size() + 1, this);
        QStringList hdr = {"Index"};
        for (const auto& k : keys) hdr << k.toUpper();
        table->setHorizontalHeaderLabels(hdr);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(rows * 22 + 32);
        for (int i = 0, r = 0; i < max_n && r < rows; i += step, ++r) {
            table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
            for (int c = 0; c < keys.size(); ++c) {
                const auto& a = series[keys[c]];
                if (i < a.size() && !a[i].isNull()) {
                    const double v = a[i].toDouble();
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, c + 1, it);
                }
            }
            table->setRowHeight(r, 22);
        }
        results_layout_->addWidget(table);
    };

    if (command == "macd") {
        render_multi_series({"macd", "signal", "histogram"}, payload);
        status_label_->setText("MACD computed");
        return;
    }
    if (command == "bollinger_bands") {
        render_multi_series({"upper", "middle", "lower"}, payload);
        status_label_->setText("Bollinger bands computed");
        return;
    }
    if (command == "stochastic_oscillator") {
        render_multi_series({"k", "d"}, payload);
        status_label_->setText("Stochastic oscillator computed");
        return;
    }

    // ── Single-series indicators + evaluate_expression ───────────────────────
    if (payload.contains("data") && payload.value("data").isArray()) {
        const auto series = payload.value("data").toArray();
        double sum = 0, mn = std::numeric_limits<double>::infinity(),
               mx = -std::numeric_limits<double>::infinity();
        int n = 0;
        for (const auto& v : series) {
            if (v.isNull()) continue;
            const double d = v.toDouble();
            sum += d;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
            ++n;
        }
        const double mean = n > 0 ? sum / n : 0.0;

        QList<QWidget*> top = {
            gs_make_card("INDICATOR", command.toUpper(), this),
            gs_make_card("OBSERVATIONS", QString::number(n), this),
            gs_make_card("MEAN", QString::number(mean, 'f', 4), this, gs_pos_neg_color(mean)),
            gs_make_card("RANGE",
                         n > 0 ? QString("[%1, %2]").arg(mn, 0, 'f', 4).arg(mx, 0, 'f', 4) : "—",
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Sampled table
        const int rows = std::min<int>(15, series.size());
        const int step = std::max<int>(1, series.size() / rows);
        auto* table = new QTableWidget(rows, 2, this);
        table->setHorizontalHeaderLabels({"Index", command.toUpper()});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(rows * 22 + 32);
        for (int i = 0, r = 0; i < series.size() && r < rows; i += step, ++r) {
            table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
            if (!series[i].isNull()) {
                const double v = series[i].toDouble();
                auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, it);
            }
            table->setRowHeight(r, 22);
        }
        results_layout_->addWidget(table);
        status_label_->setText(QString("%1: %2 obs").arg(command).arg(n));
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 5. PORTFOLIO OPTIMIZATION
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_portfolio_opt_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    const QString method = payload.value("method").toString();
    QString header_text = (method.isEmpty() ? command : method).toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "check_status") {
        const bool sp = payload.value("scipy_available").toBool();
        const auto methods = payload.value("methods_available").toArray();
        QList<QWidget*> top = {
            gs_make_card("SCIPY", sp ? "AVAILABLE" : "MISSING", this,
                         sp ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("METHODS", QString::number(methods.size()), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        status_label_->setText("Portfolio optimization ready");
        return;
    }

    // Efficient frontier — special case: array of {return, volatility, sharpe, weights}
    if (command == "efficient_frontier") {
        const auto frontier = payload.value("frontier").toArray();
        // Find min-vol and max-sharpe portfolios
        double min_vol = std::numeric_limits<double>::infinity();
        double max_sharpe = -std::numeric_limits<double>::infinity();
        QJsonObject min_vol_pt, max_sharpe_pt;
        for (const auto& v : frontier) {
            const auto p = v.toObject();
            const double vol = p.value("volatility").toDouble();
            const double sh = p.value("sharpe").toDouble();
            if (vol < min_vol) { min_vol = vol; min_vol_pt = p; }
            if (sh > max_sharpe) { max_sharpe = sh; max_sharpe_pt = p; }
        }

        QList<QWidget*> top = {
            gs_make_card("PORTFOLIOS", QString::number(payload.value("num_portfolios").toInt()), this,
                         ui::colors::POSITIVE()),
            gs_make_card("MIN VOL", fmt_pct_safe(QJsonValue(min_vol)), this, ui::colors::INFO()),
            gs_make_card("MAX SHARPE", QString::number(max_sharpe, 'f', 3), this,
                         max_sharpe > 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("RANGE",
                         frontier.isEmpty() ? "—"
                                            : QString("%1% – %2%")
                                                  .arg(frontier.first().toObject().value("return").toDouble() * 100, 0, 'f', 1)
                                                  .arg(frontier.last().toObject().value("return").toDouble() * 100, 0, 'f', 1),
                         this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Sample frontier
        if (!frontier.isEmpty()) {
            const int rows = std::min<int>(15, frontier.size());
            const int step = std::max<int>(1, frontier.size() / rows);
            auto* table = new QTableWidget(rows, 3, this);
            table->setHorizontalHeaderLabels({"Return", "Volatility", "Sharpe"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 24 + 32);
            for (int i = 0, r = 0; i < frontier.size() && r < rows; i += step, ++r) {
                const auto p = frontier[i].toObject();
                const double ret = p.value("return").toDouble();
                const double vol = p.value("volatility").toDouble();
                const double sh = p.value("sharpe").toDouble();
                auto add = [&](int col, const QString& s, const QString& fg = {}) {
                    auto* it = new QTableWidgetItem(s);
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (!fg.isEmpty()) it->setForeground(QColor(fg));
                    table->setItem(r, col, it);
                };
                add(0, QString::number(ret * 100, 'f', 2) + "%", gs_pos_neg_color(ret));
                add(1, QString::number(vol * 100, 'f', 2) + "%");
                add(2, QString::number(sh, 'f', 3),
                    sh >= 1.0 ? ui::colors::POSITIVE() : sh > 0 ? ui::colors::WARNING() : ui::colors::NEGATIVE());
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("EF: %1 portfolios  best Sharpe=%2")
                                   .arg(payload.value("num_portfolios").toInt())
                                   .arg(max_sharpe, 0, 'f', 3));
        return;
    }

    // Standard optimization methods (BL, HRP, MinVar, MaxSharpe)
    const QJsonObject weights = payload.value("weights").toObject();
    const double exp_ret = payload.value("expected_return").toDouble();
    const double exp_vol = payload.value("expected_volatility").toDouble();
    const double sharpe = payload.value("sharpe_ratio").toDouble();
    const bool has_ret = payload.contains("expected_return");
    const bool has_sharpe = payload.contains("sharpe_ratio");

    QList<QWidget*> top = {
        gs_make_card("ASSETS", QString::number(weights.size()), this, ui::colors::POSITIVE()),
        gs_make_card("EXP. VOLATILITY", fmt_pct_safe(QJsonValue(exp_vol)), this),
        gs_make_card("EXP. RETURN", has_ret ? fmt_pct_safe(QJsonValue(exp_ret)) : "—", this,
                     has_ret ? gs_pos_neg_color(exp_ret) : ui::colors::TEXT_TERTIARY()),
        gs_make_card("SHARPE", has_sharpe ? QString::number(sharpe, 'f', 3) : "—", this,
                     !has_sharpe ? ui::colors::TEXT_TERTIARY()
                                 : sharpe >= 1.0 ? ui::colors::POSITIVE()
                                                  : sharpe > 0 ? ui::colors::WARNING()
                                                                : ui::colors::NEGATIVE()),
    };
    results_layout_->addWidget(gs_card_row(top, this));

    // Black-Litterman extras
    if (command == "black_litterman") {
        QList<QWidget*> bl = {
            gs_make_card("VIEWS", QString::number(payload.value("views_incorporated").toInt()), this, ui::colors::INFO()),
            gs_make_card("EQUILIBRIUM",
                         QString::number(payload.value("equilibrium_returns").toObject().size()) + " assets", this),
            gs_make_card("POSTERIOR",
                         QString::number(payload.value("posterior_returns").toObject().size()) + " assets", this),
            gs_make_card("METHOD", "BL", this),
        };
        results_layout_->addWidget(gs_card_row(bl, this));
    }

    if (!weights.isEmpty()) {
        results_layout_->addWidget(gs_section_header("WEIGHTS", accent));
        results_layout_->addWidget(build_weights_table(weights, this));
    }

    status_label_->setText(QString("%1: %2 assets, vol=%3%")
                               .arg(method.isEmpty() ? command : method)
                               .arg(weights.size())
                               .arg(exp_vol * 100, 0, 'f', 2));
}

// ═════════════════════════════════════════════════════════════════════════════
// 6. FACTOR EVALUATION
// ═════════════════════════════════════════════════════════════════════════════
} // namespace fincept::screens
