#include "screens/notes/NotesScreen.h"

#include "core/logging/Logger.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QRegularExpression>
#include <QTextStream>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui::colors;

// ── Style constants (Obsidian Design System) ────────────────────────────────

// §5.2 — List items: 26px row, alternating bg, hairline borders
static const QString kListItem =
    "QListWidget { background: %1; border: none; font-family: 'Consolas','Courier New',monospace; font-size: 12px; }"
    "QListWidget::item { padding: 6px 12px; color: #e5e5e5; border-bottom: 1px solid %2; height: 26px; }"
    "QListWidget::item:selected { background: #161616; color: #e5e5e5; }"
    "QListWidget::item:hover { background: #161616; }";

// §5.6 — Input fields: #0a0a0a bg, focus → #333333
static const QString kInput =
    "QLineEdit { background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a; "
    "padding: 3px 6px; font-size: 13px; font-family: 'Consolas','Courier New',monospace; height: 28px; }"
    "QLineEdit:focus { border-color: #333333; }"
    "QLineEdit::selection { background: #d97706; color: #080808; }";

// §5.6 — Combo: same input style
static const QString kCombo = "QComboBox { background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a; "
                              "padding: 3px 6px; font-size: 12px; font-family: 'Consolas','Courier New',monospace; }"
                              "QComboBox:focus { border-color: #333333; }"
                              "QComboBox QAbstractItemView { background: #0a0a0a; color: #e5e5e5; "
                              "selection-background-color: #1a1a1a; border: 1px solid #1a1a1a; }";

// §5.6 — Text editor: same as input
static const QString kTextEdit = "QTextEdit { background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a; "
                                 "padding: 6px; font-size: 14px; font-family: 'Consolas','Courier New',monospace; }"
                                 "QTextEdit:focus { border-color: #333333; }"
                                 "QTextEdit::selection { background: #d97706; color: #080808; }";

// §5.5 — Accent button (amber): rgba bg + amber text + amber_dim border
static const QString kActionBtn =
    "QPushButton { background: rgba(217,119,6,0.1); color: #d97706; "
    "border: 1px solid #78350f; padding: 0 12px; height: 24px; "
    "font-size: 12px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #d97706; color: #080808; }";

// §5.5 — Standard button: #111111 bg + #808080 text
static const QString kSecondaryBtn =
    "QPushButton { background: #111111; color: #808080; border: 1px solid #1a1a1a; "
    "padding: 0 10px; height: 24px; "
    "font-size: 11px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { color: #e5e5e5; background: #161616; }";

// §5.5 — Danger button (red): rgba bg + red text + dark red border
static const QString kDangerBtn =
    "QPushButton { background: rgba(220,38,38,0.1); color: #dc2626; "
    "border: 1px solid #7f1d1d; padding: 0 10px; height: 24px; "
    "font-size: 11px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #dc2626; color: #e5e5e5; }";

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
};

static const QStringList kPriorities = {"HIGH", "MEDIUM", "LOW"};
static const QStringList kSentiments = {"BULLISH", "BEARISH", "NEUTRAL"};

static QString priority_color(const QString& p) {
    if (p == "HIGH")
        return "#dc2626";
    if (p == "MEDIUM")
        return "#ca8a04";
    return "#16a34a";
}

static QString sentiment_color(const QString& s) {
    if (s == "BULLISH")
        return "#16a34a";
    if (s == "BEARISH")
        return "#dc2626";
    return "#ca8a04";
}

// ── Constructor ──────────────────────────────────────────────────────────────

NotesScreen::NotesScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(BG_BASE));
    build_ui();
    load_notes();
}

void NotesScreen::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet("QSplitter::handle { background: #1a1a1a; width: 1px; }");

    splitter->addWidget(build_category_sidebar());
    splitter->addWidget(build_notes_list_panel());
    splitter->addWidget(build_editor_panel());
    splitter->setSizes({200, 320, 600});
    splitter->setStretchFactor(2, 1);

    root->addWidget(splitter);
}

// ── Category Sidebar ─────────────────────────────────────────────────────────

