#include "database.h"
#include "screens/algo_trading/algo_types.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace fincept::db {
namespace ops {

// Operations — Notes
// ============================================================================

static int64_t count_words(const std::string& text) {
    int64_t count = 0;
    bool in_word = false;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++count;
        }
    }
    return count;
}

static Note read_note_row(Statement& stmt) {
    Note n;
    n.id           = stmt.col_int(0);
    n.title        = stmt.col_text(1);
    n.content      = stmt.col_text(2);
    n.category     = stmt.col_text(3);
    n.priority     = stmt.col_text(4);
    n.tags         = stmt.col_text(5);
    n.tickers      = stmt.col_text(6);
    n.sentiment    = stmt.col_text(7);
    n.is_favorite  = stmt.col_bool(8);
    n.is_archived  = stmt.col_bool(9);
    n.color_code   = stmt.col_text(10);
    n.created_at   = stmt.col_text(11);
    n.updated_at   = stmt.col_text(12);
    n.reminder_date = stmt.col_text_opt(13);
    n.word_count   = stmt.col_int(14);
    return n;
}

static const char* NOTE_SELECT_COLS =
    "id, title, content, category, priority, tags, tickers, sentiment, "
    "is_favorite, is_archived, color_code, created_at, updated_at, reminder_date, word_count";

Note create_note(const Note& note) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t wc = count_words(note.content);
    auto stmt = db.prepare(
        "INSERT INTO notes (title, content, category, priority, tags, tickers, sentiment, "
        "is_favorite, is_archived, color_code, reminder_date, word_count) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)");
    stmt.bind_text(1, note.title);
    stmt.bind_text(2, note.content);
    stmt.bind_text(3, note.category);
    stmt.bind_text(4, note.priority);
    stmt.bind_text(5, note.tags);
    stmt.bind_text(6, note.tickers);
    stmt.bind_text(7, note.sentiment);
    stmt.bind_int(8, note.is_favorite ? 1 : 0);
    stmt.bind_int(9, note.is_archived ? 1 : 0);
    stmt.bind_text(10, note.color_code.empty() ? "#FF8C00" : note.color_code);
    if (note.reminder_date) stmt.bind_text(11, *note.reminder_date); else stmt.bind_null(11);
    stmt.bind_int(12, wc);
    stmt.execute();

    // Retrieve the inserted row
    auto q = db.prepare((std::string("SELECT ") + NOTE_SELECT_COLS +
                         " FROM notes WHERE id = last_insert_rowid()").c_str());
    q.step();
    return read_note_row(q);
}

std::vector<Note> get_all_notes() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare((std::string("SELECT ") + NOTE_SELECT_COLS +
                            " FROM notes ORDER BY updated_at DESC").c_str());
    std::vector<Note> result;
    while (stmt.step()) {
        result.push_back(read_note_row(stmt));
    }
    return result;
}

void update_note(const Note& note) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t wc = count_words(note.content);
    auto stmt = db.prepare(
        "UPDATE notes SET title=?1, content=?2, category=?3, priority=?4, "
        "tags=?5, tickers=?6, sentiment=?7, is_favorite=?8, is_archived=?9, "
        "color_code=?10, reminder_date=?11, word_count=?12, updated_at=CURRENT_TIMESTAMP "
        "WHERE id=?13");
    stmt.bind_text(1, note.title);
    stmt.bind_text(2, note.content);
    stmt.bind_text(3, note.category);
    stmt.bind_text(4, note.priority);
    stmt.bind_text(5, note.tags);
    stmt.bind_text(6, note.tickers);
    stmt.bind_text(7, note.sentiment);
    stmt.bind_int(8, note.is_favorite ? 1 : 0);
    stmt.bind_int(9, note.is_archived ? 1 : 0);
    stmt.bind_text(10, note.color_code);
    if (note.reminder_date) stmt.bind_text(11, *note.reminder_date); else stmt.bind_null(11);
    stmt.bind_int(12, wc);
    stmt.bind_int(13, note.id);
    stmt.execute();
}

void delete_note(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM notes WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

std::vector<Note> search_notes(const std::string& query) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string like = "%" + query + "%";
    auto stmt = db.prepare((std::string("SELECT ") + NOTE_SELECT_COLS +
        " FROM notes WHERE title LIKE ?1 OR content LIKE ?1 OR tags LIKE ?1 "
        "OR tickers LIKE ?1 ORDER BY updated_at DESC").c_str());
    stmt.bind_text(1, like);
    std::vector<Note> result;
    while (stmt.step()) {
        result.push_back(read_note_row(stmt));
    }
    return result;
}

