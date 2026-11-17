// tst_run_scheduling — the pure rules of scheduled runs, task templates and run
// budgets (src/services/agent_tasks/ScheduleLogic, RunTemplates, TaskRunTypes):
//
//   * next occurrence: daily / weekdays / weekly / every N hours / once, in an
//     explicit time zone, across the DST changes of America/New_York (a time in
//     the spring-forward gap runs after it, a time in the fall-back fold runs once)
//     and in zones without DST;
//   * missed occurrences: on time, caught up within the window (once, however many
//     were missed), recorded as missed beyond it; disabled / paused do nothing;
//   * never overlap the previous run; the daily credit cap across scheduled runs;
//   * schedule validation and descriptions; relative date ranges;
//   * template placeholder filling (optional lines dropped, unknown placeholders
//     kept), parameter validation, request building, the JSON of user templates;
//   * the cost estimate and the confirmation threshold of a run's budget;
//   * the working clock: a paused run's elapsed time stands still.
//
// Links TaskRunTypes.cpp, ScheduleLogic.cpp and RunTemplates.cpp only (Qt Core).

#include "services/agent_tasks/RunTemplates.h"
#include "services/agent_tasks/ScheduleLogic.h"

#include <QDateTime>
#include <QTimeZone>
#include <QtTest>

using namespace fincept::agent_tasks;
namespace sl = fincept::agent_tasks::schedule_logic;

namespace {

const QByteArray kNy = QByteArrayLiteral("America/New_York");

qint64 at(const char* zone, int y, int m, int d, int hh, int mm) {
    return QDateTime(QDate(y, m, d), QTime(hh, mm), QTimeZone(QByteArray(zone))).toMSecsSinceEpoch();
}

qint64 utc(int y, int m, int d, int hh, int mm) {
    return QDateTime(QDate(y, m, d), QTime(hh, mm), QTimeZone::UTC).toMSecsSinceEpoch();
}

RunSchedule daily(const QString& time, int mask = 0x7f, const QString& zone = QString::fromLatin1(kNy)) {
    RunSchedule s;
    s.id = QStringLiteral("s1");
    s.name = QStringLiteral("Morning brief");
    s.prompt = QStringLiteral("Brief me");
    s.kind = QString::fromLatin1(kScheduleDaily);
    s.daily_time = time;
    s.weekdays_mask = mask;
    s.time_zone = zone;
    s.enabled = true;
    return s;
}

} // namespace

class TestRunScheduling : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void daily_and_weekdays();
    void daily_across_dst_spring_forward();
    void daily_across_dst_fall_back();
    void zones_without_dst();
    void weekly_interval_once();
    void missed_runs_and_catch_up();
    void overlap_and_daily_cap();
    void validation_and_descriptions();
    void relative_dates();
    void template_placeholders();
    void template_validation_and_request();
    void builtin_templates_are_complete();
    void user_template_json_round_trip();
    void cost_estimate_and_confirmation();
    void working_clock_stops_while_paused();
    void login_gate_and_cap_per_engine();
};

void TestRunScheduling::initTestCase() {
    if (!QTimeZone(kNy).isValid())
        QSKIP("No time-zone database on this machine");
}

void TestRunScheduling::daily_and_weekdays() {
    RunSchedule s = daily(QStringLiteral("08:30"), 0x1f); // weekdays
    // Friday 2026-11-13 09:00 NY → Monday 2026-11-16 08:30 NY.
    QCOMPARE(sl::next_occurrence_ms(s, at("America/New_York", 2026, 11, 13, 9, 0)),
             at("America/New_York", 2026, 11, 16, 8, 30));
    // Strictly after: exactly 08:30 Monday → Tuesday.
    QCOMPARE(sl::next_occurrence_ms(s, at("America/New_York", 2026, 11, 16, 8, 30)),
             at("America/New_York", 2026, 11, 17, 8, 30));
    // Before the time on a weekday → the same day.
    QCOMPARE(sl::next_occurrence_ms(s, at("America/New_York", 2026, 11, 17, 7, 0)),
             at("America/New_York", 2026, 11, 17, 8, 30));
    s.weekdays_mask = 0;
    QCOMPARE(sl::next_occurrence_ms(s, at("America/New_York", 2026, 11, 17, 7, 0)), qint64(0));
    s = daily(QStringLiteral("25:99"));
    QCOMPARE(sl::next_occurrence_ms(s, 0), qint64(0));
}

