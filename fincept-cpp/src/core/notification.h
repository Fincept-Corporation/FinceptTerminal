#pragma once
// Notification System — cross-platform local OS notifications + ImGui fallback
//
// Usage:
//   fincept::core::notify::send("Title", "Body", NotifyLevel::Success);
//   fincept::core::notify::render_toasts();  // call once per frame in main render loop
//
// Platforms:
//   Windows  — WinRT ToastNotificationManager (Windows 10+)
//   macOS    — NSUserNotificationCenter (Obj-C++ bridge)
//   Linux    — D-Bus org.freedesktop.Notifications
//   Fallback — ImGui overlay toast (always works)

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace fincept::core {

enum class NotifyLevel { Info, Success, Warning, Error };

namespace notify {

// Send a notification: ImGui toast + OS notification + enabled external providers.
// Thread-safe — can be called from any thread.
void send(const std::string& title, const std::string& body,
          NotifyLevel level = NotifyLevel::Info);

// Render in-app ImGui toast overlay. Call once per frame from main render loop.
void render_toasts();

// Clear all pending toasts
void clear();

// Enable/disable OS-level notifications (ImGui toasts always show)
void set_os_notifications_enabled(bool enabled);
bool os_notifications_enabled();

// --- External provider routing ---
// Load provider config from DB (call at startup and after settings change)
void load_provider_config();

// Send to all enabled external providers (called internally by send())
void send_to_providers(const std::string& title, const std::string& body,
                       NotifyLevel level);

// Send to a specific provider by ID (for test button in settings)
bool test_provider(const std::string& provider_id);

// Event filter check
bool is_level_enabled(NotifyLevel level);

} // namespace notify
} // namespace fincept::core
