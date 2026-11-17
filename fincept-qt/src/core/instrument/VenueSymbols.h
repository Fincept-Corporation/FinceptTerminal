#pragma once
// VenueSymbols — THE venue / provider symbol table of the terminal (symbol
// system, plan E3). One table each for: listing markets (Yahoo suffix, MIC,
// TradingView / Yahoo / broker exchange codes), indices (with every alias users
// and brokers type), futures and commodity aliases, crypto assets, and the ISO
// 4217 currency list. Every exchange -> suffix map, index-alias map, series-suffix
// set and asset-class guesser elsewhere in the code base is replaced by a call
// here (see the adoption table in the WP1 report).
//
// Qt Core only. Every table is an immutable function-local static built on first
// use (thread-safe initialisation); no function blocks, allocates shared state or
// depends on the locale. Safe from any thread.
//
// Relationship to core/market/MarketSessions
//   MarketSessions stays the registry of venues WITH a session model (hours,
//   holidays). VenueInfo::venue is a MarketSessions venue id whenever the venue
//   has one; venues Yahoo lists but MarketSessions does not model (Athens,
//   Prague, Buenos Aires, ...) get a listing-only id here (venue_by_code() returns
//   nullptr for those). Every Yahoo suffix MarketSessions knows maps to the same
//   venue here (unit-tested).
//
// Code spaces
//   Exchange codes collide between namespaces: "NGM" is Nasdaq Global Market in
//   Yahoo's quote.exchange but the Nordic Growth Market in TradingView; "CSE" is
//   the Canadian Securities Exchange for TradingView / MarketSessions but Colombo
//   in Yahoo's quote.exchange. Lookups therefore take a CodeSpace:
//     Any          this table's own codes, then MarketSessions ids / MICs /
//                  aliases / country codes, then TradingView codes, then Yahoo
//                  exchange codes
//     TradingView  TradingView codes first (backend /market/search rows and the
//                  "EXCH:SYM" prefix form), then as Any
//     Yahoo        Yahoo quote.exchange codes first, then as Any
//
// Never-wrong rules (owner decisions)
//   * The canonical market-data key is the Yahoo form.
//   * A venue that is unknown, or that Yahoo does not carry, yields NO Yahoo
//     symbol (empty string) — never the bare ticker (a bare ticker is a US
//     listing on Yahoo and would quote a different company).
//   * An ISIN never comes out as a symbol.
//   * Crypto bases are mapped only through the crypto table: Yahoo reuses short
//     tickers for unrelated coins (TON-USD is "TON Token", Toncoin is
//     TON11419-USD; XBT-USD is "XBIT", Kraken's XBT is Bitcoin), so an unknown
//     base gives no Yahoo symbol rather than a guess.

#include "core/instrument/Instrument.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::core::instrument {

// ── Venue table ─────────────────────────────────────────────────────────────

enum class CodeSpace : quint8 { Any, TradingView, Yahoo };

enum class VenueRole : quint8 {
    Listing,        ///< securities venue; Yahoo symbol = code + "." + suffix
    UsListing,      ///< US venue; Yahoo symbol = code without suffix ('.' / '/' -> '-')
    Euronext,       ///< multi-market code ("EURONEXT"): the country picks Paris / Amsterdam / ...
    Fx,             ///< currency pairs (FX, OANDA, FX_IDC, ...)
    Crypto,         ///< crypto exchanges and the CRYPTO pseudo venue
    IndexNamespace, ///< index providers / namespaces (SP, DJ, TVC, INDEX, NSE_INDEX, BSE_INDEX)
    Futures,        ///< US futures exchanges Yahoo carries (CME, CBOT, NYMEX, COMEX, ICE US)
    Derivatives,    ///< contract venues Yahoo does not carry (NFO, BFO, MCX, CDS, NCDEX, EUREX)
};