void TestRunScheduling::daily_across_dst_spring_forward() {
    // 2026-03-08: clocks go 02:00 EST → 03:00 EDT in New York.
    RunSchedule s = daily(QStringLiteral("02:30"));
    const qint64 next = sl::next_occurrence_ms(s, at("America/New_York", 2026, 3, 8, 0, 0));
    // 02:30 does not exist: it runs 30 minutes after the gap, at 03:30 EDT = 07:30 UTC.
    QCOMPARE(next, utc(2026, 3, 8, 7, 30));
    // The day after it is 02:30 EDT again (06:30 UTC).
    QCOMPARE(sl::next_occurrence_ms(s, next), utc(2026, 3, 9, 6, 30));
    // A normal time keeps its wall-clock time across the change (08:00 EST → 08:00 EDT).
    RunSchedule m = daily(QStringLiteral("08:00"));
    QCOMPARE(sl::next_occurrence_ms(m, at("America/New_York", 2026, 3, 7, 9, 0)), utc(2026, 3, 8, 12, 0));
    QCOMPARE(sl::next_occurrence_ms(m, at("America/New_York", 2026, 3, 6, 9, 0)), utc(2026, 3, 7, 13, 0));
}

void TestRunScheduling::daily_across_dst_fall_back() {
    // 2026-11-01: clocks go 02:00 EDT → 01:00 EST; 01:30 happens twice.
    RunSchedule s = daily(QStringLiteral("01:30"));
    const qint64 first = sl::next_occurrence_ms(s, at("America/New_York", 2026, 11, 1, 0, 0));
    QCOMPARE(first, utc(2026, 11, 1, 5, 30)); // the first 01:30 (EDT)
    // After it ran, the second 01:30 (EST, 06:30 UTC) is NOT another occurrence.
    QCOMPARE(sl::next_occurrence_ms(s, first), utc(2026, 11, 2, 6, 30));
}

void TestRunScheduling::zones_without_dst() {
    RunSchedule s = daily(QStringLiteral("09:15"), 0x7f, QStringLiteral("Asia/Kolkata"));
    QCOMPARE(sl::next_occurrence_ms(s, utc(2026, 6, 1, 0, 0)), utc(2026, 6, 1, 3, 45)); // UTC+05:30
    s.time_zone = QStringLiteral("UTC");
    QCOMPARE(sl::next_occurrence_ms(s, utc(2026, 6, 1, 10, 0)), utc(2026, 6, 2, 9, 15));
    // An unknown zone falls back to the computer's zone (never no schedule at all).
    s.time_zone = QStringLiteral("Mars/Olympus");
    QVERIFY(sl::next_occurrence_ms(s, utc(2026, 6, 1, 10, 0)) > 0);
    QVERIFY(sl::zone_label(QTimeZone("Asia/Kolkata"), utc(2026, 6, 1, 0, 0)).contains(QStringLiteral("UTC+05:30")));
    QVERIFY(sl::zone_label(QTimeZone(kNy), utc(2026, 1, 1, 0, 0)).contains(QStringLiteral("UTC-05:00")));
}

void TestRunScheduling::weekly_interval_once() {
    RunSchedule w = daily(QStringLiteral("07:00"), 1 << 0); // Mondays
    w.kind = QString::fromLatin1(kScheduleWeekly);
    QCOMPARE(sl::next_occurrence_ms(w, at("America/New_York", 2026, 11, 17, 7, 0)), at("America/New_York", 2026, 11, 23, 7, 0));

    RunSchedule i;
    i.kind = QString::fromLatin1(kScheduleInterval);
    i.interval_minutes = 120;
    i.weekdays_mask = 0x7f;
    i.time_zone = QStringLiteral("UTC");
    i.created_at_ms = utc(2026, 11, 17, 9, 0);
    QCOMPARE(sl::next_occurrence_ms(i, utc(2026, 11, 17, 9, 0)), utc(2026, 11, 17, 11, 0));
    QCOMPARE(sl::next_occurrence_ms(i, utc(2026, 11, 17, 12, 59)), utc(2026, 11, 17, 13, 0));
    i.last_run_at_ms = utc(2026, 11, 17, 12, 10); // counted from the last run
    QCOMPARE(sl::next_occurrence_ms(i, utc(2026, 11, 17, 12, 10)), utc(2026, 11, 17, 14, 10));
    // Weekdays only: Friday 23:00 + 2h lands on Saturday → Monday 01:00 (on the 2-hour grid).
    i.weekdays_mask = 0x1f;
    i.last_run_at_ms = utc(2026, 11, 20, 23, 0);
    QCOMPARE(sl::next_occurrence_ms(i, utc(2026, 11, 20, 23, 0)), utc(2026, 11, 23, 1, 0));
    // Less than hourly is raised to hourly.
    i.weekdays_mask = 0x7f;
    i.interval_minutes = 5;
    QCOMPARE(sl::next_occurrence_ms(i, utc(2026, 11, 20, 23, 0)), utc(2026, 11, 21, 0, 0));

    RunSchedule o;
    o.kind = QString::fromLatin1(kScheduleOnce);
    o.once_at_ms = utc(2026, 12, 1, 9, 0);
    QCOMPARE(sl::next_occurrence_ms(o, utc(2026, 11, 30, 0, 0)), o.once_at_ms);
    QCOMPARE(sl::next_occurrence_ms(o, o.once_at_ms), qint64(0));
}

