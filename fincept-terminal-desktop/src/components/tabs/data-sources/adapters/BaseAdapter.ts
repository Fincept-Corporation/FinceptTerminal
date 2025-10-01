// Base Adapter Interface for Data Source Connections
// All data source adapters should extend this base class

import { DataSourceConnection, TestConnectionResult } from '../types';

export abstract class BaseAdapter {
  protected connection: DataSourceConnection;

  constructor(connection: DataSourceConnection) {
    this.connection = connection;
  }

  /**
   * Test the connection to the data source
   * Must be implemented by each adapter
   */
  abstract testConnection(): Promise<TestConnectionResult>;

  /**
   * Connect to the data source
   * Optional: Override if your adapter needs explicit connection setup
   */
  async connect(): Promise<void> {
    // Default implementation - can be overridden
    console.log(`Connecting to ${this.connection.name}...`);
  }

  /**
   * Disconnect from the data source
   * Optional: Override if your adapter needs cleanup
   */
  async disconnect(): Promise<void> {
    // Default implementation - can be overridden
    console.log(`Disconnecting from ${this.connection.name}...`);
  }

  /**
   * Query data from the data source
   * Optional: Implement if your adapter supports querying
   */
  async query(query: string, request?: any): Promise<any> {
    throw new Error(`Query not implemented for ${this.connection.type}`);
  }

  /**
   * Get metadata about the data source
   * Optional: Override to provide specific metadata
   */
  async getMetadata(): Promise<Record<string, any>> {
    return {
      type: this.connection.type,
      name: this.connection.name,
      category: this.connection.category,
    };
  }

  /**
   * Validate connection configuration
   * Override to add custom validation logic
   */
  protected validateConfig(): { valid: boolean; errors: string[] } {
    const errors: string[] = [];

    if (!this.connection.config) {
      errors.push('Configuration is missing');
    }

    return {
      valid: errors.length === 0,
      errors,
    };
  }

  /**
   * Get configuration value with type safety
   */
  protected getConfig<T = any>(key: string, defaultValue?: T): T {
    return this.connection.config[key] ?? defaultValue;
  }

  /**
   * Create a standardized error response
   */
  protected createErrorResult(error: unknown): TestConnectionResult {
    const message = error instanceof Error ? error.message : String(error);
    return {
      success: false,
      message: `Connection failed: ${message}`,
    };
  }

  /**
   * Create a standardized success response
   */
  protected createSuccessResult(
    message: string = 'Connection successful',
    metadata?: Record<string, any>
  ): TestConnectionResult {
    return {
      success: true,
      message,
      metadata,
    };
  }
}
