#pragma once
// Node Editor Screen — Visual workflow editor using imnodes
// Layout: [Left: Node Palette] [Center: imnodes Canvas] [Right: Config Panel]

#include "editor_state.h"
#include "workflow_executor.h"
#include "node_types.h"
#include <string>
#include <atomic>
#include <map>

namespace fincept::node_editor {

class NodeEditorScreen {
public:
    void render();

private:
    void init();

    // Panel renderers
    void render_toolbar(float width);
    void render_palette(float width, float height);
    void render_canvas(float width, float height);
    void render_config_panel(float width, float height);
    void render_workflow_manager(float width, float height);
    void render_template_gallery(float width, float height);
    void render_deploy_dialog();
    void render_results_modal();

    // Node rendering inside imnodes
    void render_node(NodeInstance& node);

    // Execution
    void execute_workflow();

    bool initialized_ = false;
    bool imnodes_initialized_ = false;

    // Editor state
    EditorState state_;

    // UI state
    bool show_workflows_ = false;  // toggle: editor vs workflow list
    char palette_filter_[128] = {};
    char workflow_name_buf_[256] = {};
    char workflow_desc_buf_[512] = {};

    // Execution
    std::atomic<bool> executing_{false};

    // Auto-save
    double last_save_time_ = 0;

    // Execution results (for results modal)
    std::map<int, ExecutionResult> execution_results_;

    // Results modal state
    bool show_results_modal_ = false;
    int results_modal_node_id_ = -1;

    // Template gallery state
    bool show_template_gallery_ = false;

    // Deploy dialog state
    bool show_deploy_dialog_ = false;

    // Cached workflow list (for manager view)
    std::vector<Workflow> workflow_list_;
    double last_list_refresh_ = 0;
};

} // namespace fincept::node_editor
