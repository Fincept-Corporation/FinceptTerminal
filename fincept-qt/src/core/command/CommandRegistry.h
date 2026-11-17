#pragma once
// CommandRegistry — the public list of everything the header search box can find
// and do, apart from instruments (the box asks InstrumentResolver itself). Qt Core
// only: any screen or service can contribute rows without touching the box.
//
// ── Static rows: register_item / register_items ─────────────────────────────────
//   A row is a CommandItem. Re-registering an id replaces the row; unregister_owner()
//   removes every row (and provider) a screen added. Typical rows:
//     a sub-tab   {id "backtesting.runs", title tr("Runs"), keywords {"history"}, group kScreen,
//                  screen_id "backtesting", section "runs"}      -> the box publishes
//                  nav.switch_screen {screen_id, section, focus, new_panel}; the screen
//                  must implement ISectionLinked::open_section(section, focus) for the
//                  link to land on the tab (else the screen opens on its current tab).
//     an action   {id "notes.new", title tr("New note"), group kAction, run [](ctx) {...}}
//     a setting   {id "settings.appearance.theme", group kSetting, screen_id "settings",
//                  section "appearance", focus "theme"}
//   Rows registered once (constructor, static init) stay until unregistered — they do
//   NOT need the screen to exist: the box navigates first, then the screen gets the
//   section. Register sub-tabs at start-up (a free function called from the screen's
//   .cpp file scope via a static registrar, or from the service that owns the data).
//
// ── Dynamic rows: register_provider ──────────────────────────────────────────────
//   For sets that change or are too large to list (saved items, notebooks, chat runs):
//   the box calls the provider with the query and a request id; the provider answers
//   through the ResultSink — at once, later, several times (each call REPLACES that
//   provider's rows for the request), from any thread. Cancelled / superseded requests
//   are dropped by the registry; `cancel` (optional) lets the provider stop work.
//   A provider never blocks the GUI thread: do I/O on a worker and call the sink there.
//
// ── Threads ───────────────────────────────────────────────────────────────────────
//   register_* / unregister_* / items() / query_providers() / cancel(): any thread
//   (calls from another thread are re-posted to the registry's thread, the GUI
//   thread). ResultSink: any thread, any number of times. RunFn: always called on
//   the GUI thread by the box. changed() is emitted on the GUI thread.
//
// ── Privacy ────────────────────────────────────────────────────────────────────────
//   CommandContext::query and a provider's `query` are what the user typed — they can
//   hold account or order details. Never log them (log counts and timings only).

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

