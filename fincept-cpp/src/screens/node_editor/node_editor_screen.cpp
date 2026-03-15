// Node Editor Screen — Main implementation
// Three-panel layout with imnodes canvas, node palette, and config panel

#include "node_editor_screen.h"
#include "workflow_store.h"
#include "node_defs.h"
#include "node_registry.h"
#include "workflow_executor.h"
#include "node_executor.h"
#include "workflow_templates.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <imnodes.h>
#include <algorithm>
#include <cstring>
#include <thread>

namespace fincept::node_editor {

using namespace theme::colors;

// ============================================================================
// Init
// ============================================================================
void NodeEditorScreen::init() {
    // Initialize node registry (all 82 node types)
    if (!NodeRegistry::instance().is_initialized()) {
        NodeRegistry::instance().init();
    }

    if (!imnodes_initialized_) {
        ImNodes::CreateContext();
        ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

        // Style matching Fincept dark theme
        auto& style = ImNodes::GetStyle();
        style.Colors[ImNodesCol_NodeBackground] = IM_COL32(15, 15, 17, 255);
        style.Colors[ImNodesCol_NodeBackgroundHovered] = IM_COL32(25, 25, 28, 255);
        style.Colors[ImNodesCol_NodeBackgroundSelected] = IM_COL32(30, 30, 35, 255);
        style.Colors[ImNodesCol_NodeOutline] = IM_COL32(42, 42, 42, 255);
        style.Colors[ImNodesCol_TitleBar] = IM_COL32(26, 26, 26, 255);
        style.Colors[ImNodesCol_TitleBarHovered] = IM_COL32(35, 35, 38, 255);
        style.Colors[ImNodesCol_TitleBarSelected] = IM_COL32(255, 136, 0, 180);
        style.Colors[ImNodesCol_Link] = IM_COL32(234, 88, 12, 255);
        style.Colors[ImNodesCol_LinkHovered] = IM_COL32(255, 136, 0, 255);
        style.Colors[ImNodesCol_LinkSelected] = IM_COL32(255, 165, 0, 255);
        style.Colors[ImNodesCol_Pin] = IM_COL32(16, 185, 129, 255);
        style.Colors[ImNodesCol_PinHovered] = IM_COL32(0, 229, 255, 255);
        style.Colors[ImNodesCol_GridBackground] = IM_COL32(10, 10, 10, 255);
        style.Colors[ImNodesCol_GridLine] = IM_COL32(45, 45, 45, 255);
        style.Colors[ImNodesCol_GridLinePrimary] = IM_COL32(60, 60, 60, 255);
        style.Colors[ImNodesCol_MiniMapBackground] = IM_COL32(15, 15, 17, 200);
        style.Colors[ImNodesCol_MiniMapOutline] = IM_COL32(255, 136, 0, 100);
        style.NodeCornerRounding = 3.0f;
        style.PinCircleRadius = 5.0f;
        style.LinkThickness = 2.5f;

        imnodes_initialized_ = true;
        LOG_INFO("NodeEditor", "imnodes context created");
    }

    // Load saved canvas
    std::vector<NodeInstance> nodes;
    std::vector<Link> links;
    if (WorkflowStore::load_current(nodes, links)) {
        state_.load_from(std::move(nodes), std::move(links));
        LOG_INFO("NodeEditor", "Loaded saved canvas: %zu nodes, %zu links",
                 state_.nodes().size(), state_.links().size());
    }

    initialized_ = true;
}

// ============================================================================
// Main render
// ============================================================================
void NodeEditorScreen::render() {
    if (!initialized_) init();

    ui::ScreenFrame frame("##node_editor_screen", ImVec2(4, 4), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    ImVec2 avail = frame.content_size();

    // Toolbar
    render_toolbar(avail.x);
    float toolbar_h = ImGui::GetCursorPosY();
    float content_h = avail.y - toolbar_h - 4;

    if (show_workflows_) {
        // Workflow manager view (full width)
        render_workflow_manager(avail.x, content_h);
    } else if (show_template_gallery_) {
        // Template gallery view
        render_template_gallery(avail.x, content_h);
    } else {
        // Three-panel editor layout
        auto sizes = ui::three_panel_layout(avail.x, content_h, 15, 22, 180, 220, 280, 4);

        // Left: Palette
        ImGui::BeginChild("##ne_palette", ImVec2(sizes.left_w, content_h), ImGuiChildFlags_Borders);
        render_palette(sizes.left_w, content_h);
        ImGui::EndChild();

        ImGui::SameLine(0, sizes.gap);

        // Center: Canvas
        ImGui::BeginChild("##ne_canvas", ImVec2(sizes.center_w, content_h), ImGuiChildFlags_Borders);
        render_canvas(sizes.center_w, content_h);
        ImGui::EndChild();

        // Right: Config panel (only if a node is selected)
        if (sizes.right_w > 0) {
            ImGui::SameLine(0, sizes.gap);
            ImGui::BeginChild("##ne_config", ImVec2(sizes.right_w, content_h), ImGuiChildFlags_Borders);
            render_config_panel(sizes.right_w, content_h);
            ImGui::EndChild();
        }
    }

    // Render modal dialogs
    render_deploy_dialog();
    render_results_modal();

    // Auto-save every 2 seconds when dirty
    if (state_.is_dirty() && ImGui::GetTime() - last_save_time_ > 2.0) {
        WorkflowStore::save_current(state_.nodes(), state_.links());
        state_.mark_clean();
        last_save_time_ = ImGui::GetTime();
    }

    frame.end();
}

// ============================================================================
// Toolbar
// ============================================================================
void NodeEditorScreen::render_toolbar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##ne_toolbar", ImVec2(width, ImGui::GetFrameHeight() + 8), ImGuiChildFlags_Borders);

    ImGui::TextColored(ACCENT, "NODE EDITOR");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "|");
    ImGui::SameLine();

