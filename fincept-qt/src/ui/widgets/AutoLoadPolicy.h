#pragma once
// AutoLoadPolicy — the pure rules behind ui::AutoLoadPane, ui::AutoLoadGate and
// ui::EmptyState (QtCore only, unit-tested in tests/tst_autoload_logic.cpp):
//   * when a remembered result is stale (max age),
//   * a stable signature of the inputs that produced a result,
//   * whether showing a view starts its default run,
//   * the app-wide queue of automatic runs (capacity, priority, FIFO),
//   * whether a run may start (requirements first, on every run path),
//   * which remembered results a pane drops to stay inside its memory budget,
//   * what a pane fed by a hub / stream shows (feed mode: page, marker, staleness),
//   * how far an EmptyState compacts in a short pane (empty_state_fit),
//   * when EmptyState::apply() has nothing to do (empty_state_apply::LastApplied),
//   * the persisted copy off the GUI thread: its text format, whether a finished
//     read may still be shown, and the write-behind queue (since round 6 (8)).

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <memory>

class QThreadPool;

namespace fincept::ui::autoload {

/// Max-age values: refresh on every show / never refresh automatically.
constexpr int kAlwaysRefresh = 0;
constexpr int kNeverRefresh = -1;
/// Default max age of a remembered result: a day.
constexpr int kDefaultMaxAgeSeconds = 24 * 3600;

/// True when a result computed at `as_of` must be refreshed at `now`:
/// max_age 0 = always, negative = never, otherwise older than max_age seconds.
/// An invalid `as_of` is stale (unless max_age is negative).
bool is_stale(const QDateTime& as_of, const QDateTime& now, int max_age_seconds);

/// Stable signature of the inputs that produce a result: SHA-1 (16 hex chars)
/// of the canonical compact JSON (object keys sorted, so key order never matters).
QString signature_of(const QJsonValue& inputs);
QString signature_of(const QVariantMap& inputs);

/// What decides whether showing a view starts its default run.
struct ViewFacts {
    bool auto_policy = true;         ///< Policy::AutoRun (free / local); Manual never runs by itself
    bool requirements_ok = true;     ///< nothing missing (key, model, Python, broker, credits)
    bool running = false;            ///< a run of this pane is already in flight
    bool attempted_this_visit = false; ///< a default run already started (or was declined) during this show
    bool has_result = false;         ///< a remembered result exists for the view
    bool stale = false;              ///< … and it is older than the max age
    bool signature_mismatch = false; ///< … and it was produced from other inputs than the current ones
    bool forced = false;             ///< the host called invalidate()
};

/// AutoRun views run on show when nothing is remembered, when the remembered
/// result is stale and still matches the inputs, or when the host forced it.
/// A result from other inputs is never silently re-run (the pane shows an
/// "inputs changed" notice). At most one automatic attempt per visit.
bool should_auto_run(const ViewFacts& facts);

/// What a request to start a run leads to — every path: the user's RUN / RETRY /
/// RUN AGAIN / REFRESH, the default run, its turn in the gate.
enum class RunGate {
    Start,      ///< start the work
    NeedsSetup, ///< a requirement is missing: the setup page (or notice), nothing starts
    NoAction,   ///< the pane has no start function
};
RunGate gate_run(bool has_start, bool requirements_ok);

/// Since round 6 (7). The "fincept_agent_risk" kind on a pane's own buttons that
/// start or re-run its load (RUN, RETRY, RUN AGAIN, REFRESH, RECONNECT): the
/// view's explicit kind, else the pane's explicit kind, else "spend" when one run
/// has a list cost (> 0) that the account's plan does not include (`included`:
/// its category costs this account nothing), else "" (no marker). An explicit
/// kind always wins; an empty one means "use the rule". `list_cost` <= 0 or NaN =
/// free. Kinds are passed through as given (trimmed).
QString effective_agent_risk(const QString& pane_kind, const QString& view_kind, double list_cost, bool included);

// ── Memory budget of the results a pane keeps per view ───────────────────────

/// Default byte cap of one pane's remembered results (estimated sizes).
constexpr qint64 kDefaultMemoryBudgetBytes = 32LL * 1024 * 1024;

/// One view's result as the budget sees it.
struct MemorySlot {
    QString key;
    qint64 bytes = 0;        ///< estimated size of its result
    bool pinned = false;     ///< on screen, running or queued: never dropped
    bool reloadable = true;  ///< comes back without a run (persistent cache / host copy / pushed again)
    bool manual = false;     ///< a Manual (class C: paid / heavy) view's result
};

/// Views whose results to drop so the rest fit `budget_bytes`, least recently
/// used first: results that reload without a run go first, then AutoRun results
/// (free / cheap to compute again). Never a pinned view, never a Manual result
/// that cannot be reloaded (the user paid for it). Empty when the total fits or
/// the budget is 0 or less (no cap). `lru_first`: least recently used first.
QStringList choose_evictions(const QList<MemorySlot>& lru_first, qint64 budget_bytes);

/// Rough in-memory size of a JSON payload (the default size estimator): strings
/// as UTF-16 plus overhead, numbers / bools / nulls as a fixed cell, containers
/// as the sum of their parts. Linear in the payload; a guide, not an exact count.
qint64 approx_json_bytes(const QJsonValue& value);

/// The app-wide queue of automatic runs: `capacity` run at once, the rest wait
/// in priority order (higher first), FIFO within a priority.
class AutoLoadQueue {
  public:
    explicit AutoLoadQueue(int capacity = 2);

