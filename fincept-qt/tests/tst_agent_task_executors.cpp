// tst_agent_task_executors — Chat Mode's multi-step executors (WP3 / WP4) in
// src/services/agent_tasks, without network, Python or the app:
//   * RunPlanPrompts.h: the plan parser and the plan / step / synthesis prompts;
//   * CloudPlanExecutor driven by a fake ICloudBackend / ICloudTurn
//     (plan -> steps -> synthesis, pause at a boundary, budget stop, cancel,
//     retry-once-then-fail, resume from step k, questions, restart);
//   * LocalAgenticExecutor fed recorded AGENTIC_EVENT payloads (shapes from
//     scripts/agents/finagent_core/agentic/runner.py and AgenticTasksPanel).

#include "services/agent_tasks/CloudPlanExecutor.h"
#include "services/agent_tasks/CloudTurnPort.h"
#include "services/agent_tasks/LocalAgentPort.h"
#include "services/agent_tasks/LocalAgenticExecutor.h"
#include "services/agent_tasks/RunPlanPrompts.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>

using namespace fincept::agent_tasks;

// The production default constructors (network backend / AgentService engine) live in
// CloudTurnDriver.cpp and LocalAgentEngine.cpp, which this Qt-Core-only target does not
// link. moc still references a default constructor for each Q_OBJECT class, so provide
// inert ones here; the tests always inject a fake backend / engine.
namespace fincept::agent_tasks {
CloudPlanExecutor::CloudPlanExecutor(QObject* parent) : CloudPlanExecutor(std::unique_ptr<ICloudBackend>(), parent) {}
LocalAgenticExecutor::LocalAgenticExecutor(QObject* parent)
    : LocalAgenticExecutor(std::unique_ptr<ILocalAgentEngine>(), parent) {}
} // namespace fincept::agent_tasks
namespace rpp = fincept::agent_tasks::run_plan_prompts;

namespace {

QJsonObject obj(const char* json) {
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

void pump() {
    for (int i = 0; i < 8; ++i)
        QCoreApplication::processEvents();
}

const char* kTwoStepPlan = "Here is the plan.\n```json\n"
                           R"({"steps":[{"title":"Gather","instruction":"Get the AAPL quote and ratios","deliverable":"a table"},)"
                           R"({"title":"Compare","instruction":"Compare with MSFT and GOOGL","deliverable":"a ranking"}]})"
                           "\n```\nShall I start?";

TaskRun cloud_run(const QString& id) {
    TaskRun r;
    r.id = id;
    r.title = QStringLiteral("AAPL review");
    r.prompt = QStringLiteral("Analyse AAPL against its peers");
    r.engine = QString::fromLatin1(kEngineCloud);
    r.plan_first = true;
    r.mode = QStringLiteral("lite");
    r.session_uuid = QStringLiteral("sess-1");
    r.status = RunStatus::Planning;
    r.started_at_ms = QDateTime::currentMSecsSinceEpoch();
    r.budget = {50, 8, 0};
    return r;
}

// ── Fake cloud turns ─────────────────────────────────────────────────────────

class FakeTurn final : public QObject, public ICloudTurn {
  public:
    FakeTurn(ICloudTurnListener* listener, const QString& run_id, QObject* parent)
        : QObject(parent), listener_(listener), run_id_(run_id) {}

    void start(const CloudTurnRequest& request) override { requests.append(request); }
    void detach() override {
        if (released)
            return;
        ++detaches;
        if (recovering_) {
            CloudTurnOutcome o;
            o.kind = CloudTurnOutcome::Kind::Stopped;
            o.error = QStringLiteral("Stopped waiting for the stored answer.");
            listener_->cloud_turn_outcome(run_id_, o);
            return;
        }
        recovering_ = true;
    }
    void release() override {
        released = true;
        deleteLater();
    }
    bool recovering() const override { return recovering_; }
    bool is_open() const override { return open; }

    // ── Test drivers ──
    void text(const QString& chunk) {
        listener_->cloud_turn_text(run_id_, requests.isEmpty() ? 0 : requests.last().step, chunk);
    }
    void answer(const QString& text, double credits = 5) {
        if (credits > 0)
            listener_->cloud_turn_usage(run_id_, credits, 0);
        listener_->cloud_turn_usage(run_id_, 0, 100);
        CloudTurnOutcome o;
        o.kind = CloudTurnOutcome::Kind::Answered;
        o.text = text;
        listener_->cloud_turn_outcome(run_id_, o);
    }
    void adopt(const QString& text) {
        recovering_ = false;
        CloudTurnOutcome o;
        o.kind = CloudTurnOutcome::Kind::Answered;
        o.adopted = true;
        o.message_uuid = QStringLiteral("msg-1");
        o.text = text;
        listener_->cloud_turn_outcome(run_id_, o);
    }
    void ask(const QString& question) {
        CloudTurnOutcome o;
        o.kind = CloudTurnOutcome::Kind::Answered;
        o.question = true;
        o.resumable = true;
        o.question_text = question;
        o.text = question;
        listener_->cloud_turn_outcome(run_id_, o);
    }
    void fail(const QString& code, const QString& message = QStringLiteral("The connection was lost.")) {
        CloudTurnOutcome o;
        o.kind = CloudTurnOutcome::Kind::Failed;
        o.error = message;
        o.code = code;
        listener_->cloud_turn_outcome(run_id_, o);
    }
    void close() {
        open = false;
        listener_->cloud_turn_closed(run_id_);
    }

    QVector<CloudTurnRequest> requests;
    bool released = false;
    bool open = false;
    int detaches = 0;

  private:
    ICloudTurnListener* listener_;
    QString run_id_;
    bool recovering_ = false;
};

class FakeBackend final : public ICloudBackend {
  public:
    ICloudTurn* create_turn(ICloudTurnListener* listener, const QString& run_id, QObject* parent) override {
        auto* t = new FakeTurn(listener, run_id, parent);
        turns.append(t);
        return t;
    }
    void create_session(QObject* ctx, std::function<void(bool, QString, QString)> cb) override {
        ++sessions;
        QPointer<QObject> context(ctx);
        const bool ok = session_ok;
        QTimer::singleShot(0, [context, cb, ok]() {
            if (context)
                cb(ok, ok ? QStringLiteral("sess-new") : QString(), ok ? QString() : QStringLiteral("server down"));
        });
    }
    const TaskRun* find_run(const QString& run_id) const override {
        const auto it = runs.constFind(run_id);
        return it == runs.cend() ? nullptr : &it.value();
    }
    QVector<RunStep> run_steps(const QString& run_id) const override { return steps.value(run_id); }
    void log_info(const QString& message) override { logs << message; }

    FakeTurn* turn(int i) const { return i >= 0 && i < turns.size() ? turns.at(i).data() : nullptr; }
    FakeTurn* last() const { return turn(static_cast<int>(turns.size()) - 1); }

    QVector<QPointer<FakeTurn>> turns;
    QHash<QString, TaskRun> runs;
    QHash<QString, QVector<RunStep>> steps;
    bool session_ok = true;
    int sessions = 0;
    QStringList logs;
};

// ── Fake local engine ────────────────────────────────────────────────────────

class FakeEngine final : public ILocalAgentEngine {
  public:
    void set_listener(ILocalAgentListener* l) override { listener = l; }
    LocalAgentReadiness readiness() const override { return ready; }
    QString start_task(const QString& query, const QJsonObject& config) override {
        calls << QStringLiteral("start:") + query;
        last_config = config;
        if (sync_error && listener) // AgentService: Python unavailable -> error event before returning
            listener->local_agent_event({}, obj(R"({"kind":"error","task_id":"","error":"Python not available"})"));
        return QStringLiteral("req-%1").arg(++requests);
    }
    QString resume_task(const QString& task_id, const QJsonObject& config) override {
        calls << QStringLiteral("resume:") + task_id;
        last_config = config;
        return QStringLiteral("req-%1").arg(++requests);
    }
    QString reply_to_question(const QString& task_id, const QString& answer, const QJsonObject& config) override {
        calls << QStringLiteral("reply:%1:%2").arg(task_id, answer);
        last_config = config;
        return QStringLiteral("req-%1").arg(++requests);
    }
    void pause_task(const QString& task_id) override { calls << QStringLiteral("pause:") + task_id; }
    void cancel_task(const QString& task_id) override { calls << QStringLiteral("cancel:") + task_id; }
    const TaskRun* find_run(const QString& run_id) const override {
        const auto it = runs.constFind(run_id);
        return it == runs.cend() ? nullptr : &it.value();
    }
    QVector<RunStep> run_steps(const QString& run_id) const override { return steps.value(run_id); }
    void log_info(const QString&) override {}

    /// One AGENTIC_EVENT as AgentService publishes it (task id + request id).
    void emit_event(const QString& task_id, const char* json, const QString& request_id = {}) {
        QJsonObject e = obj(json);
        e["task_id"] = task_id;
        if (!request_id.isEmpty())
            e["request_id"] = request_id;
        listener->local_agent_event(task_id, e);
    }

    ILocalAgentListener* listener = nullptr;
    LocalAgentReadiness ready{true, {}, {}, QStringLiteral("openai · gpt-4o")};
    bool sync_error = false;
    QStringList calls;
    QJsonObject last_config;
    int requests = 0;
    QHash<QString, TaskRun> runs;
    QHash<QString, QVector<RunStep>> steps;
};

// Recorded payloads (runner.py emit calls; "budget" = BudgetGuard.snapshot()).
const char* kPlanReady = R"({"kind":"plan_ready","plan":{"id":"plan_1","name":"Research plan","steps":[
    {"id":"step_1","name":"Gather data","step_type":"agent","config":{"query":"Get the AAPL quote"},"dependencies":[],"status":"pending"},
    {"id":"step_2","name":"Analyse","step_type":"agent","config":{"query":"Compare with peers"},"dependencies":["step_1"],"status":"pending"}]}})";
