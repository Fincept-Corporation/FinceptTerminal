#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUrlQuery>
#include <QVector>

namespace fincept::multiuser {

struct PhaseOneAuditFilter {
    QString user_identity;
    QString action_type;

    QString to_query_string() const {
        QUrlQuery query;
        if (!user_identity.isEmpty())
            query.addQueryItem("user_identity", user_identity);
        if (!action_type.isEmpty())
            query.addQueryItem("action_type", action_type);
        return query.toString(QUrl::FullyEncoded);
    }
};

struct PhaseOneAuditEvent {
    QString timestamp;
    QString user_identity;
    QString action_type;
    QString target;
    QString result_status;

    static PhaseOneAuditEvent from_json(const QJsonObject& obj) {
        PhaseOneAuditEvent event;
        event.timestamp = obj.value("timestamp").toString();
        event.user_identity = obj.value("user_identity").toString();
        event.action_type = obj.value("action_type").toString();
        event.target = obj.value("target").toString();
        event.result_status = obj.value("result_status").toString();
        return event;
    }
};

struct PhaseOneAuditListResponse {
    QVector<PhaseOneAuditEvent> audit_events;

    static PhaseOneAuditListResponse from_json(const QJsonObject& obj) {
        PhaseOneAuditListResponse response;
        const QJsonArray array = obj.value("audit_events").toArray();
        response.audit_events.reserve(array.size());
        for (const QJsonValue& value : array)
            response.audit_events.append(PhaseOneAuditEvent::from_json(value.toObject()));
        return response;
    }
};

} // namespace fincept::multiuser
