#include "screens/node_editor/toolbar/DeployDialog.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::workflow {

static QString deploy_input_style() {
    return QString("background: %1; color: %2; border: 1px solid %3;"
                   "font-family: Consolas; font-size: 12px; padding: 6px 8px;")
        .arg(ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED());
}

DeployDialog::DeployDialog(const QString& current_name, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Save & Run Workflow"));
    setFixedSize(420, 320);
    setStyleSheet(QString("QDialog { background: %1; }").arg(ui::colors::BG_SURFACE()));
    build_ui(current_name);
}

QString DeployDialog::workflow_name() const {
    return name_edit_->text();
}
QString DeployDialog::workflow_description() const {
    return desc_edit_->toPlainText();
}

void DeployDialog::build_ui(const QString& current_name) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    // ── Title ──────────────────────────────────────────────────────
    title_label_ = new QLabel(tr("SAVE & RUN WORKFLOW"));
    title_label_->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 14px;"
                                         "font-weight: bold; letter-spacing: 0.5px;")
                                    .arg(ui::colors::AMBER()));
    root->addWidget(title_label_);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED()));
    root->addWidget(sep);

    // ── Name ───────────────────────────────────────────────────────
    name_label_ = new QLabel(tr("WORKFLOW NAME"));
    name_label_->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px; font-weight: bold;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
    root->addWidget(name_label_);

    name_edit_ = new QLineEdit(current_name);
    name_edit_->setStyleSheet(QString("QLineEdit { %1 } QLineEdit:focus { border: 1px solid %2; }")
                                  .arg(deploy_input_style(), ui::colors::AMBER()));
    root->addWidget(name_edit_);

    // ── Description ────────────────────────────────────────────────
    desc_label_ = new QLabel(tr("DESCRIPTION"));
    desc_label_->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px; font-weight: bold;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
    root->addWidget(desc_label_);

    desc_edit_ = new QPlainTextEdit;
    desc_edit_->setPlaceholderText(tr("Describe what this workflow does..."));
    desc_edit_->setMaximumHeight(80);
    desc_edit_->setStyleSheet(QString("QPlainTextEdit { %1 } QPlainTextEdit:focus { border: 1px solid %2; }")
                                  .arg(deploy_input_style(), ui::colors::AMBER()));
    root->addWidget(desc_edit_);

    root->addStretch();

    // ── Buttons ────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(8);

    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedHeight(28);
    cancel_btn_->setStyleSheet(QString("QPushButton {"
                                       "  background: %1; color: %2; border: 1px solid %3;"
                                       "  font-family: Consolas; font-size: 11px; font-weight: bold; padding: 0 16px;"
                                       "}"
                                       "QPushButton:hover { color: %4; background: %5; }")
                                   .arg(ui::colors::BG_HOVER(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(),
                                        ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER()));
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addWidget(cancel_btn_);

    btn_row->addStretch();

    draft_btn_ = new QPushButton(tr("SAVE DRAFT"));
    draft_btn_->setFixedHeight(28);
    draft_btn_->setStyleSheet(
        QString("QPushButton {"
                "  background: %1; color: %2; border: 1px solid %3;"
                "  font-family: Consolas; font-size: 11px; font-weight: bold; padding: 0 16px;"
                "}"
                "QPushButton:hover { background: %4; }")
            .arg(ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(draft_btn_, &QPushButton::clicked, this, [this]() {
        deploy_ = false;
        accept();
    });
    btn_row->addWidget(draft_btn_);

    deploy_btn_ = new QPushButton(tr("SAVE & RUN"));
    deploy_btn_->setFixedHeight(28);
    deploy_btn_->setStyleSheet(
        QString("QPushButton {"
                "  background: %1; color: %2;"
                "  border: 1px solid %3; font-family: Consolas;"
                "  font-size: 11px; font-weight: bold; padding: 0 16px;"
                "}"
                "QPushButton:hover { background: %2; color: %4; }")
            .arg(ui::colors::ACCENT_BG(), ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));
    connect(deploy_btn_, &QPushButton::clicked, this, [this]() {
        deploy_ = true;
        accept();
    });
    btn_row->addWidget(deploy_btn_);

    root->addLayout(btn_row);
}

void DeployDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void DeployDialog::retranslateUi() {
    setWindowTitle(tr("Save & Run Workflow"));
    if (title_label_) title_label_->setText(tr("SAVE & RUN WORKFLOW"));
    if (name_label_) name_label_->setText(tr("WORKFLOW NAME"));
    if (desc_label_) desc_label_->setText(tr("DESCRIPTION"));
    if (desc_edit_) desc_edit_->setPlaceholderText(tr("Describe what this workflow does..."));
    if (cancel_btn_) cancel_btn_->setText(tr("CANCEL"));
    if (draft_btn_) draft_btn_->setText(tr("SAVE DRAFT"));
    if (deploy_btn_) deploy_btn_->setText(tr("SAVE & RUN"));
}

} // namespace fincept::workflow
