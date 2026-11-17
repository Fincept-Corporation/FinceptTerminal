// tst_autoload_logic.cpp — the pure rules behind ui::AutoLoadPane / AutoLoadGate
// (staleness, input signatures, when a view auto-runs, the app-wide queue, feed
// mode for pushed data), ui::EmptyState's auto-compaction (empty_state_fit) and
// ui::check_requirements with a fake environment.

#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/Requirements.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QSemaphore>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QtTest>

#include <algorithm>
#include <cmath>

using namespace fincept::ui;
using namespace fincept::ui::autoload;

namespace {

class FakeEnv final : public RequirementEnvironment {
  public:
    bool signed_in_ = true;
    bool sign_in_pending_ = false; ///< a saved sign-in still being confirmed
    QSet<QString> secrets;
    bool llm = true;
    bool python = true;
    QSet<QString> brokers; ///< connected broker ids; "*" = any
    double balance = -1;
    QHash<QString, QString> unreadable; ///< env key -> reason: secure storage could not be read
    /// broker id ("" = any) -> sentence: an active account's credentials could not be read
    QHash<QString, QString> broker_unreadable;

    bool signed_in() const override { return signed_in_; }
    bool sign_in_pending() const override { return sign_in_pending_; }
    bool has_secret(const QString& k) const override { return secrets.contains(k); }
    SecretPresence secret_presence(const QString& k, QString* reason) const override {
        if (unreadable.contains(k)) {
            if (reason)
                *reason = unreadable.value(k);
            return SecretPresence::Unreadable;
        }
        return has_secret(k) ? SecretPresence::Present : SecretPresence::Absent;
    }
    bool llm_configured() const override { return llm; }
    bool python_ready() const override { return python; }
    bool broker_connected(const QString& id) const override {
        return brokers.contains(QStringLiteral("*")) || (id.isEmpty() ? !brokers.isEmpty() : brokers.contains(id));
    }
    bool broker_credentials_unreadable(const QString& id, QString* reason) const override {
        for (auto it = broker_unreadable.cbegin(); it != broker_unreadable.cend(); ++it) {
            if (id.isEmpty() || it.key().isEmpty() || it.key() == id) {
                if (reason)
                    *reason = it.value();
                return true;
            }
        }
        return false;
    }
    double credit_balance() const override { return balance; }
};

/// Only the four pure methods: the defaults of secret_presence() and
/// broker_credentials_unreadable() must keep an environment that cannot fail working.
class MinimalEnv final : public RequirementEnvironment {
  public:
    bool signed_in() const override { return true; }
    bool has_secret(const QString&) const override { return false; }
    bool llm_configured() const override { return true; }
    bool python_ready() const override { return true; }
    bool broker_connected(const QString&) const override { return false; }
    double credit_balance() const override { return -1; }
};

/// Signed out, written before sign_in_pending() existed: the default (never
/// pending) must keep it compiling and answering "sign in".
class SignedOutLegacyEnv final : public RequirementEnvironment {
  public:
    bool signed_in() const override { return false; }
    bool has_secret(const QString&) const override { return false; }
    bool llm_configured() const override { return true; }
    bool python_ready() const override { return true; }
    bool broker_connected(const QString&) const override { return false; }
    double credit_balance() const override { return -1; }
};

/// The blocks of an EmptyState as EmptyState::measure() reports them, from font
/// metrics at base font `base` (QFontMetrics::height() ≈ 1.25 × the pixel size):
/// title base+2 (one line), message base-1 (two lines), buttons base-2 with 6 px
/// padding and a 1 px border, glyph 3.6 × base (2.4 compact), inline title base-1.
empty_state_fit::Parts es_parts(int base, bool compact) {
    const auto line = [](int px) { return (px * 5 + 3) / 4; };
    empty_state_fit::Parts p;
    p.margins = compact ? 20 : 48;
    p.tight_margins = 8;
    p.spacing = 10;
    p.glyph = int(base * (compact ? 2.4 : 3.6));
    p.title = line(base + 2);
    p.message = 2 * line(base - 1);
    const int button = line(base - 2) + 2 * 6 + 2;
    p.actions = 4 + button;
    p.title_line = 4 + line(base - 1);
    p.inline_row = 4 + std::max(line(base - 1), button);
    return p;
}

} // namespace

class TestAutoLoadLogic : public QObject {
    Q_OBJECT
  private slots:
    void staleness();
    void signature_is_order_independent();
    void signature_tracks_values();
    void auto_run_rules();
    void queue_capacity_and_fifo();
    void queue_priority();
    void queue_withdraw_and_release();
    void requirements_all_met();
    void requirements_first_missing_wins();
    void requirements_api_key_names_key_and_link();
    void requirements_credits();
    void requirements_broker();
    void requirements_unreadable_key_retries();
    void requirements_unreadable_broker_retries();
    void requirements_secondary_action_defaults();
    void requirements_host_action();
    // Feed mode (pushed data) — since 2026-11-17 round 3.
    void feed_staleness();
    void feed_pages_when_empty();
    void feed_markers_while_shown();
    void feed_requirements_gate();
    // EmptyState auto-compaction.
    void fit_heights_per_level();
    void fit_steps_in_order();
    void fit_short_tile_at_base_fonts();
    void fit_edge_cases();
    // Round 4: requirements on every run path, memory budget.
    void run_gate_checks_requirements_first();
    void eviction_order_and_limits();
    void eviction_never_drops_pinned_or_paid();
    void evicted_view_returns_through_first_show();
    void json_size_estimate();
    // Round 5: sign-in being confirmed (pending) and the SIGN IN action.
    void requirements_sign_in_pending();
    void requirements_sign_in_action();
    // Round 6: EmptyState::apply() of the spec already on screen is a no-op.
    void apply_memo_rule();
    // Round 6 (7): the agent-approval kind on a pane's own re-run buttons.
    void agent_risk_rule();
    // Round 6 (8): the persisted copy off the GUI thread.
    void remembered_text_round_trip();
    void read_verdict_rule();
    void size_estimates_without_a_walk();
    void write_queue_rules();
    void write_behind_threaded();
    void write_behind_flush_at_quit();
};

void TestAutoLoadLogic::remembered_text_round_trip() {
    const QJsonObject payload{{QStringLiteral("rows"), QJsonArray{1, 2, 3}}, {QStringLiteral("name"), QStringLiteral("x")}};
    const QDateTime at = QDateTime::fromString(QStringLiteral("2026-11-17T14:02:00Z"), Qt::ISODate);
    const QByteArray text = serialize_remembered(payload, at, QStringLiteral("abc123"));
    const RememberedRead hit = parse_remembered(QString::fromUtf8(text), 4 * 1024 * 1024);
    QCOMPARE(hit.outcome, ReadOutcome::Hit);
    QCOMPARE(hit.payload, QJsonValue(payload));
    QCOMPARE(hit.as_of, at);
    QCOMPARE(hit.signature, QStringLiteral("abc123"));
    QCOMPARE(hit.text_bytes, qint64(text.size()));
    // No signature: none stored, none read.
    QVERIFY(!serialize_remembered(payload, at, {}).contains("\"sig\""));
    QVERIFY(parse_remembered(QString::fromUtf8(serialize_remembered(payload, at, {})), 0).signature.isEmpty());
    // Miss: nothing, or above the ceiling (as before: not removed).
    QCOMPARE(parse_remembered(QString(), 100).outcome, ReadOutcome::Miss);
    QCOMPARE(parse_remembered(QString::fromUtf8(text), 10).outcome, ReadOutcome::Miss);
    // Corrupt: a cache miss whose row goes — never an error page.
    QCOMPARE(parse_remembered(QStringLiteral("not json {"), 100).outcome, ReadOutcome::Corrupt);
    QCOMPARE(parse_remembered(QStringLiteral("[1,2]"), 100).outcome, ReadOutcome::Corrupt);
    QCOMPARE(parse_remembered(QStringLiteral("{\"at\":\"2026-11-17T14:02:00Z\"}"), 100).outcome, ReadOutcome::Corrupt);
    QCOMPARE(parse_remembered(QStringLiteral("{\"at\":\"yesterday\",\"p\":1}"), 100).outcome, ReadOutcome::Corrupt);
    QVERIFY(parse_remembered(QStringLiteral("not json {"), 100).payload.isUndefined() ||
            parse_remembered(QStringLiteral("not json {"), 100).payload.isNull());
}

