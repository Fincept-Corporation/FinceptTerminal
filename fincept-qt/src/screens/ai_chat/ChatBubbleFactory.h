#pragma once
// ChatBubbleFactory — shared message-bubble builder used by both the AI Chat
// tab (AiChatScreen) and the floating Quick Chat bubble (AiChatBubble).
//
// Single source of truth for bubble visuals. Producing a bubble:
//   ChatBubbleFactory::Options opts;
//   opts.role = "user" | "assistant" | "system";
//   opts.content = "...";        // for finished bubbles only
//   opts.timestamp_iso = "...";  // optional; "" -> hide footer time
//   opts.show_footer = true;     // copy + timestamp row
//   opts.user_col_max_width = 560;
//   opts.ai_col_max_width   = 680;
//   ChatBubbleFactory::Bubble b = ChatBubbleFactory::build(opts);
//   layout->insertWidget(layout->count() - 1, b.row);
//
// Streaming bubble:
//   auto sb = ChatBubbleFactory::build_streaming(opts);
//   sb.body / sb.copy_btn ...
//   ChatBubbleFactory::append_streaming_chunk(sb.body, "more text");
//   ChatBubbleFactory::finalize_streaming(sb.body, "final markdown");

#include <QString>

class QFrame;
class QLabel;
class QPushButton;
class QWidget;

namespace fincept::ai_chat {

class ChatBubbleFactory {
  public:
    struct Options {
        QString role;                                // "user" | "assistant" | "system"
        QString content;                             // markdown
        QString timestamp_iso;                       // ISO-8601 or "yyyy-MM-dd HH:mm:ss"; "" hides time
        bool    show_footer       = true;            // timestamp + copy button
        int     user_col_max_width = 560;
        int     ai_col_max_width   = 680;
    };

    /// Result of building a bubble. `row` is what the caller inserts into its
    /// messages layout. The other handles are exposed for callers that want to
    /// observe / mutate (e.g. streaming).
    struct Bubble {
        QWidget*     row      = nullptr;   // outer row, includes alignment stretch
        QWidget*     column   = nullptr;
        QFrame*      frame    = nullptr;   // the visible bubble frame
        QLabel*      body     = nullptr;
        QLabel*      role_lbl = nullptr;
        QLabel*      time_lbl = nullptr;   // nullptr if footer hidden
        QPushButton* copy_btn = nullptr;   // nullptr if footer hidden / user / system
    };

    /// Build a finished message bubble (markdown rendered).
    static Bubble build(const Options& opts);

    /// Build a streaming bubble. The body starts in PlainText mode; chunks are
    /// appended via append_streaming_chunk(). When the stream completes call
    /// finalize_streaming() to switch to MarkdownText and reveal the copy
    /// button.
    static Bubble build_streaming(const Options& opts);

    /// Append a chunk to a streaming body. Accumulated text is stored on the
    /// label as the dynamic property "acc" so the caller doesn't need state.
    /// Honors the "Calling tool..." sentinel used by AiChatScreen.
    static void append_streaming_chunk(QLabel* body, const QString& chunk);

    /// Replace the body content (used by tool-call sentinel resets).
    static void replace_streaming_text(QLabel* body, const QString& text);

    /// Finalize a streaming bubble: re-render as markdown and unhide the copy
    /// button. If `final_text` is empty the current accumulated text is used.
    static void finalize_streaming(QLabel* body, const QString& final_text = {});
};

} // namespace fincept::ai_chat
