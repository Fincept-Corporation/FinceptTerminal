// Neo4j Graph Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class Neo4jAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url', 'bolt://localhost:7687');
      const username = this.getConfig<string>('username', 'neo4j');
      const password = this.getConfig<string>('password');
      const database = this.getConfig<string>('database', 'neo4j');

      if (!url || !username || !password) {
        return this.createErrorResult('URL, username, and password are required');
      }

      console.log('Validating Neo4j configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for Neo4j at ${url}`,
        {
          validatedAt: new Date().toISOString(),
          url,
          username,
          database,
          note: 'Client-side validation only. Neo4j uses Bolt protocol and requires backend integration with neo4j-driver.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(cypher: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'neo4j',
          query: cypher,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Neo4j query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      database: this.getConfig('database', 'neo4j'),
      username: this.getConfig('username', 'neo4j'),
    };
  }

  /**
   * Get database information
   */
  async getDatabaseInfo(): Promise<any> {
    return await this.query('CALL dbms.components() YIELD name, versions, edition RETURN name, versions, edition');
  }

  /**
   * Get node labels
   */
  async getNodeLabels(): Promise<any> {
    return await this.query('CALL db.labels()');
  }

  /**
   * Get relationship types
   */
  async getRelationshipTypes(): Promise<any> {
    return await this.query('CALL db.relationshipTypes()');
  }

  /**
   * Get property keys
   */
  async getPropertyKeys(): Promise<any> {
    return await this.query('CALL db.propertyKeys()');
  }

  /**
   * Get database statistics
   */
  async getDatabaseStats(): Promise<any> {
    return await this.query(`
      MATCH (n)
      WITH count(n) as nodeCount
      MATCH ()-[r]->()
      WITH nodeCount, count(r) as relationshipCount
      RETURN nodeCount, relationshipCount
    `);
  }

  /**
   * Create node
   */
  async createNode(label: string, properties: Record<string, any>): Promise<any> {
    const propString = Object.entries(properties)
      .map(([key, value]) => `${key}: $${key}`)
      .join(', ');

    return await this.query(`CREATE (n:${label} {${propString}}) RETURN n`, properties);
  }

  /**
   * Create relationship
   */
  async createRelationship(
    fromId: string,
    toId: string,
    relType: string,
    properties?: Record<string, any>
  ): Promise<any> {
    const propString = properties
      ? `{${Object.entries(properties)
          .map(([key, value]) => `${key}: $${key}`)
          .join(', ')}}`
      : '';

    const params = {
      fromId,
      toId,
      ...properties,
    };

    return await this.query(
      `
      MATCH (a), (b)
      WHERE id(a) = $fromId AND id(b) = $toId
      CREATE (a)-[r:${relType} ${propString}]->(b)
      RETURN r
    `,
      params
    );
  }

  /**
   * Find nodes by label and properties
   */
  async findNodes(label: string, properties?: Record<string, any>, limit: number = 100): Promise<any> {
    if (properties && Object.keys(properties).length > 0) {
      const propString = Object.entries(properties)
        .map(([key, value]) => `n.${key} = $${key}`)
        .join(' AND ');

      return await this.query(`MATCH (n:${label}) WHERE ${propString} RETURN n LIMIT ${limit}`, properties);
    }

    return await this.query(`MATCH (n:${label}) RETURN n LIMIT ${limit}`);
  }

  /**
   * Run shortest path query
   */
  async shortestPath(fromId: string, toId: string, maxDepth: number = 5): Promise<any> {
    return await this.query(
      `
      MATCH (start), (end)
      WHERE id(start) = $fromId AND id(end) = $toId
      MATCH p = shortestPath((start)-[*..${maxDepth}]-(end))
      RETURN p
    `,
      { fromId, toId }
    );
  }
}
