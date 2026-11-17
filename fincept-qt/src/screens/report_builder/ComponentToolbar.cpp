#include "screens/report_builder/ComponentToolbar.h"

#include "ui/theme/Theme.h"

#include <QFontComboBox>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace {

constexpr int kCtIdRole = Qt::UserRole + 1;

// Translatable display label for a component type id. The id is the stable data key
// emitted via add_component(); only the label is localized.
QString ct_type_label(const QString& type) {
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
    if (type == "cover")
        return ComponentToolbar::tr("Cover Page");
    return type;
}

} // namespace

ComponentToolbar::ComponentToolbar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("rbComponents"));
    setMinimumWidth(0);
    setMaximumWidth(preferred_width());
    // One selector-scoped sheet, set before the children exist. The border belongs to
    // this panel only — the old selector-less "border-right" framed every label and row.
    setStyleSheet(
        QStringLiteral(
            "#rbComponents { background: %1; border-right: 1px solid %2; }"
            "#rbComponents QScrollArea { background: transparent; border: none; }"
            "#rbComponents QLabel { background: transparent; color: %3; }"
            "#rbComponents QLabel[rbRole=\"section\"] { color: %3; font-weight: bold; font-size: %6px; padding-top: 6px; }"
            "#rbComponents QPushButton { background: %4; color: %9; border: 1px solid %5; padding: 2px 8px;"
            " font-size: %7px; }"
            "#rbComponents QPushButton:hover { background: %8; }"
            "#rbComponents QPushButton:disabled { color: %3; }"
            "#rbComponents QPushButton[rbRole=\"action\"] { text-align: left; color: %10; }"
            "#rbComponents QPushButton[rbRole=\"add\"] { text-align: left; background: %11; color: %12; border-color: %2; }"
            "#rbComponents QPushButton[rbRole=\"add\"]:hover { background: %4; color: %9; }"
            "#rbComponents QPushButton:checked { background: %10; color: %13; border-color: %10; }"
            "#rbComponents QListView { background: %11; color: %12; border: 1px solid %2; font-size: %6px; }"
            "#rbComponents QListView::item { padding: 3px; }"
            "#rbComponents QListView::item:selected { background: %4; color: %9; }"
            "#rbComponents QFrame[rbRole=\"rule\"] { background: %2; border: none; }")
            .arg(ui::colors::PANEL(), ui::colors::BORDER(), ui::colors::MUTED(), ui::colors::BG_RAISED(),
                 ui::colors::BORDER_MED(), ui::fonts::px(-3), ui::fonts::px(-2), ui::colors::BG_HOVER(),
                 ui::colors::TEXT_PRIMARY())
            .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_ON_ACCENT()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 1, 0); // keep the border line visible
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("rbComponentsContent"));
    content->setAttribute(Qt::WA_TranslucentBackground);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(4);
    scroll->setWidget(content);

    auto rule = [&]() {
        auto* f = new QFrame(content);
        f->setFixedHeight(1);
        f->setProperty("rbRole", QStringLiteral("rule"));
        vl->addWidget(f);
    };

    // ── Report ───────────────────────────────────────────────────────────────
    sec_file_ = make_section(vl, tr("REPORT"));
    new_btn_ = make_action(vl, tr("New report"));
    library_btn_ = make_action(vl, tr("Report library…"));
    open_btn_ = make_action(vl, tr("Open file…"));
    recent_btn_ = make_action(vl, tr("Recent files…"));
    connect(new_btn_, &QPushButton::clicked, this, &ComponentToolbar::new_report_requested);
    connect(library_btn_, &QPushButton::clicked, this, &ComponentToolbar::library_requested);
    connect(open_btn_, &QPushButton::clicked, this, &ComponentToolbar::open_report_requested);
    connect(recent_btn_, &QPushButton::clicked, this, &ComponentToolbar::recent_reports_requested);

    // ── Document ─────────────────────────────────────────────────────────────
    sec_document_ = make_section(vl, tr("DOCUMENT"));
    meta_btn_ = make_action(vl, tr("Document info…"));
    page_btn_ = make_action(vl, tr("Page setup…"));
    template_btn_ = make_action(vl, tr("Templates…"));
    save_tpl_btn_ = make_action(vl, tr("Save as template…"));
    theme_btn_ = make_action(vl, tr("Theme…"));
    vars_btn_ = make_action(vl, tr("Variables…"));
    schedule_btn_ = make_action(vl, tr("Schedule…"));
    connect(meta_btn_, &QPushButton::clicked, this, &ComponentToolbar::metadata_requested);
    connect(page_btn_, &QPushButton::clicked, this, &ComponentToolbar::page_setup_requested);
    connect(template_btn_, &QPushButton::clicked, this, &ComponentToolbar::templates_requested);
    connect(save_tpl_btn_, &QPushButton::clicked, this, &ComponentToolbar::save_as_template_requested);
    connect(theme_btn_, &QPushButton::clicked, this, &ComponentToolbar::theme_requested);
    connect(vars_btn_, &QPushButton::clicked, this, &ComponentToolbar::variables_requested);
    connect(schedule_btn_, &QPushButton::clicked, this, &ComponentToolbar::schedule_requested);
    rule();

    // ── AI drafting (document actions; block actions are in the Properties panel) ──
    sec_ai_ = make_section(vl, tr("AI DRAFTING"));
    ai_summary_btn_ = make_action(vl, tr("Executive summary…"));
    ai_title_btn_ = make_action(vl, tr("Suggest a title…"));
    connect(ai_summary_btn_, &QPushButton::clicked, this, [this]() { emit ai_document_requested(QStringLiteral("summary")); });
    connect(ai_title_btn_, &QPushButton::clicked, this, [this]() { emit ai_document_requested(QStringLiteral("title")); });
    rule();

    // ── Add ──────────────────────────────────────────────────────────────────
    sec_add_ = make_section(vl, tr("ADD BLOCK"));
    const char* type_ids[] = {"heading",   "text",        "table",       "chart",      "stats_block",
                              "market_data", "image",     "sparkline",   "callout",    "list",
                              "quote",     "code",        "divider",     "page_break", "toc",
                              "cover"};
    for (const char* type : type_ids)
        add_type_button(vl, QString::fromLatin1(type));
    rule();

    // ── Typeface ─────────────────────────────────────────────────────────────
    sec_font_ = make_section(vl, tr("REPORT FONT"));
    // Typeface / size for the report's text — document content (the page's own
    // default typeface and point size), not the terminal's UI font.
    font_combo_ = new QFontComboBox(content);
    vl->addWidget(font_combo_);
    auto* size_row = new QWidget(content);
    size_row->setAttribute(Qt::WA_TranslucentBackground);
    auto* srl = new QHBoxLayout(size_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(4);
    font_size_ = new QSpinBox(size_row);
    font_size_->setRange(8, 72);
    font_size_->setValue(12);
    srl->addWidget(font_size_, 1);
    bold_btn_ = new QPushButton(QStringLiteral("B"), size_row);
    bold_btn_->setCheckable(true);
    italic_btn_ = new QPushButton(QStringLiteral("I"), size_row);
    italic_btn_->setCheckable(true);
    QFont bf = bold_btn_->font();
    bf.setBold(true);
    bold_btn_->setFont(bf);
    QFont itf = italic_btn_->font();
    itf.setItalic(true);
    italic_btn_->setFont(itf);
    srl->addWidget(bold_btn_);
    srl->addWidget(italic_btn_);
    vl->addWidget(size_row);
    auto emit_font = [this]() { emit font_changed(font_combo_->currentFont().family(), font_size_->value()); };
    auto emit_style = [this]() { emit style_changed(bold_btn_->isChecked(), italic_btn_->isChecked()); };
    connect(font_combo_, &QFontComboBox::currentFontChanged, this, emit_font);
    connect(font_size_, &QSpinBox::valueChanged, this, emit_font);
    connect(bold_btn_, &QPushButton::toggled, this, emit_style);
    connect(italic_btn_, &QPushButton::toggled, this, emit_style);
    font_combo_->setToolTip(tr("Report typeface"));
    font_size_->setToolTip(tr("Report body size (pt) — headings and tables scale with it"));
    set_component_style(false, false, false);
    rule();

    // ── Structure (model/view, by id) ────────────────────────────────────────
    sec_structure_ = make_section(vl, tr("STRUCTURE"));
    structure_model_ = new QStandardItemModel(this);
    structure_view_ = new QListView(content);
    structure_view_->setModel(structure_model_);
    structure_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    structure_view_->setUniformItemSizes(true);
    structure_view_->setMinimumHeight(140);
    vl->addWidget(structure_view_, 1);
    connect(structure_view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& cur, const QModelIndex&) {
                if (cur.isValid())
                    emit structure_selected(cur.data(kCtIdRole).toInt());
            });

    auto* action_row = new QWidget(content);
    action_row->setAttribute(Qt::WA_TranslucentBackground);
    auto* arl = new QHBoxLayout(action_row);
    arl->setContentsMargins(0, 0, 0, 0);
    arl->setSpacing(2);
    auto make_small = [&](const QString& text) {
        auto* b = new QPushButton(text, action_row);
        arl->addWidget(b);
        return b;
    };
    up_btn_ = make_small(tr("Up"));
    dn_btn_ = make_small(tr("Down"));
    dup_btn_ = make_small(tr("Copy"));
    del_btn_ = make_small(tr("Delete"));
    connect(up_btn_, &QPushButton::clicked, this, [this]() {
        if (const int id = selected_structure_id())
            emit move_up(id);
    });
    connect(dn_btn_, &QPushButton::clicked, this, [this]() {
        if (const int id = selected_structure_id())
            emit move_down(id);
    });
    connect(dup_btn_, &QPushButton::clicked, this, [this]() {
        if (const int id = selected_structure_id())
            emit duplicate(id);
    });
    connect(del_btn_, &QPushButton::clicked, this, [this]() {
        if (const int id = selected_structure_id())
            emit delete_item(id);
    });
    vl->addWidget(action_row);
}

