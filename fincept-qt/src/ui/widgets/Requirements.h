#pragma once
// Requirements — what a view needs before its action can run (an API key, an
// LLM model, the Python environment, a broker account, sign-in, credits), a
// pure check of a list of them, and where the user fixes what is missing.
//
//   const QList<ui::Requirement> needs = {ui::Requirement::api_key("FRED_API_KEY", "FRED")};
//   const ui::RequirementStatus st = ui::check_requirements(needs);      // live terminal
//   if (!st.ok) { empty_state->apply(ui::EmptyState::needs_setup_spec(st)); ... ui::open_setup_link(st.link); }
//
// ui::AutoLoadPane::set_requirements() does exactly this on every show and
// shows the "needs setup" page with a button that opens the right Settings
// section. The check itself is pure over a RequirementEnvironment, so tests
// pass a fake one; live_requirement_environment() asks the terminal
// (SecureStorage — existence only, never the value — LlmService,
// PythonSetupManager, AccountManager, AuthManager).
//
// UI text points the user with one spelling everywhere: "Settings > Credentials".

#include <QList>
#include <QString>

#include <functional>

class QObject;
class QWidget;

namespace fincept::ui {

enum class RequirementKind { SignedIn, ApiKey, LlmConfigured, PythonReady, BrokerConnected, Credits };

struct Requirement {
    RequirementKind kind = RequirementKind::SignedIn;
    QString name;      ///< ApiKey: env-key name ("FRED_API_KEY"); BrokerConnected: broker id ("" = any broker)
    QString label;     ///< how text names it: "FRED", "Zerodha"
    double amount = 0; ///< Credits: what one run costs
    /// Optional host fix that is not a Settings deep link ("BUILD FROM YAHOO" for a
    /// dataset): when this requirement is the one missing, the "needs setup" page's
    /// PRIMARY button says `action_text` and runs `action`; the standard link moves
    /// to the secondary button (when the status has none). Never used while the
    /// check itself could not be made (RequirementStatus::retry keeps RETRY).
    QString action_text;
    std::function<void()> action;

    /// A copy with the host fix above (`text` empty or `fn` empty = none).
    Requirement with_action(const QString& text, std::function<void()> fn) const;

