// tst_report_renderer.cpp — the Report Builder renderer's range bookkeeping.
//
// The page view (DocumentCanvas) and the exports keep one QTextDocument per section and
// remember the character range every block occupies; an edit replaces only that range
// (Qt then re-lays the section from there) instead of rebuilding every page. These tests
// prove that an in-place replace / insert / remove leaves exactly the document a full
// rebuild would produce (same text, same block count), around tables, lists and stats
// blocks, and that long sections paginate. Runs on the offscreen platform.

#include "core/report/ReportChartSpec.h"
#include "core/report/ReportDocument.h"
#include "screens/report_builder/ReportChartObject.h"
#include "screens/report_builder/ReportRenderer.h"

#include <QApplication>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtTest>

#include <memory>

using namespace fincept;
namespace rb = fincept::screens::rb;
using report::ReportComponent;

namespace {

struct Range {
    int id, start, end;
};

ReportComponent mk(int id, const QString& type, const QString& content = {}, QMap<QString, QString> cfg = {}) {
    ReportComponent c;
    c.id = id;
    c.type = type;
    c.content = content;
    c.config = cfg;
    return c;
}

QTextDocument* build(const QVector<ReportComponent>& comps, const rb::RenderContext& ctx, QVector<Range>* ranges) {
    QTextDocument* doc = rb::new_section_document(ctx, nullptr);
    QTextCursor c(doc);
    rb::write_header(c, ctx);
    for (const auto& comp : comps) {
        c.movePosition(QTextCursor::End);
        const int start = c.position();
        rb::write_component(c, comp, ctx);
        if (ranges)
            ranges->append({comp.id, start, c.position()});
    }
    return doc;
}

void replace(QTextDocument* doc, QVector<Range>& ranges, int idx, const ReportComponent& comp, const rb::RenderContext& ctx) {
    Range& r = ranges[idx];
    QTextCursor c(doc);
    c.setPosition(r.start);
    c.setPosition(r.end, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.setPosition(r.start);
    rb::write_component(c, comp, ctx);
    const int delta = c.position() - r.end;
    r.end = c.position();
    for (int i = idx + 1; i < ranges.size(); ++i) {
        ranges[i].start += delta;
        ranges[i].end += delta;
    }
}

QVector<ReportComponent> sample() {
    return {mk(1, "heading", "Overview", {{"level", "1"}}),
            mk(2, "text", "First paragraph with **bold** text.", {{"format", "markdown"}}),
            mk(3, "table", {}, {{"csv", "Name,Value|Alpha,1|Beta,2"}, {"title", "Figures"}}),
            mk(4, "list", "one\ntwo\nthree"),
            mk(5, "stats_block", {}, {{"title", "KPIs"}, {"data", "P/E: 21\nYield: 1.2%\nBeta: 1.1"}}),
            mk(6, "text", "Closing words."),
            mk(7, "callout", "Mind the gap", {{"style", "warning"}, {"heading", "Risk"}})};
}

} // namespace

class TestReportRenderer : public QObject {
    Q_OBJECT

  private slots:
    void replace_equals_full_rebuild_data() {
        QTest::addColumn<int>("index");
        for (int i = 0; i < 7; ++i)
            QTest::newRow(qPrintable(QStringLiteral("block %1").arg(i + 1))) << i;
    }
    void replace_equals_full_rebuild() {
        QFETCH(int, index);
        report::ReportMetadata meta;
        const rb::RenderContext ctx = rb::make_context(sample(), meta, report::themes::light_professional(), false);
        QVector<Range> ranges;
        std::unique_ptr<QTextDocument> doc(build(sample(), ctx, &ranges));
        QVector<ReportComponent> edited = sample();
        ReportComponent& c = edited[index];
        if (c.type == QLatin1String("table"))
            c.config["csv"] = QStringLiteral("Name,Value,Extra|Alpha,1,x|Beta,2,y|Gamma,3,z");
        else if (c.type == QLatin1String("stats_block"))
            c.config["data"] = QStringLiteral("P/E: 22");
        else
            c.content += QStringLiteral(" (edited, now longer than before)");
        replace(doc.get(), ranges, index, c, ctx);
        std::unique_ptr<QTextDocument> fresh(build(edited, ctx, nullptr));
        QCOMPARE(doc->toPlainText(), fresh->toPlainText());
        QCOMPARE(doc->blockCount(), fresh->blockCount());
        // Every other range still covers its own block.
        for (int i = 0; i < ranges.size(); ++i) {
            QVERIFY(ranges[i].end > ranges[i].start);
            if (i > 0)
                QCOMPARE(ranges[i].start, ranges[i - 1].end);
        }
        QCOMPARE(ranges.last().end, doc->characterCount() - 1);
    }

