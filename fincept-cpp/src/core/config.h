#pragma once
// Application configuration — centralized constants
// All API URLs, timeouts, defaults in one place

#include <string>

namespace fincept::config {

// API
constexpr const char* API_BASE_URL     = "https://api.fincept.in";
constexpr const char* API_VERSION      = "v1";
constexpr long        HTTP_TIMEOUT_SEC = 30;
constexpr long        HTTP_CONNECT_SEC = 10;

// App
constexpr const char* APP_NAME    = "Fincept Terminal";
constexpr const char* APP_VERSION = "4.0.0";
constexpr int         VERSION_MAJOR = 4;
constexpr int         VERSION_MINOR = 0;
constexpr int         VERSION_PATCH = 0;

// Data refresh intervals (seconds)
constexpr float MARKET_REFRESH_INTERVAL = 600.0f;  // 10 min
constexpr float NEWS_REFRESH_INTERVAL   = 300.0f;  // 5 min
constexpr float TICKER_SCROLL_SPEED     = 60.0f;

// Python
constexpr const char* PYTHON_SCRIPTS_DIR = "scripts";
constexpr const char* PYTHON_VENV_DIR    = "fincept_env";

// Storage
constexpr const char* DB_FILENAME        = "fincept_terminal.db";
constexpr const char* SETTINGS_FILENAME  = "settings.json";

} // namespace fincept::config
