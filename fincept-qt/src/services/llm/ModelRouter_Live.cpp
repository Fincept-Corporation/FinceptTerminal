// ModelRouter_Live.cpp — the live ModelRouter singleton: gathers the rule's
// inputs (harness registry, profiles, assignments, the active provider row,
// settings) into a snapshot on the GUI thread and answers resolve*() from it
// on any thread. The rule itself is in ModelRouter.cpp.

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "services/harness/HarnessEligibility.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/llm/LlmService.h"
#include "services/llm/ModelRouter.h"
#include "services/llm/ProviderCatalog.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QThread>
#include <QTimer>
#include <QUuid>

namespace fincept::ai_chat {

namespace model_router_live_detail {

constexpr const char* kMrlTag = "ModelRouter";
constexpr const char* kMrlBehaviourKey = "llm/route/harness_unavailable";

QString mrl_default_key(const QString& feature) {
    return QStringLiteral("llm/route/%1/harness_default").arg(feature);
}

bool mrl_on_gui_thread() {
    QCoreApplication* app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}

/// Profiles, assignments, the active provider row and the per-feature
/// switches: database + settings, safe from any thread.
void mrl_gather_db(RouterState& s) {
    s.profiles.clear();
    s.assignments.clear();
    s.active_row = RouterActiveRow{};
    s.harness_default_allowed.clear();
    s.provider_names.clear();

    if (auto r = LlmProfileRepository::instance().list_all_profiles(); r.is_ok()) {
        for (const LlmProfile& p : r.value()) {
            RouterProfile rp;
            rp.id = p.id;
            rp.name = p.name;
            rp.provider = p.provider;
            rp.model = p.model_id;
            rp.effort = p.reasoning_effort;
            rp.is_default = p.is_default;
            rp.has_key = !p.api_key.isEmpty();
            rp.key_required = !p.is_harness() && ProviderCatalog::requires_api_key(p.provider.toLower());
            s.profiles.insert(p.id, rp);
        }
    } else {
        LOG_WARN(kMrlTag, QStringLiteral("Profiles could not be read: %1").arg(QString::fromStdString(r.error())));
    }
    if (auto r = LlmProfileRepository::instance().list_assignments(); r.is_ok()) {
        for (const LlmProfileAssignment& a : r.value())
            s.assignments.insert(ModelRouter::assignment_key(a.context_type, a.context_id), a.profile_id);
    }
    // The active provider row (else the first one — what LlmService always used).
    if (auto r = LlmConfigRepository::instance().list_providers(); r.is_ok() && !r.value().isEmpty()) {
        const QVector<LlmConfig>& rows = r.value();
        const LlmConfig* pick = &rows.constFirst();
        for (const LlmConfig& c : rows) {
            if (c.is_active) {
                pick = &c;
                break;
            }
        }
        s.active_row.present = true;
        s.active_row.provider = pick->provider.toLower();
        s.active_row.model = pick->model;
        s.active_row.has_key = !pick->api_key.isEmpty();
        s.active_row.key_required = ProviderCatalog::requires_api_key(s.active_row.provider);
    }
    auto& cfg = AppConfig::instance();
    for (const FeatureSpec& f : feature_specs()) {
        const QVariant v = cfg.get(mrl_default_key(QString::fromLatin1(f.key)));
        if (v.isValid())
            s.harness_default_allowed.insert(QString::fromLatin1(f.key), v.toBool());
    }
    for (const QString& p : ProviderCatalog::known_providers())
        s.provider_names.insert(p, ProviderCatalog::display_name(p));
}

/// Harness rows from the registry. GUI thread only. Returns the earliest
/// future moment a cooldown / refusal ends (0 = none): the rows change then.
qint64 mrl_gather_harnesses(RouterState& s, const QHash<QString, qint64>& refused_until) {
    s.harnesses.clear();
    qint64 next_change = 0;
    auto& reg = harness::HarnessRegistry::instance();
    const QStringList system_names = QProcessEnvironment::systemEnvironment().keys(); // names only, never values
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QString& id : reg.ids()) {
        const harness::IHarnessAdapter* a = reg.adapter(id);
        if (!a)
            continue;
        const harness::HarnessDescriptor& d = a->descriptor();
        harness::TerminalEligibilityInput in;
        in.descriptor = d;
        in.status = reg.status(id);
        in.key_vars_in_system = harness::key_vars_reaching_child(d, system_names, /*subscription_only=*/false);
        in.key_vars_in_child = harness::key_vars_reaching_child(
            d, system_names, reg.env_policy(id) == QLatin1String(harness::HarnessRegistry::kEnvPolicySubscriptionOnly));
        in.enabled_for_terminal = reg.enabled_for_terminal(id);
        in.consent = reg.terminal_consent(id);
        in.now_ms = now;
        in.refused_until_ms = refused_until.value(id, 0);
        for (const qint64 until : {in.status.cooldown_until_ms, in.refused_until_ms}) {
            if (until > now && (next_change == 0 || until < next_change))
                next_change = until;
        }
        RouterHarness h;
        h.id = id;
        h.display_name = d.display_name;
        h.vendor = d.vendor;
        h.eligibility = harness::terminal_eligibility(in);
        h.default_model = reg.default_model(id);
        h.default_effort = d.caps.effort ? reg.default_effort(id) : QString();
        h.enabled_for_terminal = in.enabled_for_terminal;
        s.harnesses.append(h);
    }
    s.default_harness = reg.default_harness();
    return next_change;
}

/// The profile that stores `choice` (reused when one already says the same), or "".
QString mrl_profile_for(const ModelChoice& choice, QString* error) {
    auto& repo = LlmProfileRepository::instance();
    const auto all = repo.list_all_profiles();
    if (all.is_err()) {
        if (error)
            *error = QString::fromStdString(all.error());
        return {};
    }
    const QString provider = choice.provider_id().toLower();
    for (const LlmProfile& p : all.value()) {
        if (p.provider.toLower() != provider)
            continue;
        if (choice.kind == ModelChoice::Kind::Fincept)
            return p.id;
        if (p.model_id == choice.model && p.reasoning_effort == choice.effort)
            return p.id;
    }
    LlmProfile p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.provider = provider;
    p.model_id = choice.model;
    p.reasoning_effort = choice.kind == ModelChoice::Kind::Harness ? choice.effort : QString();
    p.name = choice.label.isEmpty() ? provider : choice.label;
    if (choice.kind == ModelChoice::Kind::Api) {
        // The active provider row's key and endpoint (the profile editor does the same).
        if (auto rows = LlmConfigRepository::instance().list_providers(); rows.is_ok()) {
            for (const LlmConfig& c : rows.value()) {
                if (c.provider.toLower() == provider) {
                    p.api_key = c.api_key;
                    p.base_url = c.base_url;
                    break;
                }
            }
        }
    }
    const auto saved = repo.save_profile(p);
    if (saved.is_err()) {
        if (error)
            *error = QString::fromStdString(saved.error());
        return {};
    }
    LOG_INFO(kMrlTag, QStringLiteral("Created profile “%1” for a model choice").arg(p.name));
    return p.id;
}

} // namespace model_router_live_detail

