// tst_harness_parsers — every harness stream parser (Claude Code / Qwen Code
// stream-json, Codex exec --json, ACP session updates for Grok Build and any ACP
// agent, Gemini CLI stream-json, Cursor stream-json, plain text for Copilot CLI)
// against the transcripts in tests/data/harness/<id>/<scenario>.* fed byte-wise
// through LineBuffer exactly as the service does: event kinds in order with
// partial flags, ToolCall/ToolResult pairing by ref_id, exactly one complete
// event per streamed message, session id, usage, final text, result flags,
// FileChange extraction and failure classification. Plus the defensive
// contract: malformed / truncated / unknown lines become status events, an
// unterminated last line is parsed, a 1 MB line is handled, nothing throws.
//
// Every fixture has a .meta.json saying where it came from; the transcripts are
// TRANSCRIBED from documentation ("source": "docs"), not recordings.

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/adapters/ClaudeAdapter.h"
#include "services/harness/adapters/CodexAdapter.h"
#include "services/harness/adapters/CopilotAdapter.h"
#include "services/harness/adapters/CursorAdapter.h"
#include "services/harness/adapters/GeminiAdapter.h"
#include "services/harness/adapters/GrokAdapter.h"
#include "services/harness/adapters/QwenAdapter.h"
#include "services/harness/parsers/AcpUpdateParser.h"
#include "services/harness/parsers/CursorStreamParser.h"
#include "services/harness/parsers/GeminiStreamParser.h"
#include "services/harness/parsers/PlainTextParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtTest>

#include <memory>

using namespace fincept::harness;
namespace cad = fincept::services::coding_agents;

