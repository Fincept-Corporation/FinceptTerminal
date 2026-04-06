#include "ui/workspace/WorkspaceOpenDialog.h"
#include "services/workspace/WorkspaceManager.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::ui {

static const char* OPEN_DLG_SS =
    "QDialog{background:#0a0a0a;color:#e5e5e5;}"
    "QLabel{color:#e5e5e5;}"
    "QListWidget{background:#111;color:#e5e5e5;border:1px solid #2a2a2a;border-radius:3px;}"
    "QListWidget::item{padding:10px 12px;}"
    "QListWidget::item:selected{background:#1c1c1c;color:#d97706;border-left:2px solid #d97706;}"
    "QListWidget::item:hover{background:#161616;}"
    "QPushButton{background:#1a1a1a;color:#e5e5e5;border:1px solid #2a2a2a;"
        "border-radius:3px;padding:6px 16px;}"
    "QPushButton:hover{background:#222;border-color:#d97706;}"
    "QPushButton#openBtn{background:#d97706;color:#000;font-weight:700;border:none;}"
    "QPushButton#openBtn:hover{background:#b45309;}"
    "QPushButton#openBtn:disabled{background:#4a3000;color:#666;}";

WorkspaceOpenDialog::WorkspaceOpenDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Open Workspace");
    setModal(true);
    setFixedSize(760, 480);
    setStyleSheet(OPEN_DLG_SS);
    setup_ui();
    load_workspaces();
}

void WorkspaceOpenDialog::setup_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // ── Left: workspace list ──────────────────────────────────────────────────
    auto* left = new QVBoxLayout;
    auto* list_label = new QLabel("Saved Workspaces");
    list_label->setStyleSheet("color:#888;font-size:11px;font-weight:600;");
    left->addWidget(list_label);

    workspace_list_ = new QListWidget;
    workspace_list_->setFixedWidth(320);
    left->addWidget(workspace_list_, 1);

    auto* browse_btn = new QPushButton("Browse for File...");
    left->addWidget(browse_btn);
    root->addLayout(left);

    // ── Right: preview ────────────────────────────────────────────────────────
    auto* right = new QVBoxLayout;
    right->setSpacing(10);

    auto* prev_label = new QLabel("Preview");
    prev_label->setStyleSheet("color:#888;font-size:11px;font-weight:600;");
    right->addWidget(prev_label);

    preview_label_ = new QLabel("Select a workspace to preview");
    preview_label_->setWordWrap(true);
    preview_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    preview_label_->setStyleSheet(
        "background:#111;border:1px solid #2a2a2a;border-radius:3px;"
        "padding:12px;color:#aaa;font-size:12px;");
    right->addWidget(preview_label_, 1);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();
    auto* cancel_btn = new QPushButton("Cancel");
    open_btn_ = new QPushButton("Open");
    open_btn_->setObjectName("openBtn");
    open_btn_->setEnabled(false);
    btn_row->addWidget(cancel_btn);
    btn_row->addWidget(open_btn_);
    right->addLayout(btn_row);

    root->addLayout(right, 1);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(workspace_list_, &QListWidget::currentRowChanged,
            this, &WorkspaceOpenDialog::update_preview);

    connect(workspace_list_, &QListWidget::itemDoubleClicked, this, [this]() {
        if (open_btn_->isEnabled()) accept();
    });

    connect(browse_btn, &QPushButton::clicked, this, &WorkspaceOpenDialog::browse_for_file);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(open_btn_,  &QPushButton::clicked, this, &QDialog::accept);
}

void WorkspaceOpenDialog::load_workspaces() {
    auto result = WorkspaceManager::instance().list_workspaces();
    if (result.is_err()) return;

    for (const auto& s : result.value()) {
        auto* item = new QListWidgetItem(s.name);
        item->setData(Qt::UserRole,     s.file_path);
        item->setData(Qt::UserRole + 1, s.description);
        item->setData(Qt::UserRole + 2, s.updated_at);
        workspace_list_->addItem(item);
    }
}

void WorkspaceOpenDialog::browse_for_file() {
    QString path = QFileDialog::getOpenFileName(
        this, "Open Workspace File", {}, "Fincept Workspace (*.fwsp);;All Files (*)");
    if (path.isEmpty()) return;
    selected_path_ = path;
    open_btn_->setEnabled(true);
    preview_label_->setText(QString("File: %1").arg(path));
    accept();
}

void WorkspaceOpenDialog::update_preview(int row) {
    if (row < 0 || row >= workspace_list_->count()) return;
    auto* item = workspace_list_->item(row);
    selected_path_ = item->data(Qt::UserRole).toString();
    QString desc   = item->data(Qt::UserRole + 1).toString();
    QString date   = item->data(Qt::UserRole + 2).toString();
    preview_label_->setText(
        QString("<b>%1</b><br><br>%2<br><br><span style='color:#555'>Last saved: %3</span>")
            .arg(item->text(), desc.isEmpty() ? "No description" : desc, date));
    open_btn_->setEnabled(true);
}

QString WorkspaceOpenDialog::selected_path() const {
    return selected_path_;
}

} // namespace fincept::ui
