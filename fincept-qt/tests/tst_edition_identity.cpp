// Unit tests for src/core/config/EditionIdentity.h and EditionNames.h — the names
// this edition gives the operating system.
//
// Two builds of Fincept Terminal can be installed on one machine: this community
// edition and a separate private build. Every name below must differ from the one
// the private build uses (data root, QSettings store, single-instance socket,
// Linux package, temp-file and screenshot patterns its uninstaller sweeps, MCP
// client-config entries), or installing / running / uninstalling one edition
// breaks the other. The private build's values are written out literally here on
// purpose: they are facts about another program, not derived names.

#include "app/InstanceKey.h"
#include "core/config/EditionIdentity.h"
#include "core/config/EditionNames.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <iterator>

using namespace fincept;

class TstEditionIdentity : public QObject {
    Q_OBJECT

  private slots:
    void derived_names_carry_the_edition();
    void derived_names_differ_from_the_private_build();
    void legacy_constants_name_the_private_build();
    void instance_socket_is_edition_specific();
    void instance_socket_is_user_specific();
    void instance_socket_is_stable_and_short();
    void legacy_socket_matches_the_private_formula();
    void temp_and_screenshot_names_escape_foreign_sweeps();
    void settings_and_secret_stores_are_separate();
    // app/InstanceKey.h — the data folder is part of the single-instance identity.
    void instance_key_default_root_is_unchanged();
    void instance_key_other_root_is_its_own_instance();
    void instance_key_root_spellings_meet();
    void instance_key_canonical_root_resolves_the_folder();
};

void TstEditionIdentity::derived_names_carry_the_edition() {
    const QString id = QLatin1String(edition::kId);
    QVERIFY(!id.isEmpty());
    QVERIFY(QRegularExpression(QStringLiteral("^[a-z][a-z0-9]*$")).match(id).hasMatch());
    for (const char* name : {edition::kDataDirName, edition::kDesktopFileName, edition::kBundleId,
                             edition::kLinuxPackageName, edition::kLinuxInstallPrefix, edition::kTempPrefix,
                             edition::kMcpServerName}) {
        QVERIFY2(QString::fromLatin1(name).contains(id), name);
    }
    QVERIFY(QString::fromLatin1(edition::kSettingsOrganization).contains(id, Qt::CaseInsensitive));
    QVERIFY(QString::fromLatin1(edition::kSettingsApplication).contains(id, Qt::CaseInsensitive));
    QCOMPARE(QString::fromLatin1(edition::kLinuxInstallPrefix),
             QStringLiteral("/opt/") + QLatin1String(edition::kLinuxPackageName));
    QCOMPARE(QString::fromLatin1(edition::kInstanceKeyPrefix), QString::fromLatin1(edition::kSettingsApplication));
}

void TstEditionIdentity::derived_names_differ_from_the_private_build() {
    // What the private build uses (its src/app/main.cpp, AppPaths.cpp, CMakeLists,
    // .circleci packaging, FinceptMcpRules / HarnessModels server names).
    const QStringList private_names = {
        QStringLiteral("com.fincept.terminal"), QStringLiteral("Fincept"),       QStringLiteral("FinceptTerminal"),
        QStringLiteral("fincept-terminal"),     QStringLiteral("/opt/fincept-terminal"),
        QStringLiteral("fincept"),              QStringLiteral("fincept-cloud"),  QStringLiteral("fincept-mcp"),
        QStringLiteral("fincept_terminal"),
    };
    for (const char* name : {edition::kDataDirName, edition::kSettingsOrganization, edition::kSettingsApplication,
                             edition::kInstanceKeyPrefix, edition::kDesktopFileName, edition::kBundleId,
                             edition::kLinuxPackageName, edition::kLinuxInstallPrefix, edition::kTempPrefix,
                             edition::kMcpServerName}) {
        QVERIFY2(!private_names.contains(QString::fromLatin1(name), Qt::CaseInsensitive), name);
    }
    // QSettings on macOS keys the plist by organization + application; the
    // organization must not merely differ in case (case-insensitive file systems).
    QVERIFY(QString::fromLatin1(edition::kSettingsOrganization).compare(QLatin1String("Fincept"), Qt::CaseInsensitive) !=
            0);
    // The data root is not inside / a prefix-sibling the private build creates.
    QVERIFY(QString::fromLatin1(edition::kDataDirName).startsWith(QLatin1String("com.fincept.terminal.")));
}

