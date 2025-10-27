// Mapping Storage Service - DuckDB integration

import { DataMappingConfig } from '../types';

export class MappingStorage {
  /**
   * Save mapping to DuckDB
   */
  async saveMapping(mapping: DataMappingConfig): Promise<void> {
    // TODO: Implement DuckDB integration
    // For now, just store in localStorage as fallback
    try {
      const stored = this.getAllMappings();
      const index = stored.findIndex((m) => m.id === mapping.id);

      if (index >= 0) {
        stored[index] = mapping;
      } else {
        stored.push(mapping);
      }

      localStorage.setItem('data_mappings', JSON.stringify(stored));
    } catch (error) {
      console.error('Failed to save mapping:', error);
      throw error;
    }
  }

  /**
   * Get all saved mappings
   */
  getAllMappings(): DataMappingConfig[] {
    try {
      const stored = localStorage.getItem('data_mappings');
      return stored ? JSON.parse(stored) : [];
    } catch (error) {
      console.error('Failed to load mappings:', error);
      return [];
    }
  }

  /**
   * Get mapping by ID
   */
  async getMapping(id: string): Promise<DataMappingConfig | null> {
    const mappings = this.getAllMappings();
    return mappings.find((m) => m.id === id) || null;
  }

  /**
   * Delete mapping
   */
  async deleteMapping(id: string): Promise<void> {
    try {
      const stored = this.getAllMappings();
      const filtered = stored.filter((m) => m.id !== id);
      localStorage.setItem('data_mappings', JSON.stringify(filtered));
    } catch (error) {
      console.error('Failed to delete mapping:', error);
      throw error;
    }
  }

  /**
   * Search mappings
   */
  async searchMappings(query: string): Promise<DataMappingConfig[]> {
    const all = this.getAllMappings();
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
    const all = this.getAllMappings();
    // Note: source.connectionId no longer exists in new architecture
    return all.filter((m) => m.source.type === 'api' && m.source.apiConfig?.id === connectionId);
  }

  /**
   * Get mappings by schema
   */
  async getMappingsBySchema(schema: string): Promise<DataMappingConfig[]> {
    const all = this.getAllMappings();
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
    const original = await this.getMapping(id);
    if (!original) return null;

    const duplicate: DataMappingConfig = {
      ...original,
      id: `map_${Date.now()}`,
      name: `${original.name} (Copy)`,
      metadata: {
        ...original.metadata,
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
      },
    };

    await this.saveMapping(duplicate);
    return duplicate;
  }
}

// Export singleton
export const mappingStorage = new MappingStorage();
