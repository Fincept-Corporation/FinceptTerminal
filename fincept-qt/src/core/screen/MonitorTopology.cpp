#include "core/screen/MonitorTopology.h"

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>

#include <algorithm>

namespace fincept {

namespace {

QString hash_fields(const QScreen* screen) {
    // Compose a deterministic fingerprint from properties that are stable
    // across reboots and dock reorderings:
    //   manufacturer, model, physical size, current resolution, position
    //
    // Position is included so two physically-identical monitors at different
    // desk slots get distinct ids; otherwise a layout couldn't pin one panel
    // to "the left monitor" vs "the right monitor" when both are the same
    // make/model.
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
    fingerprint += '/';
    const QRect g = screen->geometry();
    fingerprint += QString::number(g.x()) + ',' + QString::number(g.y());

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
