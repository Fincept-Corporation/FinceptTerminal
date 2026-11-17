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
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QCoreApplication>
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
// check_success() / fmt_*_safe() used to be duplicated in a file-local
// anonymous namespace here; they now come from the shared helper header.
using namespace fincept::screens::quant_advanced_helpers;
using namespace fincept::screens::quant_charts;

// ═════════════════════════════════════════════════════════════════════════════
// 1. FACTOR DISCOVERY
// ═════════════════════════════════════════════════════════════════════════════
void QuantModulePanel::display_factor_discovery_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = ui::colors::legible(module_.color);
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
            gs_make_card(tr("RECORDS"), QString::number(total), this),
            gs_make_card(tr("INSTRUMENTS"), QString::number(instruments.size()), this),
            gs_make_card(tr("FIELDS"), QString::number(fields.size()), this),
            gs_make_card(tr("FREQUENCY"), freq.toUpper(), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> range_row = {
            gs_make_card(tr("START DATE"), range.value("start").toString("—"), this),
            gs_make_card(tr("END DATE"), range.value("end").toString("—"), this),
            gs_make_card(tr("WARNING"), warning.isEmpty() ? tr("NONE") : tr("PRESENT"), this,
                         warning.isEmpty() ? ui::colors::SUCCESS() : ui::colors::WARNING()),
            gs_make_card(tr("STATUS"), tr("OK"), this, ui::colors::SUCCESS()),
        };
        results_layout_->addWidget(gs_card_row(range_row, this));

        if (!warning.isEmpty()) {
            auto* lbl = new QLabel("⚠ " + warning);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(
                QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; padding:8px 10px; background:%2; border-left:3px solid %3;")
                    .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::WARNING()));
            results_layout_->addWidget(lbl);
        }

        // Instruments list (compact)
        if (!instruments.isEmpty()) {
            QStringList names;
            for (const auto& v : instruments)
                names << v.toString();
            const int show = std::min<int>(20, names.size());
            QString text = tr("Instruments (%1): %2").arg(names.size()).arg(names.mid(0, show).join(", "));
            if (names.size() > show)
                text += tr(" … +%1 more").arg(names.size() - show);
            auto* lbl = new QLabel(text);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }

        status_label_->setText(
            tr("%1 records  %2→%3").arg(total).arg(range.value("start").toString()).arg(range.value("end").toString()));
        return;
    }

    // ── get_instruments ──────────────────────────────────────────────────────
    if (command == "get_instruments") {
        const auto instruments = payload.value("instruments").toArray();
        const QString market = payload.value("market").toString();
        const int count = payload.value("count").toInt(instruments.size());

        QList<QWidget*> top = {
            gs_make_card(tr("MARKET"), market.toUpper().isEmpty() ? "—" : market.toUpper(), this, ui::colors::INFO()),
            gs_make_card(tr("COUNT"), QString::number(count), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Grid of instrument symbols, 4 per row, in the order returned. The columns are
        // not fields, so no header row ("Symbol | Symbol | ..."); the count is in the caption.
        if (!instruments.isEmpty()) {
            const int rows = instruments.size();
            const int cols = 4;
            const int table_rows = (rows + cols - 1) / cols;
            results_layout_->addWidget(qc_caption(tr("INSTRUMENTS  |  %1").arg(rows)));
            auto* table = new QTableWidget(table_rows, cols, this);
            table->horizontalHeader()->setVisible(false);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(table_rows * row_px(22) + row_px(32), 360));
            for (int i = 0; i < instruments.size(); ++i) {
                const int r = i / cols;
                const int c = i % cols;
                auto* it = new QTableWidgetItem(instruments[i].toString());
                it->setTextAlignment(Qt::AlignCenter);
                table->setItem(r, c, it);
                if (c == 0)
                    table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 instrument(s)  market=%2").arg(count).arg(market));
        return;
    }

    // ── get_calendar ─────────────────────────────────────────────────────────
    if (command == "get_calendar") {
        const auto dates = payload.value("dates").toArray();
        const int count = payload.value("count").toInt(dates.size());
        const QString freq = payload.value("freq").toString("day");

        QList<QWidget*> top = {
            gs_make_card(tr("DATES"), QString::number(count), this),
            gs_make_card(tr("FREQUENCY"), freq.toUpper(), this, ui::colors::INFO()),
            gs_make_card(tr("FIRST"), dates.isEmpty() ? "—" : dates.first().toString(), this),
            gs_make_card(tr("LAST"), dates.isEmpty() ? "—" : dates.last().toString(), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // First 5 and last 5 trading days (all of them when there are 10 or fewer); the old
        // evenly spaced sample never reached the last date the LAST card quotes.
        if (!dates.isEmpty()) {
            QVector<int> idx;
            for (int i = 0; i < std::min<int>(5, dates.size()); ++i)
                idx << i;
            for (int i = std::max<int>(5, static_cast<int>(dates.size()) - 5); i < dates.size(); ++i)
                idx << i;
            const int rows = idx.size();
            results_layout_->addWidget(qc_caption(
                rows < dates.size() ? tr("FIRST 5 AND LAST 5 OF %1 DATES").arg(dates.size())
                                    : tr("ALL %1 DATES").arg(dates.size())));
            auto* table = new QTableWidget(rows, 2, this);
            table->setHorizontalHeaderLabels({tr("Index"), tr("Date")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * row_px(22) + row_px(32));
            for (int r = 0; r < rows; ++r) {
                const int i = idx.at(r);
                table->setItem(r, 0, new QTableWidgetItem(QString::number(i)));
                table->setItem(r, 1, new QTableWidgetItem(dates[i].toString()));
                table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 trading day(s)").arg(count));
        return;
    }

    // ── get_factor_library (legacy shape — Python may not implement it) ──────
    if (command == "get_factor_library") {
        const auto factors = payload.value("factors").toArray();
        QList<QWidget*> top = {
            gs_make_card(tr("FACTORS"), QString::number(factors.size()), this,
                         factors.isEmpty() ? ui::colors::WARNING() : ui::colors::SUCCESS()),
            gs_make_card(tr("STATUS"), factors.isEmpty() ? tr("EMPTY") : tr("LOADED"), this,
                         factors.isEmpty() ? ui::colors::WARNING() : ui::colors::SUCCESS()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!factors.isEmpty()) {
            auto* table = new QTableWidget(factors.size(), 3, this);
            table->setHorizontalHeaderLabels({tr("Name"), tr("Category"), tr("Expression")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(factors.size() * row_px(26) + row_px(32), 360));
            for (int i = 0; i < factors.size(); ++i) {
                const auto f = factors[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(f.value("name").toString()));
                table->setItem(i, 1, new QTableWidgetItem(f.value("category").toString("—")));
                table->setItem(i, 2, new QTableWidgetItem(f.value("expression").toString("—")));
                table->setRowHeight(i, row_px(26));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 factor(s)").arg(factors.size()));
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

    const QString accent = ui::colors::legible(module_.color);
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
            gs_make_card(tr("TOTAL"), QString::number(total), this),
            gs_make_card(tr("AVAILABLE"), QString::number(avail), this,
                         avail > 0 ? ui::colors::SUCCESS() : ui::colors::WARNING()),
            gs_make_card(tr("TYPES"), QString::number(types.size()), this),
            gs_make_card(tr("UNAVAILABLE"), QString::number(total - avail), this,
                         (total - avail) > 0 ? ui::colors::WARNING() : ui::colors::SUCCESS()),
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
            table->setHorizontalHeaderLabels({tr("ID"), tr("Name"), tr("Type"), tr("Available")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(models.size() * row_px(26) + row_px(32), 360));
            for (int i = 0; i < models.size(); ++i) {
                const auto m = models[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(m.value("id").toString().toUpper()));
                table->setItem(i, 1, new QTableWidgetItem(m.value("name").toString()));
                QString t = m.value("type").toString();
                t.replace('_', ' ');
                table->setItem(i, 2, new QTableWidgetItem(t.toUpper()));
                const bool ok = m.value("available").toBool();
                auto* a = new QTableWidgetItem(ok ? tr("YES") : tr("NO"));
                a->setForeground(QColor(ok ? ui::colors::SUCCESS() : ui::colors::DANGER()));
                a->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 3, a);
                table->setRowHeight(i, row_px(26));
            }
            finish_table(table);
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 model(s)  %2 available").arg(total).arg(avail));
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
            gs_make_card(tr("QLIB"), qlib ? tr("READY") : tr("MISSING"), this,
                         qlib ? ui::colors::SUCCESS() : ui::colors::DANGER()),
            gs_make_card(tr("VERSION"), version, this, ui::colors::INFO()),
            gs_make_card(tr("HANDLERS"), QString::number(handlers.size()), this),
            gs_make_card(tr("STRATEGIES"), QString::number(strategies.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> models = {
            gs_make_card(tr("MODELS AVAIL"), QString::number(n_avail), this,
                         n_avail > 0 ? ui::colors::SUCCESS() : ui::colors::WARNING()),
            gs_make_card(tr("MODELS MISSING"), QString::number(n_unavail), this,
                         n_unavail > 0 ? ui::colors::WARNING() : ui::colors::SUCCESS()),
            gs_make_card(tr("MODELS TOTAL"), QString::number(avail.size()), this),
            gs_make_card(tr("STATUS"), qlib ? tr("OPERATIONAL") : tr("DEGRADED"), this,
                         qlib ? ui::colors::SUCCESS() : ui::colors::DANGER()),
        };
        results_layout_->addWidget(gs_card_row(models, this));

        // Models availability table
        if (!avail.isEmpty()) {
            auto* table = new QTableWidget(avail.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Model"), tr("Status")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(avail.size() * row_px(24) + row_px(32), 320));
            int r = 0;
            for (auto it = avail.begin(); it != avail.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                const bool ok = it.value().toBool();
                auto* st = new QTableWidgetItem(ok ? tr("AVAILABLE") : tr("MISSING"));
                st->setForeground(QColor(ok ? ui::colors::SUCCESS() : ui::colors::DANGER()));
                st->setTextAlignment(Qt::AlignCenter);
                table->setItem(r, 1, st);
                table->setRowHeight(r, row_px(24));
            }
            results_layout_->addWidget(table);
        }

        if (!handlers.isEmpty()) {
            QStringList lst;
            for (const auto& v : handlers)
                lst << v.toString();
            auto* lbl = new QLabel(tr("Handlers: %1").arg(lst.join(", ")));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }

        status_label_->setText(qlib ? tr("Qlib %1 ready  %2 model(s)").arg(version).arg(n_avail)
                                    : tr("Qlib unavailable"));
        return;
    }

    // ── download_data ────────────────────────────────────────────────────────
    // {region, target_dir, already_present, calendar_start, calendar_end, trading_days, message}
    if (command == "download_data") {
        const bool already = payload.value("already_present").toBool();
        const QString first = payload.value("calendar_start").toString();
        const QString last = payload.value("calendar_end").toString();
        QList<QWidget*> top = {
            gs_make_card(tr("REGION"), payload.value("region").toString().toUpper(), this),
            gs_make_card(tr("DATASET"), already ? tr("ALREADY PRESENT") : tr("DOWNLOADED"), this,
                         ui::colors::SUCCESS()),
            gs_make_card(tr("CALENDAR"),
                         first.isEmpty() ? QString::fromUtf8("—") : tr("%1 → %2").arg(first, last), this),
            gs_make_card(tr("TRADING DAYS"), fmt_int_safe(payload.value("trading_days")), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        auto* path_lbl = gs_section_header(tr("Stored in %1").arg(payload.value("target_dir").toString()), accent);
        path_lbl->setWordWrap(true);
        path_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        results_layout_->addWidget(path_lbl);
        status_label_->setText(payload.value("message").toString(tr("Qlib dataset ready")));
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
            gs_make_card(tr("MODEL ID"), model_id, this),
            gs_make_card(tr("TYPE"), model_type.toUpper(), this),
            gs_make_card(tr("HANDLER"), handler.isEmpty() ? "—" : handler, this, ui::colors::INFO()),
            gs_make_card(tr("PREDICTIONS"), n_pred > 0 ? QString::number(n_pred) : "—", this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!train.isEmpty() || !val.isEmpty()) {
            QList<QWidget*> periods = {
                gs_make_card(tr("TRAIN START"), train.value("start").toString("—"), this),
                gs_make_card(tr("TRAIN END"), train.value("end").toString("—"), this),
                gs_make_card(tr("VAL START"), val.value("start").toString("—"), this),
                gs_make_card(tr("VAL END"), val.value("end").toString("—"), this),
            };
            results_layout_->addWidget(gs_card_row(periods, this));
        }

        // Render metrics dict (legacy) if present
        if (!metrics.isEmpty()) {
            results_layout_->addWidget(gs_section_header(tr("METRICS"), accent));
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
            results_layout_->addWidget(gs_section_header(tr("CONFIG"), accent));
            auto* table = new QTableWidget(config.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Hyperparameter"), tr("Value")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(config.size() * row_px(24) + row_px(32), 280));
            int r = 0;
            for (auto it = config.begin(); it != config.end(); ++it, ++r) {
                table->setItem(r, 0, new QTableWidgetItem(it.key().toUpper()));
                auto* vi = new QTableWidgetItem(format_val(it.value()));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 1, vi);
                table->setRowHeight(r, row_px(24));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(payload.value("message").toString(tr("Trained: %1").arg(model_id)));
        return;
    }

    // ── run_backtest ─────────────────────────────────────────────────────────
    // Metrics are a nested object, which the generic key/value table skips; render them as cards.
    if (command == "run_backtest") {
        const auto m = payload.value("metrics").toObject();
        const auto period = payload.value("period").toObject();
        auto pct = [&m](const char* key, int decimals = 2) {
            const QJsonValue v = m.value(QLatin1String(key));
            return v.isNull() || v.isUndefined() ? QString::fromUtf8("—") : gs_fmt_pct(v.toDouble(), decimals);
        };
        auto num = [&m](const char* key, int decimals = 3) {
            const QJsonValue v = m.value(QLatin1String(key));
            return v.isNull() || v.isUndefined() ? QString::fromUtf8("—") : gs_fmt_num(v.toDouble(), decimals);
        };
        const double total_ret = m.value("total_return").toDouble();
        const double sharpe = m.value("sharpe_ratio").toDouble();

        QList<QWidget*> row1 = {
            gs_make_card(tr("TOTAL RETURN"), pct("total_return"), this, gs_pos_neg_color(total_ret)),
            gs_make_card(tr("ANN. RETURN"), pct("annualized_return"), this,
                         gs_pos_neg_color(m.value("annualized_return").toDouble())),
            gs_make_card(tr("SHARPE"), num("sharpe_ratio"), this,
                         sharpe >= 1.0 ? ui::colors::POSITIVE()
                         : sharpe > 0  ? ui::colors::WARNING()
                                       : ui::colors::NEGATIVE()),
            gs_make_card(tr("MAX DRAWDOWN"), pct("max_drawdown"), this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(row1, this));

        QList<QWidget*> row2 = {
            gs_make_card(tr("VOLATILITY"), pct("volatility"), this),
            gs_make_card(tr("WIN RATE"), pct("win_rate", 1), this),
            gs_make_card(tr("EXCESS ANN. RETURN"), pct("excess_annualized_return"), this,
                         gs_pos_neg_color(m.value("excess_annualized_return").toDouble())),
            gs_make_card(tr("INFO RATIO"), num("information_ratio"), this),
        };
        results_layout_->addWidget(gs_card_row(row2, this));

        QList<QWidget*> row3 = {
            gs_make_card(tr("SORTINO"), num("sortino_ratio"), this),
            gs_make_card(tr("CALMAR"), num("calmar_ratio"), this),
            gs_make_card(tr("AVG DAILY TURNOVER"), pct("avg_daily_turnover", 1), this),
        };
        results_layout_->addWidget(gs_card_row(row3, this));

        // Beta to the benchmark (null without one), total trading cost (sum of the daily cost
        // as a fraction of the account), trading days — returned by the script, shown nowhere.
        const QString bench = payload.value("benchmark").toString();
        const QJsonValue beta = m.value("beta");
        QList<QWidget*> row4 = {
            gs_make_card(tr("BETA"), beta.isNull() || beta.isUndefined() ? QString::fromUtf8("—") : num("beta"), this),
            gs_make_card(tr("TOTAL COST"), pct("total_cost", 2), this,
                         m.value("total_cost").toDouble() > 0 ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("TRADING DAYS"), QString::number(m.value("trading_days").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(row4, this));

        // Equity curve: growth of 1 after costs, with the benchmark's growth of 1 when one was
        // given (without one the script back-tests against a flat zero series, which is not
        // drawn), and the drawdown of the plotted curve below.
        const QJsonArray curve = payload.value("equity_curve").toArray();
        if (!curve.isEmpty()) {
            const XAxis x = qc_x_axis(curve, {QStringLiteral("date")});
            const QVector<double> port = qc_column(curve, QStringLiteral("portfolio"));
            const QVector<double> bm = bench.isEmpty() ? QVector<double>{} : qc_column(curve, QStringLiteral("benchmark"));
            results_layout_->addWidget(
                qc_equity_chart(tr("EQUITY CURVE  ·  GROWTH OF 1, AFTER COSTS"), x, port, bm, qc_fmt_num(3), bench, this));
        }
        QStringList universe;
        for (const auto& v : payload.value("universe").toArray())
            universe << v.toString().toUpper();
        auto* lbl = new QLabel(tr("%1  |  top-%2, drop %3  |  %4 → %5  |  benchmark: %6  |  universe (%7): %8")
                                   .arg(payload.value("model_id").toString())
                                   .arg(payload.value("topk").toInt())
                                   .arg(payload.value("n_drop").toInt())
                                   .arg(period.value("start").toString(), period.value("end").toString(),
                                        bench.isEmpty() ? tr("none") : bench)
                                   .arg(universe.size())
                                   .arg(universe.isEmpty() ? QString::fromUtf8("—") : universe.join(", ")));
        lbl->setWordWrap(true);
        results_layout_->addWidget(lbl);
        status_label_->setText(tr("Backtest: return %1  |  Sharpe %2").arg(pct("total_return"), num("sharpe_ratio", 2)));
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

    const QString accent = ui::colors::legible(module_.color);
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
            gs_make_card(tr("RECORDS"), QString::number(total), this),
            gs_make_card(tr("INSTRUMENTS"), QString::number(instruments.size()), this),
            gs_make_card(tr("FIELDS"), QString::number(fields.size()), this),
            gs_make_card(tr("FREQUENCY"), freq.toUpper(), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> range_row = {
            gs_make_card(tr("START DATE"), range.value("start").toString("—"), this),
            gs_make_card(tr("END DATE"), range.value("end").toString("—"), this),
            gs_make_card(tr("WARNING"), warning.isEmpty() ? tr("NONE") : tr("PRESENT"), this,
                         warning.isEmpty() ? ui::colors::SUCCESS() : ui::colors::WARNING()),
            gs_make_card(tr("STATUS"), tr("OK"), this, ui::colors::SUCCESS()),
        };
        results_layout_->addWidget(gs_card_row(range_row, this));

        if (!warning.isEmpty()) {
            auto* lbl = new QLabel("⚠ " + warning);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(
                QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; padding:8px 10px; background:%2; border-left:3px solid %3;")
                    .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::WARNING()));
            results_layout_->addWidget(lbl);
        }

        if (!instruments.isEmpty()) {
            QStringList names;
            for (const auto& v : instruments)
                names << v.toString();
            const int show = std::min<int>(20, names.size());
            QString text = tr("Instruments (%1): %2").arg(names.size()).arg(names.mid(0, show).join(", "));
            if (names.size() > show)
                text += tr(" … +%1 more").arg(names.size() - show);
            auto* lbl = new QLabel(text);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                       "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText(
            tr("%1 records  %2→%3").arg(total).arg(range.value("start").toString()).arg(range.value("end").toString()));
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
                gs_make_card(tr("MODEL"), model_id.isEmpty() ? "—" : model_id, this),
                gs_make_card(tr("TYPE"), analysis.toUpper().isEmpty() ? "—" : analysis.toUpper(), this,
                             ui::colors::INFO()),
                gs_make_card(tr("IC MEAN"), QString::number(ic_mean, 'f', 4), this, gs_pos_neg_color(ic_mean)),
                gs_make_card(tr("ICIR"), QString::number(icir, 'f', 3), this,
                             icir >= 0.5 ? ui::colors::POSITIVE()
                             : icir > 0  ? ui::colors::WARNING()
                                         : ui::colors::NEGATIVE()),
            };
            results_layout_->addWidget(gs_card_row(top, this));

            QList<QWidget*> rank = {
                gs_make_card(tr("IC STD"), QString::number(results.value("IC_std").toDouble(), 'f', 4), this),
                gs_make_card(tr("RANK IC MEAN"), QString::number(rank_ic, 'f', 4), this, gs_pos_neg_color(rank_ic)),
                gs_make_card(tr("RANK IC STD"), QString::number(results.value("Rank_IC_std").toDouble(), 'f', 4), this),
                gs_make_card(tr("RANK ICIR"), QString::number(rank_icir, 'f', 3), this,
                             rank_icir >= 0.5 ? ui::colors::POSITIVE()
                             : rank_icir > 0  ? ui::colors::WARNING()
                                              : ui::colors::NEGATIVE()),
            };
            results_layout_->addWidget(gs_card_row(rank, this));
            status_label_->setText(
                tr("IC=%1 ICIR=%2 RankIC=%3").arg(ic_mean, 0, 'f', 4).arg(icir, 0, 'f', 3).arg(rank_ic, 0, 'f', 4));
            return;
        }

        // Factor-library table: factors = [{name, ic, sharpe(=ICIR)}]. Produced both by the
        // universe/date-range analysis (analysis_type "factor_ic", ranked by |rank IC|) and by the
        // legacy per-model shape.
        const auto factors = payload.value("factors").toArray();
        const bool factor_ic = analysis == QLatin1String("factor_ic");
        const auto range = payload.value("date_range").toObject();
        QList<QWidget*> top = {
            gs_make_card(factor_ic ? tr("TOP FACTORS") : tr("FACTORS"), QString::number(factors.size()), this),
            gs_make_card(factor_ic ? tr("FACTORS TESTED") : tr("MODEL"),
                         factor_ic ? QString::number(payload.value("n_factors").toInt())
                                   : (model_id.isEmpty() ? QStringLiteral("—") : model_id),
                         this),
        };
        if (factor_ic) {
            top << gs_make_card(tr("TRADING DAYS"), QString::number(payload.value("n_days").toInt()), this)
                << gs_make_card(tr("PERIOD"), tr("%1 → %2").arg(range.value("start").toString(),
                                                              range.value("end").toString()),
                                this);
        }
        results_layout_->addWidget(gs_card_row(top, this));

        if (!factors.isEmpty()) {
            auto* table = new QTableWidget(factors.size(), 3, this);
            table->setHorizontalHeaderLabels({tr("Factor"), tr("IC"), factor_ic ? tr("ICIR") : tr("Sharpe")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(factors.size() * row_px(24) + row_px(32), 320));
            for (int i = 0; i < factors.size(); ++i) {
                const auto f = factors[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(f.value("name").toString("—")));
                const QJsonValue ic = f.value("ic");
                if (ic.isNull()) {
                    auto* ici = new QTableWidgetItem(tr("N/A"));
                    ici->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 1, ici);
                } else {
                    table->setItem(i, 1, num_item(QString::number(ic.toDouble(), 'f', 4), ic.toDouble(),
                                                  gs_pos_neg_color(ic.toDouble())));
                }
                const QJsonValue sh = f.value("sharpe");
                if (sh.isNull()) {
                    auto* shi = new QTableWidgetItem(tr("N/A"));
                    shi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, 2, shi);
                } else {
                    table->setItem(i, 2, num_item(QString::number(sh.toDouble(), 'f', 3), sh.toDouble(),
                                                  gs_pos_neg_color(sh.toDouble())));
                }
                table->setRowHeight(i, row_px(24));
            }
            // IC as a signed bar in its cell: strength and sign compare down the column.
            table->setItemDelegateForColumn(1, new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, table));
            finish_table(table);
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 factor(s)").arg(factors.size()));
        return;
    }

    // ── predict ──────────────────────────────────────────────────────────────
    // {model_id, model_type, handler, period{start,end}, latest_date,
    //  latest_ranking[{rank, instrument, score}], predictions[{date, instrument, score}], prediction_count, note}
    if (command == "predict") {
        const auto ranking = payload.value("latest_ranking").toArray();
        const auto period = payload.value("period").toObject();
        QList<QWidget*> top = {
            gs_make_card(tr("MODEL"), payload.value("model_id").toString(), this),
            gs_make_card(tr("TYPE"), payload.value("model_type").toString().toUpper(), this),
            gs_make_card(tr("SIGNAL DATE"), payload.value("latest_date").toString(QString::fromUtf8("—")), this),
            gs_make_card(tr("INSTRUMENTS"), QString::number(ranking.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));
        results_layout_->addWidget(gs_section_header(
            tr("RANKING ON %1  |  window %2 → %3  |  %4 score(s)")
                .arg(payload.value("latest_date").toString(), period.value("start").toString(),
                     period.value("end").toString())
                .arg(payload.value("prediction_count").toInt()),
            accent));
        if (!ranking.isEmpty()) {
            auto* table = new QTableWidget(ranking.size(), 3, this);
            table->setHorizontalHeaderLabels({tr("Rank"), tr("Instrument"), tr("Score")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(static_cast<int>(ranking.size()) * row_px(24) + row_px(32), row_px(360)));
            for (int i = 0; i < ranking.size(); ++i) {
                const auto r = ranking[i].toObject();
                auto* rank = num_item(QString::number(r.value("rank").toInt()), r.value("rank").toInt());
                rank->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 0, rank);
                table->setItem(i, 1, new QTableWidgetItem(r.value("instrument").toString().toUpper()));
                const double sc = r.value("score").toDouble();
                table->setItem(i, 2, num_item(QString::number(sc, 'f', 5), sc));
                table->setRowHeight(i, row_px(24));
            }
            // Score as a signed bar: the relative strength across instruments is the signal.
            table->setItemDelegateForColumn(2, new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, table));
            finish_table(table);
            results_layout_->addWidget(table);
        }
        const QString note_text = payload.value("note").toString();
        if (!note_text.isEmpty()) {
            auto* note = new QLabel(note_text);
            note->setWordWrap(true);
            note->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; padding:6px 10px;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
            results_layout_->addWidget(note);
        }
        status_label_->setText(tr("%1 instrument(s) ranked on %2")
                                   .arg(ranking.size())
                                   .arg(payload.value("latest_date").toString()));
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
            gs_make_card(tr("MODEL"), model_id.isEmpty() ? "—" : model_id, this),
            gs_make_card(tr("FEATURES"), QString::number(sorted.size()), this),
            gs_make_card(tr("TOP FEATURE"), top_feat, this, ui::colors::INFO()),
            gs_make_card(tr("TOP IMPORTANCE"), QString::number(top_val, 'f', 4), this, gs_pos_neg_color(top_val)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!sorted.isEmpty()) {
            auto* table = new QTableWidget(sorted.size(), 3, this);
            table->setHorizontalHeaderLabels({tr("Rank"), tr("Feature"), tr("Importance")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(sorted.size() * row_px(24) + row_px(32), 360));
            for (int i = 0; i < sorted.size(); ++i) {
                auto* r = num_item(QString::number(i + 1), i + 1, i == 0 ? QString(ui::colors::POSITIVE()) : QString());
                r->setTextAlignment(Qt::AlignCenter);
                table->setItem(i, 0, r);
                table->setItem(i, 1, new QTableWidgetItem(sorted[i].first));
                const double v = sorted[i].second;
                table->setItem(i, 2, num_item(QString::number(v, 'f', 6), v, gs_pos_neg_color(v)));
                table->setRowHeight(i, row_px(24));
            }
            // Importance as a bar in its cell (signed when any importance is negative).
            const bool any_neg = std::any_of(sorted.begin(), sorted.end(), [](const auto& p) { return p.second < 0; });
            table->setItemDelegateForColumn(
                2, new ui::BarCellDelegate(any_neg ? ui::BarCellDelegate::Mode::SignedBar : ui::BarCellDelegate::Mode::Bar,
                                           table));
            finish_table(table);
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 feature(s)  top=%2").arg(sorted.size()).arg(top_feat));
        return;
    }

    display_result(payload);
}

} // namespace fincept::screens
