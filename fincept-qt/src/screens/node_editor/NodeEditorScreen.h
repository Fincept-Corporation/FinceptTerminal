#pragma once

#include "screens/common/IStatefulScreen.h"
#include "screens/node_editor/NodeEditorTypes.h"

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

    // Clipboard for copy/paste
    QVector<NodeDef> clipboard_nodes_;
    QVector<EdgeDef> clipboard_edges_;
};

} // namespace fincept::workflow

// Convenience alias used in WindowFrame
namespace screens {
using NodeEditorScreen = fincept::workflow::NodeEditorScreen;
}
