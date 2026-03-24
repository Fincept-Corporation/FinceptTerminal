#include "services/workflow/ConfirmationService.h"

#include "core/logging/Logger.h"
#include "services/workflow/AuditLogger.h"

#include <QUuid>

namespace fincept::workflow {

ConfirmationService& ConfirmationService::instance() {
    static ConfirmationService s;
    return s;
}

ConfirmationService::ConfirmationService() : QObject(nullptr) {}

void ConfirmationService::request(const ConfirmationRequest& req, std::function<void(bool, const QString&)> callback) {
    // Auto-approve paper trading for non-critical actions
    if (req.paper_trading && auto_approve_paper_ && req.risk != RiskLevel::Critical) {
        LOG_INFO("Confirmation", QString("Auto-approved (paper): %1").arg(req.title));
        AuditLogger::instance().log(AuditAction::ConfirmationApproved, {}, {}, {}, "Auto-approved (paper trading)",
                                    req.details, true);
        callback(true, "Auto-approved (paper trading)");
        return;
    }

    // Store pending and emit signal for UI
    QString id = req.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : req.id;
    ConfirmationRequest mutable_req = req;
    mutable_req.id = id;

    pending_.insert(id, {mutable_req, std::move(callback)});
    emit confirmation_requested(mutable_req);

    LOG_INFO("Confirmation", QString("Requesting: %1 (risk=%2)")
                                 .arg(req.title)
                                 .arg(req.risk == RiskLevel::Critical ? "CRITICAL"
                                      : req.risk == RiskLevel::High   ? "HIGH"
                                      : req.risk == RiskLevel::Medium ? "MEDIUM"
                                                                      : "LOW"));
}

void ConfirmationService::resolve(const QString& id, bool approved, const QString& notes) {
    auto it = pending_.find(id);
    if (it == pending_.end()) {
        LOG_WARN("Confirmation", QString("No pending confirmation: %1").arg(id));
        return;
    }

    auto entry = it.value();
    pending_.erase(it);

    AuditLogger::instance().log(approved ? AuditAction::ConfirmationApproved : AuditAction::ConfirmationRejected,
                                entry.request.details.value("workflow_id").toString(), {}, {},
                                notes.isEmpty() ? (approved ? "Approved" : "Rejected") : notes, entry.request.details,
                                entry.request.paper_trading);

    entry.callback(approved, notes);
    emit confirmation_resolved(id, approved);

    LOG_INFO("Confirmation", QString("%1: %2").arg(approved ? "Approved" : "Rejected", entry.request.title));
}

} // namespace fincept::workflow