const char* kStepStart0 = R"({"kind":"step_start","step_index":0,"step":{"id":"step_1","name":"Gather data","config":{"query":"Get the AAPL quote"}},
    "budget":{"tokens_used":0,"max_tokens":1000000,"cost_used":0,"max_cost_usd":5.0,"wall_s":2,"max_wall_s":3600,"steps_used":0,"max_steps":50}})";
const char* kStepEnd0 = R"({"kind":"step_end","step_index":0,"result":{"step":1,"id":"step_1","description":"Gather data","response":"AAPL trades at 231.4"},
    "budget":{"tokens_used":1200,"max_tokens":1000000,"cost_used":0.01,"max_cost_usd":5.0,"wall_s":20,"max_wall_s":3600,"steps_used":1,"max_steps":50}})";
const char* kReflection0 = R"({"kind":"reflection","step_index":0,"decision":{"decision":"continue","reason":"on track"}})";
const char* kStepStart1 = R"({"kind":"step_start","step_index":1,"step":{"id":"step_2","name":"Analyse","config":{"query":"Compare with peers"}},
    "budget":{"tokens_used":1200,"max_tokens":1000000,"cost_used":0.01,"max_cost_usd":5.0,"wall_s":21,"max_wall_s":3600,"steps_used":1,"max_steps":50}})";
const char* kStepEnd1 = R"({"kind":"step_end","step_index":1,"result":{"step":2,"id":"step_2","description":"Analyse","response":"AAPL is fairly valued"},
    "budget":{"tokens_used":2000,"max_tokens":1000000,"cost_used":0.02,"max_cost_usd":5.0,"wall_s":40,"max_wall_s":3600,"steps_used":2,"max_steps":50}})";
const char* kDone = R"({"kind":"done","final":"Final answer: hold AAPL","steps_completed":2})";

TaskRun local_run(const QString& id) {
    TaskRun r;
    r.id = id;
    r.title = QStringLiteral("Local review");
    r.prompt = QStringLiteral("Review AAPL");
    r.engine = QString::fromLatin1(kEngineLocal);
    r.status = RunStatus::Planning;
    r.started_at_ms = QDateTime::currentMSecsSinceEpoch();
    r.budget = {0, 6, 30};
    return r;
}

// ── What an executor signalled ───────────────────────────────────────────────

struct Recorder {
    struct Step {
        int idx;
        StepStatus status;
        QString summary;
    };
    struct Finish {
        RunStatus status;
        QString final_text;
        QString error;
    };

    explicit Recorder(IRunExecutor* ex) {
        QObject::connect(ex, &IRunExecutor::event, ex, [this](const QString&, const RunEvent& e) { events.append(e); });
        QObject::connect(ex, &IRunExecutor::text_delta, ex,
                         [this](const QString&, int step, const QString& c) { text.append({step, c}); });
        QObject::connect(ex, &IRunExecutor::plan_ready, ex,
                         [this](const QString&, const QVector<RunStep>& s) { plans.append(s); });
        QObject::connect(ex, &IRunExecutor::step_changed, ex,
                         [this](const QString&, int idx, StepStatus st, const QString& sum) { steps.append({idx, st, sum}); });
        QObject::connect(ex, &IRunExecutor::usage, ex, [this](const QString&, double c, int t) {
            if (c < 0 || t < 0)
                negative_usage = true;
            credits += c;
            tokens += t;
        });
        QObject::connect(ex, &IRunExecutor::waiting, ex,
                         [this](const QString&, RunStatus why, const QJsonObject& p) { waits.append({why, p}); });
        QObject::connect(ex, &IRunExecutor::finished, ex, [this](const QString&, RunStatus st, const QString& f, const QString& e) {
            finishes.append({st, f, e});
        });
    }

    const RunEvent* last(RunEventKind kind) const {
        for (int i = static_cast<int>(events.size()) - 1; i >= 0; --i)
            if (events.at(i).kind == kind)
                return &events.at(i);
        return nullptr;
    }
    int count(RunEventKind kind) const {
        return static_cast<int>(std::count_if(events.cbegin(), events.cend(), [kind](const RunEvent& e) { return e.kind == kind; }));
    }
    bool has_text(RunEventKind kind, const QString& part) const {
        return std::any_of(events.cbegin(), events.cend(), [&](const RunEvent& e) {
            return e.kind == kind && (e.title.contains(part) || e.body.contains(part));
        });
    }
    int step_count(int idx, StepStatus st) const {
        return static_cast<int>(
            std::count_if(steps.cbegin(), steps.cend(), [&](const Step& s) { return s.idx == idx && s.status == st; }));
    }

    QVector<RunEvent> events;
    QVector<QPair<int, QString>> text;
    QVector<QVector<RunStep>> plans;
    QVector<Step> steps;
    QVector<QPair<RunStatus, QJsonObject>> waits;
    QVector<Finish> finishes;
    double credits = 0;
    qint64 tokens = 0;
    bool negative_usage = false;
};

/// A cloud plan executor over a fake backend; `backend` stays owned by it.
struct PlanFixture {
    FakeBackend* backend = new FakeBackend;
    CloudPlanExecutor ex{std::unique_ptr<ICloudBackend>(backend)};
    Recorder rec{&ex};

    /// Start `run` and answer its plan turn with the two-step plan.
    void start_with_plan(const TaskRun& run) {
        ex.start(run, {});
        pump();
        QVERIFY(backend->last());
        backend->last()->answer(QString::fromUtf8(kTwoStepPlan));
        pump();
    }
};

} // namespace

class TestAgentTaskExecutors : public QObject {
    Q_OBJECT

  private slots:
    // ── RunPlanPrompts ───────────────────────────────────────────────────────
    void parse_well_formed();
    void parse_fenced_with_prose();
    void parse_bare_json_in_prose();
    void parse_trailing_commas_and_smart_quotes();
    void parse_alternative_shapes_dedupe_and_empty_steps();
    void parse_oversized_is_capped();
    void parse_empty_and_garbage_fall_back_to_one_step();
    void prompts_contain_goal_step_and_deliverable();
    void prompts_stay_bounded();
    void step_request_is_deterministic();

    // ── CloudPlanExecutor ────────────────────────────────────────────────────
    void plan_steps_synthesis();
    void plan_unreadable_runs_one_step();
    void creates_conversation_when_missing();
    void pause_at_step_boundary_then_resume();
    void budget_stops_before_a_turn();
    void cancel_mid_step_keeps_recovered_result();
    void cancel_twice_stops_waiting();
    void failed_step_retried_once_then_run_fails();
    void non_retryable_failure_fails_at_once();
    void resume_from_step_k_adopts_then_continues();
    void question_waits_and_answer_resumes();
    void answer_after_restart_rebuilds_the_run();
    void waits_for_stream_close_before_next_turn();

