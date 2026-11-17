// ModelRouter.cpp — the resolution rule, the fallback decision and the feature
// table (see ModelRouter.h). Pure QtCore: no settings, registry or database
// here — the live singleton is in ModelRouter_Live.cpp.

#include "services/llm/ModelRouter.h"

#include <QStringList>

namespace fincept::ai_chat {

namespace model_router_detail {

constexpr const char* kMrHarnessPrefix = "harness:";

ModelChoice mr_fincept(const QString& model, const QString& reason, const QString& profile_id = {}) {
    ModelChoice c;
    c.kind = ModelChoice::Kind::Fincept;
    c.provider = QStringLiteral("fincept");
    c.model = model;
    c.profile_id = profile_id;
    c.label = QStringLiteral("Fincept LLM");
    c.reason = reason;
    c.vendor = QStringLiteral("Fincept");
    c.require_api_key_source = false;
    return c;
}

QString mr_provider_name(const RouterState& s, const QString& provider) {
    const QString n = s.provider_names.value(provider.toLower());
    if (!n.isEmpty())
        return n;
    return provider.isEmpty() ? provider : provider.left(1).toUpper() + provider.mid(1);
}

ModelChoice mr_api(const RouterState& s, const QString& provider, const QString& model, const QString& reason,
                   const QString& profile_id) {
    ModelChoice c;
    c.kind = ModelChoice::Kind::Api;
    c.provider = provider.toLower();
    c.model = model;
    c.profile_id = profile_id;
    c.vendor = mr_provider_name(s, c.provider);
    c.label = model.isEmpty() ? c.vendor : c.vendor + QStringLiteral(" · ") + model;
    c.reason = reason;
    c.require_api_key_source = false;
    return c;
}

ModelChoice mr_harness(const RouterHarness& h, const QString& model, const QString& effort, const QString& reason,
                       const QString& profile_id) {
    ModelChoice c;
    c.kind = ModelChoice::Kind::Harness;
    c.harness_id = h.id;
    c.provider = QString::fromLatin1(kMrHarnessPrefix) + h.id;
    c.model = model;
    c.effort = effort;
    c.profile_id = profile_id;
    c.label = ModelRouter::harness_label(h.display_name, model, effort);
    c.reason = reason;
    c.vendor = h.vendor.isEmpty() ? h.display_name : h.vendor;
    c.require_api_key_source = h.eligibility.require_api_key_source;
    return c;
}

const RouterHarness* mr_find_harness(const RouterState& s, const QString& id) {
    for (const RouterHarness& h : s.harnesses) {
        if (h.id == id)
            return &h;
    }
    return nullptr;
}

/// The feature's spec; an unknown key behaves like a plain chat feature.
FeatureSpec mr_spec_or_default(const QString& feature) {
    if (const FeatureSpec* f = feature_spec(feature))
        return *f;
    return FeatureSpec{"", "AI", HarnessFit::Ok, false, true, false, false, false, true, ""};
}

QString mr_who(const ModelChoice& c) {
    const QString first = c.label.section(QStringLiteral(" · "), 0, 0);
    return first.isEmpty() ? c.harness_id : first;
}

} // namespace model_router_detail

using namespace model_router_detail;

// ── Features ─────────────────────────────────────────────────────────────────

const QVector<FeatureSpec>& feature_specs() {
    // key, label, fit, financial data, streaming, tools, json, explicit-only, routed, why not routed
    static const QVector<FeatureSpec> k = {
        {kFeatureAiChat, "AI Chat", HarnessFit::Good, false, true, true, false, false, true, ""},
        {kFeatureQuickChat, "Quick Chat bubble", HarnessFit::Ok, false, true, true, false, false, true, ""},
        {kFeatureQuickAsk, "Dashboard Quick Ask", HarnessFit::Good, false, true, false, false, false, true, ""},
        {kFeatureToolPicker, "Workflow tool-picker node", HarnessFit::Ok, false, false, false, true, false, true, ""},
        {kFeatureChatModeHarness, "Chat Mode · harness tasks", HarnessFit::Good, true, true, true, false, false, true,
         ""},
        // Report data may include portfolio holdings: a harness only with the per-feature switch.
        {"report_draft", "Report Builder drafting", HarnessFit::Ok, true, true, false, false, false, true, ""},
        {kFeatureArena, "Alpha Arena", HarnessFit::Ok, true, false, false, true, true, false,
         "Chosen per competitor in the arena wizard; a harness is never picked automatically (about 480 calls per "
         "agent per day at the default cadence)."},
        {"portfolio_insights", "Portfolio insights", HarnessFit::Ok, true, false, true, false, false, false,
         "Runs on the Python agent runtime; harnesses need its loopback gateway first."},
        {"agent", "Agent Studio agents", HarnessFit::No, false, true, true, false, false, false,
         "Tool-using agents run on the Python agent runtime: not available for harnesses yet."},
        {"team", "Agent Studio teams", HarnessFit::No, false, true, true, false, false, false,
         "One call per member per round: a harness process per call is too slow."},
        {"workflow", "Agent workflows and planner", HarnessFit::No, false, false, true, true, false, false,
         "Run by the Python agent runtime: not available for harnesses yet."},
        {"agentic_task", "Agentic tasks (Agent Studio)", HarnessFit::No, true, false, true, false, false, false,
         "Use Chat Mode's harness engine for long harness tasks."},
        {"ai_quant_lab", "AI Quant Lab", HarnessFit::No, false, false, true, false, false, false,
         "LangChain tool loops and embeddings: API providers only."},
        {"news_analysis", "News analysis", HarnessFit::Ok, false, false, false, true, false, false,
         "Answered by the Fincept server."},
    };
    return k;
}

const FeatureSpec* feature_spec(const QString& key) {
    for (const FeatureSpec& f : feature_specs()) {
        if (key == QLatin1String(f.key))
            return &f;
    }
    return nullptr;
}

// ── ModelChoice ──────────────────────────────────────────────────────────────

QString ModelChoice::provider_id() const {
    switch (kind) {
        case Kind::Harness:
            return QString::fromLatin1(kMrHarnessPrefix) + harness_id;
        case Kind::Fincept:
            return QStringLiteral("fincept");
        case Kind::Api:
            break;
    }
    return provider;
}

bool ModelChoice::same_route(const ModelChoice& o) const {
    if (kind != o.kind)
        return false;
    if (kind == Kind::Fincept)
        return true;
    return provider_id() == o.provider_id() && model == o.model && effort == o.effort;
}

// ── Pure helpers ─────────────────────────────────────────────────────────────

QString ModelRouter::assignment_key(const QString& feature, const QString& context_id) {
    return feature + QLatin1Char('|') + context_id;
}

bool ModelRouter::default_allowed_for(const RouterState& state, const QString& feature) {
    const auto it = state.harness_default_allowed.constFind(feature);
    if (it != state.harness_default_allowed.cend())
        return it.value();
    return !mr_spec_or_default(feature).sends_user_financial_data;
}

QString ModelRouter::harness_label(const QString& display_name, const QString& model, const QString& effort) {
    QStringList parts{display_name};
    if (!model.isEmpty())
        parts << model;
    if (!effort.isEmpty())
        parts << effort;
    return parts.join(QStringLiteral(" · "));
}

QString ModelRouter::billing_note(const ModelChoice& c) {
    switch (c.kind) {
        case ModelChoice::Kind::Harness:
            return c.require_api_key_source
                       ? QStringLiteral("0 credits · billed to your %1 API key").arg(c.vendor)
                       : QStringLiteral("0 credits · billed to your %1 key").arg(c.vendor);
        case ModelChoice::Kind::Api:
            return QStringLiteral("0 credits · billed to your %1 API key").arg(c.vendor);
        case ModelChoice::Kind::Fincept:
            break;
    }
    return {};
}

// ── The rule ─────────────────────────────────────────────────────────────────

RouteResult ModelRouter::resolve_with(const RouterState& state, const QString& feature, const QString& context_id) {
    RouteResult out;
    const FeatureSpec spec = mr_spec_or_default(feature);
    const QString feature_label = QString::fromUtf8(spec.label);
    const auto add = [&out](const ModelChoice& c) {
        for (const ModelChoice& existing : out.chain) {
            if (existing.same_route(c))
                return;
        }
        out.chain.append(c);
    };
    const auto skip = [&out](const QString& what, const QString& reason, const QString& harness_id = {}) {
        out.skipped.append(SkippedChoice{what, reason, harness_id});
    };

    // ── 1. The user's explicit choice for this feature ──
    QStringList keys;
    if (!context_id.isEmpty())
        keys << assignment_key(feature, context_id);
    keys << assignment_key(feature, {});
    for (const QString& key : keys) {
        const QString pid = state.assignments.value(key);
        if (pid.isEmpty() || !state.profiles.contains(pid))
            continue;
        const RouterProfile& p = state.profiles[pid];
        const QString provider = p.provider.toLower();
        if (provider.startsWith(QLatin1String(kMrHarnessPrefix))) {
            const QString hid = provider.mid(static_cast<qsizetype>(qstrlen(kMrHarnessPrefix)));
            const RouterHarness* h = mr_find_harness(state, hid);
            if (!h) {
                skip(QStringLiteral("Profile “%1”").arg(p.name), QStringLiteral("this build has no harness “%1”").arg(hid),
                     hid);
            } else if (spec.harness_fit == HarnessFit::No) {
                skip(h->display_name, QStringLiteral("%1 cannot use a harness yet").arg(feature_label), hid);
            } else if (!h->eligibility.usable) {
                skip(h->display_name, h->eligibility.label + QStringLiteral(" — ") + h->eligibility.reason, hid);
            } else {
                add(mr_harness(*h, p.model.isEmpty() ? h->default_model : p.model,
                               p.effort.isEmpty() ? h->default_effort : p.effort, QStringLiteral("explicit"), p.id));
            }
        } else if (provider == QLatin1String("fincept")) {
            add(mr_fincept(p.model, QStringLiteral("explicit"), p.id));
        } else if (p.key_required && !p.has_key) {
            skip(QStringLiteral("Profile “%1”").arg(p.name), QStringLiteral("it has no API key"));
        } else {
            add(mr_api(state, provider, p.model, QStringLiteral("explicit"), p.id));
        }
        break; // the most specific assignment decides; the rest of the chain are fallbacks
    }

    // ── 2. The connected harness (the automatic default) ──
    if (spec.harness_fit != HarnessFit::No && !spec.explicit_harness_only) {
        QVector<const RouterHarness*> order;
        if (const RouterHarness* d = mr_find_harness(state, state.default_harness))
            order << d;
        for (const RouterHarness& h : state.harnesses) {
            if (h.id != state.default_harness)
                order << &h;
        }
        const bool allowed = default_allowed_for(state, feature);
        for (const RouterHarness* h : order) {
            const harness::TerminalState st = h->eligibility.state;
            if (!h->eligibility.usable_by_default) {
                // Only harnesses the user meant to use are worth a reason.
                const bool meant = h->id == state.default_harness || h->enabled_for_terminal ||
                                   st == harness::TerminalState::ConsentNeeded ||
                                   st == harness::TerminalState::CoolingDown;
                if (meant && st != harness::TerminalState::NotInstalled && st != harness::TerminalState::Detecting &&
                    st != harness::TerminalState::EligibleNotEnabled)
                    skip(h->display_name, h->eligibility.label + QStringLiteral(" — ") + h->eligibility.reason, h->id);
                continue;
            }
            if (!allowed) {
                skip(h->display_name,
                     QStringLiteral("not used automatically for %1: it sends your financial data to %2. Allow it for "
                                    "this feature in Settings › LLM Configuration › Terminal AI.")
                         .arg(feature_label, h->vendor.isEmpty() ? h->display_name : h->vendor),
                     h->id);
                break;
            }
            add(mr_harness(*h, h->default_model, h->default_effort, QStringLiteral("harness_default"), {}));
            break;
        }
    }

    // ── 3. A configured API provider ──
    for (auto it = state.profiles.cbegin(); it != state.profiles.cend(); ++it) {
        const RouterProfile& p = it.value();
        if (!p.is_default || p.provider.toLower().startsWith(QLatin1String(kMrHarnessPrefix)))
            continue;
        if (p.provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0)
            add(mr_fincept(p.model, QStringLiteral("profile"), p.id));
        else if (!p.key_required || p.has_key)
            add(mr_api(state, p.provider, p.model, QStringLiteral("profile"), p.id));
        else
            skip(QStringLiteral("Default profile “%1”").arg(p.name), QStringLiteral("it has no API key"));
        break;
    }
    const RouterActiveRow& row = state.active_row;
    if (row.present && !row.provider.isEmpty() && row.provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) != 0 &&
        (row.has_key || !row.key_required))
        add(mr_api(state, row.provider, row.model, QStringLiteral("active_provider"), {}));

