// DuckDB Service - High-performance analytical database for large datasets
// Uses Rust backend via Tauri commands
import { invoke } from '@tauri-apps/api/core';

export interface QueryResult {
  columns: string[];
  rows: any[][];
  row_count: number;
}

export interface ExecuteResult {
  rows_affected: number;
  success: boolean;
}

class DuckDBService {
  /**
   * Execute a SELECT query and return results
   */
  async query<T = any>(sql: string): Promise<T[]> {
    try {
      const result = await invoke<QueryResult>('duckdb_query', { sql });

      // Convert rows array to objects with column names
      return result.rows.map(row => {
        const obj: any = {};
        result.columns.forEach((col, idx) => {
          obj[col] = row[idx];
        });
        return obj as T;
      });
    } catch (error) {
      console.error('[DuckDB] Query error:', error);
      throw error;
    }
  }

  /**
   * Execute a SELECT query and return raw result (columns + rows)
   */
  async queryRaw(sql: string): Promise<QueryResult> {
    try {
      return await invoke<QueryResult>('duckdb_query', { sql });
    } catch (error) {
      console.error('[DuckDB] Query error:', error);
      throw error;
    }
  }

  /**
   * Execute a SQL statement (INSERT, UPDATE, DELETE, CREATE, etc.)
   */
  async execute(sql: string): Promise<ExecuteResult> {
    try {
      return await invoke<ExecuteResult>('duckdb_execute', { sql });
    } catch (error) {
      console.error('[DuckDB] Execute error:', error);
      throw error;
    }
  }

  /**
   * Execute multiple SQL statements in sequence
   */
  async executeBatch(sqlStatements: string[]): Promise<ExecuteResult[]> {
    try {
      return await invoke<ExecuteResult[]>('duckdb_execute_batch', { sqlStatements });
    } catch (error) {
      console.error('[DuckDB] Execute batch error:', error);
      throw error;
    }
  }

  /**
   * Get list of all tables in the database
   */
  async getTables(): Promise<string[]> {
    try {
      return await invoke<string[]>('duckdb_get_tables');
    } catch (error) {
      console.error('[DuckDB] Get tables error:', error);
      throw error;
    }
  }

  /**
   * Get table schema information (columns, types)
   */
  async getTableInfo(tableName: string): Promise<QueryResult> {
    try {
      return await invoke<QueryResult>('duckdb_get_table_info', { tableName });
    } catch (error) {
      console.error('[DuckDB] Get table info error:', error);
      throw error;
    }
  }

  /**
   * Test database connection
   */
  async testConnection(): Promise<boolean> {
    try {
      return await invoke<boolean>('duckdb_test_connection');
    } catch (error) {
      console.error('[DuckDB] Connection test failed:', error);
      return false;
    }
  }

  /**
   * Initialize database with schema
   */
  async initializeSchema(): Promise<void> {
    const tables = await this.getTables();

    // Create tables if they don't exist
    const schemaStatements: string[] = [];

    // Market data table for billions of rows
    if (!tables.includes('market_data')) {
      schemaStatements.push(`
        CREATE TABLE market_data (
          id BIGINT PRIMARY KEY,
          symbol VARCHAR NOT NULL,
          date DATE NOT NULL,
          open DOUBLE,
          high DOUBLE,
          low DOUBLE,
          close DOUBLE,
          volume BIGINT,
          adjusted_close DOUBLE,
          created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
      `);

      // Create indexes for fast queries
      schemaStatements.push('CREATE INDEX idx_market_data_symbol ON market_data(symbol)');
      schemaStatements.push('CREATE INDEX idx_market_data_date ON market_data(date)');
      schemaStatements.push('CREATE INDEX idx_market_data_symbol_date ON market_data(symbol, date)');
    }

    // Portfolio transactions
    if (!tables.includes('portfolio_transactions')) {
      schemaStatements.push(`
        CREATE TABLE portfolio_transactions (
          id BIGINT PRIMARY KEY,
          portfolio_id VARCHAR NOT NULL,
          symbol VARCHAR NOT NULL,
          transaction_type VARCHAR NOT NULL, -- 'BUY', 'SELL'
          quantity DOUBLE NOT NULL,
          price DOUBLE NOT NULL,
          transaction_date TIMESTAMP NOT NULL,
          fees DOUBLE DEFAULT 0,
          notes TEXT,
          created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
      `);

      schemaStatements.push('CREATE INDEX idx_portfolio_transactions_portfolio ON portfolio_transactions(portfolio_id)');
      schemaStatements.push('CREATE INDEX idx_portfolio_transactions_symbol ON portfolio_transactions(symbol)');
    }

    // Analytics cache
    if (!tables.includes('analytics_cache')) {
      schemaStatements.push(`
        CREATE TABLE analytics_cache (
          cache_key VARCHAR PRIMARY KEY,
          cache_value TEXT NOT NULL,
          created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
          expires_at TIMESTAMP NOT NULL
        )
      `);

      schemaStatements.push('CREATE INDEX idx_analytics_cache_expires ON analytics_cache(expires_at)');
    }

    if (schemaStatements.length > 0) {
      console.log('[DuckDB] Initializing schema with', schemaStatements.length, 'statements');
      await this.executeBatch(schemaStatements);
      console.log('[DuckDB] Schema initialized successfully');
    } else {
      console.log('[DuckDB] Schema already exists');
    }
  }

