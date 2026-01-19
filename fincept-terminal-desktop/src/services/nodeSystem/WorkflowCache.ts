/**
 * Workflow Cache
 *
 * Hybrid caching system:
 * - SQLite (via Rust): Persistent cache for serializable data (executions, parameters, expressions)
 * - In-Memory: Session-only cache for non-serializable objects (DirectedGraph, node outputs)
 *
 * Features:
 * - Cache workflow execution results (SQLite)
 * - Memoize node execution outputs (In-Memory)
 * - Cache DirectedGraph instances (In-Memory - not serializable)
 * - Cache parameter evaluations (SQLite)
 * - LRU eviction policy for in-memory caches
 * - TTL (time-to-live) support via Rust cache
 * - Cache invalidation
 */

import type {
  IRunExecutionData,
  NodeParameterValue,
  INodeExecutionData,
} from './types';
import { DirectedGraph } from './DirectedGraph';
import { cacheService, CacheTTL } from '../cache/cacheService';

// Cache category for workflow data
const WORKFLOW_CACHE_CATEGORY = 'workflow';

/**
 * Cache statistics
 */
interface ICacheStats {
  hits: number;
  misses: number;
  size: number;
  entries: number;
  hitRate: number;
}

/**
 * Simple in-memory LRU cache for non-serializable objects
 * Used only for DirectedGraph and node outputs that can't be stored in SQLite
 */
class InMemoryLRUCache<T> {
  private cache: Map<string, { value: T; timestamp: number }>;
  private maxSize: number;
  private maxAge: number;
  private hits: number = 0;
  private misses: number = 0;

  constructor(maxSize: number = 100, maxAge: number = 5 * 60 * 1000) {
    this.cache = new Map();
    this.maxSize = maxSize;
    this.maxAge = maxAge;
  }

  get(key: string): T | undefined {
    const entry = this.cache.get(key);
    if (!entry) {
      this.misses++;
      return undefined;
    }

    if (Date.now() - entry.timestamp > this.maxAge) {
      this.cache.delete(key);
      this.misses++;
      return undefined;
    }

    this.hits++;
    // Move to end (LRU)
    this.cache.delete(key);
    this.cache.set(key, { ...entry, timestamp: Date.now() });
    return entry.value;
  }

  set(key: string, value: T): void {
    if (this.cache.has(key)) {
      this.cache.delete(key);
    }
    while (this.cache.size >= this.maxSize) {
      const firstKey = this.cache.keys().next().value;
      if (firstKey) this.cache.delete(firstKey);
    }
    this.cache.set(key, { value, timestamp: Date.now() });
  }

  delete(key: string): boolean {
    return this.cache.delete(key);
  }

  clear(): void {
    this.cache.clear();
    this.hits = 0;
    this.misses = 0;
  }

  keys(): string[] {
    return Array.from(this.cache.keys());
  }

  getStats(): ICacheStats {
    const total = this.hits + this.misses;
    return {
      hits: this.hits,
      misses: this.misses,
      size: this.cache.size,
      entries: this.cache.size,
      hitRate: total > 0 ? this.hits / total : 0,
    };
  }

  cleanup(): void {
    const now = Date.now();
    for (const [key, entry] of this.cache.entries()) {
      if (now - entry.timestamp > this.maxAge) {
        this.cache.delete(key);
      }
    }
  }
}

/**
 * Workflow Cache Manager
 * Hybrid: SQLite for persistent data, in-memory for session-only objects
 */
export class WorkflowCache {
  // In-memory caches for non-serializable objects
  private graphCache: InMemoryLRUCache<DirectedGraph>;
  private nodeOutputCache: InMemoryLRUCache<INodeExecutionData[][]>;

  // Track stats for SQLite-backed caches
  private sqliteHits: number = 0;
  private sqliteMisses: number = 0;

  private enabled: boolean = true;
  private cleanupInterval: ReturnType<typeof setInterval> | null = null;

  constructor() {
    // In-memory only for non-serializable data
    this.graphCache = new InMemoryLRUCache<DirectedGraph>(100, 30 * 60 * 1000);
    this.nodeOutputCache = new InMemoryLRUCache<INodeExecutionData[][]>(200, 5 * 60 * 1000);

    this.startCleanup();
  }

