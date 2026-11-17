// src/screens/notes/NotesScreen_Actions.cpp — selection and the detail pane, the
// editor session (autosave that survives a kill), card actions (new, templates,
// pin, archive, status, review, tags, duplicate, delete with undo, export /
// import), drag / keyboard moves, the command field and the menus.
#include "core/logging/Logger.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "screens/notes/NotesBoardView.h"
#include "screens/notes/NotesDetailPane.h"
#include "screens/notes/NotesEditor.h"
#include "screens/notes/NotesModels.h"
#include "screens/notes/NotesScreen.h"
#include "screens/notes/NotesWallView.h"
#include "services/file_manager/FileManagerService.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QScreen>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTime>
#include <QTimer>

#include <algorithm>
#include <climits>

namespace fincept::screens {

using notes::BoardViewMode;
using notes::GroupBy;
using notes::NoteEntry;
namespace nb = fincept::notes_board;

namespace {

/// Tag on the notes.* events this screen publishes, so its own subscriber can skip
/// them while the dashboard Notes tile and other listeners refresh.
const QString kNotesScreenEventSource = QStringLiteral("notes_screen");

void nsa_publish(const char* topic, int note_id) {
    EventBus::instance().publish(QString::fromLatin1(topic), {{"id", note_id}, {"source", kNotesScreenEventSource}});
}

constexpr int kUndoMs = 8000;

QString nsa_template_name(const QString& id) {
    for (const auto& t : notes::builtin_templates())
        if (t.id == id)
            return QCoreApplication::translate("NotesBoard", t.name.toUtf8().constData());
    return id;
}

notes::MarkdownDoc nsa_to_doc(const fincept::FinancialNote& n) {
    notes::MarkdownDoc d;
    d.title = n.title;
    d.body = n.content;
    d.category = n.category;
    d.priority = n.priority;
    d.sentiment = n.sentiment;
    d.tags = notes::split_tags(n.tags);
    d.tickers = notes::split_tickers(n.tickers);
    d.created = n.created_at;
    d.updated = n.updated_at;
    d.pinned = n.is_favorite;
    d.archived = n.is_archived;
    return d;
}

fincept::FinancialNote nsa_from_doc(const notes::MarkdownDoc& d) {
    fincept::FinancialNote n;
    n.title = d.title.left(500);
    n.content = d.body;
    n.category = d.category.isEmpty() ? QStringLiteral("GENERAL") : notes::category_key(d.category);
    n.priority = notes::priority_rank(d.priority) > 0 ? d.priority.trimmed().toUpper() : QStringLiteral("MEDIUM");
    const QString s = d.sentiment.trimmed().toUpper();
    n.sentiment = (s == QLatin1String("BULLISH") || s == QLatin1String("BEARISH")) ? s : QStringLiteral("NEUTRAL");
    n.tags = notes::join_list(d.tags);
    QStringList tickers;
    for (const QString& t : d.tickers) {
        const QString nt = notes::normalize_ticker(t);
        if (!nt.isEmpty() && !tickers.contains(nt))
            tickers << nt;
    }
    n.tickers = notes::join_list(notes::merge_unique(tickers, notes::extract_cashtags(d.body)));
    n.is_favorite = d.pinned;
    n.is_archived = d.archived;
    n.word_count = notes::word_count(d.body);
    return n;
}

struct NsaExportedFile {
    QString stored_name;
    QString display_name;
    qint64 size = 0;
};

struct NsaExportResult {
    QVector<NsaExportedFile> files;
    int failed = 0;
};

/// "<dir>/<stem>.md", or "<stem>_<n>.md" when that exists.
QString nsa_unique_md_path(const QString& dir, const QString& stem) {
    QString path = QDir(dir).filePath(stem + QStringLiteral(".md"));
    for (int n = 2; QFile::exists(path) && n < 10000; ++n)
        path = QDir(dir).filePath(QStringLiteral("%1_%2.md").arg(stem).arg(n));
    return path;
}

QString nsa_now_db() {
    return notes::db_time(QDateTime::currentDateTimeUtc());
}

} // namespace

// ── Selection / detail ───────────────────────────────────────────────────────

QVector<int> NotesScreen::selected_ids() const {
    switch (board().view) {
    case BoardViewMode::Wall:
        return wall_view_->selected_ids();
    case BoardViewMode::List: {
        QVector<int> ids;
        if (!table_view_->selectionModel())
            return ids;
        QModelIndexList rows = table_view_->selectionModel()->selectedRows(0);
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
        for (const QModelIndex& i : rows)
            ids << i.data(notes::NotesTableModel::IdRole).toInt();
        return ids;
    }
    case BoardViewMode::Board:
        break;
    }
    return board_view_->selected_ids();
}

int NotesScreen::current_card() const {
    switch (board().view) {
    case BoardViewMode::Wall:
        return wall_view_->current_id();
    case BoardViewMode::List: {
        const QModelIndex i = table_view_->currentIndex();
        return i.isValid() ? i.data(notes::NotesTableModel::IdRole).toInt() : 0;
    }
    case BoardViewMode::Board:
        break;
    }
    return board_view_->current_id();
}

void NotesScreen::select_in_views(const QVector<int>& ids, int current, bool scroll) {
    const bool was = syncing_selection_;
    syncing_selection_ = true;
    board_view_->select_ids(ids, current, scroll && board().view == BoardViewMode::Board);
    if (board().view == BoardViewMode::Wall)
        wall_view_->select_ids(ids, current, scroll);
    if (board().view == BoardViewMode::List && table_view_->selectionModel()) {
        QItemSelection sel;
        for (int id : ids) {
            const int row = table_model_->row_of(id);
            if (row < 0)
                continue;
            const QModelIndex p = table_proxy_->mapFromSource(table_model_->index(row, 0));
            sel.select(p, p);
        }
        table_view_->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        const int row = table_model_->row_of(current);
        if (row >= 0) {
            const QModelIndex p = table_proxy_->mapFromSource(table_model_->index(row, 0));
            table_view_->selectionModel()->setCurrentIndex(p, QItemSelectionModel::NoUpdate);
            if (scroll)
                table_view_->scrollTo(p);
        }
    }
    syncing_selection_ = was;
}

void NotesScreen::on_selection_changed() {
    if (syncing_selection_)
        return;
    update_status_bar();
    show_detail_for_selection();
}

void NotesScreen::show_detail_for_selection() {
    const QVector<int> ids = selected_ids();
    if (ids.size() > 1) {
        show_bulk(ids);
        return;
    }
    if (ids.size() == 1 && (ids.first() != open_id() || detail_->page() != notes::NotesDetailPane::Page::Note))
        open_note(ids.first());
    // Nothing selected: the open note stays in the pane (never a blank pane).
}

void NotesScreen::set_focus_mode(bool on) {
    if (on && !editor_open_)
        on = false;
    if (on == focus_mode_)
        return;
    focus_mode_ = on;
    hide_peek();
    if (on && !detail_visible_)
        set_detail_visible(true);
    left_->setVisible(!on); // the full editor: the board steps aside
    detail_->set_focus_mode(on);
    if (!on) {
        QWidget* v = view_stack_->currentWidget();
        (v == wall_page_ ? static_cast<QWidget*>(wall_view_) : v)->setFocus(Qt::OtherFocusReason);
    }
    notify_state_changed();
}

void NotesScreen::set_detail_visible(bool on) {
    detail_visible_ = on;
    if (detail_->isVisibleTo(this) != on)
        detail_->setVisible(on);
    bar_->set_action_checked(QStringLiteral("detail"), on);
    if (on) {
        const QList<int> sizes = split_->sizes();
        if (sizes.size() == 2 && sizes.at(1) < 120) {
            const QFontMetrics fm(fincept::ui::fonts::font(-1));
            const int w = std::max(380, fm.averageCharWidth() * 52);
            split_->setSizes({std::max(200, sizes.at(0) - w), w});
        }
    }
    notify_state_changed();
}

void NotesScreen::peek(int id) {
    const NoteEntry* e = find_entry(id);
    if (!e)
        return;
    if (peek_->isVisible() && peek_id_ == id) {
        hide_peek();
        return;
    }
    peek_id_ = id;
    peek_title_->setText(e->note.title.isEmpty() ? tr("Untitled note") : e->note.title);
    const QString md = e->note.content.left(20000);
    peek_body_->setHtml(md.trimmed().isEmpty()
                            ? QStringLiteral("<p>%1</p>").arg(tr("No text").toHtmlEscaped())
                            : fincept::ui::MarkdownRenderer::render(notes::markdown_with_symbol_links(md)));
    const QFontMetrics fm(fincept::ui::fonts::font(0));
    const QSize sz(std::min(fm.averageCharWidth() * 64 + 28, 680), std::min(fm.height() * 22 + 40, 520));
    peek_->resize(sz);
    QRect card;
    QWidget* host = nullptr;
    if (board().view == BoardViewMode::Board) {
        card = board_view_->card_rect_for(id);
        host = board_view_->viewport();
    } else if (board().view == BoardViewMode::Wall) {
        card = wall_view_->card_rect_for(id);
        host = wall_view_->viewport();
    } else {
        const int row = table_model_->row_of(id);
        if (row >= 0) {
            card = table_view_->visualRect(table_proxy_->mapFromSource(table_model_->index(row, 0)));
            host = table_view_->viewport();
        }
    }
    QPoint at = QCursor::pos();
    if (host && card.isValid()) {
        const QRect g(host->mapToGlobal(card.topLeft()), card.size());
        at = QPoint(g.right() + 8, g.top());
        const QScreen* screen = host->screen();
        if (screen && at.x() + sz.width() > screen->availableGeometry().right())
            at.setX(std::max(screen->availableGeometry().left(), g.left() - sz.width() - 8));
        if (screen && at.y() + sz.height() > screen->availableGeometry().bottom())
            at.setY(std::max(screen->availableGeometry().top(), screen->availableGeometry().bottom() - sz.height()));
    }
    peek_->move(at);
    peek_->show();
    peek_->raise();
}

void NotesScreen::hide_peek() {
    if (peek_ && peek_->isVisible())
        peek_->hide();
    peek_id_ = 0;
}

// ── Editor session ───────────────────────────────────────────────────────────

notes::NotesEditor* NotesScreen::editor() {
    if (editor_)
        return editor_;
    editor_ = detail_->editor();
    editor_->set_mode(static_cast<notes::NotesEditor::Mode>(editor_mode_));
    connect(editor_, &notes::NotesEditor::edited, this, &NotesScreen::on_editor_edited);
    connect(editor_, &notes::NotesEditor::pin_toggled, this, [this](bool on) {
        const int id = open_id();
        if (id > 0) {
            set_pinned({id}, on);
        } else if (!editor_->is_blank()) {
            queued_revision_ = ~quint64(0); // a new note takes the pin with its (next) save
            save_editor_async();
        }
    });
    connect(editor_, &notes::NotesEditor::mode_changed, this, [this](int m) {
        editor_mode_ = m;
        notify_state_changed();
    });
    connect(editor_, &notes::NotesEditor::tickers_changed, this, [this](const QStringList& tickers) {
        editor_symbols_ = tickers.mid(0, 12);
        detail_->set_tickers(tickers);
        for (const QString& s : editor_symbols_)
            if (const notes::CardQuote* q = quotes_.find(s))
                editor_->set_quote(s, fincept::ui::formatting::price(q->price), q->change_pct, q->has_change);
        subscribe_timer_->start();
    });
    connect(editor_, &notes::NotesEditor::symbol_clicked, this, &NotesScreen::show_symbol_menu);
    connect(editor_, &notes::NotesEditor::link_clicked, this, &NotesScreen::on_link);
    connect(editor_, &notes::NotesEditor::action_requested, this, &NotesScreen::on_editor_action);
    connect(editor_, &notes::NotesEditor::notice, this, [this](const QString& text) { show_notice(text); });
    update_completions();
    return editor_;
}

int NotesScreen::open_id() const {
    if (!editor_open_)
        return 0;
    if (current_id_ > 0)
        return current_id_;
    return slot_ ? slot_->load() : 0;
}

void NotesScreen::open_note(int id, bool force) {
    if (!find_entry(id))
        return;
    if (!force && editor_open_ && open_id() == id && detail_->page() == notes::NotesDetailPane::Page::Note)
        return;
    save_editor_async(); // the note being left keeps every keystroke (and updates its entry)
    const NoteEntry* e = find_entry(id);
    if (!e)
        return;
    const NoteEntry entry = *e;
    notes::NotesEditor* ed = editor();
    ++session_;
    slot_ = notes::make_id_slot(id);
    current_id_ = id;
    editor_open_ = true;
    session_base_ = entry.note;
    session_original_ = entry.note;
    session_has_original_ = true;
    session_meta_ = entry.meta;
    ed->load(entry.note);
    queued_revision_ = saved_revision_ = ed->revision();
    ed->set_save_state(notes::NotesEditor::SaveState::Saved);
    ed->set_highlight(query_.highlight_terms());
    if (static_cast<int>(ed->mode()) != editor_mode_)
        ed->set_mode(static_cast<notes::NotesEditor::Mode>(editor_mode_));
    detail_->set_note_meta(entry, entry.note.is_archived);
    detail_->set_tickers(entry.ticker_list);
    detail_->show_rename_offer({}, {});
    detail_->show_page(notes::NotesDetailPane::Page::Note);
    if (!detail_visible_)
        set_detail_visible(true);
    load_history(id);
    check_renamed_tickers();
    if (!selected_ids().contains(id))
        select_in_views({id}, id, true);
    notify_state_changed();
}

void NotesScreen::open_new_note(const fincept::FinancialNote& seed, bool save_now, const nb::NoteBoardMeta& meta) {
    save_editor_async();
    notes::NotesEditor* ed = editor();
    ++session_;
    slot_ = notes::make_id_slot(0);
    current_id_ = -1;
    editor_open_ = true;
    session_base_ = seed;
    session_has_original_ = false;
    session_meta_ = meta;
    session_meta_.note_id = 0;
    for (auto& r : resolve_requests_)
        fincept::services::InstrumentResolver::instance().cancel(r);
    resolve_requests_.clear();
    select_in_views({}, 0, false);
    ed->load(seed);
    queued_revision_ = saved_revision_ = ed->revision();
    ed->set_save_state(notes::NotesEditor::SaveState::Idle);
    ed->set_highlight({});
    if (editor_mode_ == static_cast<int>(notes::NotesEditor::Mode::Read))
        ed->set_mode(notes::NotesEditor::Mode::Write);
    detail_->set_new_note(meta.status);
    detail_->set_tickers(notes::merge_unique(notes::split_tickers(seed.tickers), notes::extract_cashtags(seed.content)));
    detail_->show_page(notes::NotesDetailPane::Page::Note);
    if (!detail_visible_)
        set_detail_visible(true);
    if (save_now) {
        // A template (or a restored draft) is something to keep: write it at once.
        queued_revision_ = ~quint64(0);
        save_editor_async();
        ed->focus_body();
    } else {
        ed->focus_title();
    }
    notify_state_changed();
}

void NotesScreen::close_editor() {
    autosave_timer_->stop();
    editor_open_ = false;
    current_id_ = -1;
    ++session_;
    slot_.reset();
    editor_symbols_.clear();
    for (auto& r : resolve_requests_)
        fincept::services::InstrumentResolver::instance().cancel(r);
    resolve_requests_.clear();
    subscribe_timer_->start();
}

void NotesScreen::show_empty() {
    close_editor();
    if (focus_mode_)
        set_focus_mode(false);
    update_empty_detail();
    detail_->show_page(notes::NotesDetailPane::Page::Empty);
    notify_state_changed();
}

void NotesScreen::show_bulk(const QVector<int>& ids) {
    save_editor_async();
    bool any_unpinned = false;
    bool all_archived = true;
    for (int id : ids)
        if (const NoteEntry* e = find_entry(id)) {
            any_unpinned = any_unpinned || !e->note.is_favorite;
            all_archived = all_archived && e->note.is_archived;
        }
    detail_->set_bulk(static_cast<int>(ids.size()), any_unpinned, all_archived);
    close_editor();
    if (focus_mode_)
        set_focus_mode(false);
    detail_->show_page(notes::NotesDetailPane::Page::Bulk);
}

void NotesScreen::on_editor_edited() {
    if (!editor_open_ || !editor_)
        return;
    editor_->set_save_state(notes::NotesEditor::SaveState::Unsaved);
    save_state_text_ = tr("UNSAVED");
    save_state_tone_ = static_cast<int>(fincept::ui::ScreenStatusBar::Tone::Warn);
    status_->set_item(QStringLiteral("save"), save_state_text_, fincept::ui::ScreenStatusBar::Tone::Warn);
    autosave_timer_->start();
}

void NotesScreen::save_editor_async() {
    autosave_timer_->stop();
    if (!editor_ || !editor_open_ || !slot_)
        return;
    const quint64 rev = editor_->revision();
    if (rev == queued_revision_)
        return;
    const int known = open_id();
    if (known <= 0 && editor_->is_blank()) {
        queued_revision_ = rev; // an untouched new note is not worth a row
        editor_->set_save_state(notes::NotesEditor::SaveState::Idle);
        return;
    }
    const NoteEntry* e = find_entry(known);
    fincept::FinancialNote n = editor_->collect(e ? e->note : session_base_);
    const QString now = nsa_now_db();
    n.updated_at = now;
    if (n.created_at.isEmpty())
        n.created_at = now;
    queued_revision_ = rev;
    ++saves_in_flight_;
    editor_->set_save_state(notes::NotesEditor::SaveState::Saving);
    save_state_text_ = tr("SAVING…");
    save_state_tone_ = static_cast<int>(fincept::ui::ScreenStatusBar::Tone::Dim);
    status_->set_item(QStringLiteral("save"), save_state_text_, fincept::ui::ScreenStatusBar::Tone::Dim);
    if (known > 0) {
        n.id = known;
        upsert_entry(n); // the board shows the edit now; the database follows
    }
    const quint64 session = session_;
    const notes::IdSlot slot = slot_;
    std::optional<nb::NoteBoardMeta> meta;
    if (known <= 0)
        meta = session_meta_;
    QPointer<NotesScreen> self = this;
    store_->save(
        n, slot,
        [self, session, rev, n, slot, meta](int id, bool ok) {
            if (!self)
                return;
            if (ok && n.id <= 0 && meta) {
                nb::NoteBoardMeta m = *meta;
                m.note_id = id;
                m.stored = true;
                self->pending_new_meta_.insert(id, m);
            }
            self->on_save_done(session, rev, id, ok, n, slot, 1);
        },
        meta);
}

void NotesScreen::on_save_done(quint64 session, quint64 revision, int id, bool ok, fincept::FinancialNote saved,
                               notes::IdSlot slot, int attempt) {
    --saves_in_flight_;
    if (!ok) {
        LOG_ERROR("Notes", fincept::logf("Saving a note failed",
                                         {{"note", saved.id > 0 ? QString::number(saved.id) : QStringLiteral("new")},
                                          {"attempt", attempt}}));
        if (attempt < 3) {
            // The note's text lives only in `saved` once its editor moved on: retry it.
            ++saves_in_flight_;
            QPointer<NotesScreen> self = this;
            QTimer::singleShot(1500 * attempt, this, [self, session, revision, saved, slot, attempt]() {
                if (!self)
                    return;
                self->store_->save(saved, slot, [self, session, revision, saved, slot, attempt](int nid, bool nok) {
                    if (self)
                        self->on_save_done(session, revision, nid, nok, saved, slot, attempt + 1);
                });
            });
        } else {
            if (session == session_ && editor_)
                editor_->set_save_state(notes::NotesEditor::SaveState::Failed,
                                        tr("Not saved — the local database refused the write"));
            save_state_text_ = tr("NOT SAVED");
            save_state_tone_ = static_cast<int>(fincept::ui::ScreenStatusBar::Tone::Danger);
            update_status_bar();
            show_notice(tr("A note could not be saved to the local database. Its text is kept in the editor."));
        }
        return;
    }
    if (discarded_sessions_.contains(session)) {
        // A new note thrown away while its saves were running (every one of them
        // removes the row: a later update must not bring it back).
        store_->remove({id}, [](bool) {});
        pending_new_meta_.remove(id);
        return;
    }
    if (pending_delete_ids_.contains(id))
        return; // deleted (undo window open) while this save ran
    // A note queued without an id was a new one: this save created its row.
    const bool created = saved.id <= 0;
    saved.id = id;
    if (created || !find_entry(id))
        upsert_entry(saved); // updates of known notes were shown when they were queued
    nsa_publish(created ? "notes.created" : "notes.updated", id);
    if (created) {
        // A card made on the wall goes where it was asked for.
        const auto pos = wall_pos_for_session_.constFind(session);
        if (pos != wall_pos_for_session_.constEnd() && wall_view_->board_id() > 0) {
            nb::NoteWallItem w;
            w.note_id = id;
            w.x = pos->x();
            w.y = pos->y();
            if (wall_dirty_board_ != wall_view_->board_id()) {
                flush_wall(false);
                wall_dirty_board_ = wall_view_->board_id();
            }
            wall_dirty_.insert(id, w);
            wall_timer_->start();
            wall_view_->place_card(id, *pos); // where it appears once the board build adds it
            wall_pos_for_session_.remove(session);
        }
    }
    if (session == session_ && editor_) {
        current_id_ = id;
        saved_revision_ = std::max(saved_revision_, revision);
        if (editor_->revision() == saved_revision_ && saves_in_flight_ == 0) {
            const QString hhmm = QTime::currentTime().toString(QStringLiteral("HH:mm"));
            editor_->set_save_state(notes::NotesEditor::SaveState::Saved, hhmm);
            save_state_text_ = tr("SAVED %1").arg(hhmm);
            save_state_tone_ = static_cast<int>(fincept::ui::ScreenStatusBar::Tone::Ok);
            update_status_bar();
        }
        editor_->set_dates(saved.created_at, saved.updated_at);
        if (created) {
            select_in_views({id}, id, true);
            load_history(id);
            notify_state_changed();
        }
    }
}

bool NotesScreen::flush_editor_sync() {
    autosave_timer_->stop();
    if (!editor_ || !editor_open_ || !slot_)
        return true;
    store_->wait_idle();
    const quint64 rev = editor_->revision();
    if (rev == queued_revision_)
        return true;
    if (open_id() <= 0 && editor_->is_blank())
        return true;
    const NoteEntry* e = find_entry(open_id());
    fincept::FinancialNote n = editor_->collect(e ? e->note : session_base_);
    queued_revision_ = rev;
    std::optional<nb::NoteBoardMeta> meta;
    if (!e)
        meta = session_meta_;
    const auto outcome = store_->save_now(n, slot_, meta);
    const bool ok = outcome.ok();
    if (ok)
        nsa_publish(e ? "notes.updated" : "notes.created", slot_->load());
    else
        LOG_ERROR("Notes", fincept::logf("Could not write the open note before closing",
                                         {{"note", e ? QString::number(e->note.id) : QStringLiteral("new")},
                                          {"error", outcome.error}}));
    return ok;
}

void NotesScreen::flush_for_quit() {
    if (flushed_for_quit_ || !store_)
        return;
    flushed_for_quit_ = true;
    status_->clear_notice();
    flush_editor_sync();
    if (!pending_delete_ids_.isEmpty()) {
        const QVector<int> ids(pending_delete_ids_.begin(), pending_delete_ids_.end());
        pending_delete_.clear();
        pending_delete_ids_.clear();
        if (store_->remove_now(ids))
            for (int id : ids)
                nsa_publish("notes.deleted", id);
    }
    flush_meta(true);
    flush_wall(true);
    flush_board(true);
    store_->wait_idle();
}

void NotesScreen::load_history(int id) {
    const quint64 request = ++history_request_;
    const NoteEntry* e = find_entry(id);
    const QString created = e ? e->note.created_at : QString();
    const QString updated = e ? e->note.updated_at : QString();
    QPointer<NotesScreen> self = this;
    store_->load_history(id, request, [self, id, created, updated](quint64 rid, QVector<nb::NoteBoardEvent> events) {
        if (!self || rid != self->history_request_ || self->open_id() != id)
            return;
        self->detail_->set_history(events, created, updated);
    });
}

void NotesScreen::check_renamed_tickers() {
    auto& resolver = fincept::services::InstrumentResolver::instance();
    for (auto r : resolve_requests_)
        resolver.cancel(r);
    resolve_requests_.clear();
    if (!editor_ || !editor_open_)
        return;
    const quint64 session = session_;
    const QStringList syms = editor_->explicit_tickers().mid(0, 8);
    for (const QString& sym : syms) {
        if (dismissed_renames_.contains(sym))
            continue;
        // Stored symbols are canonicalised locally (never guessed, never migrated):
        // a renamed listing is OFFERED as "X is now Y".
        resolve_requests_ << resolver.resolve_stored(
            sym, QString(), this, [this, session, sym](const fincept::services::ResolveResult& r) {
                if (session != session_ || !detail_->rename_from().isEmpty())
                    return;
                if (r.matches.size() < 2 ||
                    r.matches.first().status != fincept::core::instrument::InstrumentStatus::Renamed)
                    return;
                const QString to = notes::normalize_ticker(r.matches.at(1).symbol);
                if (!to.isEmpty() && to != sym)
                    detail_->show_rename_offer(sym, to);
            });
    }
}

// ── New cards ────────────────────────────────────────────────────────────────

double NotesScreen::top_rank_of_lane(const QString& lane_key) const {
    const int lane = board_model_->lane_of_key(lane_key);
    if (lane >= 0 && !board_model_->lane(lane).cards.isEmpty())
        return board_model_->lane(lane).cards.first().meta.rank - 1.0;
    return nb::default_rank(QDateTime::currentSecsSinceEpoch());
}

fincept::FinancialNote NotesScreen::seed_for_lane(const QString& lane_key, nb::NoteBoardMeta* meta) const {
    const notes::BoardDef& d = board();
    fincept::FinancialNote seed;
    seed.category = d.filter.categories.size() == 1 ? d.filter.categories.first() : QStringLiteral("GENERAL");
    seed.priority = d.filter.priorities.size() == 1 ? d.filter.priorities.first() : QStringLiteral("MEDIUM");
    seed.sentiment = QStringLiteral("NEUTRAL");
    if (!d.filter.tag.isEmpty())
        seed.tags = d.filter.tag;
    if (!d.filter.ticker.isEmpty())
        seed.tickers = d.filter.ticker;
    seed.is_favorite = d.filter.pinned_only;
    nb::NoteBoardMeta m;
    QString status;
    switch (d.group_by) {
    case GroupBy::Status:
        if (nb::status_codes().contains(lane_key))
            status = lane_key;
        break;
    case GroupBy::Category:
        if (!lane_key.isEmpty())
            seed.category = lane_key;
        break;
    case GroupBy::Priority:
        if (!lane_key.isEmpty())
            seed.priority = lane_key;
        break;
    case GroupBy::Ticker:
        if (!lane_key.isEmpty())
            seed.tickers = lane_key;
        break;
    case GroupBy::Tag:
        if (!lane_key.isEmpty()) {
            const int l = board_model_->lane_of_key(lane_key);
            QString tag = l >= 0 ? board_model_->lane(l).title : lane_key;
            if (tag.startsWith(QLatin1Char('#')))
                tag.remove(0, 1);
            seed.tags = notes::join_list(notes::merge_unique(QStringList{tag}, notes::split_tags(seed.tags)));
        }
        break;
    case GroupBy::Date:
        break;
    }
    if (d.filter.statuses.size() == 1 && status.isEmpty())
        status = d.filter.statuses.first();
    m.status = status.isEmpty() ? nb::default_status(seed.category, false) : status;
    if (d.filter.due_only)
        m.review_on = QDate::currentDate().toString(Qt::ISODate);
    m.rank = top_rank_of_lane(lane_key.isEmpty() && d.group_by == GroupBy::Status ? m.status : lane_key);
    if (meta)
        *meta = m;
    return seed;
}

void NotesScreen::new_card(const QString& lane_key) {
    if (!loaded_)
        return;
    nb::NoteBoardMeta m;
    const fincept::FinancialNote seed = seed_for_lane(lane_key, &m);
    if (focus_mode_)
        set_focus_mode(false);
    open_new_note(seed, false, m);
}

void NotesScreen::new_card_on_wall(const QPointF& pos) {
    new_card(QString());
    wall_pos_for_session_.insert(session_, pos);
}

void NotesScreen::new_titled(const QString& title, const QString& ticker) {
    nb::NoteBoardMeta m;
    fincept::FinancialNote seed = seed_for_lane(board_view_->current_lane_key(), &m);
    seed.title = title.left(500);
    if (!ticker.isEmpty())
        seed.tickers = notes::join_list(notes::merge_unique(QStringList{ticker}, notes::split_tickers(seed.tickers)));
    open_new_note(seed, !title.trimmed().isEmpty(), m);
}

void NotesScreen::new_from_template(const QString& template_id, const QString& lane_key, const QString& ticker_in) {
    for (const auto& t : notes::builtin_templates()) {
        if (t.id != template_id)
            continue;
        nb::NoteBoardMeta m;
        fincept::FinancialNote seed = seed_for_lane(lane_key, &m);
        QString ticker = notes::normalize_ticker(ticker_in);
        if (ticker.isEmpty())
            ticker = notes::split_tickers(seed.tickers).value(0);
        const QDate today = QDate::currentDate();
        seed.title = notes::fill_template(t.title, today, ticker);
        seed.content = notes::fill_template(t.body, today, ticker);
        // The grouped attribute of the lane wins; the template fills the rest.
        if (board().group_by != GroupBy::Category)
            seed.category = t.category;
        if (board().group_by != GroupBy::Priority)
            seed.priority = t.priority;
        seed.sentiment = t.sentiment;
        seed.tags = notes::join_list(notes::merge_unique(notes::split_tags(seed.tags), notes::split_tags(t.tags)));
        if (!ticker.isEmpty())
            seed.tickers = notes::join_list(notes::merge_unique(QStringList{ticker}, notes::split_tickers(seed.tickers)));
        const bool status_lane = board().group_by == GroupBy::Status && nb::status_codes().contains(lane_key);
        if (!status_lane)
            m.status = t.status;
        m.origin = QStringLiteral("template");
        m.rank = top_rank_of_lane(board().group_by == GroupBy::Status ? m.status : lane_key);
        if (focus_mode_)
            set_focus_mode(false);
        open_new_note(seed, true, m);
        return;
    }
}

void NotesScreen::new_from_user_template(int index) {
    if (index < 0 || index >= user_templates_.size())
        return;
    const fincept::NoteTemplate& t = user_templates_.at(index);
    nb::NoteBoardMeta m;
    fincept::FinancialNote seed = seed_for_lane(board_view_->current_lane_key(), &m);
    const QString ticker = notes::split_tickers(seed.tickers).value(0);
    seed.title = notes::fill_template(t.name, QDate::currentDate(), ticker);
    seed.content = notes::fill_template(t.content, QDate::currentDate(), ticker);
    if (board().group_by != GroupBy::Category)
        seed.category = t.category.isEmpty() ? QStringLiteral("GENERAL") : notes::category_key(t.category);
    open_new_note(seed, true, m);
}

// ── Note writes ──────────────────────────────────────────────────────────────

void NotesScreen::write_notes(const QVector<fincept::FinancialNote>& changed) {
    if (changed.isEmpty())
        return;
    ++write_seq_;
    QPointer<NotesScreen> self = this;
    store_->update_many(changed, [self, changed](bool ok) {
        if (!self)
            return;
        if (!ok) {
            self->show_notice(tr("Some changes could not be written to the local database."));
            self->request_reload();
            return;
        }
        for (const auto& n : changed)
            nsa_publish("notes.updated", n.id);
    });
}

void NotesScreen::show_notice(const QString& text, std::function<void()> undo, std::function<void()> on_expire,
                              int ms) {
    status_->show_notice(text, undo ? tr("UNDO") : QString(), std::move(undo), ms,
                         fincept::ui::ScreenStatusBar::Tone::Normal, std::move(on_expire));
}

void NotesScreen::set_pinned(const QVector<int>& ids, bool pinned) {
    if (editor_open_ && ids.contains(open_id()))
        save_editor_async(); // the open note's text goes with it
    QVector<fincept::FinancialNote> changed;
    const QString now = nsa_now_db();
    for (int id : ids) {
        const NoteEntry* e = find_entry(id);
        if (!e || e->note.is_favorite == pinned)
            continue;
        fincept::FinancialNote n = e->note;
        n.is_favorite = pinned;
        n.updated_at = now;
        changed << n;
    }
    for (const auto& n : changed)
        upsert_entry(n);
    if (editor_ && editor_open_ && ids.contains(open_id()))
        editor_->set_pinned(pinned);
    write_notes(changed);
}

void NotesScreen::set_archived(const QVector<int>& ids, bool archived) {
    if (editor_open_ && ids.contains(open_id()))
        save_editor_async();
    QVector<fincept::FinancialNote> changed;
    QVector<int> touched;
    const QString now = nsa_now_db();
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    for (int id : ids) {
        const NoteEntry* e = find_entry(id);
        if (!e || e->note.is_archived == archived)
            continue;
        fincept::FinancialNote n = e->note;
        n.is_archived = archived;
        n.updated_at = now;
        changed << n;
        touched << id;
        nb::NoteBoardEvent ev;
        ev.note_id = id;
        ev.at_ms = now_ms;
        ev.kind = QStringLiteral("archived");
        ev.from_value = archived ? QStringLiteral("0") : QStringLiteral("1");
        ev.to_value = archived ? QStringLiteral("1") : QStringLiteral("0");
        queue_meta(e->meta, ev);
    }
    if (changed.isEmpty())
        return;
    for (const auto& n : changed)
        upsert_entry(n);
    if (editor_ && editor_open_ && ids.contains(open_id())) {
        editor_->set_archived(archived);
        if (const NoteEntry* e = find_entry(open_id()))
            detail_->set_note_meta(*e, archived);
    }
    write_notes(changed);
    const QString text =
        archived ? tr("Archived %n note(s) — turn on ARCHIVED to see them.", nullptr, static_cast<int>(changed.size()))
                 : tr("Restored %n note(s) from the archive.", nullptr, static_cast<int>(changed.size()));
    show_notice(text, [this, touched, archived]() { set_archived(touched, !archived); }, {}, kUndoMs);
}

void NotesScreen::set_status(const QVector<int>& ids, const QString& status) {
    const QString st = nb::normalize_status(status);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QVector<NoteEntry> before;
    for (int id : ids) {
        const NoteEntry* e = find_entry(id);
        if (!e || e->meta.status == st)
            continue;
        before << *e;
        NoteEntry ne = *e;
        nb::NoteBoardMeta m = ne.meta;
        nb::NoteBoardEvent ev;
        ev.note_id = id;
        ev.at_ms = now;
        ev.kind = QStringLiteral("status");
        ev.from_value = m.status;
        ev.to_value = st;
        m.status = st;
        m.status_changed_ms = now;
        // Placed at the top of its new lane.
        m.rank = board().group_by == GroupBy::Status ? top_rank_of_lane(st) : m.rank;
        notes::apply_meta(ne, m);
        replace_entry(ne);
        queue_meta(m, ev);
    }
    if (before.isEmpty())
        return;
    if (editor_open_ && ids.contains(open_id()))
        if (const NoteEntry* e = find_entry(open_id()))
            detail_->set_note_meta(*e, e->note.is_archived);
    if (before.size() > 1 || !editor_open_ || !ids.contains(open_id()))
        show_notice(tr("%n card(s) moved to %1", nullptr, static_cast<int>(before.size())).arg(notes::status_label(st)),
                    [this, before]() {
                        for (const auto& e : before) {
                            queue_meta(e.meta);
                            replace_entry(e);
                        }
                    },
                    {}, kUndoMs);
}

void NotesScreen::set_review(const QVector<int>& ids, const QDate& date) {
    const QString iso = date.isValid() ? date.toString(Qt::ISODate) : QString();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int id : ids) {
        const NoteEntry* e = find_entry(id);
        if (!e || e->meta.review_on == iso)
            continue;
        NoteEntry ne = *e;
        nb::NoteBoardMeta m = ne.meta;
        nb::NoteBoardEvent ev;
        ev.note_id = id;
        ev.at_ms = now;
        ev.kind = QStringLiteral("review");
        ev.from_value = m.review_on;
        ev.to_value = iso;
        m.review_on = iso;
        notes::apply_meta(ne, m);
        replace_entry(ne);
        queue_meta(m, ev);
    }
    if (editor_open_ && ids.contains(open_id()))
        if (const NoteEntry* e = find_entry(open_id()))
            detail_->set_note_meta(*e, e->note.is_archived);
}

