#pragma once
// MonitorPickerDialog — visual monitor map shown when the user requests a
// new window on a multi-monitor system. The dialog draws each connected
// QScreen as a scaled rectangle (preserving the virtual-desktop layout) and
// lets the user click the monitor where the new window should appear.
//
// Use MonitorPickerDialog::pick() instead of constructing directly — it
// short-circuits the dialog when only one monitor is connected.

#include <QDialog>

class QScreen;

namespace fincept {

class MonitorPickerDialog : public QDialog {
    Q_OBJECT
  public:
    explicit MonitorPickerDialog(QWidget* parent, QScreen* current);

    QScreen* picked_screen() const { return picked_; }

    /// Returns the chosen screen, or nullptr if the user cancelled. If only
    /// a single monitor is connected the dialog is skipped and that monitor
    /// is returned directly. `current` is visually highlighted as "the
    /// monitor the request came from" (purely a cue — does not pre-select).
    static QScreen* pick(QWidget* parent, QScreen* current);

  private:
    QScreen* picked_ = nullptr;
};

} // namespace fincept