namespace {

QString thp_data_root() {
    QString root = qEnvironmentVariable("FINCEPT_HARNESS_FIXTURES");
    if (root.isEmpty())
        root = QFINDTESTDATA("data/harness");
    return root;
}

std::unique_ptr<IHarnessAdapter> thp_adapter(const QString& id) {
    if (id == QLatin1String("claude"))
        return std::make_unique<ClaudeAdapter>();
    if (id == QLatin1String("codex"))
        return std::make_unique<CodexAdapter>();
    if (id == QLatin1String("grok"))
        return std::make_unique<GrokAdapter>();
    if (id == QLatin1String("gemini"))
        return std::make_unique<GeminiAdapter>();
    if (id == QLatin1String("cursor"))
        return std::make_unique<CursorAdapter>();
    if (id == QLatin1String("copilot"))
        return std::make_unique<CopilotAdapter>();
    if (id == QLatin1String("qwen"))
        return std::make_unique<QwenAdapter>();
    return nullptr;
}

/// The parser a fixture names: an adapter's own parser, or "acp" for the generic one.
std::unique_ptr<IStreamParser> thp_parser(const QString& id) {
    if (id == QLatin1String("acp"))
        return std::make_unique<AcpUpdateParser>();
    if (auto a = thp_adapter(id))
        return a->make_parser();
    return nullptr;
}

/// Feed `bytes` through a LineBuffer in `chunk`-byte reads, then the
/// unterminated rest, then finish() — what the service does with stdout.
QVector<cad::AgentEvent> thp_feed(IStreamParser& parser, const QByteArray& bytes, int chunk) {
    QVector<cad::AgentEvent> out;
    cad::LineBuffer lb;
    for (qsizetype i = 0; i < bytes.size(); i += chunk) {
        for (const QByteArray& line : lb.feed(bytes.mid(i, chunk)))
            out += parser.parse_line(QString::fromUtf8(line));
    }
    const QByteArray rest = lb.take_rest();
    if (!rest.isEmpty())
        out += parser.parse_line(QString::fromUtf8(rest));
    out += parser.finish();
    return out;
}

QVector<cad::AgentEvent> thp_lines(IStreamParser& parser, const QStringList& lines) {
    QByteArray bytes;
    for (const QString& l : lines)
        bytes += l.toUtf8() + '\n';
    return thp_feed(parser, bytes, 64 * 1024);
}

/// "kind[~][@ref][!]" — ~ partial, ! is_error on a non-error kind.
QString thp_sig(const cad::AgentEvent& e) {
    QString s = cad::event_kind_id(e.kind);
    if (e.partial)
        s += QLatin1Char('~');
    if (!e.ref_id.isEmpty())
        s += QLatin1Char('@') + e.ref_id;
    if (e.is_error && e.kind != cad::EventKind::Error)
        s += QLatin1Char('!');
    return s;
}

QString thp_sig(const QJsonObject& e) {
    QString s = e.value(QStringLiteral("kind")).toString();
    if (e.value(QStringLiteral("partial")).toBool(false))
        s += QLatin1Char('~');
    const QString ref = e.value(QStringLiteral("ref")).toString();
    if (!ref.isEmpty())
        s += QLatin1Char('@') + ref;
    if (e.value(QStringLiteral("error")).toBool(false) && s != QLatin1String("error"))
        s += QLatin1Char('!');
    return s;
}

QStringList thp_sigs(const QVector<cad::AgentEvent>& events) {
    QStringList out;
    for (const cad::AgentEvent& e : events)
        out << thp_sig(e);
    return out;
}

/// The contract every stream must keep, whatever the harness.
void thp_check_invariants(const QVector<cad::AgentEvent>& events) {
    QSet<QString> calls;
    QHash<QString, int> completes; // kind|ref -> complete events
    QHash<QString, int> last_partial_at;
    for (int i = 0; i < events.size(); ++i) {
        const cad::AgentEvent& e = events[i];
        QVERIFY2(e.body.toUtf8().size() <= cad::kAgentEventBodyMaxBytes + 64, qPrintable(e.title));
        if (e.kind == cad::EventKind::ToolCall && !e.ref_id.isEmpty())
            calls.insert(e.ref_id);
        if (e.kind == cad::EventKind::ToolResult && !e.ref_id.isEmpty())
            QVERIFY2(calls.contains(e.ref_id), qPrintable(QStringLiteral("result without call: ") + e.ref_id));
        const QString key = cad::event_kind_id(e.kind) + QLatin1Char('|') + e.ref_id;
        if (e.partial) {
            QVERIFY2(e.kind == cad::EventKind::Text || e.kind == cad::EventKind::Thinking, qPrintable(key));
            QVERIFY2(!e.ref_id.isEmpty(), "a partial event names its message");
            QVERIFY2(completes.value(key) == 0, qPrintable(QStringLiteral("partial after its complete event: ") + key));
            last_partial_at.insert(key, i);
        } else if (!e.ref_id.isEmpty()) {
            completes[key] += 1;
        }
    }
    for (auto it = last_partial_at.cbegin(); it != last_partial_at.cend(); ++it)
        QVERIFY2(completes.value(it.key()) == 1,
                 qPrintable(QStringLiteral("streamed message %1 has %2 complete events").arg(it.key()).arg(completes.value(it.key()))));
}

int thp_count(const QVector<cad::AgentEvent>& events, cad::EventKind kind, bool partial = false) {
    int n = 0;
    for (const cad::AgentEvent& e : events)
        n += (e.kind == kind && e.partial == partial) ? 1 : 0;
    return n;
}

const cad::AgentEvent* thp_last(const QVector<cad::AgentEvent>& events, cad::EventKind kind, bool partial = false) {
    for (int i = static_cast<int>(events.size()) - 1; i >= 0; --i) {
        if (events[i].kind == kind && events[i].partial == partial)
            return &events[i];
    }
    return nullptr;
}

const QStringList& thp_all_parser_ids() {
    static const QStringList ids = {QStringLiteral("claude"), QStringLiteral("codex"),  QStringLiteral("acp"),
                                    QStringLiteral("gemini"), QStringLiteral("cursor"), QStringLiteral("copilot"),
                                    QStringLiteral("qwen"),   QStringLiteral("grok")};
    return ids;
}

} // namespace

class TestHarnessParsers : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    // ── Fixtures ─────────────────────────────────────────────────────────────
    void fixtures_data();
    void fixtures();

    // ── Defensive contract ───────────────────────────────────────────────────
    void malformed_lines_become_events_data();
    void malformed_lines_become_events();
    void unknown_event_types_are_status();
    void unterminated_last_line_is_parsed();
    void one_megabyte_line_data();
    void one_megabyte_line();

    // ── Streaming ────────────────────────────────────────────────────────────
    void claude_deltas_one_complete_event();
    void claude_deltas_flushed_without_assistant_message();
    void claude_without_partials_keeps_old_shape();
    void long_streamed_message_stops_partials();
    void acp_wrappers_are_equivalent();
    void acp_update_for_unknown_tool_is_announced();
    void gemini_whole_message_replaces_streamed_one();

    // ── Facts ────────────────────────────────────────────────────────────────
    void file_change_extraction();
    void claude_rate_limit_reset_formats();
    void codex_cached_tokens_accumulate();
};

void TestHarnessParsers::initTestCase() {
    const QString root = thp_data_root();
    QVERIFY2(!root.isEmpty() && QDir(root).exists(),
             "tests/data/harness not found (set FINCEPT_HARNESS_FIXTURES to the folder)");
}

// ── Fixtures ─────────────────────────────────────────────────────────────────

