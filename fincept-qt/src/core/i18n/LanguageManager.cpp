#include "core/i18n/LanguageManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace fincept::i18n {

namespace {
constexpr const char* kSettingsKey = "general.language";
// Default language is forced to Thai on first launch. Only Thai + English ship.
constexpr const char* kDefaultCode = "th_TH";
} // namespace

LanguageManager& LanguageManager::instance() {
    static LanguageManager mgr;
    return mgr;
}

QStringList LanguageManager::supported_languages() {
    // Only Thai and English ship. Thai is the default; English is the base
    // (source) language with no .qm.
    return {QStringLiteral("th_TH"), QStringLiteral("en")};
}

QString LanguageManager::native_name(const QString& code) {
    if (code == QLatin1String("th_TH")) return QString::fromUtf8("\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2"); // ไทย
    if (code == QLatin1String("en"))    return QStringLiteral("English");
    return code;
}

void LanguageManager::initialize() {
    auto& repo = SettingsRepository::instance();

    // One-time migration to the Thai-default policy. Older builds auto-detected
    // and persisted the OS locale (commonly "en"), which would otherwise win
    // over the forced Thai default. On the first run under this policy we drop
    // that stale value and force Thai. Explicit language switches the user makes
    // afterwards persist normally via set_language() and are honoured.
    const auto migrated = repo.get(QStringLiteral("general.language_thai_default_v1"), QString());
    const bool already_migrated = migrated.is_ok() && migrated.value() == QLatin1String("1");

    QString code;
    if (already_migrated) {
        const auto r = repo.get(QString::fromLatin1(kSettingsKey), QString());
        code = r.is_ok() ? r.value() : QString();
    } else {
        repo.set(QStringLiteral("general.language_thai_default_v1"), QStringLiteral("1"),
                 QStringLiteral("general"));
        LOG_INFO("i18n", QStringLiteral("Applying Thai-default policy — forcing default language"));
    }

    // Only Thai + English ship. Anything unsupported (first launch, stale locale,
    // or the migration above) falls back to the forced Thai default.
    if (!supported_languages().contains(code)) {
        code = QString::fromLatin1(kDefaultCode); // force Thai default
        LOG_INFO("i18n", QStringLiteral("Defaulting language to %1").arg(code));
    }

    set_language(code);
}

QString LanguageManager::detect_system_language() {
    const auto supported = supported_languages();

    // QLocale returns a preference-ordered list. Use the underscore separator
    // so the strings match our .qm naming convention (e.g. "zh_CN" not "zh-CN").
    const QStringList preferred =
        QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore);

    for (const QString& ui : preferred) {
        // Strip script/region tags from longest to shortest (e.g.
        // "zh_Hans_CN" → "zh_Hans" → "zh") and try each form against our
        // supported set. This catches both exact matches ("zh_CN") and
        // language-only matches ("en").
        QString candidate = ui;
        while (!candidate.isEmpty()) {
            if (supported.contains(candidate))
                return candidate;
            const int last = candidate.lastIndexOf(QLatin1Char('_'));
            if (last < 0) break;
            candidate = candidate.left(last);
        }
        // No exact or shorter match — fall back to any supported locale that
        // shares the same language tag (e.g. system "fr_CA" → our "fr_FR").
        const QString lang_prefix = ui.section(QLatin1Char('_'), 0, 0);
        for (const QString& s : supported) {
            if (s.section(QLatin1Char('_'), 0, 0) == lang_prefix)
                return s;
        }
    }
    return QString();
}

void LanguageManager::set_language(const QString& code) {
    // No-op if already active. We still flow through the rest of the function
    // on a "different translator load" path (e.g. retry after a missing .qm).
    if (code == current_code_ && translator_) {
        return;
    }

    if (translator_) {
        QCoreApplication::removeTranslator(translator_);
        delete translator_;
        translator_ = nullptr;
    }

    // English is the source language — no .qm shipped. Removing the previous
    // translator is enough to fall back to the in-source strings.
    if (code != QLatin1String("en")) {
        auto* t = new QTranslator(qApp);
        const QString resource_path = QStringLiteral(":/i18n/fincept_%1.qm").arg(code);
        if (t->load(resource_path)) {
            QCoreApplication::installTranslator(t);
            translator_ = t;
            LOG_INFO("i18n", QStringLiteral("Installed translator: %1").arg(code));
        } else {
            LOG_WARN("i18n", QStringLiteral("Failed to load translator %1 from %2 — falling back to English")
                                 .arg(code, resource_path));
            delete t;
        }
    }

    current_code_ = code;
    QLocale::setDefault(QLocale(code));

    // Persist the choice. The repository call is cheap; we accept the small
    // overhead on startup so the value is always in sync with what's running.
    SettingsRepository::instance().set(
        QString::fromLatin1(kSettingsKey), code, QStringLiteral("general"));

    emit language_changed(code);
}

} // namespace fincept::i18n
