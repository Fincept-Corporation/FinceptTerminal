// Unit tests for src/app/EditionRootDecision.{h,cpp} — the rules that decide
// whether this edition may take over the com.fincept.terminal folder releases
// shared with the separate private build (LegacyRootAdoption).
//
// Every case builds a fake folder tree in a QTemporaryDir. The rules must:
//   - never claim a folder the private build has written to (any fingerprint, at
//     the root or in a named profile), or one whose database is another build's;
//   - never act on a link / junction / file in place of the folder;
//   - never act when the edition's own root already holds data;
//   - change nothing on disk while deciding (read-only).

#include "app/EditionRootDecision.h"
#include "core/config/EditionIdentity.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryDir>
#include <QTest>

#include <iterator>

using namespace fincept::legacy_root;
namespace dec = fincept::legacy_root::decision;

namespace {

bool touch(const QString& path, const QByteArray& content = "x") {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(content) == content.size();
}

/// path -> size:mtime of every entry under `dir`, to prove nothing changed.
QMap<QString, QString> snapshot(const QString& dir) {
    QMap<QString, QString> out;
    QDirIterator it(dir, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo fi(it.next());
        out.insert(fi.absoluteFilePath(),
                   QStringLiteral("%1:%2").arg(fi.size()).arg(fi.lastModified().toMSecsSinceEpoch()));
    }
    return out;
}

/// A legacy root as a release of this terminal left it: a database and logs.
void make_community_legacy(const QString& legacy) {
    QVERIFY(touch(legacy + QStringLiteral("/data/fincept.db"), "SQLite format 3"));
    QVERIFY(touch(legacy + QStringLiteral("/logs/fincept.log")));
    QVERIFY(touch(legacy + QStringLiteral("/profiles.json"), "{\"profiles\":[{\"name\":\"default\"}]}"));
}

} // namespace

class TstEditionRootDecision : public QObject {
    Q_OBJECT

  private slots:
    void fingerprints_come_from_edition_identity();
    void holds_no_data_rules();
    void fresh_when_there_is_no_legacy_root();
    void existing_when_own_root_holds_data();
    void undetermined_when_legacy_is_not_a_folder();
    void undetermined_when_legacy_is_a_link();
    void every_private_fingerprint_keeps_the_folder();
    void private_profile_fingerprint_keeps_the_folder();
    void clean_community_folder_goes_on_to_databases();
    void database_rules();
    void deciding_changes_nothing_on_disk();
};

void TstEditionRootDecision::fingerprints_come_from_edition_identity() {
    const QStringList fps = dec::private_fingerprints();
    QCOMPARE(fps.size(), int(std::size(fincept::edition::kPrivateBuildFingerprints)));
    QVERIFY(fps.contains(QStringLiteral("graphics.json"))); // written on every start of the private build
    QVERIFY(fps.contains(QStringLiteral("data/algo.db")));
}

void TstEditionRootDecision::holds_no_data_rules() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.filePath(QStringLiteral("root"));
    QVERIFY(dec::holds_no_data(root)); // missing
    QVERIFY(QDir().mkpath(root + QStringLiteral("/data")));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/logs")));
    QVERIFY(dec::holds_no_data(root)); // folders only
    QVERIFY(touch(root + QStringLiteral("/profiles.json"), "{}"));
    QVERIFY(dec::holds_no_data(root)); // + the top-level manifest main() writes
    QVERIFY(touch(root + QStringLiteral("/profiles/x/profiles.json"), "{}"));
    QVERIFY(!dec::holds_no_data(root)); // a manifest elsewhere is data
    QVERIFY(QFile::remove(root + QStringLiteral("/profiles/x/profiles.json")));
    QVERIFY(touch(root + QStringLiteral("/data/.hidden")));
    QVERIFY(!dec::holds_no_data(root)); // hidden files count
    const QString file_root = tmp.filePath(QStringLiteral("a-file"));
    QVERIFY(touch(file_root));
    QVERIFY(!dec::holds_no_data(file_root)); // a file where the folder should be
}

