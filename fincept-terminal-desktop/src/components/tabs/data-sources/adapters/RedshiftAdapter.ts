// Amazon Redshift Data Warehouse Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class RedshiftAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 5439);
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const ssl = this.getConfig<boolean>('ssl', true);

      if (!host || !database || !username || !password) {
        return this.createErrorResult('Missing required connection parameters');
      }

      console.log('Validating Amazon Redshift configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for Redshift cluster "${host}"`,
        {
          validatedAt: new Date().toISOString(),
          host,
          port,
          database,
          username,
          ssl,
          note: 'Client-side validation only. Redshift uses PostgreSQL wire protocol and requires backend integration.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'redshift',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Redshift query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 5439),
      database: this.getConfig('database'),
      ssl: this.getConfig('ssl', true),
    };
  }

  /**
   * Get cluster information
   */
  async getClusterInfo(): Promise<any> {
    return await this.query('SELECT * FROM svv_node_info;');
  }

  /**
   * Get running queries
   */
  async getRunningQueries(): Promise<any> {
    return await this.query(`
      SELECT query, pid, user_name, starttime, duration
      FROM stv_recents
      WHERE status = 'Running'
      ORDER BY starttime DESC;
    `);
  }

  /**
   * Get table size and statistics
   */
  async getTableStats(schema: string, table: string): Promise<any> {
    return await this.query(`
      SELECT
        tbl,
        COUNT(*) as rows,
        SUM(size) * 1024 * 1024 as size_bytes
      FROM svv_table_info
      WHERE schema = '${schema}' AND "table" = '${table}'
      GROUP BY tbl;
    `);
  }

  /**
   * Analyze table to update statistics
   */
  async analyzeTable(schema: string, table: string): Promise<any> {
    return await this.query(`ANALYZE ${schema}.${table};`);
  }

  /**
   * Vacuum table to reclaim space
   */
  async vacuumTable(schema: string, table: string): Promise<any> {
    return await this.query(`VACUUM ${schema}.${table};`);
  }
}
