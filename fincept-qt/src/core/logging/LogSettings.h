#pragma once
// Saved logging settings (Settings > Logging) — the AppConfig keys main.cpp and
// LoggingSection.cpp already use, in one place:
//   log/global_level      "Trace" | "Debug" | "Info" | "Warn" | "Error" | "Fatal"
//   log/json_mode         bool
//   log/tag_count         N
//   log/tag_<i>_name      tag     (i = 0 … N-1)
//   log/tag_<i>_level     level name
//
// Header-only on purpose: Logger.cpp must not depend on AppConfig (unit tests link
// Logger.cpp alone). An environment override (FINCEPT_LOG_LEVEL / FINCEPT_LOG_TAGS)
// still wins over what is applied here — see Logger::level_overrides().

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace fincept {

inline LogLevel saved_log_level() {
    const QString name = AppConfig::instance().get(QStringLiteral("log/global_level"), QStringLiteral("Info")).toString();
    return Logger::parse_level(name).value_or(LogLevel::Info);
}

inline bool saved_log_json_mode() {
    return AppConfig::instance().get(QStringLiteral("log/json_mode"), false).toBool();
}

inline QHash<QString, LogLevel> saved_log_tag_levels() {
    auto& cfg = AppConfig::instance();
    QHash<QString, LogLevel> tags;
    const int count = cfg.get(QStringLiteral("log/tag_count"), 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString tag = cfg.get(QStringLiteral("log/tag_%1_name").arg(i)).toString().trimmed();
        const auto level = Logger::parse_level(cfg.get(QStringLiteral("log/tag_%1_level").arg(i)).toString());
        if (!tag.isEmpty() && level)
            tags.insert(tag, *level);
    }
    return tags;
}

/// Applies the saved global level, JSON mode and per-tag levels to the Logger
/// (what main.cpp does at start-up today).
inline void load_log_settings() {
    auto& log = Logger::instance();
    log.set_level(saved_log_level());
    log.set_json_mode(saved_log_json_mode());
    log.clear_all_tag_levels();
    const QHash<QString, LogLevel> tags = saved_log_tag_levels();
    for (auto it = tags.cbegin(); it != tags.cend(); ++it)
        log.set_tag_level(it.key(), it.value());
}

/// Saves and applies (what LoggingSection's "Apply & Save" does today). Stale
/// log/tag_<i>_* keys beyond the new count are removed.
inline void save_log_settings(LogLevel global, const QHash<QString, LogLevel>& tags, bool json_mode) {
    auto& cfg = AppConfig::instance();
    auto& log = Logger::instance();
    cfg.set(QStringLiteral("log/global_level"), Logger::level_name(global));
    cfg.set(QStringLiteral("log/json_mode"), json_mode);
    const int old_count = cfg.get(QStringLiteral("log/tag_count"), 0).toInt();
    QStringList names = tags.keys();
    std::sort(names.begin(), names.end());
    int i = 0;
    for (const QString& tag : names) {
        cfg.set(QStringLiteral("log/tag_%1_name").arg(i), tag);
        cfg.set(QStringLiteral("log/tag_%1_level").arg(i), Logger::level_name(tags.value(tag)));
        ++i;
    }
    for (int j = i; j < old_count; ++j) {
        cfg.remove(QStringLiteral("log/tag_%1_name").arg(j));
        cfg.remove(QStringLiteral("log/tag_%1_level").arg(j));
    }
    cfg.set(QStringLiteral("log/tag_count"), i);

    log.set_level(global);
    log.set_json_mode(json_mode);
    log.clear_all_tag_levels();
    for (const QString& tag : names)
        log.set_tag_level(tag, tags.value(tag));
}

} // namespace fincept
