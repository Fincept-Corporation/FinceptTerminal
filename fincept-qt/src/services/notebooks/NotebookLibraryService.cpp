// NotebookLibraryService.cpp

#include "services/notebooks/NotebookLibraryService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "services/file_manager/FileManagerService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

namespace fincept::services {

static constexpr const char* kTag = "NotebookLibrary";

// ── Singleton ────────────────────────────────────────────────────────────────

NotebookLibraryService& NotebookLibraryService::instance() {
    static NotebookLibraryService s_instance;
    return s_instance;
}

NotebookLibraryService::NotebookLibraryService(QObject* parent) : QObject(parent) {}

// ── Asset resolution ──────────────────────────────────────────────────────────
// Mirrors PythonRunner::find_scripts_dir / ComponentCatalog::load_with_fallbacks:
// the notebooks ship under resources/notebooks/ next to the binary (POST_BUILD)
// and inside the app bundle (install). Walk a handful of candidate layouts, then
// fall back to walking up the tree to find the source-tree copy in dev runs.

QString NotebookLibraryService::assets_dir() {
    if (assets_dir_resolved_)
        return assets_dir_cache_;
    assets_dir_resolved_ = true;

    const QString exe = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        exe + "/resources/notebooks",            // dev + Windows/Linux next-to-exe
        exe + "/../Resources/resources/notebooks", // macOS .app/Contents/Resources
        exe + "/../resources/notebooks",
        exe + "/../../resources/notebooks",
        exe + "/../share/fincept-terminal/resources/notebooks", // Linux install
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c + "/notebooks_manifest.json")) {
            assets_dir_cache_ = QDir(c).absolutePath();
            return assets_dir_cache_;
        }
    }

    // Dev fallback: walk up from the executable looking for the source-tree copy.
    QDir dir(exe);
    for (int i = 0; i < 8 && dir.cdUp(); ++i) {
        const QString p = dir.filePath("resources/notebooks/notebooks_manifest.json");
        if (QFileInfo::exists(p)) {
            assets_dir_cache_ = QFileInfo(p).absolutePath();
            return assets_dir_cache_;
        }
        const QString p2 = dir.filePath("fincept-qt/resources/notebooks/notebooks_manifest.json");
        if (QFileInfo::exists(p2)) {
            assets_dir_cache_ = QFileInfo(p2).absolutePath();
            return assets_dir_cache_;
        }
    }

    LOG_WARN(kTag, "Notebook assets dir not found");
    assets_dir_cache_.clear();
    return assets_dir_cache_;
}

QString NotebookLibraryService::asset_path(const NotebookCatalogEntry& entry) {
    const QString dir = assets_dir();
    if (dir.isEmpty() || entry.file.isEmpty())
        return {};
    return dir + "/" + entry.file;
}

// ── Catalog ───────────────────────────────────────────────────────────────────

const QVector<NotebookCatalogEntry>& NotebookLibraryService::catalog() {
    if (!loaded_) {
        load_catalog();
        loaded_ = true;
    }
    return catalog_;
}

void NotebookLibraryService::load_catalog() {
    catalog_.clear();

    const QString dir = assets_dir();
    if (dir.isEmpty())
        return;

    QFile f(dir + "/notebooks_manifest.json");
    if (!f.open(QIODevice::ReadOnly)) {
        LOG_WARN(kTag, "Cannot open notebooks_manifest.json");
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    for (const QJsonValue& v : doc.object()["notebooks"].toArray()) {
        const QJsonObject o = v.toObject();
        NotebookCatalogEntry e;
        e.id = o["id"].toString();
        e.title = o["title"].toString();
        e.category = o["category"].toString();
        e.difficulty = o["difficulty"].toString();
        e.est_minutes = o["est_minutes"].toInt();
        e.requirements = o["requires"].toString();
        e.summary = o["summary"].toString();
        e.file = o["file"].toString();
        if (!e.id.isEmpty() && !e.file.isEmpty())
            catalog_.append(e);
    }
    LOG_INFO(kTag, QString("Loaded %1 prebuilt notebooks").arg(catalog_.size()));
}

// ── Seeding ───────────────────────────────────────────────────────────────────

QString NotebookLibraryService::seed_marker_path() const {
    return fincept::AppPaths::data() + "/.fincept_notebooks_seeded_v1";
}

void NotebookLibraryService::seed_into_files(bool force) {
    if (!force && QFileInfo::exists(seed_marker_path()))
        return;

    const auto& cat = catalog();
    if (cat.isEmpty())
        return; // assets missing — try again next launch (no marker written)

    auto& fm = FileManagerService::instance();

    // Index existing code_editor files by their original name so re-seeding never
    // duplicates a notebook the user already has.
    QSet<QString> existing;
    for (const QJsonValue& v : fm.files_for_screen("code_editor"))
        existing.insert(v.toObject()["originalName"].toString());

    int seeded = 0;
    for (const NotebookCatalogEntry& e : cat) {
        if (existing.contains(e.file))
            continue;
        const QString src = asset_path(e);
        if (src.isEmpty() || !QFileInfo::exists(src))
            continue;
        if (!fm.import_file(src, "code_editor").isEmpty())
            ++seeded;
    }

    // Mark seeded only once we actually have the assets, so a launch before the
    // bundle is in place doesn't permanently skip seeding.
    QFile marker(seed_marker_path());
    if (marker.open(QIODevice::WriteOnly)) {
        marker.write("seeded\n");
        marker.close();
    }
    LOG_INFO(kTag, QString("Seeded %1 prebuilt notebooks into File Manager").arg(seeded));
}

// ── Working copy (copy-on-open) ────────────────────────────────────────────────

QString NotebookLibraryService::working_copy_for(const NotebookCatalogEntry& entry) {
    auto& fm = FileManagerService::instance();

    // Reuse an existing managed copy if one is already present (e.g. seeded).
    for (const QJsonValue& v : fm.files_for_screen("code_editor")) {
        const QJsonObject o = v.toObject();
        if (o["originalName"].toString() != entry.file)
            continue;
        const QString p = fm.full_path(o["name"].toString());
        if (!p.isEmpty() && QFileInfo::exists(p))
            return p;
    }

    // Otherwise import the bundled asset into managed storage.
    const QString src = asset_path(entry);
    if (!src.isEmpty() && QFileInfo::exists(src)) {
        const QString id = fm.import_file(src, "code_editor");
        if (!id.isEmpty()) {
            const QString p = fm.full_path(fm.find_by_id(id).name);
            if (!p.isEmpty())
                return p;
        }
    }

    // Last resort: open the read-only bundled asset directly.
    return src;
}

} // namespace fincept::services
