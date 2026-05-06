#pragma once
// Geofence — pure helper that decides whether live trading is permitted in
// a given QLocale territory.
//
// Compiled into AlphaArenaScreen and into the unit tests; kept out of the
// screen.cpp itself so we can exercise it without instantiating Qt widgets.
//
// The function is *advisory* — final compliance responsibility remains with
// the user. Build with -DFINCEPT_UNSAFE_DISABLE_GEOFENCE=ON to bypass for
// development; release builds must leave the flag OFF.

#include <QLocale>

namespace fincept::screens::alpha_arena {

inline bool is_jurisdiction_blocked(QLocale::Territory territory) {
    switch (territory) {
    case QLocale::UnitedStates:
    case QLocale::NorthKorea:
    case QLocale::Iran:
    case QLocale::Syria:
    case QLocale::Cuba:
        return true;
    default:
        return false;
    }
}

} // namespace fincept::screens::alpha_arena