void TstEditionIdentity::legacy_constants_name_the_private_build() {
    QCOMPARE(QString::fromLatin1(edition::kLegacyDataDirName), QStringLiteral("com.fincept.terminal"));
    QCOMPARE(QString::fromLatin1(edition::kLegacySettingsOrganization), QStringLiteral("Fincept"));
    QCOMPARE(QString::fromLatin1(edition::kLegacySettingsApplication), QStringLiteral("FinceptTerminal"));
    QCOMPARE(QString::fromLatin1(edition::kLegacyInstanceKeyPrefix), QStringLiteral("FinceptTerminal"));
    QCOMPARE(QString::fromLatin1(edition::kLegacyLinuxPackageName), QStringLiteral("fincept-terminal"));
    QCOMPARE(QString::fromLatin1(edition::kLegacyLinuxInstallPrefix), QStringLiteral("/opt/fincept-terminal"));
    QVERIFY(std::size(edition::kPrivateBuildFingerprints) >= 5);
    for (const char* rel : edition::kPrivateBuildFingerprints) {
        const QString r = QString::fromLatin1(rel);
        QVERIFY2(!r.startsWith(QLatin1Char('/')) && !r.contains(QLatin1String("..")), rel);
    }
}

void TstEditionIdentity::instance_socket_is_edition_specific() {
    const QString user = QStringLiteral("CORP\\alice");
    for (const QString& profile : {QStringLiteral("default"), QStringLiteral("work")}) {
        const QString ours =
            edition::instance_socket_name(QLatin1String(edition::kInstanceKeyPrefix) + QLatin1Char('-') + profile, user);
        const QString theirs = edition::legacy_instance_socket_name(QStringLiteral("FinceptTerminal-") + profile);
        QVERIFY2(ours != theirs, qPrintable(ours));
        // A different prefix too, so neither edition's name space can ever meet.
        QVERIFY(ours.startsWith(QLatin1String(edition::kTempPrefix) + QLatin1Char('-')));
        QVERIFY(!QRegularExpression(QStringLiteral("^fincept-[0-9a-f]{16}$")).match(ours).hasMatch());
        // Two profiles of this edition stay apart as before.
        QVERIFY(ours != edition::instance_socket_name(QLatin1String(edition::kInstanceKeyPrefix) +
                                                          QStringLiteral("-other"),
                                                      user));
    }
}

void TstEditionIdentity::instance_socket_is_user_specific() {
    const QString key = QLatin1String(edition::kInstanceKeyPrefix) + QStringLiteral("-default");
    const QString a = edition::instance_socket_name(key, QStringLiteral("CORP\\alice"));
    const QString b = edition::instance_socket_name(key, QStringLiteral("CORP\\bob"));
    const QString c = edition::instance_socket_name(key, QStringLiteral("1000"));
    const QString d = edition::instance_socket_name(key, QStringLiteral("1001"));
    QVERIFY(a != b);
    QVERIFY(c != d);
    // The process's own scope is never empty, and stable within a run.
    QVERIFY(!edition::current_user_scope().isEmpty());
    QCOMPARE(edition::current_user_scope(), edition::current_user_scope());
}

void TstEditionIdentity::instance_socket_is_stable_and_short() {
    const QString key = QLatin1String(edition::kInstanceKeyPrefix) + QStringLiteral("-default");
    const QString a = edition::instance_socket_name(key, QStringLiteral("1000"));
    QCOMPARE(a, edition::instance_socket_name(key, QStringLiteral("1000")));
    // <prefix>-<16 hex>: fits the macOS sun_path limit under $TMPDIR (~50 chars).
    QVERIFY2(a.size() <= 40, qPrintable(a));
    QVERIFY(QRegularExpression(QStringLiteral("^[a-z0-9-]+-[0-9a-f]{16}$")).match(a).hasMatch());
    // The key and the user are separated: ("a-b","c") and ("a","b-c") differ.
    QVERIFY(edition::instance_socket_name(QStringLiteral("a-b"), QStringLiteral("c")) !=
            edition::instance_socket_name(QStringLiteral("a"), QStringLiteral("b-c")));
}

void TstEditionIdentity::legacy_socket_matches_the_private_formula() {
    // The private build (its src/app/InstanceLock.cpp) listens on
    // "fincept-" + first 16 hex of SHA-1("FinceptTerminal-<profile>"). Values
    // computed independently (Python hashlib) — LegacyRootAdoption probes this
    // address to see whether that build is running before it moves anything.
    QCOMPARE(edition::legacy_instance_socket_name(QStringLiteral("FinceptTerminal-default")),
             QStringLiteral("fincept-17a7e85e4c101153"));
    QCOMPARE(edition::legacy_instance_socket_name(QStringLiteral("FinceptTerminal-work")),
             QStringLiteral("fincept-09335bfdda3f22c4"));
}

