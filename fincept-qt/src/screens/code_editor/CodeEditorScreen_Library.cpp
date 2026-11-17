// src/screens/code_editor/CodeEditorScreen_Library.cpp — the rail's LIBRARY page
// and the start view. See CodeEditorScreen_Library.h. Styled by the screen sheet.

#include "screens/code_editor/CodeEditorScreen_Library.h"

#include "screens/code_editor/NotebookDocument.h"
#include "screens/code_editor/NotebookStyle.h"
#include "services/notebooks/NotebookLibraryService.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

using services::NotebookCatalogEntry;
using services::NotebookLibraryService;

namespace {

constexpr int kRoleKind = Qt::UserRole + 1;  ///< "template" | "path" | "section"
constexpr int kRoleValue = Qt::UserRole + 2; ///< catalog index or path
constexpr int kRoleSearch = Qt::UserRole + 3;
constexpr qint64 kPreviewMaxBytes = 3 * 1024 * 1024;

/// Catalog indices: featured first, then by category and title.
QVector<int> ordered_catalog() {
    const auto& cat = NotebookLibraryService::instance().catalog();
    QVector<int> idx;
    for (int i = 0; i < cat.size(); ++i)
        idx.append(i);
    std::stable_sort(idx.begin(), idx.end(), [&cat](int a, int b) {
        if (cat[a].featured != cat[b].featured)
            return cat[a].featured;
        return cat[a].category < cat[b].category;
    });
    return idx;
}

QString template_meta(const NotebookCatalogEntry& e) {
    QStringList parts{e.category.toUpper(), e.difficulty.toUpper(),
                      NotebookLibraryPanel::tr("~%1 MIN").arg(e.est_minutes)};
    if (!e.requirements.isEmpty())
        parts << e.requirements;
    if (e.network)
        parts << NotebookLibraryPanel::tr("downloads market data");
    return parts.join(QStringLiteral("  ·  "));
}

/// First headings of a notebook file, for the preview.
QStringList headings_of(const QString& path, int max) {
    QFile f(path);
    if (f.size() > kPreviewMaxBytes || !f.open(QIODevice::ReadOnly))
        return {};
    notebook::NotebookDoc doc;
    if (!notebook::parse_ipynb(f.readAll(), &doc))
        return {};
    QStringList out;
    for (const auto& e : notebook::outline(doc)) {
        if (e.level >= 1 && e.level <= 2)
            out << e.text;
        if (out.size() >= max)
            break;
    }
    return out;
}

} // namespace

QString nb_relative_time(const QDateTime& when) {
    if (!when.isValid())
        return {};
    const qint64 secs = when.secsTo(QDateTime::currentDateTime());
    if (secs < 60)
        return NotebookLibraryPanel::tr("just now");
    if (secs < 3600)
        return NotebookLibraryPanel::tr("%1 min ago").arg(secs / 60);
    if (secs < 86400)
        return NotebookLibraryPanel::tr("%1 h ago").arg(secs / 3600);
    if (secs < 2 * 86400)
        return NotebookLibraryPanel::tr("yesterday");
    return QLocale().toString(when.date(), QStringLiteral("d MMM"));
}

// ═════════════════════════════════════════════════════════════════════════════
// NotebookLibraryPanel

NotebookLibraryPanel::NotebookLibraryPanel(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 6, 0, 0);
    v->setSpacing(6);

    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("nbSearch"));
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this, &NotebookLibraryPanel::apply_filter);
    auto* sr = new QHBoxLayout;
    sr->setContentsMargins(8, 0, 8, 0);
    sr->addWidget(search_);
    v->addLayout(sr);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("nbTree"));
    tree_->setHeaderHidden(true);
    tree_->setColumnCount(1);
    tree_->setRootIsDecorated(false);
    tree_->setIndentation(10);
    tree_->setUniformRowHeights(true);
    tree_->setTextElideMode(Qt::ElideRight);
    tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(tree_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* cur, QTreeWidgetItem*) { show_preview(cur); });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) { open_item(item); });
    v->addWidget(tree_, 1);

    preview_ = new QWidget(this);
    preview_->setObjectName(QStringLiteral("nbPanelHead"));
    preview_->setAttribute(Qt::WA_StyledBackground, true);
    auto* pv = new QVBoxLayout(preview_);
    pv->setContentsMargins(10, 8, 10, 10);
    pv->setSpacing(4);
    preview_title_ = new QLabel(preview_);
    preview_title_->setObjectName(QStringLiteral("nbCardTitle"));
    preview_title_->setWordWrap(true);
    pv->addWidget(preview_title_);
    preview_meta_ = new QLabel(preview_);
    preview_meta_->setObjectName(QStringLiteral("nbDim"));
    preview_meta_->setWordWrap(true);
    pv->addWidget(preview_meta_);
    preview_text_ = new QLabel(preview_);
    preview_text_->setObjectName(QStringLiteral("nbCardText"));
    preview_text_->setWordWrap(true);
    preview_text_->setTextFormat(Qt::PlainText);
    pv->addWidget(preview_text_);
    preview_open_ = new QPushButton(preview_);
    preview_open_->setObjectName(QStringLiteral("nbPrimary"));
    preview_open_->setCursor(Qt::PointingHandCursor);
    connect(preview_open_, &QPushButton::clicked, this, [this]() { open_item(tree_->currentItem()); });
    pv->addWidget(preview_open_, 0, Qt::AlignLeft);
    preview_->setVisible(false);
    v->addWidget(preview_);
    retranslate();
}

