#pragma once
#include "auth/AuthTypes.h"
#include "storage/secure/SecretRead.h"

#include <QObject>
#include <QPointer>

#include <functional>
#include <vector>

namespace fincept::auth {

class GoogleDesktopLogin;

/// Central auth state machine.
/// Manages login/signup/MFA flows, session persistence, and validation.
class AuthManager : public QObject {
    Q_OBJECT
  public:
    static AuthManager& instance();

    const SessionData& session() const { return session_; }
    bool is_authenticated() const { return session_.authenticated; }
    bool is_loading() const { return is_loading_; }

    /// Resolve the Fincept api_key for LLM/service callers WITHOUT touching the
    /// plaintext SQLite settings table. Prefers the live in-memory session;
    /// falls back to the encrypted SecureStorage copy (which load_session also
    /// restores into the session at startup). Returns empty if no key is known.
    /// This is the single supported resolver — callers must NOT read the legacy
    /// plaintext "fincept_api_key" settings row.
    QString fincept_api_key() const;

    // Auth flows
    void login(const QString& email, const QString& password, bool force_login = false);

    /// Desktop Google sign-in via the website loopback handoff. Opens the
    /// browser, redeems the returned one-time code, then drives the same
    /// post-login flow as login() (emits login_succeeded / login_failed).
    void login_with_google();

    /// Stop waiting for the browser in a login_with_google() that has not been
    /// called back yet (the website never calls the loopback when the user
    /// closes the tab, cancels at Google, or the sign-in fails there). Emits
    /// login_failed("Google sign-in cancelled."). Returns false — and does
    /// nothing — when no such wait is running (none started, or the code has
    /// already arrived and is being redeemed).
    bool cancel_google_login();

    void signup(const QString& username, const QString& email, const QString& password, const QString& phone,
                const QString& country = {}, const QString& country_code = {});
    /// POST /user/resend-otp: a new verification code for an account that is
    /// not verified yet. Answered by otp_resent(server message) / otp_resend_failed.
    void resend_signup_otp(const QString& email);
    void verify_otp(const QString& email, const QString& otp);
    void verify_mfa(const QString& email, const QString& otp);
    void forgot_password(const QString& email);
    void reset_password(const QString& email, const QString& otp, const QString& new_password);
    void logout();

    // Session management
    void initialize();

    /// The saved API key / session token could not be read from SecureStorage
    /// at start-up (failed(): kind + reason()). While set, the saved session is
    /// neither used nor deleted — the terminal shows "Could not read secure
    /// storage: <reason>. Retry" instead of the login screen, and nothing is
    /// written over the credentials it could not read. retry_saved_session()
    /// reads them again (and resumes the normal start-up when it can).
    const SecretRead& saved_session_read_failure() const { return saved_session_failure_; }
    bool saved_session_unreadable() const { return saved_session_failure_.failed(); }
    void retry_saved_session();

    /// How eager a profile + subscription refresh is. Both records change only
    /// when the user acts (profile edit, purchase, key rotation) or out of band
    /// (a purchase on the website, a grant, a plan expiring). The credit balance
    /// in between is kept current from billed responses (report_credit_balance),
    /// so nothing needs to poll for it.
    enum class Refresh {
        Background, ///< timers / app focus: only when older than ~30 min (jittered) and the user is active
        OnView,     ///< a screen showing plan / credits opened: only when older than kOnViewMaxAgeMs
        Now,        ///< an action changed them (profile edit, key rotation, checkout, REFRESH button)
    };

    /// Refresh profile + subscription (GET /user/profile, GET /cashfree/subscription).
    /// Answered by auth_state_changed() + subscription_fetched(). Every caller
    /// shares one in-flight request; a caller that needs nothing new is served
    /// the cached session(). A Refresh::Now that arrives while an older request
    /// is in flight gets ONE follow-up request, shared by every such caller.
    /// No-op when signed out.
    void refresh_user_data(Refresh mode = Refresh::Background);