void TestAutoLoadLogic::read_verdict_rule() {
    ReadFacts f;
    f.started_gen = f.current_gen = 7;
    f.started_view = f.current_view = QStringLiteral("macro/us");
    QCOMPARE(decide_read(f), ReadVerdict::Accept);
    // Each event that makes the answer out of date drops it.
    ReadFacts g = f;
    g.current_gen = 8; // hidden, view left, invalidate(), clear_result(), a run started
    QCOMPARE(decide_read(g), ReadVerdict::Drop);
    g = f;
    g.current_view = QStringLiteral("macro/eu"); // the user left the view
    QCOMPARE(decide_read(g), ReadVerdict::Drop);
    g = f;
    g.visible = false;
    QCOMPARE(decide_read(g), ReadVerdict::Drop);
    g = f;
    g.run_for_view = true; // a run will answer
    QCOMPARE(decide_read(g), ReadVerdict::Drop);
    g = f;
    g.has_result = true; // a run delivered first: the real run wins
    QCOMPARE(decide_read(g), ReadVerdict::Drop);
}

void TestAutoLoadLogic::size_estimates_without_a_walk() {
    QCOMPARE(bytes_from_json_text(1000), qint64(2000));
    QCOMPARE(bytes_from_json_text(-5), qint64(0));
    // Provisional: the top-level count only — a deep payload costs the same to estimate.
    QJsonArray flat;
    QJsonArray deep;
    for (int i = 0; i < 1000; ++i) {
        flat.append(i);
        deep.append(QJsonArray{QJsonObject{{QStringLiteral("k"), QJsonArray{1, 2, 3, 4, 5}}}});
    }
    QCOMPARE(shallow_json_bytes(flat), shallow_json_bytes(deep));
    QVERIFY(shallow_json_bytes(flat) > shallow_json_bytes(QJsonArray{1, 2}));
    QVERIFY(shallow_json_bytes(QJsonValue(QString(500, QLatin1Char('x')))) >= 1000);
    QVERIFY(shallow_json_bytes(QJsonValue(3.5)) > 0);
    // The exact estimate (worker) sees the depth.
    QVERIFY(approx_json_bytes(deep) > approx_json_bytes(flat));
}

void TestAutoLoadLogic::write_queue_rules() {
    WriteQueue q(2, 3);
    quint64 replaced = 99;
    quint64 dropped = 99;
    q.enqueue(QStringLiteral("a"), 1, &replaced, &dropped);
    QCOMPARE(replaced, quint64(0));
    QCOMPARE(dropped, quint64(0));
    q.enqueue(QStringLiteral("a"), 2, &replaced, &dropped); // latest payload wins
    QCOMPARE(replaced, quint64(1));
    QCOMPARE(q.queued_seq(QStringLiteral("a")), quint64(2));
    QCOMPARE(q.queued_count(), 1);
    // It starts; one in flight per key: a newer write of "a" waits for it.
    QCOMPARE(q.take_ready(), (QList<QPair<QString, quint64>>{{QStringLiteral("a"), 2}}));
    QVERIFY(q.in_flight(QStringLiteral("a")));
    QCOMPARE(q.in_flight_seq(QStringLiteral("a")), quint64(2));
    q.enqueue(QStringLiteral("a"), 3);
    QVERIFY(q.take_ready().isEmpty());
    // Other keys go ahead, up to max_in_flight (2).
    q.enqueue(QStringLiteral("b"), 4);
    q.enqueue(QStringLiteral("c"), 5);
    QCOMPARE(q.take_ready(), (QList<QPair<QString, quint64>>{{QStringLiteral("b"), 4}}));
    QCOMPARE(q.in_flight_count(), 2);
    QVERIFY(q.take_ready().isEmpty()); // "c" waits for a free place
    q.finished(QStringLiteral("a"));
    // First queued first: "a" (3) was queued before "c".
    QCOMPARE(q.take_ready(), (QList<QPair<QString, quint64>>{{QStringLiteral("a"), 3}}));
    q.finished(QStringLiteral("b"));
    QCOMPARE(q.take_ready(), (QList<QPair<QString, quint64>>{{QStringLiteral("c"), 5}}));
    // Bounded: a fourth waiting key drops the oldest waiting op.
    WriteQueue b(1, 3);
    b.enqueue(QStringLiteral("x"), 1);
    b.enqueue(QStringLiteral("y"), 2);
    b.enqueue(QStringLiteral("z"), 3);
    b.enqueue(QStringLiteral("w"), 4, &replaced, &dropped);
    QCOMPARE(dropped, quint64(1));
    QCOMPARE(b.queued_count(), 3);
    // Flush order: queue order, and the queue is empty afterwards.
    QCOMPARE(b.drain(), (QList<QPair<QString, quint64>>{{QStringLiteral("y"), 2}, {QStringLiteral("z"), 3},
                                                         {QStringLiteral("w"), 4}}));
    QCOMPARE(b.queued_count(), 0);
}

void TestAutoLoadLogic::write_behind_threaded() {
    QThreadPool pool;
    pool.setMaxThreadCount(2);
    QMutex mutex;
    QStringList ran;  // work, on pool threads
    QStringList done; // callbacks, on this thread
    QSemaphore gate;
    auto op = [&](const QString& key, const QString& tag, bool block) {
        WriteBehind::Op o;
        o.key = key;
        o.payload = QJsonValue(tag);
        o.as_of = QDateTime::currentDateTimeUtc();
        o.work = [&, tag, block]() -> qint64 {
            if (block)
                gate.acquire();
            QMutexLocker lock(&mutex);
            ran << tag;
            return tag.size();
        };
        o.done = [&, tag](qint64 bytes) {
            QCOMPARE(QThread::currentThread(), QCoreApplication::instance()->thread());
            done << QStringLiteral("%1=%2").arg(tag).arg(bytes);
        };
        return o;
    };
    {
        WriteBehind w(&pool, 2, 64);
        QVERIFY(w.submit(op(QStringLiteral("k"), QStringLiteral("k1"), true))); // runs, blocked
        QVERIFY(w.submit(op(QStringLiteral("k"), QStringLiteral("k2"), false))); // waits behind k1
        QVERIFY(w.submit(op(QStringLiteral("k"), QStringLiteral("k3"), false))); // replaces k2: latest wins
        QVERIFY(w.busy(QStringLiteral("k")));
        WriteBehind::Op seen;
        QVERIFY(w.pending(QStringLiteral("k"), &seen)); // read-your-writes: the newest copy
        QCOMPARE(seen.payload, QJsonValue(QStringLiteral("k3")));
        QVERIFY(!w.pending(QStringLiteral("other"), &seen));
        QVERIFY(w.submit(op(QStringLiteral("j"), QStringLiteral("j1"), false))); // another key runs meanwhile
        QTRY_VERIFY(done.contains(QStringLiteral("j1=2")));
        QVERIFY(!done.contains(QStringLiteral("k1=2")));
        gate.release();
        QTRY_COMPARE(w.in_flight_count() + w.queued_count(), 0);
        QTRY_COMPARE(done.size(), 3);
        {
            QMutexLocker lock(&mutex);
            QVERIFY(!ran.contains(QStringLiteral("k2"))); // replaced: never ran
            QVERIFY(ran.indexOf(QStringLiteral("k1")) < ran.indexOf(QStringLiteral("k3"))); // per-key order kept
        }
        QVERIFY(done.contains(QStringLiteral("k3=2")));
        QVERIFY(!w.busy(QStringLiteral("k")));
    }
    pool.waitForDone();
}

