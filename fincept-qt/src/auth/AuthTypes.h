#pragma once
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QTimeZone>

namespace fincept::auth {

/// An API timestamp shown in the user's own time zone: "yyyy-MM-dd" or
/// "yyyy-MM-dd HH:mm" (with_time). The API serialises naive UTC
/// ("2026-05-01T12:34:56", no offset), which QDateTime would read as LOCAL time,
/// so an offset-less value is taken as UTC. Empty input gives an em dash; an
/// unparseable one is shown as sent (date part, 'T' replaced by a space).
inline QString format_local_stamp(const QString& iso, bool with_time) {
    if (iso.isEmpty())
        return QString::fromUtf8("\xe2\x80\x94");
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) {
        QString v = iso.left(with_time ? 16 : 10);
        v.replace(QLatin1Char('T'), QLatin1Char(' '));
        return v;
    }
    const QString tail = iso.mid(10);
    const bool has_offset = tail.contains(QLatin1Char('Z')) || tail.contains(QLatin1Char('+')) ||
                            tail.contains(QLatin1Char('-'));
    if (!has_offset)
        dt = QDateTime(dt.date(), dt.time(), QTimeZone::utc());
    return dt.toLocalTime().toString(with_time ? QStringLiteral("yyyy-MM-dd HH:mm") : QStringLiteral("yyyy-MM-dd"));
}

/// Credit balances are fractional — one AI call can cost 0.35 credits — so
/// never truncate them to int (a 349.65 balance used to render as "0" because
/// QJsonValue::toInt() rejects non-integers). Up to two decimals with trailing
/// zeros trimmed and thousands grouped: 350 → "350", 1234.5 → "1,234.5",
/// 0.35 → "0.35".
inline QString format_credits(double credits) {
    QString s = QLocale(QLocale::English).toString(credits, 'f', 2);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))
        s = QStringLiteral("0");
    return s;
}

// ── Request types ────────────────────────────────────────────────────────────

struct LoginRequest {
    QString email;
    QString password;
    bool force_login = false;

    QJsonObject to_json() const {
        QJsonObject obj;
        obj["email"] = email;
        obj["password"] = password;
        if (force_login)
            obj["force_login"] = true;
        return obj;
    }
};

struct RegisterRequest {
    QString username;
    QString email;
    QString password;
    QString phone;
    QString country;
    QString country_code;

    QJsonObject to_json() const {
        QJsonObject obj;
        obj["username"] = username;
        obj["email"] = email;
        obj["password"] = password;
        obj["phone"] = phone;               // required by API
        obj["country_code"] = country_code; // required by API
        if (!country.isEmpty())
            obj["country"] = country;
        return obj;
    }
};

struct VerifyOtpRequest {
    QString email;
    QString otp;

    QJsonObject to_json() const {
        QJsonObject obj;
        obj["email"] = email;
        obj["otp"] = otp;
        return obj;
    }
};

struct ForgotPasswordRequest {
    QString email;

    QJsonObject to_json() const {
        QJsonObject obj;
        obj["email"] = email;
        return obj;
    }
};

struct ResetPasswordRequest {
    QString email;
    QString otp;
    QString new_password;

    QJsonObject to_json() const {
        QJsonObject obj;
        obj["email"] = email;
        obj["otp"] = otp;
        obj["new_password"] = new_password;
        return obj;
    }
};

// ── Response types ───────────────────────────────────────────────────────────

struct ApiResponse {
    bool success = false;
    QJsonObject data;
    QString error;
    /// 200 for every success except a conditional GET answered 304 (success with
    /// empty `data`: "unchanged — keep your copy").
    int status_code = 0;
    /// Response metadata (AuthApi / UserApi fill them; empty / 0 when the
    /// response did not carry them, and for a transport failure):
    QString etag{};              ///< ETag, for a later If-None-Match
    qint64 poll_interval_ms = 0; ///< X-Poll-Interval, bounded (HttpClient::poll_interval_ms)
};

struct RateLimitInfo {
    int limit = 0;
    int remaining = 0;
    qint64 reset_at = 0; // unix timestamp (seconds)
    int window_seconds = 0;
    int concurrent_limit = 0;

    static RateLimitInfo from_json(const QJsonObject& obj) {
        RateLimitInfo r;
        r.limit = obj["limit"].toInt();
        r.remaining = obj["remaining"].toInt();
        r.reset_at = static_cast<qint64>(obj["reset_at"].toDouble());
        r.window_seconds = obj["window_seconds"].toInt();
        r.concurrent_limit = obj["concurrent_limit"].toInt();
        return r;
    }
};

