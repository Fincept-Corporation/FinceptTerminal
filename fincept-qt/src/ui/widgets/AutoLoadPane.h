#pragma once
// AutoLoadPane — the one "never blank" mechanism for a view whose content comes
// from an action (a calculation, a fetch, a script run). Every tab / sub-tab
// result area uses it instead of its own first-show / cache / stale-reply code:
//
//   * First show → the last result appears at once (memory, else a host store
//     set_remembered_source(), else the persistent cache under set_cache_key())
//     with its "as of" time, and
//       - Policy::AutoRun (free, local or cheap — "class A/B"): the default
//         action runs when nothing is remembered, or quietly refreshes a
//         remembered result older than set_max_age() (default a day; 0 = on
//         every show; -1 = never). Automatic runs pass the app-wide
//         AutoLoadGate (2 at a time; a waiting pane shows "Queued…").
//       - Policy::Manual (costs credits, rate-limited third party, heavy, sends
//         a prompt — "class C"): nothing runs by itself; with no remembered
//         result the designed preview (preview(): what it returns, the inputs,
//         the cost, RUN) is shown; a remembered result older than the max age
//         gets an "out of date · REFRESH" notice.
//   * set_requirements() (API key, LLM model, Python, broker, sign-in, credits —
//     ui/widgets/Requirements.h) is checked on every show before anything
//     runs: missing → a "needs setup" page naming exactly what is missing, with
//     a button that opens the right Settings section.
//   * set_signature(inputs hash): a remembered result made from other inputs
//     shows "Inputs changed — RUN AGAIN"; it is never silently re-run.
//   * Hide while the DEFAULT action runs or waits → it is cancelled and runs
//     again on the next show. A run the user started keeps going.
//   * Every run gets a request id; deliver()/fail() with an older id are
//     dropped, so a stale answer never draws.
//   * While running: a skeleton (nothing on screen yet), CANCEL after 3 s,
//     set_timeout() → "Timed out" + RETRY. fail() with nothing on screen → an
//     error page with RETRY; with a result on screen the result stays and the
//     status says the refresh failed. deliver_partial() + set_notice() show
//     what arrived and what did not ("3 of 5 series loaded — FRED needs a key").
//
//   auto* pane = new ui::AutoLoadPane(parent);                 // in its final parent
//   auto* view = new MyResultWidget(pane->content_parent());    // built in place
//   pane->set_content(view);
//   pane->set_cache_key(QStringLiteral("derivatives/bond_price"));
//   pane->set_start([this, pane](quint64 id) {                  // start the work (async or not)
//       run_async(inputs(), [pane, id](QJsonValue out) { pane->deliver(id, out); });
//   });
//   pane->set_render([view](const QJsonValue& p, const QDateTime&, bool) { view->show(p); });
//   pane->set_status_label(as_of_label);                         // "as of 14:02 · refreshing…"
//   connect(calc_button, &QPushButton::clicked, pane, &ui::AutoLoadPane::run);
//
// One pane can serve several views that share a result area (sub-tabs of a
// module, endpoints of a catalog, analyzer + inputs): set_view(key) switches
// the pane to view `key` — each key has its own remembered result, "as of",
// signature, default-run state and persistent cache entry (cache key + "/" +
// view key, long keys hashed). The policy can differ per view:
// set_view(key, Policy::Manual). Configure preview() for a view before
// switching to it.
//
// deliver()/fail() must be called on the GUI thread (post back from workers).
// The payload is a QJsonValue so it can be cached; render() turns it into
// widgets. Persisted payloads are capped (set_persist_limit) and serialised
// off the GUI thread. Styling: the pages are the shared EmptyState and
// LoadingSkeleton (theme-driven; no theme_changed handling needed).
//
// Feed mode — data PUSHED by a hub / adapter (DataHub topics, streams) rather
// than started by the pane: no StartFn, no gate, no persistent cache. The host
// reports what its source does and the pane shows the matching state:
//
//   pane->set_feed_mode(true);
//   pane->set_render([=](const QJsonValue& p, const QDateTime&, bool) { table->set_rows(p); });
//   pane->set_feed_subscription([=] { hub.subscribe(pane, topic, ...); },     // shown, requirements met, RETRY
//                               [=] { hub.unsubscribe(pane, topic); });       // hidden / view left
//   pane->set_feed_stale_after(30);                    // 30 s without an update → stale marker
//   pane->feed_waiting();                              // subscribed, nothing yet → skeleton
//   pane->feed_data(json, tick_time);                  // render + "as of" (+ restarts the stale clock)
//   pane->feed_error(tr("Exchange disconnected"), raw); // error page (RETRY → re-subscribe) or notice
//   pane->feed_idle(tr("Market closed"), tr("Opens 09:15 IST."));   // designed neutral state
//
// The requirement gate (setup page / notice, no subscription while unmet),
// the host notice, per-view memory (set_view) and leave-on-hide (unsubscribe)
// apply as for runs. The feed_*() calls are GUI-thread only.