  /**
   * Cache workflow execution result (SQLite - persistent)
   */
  async cacheExecution(workflowId: string, executionId: string, result: IRunExecutionData): Promise<void> {
    if (!this.enabled) return;

    const key = `workflow:exec:${workflowId}:${executionId}`;
    await cacheService.setJSON(key, result, WORKFLOW_CACHE_CATEGORY, CacheTTL.LONG);
  }

  /**
   * Get cached workflow execution result (SQLite)
   */
  async getCachedExecution(workflowId: string, executionId: string): Promise<IRunExecutionData | undefined> {
    if (!this.enabled) return undefined;

    const key = `workflow:exec:${workflowId}:${executionId}`;
    const result = await cacheService.getJSON<IRunExecutionData>(key);

    if (result) {
      this.sqliteHits++;
      return result;
    }
    this.sqliteMisses++;
    return undefined;
  }

  /**
   * Cache node execution output (In-Memory - contains complex objects)
   */
  cacheNodeOutput(
    workflowId: string,
    nodeId: string,
    inputHash: string,
    output: INodeExecutionData[][]
  ): void {
    if (!this.enabled) return;
    const key = `${workflowId}:${nodeId}:${inputHash}`;
    this.nodeOutputCache.set(key, output);
  }

  /**
   * Get cached node output (In-Memory)
   */
  getCachedNodeOutput(
    workflowId: string,
    nodeId: string,
    inputHash: string
  ): INodeExecutionData[][] | undefined {
    if (!this.enabled) return undefined;
    const key = `${workflowId}:${nodeId}:${inputHash}`;
    return this.nodeOutputCache.get(key);
  }

  /**
   * Cache DirectedGraph for workflow (In-Memory - not serializable)
   */
  cacheGraph(workflowId: string, graph: DirectedGraph): void {
    if (!this.enabled) return;
    this.graphCache.set(workflowId, graph);
  }

  /**
   * Get cached DirectedGraph (In-Memory)
   */
  getCachedGraph(workflowId: string): DirectedGraph | undefined {
    if (!this.enabled) return undefined;
    return this.graphCache.get(workflowId);
  }

  /**
   * Cache parameter value (SQLite - persistent)
   */
  async cacheParameter(
    workflowId: string,
    nodeId: string,
    paramName: string,
    value: NodeParameterValue
  ): Promise<void> {
    if (!this.enabled) return;

    const key = `workflow:param:${workflowId}:${nodeId}:${paramName}`;
    await cacheService.setJSON(key, value, WORKFLOW_CACHE_CATEGORY, CacheTTL.MEDIUM);
  }

  /**
   * Get cached parameter value (SQLite)
   */
  async getCachedParameter(
    workflowId: string,
    nodeId: string,
    paramName: string
  ): Promise<NodeParameterValue | undefined> {
    if (!this.enabled) return undefined;

    const key = `workflow:param:${workflowId}:${nodeId}:${paramName}`;
    const result = await cacheService.getJSON<NodeParameterValue>(key);

    if (result !== null) {
      this.sqliteHits++;
      return result;
    }
    this.sqliteMisses++;
    return undefined;
  }

  /**
   * Cache expression evaluation result (SQLite - persistent)
   */
  async cacheExpression(expression: string, contextHash: string, result: any): Promise<void> {
    if (!this.enabled) return;

    const key = `workflow:expr:${contextHash}:${hashData(expression)}`;
    await cacheService.setJSON(key, result, WORKFLOW_CACHE_CATEGORY, CacheTTL.MEDIUM);
  }

  /**
   * Get cached expression result (SQLite)
   */
  async getCachedExpression(expression: string, contextHash: string): Promise<any | undefined> {
    if (!this.enabled) return undefined;

    const key = `workflow:expr:${contextHash}:${hashData(expression)}`;
    const result = await cacheService.getJSON(key);

    if (result !== null) {
      this.sqliteHits++;
      return result;
    }
    this.sqliteMisses++;
    return undefined;
  }

  /**
   * Invalidate all caches for a workflow
   */
  async invalidateWorkflow(workflowId: string): Promise<void> {
    // Invalidate SQLite caches
    await cacheService.invalidatePattern(`workflow:%:${workflowId}:%`);

    // Invalidate in-memory caches
    this.graphCache.delete(workflowId);

    for (const key of this.nodeOutputCache.keys()) {
      if (key.startsWith(`${workflowId}:`)) {
        this.nodeOutputCache.delete(key);
      }
    }
  }

