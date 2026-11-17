// src/screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h
//
// Private header for the QuantModulePanel ADVANCED-category display TUs.
// Each helper is `inline` so this header can be included from multiple
// split files without violating ODR. Mirrors the QuantModulePanel_GsHelpers
// pattern.

#pragma once

#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QCoreApplication>
#include <QHeaderView>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>

namespace fincept::screens::quant_advanced_helpers {

inline QString fmt_pct_safe(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    return QString::number(v.toDouble() * 100.0, 'f', decimals) + "%";
}

/// A value that is already in percent points (12.5 → "12.50%"); null → "—".
inline QString fmt_pts_safe(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals) + "%";
}

inline QString fmt_num_safe(const QJsonValue& v, int decimals = 4) {
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals);
}

inline QString fmt_int_safe(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    return QString::number(v.toInt());
}

inline QString fmt_bps(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals) + " bps";
}

inline QString verdict_color_for(const QString& v) {
    const QString u = v.toUpper();
    if (u == "STRONG" || u == "GOOD" || u == "EXCELLENT" || u == "CLEAN" || u == "NORMAL" || u == "BUY")
        return fincept::ui::colors::POSITIVE();
    if (u == "MODERATE" || u == "MAINTAIN" || u == "FAIR" || u == "MIXED")
        return fincept::ui::colors::TEXT_PRIMARY();
    if (u == "WEAK" || u == "WARNING" || u == "ELEVATED" || u == "NEUTRAL")
        return fincept::ui::colors::WARNING();
    if (u == "TOXIC" || u == "POOR" || u == "AVOID" || u == "SELL")
        return fincept::ui::colors::NEGATIVE();
    return fincept::ui::colors::TEXT_PRIMARY();
}

// Returns false if the payload was an error (already displayed via callback) — caller should bail.
//
// Single implementation for every QuantModulePanel display TU. It used to be
// copy-pasted into anonymous namespaces in CoreDisplays.cpp and
// AIMLDisplays.cpp; those copies were translated while this one was not, so the
// same message appeared translated on some panels and not on others.
inline bool check_success(const QJsonObject& payload, const std::function<void(const QString&)>& display_error_fn) {
    if (!payload.value("success").toBool(false)) {
        const QString err =
            payload.value("error").toString(QCoreApplication::translate("QuantModulePanel", "Unknown error"));
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? QCoreApplication::translate("QuantModulePanel", "Input error: ")
                               : kind == "runtime"
                                   ? QCoreApplication::translate("QuantModulePanel", "Computation failed: ")
                                   : QString();
        display_error_fn(prefix + err);
        return false;
    }
    return true;
}

// Render a {asset → weight} dict as a sorted-by-weight table.
inline QTableWidget* build_weights_table(const QJsonObject& weights, QWidget* parent, int max_height = 320) {
    QList<QPair<QString, double>> rows;
    for (auto it = weights.begin(); it != weights.end(); ++it)
        rows.append({it.key(), it.value().toDouble()});
    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return std::abs(a.second) > std::abs(b.second); });

    auto* table = new QTableWidget(rows.size(), 2, parent);
    // These two were the only untranslated column headers in the panel family.
    table->setHorizontalHeaderLabels({QCoreApplication::translate("QuantModulePanel", "Asset"),
                                      QCoreApplication::translate("QuantModulePanel", "Weight")});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setStyleSheet(fincept::screens::quant_styles::table_ss());
    table->setMaximumHeight(qMin(rows.size() * fincept::screens::quant_styles::row_px(24) + fincept::screens::quant_styles::row_px(32), max_height));
    bool any_short = false;
    for (int r = 0; r < rows.size(); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(rows[r].first));
        const double w = rows[r].second;
        any_short = any_short || w < -1e-9;
        const QString fg = w > 0.001    ? QString(fincept::ui::colors::POSITIVE())
                           : w < -0.001 ? QString(fincept::ui::colors::NEGATIVE())
                                        : QString();
        table->setItem(r, 1, fincept::screens::quant_styles::num_item(QString::number(w * 100.0, 'f', 3) + "%", w, fg));
        table->setRowHeight(r, fincept::screens::quant_styles::row_px(24));
    }
    // Relative size at a glance: a bar in the weight cell (from the left for a long-only
    // book, from a centre line when there are shorts). The delegate reads the "%" text.
    table->setItemDelegateForColumn(
        1, new fincept::ui::BarCellDelegate(any_short ? fincept::ui::BarCellDelegate::Mode::SignedBar
                                                      : fincept::ui::BarCellDelegate::Mode::Bar,
                                            table));
    fincept::screens::quant_styles::finish_table(table);
    return table;
}

// The weights table with, for a long-only book of up to 12 assets, a donut of
// the allocation beside it (same order as the table: largest first).
inline QWidget* build_weights_view(const QJsonObject& weights, QWidget* parent, int max_height = 320) {
    QTableWidget* table = build_weights_table(weights, parent, max_height);
    QStringList labels;
    QVector<double> values;
    for (int r = 0; r < table->rowCount(); ++r) {
        const auto* name = table->item(r, 0);
        const auto* num = dynamic_cast<const fincept::ui::NumericTableWidgetItem*>(table->item(r, 1));
        if (!name || !num)
            return table;
        labels << name->text();
        values << num->numeric_value();
    }
    return fincept::screens::quant_charts::qc_side_by_side(
        table, fincept::screens::quant_charts::qc_weights_donut(labels, values, parent), parent);
}

} // namespace fincept::screens::quant_advanced_helpers
