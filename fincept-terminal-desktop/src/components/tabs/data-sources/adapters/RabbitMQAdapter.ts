// RabbitMQ Message Broker Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class RabbitMQAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host', 'localhost');
      const port = this.getConfig<number>('port', 5672);
      const managementPort = this.getConfig<number>('managementPort', 15672);
      const username = this.getConfig<string>('username', 'guest');
      const password = this.getConfig<string>('password', 'guest');
      const vhost = this.getConfig<string>('vhost', '/');

      if (!host || !username || !password) {
        return this.createErrorResult('Host, username, and password are required');
      }

      console.log('Testing RabbitMQ connection...');

      try {
        // Try to connect to management API
        const credentials = btoa(`${username}:${password}`);
        const managementUrl = `http://${host}:${managementPort}/api/overview`;

        const response = await fetch(managementUrl, {
          method: 'GET',
          headers: {
            Authorization: `Basic ${credentials}`,
          },
        });

        if (!response.ok) {
          if (response.status === 401) {
            throw new Error('Authentication failed. Check username and password.');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to RabbitMQ', {
          host,
          port,
          managementPort,
          vhost,
          version: data.rabbitmq_version,
          erlangVersion: data.erlang_version,
          clusterName: data.cluster_name,
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          if (fetchError.message.includes('Failed to fetch')) {
            return this.createErrorResult(
              `Cannot reach RabbitMQ management API at ${host}:${managementPort}. Is the server running?`
            );
          }
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(endpoint: string, method: string = 'GET', body?: any): Promise<any> {
    try {
      const host = this.getConfig<string>('host');
      const managementPort = this.getConfig<number>('managementPort', 15672);
      const username = this.getConfig<string>('username', 'guest');
      const password = this.getConfig<string>('password', 'guest');

      const credentials = btoa(`${username}:${password}`);
      const url = `http://${host}:${managementPort}/api${endpoint}`;

      const options: RequestInit = {
        method,
        headers: {
          Authorization: `Basic ${credentials}`,
          'Content-Type': 'application/json',
        },
      };

      if (body && method !== 'GET') {
        options.body = JSON.stringify(body);
      }

      const response = await fetch(url, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`RabbitMQ query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 5672),
      managementPort: this.getConfig('managementPort', 15672),
      vhost: this.getConfig('vhost', '/'),
    };
  }

  /**
   * Get overview
   */
  async getOverview(): Promise<any> {
    return await this.query('/overview');
  }

  /**
   * Get queues
   */
  async getQueues(vhost?: string): Promise<any> {
    const vh = vhost || this.getConfig<string>('vhost', '/');
    const encodedVhost = encodeURIComponent(vh);
    return await this.query(`/queues/${encodedVhost}`);
  }

  /**
   * Get exchanges
   */
  async getExchanges(vhost?: string): Promise<any> {
    const vh = vhost || this.getConfig<string>('vhost', '/');
    const encodedVhost = encodeURIComponent(vh);
    return await this.query(`/exchanges/${encodedVhost}`);
  }

  /**
   * Get connections
   */
  async getConnections(): Promise<any> {
    return await this.query('/connections');
  }

  /**
   * Get channels
   */
  async getChannels(): Promise<any> {
    return await this.query('/channels');
  }

  /**
   * Get consumers
   */
  async getConsumers(): Promise<any> {
    return await this.query('/consumers');
  }

  /**
   * Create queue
   */
  async createQueue(queueName: string, options?: any): Promise<any> {
    const vhost = this.getConfig<string>('vhost', '/');
    const encodedVhost = encodeURIComponent(vhost);
    return await this.query(`/queues/${encodedVhost}/${queueName}`, 'PUT', options);
  }

  /**
   * Delete queue
   */
  async deleteQueue(queueName: string): Promise<any> {
    const vhost = this.getConfig<string>('vhost', '/');
    const encodedVhost = encodeURIComponent(vhost);
    return await this.query(`/queues/${encodedVhost}/${queueName}`, 'DELETE');
  }

  /**
   * Get nodes
   */
  async getNodes(): Promise<any> {
    return await this.query('/nodes');
  }
}
