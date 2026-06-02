#pragma once
#include <QWidget>

class QScrollArea;

namespace fincept::screens {

/// Privacy Policy — scrollable legal document with data collection details.
///
/// Internationalised: all visible strings flow through tr(). Static content is
/// built once in the constructor and rebuilt on QEvent::LanguageChange via the
/// scroll area, avoiding dozens of cached member pointers.
class PrivacyScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PrivacyScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();
    void navigate_terms();
    void navigate_contact();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    QWidget* build_page();
    QScrollArea* scroll_ = nullptr;
};

} // namespace fincept::screens