void TestAutoLoadLogic::write_behind_flush_at_quit() {
    QThreadPool pool;
    pool.setMaxThreadCount(2);
    QMutex mutex;
    QStringList ran;
    QSemaphore gate;
    QThread* main_thread = QThread::currentThread();
    bool y_on_caller = false;
    WriteBehind w(&pool, 1, 64); // one at a time: "y" has to wait
    auto make = [&](const QString& key, const QString& tag, bool block) {
        WriteBehind::Op o;
        o.key = key;
        o.work = [&, tag, block]() -> qint64 {
            if (block)
                gate.acquire();
            QMutexLocker lock(&mutex);
            ran << tag;
            if (tag == QLatin1String("y1"))
                y_on_caller = QThread::currentThread() == main_thread;
            return 1;
        };
        o.done = [](qint64) { QFAIL("no callbacks at quit"); };
        return o;
    };
    QVERIFY(w.submit(make(QStringLiteral("x"), QStringLiteral("x1"), true))); // running, stuck
    QVERIFY(w.submit(make(QStringLiteral("x"), QStringLiteral("x2"), false))); // waits behind x1
    QVERIFY(w.submit(make(QStringLiteral("y"), QStringLiteral("y1"), false))); // waits for a free place
    QTRY_COMPARE(w.in_flight_count(), 1);
    QElapsedTimer t;
    t.start();
    const int skipped = w.flush(80); // bounded wait: x1 is still running after it
    QVERIFY(t.elapsed() < 2000);
    QCOMPARE(skipped, 1); // x2 skipped: the older x1 could land after it
    {
        QMutexLocker lock(&mutex);
        QVERIFY(ran.contains(QStringLiteral("y1"))); // written now, on the calling thread
        QVERIFY(!ran.contains(QStringLiteral("x2")));
    }
    QVERIFY(y_on_caller);
    QVERIFY(!w.submit(make(QStringLiteral("z"), QStringLiteral("z1"), false))); // closed after the flush
    gate.release();
    pool.waitForDone();
}

void TestAutoLoadLogic::agent_risk_rule() {
    const QString spend = QStringLiteral("spend");
    // Free run, nothing explicit: no marker.
    QCOMPARE(effective_agent_risk({}, {}, 0, false), QString());
    // A run with a list cost the plan does not include: "spend".
    QCOMPARE(effective_agent_risk({}, {}, 8, false), spend);
    QCOMPARE(effective_agent_risk({}, {}, 0.35, false), spend);
    // Included with the account's plan (costs it nothing): no marker.
    QCOMPARE(effective_agent_risk({}, {}, 8, true), QString());
    // No cost / nonsense cost is free.
    QCOMPARE(effective_agent_risk({}, {}, -1, false), QString());
    QCOMPARE(effective_agent_risk({}, {}, std::nan(""), false), QString());
    // An explicit pane kind always wins — with or without a cost, included or not.
    QCOMPARE(effective_agent_risk(QStringLiteral("delete"), {}, 0, false), QStringLiteral("delete"));
    QCOMPARE(effective_agent_risk(QStringLiteral("delete"), {}, 8, true), QStringLiteral("delete"));
    QCOMPARE(effective_agent_risk(spend, {}, 8, true), spend);
    // The view's kind wins over the pane's.
    QCOMPARE(effective_agent_risk(QStringLiteral("delete"), QStringLiteral("order"), 8, false), QStringLiteral("order"));
    QCOMPARE(effective_agent_risk({}, QStringLiteral("order"), 0, false), QStringLiteral("order"));
    // Blank kinds mean "use the rule"; kinds are trimmed.
    QCOMPARE(effective_agent_risk(QStringLiteral("  "), QStringLiteral(" "), 8, false), spend);
    QCOMPARE(effective_agent_risk(QStringLiteral(" money "), {}, 0, false), QStringLiteral("money"));
}

namespace {
/// QtCore stand-in for EmptyState::Spec (its own equality: tst_empty_state_spec).
struct MemoPage {
    QString title;
    int rows = 0;
    bool operator==(const MemoPage&) const = default;
};
} // namespace

void TestAutoLoadLogic::apply_memo_rule() {
    using Page = MemoPage;
    fincept::ui::empty_state_apply::LastApplied<Page> memo;
    const Page a{QStringLiteral("Could not load"), 2};
    const Page b{QStringLiteral("Could not load"), 3};
    QVERIFY(!memo.valid());
    QVERIFY(memo.should_apply(a));   // first time: applies
    QVERIFY(memo.valid());
    QVERIFY(!memo.should_apply(a));  // equal: nothing to do
    QVERIFY(!memo.should_apply(Page{QStringLiteral("Could not load"), 2})); // equal by value, not identity
    QVERIFY(memo.should_apply(b));   // one field differs
    QVERIFY(memo.should_apply(a));   // back: b was on screen
    memo.invalidate();               // a setter changed the widget outside apply()
    QVERIFY(!memo.valid());
    QVERIFY(memo.should_apply(a));   // so the same spec applies again
    QVERIFY(!memo.should_apply(a));
}

