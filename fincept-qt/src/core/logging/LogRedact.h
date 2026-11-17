#pragma once
// Redaction helpers for log lines — policy B.6. Pure functions over immutable / lock-guarded
// roots; thread-safe; cheap enough to call inside a LOG_* argument (they only run when the
// line is written).
//
//   LOG_WARN("HTTP", logf("HTTP request failed", {{"status", 502}, {"url", log_url(url)}}));
//   LOG_INFO("DB", logf("Database opened", {{"path", log_path(file)}}));
//   LOG_AUDIT("Orders", "Order submitted", {{"venue", "zerodha"}, {"account", log_mask(client_id)}, ...});
//
// What is never logged (policy B.6) — and therefore never passed to these helpers either:
// passwords, PINs, TOTP codes, API keys, session / bearer tokens, OAuth codes, signatures of
// signed requests, private keys, seed phrases, cookies, Authorization headers. Not the value,
// not a prefix or suffix, not its length. For a secret, log only the setting NAME
// (key=broker.api_key) or, at most, that one is present: log_mask(secret, 0) → "…".

#include <QString>

QT_FORWARD_DECLARE_CLASS(QUrl)

namespace fincept {

/// URL for a log line: scheme://host[:port] plus, for a first-party host (the Fincept API host,
/// api.fincept.in, and any host registered with log_url_add_first_party_host), the path.
/// Never the query (written as "?<redacted>"), the fragment or user:password@. For any other
/// host the path is written as "/<redacted>" (webhook and bot URLs carry the secret in the
/// path). A relative API path ("/user/login?x=1") keeps its path. Anything that is not an
/// absolute URL with a host or a relative path is written as "<invalid url>".
///   https://u:p@hooks.slack.com/services/T0/B1/xyz → https://hooks.slack.com/<redacted>
///   https://api.fincept.in/market/quote?apikey=k   → https://api.fincept.in/market/quote?<redacted>
///   http://user:pass@proxy.corp:3128                → http://proxy.corp:3128
QString log_url(const QUrl& url);
QString log_url(const QString& url);

/// Register a host whose URL paths may be logged (HttpClient: the configured API base host).
void log_url_add_first_party_host(const QString& host);

/// Path for a log line: the data root becomes <data>, the install directory <app>, the home
/// directory ~ (case-insensitive and with either separator on Windows); a path under some
/// other user's home (C:\Users\<name>, /home/<name>, /Users/<name>) also becomes ~. The rest
/// of the path is kept as given. The data root is learned from Logger::set_file()
/// (<root>/logs/fincept.log → <root>) unless set_log_path_roots() named it.
///   C:\Users\bob\AppData\Local\com.fincept.terminal.community\data\fincept.db → <data>\data\fincept.db
///   /home/bob/notes/x.ipynb → ~/notes/x.ipynb
QString log_path(const QString& path);

/// Explicit roots for log_path() (optional — set_file() already provides the data root).
void set_log_path_roots(const QString& data_root, const QString& app_dir = {});

/// Identifier for a log line, showing only that it is present and its last `keep_last`
/// characters: "AB12345678" → "…5678". "" stays "" (absent). An id of `keep_last` characters or
/// fewer, or keep_last <= 0, becomes "…" (present, nothing shown). Use it for broker client ids
/// / UCC, account numbers, wallet addresses, user ids. Never for a secret beyond keep_last = 0.
QString log_mask(const QString& id, int keep_last = 4);

/// Free text scrubbed for a diagnostics copy (recent_problems(), "Copy diagnostics"):
/// URLs through log_url(), "Bearer <token>", JWTs, key=value / key: value secrets
/// (api_key token secret password pin otp signature cookie authorization …) → <redacted>,
/// e-mail addresses → <email>, home directories → ~, opaque tokens of 32+ characters mixing
/// letters and digits → <token>. Slower than the helpers above (regular expressions): for
/// copies of log text, not for every line.
QString log_scrub(const QString& text);

namespace log_detail {
struct PathRoots {
    QString data_root;
    QString app_dir;
    QString home;
    bool case_insensitive = false;
};
/// log_path() against explicit roots (tests run the Windows rules on Linux through this).
QString log_path_with(const QString& path, const PathRoots& roots);
/// Called by Logger::set_file(): the data root implied by the log file's location.
void set_implied_data_root(const QString& data_root);
} // namespace log_detail

} // namespace fincept