    // View toggle
    {
        bool is_editor = !show_workflows_;
        if (is_editor) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        if (ImGui::SmallButton("Editor")) show_workflows_ = false;
        if (is_editor) ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 4);
    {
        bool is_wf = show_workflows_;
        if (is_wf) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        if (ImGui::SmallButton("Workflows")) {
            show_workflows_ = true;
            show_template_gallery_ = false;
            workflow_list_ = WorkflowStore::list_workflows();
        }
        if (is_wf) ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 4);
    {
        bool is_tpl = show_template_gallery_;
        if (is_tpl) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        if (ImGui::SmallButton("Templates")) {
            show_template_gallery_ = true;
            show_workflows_ = false;
        }
        if (is_tpl) ImGui::PopStyleColor();
    }

    if (!show_workflows_) {
        ImGui::SameLine(0, 20);

        // Execute
        if (executing_.load()) {
            theme::LoadingSpinner("Running...");
        } else {
            if (theme::AccentButton("Execute", ImVec2(0, 0))) {
                execute_workflow();
            }
        }

        ImGui::SameLine(0, 8);

        // Undo / Redo
        if (!state_.can_undo()) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Undo")) state_.undo();
        if (!state_.can_undo()) ImGui::EndDisabled();

        ImGui::SameLine(0, 4);

        if (!state_.can_redo()) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Redo")) state_.redo();
        if (!state_.can_redo()) ImGui::EndDisabled();

        ImGui::SameLine(0, 8);

        if (ImGui::SmallButton("Clear")) {
            state_.clear();
        }

        // Save/Deploy section on right side
        ImGui::SameLine(width - 360);

        ImGui::PushItemWidth(120);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputTextWithHint("##wf_name", "Workflow name", workflow_name_buf_, sizeof(workflow_name_buf_));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::SameLine(0, 4);
        if (theme::SecondaryButton("Save")) {
            if (std::strlen(workflow_name_buf_) > 0) {
                Workflow wf;
                wf.id = "wf_" + std::to_string(time(nullptr));
                wf.name = workflow_name_buf_;
                wf.description = workflow_desc_buf_;
                wf.status = "draft";
                wf.nodes = state_.nodes();
                wf.links = state_.links();
                WorkflowStore::save_workflow(wf);
            }
        }

        ImGui::SameLine(0, 4);
        if (theme::AccentButton("Deploy", ImVec2(0, 0))) {
            show_deploy_dialog_ = true;
        }

        ImGui::SameLine(0, 4);
        if (ImGui::SmallButton("Export")) {
            std::string json = WorkflowStore::export_json(state_.nodes(), state_.links());
            LOG_INFO("NodeEditor", "Exported %zu bytes", json.size());
            // TODO: file dialog
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ============================================================================
// Node Palette (Left Panel)
// ============================================================================
void NodeEditorScreen::render_palette(float /*width*/, float /*height*/) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    theme::SectionHeader("NODE PALETTE");

    // Search filter
    ImGui::PushItemWidth(-1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputTextWithHint("##palette_search", "Search nodes...", palette_filter_, sizeof(palette_filter_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::Spacing();

    std::string filter_lower;
    for (char c : std::string(palette_filter_))
        filter_lower += (char)std::tolower((unsigned char)c);

    // Group by category
    for (auto& cat : get_categories()) {
        bool has_nodes = false;

        // Check if any nodes match filter in this category
        for (auto& def : get_builtin_defs()) {
            if (def.category != cat.name) continue;
            if (!filter_lower.empty()) {
                std::string name_lower;
                for (char c : def.label) name_lower += (char)std::tolower((unsigned char)c);
                if (name_lower.find(filter_lower) == std::string::npos) continue;
            }
            has_nodes = true;
            break;
        }
        if (!has_nodes) continue;

        // Category header
        ImVec4 cat_col = ImGui::ColorConvertU32ToFloat4(cat.color);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(cat_col.x, cat_col.y, cat_col.z, 0.15f));
        bool open = ImGui::TreeNodeEx(cat.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor();

        if (open) {
            for (auto& def : get_builtin_defs()) {
                if (def.category != cat.name) continue;
                if (!filter_lower.empty()) {
                    std::string name_lower;
                    for (char c : def.label) name_lower += (char)std::tolower((unsigned char)c);
                    if (name_lower.find(filter_lower) == std::string::npos) continue;
                }

                // Color indicator + button
                ImVec4 col = ImGui::ColorConvertU32ToFloat4(def.color);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x, col.y, col.z, 0.3f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(col.x, col.y, col.z, 1.0f));

                if (ImGui::Button(def.label.c_str(), ImVec2(-1, 0))) {
                    // Add node at center of canvas
                    state_.add_node(def, ImVec2(300, 200));
                }

                ImGui::PopStyleColor(3);

                // Tooltip with description
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s\n%s", def.label.c_str(), def.description.c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::PopStyleColor();
}

// ============================================================================
// imnodes Canvas (Center Panel)
// ============================================================================
void NodeEditorScreen::render_canvas(float /*width*/, float /*height*/) {
    ImNodes::BeginNodeEditor();

    // Render all nodes
    for (auto& node : state_.nodes()) {
        render_node(node);
    }

    // Render all links (colored by source pin's connection type)
    for (auto& link : state_.links()) {
        auto* src_node = state_.find_node_by_pin(link.start_pin);
        ImU32 link_color = IM_COL32(234, 88, 12, 255); // default orange
        if (src_node) {
            for (auto& p : src_node->outputs) {
                if (p.id == link.start_pin) {
                    link_color = p.color;
                    break;
                }
            }
        }
        ImNodes::PushColorStyle(ImNodesCol_Link, link_color);
        ImNodes::Link(link.id, link.start_pin, link.end_pin);
        ImNodes::PopColorStyle();
    }

    // Minimap
    ImNodes::MiniMap(0.15f, ImNodesMiniMapLocation_BottomRight);

    ImNodes::EndNodeEditor();

    // Handle new link creation
    {
        int start_pin, end_pin;
        if (ImNodes::IsLinkCreated(&start_pin, &end_pin)) {
            // Validate: start must be output, end must be input
            auto* start_node = state_.find_node_by_pin(start_pin);
            auto* end_node = state_.find_node_by_pin(end_pin);

            if (start_node && end_node && start_node != end_node) {
                // Check pin types — start should be output, end should be input
                Pin* out_pin = nullptr;
                Pin* in_pin = nullptr;
                for (auto& p : start_node->outputs)
                    if (p.id == start_pin) out_pin = &p;
                for (auto& p : end_node->inputs)
                    if (p.id == end_pin) in_pin = &p;

                if (out_pin && in_pin) {
                    // Validate connection type compatibility
                    if (connection_types_compatible(out_pin->conn_type, in_pin->conn_type)) {
                        state_.add_link(start_pin, end_pin);
                    }
                } else {
                    // Try reversed (user dragged from input to output)
                    Pin* rev_out = nullptr;
                    Pin* rev_in = nullptr;
                    for (auto& p : end_node->outputs)
                        if (p.id == end_pin) rev_out = &p;
                    for (auto& p : start_node->inputs)
                        if (p.id == start_pin) rev_in = &p;
                    if (rev_out && rev_in &&
                        connection_types_compatible(rev_out->conn_type, rev_in->conn_type)) {
                        state_.add_link(end_pin, start_pin);
                    }
                }
            }
        }
    }

    // Handle link destruction
    {
        int link_id;
        if (ImNodes::IsLinkDestroyed(&link_id)) {
            state_.remove_link(link_id);
        }
    }

    // Handle node selection
    {
        int num_selected = ImNodes::NumSelectedNodes();
        if (num_selected == 1) {
            int selected_id;
            ImNodes::GetSelectedNodes(&selected_id);
            state_.set_selected(selected_id);

            // Double-click opens results modal
            if (ImGui::IsMouseDoubleClicked(0)) {
                auto* sel_node = state_.find_node(selected_id);
                if (sel_node && (sel_node->status == "completed" || sel_node->status == "error")) {
                    results_modal_node_id_ = selected_id;
                    show_results_modal_ = true;
                }
            }
        } else if (num_selected == 0) {
            state_.set_selected(-1);
        }
    }

    // Delete key removes selected nodes
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        int num = ImNodes::NumSelectedNodes();
        if (num > 0) {
            std::vector<int> selected(num);
            ImNodes::GetSelectedNodes(selected.data());
            for (int id : selected) {
                state_.remove_node(id);
            }
            ImNodes::ClearNodeSelection();
        }

        int num_links = ImNodes::NumSelectedLinks();
        if (num_links > 0) {
            std::vector<int> sel_links(num_links);
            ImNodes::GetSelectedLinks(sel_links.data());
            for (int id : sel_links) {
                state_.remove_link(id);
            }
            ImNodes::ClearLinkSelection();
        }
    }

    // Keyboard shortcuts
    if (ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) state_.undo();
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) state_.redo();
        if (ImGui::IsKeyPressed(ImGuiKey_S)) {
            WorkflowStore::save_current(state_.nodes(), state_.links());
            state_.mark_clean();
            last_save_time_ = ImGui::GetTime();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E) && !executing_.load()) execute_workflow();
        if (ImGui::IsKeyPressed(ImGuiKey_D)) {
            // Duplicate selected node
            int sel = state_.selected_node_id();
            auto* n = (sel >= 0) ? state_.find_node(sel) : nullptr;
            if (n) {
                const NodeDef* d = find_def(n->type);
                if (d) {
                    auto& dup = state_.add_node(*d, ImVec2(n->position.x + 40, n->position.y + 40));
                    dup.label = n->label + " (copy)";
                    dup.params = n->params;
                }
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_A)) {
            ImNodes::ClearNodeSelection();
            for (auto& node : state_.nodes()) {
                ImNodes::SelectNode(node.id);
            }
        }
    }

    // Update node positions from imnodes
    for (auto& node : state_.nodes()) {
        ImVec2 pos = ImNodes::GetNodeEditorSpacePos(node.id);
        if (pos.x != node.position.x || pos.y != node.position.y) {
            node.position = pos;
            // Don't push undo for drag (too many snapshots)
        }
    }
}

// ============================================================================
// Render individual node
// ============================================================================
void NodeEditorScreen::render_node(NodeInstance& node) {
    const NodeDef* def = find_def(node.type);
    ImU32 node_color = def ? def->color : IM_COL32(128, 128, 128, 255);

    // Set node position (only on first frame or after load)
    ImNodes::SetNodeEditorSpacePos(node.id, node.position);

    // Node color for title bar
    ImNodes::PushColorStyle(ImNodesCol_TitleBar, node_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
        (node_color & 0x00FFFFFF) | 0xE0000000);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(255, 115, 13, 255));

    ImNodes::BeginNode(node.id);

    // Title bar
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(node.label.c_str());
    ImNodes::EndNodeTitleBar();

    // Status badge
    if (node.status == "running") {
        ImGui::TextColored(ImVec4(1, 0.53f, 0, 1), "[~] Running...");
    } else if (node.status == "completed") {
        ImGui::TextColored(ImVec4(0, 0.84f, 0.44f, 1), "[v] Done");
        // Show exec time if available
        if (!node.result.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, "(dbl-click)");
        }
    } else if (node.status == "error") {
        ImGui::TextColored(ImVec4(1, 0.23f, 0.23f, 1), "[x] Error");
    }

    // Input pins (colored by connection type)
    for (auto& pin : node.inputs) {
        ImNodes::PushColorStyle(ImNodesCol_Pin, pin.color);
        ImNodes::BeginInputAttribute(pin.id);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(pin.color), "%s",
            pin.name.empty() ? "In" : pin.name.c_str());
        ImNodes::EndInputAttribute();
        ImNodes::PopColorStyle();
    }

