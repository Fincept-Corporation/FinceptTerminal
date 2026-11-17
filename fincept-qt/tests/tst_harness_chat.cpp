// tst_harness_chat.cpp — terminal AI through a harness (HarnessChatRun +
// HarnessProcessPool + the Claude / Codex chat command lines), driven against a
// FAKE harness CLI: this test executable re-runs itself with
// FINCEPT_FAKE_HARNESS=<fixture>, reads the prompt from stdin and replays the
// fixture's stdout lines (tests/data/harness/<id>/*.jsonl). No real CLI, no
// model, no network, no key — nothing here can spend quota.
//
// Covered: the chat argv (no built-in tools, read-only, strict MCP config, the
// subscription token removed, no token in argv), stream mapping (thinking,
// text deltas, the narration reset before a tool call, tool call / result
// pairing, usage, cost, the reported model), the fresh scratch folder removed
// afterwards, the MCP config file inside it, cancel (process tree gone),
// first-event and total timeouts, failure before output → fallback with the
// announcement, failure after output → no fallback, a subscription session
// refused before any output, and the process pool.

#include "services/harness/HarnessChatRun.h"
#include "services/harness/HarnessEligibility.h"
#include "services/harness/HarnessProcessPool.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/harness/adapters/ClaudeAdapter.h"
#include "services/harness/adapters/CodexAdapter.h"
#include "services/llm/ModelRouter.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#ifndef Q_OS_WIN
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace fincept::harness;
namespace cad = fincept::services::coding_agents;

