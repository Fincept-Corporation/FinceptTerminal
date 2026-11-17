// tst_harness_commands — what the harness adapters (src/services/harness/adapters)
// put on a command line, on stdin and in per-run files, against golden lists that
// match the Agent Workbench's command lines as they were before the adapters
// (Claude Code / Codex × read-only analysis / workspace edits × new / resume ×
// terminal tools on / off); that the MCP token never appears in any of it; the
// version-gated Claude flag; the launch spec and the child environment; and the
// sign-in / model-list parsers against small hand-written fixtures (no identity
// fields in them).

#include "services/coding_agents/CodingAgentPrompts.h"
#include "services/harness/HarnessLaunch.h"
#include "services/harness/adapters/ClaudeAdapter.h"
#include "services/harness/adapters/CodexAdapter.h"
#include "services/harness/adapters/CopilotAdapter.h"
#include "services/harness/adapters/CursorAdapter.h"
#include "services/harness/adapters/GeminiAdapter.h"
#include "services/harness/adapters/GrokAdapter.h"
#include "services/harness/adapters/QwenAdapter.h"
#include "services/harness/McpLegacyEntry.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QtTest>

#include <memory>
#include <vector>

using namespace fincept::harness;

namespace {

const QString kUrl = QStringLiteral("http://127.0.0.1:41720/mcp");
// A stand-in for the MCP server's token. Adapters never receive it; it must not
// show up anywhere in a LaunchPlan.
const QString kToken = QStringLiteral("tok_7f3a9c2e5b1d4f60a8e2c4d6");
const QString kSystem = QStringLiteral("SYSTEM CONTEXT");
const QString kPrompt = QStringLiteral("Summarise AAPL");
const QString kTempDir = QStringLiteral("/cache/coding_agents");
const QString kRunId = QStringLiteral("0123abcd-ffff-4444-8888-000000000000");
const QString kClaudeSession = QStringLiteral("sess-1234");
const QString kCodexThread = QStringLiteral("thread-5678");

Installation installed(const QString& path, const QString& version) {
    Installation i;
    i.path = path;
    i.version = version;
    return i;
}

/// What CodingAgentService::launch() hands the adapter for a run.
RunSpec run_spec(bool workspace_edits, const QString& resume_id, bool tools, const QString& server_name) {
    RunSpec s;
    s.prompt = kPrompt;
    s.system_prompt = kSystem;
    s.cwd = QStringLiteral("/work/run");
    s.resume_session_id = resume_id;
    s.permission = workspace_edits ? PermissionMode::WorkspaceWrite : PermissionMode::ReadOnly;
    if (tools) {
        McpEndpoint ep;
        ep.name = server_name;
        ep.url = kUrl;
        ep.token_env_var = QString::fromLatin1(kMcpTokenEnvVar);
        ep.tool_timeout_s = 300;
        s.mcp = ep;
    }
    s.run_id = kRunId;
    s.temp_dir = kTempDir;
    return s;
}

void verify_no_secret(const LaunchPlan& plan) {
    static const QRegularExpression literal_bearer(QStringLiteral(R"(Bearer\s+[^$\s])"));
    for (const QString& a : plan.args) {
        QVERIFY2(!a.contains(kToken), qPrintable(a));
        QVERIFY2(!a.contains(literal_bearer), qPrintable(a));
    }
    QVERIFY(!plan.stdin_payload.contains(kToken.toUtf8()));
    for (const TempFile& f : plan.temp_files) {
        QVERIFY(!f.content.contains(kToken.toUtf8()));
        QVERIFY(!QString::fromUtf8(f.content).contains(literal_bearer));
    }
    for (auto it = plan.env_set.cbegin(); it != plan.env_set.cend(); ++it)
        QVERIFY(!it.value().contains(kToken));
}

// The prompt and context wrapped into one prompt for harnesses without a system-prompt flag.
const QString kWrapped =
    QStringLiteral("<context>\nSYSTEM CONTEXT\n</context>\n\n<user_request>\nSummarise AAPL\n</user_request>\n");
const QString kGeminiSession = QStringLiteral("c6b5e3a2-1f4d-4b8e-9a0c-2d7e8f9a1b3c");

/// Every McpRegistration rule: no ProbeCommand carries the token or the
/// placeholder; the placeholder appears only in the copy-paste forms; a
/// one-click add exists only when the harness reads the token from the variable.
void verify_registration(const McpRegistration& r) {
    static const QRegularExpression literal_bearer(QStringLiteral(R"(Bearer\s+[^$\s])"));
    const QString placeholder = QString::fromLatin1(kMcpTokenPlaceholder);
    for (const std::optional<ProbeCommand>& cmd : {r.check, r.remove, r.add}) {
        if (!cmd)
            continue;
        for (const QString& a : cmd->args) {
            QVERIFY2(!a.contains(kToken), qPrintable(a));
            QVERIFY2(!a.contains(placeholder), qPrintable(a));
            QVERIFY2(!a.contains(literal_bearer), qPrintable(a));
        }
    }
    QCOMPARE(r.add.has_value(), r.token_from_env);
    for (const QString& a : r.manual_args)
        QVERIFY(!a.contains(kToken));
    QVERIFY(!r.config_snippet.contains(kToken));
    QVERIFY(!r.server_name.isEmpty());
    QVERIFY(!r.manual_args.isEmpty() || !r.config_snippet.isEmpty());
    QVERIFY(r.config_snippet.isEmpty() || !r.config_path_hint.isEmpty());
    QVERIFY(!r.note.isEmpty());
}

McpEndpoint endpoint() {
    McpEndpoint ep;
    ep.name = QStringLiteral("fincept_community");
    ep.url = kUrl;
    return ep;
}

std::vector<std::unique_ptr<IHarnessAdapter>> all_adapters() {
    std::vector<std::unique_ptr<IHarnessAdapter>> out;
    out.push_back(std::make_unique<ClaudeAdapter>());
    out.push_back(std::make_unique<CodexAdapter>());
    out.push_back(std::make_unique<GrokAdapter>());
    out.push_back(std::make_unique<GeminiAdapter>());
    out.push_back(std::make_unique<CursorAdapter>());
    out.push_back(std::make_unique<CopilotAdapter>());
    out.push_back(std::make_unique<QwenAdapter>());
    return out;
}

} // namespace

class TestHarnessCommands : public QObject {
    Q_OBJECT

  private slots:
    // ── Golden argument lists (match the pre-adapter command lines) ──────────
    void claude_golden_data();
    void claude_golden();
    void codex_golden_data();
    void codex_golden();

    // ── Details ──────────────────────────────────────────────────────────────
    void claude_mcp_config_file();
    void claude_permission_prompts_gate();
    void claude_optional_flags_order();
    void claude_system_prompt_through_cmd_shim();
    void codex_optional_flags_order();
    void codex_ignores_budget();
    void invalid_specs_are_refused();

    // ── Full access, deltas ──────────────────────────────────────────────────
    void claude_full_access_data();
    void claude_full_access();
    void codex_full_access();
    void claude_want_deltas();

    // ── Grok Build and the documentation-only adapters ──────────────────────
    void grok_golden_data();
    void grok_golden();
    void grok_long_prompt_uses_prompt_file();
    void grok_refusals();
    void gemini_golden_data();
    void gemini_golden();
    void gemini_needs_api_key();
    void cursor_golden_data();
    void cursor_golden();
    void copilot_golden_data();
    void copilot_golden();
    void qwen_golden_data();
    void qwen_golden();
    void argv_adapters_refuse_what_they_cannot_do();
    void prompt_starting_with_dash_stays_a_value();

    // ── CONNECT registration ─────────────────────────────────────────────────
    void mcp_registration_claude_codex();
    void mcp_registration_other_adapters();
    void mcp_registration_rejects_bad_endpoints();
    void mcp_config_windows_paths_with_spaces();
    // Edition co-existence (the private build on the same machine)
    void edition_mcp_names_and_ports();
    void legacy_entry_renamed_only_when_ours();
    void codex_runs_see_only_the_terminal_server();

    // ── Launch spec and environment ──────────────────────────────────────────
    void launch_spec_native_and_node_script();
    void child_environment_rules();

    // ── Sign-in, models, efforts ─────────────────────────────────────────────
    void claude_parse_auth();
    void codex_parse_auth();
    void auth_hints_use_names_and_existence_only();
    void codex_parse_models();
    void builtin_models_and_efforts();
};

// ── Claude Code ──────────────────────────────────────────────────────────────

void TestHarnessCommands::claude_golden_data() {
    QTest::addColumn<bool>("workspace");
    QTest::addColumn<bool>("resume");
    QTest::addColumn<bool>("tools");
    QTest::addColumn<QStringList>("expected");

    const QString ro = QStringLiteral("Read,Grep,Glob,WebSearch,WebFetch");
    const QString rw = QStringLiteral("Read,Grep,Glob,WebSearch,WebFetch,Edit,Write");
    const QString cfg = QStringLiteral("/cache/coding_agents/mcp-0123abcd.json");

    QTest::newRow("analysis/new/tools") << false << false << true
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--mcp-config", cfg, "--strict-mcp-config", "--tools", ro,
                       "--allowedTools", "mcp__fincept_community__*," + ro, "--append-system-prompt", kSystem};
    QTest::newRow("analysis/new/no-tools") << false << false << false
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--strict-mcp-config", "--tools", ro, "--allowedTools", ro,
                       "--append-system-prompt", kSystem};
    QTest::newRow("analysis/resume/tools") << false << true << true
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--mcp-config", cfg, "--strict-mcp-config", "--tools", ro,
                       "--allowedTools", "mcp__fincept_community__*," + ro, "--append-system-prompt", kSystem, "--resume",
                       kClaudeSession};
    QTest::newRow("analysis/resume/no-tools") << false << true << false
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--strict-mcp-config", "--tools", ro, "--allowedTools", ro,
                       "--append-system-prompt", kSystem, "--resume", kClaudeSession};
    QTest::newRow("edits/new/tools") << true << false << true
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--mcp-config", cfg, "--strict-mcp-config", "--tools", rw,
                       "--allowedTools", "mcp__fincept_community__*," + rw, "--append-system-prompt", kSystem};
    QTest::newRow("edits/new/no-tools") << true << false << false
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--strict-mcp-config", "--tools", rw, "--allowedTools", rw,
                       "--append-system-prompt", kSystem};
    QTest::newRow("edits/resume/tools") << true << true << true
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--mcp-config", cfg, "--strict-mcp-config", "--tools", rw,
                       "--allowedTools", "mcp__fincept_community__*," + rw, "--append-system-prompt", kSystem, "--resume",
                       kClaudeSession};
    QTest::newRow("edits/resume/no-tools") << true << true << false
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "dontAsk",
                       "--permission-prompts", "none", "--strict-mcp-config", "--tools", rw, "--allowedTools", rw,
                       "--append-system-prompt", kSystem, "--resume", kClaudeSession};
}