struct VenueInfo {
    QString key;             ///< unique key in this table ("NSE", "KOSDAQ", "LSE_IOB", "FX", ...)
    QString venue;           ///< MarketSessions venue id, or a listing-only id; empty for pure namespaces
    QString mic;             ///< ISO 10383 MIC; empty for pseudo venues and approximations
    QString country;         ///< ISO 3166-1 alpha-2; empty for global namespaces
    QString currency;        ///< trading currency as quoted (minor units kept: GBp, ZAc, ILA, KWF)
    QStringList suffixes;    ///< Yahoo suffixes (upper case, no dot); the first is what to_yahoo writes
    VenueRole role = VenueRole::Listing;
    bool yahoo_covered = true;  ///< false: Yahoo carries no data for the venue -> to_yahoo() returns ""
    bool approximate = false;   ///< codes are quoted through another listing of the same share
                                ///< (gettex / Tradegate / L&S -> Xetra .DE, BIVA -> .MX, BX -> .SW)
    bool session_model = false; ///< `venue` is a MarketSessions venue
    QString broker_exchange; ///< instrument-master exchange code ("NSE", "BSE", "NSE_INDEX", "NFO"); empty if none
    QStringList codes;       ///< this table's own codes (CodeSpace::Any)
    QStringList tv_codes;    ///< TradingView exchange codes
    QStringList yahoo_codes; ///< Yahoo quote.exchange codes
};

/// Every entry, in table order.
const QVector<VenueInfo>& all_venue_infos();

/// Entry by its key ("KOSDAQ"); nullptr if none.
const VenueInfo* venue_info(const QString& key);

/// Entry for an exchange code in `space` (see the header comment), ASCII case-
/// insensitive, surrounding blanks ignored. For the multi-market code "EURONEXT"
/// a non-empty `country` (FR NL BE PT IE NO IT) selects the market; without it
/// the Euronext entry itself comes back (to_yahoo() then needs a suffixed code).
/// nullptr for an unknown code.
const VenueInfo* venue_info_for_code(const QString& code, CodeSpace space = CodeSpace::Any,
                                     const QString& country = {});

/// Owner of a Yahoo suffix ("NS", ".ns", "KQ", "NYM"); nullptr if unknown. Approximate
/// entries never own a suffix (".DE" is Xetra, not gettex).
const VenueInfo* venue_info_for_suffix(const QString& suffix);

/// The venue id a code names (venue_info_for_code(code, space)->venue); empty if unknown.
QString venue_id_for_code(const QString& code, CodeSpace space = CodeSpace::Any);

// ── Yahoo form ──────────────────────────────────────────────────────────────

/// Yahoo symbol of a listing / pair / index / futures code on a venue, or "" when
/// there is none. `venue_or_code` is any code venue_info_for_code() accepts (CodeSpace::Any);
/// `country` resolves "EURONEXT". Rules, in order:
///   1. `listing_code` already self-describing — "^NSEI", "EURUSD=X", "GC=F", an OCC
///      option, or CODE.SUFFIX with a known Yahoo suffix — comes back as
///      canonical_symbol(listing_code), whatever the venue.
///   2. Empty `venue_or_code`: the code is read as Yahoo/US form -> canonical_symbol().
///      An unknown non-empty venue -> "".
///   3. By venue role:
///        Derivatives, !yahoo_covered, Euronext without country -> ""
///        Fx         EURUSD / EUR/USD / EUR-USD -> EURUSD=X; USD base -> INR=X (Yahoo's own form);
///                   XAUUSD / XAGUSD / XPTUSD / XPDUSD -> GC=F / SI=F / PL=F / PA=F (Yahoo has no spot metals)
///        Crypto     BTCUSDT / BTC/USDT / BTC-USD / BTCUSDT.P / BTC -> BTC-USD: base through the crypto
///                   table only (unknown base -> ""); stablecoin quotes (USDT USDC BUSD FDUSD TUSD DAI)
///                   -> USD (Yahoo has no USDT books); fiat / BTC / ETH quotes kept (BTC-EUR, ETH-BTC)
///        IndexNamespace  index alias (NIFTY 50, SPX, NI225, UKX, ...) or commodity alias (TVC:GOLD, USOIL)
///        Futures    GC / GC1! -> GC=F; dated GCZ2026 / GCZ26 -> GCZ26.CMX (exchange suffix from the root)
///        Listing / UsListing:
///          cls Index, or cls Unknown and the code is an index code of the venue's country
///            (NSE + NIFTY -> ^NSEI, CBOE + SPX -> ^GSPC; "DOW" on NYSE stays Dow Inc) -> the index
///          cls Fx / Crypto / Future / Option / Commodity -> ""
///          an Indian contract name (NIFTY2660923200CE, RELIANCE26JULFUT) -> ""
///          else equity: trailing '.' dropped; '_' -> '-' (TradingView BAJAJ_AUTO); NSE/BSE series
///          suffixes -EQ -BE -BZ -BL -SM -ST -MF -SG dropped; class share X.Y / X/Y / "X Y" -> X-Y;
///          US: every '.' / '/' -> '-' (BRK.B -> BRK-B); HKEX: digits re-padded to 4 (700 -> 0700);
///          SSE / SZSE / KOSPI / KOSDAQ: digits padded to 6; Colombo: '.' -> '-'; then + "." + suffix.
QString to_yahoo(const QString& listing_code, const QString& venue_or_code, AssetClass cls = AssetClass::Unknown,
                 const QString& country = {});