    // Show key parameter value (compact preview)
    if (!node.params.empty()) {
        auto it = node.params.begin();
        std::string preview = std::visit([](auto&& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) return v;
            else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
            else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
            else return std::to_string(v);
        }, it->second);

        if (preview.size() > 20) preview = preview.substr(0, 17) + "...";
        ImGui::TextColored(TEXT_DIM, "%s: %s", it->first.c_str(), preview.c_str());
    }

    // Output pins (colored by connection type)
    for (auto& pin : node.outputs) {
        ImNodes::PushColorStyle(ImNodesCol_Pin, pin.color);
        ImNodes::BeginOutputAttribute(pin.id);
        ImGui::Indent(40);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(pin.color), "%s",
            pin.name.empty() ? "Out" : pin.name.c_str());
        ImNodes::EndOutputAttribute();
        ImNodes::PopColorStyle();
    }

    ImNodes::EndNode();
    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

// ============================================================================
// Config Panel (Right)
// ============================================================================
void NodeEditorScreen::render_config_panel(float /*width*/, float /*height*/) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    int sel = state_.selected_node_id();
    NodeInstance* node = (sel >= 0) ? state_.find_node(sel) : nullptr;

    if (!node) {
        theme::SectionHeader("PROPERTIES");
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Select a node to edit.");
        ImGui::PopStyleColor();
        return;
    }

    const NodeDef* def = find_def(node->type);

    // Node header
    theme::SectionHeader("NODE CONFIG");
    ImGui::Spacing();

    // Label (editable)
    char label_buf[256];
    std::strncpy(label_buf, node->label.c_str(), sizeof(label_buf) - 1);
    label_buf[sizeof(label_buf) - 1] = '\0';
    ImGui::TextColored(TEXT_DIM, "Label:");
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##node_label", label_buf, sizeof(label_buf))) {
        node->label = label_buf;
    }
    ImGui::PopItemWidth();
    ImGui::Spacing();

    // Type info
    ImGui::TextColored(TEXT_DIM, "Type:");
    ImGui::SameLine();
    if (def) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(def->color), "%s", node->type.c_str());
    } else {
        ImGui::TextUnformatted(node->type.c_str());
    }

    if (def) {
        ImGui::TextColored(TEXT_DIM, "Category:");
        ImGui::SameLine();
        ImGui::TextUnformatted(def->category.c_str());
    }

    // Status
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Status:");
    ImGui::SameLine();
    if (node->status == "idle") ImGui::TextColored(TEXT_SECONDARY, "Idle");
    else if (node->status == "running") ImGui::TextColored(WARNING, "Running");
    else if (node->status == "completed") ImGui::TextColored(SUCCESS, "Completed");
    else if (node->status == "error") ImGui::TextColored(ERROR_RED, "Error");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Parameters
    theme::SectionHeader("PARAMETERS");
    ImGui::Spacing();

    if (def) {
        for (auto& param_def : def->default_params) {
            ImGui::PushID(param_def.name.c_str());

            // Label
            if (param_def.required) {
                ImGui::TextColored(WARNING, "%s *", param_def.display_name.c_str());
            } else {
                ImGui::TextColored(TEXT_DIM, "%s", param_def.display_name.c_str());
            }

            // Get or create param value
            auto it = node->params.find(param_def.name);
            if (it == node->params.end()) {
                node->params[param_def.name] = param_def.default_value;
                it = node->params.find(param_def.name);
            }

            ImGui::PushItemWidth(-1);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);

            if (param_def.type == "options" && !param_def.options.empty()) {
                // Dropdown
                std::string current = std::holds_alternative<std::string>(it->second)
                    ? std::get<std::string>(it->second) : "";
                if (ImGui::BeginCombo("##param", current.c_str())) {
                    for (auto& opt : param_def.options) {
                        bool selected = (opt == current);
                        if (ImGui::Selectable(opt.c_str(), selected)) {
                            it->second = opt;
                            state_.push_undo();
                        }
                    }
                    ImGui::EndCombo();
                }
            } else if (param_def.type == "number") {
                if (std::holds_alternative<int>(it->second)) {
                    int val = std::get<int>(it->second);
                    if (ImGui::InputInt("##param", &val)) {
                        it->second = val;
                    }
                } else if (std::holds_alternative<double>(it->second)) {
                    float val = (float)std::get<double>(it->second);
                    if (ImGui::InputFloat("##param", &val, 0, 0, "%.4f")) {
                        it->second = (double)val;
                    }
                }
            } else if (param_def.type == "boolean") {
                bool val = std::holds_alternative<bool>(it->second)
                    ? std::get<bool>(it->second) : false;
                if (ImGui::Checkbox("##param", &val)) {
                    it->second = val;
                }
            } else {
                // String input — use multiline for prompt/code/expression fields
                std::string val = std::holds_alternative<std::string>(it->second)
                    ? std::get<std::string>(it->second) : "";
                bool use_multiline = (param_def.name == "prompt" || param_def.name == "code" ||
                                      param_def.name == "system_prompt" || param_def.name == "expression" ||
                                      param_def.name == "message_template" || param_def.name == "condition");
                if (use_multiline) {
                    char buf[2048];
                    std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';
                    if (ImGui::InputTextMultiline("##param", buf, sizeof(buf), ImVec2(-1, 80))) {
                        it->second = std::string(buf);
                    }
                } else {
                    char buf[512];
                    std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';
                    if (ImGui::InputText("##param", buf, sizeof(buf))) {
                        it->second = std::string(buf);
                    }
                }
            }

            ImGui::PopStyleColor();
            ImGui::PopItemWidth();

            // Description tooltip
            if (!param_def.description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", param_def.description.c_str());
            }

            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    // Connections info
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    theme::SectionHeader("CONNECTIONS");
    ImGui::Spacing();

    if (!node->inputs.empty()) {
        ImGui::TextColored(TEXT_DIM, "Inputs:");
        for (auto& pin : node->inputs) {
            ImVec4 col = ImGui::ColorConvertU32ToFloat4(pin.color);
            ImGui::TextColored(col, "  %s (%s)", pin.name.c_str(),
                               connection_type_name(pin.conn_type));
        }
    }
    if (!node->outputs.empty()) {
        ImGui::TextColored(TEXT_DIM, "Outputs:");
        for (auto& pin : node->outputs) {
            ImVec4 col = ImGui::ColorConvertU32ToFloat4(pin.color);
            ImGui::TextColored(col, "  %s (%s)", pin.name.c_str(),
                               connection_type_name(pin.conn_type));
        }
    }

    // Actions
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    theme::SectionHeader("ACTIONS");

    // Test Node button — execute this node in isolation (background thread)
    if (theme::AccentButton("Test Node", ImVec2(-1, 0))) {
        node->status = "running";
        auto node_copy = *node;  // copy for thread safety
        int nid = node->id;
        std::thread([this, node_copy, nid]() {
            std::map<std::string, nlohmann::json> empty_inputs;
            auto result = NodeExecutor::execute(node_copy, empty_inputs);
            // Find the live node and update it
            for (auto& n : state_.nodes()) {
                if (n.id == nid) {
                    if (result.success) {
                        n.status = "completed";
                        n.result = result.display_text;
                        n.error.clear();
                    } else {
                        n.status = "error";
                        n.error = result.error;
                    }
                    break;
                }
            }
            execution_results_[nid] = result;
        }).detach();
    }

    ImGui::Spacing();

    if (ImGui::Button("Delete Node", ImVec2(-1, 0))) {
        state_.remove_node(node->id);
    }

    if (ImGui::Button("Duplicate", ImVec2(-1, 0))) {
        if (def) {
            auto& dup = state_.add_node(*def, ImVec2(node->position.x + 40, node->position.y + 40));
            dup.label = node->label + " (copy)";
            dup.params = node->params;
        }
    }

    // View Results button (if completed)
    if (node->status == "completed" || node->status == "error") {
        ImGui::Spacing();
        if (ImGui::Button("View Results", ImVec2(-1, 0))) {
            results_modal_node_id_ = node->id;
            show_results_modal_ = true;
        }
    }

    // Result display (inline preview)
    if (node->status == "completed" && !node->result.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        theme::SectionHeader("RESULT PREVIEW");
        ImGui::TextWrapped("%s", node->result.c_str());
    }

    if (node->status == "error" && !node->error.empty()) {
        ImGui::Spacing();
        theme::ErrorMessage(node->error.c_str());
    }

    ImGui::PopStyleColor();
}

