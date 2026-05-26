#pragma once
// LanguageSwitcher — compact language picker for pre-login screens.
//
// Renders as a small Obsidian-styled button showing the active language's
// native name with a chevron. Clicking opens a popup menu listing all
// supported languages; selecting one calls LanguageManager::set_language(),
// which swaps the QTranslator and posts QEvent::LanguageChange to every
// widget in the app (including the auth/setup card currently on screen).
//
// Also subscribes to LanguageManager::language_changed so the button updates
// itself when the language is changed elsewhere (e.g. Settings → General).

#include <QPushButton>

namespace fincept::ui {

class LanguageSwitcher : public QPushButton {
    Q_OBJECT
  public:
    explicit LanguageSwitcher(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void rebuild_label();
    void show_menu();
};

} // namespace fincept::ui
