/**
 * Execution Hooks System
 * Provides lifecycle hooks and events for node execution
 *
 * Hooks allow external code to:
 * - Monitor workflow and node execution
 * - Intercept and modify execution flow
 * - Add custom logging and debugging
 * - Implement custom error handling
 * - Track performance metrics
 * - Implement audit trails
 *
 * Based on common event-driven architecture patterns.
 */

import type {
  INode,
  INodeExecutionData,
  IRunExecutionData,
  IWorkflow,
  NodeError,
} from './types';

/**
 * Hook event types
 */
export enum HookEventType {
  // Workflow-level events
  WorkflowExecuteStart = 'workflow.executeStart',
  WorkflowExecuteEnd = 'workflow.executeEnd',
  WorkflowExecuteError = 'workflow.executeError',

  // Node-level events
  NodeExecuteStart = 'node.executeStart',
  NodeExecuteEnd = 'node.executeEnd',
  NodeExecuteError = 'node.executeError',

  // Item-level events
  ItemProcessStart = 'item.processStart',
  ItemProcessEnd = 'item.processEnd',
  ItemProcessError = 'item.processError',

  // Data-level events
  DataReceived = 'data.received',
  DataSent = 'data.sent',

  // Performance events
  PerformanceMetric = 'performance.metric',

  // Custom events
  Custom = 'custom',
}

/**
 * Hook event data
 */
export interface IHookEvent {
  type: HookEventType;
  timestamp: Date;
  executionId: string;
  workflowId?: string;
  nodeName?: string;
  data?: any;
  metadata?: Record<string, any>;
}

/**
 * Workflow execution start event
 */
export interface IWorkflowExecuteStartEvent extends IHookEvent {
  type: HookEventType.WorkflowExecuteStart;
  workflow: IWorkflow;
  startNode?: string;
}

/**
 * Workflow execution end event
 */
export interface IWorkflowExecuteEndEvent extends IHookEvent {
  type: HookEventType.WorkflowExecuteEnd;
  workflow: IWorkflow;
  executionData: IRunExecutionData;
  success: boolean;
  duration: number; // milliseconds
}

/**
 * Workflow execution error event
 */
export interface IWorkflowExecuteErrorEvent extends IHookEvent {
  type: HookEventType.WorkflowExecuteError;
  workflow: IWorkflow;
  error: Error | NodeError;
  nodeName?: string;
}

/**
 * Node execution start event
 */
export interface INodeExecuteStartEvent extends IHookEvent {
  type: HookEventType.NodeExecuteStart;
  node: INode;
  inputData: INodeExecutionData[];
}

/**
 * Node execution end event
 */
export interface INodeExecuteEndEvent extends IHookEvent {
  type: HookEventType.NodeExecuteEnd;
  node: INode;
  outputData: INodeExecutionData[][];
  duration: number; // milliseconds
}

/**
 * Node execution error event
 */
export interface INodeExecuteErrorEvent extends IHookEvent {
  type: HookEventType.NodeExecuteError;
  node: INode;
  error: Error | NodeError;
  continueOnFail: boolean;
}

/**
 * Performance metric event
 */
export interface IPerformanceMetricEvent extends IHookEvent {
  type: HookEventType.PerformanceMetric;
  metric: {
    name: string;
    value: number;
    unit: string;
    tags?: Record<string, string>;
  };
}

/**
 * Hook function signature
 */
export type HookFunction = (event: IHookEvent) => void | Promise<void>;

/**
 * Hook options
 */
export interface IHookOptions {
  once?: boolean; // Execute only once
  priority?: number; // Higher priority = earlier execution
  filter?: {
    nodes?: string[]; // Only for specific nodes
    workflows?: string[]; // Only for specific workflows
    eventTypes?: HookEventType[]; // Only for specific event types
  };
}

/**
 * Hook registration
 */
interface IHookRegistration {
  id: string;
  eventType: HookEventType | '*'; // '*' = all events
  handler: HookFunction;
  options: IHookOptions;
  executionCount: number;
}

