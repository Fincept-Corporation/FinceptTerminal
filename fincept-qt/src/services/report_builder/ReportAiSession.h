#pragma once
// ReportAiSession — one AI drafting request of the Report Builder (RB-7), from "what would
// be sent" to "the user accepted it".
//
//   prepare(request)   facts + prompt built, NOTHING sent (state Ready): the panel shows the
//                      model, the cost and exactly what would leave the machine;
//   send()             the user's click: the backend starts a request with a new id
//                      (state Streaming); chunks accumulate into the PROPOSAL, never the
//                      document; an idle time-out (default 2 minutes without a chunk)
//                      cancels it (state Failed, "nothing was changed");
//   on success         the proposal (a selection's reply replaces only the selection), its
//                      paragraph hunks and the numbers that are not in the report's data
//                      (state Proposal);
//   accept_all() / accept_hunks() / use_title()   emit apply_requested() exactly once —
//                      the screen applies it as one undo step; discard() drops it.
// Owner decisions: drafting goes through the terminal's model router (feature
// kReportDraftFeature); it is always user-initiated; nothing is spent without a click.
// The session never touches ReportBuilderService or the document.
//
// GUI thread only. The backend (ReportAiBackend) delivers its callbacks on the GUI thread;
// chunks and results of an older request id are ignored. Logs one INFO line when a request
// starts and one when it ends (counts, model label, outcome, duration) — never the prompt or
// the reply.

#include "core/report/ReportAiDraft.h"
#include "core/report/ReportDocument.h"

#include <QElapsedTimer>
#include <QMetaType>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QTimer;

namespace fincept::services {

/// The ModelRouter feature key of Report Builder drafting.
inline constexpr const char* kReportDraftFeature = "report_draft";

/// What the user asked for. `doc` is a snapshot of the report when the action was chosen.
struct AiRequest {
    report::ai::Action action = report::ai::Action::Rewrite;
    QString tone;           ///< ChangeTone: "formal", "neutral", "concise" … (empty = formal)
    int block_id = 0;       ///< the block written / rewritten (0 for report actions)
    QString block_text;     ///< its current text
    int sel_start = -1;     ///< a selection inside block_text: [sel_start, sel_end)
    int sel_end = -1;
    report::ReportDocument doc;
};

/// Who would answer, and who pays.
struct AiModelInfo {
    QString label;     ///< "Claude Code · sonnet · high", "OpenAI · gpt-5", "Fincept LLM"
    QString cost_note; ///< "0 credits · billed to your Anthropic API key" / "Fincept LLM — uses your Fincept credits"
    bool needs_setup = false;
    QString setup_reason;
};

struct AiBackendResult {
    bool ok = false;
    bool cancelled = false;
    QString text;            ///< the full reply (used when nothing streamed)
    QString error;           ///< user-facing sentence
    QString route_label;     ///< who answered
    QString fallback_notice; ///< "Claude Code is rate limited — answered by Fincept LLM"
    bool settings_hint = false; ///< the error is fixed in Settings (no model, credits, sign-in)
};

/// The model behind a session. The production one is ReportAiLlmBackend (LlmService +
/// ModelRouter); tests use a fake. Callbacks are delivered on the GUI thread, possibly after
/// cancel() (the session ignores them).
class ReportAiBackend {
  public:
    virtual ~ReportAiBackend() = default;
    virtual void start(quint64 id, const QString& system, const QString& user,
                       std::function<void(quint64, const QString& chunk)> on_chunk,
                       std::function<void(quint64, const AiBackendResult&)> on_done) = 0;
    virtual void cancel(quint64 id) = 0;
    virtual AiModelInfo model_info() const = 0;
};

/// What the screen applies (one undo step).
struct ApplyResult {
    enum class Kind { ReplaceText, InsertBlocks, SetTitle };
    Kind kind = Kind::ReplaceText;
    int block_id = 0;     ///< ReplaceText: the block
    QString new_content;  ///< ReplaceText: its new text
    QString old_content;  ///< ReplaceText: the text the proposal was made from (refuse if the block changed since)
    int after_block_id = 0; ///< InsertBlocks: insert after this block; 0 = at the start of the report
    QVector<report::ReportComponent> blocks; ///< InsertBlocks: new blocks (id 0 — the screen allocates ids)
    QString title;        ///< SetTitle: the new report title
    QString undo_text;    ///< label of the undo step ("AI: rewrite")
};

class ReportAiSession : public QObject {
    Q_OBJECT
  public:
    enum class State { Idle, Ready, Streaming, Proposal, Failed, Cancelled };
    Q_ENUM(State)

