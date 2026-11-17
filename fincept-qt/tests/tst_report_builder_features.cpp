// tst_report_builder_features.cpp — the Report Builder's second-stage core (Qt Core only,
// header-only sources): formatted tables (column formats, totals, colour scales, round trips,
// old tables unchanged), {{variables}} and the heading outline, scheduled-regeneration maths
// (due / missed / on start), stale-save revisions, and the document JSON (new metadata fields
// round-trip; files written before them load unchanged).

#include "core/report/ReportBinding.h"
#include "core/report/ReportChartSpec.h"
#include "core/report/ReportDocument.h"
#include "core/report/ReportSchedule.h"
#include "core/report/ReportTableFormat.h"
#include "core/report/ReportVariables.h"
#include "services/report_builder/ReportLibraryLogic.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <cmath>

using namespace fincept;
using report::ReportComponent;
using report::ReportDocument;
namespace tf = fincept::report::tablefmt;
namespace sch = fincept::report::schedule;

namespace {

ReportComponent mk(int id, const QString& type, const QString& content = {}, QMap<QString, QString> cfg = {}) {
    ReportComponent c;
    c.id = id;
    c.type = type;
    c.content = content;
    c.config = cfg;
    return c;
}

QStringList texts_of_row(const tf::FormattedTable& t, int r) {
    QStringList out;
    for (const tf::Cell& c : t.rows[r])
        out << c.text;
    return out;
}

} // namespace

class TestReportBuilderFeatures : public QObject {
    Q_OBJECT

  private slots:
    // ── Numbers in cells ─────────────────────────────────────────────────────
    void parse_number_forms() {
        bool pct = false;
        QCOMPARE(tf::parse_number(QStringLiteral("1,234.5")), 1234.5);
        QCOMPARE(tf::parse_number(QStringLiteral("(1,234)")), -1234.0);
        QCOMPARE(tf::parse_number(QStringLiteral("−12.5")), -12.5);
        QCOMPARE(tf::parse_number(QStringLiteral("$1,234.50")), 1234.5);
        QCOMPARE(tf::parse_number(QStringLiteral("USD 1,234")), 1234.0);
        QCOMPARE(tf::parse_number(QStringLiteral("1,234 EUR")), 1234.0);
        QCOMPARE(tf::parse_number(QStringLiteral("12.3%"), &pct), 12.3);
        QVERIFY(pct);
        QCOMPARE(tf::parse_number(QStringLiteral("1.2B")), 1.2e9);
        QCOMPARE(tf::parse_number(QStringLiteral("3.4M")), 3.4e6);
        QCOMPARE(tf::parse_number(QStringLiteral("12.3x")), 12.3);
        QCOMPARE(tf::parse_number(QStringLiteral("1.234.567,89")), 1234567.89);
        QCOMPARE(tf::parse_number(QStringLiteral("12,5")), 12.5);
        QVERIFY(std::isnan(tf::parse_number(QStringLiteral("—"))));
        QVERIFY(std::isnan(tf::parse_number(QStringLiteral("Q3 2024"))));
        QVERIFY(std::isnan(tf::parse_number(QStringLiteral("FY2025"))));
        QVERIFY(std::isnan(tf::parse_number(QStringLiteral("AAPL"))));
        QCOMPARE(tf::typed_decimals(QStringLiteral("1,234.50")), 2);
        QCOMPARE(tf::typed_decimals(QStringLiteral("1,234")), 0);
    }

