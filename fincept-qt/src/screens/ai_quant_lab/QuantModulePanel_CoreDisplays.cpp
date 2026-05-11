// src/screens/ai_quant_lab/QuantModulePanel_CoreDisplays.cpp
//
// Card-based result renderers for CORE-category panels that previously
// rendered plain QLabel text into the bottom results pane.
//
//   - factor_discovery — get_factor_library, get_data, get_instruments,
//                        get_calendar
//   - model_library    — list_models, check_status, train_model
//   - live_signals     — get_data, get_factor_analysis,
//                        get_feature_importance
//
// Skipped:
//   - backtesting — already has display_backtest_result (KPI grid in
//     QuantModulePanel_Backtesting.cpp). The dispatch in on_result still
//     routes there directly.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QPair>
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

namespace {

[[maybe_unused]] QString fmt_int_safe(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toInt());
}

// Returns false if the payload was an error (already displayed via callback).
bool check_success(const QJsonObject& payload,
                   const std::function<void(const QString&)>& display_error_fn) {
    if (!payload.value("success").toBool(false)) {
        const QString err = payload.value("error").toString("Unknown error");
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? "Input error: "
                              : kind == "runtime"    ? "Computation failed: "
                                                     : "";
        display_error_fn(prefix + err);
        return false;
    }
    return true;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// 1. FACTOR DISCOVERY
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_factor_discovery_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── get_data ─────────────────────────────────────────────────────────────
    if (command == "get_data") {
        const int total = payload.value("total_records").toInt();
        const auto range = payload.value("date_range").toObject();
        const auto instruments = payload.value("instruments").toArray();
        const auto fields = payload.value("fields").toArray();
        const QString freq = payload.value("freq").toString("day");
        const QString warning = payload.value("warning").toString();

        QList<QWidget*> top = {
            gs_make_card("RECORDS", QString::number(total), this, ui::colors::POSITIVE()),
            gs_make_card("INSTRUMENTS", QString::number(instruments.size()), this),
            gs_make_card("FIELDS", QString::number(fields.size()), this),
            gs_make_card("FREQUENCY", freq.toUpper(), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> range_row = {
            gs_make_card("START DATE", range.value("start").toString("—"), this),
            gs_make_card("END DATE", range.value("end").toString("—"), this),
            gs_make_card("WARNING", warning.isEmpty() ? "NONE" : "PRESENT", this,
                         warning.isEmpty() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("STATUS", "OK", this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(range_row, this));

        if (!warning.isEmpty()) {
            auto* lbl = new QLabel("⚠ " + warning);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:11px; padding:8px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::WARNING()));
            results_layout_->addWidget(lbl);
        }

        // Instruments list (compact)
        if (!instruments.isEmpty()) {
            QStringList names;
            for (const auto& v : instruments) names << v.toString();
            const int show = std::min<int>(20, names.size());
            QString text = "Instruments (" + QString::number(names.size()) + "): "
                           + names.mid(0, show).join(", ");
            if (names.size() > show)
                text += " … +" + QString::number(names.size() - show) + " more";
            auto* lbl = new QLabel(text);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }

        status_label_->setText(QString("%1 records  %2→%3")
                                   .arg(total).arg(range.value("start").toString())
                                   .arg(range.value("end").toString()));
        return;
    }

    // ── get_instruments ──────────────────────────────────────────────────────
    if (command == "get_instruments") {
        const auto instruments = payload.value("instruments").toArray();
        const QString market = payload.value("market").toString();
        const int count = payload.value("count").toInt(instruments.size());

        QList<QWidget*> top = {
            gs_make_card("MARKET", market.toUpper().isEmpty() ? "—" : market.toUpper(), this, ui::colors::INFO()),
            gs_make_card("COUNT", QString::number(count), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Table of instrument symbols
        if (!instruments.isEmpty()) {
            const int rows = instruments.size();
            const int cols = 4;
            const int table_rows = (rows + cols - 1) / cols;
            auto* table = new QTableWidget(table_rows, cols, this);
            table->setHorizontalHeaderLabels({"Symbol", "Symbol", "Symbol", "Symbol"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(table_rows * 22 + 32, 360));
            for (int i = 0; i < instruments.size(); ++i) {
                const int r = i / cols;
                const int c = i % cols;
                auto* it = new QTableWidgetItem(instruments[i].toString());
                it->setTextAlignment(Qt::AlignCenter);
                table->setItem(r, c, it);
                if (c == 0) table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 instrument(s)  market=%2").arg(count).arg(market));
        return;
    }

    // ── get_calendar ─────────────────────────────────────────────────────────
    if (command == "get_calendar") {
        const auto dates = payload.value("dates").toArray();
        const int count = payload.value("count").toInt(dates.size());
        const QString freq = payload.value("freq").toString("day");

        QList<QWidget*> top = {
            gs_make_card("DATES", QString::number(count), this, ui::colors::POSITIVE()),
            gs_make_card("FREQUENCY", freq.toUpper(), this, ui::colors::INFO()),
            gs_make_card("FIRST",
                         dates.isEmpty() ? "—" : dates.first().toString(), this),
            gs_make_card("LAST",
                         dates.isEmpty() ? "—" : dates.last().toString(), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Sample dates table
        if (!dates.isEmpty()) {
            const int rows = std::min<int>(15, dates.size());
            const int step = std::max<int>(1, dates.size() / rows);
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({"Index", "Date"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            for (int i = 0, r = 0; i < dates.size() && r < rows; i += step, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                table->setItem(r, 1, new QTableWidgetItem(dates[i].toString()));
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 trading day(s)").arg(count));
        return;
    }

    // ── get_factor_library (legacy shape — Python may not implement it) ──────
    if (command == "get_factor_library") {
        const auto factors = payload.value("factors").toArray();
        QList<QWidget*> top = {
            gs_make_card("FACTORS", QString::number(factors.size()), this,
                         factors.isEmpty() ? ui::colors::WARNING() : ui::colors::POSITIVE()),
            gs_make_card("STATUS",
                         factors.isEmpty() ? "EMPTY" : "LOADED", this,
                         factors.isEmpty() ? ui::colors::WARNING() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!factors.isEmpty()) {
            auto* table = new QTableWidget(factors.size(), 3, this);
            table->setHorizontalHeaderLabels({"Name", "Category", "Expression"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(factors.size() * 26 + 32, 360));
            for (int i = 0; i < factors.size(); ++i) {
                const auto f = factors[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(f.value("name").toString()));
                table->setItem(i, 1, new QTableWidgetItem(f.value("category").toString("—")));
                table->setItem(i, 2, new QTableWidgetItem(f.value("expression").toString("—")));
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 factor(s)").arg(factors.size()));
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 2. MODEL LIBRARY
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_model_library_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── list_models ──────────────────────────────────────────────────────────
    if (command == "list_models") {
        const auto models = payload.value("models").toArray();
        const int total = payload.value("count").toInt(models.size());
        const int avail = payload.value("available_count").toInt();
        const auto types = payload.value("model_types").toObject();

        QList<QWidget*> top = {
            gs_make_card("TOTAL", QString::number(total), this, ui::colors::POSITIVE()),
            gs_make_card("AVAILABLE", QString::number(avail), this,
                         avail > 0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("TYPES", QString::number(types.size()), this),
            gs_make_card("UNAVAILABLE", QString::number(total - avail), this,
                         (total - avail) > 0 ? ui::colors::WARNING() : ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!types.isEmpty()) {
            QList<QWidget*> type_cards;
            for (auto it = types.begin(); it != types.end(); ++it) {
                const auto arr = it.value().toArray();
                QString lbl = it.key().toUpper();
                lbl.replace('_', ' ');
                type_cards << gs_make_card(lbl, QString::number(arr.size()), this, ui::colors::INFO());
                if (type_cards.size() == 4) {
                    results_layout_->addWidget(gs_card_row(type_cards, this));
                    type_cards.clear();
                }
            }
            // Pad and emit any remainder
            while (!type_cards.isEmpty() && type_cards.size() < 4)
                type_cards << gs_make_card(" ", " ", this);
            if (!type_cards.isEmpty())
                results_layout_->addWidget(gs_card_row(type_cards, this));
        }

        if (!models.isEmpty()) {
            auto* table = new QTableWidget(models.size(), 4, this);
            table->setHorizontalHeaderLabels({"ID", "Name", "Type", "Available"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * 26 + 32, 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("id").toString().toUpper()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("name").toString()));
                QString t = m.value("type").toString();
                t.replace('_', ' ');
                table->setItem(i, 2, new QTableWidgetItem(t.toUpper()));
                const bool ok = m.value("available").toBool();
                auto* a = new QTableWidgetItem(ok ? "YES" : "NO");
                a->setForeground(QColor(ok ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
                a->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 3, a);
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 model(s)  %2 available").arg(total).arg(avail));
        return;
    }

    // ── check_status ─────────────────────────────────────────────────────────
    if (command == "check_status") {
        const bool qlib = payload.value("qlib_available").toBool();
        const QString version = payload.value("version").toString("—");
        const auto avail = payload.value("models_available").toObject();
        const auto handlers = payload.value("handlers_available").toArray();
        const auto strategies = payload.value("strategies_available").toArray();

        int n_avail = 0, n_unavail = 0;
        for (auto it = avail.begin(); it != avail.end(); ++it)
            (it.value().toBool() ? n_avail : n_unavail)++;

        QList<QWidget*> top = {
            gs_make_card("QLIB", qlib ? "READY" : "MISSING", this,
                         qlib ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("VERSION", version, this, ui::colors::INFO()),
            gs_make_card("HANDLERS", QString::number(handlers.size()), this),
            gs_make_card("STRATEGIES", QString::number(strategies.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> models = {
            gs_make_card("MODELS AVAIL", QString::number(n_avail), this,
                         n_avail > 0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("MODELS MISSING", QString::number(n_unavail), this,
                         n_unavail > 0 ? ui::colors::WARNING() : ui::colors::POSITIVE()),
            gs_make_card("MODELS TOTAL", QString::number(avail.size()), this),
            gs_make_card("STATUS",
                         qlib ? "OPERATIONAL" : "DEGRADED", this,
                         qlib ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(models, this));

        // Models availability table
        if (!avail.isEmpty()) {
            auto* table = new QTableWidget(avail.size(), 2, this);
            table->setHorizontalHeaderLabels({"Model", "Status"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(avail.size() * 24 + 32, 320));
            int r = 0;
            for (auto it = avail.begin(); it != avail.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                const bool ok = it.value().toBool();
                auto* st = new QTableWidgetItem(ok ? "AVAILABLE" : "MISSING");
                st->setForeground(QColor(ok ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
                st->setTextAlignment(Qt::AlignCenter);
                table->setItem(r, 1, st);
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }

        if (!handlers.isEmpty()) {
            QStringList lst;
            for (const auto& v : handlers) lst << v.toString();
            auto* lbl = new QLabel("Handlers: " + lst.join(", "));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }

        status_label_->setText(qlib ? QString("Qlib %1 ready  %2 model(s)").arg(version).arg(n_avail)
                                    : "Qlib unavailable");
        return;
    }

    // ── train_model ──────────────────────────────────────────────────────────
    if (command == "train_model") {
        const QString model_id = payload.value("model_id").toString();
        const QString model_type = payload.value("model_type").toString();
        const QString handler = payload.value("handler").toString();
        const int n_pred = payload.value("predictions_count").toInt();
        const auto config = payload.value("config").toObject();
        const auto train = payload.value("training_period").toObject();
        const auto val = payload.value("validation_period").toObject();
        // Legacy shape: metrics dict
        const auto metrics = payload.value("metrics").toObject();

        QList<QWidget*> top = {
            gs_make_card("MODEL ID", model_id, this, ui::colors::POSITIVE()),
            gs_make_card("TYPE", model_type.toUpper(), this),
            gs_make_card("HANDLER", handler.isEmpty() ? "—" : handler, this, ui::colors::INFO()),
            gs_make_card("PREDICTIONS", n_pred > 0 ? QString::number(n_pred) : "—", this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!train.isEmpty() || !val.isEmpty()) {
            QList<QWidget*> periods = {
                gs_make_card("TRAIN START", train.value("start").toString("—"), this),
                gs_make_card("TRAIN END", train.value("end").toString("—"), this),
                gs_make_card("VAL START", val.value("start").toString("—"), this),
                gs_make_card("VAL END", val.value("end").toString("—"), this),
            };
            results_layout_->addWidget(gs_card_row(periods, this));
        }

        // Render metrics dict (legacy) if present
        if (!metrics.isEmpty()) {
            results_layout_->addWidget(gs_section_header("METRICS", accent));
            QList<QWidget*> metric_cards;
            for (auto it = metrics.begin(); it != metrics.end(); ++it) {
                const double v = it.value().toDouble();
                QString lbl = it.key().toUpper();
                lbl.replace('_', ' ');
                metric_cards << gs_make_card(lbl, QString::number(v, 'f', 4), this, gs_pos_neg_color(v));
                if (metric_cards.size() == 4) {
                    results_layout_->addWidget(gs_card_row(metric_cards, this));
                    metric_cards.clear();
                }
            }
            while (!metric_cards.isEmpty() && metric_cards.size() < 4)
                metric_cards << gs_make_card(" ", " ", this);
            if (!metric_cards.isEmpty())
                results_layout_->addWidget(gs_card_row(metric_cards, this));
        }

        // Render config dict
        if (!config.isEmpty()) {
            results_layout_->addWidget(gs_section_header("CONFIG", accent));
            auto* table = new QTableWidget(config.size(), 2, this);
            table->setHorizontalHeaderLabels({"Hyperparameter", "Value"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(config.size() * 24 + 32, 280));
            int r = 0;
            for (auto it = config.begin(); it != config.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                auto* vi = new QTableWidgetItem(format_val(it.value()));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                table->setRowHeight(r, 24);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(payload.value("message").toString(QString("Trained: %1").arg(model_id)));
        return;
    }

    display_result(payload);
}

// ═════════════════════════════════════════════════════════════════════════════
// 3. LIVE SIGNALS
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_live_signals_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = module_.color.name();
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── get_data (same shape as factor_discovery::get_data) ──────────────────
    if (command == "get_data") {
        const int total = payload.value("total_records").toInt();
        const auto range = payload.value("date_range").toObject();
        const auto instruments = payload.value("instruments").toArray();
        const auto fields = payload.value("fields").toArray();
        const QString freq = payload.value("freq").toString("day");
        const QString warning = payload.value("warning").toString();

        QList<QWidget*> top = {
            gs_make_card("RECORDS", QString::number(total), this, ui::colors::POSITIVE()),
            gs_make_card("INSTRUMENTS", QString::number(instruments.size()), this),
            gs_make_card("FIELDS", QString::number(fields.size()), this),
            gs_make_card("FREQUENCY", freq.toUpper(), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> range_row = {
            gs_make_card("START DATE", range.value("start").toString("—"), this),
            gs_make_card("END DATE", range.value("end").toString("—"), this),
            gs_make_card("WARNING", warning.isEmpty() ? "NONE" : "PRESENT", this,
                         warning.isEmpty() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("STATUS", "OK", this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(range_row, this));

        if (!warning.isEmpty()) {
            auto* lbl = new QLabel("⚠ " + warning);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:11px; padding:8px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::WARNING()));
            results_layout_->addWidget(lbl);
        }

        if (!instruments.isEmpty()) {
            QStringList names;
            for (const auto& v : instruments) names << v.toString();
            const int show = std::min<int>(20, names.size());
            QString text = "Instruments (" + QString::number(names.size()) + "): "
                           + names.mid(0, show).join(", ");
            if (names.size() > show)
                text += " … +" + QString::number(names.size() - show) + " more";
            auto* lbl = new QLabel(text);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-family:'Courier New'; font-size:10px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText(QString("%1 records  %2→%3")
                                   .arg(total).arg(range.value("start").toString())
                                   .arg(range.value("end").toString()));
        return;
    }

    // ── get_factor_analysis ──────────────────────────────────────────────────
    if (command == "get_factor_analysis") {
        const QString model_id = payload.value("model_id").toString();
        const QString analysis = payload.value("analysis_type").toString();
        const auto results = payload.value("results").toObject();

        // Modern shape: results = {IC_mean, IC_std, ICIR, Rank_IC_mean, Rank_IC_std, Rank_ICIR}
        if (!results.isEmpty()) {
            const double ic_mean = results.value("IC_mean").toDouble();
            const double icir = results.value("ICIR").toDouble();
            const double rank_ic = results.value("Rank_IC_mean").toDouble();
            const double rank_icir = results.value("Rank_ICIR").toDouble();

            QList<QWidget*> top = {
                gs_make_card("MODEL", model_id.isEmpty() ? "—" : model_id, this),
                gs_make_card("TYPE", analysis.toUpper().isEmpty() ? "—" : analysis.toUpper(), this, ui::colors::INFO()),
                gs_make_card("IC MEAN", QString::number(ic_mean, 'f', 4), this, gs_pos_neg_color(ic_mean)),
                gs_make_card("ICIR", QString::number(icir, 'f', 3), this,
                             icir >= 0.5 ? ui::colors::POSITIVE()
                                         : icir > 0 ? ui::colors::WARNING()
                                                    : ui::colors::NEGATIVE()),
            };
            results_layout_->addWidget(gs_card_row(top, this));

            QList<QWidget*> rank = {
                gs_make_card("IC STD", QString::number(results.value("IC_std").toDouble(), 'f', 4), this),
                gs_make_card("RANK IC MEAN", QString::number(rank_ic, 'f', 4), this, gs_pos_neg_color(rank_ic)),
                gs_make_card("RANK IC STD", QString::number(results.value("Rank_IC_std").toDouble(), 'f', 4), this),
                gs_make_card("RANK ICIR", QString::number(rank_icir, 'f', 3), this,
                             rank_icir >= 0.5 ? ui::colors::POSITIVE()
                                              : rank_icir > 0 ? ui::colors::WARNING()
                                                              : ui::colors::NEGATIVE()),
            };
            results_layout_->addWidget(gs_card_row(rank, this));
            status_label_->setText(QString("IC=%1 ICIR=%2 RankIC=%3")
                                       .arg(ic_mean, 0, 'f', 4).arg(icir, 0, 'f', 3).arg(rank_ic, 0, 'f', 4));
            return;
        }

        // Legacy shape: factors = [{name, ic, sharpe}]
        const auto factors = payload.value("factors").toArray();
        QList<QWidget*> top = {
            gs_make_card("FACTORS", QString::number(factors.size()), this, ui::colors::POSITIVE()),
            gs_make_card("MODEL", model_id.isEmpty() ? "—" : model_id, this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!factors.isEmpty()) {
            auto* table = new QTableWidget(factors.size(), 3, this);
            table->setHorizontalHeaderLabels({"Factor", "IC", "Sharpe"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(factors.size() * 24 + 32, 320));
            for (int i = 0; i < factors.size(); ++i) {
                const auto f = factors[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(f.value("name").toString("—")));
                const QJsonValue ic = f.value("ic");
                auto* ici = new QTableWidgetItem(ic.isNull() ? "N/A" : QString::number(ic.toDouble(), 'f', 4));
                ici->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                if (!ic.isNull()) ici->setForeground(QColor(gs_pos_neg_color(ic.toDouble())));
                table->setItem(i, 1, ici);
                const QJsonValue sh = f.value("sharpe");
                auto* shi = new QTableWidgetItem(sh.isNull() ? "N/A" : QString::number(sh.toDouble(), 'f', 3));
                shi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                if (!sh.isNull()) shi->setForeground(QColor(gs_pos_neg_color(sh.toDouble())));
                table->setItem(i, 2, shi);
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 factor(s)").arg(factors.size()));
        return;
    }

    // ── get_feature_importance ───────────────────────────────────────────────
    if (command == "get_feature_importance") {
        const QString model_id = payload.value("model_id").toString();
        const auto features = payload.value("feature_importance").toObject();

        QList<QPair<QString, double>> sorted;
        for (auto it = features.begin(); it != features.end(); ++it)
            sorted.append({it.key(), it.value().toDouble()});
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return std::abs(a.second) > std::abs(b.second); });

        const double top_val = sorted.isEmpty() ? 0.0 : sorted.first().second;
        const QString top_feat = sorted.isEmpty() ? "—" : sorted.first().first;

        QList<QWidget*> top = {
            gs_make_card("MODEL", model_id.isEmpty() ? "—" : model_id, this),
            gs_make_card("FEATURES", QString::number(sorted.size()), this, ui::colors::POSITIVE()),
            gs_make_card("TOP FEATURE", top_feat, this, ui::colors::INFO()),
            gs_make_card("TOP IMPORTANCE", QString::number(top_val, 'f', 4), this,
                         gs_pos_neg_color(top_val)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!sorted.isEmpty()) {
            auto* table = new QTableWidget(sorted.size(), 3, this);
            table->setHorizontalHeaderLabels({"Rank", "Feature", "Importance"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(sorted.size() * 24 + 32, 360));
            for (int i = 0; i < sorted.size(); ++i) {
                auto* r = new QTableWidgetItem(QString::number(i + 1));
                r->setTextAlignment(Qt::AlignCenter);
                if (i == 0) r->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(i, 0, r);
                table->setItem(i, 1, new QTableWidgetItem(sorted[i].first));
                const double v = sorted[i].second;
                auto* vi = new QTableWidgetItem(QString::number(v, 'f', 6));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                vi->setForeground(QColor(gs_pos_neg_color(v)));
                table->setItem(i, 2, vi);
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 feature(s)  top=%2").arg(sorted.size()).arg(top_feat));
        return;
    }

    display_result(payload);
}

} // namespace fincept::screens