void TestRunScheduling::missed_runs_and_catch_up() {
    RunSchedule s = daily(QStringLiteral("08:00"), 0x7f, QStringLiteral("UTC"));
    s.catch_up_minutes = 120;
    s.next_run_at_ms = utc(2026, 11, 17, 8, 0);
    // Not due yet.
    sl::DueDecision d = sl::decide_due(s, utc(2026, 11, 17, 7, 59));
    QVERIFY(!d.fire && !d.missed);
    QCOMPARE(d.next_ms, s.next_run_at_ms);
    // On time (the timer a few seconds late).
    d = sl::decide_due(s, utc(2026, 11, 17, 8, 1));
    QVERIFY(d.fire && !d.caught_up);
    QCOMPARE(d.next_ms, utc(2026, 11, 18, 8, 0));
    // The terminal started 90 minutes late: caught up once.
    d = sl::decide_due(s, utc(2026, 11, 17, 9, 30));
    QVERIFY(d.fire && d.caught_up);
    // Three hours late: missed, not run; the next occurrence is tomorrow.
    d = sl::decide_due(s, utc(2026, 11, 17, 11, 0));
    QVERIFY(!d.fire && d.missed);
    QCOMPARE(d.missed_at_ms, utc(2026, 11, 17, 8, 0));
    QCOMPARE(d.next_ms, utc(2026, 11, 18, 8, 0));
    // Closed for three days, back 30 minutes after today's time: today's runs once.
    s.next_run_at_ms = utc(2026, 11, 14, 8, 0);
    d = sl::decide_due(s, utc(2026, 11, 17, 8, 30));
    QVERIFY(d.fire && d.caught_up);
    QCOMPARE(d.next_ms, utc(2026, 11, 18, 8, 0));
    // A window of 0 never catches up.
    s.catch_up_minutes = 0;
    d = sl::decide_due(s, utc(2026, 11, 17, 8, 30));
    QVERIFY(!d.fire && d.missed);
    // Disabled / paused for review: nothing happens.
    s.catch_up_minutes = 720;
    s.enabled = false;
    QVERIFY(!sl::decide_due(s, utc(2026, 11, 17, 8, 30)).fire);
    s.enabled = true;
    s.paused_for_review = true;
    QVERIFY(!sl::decide_due(s, utc(2026, 11, 17, 8, 30)).fire);
    // No next time yet: it is computed, nothing fires.
    s.paused_for_review = false;
    s.next_run_at_ms = 0;
    d = sl::decide_due(s, utc(2026, 11, 17, 8, 30));
    QVERIFY(!d.fire);
    QCOMPARE(d.next_ms, utc(2026, 11, 18, 8, 0));
}

