/**
 * Workflow Cache
 *
 * Provides caching and memoization for workflow execution to improve performance.
 *
 * Features:
 * - Cache workflow execution results
 * - Memoize node execution outputs
 * - Cache DirectedGraph instances
 * - Cache parameter evaluations
 * - LRU eviction policy
 * - TTL (time-to-live) support
 * - Cache invalidation
 * - Memory management
 *
 * Based on caching patterns from n8n and general workflow systems.
 */

import type {
  IWorkflow,
  INode,
  IRunExecutionData,
  IRunData,
  NodeParameterValue,
  INodeExecutionData,
} from './types';
import { DirectedGraph } from './DirectedGraph';

/**
 * Cache entry with metadata
 */
interface ICacheEntry<T> {
  value: T;
  timestamp: number;
  hits: number;
  size: number; // Approximate size in bytes
}

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
 * LRU Cache with TTL support
 */
class LRUCache<T> {
  private cache: Map<string, ICacheEntry<T>>;
  private maxSize: number;
  private maxAge: number; // milliseconds
  private currentSize: number;
  private hits: number;
  private misses: number;

  constructor(maxSize: number = 100, maxAge: number = 5 * 60 * 1000) {
    this.cache = new Map();
    this.maxSize = maxSize;
    this.maxAge = maxAge;
    this.currentSize = 0;
    this.hits = 0;
    this.misses = 0;
  }

  /**
   * Get value from cache
   */
  get(key: string): T | undefined {
    const entry = this.cache.get(key);

    if (!entry) {
      this.misses++;
      return undefined;
    }

    // Check if expired
    const age = Date.now() - entry.timestamp;
    if (age > this.maxAge) {
      this.cache.delete(key);
      this.currentSize -= entry.size;
      this.misses++;
      return undefined;
    }

    // Update access metadata
    entry.hits++;
    entry.timestamp = Date.now();

    // Move to end (most recently used)
    this.cache.delete(key);
    this.cache.set(key, entry);

    this.hits++;
    return entry.value;
  }

  /**
   * Set value in cache
   */
  set(key: string, value: T, size: number = 1): void {
    // Remove existing entry if present
    const existing = this.cache.get(key);
    if (existing) {
      this.currentSize -= existing.size;
      this.cache.delete(key);
    }

    // Evict entries if necessary
    while (this.cache.size >= this.maxSize) {
      this.evictOldest();
    }

    // Add new entry
    const entry: ICacheEntry<T> = {
      value,
      timestamp: Date.now(),
      hits: 0,
      size,
    };

    this.cache.set(key, entry);
    this.currentSize += size;
  }

  /**
   * Check if key exists (without updating access)
   */
  has(key: string): boolean {
    const entry = this.cache.get(key);
    if (!entry) return false;

    // Check expiration
    const age = Date.now() - entry.timestamp;
    if (age > this.maxAge) {
      this.cache.delete(key);
      this.currentSize -= entry.size;
      return false;
    }

    return true;
  }

  /**
   * Delete entry
   */
  delete(key: string): boolean {
    const entry = this.cache.get(key);
    if (entry) {
      this.currentSize -= entry.size;
    }
    return this.cache.delete(key);
  }

  /**
   * Clear all entries
   */
  clear(): void {
    this.cache.clear();
    this.currentSize = 0;
    this.hits = 0;
    this.misses = 0;
  }

  /**
   * Evict oldest (least recently used) entry
   */
  private evictOldest(): void {
    const firstKey = this.cache.keys().next().value;
    if (firstKey) {
      const entry = this.cache.get(firstKey);
      if (entry) {
        this.currentSize -= entry.size;
      }
      this.cache.delete(firstKey);
    }
  }

  /**
   * Get cache statistics
   */
  getStats(): ICacheStats {
    const total = this.hits + this.misses;
    return {
      hits: this.hits,
      misses: this.misses,
      size: this.currentSize,
      entries: this.cache.size,
      hitRate: total > 0 ? this.hits / total : 0,
    };
  }

  /**
   * Get all keys
   */
  keys(): string[] {
    return Array.from(this.cache.keys());
  }

  /**
   * Cleanup expired entries
   */
  cleanup(): void {
    const now = Date.now();
    const keysToDelete: string[] = [];

    for (const [key, entry] of this.cache.entries()) {
      const age = now - entry.timestamp;
      if (age > this.maxAge) {
        keysToDelete.push(key);
      }
    }

    for (const key of keysToDelete) {
      const entry = this.cache.get(key);
      if (entry) {
        this.currentSize -= entry.size;
      }
      this.cache.delete(key);
    }
  }
}

