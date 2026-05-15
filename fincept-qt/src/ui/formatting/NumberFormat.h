#pragma once
#include <QString>

namespace fincept::ui::formatting {

inline QString format_compact_volume(qint64 volume) {
    if (volume <= 0) return "--";
    double v = static_cast<double>(volume);
    if (v >= 1e9) {
        return QString::number(v / 1e9, 'f', 2) + "B";
    } else if (v >= 1e6) {
        return QString::number(v / 1e6, 'f', 2) + "M";
    } else if (v >= 1e3) {
        return QString::number(v / 1e3, 'f', 2) + "K";
    } else {
        return QString::number(volume);
    }
}

} // namespace fincept::ui::formatting