void TestHarnessParsers::fixtures_data() {
    QTest::addColumn<QString>("expected_path");
    const QDir root(thp_data_root());
    QStringList found;
    QDirIterator it(root.absolutePath(), {QStringLiteral("*.expected.json")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        found << it.next();
    found.sort();
    QVERIFY2(found.size() >= 12, "fewer fixtures than expected");
    for (const QString& p : std::as_const(found))
        QTest::newRow(qPrintable(root.relativeFilePath(p))) << p;
}

void TestHarnessParsers::fixtures() {
    QFETCH(QString, expected_path);
    QFile ef(expected_path);
    QVERIFY(ef.open(QIODevice::ReadOnly));
    const QJsonObject exp = QJsonDocument::fromJson(ef.readAll()).object();
    QVERIFY2(!exp.isEmpty(), "expected.json is not a JSON object");
    const QString dir = QFileInfo(expected_path).absolutePath();
    const QString scenario = QFileInfo(expected_path).fileName().chopped(int(qstrlen(".expected.json")));

    // Provenance is recorded next to every transcript.
    QFile mf(dir + QLatin1Char('/') + scenario + QStringLiteral(".meta.json"));
    QVERIFY2(mf.open(QIODevice::ReadOnly), "missing .meta.json");
    const QJsonObject meta = QJsonDocument::fromJson(mf.readAll()).object();
    QVERIFY(meta.contains(QStringLiteral("source")) && meta.contains(QStringLiteral("command")) &&
            meta.contains(QStringLiteral("date")) && meta.contains(QStringLiteral("harness_version")));
    QVERIFY(meta.value(QStringLiteral("source")).toString() == QLatin1String("docs") ||
            meta.value(QStringLiteral("source")).toString() == QLatin1String("recorded"));

    QFile in(dir + QLatin1Char('/') + exp.value(QStringLiteral("input")).toString());
    QVERIFY2(in.open(QIODevice::ReadOnly), qPrintable(in.fileName()));
    const QByteArray bytes = in.readAll();
    const QString parser_id = exp.value(QStringLiteral("parser")).toString();

    // Byte-wise (7-byte reads split lines and UTF-8 sequences) and whole-file
    // feeding must give the same events.
    auto parser = thp_parser(parser_id);
    QVERIFY2(parser, qPrintable(parser_id));
    const QVector<cad::AgentEvent> events = thp_feed(*parser, bytes, 7);
    auto parser2 = thp_parser(parser_id);
    QCOMPARE(thp_sigs(thp_feed(*parser2, bytes, 1 << 20)), thp_sigs(events));

    QStringList want;
    for (const QJsonValue& v : exp.value(QStringLiteral("events")).toArray())
        want << thp_sig(v.toObject());
    QCOMPARE(thp_sigs(events), want);
    thp_check_invariants(events);

    const cad::StreamFacts& f = parser->facts();
    auto check_str = [&](const char* key, const QString& actual) {
        if (exp.contains(QLatin1String(key)))
            QCOMPARE(actual, exp.value(QLatin1String(key)).toString());
    };
    auto check_int = [&](const char* key, qint64 actual) {
        if (exp.contains(QLatin1String(key)))
            QCOMPARE(actual, static_cast<qint64>(exp.value(QLatin1String(key)).toDouble()));
    };
    auto check_bool = [&](const char* key, bool actual) {
        if (exp.contains(QLatin1String(key)))
            QCOMPARE(actual, exp.value(QLatin1String(key)).toBool());
    };
    check_str("session_id", f.session_id);
    check_str("final_text", f.final_text);
    check_str("stop_reason", f.stop_reason);
    check_str("rate_limit_status", f.rate_limit_status);
    check_int("input_tokens", f.input_tokens);
    check_int("output_tokens", f.output_tokens);
    check_int("cached_tokens", f.cached_tokens);
    check_int("tool_calls", f.tool_calls);
    check_int("rate_limit_resets_at_ms", f.rate_limit_resets_at_ms);
    check_bool("saw_result", f.saw_result);
    check_bool("result_is_error", f.result_is_error);
    if (exp.contains(QStringLiteral("cost_usd"))) {
        QVERIFY(f.cost_usd.has_value());
        QVERIFY(qFuzzyCompare(*f.cost_usd, exp.value(QStringLiteral("cost_usd")).toDouble()));
    }
    if (exp.contains(QStringLiteral("file_changes"))) {
        QStringList got;
        for (const cad::AgentEvent& e : events) {
            if (e.kind != cad::EventKind::FileChange || e.is_error)
                continue;
            for (const cad::FileChangeEntry& fc : cad::parse_file_change_body(e.body))
                got << fc.kind + QLatin1Char(' ') + fc.path;
        }
        QStringList wanted;
        for (const QJsonValue& v : exp.value(QStringLiteral("file_changes")).toArray())
            wanted << v.toArray().at(0).toString() + QLatin1Char(' ') + v.toArray().at(1).toString();
        QCOMPARE(got, wanted);
    }
    if (exp.contains(QStringLiteral("failure"))) {
        const auto adapter = thp_adapter(parser_id);
        QVERIFY(adapter);
        const QJsonObject fo = exp.value(QStringLiteral("failure")).toObject();
        QStringList stderr_tail;
        for (const QJsonValue& v : fo.value(QStringLiteral("stderr")).toArray())
            stderr_tail << v.toString();
        QCOMPARE(failure_id(adapter->classify_failure(fo.value(QStringLiteral("exit_code")).toInt(), f, stderr_tail)),
                 fo.value(QStringLiteral("kind")).toString());
    }
}

// ── Defensive contract ───────────────────────────────────────────────────────

void TestHarnessParsers::malformed_lines_become_events_data() {
    QTest::addColumn<QString>("parser_id");
    for (const QString& id : thp_all_parser_ids())
        QTest::newRow(qPrintable(id)) << id;
}

void TestHarnessParsers::malformed_lines_become_events() {
    QFETCH(QString, parser_id);
    const QStringList bad = {
        QStringLiteral("{not json"),
        QStringLiteral("[1,2,3]"),
        QStringLiteral("\"just a string\""),
        QStringLiteral("{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"text\",\"text\":\"cut"), // truncated
        QStringLiteral("Update available: run the installer"),
    };
    for (const QString& line : bad) {
        auto p = thp_parser(parser_id);
        const QVector<cad::AgentEvent> ev = thp_lines(*p, {line});
        QVERIFY2(!ev.isEmpty(), qPrintable(parser_id + QStringLiteral(": dropped ") + line));
    }
    // A line that reads like an error is flagged and remembered.
    auto p = thp_parser(parser_id);
    const QVector<cad::AgentEvent> ev = thp_lines(*p, {QStringLiteral("Error: invalid api key provided")});
    QVERIFY(!ev.isEmpty());
    QVERIFY(p->facts().last_error.contains(QLatin1String("invalid api key")));
    if (parser_id != QLatin1String("copilot")) // plain text: the line is part of the answer
        QCOMPARE(ev.first().kind, cad::EventKind::Error);
    // Empty and whitespace lines show nothing and break nothing.
    auto q = thp_parser(parser_id);
    QVERIFY(thp_lines(*q, {QString(), QStringLiteral("   "), QStringLiteral("\t")}).isEmpty());
}

void TestHarnessParsers::unknown_event_types_are_status() {
    const struct {
        const char* parser;
        const char* line;
    } cases[] = {
        {"claude", R"({"type":"brand_new_event","x":1})"},
        {"claude", R"({"type":"system","subtype":"brand_new_subtype"})"},
        {"codex", R"({"type":"brand.new","x":1})"},
        {"acp", R"({"sessionUpdate":"brand_new_update","x":1})"},
        {"acp", R"({"something":"else"})"},
        {"acp", R"({"jsonrpc":"2.0","method":"fs/read_text_file","id":1,"params":{"path":"/x"}})"},
        {"gemini", R"({"type":"brand_new","timestamp":"t"})"},
        {"cursor", R"({"type":"brand_new"})"},
        {"cursor", R"({"type":"system","subtype":"brand_new"})"},
        {"qwen", R"({"type":"brand_new"})"},
    };
    for (const auto& c : cases) {
        auto p = thp_parser(QString::fromLatin1(c.parser));
        const QVector<cad::AgentEvent> ev = thp_lines(*p, {QString::fromLatin1(c.line)});
        QVERIFY2(ev.size() == 1, c.line);
        QCOMPARE(ev[0].kind, cad::EventKind::Status);
        QVERIFY(!ev[0].is_error);
    }
}

void TestHarnessParsers::unterminated_last_line_is_parsed() {
    // The process exits after writing its last line without "\n".
    const QByteArray claude = R"({"type":"result","subtype":"success","is_error":false,"result":"ok","session_id":"s1"})";
    auto p = thp_parser(QStringLiteral("claude"));
    thp_feed(*p, claude, 5);
    QVERIFY(p->facts().saw_result);
    QCOMPARE(p->facts().final_text, QStringLiteral("ok"));

    const QByteArray acp = "{\"sessionUpdate\":\"agent_message_chunk\",\"content\":{\"type\":\"text\",\"text\":\"a\"}}\n"
                           "{\"stopReason\":\"end_turn\"}\r";
    auto a = thp_parser(QStringLiteral("acp"));
    thp_feed(*a, acp, 3);
    QVERIFY(a->facts().saw_result);
    QCOMPARE(a->facts().final_text, QStringLiteral("a"));

    const QByteArray cursor = R"({"type":"result","subtype":"success","is_error":false,"result":"done","session_id":"c"})";
    auto c = thp_parser(QStringLiteral("cursor"));
    thp_feed(*c, cursor, 11);
    QVERIFY(c->facts().saw_result);
}

void TestHarnessParsers::one_megabyte_line_data() {
    QTest::addColumn<QString>("parser_id");
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("final_len"); // facts().final_text length; -1 = not checked
    const int mb = 1024 * 1024;
    const QString big = QString(mb, QLatin1Char('x'));
    QTest::newRow("claude/assistant")
        << "claude"
        << QStringLiteral(R"({"type":"assistant","message":{"id":"m","content":[{"type":"text","text":"%1"}]}})").arg(big)
        << -1; // Claude's final text comes from the result line
    QTest::newRow("codex/agent_message")
        << "codex"
        << QStringLiteral(R"({"type":"item.completed","item":{"id":"i","type":"agent_message","text":"%1"}})").arg(big)
        << mb;
    QTest::newRow("acp/chunk")
        << "acp"
        << QStringLiteral(R"({"sessionUpdate":"agent_message_chunk","content":{"type":"text","text":"%1"}})").arg(big)
        << mb;
    QTest::newRow("gemini/delta")
        << "gemini" << QStringLiteral(R"({"type":"message","role":"assistant","content":"%1","delta":true})").arg(big)
        << mb;
    QTest::newRow("cursor/assistant")
        << "cursor"
        << QStringLiteral(R"({"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":"%1"}]}})")
               .arg(big)
        << mb;
    QTest::newRow("copilot/plain") << "copilot" << big << mb;
    QTest::newRow("claude/not-json") << "claude" << big << 0;
    QTest::newRow("acp/not-json") << "acp" << QStringLiteral("{") + big << 0;
    QTest::newRow("gemini/not-json") << "gemini" << big << 0;
}

void TestHarnessParsers::one_megabyte_line() {
    QFETCH(QString, parser_id);
    QFETCH(QString, line);
    QFETCH(int, final_len);
    QVERIFY(line.size() >= 1024 * 1024);
    auto p = thp_parser(parser_id);
    const QVector<cad::AgentEvent> ev = thp_feed(*p, line.toUtf8() + '\n', 64 * 1024);
    QVERIFY(!ev.isEmpty());
    for (const cad::AgentEvent& e : ev) {
        QVERIFY(e.body.toUtf8().size() <= cad::kAgentEventBodyMaxBytes + 64);
        QVERIFY(e.raw_json.toUtf8().size() <= cad::kAgentEventRawMaxBytes + 64);
    }
    thp_check_invariants(ev);
    if (final_len >= 0) // the answer itself is kept whole in the facts
        QCOMPARE(p->facts().final_text.size(), qsizetype(final_len));
}

// ── Streaming ────────────────────────────────────────────────────────────────

void TestHarnessParsers::claude_deltas_one_complete_event() {
    QStringList lines;
    lines << QStringLiteral(R"({"type":"stream_event","event":{"type":"message_start","message":{"id":"msg_X"}}})");
    lines << QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}})");
    QString full;
    for (int i = 0; i < 300; ++i) {
        const QString piece = QStringLiteral("w%1 ").arg(i);
        full += piece;
        lines << QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"%1"}}})")
                     .arg(piece);
    }
    lines << QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_stop","index":0}})");
    lines << QStringLiteral(R"({"type":"assistant","message":{"id":"msg_X","content":[{"type":"text","text":"%1"}]}})").arg(full);
    lines << QStringLiteral(R"({"type":"stream_event","event":{"type":"message_stop"}})");
    auto p = thp_parser(QStringLiteral("claude"));
    const QVector<cad::AgentEvent> ev = thp_lines(*p, lines);
    QCOMPARE(thp_count(ev, cad::EventKind::Text, true), 300);
    QCOMPARE(thp_count(ev, cad::EventKind::Text, false), 1);
    const cad::AgentEvent* last_partial = thp_last(ev, cad::EventKind::Text, true);
    QCOMPARE(last_partial->body, full);
    QCOMPARE(last_partial->ref_id, QStringLiteral("msg_X:0"));
    QCOMPARE(thp_last(ev, cad::EventKind::Text)->ref_id, QStringLiteral("msg_X:0"));
    QCOMPARE(thp_last(ev, cad::EventKind::Text)->body, full);
    thp_check_invariants(ev);
    // finish() after a completed message adds nothing.
    QVERIFY(p->finish().isEmpty());
}

