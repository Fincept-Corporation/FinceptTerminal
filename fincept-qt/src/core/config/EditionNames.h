#pragma once
// EditionNames.h — names this edition gives OS-global resources at run time,
// derived from EditionIdentity.h. Header-only, Qt Core only (unit-tested in
// tests/tst_edition_identity.cpp).
//
// A name is "OS-global" when another program on the machine can see or take it:
// a local-socket / named-pipe address, a file in the shared temp folder. Each one
// carries the edition (so the separate private build never answers, blocks or
// deletes it) and, where the namespace is shared by every account on the machine,
// the OS user (Windows named pipes live in one machine-wide namespace; on Linux
// the socket file sits in the shared /tmp).

#include "core/config/EditionIdentity.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QString>
#include <QUrl>

#ifndef Q_OS_WIN
#    include <unistd.h>
#endif

#ifndef FINCEPT_EDITION_MCP_TOKEN_ENV
#    define FINCEPT_EDITION_MCP_TOKEN_ENV "FINCEPT_COMMUNITY_MCP_TOKEN"
#endif

namespace fincept::edition {

// ── The terminal's MCP server, as other programs see it ─────────────────────
// Both editions run an MCP server for the same harness CLIs (Claude Code, Codex,
// …) of the same user. Everything those CLIs are told about this edition's
// server — the entry name in their configs (kMcpServerName, EditionIdentity.h),
// the environment variable that carries the token, the port — must differ from
// the private build's, or one edition's registration replaces, deletes or
// authenticates against the other's.

/// Environment variable that carries this edition's MCP bearer token to the
/// harness processes it starts, and that its client registrations name.
inline constexpr char kMcpTokenEnvVar[] = FINCEPT_EDITION_MCP_TOKEN_ENV;
/// The variable both editions used before the split — and the one the private
/// build still sets in its own harness processes. This edition never reads it
/// and removes it from the environment of every process it starts.
inline constexpr char kForeignMcpTokenEnvVar[] = "FINCEPT_MCP_TOKEN";

/// The entry name this edition registered itself under before the split. An
/// entry with this name is renamed to kMcpServerName only when it points at this
/// edition's own loopback ports (is_own_mcp_url); otherwise it is left alone.
inline constexpr char kLegacyMcpServerName[] = "fincept_terminal";
/// Entry names the private build writes. Never read, rewritten or removed.
inline constexpr const char* kPrivateMcpServerNames[] = {"fincept", "fincept-terminal", "fincept-cloud",
                                                        "fincept-mcp"};

/// This edition's MCP listener: 127.0.0.1:41720, falling back to the next free
/// port up to 41739 when it is taken (a user-set port works the same way).
inline constexpr quint16 kMcpDefaultPort = 41720;
inline constexpr int kMcpPortFallbackRange = 19;
/// The private build's MCP listeners (external server, self-test). Never bound
/// by this edition, whatever the user sets or the fallback scan reaches.
inline constexpr quint16 kPrivateMcpPorts[] = {47821, 47999};

inline bool is_private_mcp_port(quint16 port) {
    for (const quint16 p : kPrivateMcpPorts) {
        if (p == port)
            return true;
    }
    return false;
}

/// True when `url` is an MCP endpoint of THIS edition: loopback, and either in
/// this edition's default range (41720–41739) or on `own_port` (the port it is
/// configured for / bound to) — never one of the private build's ports.
inline bool is_own_mcp_url(const QString& url, quint16 own_port = 0) {
    const QUrl u(url.trimmed());
    if (!u.isValid() || u.scheme() != QLatin1String("http"))
        return false;
    const QString host = u.host().toLower();
    if (host != QLatin1String("127.0.0.1") && host != QLatin1String("localhost") && host != QLatin1String("::1"))
        return false;
    const int port = u.port(-1);
    if (port <= 0 || port > 65535 || is_private_mcp_port(static_cast<quint16>(port)))
        return false;
    const bool in_default_range = port >= kMcpDefaultPort && port <= kMcpDefaultPort + kMcpPortFallbackRange;
    return in_default_range || (own_port != 0 && port == own_port);
}

/// The OS account this process runs as, as a stable string: "DOMAIN\user" on
/// Windows, the numeric uid elsewhere. Only used to keep per-user names apart; it
/// is not a security boundary (InstanceLock restricts its socket to the user).
inline QString current_user_scope() {
#ifdef Q_OS_WIN
    const QString user = qEnvironmentVariable("USERNAME");
    const QString domain = qEnvironmentVariable("USERDOMAIN");
    if (!user.isEmpty())
        return domain.isEmpty() ? user : domain + QLatin1Char('\\') + user;
    return QDir::homePath();
#else
    return QString::number(static_cast<qulonglong>(::getuid()));
#endif
}

/// Local-socket name (Unix socket file in the temp folder / Windows named pipe)
/// of the single-instance server for `key` ("<kInstanceKeyPrefix>-<profile>"),
/// owned by `user_scope`: "<kTempPrefix>-<16 hex>". Short enough for the macOS
/// sun_path limit under $TMPDIR.
inline QString instance_socket_name(const QString& key, const QString& user_scope) {
    const QByteArray material = key.toUtf8() + '\x1f' + user_scope.toUtf8();
    const QByteArray hash = QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex();
    return QLatin1String(kTempPrefix) + QLatin1Char('-') + QString::fromLatin1(hash.left(16));
}

/// The name the private build and releases before the edition split listen on
/// for `key` ("FinceptTerminal-<profile>"): "fincept-" + the first 16 hex digits
/// of SHA-1(key), with no user scope. Used only to ASK whether a program with
/// the legacy identity is running — never to listen.
inline QString legacy_instance_socket_name(const QString& key) {
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QLatin1String(kLegacyInstanceSocketPrefix) + QString::fromLatin1(hash.left(16));
}

/// File name of the boot log (the fallback log when the data root cannot be
/// written), in the temp folder: "<kTempPrefix>-boot.log".
inline QString boot_log_file_name() {
    return QLatin1String(kTempPrefix) + QStringLiteral("-boot.log");
}

/// Suggested name of a saved screenshot: "<kTempPrefix>_<yyyyMMdd_HHmmss>.png".
/// Not "fincept_<timestamp>.png": the private build's uninstaller deletes files
/// of exactly that pattern from the user's home folder.
inline QString screenshot_file_name(const QDateTime& when) {
    return QLatin1String(kTempPrefix) + QLatin1Char('_') + when.toString(QStringLiteral("yyyyMMdd_HHmmss")) +
           QStringLiteral(".png");
}

} // namespace fincept::edition