    // ── Formatted tables ─────────────────────────────────────────────────────
    void old_tables_print_unchanged() {
        // No col_fmt / table_style: every cell prints exactly as typed, numbers right-aligned
        // from the second column, the header row repeats, zebra from 5 rows.
        const QVector<QStringList> grid{{"Item", "FY24", "FY25"}, {"Revenue", "1,234", "(56)"}, {"Note", "n/a", "x"}};
        const tf::FormattedTable t = tf::format_table(grid, {}, {}, tf::default_style());
        QCOMPARE(t.rows.size(), 3);
        QCOMPARE(texts_of_row(t, 1), QStringList({"Revenue", "1,234", "(56)"}));
        QCOMPARE(t.rows[1][0].align, QStringLiteral("left"));
        QCOMPARE(t.rows[1][1].align, QStringLiteral("right"));
        QCOMPARE(t.rows[2][1].align, QStringLiteral("left")); // "n/a" is not numeric
        QCOMPARE(t.rows[0][1].align, QStringLiteral("right"));
        QCOMPARE(t.header_rows, 1);
        QVERIFY(t.header_repeat);
        QVERIFY(!t.zebra); // three rows
        QVERIFY(t.widths.isEmpty());
        QCOMPARE(t.totals_row, -1);
    }

    void column_formats_apply() {
        const QVector<QStringList> grid{{"Name", "Value", "Share", "Date"},
                                        {"A", "-1234.5", "12.345", "2026-11-17"},
                                        {"B", "2500", "0", "2026-01-02"}};
        QVector<tf::ColumnFormat> cols(4);
        cols[1].type = QStringLiteral("currency");
        cols[1].currency = QStringLiteral("USD");
        cols[1].brackets = true;
        cols[2].type = QStringLiteral("percent");
        cols[2].decimals = 1;
        cols[3].type = QStringLiteral("date");
        cols[3].date_format = QStringLiteral("d MMM yyyy");
        const tf::FormattedTable t = tf::format_table(grid, {}, cols, tf::default_style());
        QCOMPARE(t.rows[1][1].text, QStringLiteral("($1,234.50)"));
        QCOMPARE(t.rows[2][1].text, QStringLiteral("$2,500.00"));
        QCOMPARE(t.rows[1][2].text, QStringLiteral("12.3%"));
        QCOMPARE(t.rows[2][2].text, QStringLiteral("0.0%"));
        QCOMPARE(t.rows[1][3].text, QStringLiteral("17 Nov 2026"));
        QCOMPARE(t.rows[1][1].align, QStringLiteral("right"));
        QCOMPARE(t.rows[1][3].align, QStringLiteral("left"));
        // Scaled numbers: "in millions" with one decimal by default; "-0.0" prints unsigned.
        tf::ColumnFormat m;
        m.type = QStringLiteral("number");
        m.divide = 1e6;
        QCOMPARE(tf::format_value(QStringLiteral("12,345,678"), m, {}, tf::default_style()), QStringLiteral("12.3"));
        QCOMPARE(tf::format_value(QStringLiteral("-10"), m, {}, tf::default_style()), QStringLiteral("0.0"));
        // A cell that is not a number keeps its text.
        QCOMPARE(tf::format_value(QStringLiteral("n/a"), cols[1], {}, tf::default_style()), QStringLiteral("n/a"));
    }

    void totals_and_colour_scales() {
        const QVector<QStringList> grid{{"Name", "Q1", "Q2"}, {"A", "1.5", "10"}, {"B", "2.25", "20"}, {"C", "x", "30"}};
        tf::TableStyle st;
        st.totals = QStringLiteral("sum");
        QVector<tf::ColumnFormat> cols(3);
        cols[2].heat = QStringLiteral("green_red");
        const tf::FormattedTable t = tf::format_table(grid, st, cols, tf::default_style(), QStringLiteral("#ffffff"));
        QCOMPARE(t.totals_row, 4);
        QCOMPARE(t.rows[4][0].text, QStringLiteral("Total"));
        QCOMPARE(t.rows[4][1].text, QStringLiteral("3.75")); // the text cell is skipped; 2 decimals as typed
        QCOMPARE(t.rows[4][2].text, QStringLiteral("60"));
        QVERIFY(t.rows[4][1].total);
        // Colour scale over the body only: low red, high green, the totals row untouched.
        QVERIFY(!t.rows[1][2].background.isEmpty());
        QVERIFY(t.rows[1][2].background != t.rows[3][2].background);
        QVERIFY(t.rows[4][2].background.isEmpty());
        QVERIFY(t.rows[1][1].background.isEmpty());
        // Blend stays on the paper side: a light tint on white paper.
        const QString hi = t.rows[3][2].background;
        bool ok = false;
        QVERIFY(hi.mid(1, 2).toInt(&ok, 16) > 150 && ok);
        // "last" marks the last row as the total instead of adding one.
        tf::TableStyle last;
        last.totals = QStringLiteral("last");
        const tf::FormattedTable t2 = tf::format_table(grid, last, {}, tf::default_style());
        QCOMPARE(t2.totals_row, 3);
        QCOMPARE(t2.rows.size(), 4);
    }

