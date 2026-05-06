#include "core/layout/LayoutCatalog.h"

#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/profile/ProfilePaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

namespace {
constexpr const char* kCatalogTag = "LayoutCatalog";
constexpr const char* kConnectionName = "fincept_layouts_index";

QSqlDatabase& open_index_db_(const QString& path, bool& opened) {
    static QSqlDatabase db;
    if (opened)
        return db;
    db = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
    db.setDatabaseName(path);
    if (!db.open()) {
        LOG_ERROR(kCatalogTag, QString("Failed to open layouts index db: %1").arg(db.lastError().text()));
        opened = false;
    } else {
        opened = true;
    }
    return db;
}
} // namespace

LayoutCatalog& LayoutCatalog::instance() {
    static LayoutCatalog s;
    return s;
}

Result<void> LayoutCatalog::open() {
    if (opened_)
        return Result<void>::ok();

    // Ensure layouts dir + index file path exist before SQLite opens.
    QDir().mkpath(ProfilePaths::layouts_dir());

    auto& db = open_index_db_(ProfilePaths::layouts_index_db(), opened_);
    if (!opened_)
        return Result<void>::err("layouts_index.db open failed");

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode = WAL");
    pragma.exec("PRAGMA synchronous = NORMAL");

    auto r = ensure_index_schema();
    if (r.is_err()) {
        opened_ = false;
        return r;
    }
    LOG_INFO(kCatalogTag, "Layouts catalogue opened: " + ProfilePaths::layouts_index_db());
    return Result<void>::ok();
}

Result<void> LayoutCatalog::ensure_index_schema() {
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen())
        return Result<void>::err("index db not open");

    QSqlQuery q(db);
    if (!q.exec("CREATE TABLE IF NOT EXISTS layouts ("
                "  uuid TEXT PRIMARY KEY,"
                "  name TEXT NOT NULL,"
                "  kind TEXT NOT NULL,"
                "  created_at_unix INTEGER NOT NULL,"
                "  updated_at_unix INTEGER NOT NULL,"
                "  description TEXT,"
                "  thumbnail_path TEXT"
                ")")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_layouts_kind_updated "
                "ON layouts(kind, updated_at_unix DESC)")) {
        LOG_WARN(kCatalogTag, QString("idx create failed: %1").arg(q.lastError().text()));
    }
    // `_meta` is a generic key/value store. Used today by:
    //   last_loaded_uuid    — Phase 3 cold-boot restore pointer.
    //   fwsp_import_done    — Phase 7 one-shot legacy import flag.
    if (!q.exec("CREATE TABLE IF NOT EXISTS _meta ("
                "  key TEXT PRIMARY KEY,"
                "  value TEXT NOT NULL"
                ")")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

QString LayoutCatalog::file_path_for_(const LayoutId& id) const {
    return ProfilePaths::layouts_dir() + "/" + id.to_string() + ".json";
}

Result<void> LayoutCatalog::upsert_index_row(const layout::Workspace& w) {
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen())
        return Result<void>::err("index db not open");

    QSqlQuery q(db);
    q.prepare("INSERT INTO layouts(uuid, name, kind, created_at_unix, updated_at_unix, "
              "                     description, thumbnail_path) "
              "VALUES (?, ?, ?, ?, ?, ?, ?) "
              "ON CONFLICT(uuid) DO UPDATE SET "
              "  name=excluded.name, kind=excluded.kind, "
              "  updated_at_unix=excluded.updated_at_unix, "
              "  description=excluded.description, "
              "  thumbnail_path=excluded.thumbnail_path");
    q.addBindValue(w.id.to_string());
    q.addBindValue(w.name);
    q.addBindValue(w.kind);
    q.addBindValue(w.created_at_unix);
    q.addBindValue(w.updated_at_unix);
    q.addBindValue(w.description);
    q.addBindValue(w.thumbnail_path);
    if (!q.exec())
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<LayoutId> LayoutCatalog::save_workspace(const layout::Workspace& w_in) {
    if (!opened_) {
        auto r = open();
        if (r.is_err()) return Result<LayoutId>::err(r.error());
    }

    layout::Workspace w = w_in;
    if (w.id.is_null())
        w.id = LayoutId::generate();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (w.created_at_unix == 0)
        w.created_at_unix = now;
    w.updated_at_unix = now;

    // Write the JSON file first; if that fails, we don't want a stale
    // index row pointing at nothing.
    const QString path = file_path_for_(w.id);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return Result<LayoutId>::err(("Failed to open " + path + ": " + f.errorString()).toStdString());
    f.write(QJsonDocument(w.to_json()).toJson(QJsonDocument::Indented));
    f.close();

    auto idx = upsert_index_row(w);
    if (idx.is_err()) {
        LOG_WARN(kCatalogTag,
                 QString("File written but index update failed for %1: %2")
                     .arg(w.id.to_string(), QString::fromStdString(idx.error())));
        return Result<LayoutId>::err(idx.error());
    }
    LOG_INFO(kCatalogTag, QString("Saved layout '%1' (uuid=%2)").arg(w.name, w.id.to_string()));
    emit layout_saved(w.id);
    return Result<LayoutId>::ok(w.id);
}

Result<layout::Workspace> LayoutCatalog::load_workspace(const LayoutId& id) {
    if (!opened_) {
        auto r = open();
        if (r.is_err()) return Result<layout::Workspace>::err(r.error());
    }
    const QString path = file_path_for_(id);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return Result<layout::Workspace>::err(("Failed to open " + path + ": " + f.errorString()).toStdString());
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError)
        return Result<layout::Workspace>::err(("Parse error in " + path + ": " + err.errorString()).toStdString());
    if (!doc.isObject())
        return Result<layout::Workspace>::err(("Layout file is not a JSON object: " + path).toStdString());

    return Result<layout::Workspace>::ok(layout::Workspace::from_json(doc.object()));
}

