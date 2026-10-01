// FileManagerService.cpp

#include "services/file_manager/FileManagerService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QUuid>

namespace fincept::services {

static constexpr const char* kFileManagerTag = "FileManagerService";
static bool is_safe_basename(const QString& s) {
    if (s.isEmpty() || s == "." || s == "..")
        return false;

    if (s.contains('/') || s.contains('\\'))
        return false;

    if (s.contains(QLatin1String("..")))
        return false;

    if (QFileInfo(s).fileName() != s)
        return false;

    return true;
}

// ── Singleton ────────────────────────────────────────────────────────────────

FileManagerService& FileManagerService::instance() {
    static FileManagerService s_instance;
    return s_instance;
}

FileManagerService::FileManagerService(QObject* parent) : QObject(parent) {
    QDir().mkpath(storage_dir());
    files_cache_ = read_metadata();
    LOG_INFO(kFileManagerTag, QString("Loaded %1 managed files").arg(files_cache_.size()));
}

// ── Paths ────────────────────────────────────────────────────────────────────

QString FileManagerService::storage_dir() const {
    return fincept::AppPaths::files();
}

QString FileManagerService::full_path(const QString& stored_name) const {
    if (!is_safe_basename(stored_name))
        return {};

    const QString resolved = QDir(storage_dir()).absoluteFilePath(stored_name);

    const QString root = QDir(storage_dir()).absolutePath() + '/';

    if (!resolved.startsWith(root))
        return {};

    return resolved;
}

QString FileManagerService::metadata_path() const {
    return storage_dir() + "/metadata.json";
}

// ── Persistence ──────────────────────────────────────────────────────────────

QJsonArray FileManagerService::read_metadata() const {
    QFile file(metadata_path());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return {};
    auto doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isObject() && doc.object().contains("files"))
        return doc.object()["files"].toArray();
    return {};
}

void FileManagerService::write_metadata(const QJsonArray& files) const {
    QDir().mkpath(storage_dir());
    QJsonObject root;
    root["files"] = files;
    // QSaveFile writes to a temp file and renames on commit(): a crash or full disk
    // mid-write can no longer truncate metadata.json and orphan every managed file
    // (the index is the only record of what is in storage).
    QSaveFile file(metadata_path());
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(kFileManagerTag, "Cannot write metadata index: " + file.errorString());
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit())
        LOG_ERROR(kFileManagerTag, "Failed to commit metadata index: " + file.errorString());
}

// ── Serialisation helpers ─────────────────────────────────────────────────────

QJsonObject FileManagerService::to_json(const ManagedFile& f) const {
    return QJsonObject{{"id", f.id},
                       {"name", f.name},
                       {"originalName", f.original_name},
                       {"size", f.size},
                       {"type", f.mime_type},
                       {"uploadedAt", f.uploaded_at},
                       {"path", f.path},
                       {"sourceScreen", f.source_screen}};
}

ManagedFile FileManagerService::from_json(const QJsonObject& obj) const {
    ManagedFile f;
    f.id = obj["id"].toString();
    f.name = obj["name"].toString();
    f.original_name = obj["originalName"].toString();
    f.size = obj["size"].toInteger();
    f.mime_type = obj["type"].toString();
    f.uploaded_at = obj["uploadedAt"].toString();
    f.path = obj["path"].toString();
    f.source_screen = obj["sourceScreen"].toString();
    return f;
}

// ── Query ─────────────────────────────────────────────────────────────────────

QJsonArray FileManagerService::all_files() const {
    return files_cache_;
}

ManagedFile FileManagerService::find_by_id(const QString& id) const {
    for (const auto& v : files_cache_) {
        if (!v.isObject())
            continue;
        auto obj = v.toObject();
        if (obj["id"].toString() == id)
            return from_json(obj);
    }
    return {};
}

QJsonArray FileManagerService::files_for_screen(const QString& screen) const {
    QJsonArray result;
    for (const auto& v : files_cache_) {
        if (v.isObject() && v.toObject()["sourceScreen"].toString() == screen)
            result.append(v);
    }
    return result;
}

QJsonArray FileManagerService::files_by_mime(const QString& mime_fragment) const {
    QJsonArray result;
    for (const auto& v : files_cache_) {
        if (v.isObject() && v.toObject()["type"].toString().contains(mime_fragment))
            result.append(v);
    }
    return result;
}

// ── Mutations ─────────────────────────────────────────────────────────────────

