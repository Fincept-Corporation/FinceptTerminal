// ClickHouse OLAP Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ClickHouseAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host', 'localhost');
      const port = this.getConfig<number>('port', 8123);
      const database = this.getConfig<string>('database', 'default');
      const username = this.getConfig<string>('username', 'default');
      const password = this.getConfig<string>('password');
      const protocol = this.getConfig<string>('protocol', 'http');

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Testing ClickHouse connection...');

      try {
        // Build URL
        const baseUrl = `${protocol}://${host}:${port}`;

        // Prepare headers
        const headers: Record<string, string> = {};
        if (username && password) {
          const credentials = btoa(`${username}:${password}`);
          headers['Authorization'] = `Basic ${credentials}`;
        }

        // Test with a simple SELECT 1 query
        const url = `${baseUrl}/?query=${encodeURIComponent('SELECT version()')}`;

        const response = await fetch(url, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          if (response.status === 401) {
            throw new Error('Authentication failed. Check your credentials.');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const version = await response.text();

        return this.createSuccessResult(`Successfully connected to ClickHouse at ${host}:${port}`, {
          version: version.trim(),
          host,
          port,
          database,
          protocol,
          authenticated: !!(username && password),
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          if (fetchError.message.includes('Failed to fetch')) {
            return this.createErrorResult(`Cannot reach ClickHouse at ${host}:${port}. Is the server running?`);
          }
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string, format: string = 'JSON'): Promise<any> {
    try {
      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 8123);
      const database = this.getConfig<string>('database', 'default');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const protocol = this.getConfig<string>('protocol', 'http');

      const baseUrl = `${protocol}://${host}:${port}`;

      const headers: Record<string, string> = {
        'Content-Type': 'text/plain',
      };

      if (username && password) {
        const credentials = btoa(`${username}:${password}`);
        headers['Authorization'] = `Basic ${credentials}`;
      }

      // Add database context
      const queryWithDb = database !== 'default' ? `USE ${database}; ${sql}` : sql;

      const url = `${baseUrl}/?query=${encodeURIComponent(queryWithDb)}&default_format=${format}`;

      const response = await fetch(url, {
        method: 'GET',
        headers,
      });

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`Query failed: ${response.status} - ${errorText}`);
      }

      if (format === 'JSON') {
        const text = await response.text();
        if (!text.trim()) {
          return { data: [], rows: 0 };
        }
        return JSON.parse(text);
      }

      return await response.text();
    } catch (error) {
      throw new Error(`ClickHouse query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 8123),
      database: this.getConfig('database', 'default'),
      protocol: this.getConfig('protocol', 'http'),
      hasAuth: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Get list of databases
   */
  async getDatabases(): Promise<any> {
    return await this.query('SHOW DATABASES');
  }

  /**
   * Get list of tables
   */
  async getTables(database?: string): Promise<any> {
    const db = database || this.getConfig<string>('database', 'default');
    return await this.query(`SHOW TABLES FROM ${db}`);
  }

  /**
   * Get table schema
   */
  async getTableSchema(table: string, database?: string): Promise<any> {
    const db = database || this.getConfig<string>('database', 'default');
    return await this.query(`DESCRIBE TABLE ${db}.${table}`);
  }

  /**
   * Get table size and statistics
   */
  async getTableStats(table: string, database?: string): Promise<any> {
    const db = database || this.getConfig<string>('database', 'default');
    return await this.query(`
      SELECT
        table,
        sum(rows) as total_rows,
        formatReadableSize(sum(bytes)) as total_size,
        sum(bytes) as bytes,
        min(min_date) as min_date,
        max(max_date) as max_date
      FROM system.parts
      WHERE database = '${db}' AND table = '${table}' AND active
      GROUP BY table
    `);
  }

  /**
   * Get cluster information
   */
  async getClusterInfo(): Promise<any> {
    return await this.query('SELECT * FROM system.clusters');
  }

  /**
   * Get running queries
   */
  async getRunningQueries(): Promise<any> {
    return await this.query('SELECT * FROM system.processes');
  }

  /**
   * Kill a query
   */
  async killQuery(queryId: string): Promise<any> {
    return await this.query(`KILL QUERY WHERE query_id = '${queryId}'`);
  }

  /**
   * Optimize table (merge parts)
   */
  async optimizeTable(table: string, database?: string): Promise<any> {
    const db = database || this.getConfig<string>('database', 'default');
    return await this.query(`OPTIMIZE TABLE ${db}.${table}`);
  }

  /**
   * Get system metrics
   */
  async getSystemMetrics(): Promise<any> {
    return await this.query('SELECT * FROM system.metrics');
  }

  /**
   * Get query log
   */
  async getQueryLog(limit: number = 100): Promise<any> {
    return await this.query(`
      SELECT
        type,
        query_start_time,
        query_duration_ms,
        query,
        read_rows,
        read_bytes,
        result_rows,
        result_bytes,
        memory_usage
      FROM system.query_log
      WHERE type = 'QueryFinish'
      ORDER BY query_start_time DESC
      LIMIT ${limit}
    `);
  }

  /**
   * Insert data using INSERT INTO format
   */
  async insert(table: string, data: any[], database?: string): Promise<any> {
    try {
      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 8123);
      const db = database || this.getConfig<string>('database', 'default');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const protocol = this.getConfig<string>('protocol', 'http');

      const baseUrl = `${protocol}://${host}:${port}`;

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
      };

      if (username && password) {
        const credentials = btoa(`${username}:${password}`);
        headers['Authorization'] = `Basic ${credentials}`;
      }

      const url = `${baseUrl}/?query=${encodeURIComponent(`INSERT INTO ${db}.${table} FORMAT JSONEachRow`)}`;

      const body = data.map((row) => JSON.stringify(row)).join('\n');

      const response = await fetch(url, {
        method: 'POST',
        headers,
        body,
      });

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`Insert failed: ${response.status} - ${errorText}`);
      }

      return { success: true, rowsInserted: data.length };
    } catch (error) {
      throw new Error(`ClickHouse insert error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }
}
