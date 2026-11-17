#pragma once
// cur:: — call-site helpers for the display currency. A thin facade over
// ui/formatting (FormatSettings, CurrencyTable, NumberFormat, FormatWidgets),
// kept so existing callers compile and follow Settings > Currency & Number
// Format. New code should call ui/formatting directly:
//
//   cur::symbol()                   -> fmt::currency_unit()          "€"
//   cur::symbol_for("CAD")          -> fmt::currency_unit("CAD")     "C$"
//   cur::money(v)                   -> fmt::money(v, fmt::display_currency())
//   cur::money(v, true)             -> fmt::money(v, display, fmt::Compact::FromSettings)
//   cur::money(v, true, "USD")      -> pin to an intrinsic currency (market data)
//   cur::bindLabel(lbl, "Rev (%1)") -> fmt::bind_currency_label(lbl, "Rev (%1)")
//   cur::bindPrefix(spin)           -> fmt::bind_currency_affix(spin)
//
// Symbol-only: these never convert. A value in another currency must be
// converted first (FxRateService::convert_for_display) or formatted with its
// own currency.

#include "core/currency/CurrencyManager.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLocale>
#include <QSpinBox>
#include <QString>

#include <cmath>

namespace cur {

/// The display currency's unit in the user's symbol style, e.g. "€".
inline QString symbol() {
    return fincept::ui::formatting::currency_unit();
}

/// Unit for a specific currency code, independent of the display currency.
/// Use for intrinsic-currency surfaces (e.g. a portfolio's own currency).
inline QString symbol_for(const QString& code) {
    return fincept::currency::CurrencyManager::symbol_for(code);
}

/// Format an amount. `compact` abbreviates per the user's scheme (K/M/B or
/// lakh / crore). `intrinsic` (if non-empty) is the amount's own currency;
/// otherwise the display currency is printed. Symbol placement, grouping,
/// decimals and negative style follow the user's settings; NaN / inf print "—".
inline QString money(double v, bool compact = false, const QString& intrinsic = QString()) {
    namespace uifmt = fincept::ui::formatting;
    return uifmt::money(v, intrinsic.isEmpty() ? uifmt::display_currency() : intrinsic,
                        compact ? uifmt::Compact::FromSettings : uifmt::Compact::None);
}

/// `tmpl` must contain a single "%1" placeholder for the unit; kept in sync
/// with the settings.
inline void bindLabel(QLabel* lbl, const QString& tmpl) {
    fincept::ui::formatting::bind_currency_label(lbl, tmpl);
}

/// Puts the display currency's unit on the spin box (prefix or suffix, per the
/// user's settings) and keeps it in sync.
inline void bindPrefix(QAbstractSpinBox* spin) {
    fincept::ui::formatting::bind_currency_affix(spin);
}

} // namespace cur
