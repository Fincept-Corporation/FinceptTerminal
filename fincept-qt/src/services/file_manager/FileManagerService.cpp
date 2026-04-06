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
#include <QUuid>

namespace fincept::services {

static constexpr const char* kFileManagerTag = "FileManagerService";

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
    return storage_dir() + "/" + stored_name;
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
    QFile file(metadata_path());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
    }
}

// ── Serialisation helpers ─────────────────────────────────────────────────────

QJsonObject FileManagerService::to_json(const ManagedFile& f) const {
    return QJsonObject{
        {"id",           f.id},
        {"name",         f.name},
        {"originalName", f.original_name},
        {"size",         f.size},
        {"type",         f.mime_type},
        {"uploadedAt",   f.uploaded_at},
        {"path",         f.path},
        {"sourceScreen", f.source_screen}
    };
}

ManagedFile FileManagerService::from_json(const QJsonObject& obj) const {
    ManagedFile f;
    f.id            = obj["id"].toString();
    f.name          = obj["name"].toString();
    f.original_name = obj["originalName"].toString();
    f.size          = obj["size"].toInteger();
    f.mime_type     = obj["type"].toString();
    f.uploaded_at   = obj["uploadedAt"].toString();
    f.path          = obj["path"].toString();
    f.source_screen = obj["sourceScreen"].toString();
    return f;
}

// ── Query ─────────────────────────────────────────────────────────────────────

QJsonArray FileManagerService::all_files() const {
    return files_cache_;
}

ManagedFile FileManagerService::find_by_id(const QString& id) const {
    for (const auto& v : files_cache_) {
        if (!v.isObject()) continue;
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
    if (!info.exists()) {
        LOG_WARN(kFileManagerTag, "import_file: source does not exist: " + source_path);
        return {};
    }

    QString id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
                 QUuid::createUuid().toString(QUuid::Id128).left(8);
    QString ext = info.suffix();
    QString stored_name = id + (ext.isEmpty() ? "" : "." + ext);
    QString dest = full_path(stored_name);

    if (!QFile::copy(source_path, dest)) {
        LOG_ERROR(kFileManagerTag, "import_file: copy failed: " + source_path + " -> " + dest);
        return {};
    }

    QMimeDatabase mime_db;
    ManagedFile f;
    f.id            = id;
    f.name          = stored_name;
    f.original_name = info.fileName();
    f.size          = info.size();
    f.mime_type     = mime_db.mimeTypeForFile(source_path).name();
    f.uploaded_at   = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    f.path          = "fincept-files/" + stored_name;
    f.source_screen = source_screen;

    files_cache_.append(to_json(f));
    write_metadata(files_cache_);

    LOG_INFO(kFileManagerTag, QString("Imported '%1' (id=%2, screen=%3)").arg(f.original_name, id, source_screen));
    emit file_added(id);
    emit files_changed();
    return id;
}

QString FileManagerService::register_file(const QString& stored_name, const QString& original_name,
                                           qint64 size, const QString& mime_type,
                                           const QString& source_screen) {
    QString id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
                 QUuid::createUuid().toString(QUuid::Id128).left(8);

    ManagedFile f;
    f.id            = id;
    f.name          = stored_name;
    f.original_name = original_name;
    f.size          = size;
    f.mime_type     = mime_type;
    f.uploaded_at   = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    f.path          = "fincept-files/" + stored_name;
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
        if (obj["id"].toString() != id) continue;

        // Remove physical file
        QString stored = obj["name"].toString();
        QFile::remove(full_path(stored));

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
