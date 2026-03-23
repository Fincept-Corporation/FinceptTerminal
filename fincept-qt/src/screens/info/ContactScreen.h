#pragma once
#include <QWidget>

namespace fincept::screens {

/// Contact Us — email, phone, office address, quick actions.
class ContactScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ContactScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();
};

} // namespace fincept::screens