    // ── LocalAgenticExecutor ─────────────────────────────────────────────────
    void local_payload_helpers();
    void local_happy_path();
    void local_question_and_answer();
    void local_pause_before_task_id_then_resume();
    void local_cancel_and_cancel_again();
    void local_budget_stop();
    void local_error_event();
    void local_not_ready_fails_fast();
    void local_sync_error_is_attributed();
    void local_replan_keeps_finished_steps();
    void local_answer_after_restart();
};

// ── RunPlanPrompts ───────────────────────────────────────────────────────────

void TestAgentTaskExecutors::parse_well_formed() {
    QString err;
    const auto steps = rpp::parse_plan_reply(
        R"({"steps":[{"title":"A","instruction":"do a","deliverable":"table"},{"title":"B","instruction":"do b","deliverable":"chart"}]})",
        QStringLiteral("goal"), 8, &err);
    QVERIFY(err.isEmpty());
    QCOMPARE(steps.size(), 2);
    QCOMPARE(steps[0].idx, 0);
    QCOMPARE(steps[1].idx, 1);
    QCOMPARE(steps[0].title, QStringLiteral("A"));
    QCOMPARE(steps[0].instruction, QStringLiteral("do a"));
    QCOMPARE(steps[1].deliverable, QStringLiteral("chart"));
}

void TestAgentTaskExecutors::parse_fenced_with_prose() {
    QString err;
    int proposed = 0;
    const auto steps = rpp::parse_plan_reply(QString::fromUtf8(kTwoStepPlan), QStringLiteral("goal"), 8, &err, &proposed);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(steps.size(), 2);
    QCOMPARE(proposed, 2);
    QCOMPARE(steps[1].title, QStringLiteral("Compare"));
    // An unlabelled fence works as well.
    const auto plain = rpp::parse_plan_reply(QStringLiteral("Plan:\n```\n{\"steps\":[{\"title\":\"X\"}]}\n```"),
                                             QStringLiteral("goal"), 8, &err);
    QVERIFY(err.isEmpty());
    QCOMPARE(plain.size(), 1);
    QCOMPARE(plain[0].instruction, QStringLiteral("X")); // no instruction: the title
}

void TestAgentTaskExecutors::parse_bare_json_in_prose() {
    QString err;
    const auto steps = rpp::parse_plan_reply(
        QStringLiteral("I would do this [roughly]: {\"steps\":[{\"title\":\"Fetch {data}\",\"instruction\":\"use \\\"quote\\\"\"}]} — ok?"),
        QStringLiteral("goal"), 8, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(steps.size(), 1);
    QCOMPARE(steps[0].title, QStringLiteral("Fetch {data}")); // braces inside strings do not break the scan
    QCOMPARE(steps[0].instruction, QStringLiteral("use \"quote\""));
}

void TestAgentTaskExecutors::parse_trailing_commas_and_smart_quotes() {
    QString err;
    const auto steps = rpp::parse_plan_reply(
        QStringLiteral("```json\n{\"steps\":[{\"title\":\"A\",\"instruction\":\"x\",},{\"title\":\"B\",},],}\n```"),
        QStringLiteral("goal"), 8, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(steps.size(), 2);
    const auto smart = rpp::parse_plan_reply(QString::fromUtf8("{\xE2\x80\x9Csteps\xE2\x80\x9D:[{\xE2\x80\x9Ctitle\xE2\x80\x9D:"
                                                               "\xE2\x80\x9CGather\xE2\x80\x9D}]}"),
                                             QStringLiteral("goal"), 8, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(smart.size(), 1);
    QCOMPARE(smart[0].title, QStringLiteral("Gather"));
}

void TestAgentTaskExecutors::parse_alternative_shapes_dedupe_and_empty_steps() {
    QString err;
    // name / description keys, string steps, an empty step and a duplicate.
    const auto steps = rpp::parse_plan_reply(
        R"({"plan":{"steps":[{"name":"Quote","description":"Get the quote. Then stop."},{},{"title":""},
            "Compare the peers",{"name":"Quote","description":"Get the quote. Then stop."}]}})",
        QStringLiteral("goal"), 8, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(steps.size(), 2);
    QCOMPARE(steps[0].title, QStringLiteral("Quote"));
    QCOMPARE(steps[0].instruction, QStringLiteral("Get the quote. Then stop."));
    QCOMPARE(steps[1].title, QStringLiteral("Compare the peers"));
    QCOMPARE(steps[1].idx, 1);
    // A top-level array is a plan too.
    const auto arr = rpp::parse_plan_reply(R"([{"title":"Only"}])", QStringLiteral("goal"), 8, &err);
    QCOMPARE(arr.size(), 1);
    // Long fields are clipped.
    const QString long_title = QString(500, QLatin1Char('t'));
    const auto clipped = rpp::parse_plan_reply(QStringLiteral("{\"steps\":[{\"title\":\"%1\"}]}").arg(long_title),
                                               QStringLiteral("goal"), 8, &err);
    QVERIFY(clipped[0].title.size() <= rpp::kTitleChars);
}

void TestAgentTaskExecutors::parse_oversized_is_capped() {
    QJsonArray arr;
    for (int i = 0; i < 20; ++i)
        arr.append(QJsonObject{{"title", QStringLiteral("Step %1").arg(i)}, {"instruction", QStringLiteral("do %1").arg(i)}});
    const QString reply = QString::fromUtf8(QJsonDocument(QJsonObject{{"steps", arr}}).toJson());
    QString err;
    int proposed = 0;
    const auto steps = rpp::parse_plan_reply(reply, QStringLiteral("goal"), 8, &err, &proposed);
    QVERIFY(err.isEmpty());
    QCOMPARE(steps.size(), 8);
    QCOMPARE(proposed, 20);
    QCOMPARE(steps.last().idx, 7);
    QCOMPARE(steps.last().title, QStringLiteral("Step 7"));
    // 0 = no cap
    QCOMPARE(rpp::parse_plan_reply(reply, QStringLiteral("goal"), 0).size(), 20);
}

void TestAgentTaskExecutors::parse_empty_and_garbage_fall_back_to_one_step() {
    const QString goal = QStringLiteral("Analyse AAPL");
    for (const QString& reply : {QString(), QStringLiteral("I cannot plan this."), QStringLiteral("{\"foo\":1}"),
                                 QStringLiteral("{\"steps\":[]}"), QStringLiteral("{\"steps\":[{},{\"title\":\" \"}]}"),
                                 QStringLiteral("```json\n{\"steps\": [ {\"title\": \n```")}) {
        QString err;
        int proposed = -1;
        const auto steps = rpp::parse_plan_reply(reply, goal, 8, &err, &proposed);
        QVERIFY2(!err.isEmpty(), qPrintable(reply));
        QCOMPARE(steps.size(), 1);
        QCOMPARE(steps[0].instruction, goal);
        QVERIFY(!steps[0].title.isEmpty());
        QCOMPARE(proposed, 0);
    }
}

void TestAgentTaskExecutors::prompts_contain_goal_step_and_deliverable() {
    const QJsonObject ctx = obj(R"({"symbol":"AAPL","answer_to_pause":true,"portfolio":{"id":3}})");
    const QString plan = rpp::plan_request(QStringLiteral("Review AAPL valuation"), ctx, 5);
    QVERIFY(plan.contains(QStringLiteral("Review AAPL valuation")));
    QVERIFY(plan.contains(QStringLiteral("At most 5 steps")));
    QVERIFY(plan.contains(QStringLiteral("- symbol: AAPL")));
    QVERIFY(!plan.contains(QStringLiteral("answer_to_pause"))); // internal keys stay out
    QVERIFY(plan.contains(QStringLiteral("```json")));
    QVERIFY(rpp::plan_request(QStringLiteral("g"), {}, 0).contains(QStringLiteral("At most %1 steps").arg(rpp::kPlanHardMaxSteps)));

    TaskRun run = cloud_run(QStringLiteral("r"));
    QVector<RunStep> steps(3);
    for (int i = 0; i < 3; ++i) {
        steps[i].idx = i;
        steps[i].title = QStringLiteral("Title %1").arg(i + 1);
        steps[i].instruction = QStringLiteral("Instruction %1").arg(i + 1);
        steps[i].deliverable = QStringLiteral("Deliverable %1").arg(i + 1);
    }
    steps[0].status = StepStatus::Done;
    steps[0].summary = QStringLiteral("First result");
    const QString step = rpp::step_request(run, steps[1], 3, steps);
    QVERIFY(step.contains(QStringLiteral("Step 2 of 3")));
    QVERIFY(step.contains(run.prompt));
    QVERIFY(step.contains(QStringLiteral("Title 2")));
    QVERIFY(step.contains(QStringLiteral("Instruction 2")));
    QVERIFY(step.contains(QStringLiteral("Deliverable 2")));
    QVERIFY(step.contains(QStringLiteral("First result")));
    QVERIFY(!step.contains(QStringLiteral("Instruction 3"))); // later steps are not in this turn

    steps[1].status = StepStatus::Done;
    steps[1].summary = QStringLiteral("Second result");
    steps[2].status = StepStatus::Failed;
    const QString synth = rpp::synthesis_request(run, steps);
    QVERIFY(synth.contains(run.prompt));
    QVERIFY(synth.contains(QStringLiteral("First result")));
    QVERIFY(synth.contains(QStringLiteral("Second result")));
    QVERIFY(synth.contains(QStringLiteral("Title 3")));
    QVERIFY(synth.contains(QStringLiteral("did not complete")));
}