    /// Wall-clock ms of the last successful profile fetch (0 = none this session).
    /// A profile fetch carries the session token, so it also proves the session.
    qint64 user_data_fetched_ms() const { return user_data_fetched_ms_; }

    /// The account balance after a billed call, as a Fincept response reported
    /// it (HttpClient forwards X-Credits-Balance; clients with their own network
    /// stack may call this). Updates session() and emits credit_balance_changed()
    /// when it moved. Any thread.
    void report_credit_balance(double balance);

    /// Background refresh cadence: base + random [0, jitter). For tests.
    void set_background_refresh_interval_ms(qint64 base_ms, qint64 jitter_ms);

    /// Re-validate the API key alone (GET /user/profile without X-Session-Token).
    /// `cb(true)` = the session is kept (key valid, or the check was inconclusive:
    /// offline, 5xx, IP block); `cb(false)` = the key was refused (401) — AuthManager
    /// then emits session_expired() and signs out by itself, callers need not.
    /// Concurrent calls share one request; every callback gets the result.
    void attempt_session_recovery(std::function<void(bool)> cb);

    /// Report a 401 from the Fincept API received by a client that does NOT go
    /// through HttpClient (HttpClient reports its own) — e.g. ChatModeService:
    ///   handle_unauthorized(QString::fromUtf8(reply->request().rawHeader("X-Session-Token")),
    ///                       reply->rawHeader("X-Session-Expired").trimmed().toLower() == "true");
    /// `sent_session_token` is the X-Session-Token the request carried (empty if
    /// none); `session_expired` is true when the 401 is the server's
    /// "session_invalidated" (signed in elsewhere / password reset).
    /// Session expired + still our token: the token is forgotten at once (every
    /// reader of session().session_token stops sending it) and the key is
    /// re-validated. Any other 401: the key is re-validated, at most once a
    /// minute. Safe to call for every 401, from any thread, any number of times:
    /// reports for a token already forgotten are ignored and checks never overlap.
    void handle_unauthorized(const QString& sent_session_token, bool session_expired);

    /// POST /user/regenerate-api-key and adopt the returned key everywhere the old
    /// one lived (session, HttpClient, SecureStorage), then refresh the profile.
    /// The server invalidates the old key immediately, so the new key is adopted
    /// even if the caller is gone; `done` runs afterwards (guard your own lifetime).
    void regenerate_api_key(std::function<void(bool ok, const QString& error)> done = {});

    /// True if user needs to set up a PIN (authenticated but no PIN configured).
    bool needs_pin_setup() const;

  signals:
    void auth_state_changed();
    void login_succeeded();
    void login_failed(const QString& error);
    /// login() was refused because the account's email is not verified yet
    /// (403 from /user/login, password correct). Emitted INSTEAD of
    /// login_failed; `email` is the address signed in with, `message` the
    /// server's wording.
    void login_unverified(const QString& email, const QString& message);
    void login_mfa_required();
    void login_active_session(const QString& message);
    void signup_succeeded();
    void signup_failed(const QString& error);
    void otp_verified();
    void otp_failed(const QString& error);
    /// resend_signup_otp() answered. `message` is the server's (neutral) wording.
    void otp_resent(const QString& message);
    void otp_resend_failed(const QString& error);
    void mfa_verified();
    void mfa_failed(const QString& error);
    void forgot_password_sent();
    void forgot_password_failed(const QString& error);
    void password_reset_succeeded();
    void password_reset_failed(const QString& error);
    void logged_out();
    void session_expired();
    void loading_changed(bool loading);
    void subscription_fetched();

    /// session()'s credit balance changed — after a refresh, or from a billed
    /// response. Balance-only updates do NOT emit auth_state_changed, so this is
    /// cheap to handle (header chip, account screens, services waiting on credits).
    void credit_balance_changed(double balance);

