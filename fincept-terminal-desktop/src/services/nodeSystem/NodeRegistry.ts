/**
 * Node Registry
 * Central registry for all node types in the system
 *
 * Manages node type registration, versioning, and retrieval
 */

import { INodeType, INodeTypeDescription, IVersionedNodeType } from './types';

/**
 * Registry entry for a node type
 */
interface INodeTypeRegistry {
  [nodeType: string]: {
    versions: {
      [version: number]: INodeType;
    };
    latestVersion: number;
    className: string;
  };
}

/**
 * Node Registry Class
 * Singleton pattern for managing node types
 */
class NodeRegistryClass {
  private registry: INodeTypeRegistry = {};
  private loadedNodeTypes: Set<string> = new Set();

  /**
   * Register a node type
   */
  registerNodeType(nodeType: INodeType, className?: string): void {
    const description = nodeType.description;
    const versions = Array.isArray(description.version)
      ? description.version
      : [description.version];

    // Get node type name
    const nodeTypeName = description.name;

    // Initialize registry entry if needed
    if (!this.registry[nodeTypeName]) {
      this.registry[nodeTypeName] = {
        versions: {},
        latestVersion: Math.max(...versions),
        className: className || nodeTypeName,
      };
    }

    // Register each version
    for (const version of versions) {
      this.registry[nodeTypeName].versions[version] = nodeType;

      // Update latest version
      if (version > this.registry[nodeTypeName].latestVersion) {
        this.registry[nodeTypeName].latestVersion = version;
      }
    }

    this.loadedNodeTypes.add(nodeTypeName);

    console.log(
      `[NodeRegistry] Registered ${nodeTypeName} versions: ${versions.join(', ')}`
    );
  }

  /**
   * Register a versioned node type
   */
  registerVersionedNodeType(versionedNodeType: IVersionedNodeType, className?: string): void {
    const versions = Object.keys(versionedNodeType.nodeVersions).map(Number);
    const nodeTypeName = versionedNodeType.nodeVersions[versions[0]].description.name;

    // Initialize registry entry
    this.registry[nodeTypeName] = {
      versions: {},
      latestVersion: Math.max(...versions),
      className: className || nodeTypeName,
    };

    // Register each version
    for (const [version, nodeType] of Object.entries(versionedNodeType.nodeVersions)) {
      this.registry[nodeTypeName].versions[Number(version)] = nodeType;
    }

    this.loadedNodeTypes.add(nodeTypeName);

    console.log(
      `[NodeRegistry] Registered versioned node ${nodeTypeName} versions: ${versions.join(', ')}`
    );
  }

  /**
   * Get a node type by name and version
   */
  getNodeType(nodeTypeName: string, version?: number): INodeType {
    const entry = this.registry[nodeTypeName];

    if (!entry) {
      throw new Error(`Node type "${nodeTypeName}" not found in registry`);
    }

    // Use latest version if not specified
    const targetVersion = version ?? entry.latestVersion;

    const nodeType = entry.versions[targetVersion];

    if (!nodeType) {
      throw new Error(
        `Node type "${nodeTypeName}" version ${targetVersion} not found. ` +
        `Available versions: ${Object.keys(entry.versions).join(', ')}`
      );
    }

    return nodeType;
  }

  /**
   * Get node type description
   */
  getNodeTypeDescription(nodeTypeName: string, version?: number): INodeTypeDescription {
    const nodeType = this.getNodeType(nodeTypeName, version);
    return nodeType.description;
  }

  /**
   * Get all registered node types
   */
  getAllNodeTypes(): Array<{
    name: string;
    versions: number[];
    latestVersion: number;
  }> {
    return Object.entries(this.registry).map(([name, entry]) => ({
      name,
      versions: Object.keys(entry.versions).map(Number).sort((a, b) => a - b),
      latestVersion: entry.latestVersion,
    }));
  }

  /**
   * Get all nodes (latest version of each node type with descriptions)
   */
  getAllNodes(): INodeTypeDescription[] {
    return Object.entries(this.registry).map(([name, entry]) => {
      const latestNodeType = entry.versions[entry.latestVersion];
      return latestNodeType.description;
    });
  }

