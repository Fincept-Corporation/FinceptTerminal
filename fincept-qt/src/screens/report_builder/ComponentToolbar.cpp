#include "screens/report_builder/ComponentToolbar.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

namespace fincept::screens {

// Translatable display label for a component type id. The id is the stable
// data key emitted via add_component(); only the label is localized.
static QString component_type_label(const QString& type) {
    if (type == "heading")
        return ComponentToolbar::tr("Heading");
    if (type == "text")
        return ComponentToolbar::tr("Text Block");
    if (type == "table")
        return ComponentToolbar::tr("Table");
    if (type == "image")
        return ComponentToolbar::tr("Image");
    if (type == "chart")
        return ComponentToolbar::tr("Chart");
    if (type == "sparkline")
        return ComponentToolbar::tr("Sparkline");
    if (type == "stats_block")
        return ComponentToolbar::tr("Stats Block");
    if (type == "callout")
        return ComponentToolbar::tr("Callout Box");
    if (type == "code")
        return ComponentToolbar::tr("Code Block");
    if (type == "divider")
        return ComponentToolbar::tr("Divider");
    if (type == "quote")
        return ComponentToolbar::tr("Block Quote");
    if (type == "list")
        return ComponentToolbar::tr("List");
    if (type == "market_data")
        return ComponentToolbar::tr("Market Data");
    if (type == "page_break")
        return ComponentToolbar::tr("Page Break");
    if (type == "toc")
        return ComponentToolbar::tr("Table of Contents");
    return type;
}

ComponentToolbar::ComponentToolbar(QWidget* parent) : QWidget(parent) {
    // Use min/max width pair (not setFixedWidth) so the parent splitter can
    // drive resize and the collapse animation can drive maximumWidth → 0.
    setMinimumWidth(0);
    setMaximumWidth(240);
    setStyleSheet(QString("background: %1; border-right: 1px solid %2;").arg(ui::colors::PANEL(), ui::colors::BORDER()));

    // Wrap content in a scroll area so it doesn't clip on small displays
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("background: transparent;");

    auto* content = new QWidget(this);
    content->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(4);

    auto make_section = [&](const QString& text) -> QLabel* {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; "
                                   "padding-top: 6px;")
                               .arg(ui::colors::MUTED()));
        vl->addWidget(lbl);
        return lbl;
    };

    auto make_action_btn = [&](const QString& text) -> QPushButton* {
        auto* b = new QPushButton(text);
        b->setFixedHeight(26);
        b->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                 "text-align: left; padding: 0 8px; font-size: 12px; }"
                                 "QPushButton:hover { background: %4; color: %5; }")
                             .arg(ui::colors::BG_RAISED(), ui::colors::AMBER(), ui::colors::BORDER_MED(),
                                  ui::colors::BG_HOVER(), ui::colors::WHITE()));
        vl->addWidget(b);
        return b;
    };

    // ── File actions ──────────────────────────────────────────────────────────
    sec_file_ = make_section(tr("FILE"));
    new_btn_ = make_action_btn(tr("New Report"));
    open_btn_ = make_action_btn(tr("Open..."));
    recent_btn_ = make_action_btn(tr("Recent Reports..."));

    connect(new_btn_, &QPushButton::clicked, this, &ComponentToolbar::new_report_requested);
    connect(open_btn_, &QPushButton::clicked, this, &ComponentToolbar::open_report_requested);
    connect(recent_btn_, &QPushButton::clicked, this, &ComponentToolbar::recent_reports_requested);

    // ── Document settings ─────────────────────────────────────────────────────
    sec_document_ = make_section(tr("DOCUMENT"));
    meta_btn_ = make_action_btn(tr("Metadata..."));
    template_btn_ = make_action_btn(tr("Templates..."));
    theme_btn_ = make_action_btn(tr("Theme..."));

    connect(meta_btn_, &QPushButton::clicked, this, &ComponentToolbar::metadata_requested);
    connect(template_btn_, &QPushButton::clicked, this, &ComponentToolbar::templates_requested);
    connect(theme_btn_, &QPushButton::clicked, this, &ComponentToolbar::theme_requested);

    // Separator
    auto* sep0 = new QFrame;
    sep0->setFixedHeight(1);
    sep0->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
    vl->addWidget(sep0);

    // ── Add Component ─────────────────────────────────────────────────────────
    sec_add_ = make_section(tr("ADD COMPONENT"));

    // Type ids are stable data keys; the visible label comes from
    // component_type_label() so it can be localized + retranslated.
    const char* type_ids[] = {"heading",  "text",        "table",   "image",  "chart",
                              "sparkline", "stats_block", "callout", "code",   "divider",
                              "quote",     "list",        "market_data", "page_break", "toc"};

    for (const char* type : type_ids) {
        add_type_button(vl, QString::fromLatin1(type));
    }

    // Separator
    auto* sep1 = new QFrame;
    sep1->setFixedHeight(1);
    sep1->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
    vl->addWidget(sep1);

    // ── Font Settings ─────────────────────────────────────────────────────────
    sec_font_ = make_section(tr("FONT"));

    font_combo_ = new QFontComboBox;
    font_combo_->setCurrentFont(QFont("Segoe UI"));
    vl->addWidget(font_combo_);

    auto* size_row = new QWidget(this);
    size_row->setStyleSheet("background: transparent;");
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
    sep2->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
    vl->addWidget(sep2);

    // ── Document Structure ────────────────────────────────────────────────────
    sec_structure_ = make_section(tr("STRUCTURE"));

    structure_list_ = new QListWidget;
    structure_list_->setMinimumHeight(120);
    structure_list_->setStyleSheet(
        QString("QListWidget { background: %1; border: 1px solid %2; } "
                "QListWidget::item { color: %3; padding: 3px; font-size: 11px; } "
                "QListWidget::item:selected { background: %4; color: %5; }")
            .arg(ui::colors::DARK(), ui::colors::BORDER(), ui::colors::GRAY(), ui::colors::BG_RAISED(), ui::colors::WHITE()));
    connect(structure_list_, &QListWidget::currentRowChanged, this, &ComponentToolbar::structure_selected);
    vl->addWidget(structure_list_, 1);

    // Structure action buttons
    auto* action_row = new QWidget(this);
    action_row->setStyleSheet("background: transparent;");
    auto* arl = new QHBoxLayout(action_row);
    arl->setContentsMargins(0, 0, 0, 0);
    arl->setSpacing(2);

    auto make_small = [&](const QString& text) {
        auto* b = new QPushButton(text);
        b->setFixedHeight(24);
        arl->addWidget(b);
        return b;
    };

    up_btn_ = make_small(tr("Up"));
    dn_btn_ = make_small(tr("Dn"));
    dup_btn_ = make_small(tr("Dup"));
    del_btn_ = make_small(tr("Del"));

    connect(up_btn_, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r > 0)
            emit move_up(r);
    });
    connect(dn_btn_, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r >= 0)
            emit move_down(r);
    });
    connect(dup_btn_, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r >= 0)
            emit duplicate(r);
    });
    connect(del_btn_, &QPushButton::clicked, this, [this]() {
        int r = structure_list_->currentRow();
        if (r >= 0)
            emit delete_item(r);
    });

    vl->addWidget(action_row);
    vl->addStretch();

    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

