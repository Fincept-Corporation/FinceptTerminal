// Quandl (Nasdaq Data Link) Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class QuandlAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const apiKey = this.getConfig<string>('apiKey');

      if (!apiKey) {
        return this.createErrorResult('API key is required');
      }

      console.log('Testing Quandl API connection...');

      try {
        // Test with a simple database metadata request
        const response = await fetch(`https://data.nasdaq.com/api/v3/databases.json?api_key=${apiKey}&per_page=1`);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.quandl_error) {
          throw new Error(data.quandl_error.message || 'API request failed');
        }

        return this.createSuccessResult('Successfully connected to Quandl API', {
          databasesAvailable: data.meta?.total_count || 'Unknown',
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(endpoint: string, params?: Record<string, any>): Promise<any> {
    try {
      const apiKey = this.getConfig<string>('apiKey');

      const queryParams = new URLSearchParams({ ...params, api_key: apiKey });
      const url = `https://data.nasdaq.com/api/v3${endpoint}?${queryParams.toString()}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.quandl_error) {
        throw new Error(data.quandl_error.message || 'Query failed');
      }

      return data;
    } catch (error) {
      throw new Error(`Quandl query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Quandl (Nasdaq Data Link)',
    };
  }

  /**
   * Get dataset data
   */
  async getDataset(databaseCode: string, datasetCode: string, params?: Record<string, any>): Promise<any> {
    return await this.query(`/datasets/${databaseCode}/${datasetCode}.json`, params);
  }

  /**
   * Get dataset metadata
   */
  async getDatasetMetadata(databaseCode: string, datasetCode: string): Promise<any> {
    return await this.query(`/datasets/${databaseCode}/${datasetCode}/metadata.json`);
  }

  /**
   * Search datasets
   */
  async searchDatasets(query: string, perPage: number = 10): Promise<any> {
    return await this.query('/datasets.json', { query, per_page: perPage.toString() });
  }

  /**
   * Get database metadata
   */
  async getDatabase(databaseCode: string): Promise<any> {
    return await this.query(`/databases/${databaseCode}.json`);
  }

  /**
   * List databases
   */
  async listDatabases(perPage: number = 100): Promise<any> {
    return await this.query('/databases.json', { per_page: perPage.toString() });
  }

  /**
   * Get time series data
   */
  async getTimeSeries(
    databaseCode: string,
    datasetCode: string,
    startDate?: string,
    endDate?: string
  ): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.start_date = startDate;
    if (endDate) params.end_date = endDate;

    return await this.query(`/datasets/${databaseCode}/${datasetCode}/data.json`, params);
  }
}
