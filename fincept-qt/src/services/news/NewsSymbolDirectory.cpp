// NewsSymbolDirectory.cpp — see NewsSymbolDirectory.h.

#include "services/news/NewsSymbolDirectory.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/markets/MarketSearchService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/NewsAnalyticsRepository.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QtConcurrent>

#include <utility>

namespace fincept::services {

namespace {

QMutex g_nsd_mutex;
std::shared_ptr<const news_tags::SymbolDictionary> g_nsd_current; // guarded by g_nsd_mutex

void nsd_publish(std::shared_ptr<const news_tags::SymbolDictionary> dict) {
    QMutexLocker lock(&g_nsd_mutex);
    g_nsd_current = std::move(dict);
}

struct NsdLoad {
    std::shared_ptr<const news_tags::SymbolDictionary> dict;
    QStringList user_symbols;
    QHash<QString, QStringList> watchlist_symbols;
    QVector<QPair<QString, QString>> watchlists;
};

/// Worker half of a reload: every source read here runs on a pooled thread
/// (Database hands out a per-thread connection).
NsdLoad nsd_build() {
    NsdLoad out;
    auto dict = std::make_shared<news_tags::SymbolDictionary>(news_tags::SymbolDictionary::builtin());

    // Watchlists first: they are the user's own symbols (and carry names).
    QSet<QString> user;
    auto& wl_repo = fincept::WatchlistRepository::instance();
    if (auto lists = wl_repo.list_all(); lists.is_ok()) {
        for (const auto& wl : lists.value()) {
            out.watchlists.append({wl.id, wl.name});
            auto stocks = wl_repo.get_stocks(wl.id);
            if (stocks.is_err())
                continue;
            QStringList syms;
            for (const auto& st : stocks.value()) {
                QString symbol = st.symbol.trimmed().toUpper();
                if (symbol.isEmpty())
                    continue;
                // A bare listing code with an exchange ("RELIANCE" on NSE) is
                // registered in Yahoo form (shared venue table) so suffixed
                // mentions resolve to it; an unknown venue keeps the code.
                if (!symbol.contains(QLatin1Char('.')) && !st.exchange.isEmpty()) {
                    const QString yahoo = news_tags::yahoo_for_exchange_code(st.exchange, symbol);
                    if (!yahoo.isEmpty())
                        symbol = yahoo;
                }
                dict->add_symbol(symbol, st.name, true);
                syms.append(symbol);
                user.insert(symbol);
            }
            out.watchlist_symbols.insert(wl.id, syms);
        }
    }
    for (const QString& s : PortfolioService::held_symbols()) {
        const QString symbol = s.trimmed().toUpper();
        if (symbol.isEmpty())
            continue;
        dict->add_symbol(symbol, {}, true);
        user.insert(symbol);
    }
    // Names learned from symbol searches (most recent first).
    if (auto aliases = fincept::NewsAnalyticsRepository::instance().load_aliases(); aliases.is_ok()) {
        for (const auto& row : aliases.value())
            dict->add_symbol(row.symbol, row.name.isEmpty() ? row.alias : row.name,
                             row.source == QLatin1String("watchlist") || row.source == QLatin1String("portfolio"));
    }
    out.user_symbols = QStringList(user.begin(), user.end());
    out.user_symbols.sort();
    out.dict = std::move(dict);
    return out;
}

} // namespace

NewsSymbolDirectory& NewsSymbolDirectory::instance() {
    static NewsSymbolDirectory s;
    return s;
}

std::shared_ptr<const news_tags::SymbolDictionary> NewsSymbolDirectory::current() {
    {
        QMutexLocker lock(&g_nsd_mutex);
        if (g_nsd_current)
            return g_nsd_current;
    }
    // First use (possibly on a parse worker) before any load: the built-in list.
    auto dict = std::make_shared<const news_tags::SymbolDictionary>(news_tags::SymbolDictionary::builtin());
    QMutexLocker lock(&g_nsd_mutex);
    if (!g_nsd_current)
        g_nsd_current = dict;
    return g_nsd_current;
}

NewsSymbolDirectory::NewsSymbolDirectory() {
    reload_timer_ = new QTimer(this);
    reload_timer_->setSingleShot(true);
    reload_timer_->setInterval(1500);
    connect(reload_timer_, &QTimer::timeout, this, [this]() {
        if (reload_running_) {
            reload_again_ = true;
            return;
        }
        reload_running_ = true;
        QPointer<NewsSymbolDirectory> self = this;
        (void)QtConcurrent::run([self]() {
            NsdLoad load = nsd_build();
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, load = std::move(load)]() mutable {
                    if (!self)
                        return;
                    self->reload_running_ = false;
                    nsd_publish(load.dict);
                    self->user_symbols_ = std::move(load.user_symbols);
                    self->watchlist_symbols_ = std::move(load.watchlist_symbols);
                    self->watchlists_ = std::move(load.watchlists);
                    LOG_DEBUG("NewsSymbolDirectory", QString("Dictionary: %1 symbols, %2 aliases, %3 user symbols")
                                                         .arg(load.dict->symbol_count())
                                                         .arg(load.dict->alias_count())
                                                         .arg(self->user_symbols_.size()));
                    emit self->dictionary_changed();
                    if (self->reload_again_) {
                        self->reload_again_ = false;
                        self->reload_timer_->start();
                    }
                },
                Qt::QueuedConnection);
        });
    });

    learn_timer_ = new QTimer(this);
    learn_timer_->setSingleShot(true);
    learn_timer_->setInterval(5000);
    connect(learn_timer_, &QTimer::timeout, this, &NewsSymbolDirectory::flush_learned);

    // Symbol searches anywhere in the app teach the directory company names.
    connect(&MarketSearchService::instance(), &MarketSearchService::results_ready, this,
            [this](const QString&, const QString&, const QList<MarketSearchService::Item>& items) {
                QVector<QPair<QString, QString>> pairs;
                for (const auto& it : items) {
                    const QString symbol = (it.yf_symbol.isEmpty() ? it.symbol : it.yf_symbol).trimmed().toUpper();
                    const QString type = it.type.toLower();
                    if (symbol.isEmpty() || it.name.trimmed().isEmpty())
                        continue;
                    if (!type.isEmpty() && type != QLatin1String("stock") && type != QLatin1String("equity") &&
                        type != QLatin1String("etf") && type != QLatin1String("crypto"))
                        continue;
                    pairs.append({symbol, it.name.trimmed()});
                    if (pairs.size() >= 20)
                        break;
                }
                if (!pairs.isEmpty())
                    learn(pairs);
            });

    // Watchlist / portfolio edits change the user's own symbols.
    EventBus::instance().subscribe(this, QStringLiteral("watchlist.updated"),
                                   [this](const QVariantMap&) {
                                       QMetaObject::invokeMethod(this, [this]() { request_reload(); },
                                                                 Qt::QueuedConnection);
                                   });
    auto& psvc = PortfolioService::instance();
    auto on_holdings = [this](const QString&) { request_reload(); };
    connect(&psvc, &PortfolioService::asset_added, this, on_holdings);
    connect(&psvc, &PortfolioService::asset_sold, this, on_holdings);
    connect(&psvc, &PortfolioService::portfolio_deleted, this, on_holdings);
}