  /**
   * Get node types by category
   */
  getNodeTypesByCategory(category: string): INodeTypeDescription[] {
    const nodeTypes: INodeTypeDescription[] = [];

    for (const [name, entry] of Object.entries(this.registry)) {
      const description = entry.versions[entry.latestVersion].description;

      if (description.group.includes(category)) {
        nodeTypes.push(description);
      }
    }

    return nodeTypes;
  }

  /**
   * Check if node type exists
   */
  hasNodeType(nodeTypeName: string, version?: number): boolean {
    const entry = this.registry[nodeTypeName];

    if (!entry) {
      return false;
    }

    if (version === undefined) {
      return true;
    }

    return entry.versions[version] !== undefined;
  }

  /**
   * Get node type versions
   */
  getNodeTypeVersions(nodeTypeName: string): number[] {
    const entry = this.registry[nodeTypeName];

    if (!entry) {
      return [];
    }

    return Object.keys(entry.versions).map(Number).sort((a, b) => a - b);
  }

  /**
   * Unregister a node type (for testing/development)
   */
  unregisterNodeType(nodeTypeName: string): void {
    delete this.registry[nodeTypeName];
    this.loadedNodeTypes.delete(nodeTypeName);
    console.log(`[NodeRegistry] Unregistered ${nodeTypeName}`);
  }

  /**
   * Clear all registrations (for testing)
   */
  clear(): void {
    this.registry = {};
    this.loadedNodeTypes.clear();
    console.log('[NodeRegistry] Cleared all node types');
  }

  /**
   * Get registry statistics
   */
  getStatistics(): {
    totalNodeTypes: number;
    totalVersions: number;
    nodeTypes: Array<{
      name: string;
      versions: number;
      latestVersion: number;
    }>;
  } {
    let totalVersions = 0;

    const nodeTypes = Object.entries(this.registry).map(([name, entry]) => {
      const versions = Object.keys(entry.versions).length;
      totalVersions += versions;

      return {
        name,
        versions,
        latestVersion: entry.latestVersion,
      };
    });

    return {
      totalNodeTypes: Object.keys(this.registry).length,
      totalVersions,
      nodeTypes,
    };
  }

  /**
   * Validate node configuration
   */
  validateNode(nodeTypeName: string, version: number, parameters: any): {
    valid: boolean;
    errors: string[];
  } {
    const errors: string[] = [];

    try {
      const nodeType = this.getNodeType(nodeTypeName, version);
      const description = nodeType.description;

      // Validate required parameters
      for (const property of description.properties) {
        if (property.required) {
          if (parameters[property.name] === undefined || parameters[property.name] === null) {
            errors.push(`Required parameter "${property.name}" is missing`);
          }
        }
      }

      // TODO: Add more validation
      // - Type validation
      // - Value range validation
      // - Dependent parameter validation

      return {
        valid: errors.length === 0,
        errors,
      };
    } catch (error: any) {
      return {
        valid: false,
        errors: [error.message],
      };
    }
  }

  /**
   * Get node type categories
   */
  getCategories(): string[] {
    const categories = new Set<string>();

    for (const entry of Object.values(this.registry)) {
      const description = entry.versions[entry.latestVersion].description;
      description.group.forEach((group) => categories.add(group));
    }

    return Array.from(categories).sort();
  }

  /**
   * Search node types
   */
  searchNodeTypes(query: string): INodeTypeDescription[] {
    const lowerQuery = query.toLowerCase();
    const results: INodeTypeDescription[] = [];

    for (const entry of Object.values(this.registry)) {
      const description = entry.versions[entry.latestVersion].description;

      // Search in name, displayName, and description
      if (
        description.name.toLowerCase().includes(lowerQuery) ||
        description.displayName.toLowerCase().includes(lowerQuery) ||
        description.description.toLowerCase().includes(lowerQuery)
      ) {
        results.push(description);
      }
    }

    return results;
  }
}

// Export singleton instance
export const NodeRegistry = new NodeRegistryClass();

// Export class for testing
export { NodeRegistryClass };