namespace {

constexpr const char* kFakeVar = "FINCEPT_FAKE_HARNESS";

QString thc_data_root() {
    QString root = qEnvironmentVariable("FINCEPT_HARNESS_FIXTURES");
    if (root.isEmpty())
        root = QFINDTESTDATA("data/harness");
    return root;
}

void thc_sleep_ms(int ms) {
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int thc_env_int(const char* name, int fallback = 0) {
    bool ok = false;
    const int v = qEnvironmentVariableIntValue(name, &ok);
    return ok ? v : fallback;
}

/// The fake CLI: this executable with FINCEPT_FAKE_HARNESS set.
int thc_fake_harness_main(int argc, char** argv) {
    // The prompt arrives on stdin and stdin is closed afterwards.
    std::string prompt;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), stdin)) > 0)
        prompt.append(buf, n);

    const QString report = qEnvironmentVariable("FINCEPT_FAKE_REPORT");
    if (!report.isEmpty()) {
        QFile f(report);
        if (f.open(QIODevice::WriteOnly)) {
            QStringList args;
            for (int i = 1; i < argc; ++i) // one report line: line breaks inside an argument are escaped
                args << QString::fromLocal8Bit(argv[i]).replace(QLatin1Char('\n'), QStringLiteral("\\n"));
            QString mcp_file;
            const qsizetype at = args.indexOf(QStringLiteral("--mcp-config"));
            if (at >= 0 && at + 1 < args.size())
                mcp_file = args.at(at + 1);
            const QByteArray out =
                QStringLiteral("cwd=%1\npid=%2\noauth=%3\ntoken=%4\nmcp=%5\nargs=%6\nprompt=%7\n")
                    .arg(QDir::currentPath(), QString::number(QCoreApplication::applicationPid()),
                         qEnvironmentVariableIsSet("CLAUDE_CODE_OAUTH_TOKEN") ? QStringLiteral("1") : QStringLiteral("0"),
                         qEnvironmentVariableIsSet("FINCEPT_MCP_TOKEN") ? QStringLiteral("1") : QStringLiteral("0"),
                         !mcp_file.isEmpty() && QFileInfo::exists(mcp_file) ? QStringLiteral("1") : QStringLiteral("0"),
                         args.join(QStringLiteral("\x1f")), QString::fromStdString(prompt))
                    .toUtf8();
            f.write(out);
        }
    }
    thc_sleep_ms(thc_env_int("FINCEPT_FAKE_SLEEP_BEFORE_MS"));
    QFile fixture(qEnvironmentVariable(kFakeVar));
    if (!fixture.open(QIODevice::ReadOnly))
        return 3;
    const int delay = thc_env_int("FINCEPT_FAKE_DELAY_MS");
    const int sleep_after_first = thc_env_int("FINCEPT_FAKE_SLEEP_AFTER_FIRST_MS");
    bool first = true;
    while (!fixture.atEnd()) {
        const QByteArray line = fixture.readLine();
        if (line.trimmed().isEmpty())
            continue;
        std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
        if (!line.endsWith('\n'))
            std::fputc('\n', stdout);
        std::fflush(stdout);
        if (first) {
            first = false;
            thc_sleep_ms(sleep_after_first);
        }
        thc_sleep_ms(delay);
    }
    const QByteArray err = qgetenv("FINCEPT_FAKE_STDERR");
    if (!err.isEmpty()) {
        std::fwrite(err.constData(), 1, static_cast<size_t>(err.size()), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
    thc_sleep_ms(thc_env_int("FINCEPT_FAKE_SLEEP_AFTER_MS"));
    return thc_env_int("FINCEPT_FAKE_EXIT");
}

/// Codex's parser / classification with a plain command line, so the runner
/// is tested the same way on every OS (Codex chat is refused on Windows).
class ThcWrappedAdapter final : public IHarnessAdapter {
  public:
    explicit ThcWrappedAdapter(std::unique_ptr<IHarnessAdapter> real) : real_(std::move(real)) {}
    const HarnessDescriptor& descriptor() const override { return real_->descriptor(); }
    QStringList candidate_paths(const DetectContext& c) const override { return real_->candidate_paths(c); }
    ProbeCommand version_command() const override { return real_->version_command(); }
    QString parse_version(const QString& o) const override { return real_->parse_version(o); }
    std::optional<ProbeCommand> auth_command() const override { return real_->auth_command(); }
    AuthStatus parse_auth(int c, const QByteArray& o, const QByteArray& e) const override {
        return real_->parse_auth(c, o, e);
    }
    AuthStatus auth_hint(const DetectContext& c) const override { return real_->auth_hint(c); }
    std::optional<ProbeCommand> models_command() const override { return real_->models_command(); }
    QVector<ModelInfo> parse_models(int c, const QByteArray& o) const override { return real_->parse_models(c, o); }
    QVector<ModelInfo> builtin_models(const QString& v) const override { return real_->builtin_models(v); }
    QStringList efforts(const QString& m, const QString& v) const override { return real_->efforts(m, v); }
    LaunchPlan build_command(const RunSpec& spec, const Installation&) const override {
        LaunchPlan p;
        p.args = {QStringLiteral("exec"), QStringLiteral("--json"), QStringLiteral("-")};
        p.stdin_payload = spec.prompt.toUtf8();
        return p;
    }
    LaunchPlan mcp_config_for_run(const McpEndpoint& e, const RunSpec& s) const override {
        return real_->mcp_config_for_run(e, s);
    }
    std::unique_ptr<IStreamParser> make_parser() const override { return real_->make_parser(); }
    CancelPlan cancel_plan() const override { return real_->cancel_plan(); }
    FailureKind classify_failure(int c, const cad::StreamFacts& f, const QStringList& t) const override {
        return real_->classify_failure(c, f, t);
    }
    std::optional<McpRegistration> mcp_registration(const McpEndpoint& e) const override {
        return real_->mcp_registration(e);
    }

  private:
    std::unique_ptr<IHarnessAdapter> real_;
};

/// What the sink saw, in order.
struct ThcRecord {
    QStringList order; // "text:<d>" "think:<d>" "reset" "call:<id>:<tool>" "result:<id>:<tool>:<ok>"
    QString text;
    QString thinking;
    int events = 0;
};

HarnessChatSink thc_sink(ThcRecord& rec, const std::function<bool()>& cancelled = {}) {
    HarnessChatSink s;
    s.text = [&rec](const QString& d) {
        rec.order << QStringLiteral("text:") + d;
        rec.text += d;
    };
    s.thinking = [&rec](const QString& d) {
        rec.order << QStringLiteral("think:") + d;
        rec.thinking += d;
    };
    s.text_reset = [&rec]() {
        rec.order << QStringLiteral("reset");
        rec.text.clear();
    };
    s.tool_call = [&rec](const QString& id, const QString& tool, const QString&) {
        rec.order << QStringLiteral("call:%1:%2").arg(id, tool);
    };
    s.tool_result = [&rec](const QString& id, const QString& tool, bool ok, const QString&) {
        rec.order << QStringLiteral("result:%1:%2:%3").arg(id, tool, ok ? QStringLiteral("ok") : QStringLiteral("fail"));
    };
    s.event = [&rec](const cad::AgentEvent&) { ++rec.events; };
    s.cancelled = cancelled;
    return s;
}

struct ThcFake {
    QString fixture;
    int sleep_before_ms = 0;
    int sleep_after_first_ms = 0;
    int delay_ms = 0;
    int sleep_after_ms = 0;
    int exit_code = 0;
    QString report;
    QString stderr_text;
};

HarnessChatEnvironment thc_env(const IHarnessAdapter* adapter, const QString& version, const QString& scratch,
                               const ThcFake& fake) {
    HarnessChatEnvironment env;
    env.adapter = adapter;
    env.installation.path = QCoreApplication::applicationFilePath();
    env.installation.via = Installation::Native;
    env.installation.version = version;
    env.scratch_root = scratch;
    env.environment = [fake](const LaunchPlan& plan) {
        QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
        e.insert(QString::fromLatin1(kFakeVar), fake.fixture);
        e.insert(QStringLiteral("FINCEPT_FAKE_SLEEP_BEFORE_MS"), QString::number(fake.sleep_before_ms));
        e.insert(QStringLiteral("FINCEPT_FAKE_SLEEP_AFTER_FIRST_MS"), QString::number(fake.sleep_after_first_ms));
        e.insert(QStringLiteral("FINCEPT_FAKE_DELAY_MS"), QString::number(fake.delay_ms));
        e.insert(QStringLiteral("FINCEPT_FAKE_SLEEP_AFTER_MS"), QString::number(fake.sleep_after_ms));
        e.insert(QStringLiteral("FINCEPT_FAKE_EXIT"), QString::number(fake.exit_code));
        if (!fake.report.isEmpty())
            e.insert(QStringLiteral("FINCEPT_FAKE_REPORT"), fake.report);
        if (!fake.stderr_text.isEmpty())
            e.insert(QStringLiteral("FINCEPT_FAKE_STDERR"), fake.stderr_text);
        // A subscription token in the user's environment: the plan must remove it.
        e.insert(QStringLiteral("CLAUDE_CODE_OAUTH_TOKEN"), QStringLiteral("subscription-token-must-not-pass"));
        for (const QString& k : plan.env_unset)
            e.remove(k);
        for (auto it = plan.env_set.cbegin(); it != plan.env_set.cend(); ++it)
            e.insert(it.key(), it.value());
        return e;
    };
    return env;
}

HarnessChatRequest thc_request(const QString& harness_id) {
    HarnessChatRequest r;
    r.harness_id = harness_id;
    r.model = QStringLiteral("sonnet");
    r.effort = QStringLiteral("high");
    r.system_prompt = QStringLiteral("You are Fincept AI.");
    r.user_message = QStringLiteral("What is open?");
    r.history = {ChatTurn{QStringLiteral("system"), QStringLiteral("Node instructions.")},
                 ChatTurn{QStringLiteral("user"), QStringLiteral("hello")},
                 ChatTurn{QStringLiteral("assistant"), QStringLiteral("hi there")}};
    r.feature = QStringLiteral("ai_chat");
    r.first_event_timeout_ms = 20000;
    r.timeout_ms = 60000;
    return r;
}

QHash<QString, QString> thc_read_report(const QString& path) {
    QHash<QString, QString> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QString all = QString::fromUtf8(f.readAll());
    const qsizetype p = all.indexOf(QStringLiteral("\nprompt="));
    const QString head = p >= 0 ? all.left(p) : all;
    for (const QString& line : head.split(QLatin1Char('\n'))) {
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq > 0)
            out.insert(line.left(eq), line.mid(eq + 1));
    }
    if (p >= 0)
        out.insert(QStringLiteral("prompt"), all.mid(p + 8));
    return out;
}