void TestHarnessParsers::claude_deltas_flushed_without_assistant_message() {
    auto p = thp_parser(QStringLiteral("claude"));
    QVector<cad::AgentEvent> ev = thp_lines(
        *p, {QStringLiteral(R"({"type":"stream_event","event":{"type":"message_start","message":{"id":"m9"}}})"),
             QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"hmm"}}})"),
             QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":"cut off"}}})")});
    // thp_lines already called finish(): the process died mid-message.
    QCOMPARE(thp_sigs(ev), (QStringList{"thinking~@m9:0", "text~@m9:1", "thinking@m9:0", "text@m9:1"}));
    thp_check_invariants(ev);

    // A new message_start completes the previous message's unfinished blocks.
    auto q = thp_parser(QStringLiteral("claude"));
    ev = thp_lines(
        *q, {QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"one"}}})"),
             QStringLiteral(R"({"type":"stream_event","event":{"type":"message_start","message":{}}})"),
             QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"two"}}})"),
             QStringLiteral(R"({"type":"assistant","message":{"content":[{"type":"text","text":"two"}]}})")});
    QCOMPARE(thp_sigs(ev), (QStringList{"text~@msg-1:0", "text@msg-1:0", "text~@msg-2:0", "text@msg-2:0"}));
}