    static Requirement signed_in();
    static Requirement api_key(const QString& env_key, const QString& provider_label);
    static Requirement llm();
    static Requirement python();
    static Requirement broker(const QString& broker_id = QString(), const QString& broker_label = QString());
    static Requirement credits(double amount);
};

/// Where the user fixes a missing requirement.
struct SetupLink {
    /// "settings", "plans" (Plans & Pricing), "equity_trading" (broker accounts),
    /// "login" (the sign-in screen; asked for only while signed out), or empty
    QString screen_id;
    QString section;   ///< Settings section id: "credentials", "llm_config", "python_env", "ai_agents", "appearance", …
    QString focus;     ///< optional item inside the section, e.g. the env-key name in Credentials
    bool valid() const { return !screen_id.isEmpty(); }
};

struct RequirementStatus {
    bool ok = true;
    Requirement missing;  ///< the first unmet requirement (when !ok)
    QString title;        ///< "Needs a FRED API key"
    QString message;      ///< names exactly what is missing and where to add it
    QString action_text;  ///< "OPEN SETTINGS > CREDENTIALS" (empty when there is nothing to open)
    SetupLink link;
    /// The check itself could not be made (secure storage unreadable): the
    /// requirement may well be met. The action ("RETRY") re-checks instead of
    /// opening `link` — the page never asks for a key that may exist.
    bool retry = false;
    /// Optional second action on the needs-setup page ("" = none) and where it goes.
    QString secondary_text;
    SetupLink secondary_link;
    /// Since 2026-11-17 round 5. Not decided yet: a saved sign-in is still being
    /// confirmed (start-up) or a sign-in is in flight — the requirement may well
    /// be met in a moment. `title` says what is being checked ("Checking
    /// sign-in…"); there is no action and no link. Show a neutral waiting state,
    /// never the setup page, run nothing, and check again when it settles
    /// (watch_requirement_changes). ok is false while pending.
    bool pending = false;
};

/// Whether a credential is stored, as far as the store can tell.
enum class SecretPresence {
    Present,    ///< stored, non-empty
    Absent,     ///< the store was read and has none (or an empty value)
    Unreadable, ///< the store could not be read: unknown, never "absent"
};

/// What the checks may ask about the terminal. Fake it in tests.
class RequirementEnvironment {
  public:
    virtual ~RequirementEnvironment() = default;
    virtual bool signed_in() const = 0;
    /// A credential with this env-key name is stored (existence only — never read out the value).
    virtual bool has_secret(const QString& env_key) const = 0;
    /// Tri-state presence. `*reason` (may be null) completes "Could not read
    /// secure storage: …" when Unreadable. The default derives it from
    /// has_secret(), so an environment that cannot fail needs nothing more.
    virtual SecretPresence secret_presence(const QString& env_key, QString* reason) const {
        Q_UNUSED(reason);
        return has_secret(env_key) ? SecretPresence::Present : SecretPresence::Absent;
    }
    virtual bool llm_configured() const = 0;
    virtual bool python_ready() const = 0;
    /// An active account of `broker_id` (any broker when empty) is connected.
    virtual bool broker_connected(const QString& broker_id) const = 0;
    /// Asked only when broker_connected() is false: true when the stored
    /// credentials of such an active account could not be read (secure storage
    /// busy, key unavailable, damaged row) — it may well be connected once they
    /// read again, so the page must not say "connect a broker". `*reason` (may be
    /// null) gets a full sentence saying why, shown as is (the live one is
    /// AccountManager::credential_read_error(): "Could not read the stored
    /// credential: <reason>. …"). The live environment tries the read again first
    /// (AccountManager::load_credentials is the retry). Default: never.
    virtual bool broker_credentials_unreadable(const QString& broker_id, QString* reason) const {
        Q_UNUSED(broker_id);
        Q_UNUSED(reason);
        return false;
    }
    /// Current credit balance; negative = unknown (a Credits requirement then passes).
    virtual double credit_balance() const = 0;
    /// Asked only when signed_in() is false: true while that is not final yet —
    /// a saved sign-in is still being confirmed at start-up, or a sign-in is in
    /// flight (live: AuthManager::is_loading()). A SignedIn requirement is then
    /// pending (RequirementStatus::pending), never "sign in". Default: never.
    virtual bool sign_in_pending() const { return false; }
};

/// The live terminal. GUI thread.
const RequirementEnvironment& live_requirement_environment();

/// Calls `on_change` — queued, on `context`'s thread, for as long as `context`
/// lives — whenever something a pending or sign-in requirement depends on
/// changes: today the sign-in check finishing / starting and the auth state
/// changing (AuthManager::loading_changed, auth_state_changed). Several
/// notifications in a row may arrive; re-check idempotently. Live terminal, GUI
/// thread (RequirementsLive.cpp: not linked into the pure unit tests).
void watch_requirement_changes(QObject* context, std::function<void()> on_change);

/// The first unmet requirement in list order (ok when all are met). Pure given `env`.
/// A missing requirement that carries a host fix (Requirement::with_action) comes
/// back with it applied (apply_host_action).
RequirementStatus check_requirements(const QList<Requirement>& requirements, const RequirementEnvironment& env);
/// `status` with status.missing's host fix as the primary action: action_text =
/// missing.action_text, link cleared (the caller runs missing.action), and the
/// standard action moved to the secondary button when that is free. Unchanged when
/// ok, retry, no fix, or already applied. For custom checks that set missing.action.
RequirementStatus apply_host_action(RequirementStatus status);
/// True when `status`'s primary button is the missing requirement's host fix.
bool has_host_action(const RequirementStatus& status);
/// Same, against the live terminal.
RequirementStatus check_requirements(const QList<Requirement>& requirements);

/// Opens where `link` points: Settings on the section (nav.switch_screen with
/// {"screen_id": "settings", "section": ..., "focus": ...}), Plans & Pricing,
/// the sign-in screen ("login": nav.switch_screen {"screen_id": "login"}, only
/// while signed out and no sign-in check is running), or another screen.
/// `context` parents a dialog when one opens.
void open_setup_link(const SetupLink& link, QWidget* context = nullptr);

} // namespace fincept::ui
