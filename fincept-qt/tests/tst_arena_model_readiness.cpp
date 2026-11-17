// tst_arena_model_readiness — the reason an Alpha Arena competitor row is not
// ready (services/alpha_arena/ArenaModelReadiness.h), driven through the real
// terminal-AI eligibility rule (services/harness/HarnessEligibility.cpp) so the
// wizard's text can never drift from the rule's states.

#include "services/alpha_arena/ArenaModelReadiness.h"
#include "services/harness/HarnessEligibility.h"

#include <QDateTime>
#include <QtTest/QtTest>

using namespace fincept;
using fincept::harness::TerminalState;

namespace {

harness::TerminalEligibilityInput base_input() {
    harness::TerminalEligibilityInput in;
    in.descriptor.id = QStringLiteral("claude");
    in.descriptor.display_name = QStringLiteral("Claude Code");
    in.descriptor.vendor = QStringLiteral("Anthropic");
    in.descriptor.caps.chat = true;
    in.descriptor.caps.enforced_read_only = true;
    in.descriptor.terminal_key_env_vars = {QStringLiteral("ANTHROPIC_API_KEY")};
    in.status.id = in.descriptor.id;
    in.status.installation = harness::Installation{};
    in.status.detection_complete = true;
    in.status.auth.state = harness::AuthState::SignedIn;
    in.status.auth.method = QStringLiteral("api_key");
    in.consent = true;
    in.enabled_for_terminal = true;
    in.now_ms = QDateTime(QDate(2026, 11, 17), QTime(12, 0)).toMSecsSinceEpoch();
    return in;
}

} // namespace

class TestArenaModelReadiness : public QObject {
    Q_OBJECT
  private slots:
    void ready_has_no_reason() {
        const auto e = harness::terminal_eligibility(base_input());
        QCOMPARE(e.state, TerminalState::Ready);
        QVERIFY(arena::readiness::harness_not_ready_text(e).isEmpty());
        QVERIFY(!arena::readiness::harness_needs_consent(e));
    }

    void explicit_choice_does_not_need_the_default_switch() {
        auto in = base_input();
        in.enabled_for_terminal = false; // usable where chosen (the arena is explicit-only)
        const auto e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::EligibleNotEnabled);
        QVERIFY(e.usable);
        QVERIFY(arena::readiness::harness_not_ready_text(e).isEmpty());
    }

    void consent_needed() {
        auto in = base_input();
        in.consent = false;
        const auto e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::ConsentNeeded);
        QCOMPARE(arena::readiness::harness_not_ready_text(e), QStringLiteral("Needs your OK"));
        QVERIFY(arena::readiness::harness_needs_consent(e));
    }

    void subscription_sign_in_is_workbench_only() {
        auto in = base_input();
        in.status.auth.method = QStringLiteral("subscription");
        in.status.auth.plan = QStringLiteral("max");
        const auto e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::SubscriptionOnly);
        QCOMPARE(arena::readiness::harness_not_ready_text(e),
                 QStringLiteral("Workbench only — sign in with an API key"));
        QVERIFY(!arena::readiness::harness_needs_consent(e));
        QVERIFY(!e.reason.isEmpty()); // the tooltip sentence
    }

    void key_stripped_by_policy() {
        auto in = base_input();
        in.status.auth.method = QStringLiteral("subscription");
        in.key_vars_in_system = {QStringLiteral("ANTHROPIC_API_KEY")}; // set, but not reaching the child
        const auto e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::KeyStripped);
        QVERIFY(arena::readiness::harness_not_ready_text(e).startsWith(QStringLiteral("Workbench only — ")));
    }

    void signed_out() {
        auto in = base_input();
        in.status.auth.state = harness::AuthState::SignedOut;
        in.status.auth.method.clear();
        const auto e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::SignedOut);
        QCOMPARE(arena::readiness::harness_not_ready_text(e), QStringLiteral("Signed out"));
    }

    void rate_limited_carries_the_time() {
        auto in = base_input();
        in.status.cooldown_until_ms = in.now_ms + 5 * 60 * 1000;
        const auto e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::CoolingDown);
        const QString text = arena::readiness::harness_not_ready_text(e);
        const QString hhmm = QDateTime::fromMSecsSinceEpoch(in.status.cooldown_until_ms).toString(QStringLiteral("HH:mm"));
        QCOMPARE(text, QStringLiteral("Rate limited until %1").arg(hhmm));
    }

    void chat_unsupported_is_workbench_only() {
        auto in = base_input();
        in.descriptor.caps.chat = false;
        const auto e = harness::terminal_eligibility(in);
        QCOMPARE(arena::readiness::harness_not_ready_text(e), QStringLiteral("Workbench only"));
    }

    void not_installed_and_detecting() {
        auto in = base_input();
        in.status.installation.reset();
        auto e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::NotInstalled);
        QCOMPARE(arena::readiness::harness_not_ready_text(e), QStringLiteral("Not installed"));
        in.status.detection_complete = false;
        e = harness::terminal_eligibility(in);
        QCOMPARE(e.state, TerminalState::Detecting);
        QCOMPARE(arena::readiness::harness_not_ready_text(e), QStringLiteral("Detecting…"));
    }

    void every_unusable_state_has_a_reason() {
        // Hand-built states (the rule never reports these unusable with an empty
        // label, but the text must not be empty whatever arrives).
        for (TerminalState s : {TerminalState::ConsentNeeded, TerminalState::NotInstalled, TerminalState::Detecting,
                                TerminalState::ChatUnsupported, TerminalState::ReadOnlyUnverified,
                                TerminalState::SignedOut, TerminalState::SubscriptionOnly, TerminalState::KeyStripped,
                                TerminalState::CoolingDown}) {
            harness::TerminalEligibility e;
            e.state = s;
            e.usable = false;
            QVERIFY2(!arena::readiness::harness_not_ready_text(e).isEmpty(),
                     qPrintable(harness::terminal_state_id(s)));
        }
    }

    void api_rows_and_warning() {
        QCOMPARE(arena::readiness::api_not_ready_text(), QStringLiteral("needs API key"));
        QVERIFY(arena::readiness::api_not_ready_detail(QStringLiteral("OpenAI")).contains(QStringLiteral("OpenAI")));
        const QString w = arena::readiness::harness_call_volume_warning(QStringLiteral("Anthropic"));
        QVERIFY(w.contains(QStringLiteral("480")));
        QVERIFY(w.contains(QStringLiteral("Anthropic API key")));
    }
};

QTEST_GUILESS_MAIN(TestArenaModelReadiness)
#include "tst_arena_model_readiness.moc"
