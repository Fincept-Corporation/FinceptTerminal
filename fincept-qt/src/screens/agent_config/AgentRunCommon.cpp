// AgentRunCommon.cpp — shared run / harness UI helpers of the agents screen.

#include "screens/agent_config/AgentRunCommon.h"

#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QStyle>
#include <QWidget>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace {

QString awb_tr(const char* text) {
    return QCoreApplication::translate("AgentWorkbench", text);
}

} // namespace

QString awb_status_text(ca::RunStatus status) {
    switch (status) {
        case ca::RunStatus::Queued:
            return awb_tr("QUEUED");
        case ca::RunStatus::Running:
            return awb_tr("RUNNING");
        case ca::RunStatus::Succeeded:
            return awb_tr("SUCCEEDED");
        case ca::RunStatus::Failed:
            return awb_tr("FAILED");
        case ca::RunStatus::Cancelled:
            return awb_tr("CANCELLED");
        case ca::RunStatus::Interrupted:
            return awb_tr("INTERRUPTED");
    }
    return awb_tr("FAILED");
}

QString awb_run_pill_text(const ca::CodingAgentRun& run) {
    if (run.status == ca::RunStatus::Failed) {
        switch (harness::failure_from_id(run.failure)) {
            case harness::FailureKind::Auth:
                return awb_tr("SIGNED OUT");
            case harness::FailureKind::RateLimit:
                return awb_tr("RATE LIMITED");
            case harness::FailureKind::Timeout:
                return awb_tr("TIMED OUT");
            case harness::FailureKind::NotInstalled:
                return awb_tr("NOT INSTALLED");
            case harness::FailureKind::Model:
                return awb_tr("MODEL REJECTED");
            case harness::FailureKind::Other:
            case harness::FailureKind::None:
                break;
        }
    }
    return awb_status_text(run.status);
}

QString awb_harness_name(const QString& harness_id) {
    return harness::HarnessRegistry::instance().display_name(harness_id);
}

QString awb_permission_text(harness::PermissionMode permission) {
    switch (permission) {
        case harness::PermissionMode::WorkspaceWrite:
            return awb_tr("Workspace edits");
        case harness::PermissionMode::FullAccess:
            return awb_tr("Full access");
        case harness::PermissionMode::ReadOnly:
            break;
    }
    return awb_tr("Read-only analysis");
}

QString awb_auth_text(const harness::AuthStatus& auth) {
    QStringList parts;
    switch (auth.state) {
        case harness::AuthState::SignedIn:
            parts << awb_tr("Signed in");
            break;
        case harness::AuthState::SignedOut:
            parts << awb_tr("Signed out");
            break;
        case harness::AuthState::Error:
            parts << awb_tr("Sign-in check failed");
            break;
        case harness::AuthState::Unknown:
            parts << awb_tr("Sign-in state unknown");
            break;
    }
    if (auth.method == QLatin1String("subscription"))
        parts << awb_tr("subscription");
    else if (auth.method == QLatin1String("api_key"))
        parts << awb_tr("API key");
    else if (auth.method == QLatin1String("cloud"))
        parts << awb_tr("cloud provider");
    else if (auth.method == QLatin1String("env"))
        parts << awb_tr("token in environment");
    if (!auth.plan.isEmpty())
        parts << auth.plan;
    return parts.join(QStringLiteral(" · "));
}

QString awb_duration(qint64 ms) {
    const qint64 s = qMax<qint64>(0, ms) / 1000;
    if (s < 60)
        return QStringLiteral("%1s").arg(s);
    if (s < 3600)
        return QStringLiteral("%1m %2s").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1h %2m").arg(s / 3600).arg((s % 3600) / 60, 2, 10, QLatin1Char('0'));
}

QString awb_tokens(qint64 n) {
    if (n < 1000)
        return QString::number(n);
    if (n < 1000000)
        return QString::number(static_cast<double>(n) / 1000.0, 'f', 1) + QLatin1Char('k');
    return QString::number(static_cast<double>(n) / 1000000.0, 'f', 2) + QLatin1Char('M');
}

qint64 awb_run_duration_ms(const ca::CodingAgentRun& run) {
    if (run.started_at_ms <= 0)
        return 0;
    const qint64 end = run.finished_at_ms > 0 ? run.finished_at_ms : QDateTime::currentMSecsSinceEpoch();
    return qMax<qint64>(0, end - run.started_at_ms);
}

