// src/screens/ai_quant_lab/QuantModulePanel_GsHelpers.h
//
// Private header — included only by QuantModulePanel*.cpp translation units.
// Provides the GS-Quant-style card / section / formatting helpers that were
// originally file-static in QuantModulePanel.cpp. Multiple display TUs
// (Functime, Statsmodels, Fortitudo, Gluonts, CFA, Backtesting, Quant Reporting)
// share these — and the original GS Quant TU still uses them too — so they
// live in a shared inline-namespace header.
#pragma once

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QJsonValue>
#include <QLabel>
#include <QList>
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
    v->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; font-family:'Courier New'; background:transparent;")
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
    const double a = std::abs(v);
    if (a >= 1e9) return QString("$%1B").arg(v / 1e9, 0, 'f', 2);
    if (a >= 1e6) return QString("$%1M").arg(v / 1e6, 0, 'f', 2);
    if (a >= 1e3) return QString("$%1K").arg(v / 1e3, 0, 'f', 2);
    return QString("$%1").arg(v, 0, 'f', 2);
}

inline QString gs_fmt_signed_money(double v) {
    return (v >= 0 ? QString("+") : QString("")) + gs_fmt_money(v);
}

inline QString gs_fmt_num(double v, int decimals = 4) {
    return QString::number(v, 'f', decimals);
}

inline QString gs_pos_neg_color(double v) {
    if (v > 0) return ui::colors::POSITIVE();
    if (v < 0) return ui::colors::NEGATIVE();
    return ui::colors::TEXT_PRIMARY();
}

// Format a JSON value as a human-friendly metric string. Used by the CFA Quant
// and generic display helpers to render flat scalar metrics tables.
inline QString format_val(const QJsonValue& val) {
    if (val.isDouble()) {
        double v = val.toDouble();
        if (std::abs(v) >= 1e9)
            return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
        if (std::abs(v) >= 1e6)
            return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
        if (std::abs(v) < 1.0 && std::abs(v) > 0.0001)
            return QString("%1%").arg(v * 100, 0, 'f', 2);
        return QString::number(v, 'f', 4);
    }
    if (val.isBool())
        return val.toBool() ? "YES" : "NO";
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
