// MQTT Message Broker Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MQTTAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const broker = this.getConfig<string>('broker');
      const port = this.getConfig<number>('port', 1883);

      if (!broker) {
        return this.createErrorResult('Broker URL is required');
      }

      console.log('Validating MQTT configuration (client-side only)');

      // MQTT requires WebSocket or native MQTT protocol libraries (mqtt.js)
      // For now, validate configuration only
      return this.createSuccessResult(`Configuration validated for MQTT broker at ${broker}:${port}`, {
        validatedAt: new Date().toISOString(),
        broker,
        port,
        clientId: this.getConfig('clientId') || 'Auto-generated',
        note: 'Client-side validation only. MQTT connections require backend integration with mqtt.js or similar libraries.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // MQTT operations need to go through backend
      const response = await fetch('/api/data-sources/mqtt', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'mqtt',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`MQTT operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`MQTT query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      broker: this.getConfig('broker'),
      port: this.getConfig('port', 1883),
      clientId: this.getConfig('clientId'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Publish message to topic
   */
  async publish(topic: string, message: string, qos: 0 | 1 | 2 = 0): Promise<any> {
    return await this.query('publish', { topic, message, qos });
  }

  /**
   * Subscribe to topic
   */
  async subscribe(topic: string, qos: 0 | 1 | 2 = 0): Promise<any> {
    return await this.query('subscribe', { topic, qos });
  }

  /**
   * Unsubscribe from topic
   */
  async unsubscribe(topic: string): Promise<any> {
    return await this.query('unsubscribe', { topic });
  }

  /**
   * Get connection status
   */
  async getStatus(): Promise<any> {
    return await this.query('getStatus');
  }

  /**
   * Disconnect from broker
   */
  async disconnect(): Promise<any> {
    return await this.query('disconnect');
  }
}
