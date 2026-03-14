// Node Editor — Editor state implementation

#include "editor_state.h"
#include "core/logger.h"
#include <algorithm>

namespace fincept::node_editor {

int EditorState::alloc_id() {
    return next_id_++;
}

NodeInstance& EditorState::add_node(const NodeDef& def, ImVec2 pos) {
    push_undo();

    NodeInstance node;
    node.id = alloc_id();
    node.type = def.type;
    node.label = def.label;
    node.position = pos;

    // Create input pins with connection types
    for (int i = 0; i < def.num_inputs; i++) {
        Pin pin;
        pin.id = alloc_id();
        pin.conn_type = def.input_type(i);
        pin.name = (i < (int)def.input_defs.size() && !def.input_defs[i].name.empty())
            ? def.input_defs[i].name : "in_" + std::to_string(i);
        pin.color = connection_type_color(pin.conn_type);
        node.inputs.push_back(pin);
    }

    // Create output pins with connection types
    for (int i = 0; i < def.num_outputs; i++) {
        Pin pin;
        pin.id = alloc_id();
        pin.conn_type = def.output_type(i);
        pin.name = (i < (int)def.output_defs.size() && !def.output_defs[i].name.empty())
            ? def.output_defs[i].name : "out_" + std::to_string(i);
        pin.color = connection_type_color(pin.conn_type);
        node.outputs.push_back(pin);
    }

    // Set default parameter values
    for (auto& param : def.default_params) {
        node.params[param.name] = param.default_value;
    }

    nodes_.push_back(std::move(node));
    dirty_ = true;

    LOG_INFO("NodeEditor", "Added node '%s' (id=%d) at (%.0f, %.0f)",
             def.label.c_str(), nodes_.back().id, pos.x, pos.y);

    return nodes_.back();
}

void EditorState::remove_node(int node_id) {
    push_undo();

    // Remove links connected to this node's pins
    for (auto& node : nodes_) {
        if (node.id != node_id) continue;

        links_.erase(
            std::remove_if(links_.begin(), links_.end(), [&](const Link& l) {
                for (auto& pin : node.inputs)
                    if (pin.id == l.end_pin) return true;
                for (auto& pin : node.outputs)
                    if (pin.id == l.start_pin) return true;
                return false;
            }),
            links_.end());
        break;
    }

    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
            [node_id](const NodeInstance& n) { return n.id == node_id; }),
        nodes_.end());

    if (selected_node_ == node_id) selected_node_ = -1;
    dirty_ = true;
}

NodeInstance* EditorState::find_node(int node_id) {
    for (auto& n : nodes_) {
        if (n.id == node_id) return &n;
    }
    return nullptr;
}

NodeInstance* EditorState::find_node_by_pin(int pin_id) {
    for (auto& n : nodes_) {
        for (auto& p : n.inputs)
            if (p.id == pin_id) return &n;
        for (auto& p : n.outputs)
            if (p.id == pin_id) return &n;
    }
    return nullptr;
}

Link& EditorState::add_link(int start_pin, int end_pin) {
    push_undo();

    // Remove existing link to the same input pin (only one connection per input)
    links_.erase(
        std::remove_if(links_.begin(), links_.end(),
            [end_pin](const Link& l) { return l.end_pin == end_pin; }),
        links_.end());

    Link link;
    link.id = alloc_id();
    link.start_pin = start_pin;
    link.end_pin = end_pin;
    links_.push_back(link);
    dirty_ = true;

    LOG_DEBUG("NodeEditor", "Added link %d: pin %d -> pin %d", link.id, start_pin, end_pin);
    return links_.back();
}

void EditorState::remove_link(int link_id) {
    push_undo();
    links_.erase(
        std::remove_if(links_.begin(), links_.end(),
            [link_id](const Link& l) { return l.id == link_id; }),
        links_.end());
    dirty_ = true;
}

// ============================================================================
// Undo / Redo
// ============================================================================
void EditorState::push_undo() {
    // Truncate redo history
    if (undo_index_ < (int)undo_stack_.size() - 1) {
        undo_stack_.resize(undo_index_ + 1);
    }

    undo_stack_.push_back({nodes_, links_});

    if ((int)undo_stack_.size() > MAX_UNDO) {
        undo_stack_.erase(undo_stack_.begin());
    }

    undo_index_ = (int)undo_stack_.size() - 1;
}

void EditorState::undo() {
    if (!can_undo()) return;

    // Save current state as redo point if we're at the tip
    if (undo_index_ == (int)undo_stack_.size() - 1) {
        undo_stack_.push_back({nodes_, links_});
    }

    nodes_ = undo_stack_[undo_index_].nodes;
    links_ = undo_stack_[undo_index_].links;
    undo_index_--;
    dirty_ = true;
}

void EditorState::redo() {
    if (!can_redo()) return;

    undo_index_++;
    if (undo_index_ + 1 < (int)undo_stack_.size()) {
        nodes_ = undo_stack_[undo_index_ + 1].nodes;
        links_ = undo_stack_[undo_index_ + 1].links;
    }
    dirty_ = true;
}

bool EditorState::can_undo() const {
    return undo_index_ >= 0;
}

bool EditorState::can_redo() const {
    return undo_index_ + 1 < (int)undo_stack_.size() - 1;
}

void EditorState::clear() {
    push_undo();
    nodes_.clear();
    links_.clear();
    selected_node_ = -1;
    dirty_ = true;
}

void EditorState::load_from(std::vector<NodeInstance> nodes, std::vector<Link> links) {
    nodes_ = std::move(nodes);
    links_ = std::move(links);

    // Find max ID to avoid collisions
    int max_id = 0;
    for (auto& n : nodes_) {
        max_id = std::max(max_id, n.id);
        for (auto& p : n.inputs) max_id = std::max(max_id, p.id);
        for (auto& p : n.outputs) max_id = std::max(max_id, p.id);
    }
    for (auto& l : links_) {
        max_id = std::max(max_id, l.id);
    }
    next_id_ = max_id + 1;

    // Reset undo
    undo_stack_.clear();
    undo_index_ = -1;
    selected_node_ = -1;
    dirty_ = false;
}

} // namespace fincept::node_editor
