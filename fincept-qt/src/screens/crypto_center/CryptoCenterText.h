#pragma once
// CryptoCenterText.h — small text helpers shared by the Crypto Center's bar, tabs and panels.
// Private to src/screens/crypto_center (header-only, no CMake entry). One definition each:
// unity builds merge this directory's .cpp files into one translation unit, where the per-file
// copies these replace (same name, same body, in anonymous namespaces) collided. Files pull a
// helper in with a using-declaration, so their call sites are unchanged.

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto_center_text {

/// The data font family, for stylesheets ("font-family:%1").
inline QString font_stack() {
    return QString(fincept::ui::fonts::DATA_FAMILY);
}

/// How long ago `ts_ms` (epoch ms) was: "45s ago", "3m ago", "2h ago", "4d ago"; "never" for 0.
inline QString relative_time(qint64 ts_ms) {
    if (ts_ms == 0)
        return QStringLiteral("never");
    const auto delta = QDateTime::currentMSecsSinceEpoch() - ts_ms;
    if (delta < 0)
        return QStringLiteral("just now");
    const auto seconds = delta / 1000;
    if (seconds < 60)
        return QStringLiteral("%1s ago").arg(seconds);
    if (seconds < 3600)
        return QStringLiteral("%1m ago").arg(seconds / 60);
    if (seconds < 86400)
        return QStringLiteral("%1h ago").arg(seconds / 3600);
    return QStringLiteral("%1d ago").arg(seconds / 86400);
}

/// An on-chain amount in base units (a decimal integer string) in whole tokens; 0 when empty or
/// not a number.
inline double atomic_to_ui(const QString& raw, int decimals) {
    if (raw.isEmpty())
        return 0.0;
    bool ok = false;
    const auto u = raw.toULongLong(&ok);
    if (!ok)
        return 0.0;
    return static_cast<double>(u) / std::pow(10.0, std::max(0, decimals));
}

} // namespace fincept::screens::crypto_center_text
