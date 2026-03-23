#pragma once
#include <QWidget>

namespace fincept::screens {

/// Help — FAQ, quick actions (create account, reset password), contact info.
class HelpScreen : public QWidget {
    Q_OBJECT
  public:
    explicit HelpScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();
    void navigate_register();
    void navigate_forgot_password();
};

} // namespace fincept::screens
