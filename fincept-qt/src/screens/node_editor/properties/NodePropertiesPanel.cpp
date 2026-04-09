#include "screens/node_editor/properties/NodePropertiesPanel.h"

#include "screens/node_editor/properties/ParameterWidgets.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>

namespace fincept::workflow {

NodePropertiesPanel::NodePropertiesPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(280);
    setObjectName("nodePropertiesPanel");
    build_ui();
}

void NodePropertiesPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget;

    // ── Empty state ────────────────────────────────────────────────
    empty_page_ = new QWidget;
    auto* el = new QVBoxLayout(empty_page_);
    el->setContentsMargins(20, 40, 20, 20);
    el->setAlignment(Qt::AlignCenter);
    auto* empty_label = new QLabel("Select a node\nto edit properties");
    empty_label->setAlignment(Qt::AlignCenter);
    empty_label->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 12px;")
                                   .arg(ui::colors::TEXT_TERTIARY));
    el->addWidget(empty_label);

    // ── Editor page (populated dynamically) ────────────────────────
    editor_page_ = new QWidget;
    auto* editor_root = new QVBoxLayout(editor_page_);
    editor_root->setContentsMargins(0, 0, 0, 0);
    editor_root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }"
                "QScrollBar:vertical {"
                "  background: %1; width: 6px; margin: 0;"
                "}"
                "QScrollBar::handle:vertical {"
                "  background: %2; min-height: 20px;"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                "  height: 0; background: none;"
                "}")
            .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_DIM));

    auto* scroll_content = new QWidget;
    editor_layout_ = new QVBoxLayout(scroll_content);
    editor_layout_->setContentsMargins(10, 8, 10, 8);
    editor_layout_->setSpacing(0);
    editor_layout_->addStretch();

    scroll->setWidget(scroll_content);
    editor_root->addWidget(scroll, 1);

    stack_->addWidget(empty_page_);
    stack_->addWidget(editor_page_);
    stack_->setCurrentWidget(empty_page_);

    root->addWidget(stack_, 1);
}

void NodePropertiesPanel::show_properties(const NodeDef* node, const NodeTypeDef* type_def) {
    if (!node || !type_def) {
        clear();
        return;
    }

    current_node_id_ = node->id;
    build_editor(*node, *type_def);
    stack_->setCurrentWidget(editor_page_);
}

void NodePropertiesPanel::clear() {
    current_node_id_.clear();
    stack_->setCurrentWidget(empty_page_);
}