QString FileManagerService::import_file(const QString& source_path, const QString& source_screen) {
    QFileInfo info(source_path);
    if (!info.isFile()) {
        // exists() is also true for directories, which QFile::copy cannot import.
        LOG_WARN(kFileManagerTag, "import_file: source is not a file: " + source_path);
        return {};
    }

    // A file that already lives in managed storage (e.g. a library notebook or an
    // export a screen wrote straight into storage_dir()) must not be copied onto
    // itself under a new id: every Ctrl+S in Code Editor / Excel used to add yet
    // another duplicate entry. Refresh the existing record instead.
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity kStoragePathCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity kStoragePathCase = Qt::CaseSensitive;
#endif
    const QString storage_root = QDir::cleanPath(QDir(storage_dir()).absolutePath()) + '/';
    const QString source_abs = QDir::cleanPath(info.absoluteFilePath());
    if (source_abs.startsWith(storage_root, kStoragePathCase)) {
        const QString stored = info.fileName();
        if (is_safe_basename(stored) && source_abs.mid(storage_root.size()) == stored) {
            for (int i = 0; i < files_cache_.size(); ++i) {
                QJsonObject obj = files_cache_[i].toObject();
                if (obj["name"].toString() != stored)
                    continue;
                obj["size"] = info.size();
                files_cache_[i] = obj;
                write_metadata(files_cache_);
                emit files_changed();
                return obj["id"].toString();
            }
            // On disk but unindexed — register it where it is.
            return register_file(stored, stored, info.size(), QMimeDatabase().mimeTypeForFile(source_abs).name(),
                                 source_screen);
        }
    }

    QString id =
        QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" + QUuid::createUuid().toString(QUuid::Id128).left(8);
    QString ext = info.suffix();
    QString stored_name = id + (ext.isEmpty() ? "" : "." + ext);
    QString dest = full_path(stored_name);

    if (!QFile::copy(source_path, dest)) {
        LOG_ERROR(kFileManagerTag, "import_file: copy failed: " + source_path + " -> " + dest);
        return {};
    }

    QMimeDatabase mime_db;
    ManagedFile f;
    f.id = id;
    f.name = stored_name;
    f.original_name = info.fileName();
    f.size = info.size();
    f.mime_type = mime_db.mimeTypeForFile(source_path).name();
    f.uploaded_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    f.path = "fincept-files/" + stored_name;
    f.source_screen = source_screen;

    files_cache_.append(to_json(f));
    write_metadata(files_cache_);

    LOG_INFO(kFileManagerTag, QString("Imported '%1' (id=%2, screen=%3)").arg(f.original_name, id, source_screen));
    emit file_added(id);
    emit files_changed();
    return id;
}

QString FileManagerService::register_file(const QString& stored_name, const QString& original_name, qint64 size,
                                          const QString& mime_type, const QString& source_screen) {
    if (!is_safe_basename(stored_name)) {
        LOG_WARN(kFileManagerTag, QString("Rejected unsafe stored_name: %1").arg(stored_name));
        return {};
    }

    QString id =
        QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" + QUuid::createUuid().toString(QUuid::Id128).left(8);

    ManagedFile f;
    f.id = id;
    f.name = stored_name;
    f.original_name = original_name;
    f.size = size;
    f.mime_type = mime_type;
    f.uploaded_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    f.path = "fincept-files/" + stored_name;
    f.source_screen = source_screen;

    files_cache_.append(to_json(f));
    write_metadata(files_cache_);

    LOG_INFO(kFileManagerTag, QString("Registered '%1' (id=%2, screen=%3)").arg(original_name, id, source_screen));
    emit file_added(id);
    emit files_changed();
    return id;
}

bool FileManagerService::remove_file(const QString& id) {
    for (int i = 0; i < files_cache_.size(); ++i) {
        auto obj = files_cache_[i].toObject();
        if (obj["id"].toString() != id)
            continue;

        // Remove physical file. If it is still there afterwards (locked by another
        // program — common on Windows) keep the index entry and report failure so
        // the screen's "may be open in another program" warning is reachable; the
        // old code dropped the record and orphaned the file forever.
        const QString stored = obj["name"].toString();
        const QString on_disk = full_path(stored);
        if (!on_disk.isEmpty() && QFile::exists(on_disk) && !QFile::remove(on_disk)) {
            LOG_WARN(kFileManagerTag, "remove_file: could not delete " + on_disk);
            return false;
        }

        files_cache_.removeAt(i);
        write_metadata(files_cache_);

        LOG_INFO(kFileManagerTag, "Removed file id=" + id);
        emit file_removed(id);
        emit files_changed();
        return true;
    }
    LOG_WARN(kFileManagerTag, "remove_file: id not found: " + id);
    return false;
}

} // namespace fincept::services
