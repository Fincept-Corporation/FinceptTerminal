// CoreToolCatalog.cpp — see CoreToolCatalog.h.

#include "mcp/core/CoreToolCatalog.h"

#include "mcp/ToolSchemaBuilder.h"
#include "mcp/core/CoreTools.h"

#include <QJsonArray>
#include <QJsonObject>

namespace fincept::mcp::core {

namespace core_catalog_detail {

QJsonObject typed(const char* type) {
    return QJsonObject{{QStringLiteral("type"), QString::fromLatin1(type)}};
}

/// One step of a ui_act batch: the single-call fields plus the observe-step
/// fields (wait: until/state/timeout_ms; read: offset/limit/query/columns/format).
/// Typed, undescribed: the tool description says what they mean.
QJsonObject step_item_schema() {
    const QJsonObject str = typed("string");
    const QJsonObject integer = typed("integer");
    const QJsonObject boolean = typed("boolean");
    QJsonObject props{
        {QStringLiteral("action"), str},  {QStringLiteral("ref"), str},       {QStringLiteral("name"), str},
        {QStringLiteral("role"), str},    {QStringLiteral("target"), str},    {QStringLiteral("text"), str},
        {QStringLiteral("value"), str},   {QStringLiteral("append"), boolean}, {QStringLiteral("submit"), boolean},
        {QStringLiteral("amount"), integer}, {QStringLiteral("x"), integer},  {QStringLiteral("y"), integer},
        {QStringLiteral("column"), str},  {QStringLiteral("until"), str},     {QStringLiteral("state"), str},
        {QStringLiteral("timeout_ms"), integer}, {QStringLiteral("offset"), integer},
        {QStringLiteral("limit"), integer}, {QStringLiteral("query"), str},   {QStringLiteral("format"), str},
        {QStringLiteral("columns"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                                {QStringLiteral("items"), str}}},
    };
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                       {QStringLiteral("properties"), props},
                       {QStringLiteral("required"), QJsonArray{QStringLiteral("action")}}};
}

QStringList strings(std::initializer_list<const char*> values) {
    QStringList out;
    for (const char* v : values)
        out << QString::fromLatin1(v);
    return out;
}

} // namespace core_catalog_detail