void toggle_note_favorite(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE notes SET is_favorite = NOT is_favorite, updated_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

void archive_note(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE notes SET is_archived = NOT is_archived, updated_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

// ============================================================================
// Algo Trading Operations
// ============================================================================

void save_algo_strategy(const algo::AlgoStrategy& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_strategies
        (id, name, description, symbols, entry_conditions, exit_conditions,
         entry_logic, exit_logic, timeframe, stop_loss, take_profit, trailing_stop,
         position_size, max_positions, provider, is_active, updated_at)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, s.id);
    stmt.bind_text(2, s.name);
    stmt.bind_text(3, s.description);
    stmt.bind_text(4, s.symbols);
    stmt.bind_text(5, s.entry_conditions);
    stmt.bind_text(6, s.exit_conditions);
    stmt.bind_text(7, s.entry_logic);
    stmt.bind_text(8, s.exit_logic);
    stmt.bind_text(9, s.timeframe);
    stmt.bind_double(10, s.stop_loss);
    stmt.bind_double(11, s.take_profit);
    stmt.bind_double(12, s.trailing_stop);
    stmt.bind_double(13, s.position_size);
    stmt.bind_int(14, s.max_positions);
    stmt.bind_text(15, s.provider);
    stmt.bind_int(16, s.is_active ? 1 : 0);
    stmt.execute();
}

static algo::AlgoStrategy read_algo_strategy_row(Statement& stmt) {
    algo::AlgoStrategy s;
    s.id               = stmt.col_text(0);
    s.name             = stmt.col_text(1);
    s.description      = stmt.col_text(2);
    s.symbols          = stmt.col_text(3);
    s.entry_conditions = stmt.col_text(4);
    s.exit_conditions  = stmt.col_text(5);
    s.entry_logic      = stmt.col_text(6);
    s.exit_logic       = stmt.col_text(7);
    s.timeframe        = stmt.col_text(8);
    s.stop_loss        = stmt.col_double(9);
    s.take_profit      = stmt.col_double(10);
    s.trailing_stop    = stmt.col_double(11);
    s.position_size    = stmt.col_double(12);
    s.max_positions    = (int)stmt.col_int(13);
    s.provider         = stmt.col_text(14);
    s.is_active        = stmt.col_bool(15);
    s.created_at       = stmt.col_text(16);
    s.updated_at       = stmt.col_text(17);
    return s;
}

std::vector<algo::AlgoStrategy> list_algo_strategies() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, symbols, entry_conditions, exit_conditions, "
        "entry_logic, exit_logic, timeframe, stop_loss, take_profit, trailing_stop, "
        "position_size, max_positions, provider, is_active, created_at, updated_at "
        "FROM algo_strategies ORDER BY updated_at DESC");
    std::vector<algo::AlgoStrategy> result;
    while (stmt.step()) result.push_back(read_algo_strategy_row(stmt));
    return result;
}

std::optional<algo::AlgoStrategy> get_algo_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, symbols, entry_conditions, exit_conditions, "
        "entry_logic, exit_logic, timeframe, stop_loss, take_profit, trailing_stop, "
        "position_size, max_positions, provider, is_active, created_at, updated_at "
        "FROM algo_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_algo_strategy_row(stmt);
    return std::nullopt;
}

void delete_algo_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM algo_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- Deployments ---

void save_algo_deployment(const algo::AlgoDeployment& d) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_deployments
        (id, strategy_id, strategy_name, mode, status, pid, portfolio_id,
         started_at, stopped_at, error_message, config)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)
    )");
    stmt.bind_text(1, d.id);
    stmt.bind_text(2, d.strategy_id);
    stmt.bind_text(3, d.strategy_name);
    stmt.bind_text(4, d.mode);
    stmt.bind_text(5, algo::deployment_status_str(d.status));
    stmt.bind_int(6, d.pid);
    stmt.bind_text(7, d.portfolio_id);
    stmt.bind_text(8, d.started_at);
    stmt.bind_text(9, d.stopped_at);
    stmt.bind_text(10, d.error_message);
    stmt.bind_text(11, d.config);
    stmt.execute();
}