void awb_repolish(QWidget* w) {
    if (!w)
        return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

QString awb_breakable_html(const QString& text) {
    QString out;
    out.reserve(text.size() + text.size() / 8);
    for (const QChar c : text) {
        out += QString(c).toHtmlEscaped();
        if (c == QLatin1Char('/') || c == QLatin1Char('\\') || c == QLatin1Char('@') || c == QLatin1Char('-') ||
            c == QLatin1Char('.') || c == QLatin1Char(':'))
            out += QStringLiteral("&#8203;"); // zero-width space: a permitted line break
    }
    return out;
}

AwbHarnessState awb_harness_state(const QString& harness_id) {
    auto& registry = harness::HarnessRegistry::instance();
    AwbHarnessState out;
    const harness::IHarnessAdapter* adapter = registry.adapter(harness_id);
    if (!adapter) {
        out.tone = AwbHarnessTone::Danger;
        out.word = awb_tr("UNKNOWN");
        out.sentence = awb_tr("This build has no adapter for “%1”.").arg(harness_id);
        return out;
    }
    const harness::HarnessDescriptor& d = adapter->descriptor();
    const harness::HarnessStatus st = registry.status(harness_id);
    if (!st.installed()) {
        if (st.detecting || !st.detection_complete) {
            out.tone = AwbHarnessTone::Busy;
            out.word = awb_tr("DETECTING");
            out.sentence = awb_tr("Looking for %1 on this computer…").arg(d.display_name);
        } else if (st.host_blocked) {
            // Flatpak without the host-command permission (Flatseal / flatpak
            // override): not "not installed" — detect_error says what to run.
            out.tone = AwbHarnessTone::Danger;
            out.word = awb_tr("NO SYSTEM ACCESS");
            out.sentence = st.detect_error;
        } else {
            out.tone = AwbHarnessTone::Absent;
            out.word = awb_tr("NOT INSTALLED");
            out.sentence = st.detect_error.isEmpty() ? awb_tr("%1 was not found on this computer.").arg(d.display_name)
                                                     : st.detect_error;
        }
        return out;
    }
    if (st.detecting) {
        out.tone = AwbHarnessTone::Busy;
        out.word = awb_tr("DETECTING");
        out.sentence = awb_tr("Re-detecting %1…").arg(d.display_name);
        return out;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (st.cooldown_until_ms > now) {
        out.tone = AwbHarnessTone::Warning;
        out.word = awb_tr("RATE LIMITED");
        out.sentence = awb_tr("Rate limited until %1.")
                           .arg(QDateTime::fromMSecsSinceEpoch(st.cooldown_until_ms).toString(QStringLiteral("ddd HH:mm")));
        return out;
    }
    if (d.api_key_only && !registry.api_key_reaches_child(harness_id)) {
        out.tone = AwbHarnessTone::Warning;
        out.word = awb_tr("NEEDS API KEY");
        out.sentence = awb_tr("%1 runs from the terminal only with an API key in the environment (%2).")
                           .arg(d.display_name, d.api_key_env_vars.join(QStringLiteral(", ")));
        return out;
    }
    switch (st.auth.state) {
        case harness::AuthState::SignedOut:
            out.tone = AwbHarnessTone::Warning;
            out.word = awb_tr("SIGNED OUT");
            out.sentence = awb_tr("Signed out. %1").arg(d.signin_hint);
            return out;
        case harness::AuthState::Error:
            out.tone = AwbHarnessTone::Warning;
            out.word = awb_tr("CHECK FAILED");
            out.sentence = st.auth.note.isEmpty() ? awb_tr("The sign-in check failed.") : st.auth.note;
            return out;
        case harness::AuthState::Unknown:
            if (st.probing_auth) {
                out.tone = AwbHarnessTone::Busy;
                out.word = awb_tr("CHECKING");
                out.sentence = awb_tr("Checking the sign-in…");
                return out;
            }
            out.tone = AwbHarnessTone::Warning;
            out.word = awb_tr("SIGN-IN UNKNOWN");
            out.sentence = awb_tr("%1 has no sign-in check the terminal can run; a run fails if it is signed out. %2")
                               .arg(d.display_name, d.signin_hint);
            return out;
        case harness::AuthState::SignedIn:
            break;
    }
    if (st.installation->version.isEmpty()) {
        out.tone = AwbHarnessTone::Warning;
        out.word = awb_tr("UNVERIFIED");
        out.sentence = st.detect_error.isEmpty() ? awb_tr("The version could not be read.") : st.detect_error;
        return out;
    }
    out.tone = AwbHarnessTone::Ready;
    out.word = awb_tr("READY");
    out.sentence = awb_tr("%1 %2 is installed and signed in.").arg(d.display_name, st.installation->version);
    return out;
}

QString awb_tone_color(AwbHarnessTone tone) {
    switch (tone) {
        case AwbHarnessTone::Ready:
            return QString(ui::colors::SUCCESS());
        case AwbHarnessTone::Warning:
            return QString(ui::colors::WARNING());
        case AwbHarnessTone::Danger:
            return QString(ui::colors::DANGER());
        case AwbHarnessTone::Busy:
            return QString(ui::colors::INFO());
        case AwbHarnessTone::Absent:
            break;
    }
    return QString(ui::colors::TEXT_TERTIARY());
}

} // namespace fincept::screens