/**
 * Execution Hooks Manager
 * Central registry for all execution hooks
 */
export class ExecutionHooksManager {
  private hooks: Map<string, IHookRegistration> = new Map();
  private nextHookId = 1;
  private enabled = true;

  /**
   * Register a hook
   */
  on(
    eventType: HookEventType | '*',
    handler: HookFunction,
    options: IHookOptions = {}
  ): string {
    const hookId = `hook_${this.nextHookId++}`;

    this.hooks.set(hookId, {
      id: hookId,
      eventType,
      handler,
      options,
      executionCount: 0,
    });

    return hookId;
  }

  /**
   * Register a one-time hook
   */
  once(eventType: HookEventType | '*', handler: HookFunction, options: IHookOptions = {}): string {
    return this.on(eventType, handler, { ...options, once: true });
  }

  /**
   * Remove a hook
   */
  off(hookId: string): boolean {
    return this.hooks.delete(hookId);
  }

  /**
   * Remove all hooks for an event type
   */
  offAll(eventType?: HookEventType | '*'): void {
    if (eventType) {
      for (const [id, hook] of this.hooks.entries()) {
        if (hook.eventType === eventType) {
          this.hooks.delete(id);
        }
      }
    } else {
      this.hooks.clear();
    }
  }

  /**
   * Emit an event to all registered hooks
   */
  async emit(event: IHookEvent): Promise<void> {
    if (!this.enabled) {
      return;
    }

    // Get matching hooks
    const matchingHooks = this.getMatchingHooks(event);

    // Sort by priority (higher first)
    matchingHooks.sort((a, b) => (b.options.priority || 0) - (a.options.priority || 0));

    // Execute hooks
    for (const hook of matchingHooks) {
      try {
        await hook.handler(event);
        hook.executionCount++;

        // Remove if once
        if (hook.options.once) {
          this.hooks.delete(hook.id);
        }
      } catch (error) {
        console.error(`[ExecutionHooks] Hook ${hook.id} failed:`, error);
        // Don't throw - hooks shouldn't break execution
      }
    }
  }

  /**
   * Emit event synchronously (non-blocking)
   */
  emitSync(event: IHookEvent): void {
    // Fire and forget
    this.emit(event).catch((error) => {
      console.error('[ExecutionHooks] Async emit failed:', error);
    });
  }

  /**
   * Get matching hooks for an event
   */
  private getMatchingHooks(event: IHookEvent): IHookRegistration[] {
    const matching: IHookRegistration[] = [];

    for (const hook of this.hooks.values()) {
      // Check event type
      if (hook.eventType !== '*' && hook.eventType !== event.type) {
        continue;
      }

      // Check filters
      if (hook.options.filter) {
        const { nodes, workflows, eventTypes } = hook.options.filter;

        if (eventTypes && !eventTypes.includes(event.type)) {
          continue;
        }

        if (nodes && event.nodeName && !nodes.includes(event.nodeName)) {
          continue;
        }

        if (workflows && event.workflowId && !workflows.includes(event.workflowId)) {
          continue;
        }
      }

      matching.push(hook);
    }

    return matching;
  }

  /**
   * Get all registered hooks
   */
  getHooks(): IHookRegistration[] {
    return Array.from(this.hooks.values());
  }

  /**
   * Get hook statistics
   */
  getStats(): {
    totalHooks: number;
    hooksByType: Record<string, number>;
    totalExecutions: number;
  } {
    const hooksByType: Record<string, number> = {};
    let totalExecutions = 0;

    for (const hook of this.hooks.values()) {
      const type = hook.eventType;
      hooksByType[type] = (hooksByType[type] || 0) + 1;
      totalExecutions += hook.executionCount;
    }

    return {
      totalHooks: this.hooks.size,
      hooksByType,
      totalExecutions,
    };
  }

  /**
   * Enable/disable hooks
   */
  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
  }

  /**
   * Check if hooks are enabled
   */
  isEnabled(): boolean {
    return this.enabled;
  }

  /**
   * Clear all hooks
   */
  clear(): void {
    this.hooks.clear();
  }
}

