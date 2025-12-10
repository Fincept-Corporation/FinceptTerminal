/**
 * SQLite Database Service
 * Uses Tauri SQL plugin for database operations
 */

import Database from '@tauri-apps/plugin-sql';

// Type definitions
export interface Credential {
  id?: number;
  service_name: string;
  username: string;
  password: string;
  api_key?: string;
  api_secret?: string;
  additional_data?: string;
  created_at?: string;
  updated_at?: string;
}

// Predefined API Keys structure
export interface ApiKeys {
  FRED_API_KEY?: string;
  POLYGON_API_KEY?: string;
  ALPHA_VANTAGE_API_KEY?: string;
  OPENAI_API_KEY?: string;
  ANTHROPIC_API_KEY?: string;
  COINGECKO_API_KEY?: string;
  NASDAQ_API_KEY?: string;
  FINANCIAL_MODELING_PREP_API_KEY?: string;
  [key: string]: string | undefined;
}

export const PREDEFINED_API_KEYS = [
  { key: 'FRED_API_KEY', label: 'FRED API Key', description: 'Federal Reserve Economic Data' },
  { key: 'POLYGON_API_KEY', label: 'Polygon.io API Key', description: 'Stock market data' },
  { key: 'ALPHA_VANTAGE_API_KEY', label: 'Alpha Vantage API Key', description: 'Stock & crypto data' },
  { key: 'OPENAI_API_KEY', label: 'OpenAI API Key', description: 'GPT models' },
  { key: 'ANTHROPIC_API_KEY', label: 'Anthropic API Key', description: 'Claude models' },
  { key: 'COINGECKO_API_KEY', label: 'CoinGecko API Key', description: 'Cryptocurrency data' },
  { key: 'NASDAQ_API_KEY', label: 'NASDAQ API Key', description: 'NASDAQ market data' },
  { key: 'FINANCIAL_MODELING_PREP_API_KEY', label: 'Financial Modeling Prep', description: 'Financial statements & ratios' },
] as const;

export interface Setting {
  setting_key: string;
  setting_value: string;
  category?: string;
  updated_at?: string;
}

export interface LLMConfig {
  provider: string;
  api_key?: string;
  base_url?: string;
  model: string;
  is_active: boolean;
  created_at?: string;
  updated_at?: string;
}

export interface LLMGlobalSettings {
  temperature: number;
  max_tokens: number;
  system_prompt: string;
}

export interface ChatSession {
  session_uuid: string;
  title: string;
  message_count: number;
  created_at: string;
  updated_at: string;
}

export interface ChatMessage {
  id: string;
  session_uuid: string;
  role: 'user' | 'assistant';
  content: string;
  timestamp: string;
  provider?: string;
  model?: string;
  tokens_used?: number;
}

export interface DataSource {
  id: string;
  alias: string;
  display_name: string;
  description?: string;
  type: 'websocket' | 'rest_api';
  provider: string;
  category?: string;
  config: string;
  enabled: boolean;
  tags?: string;
  created_at: string;
  updated_at: string;
}

export interface WSProviderConfig {
  id: number;
  provider_name: string;
  enabled: boolean;
  api_key: string | null;
  api_secret: string | null;
  endpoint: string | null;
  config_data: string | null;
  created_at: string;
  updated_at: string;
}

export interface MCPServer {
  id: string;
  name: string;
  description: string;
  command: string;
  args: string;
  env?: string | null;
  category: string;
  icon: string;
  enabled: boolean;
  auto_start: boolean;
  status: string;
  created_at: string;
  updated_at: string;
}

export interface RecordedContext {
  id: string;
  tab_name: string;
  data_type: string;
  label?: string;
  raw_data: string;
  metadata?: string;
  data_size: number;
  created_at: string;
  tags?: string;
}

export interface RecordingSession {
  id: string;
  tab_name: string;
  is_active: boolean;
  auto_record: boolean;
  filters?: string;
  started_at: string;
  ended_at?: string;
}

export interface ChatContextLink {
  chat_session_uuid: string;
  context_id: string;
  added_at: string;
  is_active: boolean;
}

export interface BacktestingProvider {
  id: string;
  name: string;
  adapter_type: string;
  config: string;
  enabled: boolean;
  is_active: boolean;
  created_at: string;
  updated_at: string;
}

export interface BacktestingStrategy {
  id: string;
  name: string;
  description?: string;
  version: string;
  author?: string;
  provider_type: string;
  strategy_type: 'code' | 'visual' | 'template';
  strategy_definition: string;
  tags?: string;
  created_at: string;
  updated_at: string;
}

export interface BacktestRun {
  id: string;
  strategy_id?: string;
  provider_name: string;
  config: string;
  results?: string;
  status: 'completed' | 'failed' | 'running' | 'cancelled';
  performance_metrics?: string;
  error_message?: string;
  created_at: string;
  completed_at?: string;
  duration_seconds?: number;
}

export interface OptimizationRun {
  id: string;
  strategy_id?: string;
  provider_name: string;
  parameter_grid: string;
  objective: string;
  best_parameters?: string;
  best_result?: string;
  all_results?: string;
  iterations?: number;
  status: 'completed' | 'failed' | 'running' | 'cancelled';
  error_message?: string;
  created_at: string;
  completed_at?: string;
  duration_seconds?: number;
}

class SQLiteService {
  private db: Database | null = null;
  private initPromise: Promise<void> | null = null;

  /**
   * Initialize database connection and schema
   */
  async initialize(): Promise<void> {
    if (this.initPromise) {
      return this.initPromise;
    }

    this.initPromise = this._initializeDatabase();
    return this.initPromise;
  }

  private async _initializeDatabase(): Promise<void> {
    try {
      console.log('[SQLite] Initializing database...');

      // Connect to SQLite database
      this.db = await Database.load('sqlite:fincept_terminal.db');

      console.log('[SQLite] Creating schema...');
      await this.createSchema();

      console.log('[SQLite] ✓ Database initialized successfully');
    } catch (error) {
      console.error('[SQLite] Failed to initialize:', error);
      throw error;
    }
  }

