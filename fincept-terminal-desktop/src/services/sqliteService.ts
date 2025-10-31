// Unified SQLite Database Service using Tauri SQL Plugin
// Single database for all terminal data: credentials, settings, LLM configs, chat history

import Database from '@tauri-apps/plugin-sql';

// Query result types from Tauri SQL plugin
interface QueryResult {
  rowsAffected: number;
  lastInsertId?: number;
}

// Database Types
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

export interface Setting {
  setting_key: string;
  setting_value: string;
  category?: string; // 'terminal', 'notification', 'profile', 'llm', etc.
  updated_at?: string;
}

export interface LLMConfig {
  provider: string; // 'openai', 'gemini', 'deepseek', 'ollama', 'openrouter'
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

class SQLiteService {
  private db: Database | null = null;
  private dbPath = 'sqlite:fincept_terminal.db';
  private isInitialized = false;
  private initializationPromise: Promise<void> | null = null;

  // Initialize database and create tables
  async initialize(): Promise<void> {
    // Prevent multiple simultaneous initialization attempts
    if (this.isInitialized && this.db) {
      console.log('Database already initialized');
      return;
    }

    // If initialization is in progress, wait for it
    if (this.initializationPromise) {
      console.log('Waiting for ongoing initialization...');
      return this.initializationPromise;
    }

    // Start new initialization
    this.initializationPromise = this._performInitialization();

    try {
      await this.initializationPromise;
    } finally {
      this.initializationPromise = null;
    }
  }

  private async _performInitialization(): Promise<void> {
    try {
      console.log(`Initializing SQLite database at: ${this.dbPath}`);

      // Load database using Tauri SQL plugin
      this.db = await Database.load(this.dbPath);
      console.log('Database connection established');

      // Create schema
      await this.createTables();
      console.log('Database tables created/verified');

      // Seed default data
      await this.seedDefaultSettings();
      console.log('Default settings seeded');

      this.isInitialized = true;
      console.log('✓ SQLite database initialized successfully');
    } catch (error) {
      this.isInitialized = false;
      this.db = null;
      console.error('✗ Failed to initialize SQLite database:', error);

      // Provide more detailed error information
      if (error instanceof Error) {
        throw new Error(`Database initialization failed: ${error.message}`);
      }
      throw new Error(`Database initialization failed: ${String(error)}`);
    }
  }

