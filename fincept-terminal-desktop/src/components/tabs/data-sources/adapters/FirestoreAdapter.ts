// Google Cloud Firestore Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class FirestoreAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const projectId = this.getConfig<string>('projectId');
      const credentials = this.getConfig<string>('credentials');

      if (!projectId || !credentials) {
        return this.createErrorResult('Project ID and service account credentials are required');
      }

      console.log('Validating Firestore configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for Firestore project "${projectId}"`, {
        validatedAt: new Date().toISOString(),
        projectId,
        note: 'Client-side validation only. Firestore requires backend integration with @google-cloud/firestore.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/firestore', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'firestore',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Firestore query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      projectId: this.getConfig('projectId'),
      hasCredentials: !!this.getConfig('credentials'),
    };
  }

  async getDocument(collection: string, docId: string): Promise<any> {
    return await this.query('getDocument', { collection, docId });
  }

  async addDocument(collection: string, data: any): Promise<any> {
    return await this.query('addDocument', { collection, data });
  }

  async updateDocument(collection: string, docId: string, data: any): Promise<any> {
    return await this.query('updateDocument', { collection, docId, data });
  }

  async deleteDocument(collection: string, docId: string): Promise<any> {
    return await this.query('deleteDocument', { collection, docId });
  }

  async queryCollection(collection: string, filters?: any[]): Promise<any> {
    return await this.query('queryCollection', { collection, filters });
  }
}
