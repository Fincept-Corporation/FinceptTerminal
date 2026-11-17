// tst_harness_detect — where an agent-harness CLI is found (src/services/harness/
// HarnessDetect): fake file systems through DetectContext for Windows and POSIX
// layouts, npm .cmd shims resolved to node + script, PATH parsing, overrides,
// and the `--version` identity check every hit must pass.

#include "services/harness/HarnessDetect.h"
#include "services/harness/adapters/ClaudeAdapter.h"
#include "services/harness/adapters/CodexAdapter.h"
#include "services/harness/adapters/CopilotAdapter.h"
#include "services/harness/adapters/CursorAdapter.h"
#include "services/harness/adapters/GeminiAdapter.h"
#include "services/harness/adapters/GrokAdapter.h"
#include "services/harness/adapters/QwenAdapter.h"

#include <QHash>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QtTest>

#include <memory>

using namespace fincept::harness;

namespace {

/// An in-memory file system. Windows rules compare paths case-insensitively.
struct FakeFs {
    bool windows = false;
    QSet<QString> files;
    QSet<QString> not_executable;
    QHash<QString, QByteArray> contents;
    QHash<QString, QStringList> subdirs; // dir -> sub-directory names

    QString key(const QString& p) const { return windows ? p.toLower() : p; }
    void add(const QString& p, const QByteArray& content = {}) {
        files.insert(key(p));
        if (!content.isEmpty())
            contents.insert(key(p), content);
    }
    bool has(const QString& p) const { return files.contains(key(p)); }
};

DetectContext make_ctx(const FakeFs& fs, const QString& home, const QHash<QString, QString>& vars,
                       const QString& override_path = {}) {
    auto shared = std::make_shared<FakeFs>(fs);
    DetectContext ctx;
    ctx.windows = fs.windows;
    ctx.home = home;
    ctx.override_path = override_path;
    QProcessEnvironment env;
    for (auto it = vars.cbegin(); it != vars.cend(); ++it)
        env.insert(it.key(), it.value());
    ctx.env = env;
    ctx.is_file = [shared](const QString& p) { return shared->has(p); };
    ctx.is_executable = [shared](const QString& p) {
        return shared->has(p) && !shared->not_executable.contains(shared->key(p));
    };
    ctx.list_dir = [shared](const QString& dir, const QString& glob) {
        const QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(glob));
        QStringList out;
        for (const QString& name : shared->subdirs.value(dir)) {
            if (rx.match(name).hasMatch())
                out << name;
        }
        return out;
    };
    ctx.read_file = [shared](const QString& p, qint64 max_bytes) {
        return shared->contents.value(shared->key(p)).left(max_bytes);
    };
    return ctx;
}

// The launcher npm (cmd-shim) writes for a global package, CRLF as on disk.
const QByteArray kNpmClaudeShim = QByteArray(
    "@ECHO off\r\n"
    "GOTO start\r\n"
    ":find_dp0\r\n"
    "SET dp0=%~dp0\r\n"
    "EXIT /b\r\n"
    ":start\r\n"
    "SETLOCAL\r\n"
    "CALL :find_dp0\r\n"
    "\r\n"
    "IF EXIST \"%dp0%\\node.exe\" (\r\n"
    "  SET \"_prog=%dp0%\\node.exe\"\r\n"
    ") ELSE (\r\n"
    "  SET \"_prog=node\"\r\n"
    "  SET PATHEXT=%PATHEXT:;.JS;=;%\r\n"
    ")\r\n"
    "\r\n"
    "endLocal & goto #_undefined_# 2>NUL || title %COMSPEC% & \"%_prog%\"  "
    "\"%dp0%\\node_modules\\@anthropic-ai\\claude-code\\cli.js\" %*\r\n");

// The older / pnpm style: %~dp0 and two launch lines.
const QByteArray kPnpmCodexShim = QByteArray(
    "@SETLOCAL\r\n"
    "@IF EXIST \"%~dp0\\node.exe\" (\r\n"
    "  \"%~dp0\\node.exe\"  \"%~dp0\\global\\5\\node_modules\\@openai\\codex\\bin\\codex.js\" %*\r\n"
    ") ELSE (\r\n"
    "  @SET PATHEXT=%PATHEXT:;.JS;=;%\r\n"
    "  node  \"%~dp0\\global\\5\\node_modules\\@openai\\codex\\bin\\codex.js\" %*\r\n"
    ")\r\n");

