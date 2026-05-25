#pragma once

#include <QJsonObject>
#include <QString>

namespace fincept::multiuser {

inline constexpr int kPhaseOneDefaultPort = 45450;

struct PhaseOneError {
    QString error_code;
    QString message;

    QJsonObject to_json() const {
        return QJsonObject{{"error_code", error_code}, {"message", message}};
    }

    static PhaseOneError from_json(const QJsonObject& obj) {
        return {obj.value("error_code").toString(), obj.value("message").toString()};
    }
};

struct PhaseOneLoginRequest {
    QString username;
    QString password;

    QJsonObject to_json() const {
        return QJsonObject{{"username", username}, {"password", password}};
    }
};

struct PhaseOneSessionInfo {
    int user_id = 0;
    QString username;
    QString role;
    QString session_id;
    QString expires_at;

    QJsonObject to_json() const {
        return QJsonObject{{"user_id", user_id},
                           {"username", username},
                           {"role", role},
                           {"session_id", session_id},
                           {"expires_at", expires_at}};
    }

    static PhaseOneSessionInfo from_json(const QJsonObject& obj) {
        PhaseOneSessionInfo info;
        info.user_id = obj.value("user_id").toInt();
        info.username = obj.value("username").toString();
        info.role = obj.value("role").toString();
        info.session_id = obj.value("session_id").toString();
        info.expires_at = obj.value("expires_at").toString();
        return info;
    }
};

} // namespace fincept::multiuser