void NewsSymbolDirectory::request_reload() {
    reload_timer_->start();
}

void NewsSymbolDirectory::learn(const QVector<QPair<QString, QString>>& symbol_names) {
    for (const auto& [symbol, name] : symbol_names) {
        const QString key = symbol + QLatin1Char('|') + name;
        if (learned_keys_.contains(key))
            continue;
        learned_keys_.insert(key);
        pending_learned_.append({symbol, name});
    }
    if (!pending_learned_.isEmpty())
        learn_timer_->start();
}

void NewsSymbolDirectory::flush_learned() {
    if (pending_learned_.isEmpty())
        return;
    const auto batch = std::exchange(pending_learned_, {});
    // Merge into the live dictionary at once (a copy: snapshots are immutable).
    auto next = std::make_shared<news_tags::SymbolDictionary>(*current());
    for (const auto& [symbol, name] : batch)
        next->add_symbol(symbol, name, false);
    nsd_publish(next);
    emit dictionary_changed();

    QVector<fincept::NewsSymbolAliasRow> rows;
    rows.reserve(batch.size());
    for (const auto& [symbol, name] : batch)
        rows.append({news_tags::normalize_company_name(name), symbol, name, QStringLiteral("search")});
    (void)QtConcurrent::run([rows]() {
        auto r = fincept::NewsAnalyticsRepository::instance().upsert_aliases(rows);
        if (r.is_err())
            LOG_WARN("NewsSymbolDirectory", "Persisting learned aliases failed: " + QString::fromStdString(r.error()));
    });
}

} // namespace fincept::services
