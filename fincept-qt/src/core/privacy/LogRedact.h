#pragma once
// Redaction helpers for log lines and diagnostics — thin forwarders to the
// logger's own (core/logging/LogRedact.h), kept under these names so existing
// callers compile. ONE implementation: change the rules there, not here.
//
//   LOG_INFO("Python", QStringLiteral("Proxy detected proxy=%1").arg(privacy::url_origin(proxy_url)));
//   LOG_INFO("App", QStringLiteral("Layout exported path=%1").arg(privacy::redact_home(path)));
//
//   redact_home(path)    → log_path(): the data root becomes <data>, the install directory <app>,
//                        any home directory (this user's or another's) ~; the rest kept as given.
//   is_under_home(path)  true when `path` is this user's home directory or inside it.
//   redact_url(url)      → log_url(): scheme://host[:port], plus the path for first-party hosts
//                        (the Fincept API) and "/<redacted>" for any other; never userinfo, query
//                        ("?<redacted>") or fragment. Not a URL → "<invalid url>".
//   url_origin(url)      scheme://host[:port] only (proxies, hosts whose path is the credential).
//   mask(secret)         → log_mask(secret, 0): "…" when set, "" when empty — never any part of
//                        the value, not even its length.
//   mask_id(id, keep)    → log_mask(id, keep): "…1234"; ids of `keep` chars or fewer become "…".
//   redact_text(text)    → log_scrub(): URLs, bearer tokens / JWTs, key=value secrets, e-mail
//                        addresses, home directories and 32+ character opaque tokens replaced.
//
// The functions are defined in Logger.cpp: a test using these must link it.

#include "core/logging/LogRedact.h"

#include <QDir>
#include <QString>
#include <QUrl>

namespace fincept::privacy {

namespace detail {

inline Qt::CaseSensitivity path_case() {
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

/// '/' separators whatever the platform.
inline QString slashes(const QString& path) {
    QString p = path;
    p.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return p;
}

/// The home directory with '/' separators and no trailing '/'. `home_override` is for tests.
inline QString home_dir(const QString& home_override = {}) {
    QString home = slashes(home_override.isEmpty() ? QDir::homePath() : home_override);
    while (home.size() > 1 && home.endsWith(QLatin1Char('/')))
        home.chop(1);
    return home;
}

} // namespace detail

/// True when `path` is the home directory or lies inside it.
inline bool is_under_home(const QString& path, const QString& home_override = {}) {
    const QString home = detail::home_dir(home_override);
    if (home.size() <= 1 || path.isEmpty())
        return false; // "/" as home (a service account) would match everything
    const QString p = detail::slashes(path);
    if (!p.startsWith(home, detail::path_case()))
        return false;
    return p.size() == home.size() || p.at(home.size()) == QLatin1Char('/');
}

/// log_path(); `home_override` (tests) replaces the home directory and drops the data / app roots.
inline QString redact_home(const QString& path, const QString& home_override = {}) {
    if (home_override.isEmpty())
        return fincept::log_path(path);
    fincept::log_detail::PathRoots roots;
    roots.home = home_override;
    roots.case_insensitive = detail::path_case() == Qt::CaseInsensitive;
    return fincept::log_detail::log_path_with(path, roots);
}

inline QString redact_url(const QUrl& url) {
    return fincept::log_url(url);
}

inline QString redact_url(const QString& url) {
    return fincept::log_url(url);
}

/// scheme://host[:port] only.
inline QString url_origin(const QUrl& url) {
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty())
        return QStringLiteral("<invalid url>");
    return url.adjusted(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
        .toString();
}

inline QString url_origin(const QString& url) {
    return url_origin(QUrl(url.trimmed()));
}

/// Presence only.
inline QString mask(const QString& secret) {
    return fincept::log_mask(secret, 0);
}

inline QString mask_id(const QString& id, int keep_last = 4) {
    return fincept::log_mask(id, keep_last);
}

/// log_scrub(). `home_override` is accepted for older callers and ignored: log_scrub() hides
/// this user's home directory and any other user's.
inline QString redact_text(const QString& text, const QString& home_override = {}) {
    Q_UNUSED(home_override);
    return fincept::log_scrub(text);
}

} // namespace fincept::privacy
