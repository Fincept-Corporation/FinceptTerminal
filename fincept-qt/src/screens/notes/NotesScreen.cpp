#include "screens/notes/NotesScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QCursor>
#include <QDesktopServices>
#include <QTextStream>
#include <QTime>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui::colors;

// ── Style constants (Obsidian Design System) ────────────────────────────────

// §5.2 — List items: 26px row, alternating bg, hairline borders
static QString kListItem() {
    return QString("QListWidget { background: %1; border: none; font-family: 'Consolas','Courier New',monospace; "
                   "font-size: 12px; }"
                   "QListWidget::item { padding: 6px 12px; color: %3; border-bottom: 1px solid %2; height: 26px; }"
                   "QListWidget::item:selected { background: %4; color: %3; }"
                   "QListWidget::item:hover { background: %4; }")
        .arg(BG_SURFACE(), BORDER_DIM(), TEXT_PRIMARY(), BG_HOVER());
}

// §5.6 — Input fields
static QString kInput() {
    return QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 3px 6px; font-size: 13px; font-family: 'Consolas','Courier New',monospace; height: 28px; }"
                   "QLineEdit:focus { border-color: %4; }"
                   "QLineEdit::selection { background: %5; color: %6; }")
        .arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM(), BORDER_BRIGHT(), AMBER(), BG_BASE());
}

// §5.6 — Combo: same input style
static QString kCombo() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 3px 6px; font-size: 12px; font-family: 'Consolas','Courier New',monospace; }"
                   "QComboBox:focus { border-color: %4; }"
                   "QComboBox QAbstractItemView { background: %1; color: %2; "
                   "selection-background-color: %3; border: 1px solid %3; }")
        .arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM(), BORDER_BRIGHT());
}

// §5.6 — Text editor: same as input
static QString kTextEdit() {
    return QString("QTextEdit { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 6px; font-size: 14px; font-family: 'Consolas','Courier New',monospace; }"
                   "QTextEdit:focus { border-color: %4; }"
                   "QTextEdit::selection { background: %5; color: %6; }")
        .arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM(), BORDER_BRIGHT(), AMBER(), BG_BASE());
}

// §5.5 — Accent button (amber): rgba bg + amber text + amber_dim border
static QString kActionBtn() {
    return QString("QPushButton { background: rgba(217,119,6,0.1); color: %1; "
                   "border: 1px solid %2; padding: 0 12px; height: 24px; "
                   "font-size: 12px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
                   "QPushButton:hover { background: %1; color: %3; }")
        .arg(AMBER(), AMBER_DIM(), BG_BASE());
}

// §5.5 — Standard button
static QString kSecondaryBtn() {
    return QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 0 10px; height: 24px; "
                   "font-size: 11px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
                   "QPushButton:hover { color: %4; background: %5; }")
        .arg(BG_RAISED(), TEXT_SECONDARY(), BORDER_DIM(), TEXT_PRIMARY(), BG_HOVER());
}

// §5.5 — Danger button (red): rgba bg + red text + dark red border
static QString kDangerBtn() {
    return QString("QPushButton { background: rgba(220,38,38,0.1); color: %1; "
                   "border: 1px solid #7f1d1d; padding: 0 10px; height: 24px; "
                   "font-size: 11px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
                   "QPushButton:hover { background: %1; color: %2; }")
        .arg(NEGATIVE(), TEXT_PRIMARY());
}

// ── Categories ───────────────────────────────────────────────────────────────

struct Category {
    QString id;
    QString label;
};
static const QVector<Category> kCategories = {
    {"ALL", "ALL NOTES"},       {"TRADE_IDEA", "TRADE IDEAS"},
    {"RESEARCH", "RESEARCH"},   {"MARKET_ANALYSIS", "MARKET ANALYSIS"},
    {"EARNINGS", "EARNINGS"},   {"ECONOMIC", "ECONOMIC"},
    {"PORTFOLIO", "PORTFOLIO"}, {"GENERAL", "GENERAL"},
    // Pseudo-category (not stored on notes, not offered in the editor combo): the
    // only way to see — and un-archive — notes the ARCHIVE button has hidden.
    {"ARCHIVED", "ARCHIVED"},
};

static const QStringList kPriorities = {"HIGH", "MEDIUM", "LOW"};
static const QStringList kSentiments = {"BULLISH", "BEARISH", "NEUTRAL"};

// Translatable display label for a category id. The id stays a stable data key;
// only the visible text is localized. Used by retranslateUi for the sidebar
// list and the editor category combo.
static QString category_display_label(const QString& id) {
    if (id == "ALL")
        return QCoreApplication::translate("NotesScreen", "ALL NOTES");
    if (id == "TRADE_IDEA")
        return QCoreApplication::translate("NotesScreen", "TRADE IDEAS");
    if (id == "RESEARCH")
        return QCoreApplication::translate("NotesScreen", "RESEARCH");
    if (id == "MARKET_ANALYSIS")
        return QCoreApplication::translate("NotesScreen", "MARKET ANALYSIS");
    if (id == "EARNINGS")
        return QCoreApplication::translate("NotesScreen", "EARNINGS");
    if (id == "ECONOMIC")
        return QCoreApplication::translate("NotesScreen", "ECONOMIC");
    if (id == "PORTFOLIO")
        return QCoreApplication::translate("NotesScreen", "PORTFOLIO");
    if (id == "GENERAL")
        return QCoreApplication::translate("NotesScreen", "GENERAL");
    if (id == "ARCHIVED")
        return QCoreApplication::translate("NotesScreen", "ARCHIVED");
    return id;
}

static QString priority_color(const QString& p) {
    if (p == "HIGH")
        return NEGATIVE();
    if (p == "MEDIUM")
        return WARNING();
    return POSITIVE();
}

// ── Constructor ──────────────────────────────────────────────────────────────

NotesScreen::NotesScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(BG_BASE()));

    // Single-shot timers: interval + connection here, started only by user input.
    autosave_timer_ = new QTimer(this);
    autosave_timer_->setSingleShot(true);
    autosave_timer_->setInterval(2500);
    connect(autosave_timer_, &QTimer::timeout, this, [this]() { autosave_now(); });
    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(150);
    connect(search_timer_, &QTimer::timeout, this, &NotesScreen::update_notes_list);

    build_ui();
    load_notes();

    // Reload from the local cache when a cloud pull updates notes.
    connect(&fincept::services::cloud::CloudSyncEngine::instance(),
            &fincept::services::cloud::CloudSyncEngine::cloud_data_changed, this, [this](const QString& entity) {
                if (entity == QLatin1String("note")) {
                    load_notes();
                    update_notes_list();
                }
            });
}

