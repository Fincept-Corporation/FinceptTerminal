// src/ui/widgets/RequirementsLive.cpp — the live RequirementEnvironment and
// open_setup_link() (see Requirements.h). Kept apart from Requirements.cpp so
// the pure check links into unit tests without the app's singletons.

#include "auth/AuthManager.h"
#include "core/events/EventBus.h"
#include "python/PythonSetupManager.h"
#include "services/llm/LlmService.h"
#include "storage/secure/SecureStorage.h"
#include "trading/AccountManager.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/Requirements.h"

#include <QVariantMap>

namespace fincept::ui {

namespace {

class LiveRequirementEnvironment final : public RequirementEnvironment {
  public:
    bool signed_in() const override { return auth::AuthManager::instance().is_authenticated(); }

    bool sign_in_pending() const override {
        // Start-up validation of a saved session (is_loading_ starts true) or a
        // sign-in in flight: the answer is not final yet.
        const auto& auth = auth::AuthManager::instance();
        return !auth.is_authenticated() && auth.is_loading();
    }

    bool has_secret(const QString& env_key) const override {
        return secret_presence(env_key, nullptr) == SecretPresence::Present;
    }

    SecretPresence secret_presence(const QString& env_key, QString* reason) const override {
        // Existence only: the value is checked for emptiness and dropped here. A
        // read error is "unreadable", never "missing" (SecretRead.h).
        const SecretRead r = SecureStorage::instance().read(env_key);
        if (r.failed()) {
            if (reason)
                *reason = r.reason();
            return SecretPresence::Unreadable;
        }
        return r.found() && !r.value.trimmed().isEmpty() ? SecretPresence::Present : SecretPresence::Absent;
    }

    bool llm_configured() const override { return ai_chat::LlmService::instance().is_configured(); }

    bool python_ready() const override { return python::PythonSetupManager::instance().is_core_ready(); }

    bool broker_connected(const QString& broker_id) const override {
        const auto accounts = broker_id.isEmpty() ? trading::AccountManager::instance().active_accounts()
                                                  : trading::AccountManager::instance().list_accounts(broker_id);
        for (const auto& a : accounts) {
            if (a.is_active && a.state == trading::ConnectionState::Connected)
                return true;
        }
        return false;
    }

    bool broker_credentials_unreadable(const QString& broker_id, QString* reason) const override {
        auto& am = trading::AccountManager::instance();
        const auto accounts = broker_id.isEmpty() ? am.active_accounts() : am.list_accounts(broker_id);
        for (const auto& a : accounts) {
            if (!a.is_active || am.credential_read_error(a.account_id).isEmpty())
                continue; // only accounts whose last read failed are read again
            // The retry: a successful read puts the account back to its state before
            // the failure (AccountManager), and the check that follows sees it.
            QString err;
            am.load_credentials(a.account_id, &err);
            if (err.isEmpty())
                continue;
            if (reason)
                *reason = err;
            return true;
        }
        return false;
    }

    double credit_balance() const override {
        const auto& auth = auth::AuthManager::instance();
        if (!auth.is_authenticated())
            return -1;
        const auto& s = auth.session();
        return s.has_subscription ? s.subscription.credit_balance : s.user_info.credit_balance;
    }
};

} // namespace

const RequirementEnvironment& live_requirement_environment() {
    static const LiveRequirementEnvironment env;
    return env;
}

RequirementStatus check_requirements(const QList<Requirement>& requirements) {
    return check_requirements(requirements, live_requirement_environment());
}

void watch_requirement_changes(QObject* context, std::function<void()> on_change) {
    if (!context || !on_change)
        return;
    auto* auth = &auth::AuthManager::instance();
    // Queued: the re-check runs after AuthManager has finished its own state change.
    QObject::connect(
        auth, &auth::AuthManager::loading_changed, context, [on_change](bool) { on_change(); }, Qt::QueuedConnection);
    QObject::connect(
        auth, &auth::AuthManager::auth_state_changed, context, [on_change]() { on_change(); }, Qt::QueuedConnection);
}

void open_setup_link(const SetupLink& link, QWidget* context) {
    if (!link.valid())
        return;
    if (link.screen_id == QLatin1String("plans")) {
        billing::open_plans(context);
        return;
    }
    if (link.screen_id == QLatin1String("login")) {
        // Only while really signed out: signed in, there is nothing to do (the
        // page re-checks on the auth change); a check still running decides
        // in a moment. The frame shows its sign-in screen for this request.
        const auto& auth = auth::AuthManager::instance();
        if (auth.is_authenticated() || auth.is_loading())
            return;
    }
    QVariantMap nav{{QStringLiteral("screen_id"), link.screen_id}};
    if (!link.section.isEmpty())
        nav.insert(QStringLiteral("section"), link.section);
    if (!link.focus.isEmpty())
        nav.insert(QStringLiteral("focus"), link.focus);
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"), nav);
}

} // namespace fincept::ui
