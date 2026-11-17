#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QCheckBox>
#include <QEvent>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
}

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

    /// Id of the node whose properties are shown (empty when none).
    QString current_node_id() const { return current_node_id_; }

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    void param_changed(const QString& node_id, const QString& key, QJsonValue value);
    void name_changed(const QString& node_id, const QString& new_name);
    void delete_requested(const QString& node_id);
    /// Node-level execution flags from the SETTINGS block. `flag` is "disabled",
    /// "continue_on_fail" or "retry_on_fail". Previously these checkboxes were rendered but
    /// never connected, so toggling "Disabled" silently did nothing and the node
    /// still executed.
    void node_flag_changed(const QString& node_id, const QString& flag, bool value);
    /// "Max tries" of the Retry on fail setting (counts the first attempt).
    void node_max_tries_changed(const QString& node_id, int max_tries);

  private:
    void build_ui();
    void build_editor(const NodeDef& node, const NodeTypeDef& type_def);
    void retranslateUi();
    /// Texts of the "nothing selected" state (also on a language change).
    void apply_empty_texts();

    QStackedWidget* stack_ = nullptr;
    QWidget* empty_page_ = nullptr;
    QWidget* editor_page_ = nullptr;
    QVBoxLayout* editor_layout_ = nullptr;
    QString current_node_id_;

    // "No node selected" (compact shared EmptyState; texts re-applied by retranslateUi).
    fincept::ui::EmptyState* empty_state_ = nullptr;

    // Dynamic editor labels — recreated by build_editor, guarded with QPointer
    // because they are deleteLater()'d on each rebuild.
    QPointer<QLabel> props_title_ = nullptr;
    QPointer<QPushButton> del_btn_ = nullptr;
    QPointer<QLabel> name_label_ = nullptr;
    QPointer<QLabel> param_header_ = nullptr;
    QPointer<QLabel> settings_header_ = nullptr;
    QPointer<QCheckBox> disabled_check_ = nullptr;
    QPointer<QCheckBox> cof_check_ = nullptr;
    QPointer<QCheckBox> retry_check_ = nullptr;
    QPointer<QLabel> max_tries_label_ = nullptr;
    QPointer<QSpinBox> max_tries_spin_ = nullptr;
};

} // namespace fincept::workflow
