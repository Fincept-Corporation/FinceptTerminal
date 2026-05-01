#pragma once

#include <QFrame>
#include <QString>

class QLineEdit;
class QLabel;

namespace fincept::ui {

/// Phase 9: persistent status-line command input on every frame.
/// Reads `ActionRegistry`, parses with `CommandParser`, invokes via
/// `ActionRegistry::invoke(id, ctx)`. Bloomberg-style "just type" UX.
///
/// Sits at the bottom of WindowFrame above the status bar. Hidden by
/// default — toggle with Ctrl+\ or via the `cmdbar.toggle` action.
/// Phase 9 follow-up adds inline autocomplete + ?-prefix help.
class QuickCommandBar : public QFrame {
    Q_OBJECT
  public:
    explicit QuickCommandBar(QWidget* parent = nullptr);

    /// Toggle visibility. Focuses the input on show.
    void toggle_visible();

    /// Surface the bar (show + focus). Used when palette/launchpad
    /// dispatch a "type a command" action.
    void surface();

  private:
    void on_submit();
    void show_hint(const QString& text, bool is_error);

    QLineEdit* input_ = nullptr;
    QLabel* hint_ = nullptr;
};

} // namespace fincept::ui