static algo::AlgoDeployment read_algo_deployment_row(Statement& stmt) {
    algo::AlgoDeployment d;
    d.id            = stmt.col_text(0);
    d.strategy_id   = stmt.col_text(1);
    d.strategy_name = stmt.col_text(2);
    d.mode          = stmt.col_text(3);
    d.status        = algo::parse_deployment_status(stmt.col_text(4));
    d.pid           = (int)stmt.col_int(5);
    d.portfolio_id  = stmt.col_text(6);
    d.started_at    = stmt.col_text(7);
    d.stopped_at    = stmt.col_text(8);
    d.error_message = stmt.col_text(9);
    d.config        = stmt.col_text(10);
    return d;
}

std::vector<algo::AlgoDeployment> list_algo_deployments() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, strategy_id, strategy_name, mode, status, pid, portfolio_id, "
        "started_at, stopped_at, error_message, config "
        "FROM algo_deployments ORDER BY started_at DESC");
    std::vector<algo::AlgoDeployment> result;
    while (stmt.step()) result.push_back(read_algo_deployment_row(stmt));
    return result;
}

std::optional<algo::AlgoDeployment> get_algo_deployment(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, strategy_id, strategy_name, mode, status, pid, portfolio_id, "
        "started_at, stopped_at, error_message, config "
        "FROM algo_deployments WHERE id = ?1");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_algo_deployment_row(stmt);
    return std::nullopt;
}

void update_algo_deployment_status(const std::string& id, const std::string& status,
                                         const std::string& error) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    if (status == "stopped" || status == "error" || status == "completed") {
        auto stmt = db.prepare(
            "UPDATE algo_deployments SET status = ?2, error_message = ?3, "
            "stopped_at = CURRENT_TIMESTAMP WHERE id = ?1");
        stmt.bind_text(1, id);
        stmt.bind_text(2, status);
        stmt.bind_text(3, error);
        stmt.execute();
    } else {
        auto stmt = db.prepare(
            "UPDATE algo_deployments SET status = ?2, error_message = ?3 WHERE id = ?1");
        stmt.bind_text(1, id);
        stmt.bind_text(2, status);
        stmt.bind_text(3, error);
        stmt.execute();
    }
}

void delete_algo_deployment(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    // Cascade: delete trades, metrics, signals first
    auto s1 = db.prepare("DELETE FROM algo_trades WHERE deployment_id = ?1");
    s1.bind_text(1, id); s1.execute();
    auto s2 = db.prepare("DELETE FROM algo_metrics WHERE deployment_id = ?1");
    s2.bind_text(1, id); s2.execute();
    auto s3 = db.prepare("DELETE FROM algo_order_signals WHERE deployment_id = ?1");
    s3.bind_text(1, id); s3.execute();
    auto s4 = db.prepare("DELETE FROM algo_deployments WHERE id = ?1");
    s4.bind_text(1, id); s4.execute();
}

// --- Metrics ---

void save_algo_metrics(const algo::AlgoMetrics& m) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_metrics
        (deployment_id, total_trades, winning_trades, losing_trades, total_pnl,
         max_drawdown, win_rate, avg_win, avg_loss, sharpe_ratio, updated_at)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, m.deployment_id);
    stmt.bind_int(2, m.total_trades);
    stmt.bind_int(3, m.winning_trades);
    stmt.bind_int(4, m.losing_trades);
    stmt.bind_double(5, m.total_pnl);
    stmt.bind_double(6, m.max_drawdown);
    stmt.bind_double(7, m.win_rate);
    stmt.bind_double(8, m.avg_win);
    stmt.bind_double(9, m.avg_loss);
    stmt.bind_double(10, m.sharpe_ratio);
    stmt.execute();
}

std::optional<algo::AlgoMetrics> get_algo_metrics(const std::string& deployment_id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT deployment_id, total_trades, winning_trades, losing_trades, total_pnl, "
        "max_drawdown, win_rate, avg_win, avg_loss, sharpe_ratio, updated_at "
        "FROM algo_metrics WHERE deployment_id = ?1");
    stmt.bind_text(1, deployment_id);
    if (!stmt.step()) return std::nullopt;
    algo::AlgoMetrics m;
    m.deployment_id = stmt.col_text(0);
    m.total_trades  = (int)stmt.col_int(1);
    m.winning_trades = (int)stmt.col_int(2);
    m.losing_trades  = (int)stmt.col_int(3);
    m.total_pnl     = stmt.col_double(4);
    m.max_drawdown  = stmt.col_double(5);
    m.win_rate      = stmt.col_double(6);
    m.avg_win       = stmt.col_double(7);
    m.avg_loss      = stmt.col_double(8);
    m.sharpe_ratio  = stmt.col_double(9);
    m.updated_at    = stmt.col_text(10);
    return m;
}

// --- Trades ---