void TestRunScheduling::overlap_and_daily_cap() {
    QVERIFY(sl::overlap_reason(std::nullopt).isEmpty());
    QVERIFY(sl::overlap_reason(RunStatus::Completed).isEmpty());
    QVERIFY(sl::overlap_reason(RunStatus::Failed).isEmpty());
    for (RunStatus st : {RunStatus::Queued, RunStatus::Running, RunStatus::WaitingInput, RunStatus::Paused})
        QVERIFY(!sl::overlap_reason(st).isEmpty());

    sl::CapDecision c = sl::apply_daily_cap(100, 0, 50, 15);
    QVERIFY(c.allowed);
    QCOMPARE(c.budget, 50.0);
    QVERIFY(c.reason.isEmpty());
    c = sl::apply_daily_cap(100, 70, 50, 15); // 30 left: the budget is lowered
    QVERIFY(c.allowed);
    QCOMPARE(c.budget, 30.0);
    QVERIFY(!c.reason.isEmpty());
    c = sl::apply_daily_cap(100, 90, 50, 15); // 10 left, a plan needs 15
    QVERIFY(!c.allowed);
    QVERIFY(c.reason.contains(QStringLiteral("100")));
    c = sl::apply_daily_cap(100, 20, 0, 15); // unlimited run → at most what is left
    QCOMPARE(c.budget, 80.0);
    c = sl::apply_daily_cap(0, 1000, 50, 15); // no cap
    QVERIFY(c.allowed);
    QCOMPARE(c.budget, 50.0);
}

void TestRunScheduling::validation_and_descriptions() {
    const qint64 now = utc(2026, 11, 17, 12, 0);
    RunSchedule s = daily(QStringLiteral("08:00"), 0x1f);
    QVERIFY(sl::validate_schedule(s, now).isEmpty());
    s.name.clear();
    s.daily_time = QStringLiteral("8 o'clock");
    s.weekdays_mask = 0;
    QCOMPARE(sl::validate_schedule(s, now).size(), 3);
    RunSchedule w = daily(QStringLiteral("08:00"), 0x3);
    w.kind = QString::fromLatin1(kScheduleWeekly);
    QVERIFY(!sl::validate_schedule(w, now).isEmpty()); // two days for a weekly schedule
    RunSchedule i = daily(QStringLiteral("08:00"));
    i.kind = QString::fromLatin1(kScheduleInterval);
    i.interval_minutes = 30;
    QVERIFY(!sl::validate_schedule(i, now).isEmpty());
    RunSchedule o = daily(QStringLiteral("08:00"));
    o.kind = QString::fromLatin1(kScheduleOnce);
    o.once_at_ms = now - 1000;
    QVERIFY(!sl::validate_schedule(o, now).isEmpty());
    RunSchedule z = daily(QStringLiteral("08:00"), 0x7f, QStringLiteral("Not/AZone"));
    QVERIFY(!sl::validate_schedule(z, now).isEmpty());

    QCOMPARE(sl::describe_recurrence(daily(QStringLiteral("08:00"), 0x1f)), QStringLiteral("Weekdays at 08:00"));
    QCOMPARE(sl::describe_recurrence(daily(QStringLiteral("18:30"))), QStringLiteral("Every day at 18:30"));
    RunSchedule e = daily(QStringLiteral("06:00"));
    e.kind = QString::fromLatin1(kScheduleInterval);
    e.interval_minutes = 240;
    e.weekdays_mask = 0x1f;
    QCOMPARE(sl::describe_recurrence(e), QStringLiteral("Every 4 hours (weekdays)"));
}

void TestRunScheduling::relative_dates() {
    const QDate today(2026, 11, 17);
    QJsonObject ctx{{"date_range", "7d"}};
    QJsonObject out = sl::expand_relative_dates(ctx, today);
    QCOMPARE(out.value("date_from").toString(), QStringLiteral("2026-11-10"));
    QCOMPARE(out.value("date_to").toString(), QStringLiteral("2026-11-17"));
    QVERIFY(out.value("period").toString().contains(QStringLiteral("last 7 days")));
    out = sl::expand_relative_dates(QJsonObject{{"date_range", "ytd"}}, today);
    QCOMPARE(out.value("date_from").toString(), QStringLiteral("2026-01-01"));
    // A schedule run a week later asks for its own last 7 days.
    out = sl::expand_relative_dates(ctx, today.addDays(7));
    QCOMPARE(out.value("date_from").toString(), QStringLiteral("2026-11-17"));
    // Custom dates stay as given and get a period phrase.
    out = sl::expand_relative_dates(QJsonObject{{"date_range", "custom"}, {"date_from", "2026-01-01"}, {"date_to", "2026-03-31"}},
                                    today);
    QCOMPARE(out.value("date_from").toString(), QStringLiteral("2026-01-01"));
    QVERIFY(out.value("period").toString().contains(QStringLiteral("2026-03-31")));
    QCOMPARE(sl::date_range_codes().size(), 7);
}

