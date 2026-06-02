#pragma once

#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
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

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void apply_background();
    void retranslateUi();

    QLineEdit* name_edit_ = nullptr;
    QLabel* status_badge_ = nullptr;
    QPushButton* undo_btn_ = nullptr;
    QPushButton* redo_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* load_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QPushButton* import_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QPushButton* templates_btn_ = nullptr;
    QPushButton* deploy_btn_ = nullptr;
    QPushButton* execute_btn_ = nullptr;
};

} // namespace fincept::workflow
