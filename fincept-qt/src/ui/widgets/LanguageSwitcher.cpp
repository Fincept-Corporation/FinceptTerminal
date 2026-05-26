#include "ui/widgets/LanguageSwitcher.h"

#include "core/i18n/LanguageManager.h"
#include "ui/theme/Theme.h"

#include <QActionGroup>
#include <QEvent>
#include <QMenu>

namespace fincept::ui {

LanguageSwitcher::LanguageSwitcher(QWidget* parent) : QPushButton(parent) {
    setCursor(Qt::PointingHandCursor);
    // Slight amber-tinted background so it reads as an interactive control
    // rather than a label, while still staying quiet on the grid backdrop.
    setStyleSheet(QString("QPushButton {"
                          "  background: rgba(217,119,6,0.06);"
                          "  color: %1;"
                          "  border: 1px solid %2;"
                          "  padding: 5px 12px;"
                          "  font-size: 12px; font-weight: 700;"
                          "  font-family: 'Consolas','Courier New',monospace;"
                          "  letter-spacing: 0.5px;"
                          "}"
                          "QPushButton:hover {"
                          "  color: %3;"
                          "  border-color: %4;"
                          "  background: rgba(217,119,6,0.12);"
                          "}"
                          "QPushButton:pressed {"
                          "  background: rgba(217,119,6,0.18);"
                          "}")
                      .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(),
                           colors::AMBER(), colors::BORDER_BRIGHT()));

    rebuild_label();

    connect(this, &QPushButton::clicked, this, &LanguageSwitcher::show_menu);
    connect(&i18n::LanguageManager::instance(), &i18n::LanguageManager::language_changed,
            this, [this](const QString&) { rebuild_label(); });
}

void LanguageSwitcher::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        rebuild_label();
    }
    QPushButton::changeEvent(event);
}

void LanguageSwitcher::rebuild_label() {
    const QString code = i18n::LanguageManager::instance().current_language();
    const QString resolved = code.isEmpty() ? QStringLiteral("en") : code;
    const QString name = i18n::LanguageManager::native_name(resolved);
    // U+25BE = small down-pointing triangle. Two spaces of padding so the
    // chevron doesn't crowd the language name in a monospace font.
    setText(name + QStringLiteral("  ▾"));
}

void LanguageSwitcher::show_menu() {
    QMenu menu(this);
    menu.setStyleSheet(QString("QMenu {"
                               "  background: %1;"
                               "  color: %2;"
                               "  border: 1px solid %3;"
                               "  padding: 4px 0;"
                               "  font-family: 'Consolas','Courier New',monospace;"
                               "  font-size: 13px;"
                               "}"
                               "QMenu::item {"
                               "  padding: 6px 18px;"
                               "}"
                               "QMenu::item:selected {"
                               "  background: rgba(217,119,6,0.15);"
                               "  color: %4;"
                               "}"
                               "QMenu::item:checked {"
                               "  color: %4;"
                               "}")
                           .arg(colors::BG_SURFACE(), colors::TEXT_SECONDARY(),
                                colors::BORDER_DIM(), colors::AMBER()));

    auto* group = new QActionGroup(&menu);
    const QString current_code = i18n::LanguageManager::instance().current_language();

    for (const QString& code : i18n::LanguageManager::supported_languages()) {
        const QString name = i18n::LanguageManager::native_name(code);
        auto* action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(code == current_code);
        group->addAction(action);
        // Capture by value — the action outlives this loop iteration.
        connect(action, &QAction::triggered, this, [code]() {
            i18n::LanguageManager::instance().set_language(code);
        });
    }

    // Right-align the menu under the button so it doesn't overflow off-screen
    // when the button itself sits near the right edge of the window.
    const QPoint anchor = mapToGlobal(QPoint(width() - menu.sizeHint().width(), height()));
    menu.exec(anchor);
}

} // namespace fincept::ui