void TestAgentTaskExecutors::prompts_stay_bounded() {
    TaskRun run = cloud_run(QStringLiteral("r"));
    run.prompt = QString(kRunPromptMaxChars, QLatin1Char('g'));
    QVector<RunStep> steps(50);
    for (int i = 0; i < 50; ++i) {
        steps[i].idx = i;
        steps[i].title = QString(300, QLatin1Char('T'));
        steps[i].instruction = QString(5000, QLatin1Char('i'));
        steps[i].deliverable = QString(3000, QLatin1Char('d'));
        steps[i].status = i < 49 ? StepStatus::Done : StepStatus::Pending;
        steps[i].summary = QString(10000, QLatin1Char('s'));
    }
    const QString step = rpp::step_request(run, steps[49], 50, steps);
    QVERIFY2(step.size() < kChatMessageMaxChars, qPrintable(QString::number(step.size())));
    QVERIFY(step.contains(QStringLiteral("earlier step(s) not repeated")));
    QVERIFY(step.contains(QStringLiteral("49. "))); // the newest earlier step is listed
    steps[49].status = StepStatus::Done;
    const QString synth = rpp::synthesis_request(run, steps);
    QVERIFY2(synth.size() < kChatMessageMaxChars, qPrintable(QString::number(synth.size())));
    QVERIFY(synth.contains(QStringLiteral("more step(s)")));
    // The single-step fallback restates the goal, bounded too.
    RunStep whole;
    whole.instruction = run.prompt;
    QVERIFY(rpp::step_request(run, whole, 1).size() < kChatMessageMaxChars);
}

void TestAgentTaskExecutors::step_request_is_deterministic() {
    TaskRun run = cloud_run(QStringLiteral("r"));
    const auto steps = rpp::parse_plan_reply(QString::fromUtf8(kTwoStepPlan), run.prompt, 8);
    QCOMPARE(rpp::step_request(run, steps[0], 2, steps), rpp::step_request(run, steps[0], 2, steps));
    RunStep whole;
    whole.instruction = run.prompt;
    QVERIFY(rpp::step_request(run, whole, 1).contains(QStringLiteral("Carry out the whole task")));
}

// ── CloudPlanExecutor ────────────────────────────────────────────────────────

void TestAgentTaskExecutors::plan_steps_synthesis() {
    PlanFixture f;
    QCOMPARE(f.ex.engine_id(), QString::fromLatin1(kExecutorCloudPlan));
    QVERIFY(f.ex.property("available").toBool()); // the Chat Mode composer offers the engine
    const TaskRun run = cloud_run(QStringLiteral("run-1"));
    f.ex.start(run, {});
    pump();
    QCOMPARE(f.backend->turns.size(), 1);
    FakeTurn* plan = f.backend->turn(0);
    QCOMPARE(plan->requests.size(), 1);
    QVERIFY(plan->requests[0].text.contains(QStringLiteral("Plan the following")));
    QVERIFY(plan->requests[0].text.contains(QStringLiteral("At most 8 steps")));
    QCOMPARE(plan->requests[0].session_uuid, QStringLiteral("sess-1"));
    QVERIFY(!plan->requests[0].check_stored_first);
    QVERIFY(f.rec.has_text(RunEventKind::Note, QStringLiteral("Estimated cost")));
    QVERIFY(f.ex.can_pause(run.id));
    plan->text(QStringLiteral("{\"steps\":")); // the plan's JSON is not shown as reply text
    plan->answer(QString::fromUtf8(kTwoStepPlan));
    QCOMPARE(f.rec.plans.size(), 1);
    QCOMPARE(f.rec.plans[0].size(), 2);
    QCOMPARE(f.rec.plans[0][0].run_id, run.id);
    QVERIFY(f.rec.last(RunEventKind::Plan));
    QVERIFY(f.rec.text.isEmpty());

    pump();
    QCOMPARE(f.backend->turns.size(), 2);
    QVERIFY(!f.backend->turn(0) || f.backend->turn(0)->released); // the plan turn is let go
    FakeTurn* s1 = f.backend->turn(1);
    QCOMPARE(s1->requests[0].step, 0);
    QVERIFY(s1->requests[0].text.contains(QStringLiteral("Step 1 of 2")));
    QVERIFY(s1->requests[0].text.contains(QStringLiteral("Get the AAPL quote and ratios")));
    QVERIFY(s1->requests[0].text.contains(QStringLiteral("a table")));
    QCOMPARE(f.rec.step_count(0, StepStatus::Running), 1);
    s1->text(QStringLiteral("partial "));
    s1->answer(QStringLiteral("Result A"));
    QCOMPARE(f.rec.text.size(), 1);
    QCOMPARE(f.rec.text[0].first, 0);
    QCOMPARE(f.rec.steps.last().status, StepStatus::Done);
    QCOMPARE(f.rec.steps.last().summary, QStringLiteral("Result A"));

    pump();
    FakeTurn* s2 = f.backend->turn(2);
    QVERIFY(s2);
    QVERIFY(s2->requests[0].text.contains(QStringLiteral("Step 2 of 2")));
    QVERIFY(s2->requests[0].text.contains(QStringLiteral("Result A"))); // what step 1 found
    s2->answer(QStringLiteral("Result B"));
    pump();
    FakeTurn* synth = f.backend->turn(3);
    QVERIFY(synth);
    QVERIFY(synth->requests[0].text.contains(QStringLiteral("All 2 step(s)")));
    QVERIFY(synth->requests[0].text.contains(QStringLiteral("Result B")));
    QVERIFY(!f.ex.can_pause(run.id)); // only the report is left
    QVERIFY(f.rec.finishes.isEmpty());
    synth->answer(QStringLiteral("Final report"));
    QCOMPARE(f.rec.finishes.size(), 1);
    QCOMPARE(f.rec.finishes[0].status, RunStatus::Completed);
    QCOMPARE(f.rec.finishes[0].final_text, QStringLiteral("Final report"));
    QCOMPARE(f.rec.credits, 20.0);
    QCOMPARE(f.rec.tokens, qint64(400));
    QVERIFY(!f.rec.negative_usage);
    pump();
    QCOMPARE(f.backend->turns.size(), 4); // no fifth turn
    QCOMPARE(f.rec.finishes.size(), 1);
    QVERIFY(!f.ex.can_pause(run.id));
}

void TestAgentTaskExecutors::plan_unreadable_runs_one_step() {
    PlanFixture f;
    const TaskRun run = cloud_run(QStringLiteral("run-2"));
    f.ex.start(run, {});
    pump();
    f.backend->last()->answer(QStringLiteral("I would rather just answer."));
    QCOMPARE(f.rec.plans.size(), 1);
    QCOMPARE(f.rec.plans[0].size(), 1);
    QCOMPARE(f.rec.plans[0][0].instruction, run.prompt);
    QVERIFY(f.rec.has_text(RunEventKind::Note, QStringLiteral("could not be read")));
    pump();
    QVERIFY(f.backend->last()->requests[0].text.contains(QStringLiteral("Carry out the whole task")));
}