void NotesScreen::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet(QString("QSplitter::handle { background: %1; width: 1px; }").arg(BORDER_DIM()));

    splitter->addWidget(build_category_sidebar());
    splitter->addWidget(build_notes_list_panel());
    splitter->addWidget(build_editor_panel());
    splitter->setSizes({200, 320, 600});
    splitter->setStretchFactor(2, 1);

    root->addWidget(splitter);

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    // A notes editor with no Ctrl+S is a data-loss trap. Scoped to this screen
    // so they don't leak into other tabs.
    auto add_sc = [this](const QKeySequence& seq, auto fn) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    add_sc(QKeySequence(QKeySequence::Save), [this]() {
        if (right_stack_ && right_stack_->currentIndex() == 2)
            on_save_note();
    });
    add_sc(QKeySequence(QKeySequence::New), [this]() { on_new_note(); });
    add_sc(QKeySequence(QKeySequence::Find), [this]() {
        if (search_input_) {
            search_input_->setFocus();
            search_input_->selectAll();
        }
    });

    // ── Accessibility ─────────────────────────────────────────────────────────
    category_list_->setAccessibleName(tr("Note categories"));
    notes_list_->setAccessibleName(tr("Notes"));
    search_input_->setAccessibleName(tr("Search notes"));
    edit_title_->setAccessibleName(tr("Note title"));
    edit_content_->setAccessibleName(tr("Note content"));
    edit_category_->setAccessibleName(tr("Note category"));
    edit_priority_->setAccessibleName(tr("Note priority"));
    edit_sentiment_->setAccessibleName(tr("Note sentiment"));
    edit_tags_->setAccessibleName(tr("Note tags"));
    edit_tickers_->setAccessibleName(tr("Note tickers"));
    setTabOrder(edit_title_, edit_category_);
    setTabOrder(edit_category_, edit_priority_);
    setTabOrder(edit_priority_, edit_sentiment_);
    setTabOrder(edit_sentiment_, edit_tags_);
    setTabOrder(edit_tags_, edit_tickers_);
    setTabOrder(edit_tickers_, edit_content_);

    // Populate empty-display combos/lists and apply all translatable text.
    retranslateUi();
}

// ── Category Sidebar ─────────────────────────────────────────────────────────