void TestRunScheduling::template_placeholders() {
    const QString prompt = QStringLiteral("Analyse <SYMBOL> over <period>.\nExtra focus: <FOCUS>.\nKeep <Unknown Thing> as is.");
    const QJsonObject ctx{{"symbol", "AAPL"}, {"period", "the last 30 days"}, {"focus", ""}};
    const QString filled = fill_run_template(prompt, ctx);
    QCOMPARE(filled, QStringLiteral("Analyse AAPL over the last 30 days.\nKeep <Unknown Thing> as is."));
    QCOMPARE(run_template_placeholders(prompt), (QStringList{"SYMBOL", "period", "FOCUS", "Unknown Thing"}));
    QCOMPARE(run_template_placeholders(filled), QStringList{QStringLiteral("Unknown Thing")});
    // Arrays join, spaces and underscores are the same, "<5%" is not a placeholder.
    const QString p2 = fill_run_template(QStringLiteral("Peers: <PEER LIST>; moves <5% ignored"),
                                         QJsonObject{{"peer_list", QJsonArray{"MSFT", "GOOGL"}}});
    QCOMPARE(p2, QStringLiteral("Peers: MSFT, GOOGL; moves <5% ignored"));
}

void TestRunScheduling::template_validation_and_request() {
    const auto t = find_run_template(QStringLiteral("equity_deep_dive"));
    QVERIFY(t.has_value());
    QJsonObject ctx = template_default_context(*t);
    QStringList errors = validate_template_params(*t, ctx);
    QCOMPARE(errors.size(), 1); // the symbol is missing
    QVERIFY(errors.first().contains(QStringLiteral("symbol")));
    ctx.insert(QStringLiteral("symbol"), QStringLiteral("AAPL"));
    QVERIFY(validate_template_params(*t, ctx).isEmpty());
    const RunRequest r = request_from_template(*t, ctx);
    QCOMPARE(r.title, QStringLiteral("Deep-dive: AAPL"));
    QVERIFY(r.prompt.contains(QStringLiteral("on AAPL for an investor")));
    QVERIFY(!r.prompt.contains(QStringLiteral("Extra focus"))); // optional, left empty: the line is dropped
    QVERIFY(r.prompt.contains(QStringLiteral("Suggested approach")));
    QVERIFY(r.prompt.contains(QStringLiteral("Expected output")));
    QVERIFY(run_template_placeholders(r.prompt).isEmpty());
    QCOMPARE(r.template_id, QStringLiteral("equity_deep_dive"));
    QVERIFY(r.plan_first);
    QCOMPARE(r.budget.max_credits, 50.0);
    QCOMPARE(r.context.value("symbol").toString(), QStringLiteral("AAPL"));
    QVERIFY(!r.context.contains(QStringLiteral("focus"))); // empty parameters are not context

    // A portfolio template needs a portfolio; a custom period needs both dates in order.
    const auto pr = find_run_template(QStringLiteral("portfolio_review"));
    QVERIFY(pr.has_value());
    QJsonObject pc = template_default_context(*pr);
    QCOMPARE(pc.value("date_range").toString(), QStringLiteral("30d"));
    QVERIFY(validate_template_params(*pr, pc).join(' ').contains(QStringLiteral("portfolio")));
    pc.insert("portfolio_id", "p1");
    pc.insert("portfolio_name", "Growth");
    QVERIFY(validate_template_params(*pr, pc).isEmpty());
    pc.insert("date_range", "custom");
    pc.insert("date_from", "2026-05-01");
    pc.insert("date_to", "2026-04-01");
    QVERIFY(validate_template_params(*pr, pc).join(' ').contains(QStringLiteral("starts after")));

    // A choice must be one of its values.
    const auto er = find_run_template(QStringLiteral("earnings_preview_review"));
    QJsonObject ec{{"symbol", "MSFT"}, {"phase", "rumour"}};
    QVERIFY(!validate_template_params(*er, ec).isEmpty());
    ec.insert("phase", "review");
    QVERIFY(validate_template_params(*er, ec).isEmpty());
    QCOMPARE(request_from_template(*er, ec).title, QStringLiteral("Earnings review: MSFT"));
}

