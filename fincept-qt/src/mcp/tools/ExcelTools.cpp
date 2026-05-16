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
#include "screens/excel/ExcelScreen.h"
#include "screens/excel/SpreadsheetWidget.h"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTabWidget>
#include <QTextStream>

namespace fincept::mcp::tools {

namespace {
static constexpr const char* TAG = "ExcelTools";
static constexpr int kDefaultTimeoutMs = 15000;

screens::ExcelScreen* find_excel_screen() {
    for (auto* w : WindowRegistry::instance().frames()) {
        if (!w || !w->dock_router()) continue;
        auto* dw = w->dock_router()->find_dock_widget("excel");
        if (!dw) continue;
        auto* widget = dw->widget();
        if (!widget) continue;
        if (auto* es = qobject_cast<screens::ExcelScreen*>(widget)) return es;
        if (auto* es = widget->findChild<screens::ExcelScreen*>()) return es;
    }
    return nullptr;
}

QTabWidget* find_excel_tabs() {
    auto* es = find_excel_screen();
    return es ? es->findChild<QTabWidget*>() : nullptr;
}

screens::SpreadsheetWidget* sheet_by_index(int idx) {
    auto* tabs = find_excel_tabs();
    if (!tabs || idx < 0 || idx >= tabs->count()) return nullptr;
    return qobject_cast<screens::SpreadsheetWidget*>(tabs->widget(idx));
}

screens::SpreadsheetWidget* active_sheet() {
    auto* tabs = find_excel_tabs();
    return tabs ? sheet_by_index(tabs->currentIndex()) : nullptr;
}

screens::SpreadsheetWidget* resolve_sheet(const QJsonObject& args) {
    if (args.contains("sheet_index") && !args["sheet_index"].isNull()) {
        const int i = args["sheet_index"].toInt(-1);
        if (i >= 0) return sheet_by_index(i);
    }
    return active_sheet();
}

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
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) { resolve(ToolResult::fail("Excel screen not open")); return; }
                QJsonArray arr;
                for (int i = 0; i < tabs->count(); ++i) {
                    auto* s = qobject_cast<screens::SpreadsheetWidget*>(tabs->widget(i));
                    if (!s) continue;
                    arr.append(QJsonObject{
                        {"index", i}, {"name", s->sheet_name()},
                        {"rows", s->row_count()}, {"cols", s->col_count()},
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
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) { resolve(ToolResult::fail("Excel screen not open")); return; }
                auto* s = active_sheet();
                if (!s) { resolve(ToolResult::fail("No active sheet")); return; }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"index", tabs->currentIndex()}, {"name", s->sheet_name()},
                    {"rows", s->row_count()}, {"cols", s->col_count()},
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
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) { resolve(ToolResult::fail("Excel screen not open")); return; }
                const int i = args["sheet_index"].toInt();
                if (i < 0 || i >= tabs->count()) { resolve(ToolResult::fail("Index out of range")); return; }
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
        t.description = "Add a new blank sheet with an optional name (default: Sheet<N>). Returns the new sheet's index.";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("name", "Sheet name (empty = auto-generated)").default_str("").length(0, 64)
            .integer("rows", "Initial row count").default_int(100).between(1, 10000)
            .integer("cols", "Initial col count").default_int(26).between(1, 256)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) { resolve(ToolResult::fail("Excel screen not open")); return; }
                QString name = args["name"].toString();
                if (name.isEmpty()) name = QString("Sheet%1").arg(tabs->count() + 1);
                auto* s = new screens::SpreadsheetWidget(name,
                                                         args["rows"].toInt(100), args["cols"].toInt(26));
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
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index to delete").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) { resolve(ToolResult::fail("Excel screen not open")); return; }
                const int i = args["sheet_index"].toInt();
                if (i < 0 || i >= tabs->count()) { resolve(ToolResult::fail("Index out of range")); return; }
                if (tabs->count() <= 1) { resolve(ToolResult::fail("Cannot delete the last sheet")); return; }
                auto* w = tabs->widget(i);
                tabs->removeTab(i);
                if (w) w->deleteLater();
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
            .integer("sheet_index", "Sheet index").required().min(0)
            .string("name", "New name").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* tabs = find_excel_tabs();
                if (!tabs) { resolve(ToolResult::fail("Excel screen not open")); return; }
                const int i = args["sheet_index"].toInt();
                auto* s = sheet_by_index(i);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
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
        t.description = "Read a cell's raw text (may be a formula starting with '=').";
        t.category = "excel";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .integer("row", "Zero-indexed row").required().min(0)
            .integer("col", "Zero-indexed column").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                const int r = args["row"].toInt(), c = args["col"].toInt();
                if (r >= s->row_count() || c >= s->col_count()) {
                    resolve(ToolResult::fail("Cell out of range")); return;
                }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"row", r}, {"col", c}, {"text", s->cell_text(r, c)},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // 8. set_excel_cell
    {
        ToolDef t;
        t.name = "set_excel_cell";
        t.description = "Write a cell's text. Strings starting with '=' are evaluated as formulas (e.g. '=A1+B1', '=SUM(A1:A10)').";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .integer("row", "Zero-indexed row").required().min(0)
            .integer("col", "Zero-indexed column").required().min(0)
            .string("text", "Cell text (or formula)").required().length(0, 4096)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                s->set_cell(args["row"].toInt(), args["col"].toInt(), args["text"].toString());
                resolve(ToolResult::ok("Cell updated"));
            });
        };
        tools.push_back(std::move(t));
    }

    // 9. clear_excel_cell
    {
        ToolDef t;
        t.name = "clear_excel_cell";
        t.description = "Clear a cell (set to empty).";
        t.category = "excel";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .integer("row", "Zero-indexed row").required().min(0)
            .integer("col", "Zero-indexed column").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                s->set_cell(args["row"].toInt(), args["col"].toInt(), QString());
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
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                const auto data = s->get_data();
                QJsonArray rows;
                for (const auto& row : data) {
                    QJsonArray r;
                    for (const auto& cell : row) r.append(cell);
                    rows.append(r);
                }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"rows", rows}, {"row_count", s->row_count()}, {"col_count", s->col_count()},
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
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .array("rows", "2D array of strings (array of row arrays)",
                   QJsonObject{{"type", "array"}})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                QVector<QVector<QString>> data;
                for (const auto& row_v : args["rows"].toArray()) {
                    QVector<QString> row;
                    for (const auto& cell : row_v.toArray()) row.append(cell.toString());
                    data.append(row);
                }
                s->set_data(data);
                resolve(ToolResult::ok("Sheet data replaced",
                                        QJsonObject{{"rows", static_cast<int>(data.size())}}));
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
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"name", s->sheet_name()},
                    {"rows", s->row_count()}, {"cols", s->col_count()},
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
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
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
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .string("position", "above | below").default_str("below").enums({"above", "below"})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                if (args["position"].toString("below") == "above") s->insert_row_above();
                else s->insert_row_below();
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
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .string("position", "left | right").default_str("right").enums({"left", "right"})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                if (args["position"].toString("right") == "left") s->insert_col_left();
                else s->insert_col_right();
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
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
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
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
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
        t.description = "Write a sheet to a CSV file on disk (programmatic, no file dialog).";
        t.category = "excel";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("sheet_index", "Sheet index (-1 = active)").default_int(-1).min(-1)
            .string("path", "Output CSV file path").required().length(1, 1024)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* s = resolve_sheet(args);
                if (!s) { resolve(ToolResult::fail("Sheet not found")); return; }
                QFile f(args["path"].toString());
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    resolve(ToolResult::fail("Cannot open path for writing")); return;
                }
                QTextStream out(&f);
                const auto data = s->get_data();
                for (const auto& row : data) {
                    QStringList parts;
                    for (const auto& cell : row) {
                        QString c = cell;
                        if (c.contains(',') || c.contains('"') || c.contains('\n')) {
                            c.replace('"', "\"\"");
                            c = '"' + c + '"';
                        }
                        parts.append(c);
                    }
                    out << parts.join(',') << '\n';
                }
                f.close();
                resolve(ToolResult::ok("CSV exported", QJsonObject{
                    {"path", args["path"].toString()},
                    {"rows", static_cast<int>(data.size())},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 excel tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
