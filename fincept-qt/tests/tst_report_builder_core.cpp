// tst_report_builder_core.cpp — the Report Builder's pure core (Qt Core only, header-only
// sources): block builders (payload -> components), bindings and their JSON round trip
// (old .fincept files and old `reports` rows still load), the incremental layout planner,
// the export chunking rules and the library's document operations (rename, duplicate,
// the "append to a report that is not open" queue).

#include "core/report/ReportBinding.h"
#include "core/report/ReportBlocks.h"
#include "core/report/ReportDocument.h"
#include "core/report/ReportExportPlan.h"
#include "core/report/ReportLayoutPlan.h"
#include "core/report/ReportTableCodec.h"
#include "services/report_builder/ReportLibraryLogic.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace fincept;
using report::ReportComponent;
using report::ReportDocument;

namespace {

ReportComponent mk(int id, const QString& type, const QString& content = {}, QMap<QString, QString> cfg = {}) {
    ReportComponent c;
    c.id = id;
    c.type = type;
    c.content = content;
    c.config = cfg;
    return c;
}

QStringList types_of(const QVector<ReportComponent>& v) {
    QStringList out;
    for (const auto& c : v)
        out << c.type;
    return out;
}

} // namespace

class TestReportBuilderCore : public QObject {
    Q_OBJECT

  private slots:
    // ── Table codec ──────────────────────────────────────────────────────────
    void codec_round_trip() {
        const QVector<QStringList> rows{{"Name", "Value, USD"}, {"A|B", "1,234.5"}, {"quote \"x\"", ""}};
        const QString enc = report::table::encode(rows);
        QCOMPARE(report::table::parse(enc), (rows.mid(0, 2) + QVector<QStringList>{{"quote \"x\"", ""}}));
        QCOMPARE(report::table::parse_list(report::table::encode_list({"Jan 1, 2026", "b"})),
                 QStringList({"Jan 1, 2026", "b"}));
        // Plain CSV written by older builds reads the same.
        QCOMPARE(report::table::parse_list(QStringLiteral("Jan,Feb,Mar")), QStringList({"Jan", "Feb", "Mar"}));
    }