struct UserProfile {
    int id = 0;
    QString username;
    QString email;
    QString account_type = "free";
    /// Plan in force right now ("free" | "pro" | "enterprise"): a lapsed pass is
    /// "free" while account_type still says "pro". Empty from an older API.
    QString effective_plan;
    double credit_balance = 0;
    QString credits_expire_at;
    bool is_verified = false;
    bool is_admin = false;
    bool mfa_enabled = false;
    QString phone;
    QString country;
    QString country_code;
    QString created_at;
    QString last_login_at;
    RateLimitInfo rate_limit;

    static UserProfile from_json(const QJsonObject& obj) {
        UserProfile p;
        p.id = obj["id"].toInt();
        p.username = obj["username"].toString();
        p.email = obj["email"].toString();
        p.account_type = obj["account_type"].toString("free");
        p.effective_plan = obj["effective_plan"].toString();
        p.credit_balance = obj["credit_balance"].toDouble();
        p.credits_expire_at = obj["credits_expire_at"].toString();
        p.is_verified = obj["is_verified"].toBool();
        p.is_admin = obj["is_admin"].toBool();
        p.mfa_enabled = obj["mfa_enabled"].toBool();
        p.phone = obj["phone"].toString();
        p.country = obj["country"].toString();
        p.country_code = obj["country_code"].toString();
        p.created_at = obj["created_at"].toString();
        p.last_login_at = obj["last_login_at"].toString();
        if (obj.contains("rate_limit") && obj["rate_limit"].isObject())
            p.rate_limit = RateLimitInfo::from_json(obj["rate_limit"].toObject());
        return p;
    }
};

struct UserSubscription {
    int user_id = 0;
    QString account_type;
    QString effective_plan; ///< see UserProfile::effective_plan
    double credit_balance = 0;
    QString credits_expire_at;
    QString support_type;
    QString last_credit_purchase_at;
    QString created_at;

    static UserSubscription from_json(const QJsonObject& obj) {
        UserSubscription s;
        s.user_id = obj["user_id"].toInt();
        s.account_type = obj["account_type"].toString();
        s.effective_plan = obj["effective_plan"].toString();
        s.credit_balance = obj["credit_balance"].toDouble();
        s.credits_expire_at = obj["credits_expire_at"].toString();
        s.support_type = obj["support_type"].toString();
        s.last_credit_purchase_at = obj["last_credit_purchase_at"].toString();
        s.created_at = obj["created_at"].toString();
        return s;
    }
};

// ── Subscription plan (from /cashfree/plans) ─────────────────────────────────
//
// Since 4.5.1 the catalogue is two plans: {"plan_id":"free", "is_free":true} and
// {"plan_id":"pro", "price_usd":10, "validity_days":30, "is_recommended":true}.
// Legacy basic/standard ids are no longer sold but stay valid account types
// (see SessionData::has_paid_plan()).

struct SubscriptionPlan {
    QString plan_id;
    QString name;
    QString description;
    double price_usd = 0.0;
    QString currency;
    int credits = 0;
    QString support_type;
    int validity_days = 0;
    QStringList features;
    bool is_free = false;
    /// Highlighted plan on the pricing screen. Optional in the payload: older
    /// backends omit it, in which case the "pro" plan is the recommended one.
    bool is_recommended = false;
    int display_order = 0;

    static SubscriptionPlan from_json(const QJsonObject& obj) {
        SubscriptionPlan p;
        p.plan_id = obj["plan_id"].toString();
        p.name = obj["name"].toString();
        p.description = obj["description"].toString();
        p.price_usd = obj["price_usd"].toDouble();
        p.currency = obj["currency"].toString();
        p.credits = obj["credits"].toInt();
        p.support_type = obj["support_type"].toString();
        p.validity_days = obj["validity_days"].toInt();
        if (obj.contains("features") && obj["features"].isArray()) {
            for (const auto& f : obj["features"].toArray())
                p.features.append(f.toString());
        }
        p.is_free = obj["is_free"].toBool();
        p.is_recommended = obj.contains("is_recommended")
                               ? obj["is_recommended"].toBool()
                               : p.plan_id.compare(QLatin1String("pro"), Qt::CaseInsensitive) == 0;
        p.display_order = obj["display_order"].toInt();
        return p;
    }
};

// ── Session data ─────────────────────────────────────────────────────────────