  private async createTables(): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    // Credentials table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS credentials (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        service_name TEXT NOT NULL,
        username TEXT NOT NULL,
        password TEXT NOT NULL,
        api_key TEXT,
        api_secret TEXT,
        additional_data TEXT,
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now')),
        UNIQUE(service_name, username)
      )
    `);

    // Settings table (for terminal config, notifications, profile, etc.)
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS settings (
        setting_key TEXT PRIMARY KEY,
        setting_value TEXT NOT NULL,
        category TEXT,
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    // LLM configurations table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS llm_configs (
        provider TEXT PRIMARY KEY,
        api_key TEXT,
        base_url TEXT,
        model TEXT NOT NULL,
        is_active INTEGER DEFAULT 0,
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    // LLM global settings (single row)
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS llm_global_settings (
        id INTEGER PRIMARY KEY CHECK (id = 1),
        temperature REAL DEFAULT 0.7,
        max_tokens INTEGER DEFAULT 2048,
        system_prompt TEXT,
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    // Chat sessions table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS chat_sessions (
        session_uuid TEXT PRIMARY KEY,
        title TEXT NOT NULL,
        message_count INTEGER DEFAULT 0,
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    // Chat messages table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS chat_messages (
        id TEXT PRIMARY KEY,
        session_uuid TEXT NOT NULL,
        role TEXT NOT NULL,
        content TEXT NOT NULL,
        timestamp TEXT DEFAULT (datetime('now')),
        provider TEXT,
        model TEXT,
        tokens_used INTEGER,
        FOREIGN KEY (session_uuid) REFERENCES chat_sessions(session_uuid) ON DELETE CASCADE
      )
    `);

    // MCP servers table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS mcp_servers (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        description TEXT,
        command TEXT NOT NULL,
        args TEXT NOT NULL,
        env TEXT,
        category TEXT,
        icon TEXT,
        enabled INTEGER DEFAULT 1,
        auto_start INTEGER DEFAULT 0,
        status TEXT DEFAULT 'stopped',
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    // MCP tools cache table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS mcp_tools (
        id TEXT PRIMARY KEY,
        server_id TEXT NOT NULL,
        name TEXT NOT NULL,
        description TEXT,
        input_schema TEXT,
        created_at TEXT DEFAULT (datetime('now')),
        FOREIGN KEY (server_id) REFERENCES mcp_servers(id) ON DELETE CASCADE
      )
    `);

    // MCP tool usage logs
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS mcp_tool_usage (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        server_id TEXT NOT NULL,
        tool_name TEXT NOT NULL,
        args TEXT,
        result TEXT,
        success INTEGER,
        execution_time INTEGER,
        error TEXT,
        timestamp TEXT DEFAULT (datetime('now')),
        FOREIGN KEY (server_id) REFERENCES mcp_servers(id) ON DELETE CASCADE
      )
    `);

    // Market data cache table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS market_data_cache (
        symbol TEXT NOT NULL,
        category TEXT NOT NULL,
        quote_data TEXT NOT NULL,
        cached_at TEXT DEFAULT (datetime('now')),
        PRIMARY KEY (symbol, category)
      )
    `);

    // Portfolio tables
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS portfolios (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        owner TEXT NOT NULL,
        currency TEXT DEFAULT 'USD',
        description TEXT,
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS portfolio_assets (
        id TEXT PRIMARY KEY,
        portfolio_id TEXT NOT NULL,
        symbol TEXT NOT NULL,
        quantity REAL NOT NULL,
        avg_buy_price REAL NOT NULL,
        first_purchase_date TEXT,
        last_updated TEXT DEFAULT (datetime('now')),
        notes TEXT,
        FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE,
        UNIQUE(portfolio_id, symbol)
      )
    `);

    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS portfolio_transactions (
        id TEXT PRIMARY KEY,
        portfolio_id TEXT NOT NULL,
        symbol TEXT NOT NULL,
        transaction_type TEXT NOT NULL,
        quantity REAL NOT NULL,
        price REAL NOT NULL,
        total_value REAL NOT NULL,
        transaction_date TEXT NOT NULL,
        notes TEXT,
        created_at TEXT DEFAULT (datetime('now')),
        FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
      )
    `);

    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS portfolio_snapshots (
        id TEXT PRIMARY KEY,
        portfolio_id TEXT NOT NULL,
        total_value REAL NOT NULL,
        total_cost_basis REAL NOT NULL,
        total_pnl REAL NOT NULL,
        total_pnl_percent REAL NOT NULL,
        snapshot_date TEXT DEFAULT (datetime('now')),
        snapshot_data TEXT NOT NULL,
        FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
      )
    `);

    // Watchlist tables
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS watchlists (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        description TEXT,
        color TEXT DEFAULT '#FFA500',
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS watchlist_stocks (
        id TEXT PRIMARY KEY,
        watchlist_id TEXT NOT NULL,
        symbol TEXT NOT NULL,
        added_at TEXT DEFAULT (datetime('now')),
        notes TEXT,
        FOREIGN KEY (watchlist_id) REFERENCES watchlists(id) ON DELETE CASCADE,
        UNIQUE(watchlist_id, symbol)
      )
    `);

    // WebSocket Provider Configurations table
    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS ws_provider_configs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        provider_name TEXT NOT NULL UNIQUE,
        enabled INTEGER DEFAULT 1,
        api_key TEXT,
        api_secret TEXT,
        endpoint TEXT,
        config_data TEXT,
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now'))
      )
    `);

    // Create indexes
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_credentials_service ON credentials(service_name)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_settings_category ON settings(category)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_messages_session ON chat_messages(session_uuid)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_sessions_updated ON chat_sessions(updated_at DESC)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_mcp_tools_server ON mcp_tools(server_id)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_mcp_usage_server ON mcp_tool_usage(server_id)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_mcp_usage_timestamp ON mcp_tool_usage(timestamp DESC)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_market_cache_category ON market_data_cache(category)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_market_cache_time ON market_data_cache(cached_at)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_portfolio_assets_portfolio ON portfolio_assets(portfolio_id)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_portfolio_transactions_portfolio ON portfolio_transactions(portfolio_id)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_portfolio_transactions_date ON portfolio_transactions(transaction_date DESC)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_portfolio_snapshots_portfolio ON portfolio_snapshots(portfolio_id)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_portfolio_snapshots_date ON portfolio_snapshots(snapshot_date DESC)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_watchlist_stocks_watchlist ON watchlist_stocks(watchlist_id)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_watchlist_stocks_symbol ON watchlist_stocks(symbol)`);
    await this.db.execute(`CREATE INDEX IF NOT EXISTS idx_ws_providers_enabled ON ws_provider_configs(enabled)`);
  }

  private async seedDefaultSettings(): Promise<void> {
    if (!this.db) return;

    // Default LLM global settings
    const result = await this.db.select<Array<{ count: number }>>(
      'SELECT COUNT(*) as count FROM llm_global_settings'
    );

    if (result[0].count === 0) {
      await this.db.execute(`
        INSERT INTO llm_global_settings (id, temperature, max_tokens, system_prompt)
        VALUES (1, 0.7, 2048, 'You are Fincept AI, a helpful financial analysis assistant. You provide insights on markets, trading, portfolio management, and financial data analysis.')
      `);
    }

    // Default LLM providers
    const providers = [
      { provider: 'openai', model: 'gpt-4o-mini', is_active: 0 },
      { provider: 'gemini', model: 'gemini-2.0-flash-exp', is_active: 0 },
      { provider: 'deepseek', model: 'deepseek-chat', is_active: 0 },
      { provider: 'ollama', model: 'llama3.2:latest', base_url: 'http://localhost:11434', is_active: 1 },
      { provider: 'openrouter', model: 'meta-llama/llama-3.1-8b-instruct:free', is_active: 0 }
    ];

    for (const prov of providers) {
      const exists = await this.db.select<Array<{ count: number }>>(
        'SELECT COUNT(*) as count FROM llm_configs WHERE provider = $1',
        [prov.provider]
      );

      if (exists[0].count === 0) {
        await this.db.execute(
          `INSERT INTO llm_configs (provider, model, base_url, is_active) VALUES ($1, $2, $3, $4)`,
          [prov.provider, prov.model, prov.base_url || null, prov.is_active]
        );
      }
    }
  }

  // ==================== CREDENTIALS ====================

  async saveCredential(credential: Credential): Promise<{ success: boolean; message: string; id?: number }> {
    if (!this.db) throw new Error('Database not initialized');

    try {
      // Check if exists
      const existing = await this.db.select<Credential[]>(
        'SELECT id FROM credentials WHERE service_name = $1 AND username = $2',
        [credential.service_name, credential.username]
      );

      if (existing.length > 0) {
        // Update
        await this.db.execute(
          `UPDATE credentials SET password = $1, api_key = $2, api_secret = $3,
           additional_data = $4, updated_at = datetime('now') WHERE id = $5`,
          [credential.password, credential.api_key, credential.api_secret,
           credential.additional_data, existing[0].id]
        );
        return { success: true, message: 'Credential updated successfully', id: existing[0].id };
      } else {
        // Insert
        const result = await this.db.execute(
          `INSERT INTO credentials (service_name, username, password, api_key, api_secret, additional_data)
           VALUES ($1, $2, $3, $4, $5, $6)`,
          [credential.service_name, credential.username, credential.password,
           credential.api_key, credential.api_secret, credential.additional_data]
        );
        return { success: true, message: 'Credential saved successfully', id: result.lastInsertId || undefined };
      }
    } catch (error: any) {
      return { success: false, message: error.message };
    }
  }

  async getCredentials(): Promise<Credential[]> {
    if (!this.db) throw new Error('Database not initialized');
    return await this.db.select<Credential[]>(
      'SELECT * FROM credentials ORDER BY created_at DESC'
    );
  }

  async getCredentialByService(serviceName: string): Promise<Credential | null> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<Credential[]>(
      'SELECT * FROM credentials WHERE service_name = $1 LIMIT 1',
      [serviceName]
    );
    return result.length > 0 ? result[0] : null;
  }

  /**
   * Get API key for a specific service (simplified retrieval)
   */
  async getApiKey(serviceName: string): Promise<string | null> {
    const credential = await this.getCredentialByService(serviceName);
    return credential?.api_key || null;
  }

  async deleteCredential(id: number): Promise<{ success: boolean; message: string }> {
    if (!this.db) throw new Error('Database not initialized');
    try {
      await this.db.execute('DELETE FROM credentials WHERE id = $1', [id]);
      return { success: true, message: 'Credential deleted successfully' };
    } catch (error: any) {
      return { success: false, message: error.message };
    }
  }

  // ==================== SETTINGS ====================

  async saveSetting(key: string, value: string, category?: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    await this.db.execute(
      `INSERT OR REPLACE INTO settings (setting_key, setting_value, category, updated_at)
       VALUES ($1, $2, $3, datetime('now'))`,
      [key, value, category || null]
    );
  }

  async getSetting(key: string): Promise<string | null> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<Setting[]>(
      'SELECT setting_value FROM settings WHERE setting_key = $1',
      [key]
    );
    return result.length > 0 ? result[0].setting_value : null;
  }

  async getSettingsByCategory(category: string): Promise<Record<string, string>> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<Setting[]>(
      'SELECT setting_key, setting_value FROM settings WHERE category = $1',
      [category]
    );
    const settings: Record<string, string> = {};
    result.forEach(s => settings[s.setting_key] = s.setting_value);
    return settings;
  }

  async getAllSettings(): Promise<Record<string, string>> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<Setting[]>('SELECT setting_key, setting_value FROM settings');
    const settings: Record<string, string> = {};
    result.forEach(s => settings[s.setting_key] = s.setting_value);
    return settings;
  }

  // ==================== LLM CONFIGS ====================

  async getLLMConfigs(): Promise<LLMConfig[]> {
    if (!this.db) throw new Error('Database not initialized');
    return await this.db.select<LLMConfig[]>('SELECT * FROM llm_configs');
  }

  async getLLMConfig(provider: string): Promise<LLMConfig | null> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<LLMConfig[]>(
      'SELECT * FROM llm_configs WHERE provider = $1',
      [provider]
    );
    return result.length > 0 ? result[0] : null;
  }

  async getActiveLLMConfig(): Promise<LLMConfig | null> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<LLMConfig[]>(
      'SELECT * FROM llm_configs WHERE is_active = 1 LIMIT 1'
    );
    return result.length > 0 ? result[0] : null;
  }

  async saveLLMConfig(config: Partial<LLMConfig> & { provider: string }): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    await this.db.execute(
      `INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, updated_at)
       VALUES ($1, $2, $3, $4, $5, datetime('now'))`,
      [config.provider, config.api_key || null, config.base_url || null,
       config.model, config.is_active ? 1 : 0]
    );
  }

  async setActiveLLMProvider(provider: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    // Deactivate all
    await this.db.execute('UPDATE llm_configs SET is_active = 0');
    // Activate selected
    await this.db.execute('UPDATE llm_configs SET is_active = 1 WHERE provider = $1', [provider]);
  }

  async getLLMGlobalSettings(): Promise<LLMGlobalSettings> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<LLMGlobalSettings[]>(
      'SELECT temperature, max_tokens, system_prompt FROM llm_global_settings WHERE id = 1'
    );
    return result[0];
  }

  async saveLLMGlobalSettings(settings: Partial<LLMGlobalSettings>): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    const updates: string[] = [];
    const values: any[] = [];

    if (settings.temperature !== undefined) {
      updates.push('temperature = $' + (values.length + 1));
      values.push(settings.temperature);
    }
    if (settings.max_tokens !== undefined) {
      updates.push('max_tokens = $' + (values.length + 1));
      values.push(settings.max_tokens);
    }
    if (settings.system_prompt !== undefined) {
      updates.push('system_prompt = $' + (values.length + 1));
      values.push(settings.system_prompt);
    }

    if (updates.length > 0) {
      updates.push("updated_at = datetime('now')");
      await this.db.execute(
        `UPDATE llm_global_settings SET ${updates.join(', ')} WHERE id = 1`,
        values
      );
    }
  }

  // ==================== CHAT SESSIONS ====================

  async createChatSession(title: string): Promise<ChatSession> {
    if (!this.db) throw new Error('Database not initialized');
    const session_uuid = crypto.randomUUID();
    await this.db.execute(
      `INSERT INTO chat_sessions (session_uuid, title) VALUES ($1, $2)`,
      [session_uuid, title]
    );

    const result = await this.db.select<ChatSession[]>(
      'SELECT * FROM chat_sessions WHERE session_uuid = $1',
      [session_uuid]
    );
    return result[0];
  }

  async getChatSessions(limit: number = 100): Promise<ChatSession[]> {
    if (!this.db) throw new Error('Database not initialized');
    return await this.db.select<ChatSession[]>(
      'SELECT * FROM chat_sessions ORDER BY updated_at DESC LIMIT $1',
      [limit]
    );
  }

  async getChatSession(session_uuid: string): Promise<ChatSession | null> {
    if (!this.db) throw new Error('Database not initialized');
    const result = await this.db.select<ChatSession[]>(
      'SELECT * FROM chat_sessions WHERE session_uuid = $1',
      [session_uuid]
    );
    return result.length > 0 ? result[0] : null;
  }

  async updateChatSessionTitle(session_uuid: string, title: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    await this.db.execute(
      `UPDATE chat_sessions SET title = $1, updated_at = datetime('now') WHERE session_uuid = $2`,
      [title, session_uuid]
    );
  }

  async deleteChatSession(session_uuid: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    // Messages will be deleted by CASCADE
    await this.db.execute('DELETE FROM chat_sessions WHERE session_uuid = $1', [session_uuid]);
  }

  // ==================== CHAT MESSAGES ====================

  async addChatMessage(message: Omit<ChatMessage, 'id' | 'timestamp'>): Promise<ChatMessage> {
    if (!this.db) throw new Error('Database not initialized');
    const id = crypto.randomUUID();

    await this.db.execute(
      `INSERT INTO chat_messages (id, session_uuid, role, content, provider, model, tokens_used)
       VALUES ($1, $2, $3, $4, $5, $6, $7)`,
      [id, message.session_uuid, message.role, message.content,
       message.provider || null, message.model || null, message.tokens_used || null]
    );

    // Update session message count
    await this.db.execute(
      `UPDATE chat_sessions
       SET message_count = message_count + 1, updated_at = datetime('now')
       WHERE session_uuid = $1`,
      [message.session_uuid]
    );

    const result = await this.db.select<ChatMessage[]>(
      'SELECT * FROM chat_messages WHERE id = $1',
      [id]
    );
    return result[0];
  }

  async getChatMessages(session_uuid: string): Promise<ChatMessage[]> {
    if (!this.db) throw new Error('Database not initialized');
    return await this.db.select<ChatMessage[]>(
      'SELECT * FROM chat_messages WHERE session_uuid = $1 ORDER BY timestamp ASC',
      [session_uuid]
    );
  }

  async deleteChatMessage(id: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    // Get session_uuid before deleting
    const result = await this.db.select<ChatMessage[]>(
      'SELECT session_uuid FROM chat_messages WHERE id = $1',
      [id]
    );

    if (result.length > 0) {
      await this.db.execute('DELETE FROM chat_messages WHERE id = $1', [id]);

      // Update session message count
      await this.db.execute(
        `UPDATE chat_sessions
         SET message_count = message_count - 1
         WHERE session_uuid = $1`,
        [result[0].session_uuid]
      );
    }
  }

  async clearChatSessionMessages(session_uuid: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    await this.db.execute('DELETE FROM chat_messages WHERE session_uuid = $1', [session_uuid]);
    await this.db.execute(
      `UPDATE chat_sessions SET message_count = 0, updated_at = datetime('now') WHERE session_uuid = $1`,
      [session_uuid]
    );
  }

  // ==================== UTILITY ====================

  /**
   * Check if database is initialized and healthy
   */
  isReady(): boolean {
    return this.isInitialized && this.db !== null;
  }

  /**
   * Generic execute method for INSERT/UPDATE/DELETE/CREATE statements
   */
  async execute(sql: string, params?: any[]): Promise<QueryResult> {
    if (!this.db) throw new Error('Database not initialized');
    return await this.db.execute(sql, params);
  }

  /**
   * Generic select method for SELECT queries
   */
  async select<T = any>(sql: string, params?: any[]): Promise<T[]> {
    if (!this.db) throw new Error('Database not initialized');
    return await this.db.select<T[]>(sql, params);
  }

  /**
   * Get database connection status
   */
  getStatus(): { initialized: boolean; path: string; ready: boolean } {
    return {
      initialized: this.isInitialized,
      path: this.dbPath,
      ready: this.isReady()
    };
  }

  /**
   * Test database connection with a simple query
   */
  async healthCheck(): Promise<{ healthy: boolean; message: string }> {
    if (!this.db) {
      return { healthy: false, message: 'Database not initialized' };
    }

    try {
      // Simple query to verify database is responsive
      const result = await this.db.select<Array<{ result: number }>>(
        'SELECT 1 as result'
      );

      if (result && result[0]?.result === 1) {
        return { healthy: true, message: 'Database connection healthy' };
      }

      return { healthy: false, message: 'Unexpected query result' };
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : String(error);
      return { healthy: false, message: `Health check failed: ${errorMsg}` };
    }
  }

  /**
   * Close database connection
   */
  async close(): Promise<void> {
    if (this.db) {
      console.log('Closing database connection...');
      try {
        await this.db.close();
        console.log('✓ Database connection closed');
      } catch (error) {
        console.error('Error closing database:', error);
      } finally {
        this.db = null;
        this.isInitialized = false;
      }
    }
  }

  /**
   * Force reinitialize database (useful for recovery scenarios)
   */
  async reinitialize(): Promise<void> {
    console.log('Forcing database reinitialization...');
    await this.close();
    await this.initialize();
  }

  // ==================== MCP SERVER METHODS ====================

  /**
   * Add a new MCP server
   */
  async addMCPServer(server: {
    id: string;
    name: string;
    description: string;
    command: string;
    args: string;
    env?: string | null;
    category: string;
    icon: string;
    enabled: boolean;
    auto_start?: boolean;
  }): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execute(
      `INSERT INTO mcp_servers (id, name, description, command, args, env, category, icon, enabled, auto_start, status)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, 'stopped')`,
      [server.id, server.name, server.description, server.command, server.args, server.env, server.category, server.icon, server.enabled ? 1 : 0, server.auto_start ? 1 : 0]
    );
  }

  /**
   * Get all MCP servers
   */
  async getMCPServers(): Promise<Array<{
    id: string;
    name: string;
    description: string;
    command: string;
    args: string;
    env: string | null;
    category: string;
    icon: string;
    enabled: boolean;
    auto_start: boolean;
    status: string;
    created_at: string;
    updated_at: string;
  }>> {
    if (!this.db) throw new Error('Database not initialized');

    const servers = await this.db.select<Array<any>>(
      'SELECT * FROM mcp_servers ORDER BY created_at DESC'
    );

    return servers.map(s => ({
      ...s,
      enabled: s.enabled === 1,
      auto_start: s.auto_start === 1
    }));
  }

  /**
   * Get a single MCP server by ID
   */
  async getMCPServer(id: string): Promise<{
    id: string;
    name: string;
    description: string;
    command: string;
    args: string;
    env: string | null;
    category: string;
    icon: string;
    enabled: boolean;
    auto_start: boolean;
    status: string;
    created_at: string;
    updated_at: string;
  } | null> {
    if (!this.db) throw new Error('Database not initialized');

    const servers = await this.db.select<Array<any>>(
      'SELECT * FROM mcp_servers WHERE id = $1',
      [id]
    );

    if (servers.length === 0) return null;

    return {
      ...servers[0],
      enabled: servers[0].enabled === 1,
      auto_start: servers[0].auto_start === 1
    };
  }

  /**
   * Update MCP server status
   */
  async updateMCPServerStatus(id: string, status: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execute(
      `UPDATE mcp_servers SET status = $1, updated_at = datetime('now') WHERE id = $2`,
      [status, id]
    );
  }

  /**
   * Update MCP server configuration
   */
  async updateMCPServerConfig(id: string, updates: {
    env?: Record<string, string>;
    enabled?: boolean;
    auto_start?: boolean;
  }): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    const sets: string[] = [];
    const values: any[] = [];

    if (updates.env !== undefined) {
      sets.push('env = $' + (values.length + 1));
      values.push(JSON.stringify(updates.env));
    }

    if (updates.enabled !== undefined) {
      sets.push('enabled = $' + (values.length + 1));
      values.push(updates.enabled ? 1 : 0);
    }

    if (updates.auto_start !== undefined) {
      sets.push('auto_start = $' + (values.length + 1));
      values.push(updates.auto_start ? 1 : 0);
    }

    if (sets.length > 0) {
      sets.push('updated_at = datetime(\'now\')');
      values.push(id);

      await this.db.execute(
        `UPDATE mcp_servers SET ${sets.join(', ')} WHERE id = $${values.length}`,
        values
      );
    }
  }

  /**
   * Delete MCP server
   */
  async deleteMCPServer(id: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execute('DELETE FROM mcp_servers WHERE id = $1', [id]);
  }

  /**
   * Get all auto-start enabled servers
   */
  async getAutoStartServers(): Promise<Array<{
    id: string;
    name: string;
    description: string;
    command: string;
    args: string;
    env: string | null;
    category: string;
    icon: string;
    enabled: boolean;
    auto_start: boolean;
    status: string;
  }>> {
    if (!this.db) throw new Error('Database not initialized');

    const servers = await this.db.select<Array<any>>(
      'SELECT * FROM mcp_servers WHERE auto_start = 1 AND enabled = 1'
    );

    return servers.map(s => ({
      ...s,
      enabled: s.enabled === 1,
      auto_start: s.auto_start === 1
    }));
  }

  /**
   * Cache tools for an MCP server
   */
  async cacheMCPTools(serverId: string, tools: Array<{
    name: string;
    description?: string;
    inputSchema: any;
  }>): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    // Delete existing tools for this server
    await this.db.execute('DELETE FROM mcp_tools WHERE server_id = $1', [serverId]);

    // Insert new tools
    for (const tool of tools) {
      const toolId = `${serverId}__${tool.name}`;
      await this.db.execute(
        `INSERT INTO mcp_tools (id, server_id, name, description, input_schema)
         VALUES ($1, $2, $3, $4, $5)`,
        [toolId, serverId, tool.name, tool.description || '', JSON.stringify(tool.inputSchema)]
      );
    }
  }

  /**
   * Get all cached MCP tools
   */
  async getMCPTools(): Promise<Array<{
    id: string;
    server_id: string;
    name: string;
    description: string;
    input_schema: string;
    created_at: string;
  }>> {
    if (!this.db) throw new Error('Database not initialized');

    return await this.db.select<Array<any>>(
      'SELECT * FROM mcp_tools ORDER BY server_id, name'
    );
  }

  /**
   * Get tools for a specific server
   */
  async getMCPToolsForServer(serverId: string): Promise<Array<{
    id: string;
    server_id: string;
    name: string;
    description: string;
    input_schema: string;
    created_at: string;
  }>> {
    if (!this.db) throw new Error('Database not initialized');

    return await this.db.select<Array<any>>(
      'SELECT * FROM mcp_tools WHERE server_id = $1 ORDER BY name',
      [serverId]
    );
  }

  /**
   * Log MCP tool usage
   */
  async logMCPToolUsage(log: {
    serverId: string;
    toolName: string;
    args: string;
    result: string | null;
    success: boolean;
    executionTime: number;
    error?: string;
  }): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execute(
      `INSERT INTO mcp_tool_usage (server_id, tool_name, args, result, success, execution_time, error)
       VALUES ($1, $2, $3, $4, $5, $6, $7)`,
      [log.serverId, log.toolName, log.args, log.result, log.success ? 1 : 0, log.executionTime, log.error || null]
    );
  }

  /**
   * Get MCP server statistics
   */
  async getMCPServerStats(serverId: string): Promise<{
    toolCount: number;
    callsToday: number;
    lastUsed: string | null;
  }> {
    if (!this.db) throw new Error('Database not initialized');

    // Get tool count
    const toolCountResult = await this.db.select<Array<{ count: number }>>(
      'SELECT COUNT(*) as count FROM mcp_tools WHERE server_id = $1',
      [serverId]
    );

    // Get calls today
    const callsTodayResult = await this.db.select<Array<{ count: number }>>(
      `SELECT COUNT(*) as count FROM mcp_tool_usage
       WHERE server_id = $1 AND date(timestamp) = date('now')`,
      [serverId]
    );

    // Get last used
    const lastUsedResult = await this.db.select<Array<{ timestamp: string | null }>>(
      `SELECT timestamp FROM mcp_tool_usage
       WHERE server_id = $1
       ORDER BY timestamp DESC
       LIMIT 1`,
      [serverId]
    );

    return {
      toolCount: toolCountResult[0]?.count || 0,
      callsToday: callsTodayResult[0]?.count || 0,
      lastUsed: lastUsedResult[0]?.timestamp || null
    };
  }

  /**
   * Get recent MCP tool usage logs
   */
  async getMCPToolUsageLogs(limit: number = 50): Promise<Array<{
    id: number;
    server_id: string;
    tool_name: string;
    args: string;
    result: string;
    success: boolean;
    execution_time: number;
    error: string | null;
    timestamp: string;
  }>> {
    if (!this.db) throw new Error('Database not initialized');

    const logs = await this.db.select<Array<any>>(
      'SELECT * FROM mcp_tool_usage ORDER BY timestamp DESC LIMIT $1',
      [limit]
    );

    return logs.map(log => ({
      ...log,
      success: log.success === 1
    }));
  }

  // ==================== MARKET DATA CACHE ====================

  /**
   * Save market data to cache
   */
  async saveMarketDataCache(symbol: string, category: string, quoteData: any): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execute(
      `INSERT OR REPLACE INTO market_data_cache (symbol, category, quote_data, cached_at)
       VALUES ($1, $2, $3, datetime('now'))`,
      [symbol, category, JSON.stringify(quoteData)]
    );
  }

  /**
   * Get cached market data if it's less than maxAgeMinutes old
   */
  async getCachedMarketData(symbol: string, category: string, maxAgeMinutes: number = 10): Promise<any | null> {
    if (!this.db) throw new Error('Database not initialized');

    const result = await this.db.select<Array<{
      quote_data: string;
      cached_at: string;
    }>>(
      `SELECT quote_data, cached_at FROM market_data_cache
       WHERE symbol = $1 AND category = $2
       AND datetime(cached_at) > datetime('now', '-${maxAgeMinutes} minutes')`,
      [symbol, category]
    );

    if (result.length > 0) {
      try {
        return JSON.parse(result[0].quote_data);
      } catch (error) {
        console.error('Error parsing cached market data:', error);
        return null;
      }
    }

    return null;
  }

  /**
   * Get all cached market data for a category that's less than maxAgeMinutes old
   */
  async getCachedCategoryData(category: string, symbols: string[], maxAgeMinutes: number = 10): Promise<Map<string, any>> {
    if (!this.db) throw new Error('Database not initialized');

    const placeholders = symbols.map((_, i) => `$${i + 2}`).join(',');
    const result = await this.db.select<Array<{
      symbol: string;
      quote_data: string;
      cached_at: string;
    }>>(
      `SELECT symbol, quote_data, cached_at FROM market_data_cache
       WHERE category = $1 AND symbol IN (${placeholders})
       AND datetime(cached_at) > datetime('now', '-${maxAgeMinutes} minutes')`,
      [category, ...symbols]
    );

    const dataMap = new Map<string, any>();
    result.forEach(row => {
      try {
        dataMap.set(row.symbol, JSON.parse(row.quote_data));
      } catch (error) {
        console.error(`Error parsing cached data for ${row.symbol}:`, error);
      }
    });

    return dataMap;
  }

  /**
   * Clear all market data cache
   */
  async clearMarketDataCache(): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    await this.db.execute('DELETE FROM market_data_cache');
  }

  /**
   * Clear expired cache entries (older than maxAgeMinutes)
   */
  async clearExpiredMarketCache(maxAgeMinutes: number = 10): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');
    await this.db.execute(
      `DELETE FROM market_data_cache WHERE datetime(cached_at) <= datetime('now', '-${maxAgeMinutes} minutes')`
    );
  }

  // ==================== WEBSOCKET PROVIDER CONFIGS ====================

  /**
   * Save or update WebSocket provider configuration
   */
  async saveWSProviderConfig(config: {
    provider_name: string;
    enabled: boolean;
    api_key?: string;
    api_secret?: string;
    endpoint?: string;
    config_data?: string;
  }): Promise<{ success: boolean; message: string; id?: number }> {
    if (!this.db) throw new Error('Database not initialized');

    try {
      // Check if exists
      const existing = await this.db.select<Array<{ id: number }>>(
        'SELECT id FROM ws_provider_configs WHERE provider_name = $1',
        [config.provider_name]
      );

      if (existing.length > 0) {
        // Update
        await this.db.execute(
          `UPDATE ws_provider_configs SET enabled = $1, api_key = $2, api_secret = $3,
           endpoint = $4, config_data = $5, updated_at = datetime('now') WHERE id = $6`,
          [config.enabled ? 1 : 0, config.api_key || null, config.api_secret || null,
           config.endpoint || null, config.config_data || null, existing[0].id]
        );
        return { success: true, message: 'Provider config updated successfully', id: existing[0].id };
      } else {
        // Insert
        const result = await this.db.execute(
          `INSERT INTO ws_provider_configs (provider_name, enabled, api_key, api_secret, endpoint, config_data)
           VALUES ($1, $2, $3, $4, $5, $6)`,
          [config.provider_name, config.enabled ? 1 : 0, config.api_key || null,
           config.api_secret || null, config.endpoint || null, config.config_data || null]
        );
        return { success: true, message: 'Provider config saved successfully', id: result.lastInsertId || undefined };
      }
    } catch (error: any) {
      return { success: false, message: error.message };
    }
  }

  /**
   * Get all WebSocket provider configurations
   */
  async getWSProviderConfigs(): Promise<Array<{
    id: number;
    provider_name: string;
    enabled: boolean;
    api_key: string | null;
    api_secret: string | null;
    endpoint: string | null;
    config_data: string | null;
    created_at: string;
    updated_at: string;
  }>> {
    if (!this.db) throw new Error('Database not initialized');

    const configs = await this.db.select<Array<any>>(
      'SELECT * FROM ws_provider_configs ORDER BY provider_name ASC'
    );

    return configs.map(c => ({
      ...c,
      enabled: c.enabled === 1
    }));
  }

  /**
   * Get WebSocket provider configuration by name
   */
  async getWSProviderConfig(providerName: string): Promise<{
    id: number;
    provider_name: string;
    enabled: boolean;
    api_key: string | null;
    api_secret: string | null;
    endpoint: string | null;
    config_data: string | null;
    created_at: string;
    updated_at: string;
  } | null> {
    if (!this.db) throw new Error('Database not initialized');

    const configs = await this.db.select<Array<any>>(
      'SELECT * FROM ws_provider_configs WHERE provider_name = $1',
      [providerName]
    );

    if (configs.length === 0) return null;

    return {
      ...configs[0],
      enabled: configs[0].enabled === 1
    };
  }

  /**
   * Get only enabled WebSocket provider configurations
   */
  async getEnabledWSProviderConfigs(): Promise<Array<{
    id: number;
    provider_name: string;
    enabled: boolean;
    api_key: string | null;
    api_secret: string | null;
    endpoint: string | null;
    config_data: string | null;
    created_at: string;
    updated_at: string;
  }>> {
    if (!this.db) throw new Error('Database not initialized');

    const configs = await this.db.select<Array<any>>(
      'SELECT * FROM ws_provider_configs WHERE enabled = 1 ORDER BY provider_name ASC'
    );

    return configs.map(c => ({
      ...c,
      enabled: true
    }));
  }

  /**
   * Toggle WebSocket provider enabled status
   */
  async toggleWSProviderEnabled(providerName: string): Promise<{ success: boolean; message: string; enabled: boolean }> {
    if (!this.db) throw new Error('Database not initialized');

    try {
      const config = await this.getWSProviderConfig(providerName);

      if (!config) {
        return { success: false, message: 'Provider not found', enabled: false };
      }

      const newEnabled = !config.enabled;

      await this.db.execute(
        `UPDATE ws_provider_configs SET enabled = $1, updated_at = datetime('now') WHERE provider_name = $2`,
        [newEnabled ? 1 : 0, providerName]
      );

      return { success: true, message: `Provider ${newEnabled ? 'enabled' : 'disabled'} successfully`, enabled: newEnabled };
    } catch (error: any) {
      return { success: false, message: error.message, enabled: false };
    }
  }

  /**
   * Delete WebSocket provider configuration
   */
  async deleteWSProviderConfig(providerName: string): Promise<{ success: boolean; message: string }> {
    if (!this.db) throw new Error('Database not initialized');

    try {
      await this.db.execute('DELETE FROM ws_provider_configs WHERE provider_name = $1', [providerName]);
      return { success: true, message: 'Provider config deleted successfully' };
    } catch (error: any) {
      return { success: false, message: error.message };
    }
  }
}

// Singleton instance
export const sqliteService = new SQLiteService();
