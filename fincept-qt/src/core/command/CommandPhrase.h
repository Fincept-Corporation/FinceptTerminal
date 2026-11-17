#pragma once
// CommandPhrase — the small grammar of plain-language intents the search box
// understands WITHOUT any model call. Pure (Qt Core, no I/O); tests/tst_command_phrase.cpp.
//
//   aapl chart · chart aapl · msft news · tsla research · spy options   -> SymbolView
//   buy 10 msft · sell 5 tsla limit 250 · buy msft @ 410.5 · buy msft   -> Ticket (a PREFILL; nothing is sent)
//   compare aapl msft · aapl vs msft                                     -> Compare
//   eur usd · eurusd · eur/usd                                           -> FxPair
//   btc · eth usdt · btcusdt                                             -> Crypto
//   settings currency · preferences font                                 -> Settings
//   theme light · dark theme · dark mode                                 -> Theme
//   backtest spy sma                                                     -> Backtest
//   open notes · go to markets · show watchlist                          -> Open
//   add aapl to watchlist tech · add aapl to tech · watch aapl           -> Watchlist
//   ask why is gold up                                                   -> Ask (Ask AI row; never sent)
//   > refresh · $ aapl · @ aapl · ? backtest                             -> Scope (optional accelerators)
//   markets add news · markets replace news · markets remove             -> Dock (the old bar's commands)
//   /stock aapl · /crypto btc · /economic gdp                            -> AssetType (the old bar's /types)
//
// parse() returns every reading, strongest first. The box ALWAYS runs the plain
// search too; a phrase only adds intent rows (or narrows a group) — so a wrong
// guess never hides what the user typed.

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::command::phrase {

enum class Kind {
    None,
    Scope,      ///< scope = "action" | "instrument" | "help"; text = the rest
    SymbolView, ///< view = chart | news | research | options | trade | backtest | peers; symbols[0]
    Ticket,     ///< side = buy | sell; quantity (0 = none); symbols[0]; price (0 = none); order_type limit|market|""
    Compare,    ///< symbols (2+)
    FxPair,     ///< pair "EURUSD"; symbols[0] = "EURUSD=X" (Yahoo)
    Crypto,     ///< pair "BTC/USD"; symbols[0] = "BTC-USD" (Yahoo)
    Settings,   ///< text = what to find in Settings
    Theme,      ///< target = light | dark | system
    Backtest,   ///< symbols[0] (may be empty); text = strategy words
    Open,       ///< text = what to open
    Watchlist,  ///< symbols[0]; text = watchlist name ("" = the default / ask)
    Ask,        ///< text = the question
    Dock,       ///< verb = add | replace | remove; target = first screen id; text = second screen id
    AssetType,  ///< target = the old /type ("stock", "fund", "dr", "index", "forex", "crypto", "futures", "bond",
                ///<          "economic"); text = the query after it
};

struct Phrase {
    Kind kind = Kind::None;
    QString view;
    QStringList symbols;
    QString side;
    double quantity = 0;
    double price = 0;
    QString order_type;
    QString text;
    QString target;
    QString scope;
    QString pair;
    QString verb;
};

struct Vocabulary {
    /// Screen id for a token or title the user typed ("mkts", "markets", "equity research"); "" = not a screen.
    std::function<QString(const QString&)> screen_for;
};

/// Every reading of `text`, strongest first (may be empty). Pure.
QList<Phrase> parse(const QString& text, const Vocabulary& vocab = {});

/// Shaped like a market symbol: AAPL, BRK.B, RELIANCE.NS, ^GSPC, EURUSD=X, 7203.T, BTC-USD (1–15 chars, no spaces,
/// at least one letter or a digit-led exchange code).
bool is_ticker_like(const QString& token);
/// ISO 4217 codes the FX rules accept (majors + common EM).
bool is_currency(const QString& code);
/// Crypto asset codes the crypto rule accepts (BTC, ETH, SOL …).
bool is_crypto(const QString& code);
/// Crypto quote currencies (USDT, USDC, USD, EUR …).
bool is_crypto_quote(const QString& code);
/// The old bar's /types, in its order: stock fund dr index forex crypto futures bond economic.
QStringList legacy_asset_types();

} // namespace fincept::command::phrase