struct SessionData {
    bool authenticated = false;
    QString api_key;
    QString session_token;
    QString device_id;
    UserProfile user_info;
    UserSubscription subscription;
    bool has_subscription = false;

    // Single authoritative source for the current plan.
    // Subscription API is the most up-to-date; user_info is the fallback
    // populated at login before the subscription endpoint is called.
    const QString& account_type() const {
        return subscription.account_type.isEmpty() ? user_info.account_type : subscription.account_type;
    }

    /// The plan the server says is in force right now ("free" | "pro" |
    /// "enterprise", lower case) — GET /user/profile and /cashfree/subscription
    /// send it as effective_plan. A pass that has run out is "free" here while
    /// account_type() still names the paid plan, until the server settles the
    /// account (next sign-in or billed call). Empty when the API did not send it.
    QString effective_plan() const {
        const QString& e = subscription.effective_plan.isEmpty() ? user_info.effective_plan : subscription.effective_plan;
        return e.trimmed().toLower();
    }

    /// A paid pass that has lapsed: account_type() still names a paid plan, but
    /// the server already treats the account as Free (limits, modules).
    bool pass_lapsed() const {
        const QString at = account_type().trimmed().toLower();
        return effective_plan() == QLatin1String("free") && !at.isEmpty() && at != QLatin1String("free");
    }

    /// Paid entitlements right now. The server's effective_plan decides when it
    /// is known (a lapsed pass is not paid). Without it (older API, or a cached
    /// session saved by an older build): "pro" is the plan sold today, and
    /// basic / standard / enterprise are legacy paid tiers that existing
    /// subscribers still carry until their credits expire, so they stay paid.
    bool has_paid_plan() const {
        const QString eff = effective_plan();
        if (!eff.isEmpty())
            return eff != QLatin1String("free");
        const QString at = account_type().toLower();
        return at == "basic" || at == "standard" || at == "pro" || at == "enterprise";
    }

    /// Plan label for chrome (toolbar badge, profile): "FREE", "PRO", or a
    /// legacy tier's own name ("BASIC", "STANDARD"); "PRO (EXPIRED)" once the
    /// pass has lapsed. Plan names are product names and are not translated.
    QString plan_label() const {
        const QString at = account_type().trimmed();
        if (at.isEmpty())
            return QStringLiteral("FREE");
        if (pass_lapsed())
            return QCoreApplication::translate("AuthTypes", "%1 (EXPIRED)").arg(at.toUpper());
        return at.toUpper();
    }

    QJsonObject to_json() const {
        QJsonObject obj;
        obj["authenticated"] = authenticated;
        obj["api_key"] = api_key;
        obj["session_token"] = session_token;
        obj["device_id"] = device_id;
        obj["has_subscription"] = has_subscription;

        QJsonObject ui;
        ui["username"] = user_info.username;
        ui["email"] = user_info.email;
        ui["account_type"] = user_info.account_type;
        ui["effective_plan"] = user_info.effective_plan;
        ui["credit_balance"] = user_info.credit_balance;
        ui["is_verified"] = user_info.is_verified;
        ui["mfa_enabled"] = user_info.mfa_enabled;
        obj["user_info"] = ui;

        QJsonObject sub;
        sub["account_type"] = subscription.account_type;
        sub["effective_plan"] = subscription.effective_plan;
        sub["credit_balance"] = subscription.credit_balance;
        sub["support_type"] = subscription.support_type;
        obj["subscription"] = sub;

        return obj;
    }

    // Session JSON with secrets stripped — this is what may be persisted to the
    // UNENCRYPTED settings table. The api_key and session_token live ONLY in
    // SecureStorage (AES-256-GCM, machine-bound key). Persisting them in clear
    // would defeat encryption-at-rest (CR-08).
    QJsonObject to_persisted_json() const {
        QJsonObject obj = to_json();
        obj.remove("api_key");
        obj.remove("session_token");
        return obj;
    }