void TestAutoLoadLogic::requirements_sign_in_pending() {
    FakeEnv env;
    env.signed_in_ = false;
    env.sign_in_pending_ = true;
    const RequirementStatus st = check_requirements({Requirement::signed_in()}, env);
    QVERIFY(!st.ok);
    QVERIFY(st.pending);
    QVERIFY(!st.retry);
    QCOMPARE(st.missing.kind, RequirementKind::SignedIn);
    QVERIFY(st.title.contains(QStringLiteral("Checking")));
    QVERIFY(st.action_text.isEmpty()); // nothing to press: never the sign-in page
    QVERIFY(!st.link.valid());
    QVERIFY(st.secondary_text.isEmpty());
    // Nothing starts while it is pending (every run path asks the same gate).
    QCOMPARE(gate_run(true, st.ok), RunGate::NeedsSetup);
    // A feed-mode pane neither subscribes nor shows data it does not have.
    FeedFacts ff;
    ff.requirements_ok = st.ok;
    QCOMPARE(decide_feed(ff).page, FeedPage::NeedsSetup);
    QVERIFY(!decide_feed(ff).subscribe);

    // Signed in: pending is never asked (and never matters).
    env.signed_in_ = true;
    QVERIFY(check_requirements({Requirement::signed_in()}, env).ok);

    // Settled as signed out: the real sign-in page.
    env.signed_in_ = false;
    env.sign_in_pending_ = false;
    const RequirementStatus out = check_requirements({Requirement::signed_in()}, env);
    QVERIFY(!out.ok);
    QVERIFY(!out.pending);

    // List order still decides: an earlier missing requirement wins over a pending one …
    env.sign_in_pending_ = true;
    env.python = false;
    const RequirementStatus py_first = check_requirements({Requirement::python(), Requirement::signed_in()}, env);
    QCOMPARE(py_first.missing.kind, RequirementKind::PythonReady);
    QVERIFY(!py_first.pending);
    // … and a pending one listed first is what the pane shows.
    const RequirementStatus sign_first = check_requirements({Requirement::signed_in(), Requirement::python()}, env);
    QVERIFY(sign_first.pending);

    // A host fix is never offered while the answer is not known yet.
    bool ran = false;
    const RequirementStatus host = check_requirements(
        {Requirement::signed_in().with_action(QStringLiteral("USE DEMO DATA"), [&ran]() { ran = true; })}, env);
    QVERIFY(host.pending);
    QVERIFY(host.action_text.isEmpty());
    QVERIFY(!has_host_action(host));
    const RequirementStatus reapplied = apply_host_action(host);
    QVERIFY(reapplied.action_text.isEmpty());
    QVERIFY(reapplied.pending);
    QVERIFY(!ran);
}

void TestAutoLoadLogic::requirements_sign_in_action() {
    FakeEnv env;
    env.signed_in_ = false;
    const RequirementStatus st = check_requirements({Requirement::signed_in()}, env);
    QVERIFY(!st.ok);
    QVERIFY(!st.pending);
    QCOMPARE(st.action_text, QStringLiteral("SIGN IN"));
    QCOMPARE(st.link.screen_id, QStringLiteral("login"));
    QVERIFY(st.link.section.isEmpty());
    QVERIFY(st.link.valid());

    // A host fix takes the primary button; SIGN IN moves to the secondary one.
    const RequirementStatus host = check_requirements(
        {Requirement::signed_in().with_action(QStringLiteral("USE DEMO DATA"), []() {})}, env);
    QCOMPARE(host.action_text, QStringLiteral("USE DEMO DATA"));
    QVERIFY(has_host_action(host));
    QCOMPARE(host.secondary_text, QStringLiteral("SIGN IN"));
    QCOMPARE(host.secondary_link.screen_id, QStringLiteral("login"));

    // An environment written before sign_in_pending() existed: never pending.
    const SignedOutLegacyEnv legacy;
    const RequirementStatus old = check_requirements({Requirement::signed_in()}, legacy);
    QVERIFY(!old.ok);
    QVERIFY(!old.pending);
    QCOMPARE(old.link.screen_id, QStringLiteral("login"));
}

void TestAutoLoadLogic::run_gate_checks_requirements_first() {
    // The user's RUN, RETRY, RUN AGAIN, REFRESH and the default run all ask the same gate.
    QCOMPARE(gate_run(true, true), RunGate::Start);
    QCOMPARE(gate_run(true, false), RunGate::NeedsSetup); // a missing key: setup page, nothing starts
    QCOMPARE(gate_run(false, true), RunGate::NoAction);
    QCOMPARE(gate_run(false, false), RunGate::NoAction);
    // Built from the real requirement check: a removed key blocks a user run too.
    FakeEnv env;
    env.secrets = {QStringLiteral("FRED_API_KEY")};
    const QList<Requirement> needs{Requirement::api_key(QStringLiteral("FRED_API_KEY"), QStringLiteral("FRED"))};
    QCOMPARE(gate_run(true, check_requirements(needs, env).ok), RunGate::Start);
    env.secrets.clear();
    QCOMPARE(gate_run(true, check_requirements(needs, env).ok), RunGate::NeedsSetup);
    // And the automatic path agrees.
    ViewFacts f;
    f.requirements_ok = check_requirements(needs, env).ok;
    QVERIFY(!should_auto_run(f));
}

void TestAutoLoadLogic::eviction_order_and_limits() {
    const auto slot = [](const QString& key, qint64 bytes, bool reloadable) {
        MemorySlot m;
        m.key = key;
        m.bytes = bytes;
        m.reloadable = reloadable;
        return m;
    };
    constexpr qint64 MB = 1024 * 1024;
    // Least recently used first.
    QList<MemorySlot> lru{slot("a", 2 * MB, true), slot("b", 2 * MB, true), slot("c", 2 * MB, true),
                          slot("d", 2 * MB, true)};
    QVERIFY(choose_evictions(lru, 8 * MB).isEmpty());                  // fits
    QCOMPARE(choose_evictions(lru, 7 * MB), QStringList{"a"});          // the oldest goes first
    QCOMPARE(choose_evictions(lru, 4 * MB), QStringList({"a", "b"}));
    QVERIFY(choose_evictions(lru, 0).isEmpty());                       // no cap
    QVERIFY(choose_evictions(lru, -1).isEmpty());
    // What reloads without a run goes before an AutoRun result that must run again,
    // even when that one is older.
    QList<MemorySlot> mixed{slot("old_rerun", 3 * MB, false), slot("cached", 3 * MB, true),
                            slot("recent", 1 * MB, true)};
    QCOMPARE(choose_evictions(mixed, 4 * MB), QStringList{"cached"});
    QCOMPARE(choose_evictions(mixed, 2 * MB), QStringList({"cached", "recent", "old_rerun"}));
    // Unknown / zero sizes are never dropped for nothing.
    QList<MemorySlot> tiny{slot("zero", 0, true), slot("big", 5 * MB, true)};
    QCOMPARE(choose_evictions(tiny, 1 * MB), QStringList{"big"});
}

void TestAutoLoadLogic::eviction_never_drops_pinned_or_paid() {
    constexpr qint64 MB = 1024 * 1024;
    MemorySlot visible;
    visible.key = QStringLiteral("visible");
    visible.bytes = 10 * MB;
    visible.pinned = true; // on screen (or running / queued)
    MemorySlot paid;
    paid.key = QStringLiteral("paid");
    paid.bytes = 10 * MB;
    paid.reloadable = false;
    paid.manual = true; // a Manual result not in any cache: the user paid for it
    MemorySlot rerun;
    rerun.key = QStringLiteral("rerun");
    rerun.bytes = 10 * MB;
    rerun.reloadable = false; // AutoRun: free to compute again
    const QList<MemorySlot> lru{paid, visible, rerun};
    // Over budget whatever happens: only the free re-run goes; the rest stays over the cap.
    QCOMPARE(choose_evictions(lru, 1 * MB), QStringList{"rerun"});
    // A paid result that IS in the cache may go (it reads back without a run).
    MemorySlot paid_cached = paid;
    paid_cached.reloadable = true;
    QCOMPARE(choose_evictions({paid_cached, visible}, 12 * MB), QStringList{"paid"});
}