void NotesScreen::mark_reviewed(int id) {
    const NoteEntry* e = find_entry(id);
    if (!e)
        return;
    NoteEntry ne = *e;
    nb::NoteBoardMeta m = ne.meta;
    m.reviewed_ms = QDateTime::currentMSecsSinceEpoch();
    if (ne.review.isValid() && ne.review <= QDate::currentDate())
        m.review_on.clear(); // the due review is done
    nb::NoteBoardEvent ev;
    ev.note_id = id;
    ev.at_ms = m.reviewed_ms;
    ev.kind = QStringLiteral("reviewed");
    notes::apply_meta(ne, m);
    replace_entry(ne);
    queue_meta(m, ev);
    if (open_id() == id) {
        detail_->set_note_meta(ne, ne.note.is_archived);
        load_history(id);
    }
    show_notice(tr("Marked reviewed."));
}

void NotesScreen::add_tag(const QVector<int>& ids) {
    if (ids.isEmpty())
        return;
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, tr("Add tag"), tr("Tag for %n note(s) (comma-separate several):", nullptr, static_cast<int>(ids.size())),
        QLineEdit::Normal, QString(), &ok);
    const QStringList add = notes::split_tags(text);
    if (!ok || add.isEmpty())
        return;
    if (editor_open_ && ids.contains(open_id()))
        save_editor_async();
    QVector<fincept::FinancialNote> changed;
    const QString now = nsa_now_db();
    for (int id : ids) {
        const NoteEntry* e = find_entry(id);
        if (!e)
            continue;
        QStringList tags = e->tag_list;
        bool any = false;
        for (const QString& t : add) {
            bool dup = false;
            for (const QString& mine : tags)
                dup = dup || mine.compare(t, Qt::CaseInsensitive) == 0;
            if (!dup) {
                tags << t;
                any = true;
            }
        }
        if (!any)
            continue;
        fincept::FinancialNote n = e->note;
        n.tags = notes::join_list(tags);
        n.updated_at = now;
        changed << n;
    }
    for (const auto& n : changed)
        upsert_entry(n);
    // The open note's tags come from the editor at its next save: give it the new list.
    if (editor_ && editor_open_ && ids.contains(open_id()))
        if (const NoteEntry* e = find_entry(open_id()))
            editor_->set_tags(e->tag_list);
    write_notes(changed);
}