    // ── 4. Fincept LLM ──
    add(mr_fincept(QString(), QStringLiteral("fincept_fallback")));
    return out;
}

FallbackDecision ModelRouter::decide_fallback(const QVector<ModelChoice>& chain, int failed_index,
                                              harness::FailureKind kind, bool output_started, bool cancelled,
                                              bool subscription_detected, UnavailableBehaviour behaviour) {
    FallbackDecision d;
    if (failed_index < 0 || failed_index >= chain.size() || !chain[failed_index].is_harness())
        return d;
    // Never answer twice: once anything reached the user, the failure stands.
    if (cancelled || output_started)
        return d;
    using harness::FailureKind;
    if (kind != FailureKind::Auth && kind != FailureKind::RateLimit && kind != FailureKind::NotInstalled &&
        kind != FailureKind::Timeout)
        return d;
    const QString who = mr_who(chain[failed_index]);
    QString what;
    if (subscription_detected)
        what = QStringLiteral("is not using an API key");
    else if (kind == FailureKind::Auth)
        what = QStringLiteral("is signed out");
    else if (kind == FailureKind::RateLimit)
        what = QStringLiteral("is rate limited");
    else if (kind == FailureKind::NotInstalled)
        what = QStringLiteral("is not available");
    else
        what = QStringLiteral("did not answer in time");
    const int next = failed_index + 1;
    if (next >= chain.size()) {
        d.notice = QStringLiteral("%1 %2.").arg(who, what);
        return d;
    }
    const QString next_label = chain[next].label;
    switch (behaviour) {
        case UnavailableBehaviour::Fallback:
            d.fall_back = true;
            d.next_index = next;
            d.notice = QStringLiteral("%1 %2 — answered by %3").arg(who, what, next_label);
            break;
        case UnavailableBehaviour::Ask:
            d.ask = true;
            d.next_index = next;
            d.notice = QStringLiteral("%1 %2. Answer with %3 instead?").arg(who, what, next_label);
            break;
        case UnavailableBehaviour::Fail:
            d.notice = QStringLiteral("%1 %2.").arg(who, what);
            break;
    }
    return d;
}

QString unavailable_behaviour_id(UnavailableBehaviour b) {
    switch (b) {
        case UnavailableBehaviour::Ask:
            return QStringLiteral("ask");
        case UnavailableBehaviour::Fail:
            return QStringLiteral("fail");
        case UnavailableBehaviour::Fallback:
            break;
    }
    return QStringLiteral("fallback");
}

UnavailableBehaviour unavailable_behaviour_from_id(const QString& id) {
    if (id == QLatin1String("ask"))
        return UnavailableBehaviour::Ask;
    if (id == QLatin1String("fail"))
        return UnavailableBehaviour::Fail;
    return UnavailableBehaviour::Fallback;
}

} // namespace fincept::ai_chat