namespace fincept::command {

/// Group ids (CommandItem::group). Stable strings; the box shows one heading each.
namespace groups {
inline constexpr char kInstrument[] = "instrument"; ///< symbols (the box's own InstrumentResolver group)
inline constexpr char kScreen[] = "screen";         ///< screens, their sub-tabs and panels
inline constexpr char kAction[] = "action";         ///< things that do something (dialogs, toggles, "new …")
inline constexpr char kSetting[] = "setting";       ///< a Settings section or field
inline constexpr char kSaved[] = "saved";           ///< the user's own things: watchlists, notes, layouts, runs …
inline constexpr char kData[] = "data";             ///< connectors, indicators, templates
inline constexpr char kHelp[] = "help";             ///< docs pages, how-tos, Ask AI
} // namespace groups

/// Agent-risk kinds a row may carry (core/agent/AgentRiskMarkers.h, rules in mcp/core/UiRisk.h).
namespace risk {
inline constexpr char kOrder[] = "order";
inline constexpr char kMoney[] = "money";
inline constexpr char kSpend[] = "spend";
inline constexpr char kDelete[] = "delete";
inline constexpr char kAccount[] = "account";
inline constexpr char kCode[] = "code";
inline constexpr char kFile[] = "file";
} // namespace risk

/// Handed to RunFn on every activation. Built by the box.
struct CommandContext {
    QPointer<QObject> window;  ///< the WindowFrame the box lives in (qobject_cast in app code); may be null
    QString item_id;           ///< the activated row
    QString query;             ///< what was typed — NEVER log it
    QVariantMap args;          ///< the row's args, overlaid by the phrase's (symbol, quantity, name …)
    bool new_panel = false;    ///< Ctrl+Enter: open beside the current panel instead of replacing it
};

using RunFn = std::function<void(const CommandContext&)>;

/// One row. Aggregate: designated initialisers in this field order.
struct CommandItem {
    QString id;            ///< unique + stable: "<owner>.<thing>" ("backtesting.runs", "settings.appearance.theme")
    QString title;         ///< translated text of the row ("Runs", "Theme")
    QStringList keywords;  ///< extra words that find it (synonyms; English and translated): {"history", "results"}
    QString group;         ///< groups::kScreen / kAction / kSetting / kSaved / kData / kHelp
    QString subtitle;      ///< plain words, WHAT ENTER DOES ("Opens the Runs tab of Backtesting"); empty = generated
    QString screen_id;     ///< screen to open; empty for pure actions
    QString section;       ///< sub-tab / panel inside screen_id ("runs", "panel:fred", "module:garch/view")
    QString focus;         ///< field / row to focus inside the section (optional)
    QVariantMap args;      ///< extra payload: copied into CommandContext::args and the nav.switch_screen event
    QString agent_risk;    ///< "" or a risk:: kind — what activating the row commits (agents ask the user first)
    RunFn run;             ///< optional; empty + screen_id -> the box publishes nav.switch_screen
    int priority = 0;      ///< -100..100: tie-break inside a match tier (never lifts a weaker match over a better one)
    // ── optional extras (after the fields above; leave out freely) ──
    QString owner;         ///< for unregister_owner(); empty = the id's first segment ("backtesting")
    QString context;       ///< muted text before the title, e.g. the parent screen "Backtesting"
    QString shortcut;      ///< key hint on the right ("F5", "Ctrl+L")
    QStringList aliases;   ///< short codes typed exactly ("bt", "wl") — rank like the title itself
};

/// What a provider hands back: the rows for one request (REPLACES earlier rows of that request), final=true on
/// the last call. Any thread. Rows beyond the provider's cap are dropped.
using ResultSink = std::function<void(const QList<CommandItem>& items, bool final)>;
/// Asked with the query and a request id; answer through the sink (now or later, any thread).
using ProviderFn = std::function<void(const QString& query, quint64 request_id, ResultSink sink)>;
/// Optional: stop work for a request the box no longer needs (a newer query, the popup closed).
using CancelFn = std::function<void(quint64 request_id)>;

struct ProviderSpec {
    QString id;            ///< unique ("notes.saved"); re-registering replaces
    QString group;         ///< groups:: id its rows go under
    ProviderFn query;      ///< required
    CancelFn cancel;       ///< optional
    QString owner;         ///< for unregister_owner(); empty = the id's first segment
    int cap = 8;           ///< rows kept per request
    int min_chars = 1;     ///< not asked for shorter queries; 0 = asked for the empty box too
    bool slow = false;     ///< I/O / network: asked after the box's short debounce, under a loading row
};

class CommandRegistry : public QObject {
    Q_OBJECT
  public:
    static CommandRegistry& instance();
    /// A private registry (tests). Lives on the creating thread.
    explicit CommandRegistry(QObject* parent = nullptr);
    ~CommandRegistry() override;

    // ── rows ──
    /// false (and nothing changes) for an empty id, title or group.
    bool register_item(CommandItem item);
    /// Registers each valid row; one changed() for the batch. Returns how many were accepted.
    int register_items(const QList<CommandItem>& items);
    bool unregister_item(const QString& id);
    /// Removes every row AND provider of `owner`. Returns the number removed.
    int unregister_owner(const QString& owner);
    /// Snapshot, in registration order.
    QList<CommandItem> items() const;
    bool contains(const QString& id) const;
    /// The row, or an item with an empty id.
    CommandItem item(const QString& id) const;
    int size() const;
    /// Bumps on every change of rows or providers (cheap "did anything change" check).
    quint64 revision() const;

    // ── providers ──
    bool register_provider(ProviderSpec spec);
    /// Short form: returns the generated provider id ("<owner or group>.provider.<n>").
    QString register_provider(const QString& group, ProviderFn fn, const QString& owner = QString(), int cap = 8);
    bool unregister_provider(const QString& id);
    QStringList provider_ids() const;

    // ── driving providers (the search box, tests) ──
    /// One delivery: a provider's current rows for `request_id` (already capped), on the registry's thread.
    using Delivery = std::function<void(quint64 request_id, const QString& provider_id, const QString& group,
                                        const QList<CommandItem>& items, bool final)>;
    /// Asks every provider whose min_chars fits `query` (slow ones only when `include_slow`). Deliveries are
    /// queued (never re-entrant) and stop when `receiver` is destroyed or the request is cancelled. Returns the
    /// request id (0 when no provider was asked). `asked` (optional) receives the provider ids asked.
    quint64 query_providers(const QString& query, QObject* receiver, Delivery deliver, bool include_slow = true,
                            QStringList* asked = nullptr);
    /// Drop a request: later deliveries are discarded; each asked provider's CancelFn runs once.
    void cancel(quint64 request_id);
    /// Requests still open (tests).
    int open_requests() const;

  signals:
    /// Rows or providers changed (coalesced per call).
    void changed();

  private:
    struct Impl;
    Impl* d_;

    void deliver_from_sink(quint64 request_id, const QString& provider_id, QList<CommandItem> items, bool final);
    void finish_if_done(quint64 request_id);
};

} // namespace fincept::command

Q_DECLARE_METATYPE(fincept::command::CommandItem)
