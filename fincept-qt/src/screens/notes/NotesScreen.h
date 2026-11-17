#pragma once
// NotesScreen — the research notebook as a BOARD, in the terminal's dense
// Portfolio / Watchlist style:
//
//   command bar   board selector · › search / command field · NEW · TEMPLATE ·
//                 BOARD | WALL | LIST · GROUP BY · IMPORT · EXPORT · DETAIL
//                 FILTER chips: pinned · has ticker · due · linked · priority · tag · edited · archived
//   stats ribbon  notes · active ideas · due for review · pinned · tickers (▲/▼ today) ·
//                 edited this week · oldest unreviewed — each tile applies its filter
//   board         BOARD (cards in lanes; NotesBoardView), WALL (free placement;
//                 NotesWallView) or LIST (sortable table) | detail pane (editor,
//                 status, review date, live quotes, linked actions, history, Add to Report)
//   status bar    board · shown / total · selection · notices with UNDO · save state · quotes · clock
//
// Storage contract unchanged: financial_notes through NotesRepository, on a
// worker thread (NotesStore). After each note write the screen publishes
// notes.created / notes.updated / notes.deleted (source "notes_screen") so the
// dashboard Notes tile refreshes; pinned == is_favorite (the tile's "favorites").
// The board's own attributes (status, manual order, review date, origin, boards,
// wall layout) live in the note_board_* side tables (migration v066).
#include "core/events/EventBus.h"
#include "core/report/ReportBlocks.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/notes/NotesBoardModel.h"
#include "screens/notes/NotesCardPainter.h"
#include "screens/notes/NotesStore.h"

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QPointer>
#include <QSet>
#include <QUrl>
#include <QWidget>

#include <functional>
#include <optional>

class QFrame;
class QLabel;
class QMenu;
class QSortFilterProxyModel;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTextBrowser;
class QTimer;
class QToolButton;

namespace fincept::ui {
class KpiStrip;
class LoadingSkeleton;
class ScreenCommandBar;
class ScreenStatusBar;
} // namespace fincept::ui

namespace fincept::screens {

namespace notes {
class NotesBoardModel;
class NotesBoardView;
class NotesDetailPane;
class NotesEditor;
class NotesTableModel;
class NotesWallView;
} // namespace notes

class NotesScreen : public QWidget, public IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit NotesScreen(QWidget* parent = nullptr);
    ~NotesScreen() override;