  // ============ High-Level Data Access Methods ============

  /**
   * Get historical market data for a symbol
   */
  async getHistoricalData(symbol: string, startDate?: string, endDate?: string) {
    let sql = `
      SELECT symbol, date, open, high, low, close, volume, adjusted_close
      FROM market_data
      WHERE symbol = '${symbol}'
    `;

    if (startDate) {
      sql += ` AND date >= '${startDate}'`;
    }

    if (endDate) {
      sql += ` AND date <= '${endDate}'`;
    }

    sql += ' ORDER BY date ASC';

    return this.query(sql);
  }

  /**
   * Insert market data (batch insert for performance)
   */
  async insertMarketData(data: Array<{
    id: number;
    symbol: string;
    date: string;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
    adjusted_close?: number;
  }>) {
    if (data.length === 0) return;

    // Use DuckDB's efficient INSERT INTO ... VALUES syntax
    const values = data.map(d =>
      `(${d.id}, '${d.symbol}', '${d.date}', ${d.open}, ${d.high}, ${d.low}, ${d.close}, ${d.volume}, ${d.adjusted_close || d.close})`
    ).join(',\n');

    const sql = `
      INSERT INTO market_data (id, symbol, date, open, high, low, close, volume, adjusted_close)
      VALUES ${values}
    `;

    return this.execute(sql);
  }

  /**
   * Get market data aggregates (fast analytics on large datasets)
   */
  async getMarketAggregates(symbols?: string[], startDate?: string, endDate?: string) {
    let sql = `
      SELECT
        symbol,
        COUNT(*) as data_points,
        MIN(date) as first_date,
        MAX(date) as last_date,
        AVG(close) as avg_price,
        MIN(close) as min_price,
        MAX(close) as max_price,
        SUM(volume) as total_volume,
        AVG(volume) as avg_volume
      FROM market_data
      WHERE 1=1
    `;

    if (symbols && symbols.length > 0) {
      const symbolList = symbols.map(s => `'${s}'`).join(',');
      sql += ` AND symbol IN (${symbolList})`;
    }

    if (startDate) {
      sql += ` AND date >= '${startDate}'`;
    }

    if (endDate) {
      sql += ` AND date <= '${endDate}'`;
    }

    sql += ' GROUP BY symbol ORDER BY total_volume DESC';

    return this.query(sql);
  }

  /**
   * Clean expired cache entries
   */
  async cleanExpiredCache() {
    const sql = `DELETE FROM analytics_cache WHERE expires_at < CURRENT_TIMESTAMP`;
    return this.execute(sql);
  }

  /**
   * Get or set cache value
   */
  async getCacheValue(key: string): Promise<string | null> {
    const sql = `
      SELECT cache_value
      FROM analytics_cache
      WHERE cache_key = '${key}' AND expires_at > CURRENT_TIMESTAMP
    `;
    const result = await this.query<{ cache_value: string }>(sql);
    return result.length > 0 ? result[0].cache_value : null;
  }

  async setCacheValue(key: string, value: string, ttlMinutes: number = 60) {
    const expiresAt = new Date(Date.now() + ttlMinutes * 60 * 1000).toISOString();
    const sql = `
      INSERT OR REPLACE INTO analytics_cache (cache_key, cache_value, expires_at)
      VALUES ('${key}', '${value}', '${expiresAt}')
    `;
    return this.execute(sql);
  }
}

// Singleton instance
export const duckdbService = new DuckDBService();