void TestAgentTaskExecutors::creates_conversation_when_missing() {
    PlanFixture f;
    TaskRun run = cloud_run(QStringLiteral("run-3"));
    run.session_uuid.clear();
    f.ex.start(run, {});
    QVERIFY(f.backend->turns.isEmpty());
    pump();
    QCOMPARE(f.backend->sessions, 1);
    bool bound = false;
    for (const RunEvent& e : f.rec.events)
        if (e.kind == RunEventKind::Status && e.data.value("session_uuid").toString() == QStringLiteral("sess-new"))
            bound = true;
    QVERIFY(bound);
    QCOMPARE(f.backend->turns.size(), 1);
    QCOMPARE(f.backend->last()->requests[0].session_uuid, QStringLiteral("sess-new"));

    // A conversation that cannot be created fails the run once, with a code.
    PlanFixture g;
    g.backend->session_ok = false;
    g.ex.start(run, {});
    pump();
    QCOMPARE(g.rec.finishes.size(), 1);
    QCOMPARE(g.rec.finishes[0].status, RunStatus::Failed);
    QCOMPARE(g.rec.last(RunEventKind::Error)->data.value("code").toString(), QStringLiteral("session_create"));
}

void TestAgentTaskExecutors::pause_at_step_boundary_then_resume() {
    PlanFixture f;
    TaskRun run = cloud_run(QStringLiteral("run-4"));
    f.start_with_plan(run);
    FakeTurn* s1 = f.backend->last();
    QCOMPARE(s1->requests[0].step, 0);
    QVERIFY(f.ex.can_pause(run.id));
    f.ex.request_pause(run.id);
    QVERIFY(f.rec.waits.isEmpty()); // the step in flight finishes first
    s1->answer(QStringLiteral("Result A"));
    pump();
    QCOMPARE(f.rec.waits.size(), 1);
    QCOMPARE(f.rec.waits[0].first, RunStatus::Paused);
    QCOMPARE(f.backend->turns.size(), 2); // step 2 did not start
    QVERIFY(f.rec.finishes.isEmpty());
    QVERIFY(!f.ex.can_pause(run.id));

    // resume_run(): start() again with the steps as recorded.
    QVector<RunStep> steps = f.rec.plans.last();
    steps[0].status = StepStatus::Done;
    steps[0].summary = QStringLiteral("Result A");
    steps[0].attempts = 1;
    run.started_at_ms -= 60 * 1000;
    run.credits_spent = f.rec.credits;
    f.ex.start(run, steps);
    pump();
    QCOMPARE(f.backend->turns.size(), 3);
    FakeTurn* s2 = f.backend->last();
    QCOMPARE(s2->requests[0].step, 1);
    QVERIFY(!s2->requests[0].check_stored_first); // step 2 never ran
    QVERIFY(s2->requests[0].text.contains(QStringLiteral("Step 2 of 2")));
    // A pause is not a restart: the final report is sent, never looked up first
    // (no "answer stored while the terminal was closed" after a plain pause).
    s2->answer(QStringLiteral("Result B"));
    pump();
    FakeTurn* synth = f.backend->last();
    QCOMPARE(synth->requests[0].step, -1);
    QVERIFY(!synth->requests[0].check_stored_first);
}

void TestAgentTaskExecutors::budget_stops_before_a_turn() {
    PlanFixture f;
    TaskRun run = cloud_run(QStringLiteral("run-5"));
    run.budget = {10, 0, 0}; // pays for the plan and one step, not for the report
    f.ex.start(run, {});
    pump();
    QVERIFY(f.backend->last()->requests[0].text.contains(QStringLiteral("At most 1 steps")));
    f.backend->last()->answer(QString::fromUtf8(kTwoStepPlan)); // 2 proposed, 1 allowed
    QCOMPARE(f.rec.plans.last().size(), 1);
    QVERIFY(f.rec.has_text(RunEventKind::Note, QStringLiteral("proposed 2 steps")));
    pump();
    f.backend->last()->answer(QStringLiteral("Result A"));
    pump();
    QCOMPARE(f.backend->turns.size(), 2); // no synthesis turn
    QCOMPARE(f.rec.finishes.size(), 1);
    QCOMPARE(f.rec.finishes[0].status, RunStatus::BudgetStopped);
    QVERIFY(f.rec.finishes[0].final_text.contains(QStringLiteral("Result A"))); // partial results kept
    QVERIFY(f.rec.finishes[0].error.contains(QStringLiteral("Credit budget")));

    // A time budget already spent stops before the first step.
    PlanFixture g;
    TaskRun late = cloud_run(QStringLiteral("run-5b"));
    late.budget = {0, 0, 1};
    late.started_at_ms = QDateTime::currentMSecsSinceEpoch() - 2 * 60 * 1000;
    QVector<RunStep> planned = rpp::parse_plan_reply(QString::fromUtf8(kTwoStepPlan), late.prompt, 8);
    g.ex.start(late, planned);
    pump();
    QVERIFY(g.backend->turns.isEmpty());
    QCOMPARE(g.rec.finishes.size(), 1);
    QCOMPARE(g.rec.finishes[0].status, RunStatus::BudgetStopped);
}

void TestAgentTaskExecutors::cancel_mid_step_keeps_recovered_result() {
    PlanFixture f;
    const TaskRun run = cloud_run(QStringLiteral("run-6"));
    f.start_with_plan(run);
    FakeTurn* s1 = f.backend->last();
    f.ex.cancel(run.id);
    QCOMPARE(s1->detaches, 1); // detached: the server's answer is recovered first
    QVERIFY(f.rec.finishes.isEmpty());
    QVERIFY(!f.ex.can_pause(run.id));
    s1->adopt(QStringLiteral("Recovered A"));
    pump();
    QCOMPARE(f.rec.finishes.size(), 1);
    QCOMPARE(f.rec.finishes[0].status, RunStatus::Cancelled);
    QVERIFY(f.rec.finishes[0].final_text.contains(QStringLiteral("Recovered A")));
    QCOMPARE(f.backend->turns.size(), 2); // step 2 never started
}

void TestAgentTaskExecutors::cancel_twice_stops_waiting() {
    PlanFixture f;
    const TaskRun run = cloud_run(QStringLiteral("run-7"));
    f.start_with_plan(run);
    FakeTurn* s1 = f.backend->last();
    f.ex.cancel(run.id);
    f.ex.cancel(run.id);
    QCOMPARE(s1->detaches, 2);
    QCOMPARE(f.rec.finishes.size(), 1);
    QCOMPARE(f.rec.finishes[0].status, RunStatus::Cancelled);
    pump();
    f.ex.cancel(run.id); // nothing left: no second finished()
    pump();
    QCOMPARE(f.rec.finishes.size(), 1);

    // Between turns (no turn in flight) a cancel ends the run at once.
    PlanFixture g;
    const TaskRun r2 = cloud_run(QStringLiteral("run-7b"));
    g.ex.start(r2, {});
    pump();
    g.backend->last()->answer(QString::fromUtf8(kTwoStepPlan));
    g.ex.cancel(r2.id); // the next step is only scheduled
    QCOMPARE(g.rec.finishes.size(), 1);
    pump();
    QCOMPARE(g.backend->turns.size(), 1);
}

