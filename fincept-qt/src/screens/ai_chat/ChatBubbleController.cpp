#include "screens/ai_chat/ChatBubbleController.h"

#include "app/WindowFrame.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"

namespace fincept::ai_chat {

namespace {
constexpr const char* kBubbleTag = "ChatBubble";
} // namespace

ChatBubbleController& ChatBubbleController::instance() {
    static ChatBubbleController s;
    return s;
}

void ChatBubbleController::initialise() {
    if (initialised_)
        return;
    initialised_ = true;

    auto& reg = WindowRegistry::instance();
    connect(&reg, &WindowRegistry::frame_added, this, &ChatBubbleController::on_frame_added);
    connect(&reg, &WindowRegistry::frame_removing, this, &ChatBubbleController::on_frame_removing);
    for (auto* w : reg.frames())
        on_frame_added(w);

    LOG_INFO(kBubbleTag,
             QString("Initialised; tracking %1 frame(s)").arg(tracked_frames_.size()));
}

void ChatBubbleController::on_frame_added(fincept::WindowFrame* w) {
    if (!w) return;
    tracked_frames_.insert(w, QPointer<fincept::WindowFrame>(w));
}

void ChatBubbleController::on_frame_removing(fincept::WindowFrame* w) {
    if (!w) return;
    tracked_frames_.remove(w);
}

} // namespace fincept::ai_chat