void TestAutoLoadLogic::evicted_view_returns_through_first_show() {
    // AutoLoadPane drops only the result of a view (has_result false, nothing else):
    // its next visit is a first show — never blocked as "already attempted".
    ViewFacts back; // a new visit: attempted_this_visit is false
    back.has_result = false;
    QVERIFY(should_auto_run(back)); // AutoRun and nothing read back: the default run (through the gate)
    // A copy read back from the cache: shown at once, no run while fresh, a quiet refresh when old.
    ViewFacts reloaded;
    reloaded.has_result = true;
    QVERIFY(!should_auto_run(reloaded));
    reloaded.stale = true;
    QVERIFY(should_auto_run(reloaded));
    // Manual: never runs by itself, before or after a drop.
    ViewFacts manual = back;
    manual.auto_policy = false;
    QVERIFY(!should_auto_run(manual));
    // The visit that dropped it can only be another view's (the one on screen is pinned).
    MemorySlot on_screen;
    on_screen.key = QStringLiteral("now");
    on_screen.bytes = 64 * 1024 * 1024;
    on_screen.pinned = true;
    QVERIFY(choose_evictions({on_screen}, 1).isEmpty());
}

void TestAutoLoadLogic::json_size_estimate() {
    QCOMPARE(approx_json_bytes(QJsonValue(1.5)), qint64(16));
    QCOMPARE(approx_json_bytes(QJsonValue(QStringLiteral("abcd"))), qint64(16 + 8));
    QJsonArray rows;
    for (int i = 0; i < 1000; ++i)
        rows.append(QJsonObject{{QStringLiteral("symbol"), QStringLiteral("AAPL")}, {QStringLiteral("px"), 189.5}});
    const qint64 big = approx_json_bytes(QJsonValue(rows));
    // 1000 × (object 16 + "symbol" 16+12 + "AAPL" 16+8 + "px" 16+4 + number 16) + array 16
    QCOMPARE(big, qint64(16 + 1000 * (16 + (16 + 12 + 16 + 8) + (16 + 4 + 16))));
    QVERIFY(approx_json_bytes(QJsonValue(QJsonArray{rows.at(0)})) < big);
    QCOMPARE(approx_json_bytes(QJsonValue()), qint64(16));
}

void TestAutoLoadLogic::feed_staleness() {
    const QDateTime now = QDateTime::fromString(QStringLiteral("2026-11-17T12:00:00Z"), Qt::ISODate);
    QVERIFY(!feed_is_stale(now.addSecs(-3600), now, 0));      // 0 = never stale
    QVERIFY(!feed_is_stale(now.addSecs(-10), now, 30));       // younger than the threshold
    QVERIFY(!feed_is_stale(now.addSecs(-30), now, 30));       // exactly at it: still fresh
    QVERIFY(feed_is_stale(now.addMSecs(-30001), now, 30));    // more than it: stale
    QVERIFY(!feed_is_stale(QDateTime(), now, 30));            // unknown arrival: no marker
    QCOMPARE(feed_ms_until_stale(now.addSecs(-10), now, 30), qint64(20001));
    QCOMPARE(feed_ms_until_stale(now.addSecs(-30), now, 30), qint64(1));
    QCOMPARE(feed_ms_until_stale(now.addSecs(-31), now, 30), qint64(0)); // already stale
    QCOMPARE(feed_ms_until_stale(now, now, 0), qint64(-1));              // never
    QCOMPARE(feed_ms_until_stale(QDateTime(), now, 30), qint64(-1));
    // A clock that went back never waits longer than the threshold.
    QCOMPARE(feed_ms_until_stale(now.addSecs(600), now, 30), qint64(30001));
    // The timer's answer and the check agree at the moment it fires.
    const QDateTime last = now.addSecs(-12);
    const qint64 wait = feed_ms_until_stale(last, now, 30);
    QVERIFY(!feed_is_stale(last, now.addMSecs(wait - 1), 30));
    QVERIFY(feed_is_stale(last, now.addMSecs(wait), 30));
}

void TestAutoLoadLogic::feed_pages_when_empty() {
    FeedFacts f; // shown, requirements met, nothing reported yet
    f.now = QDateTime::currentDateTimeUtc();
    FeedDecision d = decide_feed(f);
    QCOMPARE(d.page, FeedPage::Waiting); // never blank: the skeleton while the host subscribes
    QCOMPARE(d.marker, FeedMarker::None);
    QVERIFY(d.subscribe);
    f.report = FeedReport::Waiting;
    QCOMPARE(decide_feed(f).page, FeedPage::Waiting);
    f.report = FeedReport::Error;
    QCOMPARE(decide_feed(f).page, FeedPage::Error);
    QVERIFY(decide_feed(f).subscribe); // RETRY can re-subscribe
    f.report = FeedReport::Idle;
    QCOMPARE(decide_feed(f).page, FeedPage::Idle);
    f.report = FeedReport::Data; // data but no content widget to draw it in
    QCOMPARE(decide_feed(f).page, FeedPage::Waiting);
}

void TestAutoLoadLogic::feed_markers_while_shown() {
    const QDateTime now = QDateTime::fromString(QStringLiteral("2026-11-17T12:00:00Z"), Qt::ISODate);
    FeedFacts f;
    f.now = now;
    f.has_data = true;
    f.stale_after_seconds = 30;
    f.last_update = now.addSecs(-5);

    f.report = FeedReport::Data;
    FeedDecision d = decide_feed(f);
    QCOMPARE(d.page, FeedPage::Content);
    QCOMPARE(d.marker, FeedMarker::None); // live and fresh

    f.last_update = now.addSecs(-45);
    QCOMPARE(decide_feed(f).marker, FeedMarker::Stale); // a marker, the data stays
    QCOMPARE(decide_feed(f).page, FeedPage::Content);

    f.stale_after_seconds = 0; // no threshold: never stale
    QCOMPARE(decide_feed(f).marker, FeedMarker::None);
    f.stale_after_seconds = 30;

    // Remembered / earlier data before the first update of this visit.
    f.report = FeedReport::None;
    f.last_update = now.addSecs(-5);
    QCOMPARE(decide_feed(f).marker, FeedMarker::Connecting);
    f.report = FeedReport::Waiting;
    QCOMPARE(decide_feed(f).marker, FeedMarker::Connecting);
    f.last_update = now.addSecs(-3600); // yesterday's copy while connecting: stale wins
    QCOMPARE(decide_feed(f).marker, FeedMarker::Stale);

    // Error and idle keep the data and say why it does not move (stale or not).
    f.report = FeedReport::Error;
    QCOMPARE(decide_feed(f).marker, FeedMarker::Stopped);
    QCOMPARE(decide_feed(f).page, FeedPage::Content);
    f.report = FeedReport::Idle;
    QCOMPARE(decide_feed(f).marker, FeedMarker::Idle);
    f.last_update = now.addSecs(-1);
    QCOMPARE(decide_feed(f).marker, FeedMarker::Idle);
}

void TestAutoLoadLogic::feed_requirements_gate() {
    FeedFacts f;
    f.now = QDateTime::currentDateTimeUtc();
    f.requirements_ok = false;
    FeedDecision d = decide_feed(f);
    QCOMPARE(d.page, FeedPage::NeedsSetup);
    QVERIFY(!d.subscribe); // never subscribe while something is missing
    f.report = FeedReport::Error;
    QCOMPARE(decide_feed(f).page, FeedPage::NeedsSetup); // setup first: the error is a symptom
    // Data on screen stays (the pane adds its setup notice), still no subscription.
    f.has_data = true;
    f.report = FeedReport::Data;
    f.last_update = f.now.addSecs(-3600);
    f.stale_after_seconds = 30;
    d = decide_feed(f);
    QCOMPARE(d.page, FeedPage::Content);
    QCOMPARE(d.marker, FeedMarker::None);
    QVERIFY(!d.subscribe);
}

