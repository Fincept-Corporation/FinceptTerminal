#pragma once
// AmountInput — the field rules for a number or amount the user TYPES, on top of parse_amount()
// (ParseAmount.h, which reads K / M / B, lakh / crore, symbols, the user's separators, accounting
// negatives and minor units):
//
//   namespace fmt = fincept::ui::formatting;
//   const fmt::AmountInput a = fmt::read_amount_input(edit->text(), "USD");   // a money field in USD
//   if (!a.ok()) { show_inline_error(a.error); return; }
//   use(*a.value);                                                            // in USD
//
//   fmt::read_number_input("8.7x") / ("45.3 %")   -> a plain number (multiple, percent): no currency
//   fmt::amount_input_text(v)                     -> how to put a stored value back into such a field
//                                                    so that it reads back exactly under the user's
//                                                    number settings (never C-locale text)
//
// A currency named in the text that is not the field's own is REFUSED with one plain sentence
// naming both: an amount is never converted silently. A screen with an explicit conversion
// control converts through FxRateService::convert_for_display itself.
//
// Header-only; parse_amount() and number() are implemented in NumberFormat.cpp.

#include "ui/formatting/NumberFormat.h"
#include "ui/formatting/ParseAmount.h"

#include <QCoreApplication>
#include <QString>

#include <optional>

namespace fincept::ui::formatting {

struct AmountInput {
    std::optional<double> value; ///< in the unit of the field's currency
    QString currency;            ///< currency of `value`; "" when neither the text nor the field names one
    QString error;               ///< plain, translated sentence when !value
    bool ok() const { return value.has_value(); }
};

inline QString amount_input_tr(const char* text) {
    return QCoreApplication::translate("fincept::ui::formatting::AmountInput", text);
}

/// An amount typed into a money field held in `field_ccy` ("" = the field has no fixed currency:
/// any currency the text names is reported in `currency`, nothing is refused for it).
inline AmountInput read_amount_input(const QString& text, const QString& field_ccy) {
    AmountInput out;
    const ParsedAmount a = parse_amount(text, field_ccy);
    if (!a.ok()) {
        out.error = a.error;
        return out;
    }
    // Pounds typed into a pence field were already brought to the field's unit by parse_amount.
    if (!field_ccy.isEmpty() && a.currency != field_ccy) {
        out.error = amount_input_tr("“%1” is an amount in %2, but this field is in %3 — enter it in %3 "
                                    "(amounts are not converted).")
                        .arg(text.trimmed(), a.currency, field_ccy);
        return out;
    }
    out.value = a.value;
    out.currency = a.currency;
    return out;
}

/// A plain number typed by the user — a multiple ("8.7", "8.7x") or a percentage ("45.3", "45.3 %"):
/// the user's separators and K / M suffixes are read; a currency is refused.
inline AmountInput read_number_input(const QString& text_in) {
    AmountInput out;
    QString text = text_in.trimmed();
    if (text.endsWith(QLatin1Char('x'), Qt::CaseInsensitive) || text.endsWith(QChar(0x00D7)) ||
        text.endsWith(QLatin1Char('%')))
        text.chop(1);
    const ParsedAmount a = parse_amount(text);
    if (!a.ok()) {
        out.error = a.error;
        return out;
    }
    if (a.currency_in_text) {
        out.error = amount_input_tr("Enter a plain number here, without a currency.");
        return out;
    }
    out.value = a.value;
    return out;
}

/// A stored value as field text the readers above give back exactly: the user's number format,
/// as many decimals as the value has (at most `max_decimals`), no currency.
inline QString amount_input_text(double v, int max_decimals = 6) {
    if (!std::isfinite(v))
        return {};
    const QString fixed = QString::number(v, 'f', max_decimals);
    int decimals = 0;
    const qsizetype dot = fixed.indexOf(QLatin1Char('.'));
    if (dot >= 0) {
        qsizetype end = fixed.size();
        while (end > dot + 1 && fixed.at(end - 1) == QLatin1Char('0'))
            --end;
        decimals = static_cast<int>(end - dot - 1);
    }
    return number(v, decimals);
}

} // namespace fincept::ui::formatting