    // ── Block builders ───────────────────────────────────────────────────────
    void builders_full_payload() {
        report::ReportPayload p;
        p.title = QStringLiteral("Black-Scholes call");
        p.source = QStringLiteral("Derivatives");
        p.as_of = QDateTime(QDate(2026, 11, 17), QTime(14, 2), QTimeZone::UTC);
        p.kpis = {{"Price", "12.34", "USD", ""}, {"Delta: spot", "0.55", "", "positive"}, {"Vol", "20", "%", ""}};
        p.markdown = QStringLiteral("Intro text.\n\n## Method\nBody **bold**.\n\n| A | B |\n|---|---|\n| 1 | 2 |\n\n```\ncode()\n```");
        report::PayloadTable t;
        t.title = QStringLiteral("Greeks");
        t.columns = {"Greek", "Value"};
        t.rows = {{"Gamma", "0.01"}, {"Vega", "0.2"}};
        p.tables = {t};
        report::PayloadChart line;
        line.title = QStringLiteral("Payoff");
        line.series = {{"Call", {"90", "100", "110"}, {0, 0, 10}, {}, {}, {}}};
        report::PayloadChart img;
        img.title = QStringLiteral("Surface");
        img.image = QByteArray("\x89PNG fake", 9);
        p.charts = {line, img};
        p.inputs = {{"Spot", "100"}, {"Strike", "100"}};
        p.notes = QStringLiteral("Model output; not advice.");

        const report::BuiltBlocks b = report::build_blocks(p);
        QCOMPARE(types_of(b.blocks), QStringList({"heading", "stats_block", "text", "heading", "text", "table", "code",
                                                  "chart", "image", "table", "stats_block", "text", "text"}));
        for (const auto& c : b.blocks)
            QCOMPARE(c.id, 0); // the service assigns ids
        QCOMPARE(b.blocks[0].content, QStringLiteral("Black-Scholes call"));
        // KPIs: "Label: Value unit", ':' in a label cannot break the line, tones aligned.
        const QStringList lines = b.blocks[1].config.value("data").split('\n');
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0], QStringLiteral("Price: 12.34 USD"));
        QCOMPARE(lines[1], QStringLiteral("Delta∶ spot: 0.55")); // the label's ':' cannot split the line
        QCOMPARE(lines[2], QStringLiteral("Vol: 20%"));
        QCOMPARE(b.blocks[1].config.value("tones"), QStringLiteral(",positive,"));
        QCOMPARE(b.blocks[1].config.value(report::keys::kSourceLabel), QStringLiteral("Derivatives"));
        QCOMPARE(report::as_of_of(b.blocks[1]), p.as_of);
        // Markdown: heading level kept, pipe table -> table block, fence -> code.
        QCOMPARE(b.blocks[3].config.value("level"), QStringLiteral("2"));
        QCOMPARE(report::table::parse(b.blocks[5].config.value("csv")),
                 (QVector<QStringList>{{"A", "B"}, {"1", "2"}}));
        QCOMPARE(b.blocks[6].content, QStringLiteral("code()"));
        // Native chart: data / labels, no image.
        QCOMPARE(b.blocks[7].config.value("chart_type"), QStringLiteral("line"));
        QCOMPARE(b.blocks[7].config.value("data"), QStringLiteral("0,0,10"));
        QCOMPARE(report::table::parse_list(b.blocks[7].config.value("labels")), QStringList({"90", "100", "110"}));
        // Image chart: pending until the inbox stores the bytes.
        QCOMPARE(b.images.size(), 1);
        QCOMPARE(b.images[0].block_index, 8);
        QCOMPARE(b.blocks[8].config.value("pending_image"), QStringLiteral("1"));
        // Table, inputs echo, notes, provenance.
        QCOMPARE(b.blocks[9].config.value("title"), QStringLiteral("Greeks"));
        QCOMPARE(b.blocks[10].config.value("role"), QStringLiteral("inputs"));
        QCOMPARE(b.blocks[11].config.value("style"), QStringLiteral("caption"));
        QCOMPARE(b.blocks[12].config.value("role"), QStringLiteral("provenance"));
        QVERIFY(b.blocks[12].content.startsWith(QStringLiteral("Source: Derivatives")));
    }

    void builders_chart_kinds_and_caps() {
        report::PayloadChart multi;
        multi.kind = QStringLiteral("line");
        multi.series = {{"A", {"1", "2"}, {1, 2}, {}, {}, {}}, {"B", {"1", "2"}, {3, 4}, {}, {}, {}}};
        const ReportComponent m = report::blocks::chart(multi);
        const QJsonArray s = QJsonDocument::fromJson(m.config.value("series").toUtf8()).array();
        QCOMPARE(s.size(), 2);
        QCOMPARE(s[1].toObject().value("name").toString(), QStringLiteral("B"));

        report::PayloadChart candle;
        candle.kind = QStringLiteral("candle");
        candle.series = {{"X", {"d1", "d2"}, {10, 11}, {9, 10}, {12, 12}, {8, 9}}};
        const ReportComponent c = report::blocks::chart(candle);
        QCOMPARE(c.config.value("chart_type"), QStringLiteral("candle"));
        const QJsonArray o = QJsonDocument::fromJson(c.config.value("ohlc").toUtf8()).array();
        QCOMPARE(o.size(), 2);
        QCOMPARE(o[0].toArray()[1].toDouble(), 12.0); // high

        report::PayloadChart pie;
        pie.kind = QStringLiteral("pie");
        QCOMPARE(report::blocks::chart(pie).config.value("chart_type"), QStringLiteral("donut"));

        QVector<QStringList> rows;
        for (int i = 0; i < 700; ++i)
            rows.append({QString::number(i)});
        const ReportComponent t = report::blocks::table(QString(), {"n"}, rows, QString(), 500);
        QCOMPARE(report::table::parse(t.config.value("csv")).size(), 501); // header + 500
        QVERIFY(t.config.value("note").contains(QStringLiteral("500 of 700")));
    }

    void builders_refresh_recipe_goes_to_first_data_block() {
        report::ReportPayload p;
        p.title = QStringLiteral("AAPL");
        p.kpis = {{"Last", "189.3", "", ""}};
        p.refresh = report::quote_binding(QStringLiteral("AAPL"));
        const auto b = report::build_blocks(p);
        QCOMPARE(report::binding_of(b.blocks[1]).symbol(), QStringLiteral("AAPL"));
        QVERIFY(report::is_bound(b.blocks[1]));
        QVERIFY(report::build_blocks(report::ReportPayload{}).blocks.isEmpty());
        QVERIFY(report::ReportPayload{}.is_empty());
    }

    // ── Bindings ─────────────────────────────────────────────────────────────
    void binding_round_trip() {
        const report::Binding b = report::history_binding({"AAPL", "MSFT"}, "1y", "1d", "rebased", "line");
        const report::Binding back = report::Binding::parse(b.to_string());
        QCOMPARE(back, b);
        QCOMPARE(back.symbols(), QStringList({"AAPL", "MSFT"}));
        QCOMPARE(back.key(), b.key());
        QVERIFY(report::Binding::parse(QStringLiteral("not json")).source.isEmpty());
        QVERIFY(!report::static_binding("M&A").is_refreshable());

        ReportComponent c = mk(1, "table");
        report::set_binding(c, report::financials_binding("MSFT", "income", "annual", 5));
        // The component JSON (file / library row / cloud) carries the recipe as a string.
        ReportDocument d;
        d.components = {c};
        d.next_id = 2;
        const auto r = ReportDocument::from_json(d.to_json());
        QVERIFY(r.is_ok());
        QCOMPARE(report::binding_of(r.value().components[0]).param("statement"), QStringLiteral("income"));
        QCOMPARE(report::binding_of(r.value().components[0]).int_param("n", 0), 5);

        report::clear_binding(c);
        QVERIFY(!report::is_bound(c));
    }

    void legacy_blocks_imply_recipes() {
        QCOMPARE(report::binding_of(mk(1, "market_data", {}, {{"symbol", "AAPL"}})).source, QStringLiteral("quote"));
        const report::Binding h = report::binding_of(mk(2, "chart", {}, {{"fetch_symbol", "MSFT"}}));
        QCOMPARE(h.source, QStringLiteral("history"));
        QCOMPARE(h.param("range"), QStringLiteral("6mo"));
        QCOMPARE(report::binding_of(mk(3, "stats_block", {}, {{"symbol", "NVDA"}})).source, QStringLiteral("info"));
        QVERIFY(!report::binding_of(mk(4, "stats_block", {}, {{"data", "P/E: 3"}})).is_valid());
    }

    void as_of_and_staleness() {
        // ISO (new) and "yyyy-MM-dd HH:mm tz" (older builds) both read.
        QVERIFY(report::parse_as_of(QStringLiteral("2026-11-17T13:02:00Z")).isValid());
        QCOMPARE(report::parse_as_of(QStringLiteral("2026-11-17 14:02 CET")).time(), QTime(14, 2));
        QVERIFY(!report::parse_as_of(QStringLiteral("soon")).isValid());

        const QDateTime now(QDate(2026, 11, 17), QTime(15, 0), QTimeZone::UTC);
        ReportComponent q = mk(1, "market_data", {}, {{"symbol", "AAPL"}});
        QVERIFY(report::is_stale(q, now)); // never fetched
        q.config.insert(report::keys::kAsOf, report::format_as_of(now.addSecs(-10 * 60)));
        QVERIFY(!report::is_stale(q, now)); // quotes: 30 min
        q.config.insert(report::keys::kAsOf, report::format_as_of(now.addSecs(-40 * 60)));
        QVERIFY(report::is_stale(q, now));
        ReportComponent f = mk(2, "table");
        report::set_binding(f, report::financials_binding("MSFT"));
        f.config.insert(report::keys::kAsOf, report::format_as_of(now.addDays(-3)));
        QVERIFY(!report::is_stale(f, now)); // statements: 7 days
        QVERIFY(!report::is_stale(mk(3, "text", "plain"), now));
    }

    void old_files_and_rows_still_load() {
        // A .fincept written before page setup, inbox and bindings existed.
        // (Ordinary escaped literal: moc cannot read a multi-line raw string.)
        const QString old_json = QStringLiteral(
            "{\"metadata\":{\"title\":\"Q3\",\"author\":\"A\",\"font_family\":\"Segoe UI\",\"font_size\":11},"
            "\"theme\":\"Dark Corporate\",\"components\":["
            "{\"id\":4,\"type\":\"market_data\",\"content\":\"\",\"config\":{\"symbol\":\"AAPL\",\"price\":\"189.30\","
            "\"as_of\":\"2026-10-01 16:00 EDT\",\"status\":\"ok\"}},"
            "{\"id\":7,\"type\":\"table\",\"content\":\"\",\"config\":{\"rows\":\"2\",\"cols\":\"2\","
            "\"data\":\"{\\\"0_0\\\":\\\"x\\\"}\"}}],"
            "\"next_id\":8,\"schema_version\":1}");
        const auto r = ReportDocument::from_json(old_json);
        QVERIFY(r.is_ok());
        const ReportDocument d = r.value();
        QCOMPARE(d.metadata.page_size, QStringLiteral("A4"));
        QVERIFY(!d.metadata.landscape);
        QCOMPARE(d.metadata.margin_left_mm, 15.0);
        QCOMPARE(d.metadata.margin_top_mm, 18.0);
        QCOMPARE(d.metadata.font_size, 11);
        QCOMPARE(d.theme.name, QStringLiteral("Dark Corporate"));
        QVERIFY(d.unseen_ids.isEmpty());
        QCOMPARE(d.next_id, 8);
        QCOMPARE(report::binding_of(d.components[0]).symbol(), QStringLiteral("AAPL"));
        QVERIFY(report::as_of_of(d.components[0]).isValid());
        // A `reports` row from v001 with an empty / minimal JSON body.
        QVERIFY(ReportDocument::from_json(QStringLiteral("{}")).is_ok());
        QVERIFY(ReportDocument::from_json(QStringLiteral("[1,2]")).is_err());

        // New fields round-trip.
        ReportDocument n = d;
        n.metadata.page_size = QStringLiteral("Letter");
        n.metadata.landscape = true;
        n.metadata.margin_right_mm = 9.5;
        n.unseen_ids = {4};
        n.unseen_sources = {QStringLiteral("Chat Mode")};
        const ReportDocument back = ReportDocument::from_json(n.to_json()).value();
        QCOMPARE(back.metadata.page_size, QStringLiteral("Letter"));
        QVERIFY(back.metadata.landscape);
        QCOMPARE(back.metadata.margin_right_mm, 9.5);
        QCOMPARE(back.unseen_ids, QVector<int>({4}));
        QCOMPARE(back.unseen_sources, QStringList({"Chat Mode"}));
        QCOMPARE(report::page_size_mm(back.metadata), QSizeF(279.4, 215.9));
    }

    // ── Incremental layout ───────────────────────────────────────────────────
    void layout_sections() {
        const QVector<ReportComponent> v{mk(1, "heading"), mk(2, "text"), mk(3, "page_break"), mk(4, "table"),
                                         mk(5, "page_break")};
        const auto s = report::layout::split_sections(v);
        QCOMPARE(s.size(), 3);
        QCOMPARE(s[0].first, 0);
        QCOMPARE(s[0].last, 2);
        QCOMPARE(s[1].first, 3);
        QCOMPARE(s[1].last, 4);
        QCOMPARE(s[2].first, 5);
        QCOMPARE(s[2].last, 5); // empty last section after a trailing break
        QCOMPARE(report::layout::section_of(s, 3), 1);
        QCOMPARE(report::layout::section_of(s, 2), 0); // a break counts for the section it ends
    }

    void layout_edit_one_block_relays_only_its_section() {
        namespace lay = report::layout;
        QVector<ReportComponent> a;
        for (int i = 1; i <= 40; ++i)
            a.append(mk(i, i % 10 == 0 ? "page_break" : "text", QStringLiteral("p%1").arg(i)));
        QVector<ReportComponent> b = a;
        b[24].content = QStringLiteral("edited"); // id 25, in the third section
        const lay::UpdatePlan p = lay::plan_update(a, b);
        QCOMPARE(p.kind, lay::UpdatePlan::Kind::Replace);
        QCOMPARE(p.replace_ids, QVector<int>({25}));
        QCOMPARE(p.sections, QSet<int>({2}));
        QVERIFY(!p.toc);

        QCOMPARE(lay::plan_update(a, a).kind, lay::UpdatePlan::Kind::None);

        QVector<ReportComponent> ins = a;
        ins.insert(5, mk(99, "chart"));
        const lay::UpdatePlan pi = lay::plan_update(a, ins);
        QCOMPARE(pi.kind, lay::UpdatePlan::Kind::Insert);
        QCOMPARE(pi.inserted_index, 5);
        QCOMPARE(pi.sections, QSet<int>({0}));

        QVector<ReportComponent> rem = a;
        rem.removeAt(12);
        const lay::UpdatePlan pr = lay::plan_update(a, rem);
        QCOMPARE(pr.kind, lay::UpdatePlan::Kind::Remove);
        QCOMPARE(pr.removed_id, 13);

        // Moving a page break / inserting one / two edits of structure -> full rebuild.
        QVector<ReportComponent> brk = a;
        brk.insert(3, mk(100, "page_break"));
        QCOMPARE(lay::plan_update(a, brk).kind, lay::UpdatePlan::Kind::Full);
        QVector<ReportComponent> moved = a;
        moved.move(1, 7);
        QCOMPARE(lay::plan_update(a, moved).kind, lay::UpdatePlan::Kind::Full);
        QVector<ReportComponent> retype = a;
        retype[3].type = QStringLiteral("page_break");
        QCOMPARE(lay::plan_update(a, retype).kind, lay::UpdatePlan::Kind::Full);
    }

    void layout_heading_change_rerenders_toc() {
        namespace lay = report::layout;
        const QVector<ReportComponent> a{mk(1, "toc"), mk(2, "heading", "One"), mk(3, "page_break"),
                                         mk(4, "heading", "Two")};
        QVector<ReportComponent> b = a;
        b[3].content = QStringLiteral("Two (renamed)");
        const lay::UpdatePlan p = lay::plan_update(a, b);
        QCOMPARE(p.kind, lay::UpdatePlan::Kind::Replace);
        QVERIFY(p.toc);
        QCOMPARE(p.sections, QSet<int>({0, 1})); // the heading's section and the TOC's
    }

    // ── Export chunking ──────────────────────────────────────────────────────
    void export_page_sequence() {
        const auto seq = report::exportplan::page_sequence({3, 0, 2});
        QCOMPARE(seq.size(), 6); // an empty section still prints one page
        QCOMPARE(seq[3].section, 1);
        QCOMPARE(seq[3].page, 0);
        QCOMPARE(seq[5].number, 6);
        QCOMPARE(seq[5].section, 2);
        QCOMPARE(seq[5].page, 1);
    }

    void export_slices_make_progress_and_stop() {
        namespace xp = report::exportplan;
        int remaining = 40, calls = 0;
        qint64 fake_ms = 0;
        auto step = [&]() {
            ++calls;
            fake_ms += 10; // each page costs 10 ms
            return --remaining > 0;
        };
        auto clock = [&]() { return fake_ms; };
        int slices = 0;
        bool done = false;
        while (!done) {
            fake_ms = 0;
            done = xp::run_slice(step, 25, clock);
            ++slices;
            QVERIFY(slices < 100);
        }
        QCOMPARE(calls, 40);
        QCOMPARE(slices, 14); // 3 pages per 25 ms slice, the GUI gets a turn in between
        // A step slower than the budget still advances one unit per slice.
        int slow = 3, slow_slices = 0;
        while (!xp::run_slice([&]() { fake_ms += 500; return --slow > 0; }, 25, clock))
            ++slow_slices;
        QCOMPARE(slow_slices, 2);
        QCOMPARE(xp::overall_percent(0, 0, 10), 0);
        QCOMPARE(xp::overall_percent(1, 5, 10), 40);
        QCOMPARE(xp::overall_percent(2, 10, 10), 100);
        QCOMPARE(xp::overall_percent(2, 0, 0), 50);
    }

    // ── Library operations ───────────────────────────────────────────────────
    void library_summary_rename_duplicate() {
        ReportDocument d;
        d.metadata.title = QStringLiteral("Memo");
        d.theme = report::themes::midnight_blue();
        d.components = {mk(1, "heading", "Thesis"), mk(2, "text", "x"), mk(3, "market_data", {}, {{"symbol", "AAPL"}})};
        d.next_id = 4;
        d.unseen_ids = {3, 42}; // 42 was deleted since: not counted
        const auto e = services::library_logic::summarise(7, QString(), d);
        QCOMPARE(e.id, 7);
        QCOMPARE(e.title, QStringLiteral("Memo"));
        QCOMPARE(e.blocks, 3);
        QCOMPARE(e.unseen, 1);
        QVERIFY(e.bound);
        QCOMPARE(e.outline, QStringList({"Thesis"}));
        QCOMPARE(e.kinds, QStringList({"heading", "text", "market_data"}));
        QCOMPARE(e.theme, QStringLiteral("Midnight Blue"));

        services::library_logic::rename(d, QStringLiteral("Memo v2"));
        QCOMPARE(d.metadata.title, QStringLiteral("Memo v2"));
        const ReportDocument dup = services::library_logic::duplicate(d, QStringLiteral("Memo v2 (copy)"));
        QCOMPARE(dup.metadata.title, QStringLiteral("Memo v2 (copy)"));
        QVERIFY(dup.unseen_ids.isEmpty());
        QCOMPARE(dup.components.size(), 3);
    }

    void library_append_queue_for_a_closed_report() {
        // A report that is not open (or the Report Builder never opened this session):
        // blocks land in its stored JSON, marked new, and survive a save / load.
        ReportDocument d;
        d.components = {mk(1, "heading", "Existing")};
        d.next_id = 2;
        report::ReportPayload p;
        p.title = QStringLiteral("From Chat Mode");
        p.source = QStringLiteral("Chat Mode");
        p.markdown = QStringLiteral("Answer.\n\n| x | y |\n|---|---|\n| 1 | 2 |");
        const auto built = report::build_blocks(p);
        const QVector<int> ids = services::library_logic::append_blocks(d, built.blocks, QStringLiteral("Chat Mode"));
        QCOMPARE(ids.size(), built.blocks.size());
        QCOMPARE(ids.first(), 2);
        QCOMPARE(d.next_id, 2 + ids.size());
        const ReportDocument stored = ReportDocument::from_json(d.to_json()).value();
        QCOMPARE(stored.unseen_ids, ids);
        QCOMPARE(stored.unseen_sources, QStringList({"Chat Mode"}));
        QCOMPARE(services::library_logic::summarise(1, QString(), stored).unseen, ids.size());
        // A second sender adds its name once.
        services::library_logic::append_blocks(d, {mk(0, "text", "more")}, QStringLiteral("Chat Mode"));
        QCOMPARE(d.unseen_sources.size(), 1);
        QVERIFY(services::library_logic::append_blocks(d, {}, QStringLiteral("Derivatives")).isEmpty());
        QCOMPARE(d.unseen_sources.size(), 1); // nothing appended: no notice for it
    }
};

QTEST_GUILESS_MAIN(TestReportBuilderCore)
#include "tst_report_builder_core.moc"