  /**
   * Invalidate cache for a specific node
   */
  async invalidateNode(workflowId: string, nodeId: string): Promise<void> {
    // Invalidate SQLite parameter cache
    await cacheService.invalidatePattern(`workflow:param:${workflowId}:${nodeId}:%`);

    // Invalidate in-memory node output cache
    for (const key of this.nodeOutputCache.keys()) {
      if (key.startsWith(`${workflowId}:${nodeId}:`)) {
        this.nodeOutputCache.delete(key);
      }
    }
  }

  /**
   * Clear all caches
   */
  async clearAll(): Promise<void> {
    // Clear SQLite cache
    await cacheService.invalidateCategory(WORKFLOW_CACHE_CATEGORY);

    // Clear in-memory caches
    this.graphCache.clear();
    this.nodeOutputCache.clear();
    this.sqliteHits = 0;
    this.sqliteMisses = 0;
  }

  /**
   * Enable/disable caching
   */
  async setEnabled(enabled: boolean): Promise<void> {
    this.enabled = enabled;
    if (!enabled) {
      await this.clearAll();
    }
  }

  /**
   * Check if caching is enabled
   */
  isEnabled(): boolean {
    return this.enabled;
  }

  /**
   * Get cache statistics
   */
  async getStats(): Promise<{
    graph: ICacheStats;
    nodeOutput: ICacheStats;
    sqlite: ICacheStats;
    total: {
      hits: number;
      misses: number;
      hitRate: number;
      totalEntries: number;
    };
  }> {
    const graphStats = this.graphCache.getStats();
    const nodeOutputStats = this.nodeOutputCache.getStats();

    // Get SQLite stats
    const rustStats = await cacheService.stats();
    const workflowEntries = rustStats?.categories.find(c => c.category === WORKFLOW_CACHE_CATEGORY);

    const sqliteStats: ICacheStats = {
      hits: this.sqliteHits,
      misses: this.sqliteMisses,
      size: workflowEntries?.total_size ?? 0,
      entries: workflowEntries?.entry_count ?? 0,
      hitRate: (this.sqliteHits + this.sqliteMisses) > 0
        ? this.sqliteHits / (this.sqliteHits + this.sqliteMisses)
        : 0,
    };

    const totalHits = graphStats.hits + nodeOutputStats.hits + this.sqliteHits;
    const totalMisses = graphStats.misses + nodeOutputStats.misses + this.sqliteMisses;
    const total = totalHits + totalMisses;

    return {
      graph: graphStats,
      nodeOutput: nodeOutputStats,
      sqlite: sqliteStats,
      total: {
        hits: totalHits,
        misses: totalMisses,
        hitRate: total > 0 ? totalHits / total : 0,
        totalEntries: graphStats.entries + nodeOutputStats.entries + sqliteStats.entries,
      },
    };
  }

  /**
   * Start periodic cleanup
   */
  private startCleanup(): void {
    if (this.cleanupInterval) return;

    this.cleanupInterval = setInterval(() => {
      this.graphCache.cleanup();
      this.nodeOutputCache.cleanup();
      // SQLite cleanup is handled by Rust cache_cleanup
      cacheService.cleanup();
    }, 60 * 1000);
  }

  /**
   * Stop periodic cleanup
   */
  stopCleanup(): void {
    if (this.cleanupInterval) {
      clearInterval(this.cleanupInterval);
      this.cleanupInterval = null;
    }
  }
}

/**
 * Global workflow cache instance
 */
export const globalWorkflowCache = new WorkflowCache();

/**
 * Hash function for creating cache keys
 */
export function hashData(data: any): string {
  const str = typeof data === 'string' ? data : JSON.stringify(data);
  let hash = 0;

  for (let i = 0; i < str.length; i++) {
    const char = str.charCodeAt(i);
    hash = (hash << 5) - hash + char;
    hash = hash & hash;
  }

  return hash.toString(36);
}

/**
 * Memoize function results (in-memory only)
 */
export function memoize<T extends (...args: any[]) => any>(
  fn: T,
  keyFn?: (...args: Parameters<T>) => string
): T {
  const cache = new Map<string, ReturnType<T>>();

  return ((...args: Parameters<T>): ReturnType<T> => {
    const key = keyFn ? keyFn(...args) : JSON.stringify(args);

    if (cache.has(key)) {
      return cache.get(key)!;
    }

    const result = fn(...args);
    cache.set(key, result);

    return result;
  }) as T;
}
