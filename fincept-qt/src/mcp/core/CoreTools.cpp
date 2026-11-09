// CoreTools.cpp — the ten core tools: schemas, descriptions and dispatch onto
// the GUI-thread UI engine (Ui*.cpp) and the Python script tool. See
// CoreTools.h for the design.
//
// Descriptions are written for the model and kept short: the whole catalogue
// rides on every request of every turn, so each word is paid for repeatedly.

#include "mcp/core/CoreTools.h"

#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/PythonScriptTool.h"
#include "mcp/core/UiEngine.h"
#include "mcp/server/CallApproval.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <exception>

namespace fincept::mcp::core {

namespace core_tools_detail {

using EngineFn =
    std::function<void(const QJsonObject& args, const QString& origin, ui::CancelCheck cancelled, ui::Reply reply)>;

/// An async handler that runs `fn` on the GUI thread and resolves through the
/// provider's single-winner guard. The caller's thread is never blocked here;
/// McpProvider's watchdog bounds the whole call by the tool's timeout.
AsyncToolHandler gui_handler(const QString& tool, EngineFn fn) {
    return [tool, fn](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
        const common::Resolver resolve = common::make_resolver(ctx, promise);
        if (!QCoreApplication::instance()) {
            resolve(ToolResult::fail(QStringLiteral("The terminal UI is not running.")));
            return;
        }
        QString origin = ScopedCallOrigin::current(); // read on the calling thread
        if (ScopedPaperRun::active())
            origin += QLatin1String(kPaperRunOriginSuffix); // the GUI thread cannot see the flag
        const auto is_cancelled = ctx.is_cancelled;
        common::on_gui_thread([tool, fn, args, origin, is_cancelled, resolve]() {
            ui::Reply reply = [resolve](ToolResult r) { resolve(std::move(r)); };
            try {
                fn(args, origin, is_cancelled, reply);
            } catch (const std::exception& e) {
                LOG_ERROR("CoreTools", QStringLiteral("%1 threw: %2").arg(tool, QString::fromUtf8(e.what())));
                reply(ToolResult::fail(
                    QStringLiteral("%1 failed inside the terminal: %2").arg(tool, QString::fromUtf8(e.what()))));
            } catch (...) {
                LOG_ERROR("CoreTools", QStringLiteral("%1 threw an unknown exception").arg(tool));
                reply(ToolResult::fail(QStringLiteral("%1 failed inside the terminal.").arg(tool)));
            }
        });
    };
}

ToolDef make_tool(const QString& name, const char* category, const QString& description, ToolSchema schema,
                  int timeout_ms, EngineFn fn) {
    ToolDef t;
    t.name = name;
    t.category = QString::fromLatin1(category);
    t.description = description;
    t.input_schema = std::move(schema);
    // A watchdog, not a job threshold: tools are never backgrounded generically,
    // so ui_act (approvals) and ui_wait simply carry the longest budget they need.
    t.default_timeout_ms = timeout_ms;
    t.async_handler = gui_handler(name, std::move(fn));
    return t;
}

/// A parameter of any JSON type (a string or a number, a string or an object).
void add_untyped(ToolSchema& schema, const QString& name, const QString& description) {
    schema.properties.insert(name, QJsonObject{{QStringLiteral("description"), description}});
}

QJsonObject step_item_schema() {
    auto str = [](const QStringList& values = {}) {
        QJsonObject o{{QStringLiteral("type"), QStringLiteral("string")}};
        if (!values.isEmpty()) {
            QJsonArray e;
            for (const QString& v : values)
                e.append(v);
            o.insert(QStringLiteral("enum"), e);
        }
        return o;
    };
    const QJsonObject integer{{QStringLiteral("type"), QStringLiteral("integer")}};
    const QJsonObject boolean{{QStringLiteral("type"), QStringLiteral("boolean")}};
    QJsonObject props{
        {QStringLiteral("action"),
         str({QStringLiteral("click"), QStringLiteral("double_click"), QStringLiteral("right_click"),
              QStringLiteral("hover"), QStringLiteral("type"), QStringLiteral("clear"), QStringLiteral("select"),
              QStringLiteral("check"), QStringLiteral("uncheck"), QStringLiteral("set_value"), QStringLiteral("press"),
              QStringLiteral("scroll"), QStringLiteral("focus"), QStringLiteral("expand"), QStringLiteral("collapse"),
              QStringLiteral("close")})},
        {QStringLiteral("ref"), str()},
        {QStringLiteral("name"), str()},
        {QStringLiteral("role"), str()},
        {QStringLiteral("target"), str()},
        {QStringLiteral("text"), str()},
        {QStringLiteral("value"), QJsonObject{}},
        {QStringLiteral("append"), boolean},
        {QStringLiteral("submit"), boolean},
        {QStringLiteral("amount"), integer},
        {QStringLiteral("x"), integer},
        {QStringLiteral("y"), integer},
        {QStringLiteral("column"), str()},
    };
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                       {QStringLiteral("properties"), props},
                       {QStringLiteral("required"), QJsonArray{QStringLiteral("action")}}};
}

} // namespace core_tools_detail