void TestHarnessParsers::claude_without_partials_keeps_old_shape() {
    // No --include-partial-messages: complete events carry no ref_id, exactly as before.
    auto p = thp_parser(QStringLiteral("claude"));
    const QVector<cad::AgentEvent> ev = thp_lines(
        *p, {QStringLiteral(R"({"type":"assistant","message":{"id":"m","content":[{"type":"thinking","thinking":"t"},{"type":"text","text":"hi"}]}})"),
             QStringLiteral(R"({"type":"keep_alive"})")});
    QCOMPARE(thp_sigs(ev), (QStringList{"thinking", "text"}));
    // Sub-agent messages never consume the main session's streamed blocks.
    auto q = thp_parser(QStringLiteral("claude"));
    const QVector<cad::AgentEvent> ev2 = thp_lines(
        *q, {QStringLiteral(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"main"}}})"),
             QStringLiteral(R"({"type":"assistant","parent_tool_use_id":"toolu_1","message":{"content":[{"type":"text","text":"sub"}]}})"),
             QStringLiteral(R"({"type":"assistant","message":{"content":[{"type":"text","text":"main"}]}})")});
    QCOMPARE(thp_sigs(ev2), (QStringList{"text~@msg-1:0", "text", "text@msg-1:0"}));
}

void TestHarnessParsers::long_streamed_message_stops_partials() {
    // 2,000 chunks of 100 characters: partials stop once one body is full, the
    // complete event is clipped, the final text is whole.
    const QString chunk = QString(99, QLatin1Char('a')) + QLatin1Char(' ');
    QStringList lines;
    for (int i = 0; i < 2000; ++i)
        lines << QStringLiteral(R"({"sessionUpdate":"agent_message_chunk","content":{"type":"text","text":"%1"}})").arg(chunk);
    auto p = thp_parser(QStringLiteral("acp"));
    const QVector<cad::AgentEvent> ev = thp_lines(*p, lines);
    const int partials = thp_count(ev, cad::EventKind::Text, true);
    QVERIFY(partials > 100 && partials < 2000);
    QCOMPARE(thp_count(ev, cad::EventKind::Text, false), 1);
    QCOMPARE(p->facts().final_text.size(), qsizetype(2000 * 100));
    QVERIFY(thp_last(ev, cad::EventKind::Text)->body.toUtf8().size() <= cad::kAgentEventBodyMaxBytes + 64);
    thp_check_invariants(ev);

    // Same for Gemini deltas and plain text.
    QStringList glines;
    for (int i = 0; i < 2000; ++i)
        glines << QStringLiteral(R"({"type":"message","role":"assistant","content":"%1","delta":true})").arg(chunk);
    auto g = thp_parser(QStringLiteral("gemini"));
    const QVector<cad::AgentEvent> gev = thp_lines(*g, glines);
    QCOMPARE(thp_count(gev, cad::EventKind::Text, false), 1);
    QVERIFY(thp_count(gev, cad::EventKind::Text, true) < 2000);
    QCOMPARE(g->facts().final_text.size(), qsizetype(2000 * 100));
}