void TestRunScheduling::builtin_templates_are_complete() {
    const QVector<RunTemplate> all = run_templates();
    QCOMPARE(all.size(), 8);
    QSet<QString> ids;
    for (const RunTemplate& t : all) {
        QVERIFY2(!ids.contains(t.id), qPrintable(t.id));
        ids.insert(t.id);
        QVERIFY(!t.title.isEmpty() && !t.description.isEmpty() && !t.plan_hint.isEmpty());
        QVERIFY(!t.expected_artefacts.isEmpty());
        QVERIFY(t.budget.max_credits > 0 && t.budget.max_steps > 0 && t.budget.max_minutes > 0);
        // With every parameter filled, nothing is left unfilled and it validates.
        QJsonObject ctx = template_default_context(t);
        for (const RunTemplateParam& p : t.params) {
            switch (p.type) {
                case TemplateParamType::Portfolio:
                    ctx.insert("portfolio_id", "p1");
                    ctx.insert("portfolio_name", "Growth");
                    break;
                case TemplateParamType::Watchlist:
                    ctx.insert("watchlist_id", "w1");
                    ctx.insert("watchlist_name", "Tech");
                    break;
                case TemplateParamType::DateRange:
                    ctx = sl::expand_relative_dates(ctx, QDate(2026, 11, 17));
                    break;
                case TemplateParamType::Symbol:
                case TemplateParamType::Symbols:
                case TemplateParamType::Text:
                case TemplateParamType::LongText:
                    if (!ctx.contains(p.key))
                        ctx.insert(p.key, QStringLiteral("X"));
                    break;
                case TemplateParamType::Choice:
                    break;
            }
        }
        QVERIFY2(validate_template_params(t, ctx).isEmpty(), qPrintable(t.id + ": " + validate_template_params(t, ctx).join(' ')));
        const RunRequest r = request_from_template(t, ctx);
        QVERIFY2(run_template_placeholders(r.prompt).isEmpty(), qPrintable(t.id));
        QVERIFY2(run_template_placeholders(r.title).isEmpty(), qPrintable(t.id));
        QVERIFY(r.prompt.size() < kRunPromptMaxChars);
    }
}

void TestRunScheduling::user_template_json_round_trip() {
    RunTemplate t = *find_run_template(QStringLiteral("market_wrap"));
    t.id = QStringLiteral("user:abc");
    t.title = QStringLiteral("My Asia wrap");
    t.prompt += QStringLiteral("\nAlways include the yen.");
    t.budget.max_credits = 20;
    const RunTemplate back = run_template_from_json(run_template_to_json(t));
    QCOMPARE(back.id, t.id);
    QCOMPARE(back.title, t.title);
    QCOMPARE(back.prompt, t.prompt);
    QCOMPARE(back.budget.max_credits, 20.0);
    QCOMPARE(back.params.size(), t.params.size());
    QCOMPARE(back.params.at(0).choices, t.params.at(0).choices);
    QCOMPARE(back.params.at(1).type, TemplateParamType::DateRange);
    QCOMPARE(back.expected_artefacts, t.expected_artefacts);
    QVERIFY(back.user_defined);
    QVERIFY(back.context_keys.contains(QStringLiteral("date_from")));
}

void TestRunScheduling::cost_estimate_and_confirmation() {
    // One cloud reply: 5 credits, no confirmation.
    RunCostEstimate e = estimate_run_cost(QString::fromLatin1(kExecutorCloudTurn), {}, 12);
    QCOMPARE(e.credits, kCloudTurnCredits);
    QVERIFY(!e.needs_confirmation);
    // The default task budget (50 credits / 8 steps): up to 8 steps → 50 credits → confirm.
    e = estimate_run_cost(QString::fromLatin1(kExecutorCloudPlan), {kDefaultMaxCredits, kDefaultMaxSteps, 0}, 12);
    QCOMPARE(e.steps, 8);
    QCOMPARE(e.credits, 50.0);
    QVERIFY(e.needs_confirmation);
    // 25 credits pays for 3 steps (+ plan + report) = 25: at the threshold, no confirmation.
    e = estimate_run_cost(QString::fromLatin1(kExecutorCloudPlan), {25, 8, 0}, 12);
    QCOMPARE(e.steps, 3);
    QCOMPARE(e.credits, 25.0);
    QVERIFY(!e.needs_confirmation);
    // 3 steps without a credit limit: 25 credits, no confirmation; unlimited steps: the hard cap.
    e = estimate_run_cost(QString::fromLatin1(kExecutorCloudPlan), {0, 3, 0}, 12);
    QCOMPARE(e.credits, 25.0);
    QVERIFY(e.unlimited);
    e = estimate_run_cost(QString::fromLatin1(kExecutorCloudPlan), {0, 0, 0}, 12);
    QCOMPARE(e.steps, 12);
    QCOMPARE(e.credits, 70.0);
    QVERIFY(e.needs_confirmation);
    // The local engine costs no credits.
    e = estimate_run_cost(QString::fromLatin1(kExecutorLocal), {0, 6, 0}, 12);
    QVERIFY(!e.uses_credits);
    QCOMPARE(e.credits, 0.0);
    QVERIFY(!e.needs_confirmation);
}

