// tst_model_router.cpp — ModelRouter's resolution rule and fallback decision,
// and the terminal-AI eligibility rule behind it (owner decisions: API-key
// authentication only, consent per harness, per-feature data switch).
//
// Pure: ModelRouter.cpp (resolve_with / decide_fallback / feature table) and
// HarnessEligibility.cpp, driven by hand-built states. No registry, settings,
// database, process or network.

#include "services/harness/HarnessEligibility.h"
#include "services/llm/ModelRouter.h"

#include <QDateTime>
#include <QtTest>

using namespace fincept::ai_chat;
using namespace fincept::harness;

namespace {

HarnessDescriptor mr_claude_desc() {
    HarnessDescriptor d;
    d.id = QStringLiteral("claude");
    d.display_name = QStringLiteral("Claude Code");
    d.vendor = QStringLiteral("Anthropic");
    d.caps.chat = true;
    d.caps.enforced_read_only = true;
    d.caps.effort = true;
    d.api_key_env_vars = {QStringLiteral("ANTHROPIC_API_KEY"), QStringLiteral("ANTHROPIC_AUTH_TOKEN")};
    d.terminal_key_env_vars = d.api_key_env_vars;
    d.auth_docs_url = QStringLiteral("https://example.invalid/auth");
    return d;
}

HarnessDescriptor mr_codex_desc() {
    HarnessDescriptor d;
    d.id = QStringLiteral("codex");
    d.display_name = QStringLiteral("Codex");
    d.vendor = QStringLiteral("OpenAI");
    d.caps.chat = true;
    d.caps.enforced_read_only = true;
    d.api_key_env_vars = {QStringLiteral("OPENAI_API_KEY"), QStringLiteral("CODEX_API_KEY")};
    d.terminal_key_env_vars = {QStringLiteral("CODEX_API_KEY")};
    return d;
}

HarnessStatus mr_installed(const QString& id, AuthState state, const QString& method) {
    HarnessStatus st;
    st.id = id;
    Installation inst;
    inst.path = QStringLiteral("/usr/local/bin/") + id;
    inst.version = QStringLiteral("9.9.9");
    st.installation = inst;
    st.detection_complete = true;
    st.auth.state = state;
    st.auth.method = method;
    return st;
}

struct MrHarnessSetup {
    HarnessDescriptor desc;
    HarnessStatus status;
    QStringList keys_system;
    QStringList keys_child;
    bool enabled = true;
    bool consent = true;
    qint64 refused_until = 0;
};

RouterHarness mr_harness(const MrHarnessSetup& s) {
    TerminalEligibilityInput in;
    in.descriptor = s.desc;
    in.status = s.status;
    in.key_vars_in_system = s.keys_system;
    in.key_vars_in_child = s.keys_child;
    in.enabled_for_terminal = s.enabled;
    in.consent = s.consent;
    in.now_ms = QDateTime::currentMSecsSinceEpoch();
    in.refused_until_ms = s.refused_until;
    RouterHarness h;
    h.id = s.desc.id;
    h.display_name = s.desc.display_name;
    h.vendor = s.desc.vendor;
    h.eligibility = terminal_eligibility(in);
    h.default_model = QStringLiteral("sonnet");
    h.default_effort = QStringLiteral("high");
    h.enabled_for_terminal = s.enabled;
    return h;
}

/// Claude Code signed in with an API key, consented, switched on.
MrHarnessSetup mr_ready_claude() {
    MrHarnessSetup s;
    s.desc = mr_claude_desc();
    s.status = mr_installed(QStringLiteral("claude"), AuthState::SignedIn, QStringLiteral("api_key"));
    return s;
}

RouterState mr_state_with(const QVector<RouterHarness>& harnesses, const QString& default_harness = {}) {
    RouterState st;
    st.harnesses = harnesses;
    st.default_harness = default_harness;
    st.provider_names.insert(QStringLiteral("openai"), QStringLiteral("OpenAI"));
    return st;
}

void mr_add_active_openai(RouterState& st) {
    st.active_row.present = true;
    st.active_row.provider = QStringLiteral("openai");
    st.active_row.model = QStringLiteral("gpt-5");
    st.active_row.has_key = true;
    st.active_row.key_required = true;
}

bool mr_skipped_mentions(const RouteResult& r, const QString& what, const QString& fragment) {
    for (const SkippedChoice& s : r.skipped) {
        if (s.what == what && s.reason.contains(fragment, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace

class TestModelRouter : public QObject {
    Q_OBJECT
  private slots:
    // ── Eligibility (owner decision 1 + 2) ──
    void eligibility_api_key_signin_is_ready();
    void eligibility_cloud_login_is_eligible_without_key_check();
    void eligibility_env_key_counts_when_it_reaches_the_child();
    void eligibility_subscription_is_workbench_only();
    void eligibility_key_stripped_by_policy();
    void eligibility_signed_out();
    void eligibility_consent_and_enable_gate_the_default();
    void eligibility_cooldown_and_refusal();
    void eligibility_read_only_unverified_and_beta();
    void eligibility_not_installed();
    void key_vars_reaching_child_respects_policy();
    void subscription_session_refusal_reads_init_only();

    // ── Resolution order ──
    void nothing_configured_is_fincept();
    void active_provider_then_fincept();
    void default_profile_before_active_row();
    void eligible_harness_is_the_automatic_default();
    void explicit_choice_wins_and_rest_is_fallback();
    void explicit_harness_profile();
    void explicit_harness_without_consent_is_skipped();
    void subscription_harness_is_skipped_with_reason();
    void signed_out_default_harness_is_skipped();
    void cooling_down_harness_is_skipped();
    void feature_not_suitable_never_gets_a_harness();
    void explicit_only_feature_never_defaults_to_a_harness();
    void data_policy_off_until_the_feature_switch();
    void not_enabled_harness_is_explicit_only();
    void default_harness_preferred_over_registration_order();
    void key_less_profile_is_skipped();

    // ── Fallback (owner decision 3) ──
    void fallback_before_output_falls_back_once();
    void no_fallback_after_output_or_cancel();
    void no_fallback_for_other_failures();
    void ask_and_fail_settings();
    void no_next_entry_means_notice_only();
    void billing_note_for_harness();
};

// ── Eligibility ─────────────────────────────────────────────────────────────

void TestModelRouter::eligibility_api_key_signin_is_ready() {
    const RouterHarness h = mr_harness(mr_ready_claude());
    QCOMPARE(h.eligibility.state, TerminalState::Ready);
    QVERIFY(h.eligibility.eligible && h.eligibility.usable && h.eligibility.usable_by_default);
    QCOMPARE(h.eligibility.basis, QStringLiteral("api_key"));
    QVERIFY(h.eligibility.require_api_key_source);
}

void TestModelRouter::eligibility_cloud_login_is_eligible_without_key_check() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.auth.method = QStringLiteral("cloud");
    s.status.auth.state = AuthState::Unknown; // hint from CLAUDE_CODE_USE_BEDROCK
    const RouterHarness h = mr_harness(s);
    QVERIFY(h.eligibility.usable_by_default);
    QCOMPARE(h.eligibility.basis, QStringLiteral("cloud"));
    QVERIFY(!h.eligibility.require_api_key_source); // no Anthropic key involved
}

void TestModelRouter::eligibility_env_key_counts_when_it_reaches_the_child() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.auth.method = QStringLiteral("subscription"); // signed in with a plan…
    s.keys_system = {QStringLiteral("ANTHROPIC_API_KEY")};
    s.keys_child = {QStringLiteral("ANTHROPIC_API_KEY")}; // …but -p always uses the key when present
    const RouterHarness h = mr_harness(s);
    QVERIFY(h.eligibility.usable_by_default);
    QCOMPARE(h.eligibility.basis, QStringLiteral("env_key"));
    QVERIFY(h.eligibility.require_api_key_source);
    // A gateway token alone: eligible, but the CLI reports no API key — not checked.
    s.keys_system = {QStringLiteral("ANTHROPIC_AUTH_TOKEN")};
    s.keys_child = s.keys_system;
    QVERIFY(!mr_harness(s).eligibility.require_api_key_source);
}

void TestModelRouter::eligibility_subscription_is_workbench_only() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.auth.method = QStringLiteral("subscription");
    s.status.auth.plan = QStringLiteral("max");
    const RouterHarness h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::SubscriptionOnly);
    QVERIFY(!h.eligibility.eligible && !h.eligibility.usable);
    QCOMPARE(h.eligibility.label, QStringLiteral("Workbench only"));
    QVERIFY(h.eligibility.reason.contains(QStringLiteral("API key")));
    QVERIFY(h.eligibility.reason.contains(QStringLiteral("Anthropic")));
    QCOMPARE(h.eligibility.docs_url, QStringLiteral("https://example.invalid/auth"));
    QCOMPARE(terminal_state_id(h.eligibility.state), QStringLiteral("workbench_only"));
    // Codex with a ChatGPT login and only OPENAI_API_KEY: still Workbench only
    // (exec is documented to use CODEX_API_KEY, not OPENAI_API_KEY).
    MrHarnessSetup c;
    c.desc = mr_codex_desc();
    c.status = mr_installed(QStringLiteral("codex"), AuthState::SignedIn, QStringLiteral("subscription"));
    c.keys_system = key_vars_reaching_child(c.desc, {QStringLiteral("OPENAI_API_KEY")}, false);
    c.keys_child = c.keys_system;
    QVERIFY(c.keys_child.isEmpty());
    QCOMPARE(mr_harness(c).eligibility.state, TerminalState::SubscriptionOnly);
}

void TestModelRouter::eligibility_key_stripped_by_policy() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.auth.method = QStringLiteral("subscription");
    s.keys_system = {QStringLiteral("ANTHROPIC_API_KEY")};
    s.keys_child = {}; // "subscription only" policy
    const RouterHarness h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::KeyStripped);
    QVERIFY(!h.eligibility.eligible);
    QVERIFY(h.eligibility.reason.contains(QStringLiteral("subscription only")));
}