void ComponentToolbar::add_type_button(QVBoxLayout* layout, const QString& type) {
    auto* btn = new QPushButton(component_type_label(type));
    btn->setFixedHeight(26);
    btn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                "text-align: left; padding: 0 8px; font-size: 12px; }"
                "QPushButton:hover { background: %4; color: %5; }")
            .arg(ui::colors::DARK(), ui::colors::GRAY(), ui::colors::BORDER(), ui::colors::BG_RAISED(), ui::colors::WHITE()));
    connect(btn, &QPushButton::clicked, this, [this, type]() { emit add_component(type); });
    type_buttons_.insert(type, btn);
    layout->addWidget(btn);
}

void ComponentToolbar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ComponentToolbar::retranslateUi() {
    if (sec_file_) sec_file_->setText(tr("FILE"));
    if (sec_document_) sec_document_->setText(tr("DOCUMENT"));
    if (sec_add_) sec_add_->setText(tr("ADD COMPONENT"));
    if (sec_font_) sec_font_->setText(tr("FONT"));
    if (sec_structure_) sec_structure_->setText(tr("STRUCTURE"));

    if (new_btn_) new_btn_->setText(tr("New Report"));
    if (open_btn_) open_btn_->setText(tr("Open..."));
    if (recent_btn_) recent_btn_->setText(tr("Recent Reports..."));
    if (meta_btn_) meta_btn_->setText(tr("Metadata..."));
    if (template_btn_) template_btn_->setText(tr("Templates..."));
    if (theme_btn_) theme_btn_->setText(tr("Theme..."));

    if (up_btn_) up_btn_->setText(tr("Up"));
    if (dn_btn_) dn_btn_->setText(tr("Dn"));
    if (dup_btn_) dup_btn_->setText(tr("Dup"));
    if (del_btn_) del_btn_->setText(tr("Del"));

    // Component "add" buttons — re-label by stable type id.
    for (auto it = type_buttons_.constBegin(); it != type_buttons_.constEnd(); ++it) {
        if (it.value())
            it.value()->setText(component_type_label(it.key()));
    }
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
