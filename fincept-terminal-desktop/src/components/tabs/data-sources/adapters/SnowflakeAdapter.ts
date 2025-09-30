// Snowflake Data Warehouse Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class SnowflakeAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const account = this.getConfig<string>('account');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const warehouse = this.getConfig<string>('warehouse');
      const database = this.getConfig<string>('database');
      const schema = this.getConfig<string>('schema', 'PUBLIC');
      const role = this.getConfig<string>('role');

      if (!account || !username || !password) {
        return this.createErrorResult('Account, username, and password are required');
      }

      console.log('Validating Snowflake configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for Snowflake account "${account}"`,
        {
          validatedAt: new Date().toISOString(),
          account,
          username,
          warehouse,
          database,
          schema,
          role,
          note: 'Client-side validation only. Snowflake connections require backend integration with snowflake-sdk.',
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
          type: 'snowflake',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Snowflake query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      account: this.getConfig('account'),
      warehouse: this.getConfig('warehouse'),
      database: this.getConfig('database'),
      schema: this.getConfig('schema', 'PUBLIC'),
      role: this.getConfig('role'),
    };
  }

  /**
   * Get list of databases
   */
  async getDatabases(): Promise<any> {
    return await this.query('SHOW DATABASES;');
  }

  /**
   * Get list of schemas
   */
  async getSchemas(database?: string): Promise<any> {
    const db = database || this.getConfig<string>('database');
    return await this.query(db ? `SHOW SCHEMAS IN DATABASE ${db};` : 'SHOW SCHEMAS;');
  }

  /**
   * Get list of tables
   */
  async getTables(database?: string, schema?: string): Promise<any> {
    const db = database || this.getConfig<string>('database');
    const sch = schema || this.getConfig<string>('schema');

    if (db && sch) {
      return await this.query(`SHOW TABLES IN ${db}.${sch};`);
    } else if (db) {
      return await this.query(`SHOW TABLES IN DATABASE ${db};`);
    }
    return await this.query('SHOW TABLES;');
  }

  /**
   * Get list of warehouses
   */
  async getWarehouses(): Promise<any> {
    return await this.query('SHOW WAREHOUSES;');
  }

  /**
   * Use specific warehouse
   */
  async useWarehouse(warehouse: string): Promise<any> {
    return await this.query(`USE WAREHOUSE ${warehouse};`);
  }

  /**
   * Use specific database
   */
  async useDatabase(database: string): Promise<any> {
    return await this.query(`USE DATABASE ${database};`);
  }

  /**
   * Get account usage information
   */
  async getAccountUsage(days: number = 7): Promise<any> {
    return await this.query(`
      SELECT * FROM SNOWFLAKE.ACCOUNT_USAGE.QUERY_HISTORY
      WHERE START_TIME >= DATEADD(day, -${days}, CURRENT_TIMESTAMP())
      ORDER BY START_TIME DESC
      LIMIT 100;
    `);
  }
}
