#pragma once
// ClientInfo — who is talking to the Fincept API: the descriptive User-Agent,
// the per-installation id and the per-process run id, plus the device facts of
// the usage report ("client" object). Everything is computed once and cached.
//
// Thread-safe: every accessor may be called from any thread. The first call
// that needs the install kind waits for it; prime() starts that lookup on a
// worker at startup so the GUI thread never pays for it (on a Linux .deb/.rpm
// install it runs dpkg-query / rpm, see services/updater/InstallContext.h).

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class QNetworkRequest;

namespace fincept::telemetry {

class ClientInfo {
  public:
    /// Resolve the static facts (install kind, OS, memory) on a pool thread.
    /// Call once early at startup; harmless to skip or repeat.
    static void prime();

    /// `FinceptTerminal/<X.Y.Z> (<platform>; <install_kind>; <os>; Qt <ver>)`, ASCII only.
    static QByteArray user_agent();

    /// Random UUID (no braces) minted once per installation and kept in the
    /// app-level settings store (core/config/AppSettings.h) — not per profile, not
    /// derived from hardware. A separate data folder (FINCEPT_DATA_DIR) is a
    /// separate terminal and mints its own.
    static QString install_id();

    /// Random UUID (no braces) for this process. TerminalShell uses it as its
    /// session_id, so the run id the API sees is the session_history row key.
    static QString run_id();

    /// Sets User-Agent, X-Fincept-Install and X-Fincept-Run. Only for requests
    /// to the Fincept API (the same condition as the auth headers).
    static void apply_headers(QNetworkRequest& request);

    /// The report's "client" object. Screen count and primary resolution are
    /// read live, so call it on the GUI thread.
    static QJsonObject client_json();
};

} // namespace fincept::telemetry
