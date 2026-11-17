// CoreTools.cpp — the core tools: the ui_* catalogue (CoreToolCatalog.cpp)
// bound to the GUI-thread UI engine (Ui*.cpp), plus the Python script tool and
// the Report Builder tool. See CoreTools.h for the design.

#include "mcp/core/CoreTools.h"

#include "core/logging/Logger.h"
#include "mcp/core/CoreToolCatalog.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/PythonScriptTool.h"
#include "mcp/core/ReportTool.h"
#include "mcp/core/UiEngine.h"
#include "mcp/core/UiDispatch.h"
#include "mcp/core/UiErrors.h"
#include "mcp/server/CallApproval.h"

#include <QCoreApplication>
#include <QHash>


namespace fincept::mcp::core {

namespace core_tools_detail {

using EngineFn = UiEngineFn;

/// The engine entry point of each ui_* tool.
const QHash<QString, EngineFn>& engine_functions() {
    static const QHash<QString, EngineFn> k = {
        {QStringLiteral("ui_overview"),
         [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_overview(a, std::move(r)); }},
        {QStringLiteral("ui_open"),
         [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_open(a, std::move(r)); }},
        {QStringLiteral("ui_snapshot"),
         [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_snapshot(a, std::move(r)); }},
        {QStringLiteral("ui_act"),
         [](const QJsonObject& a, const QString& origin, ui::CancelCheck cancelled, ui::Reply r) {
             ui::ui_act(a, origin, std::move(cancelled), std::move(r));
         }},
        {QStringLiteral("ui_read"),
         [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_read(a, std::move(r)); }},
        {QStringLiteral("ui_wait"),
         [](const QJsonObject& a, const QString&, ui::CancelCheck cancelled, ui::Reply r) {
             ui::ui_wait(a, std::move(cancelled), std::move(r));
         }},
        {QStringLiteral("ui_screenshot"),
         [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_screenshot(a, std::move(r)); }},
        {QStringLiteral("ui_workspace"),
         [](const QJsonObject& a, const QString& origin, ui::CancelCheck, ui::Reply r) {
             ui::ui_workspace(a, origin, std::move(r));
         }},
        {QStringLiteral("ui_invoke"),
         [](const QJsonObject& a, const QString& origin, ui::CancelCheck, ui::Reply r) {
             ui::ui_invoke(a, origin, std::move(r));
         }},
    };
    return k;
}

} // namespace core_tools_detail

using namespace core_tools_detail;

const QStringList& core_tool_names() {
    static const QStringList k = {
        QStringLiteral("ui_overview"),   QStringLiteral("ui_open"),      QStringLiteral("ui_snapshot"),
        QStringLiteral("ui_act"),        QStringLiteral("ui_read"),      QStringLiteral("ui_wait"),
        QStringLiteral("ui_screenshot"), QStringLiteral("ui_workspace"), QStringLiteral("ui_invoke"),
        QStringLiteral("python"),        QStringLiteral("report"),
    };
    return k;
}

std::vector<ToolDef> get_core_tools() {
    std::vector<ToolDef> tools;
    for (UiToolSpec& spec : ui_tool_specs()) {
        ToolDef t;
        t.name = spec.name;
        t.category = QString::fromLatin1(spec.category);
        t.description = spec.description;
        t.input_schema = std::move(spec.schema);
        // A watchdog, not a job threshold: tools are never backgrounded generically,
        // so ui_act (approvals, batches) and ui_wait carry the longest budget they need.
        t.default_timeout_ms = spec.timeout_ms;
        t.async_handler = ui_gui_handler(spec.name, engine_functions().value(spec.name));
        tools.push_back(std::move(t));
    }

    // ── python ───────────────────────────────────────────────────────────────
    tools.push_back(make_python_tool());

    // ── report (Report Builder: read the reports, build them with the user's approval) ──
    tools.push_back(make_report_tool());

    return tools;
}

} // namespace fincept::mcp::core
