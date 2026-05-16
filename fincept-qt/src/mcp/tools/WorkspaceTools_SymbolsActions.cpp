// src/mcp/tools/WorkspaceTools_SymbolsActions.cpp
//
// Symbol-link-group tools (list / metadata / reset / get|set group + slot
// symbol / clear / get_active_symbol), action-registry tools (list / invoke
// / find), and the command-bar text dispatcher.
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

void workspace_internal::register_symbol_action_tools(std::vector<ToolDef>& tools) {
    // ═══════════════════════════════════════════════════════════════════
    //  SYMBOL LINK GROUPS (9)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_symbol_groups";
        t.description = "List all symbol link groups (A..J) with name, color, enabled flag, and current symbol.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                QJsonArray arr;
                for (auto g : all_symbol_groups())
                    arr.append(group_to_json(g));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_symbol_group_metadata";
        t.description = "Update one or more of name / color / enabled for a symbol link group.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("group", "Group letter A..J").required().length(1, 1)
            .string("name", "New display name (empty to keep)").default_str("").length(0, 64)
            .string("color", "New hex color #RRGGBB or #AARRGGBB (empty to keep)").default_str("").length(0, 16)
            .boolean("enabled", "Whether the slot appears in cycle/menus").default_bool(true)
            .boolean("apply_enabled", "Whether to apply the enabled value").default_bool(false)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const SymbolGroup g = parse_group(args);
                if (g == SymbolGroup::None) { resolve(ToolResult::fail("Invalid group letter")); return; }
                auto& reg = SymbolGroupRegistry::instance();
                const QString name = args["name"].toString();
                const QString color = args["color"].toString();
                if (!name.isEmpty()) reg.set_name(g, name);
                if (!color.isEmpty()) reg.set_color(g, QColor(color));
                if (args["apply_enabled"].toBool(false))
                    reg.set_enabled(g, args["enabled"].toBool());
                resolve(ToolResult::ok("Group metadata updated", group_to_json(g)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "reset_symbol_group";
        t.description = "Reset a symbol link group to factory defaults (name, color, enabled).";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("group", "Group letter A..J").required().length(1, 1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const SymbolGroup g = parse_group(args);
                if (g == SymbolGroup::None) { resolve(ToolResult::fail("Invalid group letter")); return; }
                SymbolGroupRegistry::instance().reset_to_default(g);
                resolve(ToolResult::ok("Group reset", group_to_json(g)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_group_symbol";
        t.description = "Get the current symbol assigned to a link group's primary slot.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("group", "Group letter A..J").required().length(1, 1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const SymbolGroup g = parse_group(args);
                if (g == SymbolGroup::None) { resolve(ToolResult::fail("Invalid group letter")); return; }
                const SymbolRef ref = SymbolContext::instance().group_symbol(g);
                resolve(ToolResult::ok_data(QJsonObject{
                    {"group", QString(symbol_group_letter(g))},
                    {"symbol", ref.symbol},
                    {"exchange", ref.exchange},
                    {"asset_class", ref.asset_class},
                    {"display", ref.display()},
                    {"has_value", SymbolContext::instance().has_group_symbol(g)},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_group_symbol";
        t.description = "Push a symbol into a link group's primary slot. All linked panels update in lockstep.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("group", "Group letter A..J").required().length(1, 1)
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .string("exchange", "Exchange code").default_str("").length(0, 16)
            .string("asset_class", "Asset class (equity, fx, crypto, future, bond, index, commodity, option)").default_str("").length(0, 16)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const SymbolGroup g = parse_group(args);
                if (g == SymbolGroup::None) { resolve(ToolResult::fail("Invalid group letter")); return; }
                SymbolRef ref;
                ref.symbol = args["symbol"].toString();
                ref.exchange = args["exchange"].toString();
                ref.asset_class = args["asset_class"].toString();
                SymbolContext::instance().set_group_symbol(g, ref, nullptr);
                resolve(ToolResult::ok("Group symbol set",
                                        QJsonObject{{"group", QString(symbol_group_letter(g))},
                                                    {"symbol", ref.symbol}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "clear_symbol_group";
        t.description = "Clear the symbol assigned to a link group's primary slot.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("group", "Group letter A..J").required().length(1, 1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const SymbolGroup g = parse_group(args);
                if (g == SymbolGroup::None) { resolve(ToolResult::fail("Invalid group letter")); return; }
                SymbolContext::instance().set_group_symbol(g, SymbolRef{}, nullptr);
                resolve(ToolResult::ok("Group cleared", QJsonObject{{"group", QString(symbol_group_letter(g))}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_group_slot_symbol";
        t.description = "Get the symbol assigned to a (group, slot) pair. Slots include 'primary', 'comparison', 'currency'.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("group", "Group letter A..J").required().length(1, 1)
            .string("slot", "Slot name").required().length(1, 32)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const SymbolGroup g = parse_group(args);
                if (g == SymbolGroup::None) { resolve(ToolResult::fail("Invalid group letter")); return; }
                const QString slot = args["slot"].toString();
                const SymbolRef ref = SymbolContext::instance().group_slot_symbol(g, slot);
                resolve(ToolResult::ok_data(QJsonObject{
                    {"group", QString(symbol_group_letter(g))},
                    {"slot", slot},
                    {"symbol", ref.symbol},
                    {"exchange", ref.exchange},
                    {"asset_class", ref.asset_class},
                    {"has_value", SymbolContext::instance().has_group_slot_symbol(g, slot)},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_group_slot_symbol";
        t.description = "Push a symbol into a (group, slot) pair.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("group", "Group letter A..J").required().length(1, 1)
            .string("slot", "Slot name").required().length(1, 32)
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .string("exchange", "Exchange code").default_str("").length(0, 16)
            .string("asset_class", "Asset class").default_str("").length(0, 16)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const SymbolGroup g = parse_group(args);
                if (g == SymbolGroup::None) { resolve(ToolResult::fail("Invalid group letter")); return; }
                SymbolRef ref;
                ref.symbol = args["symbol"].toString();
                ref.exchange = args["exchange"].toString();
                ref.asset_class = args["asset_class"].toString();
                SymbolContext::instance().set_group_slot_symbol(g, args["slot"].toString(), ref, nullptr);
                resolve(ToolResult::ok("Slot symbol set",
                                        QJsonObject{{"group", QString(symbol_group_letter(g))},
                                                    {"slot", args["slot"].toString()},
                                                    {"symbol", ref.symbol}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_active_symbol";
        t.description = "Get the most recently touched symbol anywhere in the app (group or not).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                const SymbolRef ref = SymbolContext::instance().active();
                resolve(ToolResult::ok_data(QJsonObject{
                    {"symbol", ref.symbol},
                    {"exchange", ref.exchange},
                    {"asset_class", ref.asset_class},
                    {"display", ref.display()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  ACTION REGISTRY (3)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_actions";
        t.description = "List every registered shell action (id, display, category, hotkey, parameter slots).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                QJsonArray arr;
                for (const auto& id : ActionRegistry::instance().all_ids()) {
                    if (auto* a = ActionRegistry::instance().find(id))
                        arr.append(action_to_json(*a));
                }
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "invoke_action";
        t.description = "Invoke a registered shell action by id with optional args (matches command-bar / palette behaviour).";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("id", "Action id (e.g. 'frame.toggle_focus_mode', 'layout.save')").required().length(1, 128)
            .object("args", "Per-action argument map keyed by parameter_slot.name")
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                CommandContext c;
                c.shell = &TerminalShell::instance();
                c.focused_frame = qobject_cast<WindowFrame*>(qApp->activeWindow());
                const QJsonObject extra = args["args"].toObject();
                for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
                    c.args.insert(it.key(), it.value().toVariant());
                auto r = ActionRegistry::instance().invoke(args["id"].toString(), c);
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Action invoked", QJsonObject{{"id", args["id"].toString()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "find_actions";
        t.description = "Prefix-match registered actions (palette-style autocomplete).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("prefix", "Search prefix (matched against id, display, aliases)").required().length(1, 64)
            .integer("max_results", "Max results").default_int(25).between(1, 200)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const auto matches = ActionRegistry::instance().match(args["prefix"].toString(),
                                                                        args["max_results"].toInt(25));
                QJsonArray arr;
                for (auto* a : matches) if (a) arr.append(action_to_json(*a));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  COMMAND-BAR (1)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "run_command_bar_text";
        t.description = "Run a raw command-bar string ('dash add news', 'markets replace settings', 'news remove') against the focused window. Mirrors what a user typing would do.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("text", "Command-bar text (e.g. 'dashboard add news')").required().length(1, 256)
            .integer("window_id", "Target window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }

                // Inline parser — mirrors CommandBar_Suggestions::try_parse_dock_command.
                const QString text = args["text"].toString().trimmed();
                static const QRegularExpression re_add(R"(^(.+?)\s+add\s+(.+)$)",
                                                       QRegularExpression::CaseInsensitiveOption);
                static const QRegularExpression re_replace(R"(^(.+?)\s+replace\s+(.+)$)",
                                                           QRegularExpression::CaseInsensitiveOption);
                static const QRegularExpression re_remove(R"(^(.+?)\s+remove$)",
                                                          QRegularExpression::CaseInsensitiveOption);
                auto m_add = re_add.match(text);
                auto m_rep = re_replace.match(text);
                auto m_rem = re_remove.match(text);
                if (m_add.hasMatch()) {
                    w->dock_router()->add_alongside(m_add.captured(1).trimmed(), m_add.captured(2).trimmed());
                    resolve(ToolResult::ok("Added alongside",
                                            QJsonObject{{"action", "add"},
                                                        {"primary", m_add.captured(1).trimmed()},
                                                        {"secondary", m_add.captured(2).trimmed()}}));
                    return;
                }
                if (m_rep.hasMatch()) {
                    w->dock_router()->replace_screen(m_rep.captured(1).trimmed(), m_rep.captured(2).trimmed());
                    resolve(ToolResult::ok("Replaced",
                                            QJsonObject{{"action", "replace"},
                                                        {"primary", m_rep.captured(1).trimmed()},
                                                        {"secondary", m_rep.captured(2).trimmed()}}));
                    return;
                }
                if (m_rem.hasMatch()) {
                    w->dock_router()->remove_screen(m_rem.captured(1).trimmed());
                    resolve(ToolResult::ok("Removed",
                                            QJsonObject{{"action", "remove"},
                                                        {"primary", m_rem.captured(1).trimmed()}}));
                    return;
                }
                // Bare screen id → just navigate.
                w->dock_router()->navigate(text);
                resolve(ToolResult::ok("Navigated", QJsonObject{{"action", "navigate"}, {"screen_id", text}}));
            });
        };
        tools.push_back(std::move(t));
    }

}
} // namespace fincept::mcp::tools
