#pragma once
// AiChatTranscript.h — the AI Chat transcript: one continuous research document, virtualised.
//
//   YOU · 14:32 ─────────────────────────────────────────────── [COPY] [EDIT & RESEND] [QUOTE]
//   compare $AAPL with MSFT on margins
//   ─────────────────────────────────────────────────────────────────────────────────────────
//   CLAUDE CODE · SONNET · 14:32 · 1.2k tok · 4.1 s ──────── [COPY] [REGENERATE] [REPORT ▾] [QUOTE]
//   ▸ REASONING · 2,140 chars
//   ▸ TOOLS · 3 calls · 1.8 s        ✓ get_quote · AAPL  420 ms  …
//   ▌HEADING
//   paragraph with [AAPL] chips, figures in tabular monospace, +2.4% / -1.1% with sign …
//   ┌ table: header, hairline rows, numbers right-aligned ┐  [COPY CSV] [CHART]
//   ┌ python ─────────────────────────── [COPY] [OPEN IN CODE TAB] ┐
//   SOURCES  [1] reuters.com  [2] sec.gov
//
// Virtualised: a turn widget exists only while its turn is in (or near) the viewport; the
// others are a measured (or estimated) height. A thousand-message session builds the dozen
// turns on screen — switching sessions never rebuilds more than the visible part. Markdown
// of turns being realised is parsed once and cached (long answers on a worker thread, a
// one-line "rendering…" placeholder meanwhile); a streaming answer re-parses and re-renders
// only its TAIL block, at most every style::kStreamFlushMs.
//
// Rich text is built only from the parser's sanitised HTML (AiChatTypes.h) themed by
// style::inline_html(); no raw HTML from a model reaches a label. Links open externally and
// only when is_safe_link(); ticker chips open the symbol (click = Equity Research; context
// menu = Research / Equity Trading / News / Backtesting / Add to watchlist ▸ / Copy) through
// screens::equity_links (nav.open_symbol).
//
// Follows the tail while the user is at the bottom; once they scroll up a "↓ NEW" pill
// appears when new content arrives (click / End = back to the tail). New turns fade in
// over style::kFadeMs when ui::motion_enabled().
//
// Keyboard (the view takes focus): Up / Down = previous / next turn (accent tick on the
// current turn, scrolled into view), PgUp / PgDn / Home / End scroll, Enter or the Menu key
// = the current turn's action menu, Ctrl+C = copy the current turn, Ctrl+R = REGENERATE /
// RETRY (marked: fincept_agent_risk_keys "Ctrl+R=spend"), Esc = clear the current turn.
//
// Agent-risk markers (core/agent/AgentRiskMarkers.h): REGENERATE, RETRY and ANSWER WITH
// buttons carry "spend"; nothing here spends by itself.

#include "screens/ai_chat/AiChatTypes.h"

#include <QAbstractScrollArea>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

class QPoint;
class QPushButton;
class QTimer;

namespace fincept::screens::aichat {

class AicTurnWidget; // AiChatTranscript.cpp

class AicTranscriptView : public QAbstractScrollArea {
    Q_OBJECT
  public:
    /// Desk = the AI Chat tab; Compact = the Quick Chat bubble (narrow gutters, COPY only,
    /// tools and reasoning collapsed to one line each).
    enum class Density { Desk, Compact };

    explicit AicTranscriptView(Density density = Density::Desk, QWidget* parent = nullptr);
    ~AicTranscriptView() override;

    // ── Content ──────────────────────────────────────────────────────────────
    /// Remove every turn (and widget). Pending parses are dropped.
    void clear();
    /// Replace every turn (a session load) and land at the bottom, following the tail.
    /// Ids are assigned here (returned in order).
    QVector<quint64> set_turns(const QVector<Turn>& turns);
    /// Append one turn (id assigned, returned). Follows the tail if the view was following;
    /// otherwise the "↓ NEW" pill shows. Fades in when motion is on.
    quint64 append_turn(const Turn& turn);
    /// Replace a turn's data (id kept); its widget (if realised) re-renders.
    void update_turn(quint64 id, const Turn& turn);
    void remove_turn(quint64 id);
    /// nullptr when unknown. The pointer is valid until the next call that changes content.
    const Turn* turn(quint64 id) const;
    QVector<quint64> turn_ids() const;
    int turn_count() const;
    /// Newest turn of `role` (0 = none).
    quint64 last_turn_id(TurnRole role) const;

