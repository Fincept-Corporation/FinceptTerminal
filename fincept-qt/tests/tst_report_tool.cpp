// tst_report_tool.cpp — the `report` MCP tool's handlers (mcp/core/ReportTool.cpp) over a fake
// backend: read actions answer without asking; every write action asks for approval with a
// description of exactly what changes, does nothing when refused, and goes through the
// backend (ReportInbox / the service) only when approved; arguments are validated; results
// are capped.

#include "core/report/ReportBlocks.h"
#include "mcp/core/ReportTool.h"
#include "mcp/server/CallApproval.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

// An absolute path on the platform the test runs on: "C:/<rest>" is absolute only where drive
// letters exist, so on Linux / macOS the same test used to be refused as a relative path.
static QString rt_abs(const QString& rest) {
#ifdef Q_OS_WIN
    return QStringLiteral("C:/") + rest;
#else
    return QStringLiteral("/") + rest;
#endif
}

using namespace fincept;
using namespace fincept::mcp;
using namespace fincept::mcp::core;
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

class FakeBackend : public ReportToolBackend {
  public:
    ReportDocument open_doc;
    ToolApprovalVerdict verdict = ToolApprovalVerdict::Approved;
    QVector<ToolApprovalRequest> asked;
    QVector<ReportComponent> appended;
    int updated_block = 0;
    QString updated_text;
    int refreshes = 0;
    QString exported_format, exported_path;
    bool exists = false;

    void list_reports(const QString&, int limit,
                      std::function<void(const QVector<ListEntry>&, const QString&)> done) override {
        QVector<ListEntry> v;
        for (int i = 0; i < 80 && v.size() < limit; ++i)
            v.append({i, QStringLiteral("R%1").arg(i), QString(), 3, i == 0});
        done(v, QString());
    }
    void load(qint64 id, std::function<void(bool, const ReportDocument&, qint64, bool, const QString&)> done) override {
        if (id > 100) {
            done(false, {}, id, false, QStringLiteral("The report is no longer in the library."));
            return;
        }
        done(true, open_doc, id == 0 ? 5 : id, id == 0, QString());
    }
    void append(qint64, const QVector<ReportComponent>& blocks, const QString&,
                std::function<void(bool, const QVector<int>&, const QString&, const QString&)> done) override {
        appended = blocks;
        QVector<int> ids;
        for (int i = 0; i < blocks.size(); ++i)
            ids << 100 + i;
        done(true, ids, open_doc.metadata.title, QString());
    }
    void update_text(qint64, int block_id, const QString& text, std::function<void(bool, const QString&)> done) override {
        updated_block = block_id;
        updated_text = text;
        done(true, QString());
    }
    void refresh(qint64, std::function<void(bool, int, int, const QString&)> done) override {
        ++refreshes;
        done(true, 2, 1, QString());
    }
    void export_report(qint64, const QString& format, const QString& path,
                       std::function<void(bool, const QString&, const QStringList&, const QString&)> done) override {
        exported_format = format;
        exported_path = path;
        done(true, path, {}, QString());
    }
    void ask_approval(const ToolApprovalRequest& r, std::function<void(ToolApprovalVerdict)> done) override {
        asked.append(r);
        done(verdict);
    }
    QString default_export_dir() const override { return rt_abs(QStringLiteral("Users/x/Documents/Fincept Reports")); }
    bool file_exists(const QString&) const override { return exists; }
};

ToolResult call(const std::shared_ptr<FakeBackend>& b, const QJsonObject& args) {
    ReportToolHandler h(b);
    ToolResult out;
    bool got = false;
    h.call(args, QStringLiteral("test-client"), [&](ToolResult r) {
        out = r;
        got = true;
    });
    if (!got)
        out = ToolResult::fail(QStringLiteral("no reply"));
    return out;
}

std::shared_ptr<FakeBackend> backend() {
    auto b = std::make_shared<FakeBackend>();
    b->open_doc.metadata.title = QStringLiteral("Q3 Review");
    b->open_doc.components = {mk(1, "heading", "Overview", {{"level", "1"}}), mk(2, "text", "Revenue grew."),
                              mk(3, "table", {}, {{"csv", "A,B|1,2"}, {"bind", report::quote_binding("AAPL").to_string()}}),
                              mk(4, "list", {}, {{"bind", report::headlines_binding(5).to_string()}})};
    return b;
}

} // namespace

