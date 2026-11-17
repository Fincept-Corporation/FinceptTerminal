#pragma once
#include <QEvent>
#include <QLabel>

namespace fincept::ui {

/// Small colored status label (e.g., "CONNECTED" in the success colour, "OFFLINE" in the danger colour).
/// set_custom() takes a colour the caller resolved from the theme.
class StatusBadge : public QLabel {
    Q_OBJECT
  public:
    enum class Status { Connected, Disconnected, Loading, Idle };

    explicit StatusBadge(QWidget* parent = nullptr);
    void set_status(Status s);
    void set_custom(const QString& text, const QString& color);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// Re-apply tr() lookups to the badge text after a language switch.
    void retranslateUi();

    // Track the current preset status so retranslateUi() can re-apply its
    // translated label. set_custom() switches to a caller-supplied (untranslated)
    // string — guarded by custom_ so a language switch leaves it untouched.
    Status status_ = Status::Idle;
    bool custom_ = false;
};

} // namespace fincept::ui
