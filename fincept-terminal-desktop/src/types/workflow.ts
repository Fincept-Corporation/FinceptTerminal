/**
 * Workflow Type Definitions
 * Provides proper TypeScript types for the workflow system, replacing `any` usage.
 */

import { Node, Edge } from 'reactflow';

// ============================================================================
// Node Data Types
// ============================================================================

/** Base data shared by all workflow nodes */
export interface BaseNodeData {
    label: string;
    status?: 'idle' | 'running' | 'completed' | 'error';
    result?: unknown;
    onLabelChange?: (nodeId: string, label: string) => void;
}

/** Data source node configuration */
export interface DataSourceNodeData extends BaseNodeData {
    connectionId?: string;
    dataSourceType?: string;
    query?: string;
    onConnectionChange?: (connectionId: string) => void;
    onQueryChange?: (query: string) => void;
    onExecute?: () => void;
}

/** Technical indicator node configuration */
export interface TechnicalIndicatorNodeData extends BaseNodeData {
    dataSource?: 'yfinance' | 'csv' | 'json' | 'input';
    symbol?: string;
    period?: string;
    csvPath?: string;
    jsonData?: string;
    categories?: string[];
    inputEdges?: Edge[];
    onParameterChange?: (params: Record<string, unknown>) => void;
    onExecute?: (nodeId: string) => void;
}

/** Python agent node configuration */
export interface PythonAgentNodeData extends BaseNodeData {
    agentPath?: string;
    agentName?: string;
    parameters?: Record<string, unknown>;
    llmProvider?: string;
    onParameterChange?: (params: Record<string, unknown>) => void;
    onLLMChange?: (provider: string) => void;
    onExecute?: (result: unknown) => void;
}

/** MCP tool node configuration */
export interface MCPToolNodeData extends BaseNodeData {
    serverId?: string;
    toolName?: string;
    parameters?: Record<string, unknown>;
    onParameterChange?: (params: Record<string, unknown>) => void;
    onExecute?: (nodeId: string) => void;
}

/** Results display node configuration */
export interface ResultsDisplayNodeData extends BaseNodeData {
    displayType?: 'table' | 'chart' | 'json' | 'text';
    inputData?: unknown;
}

/** Agent mediator node configuration */
export interface AgentMediatorNodeData extends BaseNodeData {
    connectedAgents?: string[];
    mediationStrategy?: 'sequential' | 'parallel' | 'consensus';
}

/** Union type for all node data types */
export type WorkflowNodeData =
    | DataSourceNodeData
    | TechnicalIndicatorNodeData
    | PythonAgentNodeData
    | MCPToolNodeData
    | ResultsDisplayNodeData
    | AgentMediatorNodeData;

// ============================================================================
// Workflow Node Types
// ============================================================================

/** All supported node types in the workflow editor */
export type WorkflowNodeType =
    | 'data-source'
    | 'technical-indicator'
    | 'python-agent'
    | 'mcp-tool'
    | 'results-display'
    | 'agent-mediator'
    | 'custom'
    | string; // Allow custom node types

/** Typed workflow node extending ReactFlow's Node */
export interface WorkflowNode extends Node<WorkflowNodeData> {
    type?: WorkflowNodeType;
}

/** Typed workflow edge (alias for clarity) */
export type WorkflowEdge = Edge;

// ============================================================================
// Workflow Execution Types
// ============================================================================

/** Result of a single node execution */
export interface NodeExecutionResult {
    success: boolean;
    data?: unknown;
    error?: string;
    duration?: number;
}

/** Results from executing all nodes in a workflow */
export type WorkflowResults = Map<string, NodeExecutionResult>;

/** Workflow execution status */
export type WorkflowStatus = 'idle' | 'running' | 'completed' | 'error' | 'draft';

/** Complete workflow definition */
export interface Workflow {
    id: string;
    name: string;
    description?: string;
    nodes: WorkflowNode[];
    edges: WorkflowEdge[];
    status: WorkflowStatus;
    results?: Record<string, NodeExecutionResult>;
    createdAt?: string;
    updatedAt?: string;
}

// ============================================================================
// Execution Callbacks
// ============================================================================

/** Callback for node status updates during execution */
export type NodeStatusCallback = (
    nodeId: string,
    status: 'idle' | 'running' | 'completed' | 'error',
    result?: NodeExecutionResult
) => void;

/** Options for workflow execution */
export interface ExecuteWorkflowOptions {
    name: string;
    description: string;
    saveAsDraft: boolean;
    onNodeStatusChange?: NodeStatusCallback;
}

/** Result of workflow execution */
export interface WorkflowExecutionResult {
    success: boolean;
    workflowId?: string;
    results?: Record<string, NodeExecutionResult>;
    error?: string;
}

// ============================================================================
// Node Configuration (for palette)
// ============================================================================

/** Configuration for a node type in the palette */
export interface NodeConfig {
    type: WorkflowNodeType;
    label: string;
    color: string;
    category: 'Input' | 'Processing' | 'Output' | 'AI';
    hasInput: boolean;
    hasOutput: boolean;
    icon?: string;
}

/** MCP Tool definition for node creation */
export interface MCPToolDefinition {
    serverId: string;
    serverName: string;
    toolName: string;
    description?: string;
    parameters?: Array<{
        name: string;
        type: string;
        description?: string;
        required?: boolean;
    }>;
}

/** Python agent definition for node creation */
export interface PythonAgentDefinition {
    path: string;
    name: string;
    description?: string;
    category?: string;
    parameters?: Array<{
        name: string;
        type: string;
        description?: string;
        default?: unknown;
    }>;
}