using namespace model_router_live_detail;

ModelRouter& ModelRouter::instance() {
    static ModelRouter r;
    return r;
}

ModelRouter::ModelRouter() : QObject(nullptr) {
    behaviour_ = unavailable_behaviour_from_id(AppConfig::instance().get(QString::fromLatin1(kMrlBehaviourKey)).toString());
    QCoreApplication* app = QCoreApplication::instance();
    if (app && thread() != app->thread())
        moveToThread(app->thread());
    if (!app)
        return;
    auto& reg = harness::HarnessRegistry::instance();
    connect(&reg, &harness::HarnessRegistry::status_changed, this, [this](const QString&) { schedule_refresh(); });
    connect(&reg, &harness::HarnessRegistry::defaults_changed, this, [this]() { schedule_refresh(); });
    connect(&LlmService::instance(), &LlmService::config_changed, this, [this]() {
        {
            QMutexLocker lock(&mutex_);
            have_state_ = false; // profiles / rows changed: the next refresh re-reads the database
        }
        schedule_refresh();
    });
    if (mrl_on_gui_thread())
        refresh_now();
    else
        schedule_refresh();
}

void ModelRouter::schedule_refresh() {
    {
        QMutexLocker lock(&mutex_);
        if (refresh_pending_)
            return;
        refresh_pending_ = true;
    }
    QTimer::singleShot(0, this, [this]() { refresh_now(); });
}

void ModelRouter::refresh() {
    {
        QMutexLocker lock(&mutex_);
        have_state_ = false;
    }
    if (mrl_on_gui_thread())
        refresh_now();
    else
        schedule_refresh();
}

void ModelRouter::refresh_now() {
    // GUI thread. The database part is re-read only when it may have changed.
    RouterState s;
    QHash<QString, qint64> refused;
    bool reread_db = false;
    {
        QMutexLocker lock(&mutex_);
        refresh_pending_ = false;
        s = state_;
        refused = refused_until_;
        reread_db = !have_state_;
    }
    if (reread_db)
        mrl_gather_db(s);
    const qint64 next_change = mrl_gather_harnesses(s, refused);
    {
        QMutexLocker lock(&mutex_);
        state_ = s;
        have_state_ = true;
    }
    if (next_change > 0) {
        // A rate-limit cooldown or a refusal ends then: the harness is usable
        // again without any other event, so look again at that moment.
        if (!expiry_timer_) {
            expiry_timer_ = new QTimer(this);
            expiry_timer_->setSingleShot(true);
            connect(expiry_timer_, &QTimer::timeout, this, [this]() { schedule_refresh(); });
        }
        const qint64 in_ms = next_change - QDateTime::currentMSecsSinceEpoch() + 500;
        expiry_timer_->start(static_cast<int>(qBound<qint64>(500, in_ms, 24LL * 3600 * 1000)));
    }
    emit routes_changed();
}

