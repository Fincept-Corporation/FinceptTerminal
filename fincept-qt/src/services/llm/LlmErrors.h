#pragma once
// LlmErrors.h — LLM failures the chat surfaces render as actionable notices.
//
// LlmResponse (LlmService.h) carries only a free-text `error`. Two Fincept
// failures deserve more than a red line of text because the user can fix them
// in one click — buy credits, sign in again, or switch to their own API key —
// so the producers (LlmFinceptAsync.cpp, ArenaLlmClient.cpp) start those errors
// with a FIXED lead sentence and the consumers (AI Chat tab, Quick Chat bubble)
// recognise the lead. The sentence is plain English on purpose: agents,
// workflow nodes and Alpha Arena show `error` verbatim, and it has to read well
// there too — no codes, no JSON.
//
// Before this, an exhausted balance surfaced as
//   Error: Fincept async task failed: 402: {'error': 'insufficient_credits', ...}
// (a Python dict repr from the backend worker) and was retried once for good
// measure, doubling the wait for an answer that could never come.

#include <QRegularExpression>
#include <QString>

namespace fincept::ai_chat {

enum class LlmErrorKind {
    Other,
    FinceptOutOfCredits, ///< HTTP 402 / "insufficient_credits" — the balance cannot pay for the call
    FinceptAuth,         ///< HTTP 401 / 403 — the Fincept key or session was rejected
};

inline QString fincept_credits_error_lead() {
    return QStringLiteral("You're out of Fincept credits.");
}

inline QString fincept_auth_error_lead() {
    return QStringLiteral("Fincept did not accept your sign-in.");
}

/// Recognise an error produced by make_fincept_error() (any consumer).
inline LlmErrorKind classify_llm_error(const QString& error) {
    if (error.startsWith(fincept_credits_error_lead()))
        return LlmErrorKind::FinceptOutOfCredits;
    if (error.startsWith(fincept_auth_error_lead()))
        return LlmErrorKind::FinceptAuth;
    return LlmErrorKind::Other;
}

/// Classify a raw Fincept backend failure: an HTTP status (0 when there is none,
/// e.g. a failed async task) plus whatever text came with it.
///
/// The async endpoint reports credit failures as TEXT: its worker catches the
/// HTTPException that the credit check raises and stores str(exc), which
/// Starlette renders as "402: {'error': 'insufficient_credits', ...}". The
/// synchronous endpoints answer with a real 402 / 401 status instead.
inline LlmErrorKind classify_fincept_failure(int http_status, const QString& text) {
    if (http_status == 402)
        return LlmErrorKind::FinceptOutOfCredits;
    if (http_status == 401 || http_status == 403)
        return LlmErrorKind::FinceptAuth;
    const QString t = text.trimmed();
    if (t.startsWith(QLatin1String("402:")) || t.contains(QLatin1String("insufficient_credits")) ||
        t.contains(QLatin1String("credits are over"), Qt::CaseInsensitive) ||
        t.contains(QLatin1String("enough credits"), Qt::CaseInsensitive))
        return LlmErrorKind::FinceptOutOfCredits;
    if (t.startsWith(QLatin1String("401:")) || t.contains(QLatin1String("Invalid API key"), Qt::CaseInsensitive) ||
        t.contains(QLatin1String("API key required"), Qt::CaseInsensitive))
        return LlmErrorKind::FinceptAuth;
    return LlmErrorKind::Other;
}

/// The user-facing error for a classified Fincept failure. `raw` is mined for
/// the "Required: X, Available: Y" figures the backend's credit message carries,
/// so the user sees what the call costs against what is left.
inline QString make_fincept_error(LlmErrorKind kind, const QString& raw = {}) {
    if (kind == LlmErrorKind::FinceptOutOfCredits) {
        QString figures;
        static const QRegularExpression rx_text(
            QStringLiteral("Required:\\s*([0-9]+(?:\\.[0-9]+)?)\\s*,\\s*Available:\\s*(-?[0-9]+(?:\\.[0-9]+)?)"),
            QRegularExpression::CaseInsensitiveOption);
        const auto m = rx_text.match(raw);
        if (m.hasMatch()) {
            figures = QStringLiteral(" This request needs %1 credits; your balance is %2.")
                          .arg(QString::number(m.captured(1).toDouble(), 'g', 6),
                               QString::number(m.captured(2).toDouble(), 'g', 6));
        }
        return fincept_credits_error_lead() + figures +
               QStringLiteral(" Top up under Plans & Pricing, or add your own provider API key in "
                              "Settings → LLM Configuration.");
    }
    if (kind == LlmErrorKind::FinceptAuth) {
        return fincept_auth_error_lead() +
               QStringLiteral(" Sign in again (or check your Fincept API key), or add your own provider API key in "
                              "Settings → LLM Configuration.");
    }
    return raw;
}

} // namespace fincept::ai_chat
