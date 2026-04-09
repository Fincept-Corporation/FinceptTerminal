#pragma once
#include <QDialog>
#include <QString>

class QListWidget;
class QLabel;
class QPushButton;

namespace fincept::ui {

/// Dialog for opening a saved workspace.
class WorkspaceOpenDialog : public QDialog {
    Q_OBJECT
  public:
    explicit WorkspaceOpenDialog(QWidget* parent = nullptr);

    /// Path of the selected workspace file, or empty if none.
    QString selected_path() const;

  private:
    void setup_ui();
    void load_workspaces();
    void browse_for_file();
    void update_preview(int row);

    QListWidget* workspace_list_ = nullptr;
    QLabel* preview_label_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QString selected_path_;
};

} // namespace fincept::ui
