// ToolSelfTest.cpp — see ToolSelfTest.h.

#include "mcp/ToolSelfTest.h"

#include "mcp/JobRegistry.h"
#include "mcp/McpProvider.h"
#include "mcp/core/CoreTools.h"
#include "mcp/core/PythonScriptTool.h"
#include "mcp/server/CallApproval.h"
#include "services/llm/LlmRequestPolicy.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

namespace fincept::mcp {
namespace tool_selftest_detail {

// The whole catalogue rides on every LLM request of every turn. This budget is
// what "far fewer tokens than the old ~900-tool system" means in bytes; a tool
// that grows past it must be trimmed, not the budget raised.
constexpr int kCatalogueBudgetBytes = 16 * 1024;

void out(const QString& s) {
    const QByteArray b = s.toUtf8();
    std::fwrite(b.constData(), 1, static_cast<std::size_t>(b.size()), stdout);
    std::fputc('\n', stdout);
}

void report_list(const QString& label, const QStringList& items, bool critical) {
    if (items.isEmpty()) {
        out(QStringLiteral("    ✓ ") + label + QStringLiteral(": none"));
        return;
    }
    out((critical ? QStringLiteral("    ✗ ") : QStringLiteral("    ⚠ ")) + label + QStringLiteral(": ") +
        QString::number(items.size()));
    for (const auto& it : items)
        out(QStringLiteral("        - ") + it);
}

} // namespace tool_selftest_detail

using namespace tool_selftest_detail;

int dump_tools_json() {
    auto tools = McpProvider::instance().audit_all_tools();
    std::sort(tools.begin(), tools.end(), [](const auto& a, const auto& b) {
        if (a.category != b.category)
            return a.category < b.category;
        return a.name < b.name;
    });
    QJsonArray arr;
    for (const auto& t : tools) {
        arr.append(QJsonObject{
            {"name", t.name},
            {"category", t.category},
            {"description", t.description},
            {"destructive", t.is_destructive},
            {"has_handler", t.has_handler},
            {"enabled", t.enabled},
            {"timeout_ms", t.default_timeout_ms},
            {"input_schema", t.input_schema},
        });
    }
    QJsonObject root{{"count", static_cast<int>(arr.size())}, {"tools", arr}};
    out(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
    return 0;
}

int run_tool_selftest() {
    out(QStringLiteral("\n══════════════════════════════════════════════════════════════"));
    out(QStringLiteral("  MCP CORE TOOL SELF-TEST"));
    out(QStringLiteral("══════════════════════════════════════════════════════════════"));
    int critical = 0;

    // ── [1] Registry ─────────────────────────────────────────────────────────
    const auto tools = McpProvider::instance().audit_all_tools();
    out(QStringLiteral("\n[1] REGISTRY — %1 registered tools").arg(tools.size()));
    QStringList missing, unexpected, no_handler, bad_schema, thin_desc, bad_category, destructive, bad_timeout;
    QSet<QString> registered;
    static const QSet<QString> kCategories = {
        QString::fromLatin1(core::kCategoryUiRead), QString::fromLatin1(core::kCategoryUiControl),
        QString::fromLatin1(core::kCategoryNavigation), QString::fromLatin1(core::kCategoryAnalytics)};
    static const QRegularExpression name_rx(QStringLiteral("^[a-z][a-z0-9_]{1,40}$"));
    int catalogue_bytes = 0;
    for (const auto& t : tools) {
        registered.insert(t.name);
        if (!core::core_tool_names().contains(t.name))
            unexpected << t.name;
        if (!t.has_handler || !t.is_async)
            no_handler << t.name;
        if (!name_rx.match(t.name).hasMatch())
            bad_schema << (t.name + QStringLiteral(" (name not ^[a-z][a-z0-9_]+$)"));
        const QJsonObject schema = t.input_schema;
        if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object")) {
            bad_schema << (t.name + QStringLiteral(" (type != object)"));
        } else {
            const QJsonObject props = schema.value(QStringLiteral("properties")).toObject();
            for (const auto& r : schema.value(QStringLiteral("required")).toArray()) {
                if (!props.contains(r.toString()))
                    bad_schema << (t.name + QStringLiteral(" (required '%1' not declared)").arg(r.toString()));
            }
        }
        const int desc_len = static_cast<int>(t.description.trimmed().size());
        if (desc_len < 40 || desc_len > 700)
            thin_desc << QStringLiteral("%1 (%2 chars)").arg(t.name).arg(desc_len);
        if (!kCategories.contains(t.category))
            bad_category << QStringLiteral("%1 (\"%2\")").arg(t.name, t.category);
        // Risk is judged per ACTION inside the tools (approval dialog), so no
        // core tool may carry the blanket destructive flag — that would make the
        // in-app chat refuse it outright without the destructive grant.
        if (t.is_destructive)
            destructive << t.name;
        // python runs its own jobs and must answer inside 30 s; the UI tools are
        // bounded by their longest wait (approval ≤ 50 s, ui_wait ≤ 60 s).
        const int max_timeout = t.name == QLatin1String("python") ? 30000 : 180000;
        if (t.default_timeout_ms <= 0 || t.default_timeout_ms > max_timeout)
            bad_timeout << QStringLiteral("%1 (%2 ms)").arg(t.name).arg(t.default_timeout_ms);
        catalogue_bytes += static_cast<int>(QJsonDocument(schema).toJson(QJsonDocument::Compact).size()) +
                           static_cast<int>(t.description.toUtf8().size()) + static_cast<int>(t.name.size());
    }
    for (const QString& n : core::core_tool_names()) {
        if (!registered.contains(n))
            missing << n;
    }
    report_list(QStringLiteral("core tools missing"), missing, true);
    report_list(QStringLiteral("unexpected tools registered"), unexpected, true);
    report_list(QStringLiteral("tools without an async handler"), no_handler, true);
    report_list(QStringLiteral("invalid schemas / names"), bad_schema, true);
    report_list(QStringLiteral("descriptions outside 40–700 chars"), thin_desc, true);
    report_list(QStringLiteral("unknown categories"), bad_category, true);
    report_list(QStringLiteral("tools flagged destructive"), destructive, true);
    report_list(QStringLiteral("timeouts out of range"), bad_timeout, true);
    critical += static_cast<int>(missing.size() + unexpected.size() + no_handler.size() + bad_schema.size() +
                                 thin_desc.size() + bad_category.size() + destructive.size() + bad_timeout.size());

    // ── [2] Token budget ─────────────────────────────────────────────────────
    out(QStringLiteral("\n[2] TOKEN BUDGET — whole catalogue %1 bytes (budget %2, ~%3 tokens)")
            .arg(catalogue_bytes)
            .arg(kCatalogueBudgetBytes)
            .arg(catalogue_bytes / 4));
    if (catalogue_bytes > kCatalogueBudgetBytes) {
        out(QStringLiteral("    ✗ the catalogue outgrew its budget — trim descriptions / schemas"));
        ++critical;
    } else {
        out(QStringLiteral("    ✓ within budget"));
    }

    // ── [3] Job lifecycle (the python tool's jobs) ──────────────────────────
    {
        out(QStringLiteral("\n[3] JOB LIFECYCLE"));
        QStringList errs;
        auto& jobs = JobRegistry::instance();
        const QString id = jobs.create(QStringLiteral("python:__selftest"), 90000);
        auto snap = jobs.status(id);
        if (!snap || snap->state != JobState::Running)
            errs << QStringLiteral("a new job is not Running");
        if (auto flag = jobs.cancel_flag(id); !flag || flag->load())
            errs << QStringLiteral("a new job has no clear cancel flag (python passes it to PythonRunner)");
        jobs.set_progress(id, 0.42, QStringLiteral("halfway"));
        snap = jobs.status(id);
        if (!snap || snap->progress < 0.41 || snap->progress > 0.43 || snap->message != QLatin1String("halfway"))
            errs << QStringLiteral("progress did not reach the snapshot");
        QElapsedTimer t;
        t.start();
        jobs.wait_for(id, 300);
        if (t.elapsed() < 250)
            errs << QStringLiteral("wait_for(300) returned early without the job finishing");
        if (!jobs.request_cancel(id) || !jobs.is_cancelled(id))
            errs << QStringLiteral("cancellation did not latch");
        if (auto flag = jobs.cancel_flag(id); !flag || !flag->load())
            errs << QStringLiteral("the cancel flag handed to PythonRunner did not flip");
        jobs.complete(id, ToolResult::ok(QStringLiteral("finished")));
        if (auto r = jobs.take_result(id); !r || r->message != QLatin1String("finished"))
            errs << QStringLiteral("take_result did not return the stored payload");
        jobs.complete(id, ToolResult::fail(QStringLiteral("late loser")));
        if (auto r2 = jobs.take_result(id); r2 && r2->message != QLatin1String("finished"))
            errs << QStringLiteral("a second complete() overwrote the winning result");
        // Jobs belong to the caller that started them.
        const QString owned = jobs.create(QStringLiteral("python:__selftest_owned"), 90000, QStringLiteral("agent-a"));
        auto status_as = [&owned](const QString& who, const QString& action) {
            ScopedCallOrigin as(who);
            return McpProvider::instance().call_tool(QStringLiteral("python"),
                                                     QJsonObject{{QStringLiteral("action"), action},
                                                                 {QStringLiteral("job_id"), owned},
                                                                 {QStringLiteral("wait_s"), 0}});
        };
        const ToolResult foreign = status_as(QStringLiteral("agent-b"), QStringLiteral("status"));
        if (foreign.success || !foreign.error.contains(QStringLiteral("Unknown job")))
            errs << QStringLiteral("another caller could read a job it did not start: ") + foreign.message.left(80);
        const ToolResult foreign_cancel = status_as(QStringLiteral("agent-b"), QStringLiteral("cancel"));
        if (foreign_cancel.success || jobs.is_cancelled(owned))
            errs << QStringLiteral("another caller could cancel a job it did not start");
        const ToolResult own = status_as(QStringLiteral("agent-a"), QStringLiteral("status"));
        if (!own.success)
            errs << QStringLiteral("the owner could not read its own job: ") + own.error.left(80);
        jobs.request_cancel(owned);
        jobs.complete(owned, ToolResult::ok(QStringLiteral("done")));
        report_list(QStringLiteral("job lifecycle failures"), errs, true);
        critical += static_cast<int>(errs.size());
    }

    // ── [4] Batch planning ───────────────────────────────────────────────────
    {
        out(QStringLiteral("\n[4] TOOL-CALL BATCH PLANNING"));
        QStringList errs;
        using Batches = std::vector<std::pair<std::size_t, std::size_t>>;
        auto describe = [](const Batches& b) {
            QStringList parts;
            for (const auto& [x, y] : b)
                parts << QStringLiteral("[%1,%2)").arg(x).arg(y);
            return parts.join(QLatin1Char(' '));
        };
        auto check = [&](const QString& label, const std::vector<bool>& barrier, int fanout, const Batches& want) {
            const Batches got = ai_chat::detail::plan_tool_batches(barrier, fanout);
            if (got != want)
                errs << QStringLiteral("%1: got %2, expected %3").arg(label, describe(got), describe(want));
        };
        check(QStringLiteral("reads fan out together"), {false, false, false}, 4, {{0, 3}});
        check(QStringLiteral("fanout caps the batch"), {false, false, false, false, false}, 2,
              {{0, 2}, {2, 4}, {4, 5}});
        check(QStringLiteral("a barrier runs alone"), {false, false, true, false, false}, 4, {{0, 2}, {2, 3}, {3, 5}});
        check(QStringLiteral("consecutive barriers stay serial"), {true, true}, 4, {{0, 1}, {1, 2}});
        // UI-changing tools must be barriers: two ui_act calls racing would
        // interleave their steps on the GUI thread.
        const QString prefix = QString::fromLatin1(INTERNAL_SERVER_ID) + QStringLiteral("__");
        for (const QString& n : {QStringLiteral("ui_act"), QStringLiteral("ui_invoke"), QStringLiteral("ui_open"),
                                 QStringLiteral("ui_workspace")}) {
            if (registered.contains(n) && !ai_chat::detail::tool_is_destructive(prefix + n))
                errs << QStringLiteral("%1 is not a barrier — it could run concurrently with another UI step").arg(n);
        }
        for (const QString& n :
             {QStringLiteral("ui_snapshot"), QStringLiteral("ui_read"), QStringLiteral("ui_overview")}) {
            if (registered.contains(n) && ai_chat::detail::tool_is_destructive(prefix + n))
                errs << QStringLiteral("%1 is a barrier — read-only tools should fan out").arg(n);
        }
        report_list(QStringLiteral("batch planning failures"), errs, true);
        critical += static_cast<int>(errs.size());
    }

    // ── [5] Python script policy ─────────────────────────────────────────────
    {
        out(QStringLiteral("\n[5] PYTHON SCRIPT POLICY"));
        QStringList errs;
        using core::python_scripts::classify;
        using core::python_scripts::normalize_script_key;
        using core::python_scripts::ScriptRisk;
        const QList<QPair<QString, QString>> accepted = {
            {QStringLiteral("fred_data"), QStringLiteral("fred_data")},
            {QStringLiteral("fred_data.py"), QStringLiteral("fred_data")},
            {QStringLiteral("scripts/fred_data.py"), QStringLiteral("fred_data")},
            {QStringLiteral("Analytics\\corporateFinance\\lbo\\lbo_model.py"),
             QStringLiteral("Analytics/corporateFinance/lbo/lbo_model")},
        };
        for (const auto& [in, want] : accepted) {
            QString err;
            const QString got = normalize_script_key(in, &err);
            if (got != want)
                errs << QStringLiteral("normalize(\"%1\") = \"%2\", expected \"%3\" (%4)").arg(in, got, want, err);
        }
        const QStringList rejected = {
            QStringLiteral("../secrets"),  QStringLiteral("Analytics/../../x"),
            QStringLiteral("/etc/passwd"), QStringLiteral("C:/Windows/system32/x"),
            QStringLiteral(".hidden/x"),   QStringLiteral("a//b"),
            QStringLiteral("x;rm -rf"),    QString(),
        };
        for (const QString& in : rejected) {
            QString err;
            if (!normalize_script_key(in, &err).isEmpty())
                errs << QStringLiteral("normalize(\"%1\") accepted a path outside the scripts tree").arg(in);
        }
        auto expect = [&](const QString& key, const QStringList& args, ScriptRisk want, const char* label) {
            if (classify(key, args) != want)
                errs << QStringLiteral("%1 → wrong risk (%2)").arg(key, QString::fromLatin1(label));
        };
        expect(QStringLiteral("fred_data"), {QStringLiteral("series"), QStringLiteral("GDP")}, ScriptRisk::Normal,
               "should run");
        expect(QStringLiteral("exchange/place_order"), {}, ScriptRisk::Sensitive, "orders ask first");
        expect(QStringLiteral("prediction_polymarket"), {QStringLiteral("place_order")}, ScriptRisk::Sensitive,
               "an order command asks first");
        expect(QStringLiteral("prediction_polymarket"), {QStringLiteral("get_orders")}, ScriptRisk::Normal,
               "a read command must not ask");
        expect(QStringLiteral("prediction_kalshi"), {QStringLiteral("place_orders_batch")}, ScriptRisk::Sensitive,
               "a batch order command asks first");
        expect(QStringLiteral("prediction_kalshi"), {QStringLiteral("cancel_order_batch")}, ScriptRisk::Sensitive,
               "a batch cancel asks first");
        expect(QStringLiteral("some_connector"), {QStringLiteral("--action"), QStringLiteral("buy")},
               ScriptRisk::Sensitive, "an order passed as --action asks first");
        expect(QStringLiteral("some_connector"), {QStringLiteral("--args"), QStringLiteral("{\"action\":\"sell\"}")},
               ScriptRisk::Sensitive, "an order inside a JSON argument asks first");
        expect(QStringLiteral("some_connector"), {QStringLiteral("--action"), QStringLiteral("quote")},
               ScriptRisk::Normal, "a read passed as --action must not ask");
        if (classify(QStringLiteral("Analytics/backtesting/vectorbt/vectorbt_provider"), {}, /*runs_code=*/true) !=
            ScriptRisk::Sensitive)
            errs << QStringLiteral("a script that exec()s its input must ask first");
        expect(QStringLiteral("agents/deepagents/cli"), {}, ScriptRisk::Sensitive, "LLM agents ask first");
        expect(QStringLiteral("notebook_cell_runner"), {}, ScriptRisk::Blocked, "arbitrary code is never run");
        expect(QStringLiteral("agents/_tools/verify_all_agents"), {}, ScriptRisk::Blocked, "maintenance tools blocked");
        expect(QStringLiteral("Analytics/backtesting/vectorbt/test_broker_data_injection"), {}, ScriptRisk::Blocked,
               "tests blocked");
        // A PAPER workflow run: an order script is refused outright — no approval
        // prompt, and before Python is even needed.
        {
            ScopedPaperRun paper;
            const ToolResult r = McpProvider::instance().call_tool(
                QStringLiteral("python"),
                QJsonObject{{QStringLiteral("action"), QStringLiteral("run")},
                            {QStringLiteral("script"), QStringLiteral("prediction_polymarket")},
                            {QStringLiteral("args"), QJsonArray{QStringLiteral("place_order")}}});
            if (r.success || !r.error.contains(QStringLiteral("PAPER")))
                errs << QStringLiteral("a PAPER run must refuse an order script outright, got: %1")
                            .arg((r.success ? r.message : r.error).left(160));
        }
        report_list(QStringLiteral("script policy failures"), errs, true);
        critical += static_cast<int>(errs.size());
    }

    out(QStringLiteral("\n──────────────────────────────────────────────────────────────"));
    const bool pass = critical == 0;
    out(QStringLiteral("  VERDICT: %1  (%2 tools, %3 bytes of catalogue)")
            .arg(pass ? QStringLiteral("PASS ✓") : QStringLiteral("FAIL ✗"))
            .arg(tools.size())
            .arg(catalogue_bytes));
    out(QStringLiteral("══════════════════════════════════════════════════════════════\n"));
    std::fflush(stdout);
    return pass ? 0 : 1;
}

} // namespace fincept::mcp
