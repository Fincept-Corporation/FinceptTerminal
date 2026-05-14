// src/screens/backtesting/BacktestingScreen_internal.h
//
// Private helpers shared between the BacktestingScreen.cpp implementation
// files (core, layout, commands, strategies, results). Not intended for use
// outside this folder — kept out of BacktestingScreen.h so consumers don't
// pull in the styling/formatting machinery.
//
// Each helper is `inline` so the header can be included from multiple TUs
// without violating ODR.

#pragma once

#include "services/backtesting/BacktestingTypes.h"

#include <QJsonValue>
#include <QLayout>
#include <QLayoutItem>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QString>
#include <QWidget>

#include <cmath>

namespace fincept::screens::backtesting_internal {

// Shared pill geometry used by every chip on the top bar (brand, provider
// tabs, RUN, status) and by update_provider_buttons() when re-skinning the
// selected vs. inactive provider tab. Both files must agree on the exact
// height/padding for the row to render cleanly, so the constants live here.
inline constexpr int kPillHeight = 24;
inline constexpr int kPillPadH = 10;

inline QString pill_qss(const QString& selector,
                        const QString& fg,
                        const QString& bg,
                        const QString& border,
                        int font_px,
                        const QString& font_family,
                        const QString& weight = QStringLiteral("700")) {
    // Subtract 2px (the 1px top + bottom border) from min/max-height so the
    // total outer box equals kPillHeight in Qt's stylesheet box model.
    return QString("%1 {"
                   "  color:%2; background:%3; border:1px solid %4;"
                   "  font-family:%5; font-size:%6px; font-weight:%7;"
                   "  padding:0 %8px;"
                   "  min-height:%9px; max-height:%9px;"
                   "}")
        .arg(selector, fg, bg, border, font_family)
        .arg(font_px)
        .arg(weight)
        .arg(kPillPadH)
        .arg(kPillHeight - 2);
}

inline void apply_pill_geometry(QWidget* w) {
    w->setFixedHeight(kPillHeight);
    w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    w->setContentsMargins(0, 0, 0, 0);
}

inline QString fmt_metric(const QString& key, const QJsonValue& val) {
    using namespace fincept::services::backtest;

    if (val.isString())
        return val.toString();
    if (val.isBool())
        return val.toBool() ? "YES" : "NO";
    if (!val.isDouble())
        return QString::fromUtf8("—");

    double v = val.toDouble();

    // Count metrics → integer
    if (count_metric_keys().contains(key))
        return QString::number(static_cast<int>(v));

    // Percentage metrics → show as "xx.xx%"
    if (pct_metric_keys().contains(key)) {
        if (std::abs(v) <= 1.0)
            return QString("%1%").arg(v * 100.0, 0, 'f', 2);
        return QString("%1%").arg(v, 0, 'f', 2);
    }

    // Ratio metrics → show as-is with 2-4 decimals
    if (ratio_metric_keys().contains(key))
        return QString::number(v, 'f', std::abs(v) >= 10.0 ? 2 : 4);

    // Currency-like large values
    if (std::abs(v) >= 1e9)
        return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
    if (std::abs(v) >= 1e6)
        return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
    if (std::abs(v) >= 1e3)
        return QString("$%1K").arg(v / 1e3, 0, 'f', 0);

    return QString::number(v, 'f', 4);
}

inline void clear_layout(QLayout* layout) {
    if (!layout)
        return;
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (auto* w = item->widget()) {
            w->setParent(nullptr);
            w->deleteLater();
        } else if (auto* sub = item->layout()) {
            clear_layout(sub);
        }
        delete item;
    }
}

} // namespace fincept::screens::backtesting_internal