void TestModelRouter::eligibility_signed_out() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.auth.state = AuthState::SignedOut;
    s.status.auth.method.clear();
    const RouterHarness h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::SignedOut);
    QVERIFY(!h.eligibility.eligible);
    // Signed out but a key reaches the child: the key answers.
    s.keys_system = {QStringLiteral("ANTHROPIC_API_KEY")};
    s.keys_child = s.keys_system;
    QVERIFY(mr_harness(s).eligibility.usable_by_default);
}

void TestModelRouter::eligibility_consent_and_enable_gate_the_default() {
    MrHarnessSetup s = mr_ready_claude();
    s.consent = false;
    RouterHarness h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::ConsentNeeded);
    QVERIFY(h.eligibility.eligible && !h.eligibility.usable && !h.eligibility.usable_by_default);
    QVERIFY(h.eligibility.reason.contains(QStringLiteral("Anthropic")));
    s.consent = true;
    s.enabled = false;
    h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::EligibleNotEnabled);
    QVERIFY(h.eligibility.usable && !h.eligibility.usable_by_default);
}

void TestModelRouter::eligibility_cooldown_and_refusal() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.cooldown_until_ms = QDateTime::currentMSecsSinceEpoch() + 60000;
    RouterHarness h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::CoolingDown);
    QVERIFY(!h.eligibility.eligible && h.eligibility.label.startsWith(QStringLiteral("Rate limited until")));
    s.status.cooldown_until_ms = 0;
    s.refused_until = QDateTime::currentMSecsSinceEpoch() + 60000;
    h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::CoolingDown);
    QVERIFY(h.eligibility.label.startsWith(QStringLiteral("Paused until")));
    s.refused_until = QDateTime::currentMSecsSinceEpoch() - 1;
    QCOMPARE(mr_harness(s).eligibility.state, TerminalState::Ready);
}