void TstEditionRootDecision::fresh_when_there_is_no_legacy_root() {
    QTemporaryDir tmp;
    const dec::FileFacts f = dec::gather(tmp.filePath(QStringLiteral("own")), tmp.filePath(QStringLiteral("legacy")));
    QVERIFY(!f.root_holds_data);
    QVERIFY(!f.legacy_exists);
    const dec::Verdict v = dec::by_files(f);
    QVERIFY(v.final);
    QCOMPARE(v.outcome, Outcome::Fresh);
}

void TstEditionRootDecision::existing_when_own_root_holds_data() {
    QTemporaryDir tmp;
    const QString own = tmp.filePath(QStringLiteral("own"));
    const QString legacy = tmp.filePath(QStringLiteral("legacy"));
    QVERIFY(touch(own + QStringLiteral("/data/fincept.db")));
    make_community_legacy(legacy);
    const dec::Verdict v = dec::by_files(dec::gather(own, legacy));
    QVERIFY(v.final);
    QCOMPARE(v.outcome, Outcome::Existing);
}

void TstEditionRootDecision::undetermined_when_legacy_is_not_a_folder() {
    QTemporaryDir tmp;
    const QString legacy = tmp.filePath(QStringLiteral("legacy"));
    QVERIFY(touch(legacy));
    const dec::FileFacts f = dec::gather(tmp.filePath(QStringLiteral("own")), legacy);
    QVERIFY(f.legacy_exists);
    QVERIFY(!f.legacy_plain_dir);
    const dec::Verdict v = dec::by_files(f);
    QVERIFY(v.final);
    QCOMPARE(v.outcome, Outcome::Undetermined);
}

void TstEditionRootDecision::undetermined_when_legacy_is_a_link() {
    QTemporaryDir tmp;
    const QString target = tmp.filePath(QStringLiteral("elsewhere"));
    make_community_legacy(target);
    const QString legacy = tmp.filePath(QStringLiteral("legacy"));
#ifdef Q_OS_WIN
    // A symbolic link needs a privilege on Windows; a Windows shortcut is what
    // QFile::link makes there, which QFileInfo also reports as a link.
    if (!QFile::link(target, legacy + QStringLiteral(".lnk")))
        QSKIP("cannot create a link here");
    const QString probe = legacy + QStringLiteral(".lnk");
#else
    QVERIFY(QFile::link(target, legacy));
    const QString probe = legacy;
#endif
    const dec::FileFacts f = dec::gather(tmp.filePath(QStringLiteral("own")), probe);
    QVERIFY(f.legacy_exists);
    QVERIFY(!f.legacy_plain_dir);
    const dec::Verdict v = dec::by_files(f);
    QVERIFY(v.final);
    QCOMPARE(v.outcome, Outcome::Undetermined);
}

void TstEditionRootDecision::every_private_fingerprint_keeps_the_folder() {
    for (const QString& fp : dec::private_fingerprints()) {
        QTemporaryDir tmp;
        const QString legacy = tmp.filePath(QStringLiteral("legacy"));
        make_community_legacy(legacy);
        // Folder-type fingerprints (data/harness, data/chat_archive, ...) and
        // file-type ones both count.
        if (fp.contains(QLatin1Char('.')))
            QVERIFY(touch(legacy + QLatin1Char('/') + fp));
        else
            QVERIFY(QDir().mkpath(legacy + QLatin1Char('/') + fp));
        const dec::FileFacts f = dec::gather(tmp.filePath(QStringLiteral("own")), legacy);
        QCOMPARE(f.private_files, QStringList{fp});
        const dec::Verdict v = dec::by_files(f);
        QVERIFY2(v.final, qPrintable(fp));
        QCOMPARE(v.outcome, Outcome::LeftForOtherBuild);
        QVERIFY(v.reason.contains(fp));
    }
}

void TstEditionRootDecision::private_profile_fingerprint_keeps_the_folder() {
    QTemporaryDir tmp;
    const QString legacy = tmp.filePath(QStringLiteral("legacy"));
    make_community_legacy(legacy);
    QVERIFY(touch(legacy + QStringLiteral("/profiles/agentbench/data/settings_cache.json"), "{}"));
    const dec::FileFacts f = dec::gather(tmp.filePath(QStringLiteral("own")), legacy);
    QCOMPARE(f.private_files, QStringList{QStringLiteral("profiles/agentbench/data/settings_cache.json")});
    QCOMPARE(dec::by_files(f).outcome, Outcome::LeftForOtherBuild);
}

