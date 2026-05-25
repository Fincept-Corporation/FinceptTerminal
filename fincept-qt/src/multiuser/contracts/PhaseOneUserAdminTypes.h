#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::multiuser {

struct PhaseOneBootstrapStatus {
    bool bootstrap_open = false;

    static PhaseOneBootstrapStatus from_json(const QJsonObject& obj) {
        return {obj.value("bootstrap_open").toBool()};
    }
};

struct PhaseOneBootstrapRequest {
    QString username;
    QString password;

    QJsonObject to_json() const {
        return QJsonObject{{"username", username}, {"password", password}};
    }
};

struct PhaseOneCreateUserRequest {
    QString username;

    QJsonObject to_json() const {
        return QJsonObject{{"username", username}};
    }
};

struct PhaseOneSetInitialPasswordRequest {
    int user_id = 0;
    QString password;

    QJsonObject to_json() const {
        return QJsonObject{{"user_id", user_id}, {"password", password}};
    }
};

struct PhaseOneDisableUserRequest {
    int user_id = 0;

    QJsonObject to_json() const {
        return QJsonObject{{"user_id", user_id}};
    }
};

struct PhaseOneTransferAdminRequest {
    int target_user_id = 0;

    QJsonObject to_json() const {
        return QJsonObject{{"target_user_id", target_user_id}};
    }
};

struct PhaseOneUserSummary {
    int user_id = 0;
    QString username;
    QString role;
    QString status;

    static PhaseOneUserSummary from_json(const QJsonObject& obj) {
        PhaseOneUserSummary user;
        user.user_id = obj.value("user_id").toInt();
        user.username = obj.value("username").toString();
        user.role = obj.value("role").toString();
        user.status = obj.value("status").toString();
        return user;
    }
};

struct PhaseOneUserListResponse {
    QVector<PhaseOneUserSummary> users;

    static PhaseOneUserListResponse from_json(const QJsonObject& obj) {
        PhaseOneUserListResponse response;
        const QJsonArray array = obj.value("users").toArray();
        response.users.reserve(array.size());
        for (const QJsonValue& value : array)
            response.users.append(PhaseOneUserSummary::from_json(value.toObject()));
        return response;
    }
};

} // namespace fincept::multiuser
