#pragma once
#include <QWidget>

namespace fincept::screens {

/// Privacy Policy — scrollable legal document with data collection details.
class PrivacyScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PrivacyScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();
    void navigate_terms();
    void navigate_contact();
};

} // namespace fincept::screens
