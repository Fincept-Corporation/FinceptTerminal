#pragma once

#include <QEvent>
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
    /// Show + focus with `text` already typed (cursor at the end) and a hint — the
    /// palette hands over an action whose arguments must be typed ("link set_group ").
    void surface_with(const QString& text, const QString& hint = {});

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void on_submit();
    void show_hint(const QString& text, bool is_error);
    void retranslateUi();

    QLineEdit* input_ = nullptr;
    QLabel* hint_ = nullptr;
    quint64 resolve_id_ = 0; ///< InstrumentResolver request of the ticker being looked up (0 = none)
};

} // namespace fincept::ui