void TestAutoLoadLogic::fit_heights_per_level() {
    using namespace empty_state_fit;
    Parts p;
    p.margins = 48;
    p.tight_margins = 8;
    p.spacing = 10;
    p.glyph = 50;
    p.title = 20;
    p.message = 34;
    p.details = 120;
    p.suggestions = 80;
    p.actions = 33;
    p.inline_row = 33;
    p.title_line = 21;
    // n stacked blocks have n - 1 spacings between them.
    QCOMPARE(needed_height(p, Level::Full), 48 + 50 + 20 + 34 + 120 + 80 + 33 + 5 * 10);
    QCOMPARE(needed_height(p, Level::NoGlyph), 8 + 20 + 34 + 120 + 80 + 33 + 4 * 10);
    QCOMPARE(needed_height(p, Level::NoDetails), 8 + 20 + 34 + 33 + 2 * 10);
    QCOMPARE(needed_height(p, Level::Inline), 8 + 33);
    QCOMPARE(needed_height(p, Level::TitleOnly), 8 + 21);
    // Absent blocks add neither height nor spacing; DETAILS goes with the rows, the note stays.
    p.details = 0;
    p.note = 25;
    p.technical = 18;
    QCOMPARE(needed_height(p, Level::NoGlyph), 8 + 20 + 34 + 18 + 25 + 80 + 33 + 5 * 10);
    QCOMPARE(needed_height(p, Level::NoDetails), 8 + 20 + 34 + 25 + 33 + 3 * 10);
}

void TestAutoLoadLogic::fit_steps_in_order() {
    using namespace empty_state_fit;
    Parts p = es_parts(14, false);
    p.details = 120;
    p.suggestions = 80;
    const int full = needed_height(p, Level::Full);
    const int no_glyph = needed_height(p, Level::NoGlyph);
    const int no_details = needed_height(p, Level::NoDetails);
    const int inl = needed_height(p, Level::Inline);
    const int title = needed_height(p, Level::TitleOnly);
    // Each step needs less than the one before: glyph, then rows, then one line, then the title.
    QVERIFY(full > no_glyph);
    QVERIFY(no_glyph > no_details);
    QVERIFY(no_details > inl);
    QVERIFY(inl >= title);
    // Exactly at a level's height that level fits; one pixel less moves to the next.
    QCOMPARE(choose_level(p, 2000), Level::Full);
    QCOMPARE(choose_level(p, full), Level::Full);
    QCOMPARE(choose_level(p, full - 1), Level::NoGlyph);
    QCOMPARE(choose_level(p, no_glyph), Level::NoGlyph);
    QCOMPARE(choose_level(p, no_glyph - 1), Level::NoDetails);
    QCOMPARE(choose_level(p, no_details), Level::NoDetails);
    QCOMPARE(choose_level(p, no_details - 1), Level::Inline);
    QCOMPARE(choose_level(p, inl), Level::Inline);
    QCOMPARE(choose_level(p, inl - 1), Level::TitleOnly);
    QCOMPARE(choose_level(p, title), Level::TitleOnly);
    QCOMPARE(choose_level(p, 3), Level::TitleOnly); // nothing fits: the title still shows
}

void TestAutoLoadLogic::fit_short_tile_at_base_fonts() {
    using namespace empty_state_fit;
    // A compact state in a 3-row dashboard tile: ~90 px under the tile header.
    constexpr int kTile = 90;
    QCOMPARE(choose_level(es_parts(9, true), kTile), Level::NoGlyph); // base 9: only the glyph goes
    QCOMPARE(choose_level(es_parts(14, true), kTile), Level::Inline); // base 14: title + action on one line
    QCOMPARE(choose_level(es_parts(24, true), kTile), Level::Inline); // base 24: still one usable line
    QCOMPARE(choose_level(es_parts(24, true), 50), Level::TitleOnly); // a 2-row tile at 24: the title
    // A normal pane keeps the full layout at every base font.
    for (const int base : {9, 14, 24})
        QCOMPARE(choose_level(es_parts(base, false), 600), Level::Full);
    // The heights grow with the font: what fits at 9 may not fit at 24.
    QVERIFY(needed_height(es_parts(24, false), Level::Full) > needed_height(es_parts(9, false), Level::Full));
    QVERIFY(needed_height(es_parts(24, true), Level::Inline) > needed_height(es_parts(9, true), Level::Inline));
}

void TestAutoLoadLogic::fit_edge_cases() {
    using namespace empty_state_fit;
    const Parts p = es_parts(14, false);
    QCOMPARE(choose_level(p, 0), Level::Full);  // not laid out yet: no compaction
    QCOMPARE(choose_level(p, -5), Level::Full);
    // No glyph: the first step only tightens the margins.
    Parts bare = p;
    bare.glyph = 0;
    QCOMPARE(needed_height(bare, Level::Full) - needed_height(bare, Level::NoGlyph),
             bare.margins - bare.tight_margins);
    // No primary action: the one-line step is the title line.
    Parts no_action = p;
    no_action.actions = 0;
    no_action.inline_row = 0;
    QCOMPARE(needed_height(no_action, Level::Inline), no_action.tight_margins + no_action.title_line);
    // Nothing at all: margins only, every level fits.
    const Parts empty;
    QCOMPARE(needed_height(empty, Level::Full), 0);
    QCOMPARE(choose_level(empty, 1), Level::Full);
}

void TestAutoLoadLogic::requirements_host_action() {
    FakeEnv env;
    int built = 0;
    const Requirement need = Requirement::api_key(QStringLiteral("DATASET_KEY"), QStringLiteral("Dataset"))
                                 .with_action(QStringLiteral("BUILD FROM YAHOO"), [&built]() { ++built; });
    QCOMPARE(need.kind, RequirementKind::ApiKey);
    QCOMPARE(need.name, QStringLiteral("DATASET_KEY"));
    const RequirementStatus st = check_requirements({need}, env);
    QVERIFY(!st.ok);
    QVERIFY(has_host_action(st));
    QCOMPARE(st.action_text, QStringLiteral("BUILD FROM YAHOO")); // the primary button is the host's fix
    QVERIFY(!st.link.valid());                                    // … not a Settings deep link
    QCOMPARE(st.secondary_text, QStringLiteral("OPEN SETTINGS > CREDENTIALS")); // the standard one moved
    QCOMPARE(st.secondary_link.section, QStringLiteral("credentials"));
    QCOMPARE(st.secondary_link.focus, QStringLiteral("DATASET_KEY"));
    st.missing.action();
    QCOMPARE(built, 1);
    // Applying again changes nothing (custom checks run it too).
    const RequirementStatus again = apply_host_action(st);
    QCOMPARE(again.action_text, st.action_text);
    QCOMPARE(again.secondary_text, st.secondary_text);
    // Met: no page at all.
    env.secrets.insert(QStringLiteral("DATASET_KEY"));
    QVERIFY(check_requirements({need}, env).ok);
    // A check that could not be made keeps RETRY.
    env.secrets.clear();
    env.unreadable.insert(QStringLiteral("DATASET_KEY"), QStringLiteral("the terminal database is busy"));
    const RequirementStatus unreadable = check_requirements({need}, env);
    QVERIFY(unreadable.retry);
    QVERIFY(!has_host_action(unreadable));
    QCOMPARE(unreadable.action_text, QStringLiteral("RETRY"));
    // An empty text or callback means no fix: the default page.
    env.unreadable.clear();
    const RequirementStatus plain =
        check_requirements({Requirement::api_key(QStringLiteral("DATASET_KEY"), {}).with_action({}, [] {})}, env);
    QVERIFY(!has_host_action(plain));
    QCOMPARE(plain.link.section, QStringLiteral("credentials"));
    // A custom check can carry one.
    RequirementStatus custom;
    custom.ok = false;
    custom.title = QStringLiteral("No dataset");
    custom.missing = Requirement::python().with_action(QStringLiteral("BUILD"), [&built]() { built += 10; });
    const RequirementStatus applied = apply_host_action(custom);
    QVERIFY(has_host_action(applied));
    QCOMPARE(applied.action_text, QStringLiteral("BUILD"));
    QVERIFY(applied.secondary_text.isEmpty()); // there was no standard action to keep
    applied.missing.action();
    QCOMPARE(built, 11);
}

