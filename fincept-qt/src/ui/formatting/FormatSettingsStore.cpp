#include "ui/formatting/FormatSettingsStore.h"

#include "core/i18n/LanguageManager.h"
#include "core/logging/Logger.h"
#include "core/market/MarketProfile.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/formatting/FormatSettings.h"

#include <QHash>
#include <QLocale>
#include <QString>

#include <memory>

namespace fincept::ui::formatting {

namespace fmtstore_detail {

constexpr const char* kFmtStoreMigratedKey = "appearance.currency_settings_migrated";
constexpr const char* kFmtStoreLegacyCurrencyKey = "general.currency";
constexpr const char* kFmtStoreDisplayCurrencyKey = "appearance.display_currency";

/// What Automatic resolves to right now (region / system locale / USD).
QString fmtstore_auto_currency() {
    auto& profiles = core::market::MarketProfileStore::instance();
    return FormatSettings::resolve_auto_display_currency(profiles.is_user_choice(), profiles.current().base_currency,
                                                         QLocale::system());
}

/// One-time migration of the Settings > General display currency. Returns the
/// code to adopt ("" = none). Writes the marker (and the adopted code) only
/// when every read succeeded, so a transient DB error retries next start.
QString fmtstore_migrate_once(SettingsRepository& repo, const QHash<QString, QString>& stored, bool display_read_ok) {
    const auto marker = repo.get(QString::fromLatin1(kFmtStoreMigratedKey), QString());
    if (marker.is_err() || !marker.value().isEmpty() || !display_read_ok)
        return {};
    QString adopted;
    if (stored.value(QString::fromLatin1(kFmtStoreDisplayCurrencyKey)).trimmed().isEmpty()) {
        const auto legacy = repo.get(QString::fromLatin1(kFmtStoreLegacyCurrencyKey), QString());
        if (legacy.is_err())
            return {};
        adopted = FormatSettings::migrate_legacy_display_currency(legacy.value());
        if (!adopted.isEmpty()) {
            const auto w = repo.set(QString::fromLatin1(kFmtStoreDisplayCurrencyKey), adopted,
                                    QStringLiteral("appearance"));
            if (w.is_err()) {
                LOG_WARN("Format", QString("could not save the migrated display currency: %1")
                                       .arg(QString::fromStdString(w.error())));
                return adopted; // used this session; the marker is not written, so it retries
            }
        }
    }
    const auto w = repo.set(QString::fromLatin1(kFmtStoreMigratedKey), QStringLiteral("1"), QStringLiteral("appearance"));
    if (w.is_err())
        LOG_WARN("Format", QString("could not save the currency migration marker: %1")
                               .arg(QString::fromStdString(w.error())));
    if (!adopted.isEmpty())
        LOG_INFO("Format", QString("Display currency %1 carried over from Settings > General").arg(adopted));
    return adopted;
}

} // namespace fmtstore_detail

void load_format_settings() {
    static bool loaded = false;
    if (loaded)
        return;
    loaded = true;

    auto& fs = FormatSettings::instance();
    auto& repo = SettingsRepository::instance();

    QHash<QString, QString> stored;
    bool display_read_ok = false;
    for (const QString& key : FormatSettings::setting_keys()) {
        const auto r = repo.get(key, QString());
        if (r.is_err()) {
            // Unknown, not absent: use the default this session, never write it back.
            LOG_WARN("Format", QString("settings read failed for '%1', using the default for this session: %2")
                                   .arg(key, QString::fromStdString(r.error())));
            continue;
        }
        if (key == QLatin1String(fmtstore_detail::kFmtStoreDisplayCurrencyKey))
            display_read_ok = true;
        stored.insert(key, r.value());
    }

    // Resolve Automatic first so the first changed() already carries it.
    fs.set_auto_display_currency(fmtstore_detail::fmtstore_auto_currency());

    const QString migrated = fmtstore_detail::fmtstore_migrate_once(repo, stored, display_read_ok);
    if (!migrated.isEmpty())
        stored.insert(QString::fromLatin1(fmtstore_detail::kFmtStoreDisplayCurrencyKey), migrated);
    fs.apply_settings_values(stored);

    // What the repository holds (or is assumed to hold) right now. Only keys
    // that move away from this are written.
    auto baseline = std::make_shared<QHash<QString, QString>>(fs.settings_values());
    QObject::connect(&fs, &FormatSettings::changed, &fs, [baseline]() {
        const QHash<QString, QString> now = FormatSettings::instance().settings_values();
        for (auto it = now.constBegin(); it != now.constEnd(); ++it) {
            if (baseline->value(it.key()) == it.value())
                continue;
            const auto w = SettingsRepository::instance().set(it.key(), it.value(), QStringLiteral("appearance"));
            if (w.is_err()) {
                LOG_WARN("Format", QString("could not save '%1': %2").arg(it.key(), QString::fromStdString(w.error())));
                continue;
            }
            baseline->insert(it.key(), it.value());
        }
    });

    QObject::connect(&fincept::i18n::LanguageManager::instance(), &fincept::i18n::LanguageManager::language_changed,
                     &fs, [](const QString&) { FormatSettings::instance().notify_app_locale_changed(); });

    // Automatic follows the user's market region.
    QObject::connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed,
                     &fs, [](const QString&) {
                         FormatSettings::instance().set_auto_display_currency(fmtstore_detail::fmtstore_auto_currency());
                     });

    const auto v = fs.settings_values();
    LOG_DEBUG("Format", QString("Display formats: locale=%1 compact=%2 timezone=%3 clock=%4 currency=%5(%6) style=%7 "
                                "negative=%8 grouping=%9 decimals=%10 minor=%11")
                            .arg(fs.locale().bcp47Name(), v.value(QStringLiteral("appearance.compact_numbers")),
                                 v.value(QStringLiteral("appearance.display_timezone")),
                                 v.value(QStringLiteral("appearance.clock")), fs.display_currency(),
                                 fs.display_currency_is_auto() ? QStringLiteral("auto") : QStringLiteral("set"),
                                 v.value(QStringLiteral("appearance.currency_symbol_style")),
                                 v.value(QStringLiteral("appearance.negative_style")),
                                 v.value(QStringLiteral("appearance.number_grouping")))
                            .arg(v.value(QStringLiteral("appearance.money_decimals")),
                                 v.value(QStringLiteral("appearance.minor_units"))));
}

} // namespace fincept::ui::formatting
