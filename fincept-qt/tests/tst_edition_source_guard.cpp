// Source guard: no new hard-coded name that the community edition would share
// with the separate private build of Fincept Terminal.
//
// Every per-user / machine-wide name comes from src/core/config/EditionIdentity.h
// (C++) or scripts/fincept_paths.py (Python), both derived from CMakeLists.txt
// "Edition identity". This test scans src/ and scripts/ for the literals that
// made the two editions collide — the shared data root, the Fincept/FinceptTerminal
// QSettings store, the legacy single-instance key, the private build's Linux
// package prefix, home-directory dot-folders, fixed names in the OS temp folder —
// and fails on any occurrence outside the allow-list below.
//
// Comments are ignored. To fix a failure, derive the name from EditionIdentity.h /
// EditionNames.h / AppPaths (C++) or fincept_paths (Python) instead of allow-listing
// it; an allow-list entry is only for code that READS a legacy location on purpose.
//
// (The test class is declared before any raw string literal: moc can miss a
// Q_OBJECT class that follows one. No raw string is passed to a macro.)

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTest>

#include <utility>

#ifndef FINCEPT_SOURCE_DIR
#    error "FINCEPT_SOURCE_DIR (fincept-qt/) must be defined by tests/CMakeLists.txt"
#endif

class TstEditionSourceGuard : public QObject {
    Q_OBJECT

  private slots:
    void no_shared_names_in_sources();
    void rules_catch_known_regressions();
};