class TestReportTool : public QObject {
    Q_OBJECT

  private slots:
    void reads_need_no_approval() {
        auto b = backend();
        ToolResult r = call(b, {{"action", "get_report"}});
        QVERIFY(r.success);
        const QJsonObject o = r.data.toObject();
        QCOMPARE(o.value("title").toString(), QStringLiteral("Q3 Review"));
        QVERIFY(o.value("open_in_report_builder").toBool());
        const QJsonArray blocks = o.value("blocks").toArray();
        QCOMPARE(blocks.size(), 4);
        QCOMPARE(blocks[1].toObject().value("text").toString(), QStringLiteral("Revenue grew."));
        QVERIFY(blocks[2].toObject().contains("data")); // the live recipe is described
        r = call(b, {{"action", "list_reports"}, {"limit", 500}});
        QVERIFY(r.success);
        QCOMPARE(r.data.toObject().value("reports").toArray().size(), 50); // capped
        QVERIFY(b->asked.isEmpty());
        r = call(b, {{"action", "get_report"}, {"report_id", 999}});
        QVERIFY(!r.success);
    }

    void outline_is_capped() {
        ReportDocument d;
        for (int i = 1; i <= 400; ++i)
            d.components.append(mk(i, "text", QString(500, QLatin1Char('x'))));
        const QJsonObject o = ReportToolHandler::outline_json(d, 1, false, 1000);
        QCOMPARE(o.value("blocks").toArray().size(), 300);
        QVERIFY(o.contains("blocks_truncated"));
        QVERIFY(o.value("blocks").toArray()[0].toObject().value("text").toString().size() <= 160);
        QVERIFY(QJsonDocument(o).toJson(QJsonDocument::Compact).size() < 80 * 1024);
    }

    void append_asks_then_adds() {
        auto b = backend();
        ToolResult r = call(b, {{"action", "append_blocks"},
                                {"title", "Outlook"},
                                {"markdown", "Text here.\n\n| A | B |\n|---|---|\n| 1 | 2 |\n| 3 | 4 |"}});
        QVERIFY2(r.success, qPrintable(r.error));
        QCOMPARE(b->asked.size(), 1);
        QVERIFY(b->asked[0].description.contains(QStringLiteral("Q3 Review")));
        QVERIFY(b->asked[0].description.contains(QStringLiteral("heading “Outlook”")));
        QVERIFY(b->asked[0].description.contains(QStringLiteral("3 rows × 2 columns")));
        QCOMPARE(b->appended.size(), 3);
        QCOMPARE(r.data.toObject().value("block_ids").toArray().size(), 3);
    }

    void refused_writes_change_nothing() {
        auto b = backend();
        b->verdict = ToolApprovalVerdict::Denied;
        ToolResult r = call(b, {{"action", "append_blocks"}, {"markdown", "x"}});
        QVERIFY(!r.success);
        QVERIFY(r.error.contains(QStringLiteral("declined")));
        QVERIFY(b->appended.isEmpty());
        r = call(b, {{"action", "update_block_text"}, {"block_id", 2}, {"text", "New"}});
        QVERIFY(!r.success);
        QCOMPARE(b->updated_block, 0);
        b->verdict = ToolApprovalVerdict::TimedOut;
        r = call(b, {{"action", "refresh_report_data"}});
        QVERIFY(!r.success);
        QCOMPARE(b->refreshes, 0);
        b->verdict = ToolApprovalVerdict::Unavailable;
        r = call(b, {{"action", "export_report"}, {"format", "pdf"}});
        QVERIFY(!r.success);
        QVERIFY(b->exported_path.isEmpty());
    }