void TestAutoLoadLogic::requirements_unreadable_broker_retries() {
    const QString sentence = QStringLiteral("Could not read the stored credential: the terminal database is busy "
                                            "(another program may be using it). The terminal retries automatically.");
    FakeEnv env;
    env.broker_unreadable.insert(QStringLiteral("zerodha"), sentence);
    // The account of that broker cannot be read: RETRY, never "connect a broker".
    const RequirementStatus st = check_requirements({Requirement::broker(QStringLiteral("zerodha"), "Zerodha")}, env);
    QVERIFY(!st.ok);
    QCOMPARE(st.missing.kind, RequirementKind::BrokerConnected);
    QVERIFY(st.retry);
    QVERIFY(!st.link.valid());
    QCOMPARE(st.action_text, QStringLiteral("RETRY"));
    QCOMPARE(st.title, QStringLiteral("Could not read secure storage"));
    QVERIFY(st.message.startsWith(sentence));
    QVERIFY(st.message.contains(QStringLiteral("Zerodha credentials may already be saved")));
    QVERIFY(!st.message.contains(QStringLiteral("Connect a broker")));
    // "Any broker" asks every active account.
    const RequirementStatus any = check_requirements({Requirement::broker()}, env);
    QVERIFY(any.retry);
    QVERIFY(any.message.contains(QStringLiteral("Your broker credentials may already be saved")));
    // Another broker whose account reads fine: the plain "connect" page.
    const RequirementStatus other = check_requirements({Requirement::broker("dhan", "Dhan")}, env);
    QVERIFY(!other.retry);
    QCOMPARE(other.link.screen_id, QStringLiteral("equity_trading"));
    // A connected account wins over an unreadable one elsewhere.
    env.brokers = {"zerodha"};
    QVERIFY(check_requirements({Requirement::broker(QStringLiteral("zerodha"))}, env).ok);
    // Readable again and not connected: back to "connect a broker".
    env.brokers.clear();
    env.broker_unreadable.clear();
    const RequirementStatus back = check_requirements({Requirement::broker(QStringLiteral("zerodha"))}, env);
    QVERIFY(!back.retry);
    QCOMPARE(back.action_text, QStringLiteral("OPEN ACCOUNTS"));
    // An environment without the override never reports "unreadable".
    const MinimalEnv minimal;
    const RequirementStatus plain = check_requirements({Requirement::broker()}, minimal);
    QVERIFY(!plain.ok);
    QVERIFY(!plain.retry);
}

void TestAutoLoadLogic::requirements_unreadable_key_retries() {
    FakeEnv env;
    env.unreadable.insert(QStringLiteral("FRED_API_KEY"), QStringLiteral("the terminal database is not open yet"));
    const RequirementStatus st = check_requirements(
        {Requirement::python(), Requirement::api_key(QStringLiteral("FRED_API_KEY"), QStringLiteral("FRED"))}, env);
    QVERIFY(!st.ok);
    QVERIFY(st.retry);                 // the action re-checks …
    QVERIFY(!st.link.valid());         // … instead of opening Settings > Credentials
    QCOMPARE(st.action_text, QStringLiteral("RETRY"));
    QVERIFY(st.message.contains(QStringLiteral("Could not read secure storage: the terminal database is not open yet")));
    QVERIFY(st.message.contains(QStringLiteral("FRED")));
    // Once readable and present, the requirement is met.
    env.unreadable.clear();
    env.secrets.insert(QStringLiteral("FRED_API_KEY"));
    QVERIFY(check_requirements({Requirement::api_key(QStringLiteral("FRED_API_KEY"), QString())}, env).ok);
}

void TestAutoLoadLogic::requirements_secondary_action_defaults() {
    FakeEnv env;
    env.llm = false;
    const RequirementStatus st = check_requirements({Requirement::llm()}, env);
    QVERIFY(!st.ok);
    QVERIFY(!st.retry);
    QVERIFY(st.secondary_text.isEmpty()); // only a host (AutoLoadPane::set_setup_secondary_action) adds one
    QVERIFY(!st.secondary_link.valid());
}

void TestAutoLoadLogic::staleness() {
    const QDateTime now = QDateTime::fromString(QStringLiteral("2026-11-17T12:00:00Z"), Qt::ISODate);
    const QDateTime ten_min = now.addSecs(-600);
    QVERIFY(is_stale(ten_min, now, kAlwaysRefresh));         // 0 = refresh on every show
    QVERIFY(!is_stale(ten_min, now, kNeverRefresh));         // -1 = never
    QVERIFY(!is_stale(ten_min, now, 900));                   // younger than 15 min
    QVERIFY(is_stale(ten_min, now, 300));                    // older than 5 min
    QVERIFY(!is_stale(now.addSecs(-300), now, 300));         // exactly at the limit is still fresh
    QVERIFY(is_stale(QDateTime(), now, 3600));               // unknown age is stale
    QVERIFY(!is_stale(QDateTime(), now, kNeverRefresh));
    QVERIFY(!is_stale(now.addSecs(-3600), now, kDefaultMaxAgeSeconds));
    QVERIFY(is_stale(now.addDays(-2), now, kDefaultMaxAgeSeconds));
}

void TestAutoLoadLogic::signature_is_order_independent() {
    QVariantMap a{{"spot", 100}, {"strike", 105}, {"type", "call"}};
    QVariantMap b;
    b.insert("type", "call");
    b.insert("strike", 105);
    b.insert("spot", 100);
    QCOMPARE(signature_of(a), signature_of(b));
    QCOMPARE(signature_of(a).size(), 16);
    QJsonObject o1{{"x", 1}, {"y", QJsonArray{1, 2}}};
    QJsonObject o2{{"y", QJsonArray{1, 2}}, {"x", 1}};
    QCOMPARE(signature_of(QJsonValue(o1)), signature_of(QJsonValue(o2)));
}

void TestAutoLoadLogic::signature_tracks_values() {
    const QString base = signature_of(QVariantMap{{"spot", 100}, {"strike", 105}});
    QVERIFY(base != signature_of(QVariantMap{{"spot", 101}, {"strike", 105}}));
    QVERIFY(base != signature_of(QVariantMap{{"spot", 100}}));
    // Array order matters (a series is not a set).
    QVERIFY(signature_of(QJsonValue(QJsonArray{1, 2})) != signature_of(QJsonValue(QJsonArray{2, 1})));
    // Scalars are signed too.
    QVERIFY(signature_of(QJsonValue(1)) != signature_of(QJsonValue(2)));
}

