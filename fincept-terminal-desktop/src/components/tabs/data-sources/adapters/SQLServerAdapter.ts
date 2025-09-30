// Microsoft SQL Server Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class SQLServerAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 1433);
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const encrypt = this.getConfig<boolean>('encrypt', true);
      const trustServerCertificate = this.getConfig<boolean>('trustServerCertificate', false);

      if (!host || !database || !username || !password) {
        return this.createErrorResult('Missing required connection parameters');
      }

      console.log('Validating SQL Server configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for SQL Server database "${database}"`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        database,
        username,
        encrypt,
        trustServerCertificate,
        note: 'Client-side validation only. SQL Server connections require backend integration with mssql.',
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
          type: 'sqlserver',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`SQL Server query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 1433),
      database: this.getConfig('database'),
      encrypt: this.getConfig('encrypt', true),
      trustServerCertificate: this.getConfig('trustServerCertificate', false),
    };
  }

  /**
   * Get tables
   */
  async getTables(schema: string = 'dbo'): Promise<any> {
    return await this.query(`
      SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE
      FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = '${schema}'
      ORDER BY TABLE_NAME
    `);
  }

  /**
   * Get table columns
   */
  async getColumns(tableName: string, schema: string = 'dbo'): Promise<any> {
    return await this.query(`
      SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, IS_NULLABLE
      FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_NAME = '${tableName}' AND TABLE_SCHEMA = '${schema}'
      ORDER BY ORDINAL_POSITION
    `);
  }

  /**
   * Get schemas
   */
  async getSchemas(): Promise<any> {
    return await this.query('SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA ORDER BY SCHEMA_NAME');
  }

  /**
   * Get databases
   */
  async getDatabases(): Promise<any> {
    return await this.query('SELECT name FROM sys.databases ORDER BY name');
  }

  /**
   * Get server version
   */
  async getVersion(): Promise<any> {
    return await this.query('SELECT @@VERSION AS Version');
  }
}