/**
 * Workflow Cache Manager
 */
export class WorkflowCache {
  // Caches for different types of data
  private executionCache: LRUCache<IRunExecutionData>;
  private nodeOutputCache: LRUCache<INodeExecutionData[][]>;
  private graphCache: LRUCache<DirectedGraph>;
  private parameterCache: LRUCache<NodeParameterValue>;
  private expressionCache: LRUCache<any>;

  // Cache configuration
  private enabled: boolean = true;
  private cleanupInterval: NodeJS.Timeout | null = null;

  constructor() {
    // Initialize caches with different sizes based on typical usage
    this.executionCache = new LRUCache<IRunExecutionData>(50, 10 * 60 * 1000); // 10 min TTL
    this.nodeOutputCache = new LRUCache<INodeExecutionData[][]>(200, 5 * 60 * 1000); // 5 min TTL
    this.graphCache = new LRUCache<DirectedGraph>(100, 30 * 60 * 1000); // 30 min TTL
    this.parameterCache = new LRUCache<NodeParameterValue>(500, 5 * 60 * 1000); // 5 min TTL
    this.expressionCache = new LRUCache<any>(1000, 5 * 60 * 1000); // 5 min TTL

    // Start periodic cleanup
    this.startCleanup();
  }

  /**
   * Cache workflow execution result
   */
  cacheExecution(workflowId: string, executionId: string, result: IRunExecutionData): void {
    if (!this.enabled) return;

    const key = `${workflowId}:${executionId}`;
    const size = this.estimateSize(result);
    this.executionCache.set(key, result, size);
  }

  /**
   * Get cached workflow execution result
   */
  getCachedExecution(workflowId: string, executionId: string): IRunExecutionData | undefined {
    if (!this.enabled) return undefined;

    const key = `${workflowId}:${executionId}`;
    return this.executionCache.get(key);
  }

  /**
   * Cache node execution output
   */
  cacheNodeOutput(
    workflowId: string,
    nodeId: string,
    inputHash: string,
    output: INodeExecutionData[][]
  ): void {
    if (!this.enabled) return;

    const key = `${workflowId}:${nodeId}:${inputHash}`;
    const size = this.estimateSize(output);
    this.nodeOutputCache.set(key, output, size);
  }

  /**
   * Get cached node output
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
   * Cache DirectedGraph for workflow
   */
  cacheGraph(workflowId: string, graph: DirectedGraph): void {
    if (!this.enabled) return;

    const size = graph.getNodes().size + graph.getDirectChildren.length * 2;
    this.graphCache.set(workflowId, graph, size);
  }

  /**
   * Get cached DirectedGraph
   */
  getCachedGraph(workflowId: string): DirectedGraph | undefined {
    if (!this.enabled) return undefined;

    return this.graphCache.get(workflowId);
  }

  /**
   * Cache parameter value
   */
  cacheParameter(
    workflowId: string,
    nodeId: string,
    paramName: string,
    value: NodeParameterValue
  ): void {
    if (!this.enabled) return;

    const key = `${workflowId}:${nodeId}:${paramName}`;
    const size = this.estimateSize(value);
    this.parameterCache.set(key, value, size);
  }

  /**
   * Get cached parameter value
   */
  getCachedParameter(
    workflowId: string,
    nodeId: string,
    paramName: string
  ): NodeParameterValue | undefined {
    if (!this.enabled) return undefined;

    const key = `${workflowId}:${nodeId}:${paramName}`;
    return this.parameterCache.get(key);
  }

  /**
   * Cache expression evaluation result
   */
  cacheExpression(expression: string, contextHash: string, result: any): void {
    if (!this.enabled) return;

    const key = `${expression}:${contextHash}`;
    const size = this.estimateSize(result);
    this.expressionCache.set(key, result, size);
  }

  /**
   * Get cached expression result
   */
  getCachedExpression(expression: string, contextHash: string): any | undefined {
    if (!this.enabled) return undefined;

    const key = `${expression}:${contextHash}`;
    return this.expressionCache.get(key);
  }

