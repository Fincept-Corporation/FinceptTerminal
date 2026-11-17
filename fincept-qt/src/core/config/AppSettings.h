#pragma once
// AppSettings.h — the one way application code opens this terminal's own
// QSettings store.
//
// Default data folder: open() is exactly the store every release used — the
// edition's organisation / application names (EditionIdentity.h), the native
// format, user scope (HKCU\Software\<org>\<app>, ~/Library/Preferences/
// com.<org>.<app>.plist, ~/.config/<org>/<app>.conf). Nothing moves for an
// existing install.
//
// A FINCEPT_DATA_DIR naming another folder than the default one makes a separate
// terminal (app/InstanceKey.h). main() then calls use_store_under(<root>) before
// anything opens settings, and the store becomes <root>/settings/<org>/<app>.ini:
// a test or scratch folder never reads or writes the user's window layout, MCP
// port or approval flags, and two folders never overwrite each other's. Nothing
// is copied into such a store; it starts with defaults.
//
// Never construct QSettings with the edition's names: QSettings(org, app) always
// opens the native store, which ignores that redirection on Windows and macOS.
// QSettings() resolves to the same store as open() once main() has named the
// application; open() is right even before that (it names the store itself).
//
// Header-only, Qt Core only.

#include "core/config/EditionIdentity.h"

#include <QSettings>
#include <QString>

namespace fincept::app_settings {

/// This terminal's settings store. Cheap to open (QSettings caches the backing
/// file / registry handles per store); open one where it is needed and do not
/// share an instance between threads. Returned as a prvalue: write
/// `QSettings s = app_settings::open();`.
inline QSettings open() {
    // Default folder: defaultFormat() is NativeFormat, so this is the same store
    // as QSettings(org, app) — NativeFormat, UserScope, the same two names.
    return QSettings(QSettings::defaultFormat(), QSettings::UserScope,
                     QString::fromLatin1(edition::kSettingsOrganization),
                     QString::fromLatin1(edition::kSettingsApplication));
}

/// Put this process's settings store under <data_root>/settings (one INI file per
/// organisation / application). main() calls it once, right after naming the
/// application and before anything opens settings, when the data folder is not
/// the edition's default one. Never called for the default folder.
inline void use_store_under(const QString& data_root) {
    const QString dir = data_root + QStringLiteral("/settings");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir); // Unix only; ignored elsewhere
    QSettings::setDefaultFormat(QSettings::IniFormat);
}

} // namespace fincept::app_settings
