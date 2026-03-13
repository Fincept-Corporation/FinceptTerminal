#pragma once
// Application state machine — routes between auth screens and dashboard
// Mirrors App.tsx screen routing logic

// Fullscreen toggle (implemented in main.cpp)
void toggle_fullscreen();

namespace fincept {

enum class AppScreen {
    Loading,
    Login,
    Register,
    ForgotPassword,
    Pricing,
    Dashboard
};

class App {
public:
    void initialize();
    void render();
    void shutdown();

private:
    AppScreen current_screen_ = AppScreen::Loading;
    AppScreen next_screen_ = AppScreen::Loading;
    bool initialized_ = false;
    bool came_from_login_ = false;
    bool has_chosen_free_ = false;
    int active_tab_ = 0;

    void render_top_bar();
    void render_tab_bar();
    void render_footer();
    void render_loading();
    void apply_auto_navigation();
};

} // namespace fincept
