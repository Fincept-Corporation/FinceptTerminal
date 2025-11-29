// Mapping Storage Service - SQLite integration via MappingDatabase

import { DataMappingConfig } from '../types';
import { mappingDatabase } from './MappingDatabase';

export class MappingStorage {
  /**
   * Save mapping to SQLite
   */
  async saveMapping(mapping: DataMappingConfig): Promise<void> {
    await mappingDatabase.saveMapping(mapping);
  }

  /**
   * Get all saved mappings
   */
  async getAllMappings(): Promise<DataMappingConfig[]> {
    return await mappingDatabase.getAllMappings();
  }

  /**
   * Get mapping by ID
   */
  async getMapping(id: string): Promise<DataMappingConfig | null> {
    return await mappingDatabase.getMapping(id);
  }

  /**
   * Delete mapping
   */
  async deleteMapping(id: string): Promise<void> {
    await mappingDatabase.deleteMapping(id);
  }

  /**
   * Search mappings
   */
  async searchMappings(query: string): Promise<DataMappingConfig[]> {
    const all = await this.getAllMappings();
    const lowerQuery = query.toLowerCase();

    return all.filter(
      (m) =>
        m.name.toLowerCase().includes(lowerQuery) ||
        m.description?.toLowerCase().includes(lowerQuery) ||
        m.metadata.tags.some((tag) => tag.toLowerCase().includes(lowerQuery))
    );
  }

  /**
   * Get mappings by connection ID (OBSOLETE - kept for backward compatibility)
   */
  async getMappingsByConnection(connectionId: string): Promise<DataMappingConfig[]> {
    const all = await this.getAllMappings();
    // Note: source.connectionId no longer exists in new architecture
    return all.filter((m) => m.source.type === 'api' && m.source.apiConfig?.id === connectionId);
  }

  /**
   * Get mappings by schema
   */
  async getMappingsBySchema(schema: string): Promise<DataMappingConfig[]> {
    const all = await this.getAllMappings();
    return all.filter((m) => m.target.schema === schema);
  }

  /**
   * Export mapping as JSON
   */
  exportMapping(mapping: DataMappingConfig): string {
    return JSON.stringify(mapping, null, 2);
  }

  /**
   * Import mapping from JSON
   */
  async importMapping(json: string): Promise<DataMappingConfig> {
    try {
      const mapping = JSON.parse(json) as DataMappingConfig;

      // Generate new ID
      mapping.id = `map_${Date.now()}`;
      mapping.metadata.createdAt = new Date().toISOString();
      mapping.metadata.updatedAt = new Date().toISOString();

      await this.saveMapping(mapping);
      return mapping;
    } catch (error) {
      console.error('Failed to import mapping:', error);
      throw error;
    }
  }

  /**
   * Duplicate mapping
   */
  async duplicateMapping(id: string): Promise<DataMappingConfig | null> {
    return await mappingDatabase.duplicateMapping(id);
  }
}

// Export singleton
export const mappingStorage = new MappingStorage();