#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/Requirements.h"

#include <QDateTime>
#include <QHash>
#include <QJsonValue>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace fincept::ui {

class LoadingOverlay;
class LoadingSkeleton;

class AutoLoadPane : public QWidget {
    Q_OBJECT
  public:
    enum class Policy { AutoRun, Manual };
    enum class State { Idle, Loading, Content, Preview, Failed, NeedsSetup, Queued };

    using StartFn = std::function<void(quint64 request_id)>;
    using CancelFn = std::function<void(quint64 request_id)>;
    using RenderFn = std::function<void(const QJsonValue& payload, const QDateTime& as_of, bool from_cache)>;
    /// A remembered result kept by the host (e.g. a service's own cache) for `view`; false = none.
    using RememberedFn = std::function<bool(const QString& view, QJsonValue* payload, QDateTime* as_of)>;
    /// Same, also giving the inputs signature the result was made from ("" = unknown),
    /// so "Inputs changed" works for host-kept results too.
    using RememberedSignedFn =
        std::function<bool(const QString& view, QJsonValue* payload, QDateTime* as_of, QString* signature)>;
    /// A custom requirement check (in addition to set_requirements()).
    using RequirementCheckFn = std::function<RequirementStatus()>;
    /// A button handler of a host-built page (failure page actions, the setup page's secondary action).
    using ActionFn = std::function<void()>;
    /// A clicked detail row of a host-built failure page: `index` into Spec::details, `key` its key.
    using DetailFn = std::function<void(int index, const QString& key)>;
    /// Feed mode: the pane wants the source; `is_retry` = asked again by RETRY /
    /// RECONNECT / run() / invalidate() (reconnect, do not reuse a dead stream);
    /// false = first show, view switch, requirements met again.
    using SubscribeFn = std::function<void(bool is_retry)>;

    /// Default largest payload kept in the persistent cache (compact JSON).
    static constexpr int kMaxPersistBytes = 256 * 1024;
    /// Hard ceiling for set_persist_limit().
    static constexpr int kPersistCeilingBytes = 4 * 1024 * 1024;
    /// CANCEL appears under the skeleton after this long.
    static constexpr int kCancelRevealMs = 3000;

    explicit AutoLoadPane(QWidget* parent = nullptr);
    explicit AutoLoadPane(Policy policy, QWidget* parent = nullptr);
    ~AutoLoadPane() override;

    // ── Content ──
    /// Parent to build the content widget under (no re-parenting later).
    QWidget* content_parent() const;
    /// The view's real content (results, tables, charts). Shown once a result is rendered.
    void set_content(QWidget* content);
    QWidget* content() const { return content_; }

    // ── Policy and views ──
    void set_policy(Policy policy);
    Policy policy() const { return policy_; }
    /// Switch the pane to view `key` (keeps every view's remembered result).
    void set_view(const QString& key);
    void set_view(const QString& key, Policy policy);
    /// Same, with the view's inputs signature (set_signature) set BEFORE the
    /// switch: the first look at the view — "Inputs changed" and its default
    /// run — already uses it (hosts whose selectors fill after the switch).
    void set_view(const QString& key, Policy policy, const QString& signature);
    QString view() const { return key_; }
    /// Per-view max age (seconds, as set_max_age) — views without one use the pane's.
    void set_view_max_age(const QString& view, int seconds);
    void clear_view_max_age(const QString& view);
    /// Per-view skeleton shape while that view loads — views without one use the
    /// shape skeleton() was given (skeleton()->set_shape).
    void set_view_skeleton(const QString& view, LoadingSkeleton::Shape shape);
    void clear_view_skeleton(const QString& view);
    /// Agent approval (since round 6 (7)): the "fincept_agent_risk" kind ("spend",
    /// …) put on the pane's OWN buttons that start or re-run its load — preview RUN,
    /// error-page RETRY (a host-built failure page's primary only when it is the
    /// default RETRY), needs-setup RETRY (secure storage unreadable), and the notice
    /// strip's RUN AGAIN / RETRY / REFRESH / RECONNECT. Never on CANCEL, DETAILS,
    /// OPEN SETTINGS…, SIGN IN, host fixes or secondary buttons. Empty = the
    /// default rule: "spend" when set_cost_credits() > 0 and the account's plan
    /// does not include that category (re-evaluated when the plan changes), else
    /// no marker. An explicit kind always wins (a pane that never spends simply
    /// sets no cost). Per view: the view's kind, else the pane's.
    void set_agent_risk(const QString& kind);
    QString agent_risk() const { return agent_risk_; }
    void set_view_agent_risk(const QString& view, const QString& kind);
    void clear_view_agent_risk(const QString& view);
    /// The kind in effect for the current view (explicit or by the rule); empty = none.
    QString effective_agent_risk() const;