    /// Emitted after successful login when PIN setup is needed.
    void pin_setup_required();

    /// Emitted after login + PIN verify (or setup) — terminal is fully unlocked.
    void terminal_unlocked();

  private:
    AuthManager();

    void set_loading(bool v);
    void save_session();
    void load_session();
    void clear_session();
    // Move the API key / session token out of the plaintext places pre-CR-08
    // builds wrote them to (the "fincept_session" blob, the "fincept_api_key"
    // row) into SecureStorage: write -> read back -> compare -> only then delete
    // the plaintext copy (storage/secure/SecretMigration.h). A copy whose move
    // failed stays where it is (retried at the next start, one WARN per session)
    // and is used in memory when the store holds nothing, so a failed move never
    // signs the user out. Runs at every start; idempotent.
    void migrate_legacy_plaintext_credentials();
    /// Delete the legacy plaintext copies that may go now: a copy equal to the
    /// value SecureStorage was just verified to hold, and — once the server has
    /// confirmed the key in use — a copy that differs from it (superseded).
    void retire_legacy_copies(bool key_stored_verified, bool token_stored_verified);
    /// Write the fincept_session blob: the non-secret fields, plus the legacy
    /// plaintext secrets whose move has not been verified yet (never dropped).
    bool write_session_blob();
    /// Start-up only: the stored key was refused (401) while an older saved key
    /// that differs from it exists — try that one before signing out. Once.
    bool try_conflicting_legacy_key(const QString& refused_key);
    void validate_saved_session();
    /// `on_rejected` (optional) is called with a user-facing message when the
    /// server answers 401/403 — the login/OTP/MFA flow that triggered the fetch
    /// uses it to emit its own *_failed signal. Startup validation and refreshes
    /// pass none and keep relying on auth_state_changed.
    void fetch_user_profile(std::function<void()> on_done = {},
                            std::function<void(const QString&)> on_rejected = {});
    void fetch_user_subscription(std::function<void()> on_done = {});
    void complete_auth_flow(std::function<void()> on_done);
    /// Inject a redeemed desktop-handoff session (api_key + session_token) and
    /// run the shared post-login flow. Shared by login_with_google().
    void complete_desktop_login(const QString& api_key, const QString& session_token);
    void auto_configure_fincept_llm();
    QString generate_device_id() const;
    QJsonObject unwrap_data(const QJsonObject& raw) const;

    /// logout() without (notify_server=false) the POST /user/logout — used when
    /// the key is already refused, where that call would only add another 401.
    void logout_impl(bool notify_server);
    /// The server rejected this session token: forget it everywhere (session,
    /// HttpClient, SecureStorage). No-op unless it is still the current token.
    void forget_session_token(const QString& dead_token);
    /// Startup: check the saved session token (one GET /user/session-pulse)
    /// before putting it on HttpClient, so the first screens don't fire a burst
    /// of 401s with a token that was replaced while the app was closed.
    void verify_saved_session_token();
    /// Run attempt_session_recovery() on the next event-loop turn, so the reply
    /// handler that is processing the 401 (e.g. fetch_user_profile) acts first
    /// and a key that is already being dealt with is not checked again.
    void schedule_key_check();
    void adopt_api_key(const QString& key);

    /// refresh_user_data() plumbing: one request at a time, generation-tagged so
    /// an answer for a superseded request (sign-out, key rotation) is ignored.
    void start_user_data_refresh();
    void finish_user_data_refresh(quint64 gen);
    void reset_user_data_refresh();
    /// Emit credit_balance_changed() if session()'s balance differs from the
    /// last one announced.
    void announce_credit_balance();