/// to_yahoo() for a TradingView row: `tv_exchange` is looked up in CodeSpace::TradingView
/// ("NGM" = Nordic Growth Market, "KRX" = Korea (KOSPI suffix; KOSDAQ names come back as .KS
/// and do not quote — parse_symbol_input offers .KQ), "EURONEXT" + country). A "EXCH:SYM"
/// `tv_symbol` with empty `tv_exchange` uses its own prefix.
QString to_yahoo_tv(const QString& tv_symbol, const QString& tv_exchange = {}, const QString& country = {},
                    AssetClass cls = AssetClass::Unknown);

struct YahooParts {
    QString symbol;  ///< canonical Yahoo spelling of the input
    QString code;    ///< round-trip code: to_yahoo(code, market, cls) == symbol for every covered venue
                     ///< (the first suffix of a venue: NESN.VX comes back as NESN.SW).
                     ///< Listing "RELIANCE" / "0700" / "BRK-B"; index "NSEI" / "NIFTY_FIN_SERVICE";
                     ///< FX "EURUSD" / "USDINR"; crypto "BTC-USD" / "TON-USD" (table base); futures "GC" / "CLF27"
    QString base;    ///< listing code, FX / crypto base asset, futures root
    QString quote;   ///< FX / crypto quote currency; empty otherwise
    QString suffix;  ///< Yahoo suffix without dot ("NS"); empty when none
    QString market;  ///< VenueInfo::key of the listing market ("KOSDAQ" vs "KOSPI", "LSE_IOB" vs "LSE");
                     ///< "NYSE" for bare US symbols; "FX" / "CRYPTO" / "CME"; empty for indices off-table
    QString venue;   ///< venue id; "NYSE" for a bare US symbol (Yahoo does not say NYSE vs Nasdaq)
    QString country; ///< listing country ("US" for bare symbols); empty for FX / crypto
    AssetClass cls = AssetClass::Unknown; ///< Equity for listings (Yahoo's form cannot tell an ETF)
    bool venue_from_suffix = false;      ///< false when the venue is a convention (bare = US)
    bool valid() const { return !symbol.isEmpty(); }
};

/// Split a Yahoo symbol. Invalid (empty symbol) for "", ISINs, names and junk.
YahooParts from_yahoo(const QString& yahoo_symbol);

/// Asset class from the source's type string when it is a known one (Yahoo quoteType
/// EQUITY ETF MUTUALFUND MONEYMARKET INDEX CURRENCY CRYPTOCURRENCY FUTURE OPTION,
/// TradingView stock dr fund etf index forex crypto spot swap futures bond, our slugs),
/// else from the symbol's syntax: index table / '^' -> Index; =X -> Fx; =F and
/// .NYM .CMX .CBT .CME .NYB -> Future; BASE-QUOTE with a crypto quote -> Crypto; OCC and
/// Indian ...CE / ...PE names -> Option; ...FUT names -> Future; BASE/QUOTE -> Fx or Crypto;
/// ISIN / empty / names -> Unknown; anything else -> Equity.
AssetClass asset_class_of(const QString& symbol, const QString& quote_type = {});

