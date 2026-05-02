#pragma once
#include <QHash>
#include <QObject>
#include <QPointer>

namespace fincept {
class WindowFrame;
class AiChatBubble;
}

namespace fincept::ai_chat {

/// Phase 3 final: shell-level coordinator for the per-frame AiChatBubble.
///
/// **Final design**: keep per-frame AiChatBubble instances (one bubble per
/// frame, parented to that frame's dock_manager), but centralise the
/// shell-side observation surface. Per-frame bubbles match the multi-
/// window UX expectation that "I have a chat conversation per frame" —
/// a single shell-wide bubble that follows focus would surprise users
/// who expect each window's bubble to retain its own session.
///
/// What the controller provides:
///   - Subscribes to `WindowRegistry::frame_added` / `frame_removing`
///     so newly-spawned frames pick up the controller's settings
///     immediately.
///   - Observation point for shell-level operations: visibility toggle
///     across all frames, future telemetry on chat usage, future
///     cross-frame chat-session linking.
///
/// What stays per-frame (and why):
///   - Bubble widget instance: scoped to the frame's dock_manager so
///     reposition + raise events don't have to ping-pong across frames.
///   - Visibility state: per-frame today via `appearance.show_chat_bubble`
///     + frame-local `chat_bubble_->setVisible(...)`. The controller
///     could centralise this in a future iteration; v1 keeps the
///     existing wiring untouched.
///
/// Cumulative effect of Phase 1c + Phase 3-final + Phase 8 controller
/// scaffolds: each "shell concern" (lock, chat, panels, layouts,
/// monitors, telemetry) now has a dedicated coordinator that the rest
/// of the codebase can extend. The widgets themselves stay where they
/// always have been; the *coordination* moves to the shell.
class ChatBubbleController : public QObject {
    Q_OBJECT
  public:
    static ChatBubbleController& instance();

    /// Subscribe to WindowRegistry. Idempotent. Called from
    /// `TerminalShell::initialise`.
    void initialise();

    bool is_initialised() const { return initialised_; }

    /// Number of tracked frames — for tests / debug overlay.
    int tracked_frame_count() const { return tracked_frames_.size(); }

  private slots:
    void on_frame_added(fincept::WindowFrame* w);
    void on_frame_removing(fincept::WindowFrame* w);

  private:
    ChatBubbleController() = default;

    bool initialised_ = false;
    QHash<fincept::WindowFrame*, QPointer<fincept::WindowFrame>> tracked_frames_;
};

} // namespace fincept::ai_chat
