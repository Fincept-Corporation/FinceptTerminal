// SQLite Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class SQLiteAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const filePath = this.getConfig<string>('filePath');
      const inMemory = this.getConfig<boolean>('inMemory', false);

      if (!filePath && !inMemory) {
        return this.createErrorResult('Either file path or in-memory flag is required');
      }

      console.log('Validating SQLite configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for SQLite database ${inMemory ? '(in-memory)' : `"${filePath}"`}`,
        {
          validatedAt: new Date().toISOString(),
          filePath,
          inMemory,
          readOnly: this.getConfig('readOnly', false),
          note: 'Client-side validation only. Full connection test requires backend integration.',
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
          type: 'sqlite',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`SQLite query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filePath: this.getConfig('filePath'),
      inMemory: this.getConfig('inMemory', false),
      readOnly: this.getConfig('readOnly', false),
    };
  }
}