    void insert_and_remove_equal_full_rebuild() {
        report::ReportMetadata meta;
        const rb::RenderContext ctx = rb::make_context(sample(), meta, report::themes::light_professional(), false);
        QVector<Range> ranges;
        std::unique_ptr<QTextDocument> doc(build(sample(), ctx, &ranges));
        // Remove the table (index 2).
        {
            const Range r = ranges[2];
            QTextCursor c(doc.get());
            c.setPosition(r.start);
            c.setPosition(r.end, QTextCursor::KeepAnchor);
            c.removeSelectedText();
            ranges.removeAt(2);
            for (int i = 2; i < ranges.size(); ++i) {
                ranges[i].start -= r.end - r.start;
                ranges[i].end -= r.end - r.start;
            }
        }
        QVector<ReportComponent> expected = sample();
        expected.removeAt(2);
        std::unique_ptr<QTextDocument> fresh(build(expected, ctx, nullptr));
        QCOMPARE(doc->toPlainText(), fresh->toPlainText());
        // Insert a new block after the list (now index 2).
        const ReportComponent added = mk(9, "quote", "Inserted quote.");
        QTextCursor c(doc.get());
        c.setPosition(ranges[2].end);
        const int start = c.position();
        rb::write_component(c, added, ctx);
        expected.insert(3, added);
        std::unique_ptr<QTextDocument> fresh2(build(expected, ctx, nullptr));
        QCOMPARE(doc->toPlainText(), fresh2->toPlainText());
        QVERIFY(c.position() > start);
    }

    void long_sections_paginate_like_print() {
        QVector<ReportComponent> many;
        for (int i = 0; i < 120; ++i)
            many.append(mk(i + 1, "text", QStringLiteral("Paragraph %1. ").arg(i + 1).repeated(12)));
        report::ReportMetadata meta;
        rb::RenderContext ctx = rb::make_context(many, meta, report::themes::light_professional(), true);
        std::unique_ptr<QTextDocument> doc(build(many, ctx, nullptr));
        const int a4_pages = doc->pageCount();
        QVERIFY2(a4_pages > 5, qPrintable(QString::number(a4_pages)));
        // Landscape: wider and shorter content area -> a different page count, same text.
        meta.landscape = true;
        rb::RenderContext land = rb::make_context(many, meta, report::themes::light_professional(), true);
        std::unique_ptr<QTextDocument> doc2(build(many, land, nullptr));
        QVERIFY(doc2->pageCount() != a4_pages);
        // Same words; only the rules (drawn as wide as the text column) differ in length.
        auto words = [](const QString& text) {
            QStringList keep;
            for (const QString& line : text.split(QLatin1Char('\n')))
                if (!line.startsWith(QChar(0x2500)))
                    keep << line;
            return keep;
        };
        QCOMPARE(words(doc2->toPlainText()), words(doc->toPlainText()));
        const rb::PageGeometry g = rb::page_geometry(meta);
        QVERIFY(g.page.width() > g.page.height());
    }

    void unfilled_blocks_are_left_out_of_exports() {
        QVERIFY(rb::is_unfilled(mk(1, "text")));
        QVERIFY(rb::is_unfilled(mk(2, "text", {}, {{"placeholder", "Your thesis"}})));
        QVERIFY(!rb::is_unfilled(mk(3, "text", "x")));
        QVERIFY(rb::is_unfilled(mk(4, "table", {}, {{"bind", R"({"source":"peers","params":{"symbol":"AAPL"},"view":"table","v":1})"}})));
        QVERIFY(!rb::is_unfilled(mk(5, "chart", {}, {{"series", R"([{"name":"A","values":[1,2]}])"}})));
        QVERIFY(rb::is_unfilled(mk(6, "market_data", {}, {{"symbol", "AAPL"}})));
    }

    // ── Second stage: cover, numbering, variables, formatted tables, chart objects ──

    void cover_replaces_the_title_header() {
        QVector<ReportComponent> comps{mk(1, "cover", {}, {{"subtitle", "Q3 results"}, {"disclaimer", "Not advice."}}),
                                       mk(2, "heading", "Overview", {{"level", "1"}})};
        report::ReportMetadata meta;
        meta.title = QStringLiteral("{{symbol}} preview");
        meta.variables.insert(QStringLiteral("symbol"), QStringLiteral("AAPL"));
        const rb::RenderContext ctx = rb::make_context(comps, meta, report::themes::light_professional(), true);
        QVERIFY(ctx.has_cover);
        std::unique_ptr<QTextDocument> doc(build(comps, ctx, nullptr));
        const QString text = doc->toPlainText();
        QCOMPARE(text.count(QStringLiteral("AAPL preview")), 1); // on the cover only, no title header
        QVERIFY(text.contains(QStringLiteral("Q3 results")));
        QVERIFY(doc->pageCount() >= 2); // the report starts on the next page
    }

