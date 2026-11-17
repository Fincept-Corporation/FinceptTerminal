#pragma once
// NewsTickerTagger — validated ticker tagging for news text.
//
// A tag is only produced for something the text actually names as a security:
//   * cashtags ($AAPL, $BTC, $RELIANCE.NS),
//   * exchange-qualified mentions (NSE:RELIANCE, (NASDAQ: AAPL), HKG: 700, TYO:7203),
//   * Yahoo-style suffixed codes (RELIANCE.NS, 7203.T, 0700.HK, 600519.SS),
//   * company names / aliases from the symbol dictionary (Toyota, Tencent,
//     腾讯, Samsung Electronics, Bank of America ...),
//   * bare upper-case symbols the dictionary knows (the user's watchlists and
//     holdings, symbols resolved before, the built-in list).
// Capitalised words that merely look like tickers are NOT tags; they are
// reported separately as "unverified".
//
// SymbolDictionary is a plain value: build one, then share it read-only across
// threads (NewsSymbolDirectory hands out immutable snapshots). Pure Qt Core.

#include <QHash>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::news_tags {

enum class TagSource {
    Cashtag,  ///< $SYM
    Exchange, ///< NSE:SYM, (NYSE: SYM), 7203.T
    Alias,    ///< a company name the dictionary maps to a symbol
    Symbol,   ///< a bare upper-case symbol the dictionary knows
};

struct TickerTag {
    QString symbol;  ///< canonical (Yahoo-style) symbol, e.g. "7203.T", "AAPL", "BTC-USD"
    QString matched; ///< the text that produced it
    TagSource source = TagSource::Symbol;
    double confidence = 0.0; ///< 0..1
    int position = 0;        ///< first occurrence in headline + summary
};

/// Display text of a tag source ("cashtag", "exchange code", "company name", "symbol").
QString tag_source_label(TagSource source);

/// Name with legal-form suffixes and share-class noise removed:
/// "Apple Inc." -> "Apple", "Toyota Motor Corporation" -> "Toyota Motor",
/// "HSBC Holdings plc" -> "HSBC".
QString normalize_company_name(const QString& name);

/// Yahoo symbol of a listing code on an exchange named in news text or stored
/// with a watchlist row, through the shared venue table
/// (core::instrument::to_yahoo): ("NSE", "RELIANCE") -> "RELIANCE.NS",
/// ("HKG", "700") -> "0700.HK", ("TPE", "50") -> "0050.TW",
/// ("NASDAQ", "BRK.B") -> "BRK-B". Empty when the exchange is unknown or Yahoo
/// carries no data for it — never the bare code, which on Yahoo would be a US
/// listing of another company.
QString yahoo_for_exchange_code(const QString& exchange, const QString& code);

class SymbolDictionary {
  public:
    /// The compact built-in list of large global companies and major crypto assets.
    static SymbolDictionary builtin();

    /// Register a tradable symbol (Yahoo form). A non-empty `name` also becomes
    /// an alias (normalised; single common words are marked ambiguous).
    /// `user_owned` symbols (watchlists, holdings) are matched as bare
    /// two-letter words too; others need three letters or more.
    void add_symbol(const QString& symbol, const QString& name = {}, bool user_owned = false);
    /// Map an alias (company name, local-script name) to a symbol. Ambiguous
    /// aliases ("Visa", "Shell", "Target") only tag with finance context nearby.
    void add_alias(const QString& alias, const QString& symbol, bool ambiguous = false);

    /// Canonical symbol for an upper-case token or full symbol ("AAPL",
    /// "RELIANCE", "RELIANCE.NS", "BTC"); empty when unknown.
    QString resolve(const QString& token) const;
    bool is_user_symbol(const QString& base) const { return user_bases_.contains(base); }
    int alias_count() const { return static_cast<int>(aliases_.size() + script_aliases_.size()); }
    int symbol_count() const { return static_cast<int>(symbols_.size()); }

    struct AliasInfo {
        QString symbol;
        bool ambiguous = false;
    };
    /// Normalised Latin alias (lower case, words joined by one space) -> symbol.
    const QHash<QString, AliasInfo>& latin_aliases() const { return aliases_; }
    /// Non-Latin aliases (matched as substrings) and their symbols.
    const QVector<QPair<QString, QString>>& script_aliases() const { return script_aliases_; }
    /// Longest Latin alias, in words.
    int max_alias_words() const { return max_alias_words_; }

  private:
    QHash<QString, AliasInfo> aliases_;               // normalised Latin alias -> symbol
    QVector<QPair<QString, QString>> script_aliases_; // non-Latin alias (substring match) -> symbol
    QHash<QString, QString> base_to_symbol_;          // "AAPL" / "RELIANCE" / "RELIANCE.NS" -> symbol
    QSet<QString> symbols_;
    QSet<QString> user_bases_;
    int max_alias_words_ = 1;
};

/// Tags in `headline` + `summary`, most confident first (ties: first mention),
/// one per symbol, at most `max_tags`. `unverified` (optional) receives up to
/// three capitalised tokens that look like symbols but matched nothing known.
QVector<TickerTag> tag(const QString& headline, const QString& summary, const SymbolDictionary& dict,
                       QStringList* unverified = nullptr, int max_tags = 6);

/// Tags at or above this confidence go into NewsArticle::tickers.
constexpr double kVerifiedTagConfidence = 0.6;

} // namespace fincept::services::news_tags
