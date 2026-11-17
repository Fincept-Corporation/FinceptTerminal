#pragma once
// Money value types shared by the formatter (NumberFormat.h) and the FX
// service (services/markets/FxRateService.h). Plain data, Qt Core only.

#include <QDateTime>
#include <QString>

#include <cmath>
#include <limits>

namespace fincept::ui::formatting {

/// An amount with its currency. NaN amount = missing.
struct Money {
    double amount = std::numeric_limits<double>::quiet_NaN();
    QString currency; ///< as produced (minor units allowed: "GBp")
    bool valid() const { return std::isfinite(amount) && !currency.isEmpty(); }
};

/// One amount converted for display, with the rate and the rate's time.
/// Produced by FxRateService::convert_for_display() (or services::fx::
/// convert_for_display()); formatted by converted_money() / fx_note() /
/// export_converted().
struct ConvertedMoney {
    double amount = std::numeric_limits<double>::quiet_NaN(); ///< in `currency`; NaN = no rate
    QString currency;                                          ///< target (major code)
    double source_amount = std::numeric_limits<double>::quiet_NaN();
    QString source_currency;                               ///< as given ("GBp")
    double rate = std::numeric_limits<double>::quiet_NaN(); ///< `currency` per 1 MAJOR unit of source
    QDateTime as_of;           ///< UTC time of the rate (the market's time); invalid when not converted
    bool converted = false;    ///< false: same currency (amount only scaled for minor units)
    bool stale = false;        ///< the rate is older than the user's threshold
    bool triangulated = false; ///< computed through USD
    bool approximate = false;  ///< a stablecoin had no rate and was taken 1:1 to USD
    QString approx_reason;     ///< why it is approximate (for the tooltip)
    bool ok() const { return std::isfinite(amount); }
};

/// Unit multiplier of a scaled column / axis ("$M", "₹ Cr").
enum class Scale { One, Thousand, Million, Billion, Trillion, Lakh, Crore };

} // namespace fincept::ui::formatting
