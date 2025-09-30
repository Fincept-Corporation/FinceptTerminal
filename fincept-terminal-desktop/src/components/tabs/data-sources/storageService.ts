// DuckDB Storage Service for Data Source Connections
// This service will handle saving/loading data source configurations

import { DataSourceConnection } from './types';

// Local storage key for connections (temporary until DuckDB integration)
const STORAGE_KEY = 'fincept_data_source_connections';

export class DataSourceStorageService {
  /**
   * Save a new data source connection
   */
  static async saveConnection(connection: DataSourceConnection): Promise<void> {
    try {
      const connections = await this.getAllConnections();
      connections.push(connection);
      localStorage.setItem(STORAGE_KEY, JSON.stringify(connections));

      // TODO: Implement DuckDB storage
      // await duckdb.query(`
      //   INSERT INTO data_source_connections
      //   (id, name, type, category, config, status, created_at, updated_at)
      //   VALUES (?, ?, ?, ?, ?, ?, ?, ?)
      // `, [connection.id, connection.name, ...]);
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
      const connections = await this.getAllConnections();
      const index = connections.findIndex((c) => c.id === id);

      if (index !== -1) {
        connections[index] = {
          ...connections[index],
          ...updates,
          updatedAt: new Date().toISOString(),
        };
        localStorage.setItem(STORAGE_KEY, JSON.stringify(connections));
      }

      // TODO: Implement DuckDB update
      // await duckdb.query(`
      //   UPDATE data_source_connections
      //   SET name = ?, config = ?, status = ?, updated_at = ?
      //   WHERE id = ?
      // `, [...]);
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
      const connections = await this.getAllConnections();
      const filtered = connections.filter((c) => c.id !== id);
      localStorage.setItem(STORAGE_KEY, JSON.stringify(filtered));

      // TODO: Implement DuckDB deletion
      // await duckdb.query(`DELETE FROM data_source_connections WHERE id = ?`, [id]);
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
      const connections = await this.getAllConnections();
      return connections.find((c) => c.id === id) || null;

      // TODO: Implement DuckDB query
      // const result = await duckdb.query(`
      //   SELECT * FROM data_source_connections WHERE id = ?
      // `, [id]);
      // return result[0] || null;
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
      const stored = localStorage.getItem(STORAGE_KEY);
      return stored ? JSON.parse(stored) : [];

      // TODO: Implement DuckDB query
      // const result = await duckdb.query(`
      //   SELECT * FROM data_source_connections ORDER BY created_at DESC
      // `);
      // return result;
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
      const connections = await this.getAllConnections();
      return connections.filter((c) => c.category === category);

      // TODO: Implement DuckDB query
      // const result = await duckdb.query(`
      //   SELECT * FROM data_source_connections
      //   WHERE category = ?
      //   ORDER BY created_at DESC
      // `, [category]);
      // return result;
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
      const connections = await this.getAllConnections();
      return connections.filter((c) => c.type === type);
    } catch (error) {
      console.error('Error getting connections by type:', error);
      return [];
    }
  }

  /**
   * Initialize the database schema (for DuckDB)
   */
  static async initializeSchema(): Promise<void> {
    try {
      // TODO: Implement DuckDB schema creation
      // await duckdb.query(`
      //   CREATE TABLE IF NOT EXISTS data_source_connections (
      //     id VARCHAR PRIMARY KEY,
      //     name VARCHAR NOT NULL,
      //     type VARCHAR NOT NULL,
      //     category VARCHAR NOT NULL,
      //     config JSON NOT NULL,
      //     status VARCHAR NOT NULL,
      //     created_at TIMESTAMP NOT NULL,
      //     updated_at TIMESTAMP NOT NULL,
      //     last_tested TIMESTAMP,
      //     error_message VARCHAR
      //   )
      // `);
      console.log('Storage service initialized (using localStorage)');
    } catch (error) {
      console.error('Error initializing schema:', error);
      throw error;
    }
  }

  /**
   * Test a connection (placeholder for actual implementation)
   */
  static async testConnection(connection: DataSourceConnection): Promise<{
    success: boolean;
    message: string;
    metadata?: Record<string, any>;
  }> {
    try {
      // TODO: Implement actual connection testing based on type
      // This would make actual network requests to test the connection

      // Simulate connection test
      await new Promise((resolve) => setTimeout(resolve, 1500));

      return {
        success: true,
        message: `Successfully connected to ${connection.name}`,
        metadata: {
          latency: Math.random() * 100 + 50,
          version: '1.0.0',
        },
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Connection failed',
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
      const merged = [...existing];
      let imported = 0;

      for (const conn of connections) {
        const index = merged.findIndex((c) => c.id === conn.id);
        if (index === -1) {
          merged.push(conn);
          imported++;
        }
      }

      localStorage.setItem(STORAGE_KEY, JSON.stringify(merged));
      return imported;
    } catch (error) {
      console.error('Error importing connections:', error);
      throw error;
    }
  }
}

// Initialize on import
DataSourceStorageService.initializeSchema();