    // ── Remembering ──
    /// Persist the last delivered payload under `key` (CacheManager, category
    /// "autoload") for `ttl_days`. Empty = memory only.
    void set_cache_key(const QString& key, int ttl_days = 30);
    /// Largest payload persisted (bytes of compact JSON, up to kPersistCeilingBytes); 0 = memory only.
    void set_persist_limit(int bytes);
    /// Consulted before the persistent cache on a view's first show.
    void set_remembered_source(RememberedFn source);
    /// Same, with the signature of the inputs the remembered result was made from
    /// (replaces a source set with set_remembered_source()).
    void set_remembered_signed_source(RememberedSignedFn source);
    /// G1 — refresh / out-of-date threshold for a remembered result, in seconds:
    /// 0 = refresh on every show, -1 = never automatically. Default a day.
    void set_max_age(int seconds);
    int max_age() const { return max_age_s_; }
    /// G2 — signature of the inputs the next run would use, for the current view
    /// (ui::autoload::signature_of(inputs), or InputMemory::signature()). A
    /// remembered result made from another signature shows "Inputs changed".
    /// Called from inside StartFn, it also describes the run being started (its
    /// result carries this signature).
    void set_signature(const QString& signature);
    QString signature() const;
    /// Memory budget of the results kept per view (estimated bytes; default
    /// autoload::kDefaultMemoryBudgetBytes = 32 MB; 0 or less = no byte cap, the
    /// 48-view cap stays). Over it, the least recently used results are dropped
    /// from memory — those that come back without a run first (persistent cache,
    /// host copy, pushed data), then AutoRun ones; never the view on screen, a
    /// running / queued one, or a Manual result that cannot be reloaded. A dropped
    /// view comes back through the normal first-show path on its next visit: the
    /// remembered copy (no run, no gate slot), else — AutoRun — its default run
    /// like any first show; the drop never counts as that visit's automatic attempt.
    void set_memory_budget(qint64 bytes);
    qint64 memory_budget() const { return memory_budget_; }
    /// Size estimate of a payload (default autoload::approx_json_bytes); a host
    /// that knows its sizes (rows × columns) can answer cheaper.
    using SizeFn = std::function<qint64(const QJsonValue& payload)>;
    void set_size_estimator(SizeFn estimator);
    /// Estimated bytes of the results held now (as last estimated).
    qint64 memory_in_use() const;

    // ── The action ──
    void set_start(StartFn start);
    void set_cancel(CancelFn cancel);
    void set_render(RenderFn render);
    /// Delay between the show (or invalidate()) and the default run. Default 0
    /// (next event-loop turn); ~300 ms where users flick between views.
    void set_auto_delay(int ms);
    /// G5 — give up after `seconds` (0 = never, the default): cancel + "Timed out" + RETRY.
    void set_timeout(int seconds);
    /// G5 — show CANCEL after kCancelRevealMs (default on): under the skeleton, and
    /// on the dimmed result while the user's re-run (run()) replaces it.
    void set_cancellable(bool cancellable);
    /// G7 — priority of this pane's automatic runs in the app-wide gate (higher first; default 0).
    void set_gate_priority(int priority);

