#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>

namespace fincept::workflow {

/// Dialog for deploying or saving a workflow as a draft.
class DeployDialog : public QDialog {
    Q_OBJECT
  public:
    explicit DeployDialog(const QString& current_name, QWidget* parent = nullptr);

    QString workflow_name() const;
    QString workflow_description() const;
    bool is_deploy() const { return deploy_; }

  private:
    void build_ui(const QString& current_name);

    QLineEdit* name_edit_ = nullptr;
    QPlainTextEdit* desc_edit_ = nullptr;
    bool deploy_ = false;
};

} // namespace fincept::workflow