QWidget* NotesScreen::build_category_sidebar() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(200);
    panel->setStyleSheet(QString("background: %1; border-right: 1px solid %2;").arg(BG_SURFACE(), BORDER_DIM()));

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Header
    category_header_ = new QLabel(tr("  CATEGORIES"));
    category_header_->setFixedHeight(34);
    category_header_->setStyleSheet(QString("background: %1; color: %2; font-size: 12px; font-weight: 700; "
                                            "letter-spacing: 0.5px; border-bottom: 1px solid %3; padding-left: 12px; "
                                            "font-family: 'Consolas','Courier New',monospace;")
                                        .arg(BG_RAISED(), AMBER(), BORDER_DIM()));
    lay->addWidget(category_header_);

    // Category list — labels applied in retranslateUi (ids stay as data).
    category_list_ = new QListWidget;
    category_list_->setStyleSheet(kListItem());
    for (int i = 0; i < kCategories.size(); ++i)
        category_list_->addItem(QString());
    category_list_->setCurrentRow(0);
    connect(category_list_, &QListWidget::currentRowChanged, this, &NotesScreen::on_category_selected);
    lay->addWidget(category_list_);

    // Stats
    stats_label_ = new QLabel;
    stats_label_->setFixedHeight(26);
    stats_label_->setAlignment(Qt::AlignCenter);
    stats_label_->setStyleSheet(QString("background: %1; color: %2; font-size: 11px; border-top: 1px solid %3; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(BG_SURFACE(), TEXT_SECONDARY(), BORDER_DIM()));
    lay->addWidget(stats_label_);

    return panel;
}

// ── Notes List Panel ─────────────────────────────────────────────────────────

QWidget* NotesScreen::build_notes_list_panel() {
    auto* panel = new QWidget(this);
    panel->setMinimumWidth(280);
    panel->setStyleSheet(QString("background: %1;").arg(BG_SURFACE()));

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Header with search + new button
    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG_RAISED(), BORDER_DIM()));
    auto* tl = new QHBoxLayout(toolbar);
    tl->setContentsMargins(8, 0, 8, 0);
    tl->setSpacing(6);

    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText(tr("Search notes..."));
    search_input_->setStyleSheet(kInput());
    search_input_->setFixedHeight(28);
    connect(search_input_, &QLineEdit::textChanged, this, &NotesScreen::on_search_changed);
    tl->addWidget(search_input_);

    new_btn_ = new QPushButton(tr("+ NEW"));
    new_btn_->setFixedSize(70, 24);
    new_btn_->setStyleSheet(kActionBtn());
    connect(new_btn_, &QPushButton::clicked, this, &NotesScreen::on_new_note);
    tl->addWidget(new_btn_);

    lay->addWidget(toolbar);

    // Notes list
    notes_list_ = new QListWidget;
    notes_list_->setStyleSheet(kListItem());
    connect(notes_list_, &QListWidget::currentRowChanged, this, &NotesScreen::on_note_selected);
    lay->addWidget(notes_list_);

    // Footer count
    count_label_ = new QLabel(tr("%1 notes").arg(0));
    count_label_->setFixedHeight(26);
    count_label_->setAlignment(Qt::AlignCenter);
    count_label_->setStyleSheet(QString("background: %1; color: %2; font-size: 11px; border-top: 1px solid %3; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(BG_SURFACE(), TEXT_TERTIARY(), BORDER_DIM()));
    lay->addWidget(count_label_);

    return panel;
}

// ── Editor / Viewer Panel ────────────────────────────────────────────────────

QWidget* NotesScreen::build_editor_panel() {
    right_stack_ = new QStackedWidget;

    // ── Page 0: Empty state ──────────────────────────────────────────────────
    auto* empty = new QWidget(this);
    empty->setStyleSheet(QString("background: %1;").arg(BG_BASE()));
    auto* el = new QVBoxLayout(empty);
    el->setAlignment(Qt::AlignCenter);
    empty_label_ = new QLabel(tr("Select a note or create a new one"));
    empty_label_->setStyleSheet(
        QString("color: %1; font-size: 14px; font-family: 'Consolas','Courier New',monospace;").arg(TEXT_TERTIARY()));
    el->addWidget(empty_label_, 0, Qt::AlignCenter);
    right_stack_->addWidget(empty); // index 0

    // ── Page 1: View mode ────────────────────────────────────────────────────
    auto* view_page = new QWidget(this);
    view_page->setStyleSheet(QString("background: %1;").arg(BG_BASE()));
    auto* vl = new QVBoxLayout(view_page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    // View toolbar
    auto* view_toolbar = new QHBoxLayout;
    view_edit_btn_ = new QPushButton(tr("EDIT"));
    view_edit_btn_->setStyleSheet(kSecondaryBtn());
    connect(view_edit_btn_, &QPushButton::clicked, this, &NotesScreen::enter_edit_mode);
    view_toolbar->addWidget(view_edit_btn_);

    view_fav_btn_ = new QPushButton(tr("FAV"));
    view_fav_btn_->setStyleSheet(kSecondaryBtn());
    connect(view_fav_btn_, &QPushButton::clicked, this, &NotesScreen::on_toggle_favorite);
    view_toolbar->addWidget(view_fav_btn_);

    view_archive_btn_ = new QPushButton(tr("ARCHIVE"));
    view_archive_btn_->setStyleSheet(kSecondaryBtn());
    connect(view_archive_btn_, &QPushButton::clicked, this, &NotesScreen::on_toggle_archive);
    view_toolbar->addWidget(view_archive_btn_);

    view_delete_btn_ = new QPushButton(tr("DELETE"));
    view_delete_btn_->setStyleSheet(kDangerBtn());
    connect(view_delete_btn_, &QPushButton::clicked, this, &NotesScreen::on_delete_note);
    view_toolbar->addWidget(view_delete_btn_);

    view_toolbar->addStretch();
    vl->addLayout(view_toolbar);

    view_title_ = new QLabel;
    view_title_->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: bold; font-family: 'Consolas','Courier New',monospace;")
            .arg(TEXT_PRIMARY()));
    view_title_->setWordWrap(true);
    vl->addWidget(view_title_);

    view_meta_ = new QLabel;
    view_meta_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-family: 'Consolas','Courier New',monospace;").arg(TEXT_SECONDARY()));
    view_meta_->setWordWrap(true);
    // Tickers are links ("Open in…" menu).
    view_meta_->setTextFormat(Qt::RichText);
    view_meta_->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(view_meta_, &QLabel::linkActivated, this, [this](const QString& link) { on_symbol_link(QUrl(link)); });
    vl->addWidget(view_meta_);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(BORDER_DIM()));
    vl->addWidget(sep);

    // Rendered note body: Markdown (headings, lists, tables, emphasis, links) with
    // $TICKER cashtags turned into symbol links. Links never navigate the browser.
    view_content_ = new QTextBrowser;
    view_content_->setOpenLinks(false);
    view_content_->setOpenExternalLinks(false);
    view_content_->setStyleSheet(QString("QTextBrowser { color: %1; font-size: 14px; background: transparent;"
                                         " border: none; padding: 4px; }")
                                     .arg(TEXT_PRIMARY()));
    view_content_->document()->setDefaultStyleSheet(
        QString("a { color: %1; } code, pre { font-family: Consolas, 'Courier New', monospace; }").arg(AMBER()));
    connect(view_content_, &QTextBrowser::anchorClicked, this, &NotesScreen::on_symbol_link);
    vl->addWidget(view_content_, 1);

    right_stack_->addWidget(view_page); // index 1

    // ── Page 2: Edit mode ────────────────────────────────────────────────────
    auto* edit_page = new QWidget(this);
    edit_page->setStyleSheet(QString("background: %1;").arg(BG_BASE()));
    auto* edl = new QVBoxLayout(edit_page);
    edl->setContentsMargins(14, 14, 14, 14);
    edl->setSpacing(10);

    // Save/Cancel bar
    auto* edit_toolbar = new QHBoxLayout;
    edit_save_btn_ = new QPushButton(tr("SAVE"));
    edit_save_btn_->setStyleSheet(kActionBtn());
    connect(edit_save_btn_, &QPushButton::clicked, this, &NotesScreen::on_save_note);
    edit_toolbar->addWidget(edit_save_btn_);

    edit_cancel_btn_ = new QPushButton(tr("CANCEL"));
    edit_cancel_btn_->setStyleSheet(kSecondaryBtn());
    connect(edit_cancel_btn_, &QPushButton::clicked, this, [this]() {
        // CANCEL means "back to the note as I opened it" — including anything autosave
        // already wrote during this edit.
        if ((has_unsaved_edits() || autosaved_in_session_) &&
            QMessageBox::question(this, tr("Discard changes"),
                                  tr("Discard every change made since you started editing this note?"),
                                  QMessageBox::Discard | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Discard)
            return;
        autosave_timer_->stop();
        revert_edit_session();
        editor_baseline_ = editor_fingerprint();
        if (selected_note_id_ > 0) {
            enter_view_mode();
        } else {
            right_stack_->setCurrentIndex(0);
            is_editing_ = false;
        }
    });
    edit_toolbar->addWidget(edit_cancel_btn_);

    edit_export_btn_ = new QPushButton(tr("EXPORT"));
    edit_export_btn_->setStyleSheet(kSecondaryBtn());
    edit_export_btn_->setToolTip(tr("Export this note as a Markdown file to the File Manager"));
    connect(edit_export_btn_, &QPushButton::clicked, this, &NotesScreen::on_export_note);
    edit_toolbar->addWidget(edit_export_btn_);

    edit_toolbar->addStretch();
    autosave_label_ = new QLabel;
    autosave_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-family: 'Consolas','Courier New',monospace;").arg(TEXT_TERTIARY()));
    edit_toolbar->addWidget(autosave_label_);
    edl->addLayout(edit_toolbar);

    // Title
    edit_title_ = new QLineEdit;
    edit_title_->setPlaceholderText(tr("Note title..."));
    edit_title_->setStyleSheet(kInput() + " QLineEdit { font-size: 16px; font-weight: 700; }");
    edit_title_->setFixedHeight(28);
    edl->addWidget(edit_title_);

    // Metadata row
    auto* meta_row = new QHBoxLayout;
    meta_row->setSpacing(8);

    auto make_label = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(QString("color: %1; font-size: 11px; font-family: 'Consolas','Courier New',monospace;")
                             .arg(TEXT_SECONDARY()));
        return l;
    };

    lbl_cat_ = make_label(tr("CAT:"));
    meta_row->addWidget(lbl_cat_);
    // Category combo — display text applied in retranslateUi (ids stay as data).
    edit_category_ = new QComboBox;
    edit_category_->setStyleSheet(kCombo());
    for (int i = 1; i < kCategories.size(); ++i) { // skip "ALL"
        if (kCategories[i].id == QLatin1String("ARCHIVED"))
            continue; // a view filter, not something a note can be filed under
        edit_category_->addItem(QString(), kCategories[i].id);
    }
    meta_row->addWidget(edit_category_);

    lbl_pri_ = make_label(tr("PRI:"));
    meta_row->addWidget(lbl_pri_);
    // Priority is stored verbatim in the DB (note.priority) and matched via
    // currentText() — these are code values, intentionally left untranslated.
    edit_priority_ = new QComboBox;
    edit_priority_->setStyleSheet(kCombo());
    for (const auto& p : kPriorities)
        edit_priority_->addItem(p);
    edit_priority_->setCurrentText("MEDIUM");
    meta_row->addWidget(edit_priority_);

    lbl_sent_ = make_label(tr("SENT:"));
    meta_row->addWidget(lbl_sent_);
    // Sentiment is also a stored code value (note.sentiment) — left untranslated.
    edit_sentiment_ = new QComboBox;
    edit_sentiment_->setStyleSheet(kCombo());
    for (const auto& s : kSentiments)
        edit_sentiment_->addItem(s);
    edit_sentiment_->setCurrentText("NEUTRAL");
    meta_row->addWidget(edit_sentiment_);

    meta_row->addStretch();
    edl->addLayout(meta_row);

    // Tags + Tickers row
    auto* tag_row = new QHBoxLayout;
    tag_row->setSpacing(8);
    lbl_tags_ = make_label(tr("TAGS:"));
    tag_row->addWidget(lbl_tags_);
    edit_tags_ = new QLineEdit;
    edit_tags_->setPlaceholderText(tr("tag1, tag2, ..."));
    edit_tags_->setStyleSheet(kInput());
    tag_row->addWidget(edit_tags_);

    lbl_tickers_ = make_label(tr("TICKERS:"));
    tag_row->addWidget(lbl_tickers_);
    edit_tickers_ = new QLineEdit;
    edit_tickers_->setPlaceholderText(tr("AAPL, MSFT, ..."));
    edit_tickers_->setStyleSheet(kInput());
    tag_row->addWidget(edit_tickers_);
    edl->addLayout(tag_row);

    // Content editor
    edit_content_ = new QTextEdit;
    edit_content_->setStyleSheet(kTextEdit());
    edit_content_->setPlaceholderText(tr("Write your note here... (Markdown: # heading, **bold**, - list, $AAPL)"));
    edl->addWidget(edit_content_, 1);

    connect(edit_title_, &QLineEdit::textEdited, this, &NotesScreen::on_editor_edited);
    connect(edit_tags_, &QLineEdit::textEdited, this, &NotesScreen::on_editor_edited);
    connect(edit_tickers_, &QLineEdit::textEdited, this, &NotesScreen::on_editor_edited);
    connect(edit_content_, &QTextEdit::textChanged, this, &NotesScreen::on_editor_edited);
    connect(edit_category_, &QComboBox::activated, this, &NotesScreen::on_editor_edited);
    connect(edit_priority_, &QComboBox::activated, this, &NotesScreen::on_editor_edited);
    connect(edit_sentiment_, &QComboBox::activated, this, &NotesScreen::on_editor_edited);

    right_stack_->addWidget(edit_page); // index 2

    return right_stack_;
}

