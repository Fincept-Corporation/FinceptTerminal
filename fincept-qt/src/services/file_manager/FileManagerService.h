#pragma once
// FileManagerService.h — Singleton service that owns the file storage index.
// All screens use this instead of managing their own JSON metadata files.
// Emits signals whenever files are added, removed, or the list changes.

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace fincept::services {

/// Lightweight record describing one managed file.
struct ManagedFile {
    QString id;           // timestamp_uuid8
    QString name;         // stored filename (id + ext)
    QString original_name;
    qint64  size = 0;
    QString mime_type;
    QString uploaded_at;  // ISO-8601 UTC
    QString path;         // relative: "fincept-files/<name>"
    QString source_screen;// e.g. "report_builder", "code_editor", ""
};

class FileManagerService : public QObject {
    Q_OBJECT

  public:
    static FileManagerService& instance();

    // ── Query ────────────────────────────────────────────────────────────
    QJsonArray  all_files() const;
    ManagedFile find_by_id(const QString& id) const;
    QJsonArray  files_for_screen(const QString& screen) const;
    QJsonArray  files_by_mime(const QString& mime_fragment) const;

    // ── Mutations ────────────────────────────────────────────────────────
    /// Copy an external file into managed storage; returns the new file id.
    /// source_screen: optional tag, e.g. "report_builder".
    QString import_file(const QString& source_path, const QString& source_screen = {});

    /// Register a file that already lives inside storage_dir() by its stored name.
    /// Used when a screen writes directly to storage_dir() itself.
    QString register_file(const QString& stored_name, const QString& original_name,
                          qint64 size, const QString& mime_type,
                          const QString& source_screen = {});

    bool    remove_file(const QString& id);

    // ── Paths ────────────────────────────────────────────────────────────
    QString storage_dir()    const;
    QString full_path(const QString& stored_name) const;

  signals:
    void file_added(const QString& id);
    void file_removed(const QString& id);
    void files_changed();

  private:
    explicit FileManagerService(QObject* parent = nullptr);

    QString     metadata_path() const;
    QJsonArray  read_metadata() const;
    void        write_metadata(const QJsonArray& files) const;
    QJsonObject to_json(const ManagedFile& f) const;
    ManagedFile from_json(const QJsonObject& obj) const;

    QJsonArray files_cache_;  // in-memory copy, kept in sync with disk
};

} // namespace fincept::services