    // ── Requirements (G4) ──
    /// A requirement may carry the host's own fix (Requirement::with_action(
    /// tr("BUILD FROM YAHOO"), fn)): the setup page's primary button then runs it
    /// and the standard Settings link moves to the secondary button. A custom check
    /// (set_requirement_check) can set RequirementStatus::missing.action the same way.
    void set_requirements(const QList<Requirement>& requirements);
    void set_requirement_check(RequirementCheckFn check);
    /// A run costs this many credits: checked as a requirement (balance known and too low → setup page).
    /// `credits` is the LIST price; with `plan_category` (a billing::PlanAccess
    /// price key such as "maritime") the check uses what the account's plan
    /// really pays — nothing for a category included with Pro.
    void set_cost_credits(double credits, const QString& plan_category = QString());
    /// Re-evaluate now (e.g. after the user saved a key and came back). The setup
    /// page's RETRY (a requirement that could not be checked: RequirementStatus::retry)
    /// calls this.
    void recheck_requirements();
    /// A second button on the "needs setup" page (e.g. "USE SAMPLE DATA"), shown
    /// whatever requirement is missing; it replaces RequirementStatus::secondary_text.
    /// Empty text removes it.
    void set_setup_secondary_action(const QString& text, ActionFn action);

    // ── Look ──
    /// Caption of the skeleton / overlay while running ("Pricing the bond…").
    void set_loading_text(const QString& text);
    /// Optional label the pane keeps up to date: "Loading…", "Queued…",
    /// "as of 14:02", "as of 14:02 · refreshing…", "as of 14:02 · refresh failed",
    /// "as of 16 Nov 2026 14:02 · out of date".
    void set_status_label(QLabel* label);
    /// G3 — a one-line notice strip above the content (partial results, caveats).
    /// Empty text hides it; `action_text` adds a button that emits notice_action().
    /// The pane's own notices ("Inputs changed", "Refresh failed", needs setup,
    /// out of date) have their own strip above this one: both show at once.
    void set_notice(const QString& text, EmptyState::Tone tone = EmptyState::Tone::Warning,
                    const QString& action_text = QString());
    void clear_notice() { set_notice(QString()); }

    LoadingSkeleton* skeleton() const { return skeleton_; }
    /// The designed preview / empty state (Manual views, a declined run).
    /// Its primary action calls run(); configure texts and the secondary action freely.
    EmptyState* preview() const { return preview_; }
    /// The preview of one view when several views share the pane: applied to
    /// preview() whenever that view shows it (views without one keep whatever
    /// preview() was configured with).
    void set_view_preview(const QString& view, const EmptyState::Spec& spec);
    void clear_view_preview(const QString& view);

    // ── Runs ──
    /// Run the action now (RUN / CALCULATE / REFRESH): bypasses the gate and
    /// supersedes a run in flight. With a result on screen it stays visible
    /// (dimmed) until the new one arrives; otherwise the skeleton shows.
    /// Requirements are checked first on every run path (this, RETRY, RUN
    /// AGAIN, the default run): one missing → the setup page (or, over a
    /// result, its notice) and nothing starts.
    void run();
    /// Cancel a run in flight (or waiting in the gate); what is on screen stays.
    void cancel();
    /// The remembered result no longer matches the inputs and the host wants a
    /// re-run: an AutoRun view runs again (after the auto delay) while visible,
    /// or on its next show.
    void invalidate();
    /// Forget the remembered result (memory and persistent cache).
    void clear_result();

