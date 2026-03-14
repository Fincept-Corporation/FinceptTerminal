// Node Editor — Workflow persistence implementation

#include "workflow_store.h"
#include "storage/database.h"
#include "core/logger.h"
#include <ctime>

namespace fincept::node_editor {

using json = nlohmann::json;

// ============================================================================
// JSON serialization helpers
// ============================================================================
static json pin_to_json(const Pin& p) {
    return {{"id", p.id}, {"name", p.name}, {"color", p.color},
            {"conn_type", static_cast<int>(p.conn_type)}};
}

static Pin pin_from_json(const json& j) {
    Pin p;
    p.id = j.value("id", 0);
    p.name = j.value("name", "");
    p.conn_type = static_cast<ConnectionType>(j.value("conn_type", 0));
    p.color = j.value("color", (unsigned int)connection_type_color(p.conn_type));
    return p;
}

static json node_to_json(const NodeInstance& n) {
    json params_j;
    for (auto& [k, v] : n.params) {
        params_j[k] = param_to_json(v);
    }

    json inputs_j = json::array();
    for (auto& p : n.inputs) inputs_j.push_back(pin_to_json(p));

    json outputs_j = json::array();
    for (auto& p : n.outputs) outputs_j.push_back(pin_to_json(p));

    return {
        {"id", n.id}, {"type", n.type}, {"label", n.label},
        {"position", {{"x", n.position.x}, {"y", n.position.y}}},
        {"inputs", inputs_j}, {"outputs", outputs_j},
        {"params", params_j}, {"status", n.status},
        {"result", n.result}, {"error", n.error}
    };
}

static NodeInstance node_from_json(const json& j) {
    NodeInstance n;
    n.id = j.value("id", 0);
    n.type = j.value("type", "");
    n.label = j.value("label", "");
    if (j.contains("position")) {
        n.position.x = j["position"].value("x", 0.0f);
        n.position.y = j["position"].value("y", 0.0f);
    }
    if (j.contains("inputs")) {
        for (auto& p : j["inputs"]) n.inputs.push_back(pin_from_json(p));
    }
    if (j.contains("outputs")) {
        for (auto& p : j["outputs"]) n.outputs.push_back(pin_from_json(p));
    }
    if (j.contains("params") && j["params"].is_object()) {
        for (auto& [k, v] : j["params"].items()) {
            n.params[k] = param_from_json(v);
        }
    }
    n.status = j.value("status", "idle");
    n.result = j.value("result", "");
    n.error = j.value("error", "");
    return n;
}

static json link_to_json(const Link& l) {
    return {{"id", l.id}, {"start_pin", l.start_pin}, {"end_pin", l.end_pin}};
}

static Link link_from_json(const json& j) {
    Link l;
    l.id = j.value("id", 0);
    l.start_pin = j.value("start_pin", 0);
    l.end_pin = j.value("end_pin", 0);
    return l;
}

// ============================================================================
// Workflow to_json / from_json
// ============================================================================
json Workflow::to_json() const {
    json nodes_j = json::array();
    for (auto& n : nodes) nodes_j.push_back(node_to_json(n));

    json links_j = json::array();
    for (auto& l : links) links_j.push_back(link_to_json(l));

    return {
        {"id", id}, {"name", name}, {"description", description},
        {"status", status}, {"nodes", nodes_j}, {"links", links_j},
        {"created_at", created_at}, {"updated_at", updated_at}
    };
}

Workflow Workflow::from_json(const json& j) {
    Workflow wf;
    wf.id = j.value("id", "");
    wf.name = j.value("name", "");
    wf.description = j.value("description", "");
    wf.status = j.value("status", "new");
    wf.created_at = j.value("created_at", "");
    wf.updated_at = j.value("updated_at", "");

    if (j.contains("nodes")) {
        for (auto& n : j["nodes"]) wf.nodes.push_back(node_from_json(n));
    }
    if (j.contains("links")) {
        for (auto& l : j["links"]) wf.links.push_back(link_from_json(l));
    }
    return wf;
}

// ============================================================================
// Auto-save current canvas
// ============================================================================
void WorkflowStore::save_current(const std::vector<NodeInstance>& nodes, const std::vector<Link>& links) {
    json nodes_j = json::array();
    for (auto& n : nodes) nodes_j.push_back(node_to_json(n));

    json links_j = json::array();
    for (auto& l : links) links_j.push_back(link_to_json(l));

    json data = {{"nodes", nodes_j}, {"links", links_j}};
    db::ops::save_setting("nodeEditorWorkflow", data.dump(), "node_editor");
}

bool WorkflowStore::load_current(std::vector<NodeInstance>& nodes, std::vector<Link>& links) {
    auto stored = db::ops::get_setting("nodeEditorWorkflow");
    if (!stored.has_value() || stored->empty()) return false;

    try {
        auto j = json::parse(*stored);
        nodes.clear();
        links.clear();
        if (j.contains("nodes")) {
            for (auto& n : j["nodes"]) nodes.push_back(node_from_json(n));
        }
        if (j.contains("links")) {
            for (auto& l : j["links"]) links.push_back(link_from_json(l));
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("NodeEditor", "Failed to load canvas: %s", e.what());
        return false;
    }
}

// ============================================================================
// Workflow CRUD
// ============================================================================
static std::string now_iso() {
    time_t t = time(nullptr);
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

void WorkflowStore::save_workflow(const Workflow& wf) {
    // Load existing list
    auto list = list_workflows();

    // Update or append
    bool found = false;
    for (auto& existing : list) {
        if (existing.id == wf.id) {
            existing = wf;
            existing.updated_at = now_iso();
            found = true;
            break;
        }
    }
    if (!found) {
        Workflow copy = wf;
        if (copy.id.empty()) copy.id = "wf_" + std::to_string(time(nullptr));
        if (copy.created_at.empty()) copy.created_at = now_iso();
        copy.updated_at = now_iso();
        list.push_back(std::move(copy));
    }

    // Serialize entire list
    json arr = json::array();
    for (auto& w : list) arr.push_back(w.to_json());
    db::ops::save_setting("nodeEditorWorkflows", arr.dump(), "node_editor");

    LOG_INFO("NodeEditor", "Saved workflow '%s' (%zu total)", wf.name.c_str(), list.size());
}

std::vector<Workflow> WorkflowStore::list_workflows() {
    std::vector<Workflow> result;
    auto stored = db::ops::get_setting("nodeEditorWorkflows");
    if (!stored.has_value() || stored->empty()) return result;

    try {
        auto arr = json::parse(*stored);
        if (!arr.is_array()) return result;
        for (auto& j : arr) {
            result.push_back(Workflow::from_json(j));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("NodeEditor", "Failed to load workflows: %s", e.what());
    }
    return result;
}

bool WorkflowStore::load_workflow(const std::string& id, Workflow& wf) {
    auto list = list_workflows();
    for (auto& w : list) {
        if (w.id == id) {
            wf = w;
            return true;
        }
    }
    return false;
}

void WorkflowStore::delete_workflow(const std::string& id) {
    auto list = list_workflows();
    list.erase(
        std::remove_if(list.begin(), list.end(), [&](const Workflow& w) { return w.id == id; }),
        list.end());

    json arr = json::array();
    for (auto& w : list) arr.push_back(w.to_json());
    db::ops::save_setting("nodeEditorWorkflows", arr.dump(), "node_editor");

    LOG_INFO("NodeEditor", "Deleted workflow '%s'", id.c_str());
}

// ============================================================================
// Import / Export
// ============================================================================
std::string WorkflowStore::export_json(const std::vector<NodeInstance>& nodes, const std::vector<Link>& links) {
    json nodes_j = json::array();
    for (auto& n : nodes) nodes_j.push_back(node_to_json(n));
    json links_j = json::array();
    for (auto& l : links) links_j.push_back(link_to_json(l));
    json data = {{"nodes", nodes_j}, {"links", links_j}};
    return data.dump(2);
}

bool WorkflowStore::import_json(const std::string& json_str, std::vector<NodeInstance>& nodes, std::vector<Link>& links) {
    try {
        auto j = json::parse(json_str);
        nodes.clear();
        links.clear();
        if (j.contains("nodes")) {
            for (auto& n : j["nodes"]) nodes.push_back(node_from_json(n));
        }
        if (j.contains("links")) {
            for (auto& l : j["links"]) links.push_back(link_from_json(l));
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("NodeEditor", "Import failed: %s", e.what());
        return false;
    }
}

// ============================================================================
// Filtered lists
// ============================================================================
std::vector<Workflow> WorkflowStore::list_drafts() {
    auto all = list_workflows();
    std::vector<Workflow> result;
    for (auto& w : all) {
        if (w.status == "draft" || w.status == "new") result.push_back(w);
    }
    return result;
}

std::vector<Workflow> WorkflowStore::list_deployed() {
    auto all = list_workflows();
    std::vector<Workflow> result;
    for (auto& w : all) {
        if (w.status == "deployed") result.push_back(w);
    }
    return result;
}

// ============================================================================
// Workflow operations
// ============================================================================
Workflow WorkflowStore::duplicate_workflow(const Workflow& wf) {
    Workflow copy = wf;
    copy.id = "wf_" + std::to_string(time(nullptr)) + "_dup";
    copy.name = wf.name + " (copy)";
    copy.status = "draft";
    copy.created_at = now_iso();
    copy.updated_at = now_iso();
    save_workflow(copy);
    return copy;
}

void WorkflowStore::update_status(const std::string& id, const std::string& new_status) {
    auto list = list_workflows();
    for (auto& w : list) {
        if (w.id == id) {
            w.status = new_status;
            w.updated_at = now_iso();
            break;
        }
    }
    json arr = json::array();
    for (auto& w : list) arr.push_back(w.to_json());
    db::ops::save_setting("nodeEditorWorkflows", arr.dump(), "node_editor");
}

void WorkflowStore::bulk_delete(const std::vector<std::string>& ids) {
    auto list = list_workflows();
    list.erase(
        std::remove_if(list.begin(), list.end(), [&](const Workflow& w) {
            return std::find(ids.begin(), ids.end(), w.id) != ids.end();
        }),
        list.end());

    json arr = json::array();
    for (auto& w : list) arr.push_back(w.to_json());
    db::ops::save_setting("nodeEditorWorkflows", arr.dump(), "node_editor");

    LOG_INFO("NodeEditor", "Bulk deleted %zu workflows", ids.size());
}

} // namespace fincept::node_editor
