// src/screens/notes/NotesScreen_Data.cpp — the library and the board data: load,
// board builds on the worker, applying a layout to the three views, the ribbon /
// status bar / chips, coalesced board-row and wall writes, live quotes for what is
// on screen, and the named boards (saved views).
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/notes/NotesBoardView.h"
#include "screens/notes/NotesDetailPane.h"
#include "screens/notes/NotesEditor.h"
#include "screens/notes/NotesModels.h"
#include "screens/notes/NotesScreen.h"
#include "screens/notes/NotesWallView.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"

#include <QDateTime>
#include <QInputDialog>
#include <QLocale>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
#include <QToolButton>

#include <algorithm>

namespace fincept::screens {

using notes::BoardViewMode;
using notes::GroupBy;
using notes::NoteEntry;
namespace nb = fincept::notes_board;

namespace {

namespace colors = fincept::ui::colors;

QString nsd_number(int n) {
    return QLocale().toString(n);
}

} // namespace

// ── Library ──────────────────────────────────────────────────────────────────

void NotesScreen::request_reload() {
    reload_timer_->start();
}

void NotesScreen::reload() {
    if (loading_) {
        reload_again_ = true;
        return;
    }
    loading_ = true;
    load_write_seq_ = write_seq_;
    store_->load([this](notes::NotesStore::LoadResult r) { on_loaded(std::move(r)); });
}

void NotesScreen::on_loaded(notes::NotesStore::LoadResult r) {
    loading_ = false;
    last_load_.start();
    // A write queued while this load ran may be newer than what it read.
    if (load_write_seq_ != write_seq_)
        reload_again_ = true;
    const bool first = !loaded_;
    if (!r.ok) {
        LOG_ERROR("Notes", fincept::logf("Could not load notes", {{"error", r.error}}));
        if (!loaded_) {
            view_stack_->setCurrentWidget(board_view_);
            status_->show_notice(tr("Could not load notes from the local database: %1").arg(r.error), tr("RETRY"),
                                 [this]() { reload(); }, 30000, fincept::ui::ScreenStatusBar::Tone::Danger);
        }
    } else {
        board_tables_ = r.board_tables;
        // While the open note has edits not yet written (or being written), the copy
        // in memory is newer than the database's.
        const int open = open_id();
        const bool keep_open = editor_open_ && open > 0 && editor_ &&
                               (editor_->revision() != queued_revision_ || saves_in_flight_ > 0);
        const NoteEntry* mine = keep_open ? find_entry(open) : nullptr;
        const NoteEntry kept = mine ? *mine : NoteEntry();
        QVector<NoteEntry> entries;
        entries.reserve(r.entries.size());
        for (auto& e : r.entries) {
            if (pending_delete_ids_.contains(e.note.id))
                continue;
            if (mine && e.note.id == open) {
                entries.append(kept);
                continue;
            }
            // Board rows queued but not yet written win over what was read.
            const auto dirty = meta_dirty_.constFind(e.note.id);
            if (dirty != meta_dirty_.constEnd())
                notes::apply_meta(e, dirty.value());
            // Chat Mode told us which run a note came from.
            const auto origin = pending_origin_.constFind(e.note.id);
            if (origin != pending_origin_.constEnd()) {
                nb::NoteBoardMeta m = e.meta;
                const bool change = m.origin != nb::kOriginAgentRuns || (!origin->isEmpty() && m.origin_ref != *origin);
                m.origin = nb::kOriginAgentRuns;
                if (!origin->isEmpty())
                    m.origin_ref = *origin;
                notes::apply_meta(e, m);
                if (change)
                    queue_meta(m);
                pending_origin_.remove(e.note.id);
            }
            entries.append(std::move(e));
        }
        all_ = std::move(entries);
        reindex();
        user_templates_ = r.templates;
        // Boards: the stored ones, keeping local changes to the current board.
        if (!r.boards.isEmpty()) {
            const qint64 current_id = loaded_ ? board().id : 0;
            const notes::BoardDef local = board();
            QVector<notes::BoardDef> boards;
            for (const auto& rec : r.boards)
                boards << notes::BoardDef::from_record(rec);
            int idx = 0;
            for (int i = 0; i < boards.size(); ++i)
                if (current_id > 0 && boards.at(i).id == current_id)
                    idx = i;
            if (board_dirty_ && current_id > 0 && boards.at(idx).id == current_id)
                boards[idx] = local;
            boards_ = boards;
            board_idx_ = idx;
        }
        loaded_ = true;
        // Deleted elsewhere (MCP tool, dashboard) while open and unchanged here.
        if (editor_open_ && open > 0 && !find_entry(open) && editor_ && editor_->revision() == queued_revision_ &&
            saves_in_flight_ == 0)
            show_empty();
    }
    if (first && loaded_) {
        // The skeleton stays until the first board build lands (apply_layout shows the view).
        bar_->set_segment(QStringLiteral("view"), notes::view_mode_key(board().view));
        update_chips();
        load_wall_data();
    }
    schedule_build();
    update_completions();
    if (has_pending_restore_) {
        has_pending_restore_ = false;
        restore_state(pending_restore_);
    }
    if (has_pending_link_ && loaded_) {
        // A deep link (open_section) that arrived before the library: it wins over the restore.
        has_pending_link_ = false;
        open_section(pending_link_section_, pending_link_focus_);
    }
    if (reload_again_) {
        reload_again_ = false;
        reload();
    }
}

const NoteEntry* NotesScreen::find_entry(int id) const {
    if (id <= 0)
        return nullptr;
    const auto it = index_.constFind(id);
    return it == index_.constEnd() ? nullptr : &all_.at(it.value());
}

NoteEntry* NotesScreen::find_entry_mut(int id) {
    if (id <= 0)
        return nullptr;
    const auto it = index_.constFind(id);
    return it == index_.constEnd() ? nullptr : &all_[it.value()];
}

void NotesScreen::reindex() {
    index_.clear();
    index_.reserve(all_.size());
    for (int i = 0; i < all_.size(); ++i)
        index_.insert(all_.at(i).note.id, i);
}

bool NotesScreen::layout_relevant_change(const NoteEntry& a, const NoteEntry& b) const {
    if (a.note.is_archived != b.note.is_archived || a.note.is_favorite != b.note.is_favorite ||
        a.category != b.category || a.priority != b.priority || a.tag_list != b.tag_list ||
        a.ticker_list != b.ticker_list || a.meta.status != b.meta.status || a.meta.rank != b.meta.rank ||
        a.review != b.review || a.linked != b.linked)
        return true;
    const notes::BoardDef& d = board();
    if ((d.group_by == GroupBy::Date || d.filter.edited_days > 0 || d.filter.stale_days > 0) &&
        a.updated_ms != b.updated_ms)
        return true;
    if (!query_.is_empty() && a.haystack != b.haystack)
        return true;
    return false;
}

void NotesScreen::upsert_entry(const fincept::FinancialNote& n) {
    NoteEntry e = notes::make_entry(n);
    if (const NoteEntry* existing = find_entry(n.id)) {
        nb::NoteBoardMeta m = existing->meta;
        notes::apply_meta(e, m);
        replace_entry(std::move(e));
        return;
    }
    const auto fresh = pending_new_meta_.constFind(n.id);
    if (fresh != pending_new_meta_.constEnd()) {
        notes::apply_meta(e, fresh.value()); // the lane / order this screen created it with
        pending_new_meta_.remove(n.id);
    }
    const auto dirty = meta_dirty_.constFind(n.id);
    if (dirty != meta_dirty_.constEnd())
        notes::apply_meta(e, dirty.value());
    all_.append(std::move(e));
    index_.insert(n.id, static_cast<int>(all_.size()) - 1);
    schedule_build();
    update_completions();
}

void NotesScreen::replace_entry(NoteEntry e) {
    NoteEntry* x = find_entry_mut(e.note.id);
    if (!x) {
        all_.append(std::move(e));
        reindex();
        schedule_build();
        return;
    }
    const bool rebuild = layout_relevant_change(*x, e);
    *x = e;
    if (rebuild) {
        schedule_build();
        update_completions();
        return;
    }
    board_model_->update_entry(e);
    table_model_->update_entry(e);
    wall_view_->update_entry(e);
}

// ── Board builds ─────────────────────────────────────────────────────────────

void NotesScreen::schedule_build() {
    if (!build_timer_->isActive())
        build_timer_->start();
}

void NotesScreen::build_now() {
    build_timer_->stop();
    if (!loaded_)
        return;
    const quint64 id = ++build_seq_;
    store_->build(all_, board(), query_, id,
                  [this](quint64 rid, notes::BoardLayout layout) { on_built(rid, std::move(layout)); });
}

void NotesScreen::on_built(quint64 request_id, notes::BoardLayout layout) {
    if (request_id != build_seq_)
        return; // a newer build is on its way
    layout_ = std::move(layout);
    apply_layout();
}

QString NotesScreen::lane_title(const QString& key) const {
    switch (board().group_by) {
    case GroupBy::Status:
        return notes::status_label(key);
    case GroupBy::Category:
        return notes::category_label(key).toUpper();
    case GroupBy::Priority:
        return notes::priority_label(key);
    case GroupBy::Ticker:
        return key.isEmpty() ? tr("NO TICKER") : QLatin1Char('$') + key;
    case GroupBy::Tag:
        return key.isEmpty() ? tr("UNTAGGED") : QLatin1Char('#') + key;
    case GroupBy::Date:
        if (key == notes::kLaneToday)
            return tr("TODAY");
        if (key == notes::kLaneWeek)
            return tr("THIS WEEK");
        return tr("EARLIER");
    }
    return key;
}

QVector<notes::NotesBoardModel::Lane> NotesScreen::model_lanes(const notes::BoardLayout& layout) const {
    QVector<notes::NotesBoardModel::Lane> out;
    out.reserve(layout.lanes.size());
    const notes::BoardDef& d = board();
    for (const auto& l : layout.lanes) {
        notes::NotesBoardModel::Lane lane;
        lane.key = l.key;
        lane.title = l.title.isEmpty() ? lane_title(l.key)
                                       : (d.group_by == GroupBy::Tag ? QLatin1Char('#') + l.title : l.title);
        lane.cards = l.cards;
        lane.tickers = l.tickers;
        lane.due = l.due;
        lane.collapsed = d.is_collapsed(d.group_by, l.key);
        lane.soft_limit = d.soft_limit(d.group_by, l.key);
        switch (d.group_by) {
        case GroupBy::Status:
            lane.accent = notes::status_color(l.key);
            break;
        case GroupBy::Category:
            lane.accent = notes::category_color(l.key);
            break;
        case GroupBy::Priority: {
            const int rank = notes::priority_rank(l.key);
            lane.accent = rank == 4   ? QColor(colors::DANGER())
                          : rank == 3 ? QColor(colors::WARNING())
                          : rank == 2 ? QColor(colors::INFO())
                                      : QColor(colors::TEXT_TERTIARY());
            break;
        }
        case GroupBy::Ticker:
            lane.accent = l.key.isEmpty() ? QColor(colors::TEXT_TERTIARY()) : QColor(colors::AMBER());
            break;
        case GroupBy::Tag:
            lane.accent = l.key.isEmpty() ? QColor(colors::TEXT_TERTIARY())
                                          : colors::chart_color(static_cast<int>(qHash(l.key) % 12));
            break;
        case GroupBy::Date:
            lane.accent = l.key == notes::kLaneToday ? QColor(colors::AMBER()) : QColor(colors::INFO());
            break;
        }
        out << lane;
    }
    return out;
}

void NotesScreen::apply_layout() {
    if (!loaded_)
        return;
    const QVector<int> sel = selected_ids();
    const int cur = current_card();
    syncing_selection_ = true;
    const QHash<QString, int> scrolls = board_view_->lane_scrolls();
    board_view_->set_show_status(board().group_by != GroupBy::Status);
    board_model_->set_highlight(query_.highlight_terms());
    board_model_->set_lanes(model_lanes(layout_));
    board_view_->restore_lane_scrolls(scrolls);
    board_view_->select_ids(sel, cur, false);
    const BoardViewMode v = board().view;
    if (v == BoardViewMode::List) {
        QVector<NoteEntry> rows;
        rows.reserve(layout_.shown);
        for (const auto& lane : layout_.lanes)
            rows += lane.cards;
        table_model_->set_rows(std::move(rows));
    }
    if (v == BoardViewMode::Wall) {
        wall_view_->set_show_status(board().group_by != GroupBy::Status);
        wall_view_->set_highlight(query_.highlight_terms());
        wall_view_->set_lanes(layout_.lanes);
    }
    select_in_views(sel, cur, false);
    syncing_selection_ = false;
    if (view_stack_->currentWidget() == skeleton_)
        set_view_mode(v);
    peek_cached_quotes();
    update_ribbon();
    update_status_bar();
    update_chips();
    update_ghosts();
    update_empty_detail();
    subscribe_timer_->start();
}

// ── Ribbon / status / chips ──────────────────────────────────────────────────

void NotesScreen::update_ribbon() {
    if (!ribbon_)
        return;
    using Tone = fincept::ui::StatTile::Tone;
    const notes::BoardStats& s = layout_.stats;
    const auto set = [this](const char* key, const QString& value, const QString& sub, Tone tone) {
        if (auto* t = ribbon_->tile(QString::fromLatin1(key))) {
            t->set_value(value);
            t->set_sub_label(sub);
            t->set_tone(tone);
            t->set_state(loaded_ ? fincept::ui::StatTile::State::Normal : fincept::ui::StatTile::State::Loading);
        }
    };
    const notes::BoardFilter& f = board().filter;
    set("notes", nsd_number(s.notes), s.archived > 0 ? tr("%1 archived").arg(nsd_number(s.archived)) : QString(),
        Tone::Neutral);
    set("active", nsd_number(s.active_ideas), tr("%1 in position").arg(nsd_number(s.in_position)),
        f.statuses.isEmpty() ? Tone::Neutral : Tone::Warning);
    set("due", nsd_number(s.due), s.overdue > 0 ? tr("%1 overdue").arg(nsd_number(s.overdue)) : tr("none overdue"),
        s.due > 0 ? Tone::Warning : Tone::Neutral);
    set("pinned", nsd_number(s.pinned), tr("dashboard favourites"), Tone::Neutral);
    const auto ud = quotes_.up_down(layout_.all_tickers, 300);
    set("tickers", nsd_number(s.tickers),
        ud.first + ud.second > 0 ? QStringLiteral("▲%1  ▼%2 %3").arg(ud.first).arg(ud.second).arg(tr("today"))
                                 : tr("no quotes yet"),
        ud.first > ud.second ? Tone::Positive : ud.second > ud.first ? Tone::Negative : Tone::Neutral);
    set("week", nsd_number(s.edited_week), tr("last 7 days"), Tone::Neutral);
    QString title = s.oldest_unreviewed_title;
    if (title.size() > 28)
        title = title.left(27) + QChar(0x2026);
    set("oldest", s.oldest_unreviewed_days >= 0 ? tr("%1d").arg(s.oldest_unreviewed_days) : QStringLiteral("—"),
        title, s.oldest_unreviewed_days >= 30 ? Tone::Warning : Tone::Neutral);
}

QString NotesScreen::board_display_name(const notes::BoardDef& d) const {
    // The seeded boards show translated names until the user renames them.
    const auto defaults = notes::default_boards();
    for (const auto& x : defaults)
        if (!d.preset.isEmpty() && d.preset == x.preset && d.name == x.name) {
            if (d.preset == QLatin1String("all"))
                return tr("All notes");
            if (d.preset == QLatin1String("trade_ideas"))
                return tr("Trade ideas");
            if (d.preset == QLatin1String("earnings"))
                return tr("Earnings");
            if (d.preset == QLatin1String("journal"))
                return tr("Journal");
        }
    return d.name.isEmpty() ? tr("Untitled board") : d.name;
}

void NotesScreen::update_status_bar() {
    if (!status_)
        return;
    using Tone = fincept::ui::ScreenStatusBar::Tone;
    status_->set_text(QStringLiteral("board"), board_display_name(board()).toUpper());
    status_->set_text(QStringLiteral("count"),
                      loaded_ ? tr("%1 / %2 NOTES").arg(nsd_number(layout_.shown), nsd_number(layout_.total))
                              : tr("LOADING…"));
    const int sel = static_cast<int>(selected_ids().size());
    status_->set_text(QStringLiteral("selection"), tr("%1 SELECTED").arg(sel));
    status_->set_item_visible(QStringLiteral("selection"), sel > 1);
    QString group;
    switch (board().group_by) {
    case GroupBy::Status:
        group = tr("BY STATUS");
        break;
    case GroupBy::Category:
        group = tr("BY CATEGORY");
        break;
    case GroupBy::Ticker:
        group = tr("BY TICKER");
        break;
    case GroupBy::Priority:
        group = tr("BY PRIORITY");
        break;
    case GroupBy::Tag:
        group = tr("BY TAG");
        break;
    case GroupBy::Date:
        group = tr("BY DATE");
        break;
    }
    const QString view = board().view == BoardViewMode::Wall   ? tr("WALL")
                         : board().view == BoardViewMode::List ? tr("LIST")
                                                               : tr("BOARD");
    status_->set_text(QStringLiteral("view"), view + QStringLiteral(" · ") + group);
    status_->set_item(QStringLiteral("quotes"),
                      subscribed_.isEmpty() ? tr("NO LIVE QUOTES")
                                            : QStringLiteral("● ") + tr("%n LIVE QUOTE(S)", nullptr,
                                                                             static_cast<int>(subscribed_.size())),
                      subscribed_.isEmpty() ? Tone::Dim : Tone::Ok);
    status_->set_tooltip(QStringLiteral("quotes"),
                         tr("Quotes are subscribed only for cards on screen (up to 40) and the open note, and only "
                            "while this tab is visible."));
    if (save_state_text_.isEmpty()) {
        save_state_text_ = tr("SAVED");
        save_state_tone_ = static_cast<int>(Tone::Ok);
    }
    status_->set_item(QStringLiteral("save"), save_state_text_, static_cast<Tone>(save_state_tone_));
}

void NotesScreen::update_chips() {
    if (!bar_)
        return;
    const notes::BoardFilter& f = board().filter;
    bar_->set_action_text(QStringLiteral("board"), tr("BOARD: %1").arg(board_display_name(board()).toUpper()));
    bar_->set_action_tooltip(QStringLiteral("board"), tr("Named boards: each keeps its filters, grouping, lane order "
                                                         "and wall layout"));
    QString group;
    switch (board().group_by) {
    case GroupBy::Status:
        group = tr("STATUS");
        break;
    case GroupBy::Category:
        group = tr("CATEGORY");
        break;
    case GroupBy::Ticker:
        group = tr("TICKER");
        break;
    case GroupBy::Priority:
        group = tr("PRIORITY");
        break;
    case GroupBy::Tag:
        group = tr("TAG");
        break;
    case GroupBy::Date:
        group = tr("DATE");
        break;
    }
    bar_->set_action_text(QStringLiteral("group"), tr("GROUP: %1").arg(group));
    bar_->set_segment(QStringLiteral("view"), notes::view_mode_key(board().view));
    bar_->set_chip_checked(QStringLiteral("pinned"), f.pinned_only);
    bar_->set_chip_checked(QStringLiteral("ticker"), f.has_ticker);
    bar_->set_chip_checked(QStringLiteral("due"), f.due_only);
    bar_->set_chip_checked(QStringLiteral("linked"), f.linked_only);
    bar_->set_chip_checked(QStringLiteral("archived"), f.show_archived);
    bar_->set_chip_checked(QStringLiteral("clear"), false);
    bar_->set_chip_checked(QStringLiteral("priority"), !f.priorities.isEmpty());
    bar_->set_chip_text(QStringLiteral("priority"),
                        f.priorities.isEmpty() ? tr("PRIORITY") : tr("PRIORITY: %1").arg(f.priorities.join(QLatin1Char('+'))));
    bar_->set_chip_checked(QStringLiteral("tag"), !f.tag.isEmpty());
    bar_->set_chip_text(QStringLiteral("tag"), f.tag.isEmpty() ? tr("TAG") : tr("TAG: #%1").arg(f.tag));
    bar_->set_chip_checked(QStringLiteral("date"), f.edited_days > 0);
    bar_->set_chip_text(QStringLiteral("date"), f.edited_days > 0 ? tr("EDITED: %1D").arg(f.edited_days) : tr("EDITED"));
    for (QToolButton* b : wall_btns_)
        if (b->property("wall_action").toString() == QLatin1String("snap"))
            b->setChecked(board().wall_snap);
}

void NotesScreen::update_completions() {
    if (!editor_)
        return;
    QStringList tags;
    for (const auto& t : layout_.top_tags)
        tags << t.first;
    editor_->set_completions({}, tags);
}

void NotesScreen::update_empty_detail() {
    if (!detail_)
        return;
    const bool empty = loaded_ && layout_.stats.notes == 0 && layout_.stats.archived == 0;
    detail_->set_empty(!loaded_ || empty, layout_.stats.last_edited_id, layout_.stats.last_edited_title);
}

void NotesScreen::update_ghosts() {
    if (!board_view_)
        return;
    QHash<QString, notes::NotesBoardView::GhostTemplate> ghosts;
    const bool fresh = loaded_ && layout_.stats.notes == 0 && layout_.stats.archived == 0;
    if (fresh && board().group_by == GroupBy::Status) {
        // A new notebook: each workflow lane suggests the template that starts there.
        for (const auto& t : notes::builtin_templates()) {
            notes::NotesBoardView::GhostTemplate g;
            g.template_id = t.id;
            g.title = QStringLiteral("+ ") + QCoreApplication::translate("NotesBoard", t.name.toUtf8().constData());
            g.hint = tr("Click to start one here. N adds a blank card.");
            ghosts.insert(t.status, g);
        }
    }
    board_view_->set_ghost_templates(ghosts);
}

// ── Coalesced writes ─────────────────────────────────────────────────────────

void NotesScreen::queue_meta(const nb::NoteBoardMeta& m, const std::optional<nb::NoteBoardEvent>& event) {
    if (m.note_id <= 0)
        return;
    nb::NoteBoardMeta row = m;
    row.stored = true;
    meta_dirty_.insert(row.note_id, row);
    if (event)
        events_pending_ << *event;
    if (!meta_timer_->isActive())
        meta_timer_->start(); // coalesce: one transaction per burst of moves
}

void NotesScreen::flush_meta(bool sync) {
    meta_timer_->stop();
    if (meta_dirty_.isEmpty() && events_pending_.isEmpty())
        return;
    const QVector<nb::NoteBoardMeta> rows(meta_dirty_.cbegin(), meta_dirty_.cend());
    const QVector<nb::NoteBoardEvent> events = events_pending_;
    meta_dirty_.clear();
    events_pending_.clear();
    ++write_seq_;
    if (sync) {
        store_->save_meta_now(rows, events);
        return;
    }
    QPointer<NotesScreen> self = this;
    store_->save_meta(rows, events, [self](bool ok) {
        if (!self || ok || !self->board_tables_)
            return;
        self->save_state_text_ = tr("BOARD NOT SAVED");
        self->save_state_tone_ = static_cast<int>(fincept::ui::ScreenStatusBar::Tone::Danger);
        self->update_status_bar();
        self->status_->show_notice(tr("Board changes (status, order, review dates) could not be written to the local "
                                      "database."),
                                   QString(), {}, 8000, fincept::ui::ScreenStatusBar::Tone::Danger);
    });
}

void NotesScreen::flush_wall(bool sync) {
    wall_timer_->stop();
    if (wall_dirty_.isEmpty())
        return;
    const QVector<nb::NoteWallItem> items(wall_dirty_.cbegin(), wall_dirty_.cend());
    const qint64 board_id = wall_dirty_board_;
    wall_dirty_.clear();
    if (sync)
        store_->save_wall_items_now(board_id, items);
    else
        store_->save_wall_items(board_id, items, nullptr);
}

void NotesScreen::flush_board(bool sync) {
    board_timer_->stop();
    if (!board_dirty_)
        return;
    board_dirty_ = false;
    const nb::NoteBoardRecord rec = board().to_record();
    if (sync) {
        store_->save_board_now(rec);
        return;
    }
    const int idx = board_idx_;
    QPointer<NotesScreen> self = this;
    store_->save_board(rec, [self, idx](qint64 id) {
        if (self && id > 0 && idx < self->boards_.size() && self->boards_.at(idx).id <= 0) {
            self->boards_[idx].id = id;
            if (idx == self->board_idx_)
                self->load_wall_data();
        }
    });
}

void NotesScreen::load_wall_data() {
    const qint64 id = board().id;
    const quint64 request = ++wall_request_;
    if (id <= 0) {
        wall_view_->set_saved(0, {}, {});
        return;
    }
    QPointer<NotesScreen> self = this;
    store_->load_wall(id, request, [self](notes::NotesStore::WallData w) {
        if (!self || w.request_id != self->wall_request_)
            return;
        self->wall_view_->set_saved(w.board_id, w.items, w.groups);
        self->wall_view_->set_snap(self->board().wall_snap);
        self->wall_view_->set_zoom(self->board().wall_zoom);
        if (self->board().wall_cx != 0.0 || self->board().wall_cy != 0.0)
            self->wall_view_->set_center(QPointF(self->board().wall_cx, self->board().wall_cy));
    });
}

// ── Quotes ───────────────────────────────────────────────────────────────────

void NotesScreen::on_visible_tickers(const QStringList& tickers) {
    card_symbols_ = tickers;
    subscribe_timer_->start();
}

void NotesScreen::resubscribe_quotes() {
    if (!isVisible())
        return;
    QStringList wanted = editor_symbols_.mid(0, 12);
    QStringList cards = card_symbols_;
    if (board().view == BoardViewMode::List && table_view_->isVisible()) {
        // The rows on screen.
        cards.clear();
        const int first = std::max(0, table_view_->rowAt(0));
        int last = table_view_->rowAt(table_view_->viewport()->height() - 1);
        if (last < 0)
            last = table_proxy_->rowCount() - 1;
        for (int r = first; r <= last && cards.size() < 40; ++r) {
            const QModelIndex src = table_proxy_->mapToSource(table_proxy_->index(r, 0));
            if (const NoteEntry* e = table_model_->entry(src.row()))
                if (!e->ticker_list.isEmpty() && !cards.contains(e->ticker_list.first()))
                    cards << e->ticker_list.first();
        }
    }
    for (const QString& s : cards)
        if (!wanted.contains(s) && wanted.size() < 52)
            wanted << s;
    auto& hub = datahub::DataHub::instance();
    const QSet<QString> want(wanted.cbegin(), wanted.cend());
    for (const QString& s : QSet<QString>(subscribed_))
        if (!want.contains(s)) {
            hub.unsubscribe(quote_owner_, services::MarketDataService::quote_topic(s));
            subscribed_.remove(s);
        }
    QPointer<NotesScreen> self = this;
    for (const QString& s : wanted) {
        if (subscribed_.contains(s))
            continue;
        subscribed_.insert(s);
        hub.subscribe(quote_owner_, services::MarketDataService::quote_topic(s), [self, s](const QVariant& v) {
            if (self)
                self->on_quote(s, v);
        });
    }
    update_status_bar();
}

void NotesScreen::unsubscribe_all_quotes() {
    if (quote_owner_)
        datahub::DataHub::instance().unsubscribe(quote_owner_);
    subscribed_.clear();
}

void NotesScreen::on_quote(const QString& symbol, const QVariant& value) {
    if (!value.canConvert<services::QuoteData>())
        return;
    const auto q = value.value<services::QuoteData>();
    if (!(q.price > 0))
        return;
    notes::CardQuote cq;
    cq.price = q.price;
    cq.change_pct = q.change_pct;
    cq.has_change = true;
    cq.at_ms = QDateTime::currentMSecsSinceEpoch();
    if (quotes_.put(symbol, cq)) {
        dirty_quotes_.insert(symbol);
        if (!quote_timer_->isActive())
            quote_timer_->start(); // one repaint per burst of ticks
    }
}

void NotesScreen::peek_cached_quotes() {
    // Last known quotes the terminal already holds (no request is made): the
    // ribbon's up / down count and cards scrolled into view start filled.
    auto& hub = datahub::DataHub::instance();
    int n = 0;
    for (const QString& s : layout_.all_tickers) {
        if (++n > 300)
            break;
        if (quotes_.find(s))
            continue;
        const QVariant v = hub.peek_raw(services::MarketDataService::quote_topic(s));
        if (!v.isValid() || !v.canConvert<services::QuoteData>())
            continue;
        const auto q = v.value<services::QuoteData>();
        if (!(q.price > 0))
            continue;
        notes::CardQuote cq;
        cq.price = q.price;
        cq.change_pct = q.change_pct;
        cq.has_change = true;
        quotes_.put(s, cq);
    }
}

void NotesScreen::repaint_quotes() {
    const QStringList dirty(dirty_quotes_.cbegin(), dirty_quotes_.cend());
    dirty_quotes_.clear();
    board_view_->viewport()->update();
    if (board().view == BoardViewMode::Wall)
        wall_view_->refresh_cards();
    if (board().view == BoardViewMode::List && table_model_->quotes_changed(dirty) && !flash_timer_->isActive())
        flash_timer_->start();
    detail_->quote_strip()->refresh();
    if (editor_ && editor_open_)
        for (const QString& s : dirty)
            if (editor_symbols_.contains(s))
                if (const notes::CardQuote* q = quotes_.find(s))
                    editor_->set_quote(s, fincept::ui::formatting::price(q->price), q->change_pct, q->has_change);
    update_ribbon();
}

// ── Boards (saved views) ─────────────────────────────────────────────────────

notes::BoardDef& NotesScreen::board() {
    if (boards_.isEmpty())
        boards_ = notes::default_boards();
    board_idx_ = std::clamp(board_idx_, 0, static_cast<int>(boards_.size()) - 1);
    return boards_[board_idx_];
}

const notes::BoardDef& NotesScreen::board() const {
    return boards_.at(std::clamp(board_idx_, 0, static_cast<int>(boards_.size()) - 1));
}

void NotesScreen::board_changed(bool rebuild) {
    board_dirty_ = true;
    board_timer_->start();
    if (rebuild)
        schedule_build();
    update_chips();
    update_status_bar();
    notify_state_changed();
}

void NotesScreen::switch_board(int index) {
    if (index < 0 || index >= boards_.size())
        return;
    flush_board(false);
    flush_wall(false);
    board_idx_ = index;
    const BoardViewMode v = board().view;
    set_view_mode(v);
    board_dirty_ = false; // switching is not an edit of the new board
    load_wall_data();
    schedule_build();
    update_chips();
    update_status_bar();
    notify_state_changed();
}

void NotesScreen::new_board() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New board"), tr("Name of the new board:"), QLineEdit::Normal,
                                               QString(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    flush_board(false);
    notes::BoardDef d;
    d.name = name.left(80);
    d.position = static_cast<int>(boards_.size());
    boards_ << d;
    board_idx_ = static_cast<int>(boards_.size()) - 1;
    board_dirty_ = true;
    flush_board(false); // stored now: its id arrives before the wall needs it
    switch_board(board_idx_);
}

void NotesScreen::duplicate_board() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Duplicate board"), tr("Name of the copy:"), QLineEdit::Normal,
                                               tr("%1 (copy)").arg(board_display_name(board())), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    flush_board(false);
    notes::BoardDef d = board();
    d.id = 0;
    d.name = name.left(80);
    d.preset.clear();
    d.position = static_cast<int>(boards_.size());
    boards_ << d;
    board_idx_ = static_cast<int>(boards_.size()) - 1;
    board_dirty_ = true;
    flush_board(false);
    switch_board(board_idx_);
}