    /// Completion of request `request_id` (GUI thread). Stale ids are ignored.
    void deliver(quint64 request_id, const QJsonValue& payload);
    /// Same, for a payload computed earlier than now (e.g. a 20-minute-old shared copy).
    void deliver(quint64 request_id, const QJsonValue& payload, const QDateTime& as_of);
    /// G3 — part of the answer: rendered at once, the run goes on.
    void deliver_partial(quint64 request_id, const QJsonValue& payload);
    /// `title` defaults to "Could not load" (e.g. "Check the inputs" for a validation failure).
    void fail(quint64 request_id, const QString& message, const QString& title = QString());
    /// Same, with technical `details` (a raw traceback, a long server message): the
    /// page shows the plain-language `message`; `details` sit behind its DETAILS
    /// button — collapsed, selectable, copyable, passed through log_scrub() before
    /// display (EmptyState::set_technical_details). Not written to the log here.
    void fail(quint64 request_id, const QString& message, const QString& title, const QString& details);
    /// A failure with a host-built page (its own glyph, title, text and buttons,
    /// e.g. UPGRADE / VIEW PLANS on a plan limit). `primary` / `secondary` run
    /// when the page's buttons are pressed; an empty `primary` means RETRY
    /// (run()). With a result on screen the result stays, as with fail(), and
    /// the page is kept for when the view has nothing else to show.
    void fail(quint64 request_id, const EmptyState::Spec& page, ActionFn primary, ActionFn secondary = ActionFn());
    /// Same, with actionable detail rows (since round 5): the page's `details` rows
    /// become buttons (Tab / Enter / Space, focus frame — Spec::details_clickable is
    /// set for you) and a click calls `on_detail(index, key)`, then emits
    /// error_detail_clicked(). E.g. one row per symbol that failed → open it.
    /// `details_agent_risk` (since round 7): the "fincept_agent_risk" kind of every row
    /// button (e.g. "order" when a row re-sends a rejected order); empty = the page's own
    /// Spec::details_agent_risk. Per-row kinds go in page.details_agent_risks and win
    /// over it. The pane's set_agent_risk() kind never covers these rows (host actions).
    void fail(quint64 request_id, const EmptyState::Spec& page, ActionFn primary, ActionFn secondary,
              DetailFn on_detail, const QString& details_agent_risk = QString());
    bool is_current(quint64 request_id) const { return request_id != 0 && request_id == current_id_; }
    /// The run `request_id` was started automatically (first show, quiet refresh,
    /// invalidate) rather than by the user's RUN — for StartFn to tell them apart
    /// (e.g. use a cached answer for an automatic run only). False for an old id.
    bool is_automatic_run(quint64 request_id) const { return is_current(request_id) && current_auto_; }
    /// Progress of run `request_id` (since round 6; GUI thread; stale ids ignored):
    /// `fraction` 0..1 draws a determinate bar on the skeleton (still with motion
    /// off — it is not an animation) and `text` under it ("Downloading 3 of 12
    /// series"); a negative / NaN fraction = indeterminate (text only). Over a
    /// result being re-run by the user, the dimmed overlay's caption carries the
    /// text and percentage. Kept while the run lasts (also across hide / show),
    /// cleared when it ends.
    void set_progress(quint64 request_id, double fraction, const QString& text = QString());

    // ── Feed mode: data pushed by a hub / adapter (since 2026-11-17 round 3) ──
    /// The pane's data comes from a source the host subscribes to, not from a
    /// run: nothing is started (StartFn / gate / policy / persistent cache are
    /// not used; a remembered source still shows a last copy at once). A run in
    /// flight is cancelled. Any feed_*() call switches the pane to feed mode.
    /// run() and invalidate() then mean "re-subscribe"; cancel() does nothing.
    void set_feed_mode(bool on);
    bool feed_mode() const { return feed_mode_; }
    /// Called when the pane wants the data (shown with requirements met, RETRY /
    /// RECONNECT, a view switch) and when it no longer does (hidden, view left,
    /// requirements lost). Optional: a host that keeps its subscription itself
    /// leaves them empty — but RETRY / RECONNECT are offered only with `subscribe`.
    /// The pane does not call `unsubscribe` from its destructor.
    void set_feed_subscription(ActionFn subscribe, ActionFn unsubscribe = ActionFn());
    /// Same, telling `subscribe` whether it is a retry (since round 6). A lambda
    /// taking no argument picks the form above, one taking a bool this one.
    void set_feed_subscription(SubscribeFn subscribe, ActionFn unsubscribe = ActionFn());
    /// Data older than this (seconds since the last feed_data()) gets a stale
    /// marker: status "· no update for over 30 s", a neutral notice, and
    /// feed_stale_changed(true) — never an error. 0 (default) = never.
    void set_feed_stale_after(int seconds);
    int feed_stale_after() const { return feed_stale_after_s_; }
    /// Subscribed (again), nothing yet: skeleton ("Waiting for data…", or
    /// set_loading_text()); with data on screen: "· connecting…".
    void feed_waiting();
    /// An update for the current view: rendered at once when visible (on the
    /// next show otherwise); `as_of` = the source's time (invalid = now). Clears
    /// an error / idle state and restarts the stale clock.
    void feed_data(const QJsonValue& payload, const QDateTime& as_of = QDateTime());
    /// Same, for data the HOST keeps (an option-chain object, a candle buffer —
    /// since round 6): the pane only switches state, "as of" and the stale clock,
    /// and calls the RenderFn with a NULL QJsonValue — draw from your own copy.
    /// Hidden: drawn on the next show. Host-held data is not counted against the
    /// memory budget and never dropped by it. (No QVariant overload of feed_data:
    /// it would make calls with QJsonObject / QJsonArray / bool arguments ambiguous.)
    void feed_data_ready(const QDateTime& as_of = QDateTime());
    /// Back to "nothing yet" for the current view (since round 6): the waiting
    /// skeleton, data and markers dropped, stale clock stopped, status label
    /// empty until the next report. The subscription stays (no unsubscribe).
    void feed_reset();
    /// The source reports an error. Nothing on screen: error page (`title`,
    /// default "Could not load"; the plain `message`; `details` behind DETAILS,
    /// scrubbed; RETRY → re-subscribe). With data: it stays, notice "Live updates
    /// stopped — <message>" + RETRY. Not logged by the pane.
    void feed_error(const QString& message, const QString& details = QString(), const QString& title = QString());
    /// The source is idle on purpose (market closed, no session). Nothing on
    /// screen: a designed neutral page (`glyph`, default "○"; `title`; `message`).
    /// With data: it stays, neutral notice "<title> — <message>".
    void feed_idle(const QString& title, const QString& message = QString(), const QString& glyph = QString());
    /// The current view's data has the stale marker.
    bool feed_stale() const;