  /**
   * Invalidate all caches for a workflow
   */
  invalidateWorkflow(workflowId: string): void {
    // Invalidate execution cache
    for (const key of this.executionCache.keys()) {
      if (key.startsWith(`${workflowId}:`)) {
        this.executionCache.delete(key);
      }
    }

    // Invalidate node output cache
    for (const key of this.nodeOutputCache.keys()) {
      if (key.startsWith(`${workflowId}:`)) {
        this.nodeOutputCache.delete(key);
      }
    }

    // Invalidate graph cache
    this.graphCache.delete(workflowId);

    // Invalidate parameter cache
    for (const key of this.parameterCache.keys()) {
      if (key.startsWith(`${workflowId}:`)) {
        this.parameterCache.delete(key);
      }
    }
  }

  /**
   * Invalidate cache for a specific node
   */
  invalidateNode(workflowId: string, nodeId: string): void {
    // Invalidate node output cache
    for (const key of this.nodeOutputCache.keys()) {
      if (key.startsWith(`${workflowId}:${nodeId}:`)) {
        this.nodeOutputCache.delete(key);
      }
    }

    // Invalidate parameter cache
    for (const key of this.parameterCache.keys()) {
      if (key.startsWith(`${workflowId}:${nodeId}:`)) {
        this.parameterCache.delete(key);
      }
    }
  }

  /**
   * Clear all caches
   */
  clearAll(): void {
    this.executionCache.clear();
    this.nodeOutputCache.clear();
    this.graphCache.clear();
    this.parameterCache.clear();
    this.expressionCache.clear();
  }

  /**
   * Enable/disable caching
   */
  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
    if (!enabled) {
      this.clearAll();
    }
  }

  /**
   * Check if caching is enabled
   */
  isEnabled(): boolean {
    return this.enabled;
  }

  /**
   * Get comprehensive cache statistics
   */
  getStats(): {
    execution: ICacheStats;
    nodeOutput: ICacheStats;
    graph: ICacheStats;
    parameter: ICacheStats;
    expression: ICacheStats;
    total: {
      hits: number;
      misses: number;
      hitRate: number;
      totalSize: number;
      totalEntries: number;
    };
  } {
    const executionStats = this.executionCache.getStats();
    const nodeOutputStats = this.nodeOutputCache.getStats();
    const graphStats = this.graphCache.getStats();
    const parameterStats = this.parameterCache.getStats();
    const expressionStats = this.expressionCache.getStats();

    const totalHits =
      executionStats.hits +
      nodeOutputStats.hits +
      graphStats.hits +
      parameterStats.hits +
      expressionStats.hits;

    const totalMisses =
      executionStats.misses +
      nodeOutputStats.misses +
      graphStats.misses +
      parameterStats.misses +
      expressionStats.misses;

    const total = totalHits + totalMisses;

    return {
      execution: executionStats,
      nodeOutput: nodeOutputStats,
      graph: graphStats,
      parameter: parameterStats,
      expression: expressionStats,
      total: {
        hits: totalHits,
        misses: totalMisses,
        hitRate: total > 0 ? totalHits / total : 0,
        totalSize:
          executionStats.size +
          nodeOutputStats.size +
          graphStats.size +
          parameterStats.size +
          expressionStats.size,
        totalEntries:
          executionStats.entries +
          nodeOutputStats.entries +
          graphStats.entries +
          parameterStats.entries +
          expressionStats.entries,
      },
    };
  }

  /**
   * Start periodic cleanup of expired entries
   */
  private startCleanup(): void {
    if (this.cleanupInterval) return;

    this.cleanupInterval = setInterval(() => {
      this.executionCache.cleanup();
      this.nodeOutputCache.cleanup();
      this.graphCache.cleanup();
      this.parameterCache.cleanup();
      this.expressionCache.cleanup();
    }, 60 * 1000); // Cleanup every minute
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

  /**
   * Estimate size of data for cache management
   */
  private estimateSize(data: any): number {
    if (data === null || data === undefined) return 1;

    if (typeof data === 'string') return data.length;
    if (typeof data === 'number') return 8;
    if (typeof data === 'boolean') return 4;

    if (Array.isArray(data)) {
      return data.reduce((sum: number, item) => sum + this.estimateSize(item), 10);
    }

    if (typeof data === 'object') {
      return Object.values(data).reduce((sum: number, value) => sum + this.estimateSize(value), 10);
    }

    return 1;
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
  const str = JSON.stringify(data);
  let hash = 0;

  for (let i = 0; i < str.length; i++) {
    const char = str.charCodeAt(i);
    hash = (hash << 5) - hash + char;
    hash = hash & hash; // Convert to 32-bit integer
  }

  return hash.toString(36);
}

/**
 * Memoize function results
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