  private async createSchema(): Promise<void> {
    if (!this.db) throw new Error('Database not connected');

    const statements = [
      // Credentials table
      `CREATE TABLE IF NOT EXISTS credentials (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        service_name TEXT NOT NULL UNIQUE,
        username TEXT,
        password TEXT,
        api_key TEXT,
        api_secret TEXT,
        additional_data TEXT,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // Settings table
      `CREATE TABLE IF NOT EXISTS settings (
        setting_key TEXT PRIMARY KEY,
        setting_value TEXT NOT NULL,
        category TEXT,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // LLM configurations table
      `CREATE TABLE IF NOT EXISTS llm_configs (
        provider TEXT PRIMARY KEY,
        api_key TEXT,
        base_url TEXT,
        model TEXT NOT NULL,
        is_active INTEGER DEFAULT 0,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // LLM global settings table
      `CREATE TABLE IF NOT EXISTS llm_global_settings (
        id INTEGER PRIMARY KEY CHECK (id = 1),
        temperature REAL DEFAULT 0.7,
        max_tokens INTEGER DEFAULT 2000,
        system_prompt TEXT DEFAULT ''
      )`,

      // Chat sessions table
      `CREATE TABLE IF NOT EXISTS chat_sessions (
        session_uuid TEXT PRIMARY KEY,
        title TEXT NOT NULL,
        message_count INTEGER DEFAULT 0,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // Chat messages table
      `CREATE TABLE IF NOT EXISTS chat_messages (
        id TEXT PRIMARY KEY,
        session_uuid TEXT NOT NULL,
        role TEXT NOT NULL CHECK (role IN ('user', 'assistant')),
        content TEXT NOT NULL,
        timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
        provider TEXT,
        model TEXT,
        tokens_used INTEGER,
        FOREIGN KEY (session_uuid) REFERENCES chat_sessions(session_uuid) ON DELETE CASCADE
      )`,

      // Data sources table
      `CREATE TABLE IF NOT EXISTS data_sources (
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
      )`,

      // WebSocket provider configs table
      `CREATE TABLE IF NOT EXISTS ws_provider_configs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        provider_name TEXT NOT NULL UNIQUE,
        enabled INTEGER DEFAULT 1,
        api_key TEXT,
        api_secret TEXT,
        endpoint TEXT,
        config_data TEXT,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // Paper trading portfolios table
      `CREATE TABLE IF NOT EXISTS paper_trading_portfolios (
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
      )`,

      // Paper trading positions table
      `CREATE TABLE IF NOT EXISTS paper_trading_positions (
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
      )`,

      // Paper trading orders table
      `CREATE TABLE IF NOT EXISTS paper_trading_orders (
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
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        filled_at TEXT,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (portfolio_id) REFERENCES paper_trading_portfolios(id) ON DELETE CASCADE
      )`,

      // Paper trading trades table
      `CREATE TABLE IF NOT EXISTS paper_trading_trades (
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
      )`,

      // Indexes for paper trading tables
      `CREATE INDEX IF NOT EXISTS idx_paper_positions_portfolio ON paper_trading_positions(portfolio_id)`,
      `CREATE INDEX IF NOT EXISTS idx_paper_positions_status ON paper_trading_positions(status)`,
      `CREATE INDEX IF NOT EXISTS idx_paper_orders_portfolio ON paper_trading_orders(portfolio_id)`,
      `CREATE INDEX IF NOT EXISTS idx_paper_orders_status ON paper_trading_orders(status)`,
      `CREATE INDEX IF NOT EXISTS idx_paper_trades_portfolio ON paper_trading_trades(portfolio_id)`,

      // MCP servers table
      `CREATE TABLE IF NOT EXISTS mcp_servers (
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
      )`,

      // MCP tools table
      `CREATE TABLE IF NOT EXISTS mcp_tools (
        id TEXT PRIMARY KEY,
        server_id TEXT NOT NULL,
        name TEXT NOT NULL,
        description TEXT,
        input_schema TEXT,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (server_id) REFERENCES mcp_servers(id) ON DELETE CASCADE
      )`,

      // MCP tool usage logs table
      `CREATE TABLE IF NOT EXISTS mcp_tool_usage_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        server_id TEXT NOT NULL,
        tool_name TEXT NOT NULL,
        args TEXT,
        result TEXT,
        success INTEGER,
        execution_time INTEGER,
        error TEXT,
        timestamp TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // Market data cache table
      `CREATE TABLE IF NOT EXISTS market_data_cache (
        symbol TEXT NOT NULL,
        category TEXT NOT NULL,
        quote_data TEXT NOT NULL,
        cached_at TEXT DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY (symbol, category)
      )`,

      // Migration: Add position_value column if it doesn't exist
      `ALTER TABLE paper_trading_positions ADD COLUMN position_value REAL`,

      // Data source connections table
      `CREATE TABLE IF NOT EXISTS data_source_connections (
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
      )`,

      // Forum cache tables
      `CREATE TABLE IF NOT EXISTS forum_categories_cache (
        id INTEGER PRIMARY KEY,
        data TEXT NOT NULL,
        cached_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      `CREATE TABLE IF NOT EXISTS forum_posts_cache (
        cache_key TEXT PRIMARY KEY,
        category_id INTEGER,
        sort_by TEXT,
        data TEXT NOT NULL,
        cached_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      `CREATE TABLE IF NOT EXISTS forum_stats_cache (
        id INTEGER PRIMARY KEY CHECK (id = 1),
        data TEXT NOT NULL,
        cached_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      `CREATE TABLE IF NOT EXISTS forum_post_details_cache (
        post_uuid TEXT PRIMARY KEY,
        data TEXT NOT NULL,
        comments_data TEXT NOT NULL,
        cached_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // Context Recording tables
      `CREATE TABLE IF NOT EXISTS recorded_contexts (
        id TEXT PRIMARY KEY,
        tab_name TEXT NOT NULL,
        data_type TEXT NOT NULL,
        label TEXT,
        raw_data TEXT NOT NULL,
        metadata TEXT,
        data_size INTEGER,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        tags TEXT
      )`,

      `CREATE INDEX IF NOT EXISTS idx_recorded_contexts_tab ON recorded_contexts(tab_name)`,
      `CREATE INDEX IF NOT EXISTS idx_recorded_contexts_type ON recorded_contexts(data_type)`,
      `CREATE INDEX IF NOT EXISTS idx_recorded_contexts_created ON recorded_contexts(created_at DESC)`,

      `CREATE TABLE IF NOT EXISTS recording_sessions (
        id TEXT PRIMARY KEY,
        tab_name TEXT NOT NULL,
        is_active INTEGER DEFAULT 1,
        auto_record INTEGER DEFAULT 0,
        filters TEXT,
        started_at TEXT DEFAULT CURRENT_TIMESTAMP,
        ended_at TEXT
      )`,

      `CREATE TABLE IF NOT EXISTS chat_context_links (
        chat_session_uuid TEXT NOT NULL,
        context_id TEXT NOT NULL,
        added_at TEXT DEFAULT CURRENT_TIMESTAMP,
        is_active INTEGER DEFAULT 1,
        PRIMARY KEY (chat_session_uuid, context_id),
        FOREIGN KEY(chat_session_uuid) REFERENCES chat_sessions(session_uuid) ON DELETE CASCADE,
        FOREIGN KEY(context_id) REFERENCES recorded_contexts(id) ON DELETE CASCADE
      )`,

      `CREATE TABLE IF NOT EXISTS report_templates (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        description TEXT,
        template_data TEXT NOT NULL,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // Backtesting providers table
      `CREATE TABLE IF NOT EXISTS backtesting_providers (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        adapter_type TEXT NOT NULL,
        config TEXT NOT NULL,
        enabled INTEGER DEFAULT 1,
        is_active INTEGER DEFAULT 0,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )`,

      // Backtesting strategies table
      `CREATE TABLE IF NOT EXISTS backtesting_strategies (
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
      )`,

      `CREATE INDEX IF NOT EXISTS idx_strategies_provider ON backtesting_strategies(provider_type)`,
      `CREATE INDEX IF NOT EXISTS idx_strategies_type ON backtesting_strategies(strategy_type)`,

      // Backtest runs table
      `CREATE TABLE IF NOT EXISTS backtest_runs (
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
      )`,

      `CREATE INDEX IF NOT EXISTS idx_backtest_runs_strategy ON backtest_runs(strategy_id)`,
      `CREATE INDEX IF NOT EXISTS idx_backtest_runs_provider ON backtest_runs(provider_name)`,
      `CREATE INDEX IF NOT EXISTS idx_backtest_runs_status ON backtest_runs(status)`,
      `CREATE INDEX IF NOT EXISTS idx_backtest_runs_created ON backtest_runs(created_at DESC)`,

      // Optimization runs table
      `CREATE TABLE IF NOT EXISTS optimization_runs (
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
      )`,

      `CREATE INDEX IF NOT EXISTS idx_optimization_runs_strategy ON optimization_runs(strategy_id)`,
      `CREATE INDEX IF NOT EXISTS idx_optimization_runs_provider ON optimization_runs(provider_name)`,
      `CREATE INDEX IF NOT EXISTS idx_optimization_runs_status ON optimization_runs(status)`,

      // Insert default LLM global settings
      `INSERT OR IGNORE INTO llm_global_settings (id, temperature, max_tokens, system_prompt)
       VALUES (1, 0.7, 2000, 'You are a helpful AI assistant specialized in financial analysis and market data.')`,
    ];

    for (const sql of statements) {
      try {
        await this.db.execute(sql);
      } catch (error: any) {
        // Ignore "duplicate column" errors from ALTER TABLE
        if (!error.message?.includes('duplicate column')) {
          throw error;
        }
      }
    }

    // Update existing positions to populate position_value
    try {
      await this.db.execute(`
        UPDATE paper_trading_positions
        SET position_value = entry_price * quantity
        WHERE position_value IS NULL
      `);
    } catch (error) {
      console.error('[SQLite] Failed to populate position_value:', error);
    }
  }

  // =========================
  // CREDENTIALS METHODS
  // =========================

  async saveCredential(credential: Omit<Credential, 'id' | 'created_at' | 'updated_at'>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        `INSERT OR REPLACE INTO credentials (service_name, username, password, api_key, api_secret, additional_data, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP)`,
        [
          credential.service_name,
          credential.username || null,
          credential.password || null,
          credential.api_key || null,
          credential.api_secret || null,
          credential.additional_data || null,
        ]
      );
      return { success: true, message: 'Credential saved successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to save credential:', error);
      return { success: false, message: 'Failed to save credential' };
    }
  }

  async getCredentials(): Promise<Credential[]> {
    await this.ensureInitialized();
    return await this.db!.select<Credential[]>('SELECT * FROM credentials ORDER BY service_name');
  }

  async getCredentialByService(serviceName: string): Promise<Credential | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<Credential[]>(
      'SELECT * FROM credentials WHERE service_name = $1',
      [serviceName]
    );
    return results[0] || null;
  }

  async getApiKey(serviceName: string): Promise<string | null> {
    // Use settings table for predefined API keys
    const keyName = serviceName.toUpperCase().replace(/[^A-Z0-9]/g, '_') + '_API_KEY';
    return await this.getSetting(keyName);
  }

  async setApiKey(keyName: string, value: string): Promise<void> {
    await this.saveSetting(keyName, value, 'api_keys');
  }

  async getAllApiKeys(): Promise<ApiKeys> {
    await this.ensureInitialized();
    const results = await this.db!.select<Setting[]>(
      "SELECT * FROM settings WHERE category = 'api_keys'"
    );

    const keys: ApiKeys = {};
    results.forEach(row => {
      keys[row.setting_key] = row.setting_value;
    });
    return keys;
  }

  async deleteCredential(id: number): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();
      await this.db!.execute('DELETE FROM credentials WHERE id = $1', [id]);
      return { success: true, message: 'Credential deleted successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to delete credential:', error);
      return { success: false, message: 'Failed to delete credential' };
    }
  }

  // =========================
  // SETTINGS METHODS
  // =========================

  async saveSetting(key: string, value: string, category?: string): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `INSERT OR REPLACE INTO settings (setting_key, setting_value, category, updated_at)
       VALUES ($1, $2, $3, CURRENT_TIMESTAMP)`,
      [key, value, category || null]
    );
  }

  async getSetting(key: string): Promise<string | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<Setting[]>(
      'SELECT * FROM settings WHERE setting_key = $1',
      [key]
    );
    return results[0]?.setting_value || null;
  }

  async getSettingsByCategory(category: string): Promise<Setting[]> {
    await this.ensureInitialized();
    return await this.db!.select<Setting[]>(
      'SELECT * FROM settings WHERE category = $1',
      [category]
    );
  }

  async getAllSettings(): Promise<Setting[]> {
    await this.ensureInitialized();
    return await this.db!.select<Setting[]>('SELECT * FROM settings');
  }

  // =========================
  // LLM METHODS
  // =========================

  async getLLMConfigs(): Promise<LLMConfig[]> {
    await this.ensureInitialized();
    return await this.db!.select<LLMConfig[]>('SELECT * FROM llm_configs ORDER BY provider');
  }

  async getLLMConfig(provider: string): Promise<LLMConfig | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<LLMConfig[]>(
      'SELECT * FROM llm_configs WHERE provider = $1',
      [provider]
    );
    return results[0] || null;
  }

  async getActiveLLMConfig(): Promise<LLMConfig | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<LLMConfig[]>(
      'SELECT * FROM llm_configs WHERE is_active = 1 LIMIT 1'
    );
    return results[0] || null;
  }

  async saveLLMConfig(config: Omit<LLMConfig, 'created_at' | 'updated_at'>): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, updated_at)
       VALUES ($1, $2, $3, $4, $5, CURRENT_TIMESTAMP)`,
      [
        config.provider,
        config.api_key || null,
        config.base_url || null,
        config.model,
        config.is_active ? 1 : 0,
      ]
    );
  }

  async setActiveLLMProvider(provider: string): Promise<void> {
    await this.ensureInitialized();

    // Deactivate all
    await this.db!.execute('UPDATE llm_configs SET is_active = 0');

    // Activate the selected one
    await this.db!.execute(
      'UPDATE llm_configs SET is_active = 1 WHERE provider = $1',
      [provider]
    );
  }

  async ensureDefaultLLMConfigs(): Promise<void> {
    await this.ensureInitialized();

    const configs = await this.getLLMConfigs();

    // If no configs exist, create defaults
    if (configs.length === 0) {
      const defaultConfigs = [
        { provider: 'ollama', model: 'llama3.2:3b', base_url: 'http://localhost:11434', is_active: true },
        { provider: 'openai', model: 'gpt-4o-mini', is_active: false },
        { provider: 'gemini', model: 'gemini-2.0-flash-exp', is_active: false },
        { provider: 'deepseek', model: 'deepseek-chat', base_url: 'https://api.deepseek.com', is_active: false },
        { provider: 'openrouter', model: 'meta-llama/llama-3.1-8b-instruct:free', base_url: 'https://openrouter.ai/api/v1', is_active: false },
      ];

      for (const config of defaultConfigs) {
        await this.saveLLMConfig(config);
      }

      console.log('[SQLite] Initialized default LLM configurations');
    }
  }

  async getLLMGlobalSettings(): Promise<LLMGlobalSettings> {
    await this.ensureInitialized();
    const results = await this.db!.select<LLMGlobalSettings[]>(
      'SELECT temperature, max_tokens, system_prompt FROM llm_global_settings WHERE id = 1'
    );

    return results[0] || {
      temperature: 0.7,
      max_tokens: 2000,
      system_prompt: 'You are a helpful AI assistant specialized in financial analysis and market data.',
    };
  }

  async saveLLMGlobalSettings(settings: LLMGlobalSettings): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `UPDATE llm_global_settings
       SET temperature = $1, max_tokens = $2, system_prompt = $3
       WHERE id = 1`,
      [settings.temperature, settings.max_tokens, settings.system_prompt]
    );
  }

  // =========================
  // CHAT METHODS
  // =========================

  async createChatSession(title: string): Promise<ChatSession> {
    await this.ensureInitialized();

    const sessionUuid = crypto.randomUUID();
    const timestamp = new Date().toISOString();

    await this.db!.execute(
      `INSERT INTO chat_sessions (session_uuid, title, message_count, created_at, updated_at)
       VALUES ($1, $2, 0, $3, $4)`,
      [sessionUuid, title, timestamp, timestamp]
    );

    // Return the full session object
    return {
      session_uuid: sessionUuid,
      title: title,
      message_count: 0,
      created_at: timestamp,
      updated_at: timestamp
    };
  }

  async getChatSessions(limit: number = 50): Promise<ChatSession[]> {
    await this.ensureInitialized();
    return await this.db!.select<ChatSession[]>(
      `SELECT * FROM chat_sessions ORDER BY updated_at DESC LIMIT $1`,
      [limit]
    );
  }

  async getChatSession(sessionUuid: string): Promise<ChatSession | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<ChatSession[]>(
      'SELECT * FROM chat_sessions WHERE session_uuid = $1',
      [sessionUuid]
    );
    return results[0] || null;
  }

  async updateChatSessionTitle(sessionUuid: string, title: string): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `UPDATE chat_sessions SET title = $1, updated_at = CURRENT_TIMESTAMP WHERE session_uuid = $2`,
      [title, sessionUuid]
    );
  }

  async deleteChatSession(sessionUuid: string): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM chat_sessions WHERE session_uuid = $1', [sessionUuid]);
  }

  async addChatMessage(message: Omit<ChatMessage, 'id' | 'timestamp'>): Promise<ChatMessage> {
    await this.ensureInitialized();

    const messageId = crypto.randomUUID();
    const timestamp = new Date().toISOString();

    await this.db!.execute(
      `INSERT INTO chat_messages (id, session_uuid, role, content, timestamp, provider, model, tokens_used)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
      [
        messageId,
        message.session_uuid,
        message.role,
        message.content,
        timestamp,
        message.provider || null,
        message.model || null,
        message.tokens_used || null,
      ]
    );

    // Update message count
    await this.db!.execute(
      `UPDATE chat_sessions
       SET message_count = message_count + 1, updated_at = CURRENT_TIMESTAMP
       WHERE session_uuid = $1`,
      [message.session_uuid]
    );

    // Return the full message object
    return {
      id: messageId,
      session_uuid: message.session_uuid,
      role: message.role,
      content: message.content,
      timestamp: timestamp,
      provider: message.provider,
      model: message.model,
      tokens_used: message.tokens_used,
    };
  }

  async getChatMessages(sessionUuid: string): Promise<ChatMessage[]> {
    await this.ensureInitialized();
    return await this.db!.select<ChatMessage[]>(
      'SELECT * FROM chat_messages WHERE session_uuid = $1 ORDER BY timestamp ASC',
      [sessionUuid]
    );
  }

  async deleteChatMessage(id: string): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM chat_messages WHERE id = $1', [id]);
  }

  async clearChatSessionMessages(sessionUuid: string): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM chat_messages WHERE session_uuid = $1', [sessionUuid]);
    await this.db!.execute(
      'UPDATE chat_sessions SET message_count = 0, updated_at = CURRENT_TIMESTAMP WHERE session_uuid = $1',
      [sessionUuid]
    );
  }

  // =========================
  // DATA SOURCES METHODS
  // =========================

  async saveDataSource(source: Omit<DataSource, 'created_at' | 'updated_at'>): Promise<{ success: boolean; message: string; id?: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        `INSERT OR REPLACE INTO data_sources
         (id, alias, display_name, description, type, provider, category, config, enabled, tags, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, CURRENT_TIMESTAMP)`,
        [
          source.id,
          source.alias,
          source.display_name,
          source.description || null,
          source.type,
          source.provider,
          source.category || null,
          source.config,
          source.enabled ? 1 : 0,
          source.tags || null,
        ]
      );
      return { success: true, message: 'Data source saved successfully', id: source.id };
    } catch (error) {
      console.error('[SQLite] Failed to save data source:', error);
      return { success: false, message: 'Failed to save data source' };
    }
  }

  async getAllDataSources(): Promise<DataSource[]> {
    await this.ensureInitialized();
    return await this.db!.select<DataSource[]>('SELECT * FROM data_sources ORDER BY display_name');
  }

  async getDataSourceById(id: string): Promise<DataSource | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<DataSource[]>(
      'SELECT * FROM data_sources WHERE id = $1',
      [id]
    );
    return results[0] || null;
  }

  async getDataSourceByAlias(alias: string): Promise<DataSource | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<DataSource[]>(
      'SELECT * FROM data_sources WHERE alias = $1',
      [alias]
    );
    return results[0] || null;
  }

  async getDataSourcesByType(type: 'websocket' | 'rest_api'): Promise<DataSource[]> {
    await this.ensureInitialized();
    return await this.db!.select<DataSource[]>(
      'SELECT * FROM data_sources WHERE type = $1',
      [type]
    );
  }

  async getDataSourcesByProvider(provider: string): Promise<DataSource[]> {
    await this.ensureInitialized();
    return await this.db!.select<DataSource[]>(
      'SELECT * FROM data_sources WHERE provider = $1',
      [provider]
    );
  }

  async getEnabledDataSources(): Promise<DataSource[]> {
    await this.ensureInitialized();
    return await this.db!.select<DataSource[]>(
      'SELECT * FROM data_sources WHERE enabled = 1'
    );
  }

  async toggleDataSourceEnabled(id: string): Promise<{ success: boolean; message: string; enabled: boolean }> {
    try {
      await this.ensureInitialized();

      // Get current state
      const source = await this.getDataSourceById(id);
      if (!source) {
        return { success: false, message: 'Data source not found', enabled: false };
      }

      const newEnabledState = !source.enabled;

      await this.db!.execute(
        'UPDATE data_sources SET enabled = NOT enabled, updated_at = CURRENT_TIMESTAMP WHERE id = $1',
        [id]
      );

      return { success: true, message: `Data source ${newEnabledState ? 'enabled' : 'disabled'}`, enabled: newEnabledState };
    } catch (error) {
      console.error('[SQLite] Failed to toggle data source:', error);
      return { success: false, message: 'Failed to toggle data source', enabled: false };
    }
  }

  async deleteDataSource(id: string): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();
      await this.db!.execute('DELETE FROM data_sources WHERE id = $1', [id]);
      return { success: true, message: 'Data source deleted successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to delete data source:', error);
      return { success: false, message: 'Failed to delete data source' };
    }
  }

  async searchDataSources(query: string): Promise<DataSource[]> {
    await this.ensureInitialized();
    const searchPattern = `%${query}%`;
    return await this.db!.select<DataSource[]>(
      `SELECT * FROM data_sources
       WHERE display_name LIKE $1 OR alias LIKE $1 OR provider LIKE $1 OR tags LIKE $1`,
      [searchPattern]
    );
  }

  // =========================
  // WS PROVIDER CONFIG METHODS
  // =========================

  async saveWSProviderConfig(config: Omit<WSProviderConfig, 'id' | 'created_at' | 'updated_at'>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        `INSERT OR REPLACE INTO ws_provider_configs
         (provider_name, enabled, api_key, api_secret, endpoint, config_data, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP)`,
        [
          config.provider_name,
          config.enabled ? 1 : 0,
          config.api_key || null,
          config.api_secret || null,
          config.endpoint || null,
          config.config_data || null,
        ]
      );
      return { success: true, message: 'WebSocket provider configuration saved successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to save WS provider config:', error);
      return { success: false, message: 'Failed to save WebSocket provider configuration' };
    }
  }

  async getWSProviderConfigs(): Promise<WSProviderConfig[]> {
    await this.ensureInitialized();
    return await this.db!.select<WSProviderConfig[]>('SELECT * FROM ws_provider_configs ORDER BY provider_name');
  }

  async getWSProviderConfig(providerName: string): Promise<WSProviderConfig | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<WSProviderConfig[]>(
      'SELECT * FROM ws_provider_configs WHERE provider_name = $1',
      [providerName]
    );
    return results[0] || null;
  }

  async getEnabledWSProviderConfigs(): Promise<WSProviderConfig[]> {
    await this.ensureInitialized();
    return await this.db!.select<WSProviderConfig[]>(
      'SELECT * FROM ws_provider_configs WHERE enabled = 1'
    );
  }

  async toggleWSProviderEnabled(providerName: string): Promise<{ success: boolean; message: string; enabled: boolean }> {
    try {
      await this.ensureInitialized();

      // Get current state
      const config = await this.getWSProviderConfig(providerName);
      if (!config) {
        return { success: false, message: 'Provider configuration not found', enabled: false };
      }

      const newEnabledState = !config.enabled;

      await this.db!.execute(
        'UPDATE ws_provider_configs SET enabled = NOT enabled, updated_at = CURRENT_TIMESTAMP WHERE provider_name = $1',
        [providerName]
      );

      return { success: true, message: `Provider ${newEnabledState ? 'enabled' : 'disabled'}`, enabled: newEnabledState };
    } catch (error) {
      console.error('[SQLite] Failed to toggle WS provider:', error);
      return { success: false, message: 'Failed to toggle provider', enabled: false };
    }
  }

  async deleteWSProviderConfig(providerName: string): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();
      await this.db!.execute('DELETE FROM ws_provider_configs WHERE provider_name = $1', [providerName]);
      return { success: true, message: 'Provider configuration deleted successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to delete WS provider config:', error);
      return { success: false, message: 'Failed to delete provider configuration' };
    }
  }

  // =========================
  // MCP METHODS
  // =========================

  async addMCPServer(server: Omit<MCPServer, 'created_at' | 'updated_at'>): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `INSERT OR REPLACE INTO mcp_servers
       (id, name, description, command, args, env, category, icon, enabled, auto_start, status, updated_at)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, CURRENT_TIMESTAMP)`,
      [
        server.id,
        server.name,
        server.description,
        server.command,
        server.args,
        server.env || null,
        server.category,
        server.icon,
        server.enabled ? 1 : 0,
        server.auto_start ? 1 : 0,
        server.status,
      ]
    );
  }

  async getMCPServers(): Promise<MCPServer[]> {
    await this.ensureInitialized();
    return await this.db!.select<MCPServer[]>('SELECT * FROM mcp_servers ORDER BY name');
  }

  async getMCPServer(id: string): Promise<MCPServer | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<MCPServer[]>(
      'SELECT * FROM mcp_servers WHERE id = $1',
      [id]
    );
    return results[0] || null;
  }

  async updateMCPServerStatus(id: string, status: string): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute(
      'UPDATE mcp_servers SET status = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2',
      [status, id]
    );
  }

  async updateMCPServerConfig(id: string, updates: Partial<MCPServer>): Promise<void> {
    await this.ensureInitialized();

    const fields: string[] = [];
    const values: any[] = [];
    let paramIndex = 1;

    if (updates.name !== undefined) {
      fields.push(`name = $${paramIndex++}`);
      values.push(updates.name);
    }
    if (updates.description !== undefined) {
      fields.push(`description = $${paramIndex++}`);
      values.push(updates.description);
    }
    if (updates.command !== undefined) {
      fields.push(`command = $${paramIndex++}`);
      values.push(updates.command);
    }
    if (updates.args !== undefined) {
      fields.push(`args = $${paramIndex++}`);
      values.push(updates.args);
    }
    if (updates.env !== undefined) {
      fields.push(`env = $${paramIndex++}`);
      values.push(updates.env);
    }
    if (updates.category !== undefined) {
      fields.push(`category = $${paramIndex++}`);
      values.push(updates.category);
    }
    if (updates.icon !== undefined) {
      fields.push(`icon = $${paramIndex++}`);
      values.push(updates.icon);
    }
    if (updates.enabled !== undefined) {
      fields.push(`enabled = $${paramIndex++}`);
      values.push(updates.enabled ? 1 : 0);
    }
    if (updates.auto_start !== undefined) {
      fields.push(`auto_start = $${paramIndex++}`);
      values.push(updates.auto_start ? 1 : 0);
    }

    if (fields.length === 0) return;

    fields.push(`updated_at = CURRENT_TIMESTAMP`);
    values.push(id);

    const sql = `UPDATE mcp_servers SET ${fields.join(', ')} WHERE id = $${paramIndex}`;
    await this.db!.execute(sql, values);
  }

  async deleteMCPServer(id: string): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM mcp_servers WHERE id = $1', [id]);
  }

  async getAutoStartServers(): Promise<MCPServer[]> {
    await this.ensureInitialized();
    return await this.db!.select<MCPServer[]>(
      'SELECT * FROM mcp_servers WHERE auto_start = 1 AND enabled = 1'
    );
  }

  async cacheMCPTools(serverId: string, tools: any[]): Promise<void> {
    await this.ensureInitialized();

    // Delete existing tools for this server
    await this.db!.execute('DELETE FROM mcp_tools WHERE server_id = $1', [serverId]);

    // Insert new tools
    for (const tool of tools) {
      await this.db!.execute(
        `INSERT INTO mcp_tools (id, server_id, name, description, input_schema)
         VALUES ($1, $2, $3, $4, $5)`,
        [
          `${serverId}_${tool.name}`,
          serverId,
          tool.name,
          tool.description || null,
          tool.inputSchema ? JSON.stringify(tool.inputSchema) : null
        ]
      );
    }
  }

  async getMCPServerStats(serverId: string): Promise<{ toolCount: number; callsToday: number; lastUsed: string | null }> {
    await this.ensureInitialized();

    // Get tool count
    const toolCountResult = await this.db!.select<{ count: number }[]>(
      'SELECT COUNT(*) as count FROM mcp_tools WHERE server_id = $1',
      [serverId]
    );
    const toolCount = toolCountResult[0]?.count || 0;

    // Get calls today
    const today = new Date().toISOString().split('T')[0]; // YYYY-MM-DD
    const callsTodayResult = await this.db!.select<{ count: number }[]>(
      `SELECT COUNT(*) as count FROM mcp_tool_usage_logs
       WHERE server_id = $1 AND DATE(timestamp) = $2`,
      [serverId, today]
    );
    const callsToday = callsTodayResult[0]?.count || 0;

    // Get last used timestamp
    const lastUsedResult = await this.db!.select<{ timestamp: string }[]>(
      `SELECT timestamp FROM mcp_tool_usage_logs
       WHERE server_id = $1 ORDER BY timestamp DESC LIMIT 1`,
      [serverId]
    );
    const lastUsed = lastUsedResult[0]?.timestamp || null;

    return {
      toolCount,
      callsToday,
      lastUsed
    };
  }

  async logMCPToolUsage(log: {
    serverId: string;
    toolName: string;
    args: string;
    result: string | null;
    success: boolean;
    executionTime: number;
    error?: string;
  }): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `INSERT INTO mcp_tool_usage_logs (server_id, tool_name, args, result, success, execution_time, error)
       VALUES ($1, $2, $3, $4, $5, $6, $7)`,
      [
        log.serverId,
        log.toolName,
        log.args,
        log.result,
        log.success ? 1 : 0,
        log.executionTime,
        log.error || null
      ]
    );
  }

  async getMCPTools(): Promise<any[]> {
    await this.ensureInitialized();
    return await this.db!.select<any[]>(
      'SELECT * FROM mcp_tools ORDER BY name'
    );
  }

  async getMCPToolsByServer(serverId: string): Promise<any[]> {
    await this.ensureInitialized();
    return await this.db!.select<any[]>(
      'SELECT * FROM mcp_tools WHERE server_id = $1 ORDER BY name',
      [serverId]
    );
  }

  // =========================
  // MARKET DATA CACHE METHODS
  // =========================

  async saveMarketDataCache(symbol: string, category: string, quoteData: any): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `INSERT OR REPLACE INTO market_data_cache (symbol, category, quote_data, cached_at)
       VALUES ($1, $2, $3, CURRENT_TIMESTAMP)`,
      [symbol, category, JSON.stringify(quoteData)]
    );
  }

  async getCachedMarketData(symbol: string, category: string, maxAgeMinutes: number = 5): Promise<any | null> {
    await this.ensureInitialized();

    const results = await this.db!.select<{ quote_data: string; cached_at: string }[]>(
      `SELECT quote_data, cached_at FROM market_data_cache
       WHERE symbol = $1 AND category = $2
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
      [symbol, category]
    );

    if (!results[0]) return null;

    return JSON.parse(results[0].quote_data);
  }

  async clearMarketDataCache(): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM market_data_cache');
  }

  async clearExpiredMarketCache(maxAgeMinutes: number = 60): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute(
      `DELETE FROM market_data_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
    );
  }

  async getCachedCategoryData(category: string, symbols: string[], maxAgeMinutes: number = 5): Promise<Map<string, any>> {
    await this.ensureInitialized();

    const result = new Map<string, any>();

    try {
      // Build placeholders using $ syntax
      const placeholders = symbols.map((_, i) => `$${i + 1}`).join(',');
      const categoryParam = symbols.length + 1;

      const results = await this.db!.select<{ symbol: string; quote_data: string; cached_at: string }[]>(
        `SELECT symbol, quote_data, cached_at FROM market_data_cache
         WHERE symbol IN (${placeholders}) AND category = $${categoryParam}
         AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
        [...symbols, category]
      );

      for (const row of results) {
        result.set(row.symbol, JSON.parse(row.quote_data));
      }
    } catch (error) {
      console.error('[SQLite] Error getting cached category data:', error);
    }

    return result;
  }

  // =========================
  // DATA SOURCE CONNECTIONS METHODS
  // =========================

  async saveDataSourceConnection(connection: {
    id: string;
    name: string;
    type: string;
    category: string;
    config: string;
    status: string;
    lastTested?: string;
    errorMessage?: string;
  }): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `INSERT OR REPLACE INTO data_source_connections
       (id, name, type, category, config, status, updated_at, last_tested, error_message)
       VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP, $7, $8)`,
      [
        connection.id,
        connection.name,
        connection.type,
        connection.category,
        connection.config,
        connection.status,
        connection.lastTested || null,
        connection.errorMessage || null,
      ]
    );
  }

  async getDataSourceConnection(id: string): Promise<any | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<any[]>(
      'SELECT * FROM data_source_connections WHERE id = $1',
      [id]
    );
    return results[0] || null;
  }

  async getAllDataSourceConnections(): Promise<any[]> {
    await this.ensureInitialized();
    return await this.db!.select<any[]>(
      'SELECT * FROM data_source_connections ORDER BY created_at DESC'
    );
  }

  async getDataSourceConnectionsByCategory(category: string): Promise<any[]> {
    await this.ensureInitialized();
    return await this.db!.select<any[]>(
      'SELECT * FROM data_source_connections WHERE category = $1 ORDER BY created_at DESC',
      [category]
    );
  }

  async getDataSourceConnectionsByType(type: string): Promise<any[]> {
    await this.ensureInitialized();
    return await this.db!.select<any[]>(
      'SELECT * FROM data_source_connections WHERE type = $1',
      [type]
    );
  }

  async updateDataSourceConnection(
    id: string,
    updates: {
      name?: string;
      config?: string;
      status?: string;
      lastTested?: string;
      errorMessage?: string;
    }
  ): Promise<void> {
    await this.ensureInitialized();

    const fields: string[] = [];
    const values: any[] = [];
    let paramIndex = 1;

    if (updates.name !== undefined) {
      fields.push(`name = $${paramIndex++}`);
      values.push(updates.name);
    }
    if (updates.config !== undefined) {
      fields.push(`config = $${paramIndex++}`);
      values.push(updates.config);
    }
    if (updates.status !== undefined) {
      fields.push(`status = $${paramIndex++}`);
      values.push(updates.status);
    }
    if (updates.lastTested !== undefined) {
      fields.push(`last_tested = $${paramIndex++}`);
      values.push(updates.lastTested);
    }
    if (updates.errorMessage !== undefined) {
      fields.push(`error_message = $${paramIndex++}`);
      values.push(updates.errorMessage);
    }

    if (fields.length === 0) return;

    fields.push(`updated_at = CURRENT_TIMESTAMP`);
    values.push(id);

    const sql = `UPDATE data_source_connections SET ${fields.join(', ')} WHERE id = $${paramIndex}`;
    await this.db!.execute(sql, values);
  }

  async deleteDataSourceConnection(id: string): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM data_source_connections WHERE id = $1', [id]);
  }

  // =========================
  // FORUM CACHE METHODS
  // =========================

  /**
   * Cache forum categories
   */
  async cacheForumCategories(categories: any[]): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute(
      `INSERT OR REPLACE INTO forum_categories_cache (id, data, cached_at)
       VALUES (1, $1, CURRENT_TIMESTAMP)`,
      [JSON.stringify(categories)]
    );
  }

  /**
   * Get cached forum categories
   */
  async getCachedForumCategories(maxAgeMinutes: number = 5): Promise<any[] | null> {
    await this.ensureInitialized();

    const results = await this.db!.select<{ data: string; cached_at: string }[]>(
      `SELECT data, cached_at FROM forum_categories_cache
       WHERE id = 1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`
    );

    if (!results[0]) return null;

    return JSON.parse(results[0].data);
  }

  /**
   * Cache forum posts by category and sort type
   */
  async cacheForumPosts(categoryId: number | null, sortBy: string, posts: any[]): Promise<void> {
    await this.ensureInitialized();

    const cacheKey = `${categoryId || 'all'}_${sortBy}`;
    await this.db!.execute(
      `INSERT OR REPLACE INTO forum_posts_cache (cache_key, category_id, sort_by, data, cached_at)
       VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP)`,
      [cacheKey, categoryId, sortBy, JSON.stringify(posts)]
    );
  }

  /**
   * Get cached forum posts
   */
  async getCachedForumPosts(categoryId: number | null, sortBy: string, maxAgeMinutes: number = 5): Promise<any[] | null> {
    await this.ensureInitialized();

    const cacheKey = `${categoryId || 'all'}_${sortBy}`;
    const results = await this.db!.select<{ data: string; cached_at: string }[]>(
      `SELECT data, cached_at FROM forum_posts_cache
       WHERE cache_key = $1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
      [cacheKey]
    );

    if (!results[0]) return null;

    return JSON.parse(results[0].data);
  }

  /**
   * Cache forum statistics
   */
  async cacheForumStats(stats: any): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute(
      `INSERT OR REPLACE INTO forum_stats_cache (id, data, cached_at)
       VALUES (1, $1, CURRENT_TIMESTAMP)`,
      [JSON.stringify(stats)]
    );
  }

  /**
   * Get cached forum statistics
   */
  async getCachedForumStats(maxAgeMinutes: number = 5): Promise<any | null> {
    await this.ensureInitialized();

    const results = await this.db!.select<{ data: string; cached_at: string }[]>(
      `SELECT data, cached_at FROM forum_stats_cache
       WHERE id = 1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`
    );

    if (!results[0]) return null;

    return JSON.parse(results[0].data);
  }

  /**
   * Cache forum post details with comments
   */
  async cacheForumPostDetails(postUuid: string, post: any, comments: any[]): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute(
      `INSERT OR REPLACE INTO forum_post_details_cache (post_uuid, data, comments_data, cached_at)
       VALUES ($1, $2, $3, CURRENT_TIMESTAMP)`,
      [postUuid, JSON.stringify(post), JSON.stringify(comments)]
    );
  }

  /**
   * Get cached forum post details
   */
  async getCachedForumPostDetails(postUuid: string, maxAgeMinutes: number = 5): Promise<{ post: any; comments: any[] } | null> {
    await this.ensureInitialized();

    const results = await this.db!.select<{ data: string; comments_data: string; cached_at: string }[]>(
      `SELECT data, comments_data, cached_at FROM forum_post_details_cache
       WHERE post_uuid = $1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
      [postUuid]
    );

    if (!results[0]) return null;

    return {
      post: JSON.parse(results[0].data),
      comments: JSON.parse(results[0].comments_data)
    };
  }

  /**
   * Clear all forum cache
   */
  async clearForumCache(): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM forum_categories_cache');
    await this.db!.execute('DELETE FROM forum_posts_cache');
    await this.db!.execute('DELETE FROM forum_stats_cache');
    await this.db!.execute('DELETE FROM forum_post_details_cache');
  }

  /**
   * Clear expired forum cache
   */
  async clearExpiredForumCache(maxAgeMinutes: number = 60): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `DELETE FROM forum_categories_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
    );

    await this.db!.execute(
      `DELETE FROM forum_posts_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
    );

    await this.db!.execute(
      `DELETE FROM forum_stats_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
    );

    await this.db!.execute(
      `DELETE FROM forum_post_details_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
    );
  }

  // =========================
  // CONTEXT RECORDING METHODS
  // =========================

  /**
   * Save recorded context data
   */
  async saveRecordedContext(context: Omit<RecordedContext, 'created_at'>): Promise<void> {
    await this.ensureInitialized();

    const dataSize = new TextEncoder().encode(context.raw_data).length;

    await this.db!.execute(
      `INSERT INTO recorded_contexts (id, tab_name, data_type, label, raw_data, metadata, data_size, tags)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
      [
        context.id,
        context.tab_name,
        context.data_type,
        context.label || null,
        context.raw_data,
        context.metadata || null,
        dataSize,
        context.tags || null
      ]
    );
  }

  /**
   * Get all recorded contexts with optional filters
   */
  async getRecordedContexts(filters?: {
    tabName?: string;
    dataType?: string;
    tags?: string[];
    limit?: number;
  }): Promise<RecordedContext[]> {
    await this.ensureInitialized();

    let sql = 'SELECT * FROM recorded_contexts WHERE 1=1';
    const params: any[] = [];
    let paramIndex = 1;

    if (filters?.tabName) {
      sql += ` AND tab_name = $${paramIndex++}`;
      params.push(filters.tabName);
    }

    if (filters?.dataType) {
      sql += ` AND data_type = $${paramIndex++}`;
      params.push(filters.dataType);
    }

    if (filters?.tags && filters.tags.length > 0) {
      const tagConditions = filters.tags.map(() => `tags LIKE $${paramIndex++}`).join(' OR ');
      sql += ` AND (${tagConditions})`;
      params.push(...filters.tags.map(tag => `%${tag}%`));
    }

    sql += ' ORDER BY created_at DESC';

    if (filters?.limit) {
      sql += ` LIMIT $${paramIndex}`;
      params.push(filters.limit);
    }

    return await this.db!.select<RecordedContext[]>(sql, params);
  }

  /**
   * Get context by ID
   */
  async getRecordedContext(id: string): Promise<RecordedContext | null> {
    await this.ensureInitialized();

    const results = await this.db!.select<RecordedContext[]>(
      'SELECT * FROM recorded_contexts WHERE id = $1',
      [id]
    );

    return results[0] || null;
  }

  /**
   * Delete recorded context
   */
  async deleteRecordedContext(id: string): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute('DELETE FROM recorded_contexts WHERE id = $1', [id]);
  }

  /**
   * Clear old contexts
   */
  async clearOldContexts(maxAgeDays: number): Promise<void> {
    await this.ensureInitialized();
    await this.db!.execute(
      `DELETE FROM recorded_contexts
       WHERE datetime(created_at) < datetime('now', '-${maxAgeDays} days')`
    );
  }

  /**
   * Get storage statistics
   */
  async getContextStorageStats(): Promise<{ count: number; totalSize: number }> {
    await this.ensureInitialized();

    const result = await this.db!.select<{ count: number; total_size: number }[]>(
      'SELECT COUNT(*) as count, SUM(data_size) as total_size FROM recorded_contexts'
    );

    return {
      count: result[0]?.count || 0,
      totalSize: result[0]?.total_size || 0
    };
  }

  /**
   * Start recording session
   */
  async startRecordingSession(session: Omit<RecordingSession, 'started_at'>): Promise<void> {
    await this.ensureInitialized();

    // End any existing active session for this tab
    await this.db!.execute(
      `UPDATE recording_sessions SET is_active = 0, ended_at = CURRENT_TIMESTAMP
       WHERE tab_name = $1 AND is_active = 1`,
      [session.tab_name]
    );

    // Start new session
    await this.db!.execute(
      `INSERT INTO recording_sessions (id, tab_name, is_active, auto_record, filters)
       VALUES ($1, $2, $3, $4, $5)`,
      [
        session.id,
        session.tab_name,
        session.is_active ? 1 : 0,
        session.auto_record ? 1 : 0,
        session.filters || null
      ]
    );
  }

  /**
   * Get active recording session for tab
   */
  async getActiveRecordingSession(tabName: string): Promise<RecordingSession | null> {
    await this.ensureInitialized();

    const results = await this.db!.select<RecordingSession[]>(
      'SELECT * FROM recording_sessions WHERE tab_name = $1 AND is_active = 1 LIMIT 1',
      [tabName]
    );

    return results[0] || null;
  }

  /**
   * Stop recording session
   */
  async stopRecordingSession(tabName: string): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `UPDATE recording_sessions SET is_active = 0, ended_at = CURRENT_TIMESTAMP
       WHERE tab_name = $1 AND is_active = 1`,
      [tabName]
    );
  }

  /**
   * Link context to chat session
   */
  async linkContextToChat(chatSessionUuid: string, contextId: string): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `INSERT OR REPLACE INTO chat_context_links (chat_session_uuid, context_id)
       VALUES ($1, $2)`,
      [chatSessionUuid, contextId]
    );
  }

  /**
   * Unlink context from chat session
   */
  async unlinkContextFromChat(chatSessionUuid: string, contextId: string): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      'DELETE FROM chat_context_links WHERE chat_session_uuid = $1 AND context_id = $2',
      [chatSessionUuid, contextId]
    );
  }

  /**
   * Get active contexts for chat session
   */
  async getChatContexts(chatSessionUuid: string): Promise<RecordedContext[]> {
    await this.ensureInitialized();

    return await this.db!.select<RecordedContext[]>(
      `SELECT rc.* FROM recorded_contexts rc
       INNER JOIN chat_context_links ccl ON rc.id = ccl.context_id
       WHERE ccl.chat_session_uuid = $1 AND ccl.is_active = 1
       ORDER BY ccl.added_at DESC`,
      [chatSessionUuid]
    );
  }

  /**
   * Toggle context active state in chat
   */
  async toggleChatContextActive(chatSessionUuid: string, contextId: string): Promise<void> {
    await this.ensureInitialized();

    await this.db!.execute(
      `UPDATE chat_context_links SET is_active = NOT is_active
       WHERE chat_session_uuid = $1 AND context_id = $2`,
      [chatSessionUuid, contextId]
    );
  }

  // =========================
  // BACKTESTING METHODS
  // =========================

  async saveBacktestingProvider(provider: Omit<BacktestingProvider, 'created_at' | 'updated_at'>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        `INSERT OR REPLACE INTO backtesting_providers (id, name, adapter_type, config, enabled, is_active, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP)`,
        [provider.id, provider.name, provider.adapter_type, provider.config, provider.enabled ? 1 : 0, provider.is_active ? 1 : 0]
      );

      return { success: true, message: 'Backtesting provider saved successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to save backtesting provider:', error);
      return { success: false, message: 'Failed to save backtesting provider' };
    }
  }

  async getBacktestingProviders(): Promise<BacktestingProvider[]> {
    await this.ensureInitialized();
    return await this.db!.select<BacktestingProvider[]>('SELECT * FROM backtesting_providers ORDER BY name');
  }

  async getBacktestingProvider(name: string): Promise<BacktestingProvider | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<BacktestingProvider[]>(
      'SELECT * FROM backtesting_providers WHERE name = $1',
      [name]
    );
    return results[0] || null;
  }

  async getActiveBacktestingProvider(): Promise<BacktestingProvider | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<BacktestingProvider[]>(
      'SELECT * FROM backtesting_providers WHERE is_active = 1 LIMIT 1'
    );
    return results[0] || null;
  }

  async setActiveBacktestingProvider(name: string): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      // Deactivate all providers first
      await this.db!.execute('UPDATE backtesting_providers SET is_active = 0');

      // Activate the specified provider
      await this.db!.execute(
        'UPDATE backtesting_providers SET is_active = 1, updated_at = CURRENT_TIMESTAMP WHERE name = $1',
        [name]
      );

      return { success: true, message: `Activated backtesting provider: ${name}` };
    } catch (error) {
      console.error('[SQLite] Failed to set active provider:', error);
      return { success: false, message: 'Failed to set active provider' };
    }
  }

  async deleteBacktestingProvider(name: string): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        'DELETE FROM backtesting_providers WHERE name = $1',
        [name]
      );

      return { success: true, message: 'Provider deleted successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to delete provider:', error);
      return { success: false, message: 'Failed to delete provider' };
    }
  }

  async saveBacktestingStrategy(strategy: Omit<BacktestingStrategy, 'created_at' | 'updated_at'>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        `INSERT OR REPLACE INTO backtesting_strategies
         (id, name, description, version, author, provider_type, strategy_type, strategy_definition, tags, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, CURRENT_TIMESTAMP)`,
        [
          strategy.id,
          strategy.name,
          strategy.description || null,
          strategy.version,
          strategy.author || null,
          strategy.provider_type,
          strategy.strategy_type,
          strategy.strategy_definition,
          strategy.tags || null,
        ]
      );

      return { success: true, message: 'Strategy saved successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to save strategy:', error);
      return { success: false, message: 'Failed to save strategy' };
    }
  }

  async getBacktestingStrategies(): Promise<BacktestingStrategy[]> {
    await this.ensureInitialized();
    return await this.db!.select<BacktestingStrategy[]>('SELECT * FROM backtesting_strategies ORDER BY updated_at DESC');
  }

  async getBacktestingStrategy(id: string): Promise<BacktestingStrategy | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<BacktestingStrategy[]>(
      'SELECT * FROM backtesting_strategies WHERE id = $1',
      [id]
    );
    return results[0] || null;
  }

  async getBacktestingStrategiesByProvider(providerType: string): Promise<BacktestingStrategy[]> {
    await this.ensureInitialized();
    return await this.db!.select<BacktestingStrategy[]>(
      'SELECT * FROM backtesting_strategies WHERE provider_type = $1 ORDER BY updated_at DESC',
      [providerType]
    );
  }

  async deleteBacktestingStrategy(id: string): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        'DELETE FROM backtesting_strategies WHERE id = $1',
        [id]
      );

      return { success: true, message: 'Strategy deleted successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to delete strategy:', error);
      return { success: false, message: 'Failed to delete strategy' };
    }
  }

  async saveBacktestRun(run: Omit<BacktestRun, 'created_at' | 'completed_at' | 'duration_seconds'>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        `INSERT INTO backtest_runs
         (id, strategy_id, provider_name, config, results, status, performance_metrics, error_message)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
        [
          run.id,
          run.strategy_id || null,
          run.provider_name,
          run.config,
          run.results || null,
          run.status,
          run.performance_metrics || null,
          run.error_message || null,
        ]
      );

      return { success: true, message: 'Backtest run saved successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to save backtest run:', error);
      return { success: false, message: 'Failed to save backtest run' };
    }
  }

  async updateBacktestRun(id: string, updates: Partial<BacktestRun>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      const fields: string[] = [];
      const values: any[] = [];

      if (updates.results !== undefined) {
        fields.push('results = $' + (values.length + 1));
        values.push(updates.results);
      }
      if (updates.status !== undefined) {
        fields.push('status = $' + (values.length + 1));
        values.push(updates.status);
      }
      if (updates.performance_metrics !== undefined) {
        fields.push('performance_metrics = $' + (values.length + 1));
        values.push(updates.performance_metrics);
      }
      if (updates.error_message !== undefined) {
        fields.push('error_message = $' + (values.length + 1));
        values.push(updates.error_message);
      }
      if (updates.completed_at !== undefined) {
        fields.push('completed_at = $' + (values.length + 1));
        values.push(updates.completed_at);
      }
      if (updates.duration_seconds !== undefined) {
        fields.push('duration_seconds = $' + (values.length + 1));
        values.push(updates.duration_seconds);
      }

      if (fields.length === 0) {
        return { success: true, message: 'No updates to apply' };
      }

      values.push(id);
      await this.db!.execute(
        `UPDATE backtest_runs SET ${fields.join(', ')} WHERE id = $${values.length}`,
        values
      );

      return { success: true, message: 'Backtest run updated successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to update backtest run:', error);
      return { success: false, message: 'Failed to update backtest run' };
    }
  }

  async getBacktestRuns(limit?: number): Promise<BacktestRun[]> {
    await this.ensureInitialized();
    const sql = limit
      ? `SELECT * FROM backtest_runs ORDER BY created_at DESC LIMIT ${limit}`
      : 'SELECT * FROM backtest_runs ORDER BY created_at DESC';
    return await this.db!.select<BacktestRun[]>(sql);
  }

  async getBacktestRun(id: string): Promise<BacktestRun | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<BacktestRun[]>(
      'SELECT * FROM backtest_runs WHERE id = $1',
      [id]
    );
    return results[0] || null;
  }

  async getBacktestRunsByStrategy(strategyId: string): Promise<BacktestRun[]> {
    await this.ensureInitialized();
    return await this.db!.select<BacktestRun[]>(
      'SELECT * FROM backtest_runs WHERE strategy_id = $1 ORDER BY created_at DESC',
      [strategyId]
    );
  }

  async getBacktestRunsByProvider(providerName: string): Promise<BacktestRun[]> {
    await this.ensureInitialized();
    return await this.db!.select<BacktestRun[]>(
      'SELECT * FROM backtest_runs WHERE provider_name = $1 ORDER BY created_at DESC',
      [providerName]
    );
  }

  async deleteBacktestRun(id: string): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        'DELETE FROM backtest_runs WHERE id = $1',
        [id]
      );

      return { success: true, message: 'Backtest run deleted successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to delete backtest run:', error);
      return { success: false, message: 'Failed to delete backtest run' };
    }
  }

  async saveOptimizationRun(run: Omit<OptimizationRun, 'created_at' | 'completed_at' | 'duration_seconds'>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        `INSERT INTO optimization_runs
         (id, strategy_id, provider_name, parameter_grid, objective, best_parameters, best_result, all_results, iterations, status, error_message)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)`,
        [
          run.id,
          run.strategy_id || null,
          run.provider_name,
          run.parameter_grid,
          run.objective,
          run.best_parameters || null,
          run.best_result || null,
          run.all_results || null,
          run.iterations || null,
          run.status,
          run.error_message || null,
        ]
      );

      return { success: true, message: 'Optimization run saved successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to save optimization run:', error);
      return { success: false, message: 'Failed to save optimization run' };
    }
  }

  async updateOptimizationRun(id: string, updates: Partial<OptimizationRun>): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      const fields: string[] = [];
      const values: any[] = [];

      if (updates.best_parameters !== undefined) {
        fields.push('best_parameters = $' + (values.length + 1));
        values.push(updates.best_parameters);
      }
      if (updates.best_result !== undefined) {
        fields.push('best_result = $' + (values.length + 1));
        values.push(updates.best_result);
      }
      if (updates.all_results !== undefined) {
        fields.push('all_results = $' + (values.length + 1));
        values.push(updates.all_results);
      }
      if (updates.iterations !== undefined) {
        fields.push('iterations = $' + (values.length + 1));
        values.push(updates.iterations);
      }
      if (updates.status !== undefined) {
        fields.push('status = $' + (values.length + 1));
        values.push(updates.status);
      }
      if (updates.error_message !== undefined) {
        fields.push('error_message = $' + (values.length + 1));
        values.push(updates.error_message);
      }
      if (updates.completed_at !== undefined) {
        fields.push('completed_at = $' + (values.length + 1));
        values.push(updates.completed_at);
      }
      if (updates.duration_seconds !== undefined) {
        fields.push('duration_seconds = $' + (values.length + 1));
        values.push(updates.duration_seconds);
      }

      if (fields.length === 0) {
        return { success: true, message: 'No updates to apply' };
      }

      values.push(id);
      await this.db!.execute(
        `UPDATE optimization_runs SET ${fields.join(', ')} WHERE id = $${values.length}`,
        values
      );

      return { success: true, message: 'Optimization run updated successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to update optimization run:', error);
      return { success: false, message: 'Failed to update optimization run' };
    }
  }

  async getOptimizationRuns(limit?: number): Promise<OptimizationRun[]> {
    await this.ensureInitialized();
    const sql = limit
      ? `SELECT * FROM optimization_runs ORDER BY created_at DESC LIMIT ${limit}`
      : 'SELECT * FROM optimization_runs ORDER BY created_at DESC';
    return await this.db!.select<OptimizationRun[]>(sql);
  }

  async getOptimizationRun(id: string): Promise<OptimizationRun | null> {
    await this.ensureInitialized();
    const results = await this.db!.select<OptimizationRun[]>(
      'SELECT * FROM optimization_runs WHERE id = $1',
      [id]
    );
    return results[0] || null;
  }

  async deleteOptimizationRun(id: string): Promise<{ success: boolean; message: string }> {
    try {
      await this.ensureInitialized();

      await this.db!.execute(
        'DELETE FROM optimization_runs WHERE id = $1',
        [id]
      );

      return { success: true, message: 'Optimization run deleted successfully' };
    } catch (error) {
      console.error('[SQLite] Failed to delete optimization run:', error);
      return { success: false, message: 'Failed to delete optimization run' };
    }
  }

  // =========================
  // UTILITY METHODS
  // =========================

  private async ensureInitialized(): Promise<void> {
    if (!this.db) {
      await this.initialize();
    }
  }

  isReady(): boolean {
    return this.db !== null;
  }

  /**
   * Execute raw SQL statement
   */
  async execute(sql: string, params?: any[]): Promise<any> {
    await this.ensureInitialized();
    return await this.db!.execute(sql, params);
  }

  /**
   * Execute raw SQL select query
   */
  async select<T = any>(sql: string, params?: any[]): Promise<T> {
    await this.ensureInitialized();
    return await this.db!.select<T>(sql, params);
  }

  async healthCheck(): Promise<{ healthy: boolean; message: string }> {
    try {
      await this.ensureInitialized();
      const results = await this.db!.select<{ result: number }[]>('SELECT 1 as result');

      if (results[0]?.result === 1) {
        return { healthy: true, message: 'Database connection healthy' };
      }

      return { healthy: false, message: 'Unexpected query result' };
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : String(error);
      return { healthy: false, message: `Health check failed: ${errorMsg}` };
    }
  }
}

// Export singleton instance
export const sqliteService = new SQLiteService();
export default sqliteService;
