#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;

namespace fincept::ui {

/// Compact dialog for "Save Workspace As" — just a name field.
class WorkspaceSaveAsDialog : public QDialog {
    Q_OBJECT
  public:
    explicit WorkspaceSaveAsDialog(QWidget* parent = nullptr);

    QString new_name() const;
    QString chosen_path() const;

  private:
    void setup_ui();

    QLineEdit* name_edit_ = nullptr;
    QString chosen_path_;
};

} // namespace fincept::ui
