// Database Schema - All table definitions and migrations

use anyhow::Result;
use rusqlite::Connection;

pub fn create_schema(conn: &Connection) -> Result<()> {
    // Execute all schema statements
    conn.execute_batch(
        "
        -- Credentials table
        CREATE TABLE IF NOT EXISTS credentials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            service_name TEXT NOT NULL UNIQUE,
            username TEXT,
            password TEXT,
            api_key TEXT,
            api_secret TEXT,
            additional_data TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Settings table
        CREATE TABLE IF NOT EXISTS settings (
            setting_key TEXT PRIMARY KEY,
            setting_value TEXT NOT NULL,
            category TEXT,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- LLM configurations table
        CREATE TABLE IF NOT EXISTS llm_configs (
            provider TEXT PRIMARY KEY,
            api_key TEXT,
            base_url TEXT,
            model TEXT NOT NULL,
            is_active INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- LLM global settings table
        CREATE TABLE IF NOT EXISTS llm_global_settings (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            temperature REAL DEFAULT 0.7,
            max_tokens INTEGER DEFAULT 2000,
            system_prompt TEXT DEFAULT ''
        );

        INSERT OR IGNORE INTO llm_global_settings (id, temperature, max_tokens, system_prompt)
        VALUES (1, 0.7, 2000, 'You are a helpful AI assistant specialized in financial analysis and market data.');

        -- LLM model configurations table (user-added custom models)
        CREATE TABLE IF NOT EXISTS llm_model_configs (
            id TEXT PRIMARY KEY,
            provider TEXT NOT NULL,
            model_id TEXT NOT NULL,
            display_name TEXT NOT NULL,
            api_key TEXT,
            base_url TEXT,
            is_enabled INTEGER DEFAULT 1,
            is_default INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_llm_model_configs_provider ON llm_model_configs(provider);
        CREATE INDEX IF NOT EXISTS idx_llm_model_configs_enabled ON llm_model_configs(is_enabled);

        -- Insert default Fincept LLM config if not exists
        INSERT OR IGNORE INTO llm_configs (provider, api_key, base_url, model, is_active)
        VALUES ('fincept', '', 'https://finceptbackend.share.zrok.io/research/llm', 'fincept-llm', 1);

        -- Chat sessions table
        CREATE TABLE IF NOT EXISTS chat_sessions (
            session_uuid TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            message_count INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Chat messages table
        CREATE TABLE IF NOT EXISTS chat_messages (
            id TEXT PRIMARY KEY,
            session_uuid TEXT NOT NULL,
            role TEXT NOT NULL CHECK (role IN ('user', 'assistant')),
            content TEXT NOT NULL,
            timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
            provider TEXT,
            model TEXT,
            tokens_used INTEGER,
            FOREIGN KEY (session_uuid) REFERENCES chat_sessions(session_uuid) ON DELETE CASCADE
        );

        -- Data sources table
        CREATE TABLE IF NOT EXISTS data_sources (
            id TEXT PRIMARY KEY,
            alias TEXT NOT NULL UNIQUE,
            display_name TEXT NOT NULL,
            description TEXT,
            type TEXT NOT NULL CHECK (type IN ('websocket', 'rest_api')),
            provider TEXT NOT NULL,
            category TEXT,
            config TEXT NOT NULL,
            enabled INTEGER DEFAULT 1,
            tags TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- WebSocket provider configs table
        CREATE TABLE IF NOT EXISTS ws_provider_configs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            provider_name TEXT NOT NULL UNIQUE,
            enabled INTEGER DEFAULT 1,
            api_key TEXT,
            api_secret TEXT,
            endpoint TEXT,
            config_data TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Watchlists table
        CREATE TABLE IF NOT EXISTS watchlists (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            color TEXT DEFAULT '#FFA500',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Watchlist stocks table
        CREATE TABLE IF NOT EXISTS watchlist_stocks (
            id TEXT PRIMARY KEY,
            watchlist_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            added_at TEXT DEFAULT CURRENT_TIMESTAMP,
            notes TEXT,
            FOREIGN KEY (watchlist_id) REFERENCES watchlists(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_watchlist_stocks ON watchlist_stocks(watchlist_id);

        -- Paper trading portfolios table
        CREATE TABLE IF NOT EXISTS paper_trading_portfolios (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            provider TEXT NOT NULL,
            initial_balance REAL NOT NULL DEFAULT 100000,
            current_balance REAL NOT NULL,
            currency TEXT DEFAULT 'USD',
            margin_mode TEXT DEFAULT 'cross',
            leverage REAL DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Paper trading positions table
        CREATE TABLE IF NOT EXISTS paper_trading_positions (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL CHECK (side IN ('long', 'short')),
            entry_price REAL NOT NULL,
            quantity REAL NOT NULL,
            position_value REAL,
            current_price REAL,
            unrealized_pnl REAL,
            realized_pnl REAL DEFAULT 0,
            leverage REAL DEFAULT 1,
            margin_mode TEXT DEFAULT 'cross',
            liquidation_price REAL,
            opened_at TEXT DEFAULT CURRENT_TIMESTAMP,
            closed_at TEXT,
            status TEXT NOT NULL CHECK (status IN ('open', 'closed')),
            FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE
        );

        -- Paper trading orders table
        CREATE TABLE IF NOT EXISTS paper_trading_orders (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL CHECK (side IN ('buy', 'sell')),
            type TEXT NOT NULL CHECK (type IN ('market', 'limit', 'stop_market', 'stop_limit')),
            quantity REAL NOT NULL,
            price REAL,
            stop_price REAL,
            filled_quantity REAL DEFAULT 0,
            avg_fill_price REAL,
            status TEXT NOT NULL CHECK (status IN ('pending', 'filled', 'partial', 'cancelled', 'rejected', 'triggered')),
            time_in_force TEXT DEFAULT 'GTC',
            post_only INTEGER DEFAULT 0,
            reduce_only INTEGER DEFAULT 0,
            trailing_percent REAL,
            trailing_amount REAL,
            iceberg_qty REAL,
            leverage REAL,
            margin_mode TEXT,
            product TEXT CHECK (product IN ('CNC', 'MIS', 'NRML')),
            exchange TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            filled_at TEXT,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE
        );

        -- Paper trading trades table
        CREATE TABLE IF NOT EXISTS paper_trading_trades (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            order_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            price REAL NOT NULL,
            quantity REAL NOT NULL,
            fee REAL DEFAULT 0,
            fee_rate REAL DEFAULT 0,
            is_maker INTEGER DEFAULT 0,
            timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE,
            FOREIGN KEY (order_id) REFERENCES paper_trading_orders(id) ON DELETE CASCADE
        );

        -- Indexes for paper trading tables
        CREATE INDEX IF NOT EXISTS idx_paper_positions_portfolio ON paper_trading_positions(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_paper_positions_status ON paper_trading_positions(status);
        CREATE INDEX IF NOT EXISTS idx_paper_orders_portfolio ON paper_trading_orders(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_paper_orders_status ON paper_trading_orders(status);
        CREATE INDEX IF NOT EXISTS idx_paper_trades_portfolio ON paper_trading_trades(portfolio_id);

        -- Paper trading margin blocks table (for pending order margin)
        CREATE TABLE IF NOT EXISTS paper_trading_margin_blocks (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            order_id TEXT NOT NULL UNIQUE,
            symbol TEXT NOT NULL,
            blocked_amount REAL NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE,
            FOREIGN KEY (order_id) REFERENCES paper_trading_orders(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_margin_blocks_portfolio ON paper_trading_margin_blocks(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_margin_blocks_order ON paper_trading_margin_blocks(order_id);

        -- Paper trading holdings table (T+1 settled positions for equity)
        CREATE TABLE IF NOT EXISTS paper_trading_holdings (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            quantity REAL NOT NULL DEFAULT 0,
            average_price REAL NOT NULL DEFAULT 0,
            invested_value REAL NOT NULL DEFAULT 0,
            current_price REAL NOT NULL DEFAULT 0,
            current_value REAL NOT NULL DEFAULT 0,
            pnl REAL DEFAULT 0,
            pnl_percent REAL DEFAULT 0,
            t1_quantity REAL DEFAULT 0,
            available_quantity REAL DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE,
            UNIQUE(portfolio_id, symbol)
        );

        CREATE INDEX IF NOT EXISTS idx_paper_holdings_portfolio ON paper_trading_holdings(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_paper_holdings_symbol ON paper_trading_holdings(symbol);

        -- Stock positions table (for equity trading)
        CREATE TABLE IF NOT EXISTS stock_positions (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            exchange TEXT NOT NULL,
            product TEXT NOT NULL CHECK (product IN ('CNC', 'MIS', 'NRML')),
            quantity REAL NOT NULL DEFAULT 0,
            average_price REAL NOT NULL DEFAULT 0,
            current_price REAL,
            unrealized_pnl REAL,
            realized_pnl REAL DEFAULT 0,
            today_realized_pnl REAL DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE,
            UNIQUE(portfolio_id, symbol, exchange, product)
        );

        CREATE INDEX IF NOT EXISTS idx_stock_positions_portfolio ON stock_positions(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_stock_positions_symbol ON stock_positions(symbol);
        CREATE INDEX IF NOT EXISTS idx_stock_positions_product ON stock_positions(product);

        -- Stock holdings table (T+1 settled positions)
        CREATE TABLE IF NOT EXISTS stock_holdings (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            exchange TEXT NOT NULL,
            quantity REAL NOT NULL DEFAULT 0,
            average_price REAL NOT NULL DEFAULT 0,
            current_price REAL,
            pnl REAL,
            pnl_percent REAL,
            settlement_date TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE,
            UNIQUE(portfolio_id, symbol, exchange)
        );

        CREATE INDEX IF NOT EXISTS idx_stock_holdings_portfolio ON stock_holdings(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_stock_holdings_symbol ON stock_holdings(symbol);

        -- MCP servers table
        CREATE TABLE IF NOT EXISTS mcp_servers (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            command TEXT NOT NULL,
            args TEXT,
            env TEXT,
            category TEXT,
            icon TEXT,
            enabled INTEGER DEFAULT 1,
            auto_start INTEGER DEFAULT 0,
            status TEXT DEFAULT 'stopped',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- MCP tools table
        CREATE TABLE IF NOT EXISTS mcp_tools (
            id TEXT PRIMARY KEY,
            server_id TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT,
            input_schema TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (server_id) REFERENCES mcp_servers(id) ON DELETE CASCADE
        );

        -- MCP tool usage logs table
        CREATE TABLE IF NOT EXISTS mcp_tool_usage_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            server_id TEXT NOT NULL,
            tool_name TEXT NOT NULL,
            args TEXT,
            result TEXT,
            success INTEGER,
            execution_time INTEGER,
            error TEXT,
            timestamp TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Internal MCP tool settings (for fincept-terminal built-in tools)
        CREATE TABLE IF NOT EXISTS internal_mcp_tool_settings (
            tool_name TEXT PRIMARY KEY,
            category TEXT NOT NULL,
            is_enabled INTEGER DEFAULT 1,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- OLD: market_data_cache table REMOVED - Use unified cache (fincept_cache.db)

        -- Data source connections table
        CREATE TABLE IF NOT EXISTS data_source_connections (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            type TEXT NOT NULL,
            category TEXT NOT NULL,
            config TEXT NOT NULL,
            status TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            last_tested TEXT,
            error_message TEXT
        );

        -- OLD: forum_*_cache tables REMOVED - Use unified cache (fincept_cache.db)

        -- Context Recording tables
        CREATE TABLE IF NOT EXISTS recorded_contexts (
            id TEXT PRIMARY KEY,
            tab_name TEXT NOT NULL,
            data_type TEXT NOT NULL,
            label TEXT,
            raw_data TEXT NOT NULL,
            metadata TEXT,
            data_size INTEGER,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            tags TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_recorded_contexts_tab ON recorded_contexts(tab_name);
        CREATE INDEX IF NOT EXISTS idx_recorded_contexts_type ON recorded_contexts(data_type);
        CREATE INDEX IF NOT EXISTS idx_recorded_contexts_created ON recorded_contexts(created_at DESC);

        CREATE TABLE IF NOT EXISTS recording_sessions (
            id TEXT PRIMARY KEY,
            tab_name TEXT NOT NULL,
            is_active INTEGER DEFAULT 1,
            auto_record INTEGER DEFAULT 0,
            filters TEXT,
            started_at TEXT DEFAULT CURRENT_TIMESTAMP,
            ended_at TEXT
        );

        CREATE TABLE IF NOT EXISTS chat_context_links (
            chat_session_uuid TEXT NOT NULL,
            context_id TEXT NOT NULL,
            added_at TEXT DEFAULT CURRENT_TIMESTAMP,
            is_active INTEGER DEFAULT 1,
            PRIMARY KEY (chat_session_uuid, context_id),
            FOREIGN KEY(chat_session_uuid) REFERENCES chat_sessions(session_uuid) ON DELETE CASCADE,
            FOREIGN KEY(context_id) REFERENCES recorded_contexts(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS report_templates (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            template_data TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Backtesting providers table
        CREATE TABLE IF NOT EXISTS backtesting_providers (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            adapter_type TEXT NOT NULL,
            config TEXT NOT NULL,
            enabled INTEGER DEFAULT 1,
            is_active INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Backtesting strategies table
        CREATE TABLE IF NOT EXISTS backtesting_strategies (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            version TEXT DEFAULT '1.0.0',
            author TEXT,
            provider_type TEXT NOT NULL,
            strategy_type TEXT NOT NULL CHECK (strategy_type IN ('code', 'visual', 'template')),
            strategy_definition TEXT NOT NULL,
            tags TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_strategies_provider ON backtesting_strategies(provider_type);
        CREATE INDEX IF NOT EXISTS idx_strategies_type ON backtesting_strategies(strategy_type);

        -- Backtest runs table
        CREATE TABLE IF NOT EXISTS backtest_runs (
            id TEXT PRIMARY KEY,
            strategy_id TEXT,
            provider_name TEXT NOT NULL,
            config TEXT NOT NULL,
            results TEXT,
            status TEXT NOT NULL CHECK (status IN ('completed', 'failed', 'running', 'cancelled')),
            performance_metrics TEXT,
            error_message TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            completed_at TEXT,
            duration_seconds INTEGER,
            FOREIGN KEY(strategy_id) REFERENCES backtesting_strategies(id) ON DELETE SET NULL
        );

        CREATE INDEX IF NOT EXISTS idx_backtest_runs_strategy ON backtest_runs(strategy_id);
        CREATE INDEX IF NOT EXISTS idx_backtest_runs_provider ON backtest_runs(provider_name);
        CREATE INDEX IF NOT EXISTS idx_backtest_runs_status ON backtest_runs(status);
        CREATE INDEX IF NOT EXISTS idx_backtest_runs_created ON backtest_runs(created_at DESC);

        -- Optimization runs table
        CREATE TABLE IF NOT EXISTS optimization_runs (
            id TEXT PRIMARY KEY,
            strategy_id TEXT,
            provider_name TEXT NOT NULL,
            parameter_grid TEXT NOT NULL,
            objective TEXT NOT NULL,
            best_parameters TEXT,
            best_result TEXT,
            all_results TEXT,
            iterations INTEGER,
            status TEXT NOT NULL CHECK (status IN ('completed', 'failed', 'running', 'cancelled')),
            error_message TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            completed_at TEXT,
            duration_seconds INTEGER,
            FOREIGN KEY(strategy_id) REFERENCES backtesting_strategies(id) ON DELETE SET NULL
        );

        CREATE INDEX IF NOT EXISTS idx_optimization_runs_strategy ON optimization_runs(strategy_id);
        CREATE INDEX IF NOT EXISTS idx_optimization_runs_provider ON optimization_runs(provider_name);
        CREATE INDEX IF NOT EXISTS idx_optimization_runs_status ON optimization_runs(status);

        -- Monitor conditions table
        CREATE TABLE IF NOT EXISTS monitor_conditions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            provider TEXT NOT NULL,
            symbol TEXT NOT NULL,
            field TEXT NOT NULL CHECK (field IN ('price', 'volume', 'change_percent', 'spread')),
            operator TEXT NOT NULL CHECK (operator IN ('>', '<', '>=', '<=', '==', 'between')),
            value REAL NOT NULL,
            value2 REAL,
            enabled INTEGER DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_monitor_conditions_provider_symbol ON monitor_conditions(provider, symbol);
        CREATE INDEX IF NOT EXISTS idx_monitor_conditions_enabled ON monitor_conditions(enabled);

        -- Monitor alerts table
        CREATE TABLE IF NOT EXISTS monitor_alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            condition_id INTEGER NOT NULL,
            provider TEXT NOT NULL,
            symbol TEXT NOT NULL,
            field TEXT NOT NULL,
            triggered_value REAL NOT NULL,
            triggered_at INTEGER NOT NULL,
            FOREIGN KEY(condition_id) REFERENCES monitor_conditions(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_monitor_alerts_triggered_at ON monitor_alerts(triggered_at DESC);
        CREATE INDEX IF NOT EXISTS idx_monitor_alerts_condition_id ON monitor_alerts(condition_id);

        -- Portfolio Management tables
        CREATE TABLE IF NOT EXISTS portfolios (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            owner TEXT NOT NULL,
            currency TEXT DEFAULT 'USD',
            description TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS portfolio_assets (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            quantity REAL NOT NULL,
            avg_buy_price REAL NOT NULL,
            first_purchase_date TEXT DEFAULT CURRENT_TIMESTAMP,
            last_updated TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS portfolio_transactions (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            transaction_type TEXT NOT NULL CHECK (transaction_type IN ('BUY', 'SELL', 'DIVIDEND', 'SPLIT')),
            quantity REAL NOT NULL,
            price REAL NOT NULL,
            total_value REAL NOT NULL,
            transaction_date TEXT DEFAULT CURRENT_TIMESTAMP,
            notes TEXT,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS portfolio_snapshots (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            total_value REAL NOT NULL,
            total_cost_basis REAL NOT NULL,
            total_pnl REAL NOT NULL,
            total_pnl_percent REAL NOT NULL,
            snapshot_data TEXT NOT NULL,
            snapshot_date TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_portfolio_assets_portfolio ON portfolio_assets(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_portfolio_assets_symbol ON portfolio_assets(symbol);
        CREATE INDEX IF NOT EXISTS idx_portfolio_transactions_portfolio ON portfolio_transactions(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_portfolio_transactions_date ON portfolio_transactions(transaction_date DESC);
        CREATE INDEX IF NOT EXISTS idx_portfolio_snapshots_portfolio ON portfolio_snapshots(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_portfolio_snapshots_date ON portfolio_snapshots(snapshot_date DESC);

        -- Agent Configurations table (CoreAgent system)
        CREATE TABLE IF NOT EXISTS agent_configs (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            config_json TEXT NOT NULL,
            category TEXT DEFAULT 'general',
            is_default INTEGER DEFAULT 0,
            is_active INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_agent_configs_category ON agent_configs(category);
        CREATE INDEX IF NOT EXISTS idx_agent_configs_is_default ON agent_configs(is_default);
        CREATE INDEX IF NOT EXISTS idx_agent_configs_is_active ON agent_configs(is_active);

        -- Broker Credentials table (Encrypted storage for broker API keys)
        CREATE TABLE IF NOT EXISTS broker_credentials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            broker_id TEXT NOT NULL UNIQUE,
            api_key TEXT,
            api_secret TEXT,
            access_token TEXT,
            refresh_token TEXT,
            additional_data TEXT,
            encrypted INTEGER DEFAULT 1,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_broker_credentials_broker_id ON broker_credentials(broker_id);

        -- Master Contract Cache table (Broker symbol data metadata)
        CREATE TABLE IF NOT EXISTS master_contract_cache (
            broker_id TEXT PRIMARY KEY,
            symbols_data TEXT NOT NULL,
            symbol_count INTEGER NOT NULL,
            last_updated INTEGER NOT NULL,
            created_at INTEGER NOT NULL
        );

        -- Symbols table (Searchable instrument database)
        CREATE TABLE IF NOT EXISTS symbols (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            broker_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            br_symbol TEXT NOT NULL,
            name TEXT,
            exchange TEXT NOT NULL,
            br_exchange TEXT,
            token TEXT NOT NULL,
            expiry TEXT,
            strike REAL,
            lot_size INTEGER,
            instrument_type TEXT,
            tick_size REAL,
            segment TEXT,
            created_at INTEGER NOT NULL
        );

        -- Symbols indices for fast search
        CREATE INDEX IF NOT EXISTS idx_symbols_broker ON symbols(broker_id);
        CREATE INDEX IF NOT EXISTS idx_symbols_symbol ON symbols(symbol);
        CREATE INDEX IF NOT EXISTS idx_symbols_name ON symbols(name);
        CREATE INDEX IF NOT EXISTS idx_symbols_exchange ON symbols(exchange);
        CREATE INDEX IF NOT EXISTS idx_symbols_token ON symbols(token);
        CREATE INDEX IF NOT EXISTS idx_symbols_instrument_type ON symbols(instrument_type);
        CREATE INDEX IF NOT EXISTS idx_symbols_broker_exchange ON symbols(broker_id, exchange);
        CREATE INDEX IF NOT EXISTS idx_symbols_symbol_exchange ON symbols(symbol, exchange);

        -- Generic Key-Value Storage table
        CREATE TABLE IF NOT EXISTS key_value_storage (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at INTEGER NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_key_value_storage_updated ON key_value_storage(updated_at DESC);

        -- User RSS Feeds table (Custom news sources)
        CREATE TABLE IF NOT EXISTS user_rss_feeds (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            url TEXT NOT NULL UNIQUE,
            category TEXT NOT NULL DEFAULT 'GENERAL',
            region TEXT NOT NULL DEFAULT 'GLOBAL',
            source_name TEXT NOT NULL,
            enabled INTEGER DEFAULT 1,
            is_default INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_user_rss_feeds_enabled ON user_rss_feeds(enabled);
        CREATE INDEX IF NOT EXISTS idx_user_rss_feeds_category ON user_rss_feeds(category);
        CREATE INDEX IF NOT EXISTS idx_user_rss_feeds_is_default ON user_rss_feeds(is_default);

        -- Default RSS Feed Preferences (stores user preferences for built-in feeds)
        CREATE TABLE IF NOT EXISTS default_rss_feed_preferences (
            feed_id TEXT PRIMARY KEY,
            enabled INTEGER DEFAULT 1,
            deleted INTEGER DEFAULT 0,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- =============================================================================
        -- Alpha Arena Tables (Competition Paper Trading)
        -- =============================================================================

        -- Alpha Arena competitions table
        CREATE TABLE IF NOT EXISTS alpha_arena_competitions (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            exchange_id TEXT DEFAULT 'kraken',
            symbols TEXT NOT NULL,
            initial_capital REAL NOT NULL DEFAULT 10000,
            mode TEXT NOT NULL DEFAULT 'baseline',
            cycle_interval_seconds INTEGER DEFAULT 150,
            max_cycles INTEGER,
            status TEXT NOT NULL DEFAULT 'created' CHECK (status IN ('created', 'running', 'paused', 'completed', 'failed')),
            cycle_count INTEGER DEFAULT 0,
            config_json TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            started_at TEXT,
            completed_at TEXT,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_alpha_competitions_status ON alpha_arena_competitions(status);
        CREATE INDEX IF NOT EXISTS idx_alpha_competitions_created ON alpha_arena_competitions(created_at DESC);

        -- Alpha Arena model portfolios (links models to paper trading portfolios)
        CREATE TABLE IF NOT EXISTS alpha_arena_model_portfolios (
            id TEXT PRIMARY KEY,
            competition_id TEXT NOT NULL,
            model_name TEXT NOT NULL,
            portfolio_id TEXT NOT NULL,
            provider TEXT,
            model_id TEXT,
            initial_capital REAL NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (competition_id) REFERENCES alpha_arena_competitions(id) ON DELETE CASCADE,
            FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id) ON DELETE CASCADE,
            UNIQUE(competition_id, model_name)
        );

        CREATE INDEX IF NOT EXISTS idx_alpha_model_portfolios_competition ON alpha_arena_model_portfolios(competition_id);
        CREATE INDEX IF NOT EXISTS idx_alpha_model_portfolios_model ON alpha_arena_model_portfolios(model_name);

        -- Alpha Arena decision logs (AI trading decisions)
        CREATE TABLE IF NOT EXISTS alpha_arena_decisions (
            id TEXT PRIMARY KEY,
            competition_id TEXT NOT NULL,
            model_name TEXT NOT NULL,
            cycle_number INTEGER NOT NULL,
            symbol TEXT NOT NULL,
            action TEXT NOT NULL CHECK (action IN ('buy', 'sell', 'hold', 'BUY', 'SELL', 'HOLD')),
            quantity REAL,
            confidence REAL,
            reasoning TEXT,
            trade_executed INTEGER DEFAULT 0,
            price_at_decision REAL,
            portfolio_value_before REAL,
            portfolio_value_after REAL,
            order_id TEXT,
            pnl REAL DEFAULT 0,
            timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (competition_id) REFERENCES alpha_arena_competitions(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_alpha_decisions_competition ON alpha_arena_decisions(competition_id);
        CREATE INDEX IF NOT EXISTS idx_alpha_decisions_model ON alpha_arena_decisions(model_name);
        CREATE INDEX IF NOT EXISTS idx_alpha_decisions_cycle ON alpha_arena_decisions(cycle_number);
        CREATE INDEX IF NOT EXISTS idx_alpha_decisions_timestamp ON alpha_arena_decisions(timestamp DESC);

        -- Alpha Arena performance snapshots (for charting)
        CREATE TABLE IF NOT EXISTS alpha_arena_snapshots (
            id TEXT PRIMARY KEY,
            competition_id TEXT NOT NULL,
            model_name TEXT NOT NULL,
            cycle_number INTEGER NOT NULL,
            portfolio_value REAL NOT NULL,
            cash REAL NOT NULL,
            pnl REAL DEFAULT 0,
            return_pct REAL DEFAULT 0,
            positions_count INTEGER DEFAULT 0,
            trades_count INTEGER DEFAULT 0,
            timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (competition_id) REFERENCES alpha_arena_competitions(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_alpha_snapshots_competition ON alpha_arena_snapshots(competition_id);
        CREATE INDEX IF NOT EXISTS idx_alpha_snapshots_model ON alpha_arena_snapshots(model_name);
        CREATE INDEX IF NOT EXISTS idx_alpha_snapshots_cycle ON alpha_arena_snapshots(cycle_number);

        -- Alpha Arena leaderboard snapshots
        CREATE TABLE IF NOT EXISTS alpha_arena_leaderboard (
            id TEXT PRIMARY KEY,
            competition_id TEXT NOT NULL,
            cycle_number INTEGER NOT NULL,
            leaderboard_json TEXT NOT NULL,
            timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (competition_id) REFERENCES alpha_arena_competitions(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_alpha_leaderboard_competition ON alpha_arena_leaderboard(competition_id);
        CREATE INDEX IF NOT EXISTS idx_alpha_leaderboard_cycle ON alpha_arena_leaderboard(cycle_number DESC);

        -- =============================================================================
        -- Custom Index Tables (Aggregate Index Feature)
        -- =============================================================================

        -- Custom indices table (user-created indices)
        CREATE TABLE IF NOT EXISTS custom_indices (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            calculation_method TEXT NOT NULL CHECK (calculation_method IN (
                'price_weighted', 'market_cap_weighted', 'equal_weighted',
                'float_adjusted', 'fundamental_weighted', 'modified_market_cap',
                'factor_weighted', 'risk_parity', 'geometric_mean', 'capped_weighted'
            )),
            base_value REAL NOT NULL DEFAULT 100,
            base_date TEXT NOT NULL,
            divisor REAL NOT NULL DEFAULT 1,
            current_value REAL NOT NULL DEFAULT 100,
            previous_close REAL NOT NULL DEFAULT 100,
            cap_weight REAL,
            currency TEXT DEFAULT 'USD',
            portfolio_id TEXT,
            is_active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE SET NULL
        );

        CREATE INDEX IF NOT EXISTS idx_custom_indices_active ON custom_indices(is_active);
        CREATE INDEX IF NOT EXISTS idx_custom_indices_portfolio ON custom_indices(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_custom_indices_method ON custom_indices(calculation_method);

        -- Index constituents table (stocks/assets in each index)
        CREATE TABLE IF NOT EXISTS index_constituents (
            id TEXT PRIMARY KEY,
            index_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            shares REAL,
            weight REAL,
            market_cap REAL,
            fundamental_score REAL,
            custom_price REAL,
            price_date TEXT,
            added_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (index_id) REFERENCES custom_indices(id) ON DELETE CASCADE,
            UNIQUE(index_id, symbol)
        );

        CREATE INDEX IF NOT EXISTS idx_index_constituents_index ON index_constituents(index_id);
        CREATE INDEX IF NOT EXISTS idx_index_constituents_symbol ON index_constituents(symbol);

        -- Index snapshots table (historical values for tracking)
        CREATE TABLE IF NOT EXISTS index_snapshots (
            id TEXT PRIMARY KEY,
            index_id TEXT NOT NULL,
            index_value REAL NOT NULL,
            day_change REAL NOT NULL DEFAULT 0,
            day_change_percent REAL NOT NULL DEFAULT 0,
            total_market_value REAL NOT NULL,
            divisor REAL NOT NULL,
            constituents_data TEXT NOT NULL,
            snapshot_date TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (index_id) REFERENCES custom_indices(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_index_snapshots_index ON index_snapshots(index_id);
        CREATE INDEX IF NOT EXISTS idx_index_snapshots_date ON index_snapshots(snapshot_date DESC);
        CREATE UNIQUE INDEX IF NOT EXISTS idx_index_snapshots_unique ON index_snapshots(index_id, snapshot_date)
        ",
    )?;

    // Migrations: Add missing columns to existing tables
    // Check if custom_price column exists, if not add it
    let column_check: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('index_constituents') WHERE name='custom_price'",
        [],
        |row| row.get(0)
    );

    if let Ok(count) = column_check {
        if count == 0 {
            // Add custom_price column
            conn.execute(
                "ALTER TABLE index_constituents ADD COLUMN custom_price REAL",
                [],
            )?;
            println!("[Migration] Added custom_price column to index_constituents");
        }
    }

    // Check if price_date column exists, if not add it
    let column_check: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('index_constituents') WHERE name='price_date'",
        [],
        |row| row.get(0)
    );

    if let Ok(count) = column_check {
        if count == 0 {
            // Add price_date column
            conn.execute(
                "ALTER TABLE index_constituents ADD COLUMN price_date TEXT",
                [],
            )?;
            println!("[Migration] Added price_date column to index_constituents");
        }
    }

    // Check if historical_start_date column exists in custom_indices, if not add it
    let column_check: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('custom_indices') WHERE name='historical_start_date'",
        [],
        |row| row.get(0)
    );

    if let Ok(count) = column_check {
        if count == 0 {
            // Add historical_start_date column
            conn.execute(
                "ALTER TABLE custom_indices ADD COLUMN historical_start_date TEXT",
                [],
            )?;
            println!("[Migration] Added historical_start_date column to custom_indices");
        }
    }

    Ok(())
}
