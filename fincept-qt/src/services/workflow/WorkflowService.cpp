#include "services/workflow/WorkflowService.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/WorkflowExecutor.h"
#include "storage/repositories/WorkflowRepository.h"
#include "trading/AccountManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>

#include <cmath>

namespace fincept::workflow {

WorkflowService& WorkflowService::instance() {
    static WorkflowService s;
    return s;
}

WorkflowService::WorkflowService() : QObject(nullptr) {}

void WorkflowService::save_workflow(const WorkflowDef& wf) {
    auto r = WorkflowRepository::instance().save(wf);
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("Save failed: %1").arg(QString::fromStdString(r.error())));
        emit save_failed(QString::fromStdString(r.error()));
        return;
    }
    emit workflow_saved(wf.id);
    LOG_INFO("WorkflowService", QString("Saved workflow: %1").arg(wf.name));
}

void WorkflowService::load_workflow(const QString& id) {
    auto r = WorkflowRepository::instance().load(id);
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("Load failed: %1").arg(QString::fromStdString(r.error())));
        emit workflow_load_failed(QString::fromStdString(r.error()));
        return;
    }
    emit workflow_loaded(r.value());
}

void WorkflowService::list_workflows() {
    auto r = WorkflowRepository::instance().list_all();
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("List failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }

    // Convert WorkflowRow summaries to lightweight WorkflowDefs (no nodes/edges)
    QVector<WorkflowDef> workflows;
    for (const auto& row : r.value()) {
        WorkflowDef wf;
        wf.id = row.id;
        wf.name = row.name;
        wf.description = row.description;
        wf.created_at = row.created_at;
        wf.updated_at = row.updated_at;

        if (row.status == "idle")
            wf.status = WorkflowStatus::Idle;
        else if (row.status == "running")
            wf.status = WorkflowStatus::Running;
        else if (row.status == "completed")
            wf.status = WorkflowStatus::Completed;
        else if (row.status == "error")
            wf.status = WorkflowStatus::Error;
        else
            wf.status = WorkflowStatus::Draft;

        workflows.append(wf);
    }
    emit workflows_listed(workflows);
}

void WorkflowService::delete_workflow(const QString& id) {
    auto r = WorkflowRepository::instance().remove(id);
    if (r.is_err()) {
        LOG_ERROR("WorkflowService", QString("Delete failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    emit workflow_deleted(id);
    LOG_INFO("WorkflowService", QString("Deleted workflow: %1").arg(id));
}

// ── Import/Export ──────────────────────────────────────────────────────

Result<WorkflowDef> WorkflowService::import_from_json(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Result<WorkflowDef>::err("Failed to open file: " + path.toStdString());

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return Result<WorkflowDef>::err("Invalid JSON workflow file");

    QJsonObject obj = doc.object();
    WorkflowDef wf;
    wf.id = obj.value("id").toString();
    wf.name = obj.value("name").toString("Imported Workflow");
    wf.description = obj.value("description").toString();
    // static_data (and with it the execution mode) is deliberately NOT imported: a file
    // from elsewhere must never arrive armed for LIVE trading. Imports always run PAPER.

    // The canvas keys nodes and edges by id: a missing or repeated id would make
    // two nodes collapse into one, so repair them on the way in.
    QSet<QString> seen_node_ids;
    QSet<QString> seen_edge_ids;

    for (const auto& nv : obj.value("nodes").toArray()) {
        QJsonObject no = nv.toObject();
        NodeDef nd;
        nd.id = no.value("id").toString();
        if (nd.id.isEmpty() || seen_node_ids.contains(nd.id))
            nd.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        seen_node_ids.insert(nd.id);
        nd.type = no.value("type").toString();
        nd.name = no.value("name").toString();
        nd.type_version = no.value("typeVersion").toInt(1);
        nd.x = no.value("position").toObject().value("x").toDouble();
        nd.y = no.value("position").toObject().value("y").toDouble();
        nd.parameters = no.value("parameters").toObject();
        nd.disabled = no.value("disabled").toBool();
        nd.continue_on_fail = no.value("continueOnFail").toBool();
        nd.retry_on_fail = no.value("retryOnFail").toBool();
        nd.max_tries = no.value("maxTries").toInt(1);
        wf.nodes.append(nd);
    }

    for (const auto& ev : obj.value("edges").toArray()) {
        QJsonObject eo = ev.toObject();
        EdgeDef ed;
        ed.id = eo.value("id").toString();
        if (ed.id.isEmpty() || seen_edge_ids.contains(ed.id))
            ed.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        seen_edge_ids.insert(ed.id);
        ed.source_node = eo.value("source").toString();
        ed.target_node = eo.value("target").toString();
        ed.source_port = eo.value("sourceHandle").toString();
        ed.target_port = eo.value("targetHandle").toString();
        wf.edges.append(ed);
    }

    return Result<WorkflowDef>::ok(std::move(wf));
}

Result<void> WorkflowService::export_to_json(const WorkflowDef& wf, const QString& path) {
    QJsonObject obj;
    obj["id"] = wf.id;
    obj["name"] = wf.name;
    obj["description"] = wf.description;

    QJsonArray nodes_arr;
    for (const auto& nd : wf.nodes) {
        QJsonObject no;
        no["id"] = nd.id;
        no["type"] = nd.type;
        no["name"] = nd.name;
        no["typeVersion"] = nd.type_version;
        no["position"] = QJsonObject{{"x", nd.x}, {"y", nd.y}};
        no["parameters"] = nd.parameters;
        no["disabled"] = nd.disabled;
        no["continueOnFail"] = nd.continue_on_fail;
        no["retryOnFail"] = nd.retry_on_fail;
        no["maxTries"] = nd.max_tries;
        nodes_arr.append(no);
    }
    obj["nodes"] = nodes_arr;

    QJsonArray edges_arr;
    for (const auto& ed : wf.edges) {
        QJsonObject eo;
        eo["id"] = ed.id;
        eo["source"] = ed.source_node;
        eo["target"] = ed.target_node;
        eo["sourceHandle"] = ed.source_port;
        eo["targetHandle"] = ed.target_port;
        edges_arr.append(eo);
    }
    obj["edges"] = edges_arr;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return Result<void>::err("Failed to write file: " + path.toStdString());

    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size())
        return Result<void>::err("Failed to write file (disk full?): " + path.toStdString());
    LOG_INFO("WorkflowService", QString("Exported workflow to: %1").arg(path));
    return Result<void>::ok();
}

// ── Execution safety ───────────────────────────────────────────────────

namespace wf_service_detail {

QString variant_text(const QJsonValue& v) {
    return v.toVariant().toString();
}

// True when the MCP tool named by an mcp.tool_call node is a declared-destructive
// internal tool (anything that mutates state: orders, transfers, writes).
bool mcp_tool_is_destructive(const QString& tool) {
    if (tool.isEmpty() || tool.contains(QLatin1String("__")))
        return false; // external tools carry no destructiveness metadata
    for (const auto& t : mcp::McpService::instance().get_all_tools()) {
        if (t.is_internal && t.name == tool)
            return t.is_destructive;
    }
    return false;
}

// The broker account an order node will use — the same resolution the trading
// bridges apply: an explicit, existing account_id wins, otherwise the first ACTIVE
// account of the node's broker (else its first account).
QString account_note(const QString& broker, const QString& account_id_param) {
    auto& am = fincept::trading::AccountManager::instance();
    fincept::trading::BrokerAccount acct;
    const QString explicit_id = account_id_param.trimmed();
    if (!explicit_id.isEmpty() && am.has_account(explicit_id)) {
        acct = am.get_account(explicit_id);
    } else {
        const auto accounts = am.list_accounts(broker);
        for (const auto& a : accounts) {
            if (a.is_active) {
                acct = a;
                break;
            }
        }
        if (acct.account_id.isEmpty() && !accounts.isEmpty())
            acct = accounts.first();
    }
    if (acct.account_id.isEmpty())
        return WorkflowService::tr("NO %1 ACCOUNT CONFIGURED — this node will fail").arg(broker.toUpper());
    const QString name = acct.display_name.isEmpty() ? acct.account_id.left(8) : acct.display_name;
    return acct.trading_mode == QLatin1String("paper")
               ? WorkflowService::tr("account \"%1\" (account is in PAPER mode — the broker adapter simulates it)")
                     .arg(name)
               : WorkflowService::tr("account \"%1\" (LIVE account)").arg(name);
}

QString order_detail(const NodeDef& nd) {
    QString detail;
    const QString symbol = nd.parameters.value(QStringLiteral("symbol")).toString();
    const QString side = nd.parameters.value(QStringLiteral("side")).toString();
    QString qty = variant_text(nd.parameters.value(QStringLiteral("quantity")));
    if (qty.isEmpty())
        qty = variant_text(nd.parameters.value(QStringLiteral("total_quantity")));
    if (qty.isEmpty())
        qty = variant_text(nd.parameters.value(QStringLiteral("position_size")));
    if (!symbol.isEmpty())
        detail += QStringLiteral("  %1").arg(symbol);
    if (!side.isEmpty())
        detail += QStringLiteral("  %1").arg(side.toUpper());
    if (!qty.isEmpty())
        detail += QStringLiteral(" x%1").arg(qty);
    return detail;
}

void collect_rows(const WorkflowDef& wf, bool live_mode, const QString& via, int depth, QSet<QString>& visited,
                  QVector<WorkflowService::ActionRow>& rows) {
    for (const auto& nd : wf.nodes) {
        if (nd.disabled)
            continue;

        const QString label = nd.name.isEmpty() ? nd.type : nd.name;
        QString detail;
        bool money = false;

        if (WorkflowExecutor::is_moded_order_type(nd.type) || WorkflowExecutor::is_live_only_order_type(nd.type)) {
            if (!live_mode)
                continue; // PAPER: the executor forces / refuses every order node
            if (WorkflowExecutor::is_moded_order_type(nd.type) && !WorkflowExecutor::node_requests_live(nd.parameters))
                continue; // the node itself is set to paper
            const QString broker = nd.parameters.value(QStringLiteral("broker")).toString(QStringLiteral("fyers"));
            detail = order_detail(nd) +
                     QStringLiteral("  → %1 · %2")
                         .arg(broker, account_note(broker, nd.parameters.value(QStringLiteral("account_id")).toString()));
            money = true;
        } else if (nd.type == QLatin1String("mcp.tool_call")) {
            const QString tool = nd.parameters.value(QStringLiteral("tool")).toString().trimmed();
            if (WorkflowExecutor::mcp_tool_moves_money(tool)) {
                if (!live_mode)
                    continue; // refused by the executor in PAPER
                detail = QStringLiteral("  %1").arg(WorkflowService::tr("live-trading MCP tool: %1").arg(tool));
                money = true;
            } else if (mcp_tool_is_destructive(tool)) {
                detail = QStringLiteral("  %1").arg(WorkflowService::tr("destructive MCP tool: %1").arg(tool));
            } else {
                continue;
            }
        } else if (nd.type == QLatin1String("control.execute_workflow")) {
            const QString sub_id = nd.parameters.value(QStringLiteral("workflow_id")).toString().trimmed();
            if (sub_id.isEmpty() || depth >= WorkflowExecutor::kMaxSubworkflowDepth || visited.contains(sub_id))
                continue;
            visited.insert(sub_id);
            auto loaded = WorkflowRepository::instance().load(sub_id);
            if (loaded.is_ok()) {
                const WorkflowDef& sub = loaded.value();
                const QString sub_via = via.isEmpty() ? sub.name : via + QStringLiteral(" › ") + sub.name;
                // A sub-workflow runs in the parent's mode, whatever it was saved with.
                collect_rows(sub, live_mode, sub_via, depth + 1, visited, rows);
            }
            continue;
        } else {
            continue;
        }

        QString text = QStringLiteral("  • %1  [%2]").arg(label, nd.type) + detail;
        if (!via.isEmpty())
            text += QStringLiteral("   ") + WorkflowService::tr("(in sub-workflow: %1)").arg(via);
        rows.append({nd.id, text, money});
    }
}

double param_number(const QJsonValue& v, bool* ok) {
    if (v.isDouble()) {
        *ok = true;
        return v.toDouble();
    }
    return v.toVariant().toString().trimmed().toDouble(ok);
}

// Numeric parameter with a fallback for missing / non-numeric values.
double param_or(const NodeDef& nd, const char* key, double fallback) {
    const QJsonValue v = nd.parameters.value(QLatin1String(key));
    if (v.isUndefined() || v.isNull())
        return fallback;
    bool ok = false;
    const double d = param_number(v, &ok);
    return ok ? d : fallback;
}

} // namespace wf_service_detail

QVector<WorkflowService::ActionRow> WorkflowService::live_action_rows(const WorkflowDef& wf, bool live_mode) {
    QVector<ActionRow> rows;
    QSet<QString> visited;
    if (!wf.id.isEmpty())
        visited.insert(wf.id); // a workflow calling itself is reported once, not recursed
    wf_service_detail::collect_rows(wf, live_mode, {}, 0, visited, rows);
    return rows;
}

QStringList WorkflowService::simulated_live_rows(const WorkflowDef& wf) {
    QStringList rows;
    for (const auto& nd : wf.nodes) {
        if (nd.disabled || !WorkflowExecutor::is_moded_order_type(nd.type) ||
            !WorkflowExecutor::node_requests_live(nd.parameters))
            continue;
        rows << QStringLiteral("  • %1  [%2]%3")
                    .arg(nd.name.isEmpty() ? nd.type : nd.name, nd.type, wf_service_detail::order_detail(nd));
    }
    return rows;
}

QVector<WorkflowService::ValidationIssue> WorkflowService::validate(const WorkflowDef& wf) {
    using wf_service_detail::param_number;
    using wf_service_detail::param_or;

    QVector<ValidationIssue> issues;
    auto& reg = NodeRegistry::instance();

    QSet<QString> has_input;
    QHash<QString, QVector<QString>> adjacency;
    QHash<QString, QString> names;
    for (const auto& nd : wf.nodes) {
        adjacency[nd.id];
        names.insert(nd.id, nd.name.isEmpty() ? nd.type : nd.name);
    }
    for (const auto& ed : wf.edges) {
        if (!adjacency.contains(ed.source_node) || !adjacency.contains(ed.target_node))
            continue;
        adjacency[ed.source_node].append(ed.target_node);
        has_input.insert(ed.target_node);
    }

    // Required parameters a node takes from its input when left blank (the bridges fall back).
    static const QHash<QString, QString> kFromInput = {
        {QStringLiteral("trading.cancel_order"), QStringLiteral("order_id")},
        {QStringLiteral("trading.modify_order"), QStringLiteral("order_id")},
        {QStringLiteral("trading.alert"), QStringLiteral("message")},
        {QStringLiteral("mcp.tool_call"), QStringLiteral("tool")}, // a Tool Picker can supply it
    };

    for (const auto& nd : wf.nodes) {
        if (nd.disabled)
            continue;
        const auto* td = reg.find(nd.type);
        if (!td) {
            issues.append({nd.id, tr("Unknown node type '%1' — this build cannot run it.").arg(nd.type)});
            continue;
        }
        for (const auto& pd : td->parameters) {
            if (!pd.required || pd.type == QLatin1String("boolean"))
                continue;
            const QJsonValue v = nd.parameters.contains(pd.key) ? nd.parameters.value(pd.key) : pd.default_value;
            if (pd.type == QLatin1String("number")) {
                bool ok = false;
                const double d = param_number(v, &ok);
                if (!ok || std::isnan(d) || std::isinf(d))
                    issues.append({nd.id, tr("'%1' needs a number.").arg(pd.label)});
                continue;
            }
            if (v.toVariant().toString().trimmed().isEmpty()) {
                if (kFromInput.value(nd.type) == pd.key && has_input.contains(nd.id))
                    continue;
                issues.append({nd.id, tr("'%1' is required.").arg(pd.label)});
            }
        }

        // Mode must be one of the two exact values — the bridges treat anything else as live.
        if (WorkflowExecutor::is_moded_order_type(nd.type) || WorkflowExecutor::is_moded_read_type(nd.type)) {
            const QString mode = nd.parameters.value(QStringLiteral("mode")).toString(QStringLiteral("paper"));
            if (mode != QLatin1String("paper") && mode != QLatin1String("live"))
                issues.append({nd.id, tr("Mode is '%1' — it must be paper or live.").arg(mode)});
        }

        if (nd.type == QLatin1String("trading.place_order")) {
            if (param_or(nd, "quantity", 0) <= 0)
                issues.append({nd.id, tr("Quantity must be greater than 0.")});
            const QString ot = nd.parameters.value(QStringLiteral("order_type")).toString(QStringLiteral("market"));
            if (ot != QLatin1String("market") && param_or(nd, "price", 0) <= 0)
                issues.append({nd.id, tr("A %1 order needs a price greater than 0.").arg(ot)});
        } else if (nd.type == QLatin1String("trading.bracket_order")) {
            const double qty = param_or(nd, "quantity", 0);
            const double entry = param_or(nd, "entry_price", 0);
            const double sl = param_or(nd, "stop_loss", 0);
            const double tp = param_or(nd, "take_profit", 0);
            const bool buy = nd.parameters.value(QStringLiteral("side")).toString(QStringLiteral("buy")) !=
                             QLatin1String("sell");
            if (qty <= 0)
                issues.append({nd.id, tr("Quantity must be greater than 0.")});
            if (sl <= 0 || tp <= 0)
                issues.append({nd.id, tr("Stop loss and take profit must both be greater than 0.")});
            else if (buy ? !(sl < tp) : !(sl > tp))
                issues.append({nd.id, buy ? tr("For a BUY bracket the stop loss must be below the take profit.")
                                          : tr("For a SELL bracket the stop loss must be above the take profit.")});
            else if (entry > 0 && (buy ? !(sl < entry && entry < tp) : !(tp < entry && entry < sl)))
                issues.append({nd.id, tr("The entry price must lie between the stop loss and the take profit.")});
        } else if (nd.type == QLatin1String("trading.scale_in")) {
            if (param_or(nd, "total_quantity", 0) <= 0)
                issues.append({nd.id, tr("Total quantity must be greater than 0.")});
            if (param_or(nd, "tranches", 1) < 1)
                issues.append({nd.id, tr("Tranches must be at least 1.")});
        } else if (nd.type == QLatin1String("trading.trailing_stop")) {
            if (param_or(nd, "trail_value", 0) <= 0)
                issues.append({nd.id, tr("Trail value must be greater than 0.")});
        }
    }

    const QStringList cycle = WorkflowExecutor::find_cycle(adjacency);
    if (!cycle.isEmpty()) {
        QStringList chain;
        for (const auto& id : cycle)
            chain << names.value(id, id);
        chain << names.value(cycle.first(), cycle.first());
        const QString msg =
            tr("Part of a cycle: %1. Workflows must be acyclic.").arg(chain.join(QStringLiteral(" → ")));
        for (const auto& id : cycle)
            issues.append({id, msg});
    }
    return issues;
}

// ── Execution ──────────────────────────────────────────────────────────

WorkflowExecutor* WorkflowService::make_executor() {
    if (executor_) {
        if (executor_->is_running()) {
            // Replacing a live executor orphans its in-flight nodes (orders included)
            // and strands the editor in "running" — refuse; the caller can Stop first.
            LOG_WARN("WorkflowService", "Execution already in progress — ignoring new run request");
            return nullptr;
        }
        executor_->deleteLater();
        executor_ = nullptr;
    }

    auto* ex = new WorkflowExecutor(this);
    executor_ = ex;

    connect(ex, &WorkflowExecutor::execution_started, this, &WorkflowService::execution_started);
    connect(ex, &WorkflowExecutor::node_started, this, &WorkflowService::node_execution_started);
    connect(ex, &WorkflowExecutor::node_completed, this, &WorkflowService::node_execution_completed);
    connect(ex, &WorkflowExecutor::execution_finished, this, [this, ex](const WorkflowExecutionResult& result) {
        emit execution_finished(result);
        // Retire only THIS run's executor — a newer one may already be current.
        ex->deleteLater();
        if (executor_ == ex)
            executor_ = nullptr;
    });
    return ex;
}

void WorkflowService::execute_workflow(const WorkflowDef& wf) {
    if (auto* ex = make_executor())
        ex->execute(wf);
}

void WorkflowService::execute_from_node(const WorkflowDef& wf, const QString& start_node_id) {
    if (auto* ex = make_executor())
        ex->execute_from(wf, start_node_id);
}

void WorkflowService::stop_execution() {
    if (executor_)
        executor_->stop();
}

bool WorkflowService::is_executing() const {
    return executor_ && executor_->is_running();
}

} // namespace fincept::workflow
