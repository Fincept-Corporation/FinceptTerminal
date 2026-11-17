// tst_harness_arena_client — the pure parts of an Alpha Arena decision answered
// by an agent harness (src/services/alpha_arena/HarnessArenaLogic.h): the
// decision schema (valid JSON, strict-mode shaped, read by the arena's own
// parser when the model fills nullable fields with null), the request built for
// the harness (no tools, the arena's 60 s for the whole call, JSON by schema
// only where the harness supports it), the JSON object pulled out of a reply
// (bare, fenced, prose around it, braces inside strings, none at all), Claude
// Code's structured_output, and the result mapping — a timeout must read
// exactly "timeout" (ArenaEngine::finish_agent compares it). No process, no
// network, no model.

#include "services/alpha_arena/ArenaDecisionParser.h"
#include "services/alpha_arena/HarnessArenaLogic.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace fincept::arena;
using namespace fincept::arena::harness_arena;
using fincept::harness::ChatTools;
using fincept::harness::FailureKind;
using fincept::harness::HarnessChatResult;

class TestHarnessArenaClient : public QObject {
    Q_OBJECT

  private slots:
    void schema_is_strict_json();
    void schema_shaped_reply_parses();
    void request_has_no_tools_and_one_budget();
    void request_without_schema_support();
    void extract_bare_object();
    void extract_fenced_object();
    void extract_with_prose_around();
    void extract_braces_inside_strings();
    void extract_skips_stray_brace_in_prose();
    void extract_none();
    void structured_output_from_claude_result();
    void map_timeout_is_exactly_timeout();
    void map_failure_keeps_sentence();
    void map_cancelled();
    void map_success_with_tokens();
    void map_success_without_json_hands_raw_text_on();
    void map_prefers_structured_output();
    void map_empty_answer();
};

void TestHarnessArenaClient::schema_is_strict_json() {
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(decision_schema().toUtf8(), &pe);
    QCOMPARE(pe.error, QJsonParseError::NoError);
    QVERIFY(doc.isObject());
    const QJsonObject root = doc.object();
    QCOMPARE(root.value("type").toString(), QStringLiteral("object"));
    QCOMPARE(root.value("additionalProperties").toBool(true), false);
    QVERIFY(root.value("required").toArray() == QJsonArray({QStringLiteral("actions")}));
    const QJsonObject item =
        root.value("properties").toObject().value("actions").toObject().value("items").toObject();
    QCOMPARE(item.value("additionalProperties").toBool(true), false);
    // Strict mode: every property listed as required.
    const QJsonObject props = item.value("properties").toObject();
    QStringList required;
    for (const auto& v : item.value("required").toArray())
        required << v.toString();
    QStringList keys = props.keys();
    keys.sort();
    required.sort();
    QCOMPARE(required, keys);
    QVERIFY(props.value("action").toObject().value("enum").toArray() ==
            QJsonArray({QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("hold")}));
}

void TestHarnessArenaClient::schema_shaped_reply_parses() {
    // What a model bound to the schema returns: nulls where the prompt lets it
    // leave a field out. The arena parser must read them like missing fields.
    const QString reply = QStringLiteral(
        R"({"actions":[{"action":"open","coin":"BTC","side":"long","size_usd":2000,"leverage":3,"reason":"trend"},)"
        R"({"action":"close","coin":"ETH","side":null,"size_usd":null,"leverage":null,"reason":"take profit"},)"
        R"({"action":"hold","coin":null,"side":null,"size_usd":null,"leverage":null,"reason":"wait"}]})");
    const ParsedDecision d = parse_decision(extract_json_object(reply), {"BTC", "ETH"}, 10);
    QVERIFY2(d.ok(), qPrintable(d.error));
    QCOMPARE(d.actions.size(), 3);
    QCOMPARE(d.actions[0].kind, ActionKind::Open);
    QCOMPARE(d.actions[0].leverage, 3.0);
    QCOMPARE(d.actions[1].kind, ActionKind::Close);
    QCOMPARE(d.actions[1].size_usd, 0.0); // null = close the whole position
    QCOMPARE(d.actions[2].kind, ActionKind::Hold);
}

