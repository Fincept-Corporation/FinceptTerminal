// ReportBuilderTools.cpp — MCP tools for live report authoring.
//
// Every mutating tool runs on a worker thread (LlmService::do_request lives
// on QtConcurrent::run). To talk to the main-thread-resident
// ReportBuilderService, we route through QMetaObject::invokeMethod with
// Qt::BlockingQueuedConnection. This guarantees:
//   • The service mutates on the main thread (Qt's invariant for QObjects
//     created there, including the QUndoStack).
//   • The tool returns only after the mutation has completed and signals
//     have been emitted — so when the LLM asks "now update component X",
//     the previous add has fully committed.
//   • Side effects visible to the user (canvas re-render) happen before
//     the LLM moves on.

#include "mcp/tools/ReportBuilderTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/report/ReportDocument.h"
#include "services/report_builder/ReportBuilderService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QThread>

namespace fincept::mcp::tools {

namespace {

constexpr const char* RB_TOOLS_TAG = "ReportBuilderTools";
namespace rep = ::fincept::report;
using Service = ::fincept::services::ReportBuilderService;

// ── Threading helper ─────────────────────────────────────────────────────────
//
// Run `fn` on the service's owning thread (the main thread) and block until
// it completes. If we're already on that thread we just call directly.

template <typename F>
void run_on_service_thread(F&& fn) {
    auto* svc = &Service::instance();
    if (QThread::currentThread() == svc->thread()) {
        fn();
        return;
    }
    QMetaObject::invokeMethod(svc, std::forward<F>(fn), Qt::BlockingQueuedConnection);
}

// ── Auto-navigate ────────────────────────────────────────────────────────────
//
// On the first mutation in an LLM tool-call burst, publish nav.switch_screen
// so the user can see the live updates. Stays silent for subsequent
// mutations within ~5 seconds (tracked by service.note_llm_mutation timer).

void maybe_request_navigation() {
    static std::atomic<qint64> last_nav_ms{0};
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 prev = last_nav_ms.load();
    if (now - prev < 5000)
        return;
    if (!last_nav_ms.compare_exchange_strong(prev, now))
        return;
    LOG_INFO(RB_TOOLS_TAG, "Auto-navigate: publishing nav.switch_screen → report_builder");
    fincept::EventBus::instance().publish(
        "nav.switch_screen",
        QVariantMap{{"screen_id", "report_builder"}, {"tab_index", 8}});
}

// Helper: called at the top of every mutating tool.
void on_llm_mutation_start() {
    maybe_request_navigation();
    auto* svc = &Service::instance();
    QMetaObject::invokeMethod(svc, [svc]() { svc->note_llm_mutation(); }, Qt::QueuedConnection);
}

// ── JSON helpers ─────────────────────────────────────────────────────────────

QMap<QString, QString> json_to_string_map(const QJsonObject& obj) {
    QMap<QString, QString> out;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QJsonValue& v = it.value();
        if (v.isString())
            out[it.key()] = v.toString();
        else if (v.isDouble()) {
            // Preserve integer formatting where possible (config strings
            // like "rows":"3" are user-visible).
            double d = v.toDouble();
            if (d == static_cast<qint64>(d))
                out[it.key()] = QString::number(static_cast<qint64>(d));
            else
                out[it.key()] = QString::number(d);
        } else if (v.isBool())
            out[it.key()] = v.toBool() ? "true" : "false";
        else
            out[it.key()] = QString::fromUtf8(QJsonDocument::fromVariant(v.toVariant()).toJson(QJsonDocument::Compact));
    }
    return out;
}

QJsonObject component_to_json(const rep::ReportComponent& c) {
    QJsonObject cfg;
    for (auto it = c.config.cbegin(); it != c.config.cend(); ++it)
        cfg[it.key()] = it.value();
    return QJsonObject{{"id", c.id}, {"type", c.type}, {"content", c.content}, {"config", cfg}};
}

QJsonObject metadata_to_json(const rep::ReportMetadata& m) {
    return QJsonObject{
        {"title", m.title},
        {"author", m.author},
        {"company", m.company},
        {"date", m.date},
        {"header_left", m.header_left},
        {"header_center", m.header_center},
        {"header_right", m.header_right},
        {"footer_left", m.footer_left},
        {"footer_center", m.footer_center},
        {"footer_right", m.footer_right},
        {"show_page_numbers", m.show_page_numbers},
    };
}

bool valid_component_type(const QString& t) {
    static const QStringList valid = {"heading",   "text",     "table",       "image",   "chart",
                                      "sparkline", "stats_block", "callout", "code",    "divider",
                                      "quote",     "list",     "market_data", "page_break", "toc"};
    return valid.contains(t);
}

} // namespace