    // ── Queries ──
    State state() const { return state_; }
    /// A run is in flight (for any view of this pane).
    bool is_running() const { return current_id_ != 0; }
    /// View the run in flight belongs to.
    QString running_view() const { return current_id_ ? current_key_ : QString(); }
    /// The current view has a remembered result.
    bool has_result() const;
    QJsonValue payload() const;
    QDateTime as_of() const;
    /// The current view's result is older than the max age.
    bool is_stale() const;
    /// The current view's result was made from other inputs than set_signature().
    bool inputs_changed() const;

    /// "as of 14:02" for today, "as of 16 Nov 2026 14:02" otherwise.
    static QString as_of_text(const QDateTime& as_of);

  signals:
    void state_changed(fincept::ui::AutoLoadPane::State state);
    /// A result is on screen: fresh (`from_cache` false) or remembered.
    void result_shown(const QDateTime& as_of, bool from_cache);
    /// True while the action runs (including quiet refreshes).
    void running_changed(bool running);
    /// A refresh failed while a previous result stayed on screen.
    void refresh_failed(const QString& message);
    /// The host notice's button was pressed.
    void notice_action();
    /// Feed mode: the current view's data gained / lost the stale marker.
    void feed_stale_changed(bool stale);
    /// A clickable detail row of the failure page was pressed (after its DetailFn, if any).
    void error_detail_clicked(int index, const QString& key);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    struct Slot {
        QJsonValue payload;
        QDateTime as_of;
        QString signature;        ///< inputs that produced the payload
        QString wanted_signature; ///< inputs the next run would use
        QString last_error;       ///< failure with nothing to show (RETRY page)
        QString last_error_title;
        QString last_error_details; ///< technical text behind the page's DETAILS button
        bool has_error_page = false; ///< fail() with a host-built page: shown instead of the RETRY page
        EmptyState::Spec error_page;
        ActionFn error_primary;      ///< empty = RETRY
        ActionFn error_secondary;
        DetailFn error_detail;       ///< clickable detail rows of the host-built page
        quint64 attempted_visit = 0; ///< visit in which a default run last started / was declined
        bool has_result = false;
        bool from_cache = false;   ///< remembered from an earlier session / a host store
        bool cache_checked = false;
        bool refresh_failed = false;
        bool forced = false;       ///< invalidate(): run on the next chance
        // Memory budget.
        qint64 bytes = -1;         ///< estimated size of payload; -1 = not estimated yet
        bool persisted = false;    ///< this payload is in the persistent cache (reloads without a run)
        bool manual = false;       ///< made under Policy::Manual (a paid / heavy result)
        // Feed mode (what the host last reported for this view).
        autoload::FeedReport feed_report = autoload::FeedReport::None;
        QDateTime feed_received;   ///< when the data on screen arrived (UTC)
        QString feed_title;        ///< idle title / error page title
        QString feed_message;      ///< error / idle sentence
        QString feed_details;      ///< error details (DETAILS)
        QString feed_glyph;        ///< idle glyph
        bool host_held = false;    ///< feed_data_ready(): the host keeps the data (payload null)
        bool feed_quiet = false;   ///< feed_reset(): empty status label until the next report
        // Since round 6 (8): the persisted copy is read / measured off the GUI thread.
        bool reading = false;      ///< a read of the persisted copy is in flight for this view
        bool size_task = false;    ///< an exact size estimate is running on a worker
        quint64 payload_seq = 0;   ///< bumped whenever payload changes (validates async sizes)
    };
    enum class NoticeAction { None, Host, Run, Setup };