    void widths_and_zebra() {
        QVector<QStringList> grid{{"a", "b", "c"}};
        for (int i = 0; i < 6; ++i)
            grid.append({"x", QString::number(i), "y"});
        QVector<tf::ColumnFormat> cols(3);
        cols[0].width = 50;
        const tf::FormattedTable t = tf::format_table(grid, {}, cols, tf::default_style());
        QVERIFY(t.zebra);
        QCOMPARE(t.widths.size(), 3);
        QCOMPARE(t.widths[0], 50.0);
        QCOMPARE(t.widths[1], 25.0);
        tf::TableStyle off;
        off.zebra = QStringLiteral("off");
        off.header_repeat = false;
        const tf::FormattedTable t2 = tf::format_table(grid, off, {}, tf::default_style());
        QVERIFY(!t2.zebra);
        QVERIFY(!t2.header_repeat);
        const QVector<double> d = tf::default_widths(4);
        double sum = 0;
        for (double w : d)
            sum += w;
        QVERIFY(std::abs(sum - 100.0) < 1e-9);
    }

    void format_json_round_trip() {
        QVector<tf::ColumnFormat> cols(3);
        cols[1].type = QStringLiteral("currency");
        cols[1].decimals = 0;
        cols[1].currency = QStringLiteral("EUR");
        cols[1].brackets = true;
        cols[1].divide = 1e3;
        cols[1].align = QStringLiteral("center");
        cols[1].width = 30;
        cols[1].heat = QStringLiteral("blue");
        cols[2].date_format = QStringLiteral("yyyy");
        const QString text = tf::columns_to_text(cols);
        QCOMPARE(tf::columns_from_text(text), cols);
        // Trailing default columns are dropped; all-default writes nothing (key removed).
        QCOMPARE(tf::columns_to_text(QVector<tf::ColumnFormat>(4)), QString());
        tf::TableStyle st;
        st.totals = QStringLiteral("sum");
        st.zebra = QStringLiteral("off");
        st.header_repeat = false;
        st.currency = QStringLiteral("USD");
        QVERIFY(tf::style_from_text(tf::style_to_text(st)) == st);
        QCOMPARE(tf::style_to_text(tf::TableStyle()), QString());
        // Garbage reads as defaults, never as nonsense.
        const tf::ColumnFormat bad = tf::ColumnFormat::from_json(
            QJsonObject{{"type", "weird"}, {"dec", 99}, {"div", 7}, {"align", "up"}, {"heat", "pink"}});
        QCOMPARE(bad.type, QStringLiteral("auto"));
        QCOMPARE(bad.decimals, 8);
        QCOMPARE(bad.divide, 1.0);
        QCOMPARE(bad.align, QStringLiteral("auto"));
        QCOMPARE(bad.heat, QStringLiteral("none"));
    }

