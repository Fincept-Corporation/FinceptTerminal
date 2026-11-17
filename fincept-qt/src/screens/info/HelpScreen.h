#pragma once
#include <QWidget>

class QScrollArea;

namespace fincept::screens {

/// Help Center: where to get help (Docs, Support, the tour, Discord), short
/// answers to the problems users actually run into, what data the terminal
/// sends, and the version.
///
/// Two hosts: a dock panel after sign-in, and the pre-login info stack
/// (WindowFrame connects the navigate_* signals there). build_page() tells them
/// apart and swaps the actions that need a signed-in terminal (tickets, tour,
/// About) for ones that work before sign-in (e-mail, password reset).
///
/// Static text: the page is built on first show and rebuilt on LanguageChange.
/// Every widget is styled by one objectName-selector stylesheet on the root,
/// rebuilt on ThemeManager::appearance_settled (set only when its text changed).
class HelpScreen : public QWidget {
    Q_OBJECT
  public:
    explicit HelpScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();
    /// Connected by the pre-login info stack; this page no longer offers sign-up.
    void navigate_register();
    void navigate_forgot_password();

  protected:
    void changeEvent(QEvent* event) override;
    /// The page is built on first show, not in the constructor — see showEvent().
    void showEvent(QShowEvent* event) override;

  private:
    QWidget* build_page();
    void refresh_theme();

    QScrollArea* scroll_ = nullptr;
    bool page_built_ = false;
};

} // namespace fincept::screens
