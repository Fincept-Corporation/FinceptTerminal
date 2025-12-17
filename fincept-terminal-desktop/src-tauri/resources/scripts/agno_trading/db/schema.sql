-- Agno Trading Database Schema
-- Agent Performance Tracking & Trade History

-- Agent Trades Table
CREATE TABLE IF NOT EXISTS agent_trades (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    model TEXT NOT NULL,
    symbol TEXT NOT NULL,
    side TEXT NOT NULL, -- 'buy' or 'sell'
    order_type TEXT NOT NULL, -- 'market', 'limit', 'stop'
    quantity REAL NOT NULL,
    entry_price REAL NOT NULL,
    exit_price REAL,
    stop_loss REAL,
    take_profit REAL,
    pnl REAL DEFAULT 0,
    pnl_percent REAL DEFAULT 0,
    status TEXT DEFAULT 'open', -- 'open', 'closed', 'cancelled'
    entry_timestamp INTEGER NOT NULL,
    exit_timestamp INTEGER,
    reasoning TEXT,
    metadata TEXT, -- JSON: strategy, signals, indicators
    execution_time_ms INTEGER,
    is_paper_trade BOOLEAN DEFAULT 1,
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Agent Performance Metrics Table
CREATE TABLE IF NOT EXISTS agent_performance (
    agent_id TEXT PRIMARY KEY,
    model TEXT NOT NULL,
    total_trades INTEGER DEFAULT 0,
    winning_trades INTEGER DEFAULT 0,
    losing_trades INTEGER DEFAULT 0,
    total_pnl REAL DEFAULT 0,
    daily_pnl REAL DEFAULT 0,
    weekly_pnl REAL DEFAULT 0,
    monthly_pnl REAL DEFAULT 0,
    win_rate REAL DEFAULT 0,
    sharpe_ratio REAL DEFAULT 0,
    sortino_ratio REAL DEFAULT 0,
    max_drawdown REAL DEFAULT 0,
    current_drawdown REAL DEFAULT 0,
    avg_win REAL DEFAULT 0,
    avg_loss REAL DEFAULT 0,
    profit_factor REAL DEFAULT 0,
    largest_win REAL DEFAULT 0,
    largest_loss REAL DEFAULT 0,
    consecutive_wins INTEGER DEFAULT 0,
    consecutive_losses INTEGER DEFAULT 0,
    max_consecutive_wins INTEGER DEFAULT 0,
    max_consecutive_losses INTEGER DEFAULT 0,
    total_volume REAL DEFAULT 0,
    positions INTEGER DEFAULT 0,
    last_trade_timestamp INTEGER,
    last_updated INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Agent Decisions Log (for real-time feed)
CREATE TABLE IF NOT EXISTS agent_decisions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    model TEXT NOT NULL,
    decision_type TEXT NOT NULL, -- 'analysis', 'signal', 'trade', 'risk'
    symbol TEXT,
    decision TEXT NOT NULL,
    reasoning TEXT,
    confidence REAL,
    timestamp INTEGER NOT NULL,
    execution_time_ms INTEGER
);

-- Competitions Table
CREATE TABLE IF NOT EXISTS competitions (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    symbol TEXT NOT NULL,
    task_type TEXT NOT NULL,
    participating_models TEXT NOT NULL, -- JSON array
    status TEXT DEFAULT 'active', -- 'active', 'completed', 'cancelled'
    start_timestamp INTEGER NOT NULL,
    end_timestamp INTEGER,
    consensus_action TEXT,
    consensus_confidence REAL,
    results TEXT, -- JSON
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Agent Evolution History
CREATE TABLE IF NOT EXISTS agent_evolution (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    model TEXT NOT NULL,
    evolution_trigger TEXT NOT NULL, -- 'performance', 'loss_streak', 'win_streak', 'manual'
    old_instructions TEXT,
    new_instructions TEXT NOT NULL,
    metrics_before TEXT, -- JSON
    metrics_after TEXT, -- JSON
    timestamp INTEGER NOT NULL,
    notes TEXT
);

-- Debate Sessions Table
CREATE TABLE IF NOT EXISTS debate_sessions (
    id TEXT PRIMARY KEY,
    symbol TEXT NOT NULL,
    bull_model TEXT NOT NULL,
    bear_model TEXT NOT NULL,
    analyst_model TEXT NOT NULL,
    bull_argument TEXT,
    bear_argument TEXT,
    analyst_decision TEXT,
    final_action TEXT,
    confidence REAL,
    timestamp INTEGER NOT NULL,
    execution_time_ms INTEGER
);

-- Indexes for Performance
CREATE INDEX IF NOT EXISTS idx_agent_trades_agent ON agent_trades(agent_id);
CREATE INDEX IF NOT EXISTS idx_agent_trades_symbol ON agent_trades(symbol);
CREATE INDEX IF NOT EXISTS idx_agent_trades_status ON agent_trades(status);
CREATE INDEX IF NOT EXISTS idx_agent_trades_timestamp ON agent_trades(entry_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_agent_decisions_agent ON agent_decisions(agent_id);
CREATE INDEX IF NOT EXISTS idx_agent_decisions_timestamp ON agent_decisions(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_competitions_status ON competitions(status);
