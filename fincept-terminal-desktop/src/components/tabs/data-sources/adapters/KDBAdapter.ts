// kdb+ Time Series Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class KDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 5000);

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Validating kdb+ configuration (client-side only)');

      // kdb+ uses a binary protocol (IPC) that requires backend integration
      // For now, validate configuration only
      return this.createSuccessResult(`Configuration validated for kdb+ at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        note: 'Client-side validation only. kdb+ connections require backend integration with node-q or similar IPC libraries.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(queryString: string): Promise<any> {
    try {
      // kdb+ queries need to go through backend
      const response = await fetch('/api/data-sources/kdb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'kdb',
          query: queryString,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`kdb+ query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 5000),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Execute q query
   */
  async executeQuery(q: string): Promise<any> {
    return await this.query(q);
  }

  /**
   * Get tables
   */
  async getTables(): Promise<any> {
    return await this.query('tables[]');
  }

  /**
   * Get table schema
   */
  async getTableSchema(tableName: string): Promise<any> {
    return await this.query(`meta ${tableName}`);
  }

  /**
   * Get table count
   */
  async getTableCount(tableName: string): Promise<any> {
    return await this.query(`count ${tableName}`);
  }

  /**
   * Select from table
   */
  async selectFromTable(tableName: string, columns: string = '*', whereClause?: string, limit?: number): Promise<any> {
    let q = `select ${columns} from ${tableName}`;
    if (whereClause) q += ` where ${whereClause}`;
    if (limit) q += ` limit ${limit}`;
    return await this.query(q);
  }

  /**
   * Get system info
   */
  async getSystemInfo(): Promise<any> {
    return await this.query('.z.K'); // kdb+ version
  }

  /**
   * List namespaces
   */
  async getNamespaces(): Promise<any> {
    return await this.query('system "d"');
  }

  /**
   * Get functions in namespace
   */
  async getFunctions(namespace: string = '.'): Promise<any> {
    return await this.query(`${namespace}\\f`);
  }

  /**
   * Get variables in namespace
   */
  async getVariables(namespace: string = '.'): Promise<any> {
    return await this.query(`${namespace}\\v`);
  }

  /**
   * Time series query with aggregation
   */
  async timeSeriesAgg(
    tableName: string,
    timeColumn: string,
    valueColumn: string,
    aggregation: 'avg' | 'sum' | 'min' | 'max' | 'count',
    interval: string
  ): Promise<any> {
    const q = `select ${aggregation} ${valueColumn} by ${interval} xbar ${timeColumn} from ${tableName}`;
    return await this.query(q);
  }
}
