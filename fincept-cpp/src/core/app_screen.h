#pragma once
// AppScreen enum and IScreen interface
// Extracted to break circular dependency (app.h <-> auth screens)

namespace fincept {

// Screen identifiers for navigation state machine
enum class AppScreen {
    Loading,
    PythonSetup,
    Login,
    Register,
    ForgotPassword,
    Pricing,
    Dashboard
};

// Whether a screen should show the app chrome (menu bar, tab bar, footer)
inline bool screen_has_chrome(AppScreen s) {
    switch (s) {
        case AppScreen::Dashboard:
        case AppScreen::Pricing:
            return true;
        default:
            return false;
    }
}

} // namespace fincept