void TestModelRouter::eligibility_read_only_unverified_and_beta() {
    MrHarnessSetup s;
    s.desc = mr_codex_desc();
    s.desc.caps.chat = false;
    s.desc.caps.enforced_read_only = false; // Codex on Windows
    s.status = mr_installed(QStringLiteral("codex"), AuthState::SignedIn, QStringLiteral("api_key"));
    RouterHarness h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::ReadOnlyUnverified);
    QVERIFY(!h.eligibility.eligible && h.eligibility.reason.contains(QStringLiteral("cannot be verified")));
    s.desc.caps.enforced_read_only = true;
    s.desc.beta = true;
    h = mr_harness(s);
    QCOMPARE(h.eligibility.state, TerminalState::ChatUnsupported);
    QVERIFY(h.eligibility.reason.contains(QStringLiteral("BETA")));
}

void TestModelRouter::eligibility_not_installed() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.installation.reset();
    QCOMPARE(mr_harness(s).eligibility.state, TerminalState::NotInstalled);
    s.status.detection_complete = false;
    QCOMPARE(mr_harness(s).eligibility.state, TerminalState::Detecting);
}

void TestModelRouter::key_vars_reaching_child_respects_policy() {
    const HarnessDescriptor d = mr_claude_desc();
    const QStringList sys = {QStringLiteral("PATH"), QStringLiteral("ANTHROPIC_API_KEY")};
    QCOMPARE(key_vars_reaching_child(d, sys, false), QStringList{QStringLiteral("ANTHROPIC_API_KEY")});
    QVERIFY(key_vars_reaching_child(d, sys, true).isEmpty());
    HarnessDescriptor api_only = d;
    api_only.api_key_only = true; // the policy does not apply
    QCOMPARE(key_vars_reaching_child(api_only, sys, true), QStringList{QStringLiteral("ANTHROPIC_API_KEY")});
}

