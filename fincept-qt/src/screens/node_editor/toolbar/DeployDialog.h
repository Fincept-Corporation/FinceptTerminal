#pragma once

#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>

namespace fincept::workflow {

/// Dialog for deploying or saving a workflow as a draft.
class DeployDialog : public QDialog {
    Q_OBJECT
  public:
    explicit DeployDialog(const QString& current_name, QWidget* parent = nullptr);

    QString workflow_name() const;
    QString workflow_description() const;
    bool is_deploy() const { return deploy_; }

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui(const QString& current_name);
    void retranslateUi();

    QLabel* title_label_ = nullptr;
    QLabel* name_label_ = nullptr;
    QLabel* desc_label_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QPlainTextEdit* desc_edit_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* draft_btn_ = nullptr;
    QPushButton* deploy_btn_ = nullptr;
    bool deploy_ = false;
};

} // namespace fincept::workflow
