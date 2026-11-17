#pragma once
// EquityStyle — the Equity Trading screen's one selector stylesheet, built from
// theme tokens (ui::colors / ui::fonts) and set once on the screen before its
// panels exist. The central appearance pass (ThemeRestyle) keeps it in step with
// theme and font changes; nothing re-sets it per update.
//
// Panels: hairlines (BORDER_DIM) between zones and under headers, no boxed
// borders; prices and P&L in POSITIVE / NEGATIVE, states in SUCCESS / DANGER /
// WARNING, text on accent fills in TEXT_ON_ACCENT.

#include <QString>

namespace fincept::screens::equity {

/// The screen sheet (object-name selectors: #eqScreen, #eqWatchlist…, #eqOrderEntry…).
QString eq_screen_sheet();

} // namespace fincept::screens::equity
