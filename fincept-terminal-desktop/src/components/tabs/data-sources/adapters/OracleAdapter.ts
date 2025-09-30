// Oracle Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class OracleAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 1521);
      const serviceName = this.getConfig<string>('serviceName');
      const sid = this.getConfig<string>('sid');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!host || !username || !password) {
        return this.createErrorResult('Host, username, and password are required');
      }

      if (!serviceName && !sid) {
        return this.createErrorResult('Either Service Name or SID is required');
      }

      console.log('Validating Oracle Database configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for Oracle Database at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        serviceName: serviceName || 'Not provided',
        sid: sid || 'Not provided',
        username,
        note: 'Client-side validation only. Oracle connections require backend integration with oracledb.',
      });
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
          type: 'oracle',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Oracle query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 1521),
      serviceName: this.getConfig('serviceName'),
      sid: this.getConfig('sid'),
      username: this.getConfig('username'),
    };
  }

  /**
   * Get tables
   */
  async getTables(owner?: string): Promise<any> {
    const whereClause = owner ? `WHERE OWNER = '${owner.toUpperCase()}'` : '';
    return await this.query(`SELECT TABLE_NAME, TABLESPACE_NAME, NUM_ROWS FROM ALL_TABLES ${whereClause}`);
  }

  /**
   * Get table columns
   */
  async getColumns(tableName: string, owner?: string): Promise<any> {
    const whereClause = owner
      ? `AND OWNER = '${owner.toUpperCase()}'`
      : '';
    return await this.query(`
      SELECT COLUMN_NAME, DATA_TYPE, DATA_LENGTH, NULLABLE
      FROM ALL_TAB_COLUMNS
      WHERE TABLE_NAME = '${tableName.toUpperCase()}' ${whereClause}
      ORDER BY COLUMN_ID
    `);
  }

  /**
   * Get schemas/users
   */
  async getSchemas(): Promise<any> {
    return await this.query('SELECT USERNAME FROM ALL_USERS ORDER BY USERNAME');
  }

  /**
   * Get tablespaces
   */
  async getTablespaces(): Promise<any> {
    return await this.query('SELECT TABLESPACE_NAME, STATUS FROM DBA_TABLESPACES');
  }
}