/**
 * Global hooks manager instance
 */
export const globalHooks = new ExecutionHooksManager();

/**
 * Convenience functions for common hooks
 */
export const hooks = {
  /**
   * Register workflow start hook
   */
  onWorkflowStart(
    handler: (event: IWorkflowExecuteStartEvent) => void | Promise<void>,
    options?: IHookOptions
  ): string {
    return globalHooks.on(HookEventType.WorkflowExecuteStart, handler as HookFunction, options);
  },

  /**
   * Register workflow end hook
   */
  onWorkflowEnd(
    handler: (event: IWorkflowExecuteEndEvent) => void | Promise<void>,
    options?: IHookOptions
  ): string {
    return globalHooks.on(HookEventType.WorkflowExecuteEnd, handler as HookFunction, options);
  },

  /**
   * Register workflow error hook
   */
  onWorkflowError(
    handler: (event: IWorkflowExecuteErrorEvent) => void | Promise<void>,
    options?: IHookOptions
  ): string {
    return globalHooks.on(HookEventType.WorkflowExecuteError, handler as HookFunction, options);
  },

  /**
   * Register node start hook
   */
  onNodeStart(
    handler: (event: INodeExecuteStartEvent) => void | Promise<void>,
    options?: IHookOptions
  ): string {
    return globalHooks.on(HookEventType.NodeExecuteStart, handler as HookFunction, options);
  },

  /**
   * Register node end hook
   */
  onNodeEnd(
    handler: (event: INodeExecuteEndEvent) => void | Promise<void>,
    options?: IHookOptions
  ): string {
    return globalHooks.on(HookEventType.NodeExecuteEnd, handler as HookFunction, options);
  },

  /**
   * Register node error hook
   */
  onNodeError(
    handler: (event: INodeExecuteErrorEvent) => void | Promise<void>,
    options?: IHookOptions
  ): string {
    return globalHooks.on(HookEventType.NodeExecuteError, handler as HookFunction, options);
  },

  /**
   * Register performance metric hook
   */
  onPerformanceMetric(
    handler: (event: IPerformanceMetricEvent) => void | Promise<void>,
    options?: IHookOptions
  ): string {
    return globalHooks.on(HookEventType.PerformanceMetric, handler as HookFunction, options);
  },

  /**
   * Register hook for all events
   */
  onAll(handler: HookFunction, options?: IHookOptions): string {
    return globalHooks.on('*', handler, options);
  },

  /**
   * Remove a hook
   */
  off(hookId: string): boolean {
    return globalHooks.off(hookId);
  },

  /**
   * Remove all hooks
   */
  clear(): void {
    globalHooks.clear();
  },
};

/**
 * Performance tracker using hooks
 */
export class PerformanceTracker {
  private startTimes: Map<string, number> = new Map();

  /**
   * Start tracking
   */
  start(key: string): void {
    this.startTimes.set(key, Date.now());
  }

  /**
   * End tracking and emit metric
   */
  end(key: string, executionId: string, metadata?: Record<string, any>): void {
    const startTime = this.startTimes.get(key);
    if (!startTime) {
      return;
    }

    const duration = Date.now() - startTime;
    this.startTimes.delete(key);

    globalHooks.emitSync({
      type: HookEventType.PerformanceMetric,
      timestamp: new Date(),
      executionId,
      metadata,
      data: {
        metric: {
          name: key,
          value: duration,
          unit: 'ms',
          tags: metadata,
        },
      },
    } as IPerformanceMetricEvent);
  }

  /**
   * Measure execution time of a function
   */
  async measure<T>(
    key: string,
    executionId: string,
    fn: () => Promise<T>,
    metadata?: Record<string, any>
  ): Promise<T> {
    this.start(key);
    try {
      return await fn();
    } finally {
      this.end(key, executionId, metadata);
    }
  }
}

/**
 * Create a performance tracker instance
 */
export function createPerformanceTracker(): PerformanceTracker {
  return new PerformanceTracker();
}