/// Stable instrument id of a Yahoo symbol (format in Instrument.h); empty when the symbol
/// is not valid or its suffix is unknown.
QString instrument_id_for(const QString& yahoo_symbol);

/// What the UI shows: "RELIANCE", "BRK.B" (US class shares dotted), "NIFTY 50" (index
/// name), "EUR/USD", "USD/INR", "BTC/USD", "GC" (futures root), "CLF27".
QString display_symbol(const QString& yahoo_symbol);

/// A record filled from the symbol alone: id, symbol, display, asset_class, venue, mic,
/// country, currency, and for table instruments (indices, futures, crypto) name and
/// aliases. A bare US symbol gets country "US" and currency "USD" but NO venue / mic (the
/// form does not say NYSE vs Nasdaq; MarketSessions::venue_for_symbol still gives the
/// session). Listings are Equity (the form cannot tell an ETF). Invalid record for an
/// invalid symbol.
Instrument instrument_skeleton(const QString& yahoo_symbol);

// ── Broker / exchange forms ─────────────────────────────────────────────────

/// The terminal's normalised broker symbol (instruments.symbol — what InstrumentService
/// ::find and the order APIs take) and its exchange code, or "" when the broker side has
/// no such instrument. Source, in order:
///   provider_symbols["broker:<broker_id>"] = "EXCHANGE|SYMBOL|TOKEN[|BRSYMBOL]";
///   NSE / BSE listings -> code + "NSE" / "BSE" (series-free: RELIANCE.NS -> RELIANCE);
///   Indian indices -> NIFTY / BANKNIFTY / FINNIFTY / MIDCPNIFTY / NIFTYNXT50 / SENSEX ... + NSE_INDEX / BSE_INDEX;
///   US listings -> code with class shares as "BRK.B" ("BRK B" for broker_id "ibkr") + the instrument's venue;
///   other listings -> code + venue id;
///   FX, crypto, futures, options, other indices -> "".
QString to_broker_symbol(const Instrument& inst, const QString& broker_id, QString* exchange = nullptr);

enum class CryptoPairStyle : quint8 {
    Unified, ///< ccxt unified "BASE/QUOTE" (what Crypto Trading and ExchangeService use)
    Native,  ///< the exchange's own code: binance "BTCUSDT", okx "BTC-USDT", coinbase "BTC-USD", kraken "XBT/USD"
};

/// Crypto pair for an exchange. provider_symbols[exchange_id] wins (Unified style); else
/// from the Yahoo symbol: a USD quote becomes the exchange's dollar book — "USD" on
/// coinbase / kraken / gemini / bitstamp / bitfinex, "USDC" + ":USDC" settle on hyperliquid,
/// "USDT" elsewhere. "" for a non-crypto instrument.
QString crypto_pair(const Instrument& inst, const QString& exchange_id, CryptoPairStyle style = CryptoPairStyle::Unified);

/// Indian series suffix removed: "RELIANCE-EQ" -> "RELIANCE" (-EQ -BE -BZ -BL -SM -ST -MF -SG,
/// upper-cased input). "BAJAJ-AUTO", "MCDOWELL-N" unchanged. The ONE series set.
QString strip_series_suffix(const QString& code);

/// True for Indian exchange contract names: ...<digit>CE / ...<digit>PE / ...FUT with a digit.
bool is_exchange_contract_name(const QString& code);

/// Broker canonical index symbol of an index name ("NIFTY 50" -> NIFTY, "Nifty Bank" ->
/// BANKNIFTY, "BSE SENSEX" -> SENSEX, "NIFTY NEXT 50" -> NIFTYNXT50, "INDIA VIX" -> INDIAVIX);
/// unknown names: upper case, "S&P " removed, spaces and '-' removed. Superset of
/// norm::normalise_index_symbol and the Angel-only copy in InstrumentService.
QString broker_index_symbol(const QString& raw_name);

