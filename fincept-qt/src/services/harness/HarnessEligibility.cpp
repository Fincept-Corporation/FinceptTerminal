// HarnessEligibility.cpp — the terminal-AI eligibility rule (see the header). QtCore only.

#include "services/harness/HarnessEligibility.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace fincept::harness {

namespace harness_eligibility_detail {

QString he_tr(const char* text) {
    return QString::fromUtf8(text);
}

QString he_time(qint64 ms) {
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("HH:mm"));
}

/// "ANTHROPIC_API_KEY" or "ANTHROPIC_API_KEY or ANTHROPIC_AUTH_TOKEN".
QString he_vars(const QStringList& vars) {
    if (vars.isEmpty())
        return QStringLiteral("an API key");
    if (vars.size() == 1)
        return vars.constFirst();
    return vars.mid(0, vars.size() - 1).join(QStringLiteral(", ")) + QStringLiteral(" or ") + vars.constLast();
}

} // namespace harness_eligibility_detail

using namespace harness_eligibility_detail;

QString terminal_state_id(TerminalState s) {
    switch (s) {
        case TerminalState::Ready:
            return QStringLiteral("ready");
        case TerminalState::EligibleNotEnabled:
            return QStringLiteral("not_enabled");
        case TerminalState::ConsentNeeded:
            return QStringLiteral("consent_needed");
        case TerminalState::NotInstalled:
            return QStringLiteral("not_installed");
        case TerminalState::Detecting:
            return QStringLiteral("detecting");
        case TerminalState::ChatUnsupported:
            return QStringLiteral("chat_unsupported");
        case TerminalState::ReadOnlyUnverified:
            return QStringLiteral("read_only_unverified");
        case TerminalState::SignedOut:
            return QStringLiteral("signed_out");
        case TerminalState::SubscriptionOnly:
            return QStringLiteral("workbench_only");
        case TerminalState::KeyStripped:
            return QStringLiteral("key_stripped");
        case TerminalState::CoolingDown:
            return QStringLiteral("cooling_down");
    }
    return QStringLiteral("unknown");
}

QStringList key_vars_reaching_child(const HarnessDescriptor& d, const QStringList& system_names, bool subscription_only) {
    QStringList out;
    if (subscription_only && !d.api_key_only)
        return out; // the policy strips every API-key variable of this harness
    for (const QString& v : d.terminal_key_env_vars) {
        for (const QString& n : system_names) {
#ifdef Q_OS_WIN
            const bool same = n.compare(v, Qt::CaseInsensitive) == 0;
#else
            const bool same = n == v;
#endif
            if (same) {
                out << v;
                break;
            }
        }
    }
    return out;
}