void NotesScreen::add_symbol_to_note(int id, const QString& symbol) {
    const QString sym = notes::normalize_ticker(symbol);
    const NoteEntry* e = find_entry(id);
    if (sym.isEmpty() || !e)
        return;
    if (e->ticker_list.contains(sym)) {
        show_notice(tr("$%1 is already linked to “%2”.").arg(sym, e->note.title.left(40)));
        return;
    }
    if (editor_open_ && open_id() == id) {
        editor_->add_ticker(sym); // an edit: autosave writes it
        return;
    }
    fincept::FinancialNote n = e->note;
    n.tickers = notes::join_list(notes::merge_unique(notes::split_tickers(n.tickers), {sym}));
    n.updated_at = nsa_now_db();
    upsert_entry(n);
    write_notes({n});
    show_notice(tr("Linked $%1 to “%2”.").arg(sym, n.title.left(40)));
}

void NotesScreen::duplicate(int id) {
    if (editor_open_ && id == open_id())
        save_editor_async();
    const NoteEntry* e = find_entry(id);
    if (!e)
        return;
    fincept::FinancialNote n = e->note;
    n.id = 0;
    n.title = tr("%1 (copy)").arg(n.title);
    n.is_favorite = false;
    n.is_archived = false;
    n.created_at = n.updated_at = nsa_now_db();
    nb::NoteBoardMeta m = e->meta; // same lane, right by the original
    m.note_id = 0;
    m.origin.clear();
    m.origin_ref.clear();
    QPointer<NotesScreen> self = this;
    store_->create_many(
        {n},
        [self, n, m](QVector<int> ids) {
            if (!self)
                return;
            if (ids.isEmpty() || ids.first() <= 0) {
                self->show_notice(tr("The note could not be duplicated."));
                return;
            }
            fincept::FinancialNote copy = n;
            copy.id = ids.first();
            nb::NoteBoardMeta cm = m;
            cm.note_id = copy.id;
            cm.stored = true;
            self->pending_new_meta_.insert(copy.id, cm);
            self->upsert_entry(copy);
            nsa_publish("notes.created", copy.id);
            self->select_in_views({copy.id}, copy.id, true);
            self->open_note(copy.id);
        },
        {m});
}