const QString kHome = QStringLiteral("C:/Users/me");

QHash<QString, QString> windows_env(const QString& path) {
    return {
        {QStringLiteral("PATH"), path},
        {QStringLiteral("PATHEXT"), QStringLiteral(".COM;.EXE;.BAT;.CMD;.VBS;.JS")},
        {QStringLiteral("APPDATA"), QStringLiteral("C:\\Users\\me\\AppData\\Roaming")},
        {QStringLiteral("LOCALAPPDATA"), QStringLiteral("C:\\Users\\me\\AppData\\Local")},
    };
}

} // namespace

class TestHarnessDetect : public QObject {
    Q_OBJECT

  private slots:
    // ── Windows ──────────────────────────────────────────────────────────────
    void windows_native_exe_preferred_over_npm_shim();
    void windows_shim_prefers_node_beside_it();
    void windows_pnpm_style_shim();
    void windows_unrecognised_shim_needs_cmd();
    void windows_shim_to_native_exe();
    void windows_shim_without_node_stays_cmd();
    void windows_path_with_spaces_quotes_and_trailing_separators();
    void windows_pathext_prefers_exe_in_same_dir();
    void windows_shared_dirs_and_codex_installer();
    void windows_paths_compare_case_insensitively();

    // ── POSIX ────────────────────────────────────────────────────────────────
    void posix_nvm_newest_first();
    void posix_order_path_adapter_shared();
    void posix_skips_non_executable();

    // ── Override ─────────────────────────────────────────────────────────────
    void override_existing_is_only_candidate();
    void override_shim_is_resolved();
    void override_missing_falls_back_to_auto();

    // ── Identity ─────────────────────────────────────────────────────────────
    void version_line_must_match_adapter();

    // ── Grok Build, Cursor's `agent`, yargs versions ─────────────────────────
    void grok_in_home_bin_windows();
    void grok_in_home_bin_posix();
    void grok_agent_alias_alone_is_accepted();
    void cursor_rejects_grok_agent();
    void gemini_qwen_copilot_bare_versions();

    // ── Helpers ──────────────────────────────────────────────────────────────
    void search_path_and_file_names();
};

void TestHarnessDetect::windows_native_exe_preferred_over_npm_shim() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/claude.cmd"), kNpmClaudeShim);
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/node_modules/@anthropic-ai/claude-code/cli.js"));
    fs.add(QStringLiteral("C:/Program Files/nodejs/node.exe"));
    fs.add(QStringLiteral("C:/Users/me/.local/bin/claude.exe"));
    // The npm folder comes first on PATH — the native binary still wins.
    const DetectContext ctx = make_ctx(fs, kHome,
                                       windows_env(QStringLiteral("C:\\Users\\me\\AppData\\Roaming\\npm;"
                                                                  "C:\\Program Files\\nodejs;C:\\Users\\me\\.local\\bin")));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 2);
    QCOMPARE(c[0].path, QStringLiteral("C:/Users/me/.local/bin/claude.exe"));
    QCOMPARE(c[0].via, Installation::Native);
    QVERIFY(!c[0].path_overridden);
    QCOMPARE(c[1].path, QStringLiteral("C:/Users/me/AppData/Roaming/npm/claude.cmd"));
    QCOMPARE(c[1].via, Installation::NodeScript);
    QCOMPARE(c[1].node_path, QStringLiteral("C:/Program Files/nodejs/node.exe"));
    QCOMPARE(c[1].script_path,
             QStringLiteral("C:/Users/me/AppData/Roaming/npm/node_modules/@anthropic-ai/claude-code/cli.js"));
}