QWidget* NotesScreen::build_category_sidebar() {
    auto* panel = new QWidget;
    panel->setFixedWidth(200);
    panel->setStyleSheet(QString("background: %1; border-right: 1px solid %2;").arg(BG_SURFACE, BORDER_DIM));

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Header
    auto* header = new QLabel("  CATEGORIES");
    header->setFixedHeight(34);
    header->setStyleSheet(QString("background: %1; color: %2; font-size: 12px; font-weight: 700; "
                                  "letter-spacing: 0.5px; border-bottom: 1px solid %3; padding-left: 12px; "
                                  "font-family: 'Consolas','Courier New',monospace;")
                              .arg(BG_RAISED, AMBER, BORDER_DIM));
    lay->addWidget(header);

    // Category list
    category_list_ = new QListWidget;
    category_list_->setStyleSheet(kListItem.arg(BG_SURFACE, BORDER_DIM));
    for (const auto& cat : kCategories) {
        category_list_->addItem(cat.label);
    }
    category_list_->setCurrentRow(0);
    connect(category_list_, &QListWidget::currentRowChanged, this, &NotesScreen::on_category_selected);
    lay->addWidget(category_list_);

    // Stats
    stats_label_ = new QLabel;
    stats_label_->setFixedHeight(26);
    stats_label_->setAlignment(Qt::AlignCenter);
    stats_label_->setStyleSheet(QString("background: %1; color: %2; font-size: 11px; border-top: 1px solid %3; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(BG_SURFACE, TEXT_SECONDARY, BORDER_DIM));
    lay->addWidget(stats_label_);

    return panel;
}

// ── Notes List Panel ─────────────────────────────────────────────────────────

QWidget* NotesScreen::build_notes_list_panel() {
    auto* panel = new QWidget;
    panel->setMinimumWidth(280);
    panel->setStyleSheet(QString("background: %1;").arg(BG_SURFACE));

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Header with search + new button
    auto* toolbar = new QWidget;
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG_RAISED, BORDER_DIM));
    auto* tl = new QHBoxLayout(toolbar);
    tl->setContentsMargins(8, 0, 8, 0);
    tl->setSpacing(6);

    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search notes...");
    search_input_->setStyleSheet(kInput);
    search_input_->setFixedHeight(28);
    connect(search_input_, &QLineEdit::textChanged, this, &NotesScreen::on_search_changed);
    tl->addWidget(search_input_);

    auto* new_btn = new QPushButton("+ NEW");
    new_btn->setFixedSize(70, 24);
    new_btn->setStyleSheet(kActionBtn);
    connect(new_btn, &QPushButton::clicked, this, &NotesScreen::on_new_note);
    tl->addWidget(new_btn);

    lay->addWidget(toolbar);

    // Notes list
    notes_list_ = new QListWidget;
    notes_list_->setStyleSheet(kListItem.arg(BG_SURFACE, BORDER_DIM));
    connect(notes_list_, &QListWidget::currentRowChanged, this, &NotesScreen::on_note_selected);
    lay->addWidget(notes_list_);

    // Footer count
    count_label_ = new QLabel("0 notes");
    count_label_->setFixedHeight(26);
    count_label_->setAlignment(Qt::AlignCenter);
    count_label_->setStyleSheet(QString("background: %1; color: %2; font-size: 11px; border-top: 1px solid %3; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(BG_SURFACE, TEXT_TERTIARY, BORDER_DIM));
    lay->addWidget(count_label_);

    return panel;
}

// ── Editor / Viewer Panel ────────────────────────────────────────────────────

QWidget* NotesScreen::build_editor_panel() {
    right_stack_ = new QStackedWidget;

    // ── Page 0: Empty state ──────────────────────────────────────────────────
    auto* empty = new QWidget;
    empty->setStyleSheet(QString("background: %1;").arg(BG_BASE));
    auto* el = new QVBoxLayout(empty);
    el->setAlignment(Qt::AlignCenter);
    auto* empty_label = new QLabel("Select a note or create a new one");
    empty_label->setStyleSheet(
        QString("color: %1; font-size: 14px; font-family: 'Consolas','Courier New',monospace;").arg(TEXT_TERTIARY));
    el->addWidget(empty_label, 0, Qt::AlignCenter);
    right_stack_->addWidget(empty); // index 0

    // ── Page 1: View mode ────────────────────────────────────────────────────
    auto* view_page = new QWidget;
    view_page->setStyleSheet(QString("background: %1;").arg(BG_BASE));
    auto* vl = new QVBoxLayout(view_page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    // View toolbar
    auto* view_toolbar = new QHBoxLayout;
    auto* edit_btn = new QPushButton("EDIT");
    edit_btn->setStyleSheet(kSecondaryBtn);
    connect(edit_btn, &QPushButton::clicked, this, &NotesScreen::enter_edit_mode);
    view_toolbar->addWidget(edit_btn);

    auto* fav_btn = new QPushButton("FAV");
    fav_btn->setStyleSheet(kSecondaryBtn);
    connect(fav_btn, &QPushButton::clicked, this, &NotesScreen::on_toggle_favorite);
    view_toolbar->addWidget(fav_btn);

    auto* archive_btn = new QPushButton("ARCHIVE");
    archive_btn->setStyleSheet(kSecondaryBtn);
    connect(archive_btn, &QPushButton::clicked, this, &NotesScreen::on_toggle_archive);
    view_toolbar->addWidget(archive_btn);

    auto* del_btn = new QPushButton("DELETE");
    del_btn->setStyleSheet(kDangerBtn);
    connect(del_btn, &QPushButton::clicked, this, &NotesScreen::on_delete_note);
    view_toolbar->addWidget(del_btn);

    view_toolbar->addStretch();
    vl->addLayout(view_toolbar);

    view_title_ = new QLabel;
    view_title_->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: bold; font-family: 'Consolas','Courier New',monospace;")
            .arg(TEXT_PRIMARY));
    view_title_->setWordWrap(true);
    vl->addWidget(view_title_);

    view_meta_ = new QLabel;
    view_meta_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-family: 'Consolas','Courier New',monospace;").arg(TEXT_SECONDARY));
    view_meta_->setWordWrap(true);
    vl->addWidget(view_meta_);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(BORDER_DIM));
    vl->addWidget(sep);

    // Scroll area for content
    auto* view_scroll = new QScrollArea;
    view_scroll->setWidgetResizable(true);
    view_scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    view_content_ = new QLabel;
    view_content_->setStyleSheet(QString("color: %1; font-size: 14px; font-family: 'Consolas','Courier New',monospace; "
                                         "line-height: 1.6; background: transparent; padding: 4px;")
                                     .arg(TEXT_PRIMARY));
    view_content_->setWordWrap(true);
    view_content_->setTextFormat(Qt::PlainText);
    view_content_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    view_scroll->setWidget(view_content_);
    vl->addWidget(view_scroll, 1);

    right_stack_->addWidget(view_page); // index 1

    // ── Page 2: Edit mode ────────────────────────────────────────────────────
    auto* edit_page = new QWidget;
    edit_page->setStyleSheet(QString("background: %1;").arg(BG_BASE));
    auto* edl = new QVBoxLayout(edit_page);
    edl->setContentsMargins(14, 14, 14, 14);
    edl->setSpacing(10);

    // Save/Cancel bar
    auto* edit_toolbar = new QHBoxLayout;
    auto* save_btn = new QPushButton("SAVE");
    save_btn->setStyleSheet(kActionBtn);
    connect(save_btn, &QPushButton::clicked, this, &NotesScreen::on_save_note);
    edit_toolbar->addWidget(save_btn);

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setStyleSheet(kSecondaryBtn);
    connect(cancel_btn, &QPushButton::clicked, this, [this]() {
        if (selected_note_id_ > 0) {
            enter_view_mode();
        } else {
            right_stack_->setCurrentIndex(0);
            is_editing_ = false;
        }
    });
    edit_toolbar->addWidget(cancel_btn);

    auto* export_btn = new QPushButton("EXPORT");
    export_btn->setStyleSheet(kSecondaryBtn);
    export_btn->setToolTip("Export this note as a Markdown file to the File Manager");
    connect(export_btn, &QPushButton::clicked, this, &NotesScreen::on_export_note);
    edit_toolbar->addWidget(export_btn);

    edit_toolbar->addStretch();
    edl->addLayout(edit_toolbar);

    // Title
    edit_title_ = new QLineEdit;
    edit_title_->setPlaceholderText("Note title...");
    edit_title_->setStyleSheet(kInput + " QLineEdit { font-size: 16px; font-weight: 700; }");
    edit_title_->setFixedHeight(28);
    edl->addWidget(edit_title_);

    // Metadata row
    auto* meta_row = new QHBoxLayout;
    meta_row->setSpacing(8);

    auto make_label = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(QString("color: %1; font-size: 11px; font-family: 'Consolas','Courier New',monospace;")
                             .arg(TEXT_SECONDARY));
        return l;
    };

    meta_row->addWidget(make_label("CAT:"));
    edit_category_ = new QComboBox;
    edit_category_->setStyleSheet(kCombo);
    for (int i = 1; i < kCategories.size(); ++i) // skip "ALL"
        edit_category_->addItem(kCategories[i].label, kCategories[i].id);
    meta_row->addWidget(edit_category_);

    meta_row->addWidget(make_label("PRI:"));
    edit_priority_ = new QComboBox;
    edit_priority_->setStyleSheet(kCombo);
    for (const auto& p : kPriorities)
        edit_priority_->addItem(p);
    edit_priority_->setCurrentText("MEDIUM");
    meta_row->addWidget(edit_priority_);

    meta_row->addWidget(make_label("SENT:"));
    edit_sentiment_ = new QComboBox;
    edit_sentiment_->setStyleSheet(kCombo);
    for (const auto& s : kSentiments)
        edit_sentiment_->addItem(s);
    edit_sentiment_->setCurrentText("NEUTRAL");
    meta_row->addWidget(edit_sentiment_);

    meta_row->addStretch();
    edl->addLayout(meta_row);

    // Tags + Tickers row
    auto* tag_row = new QHBoxLayout;
    tag_row->setSpacing(8);
    tag_row->addWidget(make_label("TAGS:"));
    edit_tags_ = new QLineEdit;
    edit_tags_->setPlaceholderText("tag1, tag2, ...");
    edit_tags_->setStyleSheet(kInput);
    tag_row->addWidget(edit_tags_);

    tag_row->addWidget(make_label("TICKERS:"));
    edit_tickers_ = new QLineEdit;
    edit_tickers_->setPlaceholderText("AAPL, MSFT, ...");
    edit_tickers_->setStyleSheet(kInput);
    tag_row->addWidget(edit_tickers_);
    edl->addLayout(tag_row);

    // Content editor
    edit_content_ = new QTextEdit;
    edit_content_->setStyleSheet(kTextEdit);
    edit_content_->setPlaceholderText("Write your note here...");
    edl->addWidget(edit_content_, 1);

    right_stack_->addWidget(edit_page); // index 2

    return right_stack_;
}