    // ── Variables ────────────────────────────────────────────────────────────
    void variables_resolve() {
        report::ReportMetadata m;
        m.title = QStringLiteral("{{symbol}} — Earnings preview");
        m.author = QStringLiteral("Jane");
        m.company = QString();
        m.date = QStringLiteral("2026-11-17");
        m.variables.insert(QStringLiteral("symbol"), QStringLiteral("AAPL"));
        m.variables.insert(QStringLiteral("period"), QString());
        const auto ctx = report::vars::context(m);
        QCOMPARE(ctx.value("title"), QStringLiteral("AAPL — Earnings preview")); // built-ins use custom vars
        QCOMPARE(report::vars::display_title(m), QStringLiteral("AAPL — Earnings preview"));
        const auto r = report::vars::resolve(QStringLiteral("{{ Symbol }} in {{period}} by {{author}} at {{company}}"), ctx);
        QCOMPARE(r.text, QStringLiteral("AAPL in {{period}} by Jane at {{company}}"));
        QCOMPARE(r.unresolved, QStringList({"period", "company"}));
        QCOMPARE(report::vars::references(QStringLiteral("{{a}} {{b_2}} {{a}} {{ not valid }}")),
                 QStringList({"a", "b_2", "not_valid"}));
        QVERIFY(report::vars::is_valid_name(QStringLiteral("period")));
        QVERIFY(!report::vars::is_valid_name(QStringLiteral("2x")));
        QVERIFY(!report::vars::is_valid_name(QStringLiteral("Upper")));
        // Segments for on-screen marking.
        const auto segs = report::vars::segments(QStringLiteral("x {{symbol}} {{period}}"), ctx);
        QCOMPARE(segs.size(), 4);
        QCOMPARE(segs[1].kind, report::vars::Segment::Kind::Value);
        QCOMPARE(segs[3].kind, report::vars::Segment::Kind::Unresolved);
    }

    void variables_in_blocks_and_bindings() {
        ReportDocument d;
        d.metadata.variables.insert(QStringLiteral("symbol"), QStringLiteral("MSFT"));
        // Table cells expand cell by cell: a value with a comma stays one cell.
        d.metadata.variables.insert(QStringLiteral("co"), QStringLiteral("Microsoft, Inc"));
        ReportComponent t = mk(1, "table", {}, {{"csv", report::table::encode({{"Name", "Ticker"}, {"{{co}}", "{{symbol}}"}})}});
        const ReportComponent e = report::vars::expanded_component(t, report::vars::context(d.metadata));
        QCOMPARE(report::table::parse(e.config.value("csv"))[1], QStringList({"Microsoft, Inc", "MSFT"}));
        // A recipe bound to {{symbol}} follows the variable; unresolved -> empty (no symbol set).
        const report::Binding b = report::quote_binding(QStringLiteral("{{symbol}}"));
        QCOMPARE(report::vars::expand_binding(b, report::vars::context(d.metadata)).symbol(), QStringLiteral("MSFT"));
        report::ReportMetadata none;
        QCOMPARE(report::vars::expand_binding(b, report::vars::context(none)).symbol(), QString());
        const report::Binding h = report::history_binding({QStringLiteral("{{symbol}}"), QStringLiteral("^GSPC")});
        QCOMPARE(report::vars::expand_binding(h, report::vars::context(d.metadata)).symbols(),
                 QStringList({"MSFT", "^GSPC"}));
        // Unresolved names across the document (blocks, bands, recipes).
        d.components = {mk(2, "text", "Q: {{period}}"), t};
        d.metadata.footer_left = QStringLiteral("{{desk}}");
        QCOMPARE(report::vars::unresolved_in(d), QStringList({"desk", "period"}));
    }

    void outline_numbers() {
        ReportDocument d;
        d.metadata.number_headings = true;
        d.components = {mk(1, "heading", "Intro", {{"level", "1"}}),   mk(2, "heading", "Detail", {{"level", "2"}}),
                        mk(3, "heading", "", {{"level", "2"}}),        mk(4, "heading", "More", {{"level", "2"}}),
                        mk(5, "heading", "Deep", {{"level", "3"}}),    mk(6, "heading", "Next", {{"level", "1"}}),
                        mk(7, "heading", "Sub", {{"level", "2"}})};
        const auto hs = report::outline::headings(d);
        QCOMPARE(hs.size(), 6); // the empty heading takes no number
        QCOMPARE(hs[0].number, QStringLiteral("1"));
        QCOMPARE(hs[1].number, QStringLiteral("1.1"));
        QCOMPARE(hs[2].number, QStringLiteral("1.2"));
        QCOMPARE(hs[3].number, QStringLiteral("1.2.1"));
        QCOMPARE(hs[4].number, QStringLiteral("2"));
        QCOMPARE(hs[5].number, QStringLiteral("2.1"));
        QCOMPARE(report::outline::heading_label(hs[3]), QStringLiteral("1.2.1  Deep"));
        // A level-2 heading before any level-1 counts as level 1 ("1", not "0.1").
        ReportDocument d2;
        d2.metadata.number_headings = true;
        d2.components = {mk(1, "heading", "A", {{"level", "2"}}), mk(2, "heading", "B", {{"level", "3"}})};
        const auto h2 = report::outline::headings(d2);
        QCOMPARE(h2[0].number, QStringLiteral("1"));
        QCOMPARE(h2[1].number, QStringLiteral("1.1"));
        d.metadata.number_headings = false;
        QVERIFY(report::outline::heading_numbers(d).isEmpty());
    }

