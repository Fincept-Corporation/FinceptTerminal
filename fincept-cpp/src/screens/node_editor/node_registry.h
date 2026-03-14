#pragma once
// Node Registry — Singleton registry for all node definitions
// Supports registration, lookup, search, category grouping, and statistics

#include "node_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace fincept::node_editor {

class NodeRegistry {
public:
    static NodeRegistry& instance();

    // Registration
    void register_node(NodeDef def);

    // Lookup
    const NodeDef* get_node(const std::string& type) const;
    std::vector<const NodeDef*> get_by_category(const std::string& category) const;
    std::vector<const NodeDef*> search(const std::string& query) const;
    const std::vector<NodeDef>& get_all() const { return defs_; }
    const std::vector<CategoryDef>& get_categories() const { return categories_; }

    // Statistics
    size_t node_count() const { return defs_.size(); }
    size_t category_count() const { return categories_.size(); }

    // Initialization — registers all builtin nodes
    void init();
    bool is_initialized() const { return initialized_; }

    NodeRegistry(const NodeRegistry&) = delete;
    NodeRegistry& operator=(const NodeRegistry&) = delete;

private:
    NodeRegistry() = default;

    void register_category(const std::string& name, ImU32 color);
    void ensure_category(const std::string& name, ImU32 color);

    std::vector<NodeDef> defs_;
    std::unordered_map<std::string, size_t> type_index_; // type -> index in defs_
    std::vector<CategoryDef> categories_;
    std::unordered_map<std::string, size_t> cat_index_;  // name -> index in categories_
    bool initialized_ = false;
};

// Convenience free functions (backward-compatible wrappers)
inline const std::vector<NodeDef>& get_builtin_defs() {
    return NodeRegistry::instance().get_all();
}

inline const std::vector<CategoryDef>& get_categories() {
    return NodeRegistry::instance().get_categories();
}

inline const NodeDef* find_def(const std::string& type) {
    return NodeRegistry::instance().get_node(type);
}

} // namespace fincept::node_editor
