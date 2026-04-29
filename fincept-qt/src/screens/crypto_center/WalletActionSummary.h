#pragma once

#include <QString>
#include <QVector>

namespace fincept::screens {

/// Plain-data description of a fund-moving wallet action, passed into
/// `WalletActionConfirmDialog`. Phase 2 covers swap + burn; later phases
/// (lock, prediction stake) reuse the same struct.
///
/// **Read carefully**: every row and warning here ends up in front of the
/// user before they sign anything, so each field is part of the security
/// contract — not just UI fluff. Don't omit a row "to keep the dialog small."
struct WalletActionSummary {
    /// Header line. Examples: "SWAP", "BURN $FNCPT", "LOCK $FNCPT".
    QString title;

    /// Short paragraph below the title, optional. One-liner is fine; longer
    /// strings wrap. Keep neutral and factual.
    QString lede;

    struct Row {
        QString label;       ///< small-caps label, e.g. "ROUTE"
        QString value;       ///< monospace value, e.g. "SOL → USDC → $FNCPT"
        bool monospace = true; ///< false if value should wrap as natural text
    };
    /// One key/value row per fact the user should verify before signing.
    /// Order matters — render top-to-bottom in the same sequence.
    QVector<Row> rows;

    /// Warnings rendered as a list of red-tinted bullets above the buttons.
    /// "Irreversible. Tokens cannot be recovered." goes here for burns.
    QVector<QString> warnings;

    /// Text on the confirm button. "SWAP", "BURN", "LOCK".
    QString primary_button_text = QStringLiteral("CONFIRM");

    /// Cosmetic — false renders the primary button in the danger color
    /// (red border / red hover); true uses the standard amber accent.
    /// Burns set `false`. Swaps set `true`.
    bool primary_is_safe = true;

    /// Anti-muscle-memory. The primary button is disabled for this many ms
    /// after the dialog appears, with a visible countdown. Per the Phase 2
    /// threat model: 1500 ms for swaps, 2500 ms for burns.
    int arm_delay_ms = 1500;
};

} // namespace fincept::screens