void TestHarnessDetect::windows_shim_prefers_node_beside_it() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/claude.cmd"), kNpmClaudeShim);
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/node_modules/@anthropic-ai/claude-code/cli.js"));
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/node.exe"));
    fs.add(QStringLiteral("C:/Program Files/nodejs/node.exe"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QStringLiteral("C:\\Program Files\\nodejs")));
    const auto inst = resolve_npm_shim(QStringLiteral("C:\\Users\\me\\AppData\\Roaming\\npm\\claude.cmd"), ctx);
    QVERIFY(inst.has_value());
    QCOMPARE(inst->via, Installation::NodeScript);
    QCOMPARE(inst->path, QStringLiteral("C:/Users/me/AppData/Roaming/npm/claude.cmd"));
    QCOMPARE(inst->node_path, QStringLiteral("C:/Users/me/AppData/Roaming/npm/node.exe"));
}

void TestHarnessDetect::windows_pnpm_style_shim() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/AppData/Local/pnpm/codex.cmd"), kPnpmCodexShim);
    fs.add(QStringLiteral("C:/Users/me/AppData/Local/pnpm/global/5/node_modules/@openai/codex/bin/codex.js"));
    fs.add(QStringLiteral("C:/nvm4w/nodejs/node.exe"));
    QHash<QString, QString> env = windows_env(QString());
    env.insert(QStringLiteral("NVM_SYMLINK"), QStringLiteral("C:\\nvm4w\\nodejs"));
    const DetectContext ctx = make_ctx(fs, kHome, env);
    const QVector<Installation> c = find_candidates(CodexAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].via, Installation::NodeScript);
    QCOMPARE(c[0].node_path, QStringLiteral("C:/nvm4w/nodejs/node.exe")); // node found through NVM_SYMLINK
    QCOMPARE(c[0].script_path,
             QStringLiteral("C:/Users/me/AppData/Local/pnpm/global/5/node_modules/@openai/codex/bin/codex.js"));
}

void TestHarnessDetect::windows_unrecognised_shim_needs_cmd() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/tools/claude.cmd"), QByteArray("@echo off\r\n\"C:\\real\\claude.exe\" %*\r\n"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QStringLiteral("C:\\tools")));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].via, Installation::CmdShim);
    QCOMPARE(c[0].path, QStringLiteral("C:/tools/claude.cmd"));
    QVERIFY(c[0].node_path.isEmpty());
}

void TestHarnessDetect::windows_shim_to_native_exe() {
    // cmd-shim for a package whose bin is a native binary: no node involved.
    const QByteArray exe_shim("@ECHO off\r\nGOTO start\r\n:find_dp0\r\nSET dp0=%~dp0\r\nEXIT /b\r\n:start\r\n"
                              "SETLOCAL\r\nCALL :find_dp0\r\n"
                              "\"%dp0%\\node_modules\\@openai\\codex\\vendor\\codex.exe\"   %*\r\n");
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/codex.cmd"), exe_shim);
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/node_modules/@openai/codex/vendor/codex.exe"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QString()));
    const QVector<Installation> c = find_candidates(CodexAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].via, Installation::Native);
    QCOMPARE(c[0].path, QStringLiteral("C:/Users/me/AppData/Roaming/npm/node_modules/@openai/codex/vendor/codex.exe"));
}

void TestHarnessDetect::windows_shim_without_node_stays_cmd() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/claude.cmd"), kNpmClaudeShim);
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/node_modules/@anthropic-ai/claude-code/cli.js"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QString()));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].via, Installation::CmdShim); // no node anywhere: cmd.exe is the last resort

    // Script missing → not resolvable either.
    FakeFs fs2;
    fs2.windows = true;
    fs2.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/claude.cmd"), kNpmClaudeShim);
    fs2.add(QStringLiteral("C:/Program Files/nodejs/node.exe"));
    const DetectContext ctx2 = make_ctx(fs2, kHome, windows_env(QStringLiteral("C:\\Program Files\\nodejs")));
    QVERIFY(!resolve_npm_shim(QStringLiteral("C:/Users/me/AppData/Roaming/npm/claude.cmd"), ctx2).has_value());
}