void NotebookLibraryPanel::refresh(const QVector<RecentNotebook>& recent) {
    recent_ = recent;
    rebuild();
}

void NotebookLibraryPanel::rebuild() {
    tree_->setUpdatesEnabled(false);
    tree_->clear();
    auto section = [this](const QString& title) {
        auto* s = new QTreeWidgetItem(tree_, {title});
        s->setFlags(Qt::ItemIsEnabled);
        s->setData(0, kRoleKind, QStringLiteral("section"));
        QFont f = s->font(0);
        f.setBold(true);
        s->setFont(0, f);
        s->setForeground(0, QColor(ui::colors::TEXT_TERTIARY()));
        s->setExpanded(true);
        return s;
    };
    auto& lib = NotebookLibraryService::instance();
    if (!recent_.isEmpty()) {
        QTreeWidgetItem* s = section(tr("RECENT"));
        for (const RecentNotebook& r : recent_) {
            auto* it = new QTreeWidgetItem(s, {lib.display_name_for(r.path)});
            it->setData(0, kRoleKind, QStringLiteral("path"));
            it->setData(0, kRoleValue, r.path);
            it->setData(0, kRoleSearch, it->text(0) + QLatin1Char(' ') + r.path);
            it->setToolTip(0, QStringLiteral("%1\n%2").arg(QDir::toNativeSeparators(r.path), nb_relative_time(r.opened)));
        }
    }
    const auto& cat = lib.catalog();
    QMap<QString, QTreeWidgetItem*> by_category;
    for (const int i : ordered_catalog()) {
        const NotebookCatalogEntry& e = cat[i];
        const QString key = e.featured ? tr("FEATURED") : e.category.toUpper();
        QTreeWidgetItem*& s = by_category[key];
        if (!s)
            s = section(tr("TEMPLATES · %1").arg(key));
        auto* it = new QTreeWidgetItem(s, {e.title});
        it->setData(0, kRoleKind, QStringLiteral("template"));
        it->setData(0, kRoleValue, i);
        it->setData(0, kRoleSearch,
                    QStringList{e.title, e.category, e.difficulty, e.summary, e.tags.join(QLatin1Char(' '))}.join(' '));
        it->setToolTip(0, e.summary);
    }
    const auto saved = lib.saved_notebooks();
    if (!saved.isEmpty()) {
        QTreeWidgetItem* s = section(tr("MY NOTEBOOKS"));
        int n = 0;
        for (const auto& f : saved) {
            if (++n > 40)
                break;
            QString label = f.name;
            if (f.from_template)
                if (const NotebookCatalogEntry* t = lib.template_for(f.path))
                    label = tr("%1 (your copy)").arg(t->title);
            auto* it = new QTreeWidgetItem(s, {label});
            it->setData(0, kRoleKind, QStringLiteral("path"));
            it->setData(0, kRoleValue, f.path);
            it->setData(0, kRoleSearch, label + QLatin1Char(' ') + f.name);
            it->setToolTip(0, QStringLiteral("%1\n%2").arg(f.name, nb_relative_time(f.modified)));
        }
    }
    if (cat.isEmpty()) {
        QTreeWidgetItem* s = section(tr("TEMPLATES"));
        auto* it = new QTreeWidgetItem(s, {tr("The template library is missing from this installation.")});
        it->setFlags(Qt::ItemIsEnabled);
    }
    tree_->setUpdatesEnabled(true);
    apply_filter(search_->text());
}