    static SessionData from_json(const QJsonObject& obj) {
        SessionData s;
        s.authenticated = obj["authenticated"].toBool();
        s.api_key = obj["api_key"].toString();
        s.session_token = obj["session_token"].toString();
        s.device_id = obj["device_id"].toString();
        s.has_subscription = obj["has_subscription"].toBool();

        if (obj.contains("user_info")) {
            auto ui = obj["user_info"].toObject();
            s.user_info.username = ui["username"].toString();
            s.user_info.email = ui["email"].toString();
            s.user_info.account_type = ui["account_type"].toString("free");
            s.user_info.effective_plan = ui["effective_plan"].toString();
            s.user_info.credit_balance = ui["credit_balance"].toDouble();
            s.user_info.is_verified = ui["is_verified"].toBool();
            s.user_info.mfa_enabled = ui["mfa_enabled"].toBool();
        }
        if (obj.contains("subscription") && obj["subscription"].isObject()) {
            auto sub = obj["subscription"].toObject();
            s.subscription = UserSubscription::from_json(sub);
            s.has_subscription = !s.subscription.account_type.isEmpty();
            // Keep user_info in sync so legacy readers are consistent
            if (!s.subscription.account_type.isEmpty())
                s.user_info.account_type = s.subscription.account_type;
            if (s.subscription.credit_balance > 0)
                s.user_info.credit_balance = s.subscription.credit_balance;
        }
        return s;
    }
};

// ── Refused data topics ──────────────────────────────────────────────────────
//
// A Fincept data service whose hub topic was refused — 402 (credits) or
// 401/403 (signed out / not allowed) — must not re-ask on a timer: waiting
// fixes none of these. It backs the topic off for hours and keeps one of these
// to decide when something changed that might fix it:
//   * any refusal: a sign-in with a different key (or after being signed out);
//   * a 402: the balance going UP (a top-up or grant) to at least the call's
//     cost. Billed calls elsewhere only ever lower it, so they never trigger a
//     retry. The service re-reads the account on a 402 (the session's figure
//     may be stale-high — spent through a client that reports no balance), so
//     that correction lands first and a later top-up reads as a rise.
// The user's explicit refresh lifts the backoff in the hub itself.

struct RefusalLatch {
    enum class Kind { None, Credits, Access };
    Kind kind = Kind::None;
    QString key;               ///< API key in use when refused (empty = signed out)
    double last_balance = -1;  ///< balance last seen while refused

    bool active() const { return kind != Kind::None; }
    void refuse(Kind k, const QString& api_key, double balance) {
        kind = k;
        key = api_key;
        last_balance = balance;
    }
    void clear() {
        kind = Kind::None;
        last_balance = -1;
    }
    /// On auth_state_changed. A sign-out forgets the key, so signing back in —
    /// even to the same account, whose API key does not change — counts.
    bool release_on_auth(bool signed_in, const QString& api_key) {
        if (!signed_in) {
            key.clear();
            return false;
        }
        return active() && !api_key.isEmpty() && api_key != key;
    }
    /// On credit_balance_changed; `cost` = credits one call of this service needs.
    bool release_on_balance(double balance, double cost) {
        if (kind != Kind::Credits)
            return false;
        const bool rose = last_balance >= 0 && balance > last_balance + 0.005;
        last_balance = balance;
        return rose && balance + 0.005 >= cost;
    }
};

// ── Validation utilities ─────────────────────────────────────────────────────

struct ValidationResult {
    bool valid = false;
    QString error;
};

inline ValidationResult validate_email(const QString& email) {
    if (email.isEmpty())
        return {false, QCoreApplication::translate("AuthTypes", "Email is required")};
    // Basic email regex
    QRegularExpression re("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$");
    if (!re.match(email).hasMatch())
        return {false, QCoreApplication::translate("AuthTypes", "Invalid email format")};
    return {true, {}};
}

struct PasswordStrength {
    bool valid = false;
    bool min_length = false;
    bool has_upper = false;
    bool has_lower = false;
    bool has_number = false;
    bool has_special = false;
    int score = 0;
};

inline PasswordStrength validate_password(const QString& pw) {
    PasswordStrength r;
    r.min_length = pw.length() >= 8;
    r.has_upper = pw.contains(QRegularExpression("[A-Z]"));
    r.has_lower = pw.contains(QRegularExpression("[a-z]"));
    r.has_number = pw.contains(QRegularExpression("[0-9]"));
    r.has_special = pw.contains(QRegularExpression("[!@#$%^&*(),.?\":{}|<>]"));
    r.score = (int)r.min_length + (int)r.has_upper + (int)r.has_lower + (int)r.has_number + (int)r.has_special;
    r.valid = r.min_length && r.has_upper && r.has_lower && r.has_number;
    return r;
}

/// Sanitize user input — strip leading/trailing whitespace, remove control chars.
inline QString sanitize_input(const QString& input) {
    QString s = input.trimmed();
    s.remove(QRegularExpression("[\\x00-\\x1F\\x7F]"));
    return s;
}

} // namespace fincept::auth