// ============================================================================
// Workflow Manager View
// ============================================================================
void NodeEditorScreen::render_workflow_manager(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##wf_manager", ImVec2(width, height), ImGuiChildFlags_Borders);

    theme::SectionHeader("SAVED WORKFLOWS");
    ImGui::Spacing();

    // Refresh button
    if (theme::SecondaryButton("Refresh")) {
        workflow_list_ = WorkflowStore::list_workflows();
    }
    ImGui::Spacing();

    if (workflow_list_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No saved workflows. Create one in the editor.");
    }

    // Table
    if (!workflow_list_.empty() &&
        ImGui::BeginTable("##wf_table", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Nodes", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)workflow_list_.size(); i++) {
            auto& wf = workflow_list_[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(wf.name.c_str());

            ImGui::TableNextColumn();
            if (wf.status == "draft") ImGui::TextColored(WARNING, "Draft");
            else if (wf.status == "deployed") ImGui::TextColored(SUCCESS, "Deployed");
            else if (wf.status == "completed") ImGui::TextColored(ACCENT, "Completed");
            else ImGui::TextColored(TEXT_DIM, "%s", wf.status.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%zu", wf.nodes.size());

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", wf.created_at.c_str());

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Load")) {
                state_.load_from(wf.nodes, wf.links);
                std::strncpy(workflow_name_buf_, wf.name.c_str(), sizeof(workflow_name_buf_) - 1);
                show_workflows_ = false;
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            if (ImGui::SmallButton("Del")) {
                WorkflowStore::delete_workflow(wf.id);
                workflow_list_ = WorkflowStore::list_workflows();
            }
            ImGui::PopStyleColor();

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Template Gallery
// ============================================================================
void NodeEditorScreen::render_template_gallery(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##tpl_gallery", ImVec2(width, height), ImGuiChildFlags_Borders);

    theme::SectionHeader("WORKFLOW TEMPLATES");
    ImGui::Spacing();

    static char tpl_filter[128] = {};
    ImGui::PushItemWidth(300);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputTextWithHint("##tpl_search", "Search templates...", tpl_filter, sizeof(tpl_filter));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::Spacing();

    auto& templates = WorkflowTemplates::get_all();

    std::string filter_lower;
    for (char c : std::string(tpl_filter))
        filter_lower += (char)std::tolower((unsigned char)c);

    const char* categories[] = {"Beginner", "Intermediate", "Advanced"};
    ImVec4 cat_colors[] = {
        {0.25f, 0.84f, 0.42f, 1.0f},  // green
        {1.0f, 0.78f, 0.08f, 1.0f},   // yellow
        {0.96f, 0.30f, 0.30f, 1.0f},  // red
    };

    for (int ci = 0; ci < 3; ci++) {
        bool has_templates = false;
        for (auto& t : templates) {
            if (t.category != categories[ci]) continue;
            if (!filter_lower.empty()) {
                std::string name_lower;
                for (char c : t.name) name_lower += (char)std::tolower((unsigned char)c);
                std::string desc_lower;
                for (char c : t.description) desc_lower += (char)std::tolower((unsigned char)c);
                if (name_lower.find(filter_lower) == std::string::npos &&
                    desc_lower.find(filter_lower) == std::string::npos) continue;
            }
            has_templates = true;
            break;
        }
        if (!has_templates) continue;

        ImGui::TextColored(cat_colors[ci], "%s", categories[ci]);
        ImGui::Separator();
        ImGui::Spacing();

        for (auto& t : templates) {
            if (t.category != categories[ci]) continue;
            if (!filter_lower.empty()) {
                std::string name_lower;
                for (char c : t.name) name_lower += (char)std::tolower((unsigned char)c);
                std::string desc_lower;
                for (char c : t.description) desc_lower += (char)std::tolower((unsigned char)c);
                if (name_lower.find(filter_lower) == std::string::npos &&
                    desc_lower.find(filter_lower) == std::string::npos) continue;
            }

            ImGui::PushID(t.id.c_str());

            ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
            ImGui::BeginChild("##tpl_card", ImVec2(width - 40, 90), ImGuiChildFlags_Borders);

            ImGui::TextColored(ACCENT, "%s", t.name.c_str());
            ImGui::TextColored(TEXT_DIM, "%s", t.description.c_str());

            ImGui::Spacing();

            // Tags
            for (auto& tag : t.tags) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, BG_INPUT);
                ImGui::SmallButton(tag.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::SameLine(width - 160);
            ImGui::TextColored(TEXT_DIM, "%d nodes", t.node_count);

            ImGui::SameLine(width - 100);
            if (theme::AccentButton("Load", ImVec2(60, 0))) {
                // Load template into editor
                state_.load_from(t.nodes, t.links);
                std::strncpy(workflow_name_buf_, t.name.c_str(), sizeof(workflow_name_buf_) - 1);
                show_template_gallery_ = false;
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PopID();
        }

        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Deploy Dialog (Modal)
// ============================================================================
void NodeEditorScreen::render_deploy_dialog() {
    if (!show_deploy_dialog_) return;

    ImGui::OpenPopup("Deploy Workflow");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 300));

    if (ImGui::BeginPopupModal("Deploy Workflow", &show_deploy_dialog_,
                                ImGuiWindowFlags_NoResize)) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

        theme::SectionHeader("WORKFLOW DETAILS");
        ImGui::Spacing();

        ImGui::TextColored(TEXT_DIM, "Name:");
        ImGui::PushItemWidth(-1);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##deploy_name", workflow_name_buf_, sizeof(workflow_name_buf_));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Description:");
        ImGui::PushItemWidth(-1);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputTextMultiline("##deploy_desc", workflow_desc_buf_, sizeof(workflow_desc_buf_),
                                   ImVec2(-1, 80));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::Spacing();

        // Stats
        ImGui::TextColored(TEXT_DIM, "Nodes: %zu  |  Links: %zu",
                          state_.nodes().size(), state_.links().size());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        float button_width = 130;

        if (theme::SecondaryButton("Save as Draft", ImVec2(button_width, 0))) {
            if (std::strlen(workflow_name_buf_) > 0) {
                Workflow wf;
                wf.id = "wf_" + std::to_string(time(nullptr));
                wf.name = workflow_name_buf_;
                wf.description = workflow_desc_buf_;
                wf.status = "draft";
                wf.nodes = state_.nodes();
                wf.links = state_.links();
                WorkflowStore::save_workflow(wf);
                show_deploy_dialog_ = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine(0, 8);

        if (theme::AccentButton("Deploy", ImVec2(button_width, 0))) {
            if (std::strlen(workflow_name_buf_) > 0) {
                Workflow wf;
                wf.id = "wf_" + std::to_string(time(nullptr));
                wf.name = workflow_name_buf_;
                wf.description = workflow_desc_buf_;
                wf.status = "deployed";
                wf.nodes = state_.nodes();
                wf.links = state_.links();
                WorkflowStore::save_workflow(wf);
                show_deploy_dialog_ = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine(0, 8);

        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            show_deploy_dialog_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
}

// ============================================================================
// Results Modal — click a completed node to see full results
// ============================================================================
void NodeEditorScreen::render_results_modal() {
    if (!show_results_modal_) return;

    ImGui::OpenPopup("Node Results");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 450));

    if (ImGui::BeginPopupModal("Node Results", &show_results_modal_,
                                ImGuiWindowFlags_NoResize)) {

        auto* node = state_.find_node(results_modal_node_id_);
        auto result_it = execution_results_.find(results_modal_node_id_);

        if (node) {
            // Header
            const NodeDef* def = find_def(node->type);
            if (def) {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(def->color),
                                   "%s", node->label.c_str());
            } else {
                ImGui::Text("%s", node->label.c_str());
            }
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, "(%s)", node->type.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Status
            ImGui::TextColored(TEXT_DIM, "Status:");
            ImGui::SameLine();
            if (node->status == "completed") ImGui::TextColored(SUCCESS, "Completed");
            else if (node->status == "error") ImGui::TextColored(ERROR_RED, "Error");
            else ImGui::TextUnformatted(node->status.c_str());

            // Execution time
            if (result_it != execution_results_.end()) {
                ImGui::SameLine(0, 20);
                ImGui::TextColored(TEXT_DIM, "Time: %.1fms", result_it->second.exec_time_ms);
            }

            ImGui::Spacing();

            // Result display
            if (result_it != execution_results_.end()) {
                auto& res = result_it->second;

                if (res.success) {
                    // Display text
                    if (!res.display_text.empty()) {
                        theme::SectionHeader("RESULT");
                        ImGui::TextWrapped("%s", res.display_text.c_str());
                        ImGui::Spacing();
                    }

                    // JSON data
                    theme::SectionHeader("OUTPUT DATA");
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
                    ImGui::BeginChild("##result_json", ImVec2(-1, 200), ImGuiChildFlags_Borders);
                    std::string json_str = res.data.dump(2);
                    ImGui::TextWrapped("%s", json_str.c_str());
                    ImGui::EndChild();
                    ImGui::PopStyleColor();

                    ImGui::Spacing();

                    // Copy button
                    if (ImGui::Button("Copy JSON to Clipboard", ImVec2(-1, 0))) {
                        ImGui::SetClipboardText(json_str.c_str());
                    }
                } else {
                    // Error display
                    theme::SectionHeader("ERROR");
                    ImGui::TextColored(ERROR_RED, "%s", res.error.c_str());
                }
            } else if (!node->result.empty()) {
                theme::SectionHeader("RESULT");
                ImGui::TextWrapped("%s", node->result.c_str());
            } else if (!node->error.empty()) {
                theme::SectionHeader("ERROR");
                ImGui::TextColored(ERROR_RED, "%s", node->error.c_str());
            } else {
                ImGui::TextColored(TEXT_DIM, "No execution results available.");
            }
        } else {
            ImGui::TextColored(TEXT_DIM, "Node not found.");
        }

        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(-1, 0))) {
            show_results_modal_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// Workflow Execution — Real engine with typed dispatch
// ============================================================================
void NodeEditorScreen::execute_workflow() {
    if (executing_.load()) return;
    if (state_.nodes().empty()) return;

    executing_ = true;

    std::thread([this]() {
        WorkflowExecutor executor;
        executor.set_executor(NodeExecutor::get_executor_fn());
        executor.set_status_callback([](int /*node_id*/, const std::string& /*status*/,
                                         const std::string& /*result*/, const std::string& /*error*/) {
            // Status is updated directly on nodes by the executor
        });

        executor.execute(state_.nodes(), state_.links());

        // Store results for the results modal
        execution_results_ = executor.results();

        LOG_INFO("NodeEditor", "Workflow execution completed (%zu nodes)", state_.nodes().size());
        executing_ = false;
    }).detach();
}

} // namespace fincept::node_editor
