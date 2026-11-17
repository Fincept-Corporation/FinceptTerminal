// tst_run_artefacts — the pure rules of Chat Mode run artefacts
// (src/services/agent_tasks/ArtefactLogic.{h,cpp}, ReportHandoff.h):
//
//   * markdown tables found in an answer (captions, fences, escaped pipes, ragged rows);
//   * CSV / Markdown / JSON forms of a table round-trip, CSV quoting and the
//     spreadsheet-formula guard; numbers that look numeric stay numeric;
//   * the atomic file write the service relies on (QSaveFile: a cancelled write
//     leaves the old file whole) and a "restart" (the file read back);
//   * which artefacts a run's steps / final answer produce, deduplicated by key,
//     from a recorded run (a plan run step by step, then its end);
//   * retention: unpinned artefacts of deleted runs go, the size quota removes the
//     oldest of finished runs only, pinned artefacts always stay;
//   * tool results shown as table / key-value / text / error;
//   * an artefact as a Report Builder payload (ReportHandoff.h: text, table cut
//     with a note, series as a chart, JSON fenced, image bytes, provenance);
//   * the Notes hand-off: the notes.created event and board row name the run, and
//     the Notes screen's reader (NotesLogic.h, header-only) gets it back.
//
// Links TaskRunTypes.cpp and ArtefactLogic.cpp only (Qt Core).

#include "services/agent_tasks/ArtefactLogic.h"
#include "services/agent_tasks/ReportHandoff.h"
#include "screens/notes/NotesLogic.h" // header-only: how the Notes screen reads the hand-off event

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace fincept::agent_tasks;
using namespace fincept::agent_tasks::artefact_logic;

namespace {

const char* const kAnswer = R"(# Apple — valuation

Some intro text with a | pipe that is not a table.

## Valuation multiples

| Company | P/E | EV/EBITDA | Note |
|---|---:|:---:|---|
| **AAPL** | 28.4 | 21.1 | `core` |
| MSFT | 33.0 | 24.8 | uses \| pipe |
| GOOGL | 22.1 |

```
| not | a | table |
|---|---|---|
| inside | a | fence |
```

Revenue by segment:
| Segment | FY24 | FY25 |
| --- | --- | --- |
| iPhone | 201.2 | 209.6 |
| Services | 96.2 | 108.0 |
)";

TaskRun make_run(const QString& id, RunStatus status, const QString& origin = QStringLiteral("user"), bool plan = true) {
    TaskRun r;
    r.id = id;
    r.title = QStringLiteral("Deep-dive: AAPL");
    r.engine = QStringLiteral("cloud");
    r.plan_first = plan;
    r.origin = origin;
    r.status = status;
    r.created_at_ms = 1000;
    r.finished_at_ms = is_terminal(status) ? 5000 : 0;
    return r;
}

RunStep make_step(int idx, StepStatus status, const QString& summary = {}) {
    RunStep s;
    s.run_id = QStringLiteral("r1");
    s.idx = idx;
    s.title = QStringLiteral("Step title %1").arg(idx + 1);
    s.status = status;
    s.summary = summary;
    return s;
}

RunArtefact make_artefact(const QString& id, const QString& run, qint64 bytes, qint64 created, bool pinned = false) {
    RunArtefact a;
    a.id = id;
    a.run_id = run;
    a.bytes = bytes;
    a.created_at_ms = created;
    a.pinned = pinned;
    return a;
}

QSet<QString> keys_of(const QVector<ArtefactDraft>& drafts) {
    QSet<QString> k;
    for (const ArtefactDraft& d : drafts)
        k.insert(d.key);
    return k;
}

} // namespace

class TestRunArtefacts : public QObject {
    Q_OBJECT

  private slots:
    void finds_markdown_tables();
    void csv_round_trip_and_quoting();
    void csv_formula_guard();
    void markdown_and_json_forms();
    void file_names();
    void atomic_write_and_restart();
    void extraction_from_a_recorded_plan_run();
    void extraction_skips_chat_replies_and_dedupes();
    void extraction_of_a_failed_run();
    void retention_deletes_with_run_unless_pinned();
    void retention_quota_oldest_first();
    void tool_results_by_shape();
    void report_payloads();
    void notes_handoff_names_the_run();
};

