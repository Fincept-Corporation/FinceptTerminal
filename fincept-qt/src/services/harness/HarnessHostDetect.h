#pragma once
// HarnessHostDetect.h — harness detection when the CLIs are on the HOST and
// this process cannot see the host's files (the Flatpak sandbox).
//
// The rules stay those of HarnessDetect (find_candidates: override, PATH, the
// adapter's own locations, the shared per-OS list, nvm versions) and of the
// adapter (auth_hint, user_mcp_server_names); only the DetectContext they ask is
// different: HOME and PATH are the host user's (login shell, see
// process::host::host_profile_blocking) and every file question — exists, is
// executable, size + mtime, sub-directories, the first bytes of a config — is
// answered by the host in ONE batched `sh` query per round
// (process::host::run_blocking). A round collects the questions the rules ask,
// the host answers them, the rules run again over the answers; a few rounds at
// most (nvm's version folders are what needs a second one).
//
// Worker threads only (it waits for host processes). QtCore only.

#include "services/harness/HarnessTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::harness {

class IHarnessAdapter;

struct HostSearchResult {
    QVector<Installation> candidates;  ///< as find_candidates(); file_size / file_mtime_ms from the host
    AuthStatus hint;                   ///< the adapter's auth_hint() over the host's files
    QStringList user_mcp_server_names; ///< the adapter's user_mcp_server_names() over the host's files
    QString error;                     ///< the host could not be asked (shown as the detection error)
    bool blocked = false;              ///< host commands are not allowed (process::host::blocked())
};

/// Detection of `adapter`'s CLI on the host; `override_path` is a host path.
HostSearchResult search_on_host(const IHarnessAdapter& adapter, const QString& override_path);

/// Only the user's MCP server names, read on the host (before a run).
/// nullopt = the host could not be asked (keep what was known).
std::optional<QStringList> host_user_mcp_server_names(const IHarnessAdapter& adapter);

} // namespace fincept::harness