void TestModelRouter::subscription_session_refusal_reads_init_only() {
    const QString none = QStringLiteral(
        R"({"type":"system","subtype":"init","session_id":"s","model":"m","apiKeySource":"none"})");
    const QString key = QStringLiteral(
        R"({"type":"system","subtype":"init","session_id":"s","model":"m","apiKeySource":"ANTHROPIC_API_KEY"})");
    const QString other = QStringLiteral(R"({"type":"assistant","message":{"content":"apiKeySource none"}})");
    QVERIFY(!subscription_session_refusal(QStringLiteral("claude"), none).isEmpty());
    QVERIFY(subscription_session_refusal(QStringLiteral("claude"), key).isEmpty());
    QVERIFY(subscription_session_refusal(QStringLiteral("claude"), other).isEmpty());
    QVERIFY(subscription_session_refusal(QStringLiteral("codex"), none).isEmpty());
    QVERIFY(subscription_session_refusal(QStringLiteral("claude"), QStringLiteral("not json apiKeySource")).isEmpty());
}

// ── Resolution order ─────────────────────────────────────────────────────────

void TestModelRouter::nothing_configured_is_fincept() {
    const RouteResult r = ModelRouter::resolve_with(RouterState{}, QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(r.chain.size(), 1);
    QVERIFY(r.chain.at(0).is_fincept());
    QCOMPARE(r.chain.at(0).reason, QStringLiteral("fincept_fallback"));
    QCOMPARE(r.chain.at(0).provider_id(), QStringLiteral("fincept"));
}

void TestModelRouter::active_provider_then_fincept() {
    RouterState st;
    mr_add_active_openai(st);
    st.provider_names.insert(QStringLiteral("openai"), QStringLiteral("OpenAI"));
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(r.chain.size(), 2);
    QCOMPARE(r.chain.at(0).kind, ModelChoice::Kind::Api);
    QCOMPARE(r.chain.at(0).reason, QStringLiteral("active_provider"));
    QCOMPARE(r.chain.at(0).label, QStringLiteral("OpenAI · gpt-5"));
    QVERIFY(r.chain.at(1).is_fincept());
    // Without a key the active row is passed over; the active row "fincept" adds nothing.
    st.active_row.has_key = false;
    QCOMPARE(ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat)).chain.size(), 1);
    st.active_row.provider = QStringLiteral("fincept");
    st.active_row.key_required = false;
    QCOMPARE(ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat)).chain.size(), 1);
}

