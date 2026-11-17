#pragma once
// FnoChainState — what the Chain tab is doing, as told to the sub-tabs that read
// its chain (Builder, OI, Multi-Straddle, Screener), and the designed pages those
// tabs show while they have no chain to work on. Header-only (no QObject).
//
// The Chain tab owns the one chain of the screen: a broker's live NSE / BSE chain
// (DataHub "option:chain:*"), Databento's US chain, or the free DELAYED US chain
// (Cboe, FreeOptionChainService). FnoScreen relays its status and — for the free
// chain, which is not a DataHub topic — the chain itself to the other sub-tabs.

#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

namespace fincept::screens::fno {

/// The Chain tab's state: a chain is on its way, on screen, or not available (no
/// source picked, a setup step missing, a failure — the reason text says which).
enum class FnoChainStatus { Loading, Ready, Unavailable };

// ── Free delayed US chain: underlying symbols ──────────────────────────────────

/// The plain US ticker the free delayed chain is fetched and shown under: Yahoo's
/// index caret dropped ("^SPX" -> "SPX") and Yahoo's own index tickers mapped to the
/// root their options list under ("^GSPC" -> "SPX", "^DJI" -> "DJX"); stock and ETF
/// tickers pass through upper-cased.
inline QString fno_us_chain_symbol(const QString& symbol) {
    QString s = symbol.trimmed().toUpper();
    if (!s.startsWith(QLatin1Char('^')))
        return s;
    s = s.mid(1);
    if (s == QLatin1String("GSPC"))
        return QStringLiteral("SPX");
    if (s == QLatin1String("DJI"))
        return QStringLiteral("DJX");
    return s;
}

/// True for a US index option root the free chain lists ("SPX", "NDX", "DJX" …) or a
/// Yahoo index ticker ("^SPX"): an index has no order ticket and links as an index.
inline bool fno_is_us_index_root(const QString& symbol) {
    const QString s = symbol.trimmed().toUpper();
    if (s.startsWith(QLatin1Char('^')))
        return true;
    static const QStringList kRoots = {QStringLiteral("SPX"), QStringLiteral("XSP"),  QStringLiteral("NDX"),
                                       QStringLiteral("RUT"), QStringLiteral("MRUT"), QStringLiteral("VIX"),
                                       QStringLiteral("DJX"), QStringLiteral("OEX"),  QStringLiteral("XEO")};
    return kRoots.contains(s);
}

/// The Yahoo ticker of a US index option root, for the screens that take Yahoo
/// symbols (research, news, symbol links): "SPX" -> "^SPX", "DJX" -> "^DJI".
inline QString fno_us_index_yahoo(const QString& root) {
    const QString s = fno_us_chain_symbol(root);
    if (s == QLatin1String("DJX"))
        return QStringLiteral("^DJI");
    return QLatin1Char('^') + s;
}

/// Text typed into the free chain's symbol box that can be tried as a ticker as-is
/// (after fno_us_chain_symbol): 1-12 ASCII letters / digits / . - /, a letter first.
inline bool fno_is_us_ticker_text(const QString& s) {
    if (s.isEmpty() || s.size() > 12 || !(s.at(0).unicode() < 128 && s.at(0).isLetter()))
        return false;
    for (const QChar c : s) {
        const bool ok = (c.unicode() < 128 && c.isLetterOrNumber()) || c == QLatin1Char('.') ||
                        c == QLatin1Char('-') || c == QLatin1Char('/');
        if (!ok)
            return false;
    }
    return true;
}

/// Caption of a sub-tab's skeleton while the Chain tab loads its chain.
inline QString fno_chain_loading_caption() {
    return QCoreApplication::translate("FnoChainState", "Loading the option chain…");
}

/// The page a chain-reading sub-tab shows while there is no chain: what the tab
/// shows once there is one, why there is none (the Chain tab's own sentence) and
/// OPEN CHAIN, with a faint picture of the result.
inline fincept::ui::EmptyState::Spec fno_no_chain_spec(const QString& title, const QString& what_it_shows,
                                                       const QString& reason,
                                                       fincept::ui::LoadingSkeleton::Shape shape) {
    fincept::ui::EmptyState::Spec s;
    s.glyph = QStringLiteral("◇");
    s.glyph_tone = fincept::ui::EmptyState::Tone::Accent;
    s.title = title;
    s.message = what_it_shows;
    s.note = reason.isEmpty()
                 ? QCoreApplication::translate("FnoChainState",
                                               "Needs an option chain — the Chain tab loads the free delayed US "
                                               "chain without a broker, or your broker's live chain.")
                 : reason;
    s.note_tone = fincept::ui::EmptyState::Tone::Info;
    s.primary = QCoreApplication::translate("FnoChainState", "OPEN CHAIN");
    s.primary_tooltip = QCoreApplication::translate("FnoChainState", "Switch to the Chain tab to pick a source");
    s.illustration = true;
    s.illustration_shape = shape;
    return s;
}

/// A view that needs a broker's intraday feed (Intraday OI, Multi-Straddle) while
/// the Chain tab shows a chain without one (the free delayed chain, Databento).
/// The wording comes from the terminal's broker requirement (Requirements.h), so a
/// broker whose stored credentials merely could not be read gets "Could not read
/// secure storage" + RETRY — never "connect a broker".
struct FnoFeedPage {
    fincept::ui::EmptyState::Spec spec;
    fincept::ui::RequirementStatus status; ///< ok = a broker is connected (switch the source instead)
};

inline FnoFeedPage fno_broker_feed_page(const QString& title, const QString& what_it_needs) {
    FnoFeedPage page;
    page.status = fincept::ui::check_requirements({fincept::ui::Requirement::broker()});
    auto& s = page.spec;
    if (!page.status.ok && page.status.retry) {
        s = fincept::ui::EmptyState::needs_setup_spec(page.status); // "Could not read secure storage" + RETRY
        return page;
    }
    s.glyph = QStringLiteral("⌁");
    s.glyph_tone = fincept::ui::EmptyState::Tone::Warning;
    s.title = title;
    s.illustration = true;
    s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Chart;
    if (!page.status.ok) {
        s.message = what_it_needs + QLatin1Char(' ') + page.status.message;
        s.primary = page.status.action_text; // OPEN ACCOUNTS
        s.secondary = QCoreApplication::translate("FnoChainState", "OPEN CHAIN");
    } else {
        s.message = what_it_needs + QLatin1Char(' ') +
                    QCoreApplication::translate("FnoChainState",
                                                "A broker is connected — switch the Chain tab's source to it.");
        s.primary = QCoreApplication::translate("FnoChainState", "OPEN CHAIN");
    }
    return page;
}

} // namespace fincept::screens::fno