    Slot& slot(const QString& key);
    const Slot* find_slot(const QString& key) const;
    RequirementStatus evaluate_requirements() const;
    void present();
    autoload::ViewFacts facts_for(const Slot& s, bool requirements_ok) const;
    void maybe_auto();
    void request_auto_start();
    void begin(bool automatic);
    void end_run();
    void leave_gate();
    void drop_running();
    void fail_current(const QString& message, const QString& title, bool timed_out,
                      const QString& details = QString());
    void show_error_page(const Slot& s);
    void on_error_action(bool primary);
    void apply_setup_page(const RequirementStatus& req);
    void on_setup_action(bool primary);
    // Since round 5: a pending requirement (sign-in being confirmed), actionable failure rows.
    void show_checking_page(const RequirementStatus& req);
    void watch_requirements(const RequirementStatus& req);
    void on_error_detail(int index, const QString& key);
    // Since round 6.
    void apply_progress();                                    ///< skeleton bar / overlay caption
    void feed_store(const QJsonValue& payload, const QDateTime& as_of, bool host_held);
    // Since round 6 (7): agent-approval markers on the pane's own re-run buttons.
    void refresh_agent_risk();      ///< after a kind / cost / plan change
    void mark_agent_risk_widgets(); ///< preview default + notice button from applied_risk_
    void watch_plan_access();       ///< PlanAccess::changed → refresh_agent_risk (once)
    void load_remembered(const QString& key, Slot& s);
    // Since round 6 (8): async read of the persisted copy, async size estimates.
    void start_read(const QString& key, const QString& pkey);
    void on_read_done(const QString& key, quint64 gen, const autoload::RememberedRead& read, const QString& pkey);
    void abandon_read();                                 ///< the read in flight no longer matters
    void request_exact_size(const QString& key, Slot& s); ///< worker walk; provisional until it lands
    QString persist_key(const QString& key) const;
    void persist(const QString& key, const QJsonValue& payload, const QDateTime& at, const QString& signature);
    void set_state(State state);
    void show_page(QWidget* page, State state);
    void show_loading_page(bool queued);
    void update_footer();
    void update_notice();
    void update_status();
    int max_age_for(const QString& key) const;
    void apply_view_skeleton();
    void enforce_memory_budget(const QString& keep = QString()); ///< keep: a view just given a result
    // Feed mode.
    void present_feed();
    autoload::FeedFacts feed_facts(const Slot& s, bool requirements_ok) const;
    autoload::FeedDecision feed_decision(const Slot& s) const; ///< with the last requirement result
    void feed_set_subscribed(bool subscribed);
    void feed_retry();
    void feed_after_change(); ///< markers, notice, status, stale clock (data already on screen)
    void feed_arm_stale_timer(bool restart);
    void feed_emit_stale();
    void feed_show_idle_page(const Slot& s);

    QStackedWidget* stack_ = nullptr;
    QWidget* content_ = nullptr;
    QWidget* loading_page_ = nullptr;
    LoadingSkeleton* skeleton_ = nullptr;
    QLabel* footer_label_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* overlay_cancel_btn_ = nullptr; ///< CANCEL on the dimmed result during the user's re-run
    EmptyState* preview_ = nullptr;
    EmptyState* error_ = nullptr;
    EmptyState* setup_ = nullptr;
    LoadingOverlay* overlay_ = nullptr;
    QWidget* notice_ = nullptr;          ///< the pane's own notice (setup / inputs changed / refresh failed / stale)
    QLabel* notice_label_ = nullptr;
    QPushButton* notice_btn_ = nullptr;
    QWidget* host_notice_ = nullptr;     ///< set_notice() — under the pane's own
    QLabel* host_notice_label_ = nullptr;
    QPushButton* host_notice_btn_ = nullptr;
    QTimer* auto_timer_ = nullptr;
    QTimer* reveal_timer_ = nullptr;
    QTimer* timeout_timer_ = nullptr;
    QPointer<QLabel> status_label_;