void TestHarnessParsers::acp_wrappers_are_equivalent() {
    const QString update = QStringLiteral(
        R"({"sessionUpdate":"tool_call","toolCallId":"c1","title":"Read a","kind":"read","status":"completed","content":[{"type":"content","content":{"type":"text","text":"x"}}]})");
    const QStringList forms = {
        QStringLiteral(R"({"jsonrpc":"2.0","method":"session/update","params":{"sessionId":"S","update":%1}})").arg(update),
        QStringLiteral(R"({"sessionId":"S","update":%1})").arg(update),
        update,
    };
    for (const QString& line : forms) {
        auto p = thp_parser(QStringLiteral("acp"));
        const QStringList sigs = thp_sigs(thp_lines(*p, {line}));
        QCOMPARE(sigs, (QStringList{"tool_call@c1", "tool_result@c1"}));
        if (line != update)
            QCOMPARE(p->facts().session_id, QStringLiteral("S"));
    }
    // The prompt response in both shapes.
    for (const QString& line : {QStringLiteral(R"({"jsonrpc":"2.0","id":1,"result":{"stopReason":"max_tokens"}})"),
                                QStringLiteral(R"({"stopReason":"max_tokens"})")}) {
        auto p = thp_parser(QStringLiteral("acp"));
        thp_lines(*p, {line});
        QVERIFY(p->facts().saw_result);
        QVERIFY(p->facts().result_is_error);
        QCOMPARE(p->facts().stop_reason, QStringLiteral("max_tokens"));
    }
}

