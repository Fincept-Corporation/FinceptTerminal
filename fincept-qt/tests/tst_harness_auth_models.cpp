// tst_harness_auth_models — sign-in state, model lists and reasoning efforts for
// every harness adapter: parse_auth / parse_models against output captured on a
// development machine (tests/data/harness/<id>/*.txt|json, see the .meta.json
// next to each: grok 1.0.41 `grok models` signed out, codex-cli 0.160.0 `codex
// debug models` and `codex login status`), built-in model lists and efforts, and
// the rule that auth_hint() looks at environment-variable NAMES and file
// EXISTENCE only — never a value, never a file's content.

#include "services/harness/adapters/ClaudeAdapter.h"
#include "services/harness/adapters/CodexAdapter.h"
#include "services/harness/adapters/CopilotAdapter.h"
#include "services/harness/adapters/CursorAdapter.h"
#include "services/harness/adapters/GeminiAdapter.h"
#include "services/harness/adapters/GrokAdapter.h"
#include "services/harness/adapters/QwenAdapter.h"

#include <QFile>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QtTest>

#include <memory>
#include <vector>

using namespace fincept::harness;

namespace {

QByteArray tam_fixture(const QString& rel) {
    QString root = qEnvironmentVariable("FINCEPT_HARNESS_FIXTURES");
    const QString path = root.isEmpty() ? QFINDTESTDATA(QStringLiteral("data/harness/") + rel) : root + QLatin1Char('/') + rel;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

std::vector<std::unique_ptr<IHarnessAdapter>> tam_all_adapters() {
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

QStringList tam_ids(const QVector<ModelInfo>& models) {
    QStringList ids;
    for (const ModelInfo& m : models)
        ids << m.id;
    return ids;
}

const QStringList& tam_five_efforts() {
    static const QStringList e = {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"),
                                  QStringLiteral("xhigh"), QStringLiteral("max")};
    return e;
}

} // namespace

class TestHarnessAuthModels : public QObject {
    Q_OBJECT

  private slots:
    // ── Grok Build ───────────────────────────────────────────────────────────
    void grok_signed_out_recorded();
    void grok_signed_in_and_errors();
    void grok_parse_models_is_strict();
    void grok_builtin_models_and_no_efforts();

    // ── Claude Code ──────────────────────────────────────────────────────────
    void claude_parse_auth_shapes();
    void claude_aliases_and_efforts();

    // ── Codex ────────────────────────────────────────────────────────────────
    void codex_login_status_recorded();
    void codex_debug_models_recorded();

    // ── Documentation-only adapters ──────────────────────────────────────────
    void gemini_api_key_only();
    void cursor_copilot_qwen_have_no_status_commands();
    void copilot_efforts_and_models();

    // ── Every adapter ────────────────────────────────────────────────────────
    void auth_hint_uses_names_and_existence_only();
    void descriptors_are_complete();
};

// ── Grok Build ───────────────────────────────────────────────────────────────

void TestHarnessAuthModels::grok_signed_out_recorded() {
    const QByteArray out = tam_fixture(QStringLiteral("grok/models_signed_out.txt"));
    QCOMPARE(out, QByteArray("You are not authenticated.\n\nDefault model: grok-4.6\n\nAvailable models:\n"
                             "  * grok-4.6 (default)\n  - grok-4.5\n")); // exact bytes, grok 1.0.41
    const GrokAdapter grok;
    QCOMPARE(grok.auth_command()->args, QStringList{QStringLiteral("models")});
    QCOMPARE(grok.models_command()->args, QStringList{QStringLiteral("models")});

    const AuthStatus a = grok.parse_auth(0, out, {}); // recorded exit code: 0
    QCOMPARE(a.state, AuthState::SignedOut);
    QVERIFY(a.method.isEmpty());
    QVERIFY(a.plan.isEmpty());

    // The list is printed even when signed out.
    const QVector<ModelInfo> models = grok.parse_models(0, out);
    QCOMPARE(tam_ids(models), (QStringList{QStringLiteral("grok-4.6"), QStringLiteral("grok-4.5")}));
    QVERIFY(models[0].is_default);
    QVERIFY(!models[1].is_default);
    QVERIFY(models[0].efforts.isEmpty());
}

void TestHarnessAuthModels::grok_signed_in_and_errors() {
    const GrokAdapter grok;
    // Signed in (derived from the recorded output without the sentence — not recorded).
    const QByteArray signed_in("Default model: grok-4.6\n\nAvailable models:\n  * grok-4.6 (default)\n  - grok-4.5\n");
    AuthStatus a = grok.parse_auth(0, signed_in, {});
    QCOMPARE(a.state, AuthState::SignedIn);
    QVERIFY(a.method.isEmpty()); // how it is signed in is not reported
    QCOMPARE(a.note, QStringLiteral("inferred from `grok models`"));

    a = grok.parse_auth(0, {}, "You are not authenticated.\n"); // on stderr
    QCOMPARE(a.state, AuthState::SignedOut);
    a = grok.parse_auth(2, {}, "error: leader socket unavailable\n");
    QCOMPARE(a.state, AuthState::Error);
    QVERIFY(a.note.contains(QLatin1String("2")));
    QVERIFY(!a.note.contains(QLatin1String("leader"))); // nothing of the output is kept
    a = grok.parse_auth(0, "something else entirely\n", {});
    QCOMPARE(a.state, AuthState::Unknown);
    a = grok.parse_auth(0, {}, {});
    QCOMPARE(a.state, AuthState::Unknown);
}

void TestHarnessAuthModels::grok_parse_models_is_strict() {
    const GrokAdapter grok;
    // CRLF, a default given only by the "Default model:" line, invalid and duplicate ids.
    const QByteArray out("Default model: grok-5\r\n\r\nAvailable models:\r\n  - grok-4.6\r\n  - grok-5\r\n"
                         "  - grok 5 with spaces\r\n  - -rf\r\n  - grok-4.6\r\n  - \"quoted\"\r\n");
    const QVector<ModelInfo> models = grok.parse_models(0, out);
    QCOMPARE(tam_ids(models), (QStringList{QStringLiteral("grok-4.6"), QStringLiteral("grok-5")}));
    QVERIFY(!models[0].is_default);
    QVERIFY(models[1].is_default);

    QVERIFY(grok.parse_models(1, tam_fixture(QStringLiteral("grok/models_signed_out.txt"))).isEmpty());
    QVERIFY(grok.parse_models(0, "  * grok-4.6 (default)\n").isEmpty()); // no "Available models:" header
    QVERIFY(grok.parse_models(0, "").isEmpty());
    QVERIFY(grok.parse_models(0, QByteArray(200000, '\n')).isEmpty());
}

void TestHarnessAuthModels::grok_builtin_models_and_no_efforts() {
    const GrokAdapter grok;
    const QVector<ModelInfo> builtin = grok.builtin_models(QStringLiteral("1.0.41"));
    QCOMPARE(tam_ids(builtin), (QStringList{QStringLiteral("grok-4.6"), QStringLiteral("grok-4.5")}));
    QVERIFY(builtin[0].is_default);
    // --reasoning-effort exists, its values are not listed: none offered.
    QVERIFY(!grok.descriptor().caps.effort);
    QVERIFY(grok.efforts(QStringLiteral("grok-4.6"), QStringLiteral("1.0.41")).isEmpty());
    QVERIFY(grok.descriptor().caps.model_list);
}

// ── Claude Code ──────────────────────────────────────────────────────────────

void TestHarnessAuthModels::claude_parse_auth_shapes() {
    // `claude auth status --json` — SHAPE NOT VERIFIED ON THIS MACHINE (running it
    // is not allowed here: its output carries identity fields). These documents
    // use only the four field names the adapter reads.
    const ClaudeAdapter claude;
    AuthStatus a = claude.parse_auth(
        0, R"({"loggedIn":true,"authMethod":"claude.ai","apiProvider":"firstParty","subscriptionType":"pro"})", {});
    QCOMPARE(a.state, AuthState::SignedIn);
    QCOMPARE(a.method, QStringLiteral("subscription"));
    QCOMPARE(a.plan, QStringLiteral("pro"));
    a = claude.parse_auth(1, R"({"loggedIn":false})", {});
    QCOMPARE(a.state, AuthState::SignedOut);
    a = claude.parse_auth(0, R"({"loggedIn":true,"authMethod":"api_key"})", {});
    QCOMPARE(a.method, QStringLiteral("api_key"));
    a = claude.parse_auth(0, R"({"loggedIn":true,"authMethod":"oauth_token"})", {});
    QCOMPARE(a.method, QStringLiteral("subscription"));
    a = claude.parse_auth(0, R"({"loggedIn":true,"apiProvider":"vertex"})", {});
    QCOMPARE(a.method, QStringLiteral("cloud"));
    QCOMPARE(a.note, QStringLiteral("vertex"));
    a = claude.parse_auth(0, R"({"loggedIn":"yes"})", {});
    QCOMPARE(a.state, AuthState::Unknown);
    a = claude.parse_auth(3, "", {});
    QCOMPARE(a.state, AuthState::Error);
    QVERIFY(!claude.models_command().has_value());
    QVERIFY(claude.parse_models(0, "anything").isEmpty());
}

void TestHarnessAuthModels::claude_aliases_and_efforts() {
    const ClaudeAdapter claude;
    const QStringList ids = tam_ids(claude.builtin_models(QStringLiteral("2.1.325")));
    for (const char* alias : {"opus", "sonnet", "haiku", "fable", "opusplan", "best"})
        QVERIFY2(ids.contains(QLatin1String(alias)), alias);
    QCOMPARE(claude.efforts(QStringLiteral("opus"), QStringLiteral("2.1.325")), tam_five_efforts());
    QCOMPARE(claude.efforts(QString(), QString()), tam_five_efforts());
}

// ── Codex ────────────────────────────────────────────────────────────────────

void TestHarnessAuthModels::codex_login_status_recorded() {
    const CodexAdapter codex;
    const QByteArray err = tam_fixture(QStringLiteral("codex/login_status_chatgpt.txt"));
    QCOMPARE(err, QByteArray("Logged in using ChatGPT\n")); // codex-cli 0.160.0, on stderr
    AuthStatus a = codex.parse_auth(0, {}, err);
    QCOMPARE(a.state, AuthState::SignedIn);
    QCOMPARE(a.method, QStringLiteral("subscription"));
    a = codex.parse_auth(1, {}, "Not logged in\n"); // signed-out text and exit code: not verified here
    QCOMPARE(a.state, AuthState::SignedOut);
    a = codex.parse_auth(0, {}, "Logged in using an API key - sk-***\n");
    QCOMPARE(a.method, QStringLiteral("api_key"));
    QVERIFY(!a.note.contains(QLatin1String("sk-")));
}

void TestHarnessAuthModels::codex_debug_models_recorded() {
    const CodexAdapter codex;
    const QByteArray out = tam_fixture(QStringLiteral("codex/debug_models.json"));
    QVERIFY(!out.isEmpty());
    const QVector<ModelInfo> models = codex.parse_models(0, out);
    // visibility "hide" (gpt-reserve, codex-auto-review) dropped; ordered by priority.
    QCOMPARE(tam_ids(models), (QStringList{QStringLiteral("gpt-6-luna"), QStringLiteral("gpt-5.6-terra"),
                                           QStringLiteral("gpt-5.6-luna")}));
    QCOMPARE(models[0].label, QStringLiteral("GPT-6-Luna"));
    QCOMPARE(models[0].default_effort, QStringLiteral("medium"));
    QCOMPARE(models[0].context_window, qint64(272000));
    QCOMPARE(models[0].efforts, tam_five_efforts());
    QCOMPARE(models[1].efforts, tam_five_efforts() + QStringList{QStringLiteral("ultra")});
    QVERIFY(!models[2].description.isEmpty());
    // The built-in fallback matches what was listed.
    QCOMPARE(tam_ids(codex.builtin_models(QStringLiteral("0.160.0"))), tam_ids(models));
    QVERIFY(codex.efforts(QStringLiteral("gpt-5.6-terra"), {}).contains(QStringLiteral("ultra")));
    QVERIFY(codex.parse_models(1, out).isEmpty());
}

// ── Documentation-only adapters ─────────────────────────────────────────────

void TestHarnessAuthModels::gemini_api_key_only() {
    const GeminiAdapter gemini;
    QVERIFY(gemini.descriptor().api_key_only);
    QCOMPARE(gemini.descriptor().api_key_env_vars, QStringList{QStringLiteral("GEMINI_API_KEY")});
    QVERIFY(!gemini.auth_command().has_value());
    QCOMPARE(gemini.parse_auth(0, "anything", "anything").state, AuthState::Unknown);
    QVERIFY(!gemini.models_command().has_value());
    QCOMPARE(tam_ids(gemini.builtin_models({})), (QStringList{QStringLiteral("auto"), QStringLiteral("pro"),
                                                              QStringLiteral("flash"), QStringLiteral("flash-lite")}));
    QVERIFY(gemini.efforts(QStringLiteral("pro"), {}).isEmpty());

    DetectContext ctx;
    ctx.windows = false;
    ctx.home = QStringLiteral("/home/me");
    AuthStatus a = gemini.auth_hint(ctx);
    QVERIFY(!a.api_key_env_present);
    QVERIFY(a.note.contains(QLatin1String("GEMINI_API_KEY")));
    QProcessEnvironment env;
    env.insert(QStringLiteral("GEMINI_API_KEY"), QStringLiteral("AIza-SECRET-VALUE"));
    ctx.env = env;
    a = gemini.auth_hint(ctx);
    QVERIFY(a.api_key_env_present);
    QCOMPARE(a.method, QStringLiteral("api_key"));
    QCOMPARE(a.state, AuthState::Unknown); // a hint is never a sign-in verdict
}

void TestHarnessAuthModels::cursor_copilot_qwen_have_no_status_commands() {
    const CursorAdapter cursor;
    const CopilotAdapter copilot;
    const QwenAdapter qwen;
    for (const IHarnessAdapter* a : {static_cast<const IHarnessAdapter*>(&cursor),
                                     static_cast<const IHarnessAdapter*>(&copilot),
                                     static_cast<const IHarnessAdapter*>(&qwen)}) {
        // Output shapes of `agent status`, `agent models`, … are not documented: not used.
        QVERIFY2(!a->auth_command().has_value(), qPrintable(a->descriptor().id));
        QVERIFY2(!a->models_command().has_value(), qPrintable(a->descriptor().id));
        QCOMPARE(a->parse_auth(0, "Logged in", {}).state, AuthState::Unknown);
        QVERIFY(a->parse_models(0, "model-a\nmodel-b\n").isEmpty());
        QVERIFY(!a->descriptor().caps.model_list);
    }
    QVERIFY(cursor.builtin_models({}).isEmpty());
    QVERIFY(cursor.efforts(QStringLiteral("x"), {}).isEmpty());
    QVERIFY(qwen.builtin_models({}).isEmpty());
    QVERIFY(qwen.efforts(QStringLiteral("x"), {}).isEmpty());
}

void TestHarnessAuthModels::copilot_efforts_and_models() {
    const CopilotAdapter copilot;
    QVERIFY(copilot.descriptor().caps.effort);
    QCOMPARE(copilot.efforts(QStringLiteral("auto"), {}), tam_five_efforts());
    const QVector<ModelInfo> models = copilot.builtin_models({});
    QCOMPARE(tam_ids(models), QStringList{QStringLiteral("auto")});
    QCOMPARE(models[0].efforts, tam_five_efforts());
    QCOMPARE(copilot.descriptor().transport, Transport::PlainText);
}

// ── Every adapter ────────────────────────────────────────────────────────────

void TestHarnessAuthModels::auth_hint_uses_names_and_existence_only() {
    const QString secret = QStringLiteral("SECRET-VALUE-7f3a9c");
    QProcessEnvironment env;
    for (const char* var : {"ANTHROPIC_API_KEY", "ANTHROPIC_AUTH_TOKEN", "CLAUDE_CODE_OAUTH_TOKEN", "OPENAI_API_KEY",
                            "CODEX_API_KEY", "GEMINI_API_KEY", "CURSOR_API_KEY", "COPILOT_GITHUB_TOKEN", "GH_TOKEN",
                            "GITHUB_TOKEN", "XAI_API_KEY", "DASHSCOPE_API_KEY"})
        env.insert(QString::fromLatin1(var), secret);
    bool read_called = false;
    DetectContext ctx;
    ctx.windows = false;
    ctx.home = QStringLiteral("/home/me");
    ctx.env = env;
    ctx.is_file = [](const QString&) { return true; };
    ctx.read_file = [&read_called](const QString&, qint64) {
        read_called = true;
        return QByteArray("{\"token\":\"SECRET-VALUE-7f3a9c\"}");
    };
    for (const auto& adapter : tam_all_adapters()) {
        const AuthStatus a = adapter->auth_hint(ctx);
        const QString id = adapter->descriptor().id;
        for (const QString& field : {a.method, a.plan, a.note})
            QVERIFY2(!field.contains(secret), qPrintable(id));
        QCOMPARE(a.state, AuthState::Unknown);
        // Every variable the descriptor names as an API key is detected by name.
        if (!adapter->descriptor().api_key_env_vars.isEmpty())
            QVERIFY2(a.api_key_env_present, qPrintable(id));
    }
    QVERIFY(!read_called); // no file is ever opened
}

void TestHarnessAuthModels::descriptors_are_complete() {
    QSet<QString> ids;
    for (const auto& adapter : tam_all_adapters()) {
        const HarnessDescriptor& d = adapter->descriptor();
        QVERIFY2(!ids.contains(d.id), qPrintable(d.id));
        ids.insert(d.id);
        QVERIFY(!d.display_name.isEmpty());
        QVERIFY(!d.vendor.isEmpty());
        QVERIFY(d.install_url.startsWith(QLatin1String("https://")));
        QVERIFY(!d.signin_hint.isEmpty());
        QVERIFY(!d.executable_names.isEmpty());
        QVERIFY(!d.run_mcp_server_name.isEmpty());
        QCOMPARE(adapter->version_command().args, QStringList{QStringLiteral("--version")});
        const bool verified = d.id == QLatin1String("claude") || d.id == QLatin1String("codex");
        QCOMPARE(d.beta, !verified);
        QCOMPARE(d.beta_note.isEmpty(), verified);
        QCOMPARE(d.caps.full_access, verified); // only Claude Code and Codex launch full access
        if (verified)
            QVERIFY(!d.tested_version.isEmpty());
        else
            QVERIFY(d.tested_version.isEmpty()); // nothing recorded yet
        // Documented model ids validate; efforts are lower-case words.
        for (const ModelInfo& m : adapter->builtin_models({})) {
            QVERIFY(!m.id.isEmpty());
            for (const QString& e : m.efforts)
                QVERIFY(QRegularExpression(QStringLiteral("^[a-z]{1,16}$")).match(e).hasMatch());
        }
    }
    QCOMPARE(ids.size(), 7);
    QCOMPARE(ClaudeAdapter().descriptor().tested_version, QStringLiteral("2.1.325"));
    QCOMPARE(CodexAdapter().descriptor().tested_version, QStringLiteral("0.160.0"));
    QCOMPARE(GrokAdapter().descriptor().beta_note,
             QStringLiteral("Written from grok --help; not yet checked against a recorded run"));
}

QTEST_GUILESS_MAIN(TestHarnessAuthModels)
#include "tst_harness_auth_models.moc"