Result<void> LayoutCatalog::remove_layout(const LayoutId& id) {
    if (!opened_) {
        auto r = open();
        if (r.is_err()) return r;
    }
    QFile::remove(file_path_for_(id));
    // Best-effort thumbnail cleanup; not fatal if it's missing.
    QFile::remove(ProfilePaths::layouts_dir() + "/thumbs/" + id.to_string() + ".png");
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("DELETE FROM layouts WHERE uuid = ?");
        q.addBindValue(id.to_string());
        q.exec();
    }
    // If the removed layout was pinned as last_loaded, clear the pointer so
    // cold-boot doesn't try to restore a dead UUID.
    auto last = meta(QStringLiteral("last_loaded_uuid"));
    if (last.is_ok() && last.value() == id.to_string())
        set_meta(QStringLiteral("last_loaded_uuid"), QString());

    // If the removed layout is also the one this session has loaded, drop
    // the in-memory current-name/id pointer so the title bar suffix and
    // the toolbar's Save-vs-Save-As decision stop pointing at a dead UUID.
    if (layout::WorkspaceShell::current_id() == id)
        layout::WorkspaceShell::clear_current();

    emit layout_removed(id);
    return Result<void>::ok();
}

Result<QList<LayoutCatalog::Entry>> LayoutCatalog::list_layouts() {
    if (!opened_) {
        auto r = open();
        if (r.is_err()) return Result<QList<Entry>>::err(r.error());
    }
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen())
        return Result<QList<Entry>>::err("index db not open");

    QSqlQuery q(db);
    if (!q.exec("SELECT uuid, name, kind, created_at_unix, updated_at_unix, "
                "       description, thumbnail_path "
                "FROM layouts ORDER BY updated_at_unix DESC"))
        return Result<QList<Entry>>::err(q.lastError().text().toStdString());

    QList<Entry> out;
    while (q.next()) {
        Entry e;
        e.id = LayoutId::from_string(q.value(0).toString());
        e.name = q.value(1).toString();
        e.kind = q.value(2).toString();
        e.created_at_unix = q.value(3).toLongLong();
        e.updated_at_unix = q.value(4).toLongLong();
        e.description = q.value(5).toString();
        e.thumbnail_path = q.value(6).toString();
        out.append(e);
    }
    return Result<QList<Entry>>::ok(out);
}

Result<QList<LayoutCatalog::Entry>> LayoutCatalog::recent_layouts(int limit, bool include_auto) {
    auto r = list_layouts();
    if (r.is_err()) return r;
    QList<Entry> filtered;
    for (const auto& e : r.value()) {
        if (!include_auto && e.kind == QStringLiteral("auto"))
            continue;
        filtered.append(e);
        if (filtered.size() >= limit) break;
    }
    return Result<QList<Entry>>::ok(filtered);
}

LayoutId LayoutCatalog::find_by_name(const QString& name) {
    auto r = list_layouts();
    if (r.is_err()) return LayoutId();
    for (const auto& e : r.value()) {
        if (e.name.compare(name, Qt::CaseInsensitive) == 0)
            return e.id;
    }
    return LayoutId();
}

Result<void> LayoutCatalog::export_to(const LayoutId& id, const QString& path) {
    auto wr = load_workspace(id);
    if (wr.is_err()) return Result<void>::err(wr.error());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return Result<void>::err(("Export open failed: " + f.errorString()).toStdString());
    f.write(QJsonDocument(wr.value().to_json()).toJson(QJsonDocument::Indented));
    f.close();
    return Result<void>::ok();
}

Result<QString> LayoutCatalog::meta(const QString& key) const {
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen())
        return Result<QString>::err("index db not open");
    QSqlQuery q(db);
    q.prepare("SELECT value FROM _meta WHERE key = ?");
    q.addBindValue(key);
    if (!q.exec())
        return Result<QString>::err(q.lastError().text().toStdString());
    if (q.next())
        return Result<QString>::ok(q.value(0).toString());
    return Result<QString>::ok(QString());
}

Result<void> LayoutCatalog::set_meta(const QString& key, const QString& value) {
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen())
        return Result<void>::err("index db not open");
    QSqlQuery q(db);
    q.prepare("INSERT INTO _meta(key, value) VALUES(?, ?) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<LayoutId> LayoutCatalog::last_loaded_id() const {
    auto m = meta(QStringLiteral("last_loaded_uuid"));
    if (m.is_err()) return Result<LayoutId>::err(m.error());
    if (m.value().isEmpty()) return Result<LayoutId>::ok(LayoutId());
    return Result<LayoutId>::ok(LayoutId::from_string(m.value()));
}

Result<void> LayoutCatalog::set_last_loaded_id(const LayoutId& id) {
    return set_meta(QStringLiteral("last_loaded_uuid"),
                    id.is_null() ? QString() : id.to_string());
}

Result<LayoutId> LayoutCatalog::import_from(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return Result<LayoutId>::err(("Import open failed: " + f.errorString()).toStdString());
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError)
        return Result<LayoutId>::err(("Parse error: " + err.errorString()).toStdString());

    layout::Workspace w = layout::Workspace::from_json(doc.object());
    // Mint a fresh id so two users importing the same shared file don't
    // collide on the same UUID. Caller can override via save_workspace
    // path-based copy if they really want to preserve cross-user identity.
    w.id = LayoutId::generate();
    return save_workspace(w);
}

} // namespace fincept
