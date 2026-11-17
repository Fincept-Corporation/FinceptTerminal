// src/ui/widgets/Requirements.cpp — the pure part of Requirements.h (QtCore
// only, so unit tests link it alone). The live environment and
// open_setup_link() are in RequirementsLive.cpp.

#include "ui/widgets/Requirements.h"

#include <QCoreApplication>
#include <QLocale>

#include <utility>

namespace fincept::ui {

namespace {

QString req_tr(const char* text) {
    return QCoreApplication::translate("Requirements", text);
}

Requirement req_make(RequirementKind kind, const QString& name = QString(), const QString& label = QString(),
                     double amount = 0) {
    Requirement r;
    r.kind = kind;
    r.name = name;
    r.label = label;
    r.amount = amount;
    return r;
}

} // namespace

Requirement Requirement::with_action(const QString& text, std::function<void()> fn) const {
    Requirement r = *this;
    const bool usable = !text.isEmpty() && static_cast<bool>(fn);
    r.action_text = usable ? text : QString();
    r.action = usable ? std::move(fn) : std::function<void()>();
    return r;
}

Requirement Requirement::signed_in() {
    return req_make(RequirementKind::SignedIn);
}

Requirement Requirement::api_key(const QString& env_key, const QString& provider_label) {
    return req_make(RequirementKind::ApiKey, env_key, provider_label);
}

Requirement Requirement::llm() {
    return req_make(RequirementKind::LlmConfigured);
}

Requirement Requirement::python() {
    return req_make(RequirementKind::PythonReady);
}

Requirement Requirement::broker(const QString& broker_id, const QString& broker_label) {
    return req_make(RequirementKind::BrokerConnected, broker_id, broker_label);
}

Requirement Requirement::credits(double amount) {
    return req_make(RequirementKind::Credits, {}, {}, amount);
}

bool has_host_action(const RequirementStatus& status) {
    return !status.ok && !status.retry && !status.pending && static_cast<bool>(status.missing.action) &&
           !status.missing.action_text.isEmpty() && status.action_text == status.missing.action_text &&
           !status.link.valid();
}

RequirementStatus apply_host_action(RequirementStatus status) {
    if (status.ok || status.retry || status.pending || !status.missing.action || status.missing.action_text.isEmpty() ||
        has_host_action(status))
        return status;
    // The standard fix (open Settings, Accounts, Plans) stays reachable as the second button.
    if (status.secondary_text.isEmpty() && !status.action_text.isEmpty() && status.link.valid()) {
        status.secondary_text = status.action_text;
        status.secondary_link = status.link;
    }
    status.action_text = status.missing.action_text;
    status.link = SetupLink{};
    return status;
}

RequirementStatus check_requirements(const QList<Requirement>& requirements, const RequirementEnvironment& env) {
    for (const Requirement& r : requirements) {
        RequirementStatus st;
        st.missing = r;
        switch (r.kind) {
            case RequirementKind::SignedIn:
                if (env.signed_in())
                    continue;
                if (env.sign_in_pending()) {
                    // A saved sign-in is still being confirmed (start-up) or a sign-in
                    // is in flight: not "signed out" yet — never the sign-in page.
                    st.title = req_tr("Checking sign-in…");
                    st.message = req_tr("Your saved sign-in is being confirmed — this view loads as soon as it is.");
                    st.pending = true;
                    break;
                }
                st.title = req_tr("Sign in to load this view");
                st.message = req_tr("This view needs your Fincept account. Sign in again, then come back.");
                st.action_text = req_tr("SIGN IN");
                st.link = SetupLink{QStringLiteral("login"), {}, {}};
                break;
            case RequirementKind::ApiKey: {
                if (r.name.isEmpty())
                    continue;
                QString why;
                const SecretPresence presence = env.secret_presence(r.name, &why);
                if (presence == SecretPresence::Present)
                    continue;
                const QString who = r.label.isEmpty() ? r.name : r.label;
                if (presence == SecretPresence::Unreadable) {
                    // The key may be stored: never ask the user to add it again.
                    st.title = req_tr("Could not read secure storage");
                    st.message = QCoreApplication::translate("Requirements",
                                                             "Could not read secure storage: %1. Your %2 key may "
                                                             "already be saved — nothing needs to be entered again.")
                                     .arg(why.isEmpty() ? req_tr("unknown error") : why, who);
                    st.action_text = req_tr("RETRY");
                    st.retry = true;
                    break;
                }
                st.title = QCoreApplication::translate("Requirements", "Needs a %1 API key").arg(who);
                st.message = QCoreApplication::translate("Requirements",
                                                         "Add %1 in Settings > Credentials. It is stored encrypted "
                                                         "on this computer; nothing else needs to change.")
                                 .arg(r.name);
                st.action_text = req_tr("OPEN SETTINGS > CREDENTIALS");
                st.link = SetupLink{QStringLiteral("settings"), QStringLiteral("credentials"), r.name};
                break;
            }
            case RequirementKind::LlmConfigured:
                if (env.llm_configured())
                    continue;
                st.title = req_tr("No AI model configured");
                st.message = req_tr("Choose a provider and a model in Settings > LLM Config — this view sends its "
                                    "request to that model.");
                st.action_text = req_tr("OPEN SETTINGS > LLM CONFIG");
                st.link = SetupLink{QStringLiteral("settings"), QStringLiteral("llm_config"), {}};
                break;
            case RequirementKind::PythonReady:
                if (env.python_ready())
                    continue;
                st.title = req_tr("Python is not set up yet");
                st.message = req_tr("The analytics environment is still being prepared. Settings > Python Env "
                                    "shows its progress; this view loads as soon as it is ready.");
                st.action_text = req_tr("OPEN SETTINGS > PYTHON ENV");
                st.link = SetupLink{QStringLiteral("settings"), QStringLiteral("python_env"), {}};
                break;
            case RequirementKind::BrokerConnected: {
                if (env.broker_connected(r.name))
                    continue;
                const QString who = r.label.isEmpty() ? (r.name.isEmpty() ? QString() : r.name) : r.label;
                QString why;
                if (env.broker_credentials_unreadable(r.name, &why)) {
                    // The account may be connected once its credentials read again:
                    // never ask the user to connect a broker they already have.
                    if (why.isEmpty())
                        why = req_tr("Could not read the stored credential.");
                    st.title = req_tr("Could not read secure storage");
                    st.message = who.isEmpty()
                                     ? QCoreApplication::translate("Requirements",
                                                                   "%1 Your broker credentials may already be saved — "
                                                                   "nothing needs to be entered again.")
                                           .arg(why)
                                     : QCoreApplication::translate("Requirements",
                                                                   "%1 Your %2 credentials may already be saved — "
                                                                   "nothing needs to be entered again.")
                                           .arg(why, who);
                    st.action_text = req_tr("RETRY");
                    st.retry = true;
                    break;
                }
                st.title = who.isEmpty()
                               ? req_tr("No broker account connected")
                               : QCoreApplication::translate("Requirements", "No %1 account connected").arg(who);
                st.message = req_tr("Connect a broker account (Equity Trading > Accounts) to load this view.");
                st.action_text = req_tr("OPEN ACCOUNTS");
                st.link = SetupLink{QStringLiteral("equity_trading"), QStringLiteral("accounts"), r.name};
                break;
            }
            case RequirementKind::Credits: {
                const double balance = env.credit_balance();
                if (r.amount <= 0 || balance < 0 || balance >= r.amount)
                    continue;
                const QLocale loc;
                st.title = req_tr("Not enough credits");
                st.message = QCoreApplication::translate("Requirements",
                                                         "One run costs %1 credits; your balance is %2.")
                                 .arg(loc.toString(r.amount, 'g', 6), loc.toString(balance, 'f', 0));
                st.action_text = req_tr("VIEW PLANS");
                st.link = SetupLink{QStringLiteral("plans"), {}, {}};
                break;
            }
        }
        st.ok = false;
        return apply_host_action(st);
    }
    return RequirementStatus{};
}

} // namespace fincept::ui
