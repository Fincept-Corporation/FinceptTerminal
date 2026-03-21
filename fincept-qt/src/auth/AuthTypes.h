#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>

namespace fincept::auth {

// ── Request types ────────────────────────────────────────────────────────────

struct LoginRequest {
    QString email;
    QString password;
    bool    force_login = false;

    QJsonObject to_json() const {
        QJsonObject obj;
        obj["email"] = email;
        obj["password"] = password;
        if (force_login) obj["force_login"] = true;
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
        obj["phone"] = phone;           // required by API
        obj["country_code"] = country_code; // required by API
        if (!country.isEmpty()) obj["country"] = country;
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
    bool    success = false;
    QJsonObject data;
    QString error;
    int     status_code = 0;
};

struct UserProfile {
    int     id = 0;
    QString username;
    QString email;
    QString account_type = "free";
    double  credit_balance = 0;
    bool    is_verified = false;
    bool    is_admin = false;
    bool    mfa_enabled = false;
    QString phone;
    QString country;
    QString country_code;
    QString created_at;
    QString last_login_at;

    static UserProfile from_json(const QJsonObject& obj) {
        UserProfile p;
        p.id = obj["id"].toInt();
        p.username = obj["username"].toString();
        p.email = obj["email"].toString();
        p.account_type = obj["account_type"].toString("free");
        p.credit_balance = obj["credit_balance"].toDouble();
        p.is_verified = obj["is_verified"].toBool();
        p.is_admin = obj["is_admin"].toBool();
        p.mfa_enabled = obj["mfa_enabled"].toBool();
        p.phone = obj["phone"].toString();
        p.country = obj["country"].toString();
        p.country_code = obj["country_code"].toString();
        p.created_at = obj["created_at"].toString();
        p.last_login_at = obj["last_login_at"].toString();
        return p;
    }
};

struct UserSubscription {
    int     user_id = 0;
    QString account_type;
    double  credit_balance = 0;
    QString credits_expire_at;
    QString support_type;
    QString last_credit_purchase_at;
    QString created_at;

    static UserSubscription from_json(const QJsonObject& obj) {
        UserSubscription s;
        s.user_id = obj["user_id"].toInt();
        s.account_type = obj["account_type"].toString();
        s.credit_balance = obj["credit_balance"].toDouble();
        s.credits_expire_at = obj["credits_expire_at"].toString();
        s.support_type = obj["support_type"].toString();
        s.last_credit_purchase_at = obj["last_credit_purchase_at"].toString();
        s.created_at = obj["created_at"].toString();
        return s;
    }
};

// ── Subscription plan (from /cashfree/plans) ─────────────────────────────────

struct SubscriptionPlan {
    QString plan_id;
    QString name;
    QString description;
    double  price_usd = 0.0;
    QString currency;
    int     credits = 0;
    QString support_type;
    int     validity_days = 0;
    QStringList features;
    bool    is_free = false;
    int     display_order = 0;

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
        p.display_order = obj["display_order"].toInt();
        return p;
    }
};

// ── Session data ─────────────────────────────────────────────────────────────

struct SessionData {
    bool    authenticated = false;
    QString api_key;
    QString session_token;
    QString device_id;
    UserProfile user_info;
    UserSubscription subscription;
    bool    has_subscription = false;

    bool has_paid_plan() const {
        QString at = user_info.account_type.toLower();
        return at == "basic" || at == "standard" || at == "pro" || at == "enterprise";
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
        ui["credit_balance"] = user_info.credit_balance;
        ui["is_verified"] = user_info.is_verified;
        ui["mfa_enabled"] = user_info.mfa_enabled;
        obj["user_info"] = ui;

        QJsonObject sub;
        sub["account_type"] = subscription.account_type;
        sub["credit_balance"] = subscription.credit_balance;
        sub["support_type"] = subscription.support_type;
        obj["subscription"] = sub;

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
            s.user_info.credit_balance = ui["credit_balance"].toDouble();
            s.user_info.is_verified = ui["is_verified"].toBool();
            s.user_info.mfa_enabled = ui["mfa_enabled"].toBool();
        }
        if (obj.contains("subscription") && obj["subscription"].isObject()) {
            auto sub = obj["subscription"].toObject();
            s.subscription = UserSubscription::from_json(sub);
            s.has_subscription = !s.subscription.account_type.isEmpty();
        }
        return s;
    }
};

// ── Validation utilities ─────────────────────────────────────────────────────

struct ValidationResult {
    bool    valid = false;
    QString error;
};

inline ValidationResult validate_email(const QString& email) {
    if (email.isEmpty()) return {false, "Email is required"};
    // Basic email regex
    QRegularExpression re("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$");
    if (!re.match(email).hasMatch()) return {false, "Invalid email format"};
    return {true, {}};
}

struct PasswordStrength {
    bool valid = false;
    bool min_length = false;
    bool has_upper = false;
    bool has_lower = false;
    bool has_number = false;
    bool has_special = false;
    int  score = 0;
};

inline PasswordStrength validate_password(const QString& pw) {
    PasswordStrength r;
    r.min_length = pw.length() >= 8;
    r.has_upper = pw.contains(QRegularExpression("[A-Z]"));
    r.has_lower = pw.contains(QRegularExpression("[a-z]"));
    r.has_number = pw.contains(QRegularExpression("[0-9]"));
    r.has_special = pw.contains(QRegularExpression("[!@#$%^&*(),.?\":{}|<>]"));
    r.score = (int)r.min_length + (int)r.has_upper + (int)r.has_lower +
              (int)r.has_number + (int)r.has_special;
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
