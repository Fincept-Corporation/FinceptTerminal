#pragma once
// Workflow Templates — Pre-built workflow configurations
// Organized by difficulty: beginner, intermediate, advanced

#include "node_types.h"
#include <string>
#include <vector>

namespace fincept::node_editor {

struct WorkflowTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string category;      // "Beginner", "Intermediate", "Advanced"
    std::vector<std::string> tags;
    int node_count = 0;

    // Pre-built workflow content
    std::vector<NodeInstance> nodes;
    std::vector<Link> links;
};

class WorkflowTemplates {
public:
    static const std::vector<WorkflowTemplate>& get_all();
    static const WorkflowTemplate* find(const std::string& id);
    static std::vector<const WorkflowTemplate*> get_by_category(const std::string& category);
    static std::vector<const WorkflowTemplate*> search(const std::string& query);

private:
    static void init();
    static std::vector<WorkflowTemplate> templates_;
    static bool initialized_;
};

} // namespace fincept::node_editor