void TestRunScheduling::working_clock_stops_while_paused() {
    TaskRun r;
    r.status = RunStatus::Queued;
    account_active_time(r, RunStatus::Running, 1000); // starts working
    r.status = RunStatus::Running;
    r.started_at_ms = 1000;
    QCOMPARE(run_active_ms(r, 61000), qint64(60000));
    account_active_time(r, RunStatus::Paused, 61000); // paused after a minute
    r.status = RunStatus::Paused;
    QCOMPARE(run_active_ms(r, 61000), qint64(60000));
    QCOMPARE(run_active_ms(r, 9'000'000), qint64(60000)); // stands still while paused
    account_active_time(r, RunStatus::Queued, 9'000'000);
    r.status = RunStatus::Queued;
    account_active_time(r, RunStatus::Running, 9'010'000);
    r.status = RunStatus::Running;
    QCOMPARE(run_active_ms(r, 9'040'000), qint64(90000)); // 60 s + 30 s
    // The time budget uses the working clock: a long pause does not stop the run.
    r.budget.max_minutes = 2;
    QVERIFY(!check_budget(r, 0, 9'040'000).exceeded);
    QVERIFY(check_budget(r, 0, 9'010'000 + 61'000).exceeded); // 60 + 61 s > 2 min
    // A run recorded before the clock existed: started → the moment it stopped moving.
    TaskRun legacy;
    legacy.status = RunStatus::Paused;
    legacy.started_at_ms = 1000;
    legacy.updated_at_ms = 31000;
    QCOMPARE(run_active_ms(legacy, 9'000'000), qint64(30000));
}

void TestRunScheduling::login_gate_and_cap_per_engine() {
    using namespace fincept::agent_tasks::schedule_logic;
    const QString cloud = QString::fromLatin1(kEngineCloud);
    const QString local = QString::fromLatin1(kEngineLocal);
    const QString harness = QStringLiteral("harness"); // agent_tasks::kEngineHarness
    // The cloud agent needs a Fincept sign-in; the user's own model / API key does not.
    QVERIFY(engine_needs_fincept_login(cloud));
    QVERIFY(engine_needs_fincept_login(QString())); // an old schedule without an engine is a cloud one
    QVERIFY(!engine_needs_fincept_login(local));
    QVERIFY(!engine_needs_fincept_login(harness));
    QVERIFY(engine_needs_fincept_login(QStringLiteral("something-new"))); // unknown: the safe side
    // Signed out: a cloud occurrence waits (with a reason), local and harness start.
    QVERIFY(!start_blocker(cloud, false).isEmpty());
    QVERIFY(start_blocker(cloud, true).isEmpty());
    QVERIFY(start_blocker(local, false).isEmpty());
    QVERIFY(start_blocker(harness, false).isEmpty());
    // Only cloud runs count toward the day's credit cap.
    QVERIFY(engine_spends_credits(cloud));
    QVERIFY(!engine_spends_credits(local));
    QVERIFY(!engine_spends_credits(harness));
    // A harness schedule is a valid schedule like any other.
    RunSchedule s;
    s.name = QStringLiteral("Morning wrap (harness)");
    s.engine = harness;
    s.kind = QString::fromLatin1(kScheduleDaily);
    s.daily_time = QStringLiteral("08:00");
    s.weekdays_mask = 31;
    s.time_zone = QString::fromLatin1(kNy);
    s.prompt = QStringLiteral("Summarise the US session.");
    QVERIFY2(validate_schedule(s, QDateTime(QDate(2026, 3, 2), QTime(7, 0), QTimeZone(kNy)).toMSecsSinceEpoch()).isEmpty(),
             qPrintable(validate_schedule(s, 0).join(QLatin1Char('|'))));
}

QTEST_GUILESS_MAIN(TestRunScheduling)
#include "tst_run_scheduling.moc"