    void set_capacity(int capacity);
    int capacity() const { return capacity_; }

    /// Ask to start `ticket`. True = start now (a slot is free and nobody with
    /// the same or higher priority is waiting); false = queued.
    bool request(quint64 ticket, int priority = 0);
    /// `ticket` finished, was cancelled or left the queue (running or queued).
    /// Returns the tickets that may start now, in start order.
    QList<quint64> release(quint64 ticket);

    bool is_running(quint64 ticket) const { return running_.contains(ticket); }
    bool is_queued(quint64 ticket) const;
    /// 0-based place in the queue; -1 when not queued.
    int position(quint64 ticket) const;
    int running_count() const { return int(running_.size()); }
    int queued_count() const { return int(waiting_.size()); }

  private:
    struct Waiting {
        quint64 ticket;
        int priority;
    };
    QList<quint64> fill();

    int capacity_ = 2;
    QList<quint64> running_;
    QList<Waiting> waiting_; ///< kept sorted: priority desc, then arrival
};

// ── Feed mode: data PUSHED by a hub / adapter (AutoLoadPane::set_feed_mode) ──

/// What the host last reported about its source.
enum class FeedReport {
    None,    ///< nothing reported yet for this view
    Waiting, ///< subscribed (again), no update yet
    Data,    ///< an update arrived
    Error,   ///< the source reports an error
    Idle,    ///< the source is idle on purpose (market closed, no session)
};

/// The page a feed-mode pane shows.
enum class FeedPage { Waiting, Content, Error, Idle, NeedsSetup };

/// What accompanies data on screen (the pane's notice strip and status label).
enum class FeedMarker {
    None,       ///< live and fresh
    Connecting, ///< remembered / earlier data, waiting for the first update of this visit
    Stale,      ///< no update for longer than the stale threshold (a marker, not an error)
    Stopped,    ///< the source reports an error; the last data stays
    Idle,       ///< the source is idle; the last data stays
};

struct FeedFacts {
    bool requirements_ok = true;
    bool has_data = false;           ///< data for the view is on screen / remembered
    FeedReport report = FeedReport::None;
    QDateTime last_update;           ///< when that data arrived (a remembered copy: its as-of time)
    QDateTime now;
    int stale_after_seconds = 0;     ///< 0 = data never turns stale
};

struct FeedDecision {
    FeedPage page = FeedPage::Waiting;
    FeedMarker marker = FeedMarker::None;
    bool subscribe = false;          ///< the pane wants the source (requirements met)
};

/// No update for MORE than `stale_after_seconds` (0 or less = never; unknown time = not stale).
bool feed_is_stale(const QDateTime& last_update, const QDateTime& now, int stale_after_seconds);
/// Milliseconds until feed_is_stale() turns true: 0 = already stale, -1 = never.
qint64 feed_ms_until_stale(const QDateTime& last_update, const QDateTime& now, int stale_after_seconds);
/// Requirements missing → setup page (data on screen stays; the pane adds its setup
/// notice) and no subscription. Data on screen → Content with the marker of the last
/// report (error → Stopped, idle → Idle, otherwise Stale when too old, Connecting
/// before the first update). Nothing yet → the error / idle page, else the skeleton.
FeedDecision decide_feed(const FeedFacts& facts);

// ── The persisted copy, off the GUI thread (since round 6 (8)) ───────────────

/// The text a pane persists: {"at": ISO time, "p": payload, "sig": inputs signature}.
QByteArray serialize_remembered(const QJsonValue& payload, const QDateTime& as_of, const QString& signature);

/// What reading a persisted copy found.
enum class ReadOutcome {
    Miss,    ///< nothing stored (or larger than the ceiling): a cache miss
    Hit,     ///< a usable result
    Corrupt, ///< text that is not a result: a cache miss, and the row should go
};
struct RememberedRead {
    ReadOutcome outcome = ReadOutcome::Miss;
    QJsonValue payload;
    QDateTime as_of;
    QString signature;
    qint64 text_bytes = 0; ///< UTF-8 size of the stored text (the memory estimate's base)
};
/// Parses stored text (worker thread). Empty or longer than `ceiling_chars` = Miss;
/// not a JSON object, no "p", or no valid "at" = Corrupt. Pure.
RememberedRead parse_remembered(const QString& text, qint64 ceiling_chars);

/// May a read that finished now still fill its view? Everything that happened
/// while it ran and makes it out of date bumps the pane's generation (hidden,
/// view left, invalidate(), clear_result(), a run started); the other facts are
/// checked as well so the rule stands on its own.
struct ReadFacts {
    quint64 started_gen = 0;   ///< pane generation when the read started
    quint64 current_gen = 0;   ///< pane generation now
    QString started_view;      ///< view the read is for
    QString current_view;      ///< view on screen now
    bool visible = true;       ///< the pane is shown
    bool run_for_view = false; ///< a run of that view is in flight (it will answer)
    bool has_result = false;   ///< that view already holds a result (a run delivered first)
};
enum class ReadVerdict { Accept, Drop };
/// Accept only when nothing above changed: same generation and view, shown, no
/// run in flight for the view and no result there yet. A dropped read is not an
/// automatic attempt; the view reads again on its next look.
ReadVerdict decide_read(const ReadFacts& facts);

/// Memory estimate of a result parsed from `utf8_bytes` of compact JSON text
/// (what the worker measured): 2 bytes per byte of text — no walk of the payload.
qint64 bytes_from_json_text(qint64 utf8_bytes);
/// Provisional estimate that never walks the payload (O(1)): the top-level count
/// of an array / object times a per-entry cell, a string's length, else one cell.
/// Used until the exact estimate from a worker lands.
qint64 shallow_json_bytes(const QJsonValue& value);

/// Bookkeeping of the write-behind queue (pure; owner thread only): one op per
/// key waits (the latest replaces the one waiting — latest payload wins), at most
/// one op per key is in flight, at most `max_in_flight` run at once, at most
/// `max_queued` keys wait (beyond that the oldest waiting op is dropped).
class WriteQueue {
  public:
    explicit WriteQueue(int max_in_flight = 2, int max_queued = 64);
    /// Queue op `seq` of `key`. `*replaced` = the op of that key it replaced, `*dropped` =
    /// the oldest waiting op dropped to stay within max_queued (0 = none).
    void enqueue(const QString& key, quint64 seq, quint64* replaced = nullptr, quint64* dropped = nullptr);
    /// Ops that may start now, in queue order (marked in flight).
    QList<QPair<QString, quint64>> take_ready();
    /// The op in flight for `key` finished.
    void finished(const QString& key);
    bool in_flight(const QString& key) const { return in_flight_.contains(key); }
    quint64 in_flight_seq(const QString& key) const { return in_flight_.value(key, 0); }
    quint64 queued_seq(const QString& key) const;
    int queued_count() const { return int(queue_.size()); }
    int in_flight_count() const { return int(in_flight_.size()); }
    /// Every waiting op in queue order, removed (the flush at quit).
    QList<QPair<QString, quint64>> drain();

