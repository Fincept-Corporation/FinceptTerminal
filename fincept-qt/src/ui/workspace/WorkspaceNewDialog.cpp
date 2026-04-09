#include "ui/workspace/WorkspaceNewDialog.h"

#include "services/workspace/WorkspaceTemplates.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::ui {

static const char* DLG_SS =
    "QDialog{background:#0a0a0a;color:#e5e5e5;}"
    "QLabel{color:#e5e5e5;}"
    "QLineEdit,QPlainTextEdit{"
    "background:#111;color:#e5e5e5;border:1px solid #2a2a2a;"
    "border-radius:3px;padding:4px;}"
    "QLineEdit:focus,QPlainTextEdit:focus{border:1px solid #d97706;}"
    "QListWidget{background:#111;color:#e5e5e5;border:1px solid #2a2a2a;border-radius:3px;}"
    "QListWidget::item{padding:8px 12px;}"
    "QListWidget::item:selected{background:#1c1c1c;color:#d97706;border-left:2px solid #d97706;}"
    "QListWidget::item:hover{background:#161616;}"
    "QPushButton{background:#1a1a1a;color:#e5e5e5;border:1px solid #2a2a2a;"
    "border-radius:3px;padding:6px 16px;}"
    "QPushButton:hover{background:#222;border-color:#d97706;}"
    "QPushButton#createBtn{background:#d97706;color:#000;font-weight:700;border:none;}"
    "QPushButton#createBtn:hover{background:#b45309;}"
    "QPushButton#createBtn:disabled{background:#4a3000;color:#666;}";

WorkspaceNewDialog::WorkspaceNewDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("New Workspace");
    setModal(true);
    setFixedSize(680, 460);
    setStyleSheet(DLG_SS);
    setup_ui();
}

void WorkspaceNewDialog::setup_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // ── Left: template list ───────────────────────────────────────────────────
    auto* left = new QVBoxLayout;
    auto* tpl_label = new QLabel("Template");
    tpl_label->setStyleSheet("color:#888;font-size:11px;font-weight:600;");
    left->addWidget(tpl_label);

    template_list_ = new QListWidget;
    template_list_->setFixedWidth(200);
    for (const auto& s : WorkspaceTemplates::available()) {
        auto* item = new QListWidgetItem(s.name);
        item->setData(Qt::UserRole, s.id);
        item->setToolTip(s.description);
        template_list_->addItem(item);
    }
    template_list_->setCurrentRow(0);
    left->addWidget(template_list_, 1);
    root->addLayout(left);

    // ── Right: name, description, preview ────────────────────────────────────
    auto* right = new QVBoxLayout;
    right->setSpacing(10);

    auto* name_label = new QLabel("Workspace Name");
    name_label->setStyleSheet("color:#888;font-size:11px;font-weight:600;");
    right->addWidget(name_label);

    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText("My Workspace");
    right->addWidget(name_edit_);

    auto* desc_label = new QLabel("Description (optional)");
    desc_label->setStyleSheet("color:#888;font-size:11px;font-weight:600;");
    right->addWidget(desc_label);

    description_edit_ = new QPlainTextEdit;
    description_edit_->setFixedHeight(70);
    description_edit_->setPlaceholderText("What is this workspace for?");
    right->addWidget(description_edit_);

    auto* prev_label = new QLabel("Preview");
    prev_label->setStyleSheet("color:#888;font-size:11px;font-weight:600;");
    right->addWidget(prev_label);

    preview_label_ = new QLabel;
    preview_label_->setWordWrap(true);
    preview_label_->setStyleSheet("background:#111;border:1px solid #2a2a2a;border-radius:3px;"
                                  "padding:8px;color:#aaa;font-size:12px;");
    preview_label_->setFixedHeight(80);
    right->addWidget(preview_label_);

    right->addStretch();

    // Buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();
    auto* cancel_btn = new QPushButton("Cancel");
    create_btn_ = new QPushButton("Create");
    create_btn_->setObjectName("createBtn");
    create_btn_->setEnabled(false);
    btn_row->addWidget(cancel_btn);
    btn_row->addWidget(create_btn_);
    right->addLayout(btn_row);

    root->addLayout(right, 1);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(name_edit_, &QLineEdit::textChanged, this,
            [this](const QString& t) { create_btn_->setEnabled(!t.trimmed().isEmpty()); });

    connect(template_list_, &QListWidget::currentRowChanged, this, [this](int) {
        if (auto* item = template_list_->currentItem())
            update_preview(item->data(Qt::UserRole).toString());
    });

    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(create_btn_, &QPushButton::clicked, this, &QDialog::accept);

    // Seed preview
    if (template_list_->count() > 0)
        update_preview(template_list_->item(0)->data(Qt::UserRole).toString());
}

void WorkspaceNewDialog::update_preview(const QString& template_id) {
    for (const auto& s : WorkspaceTemplates::available()) {
        if (s.id == template_id) {
            preview_label_->setText(s.description);
            return;
        }
    }
}

QString WorkspaceNewDialog::workspace_name() const {
    return name_edit_ ? name_edit_->text().trimmed() : QString{};
}

QString WorkspaceNewDialog::workspace_description() const {
    return description_edit_ ? description_edit_->toPlainText().trimmed() : QString{};
}

QString WorkspaceNewDialog::selected_template_id() const {
    if (!template_list_ || !template_list_->currentItem())
        return "blank";
    return template_list_->currentItem()->data(Qt::UserRole).toString();
}

} // namespace fincept::ui
