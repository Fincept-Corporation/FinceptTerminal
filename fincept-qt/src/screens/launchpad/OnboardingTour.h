#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QStackedWidget;

namespace fincept::screens {

/// Phase 9 / decision 10.10: first-run tour mode.
///
/// Shown after the user picks a starting layout from `LaunchpadScreen`'s
/// template picker (the layout-pick step of the onboarding flow). v1 ships
/// a 4-step modal walkthrough — command bar, link badges, panel header,
/// settings — and persists `onboarding.tour_seen=true` in
/// `SettingsRepository` on completion so subsequent launches skip it.
///
/// Re-run path: the user can replay the tour from the Help screen via the
/// `help.tour` action (Phase 9 follow-up — interface stable; static
/// `OnboardingTour::show_for(parent)` already re-entrant).
///
/// Skip-able: every step has a "Skip" button, and the OS close button on
/// the dialog title bar dismisses too. Either records `tour_seen=true`.
class OnboardingTour : public QDialog {
    Q_OBJECT
  public:
    /// Show the tour modally. Idempotent — does nothing if a tour is
    /// already on screen. Returns when the dialog closes.
    static void show_for(QWidget* parent);

    /// Has the user already seen + completed (or skipped) the tour?
    /// Settings-backed — true when `onboarding.tour_seen == "true"`.
    static bool has_been_seen();

    /// Mark the tour as seen. Called on Finish or Skip; also exposed
    /// publicly so a feature-flag / dev-tool can pre-mark it.
    static void mark_seen();

    /// Reset the seen-flag so the tour fires again on next launchpad
    /// surface. Wired to `help.replay_tour` action (TODO: action TBD).
    static void reset_seen();

  protected:
    void closeEvent(QCloseEvent* e) override;

  private:
    explicit OnboardingTour(QWidget* parent);

    void build_ui();
    QWidget* build_step(const QString& title, const QString& body, const QString& tip);
    void on_next();
    void on_back();
    void on_skip();
    void on_finish();
    void update_buttons();

    QStackedWidget* steps_ = nullptr;
    QPushButton* btn_back_ = nullptr;
    QPushButton* btn_next_ = nullptr;
    QPushButton* btn_skip_ = nullptr;
    QLabel* progress_ = nullptr;

    int current_step_ = 0;
    int step_count_ = 0;
    bool finished_ = false;
};

} // namespace fincept::screens