void NotesScreen::delete_notes(const QVector<int>& ids_in) {
    QVector<int> ids;
    for (int id : ids_in)
        if (id > 0 && find_entry(id))
            ids << id;
    if (ids.isEmpty())
        return;
    if (ids.size() >= 10 &&
        QMessageBox::question(this, tr("Delete notes"),
                              tr("Delete %n notes? You can undo this for a few seconds.", nullptr,
                                 static_cast<int>(ids.size())),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    // An earlier delete becomes final (its notice expires now); UNDO is for this one.
    status_->finish_notice();
    commit_deletes();
    const bool open_deleted = editor_open_ && ids.contains(open_id());
    if (open_deleted)
        save_editor_async(); // undo brings back what was typed

    // The card after the deleted ones (same lane) gets the selection.
    int next_id = 0;
    const auto pos = board_model_->position_of(ids.last());
    if (pos.first >= 0) {
        const auto& cards = board_model_->lane(pos.first).cards;
        for (int r = pos.second + 1; r < cards.size() && next_id == 0; ++r)
            if (!ids.contains(cards.at(r).note.id))
                next_id = cards.at(r).note.id;
        for (int r = pos.second - 1; r >= 0 && next_id == 0; --r)
            if (!ids.contains(cards.at(r).note.id))
                next_id = cards.at(r).note.id;
    }
    QSet<int> set;
    for (int id : ids) {
        if (const NoteEntry* e = find_entry(id))
            pending_delete_ << *e;
        pending_delete_ids_.insert(id);
        set.insert(id);
    }
    all_.erase(std::remove_if(all_.begin(), all_.end(), [&set](const NoteEntry& e) { return set.contains(e.note.id); }),
               all_.end());
    reindex();
    syncing_selection_ = true;
    board_model_->remove_ids(set);
    table_model_->remove_ids(set);
    syncing_selection_ = false;
    schedule_build();
    if (open_deleted || detail_->page() == notes::NotesDetailPane::Page::Bulk) {
        close_editor();
        if (next_id > 0) {
            select_in_views({next_id}, next_id, true);
            open_note(next_id);
        } else {
            show_empty();
        }
    }
    const QString what = ids.size() == 1 && !pending_delete_.isEmpty() && !pending_delete_.last().note.title.isEmpty()
                             ? tr("Deleted “%1”").arg(pending_delete_.last().note.title.left(60))
                             : tr("Deleted %n note(s)", nullptr, static_cast<int>(ids.size()));
    show_notice(what, [this]() { undo_delete(); }, [this]() { commit_deletes(); }, kUndoMs);
}

void NotesScreen::undo_delete() {
    if (pending_delete_.isEmpty())
        return;
    const QVector<NoteEntry> back = pending_delete_;
    pending_delete_.clear();
    pending_delete_ids_.clear();
    for (const auto& e : back)
        all_.append(e);
    reindex();
    schedule_build();
    if (back.size() == 1) {
        select_in_views({back.first().note.id}, back.first().note.id, true);
        open_note(back.first().note.id);
    }
}

void NotesScreen::commit_deletes() {
    if (pending_delete_ids_.isEmpty())
        return;
    const QVector<int> ids(pending_delete_ids_.begin(), pending_delete_ids_.end());
    pending_delete_.clear();
    pending_delete_ids_.clear();
    ++write_seq_;
    for (int id : ids)
        meta_dirty_.remove(id); // the delete trigger removes the board rows
    QPointer<NotesScreen> self = this;
    store_->remove(ids, [self, ids](bool ok) {
        for (int id : ids)
            nsa_publish("notes.deleted", id);
        if (self && !ok) {
            self->show_notice(tr("Some notes could not be deleted from the local database."));
            self->request_reload();
        }
    });
}

// ── Export / import / templates ──────────────────────────────────────────────

void NotesScreen::export_notes(const QVector<int>& ids, bool to_file_manager) {
    if (editor_open_ && ids.contains(open_id()))
        save_editor_async();
    QVector<QPair<QString, QString>> docs; // (file stem, markdown)
    for (int id : ids)
        if (const NoteEntry* e = find_entry(id)) {
            const QString title = e->note.title.isEmpty() ? tr("Untitled note") : e->note.title;
            docs.append({notes::safe_file_stem(title), notes::to_markdown(nsa_to_doc(e->note))});
        }
    if (docs.isEmpty())
        return;
    QString dir;
    if (to_file_manager) {
        dir = services::FileManagerService::instance().storage_dir();
    } else {
        dir = QFileDialog::getExistingDirectory(
            this, tr("Export %n note(s) to folder", nullptr, static_cast<int>(docs.size())), QDir::homePath());
        if (dir.isEmpty())
            return;
    }
    QPointer<NotesScreen> self = this;
    store_->run<NsaExportResult>(
        [docs, dir]() {
            NsaExportResult r;
            QDir().mkpath(dir);
            for (const auto& d : docs) {
                const QString path = nsa_unique_md_path(dir, d.first);
                QFile f(path);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    LOG_WARN("Notes", fincept::logf("Note export failed",
                                                    {{"path", fincept::log_path(path)}, {"error", f.errorString()}}));
                    ++r.failed;
                    continue;
                }
                const QByteArray bytes = d.second.toUtf8();
                if (f.write(bytes) != bytes.size()) {
                    LOG_WARN("Notes", fincept::logf("Note export failed",
                                                    {{"path", fincept::log_path(path)}, {"error", f.errorString()}}));
                    ++r.failed;
                    continue;
                }
                f.close();
                r.files.append({QFileInfo(path).fileName(), d.first + QStringLiteral(".md"), bytes.size()});
            }
            return r;
        },
        [self, to_file_manager, dir](NsaExportResult r) {
            if (to_file_manager)
                for (const auto& f : r.files)
                    services::FileManagerService::instance().register_file(f.stored_name, f.display_name, f.size,
                                                                           QStringLiteral("text/markdown"),
                                                                           QStringLiteral("notes"));
            if (!self)
                return;
            const int n = static_cast<int>(r.files.size());
            QString text = to_file_manager
                               ? tr("Exported %n note(s) to the File Manager.", nullptr, n)
                               : tr("Exported %n note(s) to %1", nullptr, n).arg(QDir::toNativeSeparators(dir));
            if (r.failed > 0)
                text += QLatin1Char(' ') + tr("%n could not be written.", nullptr, r.failed);
            self->show_notice(text);
        });
}

void NotesScreen::import_files() {
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Import notes"), QDir::homePath(),
                                                            tr("Markdown and text (*.md *.markdown *.txt);;All files (*)"));
    if (paths.isEmpty())
        return;
    QPointer<NotesScreen> self = this;
    store_->run<QVector<fincept::FinancialNote>>(
        [paths]() {
            QVector<fincept::FinancialNote> out;
            for (const QString& p : paths) {
                QFile f(p);
                if (f.size() > 5 * 1024 * 1024 || !f.open(QIODevice::ReadOnly))
                    continue;
                const QString text = QString::fromUtf8(f.readAll());
                out.append(nsa_from_doc(notes::from_markdown(text, QFileInfo(p).completeBaseName())));
            }
            return out;
        },
        [self, total = static_cast<int>(paths.size())](QVector<fincept::FinancialNote> parsed) {
            if (!self)
                return;
            if (parsed.isEmpty()) {
                self->show_notice(tr("Nothing was imported — the files could not be read."));
                return;
            }
            self->store_->create_many(parsed, [self, total](QVector<int> ids) {
                if (!self)
                    return;
                int ok = 0;
                for (int id : ids)
                    if (id > 0) {
                        ++ok;
                        nsa_publish("notes.created", id);
                    }
                self->reload();
                QString text = tr("Imported %n note(s).", nullptr, ok);
                if (ok < total)
                    text += QLatin1Char(' ') + tr("%n file(s) skipped.", nullptr, total - ok);
                self->show_notice(text);
            });
        });
}