// ── Data Loading ─────────────────────────────────────────────────────────────

void NotesScreen::load_notes() {
    // include_archived: archived notes are filtered per view in update_notes_list()
    // (hidden from every category, listed under ARCHIVED) instead of vanishing.
    auto r = fincept::NotesRepository::instance().list_all(true);
    if (r.is_ok()) {
        notes_ = r.value();
    } else {
        notes_.clear();
        LOG_ERROR("Notes", "Failed to load notes: " + QString::fromStdString(r.error()));
    }
    update_notes_list();
}

void NotesScreen::update_notes_list() {
    filtered_notes_.clear();
    QString search = search_input_ ? search_input_->text().toLower() : "";

    const bool archived_view = (current_category_ == QLatin1String("ARCHIVED"));
    for (const auto& n : notes_) {
        // Archived notes appear only under ARCHIVED; every other view hides them.
        if (n.is_archived != archived_view)
            continue;
        // Category filter
        if (!archived_view && current_category_ != "ALL" && n.category != current_category_)
            continue;
        // Search filter: every word must appear somewhere (title, body, tags, tickers);
        // a leading $ matches a ticker ("$aapl").
        if (!search.isEmpty()) {
            const QString hay = n.title.toLower() + QLatin1Char('\n') + n.content.toLower() + QLatin1Char('\n') +
                                n.tags.toLower() + QLatin1Char('\n') + n.tickers.toLower();
            bool all = true;
            for (QString term : search.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                if (term.startsWith(QLatin1Char('$')) && term.size() > 1)
                    term = term.mid(1);
                if (!hay.contains(term)) {
                    all = false;
                    break;
                }
            }
            if (!all)
                continue;
        }
        filtered_notes_.append(n);
    }

    notes_list_->blockSignals(true);
    notes_list_->clear();
    for (const auto& n : filtered_notes_) {
        QString display = n.title;
        if (!n.tickers.isEmpty())
            display += "  [" + n.tickers + "]";

        auto* item = new QListWidgetItem(display);
        // Priority color indicator via text color
        QString pri_col = priority_color(n.priority);
        item->setForeground(QColor(pri_col));
        if (n.is_favorite)
            item->setText("* " + display);
        notes_list_->addItem(item);
    }
    // Rebuilding the list dropped the highlight (search keystrokes, favourite
    // toggles, MCP/cloud reloads); put it back on the open note, if it is still
    // listed. Signals are blocked, so this does not re-enter on_note_selected().
    if (selected_note_id_ > 0) {
        for (int i = 0; i < filtered_notes_.size(); ++i) {
            if (filtered_notes_[i].id == selected_note_id_) {
                notes_list_->setCurrentRow(i);
                break;
            }
        }
    }
    notes_list_->blockSignals(false);

    count_label_->setText(tr("%1 notes").arg(filtered_notes_.size()));

    // Update stats (archived notes are out of the working set)
    int active_count = 0;
    int fav_count = 0;
    for (const auto& n : notes_) {
        if (n.is_archived)
            continue;
        ++active_count;
        if (n.is_favorite)
            ++fav_count;
    }
    stats_label_->setText(tr("Total: %1  |  Fav: %2").arg(active_count).arg(fav_count));
}