int ComponentToolbar::preferred_width() const {
    const QFontMetrics fm(ui::fonts::font(-2));
    return std::max(230, fm.averageCharWidth() * 30);
}

QLabel* ComponentToolbar::make_section(QVBoxLayout* vl, const QString& text) {
    auto* lbl = new QLabel(text, vl->parentWidget());
    lbl->setProperty("rbRole", QStringLiteral("section"));
    vl->addWidget(lbl);
    return lbl;
}

QPushButton* ComponentToolbar::make_action(QVBoxLayout* vl, const QString& text) {
    auto* b = new QPushButton(text, vl->parentWidget());
    b->setProperty("rbRole", QStringLiteral("action"));
    b->setCursor(Qt::PointingHandCursor);
    vl->addWidget(b);
    return b;
}

void ComponentToolbar::add_type_button(QVBoxLayout* layout, const QString& type) {
    auto* btn = new QPushButton(ct_type_label(type), layout->parentWidget());
    btn->setProperty("rbRole", QStringLiteral("add"));
    btn->setCursor(Qt::PointingHandCursor);
    connect(btn, &QPushButton::clicked, this, [this, type]() { emit add_component(type); });
    type_buttons_.insert(type, btn);
    layout->addWidget(btn);
}

int ComponentToolbar::selected_structure_id() const {
    const QModelIndex idx = structure_view_->currentIndex();
    return idx.isValid() ? idx.data(kCtIdRole).toInt() : 0;
}