    /// Longest block text sent (characters); a longer one asks for a selection.
    static constexpr int kMaxTextChars = 30000;
    static constexpr int kDefaultTimeoutMs = 120000;

    /// `backend` is not owned and must outlive the session.
    explicit ReportAiSession(ReportAiBackend* backend, QObject* parent = nullptr);
    ~ReportAiSession() override;

    // ── Flow ──
    /// Builds the facts and the prompt; sends nothing. Cancels a request in flight.
    void prepare(const AiRequest& request);
    /// The user's click: starts the request (Ready, or again after Failed / Cancelled /
    /// Proposal). False when there is nothing prepared or the model needs setup.
    bool send();
    /// Stops the request in flight (state Cancelled; nothing was changed).
    void cancel();
    /// Sends the same prompt again under a new request id.
    bool retry();
    /// Drops everything (cancels a request in flight); back to Idle. Emits no apply.
    void discard();
    /// Emits apply_requested() with the whole proposal, then Idle. False (nothing emitted)
    /// outside Proposal or when the proposal changes nothing.
    bool accept_all();
    /// Only the given hunk indexes (hunks()); the rest keep the old paragraphs.
    bool accept_hunks(const QSet<int>& hunks);
    /// SuggestTitle: the chosen title (any text the user picked or edited).
    bool use_title(const QString& title);

    void set_timeout_ms(int ms);
    int timeout_ms() const { return timeout_ms_; }
    /// "No answer within 2 minutes — nothing was changed."
    static QString timeout_message(int ms);

    // ── State ──
    State state() const { return state_; }
    const AiRequest& request() const { return request_; }
    const report::ai::Prompt& prompt() const { return prompt_; }
    const QVector<report::ai::Fact>& facts() const { return facts_; }
    /// The text that was / will be sent as TEXT (the selection when there is one).
    QString sent_text() const { return sent_text_; }
    bool has_selection() const;
    /// Asks the backend now (the model can change between prepare and send).
    AiModelInfo model_info() const;
    QString stream_text() const { return stream_; }
    QString reply() const { return reply_; }
    /// The block's full text with the reply applied (empty for SuggestTitle).
    QString proposal() const { return proposal_; }
    /// The text the proposal is compared with (the block text; empty for report actions).
    QString original() const { return original_; }
    const QVector<report::ai::Hunk>& hunks() const { return hunks_; }
    const QVector<report::ai::NumberFlag>& flags() const { return flags_; }
    QStringList titles() const { return titles_; }
    /// True when the proposal equals the original (nothing to accept).
    bool no_change() const;
    QString error() const { return error_; }
    bool settings_hint() const { return settings_hint_; }
    QString route_label() const { return route_label_; }
    QString fallback_notice() const { return fallback_notice_; }
    quint64 request_id() const { return active_id_; }
    /// Milliseconds since send() (0 before).
    qint64 elapsed_ms() const;

  signals:
    void state_changed();
    void stream_updated(const QString& text_so_far);
    void proposal_ready();
    void apply_requested(const fincept::services::ApplyResult& result);

  private:
    void set_state(State s);
    bool start_request();
    void on_chunk(quint64 id, const QString& chunk);
    void on_done(quint64 id, const AiBackendResult& result);
    void on_timeout();
    void stop_in_flight();
    void build_proposal();
    void emit_apply(const ApplyResult& r);
    void log_end(const char* outcome);

    ReportAiBackend* backend_ = nullptr;
    QTimer* timeout_timer_ = nullptr;
    int timeout_ms_ = kDefaultTimeoutMs;
    State state_ = State::Idle;

    AiRequest request_;
    QVector<report::ai::Fact> facts_;
    report::ai::Prompt prompt_;
    QString sent_text_;
    QString source_text_; ///< what the number check accepts besides the facts

    quint64 active_id_ = 0; ///< the request whose chunks / result count (0 = none)
    QElapsedTimer clock_;
    QString start_model_;
    QString stream_;
    QString reply_;
    QString original_;
    QString proposal_;
    QVector<report::ai::Hunk> hunks_;
    QVector<report::ai::NumberFlag> flags_;
    QStringList titles_;
    QString error_;
    bool settings_hint_ = false;
    QString route_label_;
    QString fallback_notice_;
};

} // namespace fincept::services

Q_DECLARE_METATYPE(fincept::services::ApplyResult)
