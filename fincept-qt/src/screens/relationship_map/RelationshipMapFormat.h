// src/screens/relationship_map/RelationshipMapFormat.h
#pragma once
// Money text of the Relationship Map (graph cards and the detail panel), through the
// shared formatter (ui/formatting/NumberFormat.h). Header-only, Qt Core; unit-tested by
// tests/tst_relmap_format.cpp.
//
// Two kinds of value, two rules (currency design: instrument prices stay in the
// instrument's own currency; never an assumed "$"):
//   * per-share prices (quote, peer price, 52-week range, analyst targets) are in the
//     QUOTE currency exactly as Yahoo reports it, minor units kept: an LSE quote in pence
//     (GBp) prints "1,234.50p", a Johannesburg one in cents (ZAc) keeps its unit;
//   * company amounts are reported in a MAJOR currency even when the quote is in a minor
//     unit — a London company's market cap is in pounds. Printing it with the pence unit
//     was a 100x label error. Which major currency:
//       - market cap and holding values: the quote currency's major unit (GBp -> GBP);
//       - statement figures (revenue, EPS, officers' pay): the FINANCIAL currency the
//         company reports in (a GBp-quoted company may report in USD), falling back to
//         the quote currency's major unit when Yahoo gives none (statement_currency()).

#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/NumberFormat.h"

#include <QString>
#include <QStringList>

namespace fincept::relmap {

/// Currency of a company's statement figures (revenue, EPS, pay): the financial
/// currency when known, else the quote currency's major unit ("GBp" -> "GBP");
/// "" when neither is known (amounts then print without a symbol).
inline QString statement_currency(const QString& financial_currency, const QString& quote_currency) {
    const QString f = financial_currency.trimmed();
    if (!f.isEmpty())
        return fincept::ui::formatting::major_currency(f);
    const QString q = quote_currency.trimmed();
    return q.isEmpty() ? QString() : fincept::ui::formatting::major_currency(q);
}

/// The script's labels of the sections a slow upstream left out that belong to a
/// detail-panel node kind ("company" gets all of them). Labels are the script's
/// (relationship_map.py _TimedTicker._LABELS, "peer <TICKER>").
inline QStringList missing_for_kind(const QString& kind, const QStringList& missing_sections) {
    if (missing_sections.isEmpty())
        return {};
    if (kind == QLatin1String("company"))
        return missing_sections;
    QStringList wanted;
    bool peers = false;
    if (kind == QLatin1String("institutional"))
        wanted = {QStringLiteral("institutional holders")};
    else if (kind == QLatin1String("mutualfund"))
        wanted = {QStringLiteral("fund holders")};
    else if (kind == QLatin1String("insider"))
        wanted = {QStringLiteral("insiders"), QStringLiteral("insider transactions")};
    else if (kind == QLatin1String("analyst"))
        wanted = {QStringLiteral("analyst targets"), QStringLiteral("recommendations"),
                  QStringLiteral("upgrades/downgrades")};
    else if (kind == QLatin1String("calendar"))
        wanted = {QStringLiteral("calendar")};
    else if (kind == QLatin1String("peer"))
        peers = true;
    QStringList out;
    for (const QString& s : missing_sections) {
        if (wanted.contains(s, Qt::CaseInsensitive) || (peers && s.startsWith(QLatin1String("peer "), Qt::CaseInsensitive)))
            out << s;
    }
    out.removeDuplicates();
    return out;
}

/// A per-share price in the instrument's quote currency (code used as given — "ZAc" is
/// cents, "ZAC" is not a code). An empty code prints the number without a symbol.
inline QString price_text(double value, const QString& quote_currency) {
    namespace fmt = fincept::ui::formatting;
    const QString c = quote_currency.trimmed();
    return c.isEmpty() ? fmt::price(value) : fmt::price_with_symbol(value, fmt::InstrumentFmt{c});
}

/// A company amount in the major currency of the quote currency, abbreviated per the
/// user's compact scheme unless `compact` is false. An empty code prints no symbol.
inline QString amount_text(double value, const QString& quote_currency, bool compact = true) {
    namespace fmt = fincept::ui::formatting;
    const QString c = quote_currency.trimmed();
    const fmt::Compact scheme = compact ? fmt::Compact::FromSettings : fmt::Compact::None;
    if (c.isEmpty())
        return compact ? fmt::compact(value, scheme) : fmt::number(value, 2);
    return fmt::money(value, fmt::major_currency(c), scheme);
}

} // namespace fincept::relmap