void NotesScreen::save_as_template() {
    if (!editor_ || !editor_open_)
        return;
    const fincept::FinancialNote n = editor_->collect(session_base_);
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Save as template"), tr("Template name:"), QLineEdit::Normal, n.title, &ok)
            .trimmed();
    if (!ok || name.isEmpty())
        return;
    fincept::NoteTemplate t;
    t.name = name.left(120);
    t.content = n.content;
    t.category = n.category;
    QPointer<NotesScreen> self = this;
    store_->save_template(t, [self, name](bool saved) {
        if (!self)
            return;
        self->show_notice(saved ? tr("Template “%1” saved — find it under TEMPLATE.").arg(name)
                                : tr("The template could not be saved."));
        if (saved)
            self->reload();
    });
}

void NotesScreen::revert_session() {
    if (!editor_ || !editor_open_)
        return;
    if (QMessageBox::question(this, tr("Revert note"),
                              tr("Put this note back the way it was when you opened it?\n"
                                 "Everything changed since then is discarded."),
                              QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard)
        return;
    autosave_timer_->stop();
    if (!session_has_original_) {
        // A note created in this session: reverting removes it (with undo).
        const int id = open_id();
        if (id > 0 && find_entry(id)) {
            delete_notes({id});
        } else {
            if (saves_in_flight_ > 0)
                discarded_sessions_.insert(session_);
            show_empty();
        }
        return;
    }
    fincept::FinancialNote original = session_original_;
    original.updated_at = nsa_now_db();
    editor_->load(original);
    queued_revision_ = editor_->revision();
    ++saves_in_flight_;
    upsert_entry(original);
    const quint64 session = session_;
    const notes::IdSlot slot = slot_;
    const quint64 rev = queued_revision_;
    QPointer<NotesScreen> self = this;
    store_->save(original, slot, [self, session, rev, original, slot](int id, bool ok) {
        if (self)
            self->on_save_done(session, rev, id, ok, original, slot, 1);
    });
}

void NotesScreen::on_editor_action(const QString& action) {
    const int id = open_id();
    if (action == QLatin1String("duplicate")) {
        if (id > 0)
            duplicate(id);
    } else if (action == QLatin1String("export_fm") || action == QLatin1String("export_dir")) {
        if (id <= 0) {
            save_editor_async();
            show_notice(tr("Write something first — an empty note has nothing to export."));
            return;
        }
        export_notes({id}, action == QLatin1String("export_fm"));
    } else if (action == QLatin1String("save_template")) {
        save_as_template();
    } else if (action == QLatin1String("revert")) {
        revert_session();
    } else if (action == QLatin1String("archive")) {
        if (const NoteEntry* e = find_entry(id))
            set_archived({id}, !e->note.is_archived);
    } else if (action == QLatin1String("delete")) {
        if (id > 0 && find_entry(id)) {
            delete_notes({id});
        } else {
            // Never stored (or its first save still running): just let it go.
            if (saves_in_flight_ > 0)
                discarded_sessions_.insert(session_);
            show_empty();
        }
    }
}

void NotesScreen::on_bulk_action(const QString& action) {
    const QVector<int> ids = selected_ids();
    if (ids.isEmpty())
        return;
    if (action == QLatin1String("pin") || action == QLatin1String("unpin")) {
        set_pinned(ids, action == QLatin1String("pin"));
    } else if (action.startsWith(QLatin1String("status:"))) {
        set_status(ids, action.mid(7));
    } else if (action == QLatin1String("tag")) {
        add_tag(ids);
    } else if (action.startsWith(QLatin1String("review:"))) {
        const QString v = action.mid(7);
        set_review(ids, v == QLatin1String("clear") ? QDate() : QDate::currentDate().addDays(v.toInt()));
    } else if (action == QLatin1String("archive") || action == QLatin1String("unarchive")) {
        set_archived(ids, action == QLatin1String("archive"));
    } else if (action == QLatin1String("export_fm") || action == QLatin1String("export_dir")) {
        export_notes(ids, action == QLatin1String("export_fm"));
    } else if (action == QLatin1String("delete")) {
        delete_notes(ids);
    }
    if (detail_->page() == notes::NotesDetailPane::Page::Bulk)
        show_bulk(selected_ids());
}

// ── Menus ────────────────────────────────────────────────────────────────────

void NotesScreen::open_symbol_in(const QString& screen_id, const QString& symbol) {
    const QString sym = notes::normalize_ticker(symbol);
    if (!sym.isEmpty())
        EventBus::instance().publish("nav.open_symbol", {{"screen_id", screen_id}, {"symbol", sym}});
}

void NotesScreen::show_symbol_menu(const QString& symbol) {
    const QString sym = notes::normalize_ticker(symbol);
    if (sym.isEmpty())
        return;
    QMenu menu(this);
    menu.setStyleSheet(fincept::ui::ScreenCommandBar::menu_css());
    menu.addSection(QStringLiteral("$") + sym);
    struct Target {
        QString screen_id;
        QString label;
    };
    const QVector<Target> targets = {
        {QStringLiteral("equity_research"), tr("Open in Equity Research")},
        {QStringLiteral("equity_trading"), tr("Open chart / order ticket (Equity Trading)")},
        {QStringLiteral("news"), tr("News for %1").arg(sym)},
        {QStringLiteral("watchlist"), tr("Open in Watchlist")},
        {QStringLiteral("backtesting"), tr("Backtest %1").arg(sym)},
    };
    for (const auto& t : targets) {
        const QString screen_id = t.screen_id;
        menu.addAction(t.label, this, [this, sym, screen_id]() { open_symbol_in(screen_id, sym); });
    }
    // Link groups: every panel following the group switches to this ticker.
    auto& registry = fincept::SymbolGroupRegistry::instance();
    const QList<fincept::SymbolGroup> groups = registry.enabled_groups();
    if (!groups.isEmpty()) {
        menu.addSeparator();
        for (fincept::SymbolGroup g : groups)
            menu.addAction(tr("Send to link group %1 (%2)").arg(fincept::symbol_group_letter(g)).arg(registry.name(g)),
                           this, [this, g, sym]() {
                               fincept::SymbolContext::instance().set_group_symbol(g, fincept::SymbolRef::equity(sym),
                                                                                   this);
                           });
    }
    menu.addSeparator();
    menu.addAction(tr("Show notes linked to $%1").arg(sym), this, [this, sym]() {
        notes::BoardFilter f = board().filter;
        f.ticker = f.ticker == sym ? QString() : sym;
        set_filter(f);
    });
    menu.addAction(tr("Copy ticker"), this, [sym]() { QApplication::clipboard()->setText(sym); });
    menu.exec(QCursor::pos());
}

void NotesScreen::on_link(const QUrl& url) {
    if (url.scheme() == QLatin1String("fincept-symbol")) {
        show_symbol_menu(url.path());
        return;
    }
    // Ordinary links open in the system browser, only on an explicit click.
    if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))
        QDesktopServices::openUrl(url);
}