    /// Conditional GET (weak ETag) for the two polled account records: the ETag
    /// of the last full answer and the API key it was fetched with. Sent back as
    /// If-None-Match only for that same key, so a 304 always refers to the copy
    /// held in session_. Memory only — a fresh start fetches in full. Empty (an
    /// older server sends none): requests go out unconditional, as before.
    struct ConditionalCopy {
        QString etag;
        QString key;
        QString if_none_match_for(const QString& current_key) const {
            return !etag.isEmpty() && key == current_key ? etag : QString();
        }
    };
    ConditionalCopy profile_etag_;      ///< GET /user/profile
    ConditionalCopy subscription_etag_; ///< GET /cashfree/subscription
    /// Sign-out / key change: no stored ETag may be sent for another key.
    void forget_etags();
    /// The server's X-Poll-Interval for /user/profile (bounded to an hour by
    /// HttpClient::poll_interval_ms; 0 = none): Refresh::Background never runs
    /// more often than that, whatever its own cadence.
    qint64 profile_poll_hint_ms_ = 0;

    SessionData session_;
    bool is_loading_ = true;
    bool is_logging_out_ = false;

    /// The browser round trip of the running login_with_google(), if any (the
    /// flow deletes itself once it has emitted code_received / failed).
    QPointer<GoogleDesktopLogin> google_flow_;

    bool recovery_in_flight_ = false;
    bool recovery_scheduled_ = false;
    std::vector<std::function<void(bool)>> recovery_waiters_;
    qint64 last_key_check_ms_ = 0;
    bool session_token_unverified_ = false; ///< startup: saved token not yet checked
    SecretRead saved_session_failure_;       ///< see saved_session_read_failure()

    /// Plaintext copies left by pre-CR-08 builds that are still on disk because
    /// their move into SecureStorage has not been verified (or they differ from
    /// the stored value). Kept — save_session() writes the blob ones back — until
    /// retire_legacy_copies() may delete them.
    struct LegacyCopies {
        QString blob_api_key;        ///< "api_key" inside the fincept_session blob
        QString blob_session_token;  ///< "session_token" inside the fincept_session blob
        QString row_api_key;         ///< the fincept_api_key settings row
        QString conflicting_api_key; ///< an old key that differs from the stored one (start-up fallback)
    };
    LegacyCopies legacy_;
    /// The API key the last successful profile answer was fetched with.
    QString confirmed_api_key_;
    /// validate_saved_session()'s profile fetch is in flight.
    bool validating_saved_session_ = false;
    bool key_rotation_in_flight_ = false;   ///< regenerate_api_key() awaiting the server
    /// A 401 that is not a session expiry re-checks the key at most this often
    /// (some endpoints use 401 for "Invalid password").
    static constexpr qint64 kKeyRecheckCooldownMs = 60 * 1000;

    // refresh_user_data() state.
    bool refresh_in_flight_ = false;
    bool refresh_again_ = false; ///< a Refresh::Now arrived while a request was in flight
    quint64 refresh_gen_ = 0;
    qint64 refresh_started_ms_ = 0;
    qint64 user_data_fetched_ms_ = 0;
    qint64 next_background_refresh_ms_ = 0; ///< Refresh::Background does nothing before this
    double announced_balance_ = -1.0;       ///< last value sent with credit_balance_changed()
    qint64 bg_refresh_base_ms_ = kBackgroundRefreshMs;
    qint64 bg_refresh_jitter_ms_ = kBackgroundRefreshJitterMs;
    /// Background cadence: 30–40 min. The balance is pushed by billed responses,
    /// so this only catches out-of-band changes (website purchase, grant, expiry).
    static constexpr qint64 kBackgroundRefreshMs = 30 * 60 * 1000;
    static constexpr qint64 kBackgroundRefreshJitterMs = 10 * 60 * 1000;
    /// Refresh::OnView reuses data younger than this.
    static constexpr qint64 kOnViewMaxAgeMs = 2 * 60 * 1000;
    /// A refresh whose callback never came (both requests time out at 20 s, or
    /// the reply was dropped for a key that changed) frees the slot after this.
    static constexpr qint64 kRefreshInFlightTimeoutMs = 60 * 1000;
};

} // namespace fincept::auth