void NotebookLibraryPanel::apply_filter(const QString& text) {
    const QString needle = text.trimmed();
    for (int s = 0; s < tree_->topLevelItemCount(); ++s) {
        QTreeWidgetItem* sec = tree_->topLevelItem(s);
        int visible = 0;
        for (int c = 0; c < sec->childCount(); ++c) {
            QTreeWidgetItem* it = sec->child(c);
            const bool show = needle.isEmpty() || it->data(0, kRoleSearch).toString().contains(needle, Qt::CaseInsensitive);
            it->setHidden(!show);
            visible += show ? 1 : 0;
        }
        sec->setHidden(visible == 0 && sec->childCount() > 0);
    }
}

void NotebookLibraryPanel::show_preview(QTreeWidgetItem* item) {
    const QString kind = item ? item->data(0, kRoleKind).toString() : QString();
    if (kind != QLatin1String("template") && kind != QLatin1String("path")) {
        preview_->setVisible(false);
        return;
    }
    auto& lib = NotebookLibraryService::instance();
    QStringList heads;
    if (kind == QLatin1String("template")) {
        const auto& cat = lib.catalog();
        const int i = item->data(0, kRoleValue).toInt();
        if (i < 0 || i >= cat.size())
            return;
        const NotebookCatalogEntry& e = cat[i];
        preview_title_->setText(e.title);
        preview_meta_->setText(template_meta(e));
        heads = headings_of(lib.asset_path(e), 6);
        QString text = e.summary;
        if (!e.tags.isEmpty())
            text += QStringLiteral("\n") + tr("Tags: %1").arg(e.tags.join(QStringLiteral(", ")));
        if (!heads.isEmpty())
            text += QStringLiteral("\n") + tr("Sections: %1").arg(heads.join(QStringLiteral(" · ")));
        preview_text_->setText(text);
        preview_open_->setText(tr("OPEN A COPY"));
        preview_open_->setToolTip(tr("Opens your own copy; the template stays as shipped. Nothing runs until you run it."));
    } else {
        const QString path = item->data(0, kRoleValue).toString();
        const QFileInfo fi(path);
        preview_title_->setText(item->text(0));
        preview_meta_->setText(tr("%1  ·  modified %2").arg(QDir::toNativeSeparators(fi.absolutePath()),
                                                            nb_relative_time(fi.lastModified())));
        heads = headings_of(path, 6);
        preview_text_->setText(heads.isEmpty() ? tr("No headings.")
                                               : tr("Sections: %1").arg(heads.join(QStringLiteral(" · "))));
        preview_open_->setText(tr("OPEN"));
        preview_open_->setToolTip(tr("Opens the notebook with its saved outputs. Nothing runs until you run it."));
    }
    preview_->setVisible(true);
}

void NotebookLibraryPanel::open_item(QTreeWidgetItem* item) {
    if (!item)
        return;
    const QString kind = item->data(0, kRoleKind).toString();
    if (kind == QLatin1String("template"))
        emit open_template(item->data(0, kRoleValue).toInt());
    else if (kind == QLatin1String("path"))
        emit open_path(item->data(0, kRoleValue).toString());
}

void NotebookLibraryPanel::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        rebuild();
    }
    QWidget::changeEvent(e);
}

void NotebookLibraryPanel::retranslate() {
    search_->setPlaceholderText(tr("Filter notebooks and templates…"));
    search_->setAccessibleName(tr("Filter the library"));
    tree_->setAccessibleName(tr("Notebook library"));
}

// ═════════════════════════════════════════════════════════════════════════════
// NotebookStartView

