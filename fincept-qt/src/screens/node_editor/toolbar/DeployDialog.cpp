#include "screens/node_editor/toolbar/DeployDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::workflow {

static const char* kDeployInputStyle = "background: #1e1e1e; color: #e5e5e5; border: 1px solid #2a2a2a;"
                                       "font-family: Consolas; font-size: 12px; padding: 6px 8px;";

DeployDialog::DeployDialog(const QString& current_name, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Deploy Workflow");
    setFixedSize(420, 320);
    setStyleSheet("QDialog { background: #141414; }");
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
    auto* title = new QLabel("DEPLOY WORKFLOW");
    title->setStyleSheet("color: #d97706; font-family: Consolas; font-size: 14px;"
                         "font-weight: bold; letter-spacing: 0.5px;");
    root->addWidget(title);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #2a2a2a; border: none;");
    root->addWidget(sep);

    // ── Name ───────────────────────────────────────────────────────
    auto* name_label = new QLabel("WORKFLOW NAME");
    name_label->setStyleSheet("color: #808080; font-family: Consolas; font-size: 11px; font-weight: bold;");
    root->addWidget(name_label);

    name_edit_ = new QLineEdit(current_name);
    name_edit_->setStyleSheet(
        QString("QLineEdit { %1 } QLineEdit:focus { border: 1px solid #d97706; }").arg(kDeployInputStyle));
    root->addWidget(name_edit_);

    // ── Description ────────────────────────────────────────────────
    auto* desc_label = new QLabel("DESCRIPTION");
    desc_label->setStyleSheet("color: #808080; font-family: Consolas; font-size: 11px; font-weight: bold;");
    root->addWidget(desc_label);

    desc_edit_ = new QPlainTextEdit;
    desc_edit_->setPlaceholderText("Describe what this workflow does...");
    desc_edit_->setMaximumHeight(80);
    desc_edit_->setStyleSheet(
        QString("QPlainTextEdit { %1 } QPlainTextEdit:focus { border: 1px solid #d97706; }").arg(kDeployInputStyle));
    root->addWidget(desc_edit_);

    root->addStretch();

    // ── Buttons ────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(8);

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedHeight(28);
    cancel_btn->setStyleSheet("QPushButton {"
                              "  background: #1e1e1e; color: #808080; border: 1px solid #2a2a2a;"
                              "  font-family: Consolas; font-size: 11px; font-weight: bold; padding: 0 16px;"
                              "}"
                              "QPushButton:hover { color: #e5e5e5; background: #252525; }");
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addWidget(cancel_btn);

    btn_row->addStretch();

    auto* draft_btn = new QPushButton("SAVE DRAFT");
    draft_btn->setFixedHeight(28);
    draft_btn->setStyleSheet("QPushButton {"
                             "  background: #1e1e1e; color: #e5e5e5; border: 1px solid #2a2a2a;"
                             "  font-family: Consolas; font-size: 11px; font-weight: bold; padding: 0 16px;"
                             "}"
                             "QPushButton:hover { background: #252525; }");
    connect(draft_btn, &QPushButton::clicked, this, [this]() {
        deploy_ = false;
        accept();
    });
    btn_row->addWidget(draft_btn);

    auto* deploy_btn = new QPushButton("DEPLOY");
    deploy_btn->setFixedHeight(28);
    deploy_btn->setStyleSheet("QPushButton {"
                              "  background: rgba(217,119,6,0.1); color: #d97706;"
                              "  border: 1px solid #78350f; font-family: Consolas;"
                              "  font-size: 11px; font-weight: bold; padding: 0 16px;"
                              "}"
                              "QPushButton:hover { background: #d97706; color: #080808; }");
    connect(deploy_btn, &QPushButton::clicked, this, [this]() {
        deploy_ = true;
        accept();
    });
    btn_row->addWidget(deploy_btn);

    root->addLayout(btn_row);
}

} // namespace fincept::workflow
