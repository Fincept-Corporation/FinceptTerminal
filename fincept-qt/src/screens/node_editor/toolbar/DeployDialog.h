#pragma once

#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStringList>

namespace fincept::workflow {

/// "Save & Run" dialog: name, description and the workflow's EXECUTION MODE.
///
/// PAPER (the default) simulates every order node. LIVE lets order nodes whose own
/// Mode is "live" reach the broker; choosing it lists exactly which nodes and broker
/// accounts are involved and requires typing LIVE before either button enables.
class DeployDialog : public QDialog {
    Q_OBJECT
  public:
    /// `live_rows` — what would act for real if the workflow ran LIVE (from
    /// WorkflowService::live_action_rows(wf, true)), shown when LIVE is chosen.
    DeployDialog(const QString& current_name, const QString& current_description, bool currently_live,
                 const QStringList& live_rows, QWidget* parent = nullptr);

    QString workflow_name() const;
    QString workflow_description() const;
    bool is_deploy() const { return deploy_; }
    /// True when the user chose LIVE and typed the confirmation.
    bool live_mode() const;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui(const QString& current_name, const QString& current_description, bool currently_live);
    void retranslateUi();
    void update_mode_ui();

    QStringList live_rows_;
    QLabel* title_label_ = nullptr;
    QLabel* name_label_ = nullptr;
    QLabel* desc_label_ = nullptr;
    QLabel* mode_label_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QPlainTextEdit* desc_edit_ = nullptr;
    QRadioButton* paper_radio_ = nullptr;
    QRadioButton* live_radio_ = nullptr;
    QWidget* live_panel_ = nullptr;
    QLabel* live_warning_ = nullptr;
    QPlainTextEdit* live_list_ = nullptr;
    QLabel* confirm_label_ = nullptr;
    QLineEdit* confirm_edit_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* draft_btn_ = nullptr;
    QPushButton* deploy_btn_ = nullptr;
    bool deploy_ = false;
};

} // namespace fincept::workflow