    Policy policy_ = Policy::AutoRun;
    State state_ = State::Idle;
    StartFn start_;
    CancelFn cancel_;
    RenderFn render_;
    RememberedFn remembered_;
    RememberedSignedFn remembered_signed_;
    RequirementCheckFn requirement_check_;
    QString setup_secondary_text_;
    ActionFn setup_secondary_;
    QHash<QString, EmptyState::Spec> view_previews_;
    QList<Requirement> requirements_;
    double cost_credits_ = 0;
    QString cost_category_; ///< PlanAccess price key the cost belongs to (may be empty)
    RequirementStatus setup_status_; ///< what the setup page / notice currently names
    QString cache_key_;
    int cache_ttl_days_ = 30;
    int persist_limit_ = kMaxPersistBytes;
    QString loading_text_;
    int auto_delay_ms_ = 0;
    int max_age_s_ = autoload::kDefaultMaxAgeSeconds;
    int timeout_s_ = 0;
    bool cancellable_ = true;
    int gate_priority_ = 0;

    QString notice_text_;
    EmptyState::Tone notice_tone_ = EmptyState::Tone::Warning;
    QString notice_action_text_;
    NoticeAction notice_action_kind_ = NoticeAction::None;

    QString key_;                    ///< current view
    QHash<QString, Slot> slots_;
    QStringList slot_order_;         ///< least recently used first
    QString rendered_key_;           ///< view + version the content widget currently holds
    QDateTime rendered_as_of_;
    quint64 visit_ = 1;              ///< bumped on every show and view switch

    quint64 next_id_ = 0;
    quint64 current_id_ = 0;         ///< run in flight (0 = none)
    QString current_key_;            ///< its view
    QString current_sig_;            ///< the inputs it was started with
    bool current_auto_ = false;      ///< it is a default run (cancelled when its view is left)
    quint64 gate_ticket_ = 0;        ///< AutoLoadGate ticket of the automatic run waiting / running
    bool queued_ = false;            ///< waiting in the gate
    QString queued_key_;

    // Since round 4: per-view overrides, memory budget, signature set from StartFn.
    QHash<QString, int> view_max_age_;
    QHash<QString, LoadingSkeleton::Shape> view_shapes_;
    LoadingSkeleton::Shape pane_shape_ = LoadingSkeleton::Shape::Result; ///< restored when no override shows
    bool shape_overridden_ = false;
    qint64 memory_budget_ = autoload::kDefaultMemoryBudgetBytes;
    SizeFn size_fn_;
    bool in_start_ = false;          ///< inside StartFn: set_signature() retags the run

    // Feed mode.
    bool feed_mode_ = false;
    bool feed_subscribed_ = false;   ///< the pane last asked for the source (subscribe called)
    bool feed_stale_shown_ = false;  ///< last value of feed_stale_changed()
    int feed_stale_after_s_ = 0;
    SubscribeFn feed_subscribe_;     ///< the ActionFn form is wrapped
    ActionFn feed_unsubscribe_;
    QTimer* feed_stale_timer_ = nullptr;
    EmptyState* idle_ = nullptr;     ///< the source-idle page (built on first use)

    // Since round 5.
    bool requirement_watch_ = false;          ///< watch_requirement_changes() connected
    bool requirement_recheck_queued_ = false; ///< a re-check is already on its way
    // Since round 6.
    double progress_fraction_ = -1;           ///< of the run in flight; < 0 = indeterminate / none
    QString progress_text_;
    QString overlay_caption_;                 ///< last caption given to the overlay by apply_progress()
    bool feed_retry_next_ = false;            ///< the next subscribe is a retry
    // Since round 6 (7).
    QString agent_risk_;                      ///< set_agent_risk (empty = the rule)
    QHash<QString, QString> view_agent_risk_; ///< per-view explicit kinds
    QString applied_risk_;                    ///< kind on the buttons now (current view)
    bool plan_watch_ = false;                 ///< connected to PlanAccess::changed
    bool error_reruns_ = false;               ///< the error page's primary is the pane's RETRY
    // Since round 6 (8).
    quint64 read_gen_ = 0;                    ///< bumped when a read in flight stops mattering
    bool read_in_flight_ = false;
    QString reading_key_;                     ///< view the read in flight is for
    quint64 payload_seq_ = 0;                 ///< source of Slot::payload_seq
};

} // namespace fincept::ui
