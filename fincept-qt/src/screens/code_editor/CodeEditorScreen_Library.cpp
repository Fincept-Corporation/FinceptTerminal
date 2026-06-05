// src/screens/code_editor/CodeEditorScreen_Library.cpp
// Fincept Notebook — Library view: prebuilt finance notebooks as filterable
// cards (Marketplace-style), wired to NotebookLibraryService + the editor.

#include "screens/code_editor/CodeEditorScreen.h"

#include "core/logging/Logger.h"
#include "services/notebooks/NotebookLibraryService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

using namespace fincept::ui;
using fincept::services::NotebookCatalogEntry;
using fincept::services::NotebookLibraryService;

namespace {

constexpr int kCardMinWidth = 300;
constexpr int kCardSpacing = 12;

int compute_columns(int viewport_width) {
    const int usable = viewport_width - 28; // grid margins
    int cols = (usable + kCardSpacing) / (kCardMinWidth + kCardSpacing);
    return std::clamp(cols, 1, 4);
}

// Object name for a difficulty badge → maps to the colour rules in kStyle().
const char* difficulty_badge_object(const QString& difficulty) {
    if (difficulty.compare("Beginner", Qt::CaseInsensitive) == 0)
        return "nbDiffBeginner";
    if (difficulty.compare("Hard", Qt::CaseInsensitive) == 0)
        return "nbDiffHard";
    return "nbDiffIntermediate";
}

} // namespace

// ── Page construction ───────────────────────────────────────────────────────

QWidget* CodeEditorScreen::build_library_page() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar row: descriptive label + count.
    auto* toolbar = new QWidget(page);
    toolbar->setFixedHeight(34);
    auto* tl = new QHBoxLayout(toolbar);
    tl->setContentsMargins(14, 0, 14, 0);
    lib_toolbar_lbl_ = new QLabel(tr("FINCEPT NOTEBOOK LIBRARY — curated finance, economics, trading, "
                                     "investing, portfolio & quant notebooks"),
                                  toolbar);
    lib_toolbar_lbl_->setObjectName("nbLibLabel");
    lib_toolbar_lbl_->setStyleSheet(QString("font-family:%1; font-size:%2px; font-weight:700; letter-spacing:0.3px;")
                                        .arg(fonts::DATA_FAMILY)
                                        .arg(fonts::TINY));
    tl->addWidget(lib_toolbar_lbl_);
    tl->addStretch(1);
    lib_count_lbl_ = new QLabel(toolbar);
    lib_count_lbl_->setObjectName("nbLibLabel");
    lib_count_lbl_->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::TINY));
    tl->addWidget(lib_count_lbl_);
    root->addWidget(toolbar);

    // Filter chips: category row + difficulty row.
    auto make_chip_row = [&](const QStringList& labels, QVector<QPushButton*>& store, bool is_category) {
        auto* row = new QWidget(page);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(14, 2, 14, 4);
        rl->setSpacing(6);
        for (const QString& label : labels) {
            auto* chip = new QPushButton(label, row);
            chip->setObjectName("nbChip");
            chip->setCursor(Qt::PointingHandCursor);
            chip->setProperty("active", label == "All");
            chip->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::TINY));
            connect(chip, &QPushButton::clicked, this, [this, label, is_category]() {
                if (is_category)
                    lib_category_filter_ = label;
                else
                    lib_difficulty_filter_ = label;
                // Re-read the member group through `this` (capturing the local
                // reference parameter would dangle once build_library_page returns).
                QVector<QPushButton*>& group = is_category ? cat_chips_ : diff_chips_;
                for (QPushButton* c : group) {
                    c->setProperty("active", c->text() == label);
                    c->style()->unpolish(c);
                    c->style()->polish(c);
                }
                populate_library();
            });
            store.append(chip);
            rl->addWidget(chip);
        }
        rl->addStretch(1);
        root->addWidget(row);
    };
    make_chip_row({tr("All"), tr("Finance"), tr("Economics"), tr("Trading"), tr("Investing"), tr("Portfolio"),
                   tr("Quant")},
                  cat_chips_, true);
    make_chip_row({tr("All"), tr("Beginner"), tr("Intermediate"), tr("Hard")}, diff_chips_, false);

    // Scrollable card grid.
    lib_scroll_ = new QScrollArea(page);
    lib_scroll_->setWidgetResizable(true);
    lib_scroll_->setFrameShape(QFrame::NoFrame);
    lib_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    cards_container_ = new QWidget(lib_scroll_);
    cards_layout_ = new QGridLayout(cards_container_);
    cards_layout_->setContentsMargins(14, 8, 14, 24);
    cards_layout_->setHorizontalSpacing(kCardSpacing);
    cards_layout_->setVerticalSpacing(kCardSpacing);
    cards_layout_->setAlignment(Qt::AlignTop);
    lib_scroll_->setWidget(cards_container_);
    root->addWidget(lib_scroll_, 1);

    return page;
}

// ── Card factory ────────────────────────────────────────────────────────────

