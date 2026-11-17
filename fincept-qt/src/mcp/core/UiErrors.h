#pragma once
// UiErrors.h — structured, actionable failures for the core UI tools.
//
// Every failed ui_* call answers ONE text that starts with a short stable code,
// then one plain sentence, then (optionally) what to do instead:
//
//     stale_ref: e12 (button "Refresh") is gone — the panel was rebuilt. Hint: same control now: e57 …
//
// Harnesses branch on the code (the text before the first ':'); people and
// models read the rest. Codes never change meaning; new ones may be added.
// Header-only, QtCore only.

#include "mcp/McpTypes.h"

#include <QRegularExpression>
#include <QString>

namespace fincept::mcp::core::ui {

enum class Err {
    StaleRef,      ///< the ref's element was destroyed or rebuilt
    NotFound,      ///< nothing matches the ref / name / text / option
    Ambiguous,     ///< several elements match equally well — pass a ref
    Disabled,      ///< the control is disabled (or read-only)
    Obscured,      ///< the control is covered, hidden or behind a modal dialog
    Timeout,       ///< a wait or a page answer ran out of time
    NeedsApproval, ///< the user must approve (or did not answer in time)
    Denied,        ///< the user declined, or the action is refused by policy
    InvalidValue,  ///< the value is out of range / malformed / rejected by the field
    Busy,          ///< the terminal or the scope is still loading / locked
    Unsupported,   ///< the action does not apply to this element
    Cancelled,     ///< the caller gave up
    Internal,      ///< the terminal's own code failed
};

inline const char* err_code(Err e) {
    switch (e) {
        case Err::StaleRef:
            return "stale_ref";
        case Err::NotFound:
            return "not_found";
        case Err::Ambiguous:
            return "ambiguous";
        case Err::Disabled:
            return "disabled";
        case Err::Obscured:
            return "obscured";
        case Err::Timeout:
            return "timeout";
        case Err::NeedsApproval:
            return "needs_approval";
        case Err::Denied:
            return "denied";
        case Err::InvalidValue:
            return "invalid_value";
        case Err::Busy:
            return "busy";
        case Err::Unsupported:
            return "unsupported";
        case Err::Cancelled:
            return "cancelled";
        case Err::Internal:
            return "internal";
    }
    return "error";
}

/// "code: sentence[ Hint: hint]".
inline QString err_text(Err e, const QString& sentence, const QString& hint = {}) {
    QString t = QString::fromLatin1(err_code(e)) + QStringLiteral(": ") + sentence.trimmed();
    if (!hint.trimmed().isEmpty())
        t += QStringLiteral(" Hint: ") + hint.trimmed();
    return t;
}

inline ToolResult ui_fail(Err e, const QString& sentence, const QString& hint = {}) {
    return ToolResult::fail(err_text(e, sentence, hint));
}

/// The code a failure text starts with ("stale_ref"), or "" when it has none.
inline QString error_code_of(const QString& text) {
    static const QRegularExpression rx(QStringLiteral("^([a-z_]{4,20}): "));
    const auto m = rx.match(text);
    return m.hasMatch() ? m.captured(1) : QString();
}

/// A failure text that has no code yet gets the one its words imply — the
/// safety net for messages written before the codes existed. Texts that
/// already start with a code are returned unchanged.
inline QString with_error_code(const QString& text) {
    if (!error_code_of(text).isEmpty())
        return text;
    const QString t = text.trimmed();
    const QString l = t.toLower();
    auto has = [&l](const char* w) { return l.contains(QLatin1String(w)); };
    Err e = Err::Unsupported;
    if (l == QLatin1String("cancelled") || l.startsWith(QLatin1String("cancelled")))
        e = Err::Cancelled;
    else if (has("declined") || has("refused") || has("not available to agents") || has("cannot be operated") ||
             has("never typed") || has("never read") || has("never operate") || has("secret"))
        e = Err::Denied;
    else if (has("approv"))
        e = Err::NeedsApproval;
    else if (has("is gone") || has("no longer") || has("was never listed") || has("changed while") ||
             has("closed right after") || has("page closed"))
        e = Err::StaleRef;
    else if (has("matches several") || has("ambiguous") || has("several"))
        e = Err::Ambiguous;
    else if (has("timed out") || has("did not answer") || has("no answer"))
        e = Err::Timeout;
    else if (has("is locked") || has("still loading") || has("busy"))
        e = Err::Busy;
    else if (has("blocks") || has("not visible") || has("not showing") || has("covered") || has("hidden"))
        e = Err::Obscured;
    else if (has("disabled") || has("read-only"))
        e = Err::Disabled;
    else if (has("nothing named") || has("no option") || has("no tab") || has("no row") || has("no column") ||
             has("not open") || has("unknown") || has("no screen") || has("no command") || has("does not exist") ||
             has("nothing scroll") || has("nothing on the page") || has("no saved layout") || has("not found"))
        e = Err::NotFound;
    else if (has("takes a number") || has("not an iso") || has("is not a key") || has("must be") || has("needs `") ||
             has("at most") || has("out of range") || has("is not a ref") || has("pass `"))
        e = Err::InvalidValue;
    else if (has("failed inside the terminal") || has("threw"))
        e = Err::Internal;
    return QString::fromLatin1(err_code(e)) + QStringLiteral(": ") + t;
}

} // namespace fincept::mcp::core::ui