void NotesScreen::show_card_menu(const QPoint& global_pos) {
    const QVector<int> ids = selected_ids();
    if (ids.isEmpty())
        return;
    bool any_unpinned = false;
    bool all_archived = true;
    for (int id : ids)
        if (const NoteEntry* e = find_entry(id)) {
            any_unpinned = any_unpinned || !e->note.is_favorite;
            all_archived = all_archived && e->note.is_archived;
        }
    QMenu menu(this);
    menu.setStyleSheet(fincept::ui::ScreenCommandBar::menu_css());
    if (ids.size() == 1) {
        const int id = ids.first();
        menu.addAction(tr("Open\tEnter"), this, [this, id]() {
            open_note(id);
            set_focus_mode(true);
            if (editor_)
                editor_->focus_body();
        });
        menu.addAction(tr("Quick peek\tSpace"), this, [this, id]() { peek(id); });
        menu.addAction(tr("Duplicate\tCtrl+D"), this, [this, id]() { duplicate(id); });
    }
    menu.addAction(any_unpinned ? tr("Pin\tP") : tr("Unpin\tP"), this,
                   [this, ids, any_unpinned]() { set_pinned(ids, any_unpinned); });
    QMenu* status = menu.addMenu(tr("Status"));
    for (const QString& code : nb::status_codes())
        status->addAction(notes::status_label(code), this, [this, ids, code]() { set_status(ids, code); });
    QMenu* prio = menu.addMenu(tr("Priority"));
    for (const QString& code : notes::priority_codes())
        prio->addAction(notes::priority_label(code), this, [this, ids, code]() {
            QVector<fincept::FinancialNote> changed;
            const QString now = nsa_now_db();
            for (int id : ids)
                if (const NoteEntry* e = find_entry(id); e && e->note.priority.trimmed().toUpper() != code) {
                    fincept::FinancialNote n = e->note;
                    n.priority = code;
                    n.updated_at = now;
                    changed << n;
                }
            if (editor_open_ && ids.contains(open_id()))
                save_editor_async();
            for (const auto& n : changed)
                upsert_entry(n);
            write_notes(changed);
            if (editor_open_ && ids.contains(open_id()))
                open_note(open_id(), true);
        });
    QMenu* review = menu.addMenu(tr("Review on"));
    const QDate today = QDate::currentDate();
    review->addAction(tr("Tomorrow"), this, [this, ids, today]() { set_review(ids, today.addDays(1)); });
    review->addAction(tr("Next week"), this, [this, ids, today]() { set_review(ids, today.addDays(7)); });
    review->addAction(tr("Next month"), this, [this, ids, today]() { set_review(ids, today.addMonths(1)); });
    review->addSeparator();
    review->addAction(tr("No review date"), this, [this, ids]() { set_review(ids, QDate()); });
    menu.addAction(tr("Add tag…"), this, [this, ids]() { add_tag(ids); });
    if (ids.size() == 1 && ids.first() == open_id())
        fincept::ui::add_send_to_report_action(&menu, QStringLiteral("Notes"), [this]() { return report_payload(); });
    menu.addSeparator();
    menu.addAction(tr("Export to File Manager"), this, [this, ids]() { export_notes(ids, true); });
    menu.addAction(tr("Export to folder…"), this, [this, ids]() { export_notes(ids, false); });
    menu.addSeparator();
    menu.addAction(all_archived ? tr("Unarchive") : tr("Archive"), this,
                   [this, ids, all_archived]() { set_archived(ids, !all_archived); });
    menu.addAction(tr("Delete\tDel"), this, [this, ids]() { delete_notes(ids); });
    menu.actions().constLast()->setProperty("fincept_agent_risk", "delete"); // agents: deletes the selected notes
    menu.exec(global_pos);
}