void NodePropertiesPanel::build_editor(const NodeDef& node, const NodeTypeDef& type_def) {
    // Clear existing editor widgets (keep trailing stretch)
    while (editor_layout_->count() > 1) {
        auto* item = editor_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    int pos = 0;

    // ── Header ─────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(34);
    header->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_HOVER));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 10, 0);

    auto* title = new QLabel("PROPERTIES");
    title->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 12px;"
                                 "font-weight: bold; letter-spacing: 0.5px;")
                             .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();

    auto* del_btn = new QPushButton("DEL");
    del_btn->setFixedSize(36, 22);
    del_btn->setStyleSheet(
        QString("QPushButton {"
                "  background: %1; color: %2;"
                "  border: 1px solid rgba(220,38,38,0.3); font-family: Consolas;"
                "  font-size: 10px; font-weight: bold;"
                "}"
                "QPushButton:hover { background: %2; color: %3; }")
            .arg(ui::colors::NEGATIVE_BG, ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY));
    QString node_id = node.id;
    connect(del_btn, &QPushButton::clicked, this, [this, node_id]() { emit delete_requested(node_id); });
    hl->addWidget(del_btn);

    editor_layout_->insertWidget(pos++, header);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED));
    editor_layout_->insertWidget(pos++, sep);

    // ── Node name ──────────────────────────────────────────────────
    auto* name_section = new QWidget;
    auto* nsl = new QVBoxLayout(name_section);
    nsl->setContentsMargins(0, 8, 0, 8);
    nsl->setSpacing(4);

    auto* name_label = new QLabel("NAME");
    name_label->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px; font-weight: bold;")
                                  .arg(ui::colors::TEXT_SECONDARY));
    nsl->addWidget(name_label);

    auto* name_edit = new QLineEdit(node.name);
    name_edit->setStyleSheet(
        QString("QLineEdit {"
                "  background: %1; color: %2; border: 1px solid %3;"
                "  font-family: Consolas; font-size: 12px; padding: 4px 6px;"
                "}"
                "QLineEdit:focus { border: 1px solid %4; }")
            .arg(ui::colors::BG_HOVER, ui::colors::TEXT_PRIMARY,
                 ui::colors::BORDER_MED, ui::colors::AMBER));
    nsl->addWidget(name_edit);

    connect(name_edit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (!current_node_id_.isEmpty())
            emit name_changed(current_node_id_, text);
    });

    editor_layout_->insertWidget(pos++, name_section);

    // ── Type info ──────────────────────────────────────────────────
    auto* type_label = new QLabel(QString("%1  |  %2").arg(type_def.category, type_def.type_id));
    type_label->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px; padding: 0 0 6px 0;")
                                  .arg(ui::colors::TEXT_TERTIARY));
    editor_layout_->insertWidget(pos++, type_label);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep2 = new QFrame;
    sep2->setFixedHeight(1);
    sep2->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED));
    editor_layout_->insertWidget(pos++, sep2);

    // ── Parameters ─────────────────────────────────────────────────
    if (!type_def.parameters.isEmpty()) {
        auto* param_header = new QLabel("PARAMETERS");
        param_header->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px;"
                                           "font-weight: bold; padding: 8px 0 4px 0; letter-spacing: 0.5px;")
                                       .arg(ui::colors::AMBER));
        editor_layout_->insertWidget(pos++, param_header);

        for (const auto& param : type_def.parameters) {
            QJsonValue current = node.parameters.value(param.key);
            if (current.isUndefined())
                current = param.default_value;

            auto* widget = ParameterWidgetFactory::create(param, current, [this](const QString& key, QJsonValue value) {
                if (!current_node_id_.isEmpty())
                    emit param_changed(current_node_id_, key, value);
            });
            editor_layout_->insertWidget(pos++, widget);
        }
    }

    // ── Settings section ───────────────────────────────────────────
    auto* sep3 = new QFrame;
    sep3->setFixedHeight(1);
    sep3->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED));
    editor_layout_->insertWidget(pos++, sep3);

    auto* settings_header = new QLabel("SETTINGS");
    settings_header->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px;"
                                            "font-weight: bold; padding: 8px 0 4px 0; letter-spacing: 0.5px;")
                                       .arg(ui::colors::AMBER));
    editor_layout_->insertWidget(pos++, settings_header);

    // Disabled toggle
    auto* disabled_check = new QCheckBox("Disabled");
    disabled_check->setChecked(node.disabled);
    disabled_check->setStyleSheet(
        QString("QCheckBox { color: %1; font-family: Consolas; font-size: 12px; }"
                "QCheckBox::indicator { width: 14px; height: 14px; background: %2; border: 1px solid %3; }"
                "QCheckBox::indicator:checked { background: %4; }")
            .arg(ui::colors::TEXT_PRIMARY, ui::colors::BG_HOVER,
                 ui::colors::BORDER_MED, ui::colors::AMBER));
    editor_layout_->insertWidget(pos++, disabled_check);

    // Continue on fail toggle
    auto* cof_check = new QCheckBox("Continue on Fail");
    cof_check->setChecked(node.continue_on_fail);
    cof_check->setStyleSheet(disabled_check->styleSheet());
    editor_layout_->insertWidget(pos++, cof_check);
}

} // namespace fincept::workflow