// ── Reference tables ────────────────────────────────────────────────────────

struct IndexInfo {
    QString yahoo;           ///< Yahoo symbol; empty when Yahoo has none (BANKEX, TOPIX)
    QString venue;           ///< venue id whose session the index follows
    QString country;
    QString name;            ///< display name ("NIFTY 50")
    QString broker_symbol;   ///< Indian instrument-master symbol ("NIFTY"); empty elsewhere
    QString broker_exchange; ///< "NSE_INDEX" / "BSE_INDEX"; empty elsewhere
    QStringList aliases;     ///< text users / brokers / TradingView type ("NIFTY 50", "SPX", "NI225")
    QStringList text_only;   ///< aliases that are also listed tickers or venue codes ("DOW", "NASDAQ"):
                             ///< honoured for free text, never as a listing code on a venue
};
const QVector<IndexInfo>& all_indices();
/// Index by Yahoo symbol ("^NSEI", "NIFTY_FIN_SERVICE.NS"); nullptr if not in the table.
const IndexInfo* index_for_symbol(const QString& yahoo_symbol);

struct FutureInfo {
    QString root;            ///< "GC"
    QString yahoo;           ///< continuous contract "GC=F"
    QString exchange_suffix; ///< Yahoo suffix of dated contracts ("CMX": GCZ26.CMX)
    QString name;
    QString currency;        ///< as Yahoo quotes it ("USD", "USX" = US cents)
    bool commodity = false;  ///< a commodity (vs index / rate / FX future)
    QStringList aliases;     ///< "gold", "XAUUSD", "crude", ...
};
const QVector<FutureInfo>& all_futures();
const FutureInfo* future_for_root(const QString& root);

struct CryptoInfo {
    QString base;            ///< exchange ticker ("TON")
    QString yahoo_base;      ///< Yahoo's base ("TON11419"); Yahoo symbol = yahoo_base + "-" + quote
    QString name;
    bool crypto_first = true; ///< bare base text prefers the coin over a same-named listing (BTC);
                              ///< false where a large listed company owns the ticker (STX, SUI, SAND, BCH)
    QStringList aliases;     ///< "BITCOIN", "XBT", "MATIC"
};
const QVector<CryptoInfo>& all_cryptos();
/// By base, Yahoo base or alias ("BTC", "XBT", "TON11419", "MATIC"); nullptr if unknown.
const CryptoInfo* crypto_for_base(const QString& base);

/// ISO 4217 active currency code (plus CNH, the offshore yuan Yahoo quotes). Upper case only.
bool is_iso_currency(const QString& code);
/// Quote currencies a crypto pair can end with (USD USDT USDC BUSD FDUSD TUSD DAI EUR GBP JPY
/// KRW INR TRY BRL AUD CAD CHF BTC ETH BNB); longest first.
const QStringList& crypto_quote_currencies();
/// USDT USDC BUSD FDUSD TUSD DAI.
bool is_stablecoin(const QString& code);

/// Key used for alias matching (and the symbol_alias.alias_norm column): NFKC, upper case,
/// every character that is not a letter or digit removed ("S&P 500" -> "SP500",
/// "Nifty_50" -> "NIFTY50", "^NSEI" -> "NSEI").
QString alias_key(const QString& text);

struct AliasTarget {
    AssetClass asset_class = AssetClass::Unknown; ///< Index, Future or Crypto
    QString yahoo;     ///< Yahoo symbol; empty when the index has none
    QString name;
    QString venue;
    QString country;   ///< index country; empty for futures / crypto
    bool text_only = false; ///< see IndexInfo::text_only
};
/// Every table instrument whose alias key equals alias_key(text), in table order (indices,
/// then futures, then crypto names). "CRUDE" -> CL=F, BZ=F. Bare crypto bases ("BTC") are
/// not aliases — crypto_for_base() handles them; crypto names and alternative codes are
/// ("BITCOIN", "XBT", "MATIC").
QVector<AliasTarget> lookup_alias(const QString& text);

} // namespace fincept::core::instrument
