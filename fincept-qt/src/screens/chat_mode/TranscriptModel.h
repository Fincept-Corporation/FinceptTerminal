#pragma once
// TranscriptModel.h — what Chat Mode's virtualised transcript holds (2026-11-17).
//
// The transcript is a flat list of Items (blocks) grouped into Turns. Only the items
// on screen have widgets (TranscriptView realises them); everything a widget shows or
// remembers (expanded, "show more", an approval's state, a tool result classified on
// a worker) lives here, so a block scrolled away and back looks exactly as it was.
// Plain data (Qt Core): history items are built on a worker thread.

#include "screens/chat_mode/ChatMarkdown.h"
#include "services/agent_tasks/ArtefactLogic.h"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

namespace fincept::chat_mode::transcript {

enum class ItemKind {
    Header,        ///< a turn's slim header (who · time · tokens · duration · actions); added by add_turn()
    UserText,      ///< what the user typed (text) + "sent with" context (text2)
    Markdown,      ///< one Markdown block of an agent reply (block)
    Activity,      ///< one tool call / delegation / check row (data), expandable
    ActivityGroup, ///< "ACTIVITY · 12 tool calls" — folds its children (hidden rows)
    Reasoning,     ///< the model's reasoning (text), collapsed
    Plan,          ///< a planned task's steps (data.steps) as a progress strip, expandable
    Question,      ///< the agent asks the user (text = question, text2 = hint)
    Approval,      ///< a trade proposal (data = proposal; state / status = decision)
    Note,          ///< a quiet line (text)
    Error,         ///< a failure (text) + optional action button (action / text2)
    FollowUps,     ///< suggested follow-up questions (list)
    CreditsCta,    ///< OUT OF CREDITS card
    Sources,       ///< numbered links of a reply (data.links)
};

/// One turn: a message of the user or one reply of the agent.
struct Turn {
    QString role;          ///< "user" | "assistant"
    QString label;         ///< shown upper case after the role ("STEP 2 OF 5"), may be empty
    QDateTime when;
    qint64 tokens = -1;    ///< reported tokens (< 0 unknown)
    qint64 duration_ms = -1;
    double credits = -1;   ///< reported credits (< 0 unknown)
    QString text;          ///< the whole text (copy / quote / report / notes)
    QString run_id;        ///< the run that produced it, when known
    int step = -1;         ///< planned run step index (RETRY STEP), -1 none
    bool streaming = false;
    bool failed = false;
};

enum class ApprovalState { Pending, Busy, Decided, Invalid };

/// A tool result classified off the GUI thread (shared by the item and its widget).
struct ToolResultCache {
    bool ready = false;
    bool busy = false;
    agent_tasks::artefact_logic::ToolResultView view;
};

struct Item {
    quint64 id = 0;
    ItemKind kind = ItemKind::Note;
    int turn = -1;            ///< index into the view's turns; -1 = none (notes outside turns)
    bool hidden = false;      ///< folded under an ActivityGroup (never realised, no height)

    // ── Payload ──
    QString text;
    QString text2;
    md::Block block;          ///< Markdown
    QJsonObject data;         ///< Activity: {tool, args, kind, running, success, duration_ms, preview, result, step,
                              ///<   run_id}; Approval: the proposal; Plan: {title, steps:[{title, status, ms}]};
                              ///<   Sources: {links:[{url, text}]}
    QStringList list;         ///< FollowUps
    QString action;           ///< Error: action id ("retry_load", "retry_run", "service_retry"); empty = none
    QString ref;              ///< Activity: ref id; Approval: proposal id; Error: run id

    // ── State that outlives the widget ──
    bool expanded = false;
    bool show_all = false;
    int state = 0;            ///< Activity: 0 running, 1 done, 2 failed; Approval: ApprovalState
    QString status;           ///< Approval: the hint / outcome line
    QVector<quint64> children; ///< ActivityGroup: the rows it folds
    std::shared_ptr<ToolResultCache> result;

    // ── Layout cache (TranscriptView) ──
    int height = -1;          ///< measured height at measured_width (-1: not measured)
    int measured_width = -1;
    int est_chars = -1;       ///< text size for estimates (-1: not counted yet)
    int est_breaks = 0;       ///< line breaks in that text
};

using ItemPtr = std::shared_ptr<Item>;

/// Something the user asked for in the transcript that the panel performs.
struct Action {
    QString kind;     ///< "quote" | "use_again" | "notes" | "retry_step" | "approve" | "deny" | "follow_up" |
                      ///< "symbol" | "error_action" | "get_pro" | "save_artefact" | "open_code" (value = code)
    quint64 item = 0; ///< the item it came from (0: a turn header)
    int turn = -1;
    QString value;    ///< the symbol, follow-up text, error action id …
};

} // namespace fincept::chat_mode::transcript