    // ── Document JSON ────────────────────────────────────────────────────────
    void document_new_fields_round_trip() {
        ReportDocument d;
        d.metadata.variables.insert(QStringLiteral("symbol"), QStringLiteral("AAPL"));
        d.metadata.watermark = QStringLiteral("DRAFT");
        d.metadata.confidentiality = QStringLiteral("Internal");
        d.metadata.number_headings = true;
        d.revision = {3, QStringLiteral("ab12cd34")};
        d.components = {mk(1, "cover", {}, {{"subtitle", "Q3"}})};
        const auto r = ReportDocument::from_json(d.to_json());
        QVERIFY(r.is_ok());
        const ReportDocument b = r.value();
        QCOMPARE(b.metadata.variables, d.metadata.variables);
        QCOMPARE(b.metadata.watermark, d.metadata.watermark);
        QCOMPARE(b.metadata.confidentiality, d.metadata.confidentiality);
        QVERIFY(b.metadata.number_headings);
        QVERIFY(b.revision == d.revision);
        // Without the revision (the content hash the library compares).
        QVERIFY(!QJsonDocument::fromJson(d.to_json(false).toUtf8()).object().contains("revision"));
    }

    void old_files_load_unchanged() {
        // A file written before variables / watermark / revisions (and the old company default).
        const QString old = QStringLiteral(
            R"({"metadata":{"title":"Q2 review","author":"Analyst","company":"Fincept Corporation","date":"2026-07-01",)"
            R"("footer_center":"Page {page}","font_size":12},"theme":"Dark Corporate","components":[)"
            R"({"id":1,"type":"table","content":"","config":{"csv":"A,B|1,2","rows":"2","cols":"2"}}],"next_id":2,)"
            R"("schema_version":1})");
        const auto r = ReportDocument::from_json(old);
        QVERIFY(r.is_ok());
        const ReportDocument d = r.value();
        QCOMPARE(d.metadata.company, QStringLiteral("Fincept Corporation")); // stored text stays
        QVERIFY(d.metadata.variables.isEmpty());
        QVERIFY(d.metadata.watermark.isEmpty());
        QVERIFY(!d.metadata.number_headings);
        QVERIFY(!d.revision.is_set());
        QVERIFY(!d.components[0].config.contains(QString::fromLatin1(tf::kColFmtKey)));
        // Writing it back adds none of the new keys.
        const QJsonObject meta = QJsonDocument::fromJson(d.to_json().toUtf8()).object().value("metadata").toObject();
        QVERIFY(!meta.contains("variables"));
        QVERIFY(!meta.contains("watermark"));
        QVERIFY(!meta.contains("number_headings"));
        // A new document starts with no company (the service fills the user's own).
        QCOMPARE(report::ReportMetadata().company, QString());
    }