bool thc_process_alive(qint64 pid) {
#ifndef Q_OS_WIN
    return pid > 0 && ::kill(static_cast<pid_t>(pid), 0) == 0;
#else
    Q_UNUSED(pid);
    return false; // the job object reaps the tree; not observable portably here
#endif
}

} // namespace

class TestHarnessChat : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase();
    void claude_chat_argv();
    void claude_chat_refuses_old_version_and_writes();
    void codex_chat_argv();
    void flatten_prompt_and_system_prompt();
    void claude_stream_maps_to_chat_channels();
    void codex_stream_maps_to_chat_channels();
    void failure_before_output_falls_back();
    void failure_after_output_never_falls_back();
    void subscription_session_is_refused_before_output();
    void first_event_timeout();
    void total_timeout();
    void cancel_kills_the_process();
    void process_pool_limits_and_queues();

  private:
    QString data_;
    QTemporaryDir tmp_;
};

void TestHarnessChat::initTestCase() {
    data_ = thc_data_root();
    QVERIFY2(QDir(data_).exists(), "tests/data/harness not found (set FINCEPT_HARNESS_FIXTURES)");
    QVERIFY(tmp_.isValid());
}

void TestHarnessChat::claude_chat_argv() {
    ClaudeAdapter claude;
    QVERIFY(claude.descriptor().caps.chat);
    Installation inst;
    inst.path = QStringLiteral("/usr/local/bin/claude");
    inst.version = QStringLiteral("2.1.325");
    RunSpec s;
    s.chat = true;
    s.prompt = QStringLiteral("hello");
    s.system_prompt = QStringLiteral("SYS");
    s.model = QStringLiteral("sonnet");
    s.effort = QStringLiteral("high");
    s.want_deltas = true;
    s.run_id = QStringLiteral("abcdef0123456789");
    s.temp_dir = QStringLiteral("/scratch/run");
    McpEndpoint ep;
    ep.name = QStringLiteral("fincept");
    ep.url = QStringLiteral("http://127.0.0.1:41720/mcp");
    s.mcp = ep;
    const LaunchPlan p = claude.build_command(s, inst);
    QVERIFY2(p.error.isEmpty(), qPrintable(p.error));
    const QStringList expected = {"-p", "--output-format", "stream-json", "--verbose", "--include-partial-messages",
                                  "--no-session-persistence", "--restricted", "--tools", "",
                                  "--mcp-config", "/scratch/run/mcp-abcdef01.json", "--strict-mcp-config",
                                  "--allowedTools", "mcp__fincept__*", "--permission-mode", "dontAsk",
                                  "--permission-prompts", "none", "--system-prompt", "SYS", "--model", "sonnet",
                                  "--effort", "high"};
    QCOMPARE(p.args, expected);
    QCOMPARE(p.stdin_payload, QByteArray("hello"));
    QCOMPARE(p.env_unset, QStringList{QStringLiteral("CLAUDE_CODE_OAUTH_TOKEN")});
    QCOMPARE(p.temp_files.size(), 1);
    // The edition's own variable name (never a token, never the other edition's FINCEPT_MCP_TOKEN).
    QVERIFY(p.temp_files.at(0).content.contains("${FINCEPT_COMMUNITY_MCP_TOKEN}"));
    QVERIFY(!p.temp_files.at(0).content.contains("${FINCEPT_MCP_TOKEN}"));
    // No tools at all, and never the user's own session list.
    s.mcp.reset();
    const LaunchPlan bare = claude.build_command(s, inst);
    QVERIFY(!bare.args.contains(QStringLiteral("--allowedTools")) && !bare.args.contains(QStringLiteral("--mcp-config")));
    QVERIFY(bare.args.contains(QStringLiteral("--strict-mcp-config")));
    // Chat Mode keeps the session and resumes it.
    s.chat_persist_session = true;
    s.resume_session_id = QStringLiteral("7d1c0b2a-0000-4000-8000-0000000c4a71");
    const LaunchPlan resumed = claude.build_command(s, inst);
    QVERIFY(resumed.error.isEmpty());
    QVERIFY(!resumed.args.contains(QStringLiteral("--no-session-persistence")));
    QCOMPARE(resumed.args.mid(resumed.args.size() - 2),
             (QStringList{QStringLiteral("--resume"), s.resume_session_id}));
    s.chat_persist_session = false; // a resume id without a kept session is refused
    QVERIFY(!claude.build_command(s, inst).error.isEmpty());
}