// ── Slots ────────────────────────────────────────────────────────────────────

// ── Unsaved-edit protection ──────────────────────────────────────────────────
// Every navigation path out of the editor (picking another note, switching
// category, starting a new note) used to silently throw away whatever the user
// had typed. Compare a fingerprint of the editor fields against the values that
// were loaded into it and confirm before discarding.

QString NotesScreen::editor_fingerprint() const {
    if (!edit_title_)
        return QString();
    return edit_title_->text() + QLatin1Char('\x1f') + edit_content_->toPlainText() + QLatin1Char('\x1f') +
           edit_category_->currentData().toString() + QLatin1Char('\x1f') + edit_priority_->currentText() +
           QLatin1Char('\x1f') + edit_sentiment_->currentText() + QLatin1Char('\x1f') + edit_tags_->text() +
           QLatin1Char('\x1f') + edit_tickers_->text();
}

bool NotesScreen::has_unsaved_edits() const {
    if (!is_editing_ || !right_stack_ || right_stack_->currentIndex() != 2)
        return false;
    return editor_fingerprint() != editor_baseline_;
}

bool NotesScreen::confirm_leave_editor() {
    autosave_timer_->stop();
    if (!has_unsaved_edits())
        return true;
    // Notes autosave: leaving the editor just writes the pending edits. Only a note that
    // cannot be saved yet (no title) still asks.
    if (!edit_title_->text().trimmed().isEmpty() && autosave_now())
        return true;
    const auto reply = QMessageBox::warning(
        this, tr("Unsaved note"), tr("This note has unsaved changes.\n\nSave them before leaving?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (reply == QMessageBox::Cancel)
        return false;
    if (reply == QMessageBox::Save) {
        on_save_note();
        // on_save_note() refuses an empty title; if it did, stay in the editor.
        return !edit_title_->text().trimmed().isEmpty();
    }
    editor_baseline_ = editor_fingerprint(); // discarded — stop re-asking
    return true;
}

void NotesScreen::on_category_selected(int row) {
    if (row < 0 || row >= kCategories.size())
        return;
    if (!confirm_leave_editor()) {
        // Put the sidebar selection back where it was.
        if (category_list_) {
            QSignalBlocker block(category_list_);
            for (int i = 0; i < kCategories.size(); ++i)
                if (kCategories[i].id == current_category_) {
                    category_list_->setCurrentRow(i);
                    break;
                }
        }
        return;
    }
    current_category_ = kCategories[row].id;
    // Clear the open note BEFORE rebuilding the list: update_notes_list() re-selects
    // the open note's row, which left a highlighted entry beside an empty pane
    // (and a click on it did nothing, as the row did not change).
    selected_note_id_ = -1;
    is_editing_ = false;
    update_notes_list();
    right_stack_->setCurrentIndex(0);
    ScreenStateManager::instance().notify_changed(this);
}

void NotesScreen::on_note_selected(int row) {
    if (row < 0 || row >= filtered_notes_.size())
        return;
    if (!confirm_leave_editor())
        return;
    const auto& note = filtered_notes_[row];
    selected_note_id_ = note.id;
    show_note(note);
    enter_view_mode();
    ScreenStateManager::instance().notify_changed(this);
}

void NotesScreen::on_new_note() {
    if (!confirm_leave_editor())
        return;
    selected_note_id_ = -1;
    clear_editor();

    // Pre-select current category if not ALL
    if (current_category_ != "ALL") {
        int idx = edit_category_->findData(current_category_);
        if (idx >= 0)
            edit_category_->setCurrentIndex(idx);
    }

    is_editing_ = true;
    has_session_original_ = false;
    autosaved_in_session_ = false;
    autosave_label_->clear();
    right_stack_->setCurrentIndex(2);
    editor_baseline_ = editor_fingerprint();
    edit_title_->setFocus();
}

void NotesScreen::on_save_note() {
    QString title = edit_title_->text().trimmed();
    if (title.isEmpty()) {
        // Previously this just moved focus with no explanation, so SAVE looked
        // like a dead button.
        QMessageBox::information(this, tr("Title required"), tr("Give the note a title before saving."));
        edit_title_->setFocus();
        return;
    }

    autosave_timer_->stop();
    // Keeps favourite / archived / colour / reminder: saving an edited favourite used to
    // un-favourite it (and un-archive an archived one) because those were rebuilt blank.
    fincept::FinancialNote note = note_from_editor();

    auto& repo = fincept::NotesRepository::instance();

    bool ok = true;
    if (selected_note_id_ > 0) {
        // Update existing
        note.id = selected_note_id_;
        auto r = repo.update(note);
        if (r.is_err()) {
            LOG_ERROR("Notes", "Failed to update note");
            ok = false;
        }
    } else {
        // Create new
        auto r = repo.create(note);
        if (r.is_ok()) {
            selected_note_id_ = static_cast<int>(r.value());
        } else {
            LOG_ERROR("Notes", "Failed to create note");
            ok = false;
        }
    }

    if (!ok) {
        // Never silently lose the user's writing — keep them in the editor.
        QMessageBox::warning(this, tr("Save failed"),
                             tr("The note could not be written to the local database. "
                                "Your text is still in the editor."));
        return;
    }

    // The editor now matches what is on disk.
    editor_baseline_ = editor_fingerprint();
    autosaved_in_session_ = false;

    load_notes();

    // Select the saved note
    {
        QSignalBlocker block(notes_list_); // don't re-enter on_note_selected
        for (int i = 0; i < filtered_notes_.size(); ++i) {
            if (filtered_notes_[i].id == selected_note_id_) {
                notes_list_->setCurrentRow(i);
                break;
            }
        }
    }
    for (const auto& n : notes_)
        if (n.id == selected_note_id_) {
            show_note(n);
            break;
        }
    enter_view_mode();
}

void NotesScreen::on_delete_note() {
    if (selected_note_id_ <= 0)
        return;

    auto reply = QMessageBox::question(this, tr("Delete Note"), tr("Are you sure you want to delete this note?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    auto removed = fincept::NotesRepository::instance().remove(selected_note_id_);
    if (removed.is_err()) {
        // The result used to be ignored: the note stayed in the DB but the pane
        // closed as if it had been deleted, and it came back on the next reload.
        LOG_ERROR("Notes", "Failed to delete note");
        QMessageBox::warning(this, tr("Delete failed"), tr("The note could not be deleted."));
        return;
    }
    selected_note_id_ = -1;
    right_stack_->setCurrentIndex(0);
    load_notes();
}

void NotesScreen::on_toggle_favorite() {
    if (selected_note_id_ <= 0)
        return;
    fincept::NotesRepository::instance().toggle_favorite(selected_note_id_);
    load_notes();
    // Re-show updated note
    auto r = fincept::NotesRepository::instance().get(selected_note_id_);
    if (r.is_ok())
        show_note(r.value());
}

void NotesScreen::on_toggle_archive() {
    if (selected_note_id_ <= 0)
        return;
    fincept::NotesRepository::instance().toggle_archive(selected_note_id_);
    selected_note_id_ = -1;
    right_stack_->setCurrentIndex(0);
    load_notes();
}

void NotesScreen::on_search_changed(const QString& /*text*/) {
    search_timer_->start(); // debounce: one list rebuild per pause in typing
}

// ── Autosave ─────────────────────────────────────────────────────────────────

void NotesScreen::on_editor_edited() {
    if (!is_editing_)
        return;
    autosave_timer_->start();
}

fincept::FinancialNote NotesScreen::note_from_editor() const {
    fincept::FinancialNote note;
    if (selected_note_id_ > 0) {
        for (const auto& n : notes_) {
            if (n.id == selected_note_id_) {
                note = n;
                break;
            }
        }
        note.id = selected_note_id_;
    }
    note.title = edit_title_->text().trimmed();
    note.content = edit_content_->toPlainText();
    note.category = edit_category_->currentData().toString();
    note.priority = edit_priority_->currentText();
    note.sentiment = edit_sentiment_->currentText();
    note.tags = edit_tags_->text().trimmed();
    note.tickers = edit_tickers_->text().trimmed().toUpper();
    note.word_count = note.content.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    return note;
}

bool NotesScreen::autosave_now() {
    if (!is_editing_ || !has_unsaved_edits())
        return true;
    const fincept::FinancialNote note = note_from_editor();
    if (note.title.isEmpty()) {
        autosave_label_->setText(tr("AUTOSAVE: needs a title"));
        return false;
    }
    auto& repo = fincept::NotesRepository::instance();
    bool ok = false;
    if (selected_note_id_ > 0) {
        ok = repo.update(note).is_ok();
    } else {
        auto r = repo.create(note);
        ok = r.is_ok();
        if (ok)
            selected_note_id_ = static_cast<int>(r.value());
    }
    if (!ok) {
        LOG_ERROR("Notes", "Autosave failed");
        autosave_label_->setText(tr("AUTOSAVE FAILED — use SAVE"));
        return false;
    }
    autosaved_in_session_ = true;
    editor_baseline_ = editor_fingerprint();
    autosave_label_->setText(tr("AUTOSAVED %1").arg(QTime::currentTime().toString("HH:mm:ss")));
    load_notes(); // refresh list/stats; the editor keeps its contents
    return true;
}

void NotesScreen::revert_edit_session() {
    if (!autosaved_in_session_)
        return;
    auto& repo = fincept::NotesRepository::instance();
    if (has_session_original_) {
        if (repo.update(session_original_).is_err())
            LOG_ERROR("Notes", "Could not restore the note after CANCEL");
    } else if (selected_note_id_ > 0) {
        // A brand-new note that only exists because autosave created it.
        if (repo.remove(selected_note_id_).is_ok())
            selected_note_id_ = -1;
    }
    autosaved_in_session_ = false;
    load_notes();
}

QString NotesScreen::markdown_with_symbol_links(const QString& content) {
    // $AAPL / $BRK.B / $RELIANCE.NS → link; skipped inside `code` and ``` fences.
    static const QRegularExpression cashtag(QStringLiteral("(?<![\\w$])\\$([A-Za-z][A-Za-z0-9.\\-^=]{0,14})\\b"));
    QStringList out;
    bool in_fence = false;
    for (const QString& line : content.split(QLatin1Char('\n'))) {
        if (line.trimmed().startsWith(QLatin1String("```"))) {
            in_fence = !in_fence;
            out << line;
            continue;
        }
        if (in_fence || line.contains(QLatin1Char('`'))) {
            out << line;
            continue;
        }
        QString l = line;
        l.replace(cashtag, QStringLiteral("[$\\1](fincept-symbol:\\1)"));
        out << l;
    }
    return out.join(QLatin1Char('\n'));
}

void NotesScreen::on_symbol_link(const QUrl& url) {
    if (url.scheme() != QLatin1String("fincept-symbol")) {
        // Ordinary links in a note open in the system browser only on an explicit click.
        if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))
            QDesktopServices::openUrl(url);
        return;
    }
    const QString sym = url.path().trimmed().toUpper();
    if (sym.isEmpty())
        return;
    QMenu menu(this);
    menu.addSection(tr("Open %1 in…").arg(sym));
    struct Target {
        QString screen_id;
        QString label;
    };
    const QVector<Target> targets = {
        {QStringLiteral("equity_research"), tr("Equity Research")},
        {QStringLiteral("equity_trading"), tr("Equity Trading")},
        {QStringLiteral("news"), tr("News")},
        {QStringLiteral("watchlist"), tr("Watchlist")},
        {QStringLiteral("backtesting"), tr("Backtesting")},
    };
    for (const auto& t : targets) {
        const QString screen_id = t.screen_id;
        menu.addAction(t.label, this, [sym, screen_id]() {
            EventBus::instance().publish("nav.open_symbol", {{"screen_id", screen_id}, {"symbol", sym}});
        });
    }
    menu.exec(QCursor::pos());
}

void NotesScreen::on_export_note() {
    QString title = edit_title_->text().trimmed();
    QString content = edit_content_->toPlainText();
    if (title.isEmpty()) {
        edit_title_->setFocus();
        return;
    }

    // Write to a temp .md file inside storage dir, then import via FileManagerService
    auto& svc = services::FileManagerService::instance();
    QString safe_title = title;
    safe_title.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
    QString stored_name = safe_title + ".md";
    QString dest = svc.storage_dir() + "/" + stored_name;

    // If file exists, make it unique
    if (QFile::exists(dest)) {
        QString ts = QString::number(QDateTime::currentMSecsSinceEpoch());
        stored_name = safe_title + "_" + ts + ".md";
        dest = svc.storage_dir() + "/" + stored_name;
    }

    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR("Notes", "Failed to export note: " + dest);
        // The button previously appeared to do nothing at all on failure.
        QMessageBox::warning(this, tr("Export failed"), tr("Could not write the note to:\n%1").arg(dest));
        return;
    }

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8); // non-ASCII note bodies must survive
    out << "# " << title << "\n\n" << content << "\n";
    f.close();

    QFileInfo info(dest);
    svc.register_file(stored_name, title + ".md", info.size(), "text/markdown", "notes");

    LOG_INFO("Notes", "Exported note to File Manager: " + stored_name);
    QMessageBox::information(this, tr("Note exported"),
                             tr("Saved as \"%1\" — it is now available in the File Manager.").arg(stored_name));
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void NotesScreen::show_note(const fincept::FinancialNote& note) {
    view_title_->setText(note.is_favorite ? ("* " + note.title) : note.title);

    QStringList meta_parts;
    meta_parts << note.category;
    meta_parts << tr("PRI: %1").arg(note.priority);
    meta_parts << tr("SENT: %1").arg(note.sentiment);
    if (!note.tags.isEmpty())
        meta_parts << tr("TAGS: %1").arg(note.tags);
    for (auto& part : meta_parts)
        part = part.toHtmlEscaped();
    if (!note.tickers.isEmpty()) {
        QStringList links;
        for (const QString& t : note.tickers.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts)) {
            const QString sym = t.trimmed().toUpper();
            links << QStringLiteral("<a href=\"fincept-symbol:%1\" style=\"color:%2;\">%1</a>")
                         .arg(sym.toHtmlEscaped(), AMBER());
        }
        meta_parts << tr("TICKERS: %1").arg(links.join(QStringLiteral(" ")));
    }
    meta_parts << note.updated_at.toHtmlEscaped();

    view_meta_->setText(meta_parts.join("  |  "));
    view_content_->setMarkdown(markdown_with_symbol_links(note.content));
    // The same button restores a note that is already archived.
    if (view_archive_btn_)
        view_archive_btn_->setText(note.is_archived ? tr("UNARCHIVE") : tr("ARCHIVE"));
}