    // ── Revisions (stale saves) ──────────────────────────────────────────────
    void revision_rules() {
        namespace ll = fincept::services::library_logic;
        const report::ReportRevision none;
        const report::ReportRevision mine{4, QStringLiteral("aaaa1111")};
        const report::ReportRevision theirs{5, QStringLiteral("bbbb2222")};
        QVERIFY(!ll::revision_conflicts(mine, mine, false));
        QVERIFY(ll::revision_conflicts(theirs, mine, false));  // someone saved after my base
        QVERIFY(!ll::revision_conflicts(theirs, mine, true));  // "keep mine"
        QVERIFY(!ll::revision_conflicts(none, none, false));   // rows from before revisions
        QVERIFY(ll::revision_conflicts(theirs, none, false));  // a revision appeared meanwhile
        // Same number written by two installations is still a conflict.
        QVERIFY(ll::revision_conflicts(report::ReportRevision{4, QStringLiteral("bbbb2222")}, mine, false));
        const report::ReportRevision next = ll::next_revision(mine, theirs, QStringLiteral("aaaa1111"));
        QCOMPARE(next.n, 6);
        QCOMPARE(next.by, QStringLiteral("aaaa1111"));
        QJsonObject root;
        ll::stamp_revision(root, next);
        QVERIFY(ReportDocument::revision_of(root) == next);
    }

    // ── Scheduled regeneration ───────────────────────────────────────────────
    void schedule_slots() {
        sch::Schedule s;
        s.report_id = 7;
        s.frequency = sch::Frequency::Daily;
        s.time = QTime(8, 0);
        const QDateTime morning(QDate(2026, 11, 17), QTime(7, 0)); // a Tuesday
        QCOMPARE(sch::last_slot(s, morning), QDateTime(QDate(2026, 11, 16), QTime(8, 0)));
        QCOMPARE(sch::next_slot(s, morning), QDateTime(QDate(2026, 11, 17), QTime(8, 0)));
        const QDateTime noon(QDate(2026, 11, 17), QTime(12, 0));
        QCOMPARE(sch::last_slot(s, noon), QDateTime(QDate(2026, 11, 17), QTime(8, 0)));
        s.frequency = sch::Frequency::Weekly;
        s.weekday = 1; // Monday
        QCOMPARE(sch::last_slot(s, noon), QDateTime(QDate(2026, 11, 16), QTime(8, 0)));
        QCOMPARE(sch::next_slot(s, noon), QDateTime(QDate(2026, 11, 23), QTime(8, 0)));
        const QDateTime monday_early(QDate(2026, 11, 16), QTime(7, 59));
        QCOMPARE(sch::last_slot(s, monday_early), QDateTime(QDate(2026, 11, 9), QTime(8, 0)));
    }

    void schedule_decisions() {
        sch::Schedule s;
        s.report_id = 7;
        s.time = QTime(8, 0);
        const QDateTime session(QDate(2026, 11, 17), QTime(6, 0));
        // Its time came while the terminal was running.
        QCOMPARE(sch::decide(s, QDateTime(QDate(2026, 11, 17), QTime(8, 1)), session), sch::Decision::Due);
        // Not yet.
        s.last_run = QDateTime(QDate(2026, 11, 16), QTime(8, 0, 5));
        QCOMPARE(sch::decide(s, QDateTime(QDate(2026, 11, 17), QTime(7, 59)), session), sch::Decision::NotDue);
        // Ran for today's slot already: not again.
        s.last_run = QDateTime(QDate(2026, 11, 17), QTime(8, 0, 30));
        QCOMPARE(sch::decide(s, QDateTime(QDate(2026, 11, 17), QTime(9, 0)), session), sch::Decision::NotDue);
        // Missed while closed: made up ONCE when the terminal starts (only the latest slot).
        s.last_run = QDateTime(QDate(2026, 11, 10), QTime(8, 0));
        const QDateTime late_start(QDate(2026, 11, 17), QTime(10, 0));
        QCOMPARE(sch::decide(s, late_start.addSecs(60), late_start), sch::Decision::Missed);
        s.last_run = late_start.addSecs(120); // the make-up run happened
        QCOMPARE(sch::decide(s, late_start.addSecs(3600), late_start), sch::Decision::NotDue);
        // Disabled / no report: never.
        s.enabled = false;
        QCOMPARE(sch::decide(s, late_start.addDays(1), late_start), sch::Decision::NotDue);
        // On start: once per session, at most every 12 hours.
        sch::Schedule o;
        o.report_id = 3;
        o.frequency = sch::Frequency::OnStart;
        QCOMPARE(sch::decide(o, late_start, late_start), sch::Decision::OnStart);
        QCOMPARE(sch::decide(o, late_start, late_start, /*ran_this_session=*/true), sch::Decision::NotDue);
        o.last_run = late_start.addSecs(-3600);
        QCOMPARE(sch::decide(o, late_start, late_start), sch::Decision::NotDue);
        o.last_run = late_start.addSecs(-13 * 3600);
        QCOMPARE(sch::decide(o, late_start, late_start), sch::Decision::OnStart);
    }