void TestHarnessArenaClient::request_has_no_tools_and_one_budget() {
    const auto r = build_request(QStringLiteral("claude"), QStringLiteral("sonnet"), QStringLiteral("low"),
                                 QStringLiteral("sys"), QStringLiteral("user"), 60000, true);
    QCOMPARE(r.harness_id, QStringLiteral("claude"));
    QCOMPARE(r.model, QStringLiteral("sonnet"));
    QCOMPARE(r.effort, QStringLiteral("low"));
    QCOMPARE(r.system_prompt, QStringLiteral("sys"));
    QCOMPARE(r.user_message, QStringLiteral("user"));
    QCOMPARE(r.feature, QStringLiteral("arena"));
    QCOMPARE(r.tools, ChatTools::None);
    QVERIFY(r.history.isEmpty());
    QCOMPARE(r.timeout_ms, 60000);
    QCOMPARE(r.first_event_timeout_ms, 60000);
    QCOMPARE(r.json_schema, decision_schema());
    QVERIFY(!r.persist_session);
    // A missing budget falls back to the arena's 60 s.
    QCOMPARE(build_request("codex", {}, {}, {}, "u", 0, false).timeout_ms, 60000);
}

void TestHarnessArenaClient::request_without_schema_support() {
    const auto r = build_request(QStringLiteral("codex"), QString(), QString(), QStringLiteral("s"),
                                 QStringLiteral("u"), 30000, false);
    QVERIFY(r.json_schema.isEmpty());
    QVERIFY(r.model.isEmpty()); // the CLI's default model
    QCOMPARE(r.timeout_ms, 30000);
}

void TestHarnessArenaClient::extract_bare_object() {
    const QString j = extract_json_object(QStringLiteral(" {\"actions\":[{\"action\":\"hold\",\"reason\":\"x\"}]} \n"));
    QCOMPARE(QJsonDocument::fromJson(j.toUtf8()).object().value("actions").toArray().size(), 1);
}

void TestHarnessArenaClient::extract_fenced_object() {
    const QString raw = QStringLiteral("Here is my decision:\n```json\n{\"actions\":[{\"action\":\"hold\",\"reason\":\"calm\"}]}\n```\n");
    const QJsonObject o = QJsonDocument::fromJson(extract_json_object(raw).toUtf8()).object();
    QCOMPARE(o.value("actions").toArray().at(0).toObject().value("reason").toString(), QStringLiteral("calm"));
}

void TestHarnessArenaClient::extract_with_prose_around() {
    const QString raw = QStringLiteral("I looked at the book. {\"actions\":[{\"action\":\"hold\",\"reason\":\"r\"}]} "
                                       "That is all.");
    QVERIFY(!extract_json_object(raw).isEmpty());
}

void TestHarnessArenaClient::extract_braces_inside_strings() {
    const QString raw = QStringLiteral(
        "Decision: {\"actions\":[{\"action\":\"hold\",\"reason\":\"unmatched } here and { there\",\"nested\":{\"a\":1}}]}"
        " trailing } brace");
    const QJsonObject o = QJsonDocument::fromJson(extract_json_object(raw).toUtf8()).object();
    QCOMPARE(o.value("actions").toArray().at(0).toObject().value("reason").toString(),
             QStringLiteral("unmatched } here and { there"));
}

void TestHarnessArenaClient::extract_skips_stray_brace_in_prose() {
    // A brace in the prose before the object does not hide the object.
    const QString raw = QStringLiteral("Note {not json here. Then {\"actions\":[{\"action\":\"hold\",\"reason\":\"r\"}]}");
    QVERIFY(!extract_json_object(raw).isEmpty());
}

void TestHarnessArenaClient::extract_none() {
    QVERIFY(extract_json_object(QStringLiteral("I would rather hold this round.")).isEmpty());
    QVERIFY(extract_json_object(QString()).isEmpty());
    QVERIFY(extract_json_object(QStringLiteral("[1,2,3]")).isEmpty()); // an array is not the decision object
}