void ComponentToolbar::update_structure(const QVector<StructureItem>& items, int selected_id) {
    const QSignalBlocker block(structure_view_->selectionModel());
    // Rewrite rows in place (keeps the list's scroll position); add / drop the tail.
    while (structure_model_->rowCount() > items.size())
        structure_model_->removeRow(structure_model_->rowCount() - 1);
    for (int i = 0; i < items.size(); ++i) {
        const StructureItem& it = items[i];
        QString text = it.label;
        if (it.unseen)
            text = QStringLiteral("★ ") + text;
        if (it.bound)
            text += it.stale ? QStringLiteral("  ↻!") : QStringLiteral("  ↻");
        QStandardItem* row = structure_model_->item(i);
        if (!row) {
            row = new QStandardItem(text);
            structure_model_->appendRow(row);
        } else if (row->text() != text) {
            row->setText(text);
        }
        row->setData(it.id, kCtIdRole);
        row->setToolTip(it.bound ? (it.stale ? tr("Live data block — out of date, Refresh Data updates it")
                                             : tr("Live data block"))
                                 : QString());
        if (it.id == selected_id)
            structure_view_->setCurrentIndex(structure_model_->index(i, 0));
    }
    if (selected_id <= 0)
        structure_view_->clearSelection();
}

void ComponentToolbar::set_document_font(const QString& family, int size) {
    const QSignalBlocker block_combo(font_combo_);
    const QSignalBlocker block_size(font_size_);
    if (!family.isEmpty() && font_combo_->currentFont().family() != family)
        font_combo_->setCurrentFont(QFont(family));
    if (font_size_->value() != size)
        font_size_->setValue(size);
}

