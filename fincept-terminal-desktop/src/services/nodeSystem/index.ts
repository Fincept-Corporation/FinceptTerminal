/**
 * Node System - Main Entry Point
 *
 * This is the n8n-inspired node execution system for Fincept Terminal.
 * Import from this file to use the node system in your components.
 */

// Core Types
export * from './types';

// Execution Context
export { ExecutionContext, BaseExecuteContext, LoadOptionsContext } from './ExecutionContext';

// Node Registry
export { NodeRegistry } from './NodeRegistry';

// Workflow Data Proxy
export { WorkflowDataProxy } from './WorkflowDataProxy';

// Node Loader
export { NodeLoader, initializeNodeSystem } from './NodeLoader';

// Workflow Executor
export { WorkflowExecutor } from './WorkflowExecutor';

// Expression Engine
export { ExpressionEngine, isExpression } from './ExpressionEngine';
export type { IExpressionContext } from './ExpressionEngine';

// Parameter Processor (value extraction, routing, validation)
export {
  extractParameterValue,
  applyParameterRouting,
  convertParameterValue,
  validateParameterValue,
  processNodeParameters,
  buildRequestFromRouting,
} from './ParameterProcessor';

// Execution Hooks (lifecycle events and monitoring)
export {
  ExecutionHooksManager,
  globalHooks,
  hooks,
  PerformanceTracker,
  createPerformanceTracker,
  HookEventType,
} from './ExecutionHooks';
export type {
  IHookEvent,
  IWorkflowExecuteStartEvent,
  IWorkflowExecuteEndEvent,
  IWorkflowExecuteErrorEvent,
  INodeExecuteStartEvent,
  INodeExecuteEndEvent,
  INodeExecuteErrorEvent,
  IPerformanceMetricEvent,
  HookFunction,
  IHookOptions,
} from './ExecutionHooks';

// DirectedGraph (for workflow optimization)
export { DirectedGraph } from './DirectedGraph';
export type { GraphConnection } from './DirectedGraph';

// Partial Execution Utilities
export {
  isDirty,
  findStartNodes,
  findSubgraph,
  cleanRunData,
  handleCycles,
  filterDisabledNodes,
  recreateNodeExecutionStack,
  findTriggerForPartialExecution,
  calculateExecutionOrder,
  getIncomingData,
  hasIncomingDataFromAnyRun,
} from './PartialExecutionUtils';

// Workflow Cache (caching and memoization)
export { WorkflowCache, globalWorkflowCache, hashData, memoize } from './WorkflowCache';

// Agent Node Generator (dynamic Python agent node creation)
export {
  generateAgentNode,
  generateAllAgentNodes,
  generateAgentNodesByCategory,
  getAgentNodeStatistics,
} from './utils/AgentNodeGenerator';

// Re-export commonly used types with simpler names
export type {
  INodeType as NodeType,
  INode as Node,
  IWorkflow as Workflow,
  INodeExecutionData as NodeExecutionData,
  IExecuteFunctions as ExecuteFunctions,
  INodeTypeDescription as NodeTypeDescription,
  INodeProperties as NodeProperties,
} from './types';