std::vector<UiToolSpec> ui_tool_specs() {
    using namespace core_catalog_detail;
    std::vector<UiToolSpec> out;

    out.push_back({QStringLiteral("ui_overview"), kCategoryUiRead,
                   QStringLiteral("What the terminal shows now: windows and their panels (ref, screen id, title; "
                                  "focused first), open dialogs and menus, sign-in and lock state, work still "
                                  "loading. Call it first."),
                   ToolSchemaBuilder().build(), 10000});

    out.push_back(
        {QStringLiteral("ui_open"), kCategoryNavigation,
         QStringLiteral("Open a screen by id or name, optionally on a symbol; no arguments = the list of screens "
                        "and which kind of task belongs on each. A symbol alone opens that stock in "
                        "equity_research. Answers once the panel has loaded: its ref and controls."),
         ToolSchemaBuilder()
             .string("screen", "Screen id or name, e.g. markets, news, equity_research, portfolio")
             .string("symbol", "e.g. AAPL, RELIANCE.NS, BTC/USDT")
             .string("type", "Asset class of symbol (default stock)")
             .enums(strings({"stock", "etf", "index", "fund", "crypto", "forex", "future", "bond"}))
             .string("exchange", "e.g. NSE, NASDAQ")
             .string("placement", "Default full (the screen alone); window = a new window")
             .enums(strings({"full", "only", "replace", "tab", "split", "right", "left", "top", "bottom", "window"}))
             .build(),
         15000});

    out.push_back(
        {QStringLiteral("ui_snapshot"), kCategoryUiRead,
         QStringLiteral("A panel, dialog or window, one element per line; controls carry refs (e12; rows e12:r5). "
                        "Answers carry \"snap sN\": since=\"sN\" later returns only what changed. query keeps elements "
                        "and rows holding the words; offset pages long panels."),
         ToolSchemaBuilder()
             .string("target", "Ref, screen id/name, \"window\" (default) or \"dialog\"")
             .string("query", "Words an element must hold")
             .string("detail", "brief: controls only")
             .enums(strings({"brief", "normal", "full"}))
             .integer("max_nodes", "Most elements (default 150)")
             .between(20, 800)
             .integer("offset", "Skip this many elements")
             .min(0)
             .string("since", "A snapshot id (sN): only what changed since")
             .build(),
         15000});

    {
        ToolSchema schema =
            ToolSchemaBuilder()
                .string("action")
                .enums(strings({"click", "double_click", "right_click", "hover", "type", "clear", "select", "check",
                                "uncheck", "set_value", "press", "scroll", "focus", "expand", "collapse", "close",
                                "wait", "read"}))
                .string("ref", "e12; row e12:r3; web control e12:w2")
                .string("name", "Instead of ref: the element's words")
                .string("role", "With name: button, textbox, tab, menuitem …")
                .string("target", "Where to look: panel ref or screen")
                .string("text", "type: text; select: option (#N = index); press: key, e.g. Enter, Ctrl+S")
                .string("value", "set_value: number, ISO date/time or option")
                .boolean("append", "type: add to the text")
                .boolean("submit", "type: then press Enter")
                .integer("amount", "scroll notches, + down (0 = a page)")
                .between(-100, 100)
                .integer("x", "Point in pixels within ref / target")
                .integer("y")
                .string("column", "Row ref: the cell (header or number)")
                .array("steps", "Up to 12, each {action, …as above}; also {action:\"wait\", until, text, ref, state, "
                                "timeout_ms} and {action:\"read\", ref, offset, limit, query, columns, format}",
                       step_item_schema())
                .boolean("snapshot", "Also answer with the panel's elements")
                .build();
        out.push_back(
            {QStringLiteral("ui_act"), kCategoryUiControl,
             QStringLiteral("Operate the terminal like a person, by ref or name: click, type, select, check, "
                            "set_value, press keys, scroll, expand, close … A table cell: ref e12:r3 + column. steps: "
                            "a batch in one call, stopping at the first failure — e.g. set fields, click Run, "
                            "{action:\"wait\",until:\"idle\"}, {action:\"read\",ref:…}. Answers what changed. Orders, "
                            "money moves, spends, deletes and sign-out ask the user; one order step per call; secrets "
                            "are never typed. Errors start with a code (stale_ref, not_found, ambiguous, disabled, "
                            "obscured, timeout, needs_approval, denied, invalid_value …)."),
             std::move(schema), 180000});
    }

    out.push_back(
        {QStringLiteral("ui_read"), kCategoryUiRead,
         QStringLiteral("An element's data, a page at a time (answers say how to continue): table rows with row refs "
                        "— query: words or Header:words; columns / col_offset; format=\"json\": header + typed rows, "
                        "units once — one row (e12:r5), text, dropdown options, chart series, a web page (controls "
                        "e12:w3, text, path=candles), a dialog, a panel's state, or a stored result res_… (path walks "
                        "its JSON)."),
         ToolSchemaBuilder()
             .string("ref", "Element / panel ref, screen, or res_… id")
             .required()
             .integer("offset", "First row / item (text: character)")
             .min(0)
             .integer("limit", "Per page: rows (default 20, max 500); text: characters")
             .between(1, 20000)
             .string("query", "Words; Header:words for one column; a chart's series")
             .array("columns", "Columns by header", typed("string"))
             .integer("col_offset", "First column (text pages show 10)")
             .min(0)
             .string("format", "json: typed rows")
             .enums(strings({"text", "json"}))
             .string("path", "res_…: e.g. data.rows.3; web page: controls, text, candles")
             .build(),
         15000});

    out.push_back(
        {QStringLiteral("ui_wait"), kCategoryUiRead,
         QStringLiteral("Wait until something holds instead of guessing a pause: text / gone (words appear in or "
                        "leave target), state (ref reaches state, or holds value), dialog, no_dialog, idle (loading "
                        "finished), or time (a pause)."),
         ToolSchemaBuilder()
             .string("until")
             .required()
             .enums(strings({"text", "gone", "state", "dialog", "no_dialog", "idle", "time"}))
             .string("text", "text / gone: the words")
             .string("ref", "state: the element")
             .string("state")
             .enums(strings({"visible", "hidden", "enabled", "disabled", "checked", "unchecked", "value"}))
             .string("value", "state=value: what it must hold")
             .string("target", "Where to look: a ref or screen (default: the window)")
             .integer("timeout_ms", "Default 10000")
             .between(0, 60000)
             .build(),
         65000});

    out.push_back({QStringLiteral("ui_screenshot"), kCategoryUiRead,
                   QStringLiteral("A picture of the active window, or of a panel, dialog or element by ref; secret "
                                  "fields are blacked out. marks=true labels each control with its ref."),
                   ToolSchemaBuilder()
                       .string("target", "Ref, screen, \"window\" (default) or \"dialog\"")
                       .string("format", "Default jpeg")
                       .enums(strings({"jpeg", "png"}))
                       .integer("max_width", "Default 1280")
                       .between(320, 2560)
                       .boolean("marks", "Label each control with its ref")
                       .build(),
                   15000});

    out.push_back(
        {QStringLiteral("ui_workspace"), kCategoryNavigation,
         QStringLiteral("Arrange panels and windows, manage layouts: list; a panel: focus, close, only, place (where, "
                        "anchor), duplicate, rename (title), move_to_window (to_window); tile; a layout: save, load, "
                        "delete (name); template (persona); new_window, focus_window, close_window (window)."),
         ToolSchemaBuilder()
             .string("action")
             .required()
             .enums(strings({"list", "focus", "close", "only", "place", "duplicate", "rename", "move_to_window", "tile",
                             "save", "load", "delete", "template", "new_window", "focus_window", "close_window"}))
             .string("panel", "Panel ref or screen (default: the focused one)")
             .string("where")
             .enums(strings({"right", "left", "top", "bottom", "tab"}))
             .string("anchor", "place: the panel to place against")
             .string("title")
             .string("name", "Layout name")
             .string("persona", "A persona id from list")
             .string("window", "Window id")
             .string("to_window", "Window id")
             .build(),
         20000});

    out.push_back({QStringLiteral("ui_invoke"), kCategoryUiControl,
                   QStringLiteral("Run one of the terminal's own commands (window, panel, layout, symbol-link, lock …) "
                                  "on a panel or window. action=\"list\" lists them with their args."),
                   ToolSchemaBuilder()
                       .string("action", "The command id (or \"list\")")
                       .required()
                       .object("args", "The command's arguments")
                       .string("target", "The panel's ref / screen, or a window id (default: the active window)")
                       .build(),
                   90000});
    return out;
}

} // namespace fincept::mcp::core
