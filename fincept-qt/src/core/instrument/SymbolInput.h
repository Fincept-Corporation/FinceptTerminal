#pragma once
// SymbolInput — the ONE normaliser for symbol text typed, pasted, dropped,
// imported or stored anywhere in the terminal (symbol system, plan E2).
//
// parse_symbol_input() turns text into a classified ParsedInput with ORDERED
// canonical-symbol candidates (Yahoo form). It never looks anything up over the
// network and never picks silently: when the text fits several instruments, all
// plausible candidates come back in preference order and the resolver / picker
// decides. canonical_symbol() is the idempotent spelling clean-up of an already
// stored symbol.
//
// Qt Core only, pure, deterministic, thread-safe (immutable static tables), no
// locale-dependent case conversion (QString::toUpper is Unicode, not locale).
// Shared test vectors: tests/data/symbol_vectors.json (the backend's Python port
// runs the same file).
//
// Clean-up applied first (ParsedInput::cleaned)
//   Unicode NFKC (full-width "ＡＡＰＬ" -> "AAPL", NBSP -> space); format / control
//   characters removed (zero-width space, BOM); tabs / newlines / runs of
//   whitespace -> one space; trimmed; wrapping quotes " ' ` ‘ ’ “ ” « » and a
//   leading '$' (cashtag) stripped. Case is kept in `cleaned`; tickers are
//   upper-cased for matching.
//
// Classification, in order (first match wins)
//   Empty      nothing left after clean-up.
//   Invalid    longer than 100 characters; a single token longer than 32; characters
//              no symbol uses; an ISIN-shaped token whose check digit fails.
//   Isin       2 letters + 9 alphanumerics + check digit, Luhn-valid (US0378331005).
//              candidates EMPTY — an ISIN is a lookup key, never a symbol.
//   "EXCH:REST"  a known exchange prefix (TradingView codes first: NSE:, NASDAQ:, BINANCE:,
//              OANDA:, SP:, TVC:, NFO:, ...) becomes the venue and REST is classified below.
//              An explicit `venue_hint` argument wins over the prefix. An unknown prefix
//              gives no candidates (note "unknown_venue").
//   Contract   on a derivatives venue (NFO BFO MCX CDS BCD NCDEX EUREX); Indian contract names
//              (NIFTY2660923200CE, RELIANCE26JULFUT) — no candidates; OCC options
//              (AAPL250117C00150000) — the OCC symbol; TradingView continuous futures
//              (GC1!, ES1!) — the Yahoo continuous contract when the root is a known US future.
//   YahooSymbol  "^INDEX", "XX=F", dated futures (CLF27.NYM), a table index symbol
//              (NIFTY_FIN_SERVICE.NS, 000001.SS), or CODE.SUFFIX with a known Yahoo suffix
//              (RELIANCE.NS, VOD.L, 700.HK -> 0700.HK). The suffix wins over any hint.
//   FxPair     "=X" forms; EURUSD; EUR/USD; EUR-USD; two ISO 4217 codes. USD base -> Yahoo's
//              own form INR=X (USDINR, USD/INR, USDINR=X). Metals (XAUUSD, XAG/USD) are
//              commodity aliases instead (Yahoo has no spot metals).
//   Alias      the whole text names a table index / commodity / crypto ("NIFTY", "NIFTY 50",
//              "S&P 500", "SENSEX", "gold", "crude", "bitcoin"). With a venue, index aliases
//              apply only on a venue of the index's country (NSE + NIFTY -> ^NSEI, never
//              NIFTY.NS) and never for text_only aliases ("DOW" on NYSE is Dow Inc);
//              commodity / crypto aliases only without a venue or on a futures / FX /
//              crypto / index venue. When the typed text is itself ticker-shaped (1-6 of
//              A-Z 0-9 &, not a currency pair) and there is no venue, the same-named US
//              listing is appended LAST ("gold" -> GC=F, GOLD; "DOW" -> ^DJI, DOW).
//   CryptoPair BASE/QUOTE, BASE-QUOTE, BASEQUOTE(.P) with a crypto quote; a bare crypto base
//              ("BTC"). Yahoo base from the crypto table; stablecoin quotes -> USD (note
//              "stablecoin_as_usd"; `pair` keeps "BTC/USDT" for exchanges). A bare base with
//              crypto_first (BTC, ETH, SOL, ...) -> [BASE-USD, BASE] (the same-named US
//              listing second); with crypto_first false (STX, SUI, SAND, BCH) it is a Ticker
//              with the coin appended last. An unknown base gives no Yahoo candidate (note
//              "unknown_crypto_base").
//   Ticker     one token (or "SYM EXCH" / "SYM X" two tokens): '_' -> '-'; class shares
//              BRK.B / BRK/B / "BRK B" / BRK-B -> BRK-B (base 1-5 letters, one class letter,
//              unless the letter is a Yahoo suffix: VOD.L is LSE); "SYM.EXCH" / "SYM EXCH"
//              with a venue code (AAPL.US, RELIANCE NSE) set the venue. Candidates:
//                with a venue: [to_yahoo(code, venue)] — series suffixes stripped on NSE/BSE,
//                  HK zero-padded; empty (note "no_yahoo_venue") where Yahoo has nothing.
//                without: a fan-out, every entry produced by to_yahoo():
//                  letters         US, NSE, BSE, LSE, TSX, ASX, XETRA, PARIS
//                  class share     US, TSX, LSE
//                  1-3 / 5 digits  HKEX (zero-padded to 4)
//                  4 digits        TSE, HKEX, TWSE, TADAWUL, MYX, TPEX (leading 0: HKEX, TWSE, MYX)
//                  6 digits        SSE (6/5/9xxxxx) or SZSE (0/1/2/3xxxxx), KOSPI, KOSDAQ, BSE (5xxxxx)
//                  NNNA (130A)     TSE
//                then the home country's entries moved to the front (`home_country`).
//   Name       anything else with letters (spaces, accents, any script): no candidates;
//              `search_text` carries the text for name search.
//
// Lists: a string with spaces is ONE item; split_symbol_list() splits pasted lists on
// ',' ';' tab and newlines only.

