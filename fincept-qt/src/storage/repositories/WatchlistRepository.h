#pragma once
#include "storage/repositories/BaseRepository.h"

#include <QUuid>

namespace fincept {

struct Watchlist {
    QString id;
    QString name;
    QString description;
    QString color;
    int sort_order = 0;
    bool is_default = false;
    QString created_at;
    QString updated_at;
};

struct WatchlistStock {
    int id = 0;
    QString watchlist_id;
    QString symbol;
    QString name;
    QString exchange;
    QString notes;
    int sort_order = 0;
    QString added_at;
};

class WatchlistRepository : public BaseRepository<Watchlist> {
  public:
    static WatchlistRepository& instance();

    // Watchlist CRUD
    Result<Watchlist> create(const QString& name, const QString& color = "#FF6600");
    Result<QVector<Watchlist>> list_all();
    Result<Watchlist> get(const QString& id);
    Result<void> update(const Watchlist& w);
    Result<void> remove(const QString& id);

    // Stock items
    Result<void> add_stock(const QString& watchlist_id, const QString& symbol, const QString& name = {},
                           const QString& exchange = {});
    Result<void> remove_stock(const QString& watchlist_id, const QString& symbol);
    Result<QVector<WatchlistStock>> get_stocks(const QString& watchlist_id);

  private:
    WatchlistRepository() = default;
    static Watchlist map_watchlist(QSqlQuery& q);
    static WatchlistStock map_stock(QSqlQuery& q);
};

} // namespace fincept