    void update_text_validates() {
        auto b = backend();
        QVERIFY(!call(b, {{"action", "update_block_text"}, {"text", "x"}}).success);              // no block id
        QVERIFY(!call(b, {{"action", "update_block_text"}, {"block_id", 77}, {"text", "x"}}).success); // unknown
        QVERIFY(!call(b, {{"action", "update_block_text"}, {"block_id", 3}, {"text", "x"}}).success);  // a table
        QVERIFY(!call(b, {{"action", "update_block_text"}, {"block_id", 4}, {"text", "x"}}).success);  // bound list
        QVERIFY(b->asked.isEmpty());
        ToolResult r = call(b, {{"action", "update_block_text"}, {"block_id", 2}, {"text", "Revenue grew 12%."}});
        QVERIFY(r.success);
        QCOMPARE(b->asked.size(), 1);
        QVERIFY(b->asked[0].description.contains(QStringLiteral("block 2")));
        QVERIFY(b->asked[0].description.contains(QStringLiteral("Revenue grew 12%.")));
        QCOMPARE(b->updated_block, 2);
        QCOMPARE(b->updated_text, QStringLiteral("Revenue grew 12%."));
    }

    void refresh_and_export() {
        auto b = backend();
        ToolResult r = call(b, {{"action", "refresh_report_data"}});
        QVERIFY(r.success);
        QCOMPARE(b->refreshes, 1);
        QVERIFY(b->asked[0].description.contains(QStringLiteral("2 live-data block(s)")));
        QCOMPARE(r.data.toObject().value("failed").toInt(), 1);
        // Export: default folder, extension added, overwrite flagged.
        b->exists = true;
        r = call(b, {{"action", "export_report"}, {"format", "markdown"}});
        QVERIFY2(r.success, qPrintable(r.error));
        QCOMPARE(b->exported_format, QStringLiteral("md"));
        QCOMPARE(b->exported_path, rt_abs(QStringLiteral("Users/x/Documents/Fincept Reports/Q3 Review.md")));
        QVERIFY(b->asked.last().destructive);
        QVERIFY(b->asked.last().description.contains(QStringLiteral("REPLACES")));
        QVERIFY(!call(b, {{"action", "export_report"}, {"format", "xls"}}).success);
        QVERIFY(!call(b, {{"action", "export_report"}, {"path", "relative/out.pdf"}}).success);
        QCOMPARE(ReportToolHandler::normalise_export_path(rt_abs(QStringLiteral("r/x")), QString(), QString(),
                                                          QStringLiteral("odt"), nullptr),
                 rt_abs(QStringLiteral("r/x.odt")));
    }

    // Each write asks with the kind of what it does and the caller's stable ids: an export writes
    // a file (kind file); appending, editing and refreshing blocks are ordinary edits (generic,
    // grantable). The ids are the ones set around call(), kept through the asynchronous load.
    void approvals_carry_kind_and_caller() {
        auto b = backend();
        {
            ScopedCallClient caller(QStringLiteral("codex"), QStringLiteral("run-0123456789abcdef"));
            QVERIFY(call(b, {{"action", "append_blocks"}, {"markdown", "x"}}).success);
            QVERIFY(call(b, {{"action", "update_block_text"}, {"block_id", 2}, {"text", "New"}}).success);
            QVERIFY(call(b, {{"action", "refresh_report_data"}}).success);
            QVERIFY(call(b, {{"action", "export_report"}, {"format", "pdf"}}).success);
        }
        QCOMPARE(b->asked.size(), 4);
        const QStringList kinds{QStringLiteral("generic"), QStringLiteral("generic"), QStringLiteral("generic"),
                                QStringLiteral("file")};
        for (int i = 0; i < b->asked.size(); ++i) {
            QCOMPARE(b->asked[i].kind, kinds[i]);
            QCOMPARE(b->asked[i].client_id, QStringLiteral("codex"));
            QCOMPARE(b->asked[i].run_id, QStringLiteral("run-0123456789abcdef"));
            QCOMPARE(b->asked[i].client, QStringLiteral("test-client"));
        }
        // No caller set: no ids invented (CoreApproval fills them on the calling thread).
        QVERIFY(call(b, {{"action", "export_report"}, {"format", "pdf"}}).success);
        QVERIFY(b->asked.last().client_id.isEmpty());
        QCOMPARE(b->asked.last().kind, QStringLiteral("file"));
    }

    void unknown_action() {
        auto b = backend();
        const ToolResult r = call(b, {{"action", "delete_everything"}});
        QVERIFY(!r.success);
        QVERIFY(r.error.contains(QStringLiteral("list_reports")));
    }
};

QTEST_GUILESS_MAIN(TestReportTool)
#include "tst_report_tool.moc"
