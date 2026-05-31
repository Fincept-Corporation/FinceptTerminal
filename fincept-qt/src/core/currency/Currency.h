#pragma once
// cur:: — call-site helpers built on CurrencyManager.
//
//   cur::symbol()                   -> current global symbol, e.g. "€"
//   cur::money(v)                   -> "€1,234.56" using the global currency
//   cur::money(v, true)             -> "€1.2B" compact (K/M/B)
//   cur::money(v, true, "USD")      -> pin to an intrinsic currency (market data)
//   cur::bindLabel(lbl, "Rev (%1)") -> set text now + on every currency_changed
//   cur::bindPrefix(spin)           -> set spinbox prefix now + on change
//
// Binders use the bound widget as the Qt receiver/context, so the connection
// auto-disconnects when the widget is destroyed — no manual cleanup needed.

#include "core/currency/CurrencyManager.h"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLocale>
#include <QSpinBox>
#include <QString>
#include <cmath>

namespace cur {

inline QString symbol() { return fincept::currency::CurrencyManager::instance().symbol(); }

/// Symbol for a specific currency code, independent of the global preference.
/// Use for intrinsic-currency surfaces (e.g. a portfolio's own currency).
inline QString symbol_for(const QString& code) {
    return fincept::currency::CurrencyManager::symbol_for(code);
}

/// Format an amount. `compact` abbreviates to K/M/B. `intrinsic` (if non-empty)
/// pins the symbol to that currency code instead of the global preference.
inline QString money(double v, bool compact = false, const QString& intrinsic = QString()) {
    const QString sym = intrinsic.isEmpty()
                            ? symbol()
                            : fincept::currency::CurrencyManager::symbol_for(intrinsic);
    const QString sign = v < 0 ? QStringLiteral("-") : QString();
    const double a = std::abs(v);
    if (compact) {
        if (a >= 1e9)
            return sign + sym + QString::number(a / 1e9, 'f', 1) + "B";
        if (a >= 1e6)
            return sign + sym + QString::number(a / 1e6, 'f', 1) + "M";
        if (a >= 1e3)
            return sign + sym + QString::number(a / 1e3, 'f', 0) + "K";
    }
    return sign + sym + QLocale(QLocale::English).toString(a, 'f', 2);
}

/// `tmpl` must contain a single "%1" placeholder for the symbol.
inline void bindLabel(QLabel* lbl, const QString& tmpl) {
    if (!lbl)
        return;
    lbl->setText(tmpl.arg(symbol()));
    QObject::connect(&fincept::currency::CurrencyManager::instance(),
                     &fincept::currency::CurrencyManager::currency_changed, lbl,
                     [lbl, tmpl](const QString&, const QString& s) { lbl->setText(tmpl.arg(s)); });
}

/// Sets the spin box prefix to the current symbol (plus a space) and keeps it
/// in sync with currency changes.
inline void bindPrefix(QAbstractSpinBox* spin) {
    if (!spin)
        return;
    auto apply = [spin](const QString& s) {
        if (auto* d = qobject_cast<QDoubleSpinBox*>(spin))
            d->setPrefix(s + " ");
        else if (auto* i = qobject_cast<QSpinBox*>(spin))
            i->setPrefix(s + " ");
    };
    apply(symbol());
    QObject::connect(&fincept::currency::CurrencyManager::instance(),
                     &fincept::currency::CurrencyManager::currency_changed, spin,
                     [apply](const QString&, const QString& s) { apply(s); });
}

} // namespace cur
