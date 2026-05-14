// src/mcp/tools/WorkspaceTools_LayoutsSnapshots.cpp
//
// Layout + workspace tools: list/save/apply/delete/rename/export/import_layout,
// layout templates (list/apply), current/restore workspace, workspace snapshots
// (list/snapshot_now/delete/rename/restore), and crash-recovery status.
//
// Part of the topic-based split of WorkspaceTools.cpp.

#include "mcp/tools/WorkspaceTools.h"
#include "mcp/tools/WorkspaceTools_internal.h"

#include "app/DockScreenRouter.h"
#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/actions/ActionDef.h"
#include "core/actions/ActionRegistry.h"
#include "core/identity/Uuid.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/LayoutTemplates.h"
#include "core/layout/LayoutTypes.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelHandle.h"
#include "core/panel/PanelRegistry.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "core/symbol/SymbolRef.h"
#include "core/window/WindowRegistry.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRect>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>

namespace fincept::mcp::tools {

using namespace fincept::mcp::tools::workspace_internal;

void workspace_internal::register_layout_snapshot_tools(std::vector<ToolDef>& tools) {
    // ═══════════════════════════════════════════════════════════════════
    //  LAYOUTS / WORKSPACES (13)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_layouts";
        t.description = "List all saved workspaces (id, name, kind, timestamps, description).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto r = LayoutCatalog::instance().list_layouts();
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                QJsonArray arr;
                for (const auto& e : r.value()) arr.append(layout_entry_to_json(e));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "list_recent_layouts";
        t.description = "List the most-recently-updated layouts (excludes auto-snapshots by default).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("limit", "Max rows").default_int(10).between(1, 100)
            .boolean("include_auto", "Include auto-snapshots").default_bool(false)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto r = LayoutCatalog::instance().recent_layouts(args["limit"].toInt(10),
                                                                    args["include_auto"].toBool(false));
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                QJsonArray arr;
                for (const auto& e : r.value()) arr.append(layout_entry_to_json(e));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_layout";
        t.description = "Load a full Workspace by id (frames, panels, dock state, variants).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Layout UUID").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const LayoutId id = LayoutId::from_string(args["id"].toString());
                auto r = LayoutCatalog::instance().load_workspace(id);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok_data(r.value().to_json()));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "save_current_layout";
        t.description = "Capture the current shell state into a Workspace and persist it.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("name", "Display name for the layout").required().length(1, 128)
            .string("description", "Optional description").default_str("").length(0, 512)
            .string("kind", "Layout kind: user | auto | builtin | crash_recovery").default_str("user").length(1, 32)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto ws = layout::WorkspaceShell::capture(args["name"].toString(),
                                                            args["kind"].toString("user"));
                ws.description = args["description"].toString();
                auto r = LayoutCatalog::instance().save_workspace(ws);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Layout saved",
                                        QJsonObject{{"id", r.value().to_string()},
                                                    {"name", ws.name},
                                                    {"kind", ws.kind}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "apply_layout";
        t.description = "Load a saved workspace by id and apply it to the current shell (replaces current frames).";
        t.category = "workspace";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Layout UUID").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const LayoutId id = LayoutId::from_string(args["id"].toString());
                auto r = LayoutCatalog::instance().load_workspace(id);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                const int n = layout::WorkspaceShell::apply(r.value());
                resolve(ToolResult::ok(QString("Layout applied (%1 frames)").arg(n),
                                        QJsonObject{{"id", args["id"].toString()},
                                                    {"frames_applied", n}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "delete_layout";
        t.description = "Delete a saved workspace by id.";
        t.category = "workspace";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Layout UUID").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const LayoutId id = LayoutId::from_string(args["id"].toString());
                auto r = LayoutCatalog::instance().remove_layout(id);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Layout deleted", QJsonObject{{"id", args["id"].toString()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "rename_layout";
        t.description = "Rename a saved workspace (and optionally update description).";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Layout UUID").required().length(1, 64)
            .string("name", "New name").required().length(1, 128)
            .string("description", "New description (empty to keep current)").default_str("").length(0, 512)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const LayoutId id = LayoutId::from_string(args["id"].toString());
                auto r = LayoutCatalog::instance().load_workspace(id);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                auto ws = r.value();
                ws.name = args["name"].toString();
                const QString desc = args["description"].toString();
                if (!desc.isEmpty()) ws.description = desc;
                auto sr = LayoutCatalog::instance().save_workspace(ws);
                if (sr.is_err()) { resolve(ToolResult::fail(QString::fromStdString(sr.error()))); return; }
                resolve(ToolResult::ok("Layout renamed",
                                        QJsonObject{{"id", args["id"].toString()}, {"name", ws.name}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "export_layout";
        t.description = "Export a saved workspace to a JSON file path.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Layout UUID").required().length(1, 64)
            .string("path", "Output file path (.json)").required().length(1, 1024)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const LayoutId id = LayoutId::from_string(args["id"].toString());
                auto r = LayoutCatalog::instance().export_to(id, args["path"].toString());
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Layout exported", QJsonObject{{"path", args["path"].toString()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "import_layout";
        t.description = "Import a workspace JSON file as a new layout.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("path", "Input file path (.json)").required().length(1, 1024)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto r = LayoutCatalog::instance().import_from(args["path"].toString());
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Layout imported", QJsonObject{{"id", r.value().to_string()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "list_layout_templates";
        t.description = "List built-in layout personas (e.g. equity_trader) seeded for new users.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                QJsonArray arr;
                for (const auto& p : layout::LayoutTemplates::personas()) {
                    arr.append(QJsonObject{{"id", p.id}, {"display_name", p.display_name},
                                            {"description", p.description}});
                }
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "apply_layout_template";
        t.description = "Build and apply a built-in persona template (one frame seeded with a representative panel).";
        t.category = "workspace";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("persona_id", "Persona id (use list_layout_templates)").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto ws = layout::LayoutTemplates::make(args["persona_id"].toString());
                if (ws.name.isEmpty()) { resolve(ToolResult::fail("Unknown persona id")); return; }
                auto sr = LayoutCatalog::instance().save_workspace(ws);
                if (sr.is_err()) { resolve(ToolResult::fail(QString::fromStdString(sr.error()))); return; }
                const int n = layout::WorkspaceShell::apply(ws);
                resolve(ToolResult::ok(QString("Template applied (%1 frames)").arg(n),
                                        QJsonObject{{"id", sr.value().to_string()},
                                                    {"persona", args["persona_id"].toString()},
                                                    {"frames_applied", n}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_current_workspace";
        t.description = "Get the name + id of the currently-applied workspace this session (empty if none).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                resolve(ToolResult::ok_data(QJsonObject{
                    {"name", layout::WorkspaceShell::current_name()},
                    {"id", layout::WorkspaceShell::current_id().to_string()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "restore_last_workspace";
        t.description = "Cold-boot restore: re-apply the last-loaded layout or fall back to the latest auto snapshot.";
        t.category = "workspace";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                const int n = layout::WorkspaceShell::load_last_or_default();
                resolve(ToolResult::ok(QString("Restored (%1 frames)").arg(n),
                                        QJsonObject{{"frames_restored", n}}));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  SNAPSHOTS / CRASH RECOVERY (6)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_workspace_snapshots";
        t.description = "List recent workspace snapshots (auto / named / crash_recovery), newest first.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("limit", "Max rows").default_int(10).between(1, 100)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* ring = TerminalShell::instance().snapshot_ring();
                if (!ring) { resolve(ToolResult::fail("Snapshot ring unavailable")); return; }
                auto r = ring->latest(args["limit"].toInt(10));
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                QJsonArray arr;
                for (const auto& e : r.value()) arr.append(snapshot_entry_to_json(e));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "snapshot_workspace_now";
        t.description = "Force an immediate auto-snapshot of the current workspace state.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* ring = TerminalShell::instance().snapshot_ring();
                if (!ring) { resolve(ToolResult::fail("Snapshot ring unavailable")); return; }
                auto ws = layout::WorkspaceShell::capture("auto", "auto");
                const QByteArray payload = QJsonDocument(ws.to_json()).toJson(QJsonDocument::Compact);
                auto r = ring->add(payload, "auto", /*force=*/true);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Snapshot taken", QJsonObject{{"snapshot_id", r.value()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "delete_workspace_snapshot";
        t.description = "Delete a workspace snapshot by snapshot_id.";
        t.category = "workspace";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("snapshot_id", "Snapshot id").required().min(1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* ring = TerminalShell::instance().snapshot_ring();
                if (!ring) { resolve(ToolResult::fail("Snapshot ring unavailable")); return; }
                const qint64 id = static_cast<qint64>(args["snapshot_id"].toDouble());
                auto r = ring->erase(id);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Snapshot deleted", QJsonObject{{"snapshot_id", id}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "rename_workspace_snapshot";
        t.description = "Set or clear the user-visible name on a snapshot (pass empty to clear).";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("snapshot_id", "Snapshot id").required().min(1)
            .string("name", "New name (empty to clear)").required().length(0, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* ring = TerminalShell::instance().snapshot_ring();
                if (!ring) { resolve(ToolResult::fail("Snapshot ring unavailable")); return; }
                const qint64 id = static_cast<qint64>(args["snapshot_id"].toDouble());
                auto r = ring->rename(id, args["name"].toString());
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Snapshot renamed", QJsonObject{{"snapshot_id", id}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "restore_workspace_snapshot";
        t.description = "Load a snapshot's payload and apply it as the current workspace.";
        t.category = "workspace";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("snapshot_id", "Snapshot id").required().min(1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* ring = TerminalShell::instance().snapshot_ring();
                if (!ring) { resolve(ToolResult::fail("Snapshot ring unavailable")); return; }
                const qint64 id = static_cast<qint64>(args["snapshot_id"].toDouble());
                auto p = ring->load_payload(id);
                if (p.is_err()) { resolve(ToolResult::fail(QString::fromStdString(p.error()))); return; }
                const auto ws = layout::Workspace::from_json(QJsonDocument::fromJson(p.value()).object());
                const int n = layout::WorkspaceShell::apply(ws);
                resolve(ToolResult::ok(QString("Snapshot restored (%1 frames)").arg(n),
                                        QJsonObject{{"snapshot_id", id}, {"frames_applied", n}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_crash_recovery_status";
        t.description = "Whether the previous session ended uncleanly and which snapshots are restorable.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* cr = TerminalShell::instance().crash_recovery();
                if (!cr) { resolve(ToolResult::fail("CrashRecovery unavailable")); return; }
                const bool needs = cr->needs_recovery();
                auto r = cr->latest_snapshots(5);
                QJsonArray candidates;
                if (r.is_ok()) {
                    for (const auto& e : r.value()) candidates.append(snapshot_entry_to_json(e));
                }
                resolve(ToolResult::ok_data(QJsonObject{
                    {"needs_recovery", needs},
                    {"last_clean_shutdown_ms", cr->last_clean_shutdown_ms()},
                    {"latest_auto_snapshot_ms", cr->latest_auto_snapshot_ms()},
                    {"candidates", candidates},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

}
} // namespace fincept::mcp::tools