void TestAgentTaskExecutors::failed_step_retried_once_then_run_fails() {
    PlanFixture f;
    const TaskRun run = cloud_run(QStringLiteral("run-8"));
    f.start_with_plan(run);
    FakeTurn* s1 = f.backend->last();
    const QString text = s1->requests[0].text;
    s1->fail(QStringLiteral("recovery_timeout"), QStringLiteral("No stored answer arrived within 10 minutes."));
    QCOMPARE(f.rec.step_count(0, StepStatus::Failed), 1);
    QVERIFY(f.rec.has_text(RunEventKind::Note, QStringLiteral("trying it once more")));
    QVERIFY(f.rec.finishes.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(f.backend->turns.size(), 3, 10000);
    FakeTurn* again = f.backend->last();
    QCOMPARE(again->requests[0].text, text); // the same step, sent again
    QVERIFY(!again->requests[0].check_stored_first);
    QCOMPARE(f.rec.step_count(0, StepStatus::Running), 2);
    again->fail(QStringLiteral("recovery_timeout"), QStringLiteral("No stored answer arrived within 10 minutes."));
    QCOMPARE(f.rec.finishes.size(), 1);
    QCOMPARE(f.rec.finishes[0].status, RunStatus::Failed);
    QVERIFY(f.rec.finishes[0].error.contains(QStringLiteral("Step 1")));
    QCOMPARE(f.rec.last(RunEventKind::Error)->data.value("code").toString(), QStringLiteral("recovery_timeout"));
    pump();
    QCOMPARE(f.backend->turns.size(), 3);
}

void TestAgentTaskExecutors::non_retryable_failure_fails_at_once() {
    PlanFixture f;
    const TaskRun run = cloud_run(QStringLiteral("run-9"));
    f.start_with_plan(run);
    f.backend->last()->answer(QStringLiteral("Result A"));
    pump();
    f.backend->last()->fail(QStringLiteral("insufficient_credits"), QStringLiteral("You need 5 credits."));
    QCOMPARE(f.rec.finishes.size(), 1);
    QCOMPARE(f.rec.finishes[0].status, RunStatus::Failed);
    QVERIFY(f.rec.finishes[0].final_text.contains(QStringLiteral("Result A")));
    pump();
    QCOMPARE(f.backend->turns.size(), 3);
}

void TestAgentTaskExecutors::resume_from_step_k_adopts_then_continues() {
    PlanFixture f;
    TaskRun run = cloud_run(QStringLiteral("run-10"));
    run.started_at_ms -= 10 * 60 * 1000; // started before the app quit
    run.resume_after_restart = true;     // resume_run() of an Interrupted run
    QVector<RunStep> steps(3);
    for (int i = 0; i < 3; ++i) {
        steps[i].run_id = run.id;
        steps[i].idx = i;
        steps[i].title = QStringLiteral("S%1").arg(i + 1);
        steps[i].instruction = QStringLiteral("do %1").arg(i + 1);
    }
    steps[0].status = StepStatus::Done;
    steps[0].summary = QStringLiteral("Summary one");
    steps[0].attempts = 1;
    steps[1].attempts = 1; // interrupted mid-step (the store shows it pending again)
    f.ex.start(run, steps);
    pump();
    QCOMPARE(f.backend->turns.size(), 1);
    FakeTurn* s2 = f.backend->last();
    QCOMPARE(s2->requests[0].step, 1);
    QVERIFY(s2->requests[0].check_stored_first); // adopt the stored answer, else re-send
    QVERIFY(s2->requests[0].text.contains(QStringLiteral("Summary one")));
    QCOMPARE(f.rec.step_count(0, StepStatus::Running), 0); // step 1 is never redone
    s2->adopt(QStringLiteral("Stored two"));
    pump();
    FakeTurn* s3 = f.backend->last();
    QCOMPARE(s3->requests[0].step, 2);
    QVERIFY(!s3->requests[0].check_stored_first);
    s3->answer(QStringLiteral("Three"));
    pump();
    FakeTurn* synth = f.backend->last();
    QCOMPARE(synth->requests[0].step, -1);
    QVERIFY(synth->requests[0].check_stored_first); // a resumed run may already have its report stored
    synth->answer(QStringLiteral("Report"));
    QCOMPARE(f.rec.finishes.size(), 1);
    QCOMPARE(f.rec.finishes[0].status, RunStatus::Completed);
    for (int i = 0; i < f.backend->turns.size(); ++i)
        if (FakeTurn* t = f.backend->turn(i))
            for (const CloudTurnRequest& r : t->requests)
                QVERIFY(r.step != 0);
}

void TestAgentTaskExecutors::question_waits_and_answer_resumes() {
    PlanFixture f;
    const TaskRun run = cloud_run(QStringLiteral("run-11"));
    f.start_with_plan(run);
    f.backend->last()->ask(QStringLiteral("Which account?"));
    QCOMPARE(f.rec.waits.size(), 1);
    QCOMPARE(f.rec.waits[0].first, RunStatus::WaitingInput);
    QCOMPARE(f.rec.waits[0].second.value("question").toString(), QStringLiteral("Which account?"));
    QCOMPARE(f.rec.waits[0].second.value("step").toInt(), 0);
    QVERIFY(!f.ex.can_pause(run.id));
    pump();
    QCOMPARE(f.backend->turns.size(), 2); // nothing moves while it waits
    f.ex.answer(run.id, QStringLiteral("Growth"));
    QCOMPARE(f.backend->turns.size(), 3);
    const CloudTurnRequest r = f.backend->last()->requests[0];
    QVERIFY(r.as_answer);
    QCOMPARE(r.text, QStringLiteral("Growth"));
    QCOMPARE(r.step, 0);
    f.backend->last()->answer(QStringLiteral("Result with Growth"));
    QCOMPARE(f.rec.steps.last().summary, QStringLiteral("Result with Growth"));
    pump();
    QCOMPARE(f.backend->last()->requests[0].step, 1);
}

void TestAgentTaskExecutors::answer_after_restart_rebuilds_the_run() {
    PlanFixture f;
    TaskRun run = cloud_run(QStringLiteral("run-12"));
    run.status = RunStatus::WaitingInput;
    QVector<RunStep> steps = rpp::parse_plan_reply(QString::fromUtf8(kTwoStepPlan), run.prompt, 8);
    steps[0].status = StepStatus::Done;
    steps[0].summary = QStringLiteral("A");
    steps[1].status = StepStatus::Running;
    steps[1].attempts = 1;
    f.backend->runs.insert(run.id, run);
    f.backend->steps.insert(run.id, steps);
    f.ex.answer(run.id, QStringLiteral("yes"));
    QCOMPARE(f.backend->turns.size(), 1);
    QVERIFY(f.backend->last()->requests[0].as_answer);
    QCOMPARE(f.backend->last()->requests[0].step, 1);
    f.backend->last()->answer(QStringLiteral("B"));
    pump();
    QCOMPARE(f.backend->last()->requests[0].step, -1); // the report
    // An unknown run fails once.
    PlanFixture g;
    g.ex.answer(QStringLiteral("nope"), QStringLiteral("x"));
    QCOMPARE(g.rec.finishes.size(), 1);
    QCOMPARE(g.rec.finishes[0].status, RunStatus::Failed);
}

void TestAgentTaskExecutors::waits_for_stream_close_before_next_turn() {
    PlanFixture f;
    const TaskRun run = cloud_run(QStringLiteral("run-13"));
    f.start_with_plan(run);
    FakeTurn* s1 = f.backend->last();
    s1->open = true; // deep mode: a self-check follows "finish"
    s1->answer(QStringLiteral("Result A"));
    pump();
    QCOMPARE(f.backend->turns.size(), 2); // the next step waits for the stream to end
    s1->close();
    pump();
    QCOMPARE(f.backend->turns.size(), 3);
}

// ── LocalAgenticExecutor ─────────────────────────────────────────────────────

void TestAgentTaskExecutors::local_payload_helpers() {
    const QVector<RunStep> steps = local_agent_plan_steps(obj(kPlanReady).value("plan").toObject());
    QCOMPARE(steps.size(), 2);
    QCOMPARE(steps[0].title, QStringLiteral("Gather data"));
    QCOMPARE(steps[0].instruction, QStringLiteral("Get the AAPL quote"));
    QCOMPARE(steps[1].idx, 1);
    // The trivial fallback plan (task_state.py _fallback_step): name = the query.
    const auto trivial = local_agent_plan_steps(obj(R"({"name":"Trivial Plan","steps":[{"id":"step_1","name":"Review AAPL","config":{"query":"Review AAPL"}}]})"));
    QCOMPARE(trivial.size(), 1);
    QCOMPARE(trivial[0].title, QStringLiteral("Review AAPL"));

    LocalStepResult ok = local_agent_step_result(obj(R"({"step":1,"response":"fine"})"));
    QVERIFY(!ok.failed);
    QCOMPARE(ok.text, QStringLiteral("fine"));
    LocalStepResult bad = local_agent_step_result(obj(R"({"step":1,"error":"tool crashed"})"));
    QVERIFY(bad.failed);
    QCOMPARE(bad.text, QStringLiteral("tool crashed"));
    QVERIFY(local_agent_step_result(obj(R"({"success":false})")).failed);

    const QString steps_text = local_agent_budget_text(QStringLiteral("steps"), obj(R"({"steps_used":6,"max_steps":6})"));
    QVERIFY(steps_text.contains(QStringLiteral("step limit")));
    QVERIFY(steps_text.contains(QStringLiteral("6 of 6")));
    QVERIFY(local_agent_budget_text(QStringLiteral("wall_clock"), {}).contains(QStringLiteral("time limit")));
    QCOMPARE(local_agent_error_code(QStringLiteral("Python not available")), QStringLiteral("python_not_ready"));
    QCOMPARE(local_agent_error_code(QStringLiteral("Subprocess exited without terminal event")),
             QStringLiteral("local_agent_exited"));
    QCOMPARE(local_agent_error_code(QStringLiteral("Process error: no such file")), QStringLiteral("local_agent_spawn"));
    QCOMPARE(local_agent_error_code(QStringLiteral("KeyError: x")), QStringLiteral("local_agent_error"));
    QVERIFY(local_agent_error_text(QStringLiteral("KeyError: x")).contains(QStringLiteral("KeyError: x")));
}

void TestAgentTaskExecutors::local_happy_path() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    QCOMPARE(ex.engine_id(), QString::fromLatin1(kExecutorLocal));
    QVERIFY(ex.property("available").toBool());
    const TaskRun run = local_run(QStringLiteral("loc-1"));
    ex.start(run, {});
    QCOMPARE(engine->calls, QStringList{QStringLiteral("start:Review AAPL")});
    QCOMPARE(engine->last_config.value("allow_destructive_tools").toBool(true), false);
    QCOMPARE(engine->last_config.value("budget").toObject().value("max_steps").toInt(), 6);
    QCOMPARE(engine->last_config.value("budget").toObject().value("max_wall_s").toInt(), 30 * 60);
    QVERIFY(rec.has_text(RunEventKind::Status, QStringLiteral("openai · gpt-4o")));
    QVERIFY(ex.can_pause(run.id));

    engine->emit_event(QStringLiteral("t1"), R"({"kind":"task_started","query":"Review AAPL"})", QStringLiteral("req-1"));
    bool bound = false;
    for (const RunEvent& e : rec.events)
        if (e.kind == RunEventKind::Status && e.data.value("remote_task_id").toString() == QStringLiteral("t1"))
            bound = true;
    QVERIFY(bound);
    engine->emit_event(QStringLiteral("t1"), kPlanReady);
    QCOMPARE(rec.plans.size(), 1);
    QCOMPARE(rec.plans[0].size(), 2);
    QCOMPARE(rec.plans[0][1].title, QStringLiteral("Analyse"));
    QVERIFY(rec.last(RunEventKind::Plan));
    engine->emit_event(QStringLiteral("t1"), kStepStart0);
    QCOMPARE(rec.steps.last().idx, 0);
    QCOMPARE(rec.steps.last().status, StepStatus::Running);
    engine->emit_event(QStringLiteral("t1"), kStepEnd0);
    QCOMPARE(rec.steps.last().status, StepStatus::Done);
    QCOMPARE(rec.steps.last().summary, QStringLiteral("AAPL trades at 231.4"));
    QCOMPARE(rec.tokens, qint64(1200));
    const RunEvent* turn = rec.last(RunEventKind::Usage);
    QVERIFY(turn && turn->data.value("turn_end").toBool());
    engine->emit_event(QStringLiteral("t1"), kReflection0);
    QVERIFY(rec.last(RunEventKind::Note)->data.value("verbose").toBool()); // a routine self-check
    engine->emit_event(QStringLiteral("t1"), kStepStart1);
    engine->emit_event(QStringLiteral("t1"), kStepEnd1);
    QCOMPARE(rec.tokens, qint64(2000)); // deltas of the per-process total, never summed twice
    QCOMPARE(rec.credits, 0.0);
    engine->emit_event(QStringLiteral("t1"), kDone);
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::Completed);
    QCOMPARE(rec.finishes[0].final_text, QStringLiteral("Final answer: hold AAPL"));
    QVERIFY(!rec.negative_usage);
    pump();
    engine->emit_event(QStringLiteral("t1"), kDone); // late duplicates are ignored
    QCOMPARE(rec.finishes.size(), 1);
    // Another panel's task is not this run's.
    engine->emit_event(QStringLiteral("t9"), kDone);
    QCOMPARE(rec.finishes.size(), 1);
}

