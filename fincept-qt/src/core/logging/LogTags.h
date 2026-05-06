#pragma once

// Canonical tag names used by the logging system.
//
// Usage:
//   LOG_INFO(fincept::log::kApp, "starting up");
//
// This header is purely additive — existing call sites that pass
// string literals (e.g. LOG_INFO("App", "...")) continue to work
// unchanged. Prefer these constants for new code so tag names
// stay consistent across the codebase (no drift between "Auth"
// vs "auth" vs "AUTH").

namespace fincept::log {

inline constexpr const char* kApp           = "App";
inline constexpr const char* kAuth          = "Auth";
inline constexpr const char* kDb            = "DB";
inline constexpr const char* kCacheDb       = "CacheDB";
inline constexpr const char* kHttp          = "HTTP";
inline constexpr const char* kWebSocket     = "WebSocket";
inline constexpr const char* kPython        = "Python";
inline constexpr const char* kTheme         = "ThemeManager";
inline constexpr const char* kMainWindow    = "WindowFrame";   // legacy name kept as kMainWindow for one release; new code can use kWindowFrame
inline constexpr const char* kWindowFrame   = "WindowFrame";
inline constexpr const char* kRouter        = "ScreenRouter";
inline constexpr const char* kMarketData    = "MarketData";
inline constexpr const char* kNews          = "News";
inline constexpr const char* kExchange      = "Exchange";
inline constexpr const char* kBroker        = "Broker";
inline constexpr const char* kMcp           = "MCP";
inline constexpr const char* kAiChat        = "AiChat";
inline constexpr const char* kUpdater       = "UpdateService";
inline constexpr const char* kWorkspace     = "Workspace";
inline constexpr const char* kWorkflow      = "Workflow";
inline constexpr const char* kDataHub       = "DataHub";
inline constexpr const char* kProfile       = "ProfileManager";

} // namespace fincept::log