void TstEditionIdentity::temp_and_screenshot_names_escape_foreign_sweeps() {
    // The private build's uninstaller deletes %TEMP%\fincept_*, /tmp/fincept_*,
    // $TMPDIR/fincept_*, fincept-boot.log and ~/fincept_YYYYMMDD_HHMMSS.png
    // (cmd.exe wildcards are case-insensitive).
    const QRegularExpression swept_temp(QStringLiteral("^fincept_"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression swept_shot(QStringLiteral("^fincept_\\d{8}_\\d{6}\\.png$"),
                                        QRegularExpression::CaseInsensitiveOption);
    QVERIFY(!swept_temp.match(QLatin1String(edition::kTempPrefix)).hasMatch());
    QVERIFY(!swept_temp.match(edition::boot_log_file_name()).hasMatch());
    QVERIFY(edition::boot_log_file_name().compare(QLatin1String("fincept-boot.log"), Qt::CaseInsensitive) != 0);
    // core/logging/Logger.cpp builds the same name from the edition id.
    QCOMPARE(edition::boot_log_file_name(), QStringLiteral("fincept-%1-boot.log").arg(QLatin1String(edition::kId)));
    const QString shot = edition::screenshot_file_name(QDateTime(QDate(2026, 11, 17), QTime(9, 5, 7)));
    QCOMPARE(shot, QLatin1String(edition::kTempPrefix) + QStringLiteral("_20261117_090507.png"));
    QVERIFY(!swept_shot.match(shot).hasMatch());
    QVERIFY(!swept_temp.match(shot).hasMatch());
}

void TstEditionIdentity::settings_and_secret_stores_are_separate() {
    // The MCP server's port lives in QSettings (organization/application below)
    // and its bearer token in SecureStorage, inside the data root: neither is a
    // file another program finds by a fixed name. These are the keys of both
    // stores; the private build's are Fincept/FinceptTerminal and
    // com.fincept.terminal.
    const QString windows_key = QStringLiteral("HKCU\\Software\\%1\\%2")
                                    .arg(QLatin1String(edition::kSettingsOrganization),
                                         QLatin1String(edition::kSettingsApplication));
    QVERIFY(windows_key != QStringLiteral("HKCU\\Software\\Fincept\\FinceptTerminal"));
    // The private build's uninstaller deletes HKCU\Software\Fincept recursively
    // and ~/.config/Fincept, ~/.cache/Fincept: this edition's store is not below them.
    QVERIFY(!windows_key.startsWith(QLatin1String("HKCU\\Software\\Fincept\\"), Qt::CaseInsensitive));
    const QString linux_conf = QStringLiteral("~/.config/%1/%2.conf")
                                   .arg(QLatin1String(edition::kSettingsOrganization),
                                        QLatin1String(edition::kSettingsApplication));
    QVERIFY(!linux_conf.startsWith(QLatin1String("~/.config/Fincept/"), Qt::CaseInsensitive));
    QVERIFY(QString::fromLatin1(edition::kDataDirName) != QLatin1String(edition::kLegacyDataDirName));
}

void TstEditionIdentity::instance_key_default_root_is_unchanged() {
    const QString prefix = QLatin1String(edition::kInstanceKeyPrefix);
    const QString def = instance_key::normalise_root(QStringLiteral("C:/Users/alice/AppData/Local/") +
                                                     QLatin1String(edition::kDataDirName));
    // No FINCEPT_DATA_DIR (or a refused one): exactly the key every release used.
    QCOMPARE(instance_key::make(prefix, QStringLiteral("default"), QString(), def), prefix + QStringLiteral("-default"));
    QCOMPARE(instance_key::make(prefix, QStringLiteral("work"), QString(), def), prefix + QStringLiteral("-work"));
    // FINCEPT_DATA_DIR naming the default folder itself: the same data, the same terminal.
    QCOMPARE(instance_key::make(prefix, QStringLiteral("default"), def, def), prefix + QStringLiteral("-default"));
    // So the default root's socket address is the one a running older build listens on.
    const QString user = QStringLiteral("CORP\\alice");
    QCOMPARE(edition::instance_socket_name(instance_key::make(prefix, QStringLiteral("default"), QString(), def), user),
             edition::instance_socket_name(prefix + QStringLiteral("-default"), user));
}

void TstEditionIdentity::instance_key_other_root_is_its_own_instance() {
    const QString prefix = QLatin1String(edition::kInstanceKeyPrefix);
    const QString def = instance_key::normalise_root(QStringLiteral("c:/users/alice/appdata/local/") +
                                                     QLatin1String(edition::kDataDirName));
    const QString a = instance_key::normalise_root(QStringLiteral("D:/ft-scratch/x/data"));
    const QString b = instance_key::normalise_root(QStringLiteral("D:/ft-scratch/y/data"));
    const QString key_def = instance_key::make(prefix, QStringLiteral("default"), QString(), def);
    const QString key_a = instance_key::make(prefix, QStringLiteral("default"), a, def);
    const QString key_b = instance_key::make(prefix, QStringLiteral("default"), b, def);
    QVERIFY(key_a != key_def);
    QVERIFY(key_b != key_def);
    QVERIFY(key_a != key_b);
    QCOMPARE(key_a, instance_key::make(prefix, QStringLiteral("default"), a, def)); // stable
    QVERIFY(QRegularExpression(QStringLiteral("^%1-default@[0-9a-f]{16}$").arg(prefix)).match(key_a).hasMatch());
    // Profile and root both count.
    QVERIFY(instance_key::make(prefix, QStringLiteral("work"), a, def) != key_a);
    // The socket addresses differ too, and keep the short edition form.
    const QString user = QStringLiteral("1000");
    const QString sock_def = edition::instance_socket_name(key_def, user);
    const QString sock_a = edition::instance_socket_name(key_a, user);
    QVERIFY(sock_a != sock_def);
    QVERIFY(sock_a != edition::instance_socket_name(key_b, user));
    QVERIFY(QRegularExpression(QStringLiteral("^[a-z0-9-]+-[0-9a-f]{16}$")).match(sock_a).hasMatch());
    QVERIFY(sock_a.size() <= 40);
    // Never the private build's / legacy address.
    QVERIFY(sock_a != edition::legacy_instance_socket_name(QStringLiteral("FinceptTerminal-default")));
    // The tag is 16 hex digits of the normalised root, nothing of the path itself.
    QCOMPARE(instance_key::root_tag(a).size(), 16);
    QVERIFY(!key_a.contains(QLatin1String("scratch")));
}

void TstEditionIdentity::instance_key_root_spellings_meet() {
    // Separators, "." / "..", trailing slashes; case only where paths ignore it.
    QCOMPARE(instance_key::normalise_root(QStringLiteral("C:\\Scratch\\A\\"), true), QStringLiteral("c:/scratch/a"));
    QCOMPARE(instance_key::normalise_root(QStringLiteral("C:/Scratch/B/../A/."), true), QStringLiteral("c:/scratch/a"));
    QCOMPARE(instance_key::normalise_root(QStringLiteral("  c:/scratch/a  "), true), QStringLiteral("c:/scratch/a"));
    QCOMPARE(instance_key::normalise_root(QStringLiteral("/home/Al/data/"), false), QStringLiteral("/home/Al/data"));
    QVERIFY(instance_key::normalise_root(QStringLiteral("/home/Al/data"), false) !=
            instance_key::normalise_root(QStringLiteral("/home/al/data"), false));
    QCOMPARE(instance_key::normalise_root(QStringLiteral("/"), false), QStringLiteral("/"));
    QCOMPARE(instance_key::normalise_root(QStringLiteral("C:\\"), true), QStringLiteral("c:/"));
    QVERIFY(instance_key::normalise_root(QString()).isEmpty());
    QVERIFY(instance_key::normalise_root(QStringLiteral("   ")).isEmpty());
    // Same folder, same key; the default root spelled differently is still the default.
    const QString prefix = QLatin1String(edition::kInstanceKeyPrefix);
    const QString def = instance_key::normalise_root(QStringLiteral("C:\\Users\\Alice\\AppData\\Local\\X"), true);
    QCOMPARE(instance_key::make(prefix, QStringLiteral("default"),
                                instance_key::normalise_root(QStringLiteral("c:/users/alice/appdata/local/x/"), true),
                                def),
             prefix + QStringLiteral("-default"));
}

void TstEditionIdentity::instance_key_canonical_root_resolves_the_folder() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("data/sub")));
    const QString direct = instance_key::canonical_root(tmp.path() + QStringLiteral("/data"));
    QVERIFY(!direct.isEmpty());
    QCOMPARE(instance_key::canonical_root(tmp.path() + QStringLiteral("/data/sub/..")), direct);
    QCOMPARE(instance_key::canonical_root(QDir::toNativeSeparators(tmp.path() + QStringLiteral("/data/"))), direct);
    // A folder that does not exist yet still has one spelling (its absolute path).
    const QString missing = instance_key::canonical_root(tmp.path() + QStringLiteral("/not-yet"));
    QVERIFY(!missing.isEmpty());
    QVERIFY(missing != direct);
    QVERIFY(instance_key::canonical_root(QString()).isEmpty());
}

QTEST_GUILESS_MAIN(TstEditionIdentity)
#include "tst_edition_identity.moc"
