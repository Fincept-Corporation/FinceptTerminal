#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace fincept::workflow {

/// Top toolbar for the node editor with workflow name, status, and action buttons.
class NodeEditorToolbar : public QWidget {
    Q_OBJECT
  public:
    explicit NodeEditorToolbar(QWidget* parent = nullptr);

    void set_workflow_name(const QString& name);
    QString workflow_name() const;

    void set_can_undo(bool can);
    void set_can_redo(bool can);
    void set_executing(bool running);

  signals:
    void undo_clicked();
    void redo_clicked();
    void save_clicked();
    void load_clicked();
    void clear_clicked();
    void execute_clicked();
    void import_clicked();
    void export_clicked();
    void name_changed(const QString& name);
    void templates_clicked();
    void deploy_clicked();

  private:
    void build_ui();

    QLineEdit* name_edit_ = nullptr;
    QPushButton* undo_btn_ = nullptr;
    QPushButton* redo_btn_ = nullptr;
    QPushButton* execute_btn_ = nullptr;
};

} // namespace fincept::workflow