void save_algo_trade(const algo::AlgoTrade& t) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_trades
        (id, deployment_id, symbol, side, quantity, price, pnl, fees, order_type, status, executed_at)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)
    )");
    stmt.bind_text(1, t.id);
    stmt.bind_text(2, t.deployment_id);
    stmt.bind_text(3, t.symbol);
    stmt.bind_text(4, t.side);
    stmt.bind_double(5, t.quantity);
    stmt.bind_double(6, t.price);
    stmt.bind_double(7, t.pnl);
    stmt.bind_double(8, t.fees);
    stmt.bind_text(9, t.order_type);
    stmt.bind_text(10, t.status);
    stmt.bind_text(11, t.executed_at);
    stmt.execute();
}

std::vector<algo::AlgoTrade> get_algo_trades(const std::string& deployment_id, int limit) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, deployment_id, symbol, side, quantity, price, pnl, fees, "
        "order_type, status, executed_at FROM algo_trades "
        "WHERE deployment_id = ?1 ORDER BY executed_at DESC LIMIT ?2");
    stmt.bind_text(1, deployment_id);
    stmt.bind_int(2, limit);
    std::vector<algo::AlgoTrade> result;
    while (stmt.step()) {
        algo::AlgoTrade t;
        t.id            = stmt.col_text(0);
        t.deployment_id = stmt.col_text(1);
        t.symbol        = stmt.col_text(2);
        t.side          = stmt.col_text(3);
        t.quantity      = stmt.col_double(4);
        t.price         = stmt.col_double(5);
        t.pnl           = stmt.col_double(6);
        t.fees          = stmt.col_double(7);
        t.order_type    = stmt.col_text(8);
        t.status        = stmt.col_text(9);
        t.executed_at   = stmt.col_text(10);
        result.push_back(t);
    }
    return result;
}

// --- Order Signals ---

void save_algo_signal(const algo::AlgoOrderSignal& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT INTO algo_order_signals
        (id, deployment_id, symbol, side, quantity, order_type, price, stop_price, status)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)
    )");
    stmt.bind_text(1, s.id);
    stmt.bind_text(2, s.deployment_id);
    stmt.bind_text(3, s.symbol);
    stmt.bind_text(4, s.side);
    stmt.bind_double(5, s.quantity);
    stmt.bind_text(6, s.order_type);
    stmt.bind_double(7, s.price);
    stmt.bind_double(8, s.stop_price);
    stmt.bind_text(9, s.status);
    stmt.execute();
}

std::vector<algo::AlgoOrderSignal> get_pending_signals() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT s.id, s.deployment_id, s.symbol, s.side, s.quantity, s.order_type, "
        "s.price, s.stop_price, s.status, s.created_at, s.executed_at, s.error "
        "FROM algo_order_signals s "
        "INNER JOIN algo_deployments d ON s.deployment_id = d.id "
        "WHERE s.status = 'pending' AND d.status = 'running' "
        "ORDER BY s.created_at ASC");
    std::vector<algo::AlgoOrderSignal> result;
    while (stmt.step()) {
        algo::AlgoOrderSignal sig;
        sig.id            = stmt.col_text(0);
        sig.deployment_id = stmt.col_text(1);
        sig.symbol        = stmt.col_text(2);
        sig.side          = stmt.col_text(3);
        sig.quantity      = stmt.col_double(4);
        sig.order_type    = stmt.col_text(5);
        sig.price         = stmt.col_double(6);
        sig.stop_price    = stmt.col_double(7);
        sig.status        = stmt.col_text(8);
        sig.created_at    = stmt.col_text(9);
        sig.executed_at   = stmt.col_text(10);
        sig.error         = stmt.col_text(11);
        result.push_back(sig);
    }
    return result;
}

void update_signal_status(const std::string& id, const std::string& status,
                                const std::string& error) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE algo_order_signals SET status = ?2, error = ?3, "
        "executed_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.bind_text(2, status);
    stmt.bind_text(3, error);
    stmt.execute();
}

// --- Candle Cache ---

void insert_candles(const std::vector<algo::CandleData>& candles) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    db.begin_transaction();
    try {
        for (auto& c : candles) {
            auto stmt = db.prepare(R"(
                INSERT OR IGNORE INTO candle_cache
                (symbol, timeframe, timestamp, open, high, low, close, volume, provider)
                VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)
            )");
            stmt.bind_text(1, c.symbol);
            stmt.bind_text(2, c.timeframe);
            stmt.bind_int(3, c.timestamp);
            stmt.bind_double(4, c.open);
            stmt.bind_double(5, c.high);
            stmt.bind_double(6, c.low);
            stmt.bind_double(7, c.close);
            stmt.bind_double(8, c.volume);
            stmt.bind_text(9, c.provider);
            stmt.execute();
        }
        db.commit();
    } catch (...) {
        db.rollback();
        throw;
    }
}