void TestHarnessCommands::claude_golden() {
    QFETCH(bool, workspace);
    QFETCH(bool, resume);
    QFETCH(bool, tools);
    QFETCH(QStringList, expected);

    const ClaudeAdapter claude;
    QCOMPARE(claude.descriptor().run_mcp_server_name, QStringLiteral("fincept_community"));
    const LaunchPlan plan = claude.build_command(
        run_spec(workspace, resume ? kClaudeSession : QString(), tools, claude.descriptor().run_mcp_server_name),
        installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")));
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    QCOMPARE(plan.stdin_payload, kPrompt.toUtf8()); // the prompt always travels on stdin
    QVERIFY(!plan.keep_stdin_open);
    QCOMPARE(plan.system_prompt_arg, kSystem);
    QCOMPARE(plan.temp_files.size(), tools ? 1 : 0);
    verify_no_secret(plan);
}

void TestHarnessCommands::claude_mcp_config_file() {
    const ClaudeAdapter claude;
    const LaunchPlan plan = claude.build_command(run_spec(false, {}, true, QStringLiteral("fincept_community")),
                                                 installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")));
    QCOMPARE(plan.temp_files.size(), 1);
    QCOMPARE(plan.temp_files[0].name, QStringLiteral("mcp-0123abcd.json"));
    // Byte-identical to McpHttpServer::claude_mcp_config_env() written compact
    // under "mcpServers": the header names the variable, never the token.
    QCOMPARE(plan.temp_files[0].content,
             QByteArray("{\"mcpServers\":{\"fincept_community\":{\"headers\":{\"Authorization\":\"Bearer ${FINCEPT_COMMUNITY_MCP_TOKEN}\"},"
                        "\"type\":\"http\",\"url\":\"http://127.0.0.1:41720/mcp\"}}}"));
}

void TestHarnessCommands::claude_permission_prompts_gate() {
    const ClaudeAdapter claude;
    const struct {
        const char* version;
        bool flag;
    } cases[] = {{"", false},      {"1.9.999", false}, {"2.1.258", false},
                 {"2.1.259", true}, {"2.2.0", true},    {"3.0.0", true}};
    for (const auto& c : cases) {
        const LaunchPlan plan = claude.build_command(
            run_spec(false, {}, false, QStringLiteral("fincept_community")),
            installed(QStringLiteral("/usr/local/bin/claude"), QString::fromLatin1(c.version)));
        const qsizetype at = plan.args.indexOf(QStringLiteral("--permission-prompts"));
        QVERIFY2((at >= 0) == c.flag, c.version);
        if (c.flag) {
            QCOMPARE(plan.args.at(at - 1), QStringLiteral("dontAsk")); // right after the permission mode
            QCOMPARE(plan.args.at(at + 1), QStringLiteral("none"));
        }
    }
}

void TestHarnessCommands::claude_optional_flags_order() {
    const ClaudeAdapter claude;
    RunSpec s = run_spec(false, kClaudeSession, false, QStringLiteral("fincept_community"));
    s.model = QStringLiteral("sonnet");
    s.effort = QStringLiteral("high");
    s.max_budget_usd = 2.5;
    const LaunchPlan plan = claude.build_command(s, installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")));
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    const QStringList tail = plan.args.mid(plan.args.indexOf(QStringLiteral("--append-system-prompt")));
    QCOMPARE(tail, (QStringList{"--append-system-prompt", kSystem, "--model", "sonnet", "--effort", "high",
                                "--max-budget-usd", "2.50", "--resume", kClaudeSession}));

    // Effort is passed only when a run asks for it.
    s.effort.clear();
    const LaunchPlan without = claude.build_command(s, installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")));
    QVERIFY(!without.args.contains(QStringLiteral("--effort")));
}

void TestHarnessCommands::claude_system_prompt_through_cmd_shim() {
    const ClaudeAdapter claude;
    RunSpec s = run_spec(false, {}, false, QStringLiteral("fincept_community"));
    s.system_prompt = QStringLiteral("a & b % c\n!d");

    Installation shim = installed(QStringLiteral("C:/npm/claude.cmd"), QStringLiteral("2.1.325"));
    shim.via = Installation::CmdShim;
    const LaunchPlan through_cmd = claude.build_command(s, shim);
    QCOMPARE(through_cmd.system_prompt_arg, QStringLiteral("a b c d"));
    QCOMPARE(through_cmd.system_prompt_arg, fincept::services::coding_agents::cmd_safe_text(s.system_prompt));
    QVERIFY(through_cmd.args.contains(QStringLiteral("a b c d")));

    // Resolved to node + script: no cmd.exe, nothing is stripped.
    Installation node = shim;
    node.via = Installation::NodeScript;
    node.node_path = QStringLiteral("C:/nodejs/node.exe");
    node.script_path = QStringLiteral("C:/npm/node_modules/@anthropic-ai/claude-code/cli.js");
    const LaunchPlan direct = claude.build_command(s, node);
    QCOMPARE(direct.system_prompt_arg, s.system_prompt);
}

// ── Codex ────────────────────────────────────────────────────────────────────

void TestHarnessCommands::codex_golden_data() {
    QTest::addColumn<bool>("workspace");
    QTest::addColumn<bool>("resume");
    QTest::addColumn<bool>("tools");
    QTest::addColumn<QStringList>("expected");

    const QStringList mcp = {"-c", "mcp_servers.fincept_community.url=\"http://127.0.0.1:41720/mcp\"",
                             "-c", "mcp_servers.fincept_community.bearer_token_env_var=\"FINCEPT_COMMUNITY_MCP_TOKEN\"",
                             "-c", "mcp_servers.fincept_community.tool_timeout_sec=300"};

    QTest::newRow("analysis/new/tools") << false << false << true
        << (QStringList{"exec", "--json", "--skip-git-repo-check", "-s", "read-only", "-c", "approval_policy=\"never\""}
            + mcp + QStringList{"-"});
    QTest::newRow("analysis/new/no-tools") << false << false << false
        << QStringList{"exec", "--json", "--skip-git-repo-check", "-s", "read-only", "-c", "approval_policy=\"never\"", "-"};
    QTest::newRow("analysis/resume/tools") << false << true << true
        << (QStringList{"exec", "resume", "--json", "--skip-git-repo-check", "-c", "sandbox_mode=\"read-only\"", "-c",
                        "approval_policy=\"never\""}
            + mcp + QStringList{kCodexThread, "-"});
    QTest::newRow("analysis/resume/no-tools") << false << true << false
        << QStringList{"exec", "resume", "--json", "--skip-git-repo-check", "-c", "sandbox_mode=\"read-only\"", "-c",
                       "approval_policy=\"never\"", kCodexThread, "-"};
    QTest::newRow("edits/new/tools") << true << false << true
        << (QStringList{"exec", "--json", "--skip-git-repo-check", "-s", "workspace-write", "-c", "approval_policy=\"never\""}
            + mcp + QStringList{"-"});
    QTest::newRow("edits/new/no-tools") << true << false << false
        << QStringList{"exec", "--json", "--skip-git-repo-check", "-s", "workspace-write", "-c", "approval_policy=\"never\"", "-"};
    QTest::newRow("edits/resume/tools") << true << true << true
        << (QStringList{"exec", "resume", "--json", "--skip-git-repo-check", "-c", "sandbox_mode=\"workspace-write\"",
                        "-c", "approval_policy=\"never\""}
            + mcp + QStringList{kCodexThread, "-"});
    QTest::newRow("edits/resume/no-tools") << true << true << false
        << QStringList{"exec", "resume", "--json", "--skip-git-repo-check", "-c", "sandbox_mode=\"workspace-write\"",
                       "-c", "approval_policy=\"never\"", kCodexThread, "-"};
}

void TestHarnessCommands::codex_golden() {
    QFETCH(bool, workspace);
    QFETCH(bool, resume);
    QFETCH(bool, tools);
    QFETCH(QStringList, expected);

    const CodexAdapter codex;
    QCOMPARE(codex.descriptor().run_mcp_server_name, QStringLiteral("fincept_community"));
    const LaunchPlan plan = codex.build_command(
        run_spec(workspace, resume ? kCodexThread : QString(), tools, codex.descriptor().run_mcp_server_name),
        installed(QStringLiteral("/usr/local/bin/codex"), QStringLiteral("0.160.0")));
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    // No system-prompt flag: the context is wrapped into the first prompt only.
    const QByteArray wrapped("<context>\nSYSTEM CONTEXT\n</context>\n\n<user_request>\nSummarise AAPL\n</user_request>\n");
    QCOMPARE(plan.stdin_payload, resume ? kPrompt.toUtf8() : wrapped);
    QVERIFY(plan.system_prompt_arg.isEmpty());
    QVERIFY(plan.temp_files.isEmpty()); // Codex takes its MCP server through -c, no file
    verify_no_secret(plan);
}

void TestHarnessCommands::codex_optional_flags_order() {
    const CodexAdapter codex;
    RunSpec s = run_spec(false, {}, true, QStringLiteral("fincept_community"));
    s.model = QStringLiteral("gpt-6-luna");
    s.effort = QStringLiteral("high");
    const LaunchPlan plan = codex.build_command(s, installed(QStringLiteral("/usr/local/bin/codex"), QStringLiteral("0.160.0")));
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    const qsizetype at = plan.args.indexOf(QStringLiteral("approval_policy=\"never\""));
    QVERIFY(at > 0);
    QCOMPARE(plan.args.mid(at + 1, 4),
             (QStringList{"-m", "gpt-6-luna", "-c", "model_reasoning_effort=\"high\""}));
    QCOMPARE(plan.args.constLast(), QStringLiteral("-"));
}

void TestHarnessCommands::codex_ignores_budget() {
    const CodexAdapter codex;
    RunSpec s = run_spec(false, {}, false, QStringLiteral("fincept_community"));
    s.max_budget_usd = 5.0;
    const LaunchPlan plan = codex.build_command(s, installed(QStringLiteral("/usr/local/bin/codex"), QStringLiteral("0.160.0")));
    QVERIFY(plan.error.isEmpty());
    for (const QString& a : plan.args)
        QVERIFY(!a.contains(QLatin1String("budget")));
}

void TestHarnessCommands::invalid_specs_are_refused() {
    const ClaudeAdapter claude;
    const CodexAdapter codex;
    const Installation ci = installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325"));
    const Installation xi = installed(QStringLiteral("/usr/local/bin/codex"), QStringLiteral("0.160.0"));

    RunSpec bad_effort = run_spec(false, {}, false, QStringLiteral("fincept_community"));
    bad_effort.effort = QStringLiteral("high\" -c sandbox_mode=\"danger-full-access");
    QVERIFY(!claude.build_command(bad_effort, ci).error.isEmpty());
    QVERIFY(!codex.build_command(bad_effort, xi).error.isEmpty());

    const RunSpec flag_session = run_spec(false, QStringLiteral("-rf"), false, QStringLiteral("fincept_community"));
    QVERIFY(!claude.build_command(flag_session, ci).error.isEmpty());
    QVERIFY(!codex.build_command(flag_session, xi).error.isEmpty());

    // (Full access is a supported mode for both since caps.full_access — see
    // claude_full_access / codex_full_access.)

    RunSpec bad_url = run_spec(false, {}, true, QStringLiteral("fincept_community"));
    bad_url.mcp->url = QStringLiteral("http://x\"\n[evil]");
    QVERIFY(!codex.build_command(bad_url, xi).error.isEmpty());
}

// ── Full access, deltas ──────────────────────────────────────────────────────

void TestHarnessCommands::claude_full_access_data() {
    QTest::addColumn<bool>("resume");
    QTest::addColumn<bool>("tools");
    QTest::addColumn<QStringList>("expected");
    const QString cfg = QStringLiteral("/cache/coding_agents/mcp-0123abcd.json");
    // bypassPermissions, no --permission-prompts, no tool restriction; the MCP
    // configuration stays strict.
    QTest::newRow("new/tools") << false << true
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "bypassPermissions",
                       "--mcp-config", cfg, "--strict-mcp-config", "--append-system-prompt", kSystem};
    QTest::newRow("new/no-tools") << false << false
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "bypassPermissions",
                       "--strict-mcp-config", "--append-system-prompt", kSystem};
    QTest::newRow("resume/tools") << true << true
        << QStringList{"-p", "--output-format", "stream-json", "--verbose", "--permission-mode", "bypassPermissions",
                       "--mcp-config", cfg, "--strict-mcp-config", "--append-system-prompt", kSystem, "--resume",
                       kClaudeSession};
}

void TestHarnessCommands::claude_full_access() {
    QFETCH(bool, resume);
    QFETCH(bool, tools);
    QFETCH(QStringList, expected);
    const ClaudeAdapter claude;
    QVERIFY(claude.descriptor().caps.full_access);
    RunSpec s = run_spec(true, resume ? kClaudeSession : QString(), tools, QStringLiteral("fincept_community"));
    s.permission = PermissionMode::FullAccess;
    const LaunchPlan plan = claude.build_command(s, installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")));
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    QCOMPARE(plan.stdin_payload, kPrompt.toUtf8());
    verify_no_secret(plan);
}

void TestHarnessCommands::codex_full_access() {
    const CodexAdapter codex;
    QVERIFY(codex.descriptor().caps.full_access);
    RunSpec s = run_spec(true, {}, false, QStringLiteral("fincept_community"));
    s.permission = PermissionMode::FullAccess;
    const Installation xi = installed(QStringLiteral("/usr/local/bin/codex"), QStringLiteral("0.160.0"));
    LaunchPlan plan = codex.build_command(s, xi);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, (QStringList{"exec", "--json", "--skip-git-repo-check", "-s", "danger-full-access", "-c",
                                     "approval_policy=\"never\"", "-"}));
    s.resume_session_id = kCodexThread;
    plan = codex.build_command(s, xi);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, (QStringList{"exec", "resume", "--json", "--skip-git-repo-check", "-c",
                                     "sandbox_mode=\"danger-full-access\"", "-c", "approval_policy=\"never\"",
                                     kCodexThread, "-"}));
}

void TestHarnessCommands::claude_want_deltas() {
    const ClaudeAdapter claude;
    RunSpec s = run_spec(false, {}, false, QStringLiteral("fincept_community"));
    s.want_deltas = true;
    const LaunchPlan plan = claude.build_command(s, installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")));
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args.mid(0, 7), (QStringList{"-p", "--output-format", "stream-json", "--verbose",
                                               "--include-partial-messages", "--permission-mode", "dontAsk"}));
    // Without it the golden list is unchanged (no flag at all).
    s.want_deltas = false;
    QVERIFY(!claude.build_command(s, installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")))
                 .args.contains(QStringLiteral("--include-partial-messages")));
}

// ── Grok Build ───────────────────────────────────────────────────────────────

void TestHarnessCommands::grok_golden_data() {
    QTest::addColumn<bool>("workspace");
    QTest::addColumn<bool>("resume");
    QTest::addColumn<QStringList>("expected");
    const QString sess = QStringLiteral("0199a213-81c0-7800-8aa1-bbab2a035a53");
    QTest::newRow("analysis/new") << false << false
        << QStringList{"-p", kPrompt, "--output-format", "streaming-json", "--permission-mode", "dontAsk", "--rules", kSystem};
    QTest::newRow("analysis/resume") << false << true
        << QStringList{"-p", kPrompt, "--output-format", "streaming-json", "--permission-mode", "dontAsk", "--rules", kSystem,
                       "-r", sess};
    QTest::newRow("edits/new") << true << false
        << QStringList{"-p", kPrompt, "--output-format", "streaming-json", "--permission-mode", "acceptEdits", "--rules",
                       kSystem};
    QTest::newRow("edits/resume") << true << true
        << QStringList{"-p", kPrompt, "--output-format", "streaming-json", "--permission-mode", "acceptEdits", "--rules",
                       kSystem, "-r", sess};
}

void TestHarnessCommands::grok_golden() {
    QFETCH(bool, workspace);
    QFETCH(bool, resume);
    QFETCH(QStringList, expected);
    const GrokAdapter grok;
    const QString sess = QStringLiteral("0199a213-81c0-7800-8aa1-bbab2a035a53");
    const Installation gi = installed(QStringLiteral("C:/Users/me/.grok/bin/grok.exe"), QStringLiteral("1.0.41"));
    const LaunchPlan plan = grok.build_command(run_spec(workspace, resume ? sess : QString(), false, {}), gi);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    QVERIFY(plan.stdin_payload.isEmpty()); // the prompt is an argument
    QCOMPARE(plan.prompt_arg, kPrompt);
    QCOMPARE(plan.system_prompt_arg, kSystem);
    QVERIFY(plan.temp_files.isEmpty());
    verify_no_secret(plan);

    // MCP on: no per-run option, so the run is refused rather than silently tool-less.
    const LaunchPlan with_mcp =
        grok.build_command(run_spec(workspace, resume ? sess : QString(), true, QStringLiteral("fincept_community")), gi);
    QVERIFY(!with_mcp.error.isEmpty());
    QVERIFY(!grok.mcp_config_for_run(endpoint(), run_spec(false, {}, false, {})).error.isEmpty());

    // Model and turn limit, in order before the resume flag.
    RunSpec s = run_spec(workspace, resume ? sess : QString(), false, {});
    s.model = QStringLiteral("grok-4.6");
    s.max_turns = 12;
    const LaunchPlan opt = grok.build_command(s, gi);
    QVERIFY2(opt.error.isEmpty(), qPrintable(opt.error));
    const qsizetype at = opt.args.indexOf(QStringLiteral("--rules"));
    QStringList tail = {"--rules", kSystem, "-m", "grok-4.6", "--max-turns", "12"};
    if (resume)
        tail << "-r" << sess;
    QCOMPARE(opt.args.mid(at), tail);
}

void TestHarnessCommands::grok_long_prompt_uses_prompt_file() {
    const GrokAdapter grok;
    const Installation gi = installed(QStringLiteral("/home/me/.grok/bin/grok"), QStringLiteral("1.0.41"));
    RunSpec s = run_spec(false, {}, false, {});
    s.prompt = QString(30000, QLatin1Char('q')); // past the command-line budget
    LaunchPlan plan = grok.build_command(s, gi);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, (QStringList{"--prompt-file", "/cache/coding_agents/prompt-0123abcd.txt", "--output-format",
                                     "streaming-json", "--permission-mode", "dontAsk", "--rules", kSystem}));
    QCOMPARE(plan.temp_files.size(), 1);
    QCOMPARE(plan.temp_files[0].name, QStringLiteral("prompt-0123abcd.txt"));
    QCOMPARE(plan.temp_files[0].content, s.prompt.toUtf8());
    QVERIFY(plan.prompt_arg.isEmpty());
    QVERIFY(plan.stdin_payload.isEmpty());

    // Just under the budget (prompt + rules) stays on the command line.
    s.prompt = QString(24000 - kSystem.size(), QLatin1Char('q'));
    plan = grok.build_command(s, gi);
    QCOMPARE(plan.args.at(0), QStringLiteral("-p"));
    s.prompt += QLatin1Char('q');
    plan = grok.build_command(s, gi);
    QCOMPARE(plan.args.at(0), QStringLiteral("--prompt-file"));

    // A prompt file needs the run's private folder.
    s.temp_dir.clear();
    QVERIFY(!grok.build_command(s, gi).error.isEmpty());

    // Through a cmd.exe launcher: always a prompt file, the rules made cmd-safe.
    Installation shim = installed(QStringLiteral("C:/tools/grok.cmd"), QStringLiteral("1.0.41"));
    shim.via = Installation::CmdShim;
    RunSpec t = run_spec(false, {}, false, {});
    t.system_prompt = QStringLiteral("a & b % c\n!d");
    plan = grok.build_command(t, shim);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args.at(0), QStringLiteral("--prompt-file"));
    QCOMPARE(plan.system_prompt_arg, QStringLiteral("a b c d"));
}