// ── Tool definitions ─────────────────────────────────────────────────────────

std::vector<ToolDef> get_report_builder_tools() {
    std::vector<ToolDef> tools;

    // ── report_get_state ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_get_state";
        t.description = "Read the current Report Builder document — components (with stable ids), metadata, theme, "
                        "and current file path. Call this BEFORE editing existing components so you reference the "
                        "right ids. Returns: {components:[{id,type,content,config}], metadata:{...}, theme, "
                        "current_file, recent_files}.";
        t.category = "report-builder";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& svc = Service::instance();
            QJsonArray comps;
            for (const auto& c : svc.components())
                comps.append(component_to_json(c));
            QJsonObject result{
                {"components", comps},
                {"metadata", metadata_to_json(svc.metadata())},
                {"theme", svc.theme().name},
                {"current_file", svc.current_file()},
                {"recent_files", QJsonArray::fromStringList(svc.recent_files())},
            };
            return ToolResult::ok(QString("Report has %1 components").arg(comps.size()), result);
        };
        tools.push_back(std::move(t));
    }

    // ── report_add_component ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_add_component";
        t.description =
            "Insert a component into the report. Returns the assigned stable `id` and the resulting `index`. "
            "The id is what you use to update/remove/move this component later. "
            "Types: heading, text, table, image, chart, sparkline, stats_block, callout, code, divider, quote, "
            "list, market_data, page_break, toc.\n"
            "\n"
            "Content guidelines:\n"
            "• heading: short title text, no markdown. Adds top margin + accent rule automatically.\n"
            "• text: paragraph body. SUPPORTS MARKDOWN: **bold**, *italic*, `code`, [links](url). "
            "  Use **bold** liberally to highlight key figures. One paragraph per component is ideal.\n"
            "• list: each line becomes a bullet. Markdown emphasis works inside items.\n"
            "• quote: italic block-quoted text, supports markdown.\n"
            "• callout: info/warning/success/danger boxes. Set config={'style':'warning','heading':'Risk'}.\n"
            "• code: monospaced code block (no syntax highlighting).\n"
            "• divider: horizontal rule, no content needed.\n"
            "• page_break: forces a new physical page.\n"
            "• toc: auto-generated from heading components — place near the top.\n"
            "\n"
            "Data-bearing components (use `config`):\n"
            "• table: config={'csv':'Header1,Header2,Header3|Row1Col1,Row1Col2,Row1Col3|...'}. "
            "  Use | to separate rows, , to separate cells. First row is bolded as header. "
            "  Numeric cells right-align automatically. Example for financials:\n"
            "    'Metric,FY22,FY23,FY24|Revenue ($M),81462,96773,97690|Gross Margin,25.6%,17.7%,17.9%'\n"
            "• chart: config={'chart_type':'bar'|'line'|'pie','title':'Revenue','data':'10,20,30',"
            "  'labels':'Q1,Q2,Q3','width':'640'}. Data is CSV numbers, labels CSV strings.\n"
            "• stats_block: config={'title':'Key Stats','data':'P/E:42.1\\nMarket Cap:$890B\\n...'}. "
            "  Use Label:Value lines, \\n separated.\n"
            "• market_data: config={'symbol':'TSLA'}. The symbol is enough — quote auto-fills.\n"
            "• image: config={'path':'/abs/path.png','width':'400','caption':'Fig. 1','align':'center'}.\n"
            "• sparkline: config={'title':'Revenue trend','data':'10,12,11,15','current':'15.2','change_pct':'+8.4'}.\n"
            "\n"
            "If insert_at is omitted, the component is appended.";
        t.category = "report-builder";
        t.input_schema.properties = QJsonObject{
            {"type",
             QJsonObject{{"type", "string"},
                         {"description",
                          "Component type: heading, text, table, image, chart, sparkline, stats_block, callout, "
                          "code, divider, quote, list, market_data, page_break, toc"}}},
            {"content",
             QJsonObject{{"type", "string"},
                         {"description", "Text content (used by heading/text/quote/code/list/callout)"}}},
            {"config",
             QJsonObject{{"type", "object"},
                         {"description", "Type-specific config (rows/cols, chart_type/title/data/labels, symbol, "
                                         "style, width/caption/align, …)"}}},
            {"insert_at",
             QJsonObject{{"type", "integer"},
                         {"description", "Zero-based index to insert at; omit to append at the end"}}},
        };
        t.input_schema.required = {"type"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            LOG_INFO(RB_TOOLS_TAG, QString("report_add_component called args=%1")
                                       .arg(QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))
                                                .left(300)));
            QString type = args.value("type").toString().trimmed();
            if (type.isEmpty())
                return ToolResult::fail("Missing 'type'");
            if (!valid_component_type(type))
                return ToolResult::fail("Unknown component type: " + type);

            on_llm_mutation_start();

            rep::ReportComponent c;
            c.type = type;
            c.content = args.value("content").toString();
            c.config = json_to_string_map(args.value("config").toObject());

            int insert_at = args.contains("insert_at") ? args.value("insert_at").toInt(-1) : -1;

            int new_id = -1, idx = -1;
            run_on_service_thread([&]() {
                auto& svc = Service::instance();
                new_id = svc.add_component(c, insert_at);
                idx = svc.index_of(new_id);
            });

            LOG_INFO(RB_TOOLS_TAG, QString("report_add_component done id=%1 index=%2 type=%3")
                                       .arg(new_id).arg(idx).arg(type));
            return ToolResult::ok(QString("Added %1 (id=%2)").arg(type).arg(new_id),
                                  QJsonObject{{"id", new_id}, {"index", idx}, {"type", type}});
        };
        tools.push_back(std::move(t));
    }

    // ── report_update_component ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_update_component";
        t.description =
            "Update an existing component by stable id. If `content` is provided it replaces the current content "
            "(remember: text/list/quote/callout support markdown — use **bold** for emphasis). If `config` is "
            "provided, the supplied keys are merged into the existing config (keys not mentioned are kept). "
            "For tables, set config={'csv':'h1,h2|r1c1,r1c2|...'}. For charts, set "
            "config={'chart_type':...,'data':'...','labels':'...'}. See report_add_component description for "
            "the full config reference. Returns success and the resulting component.";
        t.category = "report-builder";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "integer"}, {"description", "Stable component id from report_add_component "
                                                                    "or report_get_state"}}},
            {"content", QJsonObject{{"type", "string"}, {"description", "New content (omit to keep existing)"}}},
            {"config",
             QJsonObject{{"type", "object"},
                         {"description", "Keys to merge into existing config (omit to keep existing)"}}},
        };
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args.value("id").toInt(0);
            if (id <= 0)
                return ToolResult::fail("Missing or invalid 'id'");

            const bool has_content = args.contains("content");
            QString content = args.value("content").toString();
            QMap<QString, QString> partial = json_to_string_map(args.value("config").toObject());

            on_llm_mutation_start();

            bool ok = false;
            QJsonObject after;
            run_on_service_thread([&]() {
                auto& svc = Service::instance();
                ok = svc.patch_component(id, has_content ? &content : nullptr, partial);
                if (ok) {
                    int idx = svc.index_of(id);
                    if (idx >= 0)
                        after = component_to_json(svc.components()[idx]);
                }
            });

            if (!ok)
                return ToolResult::fail(QString("Component id=%1 not found").arg(id));
            return ToolResult::ok(QString("Updated component id=%1").arg(id), after);
        };
        tools.push_back(std::move(t));
    }

    // ── report_remove_component ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_remove_component";
        t.description = "Remove a component by stable id.";
        t.category = "report-builder";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "integer"}, {"description", "Stable component id"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args.value("id").toInt(0);
            if (id <= 0)
                return ToolResult::fail("Missing or invalid 'id'");
            on_llm_mutation_start();
            bool ok = false;
            run_on_service_thread([&]() { ok = Service::instance().remove_component(id); });
            return ok ? ToolResult::ok(QString("Removed id=%1").arg(id))
                      : ToolResult::fail(QString("Component id=%1 not found").arg(id));
        };
        tools.push_back(std::move(t));
    }

    // ── report_move_component ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_move_component";
        t.description = "Move a component to a new position by stable id. Use `to_index` (zero-based).";
        t.category = "report-builder";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "integer"}, {"description", "Stable component id"}}},
            {"to_index", QJsonObject{{"type", "integer"}, {"description", "New zero-based index"}}},
        };
        t.input_schema.required = {"id", "to_index"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args.value("id").toInt(0);
            int to = args.value("to_index").toInt(-1);
            if (id <= 0 || to < 0)
                return ToolResult::fail("Missing or invalid 'id' / 'to_index'");
            on_llm_mutation_start();
            bool ok = false;
            run_on_service_thread([&]() { ok = Service::instance().move_component(id, to); });
            return ok ? ToolResult::ok(QString("Moved id=%1 to index=%2").arg(id).arg(to))
                      : ToolResult::fail(QString("Component id=%1 not found").arg(id));
        };
        tools.push_back(std::move(t));
    }

    // ── report_clear ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_clear";
        t.description = "Clear the entire report — remove all components and reset metadata to defaults.";
        t.category = "report-builder";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            on_llm_mutation_start();
            run_on_service_thread([]() { Service::instance().clear_document(); });
            return ToolResult::ok("Report cleared");
        };
        tools.push_back(std::move(t));
    }

    // ── report_set_metadata ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_set_metadata";
        t.description = "Update report metadata. Any field omitted from the call is left unchanged.";
        t.category = "report-builder";
        t.input_schema.properties = QJsonObject{
            {"title", QJsonObject{{"type", "string"}}},
            {"author", QJsonObject{{"type", "string"}}},
            {"company", QJsonObject{{"type", "string"}}},
            {"date", QJsonObject{{"type", "string"}, {"description", "YYYY-MM-DD"}}},
            {"header_left", QJsonObject{{"type", "string"}}},
            {"header_center", QJsonObject{{"type", "string"}}},
            {"header_right", QJsonObject{{"type", "string"}}},
            {"footer_left", QJsonObject{{"type", "string"}}},
            {"footer_center", QJsonObject{{"type", "string"}}},
            {"footer_right", QJsonObject{{"type", "string"}}},
            {"show_page_numbers", QJsonObject{{"type", "boolean"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            on_llm_mutation_start();
            QJsonObject after;
            run_on_service_thread([&]() {
                auto& svc = Service::instance();
                rep::ReportMetadata m = svc.metadata();
                if (args.contains("title")) m.title = args.value("title").toString();
                if (args.contains("author")) m.author = args.value("author").toString();
                if (args.contains("company")) m.company = args.value("company").toString();
                if (args.contains("date")) m.date = args.value("date").toString();
                if (args.contains("header_left")) m.header_left = args.value("header_left").toString();
                if (args.contains("header_center")) m.header_center = args.value("header_center").toString();
                if (args.contains("header_right")) m.header_right = args.value("header_right").toString();
                if (args.contains("footer_left")) m.footer_left = args.value("footer_left").toString();
                if (args.contains("footer_center")) m.footer_center = args.value("footer_center").toString();
                if (args.contains("footer_right")) m.footer_right = args.value("footer_right").toString();
                if (args.contains("show_page_numbers"))
                    m.show_page_numbers = args.value("show_page_numbers").toBool();
                svc.set_metadata(m);
                after = metadata_to_json(svc.metadata());
            });
            return ToolResult::ok("Metadata updated", after);
        };
        tools.push_back(std::move(t));
    }

    // ── report_set_theme ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_set_theme";
        t.description = "Set the report color theme. Valid names: 'Light Professional', 'Dark Corporate', "
                        "'Bloomberg Terminal', 'Midnight Blue'.";
        t.category = "report-builder";
        t.input_schema.properties =
            QJsonObject{{"name", QJsonObject{{"type", "string"}, {"description", "Theme name (case-sensitive)"}}}};
        t.input_schema.required = {"name"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString name = args.value("name").toString();
            if (!rep::themes::all_names().contains(name))
                return ToolResult::fail("Unknown theme. Valid: " + rep::themes::all_names().join(", "));
            on_llm_mutation_start();
            run_on_service_thread([&]() { Service::instance().set_theme(rep::themes::by_name(name)); });
            return ToolResult::ok("Theme set to " + name);
        };
        tools.push_back(std::move(t));
    }

    // ── report_apply_template ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_apply_template";
        t.description =
            "Replace the report with a built-in template. Names: 'Blank Report', 'Meeting Notes', 'Investment Memo', "
            "'Stock Research', 'Portfolio Review', 'Watchlist Report', 'Dividend Income Report', "
            "'Daily Market Brief', 'Trade Journal', 'Technical Analysis', 'Pre-Market Checklist', "
            "'Equity Research Report', 'Earnings Review', 'M&A Deal Summary', 'Sector Deep Dive', "
            "'Macro Economic Summary', 'Country Risk Report', 'Central Bank Monitor', "
            "'Crypto Research Report', 'DeFi Protocol Analysis', 'Crypto Portfolio Review', "
            "'Bond Research Report', 'Yield Curve Analysis', 'Quant Strategy Report', "
            "'Risk Management Report', 'Business Performance', 'Project Status Report', "
            "'Financial Statement'.";
        t.category = "report-builder";
        t.input_schema.properties = QJsonObject{{"name", QJsonObject{{"type", "string"}}}};
        t.input_schema.required = {"name"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString name = args.value("name").toString().trimmed();
            if (name.isEmpty())
                return ToolResult::fail("Missing 'name'");
            on_llm_mutation_start();
            run_on_service_thread([&]() { Service::instance().apply_template(name); });
            return ToolResult::ok("Applied template: " + name);
        };
        tools.push_back(std::move(t));
    }

    // ── report_save ────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_save";
        t.description = "Save the report to disk. If `path` is omitted, saves to the current file (errors if none).";
        t.category = "report-builder";
        t.input_schema.properties =
            QJsonObject{{"path", QJsonObject{{"type", "string"}, {"description", "Absolute file path (.fincept)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString path = args.value("path").toString();
            on_llm_mutation_start();
            QString resolved;
            QString err;
            run_on_service_thread([&]() {
                auto& svc = Service::instance();
                QString p = path.isEmpty() ? svc.current_file() : path;
                if (p.isEmpty()) {
                    err = "No path provided and no current file. Pass `path`.";
                    return;
                }
                auto r = svc.save_to(p);
                if (r.is_err()) {
                    err = QString::fromStdString(r.error());
                    return;
                }
                resolved = p;
            });
            if (!err.isEmpty())
                return ToolResult::fail(err);
            return ToolResult::ok("Saved", QJsonObject{{"path", resolved}});
        };
        tools.push_back(std::move(t));
    }

    // ── report_load ────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "report_load";
        t.description = "Load a report from disk, replacing the current document.";
        t.category = "report-builder";
        t.input_schema.properties =
            QJsonObject{{"path", QJsonObject{{"type", "string"}, {"description", "Absolute file path"}}}};
        t.input_schema.required = {"path"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString path = args.value("path").toString();
            if (path.isEmpty())
                return ToolResult::fail("Missing 'path'");
            on_llm_mutation_start();
            QString err;
            run_on_service_thread([&]() {
                auto r = Service::instance().load_from(path);
                if (r.is_err())
                    err = QString::fromStdString(r.error());
            });
            if (!err.isEmpty())
                return ToolResult::fail(err);
            return ToolResult::ok("Loaded", QJsonObject{{"path", path}});
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(RB_TOOLS_TAG, QString("Built %1 report builder tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