void TestAutoLoadLogic::auto_run_rules() {
    ViewFacts f; // AutoRun, nothing remembered
    QVERIFY(should_auto_run(f));

    ViewFacts manual = f;
    manual.auto_policy = false; // class C never runs by itself
    QVERIFY(!should_auto_run(manual));

    ViewFacts missing = f;
    missing.requirements_ok = false; // needs setup
    QVERIFY(!should_auto_run(missing));

    ViewFacts busy = f;
    busy.running = true;
    QVERIFY(!should_auto_run(busy));

    ViewFacts once = f;
    once.attempted_this_visit = true; // at most one automatic attempt per visit
    QVERIFY(!should_auto_run(once));

    ViewFacts fresh = f;
    fresh.has_result = true; // remembered and fresh: shown, not re-run
    QVERIFY(!should_auto_run(fresh));

    ViewFacts stale = fresh;
    stale.stale = true; // older than the max age: quiet refresh
    QVERIFY(should_auto_run(stale));

    ViewFacts changed = stale;
    changed.signature_mismatch = true; // other inputs: notice, never a silent re-run
    QVERIFY(!should_auto_run(changed));

    ViewFacts forced = changed;
    forced.forced = true; // the host asked (invalidate)
    QVERIFY(should_auto_run(forced));
    forced.attempted_this_visit = true;
    QVERIFY(!should_auto_run(forced));
}

void TestAutoLoadLogic::queue_capacity_and_fifo() {
    AutoLoadQueue q(2);
    QVERIFY(q.request(1));
    QVERIFY(q.request(2));
    QVERIFY(!q.request(3));
    QVERIFY(!q.request(4));
    QCOMPARE(q.running_count(), 2);
    QCOMPARE(q.queued_count(), 2);
    QCOMPARE(q.position(3), 0);
    QCOMPARE(q.position(4), 1);
    QCOMPARE(q.release(1), QList<quint64>{3}); // FIFO
    QVERIFY(q.is_running(3));
    QCOMPARE(q.release(2), QList<quint64>{4});
    QCOMPARE(q.release(3), QList<quint64>{});
    QCOMPARE(q.running_count(), 1);
    QVERIFY(q.request(5)); // a slot is free and nobody waits
}

void TestAutoLoadLogic::queue_priority() {
    AutoLoadQueue q(1);
    QVERIFY(q.request(1, 0));
    QVERIFY(!q.request(2, 0));
    QVERIFY(!q.request(3, 5)); // higher priority jumps ahead of 2
    QVERIFY(!q.request(4, 5)); // FIFO within the same priority
    QCOMPARE(q.position(3), 0);
    QCOMPARE(q.position(4), 1);
    QCOMPARE(q.position(2), 2);
    QCOMPARE(q.release(1), QList<quint64>{3});
    QCOMPARE(q.release(3), QList<quint64>{4});
    QCOMPARE(q.release(4), QList<quint64>{2});
}

void TestAutoLoadLogic::queue_withdraw_and_release() {
    AutoLoadQueue q(2);
    q.request(1);
    q.request(2);
    q.request(3);
    q.request(4);
    QCOMPARE(q.release(3), QList<quint64>{}); // a hidden pane leaves the queue: nothing starts
    QVERIFY(!q.is_queued(3));
    QCOMPARE(q.position(4), 0);
    QCOMPARE(q.release(99), QList<quint64>{}); // unknown ticket: no effect
    q.set_capacity(3);
    QCOMPARE(q.release(99), QList<quint64>{4}); // a larger capacity fills on the next release
    QCOMPARE(q.running_count(), 3);
    QVERIFY(!q.request(0)); // ticket 0 is never valid
}

void TestAutoLoadLogic::requirements_all_met() {
    FakeEnv env;
    env.secrets = {"FRED_API_KEY"};
    env.brokers = {"zerodha"};
    env.balance = 100;
    const RequirementStatus st = check_requirements(
        {Requirement::signed_in(), Requirement::api_key("FRED_API_KEY", "FRED"), Requirement::llm(),
         Requirement::python(), Requirement::broker("zerodha"), Requirement::credits(8)},
        env);
    QVERIFY(st.ok);
    QVERIFY(check_requirements({}, env).ok);
}

void TestAutoLoadLogic::requirements_first_missing_wins() {
    FakeEnv env;
    env.llm = false;
    env.python = false;
    const RequirementStatus st = check_requirements({Requirement::python(), Requirement::llm()}, env);
    QVERIFY(!st.ok);
    QCOMPARE(st.missing.kind, RequirementKind::PythonReady);
    QCOMPARE(st.link.section, QStringLiteral("python_env"));
    const RequirementStatus st2 = check_requirements({Requirement::llm(), Requirement::python()}, env);
    QCOMPARE(st2.missing.kind, RequirementKind::LlmConfigured);
    QCOMPARE(st2.link.screen_id, QStringLiteral("settings"));
    QCOMPARE(st2.link.section, QStringLiteral("llm_config"));
}

void TestAutoLoadLogic::requirements_api_key_names_key_and_link() {
    FakeEnv env;
    const RequirementStatus st = check_requirements({Requirement::api_key("FRED_API_KEY", "FRED")}, env);
    QVERIFY(!st.ok);
    QVERIFY(st.title.contains(QStringLiteral("FRED")));
    QVERIFY(st.message.contains(QStringLiteral("FRED_API_KEY")));
    QVERIFY(st.message.contains(QStringLiteral("Settings > Credentials")));
    QCOMPARE(st.link.screen_id, QStringLiteral("settings"));
    QCOMPARE(st.link.section, QStringLiteral("credentials"));
    QCOMPARE(st.link.focus, QStringLiteral("FRED_API_KEY"));
    QVERIFY(!st.action_text.isEmpty());
}

void TestAutoLoadLogic::requirements_credits() {
    FakeEnv env;
    env.balance = -1; // unknown balance never blocks
    QVERIFY(check_requirements({Requirement::credits(8)}, env).ok);
    env.balance = 8;
    QVERIFY(check_requirements({Requirement::credits(8)}, env).ok);
    env.balance = 3;
    const RequirementStatus st = check_requirements({Requirement::credits(8)}, env);
    QVERIFY(!st.ok);
    QCOMPARE(st.link.screen_id, QStringLiteral("plans"));
    QVERIFY(check_requirements({Requirement::credits(0)}, env).ok);
}

void TestAutoLoadLogic::requirements_broker() {
    FakeEnv env;
    QVERIFY(!check_requirements({Requirement::broker()}, env).ok);
    env.brokers = {"zerodha"};
    QVERIFY(check_requirements({Requirement::broker()}, env).ok);
    QVERIFY(check_requirements({Requirement::broker("zerodha", "Zerodha")}, env).ok);
    const RequirementStatus st = check_requirements({Requirement::broker("dhan", "Dhan")}, env);
    QVERIFY(!st.ok);
    QVERIFY(st.title.contains(QStringLiteral("Dhan")));
}

QTEST_GUILESS_MAIN(TestAutoLoadLogic)
#include "tst_autoload_logic.moc"