void NotesScreen::clear_editor() {
    edit_title_->clear();
    edit_content_->clear();
    edit_category_->setCurrentIndex(edit_category_->count() - 1); // GENERAL
    edit_priority_->setCurrentText("MEDIUM");
    edit_sentiment_->setCurrentText("NEUTRAL");
    edit_tags_->clear();
    edit_tickers_->clear();
}

void NotesScreen::enter_edit_mode() {
    is_editing_ = true;
    // Find the note and populate editor. If it isn't in notes_ (stale id after
    // a delete elsewhere) start from a clean editor rather than leaving the
    // previous note's text in the fields.
    bool found = false;
    has_session_original_ = false;
    autosaved_in_session_ = false;
    if (autosave_label_)
        autosave_label_->clear();
    for (const auto& n : notes_) {
        if (n.id == selected_note_id_) {
            session_original_ = n;
            has_session_original_ = true;
            edit_title_->setText(n.title);
            edit_content_->setPlainText(n.content);
            int cat_idx = edit_category_->findData(n.category);
            if (cat_idx >= 0)
                edit_category_->setCurrentIndex(cat_idx);
            edit_priority_->setCurrentText(n.priority);
            edit_sentiment_->setCurrentText(n.sentiment);
            edit_tags_->setText(n.tags);
            edit_tickers_->setText(n.tickers);
            found = true;
            break;
        }
    }
    if (!found)
        clear_editor();
    right_stack_->setCurrentIndex(2);
    editor_baseline_ = editor_fingerprint();
}