void TestModelRouter::default_profile_before_active_row() {
    RouterState st;
    mr_add_active_openai(st);
    st.profiles.insert(QStringLiteral("p1"), RouterProfile{QStringLiteral("p1"), QStringLiteral("Groq fast"),
                                                           QStringLiteral("groq"), QStringLiteral("llama"), {}, true,
                                                           true, true});
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(r.chain.size(), 3);
    QCOMPARE(r.chain.at(0).provider, QStringLiteral("groq"));
    QCOMPARE(r.chain.at(0).reason, QStringLiteral("profile"));
    QCOMPARE(r.chain.at(1).reason, QStringLiteral("active_provider"));
}

void TestModelRouter::eligible_harness_is_the_automatic_default() {
    RouterState st = mr_state_with({mr_harness(mr_ready_claude())});
    mr_add_active_openai(st);
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(r.chain.size(), 3);
    QVERIFY(r.chain.at(0).is_harness());
    QCOMPARE(r.chain.at(0).reason, QStringLiteral("harness_default"));
    QCOMPARE(r.chain.at(0).label, QStringLiteral("Claude Code · sonnet · high"));
    QCOMPARE(r.chain.at(0).provider_id(), QStringLiteral("harness:claude"));
    QCOMPARE(r.chain.at(1).kind, ModelChoice::Kind::Api);
    QVERIFY(r.chain.at(2).is_fincept());
    QVERIFY(r.skipped.isEmpty());
}

void TestModelRouter::explicit_choice_wins_and_rest_is_fallback() {
    RouterState st = mr_state_with({mr_harness(mr_ready_claude())});
    st.profiles.insert(QStringLiteral("p1"), RouterProfile{QStringLiteral("p1"), QStringLiteral("Mine"),
                                                           QStringLiteral("openai"), QStringLiteral("gpt-4.1"), {}, false,
                                                           true, true});
    st.assignments.insert(ModelRouter::assignment_key(QString::fromLatin1(kFeatureAiChat), {}), QStringLiteral("p1"));
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(r.chain.at(0).reason, QStringLiteral("explicit"));
    QCOMPARE(r.chain.at(0).model, QStringLiteral("gpt-4.1"));
    QVERIFY(r.chain.at(1).is_harness()); // the connected harness is the first fallback
    QVERIFY(r.chain.constLast().is_fincept());
    // A context-specific assignment beats the type-level one.
    st.profiles.insert(QStringLiteral("p2"), RouterProfile{QStringLiteral("p2"), QStringLiteral("Node"),
                                                           QStringLiteral("fincept"), {}, {}, false, false, false});
    st.assignments.insert(ModelRouter::assignment_key(QString::fromLatin1(kFeatureToolPicker), QStringLiteral("node7")),
                          QStringLiteral("p2"));
    const RouteResult node = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureToolPicker), QStringLiteral("node7"));
    QVERIFY(node.chain.at(0).is_fincept() && node.chain.at(0).reason == QStringLiteral("explicit"));
}

void TestModelRouter::explicit_harness_profile() {
    MrHarnessSetup s = mr_ready_claude();
    s.enabled = false; // not the default — an explicit choice still works
    RouterState st = mr_state_with({mr_harness(s)});
    st.profiles.insert(QStringLiteral("h1"), RouterProfile{QStringLiteral("h1"), QStringLiteral("Claude opus"),
                                                           QStringLiteral("harness:claude"), QStringLiteral("opus"),
                                                           QStringLiteral("max"), false, false, false});
    st.assignments.insert(ModelRouter::assignment_key(QString::fromLatin1(kFeatureQuickChat), {}), QStringLiteral("h1"));
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureQuickChat));
    QVERIFY(r.chain.at(0).is_harness());
    QCOMPARE(r.chain.at(0).reason, QStringLiteral("explicit"));
    QCOMPARE(r.chain.at(0).label, QStringLiteral("Claude Code · opus · max"));
    QCOMPARE(r.chain.at(0).profile_id, QStringLiteral("h1"));
    QCOMPARE(r.chain.size(), 2); // + Fincept (the harness is not the default)
}