void TestHarnessCommands::grok_refusals() {
    const GrokAdapter grok;
    const Installation gi = installed(QStringLiteral("/home/me/.grok/bin/grok"), QStringLiteral("1.0.41"));
    RunSpec full = run_spec(true, {}, false, {});
    full.permission = PermissionMode::FullAccess;
    QVERIFY(!grok.build_command(full, gi).error.isEmpty());
    RunSpec effort = run_spec(false, {}, false, {});
    effort.effort = QStringLiteral("high"); // values not listed by --help
    QVERIFY(!grok.build_command(effort, gi).error.isEmpty());
    RunSpec schema = run_spec(false, {}, false, {});
    schema.json_schema = QStringLiteral("{\"type\":\"object\"}");
    QVERIFY(!grok.build_command(schema, gi).error.isEmpty());
    QVERIFY(!grok.build_command(run_spec(false, QStringLiteral("-c"), false, {}), gi).error.isEmpty());
    RunSpec model = run_spec(false, {}, false, {});
    model.model = QStringLiteral("--always-approve");
    QVERIFY(!grok.build_command(model, gi).error.isEmpty());
    RunSpec turns = run_spec(false, {}, false, {});
    turns.max_turns = 100000;
    QVERIFY(!grok.build_command(turns, gi).error.isEmpty());
    RunSpec budget = run_spec(false, {}, false, {});
    budget.max_budget_usd = 3.0; // no budget option: ignored, like Codex
    const LaunchPlan b = grok.build_command(budget, gi);
    QVERIFY(b.error.isEmpty());
    for (const QString& a : b.args)
        QVERIFY(!a.contains(QLatin1String("budget")));
}

