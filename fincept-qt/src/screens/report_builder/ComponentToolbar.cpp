#include "screens/report_builder/ComponentToolbar.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

namespace fincept::screens {

ComponentToolbar::ComponentToolbar(QWidget* parent) : QWidget(parent) {
    setFixedWidth(240);
    setStyleSheet(QString("background: %1; border-right: 1px solid %2;").arg(ui::colors::PANEL, ui::colors::BORDER));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // Section: Add Component
    auto* add_label = new QLabel("ADD COMPONENT");
    add_label->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(ui::colors::MUTED));
    vl->addWidget(add_label);

    const char* types[][2] = {
        {"Heading", "heading"}, {"Text Block", "text"},         {"Table", "table"},
        {"Image", "image"},     {"Chart Placeholder", "chart"}, {"Code Block", "code"},
        {"Divider", "divider"}, {"Block Quote", "quote"},       {"List", "list"},
    };

    for (auto& t : types) {
        add_type_button(vl, t[0], t[1]);
    }

    // Separator
    auto* sep1 = new QFrame;
    sep1->setFixedHeight(1);
    sep1->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER));
    vl->addWidget(sep1);

    // Section: Font Settings
    auto* font_label = new QLabel("FONT");
    font_label->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(ui::colors::MUTED));
    vl->addWidget(font_label);

    font_combo_ = new QFontComboBox;
    font_combo_->setCurrentFont(QFont("Segoe UI"));
    vl->addWidget(font_combo_);

    auto* size_row = new QWidget;
    auto* srl = new QHBoxLayout(size_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(4);

    font_size_ = new QSpinBox;
    font_size_->setRange(8, 72);
    font_size_->setValue(12);
    srl->addWidget(font_size_);

    bold_btn_ = new QPushButton("B");
    bold_btn_->setCheckable(true);
    bold_btn_->setFixedSize(28, 28);
    bold_btn_->setStyleSheet("QPushButton { font-weight: bold; }");
    srl->addWidget(bold_btn_);

    italic_btn_ = new QPushButton("I");
    italic_btn_->setCheckable(true);
    italic_btn_->setFixedSize(28, 28);
    italic_btn_->setStyleSheet("QPushButton { font-style: italic; }");
    srl->addWidget(italic_btn_);

    vl->addWidget(size_row);

    auto emit_font = [this]() {
        emit font_changed(font_combo_->currentFont().family(), font_size_->value(), bold_btn_->isChecked(),
                          italic_btn_->isChecked());
    };
    connect(font_combo_, &QFontComboBox::currentFontChanged, this, emit_font);
    connect(font_size_, &QSpinBox::valueChanged, this, emit_font);
    connect(bold_btn_, &QPushButton::toggled, this, emit_font);
    connect(italic_btn_, &QPushButton::toggled, this, emit_font);

    // Separator
    auto* sep2 = new QFrame;
    sep2->setFixedHeight(1);
    sep2->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER));
    vl->addWidget(sep2);

    // Section: Document Structure
    auto* struct_label = new QLabel("STRUCTURE");
    struct_label->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(ui::colors::MUTED));
    vl->addWidget(struct_label);

    structure_list_ = new QListWidget;
    structure_list_->setStyleSheet(QString("QListWidget { background: %1; border: 1px solid %2; } "
                                           "QListWidget::item { color: %3; padding: 4px; } "
                                           "QListWidget::item:selected { background: #111111; color: %4; }")
                                       .arg(ui::colors::DARK, ui::colors::BORDER, ui::colors::GRAY, ui::colors::WHITE));
    connect(structure_list_, &QListWidget::currentRowChanged, this, &ComponentToolbar::structure_selected);
    vl->addWidget(structure_list_, 1);

    // Structure action buttons
    auto* action_row = new QWidget;
    auto* arl = new QHBoxLayout(action_row);
    arl->setContentsMargins(0, 0, 0, 0);
    arl->setSpacing(2);

    auto make_btn = [&](const char* text) {
        auto* b = new QPushButton(text);
        b->setFixedHeight(24);
        arl->addWidget(b);
        return b;
    };

    auto* up_btn = make_btn("Up");
    auto* dn_btn = make_btn("Dn");
    auto* dup_btn = make_btn("Dup");
    auto* del_btn = make_btn("Del");

    connect(up_btn, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r > 0)
            emit move_up(r);
    });
    connect(dn_btn, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r >= 0)
            emit move_down(r);
    });
    connect(dup_btn, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r >= 0)
            emit duplicate(r);
    });
    connect(del_btn, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r >= 0)
            emit delete_item(r);
    });

    vl->addWidget(action_row);
}

void ComponentToolbar::add_type_button(QVBoxLayout* layout, const QString& label, const QString& type) {
    auto* btn = new QPushButton(label);
    btn->setFixedHeight(28);
    btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                               "text-align: left; padding: 0 8px; font-size: 12px; }"
                               "QPushButton:hover { background: #111111; color: %4; }")
                           .arg(ui::colors::DARK, ui::colors::GRAY, ui::colors::BORDER, ui::colors::WHITE));

    connect(btn, &QPushButton::clicked, this, [this, type]() { emit add_component(type); });

    layout->addWidget(btn);
}

void ComponentToolbar::update_structure(const QStringList& items, int selected_index) {
    structure_list_->blockSignals(true);
    structure_list_->clear();
    structure_list_->addItems(items);
    if (selected_index >= 0 && selected_index < items.size()) {
        structure_list_->setCurrentRow(selected_index);
    }
    structure_list_->blockSignals(false);
}

} // namespace fincept::screens
