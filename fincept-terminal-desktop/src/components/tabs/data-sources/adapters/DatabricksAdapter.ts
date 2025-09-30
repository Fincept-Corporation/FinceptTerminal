// Databricks Data Platform Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class DatabricksAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const token = this.getConfig<string>('token');
      const httpPath = this.getConfig<string>('httpPath');

      if (!host || !token) {
        return this.createErrorResult('Host and token are required');
      }

      console.log('Testing Databricks connection...');

      try {
        // Test with clusters list endpoint
        const url = `https://${host}/api/2.0/clusters/list`;

        const response = await fetch(url, {
          method: 'GET',
          headers: {
            Authorization: `Bearer ${token}`,
            'Content-Type': 'application/json',
          },
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Authentication failed. Check your token.');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Databricks', {
          host,
          clusterCount: data.clusters?.length || 0,
          httpPath: httpPath || 'Not provided',
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          if (fetchError.message.includes('Failed to fetch')) {
            return this.createErrorResult(`Cannot reach Databricks at ${host}.`);
          }
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string): Promise<any> {
    try {
      const host = this.getConfig<string>('host');
      const token = this.getConfig<string>('token');
      const clusterId = this.getConfig<string>('clusterId');

      if (!clusterId) {
        throw new Error('Cluster ID is required for queries');
      }

      const response = await fetch(`https://${host}/api/2.0/sql/statements`, {
        method: 'POST',
        headers: {
          Authorization: `Bearer ${token}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          statement: sql,
          warehouse_id: clusterId,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Databricks query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      httpPath: this.getConfig('httpPath'),
      hasToken: !!this.getConfig('token'),
      clusterId: this.getConfig('clusterId'),
    };
  }

  /**
   * List clusters
   */
  async listClusters(): Promise<any> {
    const host = this.getConfig<string>('host');
    const token = this.getConfig<string>('token');

    const response = await fetch(`https://${host}/api/2.0/clusters/list`, {
      method: 'GET',
      headers: {
        Authorization: `Bearer ${token}`,
      },
    });

    if (!response.ok) {
      throw new Error('Failed to list clusters');
    }

    return await response.json();
  }

  /**
   * Get cluster info
   */
  async getCluster(clusterId: string): Promise<any> {
    const host = this.getConfig<string>('host');
    const token = this.getConfig<string>('token');

    const response = await fetch(`https://${host}/api/2.0/clusters/get?cluster_id=${clusterId}`, {
      method: 'GET',
      headers: {
        Authorization: `Bearer ${token}`,
      },
    });

    if (!response.ok) {
      throw new Error('Failed to get cluster info');
    }

    return await response.json();
  }

  /**
   * List databases
   */
  async listDatabases(): Promise<any> {
    return await this.query('SHOW DATABASES');
  }

  /**
   * List tables
   */
  async listTables(database?: string): Promise<any> {
    const dbClause = database ? `IN ${database}` : '';
    return await this.query(`SHOW TABLES ${dbClause}`);
  }
}
