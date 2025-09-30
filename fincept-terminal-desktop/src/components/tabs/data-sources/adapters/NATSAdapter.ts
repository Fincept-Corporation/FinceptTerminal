// NATS Messaging System Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class NATSAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const servers = this.getConfig<string>('servers');

      if (!servers) {
        return this.createErrorResult('Server URLs are required');
      }

      console.log('Validating NATS configuration (client-side only)');

      // NATS requires specialized libraries (nats.js, nats.ws)
      // For now, validate configuration only
      return this.createSuccessResult(`Configuration validated for NATS at ${servers}`, {
        validatedAt: new Date().toISOString(),
        servers,
        hasCredentials: !!(this.getConfig('username') || this.getConfig('token')),
        note: 'Client-side validation only. NATS connections require backend integration with nats.js or nats.ws.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // NATS operations need to go through backend
      const response = await fetch('/api/data-sources/nats', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'nats',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`NATS operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`NATS query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      servers: this.getConfig('servers'),
      hasCredentials: !!(this.getConfig('username') || this.getConfig('token')),
    };
  }

  /**
   * Publish message to subject
   */
  async publish(subject: string, data: string | Uint8Array): Promise<any> {
    return await this.query('publish', { subject, data });
  }

  /**
   * Subscribe to subject
   */
  async subscribe(subject: string, queue?: string): Promise<any> {
    return await this.query('subscribe', { subject, queue });
  }

  /**
   * Request-reply pattern
   */
  async request(subject: string, data: string | Uint8Array, timeout?: number): Promise<any> {
    return await this.query('request', { subject, data, timeout });
  }

  /**
   * Unsubscribe from subject
   */
  async unsubscribe(subject: string): Promise<any> {
    return await this.query('unsubscribe', { subject });
  }

  /**
   * Get connection status
   */
  async getStatus(): Promise<any> {
    return await this.query('getStatus');
  }

  /**
   * Create JetStream context
   */
  async createJetStream(): Promise<any> {
    return await this.query('createJetStream');
  }

  /**
   * Publish to JetStream
   */
  async jetStreamPublish(subject: string, data: string | Uint8Array): Promise<any> {
    return await this.query('jetStreamPublish', { subject, data });
  }

  /**
   * Get server info
   */
  async getServerInfo(): Promise<any> {
    return await this.query('getServerInfo');
  }
}