void TestHarnessDetect::windows_path_with_spaces_quotes_and_trailing_separators() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Program Files/Claude Tools/claude.exe"));
    const QString path = QStringLiteral("\"C:\\Program Files\\Claude Tools\\\";;C:\\Windows\\system32\\;relative\\dir;");
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(path));
    QCOMPARE(split_search_path(path, true),
             QStringList({QStringLiteral("C:/Program Files/Claude Tools"), QStringLiteral("C:/Windows/system32")}));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path, QStringLiteral("C:/Program Files/Claude Tools/claude.exe"));
}

void TestHarnessDetect::windows_pathext_prefers_exe_in_same_dir() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/tools/claude.cmd")); // empty: not an npm shim
    fs.add(QStringLiteral("C:/tools/claude.exe"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QStringLiteral("C:\\tools")));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 2);
    QCOMPARE(c[0].path, QStringLiteral("C:/tools/claude.exe"));
    QCOMPARE(c[0].via, Installation::Native);
    QCOMPARE(c[1].path, QStringLiteral("C:/tools/claude.cmd"));
    QCOMPARE(c[1].via, Installation::CmdShim);
}

void TestHarnessDetect::windows_shared_dirs_and_codex_installer() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/AppData/Local/Microsoft/WinGet/Links/claude.exe"));
    fs.add(QStringLiteral("C:/Users/me/scoop/shims/claude.exe"));
    fs.add(QStringLiteral("C:/Users/me/AppData/Local/Volta/bin/claude.exe"));
    fs.add(QStringLiteral("C:/nvm4w/nodejs/claude.cmd"), kNpmClaudeShim);
    fs.add(QStringLiteral("C:/nvm4w/nodejs/node_modules/@anthropic-ai/claude-code/cli.js"));
    fs.add(QStringLiteral("C:/nvm4w/nodejs/node.exe"));
    fs.add(QStringLiteral("C:/Users/me/AppData/Local/Programs/OpenAI/Codex/bin/codex.exe"));
    QHash<QString, QString> env = windows_env(QString()); // nothing on PATH
    env.insert(QStringLiteral("NVM_SYMLINK"), QStringLiteral("C:\\nvm4w\\nodejs"));
    const DetectContext ctx = make_ctx(fs, kHome, env);

    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 4);
    QCOMPARE(c[0].path, QStringLiteral("C:/Users/me/AppData/Local/Microsoft/WinGet/Links/claude.exe"));
    QCOMPARE(c[1].path, QStringLiteral("C:/Users/me/scoop/shims/claude.exe"));
    QCOMPARE(c[2].path, QStringLiteral("C:/Users/me/AppData/Local/Volta/bin/claude.exe"));
    QCOMPARE(c[3].path, QStringLiteral("C:/nvm4w/nodejs/claude.cmd"));
    QCOMPARE(c[3].via, Installation::NodeScript);
    QCOMPARE(c[3].node_path, QStringLiteral("C:/nvm4w/nodejs/node.exe"));

    const QVector<Installation> x = find_candidates(CodexAdapter(), ctx);
    QCOMPARE(x.size(), 1);
    QCOMPARE(x[0].path, QStringLiteral("C:/Users/me/AppData/Local/Programs/OpenAI/Codex/bin/codex.exe"));
}

void TestHarnessDetect::windows_paths_compare_case_insensitively() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/.local/bin/claude.exe"));
    // Same folder twice: on PATH in other case, and in the shared list via the home dir.
    QHash<QString, QString> env = windows_env(QStringLiteral("C:\\USERS\\ME\\.LOCAL\\BIN"));
    env.remove(QStringLiteral("PATH"));
    env.insert(QStringLiteral("Path"), QStringLiteral("C:\\USERS\\ME\\.LOCAL\\BIN")); // Windows spells it "Path"
    const DetectContext ctx = make_ctx(fs, kHome, env);
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path.toLower(), QStringLiteral("c:/users/me/.local/bin/claude.exe"));
}

