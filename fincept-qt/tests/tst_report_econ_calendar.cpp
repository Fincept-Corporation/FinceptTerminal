// tst_report_econ_calendar.cpp — the Report Builder's "Economic calendar" data source
// (services/report_builder/ReportEconCalendar.h): the recipe and its {{date}} / period
// variables, the per-day connector replies (recorded fincept_connector.py envelopes of the
// data_economic_calendar connector, parsed by the real envelope parser — no network, no
// Python), the call plan (first day alone, then the feed's week), the table / list the block
// shows, its empty and failed states, and the Weekly Market Wrap template. Qt Core only.

#include "core/report/ReportBinding.h"
#include "core/report/ReportBlocks.h"
#include "core/report/ReportDocument.h"
#include "core/report/ReportTableCodec.h"
#include "core/report/ReportVariables.h"
#include "services/connectors/ConnectorEnvelope.h"
#include "services/report_builder/ReportBuilderTemplates.h"
#include "services/report_builder/ReportEconCalendar.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace fincept;
namespace ec = fincept::services::econcal;
namespace ds = fincept::services::datasources;
namespace conn = fincept::services::connectors;

namespace {

const QDate kToday(2026, 11, 17); // a Tuesday; the feed's week is Mon 16 – Fri 20 Nov

QJsonObject ev(const QString& time, const QString& ccy, int impact, const QString& label, const QString& title,
               const QString& forecast = {}, const QString& previous = {}, const QString& day = QStringLiteral("2026-11-17")) {
    return QJsonObject{{"time", time},
                       {"currency", ccy},
                       {"impact", impact},
                       {"impact_label", label},
                       {"event", title},
                       {"forecast", forecast},
                       {"previous", previous},
                       {"datetime", day + QLatin1Char('T') + time + QStringLiteral("+05:30")}};
}

/// The envelope `fincept_connector.py run economic_calendar.py day <date>` prints on success
/// (shape recorded from a real run: nested_table under "events", meta.extra with the feed's week).
QJsonObject ok_envelope(const QString& date, const QJsonArray& rows, int feed_age = 38) {
    const QJsonArray cols{"time", "currency", "impact", "impact_label", "event", "forecast", "previous", "datetime"};
    const QJsonObject types{{"time", "string"},     {"currency", "string"}, {"impact", "integer"},
                            {"impact_label", "string"}, {"event", "string"}, {"forecast", "string"},
                            {"previous", "string"}, {"datetime", "datetime"}};
    const QJsonObject extra{{"shape", "nested_table"},
                            {"rows_key", "events"},
                            {"date", date},
                            {"week_start", "2026-11-16"},
                            {"week_end", "2026-11-20"},
                            {"timezone", "UTC+05:30"},
                            {"feed_age_seconds", feed_age},
                            {"note", "Forex Factory weekly export: current week only; forecast and previous, no actuals"}};
    const QJsonObject meta{{"source", "Economic calendar (Forex Factory)"},
                           {"script", "economic_calendar.py"},
                           {"command", "day"},
                           {"as_of", "2026-11-17T05:54:46Z"},
                           {"count", rows.size()},
                           {"truncated", false},
                           {"extra", extra}};
    return QJsonObject{{"ok", true}, {"data", rows}, {"columns", cols}, {"types", types}, {"meta", meta}};
}

QJsonObject fail_envelope(const QString& message, const QString& code) {
    return QJsonObject{{"ok", false}, {"error", message}, {"code", code}};
}

/// One day's reply as the binder sees it (ConnectorService parses with the same function).
ec::DayResult reply(const QDate& day, const QJsonObject& envelope) {
    const conn::EnvelopeResult env = conn::parse_connector_envelope_object(envelope);
    return ec::day_from_connector(day, env.ok, env.error, env.data);
}

const QString kOutOfWeek = QStringLiteral("Forex Factory's public calendar feed covers this week only (2026-11-16 to 2026-11-20)");

ec::Request request(const QString& date, const QString& period, const QString& impact = QStringLiteral("medium"),
                    const QString& currencies = {}, int limit = 0) {
    report::Binding b = report::econ_calendar_binding(date, period, impact);
    if (!currencies.isEmpty())
        b.params.insert(QStringLiteral("currencies"), currencies);
    if (limit > 0)
        b.params.insert(QStringLiteral("limit"), limit);
    return ec::request_of(b, kToday);
}

/// A week: Mon two events, Tue three, Wed nothing, Sat / Sun outside the feed's week.
QVector<ec::DayResult> week_replies() {
    QVector<ec::DayResult> out;
    out << reply(QDate(2026, 11, 16),
                 ok_envelope("2026-11-16", QJsonArray{ev("19:00", "EUR", 3, "High", "ECB President Lagarde Speaks", {}, {}, "2026-11-16"),
                                                      ev("03:30", "AUD", 1, "Low", "New Motor Vehicle Sales m/m", {}, "1.2%", "2026-11-16")}));
    out << reply(QDate(2026, 11, 17),
                 ok_envelope("2026-11-17", QJsonArray{ev("19:00", "USD", 3, "High", "CPI m/m", "0.3%", "0.4%"),
                                                      ev("12:30", "GBP", 2, "Medium", "Average Earnings Index 3m/y", "4.6%", "4.8%"),
                                                      ev("00:45", "USD", 1, "Low", "FOMC Member Hammack Speaks")},
                             12));
    out << reply(QDate(2026, 11, 18), fail_envelope("The connector returned no rows for this request", "no_data"));
    out << reply(QDate(2026, 11, 21), fail_envelope(kOutOfWeek, "unknown"));
    out << reply(QDate(2026, 11, 22), fail_envelope(kOutOfWeek, "unknown"));
    return out;
}

} // namespace