namespace {

struct Rule {
    const char* name;
    QRegularExpression rx;
};

QRegularExpression re(const char* pattern) {
    return QRegularExpression(QString::fromUtf8(pattern));
}

QList<Rule> cpp_rules() {
    return {
        {"legacy data root literal", re(R"re("/?com\.fincept\.terminal(?:/[^"]*)?")re")},
        {"legacy QSettings organization",
         re(R"re(QSettings\s*(?:\w+\s*)?[({]\s*(?:QSettings::\w+(?:\(\))?\s*,\s*QSettings::\w+\s*,\s*)?(?:QStringLiteral\(\s*|QLatin1String\(\s*)?"Fincept")re")},
        {"legacy organization/application pair",
         re(R"re("Fincept"\s*\)?\s*,\s*(?:QStringLiteral\(\s*|QLatin1String\(\s*)?"FinceptTerminal")re")},
        {"legacy setOrganizationName", re(R"re(setOrganizationName\(\s*(?:QStringLiteral\(\s*)?"Fincept")re")},
        {"legacy single-instance key", re(R"re("FinceptTerminal-(?:%1)?")re")},
        {"legacy Linux package prefix", re(R"re("/opt/fincept-terminal(?:/[^"]*)?")re")},
        // MCP: the entry name harness configs know this edition by, the token
        // variable and the private build's ports come from EditionNames.h.
        {"pre-split MCP entry name", re(R"re("fincept_terminal")re")},
        {"shared MCP token variable", re(R"re("[^"]*FINCEPT_MCP_TOKEN)re")},
        {"private build MCP port", re(R"re(\b(?:47821|47999)\b)re")},
    };
}

QList<Rule> py_rules() {
    return {
        {"legacy data root literal", re(R"re(['"]/?com\.fincept\.terminal(?:/[^'"]*)?['"])re")},
        {"home .fincept folder", re(R"re(['"](?:~/)?\.fincept['"/])re")},
        {"home .oecd_cache folder", re(R"re(['"]\.oecd_cache['"])re")},
        {"home .cache/fincept-terminal folder", re(R"re(['"]\.cache['"]\s*[,/]\s*['"]fincept-terminal['"])re")},
        {"legacy agents.db", re(R"re(['"]fincept-terminal['"]\s*[,/]\s*['"]agents\.db['"])re")},
        {"fixed name in the temp folder", re(R"re(gettempdir\(\)\s*[,)]\s*(?:/\s*)?['"]fincept_)re")},
        {"legacy Linux package prefix", re(R"re(['"]/opt/fincept-terminal(?:/[^'"]*)?['"])re")},
        {"shared MCP token variable", re(R"re(['"]FINCEPT_MCP_TOKEN['"])re")},
    };
}

// A "fincept_..." name built on the OS temp folder or the home folder (same or
// previous line): the private build's uninstaller deletes %TEMP%\fincept_*,
// /tmp/fincept_* and ~/fincept_<timestamp>.png.
const char* const kTempRuleName = "fixed fincept_ name in the temp or home folder";

bool rule_hit(const QList<Rule>& rules, const QString& line) {
    for (const Rule& r : rules) {
        if (r.rx.match(line).hasMatch())
            return true;
    }
    return false;
}

/// Allowed occurrences: "<path relative to fincept-qt>|<rule name>", each with the
/// reason it reads a legacy location or is owned elsewhere.
const QSet<QString>& allow_list() {
    static const QSet<QString> s = {
        // The legacy names themselves (kLegacy*), for adoption and probes.
        QStringLiteral("src/core/config/EditionIdentity.h|legacy data root literal"),
        QStringLiteral("src/core/config/EditionIdentity.h|legacy Linux package prefix"),
        // The pre-split MCP names, only to recognise (and never reuse) them.
        QStringLiteral("src/core/config/EditionNames.h|pre-split MCP entry name"),
        QStringLiteral("src/core/config/EditionNames.h|shared MCP token variable"),
        QStringLiteral("src/core/config/EditionNames.h|private build MCP port"),
        // The copy source of the one-time script-data copy.
        QStringLiteral("scripts/fincept_paths.py|home .fincept folder"),
        // Report Builder reads the autosave earlier releases left in the temp
        // folder once, when no autosave of its own exists (a copy, never moved).
        QStringLiteral("src/services/report_builder/ReportBuilderService.cpp|") + QLatin1String(kTempRuleName),
    };
    return s;
}

bool is_comment(const QString& trimmed, bool python) {
    if (python)
        return trimmed.startsWith(QLatin1Char('#'));
    return trimmed.startsWith(QLatin1String("//")) || trimmed.startsWith(QLatin1Char('*')) ||
           trimmed.startsWith(QLatin1String("/*"));
}

struct Sample {
    bool python;
    const char* line;
    bool caught;
};

// The lines that made the editions collide (and their fixed forms).
const Sample kSamples[] = {
    {false, R"s(    return local_app_data + "/com.fincept.terminal";)s", true},
    {false, R"s(inline constexpr char kName[] = "com.fincept.terminal";)s", true},
    {false, R"s(    return platform_data_base() + QLatin1Char('/') + QLatin1String(edition::kDataDirName);)s", false},
    {false, R"s(    app.setOrganizationName("Fincept");)s", true},
    {false, R"s(QSettings s(QSettings::defaultFormat(), QSettings::UserScope, QStringLiteral("Fincept"),)s", true},
    {false, R"s(QSettings s("Fincept", "FinceptTerminal");)s", true},
    {false, R"s(    return QSettings(QStringLiteral("Fincept"), QStringLiteral("FinceptTerminal"));)s", true},
    {false, R"s(    QSettings s(QString::fromLatin1(edition::kSettingsOrganization), QString::fromLatin1(edition::kSettingsApplication));)s", false},
    {false, R"s(const QString key = QString("FinceptTerminal-%1").arg(p);)s", true},
    {false, R"s(const QString prefix = QStringLiteral("/opt/fincept-terminal");)s", true},
    {false, R"s(const QString prefix = QStringLiteral("/opt/fincept-terminal-community");)s", false},
    {true, R"s(    CACHE_DIR = Path.home() / '.fincept' / 'rl_models')s", true},
    {true, R"s(    p = os.path.expanduser("~/.fincept/x"))s", true},
    {true, R"s(    self.cache_dir = os.path.join(os.path.expanduser("~"), ".oecd_cache"))s", true},
    {true, R"s(    os.path.join(home, ".cache", "fincept-terminal"))s", true},
    {true, R"s(    db_path = Path(app_data) / 'fincept-terminal' / 'agents.db')s", true},
    {true, R"s(CACHE = os.path.join(tempfile.gettempdir(), "fincept_ff_calendar.json"))s", true},
    {true, R"s(CACHE = fincept_paths.cache_file("economic_calendar", "ff.json"))s", false},
    {true, R"s(    return str(Path(root) / "com.fincept.terminal" / "finagent"))s", true},
    {false, R"s(constexpr const char* kClientServerName = "fincept_terminal";)s", true},
    {false, R"s(    ep.name = QString::fromLatin1(harness::kMcpServerName);)s", false},
    {false, R"s(inline constexpr const char kMcpTokenEnvVar[] = "FINCEPT_MCP_TOKEN";)s", true},
    {false, R"s(         QStringLiteral("Bearer ${FINCEPT_MCP_TOKEN}"),)s", true},
    {false, R"s(    if (port == 47821) return false;)s", true},
    {true, R"s(TOKEN_ENV = "FINCEPT_MCP_TOKEN")s", true},
    {true, R"s(TOKEN_ENV = "FINCEPT_COMMUNITY_MCP_TOKEN")s", false},
};

} // namespace

void TstEditionSourceGuard::no_shared_names_in_sources() {
    const QDir base(QStringLiteral(FINCEPT_SOURCE_DIR));
    QVERIFY2(base.exists(QStringLiteral("src/core/config/EditionIdentity.h")), FINCEPT_SOURCE_DIR);
    const QList<Rule> cpp = cpp_rules();
    const QList<Rule> py = py_rules();
    const QRegularExpression temp_name = re(R"re("/?fincept_)re");
    const QRegularExpression temp_place = re(R"re(TempLocation|tempPath\(\)|homePath\(\))re");

    QStringList violations;
    int files = 0;
    for (const QString& sub : {QStringLiteral("src"), QStringLiteral("scripts")}) {
        QDirIterator it(base.filePath(sub), {QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.py")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString rel = base.relativeFilePath(path);
            if (rel.contains(QLatin1String("/__pycache__/")) || rel.contains(QLatin1String("/tests/")))
                continue;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly))
                continue;
            ++files;
            const bool python = path.endsWith(QLatin1String(".py"));
            const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
            for (int i = 0; i < lines.size(); ++i) {
                const QString& line = lines.at(i);
                const QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || is_comment(trimmed, python))
                    continue;
                QStringList hit_rules;
                for (const Rule& r : python ? py : cpp) {
                    if (r.rx.match(line).hasMatch())
                        hit_rules << QLatin1String(r.name);
                }
                if (!python && temp_name.match(line).hasMatch() &&
                    temp_place.match((i > 0 ? lines.at(i - 1) : QString()) + line).hasMatch())
                    hit_rules << QLatin1String(kTempRuleName);
                for (const QString& rule : std::as_const(hit_rules)) {
                    if (allow_list().contains(rel + QLatin1Char('|') + rule))
                        continue;
                    violations << QStringLiteral("%1:%2 [%3] %4").arg(rel).arg(i + 1).arg(rule, trimmed.left(160));
                }
            }
        }
    }
    const QString too_few = QStringLiteral("only %1 source files found under %2").arg(files).arg(base.path());
    QVERIFY2(files > 500, qPrintable(too_few));
    const QString report = QStringLiteral("\nNames shared with the private build (derive them from EditionIdentity.h / "
                                          "fincept_paths.py instead):\n  ") +
                           violations.join(QLatin1String("\n  "));
    QVERIFY2(violations.isEmpty(), qPrintable(report));
}

void TstEditionSourceGuard::rules_catch_known_regressions() {
    const QList<Rule> cpp = cpp_rules();
    const QList<Rule> py = py_rules();
    for (const Sample& s : kSamples) {
        const bool hit = rule_hit(s.python ? py : cpp, QString::fromUtf8(s.line));
        QVERIFY2(hit == s.caught, s.line);
    }
}

QTEST_GUILESS_MAIN(TstEditionSourceGuard)
#include "tst_edition_source_guard.moc"
