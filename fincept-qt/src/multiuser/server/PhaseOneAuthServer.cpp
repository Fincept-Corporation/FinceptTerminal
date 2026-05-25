#include "multiuser/server/PhaseOneAuthServer.h"

#include "core/logging/Logger.h"
#include "multiuser/server/PhaseOnePasswordHasher.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QUuid>

namespace fincept::multiuser {

namespace {

constexpr auto kAuthServerTag = "Phase1AuthServer";

QString user_target(const std::optional<PhaseOneStoredUser>& user, const QString& fallback_username) {
    if (user.has_value())
        return QStringLiteral("user:%1").arg(user->user_id);
    if (!fallback_username.isEmpty())
        return QStringLiteral("user:%1").arg(fallback_username.trimmed().toLower());
    return QStringLiteral("user:unknown");
}

QDateTime parse_session_timestamp(const QString& value) {
    QDateTime parsed = QDateTime::fromString(value, Qt::ISODate);
    if (!parsed.isValid())
        parsed = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (parsed.isValid() && parsed.timeSpec() != Qt::UTC)
        parsed = parsed.toUTC();
    return parsed;
}

} // namespace

PhaseOneAuthServer::PhaseOneAuthServer()
    : PhaseOneAuthServer(&owned_user_repository_, &owned_session_repository_, &owned_audit_repository_) {}

PhaseOneAuthServer::PhaseOneAuthServer(PhaseOneUserRepository* user_repository,
                                       PhaseOneSessionRepository* session_repository,
                                       PhaseOneAuditRepository* audit_repository)
    : user_repository_(user_repository), session_repository_(session_repository), audit_repository_(audit_repository) {}

fincept::Result<PhaseOneSessionInfo> PhaseOneAuthServer::login(const QString& username, const QString& password) {
    const auto user_count = user_repository_->count_users();
    if (user_count.is_ok() && user_count.value() == 0) {
        write_audit_event(QStringLiteral("anonymous"), QStringLiteral("login"), QStringLiteral("system:auth"),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOneSessionInfo>::err("setup_required");
    }

    const auto user = user_repository_->find_by_username(username);
    const QString actor = username.trimmed().toLower();
    const QString target = user_target(user, username);

    if (!user.has_value()) {
        write_audit_event(actor, QStringLiteral("login"), target, QStringLiteral("failure"));
        return fincept::Result<PhaseOneSessionInfo>::err("invalid_credentials");
    }

    if (user->status == QStringLiteral("disabled")) {
        write_audit_event(actor, QStringLiteral("login"), target, QStringLiteral("failure"));
        return fincept::Result<PhaseOneSessionInfo>::err("user_disabled");
    }

    if (user->password_hash.isEmpty()) {
        write_audit_event(actor, QStringLiteral("login"), target, QStringLiteral("failure"));
        return fincept::Result<PhaseOneSessionInfo>::err("setup_required");
    }

    if (!PhaseOnePasswordHasher::verify_password(password, user->password_hash)) {
        write_audit_event(actor, QStringLiteral("login"), target, QStringLiteral("failure"));
        return fincept::Result<PhaseOneSessionInfo>::err("invalid_credentials");
    }

    const auto created = replace_session_for_user(*user);
    if (created.is_err()) {
        write_audit_event(actor, QStringLiteral("login"), target, QStringLiteral("failure"));
        return fincept::Result<PhaseOneSessionInfo>::err(created.error());
    }

    write_session_event(actor, QStringLiteral("login"), created.value().session_id, QStringLiteral("success"));
    return fincept::Result<PhaseOneSessionInfo>::ok(created.value());
}

fincept::Result<void> PhaseOneAuthServer::logout(const QString& session_id) {
    if (session_id.isEmpty()) {
        write_audit_event(QStringLiteral("anonymous"), QStringLiteral("logout"), QStringLiteral("session:missing"),
                          QStringLiteral("failure"));
        return fincept::Result<void>::err("session_invalid");
    }

    const auto session = session_repository_->find_session(session_id);
    if (!session.has_value()) {
        write_audit_event(QStringLiteral("anonymous"), QStringLiteral("logout"), QStringLiteral("session:%1").arg(session_id),
                          QStringLiteral("failure"));
        return fincept::Result<void>::err("session_invalid");
    }
    const auto result = session_repository_->invalidate_session(session_id);
    const QString actor = session.has_value() ? session->username : QStringLiteral("anonymous");
    const QString target = QStringLiteral("session:%1").arg(session_id);
    write_audit_event(actor, QStringLiteral("logout"), target,
                      result.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return result;
}

fincept::Result<PhaseOneSessionInfo> PhaseOneAuthServer::current_session(const QString& session_id) const {
    if (session_id.isEmpty())
        return fincept::Result<PhaseOneSessionInfo>::err("session_invalid");

    const auto session = session_repository_->find_session(session_id);
    if (!session.has_value())
        return fincept::Result<PhaseOneSessionInfo>::err("session_invalid");

    if (session->invalidated)
        return fincept::Result<PhaseOneSessionInfo>::err("session_revoked");

    const auto user = user_repository_->find_by_id(session->user_id);
    if (!user.has_value())
        return fincept::Result<PhaseOneSessionInfo>::err("session_invalid");
    if (user->status == QStringLiteral("disabled")) {
        invalidate_session_with_audit(*session, user->username, QStringLiteral("user_disabled"));
        return fincept::Result<PhaseOneSessionInfo>::err("user_disabled");
    }

    const QDateTime expires_at = parse_session_timestamp(session->expires_at);
    if (expires_at.isValid() && expires_at <= QDateTime::currentDateTimeUtc()) {
        invalidate_session_with_audit(*session, user->username, QStringLiteral("session_expired"));
        return fincept::Result<PhaseOneSessionInfo>::err("session_expired");
    }

    const auto refresh = session_repository_->refresh_activity(session_id);
    if (refresh.is_err())
        return fincept::Result<PhaseOneSessionInfo>::err(refresh.error());

    const auto refreshed = session_repository_->find_active_session(session_id);
    if (!refreshed.has_value())
        return fincept::Result<PhaseOneSessionInfo>::err("session_invalid");

    return fincept::Result<PhaseOneSessionInfo>::ok(refreshed->to_session_info());
}

fincept::Result<void> PhaseOneAuthServer::write_audit_event(const QString& actor, const QString& action_type,
                                                            const QString& target, const QString& result_status) const {
    const auto result = audit_repository_->append_event(actor.isEmpty() ? QStringLiteral("anonymous") : actor,
                                                        action_type, target, result_status);
    if (result.is_err()) {
        LOG_WARN(kAuthServerTag,
                 QStringLiteral("Failed to write auth audit event: %1").arg(QString::fromStdString(result.error())));
    }
    return result;
}

fincept::Result<void> PhaseOneAuthServer::write_session_event(const QString& actor, const QString& action_type,
                                                              const QString& session_id,
                                                              const QString& result_status) const {
    return write_audit_event(actor, action_type, QStringLiteral("session:%1").arg(session_id), result_status);
}

fincept::Result<PhaseOneSessionInfo> PhaseOneAuthServer::replace_session_for_user(const PhaseOneStoredUser& user) const {
    const auto existing_sessions = session_repository_->list_active_sessions_for_user(user.user_id);
    if (existing_sessions.is_err())
        return fincept::Result<PhaseOneSessionInfo>::err(existing_sessions.error());

    auto& db = fincept::Database::instance();
    const auto begin = db.begin_transaction();
    if (begin.is_err())
        return fincept::Result<PhaseOneSessionInfo>::err(begin.error());

    const auto invalidated = session_repository_->invalidate_sessions_for_user(user.user_id);
    if (invalidated.is_err()) {
        db.rollback();
        return fincept::Result<PhaseOneSessionInfo>::err(invalidated.error());
    }

    const QString session_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto created = session_repository_->create_session(user.user_id, session_id);
    if (created.is_err()) {
        db.rollback();
        return fincept::Result<PhaseOneSessionInfo>::err(created.error());
    }

    const auto commit = db.commit();
    if (commit.is_err()) {
        db.rollback();
        return fincept::Result<PhaseOneSessionInfo>::err(commit.error());
    }

    for (const PhaseOneStoredSession& session : existing_sessions.value()) {
        write_session_event(user.username, QStringLiteral("forced_invalidation"), session.session_id,
                            QStringLiteral("success"));
    }

    return fincept::Result<PhaseOneSessionInfo>::ok(created.value().to_session_info());
}

fincept::Result<void> PhaseOneAuthServer::invalidate_session_with_audit(const PhaseOneStoredSession& session,
                                                                         const QString& actor,
                                                                         const QString& reason) const {
    const auto result = session_repository_->invalidate_session(session.session_id);
    Q_UNUSED(reason)
    write_session_event(actor, QStringLiteral("forced_invalidation"), session.session_id,
                        result.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return result;
}

} // namespace fincept::multiuser
