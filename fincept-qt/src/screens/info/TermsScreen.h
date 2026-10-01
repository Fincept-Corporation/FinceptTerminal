#pragma once
#include <QWidget>

class QScrollArea;

namespace fincept::screens {

/// Terms of Service — scrollable legal document.
///
/// Internationalised: all visible strings flow through tr(). Static content is
/// built once in the constructor and rebuilt on QEvent::LanguageChange via the
/// scroll area, avoiding dozens of cached member pointers.
class TermsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit TermsScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();
    void navigate_privacy();
    void navigate_contact();

  protected:
    void changeEvent(QEvent* event) override;
    /// The page is built on first show, not in the constructor — see showEvent().
    void showEvent(QShowEvent* event) override;

  private:
    QWidget* build_page();
    QScrollArea* scroll_ = nullptr;
    bool page_built_ = false;
};

} // namespace fincept::screens