void NotesScreen::enter_view_mode() {
    is_editing_ = false;
    right_stack_->setCurrentIndex(1);
}

// ── Live language switch ─────────────────────────────────────────────────────

void NotesScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NotesScreen::retranslateUi() {
    if (category_header_)
        category_header_->setText(tr("  CATEGORIES"));

    // Category sidebar list — re-apply translated labels (ids stay as data).
    if (category_list_) {
        for (int i = 0; i < kCategories.size() && i < category_list_->count(); ++i)
            category_list_->item(i)->setText(category_display_label(kCategories[i].id));
    }

    // Editor category combo — re-apply translated item text by data role.
    if (edit_category_) {
        for (int i = 0; i < edit_category_->count(); ++i)
            edit_category_->setItemText(i, category_display_label(edit_category_->itemData(i).toString()));
    }

    // Notes list panel
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search notes..."));
    if (new_btn_)
        new_btn_->setText(tr("+ NEW"));

    // Editor / viewer
    if (empty_label_)
        empty_label_->setText(tr("Select a note or create a new one"));

    // View-mode toolbar
    if (view_edit_btn_)
        view_edit_btn_->setText(tr("EDIT"));
    if (view_fav_btn_)
        view_fav_btn_->setText(tr("FAV"));
    if (view_archive_btn_)
        view_archive_btn_->setText(tr("ARCHIVE"));
    if (view_delete_btn_)
        view_delete_btn_->setText(tr("DELETE"));

    // Edit-mode toolbar
    if (edit_save_btn_)
        edit_save_btn_->setText(tr("SAVE"));
    if (edit_cancel_btn_)
        edit_cancel_btn_->setText(tr("CANCEL"));
    if (edit_export_btn_) {
        edit_export_btn_->setText(tr("EXPORT"));
        edit_export_btn_->setToolTip(tr("Export this note as a Markdown file to the File Manager"));
    }

    // Edit-mode field labels
    if (lbl_cat_)
        lbl_cat_->setText(tr("CAT:"));
    if (lbl_pri_)
        lbl_pri_->setText(tr("PRI:"));
    if (lbl_sent_)
        lbl_sent_->setText(tr("SENT:"));
    if (lbl_tags_)
        lbl_tags_->setText(tr("TAGS:"));
    if (lbl_tickers_)
        lbl_tickers_->setText(tr("TICKERS:"));

    // Edit-mode placeholders
    if (edit_title_)
        edit_title_->setPlaceholderText(tr("Note title..."));
    if (edit_tags_)
        edit_tags_->setPlaceholderText(tr("tag1, tag2, ..."));
    if (edit_tickers_)
        edit_tickers_->setPlaceholderText(tr("AAPL, MSFT, ..."));
    if (edit_content_)
        edit_content_->setPlaceholderText(tr("Write your note here..."));

    // Re-render count + stats footers in the new language (without disturbing
    // the list selection — note titles are data and need no re-translation).
    if (count_label_)
        count_label_->setText(tr("%1 notes").arg(filtered_notes_.size()));
    if (stats_label_) {
        int active_count = 0;
        int fav_count = 0;
        for (const auto& n : notes_) {
            if (n.is_archived)
                continue;
            ++active_count;
            if (n.is_favorite)
                ++fav_count;
        }
        stats_label_->setText(tr("Total: %1  |  Fav: %2").arg(active_count).arg(fav_count));
    }

    // Re-render the currently-displayed note metadata (PRI:/SENT:/… prefixes).
    if (selected_note_id_ > 0) {
        auto r = fincept::NotesRepository::instance().get(selected_note_id_);
        if (r.is_ok())
            show_note(r.value());
    }
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap NotesScreen::save_state() const {
    QVariantMap state{
        {"category", current_category_},
        {"note_id", selected_note_id_},
        {"editing", is_editing_},
    };
    if (is_editing_) {
        if (edit_title_)
            state["draft_title"] = edit_title_->text();
        if (edit_content_)
            state["draft_content"] = edit_content_->toPlainText();
        if (edit_tags_)
            state["draft_tags"] = edit_tags_->text();
        if (edit_tickers_)
            state["draft_tickers"] = edit_tickers_->text();
        if (edit_category_)
            state["draft_category"] = edit_category_->currentIndex();
        if (edit_priority_)
            state["draft_priority"] = edit_priority_->currentText();
        if (edit_sentiment_)
            state["draft_sentiment"] = edit_sentiment_->currentText();
    }
    return state;
}

void NotesScreen::restore_state(const QVariantMap& state) {
    const QString cat = state.value("category", "ALL").toString();
    const int note_id = state.value("note_id", -1).toInt();

    // Select the matching category row. Drive the sidebar itself (its
    // currentRowChanged signal runs on_category_selected) — calling the slot
    // directly filtered the list but left the sidebar highlighting "ALL NOTES".
    for (int i = 0; i < kCategories.size(); ++i) {
        if (kCategories[i].id == cat) {
            if (category_list_ && category_list_->currentRow() != i)
                category_list_->setCurrentRow(i);
            break;
        }
    }

    // Select the matching note (via the list widget so its highlight follows)
    if (note_id >= 0) {
        for (int i = 0; i < filtered_notes_.size(); ++i) {
            if (filtered_notes_[i].id == note_id) {
                notes_list_->setCurrentRow(i);
                break;
            }
        }
    }

    // Restore draft editor state
    if (state.value("editing", false).toBool()) {
        if (note_id >= 0) {
            enter_edit_mode();
        } else if (right_stack_) {
            // A not-yet-saved note: start from a blank editor and baseline it, so
            // an empty draft does not trigger an "unsaved changes" prompt.
            clear_editor();
            right_stack_->setCurrentIndex(2);
            editor_baseline_ = editor_fingerprint();
        }

        if (edit_title_)
            edit_title_->setText(state.value("draft_title").toString());
        if (edit_content_)
            edit_content_->setPlainText(state.value("draft_content").toString());
        if (edit_tags_)
            edit_tags_->setText(state.value("draft_tags").toString());
        if (edit_tickers_)
            edit_tickers_->setText(state.value("draft_tickers").toString());
        if (edit_category_ && state.contains("draft_category"))
            edit_category_->setCurrentIndex(state.value("draft_category").toInt());
        if (edit_priority_ && state.contains("draft_priority"))
            edit_priority_->setCurrentText(state.value("draft_priority").toString());
        if (edit_sentiment_ && state.contains("draft_sentiment"))
            edit_sentiment_->setCurrentText(state.value("draft_sentiment").toString());
        is_editing_ = true;
    }
}

// ── Visibility lifecycle + MCP-driven UI sync ───────────────────────────────
// MCP notes tools publish notes.created / notes.deleted when the LLM mutates
// notes via AI Chat or Finagent. We subscribe while visible so the list
// reflects the change without manual refresh. EventBus handlers may fire
// on a worker thread; each callback marshals to the screen's thread.

void NotesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // MCP events are only received while visible — pick up anything that changed
    // meanwhile (the editor's fields are not touched by a reload).
    load_notes();
    subscribe_mcp_events();
    // Rate-gated pull of cloud notes on screen entry (no-op when sync is off).
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("note"));
}

void NotesScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // Flush a pending autosave instead of letting the timer fire on a hidden screen.
    if (autosave_timer_->isActive()) {
        autosave_timer_->stop();
        autosave_now();
    }
    unsubscribe_mcp_events();
}

void NotesScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty())
        return; // idempotent

    QPointer<NotesScreen> self = this;
    auto on_notes_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self)
                    return;
                self->load_notes();
            },
            Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe(this, "notes.created", on_notes_changed));
    mcp_event_subs_.append(bus.subscribe(this, "notes.deleted", on_notes_changed));
    mcp_event_subs_.append(bus.subscribe(this, "notes.updated", on_notes_changed));
}

void NotesScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

} // namespace fincept::screens