void TestHarnessChat::claude_chat_refuses_old_version_and_writes() {
    ClaudeAdapter claude;
    Installation inst;
    inst.path = QStringLiteral("/usr/local/bin/claude");
    RunSpec s;
    s.chat = true;
    s.prompt = QStringLiteral("hello");
    inst.version = QStringLiteral("2.1.300");
    QVERIFY(!claude.build_command(s, inst).error.isEmpty());
    inst.version.clear();
    QVERIFY(!claude.build_command(s, inst).error.isEmpty());
    inst.version = QStringLiteral("2.1.330");
    QVERIFY(claude.build_command(s, inst).error.isEmpty());
    s.permission = PermissionMode::WorkspaceWrite;
    QVERIFY(!claude.build_command(s, inst).error.isEmpty());
    s.permission = PermissionMode::ReadOnly;
    s.model = QStringLiteral("-x evil");
    QVERIFY(!claude.build_command(s, inst).error.isEmpty());
}

void TestHarnessChat::codex_chat_argv() {
    CodexAdapter codex;
    Installation inst;
    inst.path = QStringLiteral("/usr/local/bin/codex");
    inst.version = QStringLiteral("0.160.0");
    RunSpec s;
    s.chat = true;
    s.prompt = QStringLiteral("hello");
    s.system_prompt = QStringLiteral("SYS");
    s.model = QStringLiteral("gpt-6-luna");
    s.effort = QStringLiteral("low");
    s.run_id = QStringLiteral("abcdef0123456789");
    s.temp_dir = QStringLiteral("/scratch/run");
    const LaunchPlan p = codex.build_command(s, inst);
#ifdef Q_OS_WIN
    // The read-only sandbox cannot be verified on Windows: no terminal-AI calls.
    QVERIFY(!codex.descriptor().caps.chat);
    QVERIFY(!p.error.isEmpty());
#else
    QVERIFY(codex.descriptor().caps.chat);
    QVERIFY2(p.error.isEmpty(), qPrintable(p.error));
    const QStringList expected = {"exec", "--json", "--ephemeral", "--ignore-user-config", "--skip-git-repo-check",
                                  "-s", "read-only", "-c", "approval_policy=\"never\"", "-c", "web_search=\"disabled\"",
                                  "-m", "gpt-6-luna", "-c", "model_reasoning_effort=\"low\"", "-"};
    QCOMPARE(p.args, expected);
    QVERIFY(p.stdin_payload.contains("SYS") && p.stdin_payload.contains("hello"));
    s.resume_session_id = QStringLiteral("0199a213-81c0-7800-8aa1-bbab2a035a53");
    QVERIFY(!codex.build_command(s, inst).error.isEmpty()); // never resumed
#endif
}

