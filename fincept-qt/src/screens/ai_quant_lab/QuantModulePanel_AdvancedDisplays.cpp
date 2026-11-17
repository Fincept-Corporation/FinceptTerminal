// src/screens/ai_quant_lab/QuantModulePanel_AdvancedDisplays.cpp
//
// Card-based result renderers for the ADVANCED-category panels.
// Mirrors the analytics pattern (gs_make_card / gs_card_row / gs_section_header).
// Replaces the generic display_result() flat-table fallback for these modules.
// (hft and rolling_retraining render into their own in-tab widgets from
// on_result() in QuantModulePanel_Results.cpp; their unused card renderers that
// lived here were removed.)
//
//   - advanced_models (train_model)
//   - feature_engineering (per-indicator, select_features_by_ic, evaluate_expression)
//   - portfolio_opt (BL, HRP, MinVar, MaxSharpe, EF)
//   - factor_evaluation (calculate_ic, generate_evaluation_report, calculate_risk_metrics)
//   - strategy_builder (create_*, portfolio_metrics)
//   - data_processors (list_processors, create_pipeline, process_data)
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

// ═════════════════════════════════════════════════════════════════════════════
// 3. ADVANCED MODELS
// ═════════════════════════════════════════════════════════════════════════════

void QuantModulePanel::display_data_processors_result(const QString& command, const QJsonObject& payload) {
    clear_results();
    if (!check_success(payload, [this](const QString& s) { display_error(s); }))
        return;

    const QString accent = ui::colors::legible(module_.color);
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    results_layout_->addWidget(gs_section_header(header_text, accent));

    if (command == "list_processors") {
        const auto procs = payload.value("processors").toArray();
        const auto descs = payload.value("descriptions").toObject();

        QList<QWidget*> top = {
            gs_make_card(tr("AVAILABLE"), QString::number(procs.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!procs.isEmpty()) {
            auto* table = new QTableWidget(procs.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Processor"), tr("Description")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(procs.size() * row_px(26) + row_px(32), 360));
            for (int i = 0; i < procs.size(); ++i) {
                const QString name = procs[i].toString();
                table->setItem(i, 0, new QTableWidgetItem(name.toUpper()));
                table->setItem(i, 1, new QTableWidgetItem(descs.value(name).toString()));
                table->setRowHeight(i, row_px(26));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 processor(s)").arg(procs.size()));
        return;
    }

    if (command == "create_pipeline") {
        const auto procs = payload.value("processors").toArray();
        QStringList plist;
        for (const auto& v : procs)
            plist << v.toString();

        QList<QWidget*> top = {
            gs_make_card(tr("PIPELINE ID"), payload.value("pipeline_id").toString(), this),
            gs_make_card(tr("PROCESSORS"), QString::number(payload.value("num_processors").toInt()), this),
            gs_make_card(tr("STATUS"), tr("CREATED"), this, ui::colors::SUCCESS()),
            gs_make_card(tr("STAGES"), QString::number(plist.size()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        if (!plist.isEmpty()) {
            auto* lbl = new QLabel(tr("Pipeline order: %1").arg(plist.join("  →  ")));
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                       "padding:8px 10px; background:%2; border-left:3px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText(tr("Pipeline created"));
        return;
    }

    if (command == "process_data") {
        const auto stats = payload.value("statistics").toObject();
        const auto in_shape = stats.value("input_shape").toArray();
        const auto out_shape = stats.value("output_shape").toArray();
        auto shape_str = [](const QJsonArray& a) {
            QStringList s;
            for (const auto& v : a)
                s << QString::number(v.toInt());
            return "(" + s.join(", ") + ")";
        };

        QList<QWidget*> top = {
            gs_make_card(tr("PIPELINE"), payload.value("pipeline_id").toString(), this),
            gs_make_card(tr("INPUT SHAPE"), shape_str(in_shape), this),
            gs_make_card(tr("OUTPUT SHAPE"), shape_str(out_shape), this),
            gs_make_card(tr("ROWS DROPPED"),
                         in_shape.size() > 0 && out_shape.size() > 0
                             ? QString::number(in_shape[0].toInt() - out_shape[0].toInt())
                             : "—",
                         this, ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> nulls = {
            gs_make_card(tr("INPUT NULLS"), QString::number(stats.value("input_nulls").toInt()), this,
                         ui::colors::WARNING()),
            gs_make_card(tr("OUTPUT NULLS"), QString::number(stats.value("output_nulls").toInt()), this,
                         stats.value("output_nulls").toInt() == 0 ? ui::colors::SUCCESS() : ui::colors::WARNING()),
            gs_make_card(tr("INPUT MEAN"), QString::number(stats.value("input_mean").toDouble(), 'f', 4), this),
            gs_make_card(tr("OUTPUT MEAN"), QString::number(stats.value("output_mean").toDouble(), 'f', 4), this),
        };
        results_layout_->addWidget(gs_card_row(nulls, this));

        QList<QWidget*> std_row = {
            gs_make_card(tr("INPUT STD"), QString::number(stats.value("input_std").toDouble(), 'f', 4), this),
            gs_make_card(tr("OUTPUT STD"), QString::number(stats.value("output_std").toDouble(), 'f', 4), this,
                         std::abs(stats.value("output_std").toDouble() - 1.0) < 0.1 ? ui::colors::SUCCESS()
                                                                                    : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("STD RATIO"),
                         stats.value("input_std").toDouble() > 0
                             ? QString::number(
                                   stats.value("output_std").toDouble() / stats.value("input_std").toDouble(), 'f', 3)
                             : "—",
                         this),
            gs_make_card(
                tr("DROPPED %"),
                in_shape.size() > 0 && in_shape[0].toInt() > 0
                    ? QString::number((1.0 - double(out_shape[0].toInt()) / in_shape[0].toInt()) * 100, 'f', 2) + "%"
                    : "—",
                this),
        };
        results_layout_->addWidget(gs_card_row(std_row, this));

        // The processed values themselves (column -> {row -> value}). Only the shape and summary
        // statistics were shown, so the point of the pipeline -- the transformed data -- was invisible.
        const auto processed = payload.value("processed_data").toObject();
        if (!processed.isEmpty()) {
            const QStringList cols = processed.keys();
            QStringList row_keys = processed.value(cols.first()).toObject().keys();
            std::sort(row_keys.begin(), row_keys.end(),
                      [](const QString& a, const QString& b) { return a.toLongLong() < b.toLongLong(); });
            const int shown = static_cast<int>(qMin<qsizetype>(row_keys.size(), 15));
            results_layout_->addWidget(gs_section_header(
                tr("PROCESSED DATA  |  FIRST %1 OF %2 ROWS").arg(shown).arg(row_keys.size()), accent));
            auto* table = new QTableWidget(shown, static_cast<int>(cols.size()), this);
            table->setHorizontalHeaderLabels(cols);
            table->setVerticalHeaderLabels(row_keys.mid(0, shown));
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            // Styled by the panel-level table rules (refresh_theme), not an inline setStyleSheet.
            table->setMaximumHeight(shown * row_px(24) + row_px(40));
            for (int r = 0; r < shown; ++r) {
                for (int c = 0; c < cols.size(); ++c) {
                    const QJsonValue v = processed.value(cols[c]).toObject().value(row_keys[r]);
                    auto* it = new QTableWidgetItem(v.isNull() || v.isUndefined()
                                                        ? QString::fromUtf8("—")
                                                        : QString::number(v.toDouble(), 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, c, it);
                }
                table->setRowHeight(r, row_px(24));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("Processed: %1 → %2").arg(shape_str(in_shape)).arg(shape_str(out_shape)));
        return;
    }

    display_result(payload);
}

} // namespace fincept::screens
