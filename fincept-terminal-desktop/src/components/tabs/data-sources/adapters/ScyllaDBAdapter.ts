// ScyllaDB (Cassandra-compatible) Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ScyllaDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const contactPoints = this.getConfig<string>('contactPoints');
      const keyspace = this.getConfig<string>('keyspace');

      if (!contactPoints) {
        return this.createErrorResult('Contact points are required');
      }

      console.log('Validating ScyllaDB configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for ScyllaDB at ${contactPoints}`, {
        validatedAt: new Date().toISOString(),
        contactPoints,
        keyspace,
        note: 'Client-side validation only. ScyllaDB requires backend integration with cassandra-driver.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(cql: string, params?: any[]): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/scylladb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'scylladb',
          cql,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`ScyllaDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      contactPoints: this.getConfig('contactPoints'),
      keyspace: this.getConfig('keyspace'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }
}
