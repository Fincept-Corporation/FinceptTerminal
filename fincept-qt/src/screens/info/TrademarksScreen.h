#pragma once
#include <QWidget>

class QScrollArea;

namespace fincept::screens {

/// Trademarks — Fincept and third-party trademark acknowledgments.
///
/// Internationalised: all visible strings flow through tr(). Static content is
/// built once in the constructor and rebuilt on QEvent::LanguageChange via the
/// scroll area, avoiding dozens of cached member pointers.
class TrademarksScreen : public QWidget {
    Q_OBJECT
  public:
    explicit TrademarksScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();

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
