// Apache Cassandra Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CassandraAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const contactPoints = this.getConfig<string>('contactPoints', 'localhost');
      const port = this.getConfig<number>('port', 9042);
      const keyspace = this.getConfig<string>('keyspace');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const datacenter = this.getConfig<string>('datacenter', 'datacenter1');
      const consistencyLevel = this.getConfig<string>('consistencyLevel', 'LOCAL_ONE');

      if (!contactPoints) {
        return this.createErrorResult('Contact points are required');
      }

      console.log('Validating Cassandra configuration (client-side only)');

      // Parse contact points (can be comma-separated)
      const hosts = contactPoints.split(',').map((h) => h.trim());

      return this.createSuccessResult(
        `Configuration validated for Cassandra cluster`,
        {
          validatedAt: new Date().toISOString(),
          contactPoints: hosts,
          port,
          keyspace,
          datacenter,
          consistencyLevel,
          hasAuth: !!(username && password),
          note: 'Client-side validation only. Cassandra uses native protocol (CQL) which requires backend integration.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(cql: string): Promise<any> {
    try {
      // Cassandra queries would need to go through a backend service
      // that has the cassandra-driver installed
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'cassandra',
          query: cql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Cassandra query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      contactPoints: this.getConfig('contactPoints'),
      port: this.getConfig('port', 9042),
      keyspace: this.getConfig('keyspace'),
      datacenter: this.getConfig('datacenter', 'datacenter1'),
      consistencyLevel: this.getConfig('consistencyLevel', 'LOCAL_ONE'),
      hasAuth: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Execute a prepared statement (requires backend support)
   */
  async executePrepared(query: string, params: any[]): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/execute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'cassandra',
          query,
          params,
          prepared: true,
        }),
      });

      if (!response.ok) {
        throw new Error(`Prepared statement execution failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Cassandra prepared statement error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Get keyspace metadata
   */
  async getKeyspaces(): Promise<any> {
    const cql = 'SELECT * FROM system_schema.keyspaces;';
    return await this.query(cql);
  }

  /**
   * Get tables in a keyspace
   */
  async getTables(keyspace?: string): Promise<any> {
    const ks = keyspace || this.getConfig<string>('keyspace');
    if (!ks) {
      throw new Error('Keyspace is required');
    }

    const cql = `SELECT * FROM system_schema.tables WHERE keyspace_name = '${ks}';`;
    return await this.query(cql);
  }

  /**
   * Get column metadata for a table
   */
  async getColumns(keyspace: string, table: string): Promise<any> {
    const cql = `SELECT * FROM system_schema.columns WHERE keyspace_name = '${keyspace}' AND table_name = '${table}';`;
    return await this.query(cql);
  }

  /**
   * Create a keyspace
   */
  async createKeyspace(
    name: string,
    replicationStrategy: 'SimpleStrategy' | 'NetworkTopologyStrategy' = 'SimpleStrategy',
    replicationFactor: number = 1
  ): Promise<any> {
    const cql =
      replicationStrategy === 'SimpleStrategy'
        ? `CREATE KEYSPACE IF NOT EXISTS ${name} WITH replication = {'class': 'SimpleStrategy', 'replication_factor': ${replicationFactor}};`
        : `CREATE KEYSPACE IF NOT EXISTS ${name} WITH replication = {'class': 'NetworkTopologyStrategy', 'datacenter1': ${replicationFactor}};`;

    return await this.query(cql);
  }

  /**
   * Batch operations
   */
  async executeBatch(statements: string[]): Promise<any> {
    const batchCql = `BEGIN BATCH\n${statements.join('\n')}\nAPPLY BATCH;`;
    return await this.query(batchCql);
  }

  /**
   * Get cluster nodes info (requires system tables access)
   */
  async getClusterInfo(): Promise<any> {
    const cql = 'SELECT * FROM system.peers;';
    return await this.query(cql);
  }
}