// ── Gemini CLI ───────────────────────────────────────────────────────────────

void TestHarnessCommands::gemini_golden_data() {
    QTest::addColumn<bool>("workspace");
    QTest::addColumn<bool>("resume");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("analysis/new") << false << false
        << QStringList{"-p", kWrapped, "-o", "stream-json", "--approval-mode", "plan", "--allowed-mcp-server-names", "fincept_community"};
    QTest::newRow("analysis/resume") << false << true
        << QStringList{"-p", kPrompt, "-o", "stream-json", "--approval-mode", "plan", "--allowed-mcp-server-names", "fincept_community", "-r", kGeminiSession};
    QTest::newRow("edits/new") << true << false
        << QStringList{"-p", kWrapped, "-o", "stream-json", "--approval-mode", "auto_edit", "--allowed-mcp-server-names", "fincept_community"};
    QTest::newRow("edits/resume") << true << true
        << QStringList{"-p", kPrompt, "-o", "stream-json", "--approval-mode", "auto_edit", "--allowed-mcp-server-names", "fincept_community", "-r", kGeminiSession};
}

void TestHarnessCommands::gemini_golden() {
    QFETCH(bool, workspace);
    QFETCH(bool, resume);
    QFETCH(QStringList, expected);
    const GeminiAdapter gemini;
    const Installation gi = installed(QStringLiteral("/usr/local/bin/gemini"), QStringLiteral("0.30.0"));
    RunSpec s = run_spec(workspace, resume ? kGeminiSession : QString(), false, {});
    s.api_key_env_present = true;
    const LaunchPlan plan = gemini.build_command(s, gi);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    QVERIFY(plan.stdin_payload.isEmpty());
    QCOMPARE(plan.prompt_arg, resume ? kPrompt : kWrapped);
    verify_no_secret(plan);

    RunSpec m = run_spec(workspace, resume ? kGeminiSession : QString(), true, QStringLiteral("fincept_community"));
    m.api_key_env_present = true;
    QVERIFY(!gemini.build_command(m, gi).error.isEmpty()); // no per-run MCP

    RunSpec model = s;
    model.model = QStringLiteral("flash");
    const LaunchPlan mp = gemini.build_command(model, gi);
    QCOMPARE(mp.args.mid(8, 2), (QStringList{"-m", "flash"}));
}

void TestHarnessCommands::gemini_needs_api_key() {
    const GeminiAdapter gemini;
    const Installation gi = installed(QStringLiteral("/usr/local/bin/gemini"), QStringLiteral("0.30.0"));
    RunSpec s = run_spec(false, {}, false, {});
    s.api_key_env_present = false; // would fall back to the personal Google sign-in
    const LaunchPlan plan = gemini.build_command(s, gi);
    QVERIFY(!plan.error.isEmpty());
    QVERIFY(plan.error.contains(QLatin1String("GEMINI_API_KEY")));
    QVERIFY(plan.error.contains(QLatin1String("terms")));
    QVERIFY(plan.args.isEmpty());
    // Resume ids are session uuids only ("latest" / an index would pick another session).
    s.api_key_env_present = true;
    s.resume_session_id = QStringLiteral("latest");
    QVERIFY(!gemini.build_command(s, gi).error.isEmpty());
    s.resume_session_id = QStringLiteral("3");
    QVERIFY(!gemini.build_command(s, gi).error.isEmpty());
}

// ── Cursor CLI ───────────────────────────────────────────────────────────────

void TestHarnessCommands::cursor_golden_data() {
    QTest::addColumn<bool>("workspace");
    QTest::addColumn<bool>("resume");
    QTest::addColumn<QStringList>("expected");
    const QString chat = QStringLiteral("6f1e1c1d-2b3a-4c5d-8e9f-0a1b2c3d4e5f");
    QTest::newRow("analysis/new") << false << false
        << QStringList{"-p", kWrapped, "--output-format", "stream-json", "--trust", "--workspace", "/work/run", "--mode", "ask"};
    QTest::newRow("analysis/resume") << false << true
        << QStringList{"-p", kPrompt, "--output-format", "stream-json", "--trust", "--workspace", "/work/run", "--mode", "ask",
                       "--resume", chat};
    QTest::newRow("edits/new") << true << false
        << QStringList{"-p", kWrapped, "--output-format", "stream-json", "--trust", "--workspace", "/work/run"};
    QTest::newRow("edits/resume") << true << true
        << QStringList{"-p", kPrompt, "--output-format", "stream-json", "--trust", "--workspace", "/work/run", "--resume", chat};
}

void TestHarnessCommands::cursor_golden() {
    QFETCH(bool, workspace);
    QFETCH(bool, resume);
    QFETCH(QStringList, expected);
    const CursorAdapter cursor;
    const QString chat = QStringLiteral("6f1e1c1d-2b3a-4c5d-8e9f-0a1b2c3d4e5f");
    const Installation ci = installed(QStringLiteral("/home/me/.local/bin/agent"), QStringLiteral("2025.11.6"));
    const LaunchPlan plan = cursor.build_command(run_spec(workspace, resume ? chat : QString(), false, {}), ci);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    QVERIFY(plan.stdin_payload.isEmpty());
    verify_no_secret(plan);
    QVERIFY(!cursor.build_command(run_spec(workspace, resume ? chat : QString(), true, QStringLiteral("fincept_community")), ci)
                 .error.isEmpty());

    RunSpec m = run_spec(workspace, resume ? chat : QString(), false, {});
    m.model = QStringLiteral("gpt-5");
    const LaunchPlan mp = cursor.build_command(m, ci);
    QCOMPARE(mp.args.mid(7, 2), (QStringList{"--model", "gpt-5"}));
}

// ── Copilot CLI ──────────────────────────────────────────────────────────────

void TestHarnessCommands::copilot_golden_data() {
    QTest::addColumn<bool>("workspace");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("effort");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("analysis") << false << QString() << QString()
        << QStringList{"-p", kWrapped, "--no-ask-user", "-C", "/work/run", "--deny-tool=write, shell, fincept, fincept-terminal, fincept-cloud, fincept-mcp"};
    QTest::newRow("edits") << true << QString() << QString()
        << QStringList{"-p", kWrapped, "--no-ask-user", "-C", "/work/run", "--allow-tool=write", "--deny-tool=shell, fincept, fincept-terminal, fincept-cloud, fincept-mcp"};
    QTest::newRow("analysis/model+effort") << false << QStringLiteral("auto") << QStringLiteral("xhigh")
        << QStringList{"-p", kWrapped, "--no-ask-user", "-C", "/work/run", "--model", "auto", "--effort", "xhigh",
                       "--deny-tool=write, shell, fincept, fincept-terminal, fincept-cloud, fincept-mcp"};
}

void TestHarnessCommands::copilot_golden() {
    QFETCH(bool, workspace);
    QFETCH(QString, model);
    QFETCH(QString, effort);
    QFETCH(QStringList, expected);
    const CopilotAdapter copilot;
    const Installation ci = installed(QStringLiteral("/usr/local/bin/copilot"), QStringLiteral("0.0.354"));
    RunSpec s = run_spec(workspace, {}, false, {});
    s.model = model;
    s.effort = effort;
    const LaunchPlan plan = copilot.build_command(s, ci);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    QVERIFY(plan.stdin_payload.isEmpty());
    verify_no_secret(plan);
    // No session id in plain text: no resume. No per-run MCP.
    QVERIFY(!copilot.build_command(run_spec(workspace, QStringLiteral("sess-1"), false, {}), ci).error.isEmpty());
    QVERIFY(!copilot.build_command(run_spec(workspace, {}, true, QStringLiteral("fincept_community")), ci).error.isEmpty());
    RunSpec bad = s;
    bad.effort = QStringLiteral("ultra");
    QVERIFY(!copilot.build_command(bad, ci).error.isEmpty());
}

// ── Qwen Code ────────────────────────────────────────────────────────────────

void TestHarnessCommands::qwen_golden_data() {
    QTest::addColumn<bool>("workspace");
    QTest::addColumn<bool>("resume");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("analysis/new") << false << false
        << QStringList{"-p", kWrapped, "-o", "stream-json", "--approval-mode", "plan"};
    QTest::newRow("analysis/resume") << false << true
        << QStringList{"-p", kPrompt, "-o", "stream-json", "--approval-mode", "plan", "--resume", "qwen-sess-1"};
    QTest::newRow("edits/new") << true << false
        << QStringList{"-p", kWrapped, "-o", "stream-json", "--approval-mode", "auto-edit"};
    QTest::newRow("edits/resume") << true << true
        << QStringList{"-p", kPrompt, "-o", "stream-json", "--approval-mode", "auto-edit", "--resume", "qwen-sess-1"};
}

