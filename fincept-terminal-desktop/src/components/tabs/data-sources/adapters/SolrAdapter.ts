// Apache Solr Search Platform Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class SolrAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 8983);
      const core = this.getConfig<string>('core');
      const useSsl = this.getConfig<boolean>('useSsl', false);

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Testing Solr connection...');

      try {
        const protocol = useSsl ? 'https' : 'http';
        const url = `${protocol}://${host}:${port}/solr/admin/info/system?wt=json`;

        const response = await fetch(url, {
          method: 'GET',
          headers: {
            'Content-Type': 'application/json',
          },
        });

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Solr', {
          solrVersion: data.lucene?.['solr-spec-version'] || 'Unknown',
          luceneVersion: data.lucene?.['lucene-spec-version'] || 'Unknown',
          core: core || 'Not specified',
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

  async query(endpoint: string, params?: Record<string, any>, method: string = 'GET'): Promise<any> {
    try {
      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 8983);
      const core = this.getConfig<string>('core');
      const useSsl = this.getConfig<boolean>('useSsl', false);

      const protocol = useSsl ? 'https' : 'http';
      const coreSegment = core ? `/solr/${core}` : '/solr';
      const queryParams = params ? `?${new URLSearchParams({ ...params, wt: 'json' }).toString()}` : '?wt=json';
      const url = `${protocol}://${host}:${port}${coreSegment}${endpoint}${queryParams}`;

      const response = await fetch(url, {
        method,
        headers: {
          'Content-Type': 'application/json',
        },
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Solr query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 8983),
      core: this.getConfig('core'),
      useSsl: this.getConfig('useSsl', false),
    };
  }

  /**
   * Search documents
   */
  async search(query: string, rows: number = 10, start: number = 0, params?: Record<string, any>): Promise<any> {
    return await this.query('/select', {
      q: query,
      rows: rows.toString(),
      start: start.toString(),
      ...params,
    });
  }

  /**
   * Get document by ID
   */
  async getDocument(id: string): Promise<any> {
    return await this.query('/get', { id });
  }

  /**
   * Add/update documents
   */
  async update(documents: any[]): Promise<any> {
    const host = this.getConfig<string>('host');
    const port = this.getConfig<number>('port', 8983);
    const core = this.getConfig<string>('core');
    const useSsl = this.getConfig<boolean>('useSsl', false);

    const protocol = useSsl ? 'https' : 'http';
    const coreSegment = core ? `/solr/${core}` : '/solr';
    const url = `${protocol}://${host}:${port}${coreSegment}/update?wt=json&commit=true`;

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(documents),
    });

    if (!response.ok) {
      throw new Error(`Update failed: ${response.status} ${response.statusText}`);
    }

    return await response.json();
  }

  /**
   * Delete documents by query
   */
  async deleteByQuery(query: string): Promise<any> {
    const host = this.getConfig<string>('host');
    const port = this.getConfig<number>('port', 8983);
    const core = this.getConfig<string>('core');
    const useSsl = this.getConfig<boolean>('useSsl', false);

    const protocol = useSsl ? 'https' : 'http';
    const coreSegment = core ? `/solr/${core}` : '/solr';
    const url = `${protocol}://${host}:${port}${coreSegment}/update?wt=json&commit=true`;

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ delete: { query } }),
    });

    if (!response.ok) {
      throw new Error(`Delete failed: ${response.status} ${response.statusText}`);
    }

    return await response.json();
  }

  /**
   * Delete document by ID
   */
  async deleteById(id: string): Promise<any> {
    const host = this.getConfig<string>('host');
    const port = this.getConfig<number>('port', 8983);
    const core = this.getConfig<string>('core');
    const useSsl = this.getConfig<boolean>('useSsl', false);

    const protocol = useSsl ? 'https' : 'http';
    const coreSegment = core ? `/solr/${core}` : '/solr';
    const url = `${protocol}://${host}:${port}${coreSegment}/update?wt=json&commit=true`;

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ delete: { id } }),
    });

    if (!response.ok) {
      throw new Error(`Delete failed: ${response.status} ${response.statusText}`);
    }

    return await response.json();
  }

  /**
   * Commit changes
   */
  async commit(): Promise<any> {
    return await this.query('/update', { commit: 'true' });
  }

  /**
   * Optimize index
   */
  async optimize(): Promise<any> {
    return await this.query('/update', { optimize: 'true' });
  }

  /**
   * Get schema
   */
  async getSchema(): Promise<any> {
    return await this.query('/schema');
  }

  /**
   * Get core status
   */
  async getCoreStatus(): Promise<any> {
    const core = this.getConfig<string>('core');
    return await this.query(`/admin/cores`, { action: 'STATUS', core });
  }

  /**
   * Faceted search
   */
  async facetSearch(query: string, facetFields: string[], params?: Record<string, any>): Promise<any> {
    return await this.query('/select', {
      q: query,
      'facet': 'true',
      'facet.field': facetFields,
      ...params,
    });
  }

  /**
   * Suggest/autocomplete
   */
  async suggest(query: string, dictionary: string = 'suggest'): Promise<any> {
    return await this.query('/suggest', {
      'suggest.q': query,
      'suggest.dictionary': dictionary,
    });
  }
}
