#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QLabel;

namespace fincept::ui {

/// Dialog for creating a new workspace — name, description, template selector.
class WorkspaceNewDialog : public QDialog {
    Q_OBJECT
  public:
    explicit WorkspaceNewDialog(QWidget* parent = nullptr);

    QString workspace_name() const;
    QString workspace_description() const;
    QString selected_template_id() const;

  private:
    void setup_ui();
    void update_preview(const QString& template_id);

    QLineEdit* name_edit_ = nullptr;
    QPlainTextEdit* description_edit_ = nullptr;
    QListWidget* template_list_ = nullptr;
    QLabel* preview_label_ = nullptr;
    QPushButton* create_btn_ = nullptr;
};

} // namespace fincept::ui
