#pragma once
// InstrumentRank — the ONE ranking of instrument search hits (symbol system,
// plan E4). A pure function over a candidate list: the resolver (WP3), the local
// index (WP2), the picker (WP4) and the backend port (WP8) all order hits with
// it, so "AAPL", "RELIANCE", "BTC", "gold" and "NIFTY" come out the same way
// everywhere. Qt Core only, deterministic, thread-safe.
//
// Match tiers (match_instrument), best first
//   ExactSymbol       instrument.symbol == the canonical symbol a fully specified input
//                     denotes (ParsedInput::canonical: "RELIANCE.NS", "EURUSD=X", "7203" + JP)
//   ExactListingCode  a listing code of the instrument equals the typed code (RELIANCE ->
//                     RELIANCE.NS / RELIANCE.BO, SAP -> SAP.DE / SAP / SAP.TO, BTC -> BTC-USD;
//                     all-digit codes compare without leading zeros: 700 == 0700), or the
//                     instrument is one of a bare ticker's candidates
//   ExactAlias        an alias or a FORMER symbol of the instrument equals the text
//                     (alias_key: "nifty 50" == "NIFTY50"; TATAMOTORS on TMPV — flagged
//                     via_former_symbol so the UI can say "now TMPV"), or the normaliser
//                     mapped an alias / crypto text to this instrument ("gold" -> GC=F)
//   ExactIsin         the text is an ISIN and equals instrument.isin
//   Prefix            a listing code starts with the typed code, or the normalised name /
//                     an alias starts with the typed text ("tata mot", "apple")
//   NameContains      the normalised name contains the text (3+ characters)
//   Fuzzy             optimal-string-alignment distance between the text (4+ characters)
//                     and a code, an alias or the name's leading words: <= 1 for 4-5
//                     characters, <= 2 for 6+ ("relaince" -> RELIANCE, "microsft" -> Microsoft)
//
// Sort key (rank_instruments), first difference decides
//   1. group: exact tiers < Prefix < NameContains < Fuzzy
//   2. active before delisted / renamed (within the group: a renamed exact hit still beats a
//      prefix hit, but the successor found through its former symbol comes first)
//   3. the class preference the TEXT implies (ParsedInput::preferred_classes: "BTC" -> crypto,
//      "gold" -> futures, "NIFTY" -> index) — the owner rule "the crypto / commodity / index
//      first, the same-named listing second"
//   4. tier inside the group (ExactSymbol < ExactListingCode < ExactAlias < ExactIsin;
//      fuzzy: smaller distance)
//   5. the user's home country (RankContext::home_country) — "INFY" in India -> INFY.NS
//   6. primary listing
//   7. the caller's class priority (RankContext::class_priority)
//   8. rank_weight, larger first
//   9. inside Prefix: a code prefix before a name prefix
//  10. the normaliser's candidate order, then symbol, then id (deterministic)
// Prefix ranks code and name prefixes together and lets weight decide before the
// code/name distinction (step 9), so "apple" puts AAPL ahead of small coins whose
// code starts with APPLE (audit C13).
//
// Filtering: instruments with no match are dropped; RankContext::allowed_classes keeps
// only those classes (Unknown dropped when the list is non-empty); options and dated
// futures are dropped unless include_derivatives (continuous "=F" futures stay);
// quote_type "cfd" rows are dropped unless include_cfd; inactive rows are dropped when
// include_inactive is false. One row per instrument (Instrument::same_instrument): the
// best-ranked copy is kept.

#include "core/instrument/Instrument.h"
#include "core/instrument/SymbolInput.h"

#include <QString>
#include <QStringView>
#include <QVector>

namespace fincept::core::instrument {

enum class MatchKind : quint8 {
    None,
    ExactSymbol,
    ExactListingCode,
    ExactAlias,
    ExactIsin,
    Prefix,
    NameContains,
    Fuzzy,
};

/// "none", "exact_symbol", "exact_listing_code", "exact_alias", "exact_isin", "prefix",
/// "name_contains", "fuzzy".
QString match_kind_slug(MatchKind kind);

/// True for the four exact tiers.
inline bool is_exact_match(MatchKind kind) {
    return kind == MatchKind::ExactSymbol || kind == MatchKind::ExactListingCode || kind == MatchKind::ExactAlias ||
           kind == MatchKind::ExactIsin;
}

struct RankContext {
    QString home_country;                 ///< ISO 3166-1 alpha-2 of the user's region; empty = none
    QVector<AssetClass> class_priority;   ///< caller's class order, best first; empty = none
    QVector<AssetClass> allowed_classes;  ///< empty = every class
    bool include_derivatives = false;     ///< keep options and dated futures
    bool include_cfd = false;             ///< keep quote_type "cfd" rows (CFD-broker duplicates)
    bool include_inactive = true;         ///< keep delisted / renamed rows (ranked after active ones)
    int limit = 0;                        ///< 0 = no limit
};

struct RankedInstrument {
    Instrument instrument;
    MatchKind match = MatchKind::None;
    QString matched_text;          ///< the code / alias / former symbol / name that matched
    bool via_former_symbol = false; ///< ExactAlias through former_symbols ("formerly TATAMOTORS")
    bool code_prefix = false;      ///< Prefix through a listing code (false: through the name / an alias)
    int distance = 0;              ///< Fuzzy: edit distance
};

/// How `inst` matches the parsed text (match == None when it does not).
RankedInstrument match_instrument(const Instrument& inst, const ParsedInput& parsed);

/// Match, filter, sort and de-duplicate `candidates` (see the header comment).
QVector<RankedInstrument> rank_instruments(const QVector<Instrument>& candidates, const ParsedInput& parsed,
                                           const RankContext& ctx = {});

/// rank_instruments() for raw text: parse_symbol_input(text, {}, ctx.home_country) first.
QVector<RankedInstrument> rank_instruments(const QVector<Instrument>& candidates, const QString& text,
                                           const RankContext& ctx = {});

/// Strict weak order used by rank_instruments (exposed for tests and for merging pages).
bool rank_before(const RankedInstrument& a, const RankedInstrument& b, const ParsedInput& parsed,
                 const RankContext& ctx);

/// Optimal-string-alignment distance (insert / delete / substitute / swap adjacent),
/// compared as given (callers normalise). Returns max_distance + 1 as soon as the distance
/// must exceed max_distance.
int edit_distance(QStringView a, QStringView b, int max_distance);

} // namespace fincept::core::instrument
