#pragma once

#include <QFrame>
#include <QString>

class QLineEdit;
class QLabel;

namespace fincept::ui {

/// "Just type" command input above the status bar. Hidden by default; toggle with Ctrl+\ or `cmdbar.toggle`.
class QuickCommandBar : public QFrame {
    Q_OBJECT
  public:
    explicit QuickCommandBar(QWidget* parent = nullptr);

    void toggle_visible();

    /// Show + focus. Used by palette/launchpad "type a command" dispatch.
    void surface();

  private:
    void on_submit();
    void show_hint(const QString& text, bool is_error);

    QLineEdit* input_ = nullptr;
    QLabel* hint_ = nullptr;
};

} // namespace fincept::ui