class TestReportEconCalendar : public QObject {
    Q_OBJECT

  private slots:
    // ── The recipe ───────────────────────────────────────────────────────────
    void recipe_follows_the_date_variable() {
        // A block bound to {{date}} follows the report's date; the period sets the range.
        const report::Binding b = report::econ_calendar_binding();
        QCOMPARE(b.param("date"), QStringLiteral("{{date}}"));
        report::ReportMetadata m;
        m.date = QStringLiteral("2026-11-19");
        const ec::Request r = ec::request_of(report::vars::expand_binding(b, report::vars::context(m)), kToday);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.anchor, QDate(2026, 11, 19));
        QCOMPARE(r.period, ec::Period::Week);
        QCOMPARE(r.days.size(), 7);
        QCOMPARE(r.days.first(), QDate(2026, 11, 16)); // Monday
        QCOMPARE(r.days.last(), QDate(2026, 11, 22));  // Sunday
        QCOMPARE(r.min_impact, 2);
        // No report date: the reference stays unresolved, the recipe reads it as today.
        report::ReportMetadata none;
        const ec::Request t = ec::request_of(report::vars::expand_binding(b, report::vars::context(none)), kToday);
        QVERIFY(t.ok);
        QCOMPARE(t.anchor, kToday);
        // {{period}} works as well (a report variable "day").
        report::Binding bp = report::econ_calendar_binding(QStringLiteral("{{date}}"), QStringLiteral("{{period}}"));
        m.variables.insert(QStringLiteral("period"), QStringLiteral("Day"));
        const ec::Request p = ec::request_of(report::vars::expand_binding(bp, report::vars::context(m)), kToday);
        QVERIFY(p.ok);
        QCOMPARE(p.period, ec::Period::Day);
        QCOMPARE(p.days, QVector<QDate>({QDate(2026, 11, 19)}));
    }

    void recipe_dates_periods_and_filters() {
        QCOMPARE(request(QString(), QStringLiteral("day")).days, QVector<QDate>({kToday}));
        const ec::Request rest = request(QStringLiteral("2026-11-19"), QStringLiteral("rest_of_week"));
        QCOMPARE(rest.days.size(), 4);
        QCOMPARE(rest.days.first(), QDate(2026, 11, 19));
        QCOMPARE(ec::range_label(rest), QStringLiteral("19 Nov – 22 Nov 2026"));
        QCOMPARE(ec::range_label(request(QString(), QStringLiteral("week"))), QStringLiteral("week of 16 Nov 2026"));
        QCOMPARE(ec::range_label(request(QString(), QStringLiteral("day"))), QStringLiteral("Tue 17 Nov 2026"));
        QCOMPARE(request(QStringLiteral("this week"), QStringLiteral("week")).ok, false); // not a date
        QCOMPARE(request(QStringLiteral("week ahead"), {}).ok, false);
        QCOMPARE(request(QString(), QStringLiteral("week ahead")).period, ec::Period::RestOfWeek);
        // Date forms a report date may be typed in.
        QCOMPARE(ec::parse_day(QStringLiteral("17 Nov 2026"), kToday), kToday);
        QCOMPARE(ec::parse_day(QStringLiteral("Nov 17, 2026"), kToday), kToday);
        QCOMPARE(ec::parse_day(QStringLiteral("2026-11-18T10:00:00Z"), kToday), QDate(2026, 11, 18));
        QCOMPARE(ec::parse_day(QStringLiteral("today"), kToday), kToday);
        QVERIFY(!ec::parse_day(QStringLiteral("2026-11-18x"), kToday).isValid());
        // Refused before any script runs, with the reason.
        const ec::Request bad = request(QStringLiteral("next tuesday"), QStringLiteral("week"));
        QVERIFY(!bad.ok);
        QVERIFY(bad.error.contains(QStringLiteral("next tuesday")));
        const ec::Request far = request(QStringLiteral("2026-10-01"), QStringLiteral("week"));
        QVERIFY(!far.ok);
        QVERIFY(far.error.contains(QStringLiteral("current week only")));
        QVERIFY(far.error.contains(QStringLiteral("week of 28 Sep 2026")));
        QVERIFY(!request(QString(), QStringLiteral("fortnight")).ok);
        QVERIFY(!request(QString(), QStringLiteral("week"), QStringLiteral("huge")).ok);
        QCOMPARE(request(QString(), QStringLiteral("week"), QStringLiteral("high")).min_impact, 3);
        QCOMPARE(request(QString(), QStringLiteral("week"), QStringLiteral("all")).min_impact, 0);
        QCOMPARE(request(QString(), QStringLiteral("week"), QStringLiteral("medium"), QStringLiteral("usd, eur;gbp usd")).currencies,
                 QStringList({"USD", "EUR", "GBP"}));
        const ec::Request badccy = request(QString(), QStringLiteral("week"), QStringLiteral("medium"), QStringLiteral("US1"));
        QVERIFY(!badccy.ok);
        QVERIFY(badccy.error.contains(QStringLiteral("US1")));
        QCOMPARE(request(QString(), QStringLiteral("week"), QStringLiteral("medium"), {}, 999).limit, ec::kMaxLimit);
    }

    void binding_round_trip_and_blocks() {
        const report::Binding b = report::econ_calendar_binding(QString(), QStringLiteral("day"), QStringLiteral("high"),
                                                                QString::fromLatin1(report::view::kList));
        const report::Binding back = report::Binding::parse(b.to_string());
        QVERIFY(back == b);
        QVERIFY(back.is_refreshable());
        QCOMPARE(report::describe(back), QStringLiteral("Economic calendar (one day)"));
        QCOMPARE(report::describe(report::econ_calendar_binding()), QStringLiteral("Economic calendar (week)"));
        QCOMPARE(report::default_max_age_seconds(back), 6 * 3600);
        QCOMPARE(report::blocks::bound(back).type, QStringLiteral("list"));
        QCOMPARE(report::blocks::bound(report::econ_calendar_binding()).type, QStringLiteral("table"));
    }

    // ── Connector replies ────────────────────────────────────────────────────
    void connector_replies_classified() {
        const ec::DayResult ok =
            reply(kToday, ok_envelope("2026-11-17", QJsonArray{ev("19:00", "USD", 3, "High", "CPI m/m", "0.3%", "0.4%")}));
        QCOMPARE(ok.status, ec::DayStatus::Ok);
        QCOMPARE(ok.week_start, QDate(2026, 11, 16));
        QCOMPARE(ok.week_end, QDate(2026, 11, 20));
        QCOMPARE(ok.timezone, QStringLiteral("UTC+05:30"));
        // as_of = when the feed copy was taken: the run time minus the script's cache age.
        QCOMPARE(ok.as_of, QDateTime(QDate(2026, 11, 17), QTime(5, 54, 8), QTimeZone::UTC));
        const QVector<ec::Event> events = ec::events_of(ok);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events[0].impact, 3);
        QCOMPARE(events[0].forecast, QStringLiteral("0.3%"));
        QCOMPARE(events[0].day, kToday);
        // No events that day: the source answered, nothing to show.
        QCOMPARE(reply(kToday, fail_envelope("The connector returned no rows", "no_data")).status, ec::DayStatus::Empty);
        QCOMPARE(reply(kToday, ok_envelope("2026-11-17", QJsonArray{})).status, ec::DayStatus::Empty);
        // The script's own error (a day outside the feed's week) and a feed outage.
        const ec::DayResult out = reply(QDate(2026, 11, 22), fail_envelope(kOutOfWeek, "unknown"));
        QCOMPARE(out.status, ec::DayStatus::Failed);
        QCOMPARE(out.error, kOutOfWeek);
        QVERIFY(!ec::is_systemic(out));
        const ec::DayResult limited =
            reply(kToday, fail_envelope("Forex Factory answered HTTP 429 (rate limited)", "rate_limited"));
        QVERIFY(ec::is_systemic(limited));
        QVERIFY(ec::is_systemic(reply(kToday, fail_envelope("The feed could not be reached", "upstream_down"))));
        // A cancelled run is neither data nor an error.
        QCOMPARE(ec::day_from_connector(kToday, false, ds::SourceError::cancel(), {}).status, ec::DayStatus::Cancelled);
    }

    void call_plan() {
        const ec::Request week = request(QString(), QStringLiteral("week"));
        QCOMPARE(ec::first_day(week, kToday), kToday); // the day surely inside the feed's week
        QCOMPARE(ec::first_day(request(QStringLiteral("2026-11-19"), QStringLiteral("day")), kToday), QDate(2026, 11, 19));
        // After an Ok first reply: only the other days of the feed's week (Sat / Sun skipped).
        bool stop = true;
        const ec::DayResult first = week_replies()[1];
        QCOMPARE(ec::remaining_days(week, first, &stop),
                 QVector<QDate>({QDate(2026, 11, 16), QDate(2026, 11, 18), QDate(2026, 11, 19), QDate(2026, 11, 20)}));
        QVERIFY(!stop);
        // No events on the first day: the week is unknown, ask every other day.
        const ec::DayResult empty = reply(kToday, fail_envelope("no rows", "no_data"));
        QCOMPARE(ec::remaining_days(week, empty, &stop).size(), 6);
        QVERIFY(!stop);
        // A failure that would repeat on every day: stop.
        QVERIFY(ec::remaining_days(week, reply(kToday, fail_envelope("down", "upstream_down")), &stop).isEmpty());
        QVERIFY(stop);
        QVERIFY(ec::remaining_days(week, ec::day_from_connector(kToday, false, ds::SourceError::cancel(), {}), &stop).isEmpty());
        QVERIFY(stop);
    }

    // ── The block ────────────────────────────────────────────────────────────
    void table_for_a_week() {
        const ec::Request r = request(QString(), QStringLiteral("week"));
        const ec::Shaped s = ec::shape(r, week_replies(), QStringLiteral("table"));
        QVERIFY2(s.ok, qPrintable(s.error));
        QCOMPARE(s.title, QStringLiteral("Economic calendar — week of 16 Nov 2026"));
        QCOMPARE(s.source_label, QStringLiteral("Forex Factory"));
        // The newest feed copy: Tuesday's reply (cache age 12 s).
        QCOMPARE(s.as_of, QDateTime(QDate(2026, 11, 17), QTime(5, 54, 34), QTimeZone::UTC));
        const QVector<QStringList> grid = report::table::parse(s.patch.value("csv"));
        QCOMPARE(grid.size(), 4); // header + three medium / high events (low left out)
        QCOMPARE(grid[0], QStringList({"Day", "Time", "Currency", "Impact", "Event", "Forecast", "Previous"}));
        QCOMPARE(grid[1], QStringList({"Mon 16 Nov", "19:00", "EUR", "High", "ECB President Lagarde Speaks", "—", "—"}));
        QCOMPARE(grid[2], QStringList({"Tue 17 Nov", "12:30", "GBP", "Medium", "Average Earnings Index 3m/y", "4.6%", "4.8%"}));
        QCOMPARE(grid[3], QStringList({"Tue 17 Nov", "19:00", "USD", "High", "CPI m/m", "0.3%", "0.4%"}));
        QCOMPARE(s.patch.value("rows"), QStringLiteral("4"));
        QCOMPARE(s.patch.value("cols"), QStringLiteral("7"));
        const QString note = s.patch.value("note");
        QVERIFY(note.contains(QStringLiteral("UTC+05:30")));
        QVERIFY(note.contains(QStringLiteral("no actual figures")));
        QVERIFY(note.contains(QStringLiteral("Medium- and high-impact")));
        QVERIFY(!note.contains(QStringLiteral("Not loaded"))); // Sat / Sun are outside the feed's week
        QVERIFY(!s.has_content);

        // Currency filter, row limit, a day inside the week that failed.
        const ec::Request usd = request(QString(), QStringLiteral("week"), QStringLiteral("all"), QStringLiteral("USD"), 1);
        QVector<ec::DayResult> replies = week_replies();
        replies << reply(QDate(2026, 11, 19), fail_envelope("The feed could not be reached", "upstream_down"));
        const ec::Shaped f = ec::shape(usd, replies, QStringLiteral("table"));
        QVERIFY(f.ok);
        const QVector<QStringList> g = report::table::parse(f.patch.value("csv"));
        QCOMPARE(g.size(), 2);
        QCOMPARE(g[1].value(4), QStringLiteral("FOMC Member Hammack Speaks")); // 00:45 sorts first
        const QString n = f.patch.value("note");
        QVERIFY(n.contains(QStringLiteral("Currencies: USD.")));
        QVERIFY(n.contains(QStringLiteral("Showing the first 1 of 2 events.")));
        QVERIFY(n.contains(QStringLiteral("Not loaded: Thu 19 Nov (The feed could not be reached).")));
    }

    void list_for_a_day() {
        const ec::Request r = request(QString(), QStringLiteral("day"), QStringLiteral("high"));
        const ec::Shaped s = ec::shape(r, {week_replies()[1]}, QStringLiteral("list"));
        QVERIFY(s.ok);
        QVERIFY(s.has_content);
        QCOMPARE(s.content, QStringLiteral("19:00 — USD, High: CPI m/m (forecast 0.3%, previous 0.4%)"));
        QCOMPARE(s.title, QStringLiteral("Economic calendar — Tue 17 Nov 2026"));
        QVERIFY(s.patch.isEmpty());
        // A day table has no Day column.
        const ec::Shaped t = ec::shape(r, {week_replies()[1]}, QStringLiteral("table"));
        QCOMPARE(report::table::parse(t.patch.value("csv"))[0].first(), QStringLiteral("Time"));
    }

    void empty_and_failed_states() {
        // The source answered with nothing: the designed "no events" sentence.
        const ec::Request day = request(QString(), QStringLiteral("day"));
        const ec::Shaped e = ec::shape(day, {reply(kToday, fail_envelope("no rows", "no_data"))}, QStringLiteral("table"));
        QVERIFY(!e.ok);
        QCOMPARE(e.error, QStringLiteral("no economic events on Tue 17 Nov 2026"));
        // Events, but none left after the filters: say which filters.
        const ec::Shaped f =
            ec::shape(request(QString(), QStringLiteral("week"), QStringLiteral("high"), QStringLiteral("JPY")),
                      week_replies(), QStringLiteral("table"));
        QVERIFY(!f.ok);
        QCOMPARE(f.error, QStringLiteral("no high-impact economic events for JPY in the week of 16 Nov 2026"));
        // Every day failed: the connector's own sentence.
        const ec::Request week = request(QString(), QStringLiteral("week"));
        const ec::Shaped x = ec::shape(week, {reply(QDate(2026, 11, 21), fail_envelope(kOutOfWeek, "unknown"))},
                                       QStringLiteral("list"));
        QVERIFY(!x.ok);
        QCOMPARE(x.error, kOutOfWeek);
        // Cancelled replies alone: nothing to say but that nothing came back.
        const ec::Shaped c =
            ec::shape(week, {ec::day_from_connector(kToday, false, ds::SourceError::cancel(), {})}, QStringLiteral("table"));
        QVERIFY(!c.ok);
        QVERIFY(!c.error.isEmpty());
    }

    // ── Template ─────────────────────────────────────────────────────────────
    void weekly_wrap_has_the_calendar() {
        const report::ReportDocument d = services::report_templates::build(QStringLiteral("Weekly Market Wrap"),
                                                                          report::themes::light_professional());
        int heading = -1;
        int block = -1;
        for (int i = 0; i < d.components.size(); ++i) {
            const report::ReportComponent& c = d.components[i];
            if (c.type == QLatin1String("heading") && c.content == QLatin1String("Economic calendar"))
                heading = i;
            if (report::binding_of(c).source == QLatin1String(report::source::kEconCalendar))
                block = i;
        }
        QVERIFY(heading >= 0);
        QCOMPARE(block, heading + 1);
        const report::ReportComponent& c = d.components[block];
        QCOMPARE(c.type, QStringLiteral("table"));
        const report::Binding b = report::binding_of(c);
        QCOMPARE(b.param("period"), QStringLiteral("week"));
        QCOMPARE(b.param("impact"), QStringLiteral("high"));
        QCOMPARE(b.param("date"), QString()); // the week of each refresh (scheduled wraps stay current)
        // The block's recipe reads as a valid request on the day the template is used.
        QVERIFY(ec::request_of(ReportVarsExpand(b, d), QDate::currentDate()).ok);
        QVERIFY(!report::vars::unresolved_in(d).contains(QStringLiteral("date")));
    }

  private:
    static report::Binding ReportVarsExpand(const report::Binding& b, const report::ReportDocument& d) {
        return report::vars::expand_binding(b, report::vars::context(d.metadata));
    }
};

QTEST_GUILESS_MAIN(TestReportEconCalendar)
#include "tst_report_econ_calendar.moc"
