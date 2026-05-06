#include "core/screen/MonitorTopology.h"

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>

#include <algorithm>

namespace fincept {

namespace {

QString hash_fields(const QScreen* screen) {
    // Compose a deterministic fingerprint from properties that survive
    // across reboots, dock reorderings, AND replugging the same monitor
    // into a different physical slot:
    //   manufacturer, model, current resolution, physical size
    //
    // Position is intentionally NOT in the fingerprint. The "where each
    // window goes" question is answered by WorkspaceVariant.frame_screens,
    // which maps each frame's WindowId to a stable monitor id and is
    // applied by WorkspaceShell::apply. Folding desktop x/y into the id
    // here would defeat that — the same physical screen would get a
    // different id every time the user reorders monitors, and the
    // WorkspaceMatcher's exact-topology tier would never hit.
    //
    // Two physically-identical monitors with no serial number do collide
    // under this scheme. That's an inherent limit of the OS (vendors
    // don't expose anything stronger than make/model/res for cheap
    // panels). serialNumber()-based ids in MonitorTopology::stable_id
    // are tried first and remain unaffected.
    QString fingerprint;
    fingerprint += screen->manufacturer();
    fingerprint += '/';
    fingerprint += screen->model();
    fingerprint += '/';
    const QSize sz = screen->size();
    fingerprint += QString::number(sz.width()) + 'x' + QString::number(sz.height());
    fingerprint += '/';
    fingerprint += QString::number(screen->physicalSize().width(), 'f', 1) + 'x';
    fingerprint += QString::number(screen->physicalSize().height(), 'f', 1);

    const QByteArray digest =
        QCryptographicHash::hash(fingerprint.toUtf8(), QCryptographicHash::Sha256);
    // 16 hex chars = 64 bits of fingerprint, enough to make collision over
    // any realistic monitor count negligible while keeping ids short.
    return "h:" + QString::fromLatin1(digest.toHex().left(16));
}

} // namespace

QString MonitorTopology::stable_id(const QScreen* screen) {
    if (!screen)
        return {};

    const QString serial = screen->serialNumber();
    if (!serial.isEmpty())
        return "s:" + serial;

    return hash_fields(screen);
}

layout::MonitorTopologyKey MonitorTopology::current_key() {
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty())
        return {};

    QStringList ids;
    ids.reserve(screens.size());
    for (const QScreen* s : screens) {
        const QString id = stable_id(s);
        if (!id.isEmpty())
            ids.append(id);
    }
    std::sort(ids.begin(), ids.end());
    return layout::MonitorTopologyKey{ids.join(',')};
}

QScreen* MonitorTopology::find_screen_by_stable_id(const QString& stable_id) {
    if (stable_id.isEmpty())
        return nullptr;
    for (QScreen* s : QGuiApplication::screens()) {
        if (MonitorTopology::stable_id(s) == stable_id)
            return s;
    }
    return nullptr;
}

} // namespace fincept
