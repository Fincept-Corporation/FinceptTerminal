#include "database.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace fincept::db {
namespace ops {

// Operations — Watchlist
// ============================================================================
Watchlist create_watchlist(const std::string& name, const std::string& color, const std::string& description) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string id = generate_uuid();
    auto stmt = db.prepare("INSERT INTO watchlists (id, name, description, color) VALUES (?1, ?2, ?3, ?4)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, name);
    stmt.bind_text(3, description);
    stmt.bind_text(4, color);
    stmt.execute();

    auto q = db.prepare("SELECT id, name, description, color, created_at, updated_at FROM watchlists WHERE id = ?1");
    q.bind_text(1, id);
    q.step();
    return {q.col_text(0), q.col_text(1), q.col_text_opt(2), q.col_text(3), q.col_text(4), q.col_text(5)};
}

std::vector<Watchlist> get_watchlists() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT id, name, description, color, created_at, updated_at FROM watchlists ORDER BY created_at DESC");
    std::vector<Watchlist> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text_opt(2),
                          stmt.col_text(3), stmt.col_text(4), stmt.col_text(5)});
    }
    return result;
}

void delete_watchlist(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM watchlists WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

WatchlistStock add_watchlist_stock(const std::string& watchlist_id, const std::string& symbol, const std::string& notes) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string id = generate_uuid();
    std::string upper_sym = symbol;
    std::transform(upper_sym.begin(), upper_sym.end(), upper_sym.begin(), ::toupper);
    auto stmt = db.prepare("INSERT INTO watchlist_stocks (id, watchlist_id, symbol, notes) VALUES (?1, ?2, ?3, ?4)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, watchlist_id);
    stmt.bind_text(3, upper_sym);
    stmt.bind_text(4, notes);
    stmt.execute();

    auto q = db.prepare("SELECT id, watchlist_id, symbol, added_at, notes FROM watchlist_stocks WHERE id = ?1");
    q.bind_text(1, id);
    q.step();
    return {q.col_text(0), q.col_text(1), q.col_text(2), q.col_text(3), q.col_text_opt(4)};
}

std::vector<WatchlistStock> get_watchlist_stocks(const std::string& watchlist_id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, watchlist_id, symbol, added_at, notes FROM watchlist_stocks "
        "WHERE watchlist_id = ?1 ORDER BY added_at DESC");
    stmt.bind_text(1, watchlist_id);
    std::vector<WatchlistStock> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3), stmt.col_text_opt(4)});
    }
    return result;
}

void remove_watchlist_stock(const std::string& watchlist_id, const std::string& symbol) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string upper_sym = symbol;
    std::transform(upper_sym.begin(), upper_sym.end(), upper_sym.begin(), ::toupper);
    auto stmt = db.prepare("DELETE FROM watchlist_stocks WHERE watchlist_id = ?1 AND symbol = ?2");
    stmt.bind_text(1, watchlist_id);
    stmt.bind_text(2, upper_sym);
    stmt.execute();
}

// ============================================================================
// Operations — M&A Deals
// ============================================================================
MADealRow create_ma_deal(const MADealRow& deal) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string id = deal.deal_id.empty() ? generate_uuid() : deal.deal_id;
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO ma_deals "
        "(deal_id, target_name, acquirer_name, deal_value, offer_price_per_share, "
        "premium_1day, payment_method, payment_cash_pct, payment_stock_pct, "
        "ev_revenue, ev_ebitda, synergies, status, deal_type, industry, "
        "announced_date, expected_close_date, breakup_fee, hostile_flag, "
        "tender_offer_flag, cross_border_flag, acquirer_country, target_country, "
        "created_at, updated_at) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,"
        "?16,?17,?18,?19,?20,?21,?22,?23,"
        "COALESCE((SELECT created_at FROM ma_deals WHERE deal_id=?1), datetime('now')),"
        "datetime('now'))");
    stmt.bind_text(1, id);
    stmt.bind_text(2, deal.target_name);
    stmt.bind_text(3, deal.acquirer_name);
    stmt.bind_double(4, deal.deal_value);
    stmt.bind_double(5, deal.offer_price_per_share);
    stmt.bind_double(6, deal.premium_1day);
    stmt.bind_text(7, deal.payment_method);
    stmt.bind_double(8, deal.payment_cash_pct);
    stmt.bind_double(9, deal.payment_stock_pct);
    stmt.bind_double(10, deal.ev_revenue);
    stmt.bind_double(11, deal.ev_ebitda);
    stmt.bind_double(12, deal.synergies);
    stmt.bind_text(13, deal.status);
    stmt.bind_text(14, deal.deal_type);
    stmt.bind_text(15, deal.industry);
    stmt.bind_text(16, deal.announced_date);
    stmt.bind_text(17, deal.expected_close_date);
    stmt.bind_double(18, deal.breakup_fee);
    stmt.bind_int(19, deal.hostile_flag);
    stmt.bind_int(20, deal.tender_offer_flag);
    stmt.bind_int(21, deal.cross_border_flag);
    stmt.bind_text(22, deal.acquirer_country);
    stmt.bind_text(23, deal.target_country);
    stmt.execute();

    MADealRow result = deal;
    result.deal_id = id;
    return result;
}