void NotesScreen::rename_board() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename board"), tr("Board name:"), QLineEdit::Normal,
                                               board_display_name(board()), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    board().name = name.left(80);
    board().preset.clear();
    board_changed(false);
}

void NotesScreen::delete_board() {
    if (boards_.size() <= 1)
        return;
    if (QMessageBox::question(this, tr("Delete board"),
                              tr("Delete the board “%1”? Its filters, lane order and wall layout are removed; the "
                                 "notes stay in the notebook.")
                                  .arg(board_display_name(board())),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    const qint64 id = board().id;
    board_dirty_ = false;
    wall_dirty_.clear();
    boards_.removeAt(board_idx_);
    for (int i = 0; i < boards_.size(); ++i)
        if (boards_.at(i).position != i) {
            boards_[i].position = i;
            store_->save_board(boards_.at(i).to_record(), nullptr);
        }
    if (id > 0)
        store_->remove_board(id, nullptr);
    switch_board(std::max(0, board_idx_ - 1));
}

void NotesScreen::move_board(int delta) {
    const int from = board_idx_;
    const int to = from + delta;
    if (delta == 0 || to < 0 || to >= boards_.size())
        return;
    const QVector<int> changed = notes::reorder_boards(boards_, from, to);
    board_idx_ = to; // the same board, at its new place
    for (const int i : changed)
        if (i != board_idx_ && boards_.at(i).id > 0)
            store_->save_board(boards_.at(i).to_record(), nullptr);
    board_changed(false);
    flush_board(false); // its row (new position included) now, so a reload reads the new order
}

void NotesScreen::set_group_by(GroupBy g) {
    if (board().group_by == g)
        return;
    board().group_by = g;
    board_view_->set_show_status(g != GroupBy::Status);
    wall_view_->set_show_status(g != GroupBy::Status);
    board_changed();
}

void NotesScreen::set_view_mode(BoardViewMode v) {
    if (!loaded_) {
        bar_->set_segment(QStringLiteral("view"), notes::view_mode_key(v));
        return;
    }
    const QVector<int> sel = selected_ids();
    const int cur = current_card();
    const bool changed = board().view != v;
    board().view = v;
    hide_peek();
    if (v == BoardViewMode::Board) {
        view_stack_->setCurrentWidget(board_view_);
    } else if (v == BoardViewMode::Wall) {
        wall_view_->set_show_status(board().group_by != GroupBy::Status);
        wall_view_->set_highlight(query_.highlight_terms());
        wall_view_->set_lanes(layout_.lanes);
        wall_view_->set_snap(board().wall_snap);
        view_stack_->setCurrentWidget(wall_page_);
    } else {
        QVector<NoteEntry> rows;
        rows.reserve(layout_.shown);
        for (const auto& lane : layout_.lanes)
            rows += lane.cards;
        table_model_->set_rows(std::move(rows));
        view_stack_->setCurrentWidget(table_view_);
    }
    syncing_selection_ = true;
    select_in_views(sel, cur, true);
    syncing_selection_ = false;
    bar_->set_segment(QStringLiteral("view"), notes::view_mode_key(v));
    card_symbols_ = v == BoardViewMode::Wall    ? wall_view_->visible_tickers()
                    : v == BoardViewMode::Board ? board_view_->visible_tickers()
                                                : QStringList();
    subscribe_timer_->start();
    if (changed)
        board_changed(false);
    QWidget* focus = v == BoardViewMode::Wall ? static_cast<QWidget*>(wall_view_)
                     : v == BoardViewMode::List ? static_cast<QWidget*>(table_view_)
                                                : static_cast<QWidget*>(board_view_);
    if (isVisible() && !focus_mode_)
        focus->setFocus(Qt::OtherFocusReason);
}

void NotesScreen::set_filter(const notes::BoardFilter& f) {
    if (f == board().filter) {
        update_chips();
        return;
    }
    board().filter = f;
    board_changed();
}

void NotesScreen::move_lane(const QString& key, int to_index) {
    QStringList order;
    for (int i = 0; i < board_model_->lane_count(); ++i)
        order << board_model_->lane(i).key;
    const int from = static_cast<int>(order.indexOf(key));
    if (from < 0)
        return;
    order.move(from, std::clamp(to_index, 0, static_cast<int>(order.size()) - 1));
    board().lane_order.insert(notes::group_by_key(board().group_by), order);
    board_changed();
}

void NotesScreen::toggle_lane(const QString& key) {
    const GroupBy g = board().group_by;
    board().set_collapsed(g, key, !board().is_collapsed(g, key));
    board_changed();
}

void NotesScreen::set_lane_limit(const QString& key) {
    const GroupBy g = board().group_by;
    bool ok = false;
    const int n = QInputDialog::getInt(this, tr("Soft limit"),
                                       tr("Mark the lane “%1” when it holds more cards than (0 = no limit):")
                                           .arg(lane_title(key)),
                                       board().soft_limit(g, key), 0, 10000, 1, &ok);
    if (!ok)
        return;
    board().set_soft_limit(g, key, n);
    board_changed();
}

} // namespace fincept::screens