void TestHarnessCommands::qwen_golden() {
    QFETCH(bool, workspace);
    QFETCH(bool, resume);
    QFETCH(QStringList, expected);
    const QwenAdapter qwen;
    const Installation qi = installed(QStringLiteral("/usr/local/bin/qwen"), QStringLiteral("0.5.0"));
    const QString sess = QStringLiteral("qwen-sess-1");
    const LaunchPlan plan = qwen.build_command(run_spec(workspace, resume ? sess : QString(), false, {}), qi);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(plan.args, expected);
    verify_no_secret(plan);
    QVERIFY(!qwen.build_command(run_spec(workspace, resume ? sess : QString(), true, QStringLiteral("fincept_community")), qi)
                 .error.isEmpty());
    RunSpec d = run_spec(workspace, resume ? sess : QString(), false, {});
    d.want_deltas = true;
    const LaunchPlan dp = qwen.build_command(d, qi);
    QCOMPARE(dp.args.mid(2, 3), (QStringList{"-o", "stream-json", "--include-partial-messages"}));
}

void TestHarnessCommands::argv_adapters_refuse_what_they_cannot_do() {
    const Installation inst = installed(QStringLiteral("/usr/local/bin/x"), QStringLiteral("1.0.0"));
    const GrokAdapter grok;
    const GeminiAdapter gemini;
    const CursorAdapter cursor;
    const CopilotAdapter copilot;
    const QwenAdapter qwen;
    for (const IHarnessAdapter* a : {static_cast<const IHarnessAdapter*>(&grok), static_cast<const IHarnessAdapter*>(&gemini),
                                     static_cast<const IHarnessAdapter*>(&cursor),
                                     static_cast<const IHarnessAdapter*>(&copilot),
                                     static_cast<const IHarnessAdapter*>(&qwen)}) {
        const QString id = a->descriptor().id;
        QVERIFY2(!a->descriptor().caps.full_access && !a->descriptor().caps.per_run_mcp, qPrintable(id));
        RunSpec base = run_spec(false, {}, false, {});
        base.api_key_env_present = true;
        QVERIFY2(a->build_command(base, inst).error.isEmpty(), qPrintable(id + a->build_command(base, inst).error));
        RunSpec full = base;
        full.permission = PermissionMode::FullAccess;
        QVERIFY2(!a->build_command(full, inst).error.isEmpty(), qPrintable(id));
        RunSpec empty = base;
        empty.prompt = QStringLiteral("  ");
        QVERIFY2(!empty.prompt.isEmpty() && !a->build_command(empty, inst).error.isEmpty(), qPrintable(id));
        RunSpec chat = base;
        chat.chat = true;
        QVERIFY2(!a->build_command(chat, inst).error.isEmpty(), qPrintable(id));
        RunSpec bad_model = base;
        bad_model.model = QStringLiteral("-x evil");
        QVERIFY2(!a->build_command(bad_model, inst).error.isEmpty(), qPrintable(id));
        if (id != QLatin1String("grok")) { // Grok moves a long prompt into a file instead
            RunSpec huge = base;
            huge.prompt = QString(30000, QLatin1Char('q'));
            QVERIFY2(!a->build_command(huge, inst).error.isEmpty(), qPrintable(id));
        }
        QVERIFY2(!a->mcp_config_for_run(endpoint(), base).error.isEmpty(), qPrintable(id));
    }
}

void TestHarnessCommands::prompt_starting_with_dash_stays_a_value() {
    // An option parser would read "-rf …" as flags: the value gets one leading space.
    const Installation inst = installed(QStringLiteral("/usr/local/bin/x"), QStringLiteral("1.0.0"));
    RunSpec s = run_spec(false, QStringLiteral("0199a213-81c0-7800-8aa1-bbab2a035a53"), false, {});
    s.prompt = QStringLiteral("-rf everything");
    s.api_key_env_present = true;
    const LaunchPlan g = GrokAdapter().build_command(s, inst);
    QCOMPARE(g.args.mid(0, 2), (QStringList{"-p", " -rf everything"}));
    const LaunchPlan c = CursorAdapter().build_command(s, inst);
    QCOMPARE(c.args.at(1), QStringLiteral(" -rf everything"));
    RunSpec rules = run_spec(false, {}, false, {});
    rules.system_prompt = QStringLiteral("--always-approve");
    const LaunchPlan r = GrokAdapter().build_command(rules, inst);
    QCOMPARE(r.args.at(r.args.indexOf(QStringLiteral("--rules")) + 1), QStringLiteral(" --always-approve"));
}

// ── CONNECT registration ─────────────────────────────────────────────────────

void TestHarnessCommands::mcp_registration_claude_codex() {
    const auto claude = ClaudeAdapter().mcp_registration(endpoint());
    QVERIFY(claude.has_value());
    verify_registration(*claude);
    QCOMPARE(claude->server_name, QStringLiteral("fincept_community"));
    QCOMPARE(claude->check->args, (QStringList{"mcp", "get", "fincept_community"}));
    QCOMPARE(claude->remove->args, (QStringList{"mcp", "remove", "fincept_community", "--scope", "user"}));
    QCOMPARE(claude->add->args, (QStringList{"mcp", "add", "--transport", "http", "--scope", "user", "fincept_community",
                                             kUrl, "--header", "Authorization: Bearer ${FINCEPT_COMMUNITY_MCP_TOKEN}"}));
    QVERIFY(claude->token_from_env);
    // The old McpHttpServer::claude_code_add_command() form, with the placeholder.
    QCOMPARE(claude->manual_args, (QStringList{"mcp", "add", "--transport", "http", "fincept_community", kUrl, "-H",
                                               "Authorization: Bearer <FINCEPT_COMMUNITY_MCP_TOKEN>", "-s", "user"}));
    QVERIFY(claude->note.contains(QLatin1String("FINCEPT_COMMUNITY_MCP_TOKEN")));

    const auto codex = CodexAdapter().mcp_registration(endpoint());
    QVERIFY(codex.has_value());
    verify_registration(*codex);
    QCOMPARE(codex->check->args, (QStringList{"mcp", "get", "fincept_community"}));
    QCOMPARE(codex->remove->args, (QStringList{"mcp", "remove", "fincept_community"}));
    QCOMPARE(codex->add->args, (QStringList{"mcp", "add", "fincept_community", "--url", kUrl, "--bearer-token-env-var",
                                            "FINCEPT_COMMUNITY_MCP_TOKEN"}));
    QCOMPARE(codex->manual_args, codex->add->args);
    // Same text as McpHttpServer::codex_config_toml().
    QCOMPARE(codex->config_snippet,
             QStringLiteral("[mcp_servers.fincept_community]\nurl = \"http://127.0.0.1:41720/mcp\"\n"
                            "bearer_token_env_var = \"FINCEPT_COMMUNITY_MCP_TOKEN\"\ntool_timeout_sec = 300\n"));
    QCOMPARE(codex->config_path_hint, QStringLiteral("~/.codex/config.toml"));
    QVERIFY(codex->note.contains(QLatin1String("FINCEPT_COMMUNITY_MCP_TOKEN")));

    // The caller's name is used everywhere.
    McpEndpoint other = endpoint();
    other.name = QStringLiteral("fincept_dev");
    QCOMPARE(ClaudeAdapter().mcp_registration(other)->check->args.last(), QStringLiteral("fincept_dev"));
    QVERIFY(CodexAdapter().mcp_registration(other)->config_snippet.startsWith(QLatin1String("[mcp_servers.fincept_dev]")));
}

void TestHarnessCommands::mcp_registration_other_adapters() {
    const auto grok = GrokAdapter().mcp_registration(endpoint());
    QVERIFY(grok.has_value());
    verify_registration(*grok);
    QVERIFY(!grok->add.has_value()); // a literal-token header only: no one-click add
    QVERIFY(!grok->check.has_value());
    QVERIFY(!grok->token_from_env);
    QCOMPARE(grok->remove->args, (QStringList{"mcp", "remove", "fincept_community"}));
    QCOMPARE(grok->manual_args, (QStringList{"mcp", "add", "--transport", "http", "fincept_community", kUrl, "-H",
                                             "Authorization: Bearer <FINCEPT_COMMUNITY_MCP_TOKEN>"}));

    const auto gemini = GeminiAdapter().mcp_registration(endpoint());
    QVERIFY(gemini.has_value());
    verify_registration(*gemini);
    QVERIFY(!gemini->add.has_value());
    QCOMPARE(gemini->manual_args, (QStringList{"mcp", "add", "--transport", "http", "fincept_community", kUrl, "-H",
                                               "Authorization: Bearer <FINCEPT_COMMUNITY_MCP_TOKEN>", "-s", "user"}));
    const QJsonObject gs = QJsonDocument::fromJson(gemini->config_snippet.toUtf8())
                               .object()
                               .value(QStringLiteral("mcpServers"))
                               .toObject()
                               .value(QStringLiteral("fincept_community"))
                               .toObject();
    QCOMPARE(gs.value(QStringLiteral("httpUrl")).toString(), kUrl);
    QCOMPARE(gs.value(QStringLiteral("headers")).toObject().value(QStringLiteral("Authorization")).toString(),
             QStringLiteral("Bearer <FINCEPT_COMMUNITY_MCP_TOKEN>"));
    QCOMPARE(gs.value(QStringLiteral("timeout")).toInt(), 300000);
    QCOMPARE(gemini->config_path_hint, QStringLiteral("~/.gemini/settings.json"));

    const auto cursor = CursorAdapter().mcp_registration(endpoint());
    QVERIFY(cursor.has_value());
    verify_registration(*cursor);
    QVERIFY(!cursor->add.has_value());
    QVERIFY(cursor->manual_args.isEmpty());
    const QJsonObject cs = QJsonDocument::fromJson(cursor->config_snippet.toUtf8())
                               .object()
                               .value(QStringLiteral("mcpServers"))
                               .toObject()
                               .value(QStringLiteral("fincept_community"))
                               .toObject();
    QCOMPARE(cs.value(QStringLiteral("type")).toString(), QStringLiteral("http"));
    QCOMPARE(cs.value(QStringLiteral("url")).toString(), kUrl);
    QCOMPARE(cs.value(QStringLiteral("headers")).toObject().value(QStringLiteral("Authorization")).toString(),
             QStringLiteral("Bearer <FINCEPT_COMMUNITY_MCP_TOKEN>"));
    QCOMPARE(cursor->config_path_hint, QStringLiteral("~/.cursor/mcp.json"));

    // Copilot CLI: documented `copilot mcp add` + mcp-config.json; the timeout
    // (Copilot's 30 s default bounds every tool call) is the endpoint's budget.
    const auto copilot = CopilotAdapter().mcp_registration(endpoint());
    QVERIFY(copilot.has_value());
    verify_registration(*copilot);
    QVERIFY(!copilot->add.has_value()); // a literal-token header only: no one-click add
    QCOMPARE(copilot->manual_args, (QStringList{"mcp", "add", "--transport", "http", "fincept_community", kUrl, "--header",
                                                "Authorization: Bearer <FINCEPT_COMMUNITY_MCP_TOKEN>", "--timeout", "300000"}));
    const QJsonObject cp = QJsonDocument::fromJson(copilot->config_snippet.toUtf8())
                               .object()
                               .value(QStringLiteral("mcpServers"))
                               .toObject()
                               .value(QStringLiteral("fincept_community"))
                               .toObject();
    QCOMPARE(cp.value(QStringLiteral("type")).toString(), QStringLiteral("http"));
    QCOMPARE(cp.value(QStringLiteral("url")).toString(), kUrl);
    QCOMPARE(cp.value(QStringLiteral("tools")).toArray(), QJsonArray{QStringLiteral("*")});
    QCOMPARE(cp.value(QStringLiteral("timeout")).toInt(), 300000);
    QCOMPARE(copilot->config_path_hint, QStringLiteral("~/.copilot/mcp-config.json"));

    // Qwen Code: the Gemini CLI settings shape under ~/.qwen.
    const auto qwen = QwenAdapter().mcp_registration(endpoint());
    QVERIFY(qwen.has_value());
    verify_registration(*qwen);
    QVERIFY(!qwen->add.has_value());
    QCOMPARE(qwen->manual_args, (QStringList{"mcp", "add", "--transport", "http", "fincept_community", kUrl, "--header",
                                             "Authorization: Bearer <FINCEPT_COMMUNITY_MCP_TOKEN>", "--scope", "user"}));
    const QJsonObject qs = QJsonDocument::fromJson(qwen->config_snippet.toUtf8())
                               .object()
                               .value(QStringLiteral("mcpServers"))
                               .toObject()
                               .value(QStringLiteral("fincept_community"))
                               .toObject();
    QCOMPARE(qs.value(QStringLiteral("httpUrl")).toString(), kUrl);
    QCOMPARE(qs.value(QStringLiteral("timeout")).toInt(), 300000);
    QCOMPARE(qwen->config_path_hint, QStringLiteral("~/.qwen/settings.json"));
}