void TestAgentTaskExecutors::local_question_and_answer() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    const TaskRun run = local_run(QStringLiteral("loc-2"));
    ex.start(run, {});
    engine->emit_event(QStringLiteral("t2"), R"({"kind":"task_started","query":"Review AAPL"})", QStringLiteral("req-1"));
    engine->emit_event(QStringLiteral("t2"), kPlanReady);
    engine->emit_event(QStringLiteral("t2"), kStepStart0);
    engine->emit_event(QStringLiteral("t2"), kStepEnd0);
    engine->emit_event(QStringLiteral("t2"),
                       R"({"kind":"question","step_index":0,"question":"Which portfolio?","reason":"two match"})");
    QVERIFY(rec.last(RunEventKind::Question));
    QCOMPARE(rec.waits.size(), 1);
    QCOMPARE(rec.waits[0].first, RunStatus::WaitingInput);
    QCOMPARE(rec.waits[0].second.value("question").toString(), QStringLiteral("Which portfolio?"));
    QVERIFY(!ex.can_pause(run.id));
    ex.answer(run.id, QStringLiteral("Growth"));
    QCOMPARE(engine->calls.last(), QStringLiteral("reply:t2:Growth"));
    // The reply process resumes from the checkpoint: step 1 stays done.
    engine->emit_event(QStringLiteral("t2"), R"({"kind":"task_resumed","from_step":1,"with_user_input":true})",
                       QStringLiteral("req-2"));
    engine->emit_event(QStringLiteral("t2"), kPlanReady);
    QCOMPARE(rec.plans.last().size(), 2);
    QCOMPARE(rec.plans.last()[0].status, StepStatus::Done);
    QCOMPARE(rec.plans.last()[0].summary, QStringLiteral("AAPL trades at 231.4"));
    QCOMPARE(rec.plans.last()[1].status, StepStatus::Pending);
    engine->emit_event(QStringLiteral("t2"), kStepStart1);
    engine->emit_event(QStringLiteral("t2"), kStepEnd1);
    QCOMPARE(rec.tokens, qint64(1200 + 2000)); // the new process counts from zero again
    engine->emit_event(QStringLiteral("t2"), kDone);
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::Completed);
}

void TestAgentTaskExecutors::local_pause_before_task_id_then_resume() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    TaskRun run = local_run(QStringLiteral("loc-3"));
    ex.start(run, {});
    ex.request_pause(run.id); // no task id yet: sent once it is known
    QCOMPARE(engine->calls.size(), 1);
    engine->emit_event(QStringLiteral("t3"), R"({"kind":"task_started","query":"Review AAPL"})", QStringLiteral("req-1"));
    QCOMPARE(engine->calls.last(), QStringLiteral("pause:t3"));
    engine->emit_event(QStringLiteral("t3"), kPlanReady);
    engine->emit_event(QStringLiteral("t3"), kStepStart0);
    engine->emit_event(QStringLiteral("t3"), kStepEnd0);
    engine->emit_event(QStringLiteral("t3"), R"({"kind":"paused","at_step":1})");
    QCOMPARE(rec.waits.size(), 1);
    QCOMPARE(rec.waits[0].first, RunStatus::Paused);
    QVERIFY(rec.finishes.isEmpty());
    pump();

    // resume_run(): Python continues the same task from its checkpoint.
    run.remote_task_id = QStringLiteral("t3");
    QVector<RunStep> steps = rec.plans.last();
    steps[0].status = StepStatus::Done;
    steps[0].summary = QStringLiteral("AAPL trades at 231.4");
    ex.start(run, steps);
    QCOMPARE(engine->calls.last(), QStringLiteral("resume:t3"));
    engine->emit_event(QStringLiteral("t3"), R"({"kind":"task_resumed","from_step":1,"with_user_input":false})",
                       QStringLiteral("req-2"));
    engine->emit_event(QStringLiteral("t3"), kPlanReady);
    QCOMPARE(rec.plans.last()[0].status, StepStatus::Done);
    engine->emit_event(QStringLiteral("t3"), kStepStart1);
    QCOMPARE(rec.steps.last().idx, 1);
    engine->emit_event(QStringLiteral("t3"), kDone);
    QCOMPARE(rec.finishes.size(), 1);
}

