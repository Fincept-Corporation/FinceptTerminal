#pragma once
// NewsSymbolDirectory — the local symbol / alias table the news tagger uses.
//
// The dictionary is built from what the app already knows, on a worker thread:
//   * the built-in list of large global companies and major crypto assets,
//   * every watchlist entry (symbol + stored name) and portfolio holding — these
//     are "user" symbols, matched even as bare two-letter words,
//   * news_symbol_aliases: names learned from symbol searches anywhere in the
//     app (MarketSearchService results are cached here as they arrive).
// Readers get an immutable snapshot (`current()`), safe on any thread — the RSS
// parse workers tag articles with it.

#include "services/news/NewsTickerTagger.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <memory>

namespace fincept::services {

class NewsSymbolDirectory : public QObject {
    Q_OBJECT
  public:
    static NewsSymbolDirectory& instance();

    /// The current dictionary; never null (the built-in list until the first
    /// load completes). Any thread.
    static std::shared_ptr<const news_tags::SymbolDictionary> current();

    /// Rebuild from the database on a worker. Coalesced (one rebuild per
    /// burst of requests). GUI thread.
    void request_reload();

    /// Watchlist + holding symbols of the last load (Yahoo form, upper case).
    QStringList user_symbols() const { return user_symbols_; }
    /// Watchlist symbols per list id (for the MY filter's list picker).
    QHash<QString, QStringList> watchlist_symbols() const { return watchlist_symbols_; }
    /// Watchlist id -> name, in sidebar order.
    QVector<QPair<QString, QString>> watchlists() const { return watchlists_; }

  signals:
    /// A new dictionary snapshot is in place (re-tag articles to pick it up).
    void dictionary_changed();

  private:
    NewsSymbolDirectory();
    /// Queue symbol / name pairs seen in a symbol search for persistence and
    /// merge them into the live dictionary.
    void learn(const QVector<QPair<QString, QString>>& symbol_names);
    void flush_learned();

    QTimer* reload_timer_ = nullptr;
    QTimer* learn_timer_ = nullptr;
    bool reload_running_ = false;
    bool reload_again_ = false;
    QVector<QPair<QString, QString>> pending_learned_;
    QSet<QString> learned_keys_; // "SYMBOL|name" already queued this session
    QStringList user_symbols_;
    QHash<QString, QStringList> watchlist_symbols_;
    QVector<QPair<QString, QString>> watchlists_;
};

} // namespace fincept::services
