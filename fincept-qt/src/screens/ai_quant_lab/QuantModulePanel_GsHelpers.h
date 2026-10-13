// src/screens/ai_quant_lab/QuantModulePanel_GsHelpers.h
//
// Private header — included only by QuantModulePanel*.cpp translation units.
// Provides the GS-Quant-style card / section / formatting helpers that were
// originally file-static in QuantModulePanel.cpp. Multiple display TUs
// (Functime, Statsmodels, Fortitudo, Gluonts, CFA, Backtesting, Quant Reporting)
// share these — and the original GS Quant TU still uses them too — so they
// live in a shared inline-namespace header.
#pragma once

#include "core/currency/Currency.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QLocale>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace fincept::screens::quant_gs_helpers {

// One labeled metric card in the same visual style as the HFT panel.
inline QWidget* gs_make_card(const QString& label, const QString& value, QWidget* parent,
                             const QString& value_color = {}) {
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* cvl = new QVBoxLayout(card);
    cvl->setContentsMargins(10, 6, 10, 6);
    cvl->setSpacing(2);
    auto* l = new QLabel(label, card);
    l->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                         .arg(ui::colors::TEXT_TERTIARY()));
    auto* v = new QLabel(value, card);
    // Cards carry model IDs, pipeline IDs, model keys and tickers that the next tab asks the user
    // to paste back in (Backtest / Feature Importance / Ensemble / Process Data); make them copyable.
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; font-family:'Courier New'; background:transparent;")
            .arg(value_color.isEmpty() ? QString(ui::colors::TEXT_PRIMARY()) : value_color));
    cvl->addWidget(l);
    cvl->addWidget(v);
    return card;
}

// A row of cards, evenly stretched.
inline QWidget* gs_card_row(const QList<QWidget*>& cards, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    for (auto* c : cards)
        hl->addWidget(c, 1);
    return row;
}

inline QString gs_fmt_pct(double v, int decimals = 2) {
    return QString("%1%").arg(v * 100.0, 0, 'f', decimals);
}

inline QString gs_fmt_money(double v) {
    // Calculator output — follows the preferred currency.
    return cur::money(v, /*compact=*/true);
}

inline QString gs_fmt_signed_money(double v) {
    return (v >= 0 ? QString("+") : QString("")) + gs_fmt_money(v);
}

inline QString gs_fmt_num(double v, int decimals = 4) {
    return QString::number(v, 'f', decimals);
}

inline QString gs_pos_neg_color(double v) {
    if (v > 0)
        return ui::colors::POSITIVE();
    if (v < 0)
        return ui::colors::NEGATIVE();
    return ui::colors::TEXT_PRIMARY();
}

// Format a JSON value for a generic key/value table (hyperparameters, CFA
// statistics, fallback results). UNIT-FREE on purpose: the value is shown as
// the number the script returned. This used to guess units — every |x| < 1
// became a percentage and every |x| >= 1e6 a currency amount — so a learning
// rate of 0.05 read "5.00%", a p-value of 0.03 read "3.00%", a correlation of
// 0.8 read "80.00%" and a 2,000,000-row count read as money. Renderers that
// know a field's unit format it themselves.
inline QString format_val(const QJsonValue& val) {
    if (val.isDouble()) {
        const double v = val.toDouble();
        if (!std::isfinite(v))
            return QString::fromUtf8("—");
        const double a = std::abs(v);
        if (a < 1e15 && v == std::floor(v))
            return QLocale::system().toString(static_cast<qlonglong>(v));
        if (a >= 1000.0)
            return QLocale::system().toString(v, 'f', 2);
        if (a >= 1.0)
            return QString::number(v, 'f', 4);
        return QString::number(v, 'g', 6); // keeps tiny p-values / ICs readable
    }
    if (val.isBool())
        return val.toBool() ? QCoreApplication::translate("QuantModulePanel", "YES")
                            : QCoreApplication::translate("QuantModulePanel", "NO");
    if (val.isString())
        return val.toString();
    return QString::fromUtf8("—");
}

// Build a section header strip ("RISK METRICS  |  252 obs").
inline QLabel* gs_section_header(const QString& text, const QString& accent) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color:%1; font-weight:700; font-size:10px; letter-spacing:1px;"
                               "padding:6px 10px; background:%2; border-left:3px solid %1;")
                           .arg(accent, ui::colors::BG_RAISED()));
    return lbl;
}

} // namespace fincept::screens::quant_gs_helpers
