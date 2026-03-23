#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::workflow {

/// Right panel showing editable properties for the selected node.
class NodePropertiesPanel : public QWidget {
    Q_OBJECT
  public:
    explicit NodePropertiesPanel(QWidget* parent = nullptr);

    /// Show properties for a node. Pass null type_def to show empty state.
    void show_properties(const NodeDef* node, const NodeTypeDef* type_def);

    /// Clear and show empty state.
    void clear();

  signals:
    void param_changed(const QString& node_id, const QString& key, QJsonValue value);
    void name_changed(const QString& node_id, const QString& new_name);
    void delete_requested(const QString& node_id);

  private:
    void build_ui();
    void build_editor(const NodeDef& node, const NodeTypeDef& type_def);

    QStackedWidget* stack_ = nullptr;
    QWidget* empty_page_ = nullptr;
    QWidget* editor_page_ = nullptr;
    QVBoxLayout* editor_layout_ = nullptr;
    QString current_node_id_;
};

} // namespace fincept::workflow