    /// Bare upper-case words recognised as tickers in answers (linked symbol, mentions,
    /// symbols from tool arguments). Applies to turns parsed afterwards.
    void set_known_symbols(const QSet<QString>& symbols);
    /// The one turn that offers REGENERATE (an answer) or RETRY / ANSWER WITH (a failed,
    /// stopped or notice turn); 0 = none.
    void set_live_turn(quint64 id);
    quint64 live_turn() const { return live_turn_; }
    /// False while a request runs: REGENERATE / RETRY / ANSWER WITH / EDIT & RESEND hide.
    void set_actions_enabled(bool enabled);
    /// "AI Chat" / "Quick Chat": source label of ADD TO REPORT.
    void set_report_source(const QString& label);

    // ── Streaming ────────────────────────────────────────────────────────────
    /// Append answer text to a Streaming turn. Buffered; painted at most every
    /// style::kStreamFlushMs; only the tail block is re-parsed and re-rendered.
    void stream_append(quint64 id, const QString& chunk);
    /// Everything streamed into the turn so far (the buffer included).
    QString stream_text(quint64 id) const;
    /// The model switched to tool calls mid-stream: what streamed so far was aborted markup.
    void stream_drop_text(quint64 id);
    /// Chain-of-thought text (collapsed REASONING section), buffered like the answer.
    void stream_append_thinking(quint64 id, const QString& chunk);
    /// The turn's tool activity (running or final).
    void set_tool_steps(quint64 id, const QVector<ToolStep>& steps);
    /// Live status of a background job shown under the activity row that started it.
    /// tone: 0 running, 1 succeeded, 2 failed, 3 cancelled / expired.
    void set_job_status(const QString& job_id, const QString& text, int tone);

    // ── Scrolling ────────────────────────────────────────────────────────────
    /// Jump to the end and follow the tail again.
    void scroll_to_bottom();
    bool following_tail() const { return follow_; }
    int scroll_position() const;
    void set_scroll_position(int y);

    /// The keyboard's current turn (0 = none).
    quint64 current_turn() const { return current_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    /// REGENERATE, RETRY, EDIT & RESEND, QUOTE, ANSWER WITH, MODEL SETTINGS, LLM SETTINGS,
    /// VIEW PLANS, OPEN SETTINGS — the host acts. COPY and ADD TO REPORT are done here and
    /// reported too.
    void action_requested(quint64 turn_id, fincept::screens::aichat::TurnAction action);
    /// OPEN IN CODE TAB on a Python block.
    void code_open_requested(const QString& code, const QString& lang);
    /// Short confirmation for the host's status bar ("Copied 1,240 characters").
    void status_message(const QString& text);
    /// The user scrolled away from / back to the tail.
    void following_changed(bool following);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void scrollContentsBy(int dx, int dy) override;
    bool viewportEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    friend class AicTurnWidget;
    struct Entry;
    struct Private;
    std::unique_ptr<Private> d_;

    // AiChatTranscript_Blocks.cpp — the widget of one parsed block (paragraph, heading, list,
    // quote, code, table, rule, math) inside a turn. `caret`: the streaming tail block.
    static QWidget* create_block_widget(AicTranscriptView* view, quint64 turn_id, const Block& block, bool caret,
                                        QWidget* parent);
    /// Re-render `widget` for `block` in place; false when it cannot (another kind): recreate it.
    static bool update_block_widget(QWidget* widget, const Block& block, bool caret);
    /// Appearance settled (`rerender_html`: style::inline_generation() changed) or the language
    /// changed: re-render themed HTML, re-fit fonts, re-set texts.
    static void restyle_block_widget(QWidget* widget, bool rerender_html);
    // AiChatTranscript.cpp — what the block widgets ask of the view.
    void open_link(quint64 turn_id, const QString& link);
    void show_text_menu(quint64 turn_id, const QPoint& global_pos, const QString& link, const QString& selection);

    Density density_ = Density::Desk;
    bool follow_ = true;
    quint64 live_turn_ = 0;
    quint64 current_ = 0;
};

} // namespace fincept::screens::aichat
