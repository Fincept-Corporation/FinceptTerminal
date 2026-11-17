// tst_notebook_document — the Code tab's notebook model (src/screens/code_editor/NotebookDocument):
// .ipynb read / write round trips (nbformat 4.5, Jupyter files, the old editor's files, nbformat 3),
// kernel event -> output accumulation, terminal text (carriage returns, ANSI), traceback lines,
// outline, display names, markdown rendering and the exports.

#include "screens/code_editor/NotebookDocument.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace fincept::screens::notebook;

namespace {

/// JSON fixture written with ` for " (moc cannot read raw string literals).
QByteArray js(const char* text) {
    return QByteArray(text).replace('`', '"');
}

QJsonObject ev(const QByteArray& json) {
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class TstNotebookDocument : public QObject {
    Q_OBJECT
  private slots:
    void round_trip_keeps_everything() {
        NotebookDoc doc;
        doc.metadata[QStringLiteral("custom")] = QStringLiteral("kept");
        doc.set_environment(QStringLiteral("numpy1"));
        NotebookCell md;
        md.type = NotebookCell::Type::Markdown;
        md.source = QStringLiteral("# Title\nText");
        md.id = QStringLiteral("md-1");
        md.attachments[QStringLiteral("a.png")] = QJsonObject{{QStringLiteral("image/png"), QStringLiteral("AAAA")}};
        NotebookCell code;
        code.id = QStringLiteral("code-1");
        code.source = QStringLiteral("x = 1\nprint(x)");
        code.title = QStringLiteral("Setup");
        code.execution_count = 3;
        code.output_collapsed = true;
        code.metadata[QStringLiteral("tags")] = QJsonArray{QStringLiteral("parameters")};
        CellOutput s;
        s.text = QStringLiteral("1\n");
        CellOutput t;
        t.kind = CellOutput::Kind::Table;
        t.text = QStringLiteral("   a\n0  1");
        t.table = QJsonObject{{QStringLiteral("columns"), QJsonArray{QStringLiteral("a")}},
                              {QStringLiteral("index"), QJsonArray{QStringLiteral("0")}},
                              {QStringLiteral("rows"), QJsonArray{QJsonArray{1}}},
                              {QStringLiteral("total_rows"), 1}};
        CellOutput img;
        img.kind = CellOutput::Kind::Image;
        img.png = QByteArray("\x89PNG\r\n\x1a\n-fake", 13);
        CellOutput err;
        err.kind = CellOutput::Kind::Error;
        err.ename = QStringLiteral("ZeroDivisionError");
        err.evalue = QStringLiteral("division by zero");
        err.traceback = {QStringLiteral("Traceback (most recent call last):"),
                         QStringLiteral("  File \"<cell>\", line 2, in <module>")};
        code.outputs = {s, t, img, err};
        doc.cells = {md, code};

        NotebookDoc back;
        QString why;
        QVERIFY2(parse_ipynb(serialize_ipynb(doc), &back, &why), qPrintable(why));
        QCOMPARE(back.cells.size(), 2);
        QCOMPARE(back.environment(), QStringLiteral("numpy1"));
        QCOMPARE(back.metadata.value(QStringLiteral("custom")).toString(), QStringLiteral("kept"));
        QCOMPARE(back.cells[0].id, QStringLiteral("md-1"));
        QCOMPARE(back.cells[0].source, md.source);
        QVERIFY(back.cells[0].attachments.contains(QStringLiteral("a.png")));
        const NotebookCell& c = back.cells[1];
        QCOMPARE(c.source, code.source);
        QCOMPARE(c.title, QStringLiteral("Setup"));
        QCOMPARE(c.execution_count, 3);
        QVERIFY(c.output_collapsed);
        QCOMPARE(c.metadata.value(QStringLiteral("tags")).toArray().size(), 1);
        QVERIFY(!c.metadata.contains(QStringLiteral("fincept_title"))); // mirrored into title, not duplicated
        QCOMPARE(c.outputs.size(), 4);
        QCOMPARE(c.outputs[0].kind, CellOutput::Kind::Stream);
        QCOMPARE(c.outputs[0].text, QStringLiteral("1\n"));
        QCOMPARE(c.outputs[1].kind, CellOutput::Kind::Table);
        QCOMPARE(c.outputs[1].table.value(QStringLiteral("total_rows")).toInt(), 1);
        QCOMPARE(c.outputs[2].kind, CellOutput::Kind::Image);
        QCOMPARE(c.outputs[2].png, img.png);
        QCOMPARE(c.outputs[3].kind, CellOutput::Kind::Error);
        QCOMPARE(c.outputs[3].error_line, 2);
        // A second round trip is byte-identical.
        QCOMPARE(serialize_ipynb(back), serialize_ipynb(doc));
    }

    void reads_jupyter_files_and_keeps_unknown_renditions() {
        const QByteArray nb = js("{\n"
"          `cells`: [\n"
"            {`cell_type`: `code`, `execution_count`: 7, `id`: `abc`, `metadata`: {`collapsed`: true},\n"
"             `source`: [`import pandas as pd\\n`, `df`],\n"
"             `outputs`: [\n"
"               {`output_type`: `execute_result`, `execution_count`: 7, `metadata`: {},\n"
"                `data`: {`text/plain`: [`   a\\n`, `0  1`], `text/html`: [`<table></table>`]}},\n"
"               {`output_type`: `display_data`, `metadata`: {`isolated`: true},\n"
"                `data`: {`image/png`: `iVBORw0K\\nGgo=`, `text/plain`: [`<Figure>`]}},\n"
"               {`output_type`: `stream`, `name`: `stderr`, `text`: `\\u001b[31mwarn\\u001b[0m\\n`},\n"
"               {`output_type`: `error`, `ename`: `KeyError`, `evalue`: `'x'`,\n"
"                `traceback`: [`\\u001b[0;31mKeyError\\u001b[0m`, `  File \\`<cell>\\`, line 9`]}\n"
"             ]},\n"
"            {`cell_type`: `markdown`, `metadata`: {}, `source`: `## Notes`}\n"
"          ],\n"
"          `metadata`: {`kernelspec`: {`name`: `python3`, `display_name`: `Python 3`},\n"
"                       `language_info`: {`name`: `python`, `version`: `3.11`}},\n"
"          `nbformat`: 4, `nbformat_minor`: 4\n"
"        }");
        NotebookDoc doc;
        QVERIFY(parse_ipynb(nb, &doc));
        QCOMPARE(doc.environment(), QStringLiteral("numpy2"));
        QCOMPARE(doc.cells.size(), 2);
        const NotebookCell& c = doc.cells[0];
        QCOMPARE(c.id, QStringLiteral("abc"));
        QCOMPARE(c.source, QStringLiteral("import pandas as pd\ndf")); // Jupyter source: no trailing newline added or removed
        QVERIFY(c.output_collapsed);
        QCOMPARE(c.outputs.size(), 4);
        QCOMPARE(c.outputs[0].kind, CellOutput::Kind::Result);
        QCOMPARE(c.outputs[0].text, QStringLiteral("   a\n0  1"));
        QVERIFY(c.outputs[0].extra_data.contains(QStringLiteral("text/html")));
        QCOMPARE(c.outputs[1].kind, CellOutput::Kind::Image);
        QVERIFY(c.outputs[1].is_display);
        QVERIFY(!c.outputs[1].png.isEmpty());
        QCOMPARE(c.outputs[1].metadata.value(QStringLiteral("isolated")).toBool(), true);
        QVERIFY(c.outputs[2].is_stderr);
        QCOMPARE(c.outputs[2].text, QStringLiteral("warn\n")); // terminal colours dropped
        QCOMPARE(c.outputs[3].error_line, 9);
        QVERIFY(!doc.cells[1].id.isEmpty()); // generated for a cell without one

        // The HTML rendition survives a save.
        const QJsonObject saved = QJsonDocument::fromJson(serialize_ipynb(doc)).object();
        const QJsonObject out0 = saved.value(QStringLiteral("cells")).toArray().at(0).toObject()
                                     .value(QStringLiteral("outputs")).toArray().at(0).toObject();
        QCOMPARE(out0.value(QStringLiteral("output_type")).toString(), QStringLiteral("execute_result"));
        QVERIFY(out0.value(QStringLiteral("data")).toObject().contains(QStringLiteral("text/html")));
        QCOMPARE(saved.value(QStringLiteral("nbformat_minor")).toInt(), 5);
    }

    void reads_the_old_editor_format() {
        // What CodeEditorScreen wrote before format 2: every line saved with "\n" (the
        // last one included, growing by one per save), no ids, kernelspec only.
        const QByteArray nb = js("{\n"
"          `cells`: [\n"
"            {`cell_type`: `code`, `execution_count`: 2, `metadata`: {`fincept_title`: `Load`},\n"
"             `source`: [`x = 1\\n`, `x\\n`, `\\n`], `outputs`: [\n"
"               {`output_type`: `execute_result`, `execution_count`: 2, `metadata`: {},\n"
"                `data`: {`text/plain`: [`1`], `application/vnd.fincept.table+json`: {`columns`: [`v`], `rows`: [[1]]}}}]},\n"
"            {`cell_type`: `markdown`, `metadata`: {}, `source`: [`# Hi\\n`]}\n"
"          ],\n"
"          `metadata`: {`kernelspec`: {`display_name`: `Python 3 (Fincept numpy1)`, `language`: `python`,\n"
"                                      `name`: `fincept-numpy1`}},\n"
"          `nbformat`: 4, `nbformat_minor`: 5\n"
"        }");
        NotebookDoc doc;
        QVERIFY(parse_ipynb(nb, &doc));
        QCOMPARE(doc.environment(), QStringLiteral("numpy1"));
        QCOMPARE(doc.cells[0].source, QStringLiteral("x = 1\nx"));
        QCOMPARE(doc.cells[0].title, QStringLiteral("Load"));
        QCOMPARE(doc.cells[0].outputs[0].kind, CellOutput::Kind::Table);
        QCOMPARE(doc.cells[1].source, QStringLiteral("# Hi"));
        // Saved again it is format 2 and reads back unchanged (no more growth).
        NotebookDoc again;
        QVERIFY(parse_ipynb(serialize_ipynb(doc), &again));
        QCOMPARE(again.cells[0].source, QStringLiteral("x = 1\nx"));
        QCOMPARE(serialize_ipynb(again), serialize_ipynb(doc));
    }

    void reads_nbformat_3() {
        const QByteArray nb = js("{\n"
"          `metadata`: {`name`: `old`}, `nbformat`: 3, `nbformat_minor`: 0,\n"
"          `worksheets`: [{`cells`: [\n"
"            {`cell_type`: `heading`, `level`: 2, `metadata`: {}, `source`: [`Intro`]},\n"
"            {`cell_type`: `code`, `input`: [`print(1)`], `language`: `python`, `metadata`: {},\n"
"             `prompt_number`: 4, `outputs`: [{`output_type`: `stream`, `stream`: `stdout`, `text`: [`1\\n`]},\n"
"                                             {`output_type`: `pyout`, `prompt_number`: 4, `text`: [`2`]}]}\n"
"          ]}]\n"
"        }");
        NotebookDoc doc;
        QVERIFY(parse_ipynb(nb, &doc));
        QCOMPARE(doc.cells.size(), 2);
        QCOMPARE(doc.cells[0].type, NotebookCell::Type::Markdown);
        QCOMPARE(doc.cells[0].source, QStringLiteral("## Intro"));
        QCOMPARE(doc.cells[1].source, QStringLiteral("print(1)"));
        QCOMPARE(doc.cells[1].execution_count, 4);
        QCOMPARE(doc.cells[1].outputs.size(), 2);
        QCOMPARE(doc.cells[1].outputs[1].text, QStringLiteral("2"));
    }

    void rejects_non_notebooks() {
        NotebookDoc doc;
        QString why;
        QVERIFY(!parse_ipynb("not json", &doc, &why));
        QVERIFY(!why.isEmpty());
        QVERIFY(!parse_ipynb("{\"a\": 1}", &doc, &why));
    }

    void kernel_events_accumulate_in_order() {
        QVector<CellOutput> outs;
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`stream`,`name`:`stdout`,`text`:`a\\n`}"))), 0);
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`stream`,`name`:`stdout`,`text`:`b\\n`}"))), 0);
        QCOMPARE(outs[0].text, QStringLiteral("a\nb\n"));
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`stream`,`name`:`stderr`,`text`:`w\\n`}"))), 1);
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`result`,`text`:`   x\\n0  1`}"))), 2);
        // The table that follows its repr turns that result into a table.
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`display`,`mime`:`application/x-table`,`columns`:[`x`],\n"
"                                                `rows`:[[1]],`total_rows`:1,`id`:`r1`}"))), 2);
        QCOMPARE(outs[2].kind, CellOutput::Kind::Table);
        QCOMPARE(outs[2].text, QStringLiteral("   x\n0  1"));
        QVERIFY(!outs[2].table.contains(QStringLiteral("id")));
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`display`,`mime`:`image/png`,`data`:`iVBORw0KGgo=`}"))), 3);
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`stream`,`name`:`stdout`,`text`:`after\\n`}"))), 4);
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`error`,`ename`:`E`,`evalue`:`v`,`line`:3,\n"
"                                                `traceback`:[`Traceback\\n`,`  File \\`<cell>\\`, line 3\\n`]}"))), 5);
        QCOMPARE(outs[5].error_line, 3);
        QCOMPARE(outs[5].traceback.at(1), QStringLiteral("  File \"<cell>\", line 3"));
        QCOMPARE(append_kernel_event(outs, ev(js("{`type`:`done`}"))), -1);
        QCOMPARE(outs.size(), 6);
    }

    void terminal_text() {
        QCOMPARE(apply_terminal_text(QString(), QStringLiteral("10%\r20%\r30%")), QStringLiteral("30%"));
        QCOMPARE(apply_terminal_text(QStringLiteral("done\n"), QStringLiteral("x\r\ny\n")), QStringLiteral("done\nx\ny\n"));
        // A trailing CR waits for the next chunk.
        const QString a = apply_terminal_text(QString(), QStringLiteral("line1\nprogress 10%\r"));
        QCOMPARE(a, QStringLiteral("line1\nprogress 10%\r"));
        QCOMPARE(apply_terminal_text(a, QStringLiteral("progress 99%\n")), QStringLiteral("line1\nprogress 99%\n"));
        QCOMPARE(strip_ansi(QStringLiteral("\x1b[1;31mred\x1b[0m")), QStringLiteral("red"));
    }

    void traceback_line() {
        QCOMPARE(error_line_from_traceback({QStringLiteral("  File \"<cell>\", line 4, in <module>"),
                                            QStringLiteral("  File \"<cell>\", line 12, in f")}),
                 12);
        QCOMPARE(error_line_from_traceback({QStringLiteral("  File \"x.py\", line 4")}), 0);
    }

    void names_and_outline() {
        NotebookCell c;
        c.type = NotebookCell::Type::Markdown;
        c.source = QStringLiteral("\n# 📘 Fincept Notebook — Time Value\nmore");
        QCOMPARE(cell_display_name(c), QStringLiteral("Fincept Notebook — Time Value"));
        c.title = QStringLiteral(" Mine ");
        QCOMPARE(cell_display_name(c), QStringLiteral("Mine"));
        NotebookCell code;
        code.source = QStringLiteral("# Load prices\nimport yfinance");
        QCOMPARE(cell_display_name(code), QStringLiteral("Load prices"));

        NotebookDoc doc;
        NotebookCell md;
        md.type = NotebookCell::Type::Markdown;
        md.source = QStringLiteral("# Top\ntext\n```\n# not a heading\n```\n## Sub");
        doc.cells = {md, code};
        const auto o = outline(doc);
        QCOMPARE(o.size(), 3);
        QCOMPARE(o[0].level, 1);
        QCOMPARE(o[0].text, QStringLiteral("Top"));
        QCOMPARE(o[1].level, 2);
        QCOMPARE(o[2].level, 0);
        QCOMPARE(o[2].cell_index, 1);
    }

    void fingerprint_tracks_saved_state() {
        NotebookDoc doc;
        NotebookCell c;
        c.source = QStringLiteral("a");
        doc.cells = {c};
        const QString fp = fingerprint(doc);
        doc.cells[0].outputs.append(CellOutput{});
        doc.cells[0].execution_count = 5;
        QCOMPARE(fingerprint(doc), fp); // running a cell is not an unsaved edit
        doc.cells[0].source = QStringLiteral("b");
        QVERIFY(fingerprint(doc) != fp);
    }

    void markdown_rendering() {
        HtmlPalette pal;
        pal.font_px = 14;
        const QString html = markdown_to_html(
            QStringLiteral("# H1\nSome **bold** and *it* and `x*y*z`\n\n- a\n- b\n\n1. one\n\n"
                           "| A | B |\n|---|---|\n| 1 | 2 |\n\n[ok](https://x.y) [bad](javascript:alert(1))\n<script>"),
            pal, false);
        QVERIFY(html.contains(QStringLiteral("<h1>H1</h1>")));
        QVERIFY(html.contains(QStringLiteral("<b>bold</b>")));
        QVERIFY(html.contains(QStringLiteral("<i>it</i>")));
        QVERIFY(html.contains(QStringLiteral("<code>x*y*z</code>"))); // no emphasis inside code
        QVERIFY(html.contains(QStringLiteral("<ul><li>a</li><li>b</li></ul>")));
        QVERIFY(html.contains(QStringLiteral("<ol><li>one</li></ol>")));
        QVERIFY(html.contains(QStringLiteral("<th>A</th>")));
        QVERIFY(html.contains(QStringLiteral("<td>2</td>")));
        QVERIFY(html.contains(QStringLiteral("<a href=\"https://x.y\">ok</a>")));
        QVERIFY(!html.contains(QStringLiteral("javascript:")));
        QVERIFY(!html.contains(QStringLiteral("<script>")));
    }

    void exports() {
        NotebookDoc doc;
        NotebookCell md;
        md.type = NotebookCell::Type::Markdown;
        md.source = QStringLiteral("# T\nline");
        NotebookCell code;
        code.source = QStringLiteral("print(1)");
        CellOutput img;
        img.kind = CellOutput::Kind::Image;
        img.png = QByteArray("PNGDATA");
        code.outputs = {img};
        doc.cells = {md, code};
        const QString py = export_python(doc, QStringLiteral("Demo"));
        QVERIFY(py.contains(QStringLiteral("# %% [markdown]\n# # T\n# line\n")));
        QVERIFY(py.contains(QStringLiteral("# %%\nprint(1)\n")));
        HtmlPalette pal;
        const QString html = export_html(doc, QStringLiteral("Demo <1>"), pal);
        QVERIFY(html.contains(QStringLiteral("<title>Demo &lt;1&gt;</title>")));
        QVERIFY(html.contains(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(QByteArray("PNGDATA").toBase64())));
        QVERIFY(html.contains(QStringLiteral("print(1)")));
    }

    void identifiers() {
        const QStringList ids = identifiers_in(QStringLiteral("prices = load(sym)\nreturns = prices.pct_change()"));
        QVERIFY(ids.contains(QStringLiteral("prices")));
        QVERIFY(ids.contains(QStringLiteral("pct_change")));
        QVERIFY(!ids.contains(QStringLiteral("returns returns")));
        QCOMPARE(ids.count(QStringLiteral("prices")), 1);
    }
};

QTEST_GUILESS_MAIN(TstNotebookDocument)
#include "tst_notebook_document.moc"