static QWidget* build_notebook_card(QWidget* parent, const NotebookCatalogEntry& e) {
    auto* card = new QFrame(parent);
    card->setObjectName("nbCard");
    card->setMinimumHeight(150);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);

    // Badge row: difficulty + category + est. time.
    auto* badges = new QHBoxLayout;
    badges->setSpacing(6);
    auto* diff = new QLabel(e.difficulty.toUpper(), card);
    diff->setObjectName(difficulty_badge_object(e.difficulty));
    diff->setStyleSheet(QString("font-family:%1; font-size:10px;").arg(fonts::DATA_FAMILY));
    badges->addWidget(diff);
    auto* cat = new QLabel(e.category.toUpper(), card);
    cat->setObjectName("nbCatBadge");
    cat->setStyleSheet(QString("font-family:%1; font-size:10px;").arg(fonts::DATA_FAMILY));
    badges->addWidget(cat);
    badges->addStretch(1);
    auto* meta = new QLabel(QObject::tr("~%1 min").arg(e.est_minutes), card);
    meta->setObjectName("nbCardMeta");
    meta->setStyleSheet(QString("font-family:%1; font-size:10px;").arg(fonts::DATA_FAMILY));
    badges->addWidget(meta);
    v->addLayout(badges);

    // Title.
    auto* title = new QLabel(e.title, card);
    title->setObjectName("nbCardTitle");
    title->setWordWrap(true);
    title->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::DATA));
    v->addWidget(title);

    // Summary.
    auto* summary = new QLabel(e.summary, card);
    summary->setObjectName("nbCardSummary");
    summary->setWordWrap(true);
    summary->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::TINY));
    v->addWidget(summary);

    v->addStretch(1);

    // Footer: requires + OPEN.
    auto* footer = new QHBoxLayout;
    auto* req = new QLabel(e.requirements, card);
    req->setObjectName("nbCardMeta");
    req->setStyleSheet(QString("font-family:%1; font-size:10px;").arg(fonts::DATA_FAMILY));
    footer->addWidget(req);
    footer->addStretch(1);
    auto* open_btn = new QPushButton(QObject::tr("OPEN"), card);
    open_btn->setObjectName("nbCardOpenBtn");
    open_btn->setCursor(Qt::PointingHandCursor);
    open_btn->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::TINY));
    footer->addWidget(open_btn);
    v->addLayout(footer);

    // Expose the OPEN button so the caller can wire it to the catalog index.
    card->setProperty("open_btn_ptr", QVariant::fromValue(static_cast<void*>(open_btn)));
    return card;
}

// ── Populate + filter ───────────────────────────────────────────────────────

void CodeEditorScreen::populate_library() {
    if (!cards_layout_ || !lib_scroll_)
        return;

    // Clear existing cards.
    while (QLayoutItem* item = cards_layout_->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const auto& catalog = NotebookLibraryService::instance().catalog();
    const QString search = lib_search_text_.trimmed();

    const int cols = compute_columns(lib_scroll_->viewport()->width());
    lib_columns_ = cols;
    for (int c = 0; c < cols; ++c)
        cards_layout_->setColumnStretch(c, 1);

    int shown = 0, beg = 0, inter = 0, hard = 0;
    int grid_index = 0;
    for (int i = 0; i < catalog.size(); ++i) {
        const NotebookCatalogEntry& e = catalog[i];
        if (lib_category_filter_ != "All" && e.category.compare(lib_category_filter_, Qt::CaseInsensitive) != 0)
            continue;
        if (lib_difficulty_filter_ != "All" && e.difficulty.compare(lib_difficulty_filter_, Qt::CaseInsensitive) != 0)
            continue;
        if (!search.isEmpty() && !e.title.contains(search, Qt::CaseInsensitive) &&
            !e.category.contains(search, Qt::CaseInsensitive) && !e.summary.contains(search, Qt::CaseInsensitive))
            continue;

        auto* card = build_notebook_card(cards_container_, e);
        auto* open_btn = static_cast<QPushButton*>(card->property("open_btn_ptr").value<void*>());
        if (open_btn) {
            const int catalog_index = i;
            connect(open_btn, &QPushButton::clicked, this, [this, catalog_index]() { on_open_library_entry(catalog_index); });
        }
        cards_layout_->addWidget(card, grid_index / cols, grid_index % cols);
        ++grid_index;

        ++shown;
        if (e.difficulty.compare("Beginner", Qt::CaseInsensitive) == 0)
            ++beg;
        else if (e.difficulty.compare("Hard", Qt::CaseInsensitive) == 0)
            ++hard;
        else
            ++inter;
    }

    if (shown == 0) {
        auto* empty = new QLabel(catalog.isEmpty()
                                     ? tr("Notebook library not found. Rebuild the app to bundle the notebooks.")
                                     : tr("No notebooks match your filters."),
                                 cards_container_);
        empty->setObjectName("nbCardSummary");
        empty->setStyleSheet(QString("font-family:%1; font-size:%2px; padding:24px;")
                                 .arg(fonts::DATA_FAMILY)
                                 .arg(fonts::SMALL));
        cards_layout_->addWidget(empty, 0, 0);
    }

    if (lib_count_lbl_)
        lib_count_lbl_->setText(tr("%1 notebooks  ·  %2 beginner · %3 intermediate · %4 hard")
                                    .arg(shown)
                                    .arg(beg)
                                    .arg(inter)
                                    .arg(hard));
}

void CodeEditorScreen::relayout_library_cards() {
    if (!cards_layout_ || !lib_scroll_)
        return;
    const int cols = compute_columns(lib_scroll_->viewport()->width());
    if (cols != lib_columns_)
        populate_library(); // re-flow only when the column count actually changes
}

void CodeEditorScreen::on_library_search(const QString& text) {
    lib_search_text_ = text;
    populate_library();
}

void CodeEditorScreen::on_open_library_entry(int catalog_index) {
    const auto& catalog = NotebookLibraryService::instance().catalog();
    if (catalog_index < 0 || catalog_index >= catalog.size())
        return;
    const NotebookCatalogEntry& e = catalog[catalog_index];
    const QString path = NotebookLibraryService::instance().working_copy_for(e);
    if (path.isEmpty()) {
        LOG_WARN("CodeEditor", "Could not open library notebook: " + e.id);
        return;
    }
    open_notebook_path(path); // loads + switches to the editor view
}

} // namespace fincept::screens