void TestModelRouter::explicit_harness_without_consent_is_skipped() {
    MrHarnessSetup s = mr_ready_claude();
    s.consent = false;
    RouterState st = mr_state_with({mr_harness(s)});
    st.profiles.insert(QStringLiteral("h1"), RouterProfile{QStringLiteral("h1"), QStringLiteral("Claude"),
                                                           QStringLiteral("harness:claude"), {}, {}, false, false, false});
    st.assignments.insert(ModelRouter::assignment_key(QString::fromLatin1(kFeatureAiChat), {}), QStringLiteral("h1"));
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QVERIFY(r.chain.at(0).is_fincept());
    QVERIFY(mr_skipped_mentions(r, QStringLiteral("Claude Code"), QStringLiteral("Needs your OK")));
}

void TestModelRouter::subscription_harness_is_skipped_with_reason() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.auth.method = QStringLiteral("subscription");
    RouterState st = mr_state_with({mr_harness(s)}, QStringLiteral("claude"));
    mr_add_active_openai(st);
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(r.chain.at(0).kind, ModelChoice::Kind::Api);
    QVERIFY(mr_skipped_mentions(r, QStringLiteral("Claude Code"), QStringLiteral("Workbench only")));
    for (const ModelChoice& c : r.chain)
        QVERIFY(!c.is_harness());
}

void TestModelRouter::signed_out_default_harness_is_skipped() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.auth.state = AuthState::SignedOut;
    s.status.auth.method.clear();
    const RouteResult r =
        ModelRouter::resolve_with(mr_state_with({mr_harness(s)}, QStringLiteral("claude")), QString::fromLatin1(kFeatureAiChat));
    QVERIFY(r.chain.at(0).is_fincept());
    QVERIFY(mr_skipped_mentions(r, QStringLiteral("Claude Code"), QStringLiteral("not signed in")));
}

void TestModelRouter::cooling_down_harness_is_skipped() {
    MrHarnessSetup s = mr_ready_claude();
    s.status.cooldown_until_ms = QDateTime::currentMSecsSinceEpoch() + 5 * 60000;
    const RouteResult r = ModelRouter::resolve_with(mr_state_with({mr_harness(s)}), QString::fromLatin1(kFeatureAiChat));
    QVERIFY(r.chain.at(0).is_fincept());
    QVERIFY(mr_skipped_mentions(r, QStringLiteral("Claude Code"), QStringLiteral("rate limit")));
}

void TestModelRouter::feature_not_suitable_never_gets_a_harness() {
    RouterState st = mr_state_with({mr_harness(mr_ready_claude())});
    st.profiles.insert(QStringLiteral("h1"), RouterProfile{QStringLiteral("h1"), QStringLiteral("Claude"),
                                                           QStringLiteral("harness:claude"), {}, {}, false, false, false});
    st.assignments.insert(ModelRouter::assignment_key(QStringLiteral("team"), {}), QStringLiteral("h1"));
    const RouteResult r = ModelRouter::resolve_with(st, QStringLiteral("team"));
    for (const ModelChoice& c : r.chain)
        QVERIFY(!c.is_harness());
    QVERIFY(mr_skipped_mentions(r, QStringLiteral("Claude Code"), QStringLiteral("cannot use a harness")));
    QCOMPARE(feature_spec(QStringLiteral("team"))->harness_fit, HarnessFit::No);
}

void TestModelRouter::explicit_only_feature_never_defaults_to_a_harness() {
    const RouteResult r =
        ModelRouter::resolve_with(mr_state_with({mr_harness(mr_ready_claude())}), QString::fromLatin1(kFeatureArena));
    for (const ModelChoice& c : r.chain)
        QVERIFY(!c.is_harness());
    QVERIFY(feature_spec(QString::fromLatin1(kFeatureArena))->explicit_harness_only);
}

