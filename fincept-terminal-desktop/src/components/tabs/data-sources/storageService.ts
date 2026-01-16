// SQLite Storage Service for Data Source Connections
// Uses SQLiteService for persistent storage

import { DataSourceConnection } from './types';
import { sqliteService } from '@/services/core/sqliteService';

export class DataSourceStorageService {
  /**
   * Save a new data source connection
   */
  static async saveConnection(connection: DataSourceConnection): Promise<void> {
    try {
      await sqliteService.saveDataSourceConnection({
        id: connection.id,
        name: connection.name,
        type: connection.type,
        category: connection.category,
        config: JSON.stringify(connection.config),
        status: connection.status,
        lastTested: connection.lastTested,
        errorMessage: connection.errorMessage,
      });
    } catch (error) {
      console.error('Error saving connection:', error);
      throw error;
    }
  }

  /**
   * Update an existing data source connection
   */
  static async updateConnection(id: string, updates: Partial<DataSourceConnection>): Promise<void> {
    try {
      const updateData: any = {};

      if (updates.name !== undefined) updateData.name = updates.name;
      if (updates.config !== undefined) updateData.config = JSON.stringify(updates.config);
      if (updates.status !== undefined) updateData.status = updates.status;
      if (updates.lastTested !== undefined) updateData.lastTested = updates.lastTested;
      if (updates.errorMessage !== undefined) updateData.errorMessage = updates.errorMessage;

      await sqliteService.updateDataSourceConnection(id, updateData);
    } catch (error) {
      console.error('Error updating connection:', error);
      throw error;
    }
  }

  /**
   * Delete a data source connection
   */
  static async deleteConnection(id: string): Promise<void> {
    try {
      await sqliteService.deleteDataSourceConnection(id);
    } catch (error) {
      console.error('Error deleting connection:', error);
      throw error;
    }
  }

  /**
   * Get a single connection by ID
   */
  static async getConnection(id: string): Promise<DataSourceConnection | null> {
    try {
      const result = await sqliteService.getDataSourceConnection(id);
      if (!result) return null;

      // Map snake_case DB fields to camelCase interface
      return {
        id: result.id,
        name: result.name,
        type: result.type,
        category: result.category,
        config: JSON.parse(result.config),
        status: result.status,
        lastTested: result.lastTested,
        errorMessage: result.errorMessage,
        createdAt: result.created_at,
        updatedAt: result.updated_at,
      } as DataSourceConnection;
    } catch (error) {
      console.error('Error getting connection:', error);
      return null;
    }
  }

  /**
   * Get all connections
   */
  static async getAllConnections(): Promise<DataSourceConnection[]> {
    try {
      const results = await sqliteService.getAllDataSourceConnections();
      return results.map((r: any) => ({
        ...r,
        config: JSON.parse(r.config),
      }));
    } catch (error) {
      console.error('Error getting connections:', error);
      return [];
    }
  }

  /**
   * Get connections by category
   */
  static async getConnectionsByCategory(category: string): Promise<DataSourceConnection[]> {
    try {
      const results = await sqliteService.getDataSourceConnectionsByCategory(category);
      return results.map((r: any) => ({
        ...r,
        config: JSON.parse(r.config),
      }));
    } catch (error) {
      console.error('Error getting connections by category:', error);
      return [];
    }
  }

  /**
   * Get connections by type
   */
  static async getConnectionsByType(type: string): Promise<DataSourceConnection[]> {
    try {
      const results = await sqliteService.getDataSourceConnectionsByType(type);
      return results.map((r: any) => ({
        ...r,
        config: JSON.parse(r.config),
      }));
    } catch (error) {
      console.error('Error getting connections by type:', error);
      return [];
    }
  }

  /**
   * Initialize the database schema
   */
  static async initializeSchema(): Promise<void> {
    try {
      // Schema is created automatically by sqliteService
      console.log('Storage service initialized (using SQLite)');
    } catch (error) {
      console.error('Error initializing schema:', error);
      throw error;
    }
  }

  /**
   * Test a connection using the appropriate adapter
   */
  static async testConnection(connection: DataSourceConnection): Promise<{
    success: boolean;
    message: string;
    metadata?: Record<string, any>;
  }> {
    try {
      // Dynamic import to avoid circular dependencies
      const { testConnection: adapterTestConnection, hasAdapter } = await import('./adapters');

      // Check if we have an adapter for this type
      if (hasAdapter(connection.type)) {
        const startTime = performance.now();
        const result = await adapterTestConnection(connection);
        const latency = performance.now() - startTime;

        return {
          ...result,
          metadata: {
            ...result.metadata,
            latency: Math.round(latency),
            testedAt: new Date().toISOString(),
          },
        };
      }

      // Fallback: Return a message indicating no adapter is available
      return {
        success: false,
        message: `No adapter available for connection type: ${connection.type}. Connection testing is not yet implemented for this type.`,
        metadata: {
          testedAt: new Date().toISOString(),
          fallback: true,
        },
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Connection test failed',
      };
    }
  }

  /**
   * Export connections to JSON
   */
  static async exportConnections(): Promise<string> {
    try {
      const connections = await this.getAllConnections();
      return JSON.stringify(connections, null, 2);
    } catch (error) {
      console.error('Error exporting connections:', error);
      throw error;
    }
  }

  /**
   * Import connections from JSON
   */
  static async importConnections(jsonData: string): Promise<number> {
    try {
      const connections: DataSourceConnection[] = JSON.parse(jsonData);
      const existing = await this.getAllConnections();

      // Merge and deduplicate by ID
      let imported = 0;

      for (const conn of connections) {
        const exists = existing.find((c) => c.id === conn.id);
        if (!exists) {
          await this.saveConnection(conn);
          imported++;
        }
      }

      return imported;
    } catch (error) {
      console.error('Error importing connections:', error);
      throw error;
    }
  }
}

// Initialize on import
DataSourceStorageService.initializeSchema();