void TestHarnessDetect::posix_nvm_newest_first() {
    FakeFs fs;
    fs.add(QStringLiteral("/home/me/.nvm/versions/node/v18.20.4/bin/claude"));
    fs.add(QStringLiteral("/home/me/.nvm/versions/node/v20.11.1/bin/claude"));
    fs.subdirs.insert(QStringLiteral("/home/me/.nvm/versions/node"),
                      {QStringLiteral("v18.20.4"), QStringLiteral("v20.11.1"), QStringLiteral("v9.11.2")});
    const DetectContext ctx = make_ctx(fs, QStringLiteral("/home/me"), {{QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin")}});
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 2);
    QCOMPARE(c[0].path, QStringLiteral("/home/me/.nvm/versions/node/v20.11.1/bin/claude"));
    QCOMPARE(c[1].path, QStringLiteral("/home/me/.nvm/versions/node/v18.20.4/bin/claude"));
}

void TestHarnessDetect::posix_order_path_adapter_shared() {
    FakeFs fs;
    fs.add(QStringLiteral("/usr/bin/claude"));
    fs.add(QStringLiteral("/home/linuxbrew/.linuxbrew/bin/claude"));
    fs.add(QStringLiteral("/home/me/.volta/bin/claude"));
    fs.add(QStringLiteral("/home/me/.claude/local/claude")); // the adapter's own location
    const DetectContext ctx = make_ctx(fs, QStringLiteral("/home/me"), {{QStringLiteral("PATH"), QStringLiteral("/usr/bin")}});
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 4);
    QCOMPARE(c[0].path, QStringLiteral("/usr/bin/claude"));                       // PATH
    QCOMPARE(c[1].path, QStringLiteral("/home/me/.claude/local/claude"));         // adapter
    QCOMPARE(c[2].path, QStringLiteral("/home/linuxbrew/.linuxbrew/bin/claude")); // shared list, in order
    QCOMPARE(c[3].path, QStringLiteral("/home/me/.volta/bin/claude"));
    for (const Installation& i : c)
        QCOMPARE(i.via, Installation::Native); // no shims outside Windows
}

void TestHarnessDetect::posix_skips_non_executable() {
    FakeFs fs;
    fs.add(QStringLiteral("/home/me/.local/bin/codex"));
    fs.not_executable.insert(QStringLiteral("/home/me/.local/bin/codex"));
    fs.add(QStringLiteral("/usr/local/bin/codex"));
    const DetectContext ctx = make_ctx(fs, QStringLiteral("/home/me"), {});
    const QVector<Installation> c = find_candidates(CodexAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path, QStringLiteral("/usr/local/bin/codex"));
}

void TestHarnessDetect::override_existing_is_only_candidate() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Tools/claude-dev/claude.exe"));
    fs.add(QStringLiteral("C:/Users/me/.local/bin/claude.exe"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QStringLiteral("C:\\Users\\me\\.local\\bin")),
                                       QStringLiteral("C:\\Tools\\claude-dev\\claude.exe"));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path, QStringLiteral("C:/Tools/claude-dev/claude.exe"));
    QVERIFY(c[0].path_overridden);
}

void TestHarnessDetect::override_shim_is_resolved() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("D:/npm/claude.cmd"), kNpmClaudeShim);
    fs.add(QStringLiteral("D:/npm/node_modules/@anthropic-ai/claude-code/cli.js"));
    fs.add(QStringLiteral("D:/npm/node.exe"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QString()), QStringLiteral("D:\\npm\\claude.cmd"));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].via, Installation::NodeScript);
    QVERIFY(c[0].path_overridden);
    QCOMPARE(c[0].script_path, QStringLiteral("D:/npm/node_modules/@anthropic-ai/claude-code/cli.js"));
}

void TestHarnessDetect::override_missing_falls_back_to_auto() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/.local/bin/claude.exe"));
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QString()), QStringLiteral("C:\\nope\\claude.exe"));
    const QVector<Installation> c = find_candidates(ClaudeAdapter(), ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path, QStringLiteral("C:/Users/me/.local/bin/claude.exe"));
    QVERIFY(!c[0].path_overridden);

    // Nothing else installed: no candidate at all.
    FakeFs empty;
    empty.windows = true;
    const DetectContext ctx2 = make_ctx(empty, kHome, windows_env(QString()), QStringLiteral("C:\\nope\\claude.exe"));
    QVERIFY(find_candidates(ClaudeAdapter(), ctx2).isEmpty());
}

