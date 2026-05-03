#pragma once

#include <QMainWindow>

class QLabel;
class QListWidget;
class QPushButton;
class QVBoxLayout;

namespace fincept::screens {

/// Phase 9 trim: small persistent window shown when the user closes the
/// last WindowFrame. Replaces `lastWindowClosed → quit()` so the user can
/// open a new window or switch layouts without re-launching the .exe.
///
/// v1 is intentionally minimal — three buttons, a placeholder recent-layouts
/// list, no command bar. Phase 9's full Launchpad lands the command-bar
/// integration, fuzzy palette, real recent-layouts pulled from
/// LayoutCatalog (Phase 6 deliverable), and onboarding tour.
///
/// Lifecycle:
///   - TerminalShell shows it on `lastWindowClosed` if it isn't already
///     visible. Constructed lazily once per session; reused on subsequent
///     last-window events.
///   - Closing it via the OS close button: quits the application
///     (`QCoreApplication::quit()`).
///   - Opening a new WindowFrame from a button hides the Launchpad
///     (it stays alive in memory; cheaper to hide+show than recreate).
///
/// Sizing: ~480×320, centred on primary screen. Not resizable to keep
/// the visual identity of "this is a portal, not a workspace."
class LaunchpadScreen : public QMainWindow {
    Q_OBJECT
  public:
    static LaunchpadScreen* instance();

    /// Show the launchpad window (centre on primary screen, raise, focus).
    /// No-op if already visible.
    void surface();

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    explicit LaunchpadScreen(QWidget* parent = nullptr);

    void on_new_window();
    void on_switch_profile();
    void on_open_layout();
    void refresh_recent_layouts();

    QLabel* greeting_ = nullptr;
    QListWidget* recent_layouts_ = nullptr;
    QPushButton* btn_new_window_ = nullptr;
    QPushButton* btn_switch_profile_ = nullptr;
    QPushButton* btn_open_layout_ = nullptr;
};

} // namespace fincept::screens
