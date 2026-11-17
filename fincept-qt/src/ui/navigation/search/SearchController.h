#pragma once
// SearchController — turns what is typed into the popup's rows and runs the picked
// row. One per search box (window); the index, the registry and the usage store are
// shared (SearchSources).
//
//   typing  -> phrase grammar (CommandPhrase) + local index (synchronous, every keystroke)
//           -> registry providers (fast at once, slow ones after a short debounce)
//           -> instruments through InstrumentResolver (local index first, backend after; ~90 ms
//              debounce; a newer search drops the older) under a loading row
//   rows    -> ResultLayout: groups ordered by their best hit; rows already on screen keep their
//              place when a slower group arrives; the top hit is preselected
//   Enter   -> SearchActions / SearchInstruments; when Enter beats the symbol lookup on a
//              ticker-shaped query, it waits for it (<= 1.5 s) instead of opening something else
//
// Never logs the query: counts and timings only.

#include "core/command/CommandIndex.h"
#include "core/command/CommandPhrase.h"
#include "core/instrument/Instrument.h"
#include "services/markets/MarketSearchService.h"
#include "ui/navigation/search/SearchActions.h"
#include "ui/navigation/search/SearchRow.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVector>

namespace fincept::ui::search {

class SearchController : public QObject {
    Q_OBJECT
  public:
    explicit SearchController(QObject* parent = nullptr);
    ~SearchController() override;

    void set_window(QWidget* window) { window_ = window; }
    /// The text changed (or the box opened): rebuild the rows. "" = the empty-box lists.
    void set_query(const QString& text);
    /// Same text, sources changed (registry / saved items): rebuild in place.
    void refresh();
    /// Run the current text again from scratch (the popup re-opens on text it had shown).
    void rerun();
    /// Stop everything in flight (the popup closed).
    void cancel_all();

    /// Run a row: action -1 = its default, else an index into SearchRow::actions.
    void activate(const QString& key, int action, Qt::KeyboardModifiers modifiers);
    /// Enter on the current row; may wait for a pending symbol lookup (see the header comment).
    void accept(const QString& current_key, Qt::KeyboardModifiers modifiers, bool user_moved);
    /// A row's actions for the Tab / right-click menu (row actions, Open in new panel, Pin / Unpin).
    struct MenuAction {
        QString label;
        QString agent_risk; ///< kind for agents ("order" for a ticket)
    };
    QList<MenuAction> menu_entries(const QString& key) const;
    QStringList menu_actions(const QString& key) const;
    /// Run the n-th entry of menu_actions(key).
    void activate_menu(const QString& key, int index, Qt::KeyboardModifiers modifiers);
    bool waiting_for_symbols() const { return pending_active_; }
    QString text() const { return text_; }

    /// Group order when nothing ranks them.
    static QStringList default_group_order();
    /// Heading of a group id (translated).
    static QString group_title(const QString& group);

  signals:
    void rows_changed(const QList<fincept::ui::search::SearchRow>& rows, const QString& preselect_key);
    void navigate_requested(const QString& screen_id);
    void dock_requested(const QString& verb, const QString& primary, const QString& secondary);
    void toolbar_action_requested(const QString& action_id);
    void fill_requested(const QString& text);
    void status(const QString& text);
    /// A row ran: close the popup, clear the box (the target takes focus).
    void finished();

  private:
    struct Entry {
        enum class Kind { Item, Instrument, Ask, Suggestion, More, Dock, Recent };
        Kind kind = Kind::Item;
        command::CommandItem item;
        core::instrument::Instrument inst;
        QString default_action;  ///< instrument rows: the phrase's action ("" = open)
        QVariantMap args;        ///< phrase values (quantity, limit_price, watchlist, peers, strategy …)
        QString text;            ///< Ask / Suggestion text; More: the group; Dock: the verb
        QString a;               ///< Dock: primary screen
        QString b;               ///< Dock: secondary screen
        QStringList actions;     ///< action ids parallel to SearchRow::actions
        QString recent_id;       ///< Recent: the usage entry id
    };
    struct Cand {
        SearchRow row;
        Entry entry;
        qint64 score = 0;
    };

    void rebuild();
    void build_empty_state();
    void collect_local(const QString& text, const QSet<QString>& only_groups, QHash<QString, QList<Cand>>* out) const;
    void collect_phrases(QHash<QString, QList<Cand>>* out) const;
    void collect_providers(QHash<QString, QList<Cand>>* out) const;
    void collect_instruments(QHash<QString, QList<Cand>>* out, qint64* best) const;
    void start_providers(bool include_slow);
    void plan_instruments();
    void fire_instruments();
    void fire_legacy_market();
    void run_entry(const Entry& e, const QString& action_id, bool new_panel);
    void record(const Entry& e) const;
    void release_pending_enter();
    actions::Ctx ctx(bool new_panel) const;
    Cand item_cand(const command::CommandItem& item, const command::rank::Match& m, qint64 score) const;
    QString first_selectable_key(const QList<SearchRow>& rows) const;

    QPointer<QWidget> window_;
    QString text_;
    QString local_text_; ///< what the local index is asked (text minus a scope prefix / phrase words)
    command::rank::Query query_;
    QList<command::phrase::Phrase> phrases_;
    QString scope_; ///< "" | action | instrument | help
    bool same_query_ = false;
    bool local_exact_ = false; ///< the best local row is an exact match (Enter need not wait for symbols)

    QHash<QString, Entry> entries_; ///< row key -> what it runs (rebuilt with the rows)
    QSet<QString> expanded_;
    command::layout::Previous previous_;
    QList<SearchRow> shown_;

    // providers
    quint64 provider_request_ = 0;
    QHash<QString, QPair<QString, QList<command::CommandItem>>> provider_rows_; ///< provider -> (group, rows)
    QSet<QString> provider_pending_;
    QTimer* slow_timer_ = nullptr;

    // instruments
    QTimer* inst_timer_ = nullptr;
    QString inst_text_;
    QString inst_action_;
    QVariantMap inst_args_;
    QVector<core::instrument::AssetClass> inst_classes_;
    QVector<core::instrument::Instrument> inst_rows_;
    bool inst_wanted_ = false;  ///< this query searches instruments
    bool symbol_intent_ = false; ///< a symbol phrase (ticket, chart, compare, FX, crypto, /type, $): the symbol IS the answer
    bool inst_waiting_ = false; ///< scheduled or in flight, nothing delivered yet
    bool inst_offline_ = false;
    quint64 inst_serial_ = 0;
    QElapsedTimer inst_clock_;
    QString legacy_type_; ///< "bond" / "economic": MarketSearchService (the old /types)
    QList<services::MarketSearchService::Item> legacy_rows_;
    bool legacy_wired_ = false;

    bool pending_active_ = false;
    Qt::KeyboardModifiers pending_mods_;
    QTimer* pending_timer_ = nullptr;
};

} // namespace fincept::ui::search