using namespace core_tools_detail;

const QStringList& core_tool_names() {
    static const QStringList k = {
        QStringLiteral("ui_overview"),   QStringLiteral("ui_open"),      QStringLiteral("ui_snapshot"),
        QStringLiteral("ui_act"),        QStringLiteral("ui_read"),      QStringLiteral("ui_wait"),
        QStringLiteral("ui_screenshot"), QStringLiteral("ui_workspace"), QStringLiteral("ui_invoke"),
        QStringLiteral("python"),
    };
    return k;
}

std::vector<ToolDef> get_core_tools() {
    std::vector<ToolDef> tools;

    // ── ui_overview ──────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_overview"), kCategoryUiRead,
        QStringLiteral("What the terminal shows now: windows and their panels (screen id, title, ref; focused first), "
                       "open dialogs and menus, sign-in and lock state, work still loading. Call it first."),
        ToolSchemaBuilder().build(), 10000,
        [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_overview(a, std::move(r)); }));

    // ── ui_open ──────────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_open"), kCategoryNavigation,
        QStringLiteral("Open a screen by id or name, optionally on a symbol; omit screen to list all screens. "
                       "placement: full (default — the screen alone, as the nav bar opens it), tab, split, "
                       "right/left/top/bottom of the focused panel, or window (a new window). Answers the panel's ref "
                       "and its controls."),
        ToolSchemaBuilder()
            .string("screen", "Screen id or name, e.g. equity_research, markets, news, portfolio")
            .string("symbol", "Instrument to open it on (screens that take one), e.g. AAPL, RELIANCE.NS, BTC/USDT")
            .string("type", "Asset class of symbol: stock (default) or crypto")
            .enums({QStringLiteral("stock"), QStringLiteral("etf"), QStringLiteral("index"), QStringLiteral("fund"),
                    QStringLiteral("crypto"), QStringLiteral("forex"), QStringLiteral("future"),
                    QStringLiteral("bond")})
            .string("exchange", "Exchange code of symbol, e.g. NSE, NASDAQ")
            .string("placement", "full | tab | split | right | left | top | bottom | window")
            .enums({QStringLiteral("full"), QStringLiteral("only"), QStringLiteral("replace"), QStringLiteral("tab"),
                    QStringLiteral("split"), QStringLiteral("right"), QStringLiteral("left"), QStringLiteral("top"),
                    QStringLiteral("bottom"), QStringLiteral("window")})
            .build(),
        15000,
        [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_open(a, std::move(r)); }));

    // ── ui_snapshot ──────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_snapshot"), kCategoryUiRead,
        QStringLiteral("Part of the screen as elements, one per line; each control has a ref for ui_act / ui_read "
                       "(e12; table rows e12:r5). target: a ref, a screen, \"window\" (default) or \"dialog\". query: "
                       "only elements and table rows holding these words. detail: brief (controls only), normal, full. "
                       "Long panels page with offset."),
        ToolSchemaBuilder()
            .string("target", "Ref, screen id/name, \"window\" or \"dialog\"")
            .string("query", "Words an element must hold")
            .string("detail", "brief | normal | full")
            .enums({QStringLiteral("brief"), QStringLiteral("normal"), QStringLiteral("full")})
            .integer("max_nodes", "Most elements to list (default 150)")
            .between(20, 800)
            .integer("offset", "Skip this many elements (the next page of a busy panel)")
            .min(0)
            .build(),
        15000,
        [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_snapshot(a, std::move(r)); }));

    // ── ui_act ───────────────────────────────────────────────────────────────
    {
        ToolSchema schema =
            ToolSchemaBuilder()
                .string("action", "click | double_click | right_click | hover | type | clear | select | check | "
                                  "uncheck | set_value | press | scroll | focus | expand | collapse | close")
                .enums({QStringLiteral("click"), QStringLiteral("double_click"), QStringLiteral("right_click"),
                        QStringLiteral("hover"), QStringLiteral("type"), QStringLiteral("clear"),
                        QStringLiteral("select"), QStringLiteral("check"), QStringLiteral("uncheck"),
                        QStringLiteral("set_value"), QStringLiteral("press"), QStringLiteral("scroll"),
                        QStringLiteral("focus"), QStringLiteral("expand"), QStringLiteral("collapse"),
                        QStringLiteral("close")})
                .string("ref", "Element ref from ui_snapshot / ui_read (rows e7:r3, web page controls e7:w2)")
                .string("name", "Instead of ref: the element's words (\"Refresh\", \"Symbol\")")
                .string("role", "With name: button, textbox, tab, menuitem, checkbox, combobox …")
                .string("target", "With name: where to look (a panel ref or screen)")
                .string("text", "type: the text; select: the option / tab / row text; press: a key such as Enter, "
                                "Escape, Down, Ctrl+S")
                .boolean("append", "type: add to the text instead of replacing it")
                .boolean("submit", "type: press Enter after")
                .integer("amount", "scroll: notches, + down, - up (0: a page)")
                .between(-100, 100)
                .integer("x", "A point, in pixels, inside the element or the target (panel / dialog / window): acts "
                              "on whatever is there — chart, map, web view, a row")
                .integer("y", "With x")
                .string("column", "Row refs: the cell to click — header words or column number")
                .array("steps", "Up to 10 steps run in order, each {action, ref or name, …} as above",
                       step_item_schema())
                .boolean("snapshot", "Answer with the panel's elements after the last step")
                .build();
        add_untyped(schema, QStringLiteral("value"), QStringLiteral("set_value: a number, or an ISO date / time"));
        tools.push_back(make_tool(
            QStringLiteral("ui_act"), kCategoryUiControl,
            QStringLiteral("Operate the screen like a person, on refs from ui_snapshot or by name (an element's "
                           "words): click, double_click, right_click, hover, type, clear, select, check, uncheck, "
                           "set_value, press, scroll, focus, expand, collapse, close. steps: up to 10 in order, "
                           "stopping at the first failure. Answers what changed (dialogs or menus that opened, with "
                           "refs; values; table row counts; new texts; loading). Orders, money moves, spends, deletes "
                           "and sign-out ask the user first; secret fields are never typed."),
            std::move(schema), 180000,
            [](const QJsonObject& a, const QString& origin, ui::CancelCheck cancelled, ui::Reply r) {
                ui::ui_act(a, origin, std::move(cancelled), std::move(r));
            }));
    }

    // ── ui_read ──────────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_read"), kCategoryUiRead,
        QStringLiteral(
            "An element's data and state, a page at a time (every answer says how to get the next page): a "
            "table's rows with row refs (offset/limit; query filters rows; columns or col_offset picks "
            "columns), one row in full (e12:r5), a text, a dropdown's options, a chart (summary, then points; "
            "query picks a series), a web page (its controls with refs e12:w3, its text, a web chart's candles; "
            "path picks one), a dialog, a panel's state, or a stored result (res_…; path walks into its JSON)."),
        ToolSchemaBuilder()
            .string("ref", "Element ref, panel ref / screen, or res_… id")
            .required()
            .integer("offset", "First row / point / item (text: first character)")
            .min(0)
            .integer("limit", "Per page: rows, points or items (default 20, max 500); text: characters (default 3000)")
            .between(1, 20000)
            .string("query", "Rows holding these words (charts: the series name)")
            .array("columns", "Table columns by header",
                   QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}})
            .integer("col_offset", "First column to show (tables show 10 at a time)")
            .min(0)
            .string("path", "res_…: where in the stored JSON, e.g. data.rows.3; a web page: controls, text or candles")
            .build(),
        15000,
        [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) { ui::ui_read(a, std::move(r)); }));

    // ── ui_wait ──────────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_wait"), kCategoryUiRead,
        QStringLiteral("Wait until something holds instead of guessing a pause: text / gone (words appear in or leave "
                       "target), state (ref becomes visible, hidden, enabled, disabled, checked, unchecked or holds "
                       "value), dialog, no_dialog, idle (loading finished), or time (a pause)."),
        ToolSchemaBuilder()
            .string("until", "text | gone | state | dialog | no_dialog | idle | time")
            .required()
            .enums({QStringLiteral("text"), QStringLiteral("gone"), QStringLiteral("state"), QStringLiteral("dialog"),
                    QStringLiteral("no_dialog"), QStringLiteral("idle"), QStringLiteral("time")})
            .string("text", "text / gone: the words")
            .string("ref", "state: the element")
            .string("state", "visible | hidden | enabled | disabled | checked | unchecked | value")
            .enums({QStringLiteral("visible"), QStringLiteral("hidden"), QStringLiteral("enabled"),
                    QStringLiteral("disabled"), QStringLiteral("checked"), QStringLiteral("unchecked"),
                    QStringLiteral("value")})
            .string("value", "state=value: what it must hold")
            .string("target", "Where to look: a ref or screen (default: the window)")
            .integer("timeout_ms", "Give up after this long (default 10000)")
            .between(0, 60000)
            .build(),
        65000, [](const QJsonObject& a, const QString&, ui::CancelCheck cancelled, ui::Reply r) {
            ui::ui_wait(a, std::move(cancelled), std::move(r));
        }));

    // ── ui_screenshot ────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_screenshot"), kCategoryUiRead,
        QStringLiteral("A picture of the active window, or of a panel, dialog or element by ref. marks=true labels "
                       "each control with its ref."),
        ToolSchemaBuilder()
            .string("target", "Ref, screen, \"window\" (default) or \"dialog\"")
            .string("format", "jpeg (default) or png")
            .enums({QStringLiteral("jpeg"), QStringLiteral("png")})
            .integer("max_width", "Pixels wide at most (default 1280)")
            .between(320, 2560)
            .boolean("marks", "Label each control with its ref")
            .build(),
        15000, [](const QJsonObject& a, const QString&, ui::CancelCheck, ui::Reply r) {
            ui::ui_screenshot(a, std::move(r));
        }));

    // ── ui_workspace ─────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_workspace"), kCategoryNavigation,
        QStringLiteral("Arrange panels and windows, manage saved layouts: list; a panel: focus, close, only, place "
                       "(where, anchor), duplicate, rename (title), move_to_window (to_window); tile; a layout: save, "
                       "load, delete (name); template (persona); new_window, focus_window, close_window (window)."),
        ToolSchemaBuilder()
            .string("action", "What to do")
            .required()
            .enums({QStringLiteral("list"), QStringLiteral("focus"), QStringLiteral("close"), QStringLiteral("only"),
                    QStringLiteral("place"), QStringLiteral("duplicate"), QStringLiteral("rename"),
                    QStringLiteral("move_to_window"), QStringLiteral("tile"), QStringLiteral("save"),
                    QStringLiteral("load"), QStringLiteral("delete"), QStringLiteral("template"),
                    QStringLiteral("new_window"), QStringLiteral("focus_window"), QStringLiteral("close_window")})
            .string("panel", "The panel's ref or screen (default: the focused one)")
            .string("where", "place: right | left | top | bottom | tab")
            .enums({QStringLiteral("right"), QStringLiteral("left"), QStringLiteral("top"), QStringLiteral("bottom"),
                    QStringLiteral("tab")})
            .string("anchor", "place: the panel to place against")
            .string("title", "rename: the new title")
            .string("name", "save / load / delete: the layout's name")
            .string("persona", "template: a persona id from list")
            .string("window", "focus_window / close_window / tile: the window id")
            .string("to_window", "move_to_window: the window id")
            .build(),
        20000, [](const QJsonObject& a, const QString& origin, ui::CancelCheck, ui::Reply r) {
            ui::ui_workspace(a, origin, std::move(r));
        }));

    // ── ui_invoke ────────────────────────────────────────────────────────────
    tools.push_back(make_tool(
        QStringLiteral("ui_invoke"), kCategoryUiControl,
        QStringLiteral("Run one of the terminal's own commands (window, panel, layout, symbol-link, lock …) on a panel "
                       "or window. action=\"list\" lists them with their args."),
        ToolSchemaBuilder()
            .string("action", "The command id (or \"list\")")
            .required()
            .object("args", "The command's arguments")
            .string("target", "The panel's ref / screen, or a window id (default: the active window)")
            .build(),
        90000, [](const QJsonObject& a, const QString& origin, ui::CancelCheck, ui::Reply r) {
            ui::ui_invoke(a, origin, std::move(r));
        }));

    // ── python ───────────────────────────────────────────────────────────────
    tools.push_back(make_python_tool());

    return tools;
}

} // namespace fincept::mcp::core
