#include "app/EditionRootDecision.h"

#include "core/config/EditionIdentity.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace fincept::legacy_root::decision {

QStringList private_fingerprints() {
    // Written into the data root by the private build only (EditionIdentity.h).
    // Any one of them means the folder is, or was, shared with the private
    // build, so it is never adopted.
    QStringList out;
    for (const char* rel : edition::kPrivateBuildFingerprints)
        out << QString::fromLatin1(rel);
    return out;
}

QStringList private_fingerprints_in(const QString& legacy_root) {
    QStringList found;
    const QStringList fingerprints = private_fingerprints();
    const auto look = [&found, &fingerprints](const QDir& dir, const QString& label_prefix) {
        for (const QString& rel : fingerprints) {
            const QFileInfo fi(dir.filePath(rel));
            // A dangling link still says somebody put it there.
            if (fi.exists() || fi.isSymLink())
                found << label_prefix + rel;
        }
    };
    const QDir root(legacy_root);
    look(root, QString());
    // Named profiles keep their own data/ and test-settings under profiles/<name>/
    // (ProfileManager::profile_root): a private-build profile there marks the
    // whole folder too.
    const QDir profiles(root.filePath(QStringLiteral("profiles")));
    if (profiles.exists()) {
        const QStringList names = profiles.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QString& name : names)
            look(QDir(profiles.filePath(name)), QStringLiteral("profiles/%1/").arg(name));
    }
    return found;
}

bool holds_no_data(const QString& dir) {
    const QFileInfo info(dir);
    if (!info.isDir())
        return !info.exists() && !info.isSymLink();
    const QString manifest = QDir(dir).absoluteFilePath(QStringLiteral("profiles.json"));
    QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (QFileInfo(it.next()).absoluteFilePath() != manifest)
            return false;
    }
    return true;
}

FileFacts gather(const QString& root, const QString& legacy_root) {
    FileFacts f;
    f.root_holds_data = !holds_no_data(root);
    const QFileInfo legacy(legacy_root);
    f.legacy_exists = legacy.exists() || legacy.isSymLink();
    // isSymLink() covers Windows shortcuts and symlinks; isJunction() the
    // NTFS junctions a user or a sync tool may have put there.
    f.legacy_plain_dir = legacy.isDir() && !legacy.isSymLink() && !legacy.isJunction();
    if (f.legacy_plain_dir)
        f.private_files = private_fingerprints_in(legacy_root);
    return f;
}

Verdict by_files(const FileFacts& facts) {
    Verdict v;
    if (facts.root_holds_data) {
        v.outcome = Outcome::Existing;
        v.reason = QStringLiteral("the data root already holds data");
        return v;
    }
    if (!facts.legacy_exists) {
        v.outcome = Outcome::Fresh;
        v.reason = QStringLiteral("no legacy data root");
        return v;
    }
    if (!facts.legacy_plain_dir) {
        v.outcome = Outcome::Undetermined;
        v.reason = QStringLiteral("the legacy data root is not a plain folder");
        return v;
    }
    if (!facts.private_files.isEmpty()) {
        v.outcome = Outcome::LeftForOtherBuild;
        v.reason = QStringLiteral("it holds files only the private build writes (%1)")
                       .arg(facts.private_files.join(QLatin1String(", ")));
        return v;
    }
    v.final = false;
    return v;
}

Verdict by_databases(DbKind main_db, const QString& main_detail, const QList<ProfileDb>& profiles) {
    Verdict v;
    switch (main_db) {
        case DbKind::Missing:
            v.outcome = Outcome::Undetermined;
            v.reason = QStringLiteral("it has no data/fincept.db");
            return v;
        case DbKind::Empty:
            v.outcome = Outcome::Undetermined;
            v.reason = QStringLiteral("its data/fincept.db is empty");
            return v;
        case DbKind::Unreadable:
            v.outcome = Outcome::Undetermined;
            v.reason = QStringLiteral("its data/fincept.db could not be read (%1)").arg(main_detail);
            return v;
        case DbKind::Foreign:
            v.outcome = Outcome::LeftForOtherBuild;
            v.reason = QStringLiteral("its data/fincept.db belongs to another build (%1)").arg(main_detail);
            return v;
        case DbKind::Ours:
            break;
    }
    for (const ProfileDb& p : profiles) {
        // A named profile without a database yet is fine; one that is another
        // build's — or that cannot be read, so it might be — keeps the folder
        // where it is.
        if (p.kind == DbKind::Foreign || p.kind == DbKind::Unreadable) {
            v.outcome = Outcome::LeftForOtherBuild;
            v.reason = QStringLiteral("profile %1's database is not this build's (%2)").arg(p.name, p.detail);
            return v;
        }
    }
    v.final = false;
    v.outcome = Outcome::Adopted;
    return v;
}

} // namespace fincept::legacy_root::decision
