// Azure Synapse Analytics Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class AzureSynapseAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const server = this.getConfig<string>('server');
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!server || !database || !username || !password) {
        return this.createErrorResult('Server, database, username, and password are required');
      }

      console.log('Validating Azure Synapse configuration (client-side only)');

      // Azure Synapse requires SQL Server protocol, needs backend integration
      return this.createSuccessResult(`Configuration validated for Azure Synapse "${database}" at ${server}`, {
        validatedAt: new Date().toISOString(),
        server,
        database,
        note: 'Client-side validation only. Azure Synapse requires backend integration with mssql or tedious driver.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string, params?: any[]): Promise<any> {
    try {
      // Synapse queries need to go through backend
      const response = await fetch('/api/data-sources/synapse', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'synapse',
          sql,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Azure Synapse query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      server: this.getConfig('server'),
      database: this.getConfig('database'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Execute SQL query
   */
  async executeQuery(sql: string, params?: any[]): Promise<any> {
    return await this.query(sql, params);
  }

  /**
   * Get tables
   */
  async getTables(schema: string = 'dbo'): Promise<any> {
    const sql = `SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = @schema AND TABLE_TYPE = 'BASE TABLE'`;
    return await this.query(sql, [schema]);
  }

  /**
   * Get table schema
   */
  async getTableSchema(tableName: string, schema: string = 'dbo'): Promise<any> {
    const sql = `SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = @schema AND TABLE_NAME = @tableName`;
    return await this.query(sql, [schema, tableName]);
  }

  /**
   * Get views
   */
  async getViews(schema: string = 'dbo'): Promise<any> {
    const sql = `SELECT TABLE_NAME FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = @schema`;
    return await this.query(sql, [schema]);
  }

  /**
   * Get stored procedures
   */
  async getStoredProcedures(schema: string = 'dbo'): Promise<any> {
    const sql = `SELECT ROUTINE_NAME FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = @schema AND ROUTINE_TYPE = 'PROCEDURE'`;
    return await this.query(sql, [schema]);
  }
}