void TestHarnessArenaClient::structured_output_from_claude_result() {
    const QString raw = QStringLiteral(
        R"({"type":"result","subtype":"success","structured_output":{"actions":[{"action":"hold","reason":"x"}]},"usage":{}})");
    const QJsonObject o = QJsonDocument::fromJson(structured_output_from_result_raw(raw).toUtf8()).object();
    QCOMPARE(o.value("actions").toArray().size(), 1);
    QVERIFY(structured_output_from_result_raw(QStringLiteral(R"({"type":"result","usage":{}})")).isEmpty());
    QVERIFY(structured_output_from_result_raw(QStringLiteral("not json structured_output")).isEmpty());
}

void TestHarnessArenaClient::map_timeout_is_exactly_timeout() {
    HarnessChatResult r;
    r.failure = FailureKind::Timeout;
    r.error = QStringLiteral("Claude Code did not finish within 60 s.");
    r.duration_ms = 60000;
    const ArenaHarnessOutcome o = map_result(r);
    QVERIFY(!o.success);
    QCOMPARE(o.error, QStringLiteral("timeout"));
    QCOMPARE(o.latency_ms, qint64(60000));
}

void TestHarnessArenaClient::map_failure_keeps_sentence() {
    HarnessChatResult r;
    r.failure = FailureKind::RateLimit;
    r.error = QStringLiteral("Codex is rate limited.");
    const ArenaHarnessOutcome o = map_result(r);
    QVERIFY(!o.success);
    QCOMPARE(o.error, QStringLiteral("Codex is rate limited."));
    HarnessChatResult blank;
    blank.failure = FailureKind::Other;
    QVERIFY(!map_result(blank).error.isEmpty());
}

void TestHarnessArenaClient::map_cancelled() {
    HarnessChatResult r;
    r.cancelled = true;
    QCOMPARE(map_result(r).error, QStringLiteral("cancelled"));
}

void TestHarnessArenaClient::map_success_with_tokens() {
    HarnessChatResult r;
    r.success = true;
    r.content = QStringLiteral("```json\n{\"actions\":[{\"action\":\"hold\",\"reason\":\"r\"}]}\n```");
    r.input_tokens = 1234;
    r.output_tokens = 56;
    r.duration_ms = 4200;
    const ArenaHarnessOutcome o = map_result(r);
    QVERIFY(o.success);
    QVERIFY(o.error.isEmpty());
    QCOMPARE(o.prompt_tokens, 1234);
    QCOMPARE(o.completion_tokens, 56);
    QCOMPARE(o.latency_ms, qint64(4200));
    QVERIFY(!o.content.contains(QLatin1String("```"))); // the object only
    QVERIFY(parse_decision(o.content, {"BTC"}, 5).ok());
}

void TestHarnessArenaClient::map_success_without_json_hands_raw_text_on() {
    HarnessChatResult r;
    r.success = true;
    r.content = QStringLiteral("I would hold.");
    const ArenaHarnessOutcome o = map_result(r);
    QVERIFY(o.success); // the engine's parser reports it and runs its repair retry
    QCOMPARE(o.content, QStringLiteral("I would hold."));
}

void TestHarnessArenaClient::map_prefers_structured_output() {
    HarnessChatResult r;
    r.success = true;
    r.content = QStringLiteral("Done.");
    const QString so = QStringLiteral(R"({"actions":[{"action":"hold","reason":"so"}]})");
    QCOMPARE(map_result(r, so).content, so);
}

void TestHarnessArenaClient::map_empty_answer() {
    HarnessChatResult r;
    r.success = true;
    r.content = QStringLiteral("   ");
    const ArenaHarnessOutcome o = map_result(r);
    QVERIFY(!o.success);
    QCOMPARE(o.error, QStringLiteral("empty completion"));
}

QTEST_GUILESS_MAIN(TestHarnessArenaClient)
#include "tst_harness_arena_client.moc"