RouterState ModelRouter::state() const {
    {
        QMutexLocker lock(&mutex_);
        if (have_state_)
            return state_;
    }
    // First use before the GUI thread built a snapshot (startup race): the
    // database part now, on this thread; harness rows join on the next refresh.
    RouterState s;
    mrl_gather_db(s);
    QMutexLocker lock(&mutex_);
    if (!have_state_) {
        s.harnesses = state_.harnesses;
        s.default_harness = state_.default_harness;
        return s;
    }
    return state_;
}

QVector<RouterHarness> ModelRouter::harness_states() const {
    return state().harnesses;
}

std::optional<RouterHarness> ModelRouter::current_harness(const QString& harness_id) const {
    if (!mrl_on_gui_thread())
        return std::nullopt; // the registry is GUI-thread state
    QHash<QString, qint64> refused;
    {
        QMutexLocker lock(&mutex_);
        refused = refused_until_;
    }
    RouterState s;
    mrl_gather_harnesses(s, refused);
    for (const RouterHarness& h : std::as_const(s.harnesses)) {
        if (h.id == harness_id)
            return h;
    }
    return std::nullopt;
}

RouteResult ModelRouter::route(const QString& feature, const QString& context_id) const {
    return resolve_with(state(), feature, context_id);
}

QVector<ModelChoice> ModelRouter::resolve_chain(const QString& feature, const QString& context_id) const {
    return route(feature, context_id).chain;
}

ModelChoice ModelRouter::resolve(const QString& feature, const QString& context_id) const {
    const QVector<ModelChoice> chain = resolve_chain(feature, context_id);
    return chain.isEmpty() ? ModelChoice{} : chain.constFirst();
}

UnavailableBehaviour ModelRouter::unavailable_behaviour() const {
    QMutexLocker lock(&mutex_);
    return behaviour_;
}

void ModelRouter::set_unavailable_behaviour(UnavailableBehaviour b) {
    {
        QMutexLocker lock(&mutex_);
        behaviour_ = b;
    }
    if (b == UnavailableBehaviour::Fallback)
        AppConfig::instance().remove(QString::fromLatin1(kMrlBehaviourKey));
    else
        AppConfig::instance().set(QString::fromLatin1(kMrlBehaviourKey), unavailable_behaviour_id(b));
    emit routes_changed();
}

bool ModelRouter::harness_default_allowed(const QString& feature) const {
    return default_allowed_for(state(), feature);
}

void ModelRouter::set_harness_default_allowed(const QString& feature, bool allowed) {
    const FeatureSpec* f = feature_spec(feature);
    const bool spec_default = f ? !f->sends_user_financial_data : true;
    if (allowed == spec_default)
        AppConfig::instance().remove(mrl_default_key(feature));
    else
        AppConfig::instance().set(mrl_default_key(feature), allowed);
    {
        QMutexLocker lock(&mutex_);
        if (allowed == spec_default)
            state_.harness_default_allowed.remove(feature);
        else
            state_.harness_default_allowed.insert(feature, allowed);
    }
    LOG_INFO(kMrlTag, QStringLiteral("Automatic harness for %1: %2").arg(feature, allowed ? QStringLiteral("allowed")
                                                                                         : QStringLiteral("off")));
    emit routes_changed();
}

bool ModelRouter::set_choice(const QString& feature, const QString& context_id, const ModelChoice& choice,
                             QString* error) {
    QString pid = choice.profile_id;
    if (pid.isEmpty() || choice.kind != ModelChoice::Kind::Api)
        pid = mrl_profile_for(choice, error);
    if (pid.isEmpty())
        return false;
    const auto r = LlmProfileRepository::instance().assign_profile(feature, context_id, pid);
    if (r.is_err()) {
        if (error)
            *error = QString::fromStdString(r.error());
        return false;
    }
    LOG_INFO(kMrlTag, QStringLiteral("Model for %1: %2").arg(feature, choice.label));
    refresh();
    return true;
}

void ModelRouter::clear_choice(const QString& feature, const QString& context_id) {
    LlmProfileRepository::instance().remove_assignment(feature, context_id);
    LOG_INFO(kMrlTag, QStringLiteral("Model for %1: Auto").arg(feature));
    refresh();
}

QString ModelRouter::choice_profile_id(const QString& feature, const QString& context_id) const {
    return state().assignments.value(assignment_key(feature, context_id));
}

void ModelRouter::refuse_harness_for(const QString& harness_id, qint64 ms) {
    {
        QMutexLocker lock(&mutex_);
        refused_until_.insert(harness_id, QDateTime::currentMSecsSinceEpoch() + ms);
    }
    if (mrl_on_gui_thread())
        refresh_now();
    else
        schedule_refresh();
}

} // namespace fincept::ai_chat
