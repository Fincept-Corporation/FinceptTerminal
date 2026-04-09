#include "ui/workspace/WorkspaceSaveAsDialog.h"

#include "core/config/AppPaths.h"
#include "services/workspace/WorkspaceManager.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace fincept::ui {

static const char* SAVEAS_DLG_SS = "QDialog{background:#0a0a0a;color:#e5e5e5;}"
                                   "QLabel{color:#e5e5e5;}"
                                   "QLineEdit{background:#111;color:#e5e5e5;border:1px solid #2a2a2a;"
                                   "border-radius:3px;padding:4px;}"
                                   "QLineEdit:focus{border:1px solid #d97706;}"
                                   "QPushButton{background:#1a1a1a;color:#e5e5e5;border:1px solid #2a2a2a;"
                                   "border-radius:3px;padding:6px 16px;}"
                                   "QPushButton:hover{background:#222;border-color:#d97706;}"
                                   "QPushButton#saveBtn{background:#d97706;color:#000;font-weight:700;border:none;}"
                                   "QPushButton#saveBtn:hover{background:#b45309;}"
                                   "QPushButton#saveBtn:disabled{background:#4a3000;color:#666;}";

WorkspaceSaveAsDialog::WorkspaceSaveAsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Save Workspace As");
    setModal(true);
    setFixedSize(420, 180);
    setStyleSheet(SAVEAS_DLG_SS);
    chosen_path_ = AppPaths::workspaces();
    setup_ui();
}

void WorkspaceSaveAsDialog::setup_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(20, 20, 20, 20);
    vl->setSpacing(12);

    auto* name_label = new QLabel("Workspace Name");
    name_label->setStyleSheet("color:#888;font-size:11px;font-weight:600;");
    vl->addWidget(name_label);

    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText("Enter a name");
    // Pre-fill with current workspace name
    name_edit_->setText(WorkspaceManager::instance().current_workspace_name());
    name_edit_->selectAll();
    vl->addWidget(name_edit_);

    // Path row
    auto* path_row = new QHBoxLayout;
    auto* path_label = new QLabel(QString("Save to: %1").arg(chosen_path_));
    path_label->setStyleSheet("color:#555;font-size:11px;");
    path_label->setWordWrap(true);
    auto* change_btn = new QPushButton("Change...");
    change_btn->setFixedWidth(80);
    path_row->addWidget(path_label, 1);
    path_row->addWidget(change_btn);
    vl->addLayout(path_row);

    vl->addStretch();

    // Buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();
    auto* cancel_btn = new QPushButton("Cancel");
    auto* save_btn = new QPushButton("Save");
    save_btn->setObjectName("saveBtn");
    save_btn->setEnabled(!name_edit_->text().trimmed().isEmpty());
    btn_row->addWidget(cancel_btn);
    btn_row->addWidget(save_btn);
    vl->addLayout(btn_row);

    connect(name_edit_, &QLineEdit::textChanged, save_btn,
            [save_btn](const QString& t) { save_btn->setEnabled(!t.trimmed().isEmpty()); });

    connect(change_btn, &QPushButton::clicked, this, [this, path_label]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Choose Save Location", chosen_path_);
        if (!dir.isEmpty()) {
            chosen_path_ = dir;
            path_label->setText(QString("Save to: %1").arg(dir));
        }
    });

    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(save_btn, &QPushButton::clicked, this, &QDialog::accept);
}

QString WorkspaceSaveAsDialog::new_name() const {
    return name_edit_ ? name_edit_->text().trimmed() : QString{};
}

QString WorkspaceSaveAsDialog::chosen_path() const {
    // Return full file path including filename
    QString name = new_name();
    if (name.isEmpty())
        return {};
    // Sanitise for filename
    QString filename = name;
    filename.replace(QRegularExpression("[^a-zA-Z0-9_\\- ]"), "_");
    filename = filename.simplified().replace(' ', '_');
    return chosen_path_ + "/" + filename + ".fwsp";
}

} // namespace fincept::ui