void TstEditionRootDecision::clean_community_folder_goes_on_to_databases() {
    QTemporaryDir tmp;
    const QString own = tmp.filePath(QStringLiteral("own"));
    const QString legacy = tmp.filePath(QStringLiteral("legacy"));
    // What main() and the crash handler create in the edition's root before
    // adoption runs: folders and a manifest — not data.
    QVERIFY(QDir().mkpath(own + QStringLiteral("/crashdumps")));
    QVERIFY(touch(own + QStringLiteral("/profiles.json"), "{}"));
    make_community_legacy(legacy);
    // Files this edition writes itself are not fingerprints.
    QVERIFY(touch(legacy + QStringLiteral("/data/.fincept_notebooks_seeded_v1")));
    QVERIFY(touch(legacy + QStringLiteral("/data/cache.db")));
    const dec::FileFacts f = dec::gather(own, legacy);
    QVERIFY(!f.root_holds_data);
    QVERIFY(f.legacy_plain_dir);
    QVERIFY(f.private_files.isEmpty());
    QVERIFY(!dec::by_files(f).final);
}

void TstEditionRootDecision::database_rules() {
    using K = dec::DbKind;
    struct Case {
        K main;
        QList<dec::ProfileDb> profiles;
        bool final;
        Outcome outcome;
    };
    const QList<Case> cases = {
        {K::Missing, {}, true, Outcome::Undetermined},
        {K::Empty, {}, true, Outcome::Undetermined},
        {K::Unreadable, {}, true, Outcome::Undetermined},
        {K::Foreign, {}, true, Outcome::LeftForOtherBuild},
        {K::Ours, {}, false, Outcome::Adopted},
        {K::Ours, {{QStringLiteral("work"), K::Missing, {}}}, false, Outcome::Adopted},
        {K::Ours, {{QStringLiteral("work"), K::Empty, {}}}, false, Outcome::Adopted},
        {K::Ours, {{QStringLiteral("work"), K::Ours, {}}}, false, Outcome::Adopted},
        {K::Ours, {{QStringLiteral("work"), K::Foreign, QStringLiteral("schema v99")}}, true, Outcome::LeftForOtherBuild},
        {K::Ours, {{QStringLiteral("a"), K::Ours, {}}, {QStringLiteral("b"), K::Unreadable, QStringLiteral("locked")}},
         true, Outcome::LeftForOtherBuild},
    };
    for (const Case& c : cases) {
        const dec::Verdict v = dec::by_databases(c.main, QStringLiteral("detail"), c.profiles);
        QCOMPARE(v.final, c.final);
        QCOMPARE(v.outcome, c.outcome);
        if (v.final)
            QVERIFY(!v.reason.isEmpty());
    }
}

void TstEditionRootDecision::deciding_changes_nothing_on_disk() {
    QTemporaryDir tmp;
    const QString own = tmp.filePath(QStringLiteral("own"));
    const QString legacy = tmp.filePath(QStringLiteral("legacy"));
    make_community_legacy(legacy);
    QVERIFY(touch(legacy + QStringLiteral("/graphics.json"), "{}"));
    QVERIFY(touch(legacy + QStringLiteral("/profiles/work/data/fincept.db"), "SQLite format 3"));
    const QMap<QString, QString> before = snapshot(tmp.path());
    for (int i = 0; i < 2; ++i) { // and the same answer twice
        const dec::FileFacts f = dec::gather(own, legacy);
        const dec::Verdict v = dec::by_files(f);
        QCOMPARE(v.outcome, Outcome::LeftForOtherBuild);
    }
    QCOMPARE(snapshot(tmp.path()), before);
    QVERIFY(!QFileInfo::exists(own)); // the edition's root is not created by deciding
}

QTEST_GUILESS_MAIN(TstEditionRootDecision)
#include "tst_edition_root_decision.moc"