void TestModelRouter::data_policy_off_until_the_feature_switch() {
    RouterState st = mr_state_with({mr_harness(mr_ready_claude())});
    const QString feature = QString::fromLatin1(kFeatureChatModeHarness);
    QVERIFY(feature_spec(feature)->sends_user_financial_data);
    QVERIFY(!ModelRouter::default_allowed_for(st, feature));
    RouteResult r = ModelRouter::resolve_with(st, feature);
    QVERIFY(!r.chain.at(0).is_harness());
    QVERIFY(mr_skipped_mentions(r, QStringLiteral("Claude Code"), QStringLiteral("financial data")));
    st.harness_default_allowed.insert(feature, true);
    r = ModelRouter::resolve_with(st, feature);
    QVERIFY(r.chain.at(0).is_harness());
    // The opposite switch for a plain feature.
    st.harness_default_allowed.insert(QString::fromLatin1(kFeatureAiChat), false);
    QVERIFY(!ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat)).chain.at(0).is_harness());
}

void TestModelRouter::not_enabled_harness_is_explicit_only() {
    MrHarnessSetup s = mr_ready_claude();
    s.enabled = false;
    const RouteResult r = ModelRouter::resolve_with(mr_state_with({mr_harness(s)}), QString::fromLatin1(kFeatureAiChat));
    QVERIFY(r.chain.at(0).is_fincept());
    QVERIFY(r.skipped.isEmpty()); // switched off on purpose: nothing to explain
}

void TestModelRouter::default_harness_preferred_over_registration_order() {
    MrHarnessSetup codex;
    codex.desc = mr_codex_desc();
    codex.status = mr_installed(QStringLiteral("codex"), AuthState::SignedIn, QStringLiteral("api_key"));
    const RouterState st = mr_state_with({mr_harness(mr_ready_claude()), mr_harness(codex)}, QStringLiteral("codex"));
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(r.chain.at(0).harness_id, QStringLiteral("codex"));
    // Only ONE automatic harness: the next entry is not the other harness.
    QVERIFY(r.chain.size() == 2 && r.chain.at(1).is_fincept());
}

void TestModelRouter::key_less_profile_is_skipped() {
    RouterState st;
    st.profiles.insert(QStringLiteral("p1"), RouterProfile{QStringLiteral("p1"), QStringLiteral("Broken"),
                                                           QStringLiteral("openai"), QStringLiteral("gpt-5"), {}, false,
                                                           false, true});
    st.assignments.insert(ModelRouter::assignment_key(QString::fromLatin1(kFeatureAiChat), {}), QStringLiteral("p1"));
    const RouteResult r = ModelRouter::resolve_with(st, QString::fromLatin1(kFeatureAiChat));
    QVERIFY(r.chain.at(0).is_fincept());
    QVERIFY(mr_skipped_mentions(r, QStringLiteral("Profile “Broken”"), QStringLiteral("no API key")));
}

// ── Fallback ─────────────────────────────────────────────────────────────────

void TestModelRouter::fallback_before_output_falls_back_once() {
    const RouteResult r =
        ModelRouter::resolve_with(mr_state_with({mr_harness(mr_ready_claude())}), QString::fromLatin1(kFeatureAiChat));
    const struct {
        FailureKind kind;
        bool subscription;
        const char* what;
    } cases[] = {{FailureKind::RateLimit, false, "is rate limited"},
                 {FailureKind::Auth, false, "is signed out"},
                 {FailureKind::Auth, true, "is not using an API key"},
                 {FailureKind::NotInstalled, false, "is not available"},
                 {FailureKind::Timeout, false, "did not answer in time"}};
    for (const auto& c : cases) {
        const FallbackDecision d = ModelRouter::decide_fallback(r.chain, 0, c.kind, false, false, c.subscription,
                                                                UnavailableBehaviour::Fallback);
        QVERIFY(d.fall_back);
        QCOMPARE(d.next_index, 1);
        QCOMPARE(d.notice, QStringLiteral("Claude Code %1 — answered by Fincept LLM").arg(QLatin1String(c.what)));
    }
}