NotebookStartView::NotebookStartView(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("nbStartBody"));
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(24, 20, 24, 24);
    v->setSpacing(14);

    // Hero: what this is, and the two ways in.
    auto* hero = new QHBoxLayout;
    hero->setSpacing(10);
    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(2);
    hero_ = new QLabel(body);
    hero_->setObjectName(QStringLiteral("nbHero"));
    hero_text->addWidget(hero_);
    hero_sub_ = new QLabel(body);
    hero_sub_->setObjectName(QStringLiteral("nbHeroSub"));
    hero_sub_->setWordWrap(true);
    hero_text->addWidget(hero_sub_);
    hero->addLayout(hero_text, 1);
    new_btn_ = new QPushButton(body);
    new_btn_->setObjectName(QStringLiteral("nbPrimary"));
    new_btn_->setCursor(Qt::PointingHandCursor);
    connect(new_btn_, &QPushButton::clicked, this, &NotebookStartView::new_requested);
    hero->addWidget(new_btn_, 0, Qt::AlignVCenter);
    open_btn_ = new QPushButton(body);
    open_btn_->setObjectName(QStringLiteral("nbSecondary"));
    open_btn_->setCursor(Qt::PointingHandCursor);
    connect(open_btn_, &QPushButton::clicked, this, &NotebookStartView::open_requested);
    hero->addWidget(open_btn_, 0, Qt::AlignVCenter);
    v->addLayout(hero);

    // Restore strip for a notebook that was never saved.
    recovery_ = new QFrame(body);
    recovery_->setObjectName(QStringLiteral("nbCard"));
    auto* rh = new QHBoxLayout(recovery_);
    rh->setContentsMargins(12, 8, 12, 8);
    recovery_text_ = new QLabel(recovery_);
    recovery_text_->setObjectName(QStringLiteral("nbCardText"));
    recovery_text_->setProperty("tone", QStringLiteral("warn"));
    recovery_text_->setWordWrap(true);
    rh->addWidget(recovery_text_, 1);
    recovery_btn_ = new QPushButton(recovery_);
    recovery_btn_->setObjectName(QStringLiteral("nbSecondary"));
    recovery_btn_->setCursor(Qt::PointingHandCursor);
    connect(recovery_btn_, &QPushButton::clicked, this, &NotebookStartView::recover_requested);
    rh->addWidget(recovery_btn_);
    recovery_->setVisible(false);
    v->addWidget(recovery_);

    // Recent notebooks.
    recent_box_ = new QWidget(body);
    auto* rv = new QVBoxLayout(recent_box_);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    NbPanelHeader rhd = nb_panel_header(QString(), recent_box_);
    recent_title_ = rhd.title;
    rv->addWidget(rhd.header);
    recent_list_ = new QListWidget(recent_box_);
    recent_list_->setObjectName(QStringLiteral("nbList"));
    recent_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recent_list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recent_list_->setCursor(Qt::PointingHandCursor);
    connect(recent_list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* it) { emit open_path(it->data(Qt::UserRole).toString()); });
    rv->addWidget(recent_list_);
    recent_box_->setVisible(false);
    v->addWidget(recent_box_);

    // Templates as cards.
    NbPanelHeader thd = nb_panel_header(QString(), body);
    templates_title_ = thd.title;
    templates_caption_ = new QLabel(thd.header);
    templates_caption_->setObjectName(QStringLiteral("nbDim"));
    thd.controls->addWidget(templates_caption_);
    v->addWidget(thd.header);
    cards_host_ = new QWidget(body);
    cards_ = new QGridLayout(cards_host_);
    cards_->setContentsMargins(0, 0, 0, 0);
    cards_->setHorizontalSpacing(10);
    cards_->setVerticalSpacing(10);
    v->addWidget(cards_host_);

    keys_ = new QLabel(body);
    keys_->setObjectName(QStringLiteral("nbDim"));
    keys_->setWordWrap(true);
    v->addWidget(keys_);
    v->addStretch(1);
    scroll->setWidget(body);

    build_cards();
    retranslate();
}

void NotebookStartView::build_cards() {
    for (QWidget* w : card_widgets_)
        w->deleteLater();
    card_widgets_.clear();
    const auto& cat = NotebookLibraryService::instance().catalog();
    for (const int i : ordered_catalog()) {
        const NotebookCatalogEntry& e = cat[i];
        auto* card = new QFrame(cards_host_);
        card->setObjectName(QStringLiteral("nbCard"));
        card->setCursor(Qt::PointingHandCursor);
        card->setProperty("catalog_index", i);
        card->installEventFilter(this);
        card->setToolTip(tr("Open your own copy of \"%1\". Nothing runs until you run it.").arg(e.title));
        auto* cv = new QVBoxLayout(card);
        cv->setContentsMargins(12, 10, 12, 10);
        cv->setSpacing(6);
        auto* chips = new QHBoxLayout;
        chips->setSpacing(6);
        auto chip = [&](const QString& text, bool accent) {
            auto* l = new QLabel(text, card);
            l->setObjectName(QStringLiteral("nbChip"));
            if (accent)
                l->setProperty("tone", QStringLiteral("accent"));
            chips->addWidget(l);
        };
        chip(e.category.toUpper(), true);
        chip(e.difficulty.toUpper(), false);
        if (e.network)
            chip(tr("LIVE DATA"), false);
        chips->addStretch(1);
        auto* mins = new QLabel(tr("~%1 min").arg(e.est_minutes), card);
        mins->setObjectName(QStringLiteral("nbDim"));
        chips->addWidget(mins);
        cv->addLayout(chips);
        auto* title = new QLabel(e.title, card);
        title->setObjectName(QStringLiteral("nbCardTitle"));
        title->setWordWrap(true);
        cv->addWidget(title);
        auto* summary = new QLabel(e.summary, card);
        summary->setObjectName(QStringLiteral("nbCardText"));
        summary->setWordWrap(true);
        cv->addWidget(summary);
        cv->addStretch(1);
        auto* foot = new QHBoxLayout;
        auto* req = new QLabel(e.tags.isEmpty() ? e.requirements : e.tags.join(QStringLiteral(" · ")), card);
        req->setObjectName(QStringLiteral("nbDim"));
        foot->addWidget(req, 1);
        auto* open = new QPushButton(tr("OPEN"), card);
        open->setObjectName(QStringLiteral("nbPrimary"));
        open->setCursor(Qt::PointingHandCursor);
        connect(open, &QPushButton::clicked, this, [this, i]() { emit open_template(i); });
        foot->addWidget(open);
        cv->addLayout(foot);
        card_widgets_.append(card);
    }
    columns_ = 0;
    reflow();
}

