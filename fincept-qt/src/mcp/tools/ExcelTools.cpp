// ExcelTools.cpp — Tools for the Excel/Spreadsheet screen.
//
// 18 tools in category "excel":
//   • Sheet ops (6)  — list, get/set active, add, delete, rename
//   • Cell ops  (3)  — get / set / clear
//   • Sheet data(4)  — get full / set full / get dimensions / recalc
//   • Row/col   (4)  — insert row, insert col, delete rows, delete cols
//   • Export    (1)  — CSV write to disk
//
// xlsx import/export is bound to file-picker dialogs and QXlsx integration —
// out of scope for headless tool use. CSV export is exposed because it's
// fully programmatic via QFile.

#include "mcp/tools/ExcelTools.h"

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/tools/ExportPathGuard.h"
#include "screens/excel/ExcelScreen.h"
#include "screens/excel/SpreadsheetWidget.h"
#include "services/file_manager/FileManagerService.h"

#include <algorithm>

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTextStream>

namespace fincept::mcp::tools {

namespace {
static constexpr const char* TAG = "ExcelTools";
static constexpr int kDefaultTimeoutMs = 15000;

// Bulk sheet reads/writes run on the UI thread and repopulate every cell, which
// on a full model is far more work than a single-cell edit. 15 s was not enough:
// set_excel_sheet_data and get_excel_sheet_data were both killed mid-flight,
// leaving a half-written sheet — which is where the stray zeros came from, since
// formulas pointing at cells that never got written evaluate to 0.
static constexpr int kBulkTimeoutMs = 90000;

screens::ExcelScreen* find_excel_screen() {
    for (auto* w : WindowRegistry::instance().frames()) {
        if (!w || !w->dock_router())
            continue;
        auto* dw = w->dock_router()->find_dock_widget("excel");
        if (!dw)
            continue;
        auto* widget = dw->widget();
        if (!widget)
            continue;
        if (auto* es = qobject_cast<screens::ExcelScreen*>(widget))
            return es;
        if (auto* es = widget->findChild<screens::ExcelScreen*>())
            return es;
    }
    return nullptr;
}

// Excel screen, building it on demand.
//
// Screens are lazily constructed (P2), so the Excel screen does not exist until
// the user visits that tab. Every tool here resolved through find_excel_screen()
// and hard-failed with "Excel screen not open" when it wasn't — which meant an
// agent driving Excel from the AI Chat tab could fetch SEC data, plan a full DCF
// model, create the sheet, and then lose 45 consecutive set_excel_cell calls to
// a screen that had simply never been built. Asking the user to go open a tab
// first is not a workable contract for a tool the model calls on its own.
//
// Tools run on the UI thread (run_on_ui), so constructing widgets here is safe.
// Two cases: the dock exists but holds a placeholder (materialize_now builds the
// screen in place, no visible change), or there is no dock at all (navigate
// creates and shows it — the user asked for work in Excel, so surfacing it is
// the expected outcome, and it is the only path that registers the dock).
screens::ExcelScreen* ensure_excel_screen() {
    if (auto* es = find_excel_screen())
        return es;

    // Never restructure the dock layout while a modal dialog is open. Tool
    // bodies are queued onto the UI thread, so they also run inside the nested
    // event loop of a file dialog — navigating there would rebuild docks under
    // a screen that is mid-export and holding pointers into them.
    if (QApplication::activeModalWidget()) {
        LOG_WARN(TAG, "Excel screen missing but a modal dialog is open — refusing to navigate");
        return nullptr;
    }

    for (auto* w : WindowRegistry::instance().frames()) {
        auto* router = w ? w->dock_router() : nullptr;
        if (!router)
            continue;
        if (router->find_dock_widget("excel"))
            router->materialize_now("excel");
        else
            router->navigate("excel");
        if (auto* es = find_excel_screen()) {
            LOG_INFO(TAG, "Excel screen was not open — opened it for a tool call");
            return es;
        }
    }
    LOG_WARN(TAG, "Could not open the Excel screen for a tool call");
    return nullptr;
}

QTabWidget* find_excel_tabs() {
    auto* es = ensure_excel_screen();
    return es ? es->findChild<QTabWidget*>() : nullptr;
}

screens::SpreadsheetWidget* sheet_by_index(int idx) {
    auto* tabs = find_excel_tabs();
    if (!tabs || idx < 0 || idx >= tabs->count())
        return nullptr;
    return qobject_cast<screens::SpreadsheetWidget*>(tabs->widget(idx));
}

screens::SpreadsheetWidget* active_sheet() {
    auto* tabs = find_excel_tabs();
    return tabs ? sheet_by_index(tabs->currentIndex()) : nullptr;
}

screens::SpreadsheetWidget* sheet_by_name(const QString& name) {
    auto* tabs = find_excel_tabs();
    if (!tabs || name.isEmpty())
        return nullptr;
    for (int i = 0; i < tabs->count(); ++i) {
        auto* s = qobject_cast<screens::SpreadsheetWidget*>(tabs->widget(i));
        if (s && s->sheet_name().compare(name, Qt::CaseInsensitive) == 0)
            return s;
    }
    return nullptr;
}

// ── Addressing aliases ──────────────────────────────────────────────────────
//
// These tools address a live Excel *screen*: sheets by integer index, cells by
// zero-based row/col. Every Excel API a model has ever seen — openpyxl,
// xlsxwriter, the Sheets API, VBA — addresses workbooks by file, sheets by NAME
// and cells by A1 reference. So models confidently send
// {workbook_name, tab_name, cell_ref:"B87"}, which matched nothing and failed
// every call; one turn made 15 such calls, had all 15 rejected, and reported a
// finished model anyway.
//
// Rather than expect every model to learn an unusual convention, accept the
// conventional one too. Index/row/col remain canonical; these are aliases.

// "B87" / "$B$87" → row 86, col 1 (both zero-based). False if not A1 notation.
bool parse_a1_ref(const QString& ref, int& row, int& col) {
    static const QRegularExpression re(QStringLiteral("^\\$?([A-Za-z]{1,3})\\$?([0-9]{1,7})$"));
    const auto m = re.match(ref.trimmed());
    if (!m.hasMatch())
        return false;

    const QString letters = m.captured(1).toUpper();
    int c = 0;
    for (const QChar& ch : letters)
        c = c * 26 + (ch.unicode() - 'A' + 1);

    const int r = m.captured(2).toInt();
    if (c <= 0 || r <= 0)
        return false;
    col = c - 1;
    row = r - 1;
    return true;
}

/// Sheet from sheet_index, or the sheet_name/tab_name alias, or the active one.
screens::SpreadsheetWidget* resolve_sheet(const QJsonObject& args) {
    for (const char* key : {"sheet_name", "tab_name"}) {
        const QString name = args.value(QLatin1String(key)).toString().trimmed();
        if (!name.isEmpty()) {
            if (auto* s = sheet_by_name(name))
                return s;
            return nullptr; // named a sheet that doesn't exist — don't silently
                            // fall through to the active one and edit the wrong sheet
        }
    }
    if (args.contains("sheet_index") && !args["sheet_index"].isNull()) {
        const int i = args["sheet_index"].toInt(-1);
        if (i >= 0)
            return sheet_by_index(i);
    }
    return active_sheet();
}

/// Row/col from the explicit integers, or from a cell/cell_ref A1 alias.
/// Returns false when neither was supplied (or the A1 string is malformed).
bool resolve_row_col(const QJsonObject& args, int& row, int& col) {
    if (args.contains("row") && args.contains("col")) {
        row = args["row"].toInt(-1);
        col = args["col"].toInt(-1);
        return row >= 0 && col >= 0;
    }
    for (const char* key : {"cell", "cell_ref", "cell_reference", "ref"}) {
        const QString ref = args.value(QLatin1String(key)).toString();
        if (!ref.isEmpty())
            return parse_a1_ref(ref, row, col);
    }
    return false;
}

/// Declare the alias params so validation accepts them (unknown args are now
/// rejected) and tool_describe advertises them.
ToolSchema with_sheet_aliases(ToolSchema s) {
    ToolParam name_p;
    name_p.type = "string";
    name_p.description = "Sheet name, as an alternative to sheet_index";
    s.params["sheet_name"] = name_p;
    s.params["tab_name"] = name_p; // common synonym

    ToolParam wb;
    wb.type = "string";
    wb.description = "Ignored - there is one live workbook (the Excel screen). Accepted for compatibility.";
    s.params["workbook_name"] = wb;
    return s;
}

ToolSchema with_cell_aliases(ToolSchema s) {
    s = with_sheet_aliases(std::move(s));
    ToolParam cell;
    cell.type = "string";
    cell.description = "Cell in A1 notation (e.g. \"B87\"), as an alternative to row + col";
    s.params["cell"] = cell;
    s.params["cell_ref"] = cell;
    // row/col stay declared but stop being mandatory — either addressing form is
    // valid, so the handler enforces "one of the two" instead.
    if (s.params.contains("row"))
        s.params["row"].required = false;
    if (s.params.contains("col"))
        s.params["col"].required = false;
    s.required.removeAll(QStringLiteral("row"));
    s.required.removeAll(QStringLiteral("col"));
    return s;
}

constexpr const char* kAddressingError =
    "Specify the cell either as row + col (zero-based integers) or as cell=\"B87\" (A1 notation)";

template <typename BodyFn>
void run_on_ui(ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise, BodyFn&& body) {
    AsyncDispatch::callback_to_promise(qApp, std::move(ctx), promise, std::forward<BodyFn>(body));
}
} // namespace

std::vector<ToolDef> get_excel_tools() {
    std::vector<ToolDef> tools;

    // 1. list_excel_sheets
    {
        ToolDef t;
        t.name = "list_excel_sheets";
        t.description = "List all sheets (index, name, rows, cols, is_active).";
        t.category = "excel";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) {
                    resolve(ToolResult::fail("Excel screen could not be opened"));
                    return;
                }
                QJsonArray arr;
                for (int i = 0; i < tabs->count(); ++i) {
                    auto* s = qobject_cast<screens::SpreadsheetWidget*>(tabs->widget(i));
                    if (!s)
                        continue;
                    arr.append(QJsonObject{
                        {"index", i},
                        {"name", s->sheet_name()},
                        {"rows", s->row_count()},
                        {"cols", s->col_count()},
                        {"is_active", i == tabs->currentIndex()},
                    });
                }
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    // 2. get_excel_active_sheet
    {
        ToolDef t;
        t.name = "get_excel_active_sheet";
        t.description = "Get the currently-active sheet (index + name + dimensions).";
        t.category = "excel";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) {
                    resolve(ToolResult::fail("Excel screen could not be opened"));
                    return;
                }
                auto* s = active_sheet();
                if (!s) {
                    resolve(ToolResult::fail("No active sheet"));
                    return;
                }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"index", tabs->currentIndex()},
                    {"name", s->sheet_name()},
                    {"rows", s->row_count()},
                    {"cols", s->col_count()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // 3. set_excel_active_sheet
    {
        ToolDef t;
        t.name = "set_excel_active_sheet";
        t.description = "Switch to a sheet by index.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder().integer("sheet_index", "Sheet index").required().min(0).build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) {
                    resolve(ToolResult::fail("Excel screen could not be opened"));
                    return;
                }
                const int i = args["sheet_index"].toInt();
                if (i < 0 || i >= tabs->count()) {
                    resolve(ToolResult::fail("Index out of range"));
                    return;
                }
                tabs->setCurrentIndex(i);
                resolve(ToolResult::ok("Sheet activated", QJsonObject{{"index", i}}));
            });
        };
        tools.push_back(std::move(t));
    }

    // 4. add_excel_sheet
    {
        ToolDef t;
        t.name = "add_excel_sheet";
        t.description =
            "Add a new blank sheet with an optional name (default: Sheet<N>). Returns the new sheet's index.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .string("name", "Sheet name (empty = auto-generated)")
                             .default_str("")
                             .length(0, 64)
                             .integer("rows", "Initial row count")
                             .default_int(100)
                             .between(1, 10000)
                             .integer("cols", "Initial col count")
                             .default_int(26)
                             .between(1, 256)
                             .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) {
                    resolve(ToolResult::fail("Excel screen could not be opened"));
                    return;
                }
                QString name = args["name"].toString();
                if (name.isEmpty())
                    name = QString("Sheet%1").arg(tabs->count() + 1);
                auto* s = new screens::SpreadsheetWidget(name, args["rows"].toInt(100), args["cols"].toInt(26));
                const int idx = tabs->addTab(s, name);
                tabs->setCurrentIndex(idx);
                resolve(ToolResult::ok("Sheet added", QJsonObject{{"index", idx}, {"name", name}}));
            });
        };
        tools.push_back(std::move(t));
    }

    // 5. delete_excel_sheet
    {
        ToolDef t;
        t.name = "delete_excel_sheet";
        t.description = "Delete a sheet by index. Cannot delete the last remaining sheet.";
        t.category = "excel";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder().integer("sheet_index", "Sheet index to delete").required().min(0).build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) {
                    resolve(ToolResult::fail("Excel screen could not be opened"));
                    return;
                }
                const int i = args["sheet_index"].toInt();
                if (i < 0 || i >= tabs->count()) {
                    resolve(ToolResult::fail("Index out of range"));
                    return;
                }
                if (tabs->count() <= 1) {
                    resolve(ToolResult::fail("Cannot delete the last sheet"));
                    return;
                }
                auto* w = tabs->widget(i);
                tabs->removeTab(i);
                if (w)
                    w->deleteLater();
                resolve(ToolResult::ok("Sheet deleted", QJsonObject{{"index", i}}));
            });
        };
        tools.push_back(std::move(t));
    }

    // 6. rename_excel_sheet
    {
        ToolDef t;
        t.name = "rename_excel_sheet";
        t.description = "Rename a sheet (tab label + internal name).";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index")
                             .required()
                             .min(0)
                             .string("name", "New name")
                             .required()
                             .length(1, 64)
                             .build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) {
                    resolve(ToolResult::fail("Excel screen could not be opened"));
                    return;
                }
                const int i = args["sheet_index"].toInt();
                auto* s = sheet_by_index(i);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                const QString name = args["name"].toString();
                s->set_sheet_name(name);
                tabs->setTabText(i, name);
                resolve(ToolResult::ok("Sheet renamed", QJsonObject{{"index", i}, {"name", name}}));
            });
        };
        tools.push_back(std::move(t));
    }

    // 7. get_excel_cell
    {
        ToolDef t;
        t.name = "get_excel_cell";
        t.description = "Read a cell's raw text (may be a formula starting with '='). Address the cell either as "
                        "row + col (zero-based) or as cell=\"B87\" (A1 notation); the sheet as sheet_index or "
                        "sheet_name.";
        t.category = "excel";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index (-1 = active)")
                             .default_int(-1)
                             .min(-1)
                             .integer("row", "Zero-indexed row")
                             .required()
                             .min(0)
                             .integer("col", "Zero-indexed column")
                             .required()
                             .min(0)
                             .build();
        t.input_schema = with_cell_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheets"));
                    return;
                }
                int r = -1, c = -1;
                if (!resolve_row_col(args, r, c)) {
                    resolve(ToolResult::fail(kAddressingError));
                    return;
                }
                if (r >= s->row_count() || c >= s->col_count()) {
                    resolve(ToolResult::fail("Cell out of range"));
                    return;
                }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"row", r},
                    {"col", c},
                    {"text", s->cell_text(r, c)},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // 8. set_excel_cell
    {
        ToolDef t;
        t.name = "set_excel_cell";
        t.description =
            "Write a cell's text. Strings starting with '=' are evaluated as formulas (e.g. '=A1+B1', "
            "'=SUM(A1:A10)'). Address the cell either as row + col (zero-based) or as cell=\"B87\" (A1 notation); "
            "the sheet as sheet_index or sheet_name.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index (-1 = active)")
                             .default_int(-1)
                             .min(-1)
                             .integer("row", "Zero-indexed row")
                             .required()
                             .min(0)
                             .integer("col", "Zero-indexed column")
                             .required()
                             .min(0)
                             .string("text", "Cell text (or formula)")
                             .required()
                             .length(0, 4096)
                             .build();
        t.input_schema = with_cell_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheets"));
                    return;
                }
                int r = -1, c = -1;
                if (!resolve_row_col(args, r, c)) {
                    resolve(ToolResult::fail(kAddressingError));
                    return;
                }
                s->set_cell(r, c, args["text"].toString());
                resolve(ToolResult::ok("Cell updated"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 9. clear_excel_cell
    {
        ToolDef t;
        t.name = "clear_excel_cell";
        t.description = "Clear a cell (set to empty). Address it as row + col (zero-based) or cell=\"B87\".";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index (-1 = active)")
                             .default_int(-1)
                             .min(-1)
                             .integer("row", "Zero-indexed row")
                             .required()
                             .min(0)
                             .integer("col", "Zero-indexed column")
                             .required()
                             .min(0)
                             .build();
        t.input_schema = with_cell_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheets"));
                    return;
                }
                int r = -1, c = -1;
                if (!resolve_row_col(args, r, c)) {
                    resolve(ToolResult::fail(kAddressingError));
                    return;
                }
                s->set_cell(r, c, QString());
                resolve(ToolResult::ok("Cell cleared"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 10. get_excel_sheet_data
    {
        ToolDef t;
        t.name = "get_excel_sheet_data";
        t.description = "Get the entire sheet as a 2D array of cell text (rows of strings).";
        t.category = "excel";
        t.default_timeout_ms = kBulkTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder().integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1).build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                const auto data = s->get_data();
                QJsonArray rows;
                for (const auto& row : data) {
                    QJsonArray r;
                    for (const auto& cell : row)
                        r.append(cell);
                    rows.append(r);
                }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"rows", rows},
                    {"row_count", s->row_count()},
                    {"col_count", s->col_count()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // 11. set_excel_sheet_data
    {
        ToolDef t;
        t.name = "set_excel_sheet_data";
        t.description = "Replace the entire sheet with a 2D array of cell text.";
        t.category = "excel";
        // is_destructive only — NOT ExplicitConfirm. The confirm modal that
        // level waits on was never built, so the checker in AgentService denies
        // everything >= Verified outright and this tool could never run: the
        // chat path got "requires explicit_confirm auth" on every call. Worse,
        // it was inconsistent — set_excel_cell, add_excel_sheet, insert_excel_row
        // and delete_excel_rows are all destructive without the gate, so writing
        // 45 cells one at a time was permitted while writing them in one call
        // was not. is_destructive still blocks the agent path (agents can't
        // prompt); the chat path, where the user is present and watching, is
        // allowed. delete_excel_sheet keeps its gate — it destroys existing work.
        t.is_destructive = true;
        t.default_timeout_ms = kBulkTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index (-1 = active)")
                             .default_int(-1)
                             .min(-1)
                             .array("rows", "2D array of strings (array of row arrays)", QJsonObject{{"type", "array"}})
                             .build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                QVector<QVector<QString>> data;
                for (const auto& row_v : args["rows"].toArray()) {
                    QVector<QString> row;
                    for (const auto& cell : row_v.toArray())
                        row.append(cell.toString());
                    data.append(row);
                }
                s->set_data(data);
                resolve(ToolResult::ok("Sheet data replaced", QJsonObject{{"rows", static_cast<int>(data.size())}}));
            });
        };
        tools.push_back(std::move(t));
    }

    // 12. get_excel_sheet_dimensions
    {
        ToolDef t;
        t.name = "get_excel_sheet_dimensions";
        t.description = "Get row/col counts for a sheet.";
        t.category = "excel";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder().integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1).build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"name", s->sheet_name()},
                    {"rows", s->row_count()},
                    {"cols", s->col_count()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // 13. recalculate_excel_sheet
    {
        ToolDef t;
        t.name = "recalculate_excel_sheet";
        t.description = "Force recalculation of all formula cells in a sheet.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kBulkTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder().integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1).build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                s->recalculate();
                resolve(ToolResult::ok("Recalculated"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 14. insert_excel_row
    {
        ToolDef t;
        t.name = "insert_excel_row";
        t.description = "Insert a row above or below the currently-selected row.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index (-1 = active)")
                             .default_int(-1)
                             .min(-1)
                             .string("position", "above | below")
                             .default_str("below")
                             .enums({"above", "below"})
                             .build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                if (args["position"].toString("below") == "above")
                    s->insert_row_above();
                else
                    s->insert_row_below();
                resolve(ToolResult::ok("Row inserted"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 15. insert_excel_col
    {
        ToolDef t;
        t.name = "insert_excel_col";
        t.description = "Insert a column to the left or right of the currently-selected column.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index (-1 = active)")
                             .default_int(-1)
                             .min(-1)
                             .string("position", "left | right")
                             .default_str("right")
                             .enums({"left", "right"})
                             .build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                if (args["position"].toString("right") == "left")
                    s->insert_col_left();
                else
                    s->insert_col_right();
                resolve(ToolResult::ok("Column inserted"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 16. delete_excel_rows
    {
        ToolDef t;
        t.name = "delete_excel_rows";
        t.description = "Delete the currently-selected rows.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder().integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1).build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                s->delete_selected_rows();
                resolve(ToolResult::ok("Rows deleted"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 17. delete_excel_cols
    {
        ToolDef t;
        t.name = "delete_excel_cols";
        t.description = "Delete the currently-selected columns.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder().integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1).build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                s->delete_selected_cols();
                resolve(ToolResult::ok("Columns deleted"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 18. export_excel_sheet_csv
    {
        ToolDef t;
        t.name = "export_excel_sheet_csv";
        t.description = "Write a sheet to a CSV file inside the export directory (programmatic, no file dialog). "
                        "'path' is a filename or relative path inside that directory; absolute paths elsewhere "
                        "on disk are refused.";
        t.category = "excel";
        // See set_excel_sheet_data above for why ExplicitConfirm is not used.
        // This is the ONLY tool that persists a sheet to disk, so leaving it
        // gated meant nothing the model built could ever leave memory — a sheet
        // it had just filled in was unreachable from File Manager.
        //
        // That argument covers the CONFIRMATION gate, not the PATH. `path` was
        // still raw model text handed straight to QFile::open(WriteOnly), i.e.
        // arbitrary write anywhere the process can reach. resolve_export_path()
        // confines it without re-gating the tool.
        t.is_destructive = true;
        t.default_timeout_ms = kBulkTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .integer("sheet_index", "Sheet index (-1 = active)")
                             .default_int(-1)
                             .min(-1)
                             .string("path", "Output CSV filename inside the export directory, e.g. 'model.csv'")
                             .required()
                             .length(1, 1024)
                             .build();
        t.input_schema = with_sheet_aliases(std::move(t.input_schema));
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) {
                    resolve(ToolResult::fail("Sheet not found - call list_excel_sheets for valid sheet_index values"));
                    return;
                }
                const auto dest = resolve_export_path(args["path"].toString());
                if (!dest.ok()) {
                    resolve(ToolResult::fail(dest.error));
                    return;
                }
                QFile f(dest.path);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    resolve(ToolResult::fail("Cannot open path for writing"));
                    return;
                }
                const auto data = s->get_data();

                // Trim to the used extent. get_data() pads every sheet to at
                // least 100x26, so a 12-row model was exporting as 100 rows of
                // bare commas.
                int last_row = -1;
                int last_col = -1;
                for (int r = 0; r < data.size(); ++r) {
                    for (int c = 0; c < data[r].size(); ++c) {
                        if (!data[r][c].trimmed().isEmpty()) {
                            last_row = std::max(last_row, r);
                            last_col = std::max(last_col, c);
                        }
                    }
                }

                {
                    QTextStream out(&f);
                    out.setEncoding(QStringConverter::Utf8);
                    for (int r = 0; r <= last_row; ++r) {
                        QStringList parts;
                        for (int c = 0; c <= last_col; ++c) {
                            QString cell = (c < data[r].size()) ? data[r][c] : QString();
                            if (cell.contains(',') || cell.contains('"') || cell.contains('\n')) {
                                cell.replace('"', "\"\"");
                                cell = '"' + cell + '"';
                            }
                            parts.append(cell);
                        }
                        out << parts.join(',') << '\n';
                    }
                    out.flush();
                }
                f.close();

                // Register with File Manager, exactly as the UI export buttons
                // do. Without this a model-driven export landed on disk but
                // never appeared in the Files tab — the file existed and was
                // simply invisible, which reads as "the export silently failed".
                const QString out_path = dest.path;
                const QString file_id = services::FileManagerService::instance().import_file(out_path, "excel");

                resolve(ToolResult::ok("CSV exported", QJsonObject{
                                                           {"path", out_path},
                                                           {"rows", last_row + 1},
                                                           {"cols", last_col + 1},
                                                           {"file_manager_id", file_id},
                                                       }));
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 excel tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