void TestModelRouter::no_fallback_after_output_or_cancel() {
    const RouteResult r =
        ModelRouter::resolve_with(mr_state_with({mr_harness(mr_ready_claude())}), QString::fromLatin1(kFeatureAiChat));
    QVERIFY(!ModelRouter::decide_fallback(r.chain, 0, FailureKind::RateLimit, true, false, false,
                                          UnavailableBehaviour::Fallback)
                 .fall_back);
    QVERIFY(!ModelRouter::decide_fallback(r.chain, 0, FailureKind::RateLimit, false, true, false,
                                          UnavailableBehaviour::Fallback)
                 .fall_back);
    // Not a harness entry: the router never falls back from it.
    QVERIFY(!ModelRouter::decide_fallback(r.chain, 1, FailureKind::RateLimit, false, false, false,
                                          UnavailableBehaviour::Fallback)
                 .fall_back);
}

void TestModelRouter::no_fallback_for_other_failures() {
    const RouteResult r =
        ModelRouter::resolve_with(mr_state_with({mr_harness(mr_ready_claude())}), QString::fromLatin1(kFeatureAiChat));
    for (FailureKind k : {FailureKind::Model, FailureKind::Other, FailureKind::None}) {
        const FallbackDecision d =
            ModelRouter::decide_fallback(r.chain, 0, k, false, false, false, UnavailableBehaviour::Fallback);
        QVERIFY(!d.fall_back && !d.ask && d.notice.isEmpty());
    }
}

void TestModelRouter::ask_and_fail_settings() {
    const RouteResult r =
        ModelRouter::resolve_with(mr_state_with({mr_harness(mr_ready_claude())}), QString::fromLatin1(kFeatureAiChat));
    const FallbackDecision ask =
        ModelRouter::decide_fallback(r.chain, 0, FailureKind::RateLimit, false, false, false, UnavailableBehaviour::Ask);
    QVERIFY(!ask.fall_back && ask.ask);
    QCOMPARE(ask.next_index, 1);
    QCOMPARE(ask.notice, QStringLiteral("Claude Code is rate limited. Answer with Fincept LLM instead?"));
    const FallbackDecision fail =
        ModelRouter::decide_fallback(r.chain, 0, FailureKind::RateLimit, false, false, false, UnavailableBehaviour::Fail);
    QVERIFY(!fail.fall_back && !fail.ask);
    QCOMPARE(fail.notice, QStringLiteral("Claude Code is rate limited."));
    QCOMPARE(unavailable_behaviour_from_id(unavailable_behaviour_id(UnavailableBehaviour::Ask)), UnavailableBehaviour::Ask);
    QCOMPARE(unavailable_behaviour_from_id(QStringLiteral("garbage")), UnavailableBehaviour::Fallback);
}

void TestModelRouter::no_next_entry_means_notice_only() {
    QVector<ModelChoice> chain;
    ModelChoice h;
    h.kind = ModelChoice::Kind::Harness;
    h.harness_id = QStringLiteral("claude");
    h.label = QStringLiteral("Claude Code · sonnet");
    chain << h;
    const FallbackDecision d =
        ModelRouter::decide_fallback(chain, 0, FailureKind::Timeout, false, false, false, UnavailableBehaviour::Fallback);
    QVERIFY(!d.fall_back);
    QCOMPARE(d.notice, QStringLiteral("Claude Code did not answer in time."));
}

void TestModelRouter::billing_note_for_harness() {
    const RouteResult r =
        ModelRouter::resolve_with(mr_state_with({mr_harness(mr_ready_claude())}), QString::fromLatin1(kFeatureAiChat));
    QCOMPARE(ModelRouter::billing_note(r.chain.at(0)), QStringLiteral("0 credits · billed to your Anthropic API key"));
    QVERIFY(ModelRouter::billing_note(r.chain.constLast()).isEmpty()); // Fincept LLM: credits, said elsewhere
}

QTEST_GUILESS_MAIN(TestModelRouter)
#include "tst_model_router.moc"