std::vector<algo::CandleData> get_candle_cache(const std::string& symbol,
                                                      const std::string& timeframe,
                                                      int limit) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT symbol, timeframe, timestamp, open, high, low, close, volume, provider "
        "FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2 "
        "ORDER BY timestamp DESC LIMIT ?3");
    stmt.bind_text(1, symbol);
    stmt.bind_text(2, timeframe);
    stmt.bind_int(3, limit);
    std::vector<algo::CandleData> result;
    while (stmt.step()) {
        algo::CandleData c;
        c.symbol    = stmt.col_text(0);
        c.timeframe = stmt.col_text(1);
        c.timestamp = stmt.col_int(2);
        c.open      = stmt.col_double(3);
        c.high      = stmt.col_double(4);
        c.low       = stmt.col_double(5);
        c.close     = stmt.col_double(6);
        c.volume    = stmt.col_double(7);
        c.provider  = stmt.col_text(8);
        result.push_back(c);
    }
    return result;
}

int64_t count_candles(const std::string& symbol, const std::string& timeframe) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT COUNT(*) FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2");
    stmt.bind_text(1, symbol);
    stmt.bind_text(2, timeframe);
    if (stmt.step()) return stmt.col_int(0);
    return 0;
}

// --- Custom Python Strategies ---

void save_custom_python_strategy(const algo::CustomPythonStrategy& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO custom_python_strategies
        (id, name, description, code, category, parameters, updated_at)
        VALUES (?1,?2,?3,?4,?5,?6,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, s.id);
    stmt.bind_text(2, s.name);
    stmt.bind_text(3, s.description);
    stmt.bind_text(4, s.code);
    stmt.bind_text(5, s.category);
    stmt.bind_text(6, s.parameters);
    stmt.execute();
}

std::vector<algo::CustomPythonStrategy> list_custom_python_strategies() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, code, category, parameters, created_at, updated_at "
        "FROM custom_python_strategies ORDER BY updated_at DESC");
    std::vector<algo::CustomPythonStrategy> result;
    while (stmt.step()) {
        algo::CustomPythonStrategy s;
        s.id          = stmt.col_text(0);
        s.name        = stmt.col_text(1);
        s.description = stmt.col_text(2);
        s.code        = stmt.col_text(3);
        s.category    = stmt.col_text(4);
        s.parameters  = stmt.col_text(5);
        s.created_at  = stmt.col_text(6);
        s.updated_at  = stmt.col_text(7);
        result.push_back(s);
    }
    return result;
}

std::optional<algo::CustomPythonStrategy> get_custom_python_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, code, category, parameters, created_at, updated_at "
        "FROM custom_python_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    if (!stmt.step()) return std::nullopt;
    algo::CustomPythonStrategy s;
    s.id          = stmt.col_text(0);
    s.name        = stmt.col_text(1);
    s.description = stmt.col_text(2);
    s.code        = stmt.col_text(3);
    s.category    = stmt.col_text(4);
    s.parameters  = stmt.col_text(5);
    s.created_at  = stmt.col_text(6);
    s.updated_at  = stmt.col_text(7);
    return s;
}

void delete_custom_python_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM custom_python_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- Price Cache ---

void update_price_cache(const std::string& symbol, double price,
                              double volume, double change_pct) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO strategy_price_cache
        (symbol, price, volume, change_pct, updated_at)
        VALUES (?1,?2,?3,?4,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, symbol);
    stmt.bind_double(2, price);
    stmt.bind_double(3, volume);
    stmt.bind_double(4, change_pct);
    stmt.execute();
}

std::vector<algo::PriceCacheEntry> get_price_cache_entries() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT symbol, price, volume, change_pct, updated_at FROM strategy_price_cache");
    std::vector<algo::PriceCacheEntry> result;
    while (stmt.step()) {
        algo::PriceCacheEntry e;
        e.symbol     = stmt.col_text(0);
        e.price      = stmt.col_double(1);
        e.volume     = stmt.col_double(2);
        e.change_pct = stmt.col_double(3);
        e.updated_at = stmt.col_text(4);
        result.push_back(e);
    }
    return result;
}

} // namespace ops
} // namespace fincept::db
