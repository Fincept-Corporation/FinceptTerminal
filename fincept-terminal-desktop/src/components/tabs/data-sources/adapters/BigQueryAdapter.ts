// Google BigQuery Data Warehouse Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class BigQueryAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const projectId = this.getConfig<string>('projectId');
      const credentials = this.getConfig<string>('credentials');
      const dataset = this.getConfig<string>('dataset');

      if (!projectId) {
        return this.createErrorResult('Project ID is required');
      }

      if (!credentials) {
        return this.createErrorResult('Service account credentials (JSON) are required');
      }

      // Validate JSON credentials format
      try {
        const creds = JSON.parse(credentials);
        if (!creds.type || !creds.project_id || !creds.private_key) {
          return this.createErrorResult('Invalid credentials format. Expected service account JSON.');
        }
      } catch {
        return this.createErrorResult('Credentials must be valid JSON');
      }

      console.log('Validating BigQuery configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for BigQuery project "${projectId}"`,
        {
          validatedAt: new Date().toISOString(),
          projectId,
          dataset,
          hasCredentials: true,
          note: 'Client-side validation only. BigQuery connections require backend integration with @google-cloud/bigquery.',
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
          type: 'bigquery',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`BigQuery query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      projectId: this.getConfig('projectId'),
      dataset: this.getConfig('dataset'),
      hasCredentials: !!this.getConfig('credentials'),
    };
  }

  /**
   * Get list of datasets
   */
  async getDatasets(): Promise<any> {
    const projectId = this.getConfig<string>('projectId');
    return await this.query(`
      SELECT schema_name as dataset_name
      FROM \`${projectId}.INFORMATION_SCHEMA.SCHEMATA\`
      ORDER BY schema_name;
    `);
  }

  /**
   * Get list of tables in a dataset
   */
  async getTables(dataset?: string): Promise<any> {
    const projectId = this.getConfig<string>('projectId');
    const ds = dataset || this.getConfig<string>('dataset');

    if (!ds) {
      throw new Error('Dataset is required');
    }

    return await this.query(`
      SELECT table_name, table_type, creation_time
      FROM \`${projectId}.${ds}.INFORMATION_SCHEMA.TABLES\`
      ORDER BY table_name;
    `);
  }

  /**
   * Get table schema
   */
  async getTableSchema(dataset: string, table: string): Promise<any> {
    const projectId = this.getConfig<string>('projectId');

    return await this.query(`
      SELECT column_name, data_type, is_nullable, is_partitioning_column
      FROM \`${projectId}.${dataset}.INFORMATION_SCHEMA.COLUMNS\`
      WHERE table_name = '${table}'
      ORDER BY ordinal_position;
    `);
  }

  /**
   * Get table size and row count
   */
  async getTableStats(dataset: string, table: string): Promise<any> {
    const projectId = this.getConfig<string>('projectId');

    return await this.query(`
      SELECT
        table_name,
        row_count,
        size_bytes,
        TIMESTAMP_MILLIS(creation_time) as creation_time,
        TIMESTAMP_MILLIS(last_modified_time) as last_modified_time
      FROM \`${projectId}.${dataset}.__TABLES__\`
      WHERE table_id = '${table}';
    `);
  }

  /**
   * Execute a parameterized query
   */
  async queryWithParams(sql: string, params: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'bigquery',
          query: sql,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Parameterized query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`BigQuery parameterized query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Get query job status
   */
  async getJobStatus(jobId: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/job-status', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'bigquery',
          jobId,
        }),
      });

      if (!response.ok) {
        throw new Error(`Failed to get job status: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`BigQuery job status error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Estimate query cost
   */
  async estimateQueryCost(sql: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/estimate-cost', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'bigquery',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Failed to estimate cost: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`BigQuery cost estimation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }
}
