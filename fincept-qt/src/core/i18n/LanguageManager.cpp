#include "core/i18n/LanguageManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace fincept::i18n {

namespace {
constexpr const char* kSettingsKey = "general.language";
constexpr const char* kDefaultCode = "en";
} // namespace

LanguageManager& LanguageManager::instance() {
    static LanguageManager mgr;
    return mgr;
}

QStringList LanguageManager::supported_languages() {
    return {QStringLiteral("en"),
            QStringLiteral("zh_CN"),
            QStringLiteral("zh_HK"),
            QStringLiteral("id_ID"),
            QStringLiteral("vi_VN"),
            QStringLiteral("tr_TR"),
            QStringLiteral("de_DE"),
            QStringLiteral("pt_BR"),
            QStringLiteral("es_ES"),
            QStringLiteral("fr_FR"),
            QStringLiteral("it_IT")};
}

QString LanguageManager::native_name(const QString& code) {
    if (code == QLatin1String("en"))    return QStringLiteral("English");
    if (code == QLatin1String("zh_CN")) return QString::fromUtf8("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"); // 简体中文
    if (code == QLatin1String("zh_HK")) return QString::fromUtf8("\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87 (\xe9\xa6\x99\xe6\xb8\xaf)"); // 繁體中文 (香港)
    if (code == QLatin1String("id_ID")) return QStringLiteral("Bahasa Indonesia");
    if (code == QLatin1String("vi_VN")) return QString::fromUtf8("Ti\xe1\xba\xbfng Vi\xe1\xbb\x87t"); // Tiếng Việt
    if (code == QLatin1String("tr_TR")) return QString::fromUtf8("T\xc3\xbcrk\xc3\xa7""e"); // Türkçe
    if (code == QLatin1String("de_DE")) return QStringLiteral("Deutsch");
    if (code == QLatin1String("pt_BR")) return QString::fromUtf8("Portugu\xc3\xaas (Brasil)"); // Português (Brasil)
    if (code == QLatin1String("es_ES")) return QString::fromUtf8("Espa\xc3\xb1ol"); // Español
    if (code == QLatin1String("fr_FR")) return QString::fromUtf8("Fran\xc3\xa7""ais"); // Français
    if (code == QLatin1String("it_IT")) return QStringLiteral("Italiano");
    return code;
}

void LanguageManager::initialize() {
    const auto r = SettingsRepository::instance().get(
        QString::fromLatin1(kSettingsKey), QString::fromLatin1(kDefaultCode));
    const QString code = r.is_ok() && !r.value().isEmpty()
                             ? r.value()
                             : QString::fromLatin1(kDefaultCode);
    set_language(code);
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