void NotebookStartView::reflow() {
    const int avail = std::max(1, cards_host_->width() > 0 ? cards_host_->width() : width() - 48);
    const int min_w = ui::metrics::width(290);
    const int cols = std::clamp((avail + 10) / (min_w + 10), 1, 4);
    if (cols == columns_)
        return;
    columns_ = cols;
    for (QWidget* w : card_widgets_)
        cards_->removeWidget(w);
    for (int i = 0; i < card_widgets_.size(); ++i)
        cards_->addWidget(card_widgets_[i], i / cols, i % cols);
    for (int c = 0; c < 4; ++c)
        cards_->setColumnStretch(c, c < cols ? 1 : 0);
}

void NotebookStartView::refresh(const QVector<RecentNotebook>& recent, const QDateTime& recovery_saved_at) {
    recovery_at_ = recovery_saved_at;
    recovery_->setVisible(recovery_saved_at.isValid());
    auto& lib = NotebookLibraryService::instance();
    recent_list_->clear();
    int n = 0;
    for (const RecentNotebook& r : recent) {
        if (!QFileInfo::exists(r.path))
            continue;
        if (++n > 8)
            break;
        const QFileInfo fi(r.path);
        auto* it = new QListWidgetItem(QStringLiteral("%1      %2  ·  %3")
                                           .arg(lib.display_name_for(r.path),
                                                QDir::toNativeSeparators(fi.absolutePath()), nb_relative_time(r.opened)),
                                       recent_list_);
        it->setData(Qt::UserRole, r.path);
        it->setToolTip(QDir::toNativeSeparators(r.path));
    }
    recent_box_->setVisible(n > 0);
    const int row_h = ui::metrics::row_height(-2, 24, 10);
    recent_list_->setFixedHeight(std::max(1, std::min(n, 8)) * row_h + 4);
    retranslate();
}

void NotebookStartView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    reflow();
}

bool NotebookStartView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        const QVariant idx = watched->property("catalog_index");
        if (idx.isValid()) {
            emit open_template(idx.toInt());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NotebookStartView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        build_cards();
    }
    QWidget::changeEvent(e);
}

void NotebookStartView::retranslate() {
    hero_->setText(tr("FINCEPT NOTEBOOK"));
    hero_sub_->setText(tr("Python research notebooks for market data. A live kernel keeps your variables between "
                          "cells; DataFrames show as sortable tables and matplotlib charts appear inline."));
    new_btn_->setText(tr("NEW NOTEBOOK"));
    new_btn_->setToolTip(tr("Start an empty notebook (Ctrl+N)"));
    open_btn_->setText(tr("OPEN…"));
    open_btn_->setToolTip(tr("Open an .ipynb file (Ctrl+O)"));
    recovery_text_->setText(tr("A notebook you had not saved was kept when the terminal closed (%1).")
                                .arg(QLocale().toString(recovery_at_, QLocale::ShortFormat)));
    recovery_btn_->setText(tr("RESTORE"));
    recent_title_->setText(tr("RECENT"));
    templates_title_->setText(tr("START FROM A TEMPLATE"));
    const int n = static_cast<int>(NotebookLibraryService::instance().catalog().size());
    templates_caption_->setText(n > 0 ? tr("%n template(s) · opening one makes your own copy", "", n)
                                      : tr("The template library is missing from this installation"));
    keys_->setText(tr("Keys:  Ctrl+Enter run  ·  Shift+Enter run and move on  ·  Alt+Enter run and insert  ·  "
                      "Esc command mode  ·  A / B insert above / below  ·  D, D delete  ·  M / Y text / code  ·  "
                      "I, I interrupt  ·  0, 0 restart  ·  Ctrl+F find  ·  Ctrl+I help for the name under the cursor"));
}

} // namespace fincept::screens
