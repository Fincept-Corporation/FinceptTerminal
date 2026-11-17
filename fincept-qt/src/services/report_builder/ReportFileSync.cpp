// ReportFileSync.cpp — see ReportFileSync.h.

#include "services/report_builder/ReportFileSync.h"

#include "core/config/AppSettings.h"
#include "core/logging/Logger.h"
#include "services/file_manager/FileManagerService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPointer>
#include <QSettings>
#include <QUuid>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrent>

namespace fincept::services::report_files {

namespace {

constexpr const char* kRfsKey = "report_builder/managed_copies";

QSettings rfs_settings() {
    return app_settings::open();
}

QString rfs_norm(const QString& path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool rfs_inside(const QString& path, const QString& root) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    return path.startsWith(QDir::cleanPath(QDir(root).absolutePath()) + QLatin1Char('/'), cs);
}

// Copy via a temporary sibling + rename, so the Files copy is never half-written.
bool rfs_copy(const QString& from, const QString& to) {
    const QString tmp = to + QStringLiteral(".part");
    QFile::remove(tmp);
    if (!QFile::copy(from, tmp))
        return false;
    QFile::remove(to);
    return QFile::rename(tmp, to);
}

} // namespace

void publish(const QString& path_in, const QString& source_screen) {
    if (path_in.isEmpty() || !QFileInfo(path_in).isFile())
        return;
    auto& files = FileManagerService::instance();
    const QString path = rfs_norm(path_in);
    const QString root = files.storage_dir();
    if (rfs_inside(path, root)) {
        files.import_file(path, source_screen); // refreshes the existing record, no copy
        return;
    }

    QSettings s = rfs_settings();
    QVariantMap map = s.value(kRfsKey).toMap();
    const QString known_id = map.value(path).toString();
    const ManagedFile known = known_id.isEmpty() ? ManagedFile{} : files.find_by_id(known_id);
    QString stored;
    bool fresh = false;
    if (!known.id.isEmpty() && !known.name.isEmpty()) {
        stored = known.name; // refresh this copy in place
    } else {
        const QFileInfo fi(path);
        stored = QString::number(QDateTime::currentMSecsSinceEpoch()) + QLatin1Char('_') +
                 QUuid::createUuid().toString(QUuid::Id128).left(8) +
                 (fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix());
        fresh = true;
    }
    const QString dest = QDir(root).absoluteFilePath(stored);
    const QString original = QFileInfo(path).fileName();
    const QString mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
    QPointer<FileManagerService> guard(&files);
    (void)QtConcurrent::run([guard, path, dest, stored, original, mime, fresh, source_screen]() {
        const bool ok = rfs_copy(path, dest);
        const qint64 size = QFileInfo(dest).size();
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [path, dest, stored, original, mime, fresh, source_screen, ok, size]() {
                if (!ok) {
                    LOG_WARN("ReportFileSync", "Could not copy " + path + " into Files");
                    return;
                }
                auto& fm = FileManagerService::instance();
                QString id;
                if (fresh)
                    id = fm.register_file(stored, original, size, mime, source_screen);
                else
                    id = fm.import_file(dest, source_screen); // inside storage: refresh size / listing
                if (id.isEmpty())
                    return;
                QSettings s2 = rfs_settings();
                QVariantMap m = s2.value(kRfsKey).toMap();
                m.insert(path, id);
                s2.setValue(kRfsKey, m);
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::services::report_files