#include "core/instrument/Instrument.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::core::instrument {

enum class InputKind : quint8 {
    Empty,
    Invalid,
    Isin,
    Contract,
    YahooSymbol,
    FxPair,
    CryptoPair,
    Alias,
    Ticker,
    Name,
};

/// "empty", "invalid", "isin", "contract", "yahoo_symbol", "fx_pair", "crypto_pair",
/// "alias", "ticker", "name" (the vector file's spelling).
QString input_kind_slug(InputKind kind);

struct ParsedInput {
    QString raw;      ///< the text as given
    QString cleaned;  ///< after clean-up (see header); original case
    InputKind kind = InputKind::Empty;
    QString base;     ///< listing code ("BRK-B", "RELIANCE"), FX / crypto base, contract underlying, ISIN
    QString quote;    ///< FX / crypto quote currency as typed ("USDT" kept)
    QString pair;     ///< "EUR/USD", "BTC/USDT" (exchange-side form); empty otherwise
    QString venue;    ///< venue id of the hint / prefix / suffix / pair (MarketSessions or listing-only id)
    QString venue_hint; ///< the hint text that applied (argument, "EXCH:" prefix or "SYM EXCH" word)
    AssetClass asset_class = AssetClass::Unknown; ///< best guess = preferred_classes.value(0)
    QVector<AssetClass> preferred_classes;  ///< class preference implied by the text, best first
    QStringList candidates; ///< canonical Yahoo symbols, best first, no duplicates, never an ISIN
    QString canonical;      ///< the one symbol a fully specified input denotes (YahooSymbol, FxPair,
                            ///< CryptoPair with a quote, OCC, Ticker with a venue); empty otherwise
    QString isin;           ///< kind == Isin
    QString search_text;    ///< text for name / fuzzy search; empty for Empty / Invalid / Isin
    QStringList notes;      ///< "unknown_venue", "no_yahoo_venue", "no_yahoo_symbol", "unknown_crypto_base",
                            ///< "stablecoin_as_usd", "perpetual", "isin_check_digit", "too_long",
                            ///< "bad_characters", "unknown_suffix", "series_stripped"

    bool has_candidates() const { return !candidates.isEmpty(); }
    bool has_note(const char* note) const { return notes.contains(QLatin1String(note)); }
};

/// Parse one item. `venue_hint`: any exchange code (venue id, MIC, TradingView / Yahoo /
/// broker code, country code); unknown hint -> no candidates + note "unknown_venue".
/// `home_country`: ISO 3166-1 alpha-2 of the user's region; reorders fan-out candidates only.
ParsedInput parse_symbol_input(const QString& text, const QString& venue_hint = {},
                               const QString& home_country = {});

/// Idempotent clean-up of a stored symbol into the canonical market-data key:
/// canonical_symbol(canonical_symbol(s, h), h) == canonical_symbol(s, h). Rules:
///   case and whitespace: "  eurusd=x " -> "EURUSD=X", "^nsei" -> "^NSEI";
///   class shares: "BRK.B" / "BRK/B" / "BRK B" -> "BRK-B";
///   HK padding: "700.HK" / "00700.HK" -> "0700.HK";
///   NSE / BSE: "BAJAJ_AUTO.NS" -> "BAJAJ-AUTO.NS", "RELIANCE-EQ.NS" -> "RELIANCE.NS"
///     (index symbols with underscores such as NIFTY_FIN_SERVICE.NS are kept);
///   FX: "USDINR=X" -> "INR=X" (same Yahoo instrument);
///   prefixed / pair forms: "NSE:RELIANCE-EQ" -> "RELIANCE.NS", "BTC/USDT" -> "BTC-USD",
///     "EUR/USD" -> "EURUSD=X";
///   with `exchange_hint` a bare code becomes its listing: ("RELIANCE", "NSE") -> "RELIANCE.NS",
///     ("NIFTY 50", "NSE_INDEX") -> "^NSEI".
/// It never guesses: a bare code without hint stays bare ("RELIANCE", "EURUSD", "BTC", "7203"
/// stay as they are — bare is Yahoo's US convention); a well-formed Yahoo crypto symbol is
/// kept verbatim ("TON-USD" is a valid, different Yahoo coin). Empty result for anything
/// that is not a symbol: ISINs, names with spaces, junk, an unknown or Yahoo-less venue.
QString canonical_symbol(const QString& stored, const QString& exchange_hint = {});

/// Split pasted text into items on ',' ';' tab, CR and LF; items cleaned (see header) and
/// empty items dropped. Spaces never split ("tata motors" stays one item).
QStringList split_symbol_list(const QString& text);

/// Clean-up only (the `cleaned` step of parse_symbol_input).
QString clean_symbol_text(const QString& text);

/// Name normalisation for name search / symbol_master.name_norm: NFKD, combining marks
/// removed ("Nestlé" -> "nestle"), Unicode case-folded, every non letter / digit except
/// '&' -> space, whitespace collapsed. CJK and other scripts are kept ("腾讯控股").
QString normalize_name(const QString& text);

/// Luhn check of an ISIN (12 characters, upper case). False for anything else.
bool is_valid_isin(const QString& isin);

} // namespace fincept::core::instrument