void TestHarnessParsers::acp_update_for_unknown_tool_is_announced() {
    auto p = thp_parser(QStringLiteral("acp"));
    const QVector<cad::AgentEvent> ev = thp_lines(
        *p, {QStringLiteral(R"({"sessionUpdate":"tool_call_update","toolCallId":"late","title":"Fetch","status":"completed","rawOutput":{"ok":true}})"),
             QStringLiteral(R"({"sessionUpdate":"tool_call_update","toolCallId":"late","status":"completed"})"),
             QStringLiteral(R"({"sessionUpdate":"tool_call","status":"pending"})")});
    // Announced once, finished once; a call without an id is shown, not dropped.
    QCOMPARE(thp_sigs(ev), (QStringList{"tool_call@late", "tool_result@late", "status"}));
    QCOMPARE(ev[1].body, QStringLiteral("{\"ok\":true}"));
}

void TestHarnessParsers::gemini_whole_message_replaces_streamed_one() {
    auto p = thp_parser(QStringLiteral("gemini"));
    const QVector<cad::AgentEvent> ev = thp_lines(
        *p, {QStringLiteral(R"({"type":"message","role":"assistant","content":"Hel","delta":true})"),
             QStringLiteral(R"({"type":"message","role":"assistant","content":"Hello there"})"),
             QStringLiteral(R"({"type":"message","role":"assistant","content":"Separate."})")});
    QCOMPARE(thp_sigs(ev), (QStringList{"text~@msg-1", "text@msg-1", "text@msg-2"}));
    QCOMPARE(ev[1].body, QStringLiteral("Hello there"));
    QCOMPARE(p->facts().final_text, QStringLiteral("Separate."));
}

// ── Facts ────────────────────────────────────────────────────────────────────

