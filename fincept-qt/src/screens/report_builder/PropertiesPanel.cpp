#include "screens/report_builder/PropertiesPanel.h"

#include "screens/report_builder/ReportBuilderScreen.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>

namespace fincept::screens {

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(260);
    setStyleSheet(QString("background: %1; border-left: 1px solid %2;").arg(ui::colors::PANEL, ui::colors::BORDER));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    stack_ = new QStackedWidget;
    build_empty_page();
    build_editor_page();
    stack_->setCurrentWidget(empty_widget_);

    vl->addWidget(stack_);
}

void PropertiesPanel::build_empty_page() {
    empty_widget_ = new QWidget;
    auto* vl = new QVBoxLayout(empty_widget_);
    vl->setAlignment(Qt::AlignCenter);

    auto* lbl = new QLabel("Select a component\nto edit properties");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(ui::colors::MUTED));
    vl->addWidget(lbl);

    stack_->addWidget(empty_widget_);
}

void PropertiesPanel::build_editor_page() {
    editor_widget_ = new QWidget;
    editor_layout_ = new QVBoxLayout(editor_widget_);
    editor_layout_->setContentsMargins(8, 8, 8, 8);
    editor_layout_->setSpacing(6);
    editor_layout_->addStretch();

    stack_->addWidget(editor_widget_);
}

void PropertiesPanel::show_empty() {
    current_index_ = -1;
    stack_->setCurrentWidget(empty_widget_);
}

void PropertiesPanel::show_properties(const ReportComponent* component, int index) {
    if (!component) {
        show_empty();
        return;
    }

    current_index_ = index;

    // Clear previous editor content
    while (editor_layout_->count() > 0) {
        auto* item = editor_layout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Title
    auto* title = new QLabel(QString("PROPERTIES — %1").arg(component->type.toUpper()));
    title->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(ui::colors::MUTED));
    editor_layout_->addWidget(title);

    // Content editor for text-based types
    if (component->type == "heading" || component->type == "text" || component->type == "code" ||
        component->type == "quote") {
        auto* lbl = new QLabel("Content:");
        lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::GRAY));
        editor_layout_->addWidget(lbl);

        auto* edit = new QTextEdit;
        edit->setPlainText(component->content);
        edit->setMaximumHeight(200);
        connect(edit, &QTextEdit::textChanged, this,
                [this, edit]() { emit content_changed(current_index_, edit->toPlainText()); });
        editor_layout_->addWidget(edit);
    }

    // Table config
    if (component->type == "table") {
        auto* rows_lbl = new QLabel("Rows:");
        rows_lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::GRAY));
        editor_layout_->addWidget(rows_lbl);

        auto* rows_spin = new QSpinBox;
        rows_spin->setRange(1, 50);
        rows_spin->setValue(component->config.value("rows", "3").toInt());
        connect(rows_spin, &QSpinBox::valueChanged, this,
                [this](int val) { emit config_changed(current_index_, "rows", QString::number(val)); });
        editor_layout_->addWidget(rows_spin);

        auto* cols_lbl = new QLabel("Columns:");
        cols_lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::GRAY));
        editor_layout_->addWidget(cols_lbl);

        auto* cols_spin = new QSpinBox;
        cols_spin->setRange(1, 20);
        cols_spin->setValue(component->config.value("cols", "3").toInt());
        connect(cols_spin, &QSpinBox::valueChanged, this,
                [this](int val) { emit config_changed(current_index_, "cols", QString::number(val)); });
        editor_layout_->addWidget(cols_spin);
    }

    // Image picker
    if (component->type == "image") {
        auto* path_lbl = new QLabel("Image:");
        path_lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::GRAY));
        editor_layout_->addWidget(path_lbl);

        auto* browse = new QPushButton("Browse...");
        connect(browse, &QPushButton::clicked, this, [this]() {
            QString path =
                QFileDialog::getOpenFileName(this, "Select Image", "", "Images (*.png *.jpg *.jpeg *.bmp *.svg)");
            if (!path.isEmpty()) {
                emit config_changed(current_index_, "path", path);
            }
        });
        editor_layout_->addWidget(browse);
    }

    // List editor
    if (component->type == "list") {
        auto* lbl = new QLabel("Items (one per line):");
        lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::GRAY));
        editor_layout_->addWidget(lbl);

        auto* edit = new QTextEdit;
        edit->setPlainText(component->content);
        edit->setMaximumHeight(150);
        connect(edit, &QTextEdit::textChanged, this,
                [this, edit]() { emit content_changed(current_index_, edit->toPlainText()); });
        editor_layout_->addWidget(edit);
    }

    // Action buttons
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER));
    editor_layout_->addWidget(sep);

    auto* dup_btn = new QPushButton("Duplicate");
    connect(dup_btn, &QPushButton::clicked, this, [this]() { emit duplicate_requested(current_index_); });
    editor_layout_->addWidget(dup_btn);

    auto* del_btn = new QPushButton("Delete");
    del_btn->setStyleSheet("QPushButton { color: #FF0000; }");
    connect(del_btn, &QPushButton::clicked, this, [this]() { emit delete_requested(current_index_); });
    editor_layout_->addWidget(del_btn);

    editor_layout_->addStretch();
    stack_->setCurrentWidget(editor_widget_);
}

} // namespace fincept::screens