void TestHarnessDetect::version_line_must_match_adapter() {
    const ClaudeAdapter claude;
    const CodexAdapter codex;

    Installation a;
    QCOMPARE(check_version(claude, a, 0, true, QStringLiteral("2.1.325 (Claude Code)\r\n")), VersionVerdict::Accepted);
    QCOMPARE(a.version, QStringLiteral("2.1.325"));
    QCOMPARE(a.version_line, QStringLiteral("2.1.325 (Claude Code)"));

    // An update notice before the version line does not matter.
    Installation b;
    QCOMPARE(check_version(claude, b, 0, true, QStringLiteral("A new version is available\n2.1.330 (Claude Code)")),
             VersionVerdict::Accepted);
    QCOMPARE(b.version, QStringLiteral("2.1.330"));

    // Another program under the expected name: Grok's `agent`, or Codex where Claude is expected.
    Installation g;
    QCOMPARE(check_version(claude, g, 0, true, QStringLiteral("grok 1.0.41")), VersionVerdict::Rejected);
    QVERIFY(g.version.isEmpty());
    QCOMPARE(g.version_line, QStringLiteral("grok 1.0.41"));
    Installation x;
    QCOMPARE(check_version(claude, x, 0, true, QStringLiteral("codex-cli 0.160.0")), VersionVerdict::Rejected);
    Installation y;
    QCOMPARE(check_version(codex, y, 0, true, QStringLiteral("codex-cli 0.160.0\n")), VersionVerdict::Accepted);
    QCOMPARE(y.version, QStringLiteral("0.160.0"));
    Installation z;
    QCOMPARE(check_version(codex, z, 0, true, QStringLiteral("2.1.325 (Claude Code)")), VersionVerdict::Rejected);

    // No usable answer is not a rejection.
    Installation u1;
    QCOMPARE(check_version(claude, u1, -1, false, QString()), VersionVerdict::Unverified);
    Installation u2;
    QCOMPARE(check_version(claude, u2, 1, true, QStringLiteral("error: cannot open config")), VersionVerdict::Unverified);
    Installation u3;
    QCOMPARE(check_version(claude, u3, 0, true, QStringLiteral("  \n")), VersionVerdict::Unverified);
}

// `grok --version` and `agent --version` of grok 1.0.41 (same binary), recorded on Windows.
const QString kGrokVersionLine = QStringLiteral("grok 1.0.41 (4220f3b224a6) [stable]");

void TestHarnessDetect::grok_in_home_bin_windows() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/.grok/bin/grok.exe"));
    fs.add(QStringLiteral("C:/Users/me/.grok/bin/agent.exe"));
    // Not on PATH (the installer's folder): found through the adapter's own candidates.
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QStringLiteral("C:\\Windows\\system32")));
    const GrokAdapter grok;
    const QVector<Installation> c = find_candidates(grok, ctx);
    QCOMPARE(c.size(), 2);
    QCOMPARE(c[0].path, QStringLiteral("C:/Users/me/.grok/bin/grok.exe"));
    QCOMPARE(c[1].path, QStringLiteral("C:/Users/me/.grok/bin/agent.exe"));
    for (const Installation& i : c)
        QCOMPARE(i.via, Installation::Native);
    Installation inst = c[0];
    QCOMPARE(check_version(grok, inst, 0, true, kGrokVersionLine + QStringLiteral("\n")), VersionVerdict::Accepted);
    QCOMPARE(inst.version, QStringLiteral("1.0.41"));
    QCOMPARE(inst.version_line, kGrokVersionLine);

    // On PATH as well: PATH first, the same file is not listed twice.
    const DetectContext on_path = make_ctx(fs, kHome, windows_env(QStringLiteral("C:\\Users\\me\\.grok\\bin")));
    const QVector<Installation> p = find_candidates(grok, on_path);
    QCOMPARE(p.size(), 2);
    QCOMPARE(p[0].path, QStringLiteral("C:/Users/me/.grok/bin/grok.exe"));
}

