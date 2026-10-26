#pragma once

#include "screens/common/IStatefulScreen.h"
#include "screens/node_editor/NodeEditorTypes.h"

#include <QHash>
#include <QPointF>
#include <QStringList>
#include <QTimer>
#include <QUndoStack>
#include <QWidget>

namespace fincept::workflow {

class NodeCanvas;
class NodeScene;
class NodePalette;
class NodePropertiesPanel;
class NodeEditorToolbar;
class NodeItem;
class MiniMap;
class ExecutionResultsPanel;

/// Main node editor screen — 3-panel layout with toolbar.
class NodeEditorScreen : public QWidget, public fincept::screens::IStatefulScreen {
    Q_OBJECT
  public:
    explicit NodeEditorScreen(QWidget* parent = nullptr);
    ~NodeEditorScreen() override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "node_editor"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

  private:
    void build_ui();
    void wire_signals();

    /// Ask before a run that can act for real. In LIVE mode, any node that would reach
    /// a broker requires typing LIVE (skipped when `already_confirmed`, i.e. the user
    /// just typed it in the Save & Run dialog). Destructive MCP tools get a plain
    /// confirmation in either mode. Returns true to proceed.
    bool confirm_run(const WorkflowDef& wf, const QString& action, bool already_confirmed);

    /// Pre-run validation: highlights every offending node (red border + tooltip),
    /// selects the first and explains. Returns true when the workflow may run.
    bool validate_before_run(const WorkflowDef& wf);

    /// The canvas as a WorkflowDef carrying this editor's id, name, description and
    /// static_data (execution mode included).
    WorkflowDef current_workflow() const;
    /// Persist the canvas. `interactive` = report a failure in a dialog (otherwise only
    /// on the status badge). Returns true on success.
    bool persist(bool interactive);
    /// Run (or partially run) the current canvas in its execution mode.
    void start_run(const QString& from_node_id, bool already_confirmed);
    /// Workflow-level execution mode (PAPER / LIVE), mirrored into static_data_.
    void set_live_mode(bool live);
    /// Something not on the undo stack changed (parameter, name, flags, rename).
    void mark_dirty();

    // Actions
    void on_node_drop(const QString& type_id, const QPointF& scene_pos);
    void on_node_selected(const QString& node_id);
    void on_param_changed(const QString& node_id, const QString& key, QJsonValue value);
    void on_name_changed(const QString& node_id, const QString& new_name);
    void on_delete_node(const QString& node_id);
    void on_save_workflow();
    void on_load_workflow();
    void on_clear_workflow();
    void on_import_workflow();
    void on_export_workflow();
    void on_execute();
    void on_auto_save();
    void on_show_templates();
    void on_deploy();
    void on_stop();

    // ── Undo plumbing ────────────────────────────────────────────────────────
    /// Push an AddNodesCommand for `nodes` (+ the edges among them).
    void push_add_nodes(const QVector<NodeDef>& nodes, const QVector<EdgeDef>& edges, const QString& label);
    /// Push a RemoveNodesCommand for `ids`, capturing their current defs and
    /// every edge that touches them so undo restores the wiring too.
    void push_remove_nodes(const QStringList& ids, const QString& label);
    /// Snapshot every node position (drag start) / diff against it (drag end).
    void begin_move_snapshot();
    void commit_move_snapshot();
    /// Discard undo history — mandatory whenever the graph is replaced wholesale
    /// from outside the editor (load / import / restore), because the queued
    /// commands refer to nodes that no longer exist.
    void reset_undo_history();

    // Components
    NodeEditorToolbar* toolbar_ = nullptr;
    NodePalette* palette_ = nullptr;
    NodeCanvas* canvas_ = nullptr;
    NodeScene* scene_ = nullptr;
    NodePropertiesPanel* properties_ = nullptr;

    QUndoStack* undo_stack_ = nullptr;
    QTimer* auto_save_timer_ = nullptr;
    MiniMap* minimap_ = nullptr;
    ExecutionResultsPanel* results_panel_ = nullptr;
    QString current_workflow_id_;
    QString current_description_;
    QJsonObject static_data_; // persisted with the workflow (execution mode lives here)
    bool live_mode_ = false;
    bool dirty_ = false;      // edits outside the undo stack since the last save
    QString pending_banner_;  // run-log banner for the run being started
    bool pending_banner_live_ = false;
    int paste_count_ = 0;     // successive pastes cascade instead of stacking

    // Clipboard for copy/paste
    QVector<NodeDef> clipboard_nodes_;
    QVector<EdgeDef> clipboard_edges_;

    // Positions captured at drag start, diffed at drag end into one undoable move.
    QHash<QString, QPointF> move_snapshot_;
};

} // namespace fincept::workflow

// Convenience alias used in WindowFrame
namespace screens {
using NodeEditorScreen = fincept::workflow::NodeEditorScreen;
}