// ── Data Loading ─────────────────────────────────────────────────────────────

void NotesScreen::load_notes() {
    auto r = fincept::NotesRepository::instance().list_all();
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

    for (const auto& n : notes_) {
        // Category filter
        if (current_category_ != "ALL" && n.category != current_category_)
            continue;
        // Search filter
        if (!search.isEmpty()) {
            if (!n.title.toLower().contains(search) && !n.content.toLower().contains(search) &&
                !n.tags.toLower().contains(search) && !n.tickers.toLower().contains(search))
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
    notes_list_->blockSignals(false);

    count_label_->setText(QString("%1 notes").arg(filtered_notes_.size()));

    // Update stats
    int fav_count = 0;
    for (const auto& n : notes_) {
        if (n.is_favorite)
            ++fav_count;
    }
    stats_label_->setText(QString("Total: %1  |  Fav: %2").arg(notes_.size()).arg(fav_count));
}

// ── Slots ────────────────────────────────────────────────────────────────────

void NotesScreen::on_category_selected(int row) {
    if (row < 0 || row >= kCategories.size())
        return;
    current_category_ = kCategories[row].id;
    update_notes_list();
    right_stack_->setCurrentIndex(0);
    selected_note_id_ = -1;
}

void NotesScreen::on_note_selected(int row) {
    if (row < 0 || row >= filtered_notes_.size())
        return;
    const auto& note = filtered_notes_[row];
    selected_note_id_ = note.id;
    show_note(note);
    enter_view_mode();
}

void NotesScreen::on_new_note() {
    selected_note_id_ = -1;
    clear_editor();

    // Pre-select current category if not ALL
    if (current_category_ != "ALL") {
        int idx = edit_category_->findData(current_category_);
        if (idx >= 0)
            edit_category_->setCurrentIndex(idx);
    }

    is_editing_ = true;
    right_stack_->setCurrentIndex(2);
}

void NotesScreen::on_save_note() {
    QString title = edit_title_->text().trimmed();
    if (title.isEmpty()) {
        edit_title_->setFocus();
        return;
    }

    fincept::FinancialNote note;
    note.title = title;
    note.content = edit_content_->toPlainText();
    note.category = edit_category_->currentData().toString();
    note.priority = edit_priority_->currentText();
    note.sentiment = edit_sentiment_->currentText();
    note.tags = edit_tags_->text().trimmed();
    note.tickers = edit_tickers_->text().trimmed().toUpper();
    note.word_count = note.content.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();

    auto& repo = fincept::NotesRepository::instance();

    if (selected_note_id_ > 0) {
        // Update existing
        note.id = selected_note_id_;
        auto r = repo.update(note);
        if (r.is_err()) {
            LOG_ERROR("Notes", "Failed to update note");
        }
    } else {
        // Create new
        auto r = repo.create(note);
        if (r.is_ok()) {
            selected_note_id_ = static_cast<int>(r.value());
        } else {
            LOG_ERROR("Notes", "Failed to create note");
        }
    }

    load_notes();

    // Select the saved note
    for (int i = 0; i < filtered_notes_.size(); ++i) {
        if (filtered_notes_[i].id == selected_note_id_) {
            notes_list_->setCurrentRow(i);
            break;
        }
    }
    enter_view_mode();
}

void NotesScreen::on_delete_note() {
    if (selected_note_id_ <= 0)
        return;

    auto reply = QMessageBox::question(this, "Delete Note", "Are you sure you want to delete this note?",
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    fincept::NotesRepository::instance().remove(selected_note_id_);
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
    update_notes_list();
}

void NotesScreen::on_export_note() {
    QString title   = edit_title_->text().trimmed();
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
        return;
    }

    QTextStream out(&f);
    out << "# " << title << "\n\n" << content << "\n";
    f.close();

    QFileInfo info(dest);
    svc.register_file(stored_name, title + ".md", info.size(),
                      "text/markdown", "notes");

    LOG_INFO("Notes", "Exported note to File Manager: " + stored_name);
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void NotesScreen::show_note(const fincept::FinancialNote& note) {
    view_title_->setText(note.is_favorite ? ("* " + note.title) : note.title);

    QStringList meta_parts;
    meta_parts << note.category;
    meta_parts << QString("PRI: %1").arg(note.priority);
    meta_parts << QString("SENT: %1").arg(note.sentiment);
    if (!note.tags.isEmpty())
        meta_parts << QString("TAGS: %1").arg(note.tags);
    if (!note.tickers.isEmpty())
        meta_parts << QString("TICKERS: %1").arg(note.tickers);
    meta_parts << note.updated_at;

    view_meta_->setText(meta_parts.join("  |  "));
    view_content_->setText(note.content);
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
    // Find the note and populate editor
    for (const auto& n : notes_) {
        if (n.id == selected_note_id_) {
            edit_title_->setText(n.title);
            edit_content_->setPlainText(n.content);
            int cat_idx = edit_category_->findData(n.category);
            if (cat_idx >= 0)
                edit_category_->setCurrentIndex(cat_idx);
            edit_priority_->setCurrentText(n.priority);
            edit_sentiment_->setCurrentText(n.sentiment);
            edit_tags_->setText(n.tags);
            edit_tickers_->setText(n.tickers);
            break;
        }
    }
    right_stack_->setCurrentIndex(2);
}

void NotesScreen::enter_view_mode() {
    is_editing_ = false;
    right_stack_->setCurrentIndex(1);
}

} // namespace fincept::screens
