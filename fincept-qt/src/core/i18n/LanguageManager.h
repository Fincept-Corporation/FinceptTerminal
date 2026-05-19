#pragma once
// LanguageManager — owns the active QTranslator and the user's language choice.
//
// Translations are compiled from .ts files under fincept-qt/translations/ at
// build time (qt6_add_lrelease) and embedded in the Qt resource system at
// :/i18n/fincept_<locale>.qm. English is the source language — no .qm exists
// for it, so set_language("en") just removes any installed translator.
//
// Changing the language at runtime triggers QEvent::LanguageChange on every
// top-level widget, which screens handle by overriding changeEvent() and
// calling their own retranslateUi() helper. Non-widget code can react via
// the language_changed signal.

#include <QObject>
#include <QString>
#include <QStringList>

class QTranslator;

namespace fincept::i18n {

class LanguageManager : public QObject {
    Q_OBJECT
  public:
    static LanguageManager& instance();

    /// Locale codes shipped with the build. "en" is always present (base).
    static QStringList supported_languages();

    /// Display name in the language's own script — used in the settings combo.
    static QString native_name(const QString& code);

    /// Read the persisted choice from SettingsRepository and install the
    /// corresponding translator. Safe to call once on startup before any
    /// windows are shown. When no preference has been saved yet (first
    /// launch), falls back to detect_system_language() so users see the UI
    /// in their OS locale immediately.
    void initialize();

    /// Walk QLocale::system().uiLanguages() (the user's preferred locales,
    /// in order) and return the first one we ship a .qm for. Falls back to
    /// the closest language match (e.g. system "fr_CA" → our "fr_FR") and
    /// returns an empty string if nothing matches.
    static QString detect_system_language();

    /// Switch language. Persists the choice, swaps the QTranslator, and emits
    /// language_changed. Qt itself posts QEvent::LanguageChange to every
    /// top-level widget as a side effect of install/removeTranslator.
    void set_language(const QString& code);

    QString current_language() const { return current_code_; }

  signals:
    void language_changed(const QString& code);

  private:
    LanguageManager() = default;
    QString current_code_;
    QTranslator* translator_ = nullptr;
};

} // namespace fincept::i18n
