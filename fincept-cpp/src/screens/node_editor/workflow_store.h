#pragma once
// Node Editor — Workflow persistence via SQLite
// Saves/loads workflows using existing Database singleton

#include "node_types.h"

namespace fincept::node_editor {

class WorkflowStore {
public:
    // Auto-save current canvas state
    static void save_current(const std::vector<NodeInstance>& nodes, const std::vector<Link>& links);
    static bool load_current(std::vector<NodeInstance>& nodes, std::vector<Link>& links);

    // Workflow CRUD
    static void save_workflow(const Workflow& wf);
    static std::vector<Workflow> list_workflows();
    static bool load_workflow(const std::string& id, Workflow& wf);
    static void delete_workflow(const std::string& id);

    // Filtered lists
    static std::vector<Workflow> list_drafts();
    static std::vector<Workflow> list_deployed();

    // Workflow operations
    static Workflow duplicate_workflow(const Workflow& wf);
    static void update_status(const std::string& id, const std::string& new_status);
    static void bulk_delete(const std::vector<std::string>& ids);

    // Import/Export JSON
    static std::string export_json(const std::vector<NodeInstance>& nodes, const std::vector<Link>& links);
    static bool import_json(const std::string& json_str, std::vector<NodeInstance>& nodes, std::vector<Link>& links);
};

} // namespace fincept::node_editor
