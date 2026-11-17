#pragma once
// McpLegacyEntry.h — the one-time rename of this edition's pre-split MCP entry.
// Header-only, QtCore only (tests/tst_harness_commands.cpp).
//
// Before the edition split this terminal registered itself in the user's
// harness configs as "fincept_terminal"; it now uses edition::kMcpServerName
// ("fincept_community"). When the CONNECT tab (re)registers with a harness, an
// existing "fincept_terminal" entry is renamed — removed, and the terminal added
// under the new name — ONLY when the harness's own `mcp get` output shows that
// it points at this edition (loopback, this edition's port range or its current
// port: edition::is_own_mcp_url). Anything else — another port (the private
// build listens on 47821), another host, an entry without a URL — is left
// exactly as it is. The private build's own names (fincept, fincept-terminal,
// fincept-cloud, fincept-mcp) are never looked at.

#include "core/config/EditionNames.h"

#include <QRegularExpression>
#include <QString>

namespace fincept::harness {

enum class LegacyEntryAction {
    Absent,       ///< no such entry (the `get` command failed)
    Migrate,      ///< every URL in it is this edition's: rename it
    LeaveForeign, ///< it points elsewhere (or nowhere provable): do not touch it
};

/// From the exit code and output of `<cli> mcp get fincept_terminal`.
inline LegacyEntryAction classify_legacy_entry(int exit_code, const QString& get_output, quint16 own_port) {
    if (exit_code != 0)
        return LegacyEntryAction::Absent;
    static const QRegularExpression url_rx(QStringLiteral("https?://[^\\s\\x22'<>,;)\\]]+"));
    bool any = false;
    auto it = url_rx.globalMatch(get_output);
    while (it.hasNext()) {
        const QString url = it.next().captured(0);
        any = true;
        if (!edition::is_own_mcp_url(url, own_port))
            return LegacyEntryAction::LeaveForeign;
    }
    return any ? LegacyEntryAction::Migrate : LegacyEntryAction::LeaveForeign;
}

} // namespace fincept::harness