void TestAgentTaskExecutors::local_cancel_and_cancel_again() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    const TaskRun run = local_run(QStringLiteral("loc-4"));
    ex.start(run, {});
    engine->emit_event(QStringLiteral("t4"), R"({"kind":"task_started","query":"Review AAPL"})", QStringLiteral("req-1"));
    engine->emit_event(QStringLiteral("t4"), kPlanReady);
    engine->emit_event(QStringLiteral("t4"), kStepStart0);
    ex.cancel(run.id);
    QCOMPARE(engine->calls.last(), QStringLiteral("cancel:t4"));
    QVERIFY(rec.has_text(RunEventKind::Status, QStringLiteral("Stopping after the current step")));
    QVERIFY(!ex.can_pause(run.id));
    QVERIFY(rec.finishes.isEmpty());
    engine->emit_event(QStringLiteral("t4"), kStepEnd0);
    engine->emit_event(QStringLiteral("t4"), R"({"kind":"cancelled","at_step":1})");
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::Cancelled);
    QVERIFY(rec.finishes[0].final_text.contains(QStringLiteral("AAPL trades at 231.4")));

    // Cancel again while the step runs: stop waiting at once, ignore what follows.
    auto* engine2 = new FakeEngine;
    LocalAgenticExecutor ex2{std::unique_ptr<ILocalAgentEngine>(engine2)};
    Recorder rec2(&ex2);
    const TaskRun run2 = local_run(QStringLiteral("loc-4b"));
    ex2.start(run2, {});
    engine2->emit_event(QStringLiteral("t5"), R"({"kind":"task_started","query":"Review AAPL"})", QStringLiteral("req-1"));
    ex2.cancel(run2.id);
    ex2.cancel(run2.id);
    QCOMPARE(rec2.finishes.size(), 1);
    QCOMPARE(rec2.finishes[0].status, RunStatus::Cancelled);
    QVERIFY(rec2.finishes[0].error.contains(QStringLiteral("Stopped waiting")));
    pump();
    engine2->emit_event(QStringLiteral("t5"), R"({"kind":"cancelled","at_step":0})");
    QCOMPARE(rec2.finishes.size(), 1);

    // A paused run has no state here: cancel reaches Python through the record.
    auto* engine3 = new FakeEngine;
    LocalAgenticExecutor ex3{std::unique_ptr<ILocalAgentEngine>(engine3)};
    Recorder rec3(&ex3);
    TaskRun paused = local_run(QStringLiteral("loc-4c"));
    paused.status = RunStatus::Paused;
    paused.remote_task_id = QStringLiteral("t6");
    engine3->runs.insert(paused.id, paused);
    ex3.cancel(paused.id);
    QCOMPARE(engine3->calls, QStringList{QStringLiteral("cancel:t6")});
    QVERIFY(rec3.finishes.isEmpty()); // the service ends a run that holds no slot itself
}

void TestAgentTaskExecutors::local_budget_stop() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    const TaskRun run = local_run(QStringLiteral("loc-5"));
    ex.start(run, {});
    engine->emit_event(QStringLiteral("t7"), R"({"kind":"task_started","query":"Review AAPL"})", QStringLiteral("req-1"));
    engine->emit_event(QStringLiteral("t7"), kPlanReady);
    engine->emit_event(QStringLiteral("t7"), kStepStart0);
    engine->emit_event(QStringLiteral("t7"), kStepEnd0);
    engine->emit_event(QStringLiteral("t7"), R"({"kind":"budget_stop","at_step":1,"breach":"steps",
        "budget":{"tokens_used":1500,"max_tokens":1000000,"cost_used":0.01,"max_cost_usd":5.0,"wall_s":30,"max_wall_s":1800,"steps_used":6,"max_steps":6}})");
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::BudgetStopped);
    QVERIFY(rec.finishes[0].error.contains(QStringLiteral("step limit")));
    QVERIFY(rec.finishes[0].final_text.contains(QStringLiteral("AAPL trades at 231.4")));
    QCOMPARE(rec.tokens, qint64(1500));
}

void TestAgentTaskExecutors::local_error_event() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    const TaskRun run = local_run(QStringLiteral("loc-6"));
    ex.start(run, {});
    // AgentService synthesises this when the process exits without a terminal event.
    engine->emit_event(QStringLiteral(""), R"({"kind":"error","error":"Subprocess exited without terminal event"})",
                       QStringLiteral("req-1"));
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::Failed);
    QCOMPARE(rec.last(RunEventKind::Error)->data.value("code").toString(), QStringLiteral("local_agent_exited"));
    QVERIFY(rec.finishes[0].error.contains(QStringLiteral("stopped unexpectedly")));
}

void TestAgentTaskExecutors::local_not_ready_fails_fast() {
    auto* engine = new FakeEngine;
    engine->ready = {false, QStringLiteral("llm_not_configured"), QStringLiteral("No LLM is configured."), {}};
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    const TaskRun run = local_run(QStringLiteral("loc-7"));
    ex.start(run, {});
    QVERIFY(rec.finishes.isEmpty()); // after start() returned
    ex.cancel(run.id);               // does not add a second finished()
    pump();
    QVERIFY(engine->calls.isEmpty());
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::Failed);
    QCOMPARE(rec.finishes[0].error, QStringLiteral("No LLM is configured."));
    QCOMPARE(rec.last(RunEventKind::Error)->data.value("code").toString(), QStringLiteral("llm_not_configured"));
}

void TestAgentTaskExecutors::local_sync_error_is_attributed() {
    auto* engine = new FakeEngine;
    engine->sync_error = true;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    ex.start(local_run(QStringLiteral("loc-8")), {});
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::Failed);
    QCOMPARE(rec.last(RunEventKind::Error)->data.value("code").toString(), QStringLiteral("python_not_ready"));
}

void TestAgentTaskExecutors::local_replan_keeps_finished_steps() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    ex.start(local_run(QStringLiteral("loc-9")), {});
    engine->emit_event(QStringLiteral("t9"), R"({"kind":"task_started","query":"Review AAPL"})", QStringLiteral("req-1"));
    engine->emit_event(QStringLiteral("t9"), kPlanReady);
    engine->emit_event(QStringLiteral("t9"), kStepStart0);
    engine->emit_event(QStringLiteral("t9"), kStepEnd0);
    engine->emit_event(QStringLiteral("t9"), R"({"kind":"replanned","reason":"need peer data","plan":{"steps":[
        {"id":"s1","name":"Peers","config":{"query":"Fetch MSFT and GOOGL"}},{"id":"s2","name":"Rank","config":{"query":"Rank them"}}]}})");
    QCOMPARE(rec.plans.last().size(), 3);
    QCOMPARE(rec.plans.last()[0].status, StepStatus::Done);
    QCOMPARE(rec.plans.last()[1].title, QStringLiteral("Peers"));
    QCOMPARE(rec.plans.last()[2].idx, 2);
    QVERIFY(rec.has_text(RunEventKind::Note, QStringLiteral("need peer data")));
    // Python's next step index is global: 1 is "Peers".
    engine->emit_event(QStringLiteral("t9"), R"({"kind":"step_start","step_index":1,"step":{"name":"Peers"},"budget":{"tokens_used":1200}})");
    QCOMPARE(rec.steps.last().idx, 1);
    // A step beyond the known plan still gets a row.
    engine->emit_event(QStringLiteral("t9"), R"({"kind":"step_start","step_index":4,"step":{"name":"Extra"},"budget":{"tokens_used":1300}})");
    QCOMPARE(rec.plans.last().size(), 5);
    QCOMPARE(rec.plans.last()[4].title, QStringLiteral("Extra"));
}

void TestAgentTaskExecutors::local_answer_after_restart() {
    auto* engine = new FakeEngine;
    LocalAgenticExecutor ex{std::unique_ptr<ILocalAgentEngine>(engine)};
    Recorder rec(&ex);
    TaskRun run = local_run(QStringLiteral("loc-10"));
    run.status = RunStatus::WaitingInput;
    run.remote_task_id = QStringLiteral("t10");
    engine->runs.insert(run.id, run);
    ex.answer(run.id, QStringLiteral("Income"));
    QCOMPARE(engine->calls, QStringList{QStringLiteral("reply:t10:Income")});
    engine->emit_event(QStringLiteral("t10"), kDone, QStringLiteral("req-1"));
    QCOMPARE(rec.finishes.size(), 1);
    QCOMPARE(rec.finishes[0].status, RunStatus::Completed);
    // A run the record no longer knows fails once.
    ex.answer(QStringLiteral("ghost"), QStringLiteral("x"));
    QCOMPARE(rec.finishes.size(), 2);
    QCOMPARE(rec.finishes[1].status, RunStatus::Failed);
}

QTEST_GUILESS_MAIN(TestAgentTaskExecutors)
#include "tst_agent_task_executors.moc"