void TestHarnessChat::flatten_prompt_and_system_prompt() {
    const HarnessChatRequest r = thc_request(QStringLiteral("claude"));
    const QString prompt = flatten_chat_prompt(r.history, r.user_message);
    QVERIFY(prompt.contains(QStringLiteral("User: hello")));
    QVERIFY(prompt.contains(QStringLiteral("Assistant: hi there")));
    QVERIFY(prompt.endsWith(QStringLiteral("What is open?")));
    QVERIFY(!prompt.contains(QStringLiteral("Node instructions."))); // system turns go to the system prompt
    QCOMPARE(chat_system_prompt(r.system_prompt, r.history), QStringLiteral("You are Fincept AI.\n\nNode instructions."));
    QCOMPARE(flatten_chat_prompt({}, QStringLiteral("just this")), QStringLiteral("just this"));
}

void TestHarnessChat::claude_stream_maps_to_chat_channels() {
    ClaudeAdapter claude;
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/claude/chat_tool_answer.jsonl");
    fake.report = tmp_.filePath(QStringLiteral("claude_report.txt"));
    HarnessChatEnvironment env = thc_env(&claude, QStringLiteral("2.1.325"), scratch.path(), fake);
    McpEndpoint ep;
    ep.name = QStringLiteral("fincept");
    ep.url = QStringLiteral("http://127.0.0.1:41720/mcp");
    env.mcp = ep;
    HarnessChatRequest req = thc_request(QStringLiteral("claude"));
    req.tools = ChatTools::All;
    ThcRecord rec;
    const HarnessChatResult r = run_harness_chat(env, req, thc_sink(rec));
    QVERIFY2(r.success, qPrintable(r.error));
    QVERIFY(r.output_started);
    QCOMPARE(r.content, QStringLiteral("You have the Watchlist open."));
    QCOMPARE(rec.thinking, QStringLiteral("The user wants the open screens."));
    QCOMPARE(rec.text, QStringLiteral("You have the Watchlist open.")); // narration dropped at the tool call
    const QStringList expected = {QStringLiteral("think:The user wants "), QStringLiteral("think:the open screens."),
                                  QStringLiteral("text:Let me look "),     QStringLiteral("text:at the terminal."),
                                  QStringLiteral("reset"),                 QStringLiteral("call:toolu_01:ui_overview"),
                                  QStringLiteral("result:toolu_01:ui_overview:ok"),
                                  QStringLiteral("text:You have the "),    QStringLiteral("text:Watchlist open.")};
    QCOMPARE(rec.order, expected);
    QVERIFY(rec.events > rec.order.size());
    QCOMPARE(r.model, QStringLiteral("claude-sonnet-4-5"));
    QCOMPARE(r.input_tokens, qint64(1500));
    QCOMPARE(r.output_tokens, qint64(45));
    QVERIFY(r.cost_usd.has_value() && qFuzzyCompare(*r.cost_usd, 0.0123));
    QCOMPARE(r.session_id, QStringLiteral("7d1c0b2a-0000-4000-8000-0000000c4a71"));

    // What the child saw: a fresh folder under the scratch root (gone now), the
    // MCP config inside it, the token in the environment only, no subscription token.
    const QHash<QString, QString> rep = thc_read_report(fake.report);
    const QString cwd = QDir::fromNativeSeparators(rep.value(QStringLiteral("cwd")));
    QVERIFY2(cwd.startsWith(QDir::fromNativeSeparators(QDir(scratch.path()).canonicalPath())) ||
                 cwd.startsWith(QDir::fromNativeSeparators(scratch.path())),
             qPrintable(cwd));
    QVERIFY(!QFileInfo::exists(cwd));
    QVERIFY(QDir(scratch.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    QCOMPARE(rep.value(QStringLiteral("mcp")), QStringLiteral("1"));
    QCOMPARE(rep.value(QStringLiteral("oauth")), QStringLiteral("0"));
    const QStringList args = rep.value(QStringLiteral("args")).split(QChar(0x1f));
    QVERIFY(args.contains(QStringLiteral("--restricted")));
    QCOMPARE(args.at(args.indexOf(QStringLiteral("--tools")) + 1), QString());
    QCOMPARE(args.at(args.indexOf(QStringLiteral("--system-prompt")) + 1),
             QStringLiteral("You are Fincept AI.\\n\\nNode instructions."));
    const QString prompt = rep.value(QStringLiteral("prompt"));
    QVERIFY(prompt.contains(QStringLiteral("User: hello")) && prompt.contains(QStringLiteral("What is open?")));
}

void TestHarnessChat::codex_stream_maps_to_chat_channels() {
    ThcWrappedAdapter codex(std::make_unique<CodexAdapter>());
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/codex/file_change_turn.jsonl");
    const HarnessChatEnvironment env = thc_env(&codex, QStringLiteral("0.160.0"), scratch.path(), fake);
    ThcRecord rec;
    const HarnessChatResult r = run_harness_chat(env, thc_request(QStringLiteral("codex")), thc_sink(rec));
    QVERIFY2(r.success, qPrintable(r.error));
    QCOMPARE(r.content, QStringLiteral("Updated README.md and added NOTES.md."));
    QCOMPARE(rec.thinking, QStringLiteral("**Planning the edit**"));
    QVERIFY(rec.order.contains(QStringLiteral("call:item_2:quote_get")));
    QVERIFY(rec.order.contains(QStringLiteral("result:item_2:quote_get:ok")));
    QCOMPARE(rec.text, QStringLiteral("Updated README.md and added NOTES.md."));
    QCOMPARE(r.input_tokens, qint64(24763));
    QCOMPARE(r.output_tokens, qint64(122));
    QVERIFY(!r.cost_usd.has_value());
}

void TestHarnessChat::failure_before_output_falls_back() {
    ClaudeAdapter claude;
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/claude/rate_limit_rejected.jsonl");
    fake.exit_code = 1;
    const HarnessChatEnvironment env = thc_env(&claude, QStringLiteral("2.1.325"), scratch.path(), fake);
    ThcRecord rec;
    const HarnessChatResult r = run_harness_chat(env, thc_request(QStringLiteral("claude")), thc_sink(rec));
    QVERIFY(!r.success && !r.cancelled);
    QCOMPARE(r.failure, FailureKind::RateLimit);
    QVERIFY(!r.output_started);
    QVERIFY(rec.order.isEmpty());
    QVERIFY(r.retry_at_ms > 0);

    // The route answers with the next entry, once, and says so.
    using namespace fincept::ai_chat;
    QVector<ModelChoice> chain(2);
    chain[0].kind = ModelChoice::Kind::Harness;
    chain[0].harness_id = QStringLiteral("claude");
    chain[0].label = QStringLiteral("Claude Code · sonnet · high");
    chain[1].kind = ModelChoice::Kind::Fincept;
    chain[1].label = QStringLiteral("Fincept LLM");
    const FallbackDecision d = ModelRouter::decide_fallback(chain, 0, r.failure, r.output_started, r.cancelled,
                                                            r.subscription_detected, UnavailableBehaviour::Fallback);
    QVERIFY(d.fall_back);
    QCOMPARE(d.notice, QStringLiteral("Claude Code is rate limited — answered by Fincept LLM"));
}

void TestHarnessChat::failure_after_output_never_falls_back() {
    ClaudeAdapter claude;
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/claude/chat_error_after_output.jsonl");
    fake.exit_code = 1;
    const HarnessChatEnvironment env = thc_env(&claude, QStringLiteral("2.1.325"), scratch.path(), fake);
    ThcRecord rec;
    const HarnessChatResult r = run_harness_chat(env, thc_request(QStringLiteral("claude")), thc_sink(rec));
    QVERIFY(!r.success);
    QVERIFY(r.output_started);
    QCOMPARE(rec.text, QStringLiteral("The first half of an answer"));
    QCOMPARE(r.failure, FailureKind::RateLimit); // "overloaded"
    using namespace fincept::ai_chat;
    QVector<ModelChoice> chain(2);
    chain[0].kind = ModelChoice::Kind::Harness;
    chain[0].label = QStringLiteral("Claude Code");
    chain[1].kind = ModelChoice::Kind::Fincept;
    const FallbackDecision d = ModelRouter::decide_fallback(chain, 0, r.failure, r.output_started, r.cancelled,
                                                            r.subscription_detected, UnavailableBehaviour::Fallback);
    QVERIFY(!d.fall_back && !d.ask);
}

void TestHarnessChat::subscription_session_is_refused_before_output() {
    ClaudeAdapter claude;
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/claude/chat_subscription_session.jsonl");
    fake.delay_ms = 200;
    HarnessChatEnvironment env = thc_env(&claude, QStringLiteral("2.1.325"), scratch.path(), fake);
    env.line_guard = [](const QString& line) { return subscription_session_refusal(QStringLiteral("claude"), line); };
    ThcRecord rec;
    const HarnessChatResult r = run_harness_chat(env, thc_request(QStringLiteral("claude")), thc_sink(rec));
    QVERIFY(!r.success);
    QCOMPARE(r.failure, FailureKind::Auth);
    QVERIFY(r.subscription_detected);
    QVERIFY(!r.output_started);
    QVERIFY(rec.text.isEmpty() && rec.order.isEmpty());
    QVERIFY(r.content.isEmpty());
}

void TestHarnessChat::first_event_timeout() {
    ClaudeAdapter claude;
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/claude/chat_tool_answer.jsonl");
    fake.sleep_before_ms = 15000;
    const HarnessChatEnvironment env = thc_env(&claude, QStringLiteral("2.1.325"), scratch.path(), fake);
    HarnessChatRequest req = thc_request(QStringLiteral("claude"));
    req.first_event_timeout_ms = 1500;
    ThcRecord rec;
    QElapsedTimer t;
    t.start();
    const HarnessChatResult r = run_harness_chat(env, req, thc_sink(rec));
    QCOMPARE(r.failure, FailureKind::Timeout);
    QVERIFY(!r.output_started);
    QVERIFY2(t.elapsed() < 10000, qPrintable(QString::number(t.elapsed())));
}

void TestHarnessChat::total_timeout() {
    ClaudeAdapter claude;
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/claude/chat_tool_answer.jsonl");
    fake.sleep_after_first_ms = 15000; // init arrives, then nothing
    const HarnessChatEnvironment env = thc_env(&claude, QStringLiteral("2.1.325"), scratch.path(), fake);
    HarnessChatRequest req = thc_request(QStringLiteral("claude"));
    req.timeout_ms = 2000;
    ThcRecord rec;
    QElapsedTimer t;
    t.start();
    const HarnessChatResult r = run_harness_chat(env, req, thc_sink(rec));
    QCOMPARE(r.failure, FailureKind::Timeout);
    QVERIFY(r.error.contains(QStringLiteral("did not finish")));
    QVERIFY2(t.elapsed() < 10000, qPrintable(QString::number(t.elapsed())));
}

void TestHarnessChat::cancel_kills_the_process() {
    ClaudeAdapter claude;
    QTemporaryDir scratch;
    ThcFake fake;
    fake.fixture = data_ + QStringLiteral("/claude/chat_tool_answer.jsonl");
    fake.sleep_after_first_ms = 20000;
    fake.report = tmp_.filePath(QStringLiteral("cancel_report.txt"));
    const HarnessChatEnvironment env = thc_env(&claude, QStringLiteral("2.1.325"), scratch.path(), fake);
    ThcRecord rec;
    QAtomicInt cancel(0);
    HarnessChatSink sink = thc_sink(rec, [&cancel]() { return cancel.loadAcquire() != 0; });
    sink.event = [&rec, &cancel](const cad::AgentEvent&) {
        ++rec.events;
        cancel.storeRelease(1); // Stop as soon as the run is under way
    };
    QElapsedTimer t;
    t.start();
    const HarnessChatResult r = run_harness_chat(env, thc_request(QStringLiteral("claude")), sink);
    QVERIFY(r.cancelled);
    QVERIFY(!r.success);
    QVERIFY2(t.elapsed() < 10000, qPrintable(QString::number(t.elapsed())));
    const qint64 pid = thc_read_report(fake.report).value(QStringLiteral("pid")).toLongLong();
    QVERIFY(pid > 0);
    QVERIFY(!thc_process_alive(pid));
}

void TestHarnessChat::process_pool_limits_and_queues() {
    HarnessProcessPool pool(1);
    QVERIFY(pool.acquire({}, 100));
    QCOMPARE(pool.in_use(), 1);
    // Full: a second caller waits, then gives up at its deadline…
    QElapsedTimer t;
    t.start();
    QVERIFY(!pool.acquire({}, 300));
    QVERIFY(t.elapsed() >= 250);
    // …or when cancelled.
    t.restart();
    QVERIFY(!pool.acquire([]() { return true; }, 5000));
    QVERIFY(t.elapsed() < 1000);
    // A waiter gets the slot as soon as it frees.
    QAtomicInt got(0);
    QThread* waiter = QThread::create([&pool, &got]() {
        if (pool.acquire({}, 5000)) {
            got.storeRelease(1);
            pool.release();
        }
    });
    waiter->start();
    QTRY_COMPARE_WITH_TIMEOUT(pool.waiting(), 1, 2000);
    pool.release();
    QVERIFY(waiter->wait(5000));
    delete waiter;
    QCOMPARE(got.loadAcquire(), 1);
    QCOMPARE(pool.in_use(), 0);
    QCOMPARE(HarnessProcessPool::chat().capacity(), HarnessProcessPool::kChatProcesses);
}

int main(int argc, char** argv) {
    if (qEnvironmentVariableIsSet(kFakeVar)) {
        QCoreApplication app(argc, argv); // applicationPid() / arguments only; no event loop
        return thc_fake_harness_main(argc, argv);
    }
    QCoreApplication app(argc, argv);
    TestHarnessChat t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_harness_chat.moc"
