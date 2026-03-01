/**
 * Agent Service — Types & Interfaces
 *
 * Extracted from agentService.ts.
 */

// =============================================================================
// Core Agent Types
// =============================================================================

export interface AgentConfig {
  model: {
    provider: string;
    model_id: string;
    temperature?: number;
    max_tokens?: number;
  };
  instructions?: string;
  tools?: string[];
  name?: string;
  // Memory: basic toggle or detailed config
  memory?: boolean | {
    enabled: boolean;
    db_path?: string;
    table_name?: string;
    create_user_memories?: boolean;
    create_session_summary?: boolean;
  };
  // Knowledge Base (RAG)
  knowledge?: {
    enabled: boolean;
    type?: 'url' | 'pdf' | 'text' | 'combined';
    urls?: string[];
    vector_db?: string;
    embedder?: string;
  };
  // Reasoning: basic toggle or detailed config
  reasoning?: boolean | {
    enabled: boolean;
    strategy?: string;
    min_steps?: number;
    max_steps?: number;
    model?: { provider: string; model_id: string };
  };
  // Guardrails
  guardrails?: boolean | {
    enabled: boolean;
    pii_detection?: boolean;
    injection_check?: boolean;
    financial_compliance?: boolean;
  };
  // Agentic Memory (separate from session memory)
  agentic_memory?: boolean | {
    enabled: boolean;
    user_id?: string;
  };
  // Storage/Session persistence
  storage?: {
    enabled: boolean;
    type?: 'sqlite' | 'postgres';
    db_path?: string;
    table_name?: string;
  };
  // Tracing for debugging
  tracing?: boolean;
  // Compression for token optimization
  compression?: boolean;
  // Hooks for pre/post processing
  hooks?: boolean;
  // Evaluation
  evaluation?: boolean;
  // Structured output model name
  output_model?: string;
  debug?: boolean;
  // User-configured MCP servers (from the MCP tab in Settings)
  // Each entry matches the MCPServer schema from the mcp_servers SQLite table
  mcp_servers?: Array<{
    id?: string;
    name?: string;
    command?: string;
    args?: string[] | string;
    env?: Record<string, string>;
    transport?: 'stdio' | 'sse' | 'streamable-http';
    url?: string;
  }>;
  // Internal TypeScript MCP tool definitions — injected automatically by agentService
  // Python TerminalToolkit uses these to register callable tools that proxy back via HTTP bridge
  terminal_tools?: Array<{
    name: string;
    description: string;
    inputSchema: Record<string, any>;
  }>;
}

export interface TeamConfig {
  name: string;
  mode: 'coordinate' | 'route' | 'collaborate';
  model?: AgentConfig['model'];  // Coordinator model for team orchestration
  members: Array<{
    name: string;
    role?: string;
    model: AgentConfig['model'];
    tools?: string[];
    instructions?: string;
  }>;
  show_members_responses?: boolean;
  leader_index?: number; // Index of the leader agent in members list
}

export interface AgentPayload {
  action: string;
  api_keys?: Record<string, string>;
  user_id?: string;
  config?: AgentConfig;
  params?: Record<string, any>;
}

export interface AgentResponse<T = unknown> {
  success: boolean;
  error?: string;
  response?: string;
  data?: T;
  result?: string | T;
}

export interface SystemInfo {
  success: boolean;
  version: string;
  framework: string;
  capabilities: {
    model_providers: number;
    tools: number;
    tool_categories: string[];
    vectordbs: string[];
    embedders: string[];
    output_models: string[];
  };
  features: string[];
}

export interface ToolsInfo {
  success: boolean;
  tools: Record<string, string[]>;
  categories: string[];
  total_count: number;
}

export interface ModelsInfo {
  success: boolean;
  providers: string[];
  count: number;
}

// Parallel execution types
export interface ParallelTask {
  action: string;
  config?: AgentConfig;
  params?: Record<string, any>;
}

export interface ParallelResult {
  task_id: string;
  success: boolean;
  result?: any;
  error?: string;
  execution_time_ms: number;
}

export interface BatchResponse {
  batch_id: string;
  results: ParallelResult[];
  total_time_ms: number;
  successful_count: number;
  failed_count: number;
}

// Priority levels
export enum AgentPriority {
  HIGH = 0,
  NORMAL = 1,
  LOW = 2,
}

// =============================================================================
// SuperAgent & Routing (v2.1)
// =============================================================================

export interface RoutingResult {
  success: boolean;
  intent: string;
  agent_id: string;
  confidence: number;
  matched_keywords: string[];
  matched_patterns: string[];
  config: Record<string, any>;
}

export interface AgentCard {
  id: string;
  name: string;
  description: string;
  category: string;
  version: string;
  provider: string;
  capabilities: string[];
  config: Record<string, any>;
}

// =============================================================================
// Execution Planner (v2.1)
// =============================================================================

export interface PlanStep {
  id: string;
  name: string;
  step_type: string;
  config: Record<string, any>;
  dependencies: string[];
  status: string;
  result?: any;
  error?: string;
}

export interface ExecutionPlan {
  id: string;
  name: string;
  description: string;
  steps: PlanStep[];
  context: Record<string, any>;
  status: string;
  is_complete: boolean;
  has_failed: boolean;
}

// =============================================================================
// Trade Decision Repository (v2.1)
// =============================================================================

export interface TradeDecision {
  id: string;
  competition_id: string;
  model_name: string;
  cycle_number: number;
  symbol: string;
  action: string;
  quantity: number;
  price?: number;
  reasoning: string;
  confidence: number;
}
