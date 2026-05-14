// src/screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h
//
// Private header for the QuantModulePanel ADVANCED-category display TUs.
// Each helper is `inline` so this header can be included from multiple
// split files without violating ODR. Mirrors the QuantModulePanel_GsHelpers
// pattern.

#pragma once

#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
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
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble() * 100.0, 'f', decimals) + "%";
}

inline QString fmt_num_safe(const QJsonValue& v, int decimals = 4) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toDouble(), 'f', decimals);
}

inline QString fmt_int_safe(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
    return QString::number(v.toInt());
}

inline QString fmt_bps(const QJsonValue& v, int decimals = 2) {
    if (v.isNull() || v.isUndefined()) return QStringLiteral("—");
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
inline bool check_success(const QJsonObject& payload,
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

// Render a {asset → weight} dict as a sorted-by-weight table.
inline QTableWidget* build_weights_table(const QJsonObject& weights,
                                         QWidget* parent,
                                         int max_height = 320) {
    QList<QPair<QString, double>> rows;
    for (auto it = weights.begin(); it != weights.end(); ++it)
        rows.append({it.key(), it.value().toDouble()});
    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return std::abs(a.second) > std::abs(b.second); });

    auto* table = new QTableWidget(rows.size(), 2, parent);
    table->setHorizontalHeaderLabels({"Asset", "Weight"});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setStyleSheet(fincept::screens::quant_styles::table_ss());
    table->setMaximumHeight(qMin(rows.size() * 24 + 32, max_height));
    for (int r = 0; r < rows.size(); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(rows[r].first));
        const double w = rows[r].second;
        auto* wi = new QTableWidgetItem(QString::number(w * 100.0, 'f', 3) + "%");
        wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (w > 0.001) wi->setForeground(QColor(fincept::ui::colors::POSITIVE()));
        else if (w < -0.001) wi->setForeground(QColor(fincept::ui::colors::NEGATIVE()));
        table->setItem(r, 1, wi);
        table->setRowHeight(r, 24);
    }
    return table;
}

} // namespace fincept::screens::quant_advanced_helpers
