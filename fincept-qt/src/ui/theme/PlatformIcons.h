#pragma once
// PlatformIcons.h — keeps Qt's own icon lookups (the QMessageBox icon, the
// QFileDialog file / folder icons, the QLineEdit clear button) off the slow path.
//
// QStyle::standardIcon() and QFileIconProvider first ask the freedesktop icon
// theme (QIcon::fromTheme). When a theme directory has no valid
// icon-theme.cache (minimal installs, containers, ~/.local/share/icons), Qt
// finds an icon by stat()ing every directory the theme lists, for every new
// icon name — 0.1–1.2 s on the GUI thread the first time a message box, a file
// dialog or a search box with a clear button appears (ui_stalls.log: QMessageBox
// ::setIcon / QFileIconProvider::icon -> QIconLoader::findIconHelper ->
// QFileInfo::exists). The terminal never uses theme icons itself (its chrome
// icons are ui::theme_icon, ThemeIcon.h), so search roots in which a theme of
// the active chain would be scanned uncached are dropped; Qt then uses its
// built-in icons for those widgets, which costs nothing. Roots whose themes
// carry a valid cache (normal desktops) are kept, so native icons stay there.
//
// Also loads the MIME database the file dialog needs on a pool thread
// (QMimeDatabase is thread-safe), so the first dialog does not parse it.
//
// Call once on the GUI thread after QApplication exists, before the first
// window (ThemeManager's constructor does). Header-only: no moc, no source list.

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMimeDatabase>
#include <QStringList>
#include <QtConcurrent/QtConcurrent>

#include <tuple>

namespace fincept::ui {

namespace platform_icons_detail {

/// Themes QIconLoader may walk for one lookup: the active theme, everything it
/// inherits (index.theme "Inherits="), the fallback theme and hicolor.
inline QStringList icon_theme_chain(const QStringList& roots) {
    QStringList chain;
    QStringList queue{QIcon::themeName(), QIcon::fallbackThemeName(), QStringLiteral("hicolor")};
    while (!queue.isEmpty() && chain.size() < 16) {
        const QString name = queue.takeFirst().trimmed();
        if (name.isEmpty() || chain.contains(name))
            continue;
        chain.append(name);
        for (const QString& root : roots) {
            if (root.startsWith(QLatin1Char(':')))
                continue;
            QFile index(root + QLatin1Char('/') + name + QStringLiteral("/index.theme"));
            if (!index.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            while (!index.atEnd()) {
                const QByteArray line = index.readLine().trimmed();
                if (line.startsWith("Inherits=")) {
                    const auto parts = QString::fromUtf8(line.mid(9)).split(QLatin1Char(','), Qt::SkipEmptyParts);
                    for (const QString& p : parts)
                        queue.append(p.trimmed());
                    break;
                }
            }
            break; // the first root holding the theme defines it
        }
    }
    return chain;
}

/// True when `theme_dir` is a theme Qt would have to scan file by file: it has
/// an index.theme but no icon-theme.cache, or a cache older than the directory
/// (Qt ignores a stale cache).
inline bool theme_dir_is_uncached(const QString& theme_dir) {
    if (!QFileInfo::exists(theme_dir + QStringLiteral("/index.theme")))
        return false;
    const QFileInfo cache(theme_dir + QStringLiteral("/icon-theme.cache"));
    if (!cache.exists())
        return true;
    return QFileInfo(theme_dir).lastModified() > cache.lastModified();
}

} // namespace platform_icons_detail

inline void tune_platform_icon_lookup() {
    static bool done = false;
    if (done)
        return;
    done = true;

    namespace d = platform_icons_detail;
    const QStringList roots = QIcon::themeSearchPaths();
    const QStringList chain = d::icon_theme_chain(roots);
    QStringList kept;
    QStringList dropped;
    for (const QString& root : roots) {
        bool slow = false;
        if (!root.startsWith(QLatin1Char(':'))) { // resources are in memory: never slow
            for (const QString& theme : chain) {
                if (d::theme_dir_is_uncached(root + QLatin1Char('/') + theme)) {
                    slow = true;
                    break;
                }
            }
        }
        (slow ? dropped : kept).append(root);
    }
    if (!dropped.isEmpty()) {
        QIcon::setThemeSearchPaths(kept);
        LOG_DEBUG("PlatformIcons", QString("Icon theme(s) %1 have no valid icon-theme.cache under %2 — using Qt's "
                                           "built-in icons for dialogs / clear buttons instead (no per-icon disk scan)")
                                       .arg(chain.join(QStringLiteral(", ")), dropped.join(QStringLiteral(", "))));
    }

    std::ignore = QtConcurrent::run([]() {
        const QMimeDatabase db;
        (void)db.mimeTypeForName(QStringLiteral("text/plain"));
        (void)db.mimeTypeForFile(QStringLiteral("warmup.csv"), QMimeDatabase::MatchExtension); // globs too
    });
}

} // namespace fincept::ui
