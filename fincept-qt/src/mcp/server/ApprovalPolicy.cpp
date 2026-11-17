// ApprovalPolicy.cpp — see ApprovalPolicy.h.

#include "mcp/server/ApprovalPolicy.h"

#include <QCoreApplication>
#include <QMutexLocker>
#include <QRegularExpression>

namespace fincept::mcp::approval {

namespace {

// The dialog's translation context, so these strings translate with it.
#define FA_TR(text) QCoreApplication::translate("fincept::mcp::ToolApprovalDialog", text)

QString normalise(const QString& raw) {
    const QString k = raw.trimmed().toLower();
    if (k == QLatin1String("trade"))
        return QStringLiteral("order");
    if (k == QLatin1String("destructive"))
        return QStringLiteral("delete");
    static const QStringList known{QStringLiteral("order"),  QStringLiteral("money"),   QStringLiteral("spend"),
                                   QStringLiteral("delete"), QStringLiteral("account"), QStringLiteral("code"),
                                   QStringLiteral("file"),   QStringLiteral("generic")};
    return known.contains(k) ? k : QStringLiteral("generic");
}

/// What a grant of this kind covers, in words ("deletes").
QString grant_noun(const QString& kind) {
    if (kind == QLatin1String("spend"))
        return FA_TR("credit / quota use");
    if (kind == QLatin1String("delete"))
        return FA_TR("deletes");
    if (kind == QLatin1String("file"))
        return FA_TR("file writes");
    return FA_TR("actions of this kind");
}

} // namespace

QString kind_of(const ToolApprovalRequest& request) {
    if (!request.kind.trimmed().isEmpty())
        return normalise(request.kind);
    return request.live_trading ? QStringLiteral("order") : QStringLiteral("generic");
}

bool timed_grant_allowed(const QString& kind) {
    const QString k = normalise(kind);
    return k == QLatin1String("spend") || k == QLatin1String("delete") || k == QLatin1String("file") ||
           k == QLatin1String("generic");
}

Presentation present(const ToolApprovalRequest& request) {
    Presentation p;
    p.kind = kind_of(request);
    const QString& k = p.kind;
    if (k == QLatin1String("order")) {
        p.heading = FA_TR("Place / change an order");
        switch (request.trading_mode) {
            case ApprovalTradingMode::Live:
                p.sentence = FA_TR("This places, changes or cancels a LIVE order: real money, sent to your broker or "
                                   "exchange.");
                p.status = FA_TR("LIVE · REAL MONEY");
                p.danger = true;
                break;
            case ApprovalTradingMode::Paper:
                p.sentence = FA_TR("This places, changes or cancels a PAPER order: simulated, no real money moves.");
                p.status = FA_TR("PAPER · SIMULATED");
                break;
            case ApprovalTradingMode::Unknown:
                p.sentence = FA_TR("This places, changes or cancels an order. The screen does not show whether it is "
                                   "LIVE or PAPER, so treat it as real money.");
                p.status = FA_TR("LIVE OR PAPER NOT SHOWN");
                p.danger = true;
                break;
        }
    } else if (k == QLatin1String("money")) {
        p.heading = FA_TR("Move money or change balances");
        p.sentence = FA_TR("This moves real money or changes balances, leverage or margin on your account.");
        p.status = FA_TR("REAL MONEY");
        p.danger = true;
    } else if (k == QLatin1String("spend")) {
        p.heading = FA_TR("Use credits or model quota");
        p.sentence = request.cost.trimmed().isEmpty()
                         ? FA_TR("This uses credits, model quota or a paid service. The cost is not shown here.")
                         : FA_TR("This uses credits, model quota or a paid service. Cost: %1.")
                               .arg(request.cost.trimmed());
        p.status = FA_TR("USES CREDITS / QUOTA");
    } else if (k == QLatin1String("delete")) {
        p.heading = FA_TR("Delete data");
        p.sentence = FA_TR("This deletes or resets your data. It may not be possible to undo it.");
        p.status = FA_TR("DELETES DATA");
    } else if (k == QLatin1String("account")) {
        p.heading = FA_TR("Change account or security settings");
        p.sentence = FA_TR("This changes account or security settings: keys, sign-in, permissions, trading mode or "
                           "approval settings.");
        p.status = FA_TR("ACCOUNT / SECURITY");
    } else if (k == QLatin1String("code")) {
        p.heading = FA_TR("Run code on this computer");
        p.sentence = FA_TR("This runs code on this computer, with your user's permissions.");
        p.status = FA_TR("RUNS CODE");
    } else if (k == QLatin1String("file")) {
        p.heading = FA_TR("Write a file");
        p.sentence = FA_TR("This writes a file outside the terminal's own data.");
        p.status = FA_TR("WRITES A FILE");
    } else {
        p.heading = FA_TR("Agent action needs your approval");
        p.sentence = FA_TR("This action changes something in the terminal and needs your approval.");
        p.status = FA_TR("CONFIRMATION REQUIRED");
    }

    p.client = request.client.trimmed().isEmpty() ? FA_TR("unknown client") : request.client.trimmed();
    if (!request.client_id.isEmpty()) {
        p.client += QStringLiteral(" — ") + request.client_id;
        if (!request.run_id.isEmpty())
            p.client += FA_TR(" · run %1").arg(request.run_id);
    }

    p.grant_allowed = timed_grant_allowed(k);
    if (p.grant_allowed) {
        p.grant_label = FA_TR("Allow %1 from this client on this screen for %2 minutes")
                            .arg(grant_noun(k))
                            .arg(kGrantMinutes);
        p.grant_note = FA_TR("Session only. For %1 minutes, further %2 from this client and run on this screen run "
                             "without asking. Every other kind of action still asks.")
                           .arg(kGrantMinutes)
                           .arg(grant_noun(k));
    } else if (k == QLatin1String("order") || k == QLatin1String("money")) {
        p.grant_label = FA_TR("Asked every time: orders and money moves are approved one at a time");
        p.grant_note = FA_TR("Orders and money moves are never approved for a period of time.");
    } else {
        p.grant_label = FA_TR("Asked every time: account, security and code actions are approved one at a time");
        p.grant_note = FA_TR("Account, security and code actions are never approved for a period of time.");
    }
    return p;
}

ApprovalTradingMode trading_mode_from_text(const QString& text) {
    // Screens write the mode in capitals ("LIVE BUY LIMIT …", a "PAPER" label);
    // dialog titles in words ("Confirm Live Order").
    static const QRegularExpression live_caps(QStringLiteral(R"(\bLIVE\b)"));
    static const QRegularExpression paper_caps(QStringLiteral(R"(\bPAPER\b)"));
    static const QRegularExpression live_words(QStringLiteral(R"(\blive\s+(order|orders|trading|trade|account|mode|buy|sell)\b)"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression paper_words(
        QStringLiteral(R"(\bpaper\s+(order|orders|trading|trade|account|mode|portfolio|buy|sell)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    const bool live = live_caps.match(text).hasMatch() || live_words.match(text).hasMatch();
    const bool paper = paper_caps.match(text).hasMatch() || paper_words.match(text).hasMatch();
    if (live == paper)
        return ApprovalTradingMode::Unknown; // neither, or the screen shows both
    return live ? ApprovalTradingMode::Live : ApprovalTradingMode::Paper;
}

QString trading_mode_word(ApprovalTradingMode mode) {
    switch (mode) {
        case ApprovalTradingMode::Live:
            return QStringLiteral("live");
        case ApprovalTradingMode::Paper:
            return QStringLiteral("paper");
        case ApprovalTradingMode::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

QString grant_key(const ToolApprovalRequest& request) {
    const QString kind = kind_of(request);
    if (!timed_grant_allowed(kind))
        return {};
    const QString who = request.client_id.isEmpty() ? QStringLiteral("label:") + request.client
                                                    : QStringLiteral("id:") + request.client_id;
    const QString scope = request.screen.trimmed().isEmpty() ? QStringLiteral("tool:") + request.tool
                                                             : QStringLiteral("screen:") + request.screen.trimmed();
    const QChar sep(0x1f);
    return who + sep + request.run_id + sep + kind + sep + scope;
}

void GrantBook::add(const ToolApprovalRequest& request, qint64 now_ms, int minutes) {
    const QString key = grant_key(request);
    if (key.isEmpty() || minutes <= 0)
        return;
    QMutexLocker lock(&mutex_);
    grants_.insert(key, Grant{now_ms + static_cast<qint64>(minutes) * 60 * 1000, request.run_id});
}

bool GrantBook::covers(const ToolApprovalRequest& request, qint64 now_ms) {
    const QString key = grant_key(request);
    if (key.isEmpty())
        return false; // order / money / account / code: never by a grant
    QMutexLocker lock(&mutex_);
    const auto it = grants_.find(key);
    if (it == grants_.end())
        return false;
    if (it->expires_ms <= now_ms) {
        grants_.erase(it);
        return false;
    }
    return true;
}

void GrantBook::clear() {
    QMutexLocker lock(&mutex_);
    grants_.clear();
}

void GrantBook::clear_run(const QString& run_id) {
    if (run_id.isEmpty())
        return;
    QMutexLocker lock(&mutex_);
    for (auto it = grants_.begin(); it != grants_.end();) {
        if (it->run_id == run_id)
            it = grants_.erase(it);
        else
            ++it;
    }
}

int GrantBook::size() const {
    QMutexLocker lock(&mutex_);
    return static_cast<int>(grants_.size());
}

GrantBook& session_grants() {
    static GrantBook book;
    return book;
}

#undef FA_TR

} // namespace fincept::mcp::approval