    void headings_are_numbered_everywhere() {
        QVector<ReportComponent> comps{mk(1, "toc"), mk(2, "heading", "Intro", {{"level", "1"}}),
                                       mk(3, "heading", "Detail", {{"level", "2"}})};
        report::ReportMetadata meta;
        meta.number_headings = true;
        const rb::RenderContext ctx = rb::make_context(comps, meta, report::themes::light_professional(), true);
        std::unique_ptr<QTextDocument> doc(build(comps, ctx, nullptr));
        const QString text = doc->toPlainText();
        QVERIFY(text.contains(QStringLiteral("1  Intro")));
        QVERIFY(text.contains(QStringLiteral("1.1  Detail")));
        QCOMPARE(text.count(QStringLiteral("1.1  Detail")), 2); // heading + contents line
    }

    void variables_expand_and_unresolved_are_marked_on_screen_only() {
        QVector<ReportComponent> comps{mk(1, "text", "{{symbol}} in {{period}}")};
        report::ReportMetadata meta;
        meta.variables.insert(QStringLiteral("symbol"), QStringLiteral("MSFT"));
        for (bool for_export : {false, true}) {
            const rb::RenderContext ctx = rb::make_context(comps, meta, report::themes::light_professional(), for_export);
            QVector<Range> ranges;
            std::unique_ptr<QTextDocument> doc(build(comps, ctx, &ranges));
            QVERIFY(doc->toPlainText().contains(QStringLiteral("MSFT in {{period}}")));
            const QTextCursor hit = doc->find(QStringLiteral("{{period}}"));
            QVERIFY(!hit.isNull());
            QTextCursor probe(doc.get());
            probe.setPosition(hit.selectionStart() + 1);
            const bool marked = probe.charFormat().hasProperty(QTextFormat::BackgroundBrush);
            QCOMPARE(marked, !for_export);
        }
    }

    void formatted_tables_print_their_formats() {
        QVector<ReportComponent> comps{mk(1, "table", {},
                                          {{"csv", "Item,Value|A,-1234.5|B,10"},
                                           {"col_fmt", R"([{},{"type":"currency","ccy":"USD","neg":"brackets"}])"},
                                           {"table_style", R"({"totals":"sum"})"}})};
        report::ReportMetadata meta;
        const rb::RenderContext ctx = rb::make_context(comps, meta, report::themes::light_professional(), true);
        std::unique_ptr<QTextDocument> doc(build(comps, ctx, nullptr));
        const QString text = doc->toPlainText();
        QVERIFY2(text.contains(QStringLiteral("($1,234.50)")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("Total")));
        QVERIFY(text.contains(QStringLiteral("($1,224.50)")));
        // The same cells reach the exports.
        const auto ft = rb::formatted_table(comps[0], ctx);
        QCOMPARE(ft.totals_row, 3);
    }

    void charts_are_fixed_size_objects_with_a_handler() {
        const ReportComponent chart = mk(1, "chart", {}, {{"chart_type", "bar"}, {"data", "1,2,3"}, {"width", "400"}});
        report::ReportMetadata meta;
        rb::RenderContext ctx = rb::make_context({chart}, meta, report::themes::light_professional(), true);
        screens::ReportChartObject handler(screens::ReportChartObject::Mode::Vector);
        ctx.chart_object = &handler;
        QTextDocument* doc = rb::new_section_document(ctx, nullptr);
        handler.install(doc);
        QTextCursor c(doc);
        rb::write_header(c, ctx);
        c.movePosition(QTextCursor::End);
        rb::write_component(c, chart, ctx);
        const QTextCursor obj = doc->find(QString(QChar::ObjectReplacementCharacter));
        QVERIFY(!obj.isNull());
        QTextCursor at(doc);
        at.setPosition(obj.selectionEnd());
        QCOMPARE(at.charFormat().objectType(), screens::ReportChartObject::kObjectType);
        const QSize size = rb::chart_size(chart, ctx);
        QCOMPARE(size.width(), 400);
        QCOMPARE(size.height(), 200);
        // Identical charts share one key (content + palette + size).
        QCOMPARE(handler.add(report::chart::from_config(chart.config), ctx.chart_palette, ctx.font_family, size),
                 handler.add(report::chart::from_config(chart.config), ctx.chart_palette, ctx.font_family, size));
        delete doc;
    }
};

QTEST_MAIN(TestReportRenderer)
#include "tst_report_renderer.moc"