void TestHarnessDetect::grok_in_home_bin_posix() {
    FakeFs fs;
    fs.add(QStringLiteral("/home/me/.grok/bin/grok"));
    const DetectContext ctx = make_ctx(fs, QStringLiteral("/home/me"), {{QStringLiteral("PATH"), QStringLiteral("/usr/bin")}});
    const GrokAdapter grok;
    const QVector<Installation> c = find_candidates(grok, ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path, QStringLiteral("/home/me/.grok/bin/grok"));
    QCOMPARE(grok.candidate_paths(ctx),
             (QStringList{QStringLiteral("/home/me/.grok/bin/grok"), QStringLiteral("/home/me/.grok/bin/agent")}));
    // Another program called grok is not Grok Build.
    Installation other = c[0];
    QCOMPARE(check_version(grok, other, 0, true, QStringLiteral("2.1.325 (Claude Code)")), VersionVerdict::Rejected);
    Installation banner = c[0];
    QCOMPARE(check_version(grok, banner, 0, true, QStringLiteral("Grok Build TUI")), VersionVerdict::Rejected);
}

void TestHarnessDetect::grok_agent_alias_alone_is_accepted() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/.grok/bin/agent.exe")); // grok.exe missing
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QString()));
    const GrokAdapter grok;
    const QVector<Installation> c = find_candidates(grok, ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path, QStringLiteral("C:/Users/me/.grok/bin/agent.exe"));
    Installation inst = c[0];
    // Accepted only because its --version names grok.
    QCOMPARE(check_version(grok, inst, 0, true, kGrokVersionLine), VersionVerdict::Accepted);
    QCOMPARE(inst.version, QStringLiteral("1.0.41"));
    Installation cursor_agent = c[0];
    QCOMPARE(check_version(grok, cursor_agent, 0, true, QStringLiteral("Cursor Agent 2025.11.06")),
             VersionVerdict::Rejected);
}

void TestHarnessDetect::cursor_rejects_grok_agent() {
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/.grok/bin/agent.exe"));
    // ~/.grok/bin on PATH (as the Grok installer leaves it): `agent` resolves to Grok.
    const DetectContext ctx = make_ctx(fs, kHome, windows_env(QStringLiteral("C:\\Users\\me\\.grok\\bin")));
    const CursorAdapter cursor;
    const QVector<Installation> c = find_candidates(cursor, ctx);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].path, QStringLiteral("C:/Users/me/.grok/bin/agent.exe"));
    Installation inst = c[0];
    QCOMPARE(check_version(cursor, inst, 0, true, kGrokVersionLine), VersionVerdict::Rejected);
    QVERIFY(inst.version.isEmpty());
    QCOMPARE(inst.version_line, kGrokVersionLine);
    QVERIFY(cursor.parse_version(QStringLiteral("grok-cursor 1.0.0")).isEmpty()); // grok anywhere: never Cursor
    // Only output naming cursor is accepted (its exact text is unconfirmed).
    Installation named = c[0];
    QCOMPARE(check_version(cursor, named, 0, true, QStringLiteral("Cursor Agent 2025.11.06")), VersionVerdict::Accepted);
    QCOMPARE(named.version, QStringLiteral("2025.11.06"));
    Installation bare = c[0];
    QCOMPARE(check_version(cursor, bare, 0, true, QStringLiteral("2025.11.06-7ae6800")), VersionVerdict::Rejected);
}