  private:
    int max_in_flight_;
    int max_queued_;
    QList<QPair<QString, quint64>> queue_; ///< waiting, first queued first
    QHash<QString, quint64> in_flight_;
};

/// Write-behind of persisted results: lives on the GUI thread, outlives the panes
/// that submit to it. `work` runs on a pool thread (serialise + store; returns the
/// bytes written, < 0 = not written); `done` runs afterwards on the owner thread —
/// only for the op that ran (a replaced or dropped op is never called back). A
/// pane's destruction never drops its write (nothing here belongs to a pane).
/// pending() serves read-your-writes: the op waiting or running for a key is the
/// newest copy. flush() is for application quit.
class WriteBehind : public QObject {
  public:
    struct Op {
        QString key;
        std::function<qint64()> work;
        std::function<void(qint64 bytes)> done;
        // What the op stores (read-your-writes): a result, or `remove`.
        bool remove = false;
        QJsonValue payload;
        QDateTime as_of;
        QString signature;
    };
    explicit WriteBehind(QThreadPool* pool = nullptr, int max_in_flight = 2, int max_queued = 64,
                         QObject* parent = nullptr);
    ~WriteBehind() override;

    /// Queue `op` (latest per key wins). Returns false after flush() (nothing queued).
    bool submit(Op op);
    /// The op waiting for `key`, else the one running; false when neither.
    bool pending(const QString& key, Op* out) const;
    bool busy(const QString& key) const;
    int queued_count() const { return queue_.queued_count(); }
    int in_flight_count() const { return queue_.in_flight_count(); }
    int dropped_count() const { return dropped_; }
    /// Quit: wait up to `wait_ms` for running ops, then run every waiting op now, on
    /// this thread (their `done` is not called). A waiting op whose key is still
    /// running after the wait is skipped (an older write could land after it).
    /// Returns the number skipped. Later submit() calls are refused.
    int flush(int wait_ms);