// "Send to Notes": the notes.created event and the note's board row carry the run,
// and the Notes screen reads the run id back from that event (its OPEN CHAT MODE
// hands it to Chat Mode).
void TestRunArtefacts::notes_handoff_names_the_run() {
    namespace nb = fincept::notes_board;
    namespace notes = fincept::screens::notes;
    const QString run = QStringLiteral("1a2b3c4d-0000-4000-8000-00000000abcd");

    const QVariantMap ev = note_created_event(42, run, 2);
    QCOMPARE(ev.value(QStringLiteral("id")).toInt(), 42);
    QCOMPARE(ev.value(QStringLiteral("source")).toString(), nb::kOriginAgentRuns);
    QCOMPARE(ev.value(QStringLiteral("run_id")).toString(), run);
    QCOMPARE(ev.value(QStringLiteral("step")).toInt(), 2);
    const auto read = notes::chat_run_from_event(ev);
    QVERIFY(read.has_value());
    QCOMPARE(*read, run);

    // An artefact of the whole run: no step key.
    QVERIFY(!note_created_event(7, run, -1).contains(QStringLiteral("step")));
    // An older build's event (no run id): still from Chat Mode, run unknown.
    const auto old = notes::chat_run_from_event({{QStringLiteral("id"), 7}, {QStringLiteral("source"), nb::kOriginAgentRuns}});
    QVERIFY(old.has_value());
    QVERIFY(old->isEmpty());
    // Any other source (the dashboard tile, MCP tools, the Notes screen itself): not a link.
    QVERIFY(!notes::chat_run_from_event({{QStringLiteral("id"), 7}, {QStringLiteral("source"), QStringLiteral("dashboard")}})
                 .has_value());

    // The board row written with the note: Notes' own defaults plus the run.
    const nb::NoteBoardMeta m = note_board_row_for_run(42, QStringLiteral("RESEARCH"), run, 1'763'380'800);
    QCOMPARE(m.note_id, 42);
    QCOMPARE(m.origin, nb::kOriginAgentRuns);
    QCOMPARE(m.origin_ref, run);
    QCOMPARE(m.status, nb::default_status(QStringLiteral("RESEARCH"), false)); // RESEARCHING
    QCOMPARE(m.status, nb::kStatusResearching);
    QCOMPARE(m.rank, nb::default_rank(1'763'380'800));
    QVERIFY(m.review_on.isEmpty());
}

void TestRunArtefacts::finds_markdown_tables() {
    const QVector<MarkdownTable> tables = find_markdown_tables(QString::fromUtf8(kAnswer));
    QCOMPARE(tables.size(), 2); // the fenced one is not a table
    const MarkdownTable& a = tables.at(0);
    QCOMPARE(a.caption, QStringLiteral("Valuation multiples"));
    QCOMPARE(a.table.columns, (QStringList{"Company", "P/E", "EV/EBITDA", "Note"}));
    QCOMPARE(a.table.rows.size(), 3);
    QCOMPARE(a.table.rows.at(0).at(0), QStringLiteral("AAPL")); // emphasis removed
    QCOMPARE(a.table.rows.at(0).at(3), QStringLiteral("core"));
    QCOMPARE(a.table.rows.at(1).at(3), QStringLiteral("uses | pipe")); // escaped pipe kept as text
    QCOMPARE(a.table.rows.at(2).size(), 4);                            // a short row is padded
    QCOMPARE(a.table.rows.at(2).at(3), QString());
    const MarkdownTable& b = tables.at(1);
    QCOMPARE(b.caption, QStringLiteral("Revenue by segment"));
    QCOMPARE(b.table.rows.size(), 2);
    QVERIFY(find_markdown_tables(QStringLiteral("a | b\nno delimiter row")).isEmpty());
    QCOMPARE(find_markdown_tables(QString::fromUtf8(kAnswer), 1).size(), 1);
}

void TestRunArtefacts::csv_round_trip_and_quoting() {
    ArtefactTable t;
    t.columns = {QStringLiteral("Name"), QStringLiteral("Value"), QStringLiteral("Note")};
    t.rows = {{QStringLiteral("Revenue, FY25"), QStringLiteral("1,234.5"), QStringLiteral("said \"strong\"")},
              {QStringLiteral("Multi\nline"), QStringLiteral("-1.5%"), QStringLiteral("a|b")},
              {QStringLiteral(" padded "), QStringLiteral(""), QStringLiteral("plain")}};
    const QString csv = table_to_csv(t);
    QVERIFY(csv.startsWith(QStringLiteral("Name,Value,Note\r\n")));
    QVERIFY(csv.contains(QStringLiteral("\"Revenue, FY25\",\"1,234.5\",\"said \"\"strong\"\"\"")));
    QVERIFY(csv.contains(QStringLiteral("\"a|b\""))); // the Report Builder's codec splits rows on '|'
    QVERIFY(csv.contains(QStringLiteral("-1.5%")));   // a number keeps its sign (no formula guard)
    const ArtefactTable back = table_from_csv(csv);
    QCOMPARE(back.columns, t.columns);
    QCOMPARE(back.rows.size(), 3);
    QCOMPARE(back.rows.at(0), t.rows.at(0));
    QCOMPARE(back.rows.at(1), t.rows.at(1));
    QCOMPARE(back.rows.at(2), t.rows.at(2));
    // Tab separated and LF rows read too.
    const ArtefactTable tsv = table_from_csv(QStringLiteral("a\tb\n1\t2\n"));
    QCOMPARE(tsv.columns, (QStringList{"a", "b"}));
    QCOMPARE(tsv.rows.at(0), (QStringList{"1", "2"}));
}

void TestRunArtefacts::csv_formula_guard() {
    ArtefactTable t;
    t.columns = {QStringLiteral("c")};
    t.rows = {{QStringLiteral("=HYPERLINK(\"http://x\")")}, {QStringLiteral("@SUM(A1)")}, {QStringLiteral("+44 20")},
              {QStringLiteral("-12.5")}, {QStringLiteral("+2%")}, {QStringLiteral("-$1,200")}};
    const QString csv = table_to_csv(t);
    QVERIFY(csv.contains(QStringLiteral("\"'=HYPERLINK(\"\"http://x\"\")\"")));
    QVERIFY(csv.contains(QStringLiteral("'@SUM(A1)")));
    QVERIFY(csv.contains(QStringLiteral("'+44 20")));
    QVERIFY(csv.contains(QStringLiteral("\r\n-12.5\r\n")));
    QVERIFY(csv.contains(QStringLiteral("\r\n+2%\r\n")));
    QVERIFY(csv.contains(QStringLiteral("\"-$1,200\""))); // numeric: no apostrophe (quoted for the comma)
    QVERIFY(looks_numeric(QStringLiteral("3.2x")));
    QVERIFY(looks_numeric(QStringLiteral("(1,234)")));
    QVERIFY(!looks_numeric(QStringLiteral("=1+1")));
    QVERIFY(!looks_numeric(QStringLiteral("AAPL")));
}

void TestRunArtefacts::markdown_and_json_forms() {
    ArtefactTable t;
    t.columns = {QStringLiteral("k"), QStringLiteral("v")};
    t.rows = {{QStringLiteral("a|b"), QStringLiteral("1")}, {QStringLiteral("c"), QStringLiteral("2")}};
    const QString md = table_to_markdown(t);
    QVERIFY(md.startsWith(QStringLiteral("| k | v |\n| --- | --- |")));
    QVERIFY(md.contains(QStringLiteral("a\\|b")));
    // The Markdown form reads back as the same table.
    const QVector<MarkdownTable> again = find_markdown_tables(md);
    QCOMPARE(again.size(), 1);
    QCOMPARE(again.first().table.rows, t.rows);
    QVERIFY(table_to_markdown(t, 1).contains(QStringLiteral("1 more rows")));
    const QJsonObject j = table_to_json(t);
    QCOMPARE(table_from_json(j).rows, t.rows);
    // An array of records → every key a column, identifying ones first.
    const QJsonArray recs =
        QJsonDocument::fromJson(R"([{"symbol":"AAPL","px":190.1},{"px":410,"symbol":"MSFT","x":true}])").array();
    const ArtefactTable rt = table_from_json(recs);
    QCOMPARE(rt.columns, (QStringList{"symbol", "px", "x"}));
    QCOMPARE(rt.rows.at(1), (QStringList{"MSFT", "410", "true"}));
    // An array of arrays with a header row.
    const ArtefactTable at = table_from_json(QJsonDocument::fromJson(R"([["d","v"],["2026-01-02",1.5]])").array());
    QCOMPARE(at.columns, (QStringList{"d", "v"}));
    QCOMPARE(at.rows.at(0), (QStringList{"2026-01-02", "1.5"}));
}

void TestRunArtefacts::file_names() {
    QCOMPARE(safe_file_stem(QStringLiteral("Final report — Deep-dive: AAPL / 2026")),
             QStringLiteral("Final-report-Deep-dive-AAPL-2026"));
    QCOMPARE(safe_file_stem(QStringLiteral("../../etc/passwd")), QStringLiteral("etc-passwd"));
    QCOMPARE(safe_file_stem(QStringLiteral("   ")), QStringLiteral("artefact"));
    QVERIFY(safe_file_stem(QString(200, QLatin1Char('x'))).size() <= 48);
    const QString name = stored_name_for(QStringLiteral("0a1b2c3d-4e5f-6789-aaaa-bbbbccccdddd"), QStringLiteral("Peer table"),
                                         QStringLiteral("csv"));
    QCOMPARE(name, QStringLiteral("0a1b2c3d-Peer-table.csv"));
    QVERIFY(!name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\')));
    QCOMPARE(extension_for_kind(QString::fromLatin1(kArtefactReport)), QStringLiteral("md"));
    QCOMPARE(mime_for_kind(QString::fromLatin1(kArtefactTable)), QStringLiteral("text/csv"));
}

void TestRunArtefacts::atomic_write_and_restart() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("run-1/0a1b2c3d-report.md"));
    QDir().mkpath(dir.filePath(QStringLiteral("run-1")));
    {
        QSaveFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Report v1\n");
        QVERIFY(f.commit());
    }
    // A write that dies half-way (cancelled before commit) leaves the old file whole.
    {
        QSaveFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Report v2 — half writ");
        f.cancelWriting();
        QVERIFY(!f.commit());
    }
    // "Restart": a fresh reader sees exactly the committed content, and no temp file is left.
    QFile r(path);
    QVERIFY(r.open(QIODevice::ReadOnly));
    QCOMPARE(r.readAll(), QByteArray("# Report v1\n"));
    const QStringList left = QDir(dir.filePath(QStringLiteral("run-1"))).entryList(QDir::Files | QDir::Hidden);
    QCOMPARE(left, QStringList{QStringLiteral("0a1b2c3d-report.md")});
    // The sweep's rule: files no row names are leftovers.
    QCOMPARE(unreferenced_files({QStringLiteral("a.md"), QStringLiteral("b.csv"), QStringLiteral("x.md.Ab12Cd")},
                                {QStringLiteral("a.md")}),
             (QStringList{"b.csv", "x.md.Ab12Cd"}));
}

void TestRunArtefacts::extraction_from_a_recorded_plan_run() {
    // A three-step plan run as TaskRunService records it: step 1 done (with a
    // table), step 2 running, step 3 pending.
    TaskRun run = make_run(QStringLiteral("r1"), RunStatus::Running);
    QVector<RunStep> steps{make_step(0, StepStatus::Done, QString::fromUtf8(kAnswer)), make_step(1, StepStatus::Running),
                           make_step(2, StepStatus::Pending)};
    steps[0].deliverable = QStringLiteral("valuation table");
    // StepEnd(done) of step 1 → its deliverable and its two tables.
    QVector<ArtefactDraft> d1 = plan_run_artefacts(run, steps, {}, 0);
    QCOMPARE(keys_of(d1), (QSet<QString>{"step:0", "table:step0:1", "table:step0:2"}));
    const ArtefactDraft& rep = d1.at(0);
    QCOMPARE(rep.kind, QString::fromLatin1(kArtefactReport));
    QCOMPARE(rep.step_idx, 0);
    QVERIFY(rep.text.contains(QStringLiteral("Deliverable: valuation table")));
    QCOMPARE(rep.source.value("origin").toString(), QStringLiteral("step"));
    const ArtefactDraft& tab = d1.at(1);
    QCOMPARE(tab.kind, QString::fromLatin1(kArtefactTable));
    QCOMPARE(tab.title, QStringLiteral("Step 1 — Valuation multiples"));
    QCOMPARE(tab.source.value("step").toInt(), 0);
    // Nothing for the run as a whole while it is going.
    QCOMPARE(plan_run_artefacts(run, steps, keys_of(d1)).size(), 0);

    // The run completes: steps 2 and 3 done, a final report with one table.
    steps[1].status = StepStatus::Done;
    steps[1].summary = QStringLiteral("Margins are stable.");
    steps[2].status = StepStatus::Done;
    steps[2].summary = QStringLiteral("Risks: China.");
    run.status = RunStatus::Completed;
    run.finished_at_ms = 9000;
    run.final_text = QStringLiteral("## Verdict\n\n| View | Target |\n|---|---|\n| Own | 220 |\n");
    const QVector<ArtefactDraft> d2 = plan_run_artefacts(run, steps, keys_of(d1));
    QCOMPARE(keys_of(d2), (QSet<QString>{"step:1", "step:2", "final", "table:final:1"}));
    for (const ArtefactDraft& d : d2) {
        if (d.key == QLatin1String("final")) {
            QVERIFY(d.title.startsWith(QStringLiteral("Final report")));
            QVERIFY(d.text.startsWith(QStringLiteral("## Verdict"))); // a heading of its own: no title added
            QVERIFY(d.text.contains(QStringLiteral("Fincept Terminal agent run")));
        }
    }
    // Everything recorded: a second pass (restart, panel opened) adds nothing.
    QSet<QString> all = keys_of(d1);
    all.unite(keys_of(d2));
    QVERIFY(plan_run_artefacts(run, steps, all).isEmpty());
}

void TestRunArtefacts::extraction_skips_chat_replies_and_dedupes() {
    TaskRun chat = make_run(QStringLiteral("c1"), RunStatus::Completed, QStringLiteral("chat"), false);
    chat.final_text = QString::fromUtf8(kAnswer);
    QVERIFY(!run_produces_artefacts(chat));
    QVERIFY(plan_run_artefacts(chat, {}, {}).isEmpty());
    // A NEW RUN single reply (origin user) keeps its answer and tables.
    TaskRun single = make_run(QStringLiteral("s1"), RunStatus::Completed, QStringLiteral("user"), false);
    single.final_text = QString::fromUtf8(kAnswer);
    QVERIFY(run_produces_artefacts(single));
    const QVector<ArtefactDraft> d = plan_run_artefacts(single, {make_step(0, StepStatus::Done, single.final_text)}, {});
    QCOMPARE(keys_of(d), (QSet<QString>{"final", "table:final:1", "table:final:2"})); // one step: no step artefact
    QVERIFY(plan_run_artefacts(single, {}, {QStringLiteral("final"), QStringLiteral("table:final:1")}).size() == 1);
    // A scheduled local run makes artefacts too.
    TaskRun local = make_run(QStringLiteral("l1"), RunStatus::Completed, QStringLiteral("schedule"), false);
    local.engine = QStringLiteral("local");
    local.final_text = QStringLiteral("Done.");
    QCOMPARE(plan_run_artefacts(local, {}, {}).size(), 1);
}

void TestRunArtefacts::extraction_of_a_failed_run() {
    // A planned run that stopped at its budget: its steps are the artefacts, no
    // "partial results" duplicate of them.
    TaskRun run = make_run(QStringLiteral("r2"), RunStatus::BudgetStopped);
    run.final_text = QStringLiteral("## Step 1 — A\n\nResult A");
    const QVector<RunStep> steps{make_step(0, StepStatus::Done, QStringLiteral("Result A")),
                                 make_step(1, StepStatus::Pending)};
    QCOMPARE(keys_of(plan_run_artefacts(run, steps, {})), (QSet<QString>{"step:0"}));
    // A single-step run that failed with some text keeps it as partial results.
    TaskRun one = make_run(QStringLiteral("r3"), RunStatus::Failed, QStringLiteral("schedule"), false);
    one.final_text = QStringLiteral("Half an answer");
    const QVector<ArtefactDraft> d = plan_run_artefacts(one, {}, {});
    QCOMPARE(d.size(), 1);
    QVERIFY(d.first().title.startsWith(QStringLiteral("Partial results")));
}

void TestRunArtefacts::retention_deletes_with_run_unless_pinned() {
    RetentionInput in;
    in.artefacts = {make_artefact(QStringLiteral("a1"), QStringLiteral("gone"), 100, 1),
                    make_artefact(QStringLiteral("a2"), QStringLiteral("gone"), 100, 2, /*pinned=*/true),
                    make_artefact(QStringLiteral("a3"), QStringLiteral("live"), 100, 3)};
    in.live_run_ids = {QStringLiteral("live")};
    const RetentionPlan p = plan_retention(in);
    QCOMPARE(p.delete_ids, QStringList{QStringLiteral("a1")});
    QCOMPARE(p.bytes_before, qint64(300));
    QCOMPARE(p.bytes_after, qint64(200));
}

void TestRunArtefacts::retention_quota_oldest_first() {
    RetentionInput in;
    in.quota_bytes = 1000;
    in.live_run_ids = {QStringLiteral("old"), QStringLiteral("new"), QStringLiteral("active")};
    in.active_run_ids = {QStringLiteral("active")};
    in.artefacts = {make_artefact(QStringLiteral("o1"), QStringLiteral("old"), 400, 10),
                    make_artefact(QStringLiteral("o2"), QStringLiteral("old"), 400, 20, /*pinned=*/true),
                    make_artefact(QStringLiteral("act"), QStringLiteral("active"), 500, 5), // oldest, but its run is going
                    make_artefact(QStringLiteral("n1"), QStringLiteral("new"), 300, 30),
                    make_artefact(QStringLiteral("n2"), QStringLiteral("new"), 200, 40)};
    // Unpinned: 400 + 500 + 300 + 200 = 1400 > 1000 → down to <= 900: o1 (400) then n1 (300).
    const RetentionPlan p = plan_retention(in);
    QCOMPARE(p.delete_ids, (QStringList{"o1", "n1"}));
    QVERIFY(!p.delete_ids.contains(QStringLiteral("o2")));  // pinned
    QVERIFY(!p.delete_ids.contains(QStringLiteral("act"))); // a running run's artefact
    // Under the quota: nothing.
    in.quota_bytes = 10'000;
    QVERIFY(plan_retention(in).delete_ids.isEmpty());
}

void TestRunArtefacts::tool_results_by_shape() {
    // Terminal tool wrapper with a list of records → table.
    const QJsonObject wrapped = QJsonDocument::fromJson(
                                    R"({"success":true,"data":[{"symbol":"AAPL","price":190.2},{"symbol":"MSFT","price":410}]})")
                                    .object();
    ToolResultView v = classify_tool_result(wrapped, {}, true);
    QCOMPARE(v.shape, ResultShape::Table);
    QCOMPARE(v.table.columns, (QStringList{"symbol", "price"}));
    // A flat object → key / value.
    v = classify_tool_result(QJsonDocument::fromJson(R"({"pe":28.1,"beta":1.2,"name":"Apple"})").object(), {}, true);
    QCOMPARE(v.shape, ResultShape::KeyValue);
    QCOMPARE(v.fields.size(), 3);
    // One array of records under a key → table with the key as caption.
    v = classify_tool_result(QJsonDocument::fromJson(R"({"count":2,"quotes":[{"s":"A"},{"s":"B"}]})").object(), {}, true);
    QCOMPARE(v.shape, ResultShape::Table);
    QCOMPARE(v.text, QStringLiteral("quotes"));
    // The cloud's output preview: JSON text is parsed, other text stays text.
    v = classify_tool_result(QJsonValue(QJsonValue::Undefined), QStringLiteral(R"([{"d":"2026-01-02","v":1}])"), true);
    QCOMPARE(v.shape, ResultShape::Table);
    v = classify_tool_result(QJsonValue(QJsonValue::Undefined), QStringLiteral("Apple closed at 190.2 (+1.1%)…"), true);
    QCOMPARE(v.shape, ResultShape::Text);
    // Failures → error, with the tool's own message.
    v = classify_tool_result(QJsonDocument::fromJson(R"({"success":false,"error":"rate limited"})").object(), {}, true);
    QCOMPARE(v.shape, ResultShape::Error);
    QCOMPARE(v.text, QStringLiteral("rate limited"));
    v = classify_tool_result(QJsonValue(QJsonValue::Undefined), QStringLiteral("Timeout after 30 s"), false);
    QCOMPARE(v.shape, ResultShape::Error);
    QCOMPARE(v.text, QStringLiteral("Timeout after 30 s"));
    QCOMPARE(classify_tool_result(QJsonValue(QJsonValue::Undefined), {}, true).shape, ResultShape::Empty);
    QCOMPARE(summarize_arguments(QStringLiteral(R"({"symbol": "AAPL", "period": "1y"})")),
             QStringLiteral("symbol=AAPL, period=1y"));
    QVERIFY(summarize_arguments(QString(500, QLatin1Char('a')), 40).size() <= 40);
}

void TestRunArtefacts::report_payloads() {
    RunArtefact a;
    a.id = QStringLiteral("a1");
    a.run_id = QStringLiteral("run-12345678");
    a.run_title = QStringLiteral("Deep-dive: AAPL");
    a.step_idx = 1;
    a.kind = QString::fromLatin1(kArtefactReport);
    a.title = QStringLiteral("Step 2 — Valuation");
    a.created_at_ms = 1'700'000'000'000;

    // Markdown: the text as it is (ReportInbox splits headings / tables / fences).
    ArtefactReportContent c;
    c.text = QString::fromUtf8(kAnswer);
    fincept::report::ReportPayload p = artefact_report_payload(a, c);
    QVERIFY(!p.is_empty());
    QCOMPARE(p.title, a.title);
    QCOMPARE(p.markdown, c.text);
    QCOMPARE(p.source, QString::fromLatin1(kReportSourceLabel));
    QCOMPARE(p.as_of.toMSecsSinceEpoch(), a.created_at_ms);
    QVERIFY(p.notes.contains(QStringLiteral("Deep-dive: AAPL")));
    QVERIFY(p.notes.contains(QStringLiteral("2")));
    QVERIFY(p.tables.isEmpty());

    // A table: the rows, cut with a visible note above kReportMaxTableRows.
    a.kind = QString::fromLatin1(kArtefactTable);
    ArtefactTable t;
    t.columns = {QStringLiteral("Date"), QStringLiteral("Close"), QStringLiteral("Note")};
    for (int i = 0; i < kReportMaxTableRows + 20; ++i)
        t.rows.append({QStringLiteral("2026-01-%1").arg(i), QString::number(100 + i), QStringLiteral("x")});
    c = {};
    c.table = t;
    p = artefact_report_payload(a, c);
    QCOMPARE(p.tables.size(), 1);
    QCOMPARE(p.tables.first().columns, t.columns);
    QCOMPARE(p.tables.first().rows.size(), kReportMaxTableRows);
    QVERIFY(p.tables.first().note.contains(QString::number(kReportMaxTableRows + 20)));
    QVERIFY(p.charts.isEmpty());
    QVERIFY(p.markdown.isEmpty());

    // A series: a line chart of its numeric columns (the text column is left out) + the table.
    a.kind = QString::fromLatin1(kArtefactSeries);
    t.rows.resize(30);
    c.table = t;
    p = artefact_report_payload(a, c);
    QCOMPARE(p.charts.size(), 1);
    QCOMPARE(p.charts.first().kind, QStringLiteral("line"));
    QCOMPARE(p.charts.first().series.size(), 1);
    QCOMPARE(p.charts.first().series.first().name, QStringLiteral("Close"));
    QCOMPARE(p.charts.first().series.first().y.size(), 30);
    QCOMPARE(p.charts.first().series.first().x.first(), QStringLiteral("2026-01-0"));
    QCOMPARE(p.tables.size(), 1);

    // JSON that is not tabular: fenced as code.
    a.kind = QString::fromLatin1(kArtefactJson);
    c = {};
    c.text = QStringLiteral("{\n  \"pe\": 28.4\n}");
    p = artefact_report_payload(a, c);
    QVERIFY(p.markdown.startsWith(QStringLiteral("```json\n")));
    QVERIFY(p.markdown.endsWith(QStringLiteral("\n```")));

    // An image: its bytes as they are; without bytes nothing is sent.
    a.kind = QString::fromLatin1(kArtefactImage);
    c = {};
    p = artefact_report_payload(a, c);
    QVERIFY(p.is_empty());
    c.image = QByteArray("\x89PNG....", 8);
    c.image_format = QStringLiteral("png");
    p = artefact_report_payload(a, c);
    QCOMPARE(p.charts.size(), 1);
    QCOMPARE(p.charts.first().image, c.image);
    QCOMPARE(p.charts.first().image_format, QStringLiteral("png"));

    // A file without previewable content: nothing to add.
    a.kind = QString::fromLatin1(kArtefactFile);
    p = artefact_report_payload(a, ArtefactReportContent{});
    QVERIFY(p.is_empty());

    // Provenance of a tool result without a run title.
    a.run_title.clear();
    a.step_idx = -1;
    a.source = QJsonObject{{QStringLiteral("tool"), QStringLiteral("get_quote")}};
    const QString prov = artefact_provenance(a);
    QVERIFY(prov.contains(QStringLiteral("run-1234")));
    QVERIFY(prov.contains(QStringLiteral("get_quote")));
    QVERIFY(!prov.contains(QStringLiteral("step")));
}

QTEST_GUILESS_MAIN(TestRunArtefacts)
#include "tst_run_artefacts.moc"