TerminalEligibility terminal_eligibility(const TerminalEligibilityInput& in) {
    TerminalEligibility e;
    const HarnessDescriptor& d = in.descriptor;
    const HarnessStatus& st = in.status;
    const QString name = d.display_name.isEmpty() ? d.id : d.display_name;
    const QString vendor = d.vendor.isEmpty() ? name : d.vendor;
    e.docs_url = d.api_key_docs_url.isEmpty() ? d.auth_docs_url : d.api_key_docs_url;

    // ── What the adapter can do on this OS ──
    if (!d.caps.chat) {
        e.label = he_tr("Workbench only");
        if (!d.caps.enforced_read_only) {
            e.state = TerminalState::ReadOnlyUnverified;
            e.reason = QStringLiteral("%1's read-only mode cannot be verified on this system without running it, so "
                                      "terminal AI does not use it here. It stays available in the Agent Workbench.")
                           .arg(name);
        } else {
            e.state = TerminalState::ChatUnsupported;
            e.reason = d.beta ? QStringLiteral("%1 support is in BETA: terminal AI uses it once its read-only mode has "
                                               "been verified. It stays available in the Agent Workbench.")
                                    .arg(name)
                              : QStringLiteral("%1 cannot answer terminal AI yet. It stays available in the Agent "
                                               "Workbench.")
                                    .arg(name);
        }
        e.docs_url.clear();
        return e;
    }
    if (!st.installed()) {
        const bool detecting = st.detecting || !st.detection_complete;
        e.state = detecting ? TerminalState::Detecting : TerminalState::NotInstalled;
        if (!detecting && st.host_blocked) {
            // Flatpak without the host-command permission: the CLI could not even
            // be looked for — say that, and how to allow it again.
            e.label = he_tr("No access to your system");
            e.reason = st.detect_error;
            e.docs_url.clear();
            return e;
        }
        e.label = detecting ? he_tr("Detecting…") : he_tr("Not installed");
        e.reason = detecting ? QStringLiteral("Looking for %1 on this computer.").arg(name)
                             : QStringLiteral("%1 is not installed on this computer.").arg(name);
        e.docs_url = d.install_url;
        return e;
    }

    // ── Authentication: API key or cloud provider only (owner decision 1) ──
    const AuthStatus& a = st.auth;
    QString basis;
    if (a.state == AuthState::SignedIn && a.method == QLatin1String("api_key"))
        basis = QStringLiteral("api_key");
    else if (a.state != AuthState::SignedOut && a.method == QLatin1String("cloud"))
        basis = QStringLiteral("cloud");
    else if (!in.key_vars_in_child.isEmpty())
        basis = QStringLiteral("env_key");

    if (basis.isEmpty()) {
        e.label = he_tr("Workbench only");
        if (a.state == AuthState::SignedOut && in.key_vars_in_system.isEmpty()) {
            e.state = TerminalState::SignedOut;
            e.label = he_tr("Signed out");
            e.reason = QStringLiteral("%1 is not signed in. Terminal AI needs it to authenticate with an API key: set "
                                      "%2 in your environment, or sign it in with an API key.")
                           .arg(name, he_vars(d.terminal_key_env_vars));
        } else if (!in.key_vars_in_system.isEmpty()) {
            e.state = TerminalState::KeyStripped;
            e.reason = QStringLiteral("An API key for %1 is set (%2), but your “subscription only” setting removes it "
                                      "from its runs. Terminal AI needs the key — change that setting in Settings › "
                                      "AI Agents. %1 stays available in the Agent Workbench.")
                           .arg(name, in.key_vars_in_system.join(QStringLiteral(", ")));
        } else {
            e.state = TerminalState::SubscriptionOnly;
            const QString how = a.method == QLatin1String("subscription")
                                    ? QStringLiteral("%1 is signed in with a personal %2 plan.")
                                          .arg(name, a.plan.isEmpty() ? vendor : a.plan)
                                    : QStringLiteral("%1 is not signed in with an API key.").arg(name);
            e.reason = how + QLatin1Char(' ') +
                       QStringLiteral("%1 does not allow other apps to route requests through a personal sign-in, so "
                                      "terminal AI needs an API key: set %2 in your environment (or sign %3 in with "
                                      "an API key). It stays available in the Agent Workbench.")
                           .arg(vendor, he_vars(d.terminal_key_env_vars), name);
        }
        return e;
    }

    e.eligible = true;
    e.basis = basis;
    // A cloud login (Bedrock / Vertex / Foundry) or a gateway token involves no
    // Anthropic API key, so the CLI's own "which key" report is not checked.
    bool api_key_var = basis == QLatin1String("api_key");
    for (const QString& v : in.key_vars_in_child)
        api_key_var = api_key_var || v.endsWith(QLatin1String("_API_KEY"));
    e.require_api_key_source = basis != QLatin1String("cloud") && api_key_var;

    // ── Temporarily unavailable ──
    const qint64 until = qMax(st.cooldown_until_ms, in.refused_until_ms);
    if (until > in.now_ms) {
        e.state = TerminalState::CoolingDown;
        const bool rate = st.cooldown_until_ms > in.now_ms;
        e.label = rate ? QStringLiteral("Rate limited until %1").arg(he_time(until))
                       : QStringLiteral("Paused until %1").arg(he_time(until));
        e.reason = rate ? QStringLiteral("%1 reported a rate limit; terminal AI uses the next model until %2.")
                              .arg(name, he_time(until))
                        : QStringLiteral("%1 did not use its API key on the last call; terminal AI uses the next model "
                                         "until %2.")
                              .arg(name, he_time(until));
        e.eligible = false;
        return e;
    }

    // ── The user's consent and default switch (owner decision 2) ──
    if (!in.consent) {
        e.state = TerminalState::ConsentNeeded;
        e.label = he_tr("Needs your OK");
        e.reason = QStringLiteral("Confirm once that terminal AI may send your messages — and what its tools read in "
                                  "the terminal — to %1.")
                       .arg(vendor);
        return e;
    }
    e.usable = true;
    if (!in.enabled_for_terminal) {
        e.state = TerminalState::EligibleNotEnabled;
        e.label = he_tr("Ready · not default");
        e.reason = QStringLiteral("Used where you choose it. Switch on “Use for terminal AI” to make it the default.");
        return e;
    }
    e.state = TerminalState::Ready;
    e.usable_by_default = true;
    e.label = he_tr("Ready");
    e.reason = basis == QLatin1String("cloud")
                   ? QStringLiteral("%1 answers terminal AI with your cloud provider's key.").arg(name)
                   : QStringLiteral("%1 answers terminal AI with your %2 API key.").arg(name, vendor);
    return e;
}

QString subscription_session_refusal(const QString& harness_id, const QString& line) {
    if (harness_id != QLatin1String("claude") || !line.contains(QLatin1String("apiKeySource")))
        return {};
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(line.trimmed().toUtf8(), &pe);
    if (!doc.isObject())
        return {};
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("type")).toString() != QLatin1String("system") ||
        o.value(QStringLiteral("subtype")).toString() != QLatin1String("init"))
        return {};
    const QString source = o.value(QStringLiteral("apiKeySource")).toString().trimmed().toLower();
    if (source != QLatin1String("none"))
        return {};
    return QStringLiteral("Claude Code started without an API key (it is using a personal sign-in), so it may not "
                          "answer terminal AI. Set ANTHROPIC_API_KEY for it, or keep using it in the Agent Workbench.");
}

} // namespace fincept::harness