  private:
    struct Shared;
    void start_ready();
    void on_finished(const QString& key, quint64 seq, qint64 bytes);

    QThreadPool* pool_;
    WriteQueue queue_;
    QHash<quint64, Op> ops_;
    quint64 next_seq_ = 0;
    int dropped_ = 0;
    bool closed_ = false;
    std::shared_ptr<Shared> shared_;
};

} // namespace fincept::ui::autoload

// ── EmptyState auto-compaction (the widget measures from font metrics, this decides) ──
namespace fincept::ui::empty_state_fit {

/// Steps an EmptyState takes when its full layout does not fit the height it has.
enum class Level {
    Full = 0,      ///< everything, normal margins
    NoGlyph = 1,   ///< tight margins, no glyph
    NoDetails = 2, ///< … and no detail / suggestion rows, no DETAILS block
    Inline = 3,    ///< title + primary action on one line (message as tooltip)
    TitleOnly = 4, ///< the title alone (message as tooltip)
};

/// Pixel heights of the blocks of one EmptyState at its current width (0 = not shown).
struct Parts {
    int margins = 0;       ///< top + bottom margin of the full layout
    int tight_margins = 0; ///< top + bottom margin once compacted
    int spacing = 0;       ///< between two stacked blocks
    int glyph = 0;
    int title = 0;
    int message = 0;
    int note = 0;
    int technical = 0;     ///< DETAILS button (+ its box when open)
    int details = 0;       ///< key / value and bullet rows
    int suggestions = 0;   ///< clickable suggestion rows
    int actions = 0;       ///< the primary / secondary button row
    int inline_row = 0;    ///< title and primary action on one line
    int title_line = 0;    ///< one line of the title
};

/// Height `level` needs (margins included).
int needed_height(const Parts& parts, Level level);
/// The first level whose layout fits `available_height`; TitleOnly when none does.
/// A widget that has no height yet (<= 0) stays Full.
Level choose_level(const Parts& parts, int available_height);

} // namespace fincept::ui::empty_state_fit

// ── EmptyState::apply() of the spec already on screen is a no-op (since round 6) ──
namespace fincept::ui::empty_state_apply {

/// Remembers the last value applied (an EmptyState::Spec): should_apply() is
/// false when `next` equals it — no relayout, repaint or fit pass — and true
/// (remembering `next`) the first time, after invalidate(), or for anything
/// different. The widget calls invalidate() whenever a single setter changes
/// it outside apply(), so a later apply() of the old value still applies.
template <typename T>
class LastApplied {
  public:
    bool should_apply(const T& next) {
        if (valid_ && last_ == next)
            return false;
        last_ = next;
        valid_ = true;
        return true;
    }
    void invalidate() { valid_ = false; }
    bool valid() const { return valid_; }

  private:
    T last_{};
    bool valid_ = false;
};

} // namespace fincept::ui::empty_state_apply
