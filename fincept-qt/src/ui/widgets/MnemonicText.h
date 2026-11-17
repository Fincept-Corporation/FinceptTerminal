#pragma once
// Literal ampersands in text that Qt reads as keyboard mnemonics.
#include <QString>

namespace fincept::ui {

/// Tab-bar tabs, menu titles and actions, push buttons, check boxes, radio
/// buttons and group boxes treat '&' as a mnemonic marker: "TIME & SALES"
/// showed as "TIME _SALES" (the S underlined, the '&' gone), "M&A" as "MA".
/// The terminal uses no mnemonics, so every '&' in such text is meant
/// literally: this doubles each one ("&&" displays as '&'). Text that is
/// already escaped ("F&&O") is left as it is.
inline QString literal_ampersands(const QString& text) {
    if (!text.contains(QLatin1Char('&')))
        return text;
    QString out;
    out.reserve(text.size() + 4);
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('&')) {
            out += QLatin1String("&&");
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('&'))
                ++i; // an escaped pair stays one escaped pair
            continue;
        }
        out += c;
    }
    return out;
}

} // namespace fincept::ui
