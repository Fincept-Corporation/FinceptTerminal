#pragma once
// MaResultFormat.h — how M&A results read (shared by the result view, COPY / CSV / Add to Report and saved-case
// comparison): the unit of every field by its name (rates mostly arrive in percent, amounts in money, a few
// fractions), each analysis's headline KPIs and the money text in the user's display currency.

#include "ui/widgets/ResultLayout.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fincept::screens::ma_fmt {

/// Result-layout rules for the analysis `context` (units, KPIs, hidden / consumed keys, matrix axes).
fincept::ui::result::Options ma_result_options(const QString& context, const QJsonObject& payload,
                                               const QString& intrinsic);
/// An M&A amount, abbreviated per the user's compact scheme; `intrinsic` pins the currency (empty = display ccy).
QString ma_money_text(double v, const QString& intrinsic);
/// The headline KPI paths of `context` (dotted paths into the flattened result).
QStringList ma_headline_paths(const QString& context, const QJsonObject& payload);
/// Most scripts answer {"success": true, "data": <result>}: the result itself (data's fields, plus the other
/// top-level scalars); any other reply unchanged.
QJsonObject ma_flatten_result(const QJsonObject& envelope);
/// A numeric field of `o`, NaN when absent / not a number.
double ma_num(const QJsonObject& o, const char* key);

} // namespace fincept::screens::ma_fmt