void NotesScreen::show_lane_menu(const QString& lane_key, const QPoint& global_pos) {
    const int lane = board_model_->lane_of_key(lane_key);
    if (lane < 0)
        return;
    const bool collapsed = board_model_->lane(lane).collapsed;
    QMenu menu(this);
    menu.setStyleSheet(fincept::ui::ScreenCommandBar::menu_css());
    QString title = board_model_->lane(lane).title;
    title.replace(QLatin1Char('&'), QStringLiteral("&&"));
    menu.addSection(title);
    menu.addAction(tr("New card here\tN"), this, [this, lane_key]() { new_card(lane_key); });
    QMenu* tpl = menu.addMenu(tr("New from template"));
    for (const auto& t : notes::builtin_templates()) {
        const QString id = t.id;
        tpl->addAction(nsa_template_name(id), this, [this, id, lane_key]() { new_from_template(id, lane_key, QString()); });
    }
    menu.addSeparator();
    menu.addAction(collapsed ? tr("Expand lane") : tr("Collapse lane"), this,
                   [this, lane_key]() { toggle_lane(lane_key); });
    menu.addAction(tr("Set soft limit…"), this, [this, lane_key]() { set_lane_limit(lane_key); });
    QAction* left = menu.addAction(tr("Move lane left"), this, [this, lane_key, lane]() { move_lane(lane_key, lane - 1); });
    left->setEnabled(lane > 0);
    QAction* right =
        menu.addAction(tr("Move lane right"), this, [this, lane_key, lane]() { move_lane(lane_key, lane + 1); });
    right->setEnabled(lane < board_model_->lane_count() - 1);
    menu.addSeparator();
    menu.addAction(tr("Expand every lane"), this, [this]() {
        board().collapsed.remove(notes::group_by_key(board().group_by));
        board_changed();
    });
    menu.exec(global_pos);
}

// ── Moves ────────────────────────────────────────────────────────────────────

QString NotesScreen::check_drop(const QVector<int>& ids, const QString&, const QString& to_lane) const {
    const GroupBy g = board().group_by;
    const QDateTime now = QDateTime::currentDateTime();
    for (int id : ids) {
        const NoteEntry* e = find_entry(id);
        if (!e)
            continue;
        const QString from = notes::lane_key_of(*e, g, now);
        const notes::MoveCheck c = notes::check_move(*e, g, from, to_lane);
        if (c.allowed)
            continue;
        switch (c.reason) {
        case notes::MoveDenied::DateLane:
            return tr("Date lanes follow the last edit — cards can only be reordered inside a lane.");
        case notes::MoveDenied::TickerInText:
            return tr("“%1” mentions $%2 in its text, so it stays in a ticker lane. Remove $%2 from the text first.")
                .arg(e->note.title.left(40), c.detail);
        case notes::MoveDenied::NoPriority:
            return tr("Pick a priority lane — every note has a priority.");
        case notes::MoveDenied::SameLane:
        case notes::MoveDenied::None:
            break;
        }
    }
    return {};
}

void NotesScreen::move_cards(const QVector<int>& ids, const QString& from_lane, const QString& to_lane, int to_row) {
    Q_UNUSED(from_lane);
    const QString reason = check_drop(ids, from_lane, to_lane);
    if (!reason.isEmpty()) {
        show_notice(reason);
        return;
    }
    const int lane = board_model_->lane_of_key(to_lane);
    if (lane < 0)
        return;
    if (editor_open_ && ids.contains(open_id()))
        save_editor_async(); // the open note's text goes with it
    const GroupBy g = board().group_by;
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 now_ms = now.toMSecsSinceEpoch();
    const QString to_title = [&]() {
        QString t = board_model_->lane(lane).title;
        if (g == GroupBy::Tag && t.startsWith(QLatin1Char('#')))
            t.remove(0, 1);
        return t;
    }();

    // Order: ranks in the target lane, as it is now.
    QVector<QPair<int, double>> pairs;
    for (const auto& c : board_model_->lane(lane).cards)
        pairs.append({c.note.id, c.meta.rank});
    const notes::RankPlan plan = notes::plan_insert(pairs, ids, to_row);
    QHash<int, double> new_rank;
    for (const auto& r : plan.ranks)
        new_rank.insert(r.first, r.second);

    QVector<NoteEntry> before;
    QVector<NoteEntry> after;
    QVector<fincept::FinancialNote> note_writes;
    bool attribute_move = false;
    for (int id : ids) {
        const NoteEntry* e = find_entry(id);
        if (!e)
            continue;
        before << *e;
        const QString from = notes::lane_key_of(*e, g, now);
        auto r = notes::apply_lane_move(*e, g, from, to_lane, to_title, now_ms);
        NoteEntry ne = *e;
        if (r.note_changed) {
            r.note.updated_at = notes::db_time(now.toUTC());
            ne = notes::make_entry(r.note);
            note_writes << r.note;
            attribute_move = true;
        }
        nb::NoteBoardMeta m = r.meta;
        if (new_rank.contains(id))
            m.rank = new_rank.value(id);
        notes::apply_meta(ne, m);
        std::optional<nb::NoteBoardEvent> ev;
        if (r.meta_changed && !r.old_status.isEmpty()) {
            nb::NoteBoardEvent x;
            x.note_id = id;
            x.at_ms = now_ms;
            x.kind = QStringLiteral("status");
            x.from_value = r.old_status;
            x.to_value = m.status;
            ev = x;
            attribute_move = true;
        }
        if (r.note_changed && r.note.is_archived != e->note.is_archived) {
            nb::NoteBoardEvent x;
            x.note_id = id;
            x.at_ms = now_ms;
            x.kind = QStringLiteral("archived");
            x.from_value = e->note.is_archived ? QStringLiteral("1") : QStringLiteral("0");
            x.to_value = r.note.is_archived ? QStringLiteral("1") : QStringLiteral("0");
            ev = x;
        }
        if (!m.same_values(e->meta) || ev)
            queue_meta(m, ev);
        after << ne;
    }
    // Cards of the lane renumbered because the gaps ran out.
    if (plan.renumbered)
        for (const auto& r : plan.ranks)
            if (!ids.contains(r.first))
                if (NoteEntry* x = find_entry_mut(r.first)) {
                    nb::NoteBoardMeta m = x->meta;
                    m.rank = r.second;
                    notes::apply_meta(*x, m);
                    queue_meta(m);
                }
    // The board moves at once; the next build confirms the same order.
    syncing_selection_ = true;
    board_model_->move_cards(ids, lane, to_row);
    for (const auto& ne : after) {
        if (NoteEntry* x = find_entry_mut(ne.note.id))
            *x = ne;
        board_model_->update_entry(ne);
    }
    board_view_->select_ids(ids, ids.contains(board_view_->current_id()) ? board_view_->current_id() : ids.last(), true);
    syncing_selection_ = false;
    write_notes(note_writes);
    schedule_build();
    if (editor_open_ && ids.contains(open_id()))
        if (const NoteEntry* e = find_entry(open_id())) {
            detail_->set_note_meta(*e, e->note.is_archived);
            if (!note_writes.isEmpty())
                open_note(open_id(), true); // the editor shows the new attribute
        }
    if (attribute_move) {
        show_notice(tr("Moved %n card(s) to %1", nullptr, static_cast<int>(after.size())).arg(board_model_->lane(lane).title),
                    [this, before]() {
                        QVector<fincept::FinancialNote> notes_back;
                        for (const auto& e : before) {
                            const NoteEntry* cur = find_entry(e.note.id);
                            if (!cur)
                                continue;
                            if (cur->note.category != e.note.category || cur->note.priority != e.note.priority ||
                                cur->note.tags != e.note.tags || cur->note.tickers != e.note.tickers ||
                                cur->note.is_archived != e.note.is_archived) {
                                fincept::FinancialNote n = cur->note;
                                n.category = e.note.category;
                                n.priority = e.note.priority;
                                n.tags = e.note.tags;
                                n.tickers = e.note.tickers;
                                n.is_archived = e.note.is_archived;
                                n.updated_at = nsa_now_db();
                                notes_back << n;
                                upsert_entry(n);
                            }
                            if (NoteEntry* x = find_entry_mut(e.note.id)) {
                                notes::apply_meta(*x, e.meta);
                                queue_meta(e.meta);
                            }
                        }
                        write_notes(notes_back);
                        schedule_build();
                    },
                    {}, kUndoMs);
    }
}

