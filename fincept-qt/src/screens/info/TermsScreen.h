#pragma once
#include <QWidget>

namespace fincept::screens {

/// Terms of Service — scrollable legal document.
class TermsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit TermsScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();
    void navigate_privacy();
    void navigate_contact();
};

} // namespace fincept::screens
