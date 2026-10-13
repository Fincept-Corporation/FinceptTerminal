#pragma once
// ChatBubbleFactory — shared message-bubble builder used by both the AI Chat
// tab (AiChatScreen) and the floating Quick Chat bubble (AiChatBubble).
//
// Single source of truth for bubble visuals. Producing a bubble:
//   ChatBubbleFactory::Options opts;
//   opts.role = "user" | "assistant" | "system";
//   opts.content = "...";        // for finished bubbles only
//   opts.timestamp_iso = "...";  // optional; "" -> hide footer time
//   opts.footer_meta = "...";    // optional; e.g. "openai / gpt-4o · 1.2k tok"
//   opts.show_footer = true;     // copy + timestamp row
//   opts.user_col_max_width = 560;
//   opts.ai_col_max_width   = 680;
//   ChatBubbleFactory::Bubble b = ChatBubbleFactory::build(opts);
//   layout->insertWidget(layout->count() - 1, b.row);
//
// Streaming bubble:
//   auto sb = ChatBubbleFactory::build_streaming(opts);
//   ChatBubbleFactory::append_streaming_chunk(sb.body, "more text"); // coalesced
//   ChatBubbleFactory::finalize_streaming(sb.body, "final markdown");
//
// Streaming repaints are COALESCED: chunks are buffered on the label and
// painted at most every kStreamFlushMs. A word-wrapped QLabel re-lays out its
// whole text on every setText, so painting each SSE token made a long answer
// cost O(n²) layout work and starved the event loop. Read the accumulated text
// through streaming_text(), which flushes first.

#include <QString>

class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QWidget;

namespace fincept::ai_chat {

class ChatBubbleFactory {
  public:
    /// Upper bound on streaming repaint rate (~15 per second).
    static constexpr int kStreamFlushMs = 66;

    struct Options {
        QString role;            // "user" | "assistant" | "system"
        QString content;         // markdown
        QString timestamp_iso;   // ISO-8601 or "yyyy-MM-dd HH:mm:ss"; "" hides time
        QString footer_meta;     // shown after the time (provider / model · tokens)
        bool show_footer = true; // timestamp + copy button
        int user_col_max_width = 560;
        int ai_col_max_width = 680;
    };

    /// Result of building a bubble. `row` is what the caller inserts into its
    /// messages layout. The other handles are exposed for callers that want to
    /// observe / mutate (e.g. streaming, extra footer actions).
    struct Bubble {
        QWidget* row = nullptr; // outer row, includes alignment stretch
        QWidget* column = nullptr;
        QFrame* frame = nullptr; // the visible bubble frame
        QLabel* body = nullptr;
        QLabel* role_lbl = nullptr;
        QLabel* time_lbl = nullptr;              // nullptr if footer hidden
        QPushButton* copy_btn = nullptr;         // nullptr if footer hidden / system
        QWidget* footer = nullptr;               // nullptr if footer hidden
        QHBoxLayout* footer_layout = nullptr;    // add extra actions here (make_footer_button)
    };

    /// Build a finished message bubble (markdown rendered).
    static Bubble build(const Options& opts);

    /// Build a streaming bubble. The body starts in PlainText mode; chunks are
    /// appended via append_streaming_chunk(). When the stream completes call
    /// finalize_streaming() to switch to MarkdownText and reveal the copy
    /// button.
    static Bubble build_streaming(const Options& opts);

    /// Append a chunk to a streaming body. Buffered; painted at most every
    /// kStreamFlushMs. Honors the "Calling tool..." placeholder.
    static void append_streaming_chunk(QLabel* body, const QString& chunk);

    /// Everything streamed into `body` so far (flushes the buffer first).
    static QString streaming_text(QLabel* body);

    /// Replace the body content (used by tool-call sentinel resets). Drops any
    /// buffered, not yet painted chunks.
    static void replace_streaming_text(QLabel* body, const QString& text);

    /// Finalize a streaming bubble: re-render as markdown, unhide the copy
    /// button, add "Copy code" when the answer has fenced code blocks, and fill
    /// the footer with the current time plus `footer_meta`. If `final_text` is
    /// empty the current accumulated text is used.
    static void finalize_streaming(QLabel* body, const QString& final_text = {}, const QString& footer_meta = {});

    /// The footer of a bubble built by this factory (its layout is an
    /// QHBoxLayout); nullptr when the bubble has no footer.
    static QWidget* footer_of(QLabel* body);

    /// A small footer action button in the shared bubble style (Copy, Retry,
    /// Regenerate, notice actions). One style source for every caller.
    static QPushButton* make_footer_button(const QString& text, const QString& tooltip = {});
};

} // namespace fincept::ai_chat