void NotesScreen::move_by_key(int dlane, int drow) {
    if (board().view != BoardViewMode::Board)
        return;
    QVector<int> ids = board_view_->selected_ids();
    const int cur = board_view_->current_id();
    if (ids.isEmpty() && cur > 0)
        ids = {cur};
    if (ids.isEmpty())
        return;
    const auto pos = board_model_->position_of(cur > 0 ? cur : ids.first());
    if (pos.first < 0)
        return;
    const QString from_key = board_model_->lane(pos.first).key;
    if (drow != 0) {
        // Up / down inside the lane (the block of selected cards in this lane moves).
        int first = INT_MAX, last = -1;
        QVector<int> in_lane;
        for (int id : ids) {
            const auto p = board_model_->position_of(id);
            if (p.first != pos.first)
                continue;
            in_lane << id;
            first = std::min(first, p.second);
            last = std::max(last, p.second);
        }
        if (in_lane.isEmpty())
            return;
        std::sort(in_lane.begin(), in_lane.end(), [this](int a, int b) {
            return board_model_->position_of(a).second < board_model_->position_of(b).second;
        });
        const int n = static_cast<int>(board_model_->lane(pos.first).cards.size());
        if (drow < 0 && first == 0)
            return;
        if (drow > 0 && last >= n - 1)
            return;
        const int target = drow < 0 ? first - 1 : last + 2;
        move_cards(in_lane, from_key, from_key, target);
        return;
    }
    // Left / right: the next expanded lane.
    int lane = pos.first;
    do {
        lane += dlane;
    } while (lane >= 0 && lane < board_model_->lane_count() && board_model_->lane(lane).collapsed);
    if (lane < 0 || lane >= board_model_->lane_count())
        return;
    const QString to_key = board_model_->lane(lane).key;
    const QString reason = check_drop(ids, from_key, to_key);
    if (!reason.isEmpty()) {
        show_notice(reason);
        return;
    }
    const int row = std::min(pos.second, static_cast<int>(board_model_->lane(lane).cards.size()));
    move_cards(ids, from_key, to_key, row);
}

// ── Command field ────────────────────────────────────────────────────────────

void NotesScreen::on_command_edited(const QString& text) {
    const notes::Command c = notes::parse_command(text);
    if (!c.is_command()) {
        bar_->set_command_hint(QString());
        search_timer_->start();
        return;
    }
    search_timer_->stop();
    if (!query_.is_empty())
        apply_search(QString()); // the board is not filtered by a command being typed
    QString hint;
    switch (c.kind) {
    case notes::CommandKind::NewNote:
        hint = c.text.isEmpty() ? tr("ENTER  new card%1").arg(c.ticker.isEmpty() ? QString() : QStringLiteral(" · $") + c.ticker)
                                : tr("ENTER  new card “%1”").arg(c.text.left(40));
        break;
    case notes::CommandKind::NewFromTemplate:
        hint = tr("ENTER  new %1%2")
                   .arg(nsa_template_name(c.template_id),
                        c.ticker.isEmpty() ? QString() : QStringLiteral(" · $") + c.ticker);
        break;
    case notes::CommandKind::GroupBy:
        hint = tr("ENTER  group lanes by %1").arg(notes::group_by_key(c.group));
        break;
    case notes::CommandKind::View:
        hint = tr("ENTER  switch to the %1 view").arg(notes::view_mode_key(c.view));
        break;
    case notes::CommandKind::Open:
        hint = tr("ENTER  open the note titled “%1”").arg(c.text.left(40));
        break;
    case notes::CommandKind::Board:
        hint = tr("ENTER  switch to the board “%1”").arg(c.text.left(40));
        break;
    case notes::CommandKind::Filter:
        hint = tr("ENTER  toggle the %1 filter").arg(c.text);
        break;
    case notes::CommandKind::Clear:
        hint = tr("ENTER  clear the search and filters");
        break;
    case notes::CommandKind::Help:
        hint = tr("ENTER  list the commands");
        break;
    case notes::CommandKind::Search:
        break;
    }
    bar_->set_command_hint(hint);
}

void NotesScreen::apply_search(const QString& text) {
    search_text_ = text;
    const notes::SearchQuery q = notes::parse_command(text).is_command() ? notes::SearchQuery() : notes::parse_search(text);
    if (q == query_)
        return;
    query_ = q;
    board_model_->set_highlight(query_.highlight_terms());
    if (editor_)
        editor_->set_highlight(query_.highlight_terms());
    schedule_build();
    notify_state_changed();
}

void NotesScreen::run_command(const QString& text) {
    const notes::Command c = notes::parse_command(text);
    const auto done = [this]() {
        bar_->set_command_text(QString());
        bar_->set_command_hint(QString());
    };
    switch (c.kind) {
    case notes::CommandKind::Search: {
        search_timer_->stop();
        apply_search(text);
        QWidget* v = view_stack_->currentWidget();
        (v == wall_page_ ? static_cast<QWidget*>(wall_view_) : v)->setFocus(Qt::ShortcutFocusReason);
        if (v == board_view_ && !board_view_->currentIndex().isValid())
            board_view_->focus_lane(0);
        return;
    }
    case notes::CommandKind::NewNote:
        done();
        new_titled(c.text, c.ticker);
        return;
    case notes::CommandKind::NewFromTemplate:
        done();
        new_from_template(c.template_id, board_view_->current_lane_key(), c.ticker);
        return;
    case notes::CommandKind::GroupBy:
        done();
        set_group_by(c.group);
        return;
    case notes::CommandKind::View:
        done();
        set_view_mode(c.view);
        return;
    case notes::CommandKind::Open: {
        done();
        const QVector<NoteEntry> all = all_;
        const QString query = c.text;
        QPointer<NotesScreen> self = this;
        // The lookup scans every title: on the worker, not here.
        store_->run<int>([all, query]() { return notes::find_note_by_title(all, query); },
                         [self, query](int id) {
                             if (!self)
                                 return;
                             if (id <= 0 || !self->find_entry(id)) {
                                 self->show_notice(tr("No note titled “%1”.").arg(query.left(60)));
                                 return;
                             }
                             const NoteEntry* e = self->find_entry(id);
                             if (e->note.is_archived && !self->board().filter.show_archived)
                                 self->set_filter(notes::toggle_filter_key(self->board().filter, QStringLiteral("archived")));
                             self->select_in_views({id}, id, true);
                             self->open_note(id);
                         });
        return;
    }
    case notes::CommandKind::Board: {
        done();
        int found = -1;
        for (int i = 0; i < boards_.size() && found < 0; ++i)
            if (board_display_name(boards_.at(i)).compare(c.text, Qt::CaseInsensitive) == 0)
                found = i;
        for (int i = 0; i < boards_.size() && found < 0; ++i)
            if (board_display_name(boards_.at(i)).startsWith(c.text, Qt::CaseInsensitive))
                found = i;
        if (found < 0)
            show_notice(tr("No board named “%1”. Make one from the BOARD menu.").arg(c.text.left(40)));
        else
            switch_board(found);
        return;
    }
    case notes::CommandKind::Filter:
        done();
        set_filter(notes::toggle_filter_key(board().filter, c.text));
        return;
    case notes::CommandKind::Clear:
        done();
        apply_search(QString());
        set_filter(notes::BoardFilter());
        return;
    case notes::CommandKind::Help:
        done();
        show_help();
        return;
    }
}

void NotesScreen::show_help() {
    QMessageBox box(this);
    box.setWindowTitle(tr("Notes commands"));
    box.setTextFormat(Qt::PlainText);
    box.setText(tr("Type in the command field (/ focuses it) and press Enter:\n\n"
                   "  words, $AAPL, #macro      search (live while typing; quotes force a search)\n"
                   "  new                       a blank card in the focused lane\n"
                   "  new <title> / new $NVDA   a card with that title / ticker\n"
                   "  new trade idea AAPL       trade idea, earnings prep, journal, post-trade review\n"
                   "  group by ticker           status, category, ticker, priority, tag, date\n"
                   "  view wall / board / list  switch the view (Alt+1 / 2 / 3)\n"
                   "  open <title>              open a note by its title\n"
                   "  board <name>              switch to a named board\n"
                   "  filter pinned             pinned, ticker, due, linked, archived, high, week, stale …\n"
                   "  clear                     clear the search and filters\n\n"
                   "On the board: arrows move, Shift+arrows move the card, Enter opens, N new card, Space peek, "
                   "P pin, Del delete (Ctrl+Z undoes), T template, G group, Ctrl+Enter full editor."));
    box.exec();
}

// ── Chat Mode / Report ───────────────────────────────────────────────────────

void NotesScreen::open_chat_run() {
    QVariantMap nav{{QStringLiteral("screen_id"), QStringLiteral("chat_mode")}};
    if (const NoteEntry* e = find_entry(open_id())) {
        // The full run id when Chat Mode's event carried it, else the footer's prefix.
        const QString ref =
            e->meta.origin_ref.isEmpty() ? notes::chat_run_ref_from_body(e->note.content) : e->meta.origin_ref;
        if (!ref.isEmpty())
            nav.insert(QStringLiteral("run_id"), ref);
    }
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"), nav);
}

fincept::report::ReportPayload NotesScreen::report_payload() const {
    fincept::report::ReportPayload p;
    p.source = QStringLiteral("Notes");
    p.as_of = QDateTime::currentDateTime();
    const NoteEntry* e = find_entry(open_id());
    fincept::FinancialNote n = e ? e->note : session_base_;
    if (editor_ && editor_open_)
        n = editor_->collect(n); // what is on screen, typing included
    p.title = n.title.isEmpty() ? tr("Untitled note") : n.title;
    p.markdown = n.content;
    p.kpi_title = tr("Note");
    if (e) {
        p.kpis.append({tr("Status"), notes::status_label(e->meta.status), QString(), QString()});
        if (e->priority > 0)
            p.kpis.append({tr("Priority"), notes::priority_label(notes::priority_code(e->priority)), QString(),
                           e->priority >= 3 ? QStringLiteral("warning") : QString()});
        if (e->review.isValid())
            p.kpis.append({tr("Review on"), e->review.toString(Qt::ISODate), QString(), QString()});
        for (int i = 0; i < e->ticker_list.size() && i < 6; ++i) {
            const QString s = e->ticker_list.at(i);
            if (const notes::CardQuote* q = quotes_.find(s)) {
                p.kpis.append({QLatin1Char('$') + s,
                               fincept::ui::formatting::price(q->price) +
                                   (q->has_change ? QStringLiteral("  ") + fincept::ui::formatting::pct(q->change_pct, 2, true)
                                                  : QString()),
                               QString(),
                               q->has_change ? (q->change_pct >= 0 ? QStringLiteral("positive") : QStringLiteral("negative"))
                                             : QString()});
            }
        }
    }
    return p;
}

} // namespace fincept::screens