    // ISectionLinked — "board" / "wall" / "list" (the BOARD | WALL | LIST segments), "new" (NEW),
    // "note:<id>" (selects and opens that note). `focus`: "<note id>" / "note:<id>" opens a note,
    // "board:<board id>" switches to that saved board first. Before the library has loaded the
    // link is kept and applied when it lands.
    bool open_section(const QString& section, const QString& focus) override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "notes"; }
    int state_version() const override { return 2; }
    /// Write the open note and pending board changes now (layout switch / tear-off).
    void flush_pending_state() override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    // ── Building (NotesScreen.cpp) ──────────────────────────────────────────
    void apply_styles();
    void build_ui();
    void build_command_bar();
    void build_ribbon();
    void build_views();
    void build_status_bar();
    void build_peek();
    void install_shortcuts();
    void refit();
    void retranslateUi();
    void populate_board_menu();
    void populate_template_menu();
    void populate_group_menu();
    void populate_export_menu();
    void populate_priority_menu();
    void populate_tag_menu();
    void populate_date_menu();

    // ── Library and board data (NotesScreen_Data.cpp) ───────────────────────
    void request_reload();
    void reload();
    void on_loaded(notes::NotesStore::LoadResult result);
    void schedule_build();
    void build_now();
    void on_built(quint64 request_id, notes::BoardLayout layout);
    void apply_layout();
    QVector<notes::NotesBoardModel::Lane> model_lanes(const notes::BoardLayout& layout) const;
    QString lane_title(const QString& key) const;
    const notes::NoteEntry* find_entry(int id) const;
    notes::NoteEntry* find_entry_mut(int id);
    void reindex();
    /// Put `n` into the library (insert or replace), keeping its board row.
    void upsert_entry(const fincept::FinancialNote& n);
    /// Replace an entry (note and/or board row) and refresh what shows it.
    void replace_entry(notes::NoteEntry e);
    bool layout_relevant_change(const notes::NoteEntry& a, const notes::NoteEntry& b) const;
    void update_ribbon();
    void update_status_bar();
    void update_chips();
    void update_completions();
    void update_empty_detail();
    void update_ghosts();
    // Board rows / wall writes (coalesced, flushed on hide / quit / lock)
    void queue_meta(const fincept::notes_board::NoteBoardMeta& m,
                    const std::optional<fincept::notes_board::NoteBoardEvent>& event = std::nullopt);
    void flush_meta(bool sync);
    void flush_wall(bool sync);
    void flush_board(bool sync);
    void load_wall_data();
    // Quotes
    void on_visible_tickers(const QStringList& tickers);
    void resubscribe_quotes();
    void unsubscribe_all_quotes();
    void on_quote(const QString& symbol, const QVariant& value);
    void peek_cached_quotes();
    void repaint_quotes();
    // Boards (saved views)
    notes::BoardDef& board();
    const notes::BoardDef& board() const;
    QString board_display_name(const notes::BoardDef& d) const;
    void switch_board(int index);
    void board_changed(bool rebuild = true);
    void new_board();
    void rename_board();
    void duplicate_board();
    void delete_board();
    /// Move the current board up (-1) or down (+1) in the board menu; stored.
    void move_board(int delta);
    void set_group_by(notes::GroupBy g);
    void set_view_mode(notes::BoardViewMode v);
    void set_filter(const notes::BoardFilter& f);
    void move_lane(const QString& key, int to_index);
    void toggle_lane(const QString& key);
    void set_lane_limit(const QString& key);

    // ── Selection / detail (NotesScreen_Actions.cpp) ────────────────────────
    QVector<int> selected_ids() const;
    int current_card() const;
    void select_in_views(const QVector<int>& ids, int current, bool scroll);
    void on_selection_changed();
    void show_detail_for_selection();
    void set_focus_mode(bool on);
    void set_detail_visible(bool on);
    void peek(int id);
    void hide_peek();

    // ── Editor session ──────────────────────────────────────────────────────
    notes::NotesEditor* editor();
    /// Show `id` in the detail pane; `force` reloads it when it is already open
    /// (its title / attributes changed outside the editor).
    void open_note(int id, bool force = false);
    void open_new_note(const fincept::FinancialNote& seed, bool save_now, const fincept::notes_board::NoteBoardMeta& meta);
    void close_editor();
    void show_empty();
    void show_bulk(const QVector<int>& ids);
    int open_id() const;
    void on_editor_edited();
    void save_editor_async();
    bool flush_editor_sync();
    void on_save_done(quint64 session, quint64 revision, int id, bool ok, fincept::FinancialNote saved,
                      notes::IdSlot slot, int attempt);
    void flush_for_quit();
    void load_history(int id);
    void check_renamed_tickers();

    // ── Actions ─────────────────────────────────────────────────────────────
    void new_card(const QString& lane_key);
    void new_from_template(const QString& template_id, const QString& lane_key, const QString& ticker);
    void new_from_user_template(int index);
    void new_titled(const QString& title, const QString& ticker);
    void new_card_on_wall(const QPointF& pos);
    fincept::FinancialNote seed_for_lane(const QString& lane_key, fincept::notes_board::NoteBoardMeta* meta) const;
    double top_rank_of_lane(const QString& lane_key) const;
    void set_pinned(const QVector<int>& ids, bool pinned);
    void set_archived(const QVector<int>& ids, bool archived);
    void set_status(const QVector<int>& ids, const QString& status);
    void set_review(const QVector<int>& ids, const QDate& date);
    void mark_reviewed(int id);
    void add_tag(const QVector<int>& ids);
    void add_symbol_to_note(int id, const QString& symbol);
    void duplicate(int id);
    void delete_notes(const QVector<int>& ids);
    void undo_delete();
    void commit_deletes();
    void export_notes(const QVector<int>& ids, bool to_file_manager);
    void import_files();
    void save_as_template();
    void revert_session();
    void on_editor_action(const QString& action);
    void on_bulk_action(const QString& action);
    void show_symbol_menu(const QString& symbol);
    void open_symbol_in(const QString& screen_id, const QString& symbol);
    void on_link(const QUrl& url);
    void show_card_menu(const QPoint& global_pos);
    void show_lane_menu(const QString& lane_key, const QPoint& global_pos);
    QString check_drop(const QVector<int>& ids, const QString& from_lane, const QString& to_lane) const;
    void move_cards(const QVector<int>& ids, const QString& from_lane, const QString& to_lane, int to_row);
    void move_by_key(int dlane, int drow);
    void write_notes(const QVector<fincept::FinancialNote>& notes);
    void show_notice(const QString& text, std::function<void()> undo = {}, std::function<void()> on_expire = {},
                     int ms = 7000);
    void run_command(const QString& text);
    void on_command_edited(const QString& text);
    void apply_search(const QString& text);
    void show_help();
    void open_chat_run();
    fincept::report::ReportPayload report_payload() const;

    // ── Quotes for the open note / events ──────────────────────────────────
    void subscribe_events();
    void unsubscribe_events();
    void notify_state_changed();

    // Library
    notes::NotesStore* store_ = nullptr;
    QVector<notes::NoteEntry> all_;
    QHash<int, int> index_;                  ///< note id -> position in all_
    QVector<fincept::NoteTemplate> user_templates_;
    notes::BoardLayout layout_;              ///< the last board build
    bool loaded_ = false;
    bool loading_ = false;
    bool reload_again_ = false;
    bool board_tables_ = true;
    quint64 write_seq_ = 0;
    quint64 load_write_seq_ = 0;
    QElapsedTimer last_load_;
    quint64 build_seq_ = 0;
    notes::SearchQuery query_;
    QString search_text_;
    QHash<int, QString> pending_origin_;     ///< Chat Mode run ids from notes.created events

    // Boards
    QVector<notes::BoardDef> boards_;
    int board_idx_ = 0;
    bool board_dirty_ = false;

    // Coalesced writes
    QHash<int, fincept::notes_board::NoteBoardMeta> meta_dirty_;
    QVector<fincept::notes_board::NoteBoardEvent> events_pending_;
    QHash<int, fincept::notes_board::NoteWallItem> wall_dirty_;
    qint64 wall_dirty_board_ = 0;
    quint64 wall_request_ = 0;
    quint64 history_request_ = 0;

    // Editor session (one per opened note)
    int editor_mode_ = 0;
    bool editor_open_ = false;
    int current_id_ = -1;
    quint64 session_ = 0;
    notes::IdSlot slot_;
    quint64 queued_revision_ = 0;
    quint64 saved_revision_ = 0;
    int saves_in_flight_ = 0;
    fincept::FinancialNote session_base_;
    fincept::FinancialNote session_original_;
    bool session_has_original_ = false;
    fincept::notes_board::NoteBoardMeta session_meta_; ///< board row of a new note (written with its first save)
    QHash<int, fincept::notes_board::NoteBoardMeta> pending_new_meta_; ///< rows of notes just created here
    QHash<quint64, QPointF> wall_pos_for_session_;    ///< where a card made on the wall goes
    QSet<quint64> discarded_sessions_;
    QVector<quint64> resolve_requests_;
    QSet<QString> dismissed_renames_;
    QString save_state_text_;
    int save_state_tone_ = 0;

    // Delete with undo
    QVector<notes::NoteEntry> pending_delete_;
    QSet<int> pending_delete_ids_;

    // Quotes
    notes::QuoteBook quotes_;
    QObject* quote_owner_ = nullptr;
    QStringList card_symbols_;   ///< visible cards' tickers (capped)
    QStringList editor_symbols_; ///< the open note's tickers
    QSet<QString> subscribed_;
    QSet<QString> dirty_quotes_;

    QVariantMap pending_restore_;
    bool has_pending_restore_ = false;
    // A deep link (open_section) that arrived before the library loaded.
    QString pending_link_section_;
    QString pending_link_focus_;
    bool has_pending_link_ = false;
    bool flushed_for_quit_ = false;
    bool syncing_selection_ = false;
    bool focus_mode_ = false;
    bool detail_visible_ = true;
    QList<int> detail_sizes_;

    // UI
    fincept::ui::ScreenCommandBar* bar_ = nullptr;
    QMenu* board_menu_ = nullptr;
    QMenu* template_menu_ = nullptr;
    QMenu* group_menu_ = nullptr;
    QMenu* export_menu_ = nullptr;
    QMenu* priority_menu_ = nullptr;
    QMenu* tag_menu_ = nullptr;
    QMenu* date_menu_ = nullptr;
    fincept::ui::KpiStrip* ribbon_ = nullptr;
    QSplitter* split_ = nullptr;
    QWidget* left_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    notes::NotesBoardModel* board_model_ = nullptr;
    notes::NotesBoardView* board_view_ = nullptr;
    QWidget* wall_page_ = nullptr;
    notes::NotesWallView* wall_view_ = nullptr;
    QList<QToolButton*> wall_btns_;
    QLabel* wall_hint_ = nullptr;
    notes::NotesTableModel* table_model_ = nullptr;
    QSortFilterProxyModel* table_proxy_ = nullptr;
    QTableView* table_view_ = nullptr;
    notes::NotesDetailPane* detail_ = nullptr;
    notes::NotesEditor* editor_ = nullptr; ///< owned by detail_
    fincept::ui::ScreenStatusBar* status_ = nullptr;
    QFrame* peek_ = nullptr;
    QLabel* peek_title_ = nullptr;
    QTextBrowser* peek_body_ = nullptr;
    int peek_id_ = 0;

    QTimer* autosave_timer_ = nullptr;
    QTimer* search_timer_ = nullptr;
    QTimer* reload_timer_ = nullptr;
    QTimer* build_timer_ = nullptr;
    QTimer* meta_timer_ = nullptr;
    QTimer* wall_timer_ = nullptr;
    QTimer* board_timer_ = nullptr;
    QTimer* quote_timer_ = nullptr;
    QTimer* flash_timer_ = nullptr;
    QTimer* subscribe_timer_ = nullptr;

    // notes.* events from other publishers (dashboard, MCP tools, Chat Mode) while visible.
    QList<EventBus::HandlerId> event_subs_;
};

} // namespace fincept::screens