void TestHarnessCommands::edition_mcp_names_and_ports() {
    // Everything harness configs know this edition by differs from the private build.
    QCOMPARE(QString::fromLatin1(kMcpServerName), QStringLiteral("fincept_community"));
    QCOMPARE(QString::fromLatin1(kMcpTokenEnvVar), QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN"));
    for (const char* theirs : fincept::edition::kPrivateMcpServerNames)
        QVERIFY(QString::fromLatin1(kMcpServerName) != QLatin1String(theirs));
    QVERIFY(QString::fromLatin1(kMcpTokenEnvVar) != QLatin1String(fincept::edition::kForeignMcpTokenEnvVar));
    // Every adapter registers / runs under this edition's name.
    for (const IHarnessAdapter* a :
         {static_cast<const IHarnessAdapter*>(new ClaudeAdapter), static_cast<const IHarnessAdapter*>(new CodexAdapter),
          static_cast<const IHarnessAdapter*>(new GeminiAdapter), static_cast<const IHarnessAdapter*>(new GrokAdapter),
          static_cast<const IHarnessAdapter*>(new CursorAdapter), static_cast<const IHarnessAdapter*>(new CopilotAdapter),
          static_cast<const IHarnessAdapter*>(new QwenAdapter)}) {
        QCOMPARE(a->descriptor().run_mcp_server_name, QStringLiteral("fincept_community"));
        delete a;
    }
    // Ports: the private build's two are never this edition's.
    QVERIFY(fincept::edition::is_private_mcp_port(47821));
    QVERIFY(fincept::edition::is_private_mcp_port(47999));
    QVERIFY(!fincept::edition::is_private_mcp_port(41720));
    using fincept::edition::is_own_mcp_url;
    QVERIFY(is_own_mcp_url(QStringLiteral("http://127.0.0.1:41720/mcp")));
    QVERIFY(is_own_mcp_url(QStringLiteral("http://localhost:41739/mcp")));
    QVERIFY(!is_own_mcp_url(QStringLiteral("http://127.0.0.1:41740/mcp")));
    QVERIFY(!is_own_mcp_url(QStringLiteral("http://127.0.0.1:47821/mcp")));
    QVERIFY(!is_own_mcp_url(QStringLiteral("http://127.0.0.1:47821/mcp"), 47821)); // never, even if "configured"
    QVERIFY(is_own_mcp_url(QStringLiteral("http://127.0.0.1:50123/mcp"), 50123));  // a user-set port
    QVERIFY(!is_own_mcp_url(QStringLiteral("http://example.com:41720/mcp")));
    QVERIFY(!is_own_mcp_url(QStringLiteral("https://127.0.0.1:41720/mcp")));
    QVERIFY(!is_own_mcp_url(QStringLiteral("not a url")));
}

void TestHarnessCommands::legacy_entry_renamed_only_when_ours() {
    using fincept::harness::classify_legacy_entry;
    using fincept::harness::LegacyEntryAction;
    // `claude mcp get fincept_terminal` (HTTP entry on this edition's port).
    const QString claude_ours = QStringLiteral("fincept_terminal:\n  Scope: User config\n  Status: Connected\n  Type: http\n"
                                               "  URL: http://127.0.0.1:41720/mcp\n  Headers:\n    Authorization: Bearer "
                                               "${FINCEPT_MCP_TOKEN}\n\nTo remove this server, run: claude mcp remove "
                                               "\"fincept_terminal\" -s user\n");
    QCOMPARE(classify_legacy_entry(0, claude_ours, 41720), LegacyEntryAction::Migrate);
    // `codex mcp get` of an entry pointing at the private build's port: left alone.
    const QString codex_theirs = QStringLiteral("fincept_terminal\n  enabled: true\n  transport: streamable_http\n"
                                                "  url: http://127.0.0.1:47821/mcp\n  bearer_token_env_var: X\n");
    QCOMPARE(classify_legacy_entry(0, codex_theirs, 41720), LegacyEntryAction::LeaveForeign);
    // The stdio bridge form of an old entry (python … --url http://127.0.0.1:41722/mcp): ours.
    QCOMPARE(classify_legacy_entry(0,
                                   QStringLiteral("  Command: C:/py/python.exe\n  Args: C:/x/fincept_mcp_stdio.py --url "
                                                  "http://127.0.0.1:41722/mcp\n"),
                                   41720),
             LegacyEntryAction::Migrate);
    // Another host, no URL at all, a user-set port only when it is ours now.
    QCOMPARE(classify_legacy_entry(0, QStringLiteral("url: https://mcp.example.com/mcp"), 41720),
             LegacyEntryAction::LeaveForeign);
    QCOMPARE(classify_legacy_entry(0, QStringLiteral("Command: npx something"), 41720), LegacyEntryAction::LeaveForeign);
    QCOMPARE(classify_legacy_entry(0, QStringLiteral("URL: http://127.0.0.1:50000/mcp"), 50000), LegacyEntryAction::Migrate);
    QCOMPARE(classify_legacy_entry(0, QStringLiteral("URL: http://127.0.0.1:50000/mcp"), 41720),
             LegacyEntryAction::LeaveForeign);
    // Two URLs, one of them not ours: not provably ours.
    QCOMPARE(classify_legacy_entry(0, QStringLiteral("url: http://127.0.0.1:41720/mcp\nalso http://127.0.0.1:47999/x"), 41720),
             LegacyEntryAction::LeaveForeign);
    // No such entry.
    QCOMPARE(classify_legacy_entry(1, QStringLiteral("No MCP server named fincept_terminal"), 41720), LegacyEntryAction::Absent);
}

void TestHarnessCommands::codex_runs_see_only_the_terminal_server() {
    // config.toml forms: table headers (bare / quoted / sub-tables / arrays of
    // tables), keys inside [mcp_servers], top-level dotted keys, an inline
    // table, comments. Values (urls, tokens) are never returned.
    const QByteArray toml =
        "model = \"gpt-6\"\n"
        "# [mcp_servers.commented_out]\n"
        "[mcp_servers.fincept]  # the private build's\n"
        "url = \"http://127.0.0.1:47821/mcp\"\n"
        "http_headers = { Authorization = \"Bearer secret-value\" }\n"
        "[mcp_servers.\"fincept-cloud\"]\n"
        "command = \"npx\"\n"
        "[mcp_servers.github.env]\n"
        "GITHUB_TOKEN = \"ghp_secret\"\n"
        "[mcp_servers]\n"
        "fincept_terminal = { url = \"http://127.0.0.1:41720/mcp\" }\n"
        "notion.command = \"npx\"\n"
        "[projects.'C:\\work']\n"
        "trust_level = \"trusted\"\n";
    QCOMPARE(CodexAdapter::parse_mcp_server_names(toml),
             (QStringList{"fincept", "fincept-cloud", "github", "fincept_terminal", "notion"}));
    const QByteArray dotted = "mcp_servers.slack.url = \"http://x\"\nmcp_servers = { linear = { url = \"http://y\" }, "
                              "\"fincept_community\" = { url = \"http://z\" } }\n";
    QCOMPARE(CodexAdapter::parse_mcp_server_names(dotted), (QStringList{"slack", "linear", "fincept_community"}));
    QVERIFY(CodexAdapter::parse_mcp_server_names("[mcp_servers.\"bad name\"]\n").isEmpty()); // not -c addressable

    // Where the file is read: $CODEX_HOME, else ~/.codex.
    DetectContext ctx;
    ctx.home = QStringLiteral("/home/me");
    QString asked;
    ctx.read_file = [&asked, &toml](const QString& path, qint64) {
        asked = path;
        return toml;
    };
    QCOMPARE(CodexAdapter().user_mcp_server_names(ctx).size(), 5);
    QCOMPARE(asked, QStringLiteral("/home/me/.codex/config.toml"));
    ctx.env.insert(QStringLiteral("CODEX_HOME"), QStringLiteral("/data/codex"));
    CodexAdapter().user_mcp_server_names(ctx);
    QCOMPARE(asked, QStringLiteral("/data/codex/config.toml"));

    // The run switches every other entry off and keeps the terminal's own.
    const Installation inst = installed(QStringLiteral("/usr/local/bin/codex"), QStringLiteral("0.160.0"));
    RunSpec s = run_spec(false, {}, true, QStringLiteral("fincept_community"));
    s.user_mcp_server_names = {"fincept", "fincept-cloud", "github", "fincept_terminal", "fincept_community", "github"};
    const LaunchPlan plan = CodexAdapter().build_command(s, inst);
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    verify_no_secret(plan);
    for (const char* other : {"fincept", "fincept-cloud", "github", "fincept_terminal"})
        QCOMPARE(plan.args.count(QStringLiteral("mcp_servers.%1.enabled=false").arg(QLatin1String(other))), 1);
    QVERIFY(!plan.args.contains(QStringLiteral("mcp_servers.fincept_community.enabled=false")));
    QVERIFY(plan.args.contains(QStringLiteral("mcp_servers.fincept_community.url=\"http://127.0.0.1:41720/mcp\"")));
    QVERIFY(!plan.args.contains(QStringLiteral("--ignore-user-config"))); // the user's model / sign-in settings stay
    // Without terminal tools the run gets no MCP server at all (like Claude's --strict-mcp-config).
    RunSpec bare = run_spec(false, {}, false, {});
    bare.user_mcp_server_names = {"fincept_community", "fincept"};
    const LaunchPlan bp = CodexAdapter().build_command(bare, inst);
    QVERIFY(bp.args.contains(QStringLiteral("mcp_servers.fincept_community.enabled=false")));
    QVERIFY(bp.args.contains(QStringLiteral("mcp_servers.fincept.enabled=false")));
    // Claude: only the per-run config, every run.
    const LaunchPlan cp = ClaudeAdapter().build_command(run_spec(false, {}, false, {}),
                                                        installed(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("2.1.325")));
    QVERIFY(cp.args.contains(QStringLiteral("--strict-mcp-config")));
}

void TestHarnessCommands::mcp_config_windows_paths_with_spaces() {
    // Claude Code's per-run --mcp-config file lives in the user's cache folder,
    // which on Windows often contains a space ("C:/Users/John Doe/...").
    RunSpec s = run_spec(false, {}, true, QStringLiteral("fincept_community"));
    s.temp_dir = QStringLiteral("C:/Users/John Doe/AppData/Local/Fincept Terminal/cache/coding_agents");
    const LaunchPlan plan = ClaudeAdapter().build_command(
        s, installed(QStringLiteral("C:/Program Files/claude/claude.exe"), QStringLiteral("2.1.325")));
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    verify_no_secret(plan);
    const qsizetype i = plan.args.indexOf(QStringLiteral("--mcp-config"));
    QVERIFY(i >= 0);
    // One argv element with the spaces intact, and declared as a temp file so the
    // service writes it before the start and deletes it with the run.
    const QString cfg = s.temp_dir + QStringLiteral("/mcp-0123abcd.json");
    QCOMPARE(plan.args.at(i + 1), cfg);
    QCOMPARE(plan.temp_files.size(), 1);
    QCOMPARE(plan.temp_files[0].name, QStringLiteral("mcp-0123abcd.json"));
#ifdef Q_OS_WIN
    // Through an npm .cmd shim the path is quoted for cmd.exe, never split.
    Installation shim =
        installed(QStringLiteral("C:/Users/John Doe/AppData/Roaming/npm/claude.cmd"), QStringLiteral("2.1.325"));
    shim.via = Installation::CmdShim;
    const LaunchSpec ls = make_launch_spec(shim, plan.args);
    QVERIFY2(ls.error.isEmpty(), qPrintable(ls.error));
    QVERIFY2(ls.native_args.contains(QLatin1Char('"') + cfg + QLatin1Char('"')), qPrintable(ls.native_args));
#endif
    // Codex takes the server through -c values: the URL is one TOML string, no file.
    RunSpec c = run_spec(false, {}, true, QStringLiteral("fincept_community"));
    c.temp_dir = s.temp_dir;
    const LaunchPlan cp = CodexAdapter().build_command(c, installed(QStringLiteral("C:/x/codex.exe"), QStringLiteral("0.160.0")));
    QVERIFY2(cp.error.isEmpty(), qPrintable(cp.error));
    verify_no_secret(cp);
    QVERIFY(cp.args.contains(QStringLiteral("mcp_servers.fincept_community.url=\"http://127.0.0.1:41720/mcp\"")));
    QVERIFY(cp.temp_files.isEmpty());
}

void TestHarnessCommands::mcp_registration_rejects_bad_endpoints() {
    QVector<McpEndpoint> bad;
    McpEndpoint e = endpoint();
    e.name = QStringLiteral("fincept terminal");
    bad << e;
    e = endpoint();
    e.name = QStringLiteral("-x");
    bad << e;
    e = endpoint();
    e.url = QStringLiteral("http://x\" --evil");
    bad << e;
    e = endpoint();
    e.url = QString();
    bad << e;
    e = endpoint();
    e.url = QStringLiteral("file:///etc/passwd");
    bad << e;
    e = endpoint();
    e.token_env_var = QStringLiteral("BAD VAR");
    bad << e;
    for (const auto& adapter : all_adapters()) {
        QVERIFY(!adapter->descriptor().id.isEmpty());
        for (int i = 0; i < bad.size(); ++i)
            QVERIFY2(!adapter->mcp_registration(bad[i]).has_value(),
                     qPrintable(adapter->descriptor().id + QStringLiteral(" #") + QString::number(i)));
    }
}

// ── Launch spec and environment ─────────────────────────────────────────────

void TestHarnessCommands::launch_spec_native_and_node_script() {
    const QStringList args = {QStringLiteral("--version")};

    const LaunchSpec native = make_launch_spec(installed(QStringLiteral("/usr/local/bin/claude"), {}), args);
    QVERIFY(native.error.isEmpty());
    QCOMPARE(native.program, QStringLiteral("/usr/local/bin/claude"));
    QCOMPARE(native.args, args);
    QVERIFY(!native.use_native);

    Installation node = installed(QStringLiteral("C:/npm/claude.cmd"), {});
    node.via = Installation::NodeScript;
    node.node_path = QStringLiteral("C:/Program Files/nodejs/node.exe");
    node.script_path = QStringLiteral("C:/npm/node_modules/@anthropic-ai/claude-code/cli.js");
    const LaunchSpec viaNode = make_launch_spec(node, args);
    QVERIFY(viaNode.error.isEmpty());
    QCOMPARE(viaNode.program, node.node_path);
    QCOMPARE(viaNode.args, (QStringList{QDir::toNativeSeparators(node.script_path), QStringLiteral("--version")}));
    QVERIFY(!viaNode.use_native); // a plain argument list — no cmd.exe

    Installation broken = node;
    broken.node_path.clear();
    QVERIFY(!make_launch_spec(broken, args).error.isEmpty());

    Installation wsl = installed(QStringLiteral("/usr/bin/claude"), {});
    wsl.via = Installation::Wsl;
    QVERIFY(!make_launch_spec(wsl, args).error.isEmpty());

    QCOMPARE(msvc_quote(QStringLiteral("plain")), QStringLiteral("plain"));
    QCOMPARE(msvc_quote(QStringLiteral("a b")), QStringLiteral("\"a b\""));
    QCOMPARE(msvc_quote(QStringLiteral("C:\\dir\\")), QStringLiteral("C:\\dir\\"));
    QCOMPARE(msvc_quote(QStringLiteral("C:\\my dir\\")), QStringLiteral("\"C:\\my dir\\\\\""));
    QCOMPARE(msvc_quote(QStringLiteral("say \"hi\"")), QStringLiteral("\"say \\\"hi\\\"\""));
    QVERIFY(cmd_arg_safe(QStringLiteral("--model sonnet")));
    QVERIFY(!cmd_arg_safe(QStringLiteral("100%")));
    QVERIFY(!cmd_arg_safe(QStringLiteral("a&b")));
}

void TestHarnessCommands::child_environment_rules() {
    QProcessEnvironment base;
    base.insert(QStringLiteral("CLAUDECODE"), QStringLiteral("1"));
    base.insert(QStringLiteral("CLAUDE_CODE_ENTRYPOINT"), QStringLiteral("cli"));
    base.insert(QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN"), QStringLiteral("stale-token"));
    base.insert(QStringLiteral("FINCEPT_MCP_TOKEN"), QStringLiteral("private-build-token")); // the other edition's
    base.insert(QStringLiteral("ANTHROPIC_API_KEY"), QStringLiteral("placeholder"));
    base.insert(QStringLiteral("KEEP_ME_NOT"), QStringLiteral("1"));
    base.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));

    const Installation inst = installed(QStringLiteral("/opt/tools/bin/claude"), QStringLiteral("2.1.325"));
    ChildEnvOptions opt;
    opt.base = base;
    opt.home = QStringLiteral("/home/me");
    opt.mcp_token = kToken;
    opt.set.insert(QStringLiteral("FOO"), QStringLiteral("bar"));
    opt.set.insert(QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN"), QStringLiteral("from-adapter")); // never wins
    opt.set.insert(QStringLiteral("FINCEPT_MCP_TOKEN"), QStringLiteral("from-adapter")); // never set at all
    opt.unset << QStringLiteral("KEEP_ME_NOT");
    opt.strip << QStringLiteral("ANTHROPIC_API_KEY"); // env policy "subscription only"

    const QProcessEnvironment env = child_environment(inst, opt);
    QVERIFY(!env.contains(QStringLiteral("CLAUDECODE")));
    QVERIFY(!env.contains(QStringLiteral("CLAUDE_CODE_ENTRYPOINT")));
    QCOMPARE(env.value(QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN")), kToken); // the token travels here and only here
    QVERIFY(!env.contains(QStringLiteral("FINCEPT_MCP_TOKEN"))); // the private build's variable never reaches a child
    QVERIFY(!env.contains(QStringLiteral("ANTHROPIC_API_KEY")));
    QVERIFY(!env.contains(QStringLiteral("KEEP_ME_NOT")));
    QCOMPARE(env.value(QStringLiteral("FOO")), QStringLiteral("bar"));
#ifndef Q_OS_WIN
    QCOMPARE(env.value(QStringLiteral("PATH")),
             QStringLiteral("/opt/tools/bin:/home/me/.local/bin:/opt/homebrew/bin:/usr/local/bin:"
                            "/home/me/.npm-global/bin:/home/me/.bun/bin:/usr/bin"));
#endif

    // Default policy (inherit) and no tools: the key stays, no token at all.
    ChildEnvOptions plain;
    plain.base = base;
    plain.home = QStringLiteral("/home/me");
    const QProcessEnvironment env2 = child_environment(inst, plain);
    QVERIFY(env2.contains(QStringLiteral("ANTHROPIC_API_KEY")));
    QVERIFY(!env2.contains(QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN")));
    QVERIFY(!env2.contains(QStringLiteral("FINCEPT_MCP_TOKEN")));
}

// ── Sign-in ──────────────────────────────────────────────────────────────────

void TestHarnessCommands::claude_parse_auth() {
    const ClaudeAdapter claude;

    AuthStatus a = claude.parse_auth(
        0, R"({"loggedIn":true,"authMethod":"claude.ai","apiProvider":"firstParty","subscriptionType":"max"})", {});
    QCOMPARE(a.state, AuthState::SignedIn);
    QCOMPARE(a.method, QStringLiteral("subscription"));
    QCOMPARE(a.plan, QStringLiteral("max"));

    a = claude.parse_auth(1, R"({"loggedIn":false})", {});
    QCOMPARE(a.state, AuthState::SignedOut);
    QVERIFY(a.method.isEmpty());
    QVERIFY(a.plan.isEmpty());

    a = claude.parse_auth(0, R"({"loggedIn":true,"authMethod":"api_key","apiProvider":"firstParty"})", {});
    QCOMPARE(a.state, AuthState::SignedIn);
    QCOMPARE(a.method, QStringLiteral("api_key"));

    a = claude.parse_auth(0, R"({"loggedIn":true,"authMethod":"third_party","apiProvider":"bedrock"})", {});
    QCOMPARE(a.method, QStringLiteral("cloud"));
    QCOMPARE(a.note, QStringLiteral("bedrock"));

    // A warning line before the document is tolerated.
    a = claude.parse_auth(0, "Update available\n{\"loggedIn\":true,\"authMethod\":\"claude.ai\"}\n", {});
    QCOMPARE(a.state, AuthState::SignedIn);
    QCOMPARE(a.method, QStringLiteral("subscription"));

    a = claude.parse_auth(1, "error: unknown command\n", {});
    QCOMPARE(a.state, AuthState::Error);

    a = claude.parse_auth(0, "", {});
    QCOMPARE(a.state, AuthState::Unknown);

    // Odd values are dropped rather than shown; stderr is never read.
    a = claude.parse_auth(0, R"({"loggedIn":true,"subscriptionType":"<b>x</b> plan with a very long name indeed ok"})",
                          "IDENTITY-MARKER");
    QVERIFY(a.plan.isEmpty());
    for (const QString& f : {a.method, a.plan, a.note})
        QVERIFY(!f.contains(QLatin1String("IDENTITY-MARKER")));
}

void TestHarnessCommands::codex_parse_auth() {
    const CodexAdapter codex;

    AuthStatus a = codex.parse_auth(0, {}, "Logged in using ChatGPT\n"); // the answer is on stderr
    QCOMPARE(a.state, AuthState::SignedIn);
    QCOMPARE(a.method, QStringLiteral("subscription"));

    a = codex.parse_auth(0, {}, "Logged in using an API key - sk-proj-***ABCD\n");
    QCOMPARE(a.state, AuthState::SignedIn);
    QCOMPARE(a.method, QStringLiteral("api_key"));
    for (const QString& f : {a.method, a.plan, a.note})
        QVERIFY(!f.contains(QLatin1String("sk-"))); // nothing of the text is kept

    a = codex.parse_auth(1, {}, "Not logged in\n");
    QCOMPARE(a.state, AuthState::SignedOut);

    a = codex.parse_auth(2, {}, {});
    QCOMPARE(a.state, AuthState::Error);
    QVERIFY(a.note.contains(QLatin1String("2")));

    a = codex.parse_auth(0, {}, {});
    QCOMPARE(a.state, AuthState::Unknown);
}

void TestHarnessCommands::auth_hints_use_names_and_existence_only() {
    auto ctx_with = [](const QHash<QString, QString>& vars, const QString& existing_file) {
        DetectContext ctx;
        ctx.windows = false;
        ctx.home = QStringLiteral("/home/me");
        QProcessEnvironment env;
        for (auto it = vars.cbegin(); it != vars.cend(); ++it)
            env.insert(it.key(), it.value());
        ctx.env = env;
        ctx.is_file = [existing_file](const QString& p) { return p == existing_file; };
        return ctx;
    };
    const ClaudeAdapter claude;
    const CodexAdapter codex;

    AuthStatus a = claude.auth_hint(ctx_with({{QStringLiteral("ANTHROPIC_API_KEY"), QStringLiteral("placeholder")}}, {}));
    QVERIFY(a.api_key_env_present);
    QCOMPARE(a.method, QStringLiteral("api_key"));
    QCOMPARE(a.state, AuthState::Unknown); // a hint is never a sign-in verdict

    a = claude.auth_hint(ctx_with({{QStringLiteral("CLAUDE_CODE_USE_BEDROCK"), QStringLiteral("1")}}, {}));
    QCOMPARE(a.method, QStringLiteral("cloud"));
    QCOMPARE(a.note, QStringLiteral("bedrock"));
    QVERIFY(!a.api_key_env_present);

    a = claude.auth_hint(ctx_with({{QStringLiteral("CLAUDE_CODE_USE_BEDROCK"), QStringLiteral("0")}},
                                  QStringLiteral("/home/me/.claude/.credentials.json")));
    QVERIFY(a.method.isEmpty());
    QVERIFY(!a.note.isEmpty()); // a saved sign-in exists (never opened)

    a = codex.auth_hint(ctx_with({{QStringLiteral("OPENAI_API_KEY"), QStringLiteral("placeholder")}},
                                 QStringLiteral("/home/me/.codex/auth.json")));
    QVERIFY(a.api_key_env_present);
    QCOMPARE(a.method, QStringLiteral("api_key"));
    QVERIFY(!a.note.isEmpty());
}

// ── Models ───────────────────────────────────────────────────────────────────

void TestHarnessCommands::codex_parse_models() {
    const CodexAdapter codex;
    const QByteArray fixture = R"({"models":[
        {"slug":"gpt-hidden","display_name":"Hidden","visibility":"hide","priority":1,
         "supported_reasoning_levels":[{"effort":"low"}]},
        {"slug":"gpt-b","display_name":"GPT-B","description":"Second","visibility":"list","priority":3,
         "default_reasoning_level":"medium","context_window":272000,
         "supported_reasoning_levels":[{"effort":"low","description":"x"},{"effort":"high"},{"effort":"ultra"}]},
        {"slug":"gpt-a","display_name":"GPT-A","visibility":"list","priority":2,"default_reasoning_level":"high",
         "supported_reasoning_levels":[{"effort":"medium"},{"effort":"NOT VALID"}]},
        {"slug":"gpt-c","display_name":"GPT-C","priority":4},
        {"slug":"bad slug with spaces","visibility":"list","priority":0},
        {"display_name":"no slug"}
    ]})";
    const QVector<ModelInfo> list = codex.parse_models(0, fixture);
    QCOMPARE(list.size(), 3);
    QCOMPARE(list[0].id, QStringLiteral("gpt-a")); // by priority, hidden and invalid entries dropped
    QCOMPARE(list[0].label, QStringLiteral("GPT-A"));
    QCOMPARE(list[0].efforts, QStringList({QStringLiteral("medium")}));
    QCOMPARE(list[0].default_effort, QStringLiteral("high"));
    QCOMPARE(list[1].id, QStringLiteral("gpt-b"));
    QCOMPARE(list[1].description, QStringLiteral("Second"));
    QCOMPARE(list[1].efforts, QStringList({QStringLiteral("low"), QStringLiteral("high"), QStringLiteral("ultra")}));
    QCOMPARE(list[1].default_effort, QStringLiteral("medium"));
    QCOMPARE(list[1].context_window, qint64(272000));
    QCOMPARE(list[2].id, QStringLiteral("gpt-c")); // no visibility field: kept

    QVERIFY(codex.parse_models(1, fixture).isEmpty());
    QVERIFY(codex.parse_models(0, "not json").isEmpty());
    QVERIFY(codex.parse_models(0, R"({"something":"else"})").isEmpty());

    const ClaudeAdapter claude;
    QVERIFY(!claude.models_command().has_value()); // no list command: built-in aliases
    QVERIFY(codex.models_command().has_value());
    QCOMPARE(codex.models_command()->args, QStringList({QStringLiteral("debug"), QStringLiteral("models")}));
}

void TestHarnessCommands::builtin_models_and_efforts() {
    const ClaudeAdapter claude;
    const CodexAdapter codex;
    const QStringList claude_efforts = {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"),
                                        QStringLiteral("xhigh"), QStringLiteral("max")};
    const QVector<ModelInfo> aliases = claude.builtin_models(QStringLiteral("2.1.325"));
    QVERIFY(!aliases.isEmpty());
    QStringList ids;
    for (const ModelInfo& m : aliases) {
        ids << m.id;
        QCOMPARE(m.efforts, claude_efforts);
    }
    QVERIFY(ids.contains(QStringLiteral("opus")));
    QVERIFY(ids.contains(QStringLiteral("sonnet")));
    QCOMPARE(claude.efforts(QStringLiteral("sonnet"), QStringLiteral("2.1.325")), claude_efforts);

    QVERIFY(!codex.builtin_models(QStringLiteral("0.160.0")).isEmpty());
    QVERIFY(codex.efforts(QStringLiteral("gpt-5.6-terra"), {}).contains(QStringLiteral("ultra")));
    QVERIFY(!codex.efforts(QStringLiteral("gpt-6-luna"), {}).contains(QStringLiteral("ultra")));
    QVERIFY(!codex.efforts(QStringLiteral("some-future-model"), {}).isEmpty());

    QCOMPARE(claude.version_command().args, QStringList({QStringLiteral("--version")}));
    QCOMPARE(claude.auth_command()->args,
             QStringList({QStringLiteral("auth"), QStringLiteral("status"), QStringLiteral("--json")}));
    QCOMPARE(codex.auth_command()->args, QStringList({QStringLiteral("login"), QStringLiteral("status")}));
}

QTEST_GUILESS_MAIN(TestHarnessCommands)
#include "tst_harness_commands.moc"