void ComponentToolbar::set_component_style(bool enabled, bool bold, bool italic) {
    const QSignalBlocker block_bold(bold_btn_);
    const QSignalBlocker block_italic(italic_btn_);
    bold_btn_->setEnabled(enabled);
    italic_btn_->setEnabled(enabled);
    bold_btn_->setChecked(enabled && bold);
    italic_btn_->setChecked(enabled && italic);
    const QString tip = enabled ? QString() : tr("Select a text block to make it bold or italic");
    bold_btn_->setToolTip(enabled ? tr("Bold — the selected block") : tip);
    italic_btn_->setToolTip(enabled ? tr("Italic — the selected block") : tip);
}

void ComponentToolbar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ComponentToolbar::retranslateUi() {
    sec_file_->setText(tr("REPORT"));
    sec_document_->setText(tr("DOCUMENT"));
    sec_add_->setText(tr("ADD BLOCK"));
    sec_font_->setText(tr("REPORT FONT"));
    sec_structure_->setText(tr("STRUCTURE"));
    new_btn_->setText(tr("New report"));
    library_btn_->setText(tr("Report library…"));
    open_btn_->setText(tr("Open file…"));
    recent_btn_->setText(tr("Recent files…"));
    meta_btn_->setText(tr("Document info…"));
    page_btn_->setText(tr("Page setup…"));
    template_btn_->setText(tr("Templates…"));
    save_tpl_btn_->setText(tr("Save as template…"));
    theme_btn_->setText(tr("Theme…"));
    vars_btn_->setText(tr("Variables…"));
    schedule_btn_->setText(tr("Schedule…"));
    sec_ai_->setText(tr("AI DRAFTING"));
    ai_summary_btn_->setText(tr("Executive summary…"));
    ai_title_btn_->setText(tr("Suggest a title…"));
    vars_btn_->setToolTip(tr("{{symbol}}, {{period}} and your own values, usable in any text"));
    schedule_btn_->setToolTip(tr("Refresh and export this report automatically while the terminal is open"));
    ai_summary_btn_->setToolTip(tr("Draft an executive summary from the report (you review it before it is added)"));
    ai_title_btn_->setToolTip(tr("Three title ideas from the report's content (nothing changes until you pick one)"));
    up_btn_->setText(tr("Up"));
    dn_btn_->setText(tr("Down"));
    dup_btn_->setText(tr("Copy"));
    del_btn_->setText(tr("Delete"));
    font_combo_->setToolTip(tr("Report typeface"));
    font_size_->setToolTip(tr("Report body size (pt) — headings and tables scale with it"));
    set_component_style(bold_btn_->isEnabled(), bold_btn_->isChecked(), italic_btn_->isChecked());
    for (auto it = type_buttons_.constBegin(); it != type_buttons_.constEnd(); ++it)
        it.value()->setText(ct_type_label(it.key()));
}

} // namespace fincept::screens