void TestHarnessParsers::file_change_extraction() {
    auto changes = [](const QVector<cad::AgentEvent>& ev) {
        QStringList out;
        for (const cad::AgentEvent& e : ev) {
            if (e.kind == cad::EventKind::FileChange)
                for (const cad::FileChangeEntry& fc : cad::parse_file_change_body(e.body))
                    out << QStringLiteral("%1 %2%3").arg(fc.kind, fc.path, e.is_error ? QStringLiteral(" !") : QString());
        }
        return out;
    };
    // Claude: Edit / Write / MultiEdit / NotebookEdit after a successful result; not after a failed one.
    auto c = thp_parser(QStringLiteral("claude"));
    const QVector<cad::AgentEvent> cev = thp_lines(
        *c, {QStringLiteral(R"({"type":"assistant","message":{"content":[
               {"type":"tool_use","id":"e1","name":"Edit","input":{"file_path":"/a.txt","old_string":"x","new_string":"y"}},
               {"type":"tool_use","id":"w1","name":"Write","input":{"file_path":"/b.txt","content":"z"}},
               {"type":"tool_use","id":"m1","name":"MultiEdit","input":{"file_path":"/c.txt","edits":[]}},
               {"type":"tool_use","id":"n1","name":"NotebookEdit","input":{"notebook_path":"/d.ipynb"}},
               {"type":"tool_use","id":"r1","name":"Read","input":{"file_path":"/e.txt"}}]}})")
                 .simplified(),
             QStringLiteral(R"({"type":"user","message":{"content":[
               {"type":"tool_result","tool_use_id":"e1","content":"ok"},
               {"type":"tool_result","tool_use_id":"w1","content":"ok","is_error":false},
               {"type":"tool_result","tool_use_id":"m1","content":"denied","is_error":true},
               {"type":"tool_result","tool_use_id":"n1","content":[{"type":"text","text":"ok"}]},
               {"type":"tool_result","tool_use_id":"r1","content":"text"}]}})")
                 .simplified()});
    QCOMPARE(changes(cev), (QStringList{"update /a.txt", "write /b.txt", "update /d.ipynb"}));
    thp_check_invariants(cev);

    // Codex: file_change items; a failed one is flagged.
    auto x = thp_parser(QStringLiteral("codex"));
    const QVector<cad::AgentEvent> xev = thp_lines(
        *x, {QStringLiteral(R"({"type":"item.completed","item":{"id":"f1","type":"file_change","changes":[{"path":"a.py","kind":"add"},{"path":"b.py"}],"status":"completed"}})"),
             QStringLiteral(R"({"type":"item.completed","item":{"id":"f2","type":"file_change","changes":[{"path":"c.py","kind":"delete"}],"status":"failed"}})")});
    QCOMPARE(changes(xev), (QStringList{"add a.py", "update b.py", "delete c.py !"}));

    // ACP: diff content (new file = no oldText), a delete tool with a location, a failed edit.
    auto a = thp_parser(QStringLiteral("acp"));
    const QVector<cad::AgentEvent> aev = thp_lines(
        *a, {QStringLiteral(R"({"sessionUpdate":"tool_call","toolCallId":"d1","title":"Write","kind":"edit","status":"completed","content":[{"type":"diff","path":"/n.md","oldText":null,"newText":"x"},{"type":"diff","path":"/o.md","oldText":"a","newText":"b"}]})"),
             QStringLiteral(R"({"sessionUpdate":"tool_call","toolCallId":"d2","title":"Remove","kind":"delete","status":"pending","locations":[{"path":"/gone.md"}]})"),
             QStringLiteral(R"({"sessionUpdate":"tool_call_update","toolCallId":"d2","status":"completed"})"),
             QStringLiteral(R"({"sessionUpdate":"tool_call","toolCallId":"d3","title":"Edit","kind":"edit","status":"pending","locations":[{"path":"/f.md"}]})"),
             QStringLiteral(R"({"sessionUpdate":"tool_call_update","toolCallId":"d3","status":"failed"})"),
             QStringLiteral(R"({"sessionUpdate":"tool_call","toolCallId":"d4","title":"Read","kind":"read","status":"completed","locations":[{"path":"/r.md"}]})")});
    QCOMPARE(changes(aev), (QStringList{"add /n.md", "update /o.md", "delete /gone.md"}));
    thp_check_invariants(aev);
}

void TestHarnessParsers::claude_rate_limit_reset_formats() {
    const struct {
        const char* resets;
        qint64 ms;
    } cases[] = {
        {"1794945600", 1794945600000LL},     // epoch seconds
        {"1794945600123", 1794945600123LL},  // epoch milliseconds
        {"\"1794945600\"", 1794945600000LL}, // digits in a string
        {"\"2026-11-17T18:00:00Z\"", 1794938400000LL},
        {"\"2026-11-17T18:00:00.500Z\"", 1794938400500LL},
        {"\"soon\"", 0},
        {"-5", 0},
        {"null", 0},
    };
    for (const auto& c : cases) {
        auto p = thp_parser(QStringLiteral("claude"));
        const QVector<cad::AgentEvent> ev = thp_lines(
            *p, {QStringLiteral(R"({"type":"rate_limit_event","rate_limit_info":{"status":"allowed","resetsAt":%1}})")
                     .arg(QLatin1String(c.resets))});
        QCOMPARE(ev.size(), 1);
        QCOMPARE(ev[0].kind, cad::EventKind::Status);
        QCOMPARE(p->facts().rate_limit_resets_at_ms, c.ms);
        QCOMPARE(p->facts().rate_limit_status, QStringLiteral("allowed"));
    }
    auto r = thp_parser(QStringLiteral("claude"));
    const QVector<cad::AgentEvent> ev =
        thp_lines(*r, {QStringLiteral(R"({"type":"rate_limit_event","rate_limit_info":{"status":"rejected"}})")});
    QCOMPARE(ev[0].kind, cad::EventKind::Error);
    QVERIFY(ev[0].is_error);
    QCOMPARE(ClaudeAdapter().classify_failure(1, r->facts(), {}), FailureKind::RateLimit);
}

void TestHarnessParsers::codex_cached_tokens_accumulate() {
    auto p = thp_parser(QStringLiteral("codex"));
    thp_lines(*p, {QStringLiteral(R"({"type":"turn.completed","usage":{"input_tokens":100,"cached_input_tokens":60,"output_tokens":5}})"),
                   QStringLiteral(R"({"type":"turn.completed","usage":{"input_tokens":50,"cached_input_tokens":40,"output_tokens":7}})")});
    QCOMPARE(p->facts().input_tokens, qint64(150));
    QCOMPARE(p->facts().cached_tokens, qint64(100));
    QCOMPARE(p->facts().output_tokens, qint64(12));
}

QTEST_GUILESS_MAIN(TestHarnessParsers)
#include "tst_harness_parsers.moc"
