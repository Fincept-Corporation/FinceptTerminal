#pragma once
// Node Editor — Editor state: ID allocation, undo/redo, node/link management

#include "node_types.h"
#include "node_defs.h"

namespace fincept::node_editor {

class EditorState {
public:
    // ID allocation
    int alloc_id();

    // Node operations
    NodeInstance& add_node(const NodeDef& def, ImVec2 pos);
    void remove_node(int node_id);
    NodeInstance* find_node(int node_id);
    NodeInstance* find_node_by_pin(int pin_id);

    // Link operations
    Link& add_link(int start_pin, int end_pin);
    void remove_link(int link_id);

    // Undo/redo
    void push_undo();
    void undo();
    void redo();
    bool can_undo() const;
    bool can_redo() const;

    // Selection
    int selected_node_id() const { return selected_node_; }
    void set_selected(int node_id) { selected_node_ = node_id; }

    // Access
    std::vector<NodeInstance>& nodes() { return nodes_; }
    const std::vector<NodeInstance>& nodes() const { return nodes_; }
    std::vector<Link>& links() { return links_; }
    const std::vector<Link>& links() const { return links_; }

    // Clear everything
    void clear();

    // Restore from saved state (adjusts next_id_ to avoid collisions)
    void load_from(std::vector<NodeInstance> nodes, std::vector<Link> links);

    // Check if canvas is dirty (changed since last save)
    bool is_dirty() const { return dirty_; }
    void mark_clean() { dirty_ = false; }

private:
    std::vector<NodeInstance> nodes_;
    std::vector<Link> links_;
    int next_id_ = 1;
    int selected_node_ = -1;
    bool dirty_ = false;

    // Undo stack
    std::vector<EditorSnapshot> undo_stack_;
    int undo_index_ = -1;
    static constexpr int MAX_UNDO = 50;
};

} // namespace fincept::node_editor