void TestHarnessDetect::gemini_qwen_copilot_bare_versions() {
    const GeminiAdapter gemini;
    const QwenAdapter qwen;
    const CopilotAdapter copilot;
    // yargs prints a bare version (assumed); a line naming the tool is accepted too.
    QCOMPARE(gemini.parse_version(QStringLiteral("0.30.0")), QStringLiteral("0.30.0"));
    QCOMPARE(gemini.parse_version(QStringLiteral("v0.31.0-preview.1")), QStringLiteral("0.31.0"));
    QCOMPARE(gemini.parse_version(QStringLiteral("gemini-cli 0.30.0")), QStringLiteral("0.30.0"));
    QCOMPARE(qwen.parse_version(QStringLiteral("0.5.0")), QStringLiteral("0.5.0"));
    QCOMPARE(qwen.parse_version(QStringLiteral("qwen-code 0.5.0")), QStringLiteral("0.5.0"));
    QCOMPARE(copilot.parse_version(QStringLiteral("0.0.354")), QStringLiteral("0.0.354"));
    QCOMPARE(copilot.parse_version(QStringLiteral("GitHub Copilot CLI 0.0.354")), QStringLiteral("0.0.354"));
    // Another harness answering under this name is rejected.
    for (const IHarnessAdapter* a : {static_cast<const IHarnessAdapter*>(&gemini), static_cast<const IHarnessAdapter*>(&qwen),
                                     static_cast<const IHarnessAdapter*>(&copilot)}) {
        QVERIFY(a->parse_version(kGrokVersionLine).isEmpty());
        QVERIFY(a->parse_version(QStringLiteral("2.1.325 (Claude Code)")).isEmpty());
        QVERIFY(a->parse_version(QStringLiteral("codex-cli 0.160.0")).isEmpty());
        QVERIFY(a->parse_version(QStringLiteral("Usage: something [options]")).isEmpty());
        QVERIFY(a->parse_version(QStringLiteral("1.2")).isEmpty());
    }
    QVERIFY(gemini.parse_version(QStringLiteral("qwen 0.5.0")).isEmpty());
    QVERIFY(qwen.parse_version(QStringLiteral("gemini 0.30.0")).isEmpty());

    // A version notice line before the bare version.
    Installation g;
    QCOMPARE(check_version(gemini, g, 0, true, QStringLiteral("Update available!\n0.30.0\n")), VersionVerdict::Accepted);
    QCOMPARE(g.version, QStringLiteral("0.30.0"));

    // npm shims for these tools are found in the shared list.
    FakeFs fs;
    fs.windows = true;
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/gemini.cmd"), QByteArray(
        "@ECHO off\r\n\"%dp0%\\node.exe\" \"%dp0%\\node_modules\\@google\\gemini-cli\\dist\\index.js\" %*\r\n"));
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/node_modules/@google/gemini-cli/dist/index.js"));
    fs.add(QStringLiteral("C:/Users/me/AppData/Roaming/npm/node.exe"));
    const QVector<Installation> c = find_candidates(gemini, make_ctx(fs, kHome, windows_env(QString())));
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].via, Installation::NodeScript);
}

void TestHarnessDetect::search_path_and_file_names() {
    QCOMPARE(split_search_path(QStringLiteral("/usr/bin::/usr/local/bin/:relative:/usr/bin"), false),
             QStringList({QStringLiteral("/usr/bin"), QStringLiteral("/usr/local/bin")}));

    FakeFs fs;
    fs.windows = true;
    DetectContext win = make_ctx(fs, kHome, {{QStringLiteral("PATHEXT"), QStringLiteral(".COM;.EXE")}});
    QCOMPARE(executable_file_names(QStringLiteral("claude"), win), QStringList({QStringLiteral("claude.exe")}));
    win = make_ctx(fs, kHome, {});
    QCOMPARE(executable_file_names(QStringLiteral("claude"), win),
             QStringList({QStringLiteral("claude.exe"), QStringLiteral("claude.cmd"), QStringLiteral("claude.bat")}));
    QCOMPARE(executable_file_names(QStringLiteral("claude.exe"), win), QStringList({QStringLiteral("claude.exe")}));
    FakeFs posix;
    const DetectContext px = make_ctx(posix, QStringLiteral("/home/me"), {});
    QCOMPARE(executable_file_names(QStringLiteral("claude"), px), QStringList({QStringLiteral("claude")}));

    QVERIFY(is_cmd_shim_path(QStringLiteral("C:/x/CLAUDE.CMD")));
    QVERIFY(is_cmd_shim_path(QStringLiteral("C:/x/claude.bat")));
    QVERIFY(!is_cmd_shim_path(QStringLiteral("C:/x/claude.exe")));
    QCOMPARE(normalize_detect_path(QStringLiteral("  \"C:\\a\\\\b\\..\\c\\\" "), true), QStringLiteral("C:/a/c"));
}

QTEST_GUILESS_MAIN(TestHarnessDetect)
#include "tst_harness_detect.moc"