std::vector<MADealRow> get_all_ma_deals() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT * FROM ma_deals ORDER BY announced_date DESC");
    std::vector<MADealRow> deals;
    while (stmt.step()) {
        MADealRow d;
        d.deal_id              = stmt.col_text(0);
        d.target_name          = stmt.col_text(1);
        d.acquirer_name        = stmt.col_text(2);
        d.deal_value           = stmt.col_double(3);
        d.offer_price_per_share= stmt.col_double(4);
        d.premium_1day         = stmt.col_double(5);
        d.payment_method       = stmt.col_text(6);
        d.payment_cash_pct     = stmt.col_double(7);
        d.payment_stock_pct    = stmt.col_double(8);
        d.ev_revenue           = stmt.col_double(9);
        d.ev_ebitda            = stmt.col_double(10);
        d.synergies            = stmt.col_double(11);
        d.status               = stmt.col_text(12);
        d.deal_type            = stmt.col_text(13);
        d.industry             = stmt.col_text(14);
        d.announced_date       = stmt.col_text(15);
        d.expected_close_date  = stmt.col_text(16);
        d.breakup_fee          = stmt.col_double(17);
        d.hostile_flag         = static_cast<int>(stmt.col_int(18));
        d.tender_offer_flag    = static_cast<int>(stmt.col_int(19));
        d.cross_border_flag    = static_cast<int>(stmt.col_int(20));
        d.acquirer_country     = stmt.col_text(21);
        d.target_country       = stmt.col_text(22);
        d.created_at           = stmt.col_text(23);
        d.updated_at           = stmt.col_text(24);
        deals.push_back(std::move(d));
    }
    return deals;
}

std::vector<MADealRow> search_ma_deals(const std::string& query) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string pattern = "%" + query + "%";
    auto stmt = db.prepare(
        "SELECT * FROM ma_deals WHERE "
        "target_name LIKE ?1 OR acquirer_name LIKE ?1 OR industry LIKE ?1 OR deal_type LIKE ?1 "
        "ORDER BY announced_date DESC");
    stmt.bind_text(1, pattern);
    std::vector<MADealRow> deals;
    while (stmt.step()) {
        MADealRow d;
        d.deal_id              = stmt.col_text(0);
        d.target_name          = stmt.col_text(1);
        d.acquirer_name        = stmt.col_text(2);
        d.deal_value           = stmt.col_double(3);
        d.offer_price_per_share= stmt.col_double(4);
        d.premium_1day         = stmt.col_double(5);
        d.payment_method       = stmt.col_text(6);
        d.payment_cash_pct     = stmt.col_double(7);
        d.payment_stock_pct    = stmt.col_double(8);
        d.ev_revenue           = stmt.col_double(9);
        d.ev_ebitda            = stmt.col_double(10);
        d.synergies            = stmt.col_double(11);
        d.status               = stmt.col_text(12);
        d.deal_type            = stmt.col_text(13);
        d.industry             = stmt.col_text(14);
        d.announced_date       = stmt.col_text(15);
        d.expected_close_date  = stmt.col_text(16);
        d.breakup_fee          = stmt.col_double(17);
        d.hostile_flag         = static_cast<int>(stmt.col_int(18));
        d.tender_offer_flag    = static_cast<int>(stmt.col_int(19));
        d.cross_border_flag    = static_cast<int>(stmt.col_int(20));
        d.acquirer_country     = stmt.col_text(21);
        d.target_country       = stmt.col_text(22);
        d.created_at           = stmt.col_text(23);
        d.updated_at           = stmt.col_text(24);
        deals.push_back(std::move(d));
    }
    return deals;
}

void update_ma_deal(const MADealRow& deal) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE ma_deals SET target_name=?2, acquirer_name=?3, deal_value=?4, "
        "offer_price_per_share=?5, premium_1day=?6, payment_method=?7, "
        "payment_cash_pct=?8, payment_stock_pct=?9, ev_revenue=?10, ev_ebitda=?11, "
        "synergies=?12, status=?13, deal_type=?14, industry=?15, announced_date=?16, "
        "expected_close_date=?17, breakup_fee=?18, hostile_flag=?19, tender_offer_flag=?20, "
        "cross_border_flag=?21, acquirer_country=?22, target_country=?23, "
        "updated_at=datetime('now') WHERE deal_id=?1");
    stmt.bind_text(1, deal.deal_id);
    stmt.bind_text(2, deal.target_name);
    stmt.bind_text(3, deal.acquirer_name);
    stmt.bind_double(4, deal.deal_value);
    stmt.bind_double(5, deal.offer_price_per_share);
    stmt.bind_double(6, deal.premium_1day);
    stmt.bind_text(7, deal.payment_method);
    stmt.bind_double(8, deal.payment_cash_pct);
    stmt.bind_double(9, deal.payment_stock_pct);
    stmt.bind_double(10, deal.ev_revenue);
    stmt.bind_double(11, deal.ev_ebitda);
    stmt.bind_double(12, deal.synergies);
    stmt.bind_text(13, deal.status);
    stmt.bind_text(14, deal.deal_type);
    stmt.bind_text(15, deal.industry);
    stmt.bind_text(16, deal.announced_date);
    stmt.bind_text(17, deal.expected_close_date);
    stmt.bind_double(18, deal.breakup_fee);
    stmt.bind_int(19, deal.hostile_flag);
    stmt.bind_int(20, deal.tender_offer_flag);
    stmt.bind_int(21, deal.cross_border_flag);
    stmt.bind_text(22, deal.acquirer_country);
    stmt.bind_text(23, deal.target_country);
    stmt.execute();
}

void delete_ma_deal(const std::string& deal_id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM ma_deals WHERE deal_id = ?1");
    stmt.bind_text(1, deal_id);
    stmt.execute();
}

// ============================================================================

} // namespace ops
} // namespace fincept::db