    void schedule_json_and_paths() {
        sch::Schedule s;
        s.report_id = 42;
        s.title = QStringLiteral("Weekly wrap");
        s.frequency = sch::Frequency::Weekly;
        s.weekday = 5;
        s.time = QTime(17, 30);
        s.folder = QStringLiteral("D:/out");
        s.formats = {QStringLiteral("pdf"), QStringLiteral("md"), QStringLiteral("bogus")};
        s.last_run = QDateTime(QDate(2026, 11, 14), QTime(17, 30), QTimeZone::UTC);
        const sch::Schedule b = sch::Schedule::from_json(s.to_json());
        QCOMPARE(b.report_id, 42);
        QCOMPARE(b.frequency, sch::Frequency::Weekly);
        QCOMPARE(b.weekday, 5);
        QCOMPARE(b.time, QTime(17, 30));
        QCOMPARE(b.formats, QStringList({"pdf", "md"})); // unknown formats dropped
        QCOMPARE(b.last_run, s.last_run);
        QCOMPARE(sch::describe(b), QStringLiteral("Fridays at 17:30"));
        QCOMPARE(sch::output_path(QStringLiteral("D:/out/"), QStringLiteral("AAPL — Q3: preview?"), QDate(2026, 11, 17),
                                  QStringLiteral("pdf")),
                 QStringLiteral("D:/out/AAPL - Q3 preview 2026-11-17.pdf"));
        QCOMPARE(sch::safe_file_stem(QStringLiteral("../..")), QStringLiteral("Report"));
        sch::RunRecord r;
        r.report_id = 42;
        r.ok = true;
        r.files = {QStringLiteral("D:/out/a.pdf")};
        r.warnings = {QStringLiteral("w")};
        r.started = QDateTime(QDate(2026, 11, 17), QTime(8, 0), QTimeZone::UTC);
        r.finished = r.started.addSecs(30);
        const sch::RunRecord rb = sch::RunRecord::from_json(r.to_json());
        QCOMPARE(rb.files, r.files);
        QCOMPARE(rb.warnings, r.warnings);
        QCOMPARE(rb.started, r.started);
    }

    // ── Chart spec (the renderer's input) ────────────────────────────────────
    void chart_spec_from_config() {
        // Legacy single-series block reads as before.
        const auto a = report::chart::from_config({{"chart_type", "bar"}, {"data", "1,2,3"}, {"labels", "a,b,c"}});
        QCOMPARE(a.kind, report::chart::Kind::Bar);
        QCOMPARE(a.series.size(), 1);
        QCOMPARE(a.point_count(), 3);
        // Rebased display transform; dates of another length are ignored.
        const auto b = report::chart::from_config({{"chart_type", "line"},
                                                   {"series", R"([{"name":"A","values":[50,100]},{"name":"B","values":[10,15],"axis":"right"}])"},
                                                   {"transform", "rebased"},
                                                   {"dates", "2026-01-01"}});
        QCOMPARE(b.series[0].values[1], 200.0);
        QVERIFY(b.series[1].right_axis);
        QVERIFY(b.dates_ms.isEmpty());
        // Identical content -> identical hash; any change -> another.
        QCOMPARE(report::chart::content_hash(a), report::chart::content_hash(report::chart::from_config(
                                                     {{"chart_type", "bar"}, {"data", "1,2,3"}, {"labels", "a,b,c"}})));
        QVERIFY(report::chart::content_hash(a) != report::chart::content_hash(b));
    }
};

QTEST_GUILESS_MAIN(TestReportBuilderFeatures)
#include "tst_report_builder_features.moc"
