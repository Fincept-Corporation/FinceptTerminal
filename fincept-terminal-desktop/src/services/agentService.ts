/**
 * Agent Service - High-Performance Centralized API for CoreAgent operations
 *
 * Features:
 * - Single entry point for all agent operations
 * - LRU caching with smart invalidation for static & response data
 * - Real-time streaming via Tauri events
 * - Parallel execution for batch operations
 * - Priority queue support for urgent queries
 * - Type-safe payload construction
 * - Error handling and retry logic
 */

import { invoke } from '@tauri-apps/api/core';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { mcpToolService } from '@/services/mcp/mcpToolService';
import { INTERNAL_SERVER_ID } from '@/services/mcp/internal';

// =============================================================================
// Types
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

export interface AgentResponse<T = any> {
  success: boolean;
  error?: string;
  response?: string;
  data?: T;
  result?: any;
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

// Streaming types
export interface StreamChunk {
  stream_id: string;
  chunk_type: 'token' | 'thinking' | 'tool_call' | 'tool_result' | 'done' | 'error' | 'agent_token' | 'agent_done';
  content: string;
  metadata?: Record<string, any>;
  sequence: number;
}

export interface StreamHandle {
  streamId: string;
  eventName: string;
  unlisten: UnlistenFn | null;
  cancel: () => Promise<void>;
}

export type StreamCallback = (chunk: StreamChunk) => void;

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
// LRU Cache with Smart Invalidation
// =============================================================================

interface LRUEntry<T> {
  data: T;
  timestamp: number;
  ttl: number;
  accessCount: number;
  lastAccess: number;
}

class LRUCache {
  private cache = new Map<string, LRUEntry<any>>();
  private maxSize: number;
  private responseCache = new Map<string, LRUEntry<any>>(); // Separate cache for query responses

  constructor(maxSize = 100) {
    this.maxSize = maxSize;
  }

  set<T>(key: string, data: T, ttlMs: number = 5 * 60 * 1000): void {
    // Evict if at capacity
    if (this.cache.size >= this.maxSize) {
      this.evictLRU();
    }

    this.cache.set(key, {
      data,
      timestamp: Date.now(),
      ttl: ttlMs,
      accessCount: 1,
      lastAccess: Date.now(),
    });
  }

  get<T>(key: string): T | null {
    const entry = this.cache.get(key);
    if (!entry) return null;

    // Check TTL
    if (Date.now() - entry.timestamp > entry.ttl) {
      this.cache.delete(key);
      return null;
    }

    // Update access stats
    entry.accessCount++;
    entry.lastAccess = Date.now();

    return entry.data as T;
  }

  // Cache query responses with content-based key
  setResponse<T>(query: string, config: AgentConfig, data: T, ttlMs: number = 2 * 60 * 1000): void {
    const key = this.generateResponseKey(query, config);

    if (this.responseCache.size >= this.maxSize) {
      this.evictLRUResponse();
    }

    this.responseCache.set(key, {
      data,
      timestamp: Date.now(),
      ttl: ttlMs,
      accessCount: 1,
      lastAccess: Date.now(),
    });
  }

  getResponse<T>(query: string, config: AgentConfig): T | null {
    const key = this.generateResponseKey(query, config);
    const entry = this.responseCache.get(key);

    if (!entry) return null;

    if (Date.now() - entry.timestamp > entry.ttl) {
      this.responseCache.delete(key);
      return null;
    }

    entry.accessCount++;
    entry.lastAccess = Date.now();

    return entry.data as T;
  }

  private generateResponseKey(query: string, config: AgentConfig): string {
    // Create deterministic key from query and relevant config
    const configKey = `${config.model.provider}:${config.model.model_id}:${config.tools?.join(',') || ''}`;
    return `${query.slice(0, 100)}|${configKey}`;
  }

  private evictLRU(): void {
    let oldestKey: string | null = null;
    let oldestAccess = Infinity;

    for (const [key, entry] of this.cache) {
      if (entry.lastAccess < oldestAccess) {
        oldestAccess = entry.lastAccess;
        oldestKey = key;
      }
    }

    if (oldestKey) {
      this.cache.delete(oldestKey);
    }
  }

  private evictLRUResponse(): void {
    let oldestKey: string | null = null;
    let oldestAccess = Infinity;

    for (const [key, entry] of this.responseCache) {
      if (entry.lastAccess < oldestAccess) {
        oldestAccess = entry.lastAccess;
        oldestKey = key;
      }
    }

    if (oldestKey) {
      this.responseCache.delete(oldestKey);
    }
  }

  clear(): void {
    this.cache.clear();
    this.responseCache.clear();
  }

  invalidate(key: string): void {
    this.cache.delete(key);
  }

  invalidateResponses(): void {
    this.responseCache.clear();
  }

  getStats(): { cacheSize: number; responseCacheSize: number; maxSize: number } {
    return {
      cacheSize: this.cache.size,
      responseCacheSize: this.responseCache.size,
      maxSize: this.maxSize,
    };
  }
}

const cache = new LRUCache(100);

// =============================================================================
// Core Execution
// =============================================================================

// Tool names blocked from agent access (execution/write operations)
const BLOCKED_TERMINAL_TOOLS = new Set([
  'place_order', 'cancel_order', 'modify_order', 'place_market_order',
  'place_limit_order', 'place_stop_order', 'execute_trade', 'submit_order',
  'place_crypto_order', 'cancel_crypto_order',
]);

/**
 * Fetch internal TypeScript MCP tool definitions, filter to safe read-only tools,
 * and return them ready for injection into an agent payload.
 */
async function getTerminalToolDefinitions(): Promise<AgentConfig['terminal_tools']> {
  try {
    const allTools = await mcpToolService.getAllTools();
    return allTools
      .filter(t => t.serverId === INTERNAL_SERVER_ID && !BLOCKED_TERMINAL_TOOLS.has(t.name))
      .map(t => ({
        name: t.name,
        description: t.description || `Terminal tool: ${t.name}`,
        inputSchema: t.inputSchema || { type: 'object', properties: {} },
      }));
  } catch {
    return [];
  }
}

/**
 * Execute a CoreAgent action with the given payload.
 * Automatically injects internal TypeScript MCP tool definitions if the action is 'run'.
 */
async function execute<T = any>(payload: AgentPayload): Promise<AgentResponse<T>> {
  try {
    // Inject terminal tool schemas for 'run' actions so Python can use TypeScript tools.
    // Skip for Fincept provider — its endpoint has a ~50KB payload limit and terminal tools
    // are not needed there (the Portfolio Analyst agent uses its own built-in MCP tools).
    if (payload.action === 'run' && payload.config) {
      const provider = payload.config.model?.provider?.toLowerCase() ?? '';
      const isFincept = provider === 'fincept' || provider === 'fincept-llm';
      if (!isFincept) {
        const terminalTools = await getTerminalToolDefinitions();
        if (terminalTools && terminalTools.length > 0) {
          payload = {
            ...payload,
            config: { ...payload.config, terminal_tools: terminalTools },
          };
        }
      }
    }
    const result = await invoke<string>('execute_core_agent', { payload });
    return JSON.parse(result) as AgentResponse<T>;
  } catch (error: any) {
    return {
      success: false,
      error: error?.message || String(error),
    };
  }
}

/**
 * Execute with priority (0=HIGH, 1=NORMAL, 2=LOW)
 */
async function executeWithPriority<T = any>(
  payload: AgentPayload,
  priority: AgentPriority = AgentPriority.NORMAL
): Promise<AgentResponse<T>> {
  try {
    const result = await invoke<string>('execute_agent_priority', { payload, priority });
    return JSON.parse(result) as AgentResponse<T>;
  } catch (error: any) {
    return {
      success: false,
      error: error?.message || String(error),
    };
  }
}

// =============================================================================
// Streaming Operations
// =============================================================================

/**
 * Run agent with real-time streaming
 * Returns a handle to control the stream
 */
export async function runAgentStreaming(
  query: string,
  config: AgentConfig,
  apiKeys: Record<string, string>,
  onChunk: StreamCallback,
  sessionId?: string
): Promise<StreamHandle> {
  const payload = {
    action: 'run',
    api_keys: apiKeys,
    config,
    params: { query, session_id: sessionId },
  };

  // Start streaming
  const result = await invoke<string>('execute_agent_streaming', { payload });
  const parsed = JSON.parse(result);

  if (!parsed.success) {
    throw new Error(parsed.error || 'Failed to start stream');
  }

  const streamId = parsed.stream_id;
  const eventName = parsed.event_name;

  // Subscribe to stream events
  const unlisten = await listen<StreamChunk>(eventName, (event) => {
    onChunk(event.payload);
  });

  return {
    streamId,
    eventName,
    unlisten,
    cancel: async () => {
      unlisten();
      await invoke('cancel_agent_stream', { streamId });
    },
  };
}

/**
 * Run team with streaming updates per agent
 */
export async function runTeamStreaming(
  query: string,
  teamConfig: TeamConfig,
  apiKeys: Record<string, string>,
  onChunk: StreamCallback,
  sessionId?: string
): Promise<StreamHandle> {
  const payload = {
    action: 'run_team',
    api_keys: apiKeys,
    params: { query, team_config: teamConfig, session_id: sessionId },
  };

  const result = await invoke<string>('execute_agent_streaming', { payload });
  const parsed = JSON.parse(result);

  if (!parsed.success) {
    throw new Error(parsed.error || 'Failed to start team stream');
  }

  const streamId = parsed.stream_id;
  const eventName = parsed.event_name;

  const unlisten = await listen<StreamChunk>(eventName, (event) => {
    onChunk(event.payload);
  });

  return {
    streamId,
    eventName,
    unlisten,
    cancel: async () => {
      unlisten();
      await invoke('cancel_agent_stream', { streamId });
    },
  };
}

/**
 * Get all active streams
 */
export async function getActiveStreams(): Promise<string[]> {
  const result = await invoke<string>('get_active_agent_streams');
  const parsed = JSON.parse(result);
  return parsed.active_streams || [];
}

// =============================================================================
// Parallel Execution
// =============================================================================

/**
 * Execute multiple agent tasks in parallel
 * Useful for batch analysis, multi-symbol research, etc.
 */
export async function runAgentsParallel(
  tasks: ParallelTask[],
  apiKeys: Record<string, string>,
  maxConcurrent: number = 4
): Promise<BatchResponse> {
  const payloads = tasks.map(task => ({
    action: task.action,
    api_keys: apiKeys,
    config: task.config,
    params: task.params,
  }));

  const result = await invoke<string>('execute_agents_parallel', {
    tasks: payloads,
    maxConcurrent,
  });

  return JSON.parse(result) as BatchResponse;
}

/**
 * Run stock analysis on multiple symbols in parallel
 */
export async function runBatchStockAnalysis(
  symbols: string[],
  apiKeys: Record<string, string>,
  config?: Partial<AgentConfig>,
  maxConcurrent: number = 4
): Promise<BatchResponse> {
  const tasks: ParallelTask[] = symbols.map(symbol => ({
    action: 'stock_analysis',
    config: config as AgentConfig,
    params: { symbol },
  }));

  return runAgentsParallel(tasks, apiKeys, maxConcurrent);
}

// =============================================================================
// Priority Execution
// =============================================================================

/**
 * Run agent with high priority (jumps the queue)
 */
export async function runAgentHighPriority(
  query: string,
  config: AgentConfig,
  apiKeys: Record<string, string>,
  sessionId?: string
): Promise<AgentResponse> {
  return executeWithPriority(
    {
      action: 'run',
      api_keys: apiKeys,
      config,
      params: { query, session_id: sessionId },
    },
    AgentPriority.HIGH
  );
}

/**
 * Run agent with low priority (background task)
 */
export async function runAgentLowPriority(
  query: string,
  config: AgentConfig,
  apiKeys: Record<string, string>,
  sessionId?: string
): Promise<AgentResponse> {
  return executeWithPriority(
    {
      action: 'run',
      api_keys: apiKeys,
      config,
      params: { query, session_id: sessionId },
    },
    AgentPriority.LOW
  );
}

// =============================================================================
// Cached Agent Operations
// =============================================================================

/**
 * Run agent with response caching
 * Identical queries with same config return cached results
 */
export async function runAgentCached(
  query: string,
  config: AgentConfig,
  apiKeys: Record<string, string>,
  sessionId?: string,
  cacheTtlMs: number = 2 * 60 * 1000
): Promise<AgentResponse> {
  // Check cache first
  const cached = cache.getResponse<AgentResponse>(query, config);
  if (cached) {
    return { ...cached, _cached: true } as AgentResponse & { _cached: boolean };
  }

  // Execute and cache
  const response = await execute({
    action: 'run',
    api_keys: apiKeys,
    config,
    params: { query, session_id: sessionId },
  });

  if (response.success) {
    cache.setResponse(query, config, response, cacheTtlMs);
  }

  return response;
}

// =============================================================================
// Standard Agent Operations
// =============================================================================

/**
 * Run a single agent query
 */
export async function runAgent(
  query: string,
  config: AgentConfig,
  apiKeys: Record<string, string>,
  sessionId?: string
): Promise<AgentResponse> {
  return execute({
    action: 'run',
    api_keys: apiKeys,
    config,
    params: { query, session_id: sessionId },
  });
}

/**
 * Run agent with structured output (Pydantic model)
 */
export async function runAgentStructured(
  query: string,
  config: AgentConfig,
  outputModel: string,
  apiKeys: Record<string, string>,
  sessionId?: string
): Promise<AgentResponse> {
  return execute({
    action: 'run_structured',
    api_keys: apiKeys,
    config,
    params: { query, output_model: outputModel, session_id: sessionId },
  });
}

/**
 * Run a multi-agent team
 */
export async function runTeam(
  query: string,
  teamConfig: TeamConfig,
  apiKeys: Record<string, string>,
  sessionId?: string
): Promise<AgentResponse> {
  return execute({
    action: 'run_team',
    api_keys: apiKeys,
    params: { query, team_config: teamConfig, session_id: sessionId },
  });
}

// =============================================================================
// Financial Workflows
// =============================================================================

/**
 * Run stock analysis workflow
 */
export async function runStockAnalysis(
  symbol: string,
  apiKeys: Record<string, string>,
  config?: Partial<AgentConfig>
): Promise<AgentResponse> {
  return execute({
    action: 'stock_analysis',
    api_keys: apiKeys,
    config: config as AgentConfig,
    params: { symbol },
  });
}

/**
 * Run portfolio rebalancing workflow
 */
export async function runPortfolioRebalancing(
  portfolioData: Record<string, any>,
  apiKeys: Record<string, string>,
  config?: Partial<AgentConfig>
): Promise<AgentResponse> {
  return execute({
    action: 'portfolio_rebal',
    api_keys: apiKeys,
    config: config as AgentConfig,
    params: { portfolio_data: portfolioData },
  });
}

/**
 * Run risk assessment workflow
 */
export async function runRiskAssessment(
  portfolioData: Record<string, any>,
  apiKeys: Record<string, string>,
  config?: Partial<AgentConfig>
): Promise<AgentResponse> {
  return execute({
    action: 'risk_assessment',
    api_keys: apiKeys,
    config: config as AgentConfig,
    params: { portfolio_data: portfolioData },
  });
}

// =============================================================================
// Memory & Knowledge
// =============================================================================

/**
 * Store a memory
 */
export async function storeMemory(
  content: string,
  memoryType: string = 'fact',
  metadata?: Record<string, any>
): Promise<AgentResponse<{ memory_id: number }>> {
  return execute({
    action: 'store_memory',
    params: { content, type: memoryType, metadata },
  });
}

/**
 * Recall memories
 */
export async function recallMemories(
  query?: string,
  memoryType?: string,
  limit: number = 5
): Promise<AgentResponse<{ memories: any[]; count: number }>> {
  return execute({
    action: 'recall_memories',
    params: { query, type: memoryType, limit },
  });
}

/**
 * Search knowledge base
 */
export async function searchKnowledge(
  query: string,
  limit: number = 5
): Promise<AgentResponse<{ results: any[]; count: number }>> {
  return execute({
    action: 'search_knowledge',
    params: { query, limit },
  });
}

// =============================================================================
// Guardrails & Evaluation
// =============================================================================

/**
 * Check input against guardrails
 */
export async function checkGuardrails(
  text: string,
  context?: Record<string, any>
): Promise<AgentResponse<{ passed: boolean; violations: string[] }>> {
  return execute({
    action: 'check_guardrails',
    params: { text, context, check_type: 'input' },
  });
}

/**
 * Evaluate agent response
 */
export async function evaluateResponse(
  query: string,
  response: string,
  context?: Record<string, any>
): Promise<AgentResponse<{ passed: boolean; score: number; details: any }>> {
  return execute({
    action: 'evaluate',
    params: { query, response, context, type: 'response' },
  });
}

// =============================================================================
// System Information (Cached)
// =============================================================================

/**
 * Get system info (cached for 5 minutes)
 */
export async function getSystemInfo(forceRefresh = false): Promise<SystemInfo | null> {
  const cacheKey = 'system_info';

  if (!forceRefresh) {
    const cached = cache.get<SystemInfo>(cacheKey);
    if (cached) return cached;
  }

  const response = await execute<SystemInfo>({ action: 'system_info' });

  if (response.success) {
    const info = response as unknown as SystemInfo;
    cache.set(cacheKey, info, 5 * 60 * 1000); // 5 min TTL
    return info;
  }

  return null;
}

/**
 * Get available tools (cached for 10 minutes)
 */
export async function getTools(forceRefresh = false): Promise<ToolsInfo | null> {
  const cacheKey = 'tools_info';

  if (!forceRefresh) {
    const cached = cache.get<ToolsInfo>(cacheKey);
    if (cached) return cached;
  }

  const response = await execute<ToolsInfo>({ action: 'list_tools' });

  if (response.success) {
    const info = response as unknown as ToolsInfo;
    cache.set(cacheKey, info, 10 * 60 * 1000); // 10 min TTL
    return info;
  }

  return null;
}

/**
 * Get available models (cached for 10 minutes)
 */
export async function getModels(forceRefresh = false): Promise<ModelsInfo | null> {
  const cacheKey = 'models_info';

  if (!forceRefresh) {
    const cached = cache.get<ModelsInfo>(cacheKey);
    if (cached) return cached;
  }

  const response = await execute<ModelsInfo>({ action: 'list_models' });

  if (response.success) {
    const info = response as unknown as ModelsInfo;
    cache.set(cacheKey, info, 10 * 60 * 1000); // 10 min TTL
    return info;
  }

  return null;
}

/**
 * Get available output models (cached)
 */
export async function getOutputModels(forceRefresh = false): Promise<string[]> {
  const cacheKey = 'output_models';

  if (!forceRefresh) {
    const cached = cache.get<string[]>(cacheKey);
    if (cached) return cached;
  }

  const response = await execute<{ models: string[] }>({ action: 'list_output_models' });

  if (response.success && response.data?.models) {
    cache.set(cacheKey, response.data.models, 10 * 60 * 1000);
    return response.data.models;
  }

  // Default output models if API fails
  return [
    'trade_signal',
    'stock_analysis',
    'portfolio_analysis',
    'risk_assessment',
    'market_analysis',
    'research_report',
  ];
}

// =============================================================================
// Workflow Execution
// =============================================================================

/**
 * Run a workflow (DAG of agent steps)
 */
export async function runWorkflow(
  workflowConfig: Record<string, any>,
  inputData: Record<string, any> = {},
  apiKeys?: Record<string, string>,
): Promise<AgentResponse> {
  return execute({
    action: 'run_workflow',
    api_keys: apiKeys,
    params: { workflow_config: workflowConfig, input_data: inputData },
  });
}

// =============================================================================
// Dynamic Agent Management
// =============================================================================

/**
 * List agents by category
 */
export async function listAgents(
  category?: string,
): Promise<AgentResponse<{ agents: any[]; count: number; categories: string[] }>> {
  return execute({
    action: 'list_agents',
    params: { category },
  });
}

/**
 * Create a dynamic agent from registry
 */
export async function createAgent(
  agentId: string,
  apiKeys?: Record<string, string>,
  config?: Record<string, any>,
): Promise<AgentResponse<{ agent_id: string; created: boolean }>> {
  return execute({
    action: 'create_agent',
    api_keys: apiKeys,
    config: config as any,
    params: { agent_id: agentId },
  });
}

// =============================================================================
// Multi-Query Routing
// =============================================================================

/**
 * Execute a query with multi-agent routing (routes to multiple agents)
 */
export async function executeMultiQuery(
  query: string,
  apiKeys?: Record<string, string>,
  sessionId?: string,
  aggregate = true,
): Promise<AgentResponse> {
  return execute({
    action: 'execute_multi_query',
    api_keys: apiKeys,
    params: { query, session_id: sessionId, aggregate },
  });
}

// =============================================================================
// Portfolio Plan
// =============================================================================

/**
 * Create a portfolio rebalancing plan
 */
export async function createPortfolioPlan(
  portfolioId: string,
): Promise<AgentResponse<{ plan: any }>> {
  return execute({
    action: 'create_portfolio_plan',
    params: { portfolio_id: portfolioId },
  });
}

/**
 * Run AI analysis on a portfolio using live internal MCP tools.
 * The agent fetches real-time holdings, risk metrics, diversification, and
 * macro data itself via the terminal tool bridge — not pre-baked text.
 */
export async function runPortfolioAnalysis(
  portfolioSummary: {
    portfolio: { id: string; name: string; currency: string };
    holdings: Array<{
      symbol: string;
      quantity: number;
      avg_buy_price: number;
      current_price: number;
      market_value: number;
      unrealized_pnl: number;
      unrealized_pnl_percent: number;
      day_change_percent: number;
      weight: number;
    }>;
    total_market_value: number;
    total_unrealized_pnl: number;
    total_unrealized_pnl_percent: number;
    total_day_change_percent: number;
  },
  analysisType: 'full' | 'risk' | 'rebalance' | 'opportunities' = 'full',
  apiKeys?: Record<string, string>,
  agentConfig?: AgentConfig,
): Promise<AgentResponse> {
  const portfolioId = portfolioSummary.portfolio.id;
  const portfolioName = portfolioSummary.portfolio.name;
  const currency = portfolioSummary.portfolio.currency;
  const nav = `${currency} ${portfolioSummary.total_market_value.toFixed(2)}`;

  // Each analysis type gets a focused query that tells the agent
  // exactly which tools to call and what to do with the results.
  const queries: Record<string, string> = {
    full: `Run a comprehensive analysis of my portfolio "${portfolioName}" (ID: ${portfolioId}, NAV: ${nav}).
Use these tools in order:
1. get_my_stocks — get all holdings with current prices, weights, and P&L
2. calculate_advanced_metrics with portfolio_id="${portfolioId}" — get Sharpe ratio, VaR, volatility, max drawdown
3. analyze_diversification with portfolio_id="${portfolioId}" — assess concentration and sector spread
4. economics_central_bank_rates — get global central bank rates
5. economics_inverted_yields — check yield curve inversion signals
6. economics_calendar — get upcoming macro events
7. economics_ceic_series with country="united-states" indicator="GDP" — get GDP trend
8. economics_ceic_series with country="united-states" indicator="CPI" — get inflation trend

Then provide a structured report: portfolio snapshot, risk metrics, diversification, macro environment, and specific actionable recommendations.`,

    risk: `Perform a risk analysis of portfolio "${portfolioName}" (ID: ${portfolioId}, NAV: ${nav}).
Use these tools:
1. get_my_stocks — get all holdings and current weights
2. calculate_advanced_metrics with portfolio_id="${portfolioId}" — get Sharpe ratio, VaR 95%, volatility, max drawdown
3. analyze_diversification with portfolio_id="${portfolioId}" — get concentration risk and sector breakdown
4. economics_inverted_yields — check recession signals from yield curve
5. economics_central_bank_rates — get rate environment

Deliver: overall risk rating, key risk factors with specific numbers, stress scenarios, and risk mitigation actions.`,

    rebalance: `Generate a rebalancing plan for portfolio "${portfolioName}" (ID: ${portfolioId}, NAV: ${nav}).
Use these tools:
1. get_my_stocks — get all holdings with exact weights
2. analyze_diversification with portfolio_id="${portfolioId}" — identify concentration issues
3. calculate_advanced_metrics with portfolio_id="${portfolioId}" — get current Sharpe and volatility
4. economics_central_bank_rates — understand the rate environment
5. economics_bond_spreads — assess fixed income conditions
6. economics_inverted_yields — check recession signals

Deliver: current vs target allocation table, specific trades to make (buy X / sell Y with amounts), and expected improvement in risk metrics.`,

    opportunities: `Identify investment opportunities for portfolio "${portfolioName}" (ID: ${portfolioId}, NAV: ${nav}).
Use these tools:
1. get_my_stocks — see current holdings and any gaps
2. analyze_diversification with portfolio_id="${portfolioId}" — identify underweighted sectors/geographies
3. economics_ceic_series with country="china" indicator="GDP" — check China growth
4. economics_ceic_series with country="india" indicator="GDP" — check India growth
5. economics_central_bank_rates — find easing cycles (rate cuts = opportunity signal)
6. economics_upcoming_events — upcoming catalysts

Deliver: gaps in current allocation, specific assets/ETFs/sectors to add, and macro-driven opportunities with rationale.`,
  };

  return execute({
    action: 'run',
    api_keys: apiKeys,
    config: agentConfig,
    params: {
      query: queries[analysisType],
    },
  });
}

// =============================================================================
// Paper Trading
// =============================================================================

/**
 * Execute a paper trade
 */
export async function paperExecuteTrade(
  portfolioId: string,
  symbol: string,
  action: 'buy' | 'sell',
  quantity: number,
  price: number,
): Promise<AgentResponse> {
  return execute({
    action: 'paper_execute_trade',
    params: { portfolio_id: portfolioId, symbol, action, quantity, price },
  });
}

/**
 * Get paper trading portfolio value
 */
export async function paperGetPortfolio(
  portfolioId: string,
): Promise<AgentResponse> {
  return execute({
    action: 'paper_get_portfolio',
    params: { portfolio_id: portfolioId },
  });
}

/**
 * Get paper trading positions
 */
export async function paperGetPositions(
  portfolioId: string,
): Promise<AgentResponse> {
  return execute({
    action: 'paper_get_positions',
    params: { portfolio_id: portfolioId },
  });
}

// =============================================================================
// Session Management
// =============================================================================

/**
 * Save an agent session
 */
export async function saveSession(
  sessionData: { agent_id: string; user_id?: string; messages?: any[]; status?: string },
): Promise<AgentResponse<{ session_id: string }>> {
  return execute({
    action: 'save_session',
    params: sessionData,
  });
}

/**
 * Get an agent session by ID
 */
export async function getSession(
  sessionId: string,
): Promise<AgentResponse<{ session: any }>> {
  return execute({
    action: 'get_session',
    params: { session_id: sessionId },
  });
}

/**
 * Add a message to an agent session
 */
export async function addSessionMessage(
  sessionId: string,
  role: 'user' | 'assistant' | 'system',
  content: string,
): Promise<AgentResponse> {
  return execute({
    action: 'add_message',
    params: { session_id: sessionId, role, content },
  });
}

// =============================================================================
// Repository Memory Operations
// =============================================================================

/**
 * Save a memory to the repository (persistent, searchable)
 */
export async function saveMemoryRepo(
  content: string,
  options?: {
    agent_id?: string;
    user_id?: string;
    type?: string;
    metadata?: Record<string, any>;
    importance?: number;
  },
): Promise<AgentResponse<{ memory_id: string }>> {
  return execute({
    action: 'save_memory',
    params: { content, ...options },
  });
}

/**
 * Search memories in the repository
 */
export async function searchMemoriesRepo(
  query: string,
  agentId?: string,
  limit = 10,
): Promise<AgentResponse<{ memories: any[] }>> {
  return execute({
    action: 'search_memories',
    params: { query, agent_id: agentId, limit },
  });
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Clear all caches
 */
export function clearCache(): void {
  cache.clear();
}

/**
 * Clear only response cache (keep static data)
 */
export function clearResponseCache(): void {
  cache.invalidateResponses();
}

/**
 * Get cache statistics
 */
export function getCacheStats(): { cacheSize: number; responseCacheSize: number; maxSize: number } {
  return cache.getStats();
}

/**
 * Pre-warm the agent system (call on app startup)
 */
export async function prewarmAgent(): Promise<void> {
  // Fetch and cache static data in parallel
  await Promise.all([
    getSystemInfo(),
    getTools(),
    getModels(),
  ]);
}

/**
 * Build agent config from UI state — wires ALL CoreAgent features
 */
export function buildAgentConfig(
  provider: string,
  modelId: string,
  instructions: string,
  options?: {
    tools?: string[];
    temperature?: number;
    maxTokens?: number;
    // Core features
    reasoning?: boolean | { enabled: boolean; strategy?: string; min_steps?: number; max_steps?: number; model?: { provider: string; model_id: string } };
    memory?: boolean | { enabled: boolean; db_path?: string; table_name?: string; create_user_memories?: boolean; create_session_summary?: boolean };
    // Advanced features
    knowledge?: AgentConfig['knowledge'];
    guardrails?: boolean | { enabled: boolean; pii_detection?: boolean; injection_check?: boolean; financial_compliance?: boolean };
    agentic_memory?: boolean | { enabled: boolean; user_id?: string };
    storage?: AgentConfig['storage'];
    tracing?: boolean;
    compression?: boolean;
    hooks?: boolean;
    evaluation?: boolean;
    output_model?: string;
  }
): AgentConfig {
  const config: AgentConfig = {
    model: {
      provider,
      model_id: modelId,
      temperature: options?.temperature ?? 0.7,
      max_tokens: options?.maxTokens ?? 4096,
    },
    instructions,
    tools: options?.tools ?? [],
  };

  // Memory
  if (options?.memory) {
    config.memory = typeof options.memory === 'boolean'
      ? { enabled: true }
      : options.memory;
  }

  // Reasoning
  if (options?.reasoning) {
    config.reasoning = typeof options.reasoning === 'boolean'
      ? true
      : options.reasoning;
  }

  // Knowledge Base
  if (options?.knowledge?.enabled) {
    config.knowledge = options.knowledge;
  }

  // Guardrails
  if (options?.guardrails) {
    config.guardrails = typeof options.guardrails === 'boolean'
      ? { enabled: true }
      : options.guardrails;
  }

  // Agentic Memory
  if (options?.agentic_memory) {
    config.agentic_memory = typeof options.agentic_memory === 'boolean'
      ? { enabled: true }
      : options.agentic_memory;
  }

  // Storage
  if (options?.storage?.enabled) {
    config.storage = options.storage;
  }

  // Simple toggles
  if (options?.tracing) config.tracing = true;
  if (options?.compression) config.compression = true;
  if (options?.hooks) config.hooks = true;
  if (options?.evaluation) config.evaluation = true;
  if (options?.output_model) config.output_model = options.output_model;

  return config;
}

/**
 * Build team config from UI state
 */
export function buildTeamConfig(
  name: string,
  mode: 'coordinate' | 'route' | 'collaborate',
  members: Array<{
    name: string;
    role?: string;
    provider: string;
    modelId: string;
    tools?: string[];
    instructions?: string;
  }>,
  options?: {
    coordinatorModel?: { provider: string; model_id: string };
    show_members_responses?: boolean;
    leader_index?: number;
  }
): TeamConfig {
  const config: TeamConfig = {
    name,
    mode,
    // Coordinator model for team orchestration (uses first member's model if not specified)
    model: options?.coordinatorModel || (members.length > 0 ? { provider: members[0].provider, model_id: members[0].modelId } : undefined),
    members: members.map(m => ({
      name: m.name,
      role: m.role,
      model: { provider: m.provider, model_id: m.modelId },
      tools: m.tools,
      instructions: m.instructions,
    })),
  };

  if (options?.show_members_responses) {
    config.show_members_responses = true;
  }
  if (options?.leader_index !== undefined && options.leader_index >= 0) {
    config.leader_index = options.leader_index;
  }

  return config;
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

/**
 * Route a query to the appropriate agent (SuperAgent)
 */
export async function routeQuery(
  query: string,
  apiKeys?: Record<string, string>
): Promise<RoutingResult> {
  try {
    const result = await invoke<string>('route_query', { query, apiKeys });
    return JSON.parse(result) as RoutingResult;
  } catch (error: any) {
    return {
      success: false,
      intent: 'general',
      agent_id: 'core_agent',
      confidence: 0,
      matched_keywords: [],
      matched_patterns: [],
      config: {},
    };
  }
}

/**
 * Execute query with automatic routing via SuperAgent
 * Passes user's model config so the agent uses the configured LLM
 */
export async function executeRoutedQuery(
  query: string,
  apiKeys?: Record<string, string>,
  sessionId?: string,
  config?: AgentConfig
): Promise<AgentResponse> {
  try {
    const result = await invoke<string>('execute_routed_query', {
      query,
      apiKeys,
      sessionId,
      config: config || undefined,
    });
    return JSON.parse(result) as AgentResponse;
  } catch (error: any) {
    return {
      success: false,
      error: error?.message || String(error),
    };
  }
}

/**
 * Discover all available agents
 * Now with proper error logging and timeout handling
 */
export async function discoverAgents(): Promise<AgentCard[]> {
  try {
    console.log('[AgentService] Starting agent discovery...');
    const startTime = Date.now();

    const result = await invoke<string>('discover_agents');
    const parsed = JSON.parse(result);

    const elapsed = Date.now() - startTime;
    console.log(`[AgentService] Discovered ${parsed.agents?.length || 0} agents in ${elapsed}ms`);

    if (!parsed.success && parsed.error) {
      console.warn('[AgentService] Discovery returned error:', parsed.error);
    }

    return parsed.agents || [];
  } catch (error: any) {
    // Log the actual error instead of swallowing it
    console.error('[AgentService] Agent discovery failed:', error?.message || error);

    // Return empty array but the error is now logged for debugging
    return [];
  }
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

/**
 * Create a stock analysis execution plan
 */
export async function createStockAnalysisPlan(symbol: string): Promise<ExecutionPlan | null> {
  try {
    const result = await invoke<string>('create_stock_analysis_plan', { symbol });
    const parsed = JSON.parse(result);
    return parsed.plan || null;
  } catch {
    return null;
  }
}

/**
 * Execute an execution plan
 */
export async function executePlan(
  plan: ExecutionPlan,
  apiKeys?: Record<string, string>
): Promise<ExecutionPlan | null> {
  try {
    const result = await invoke<string>('execute_plan', { plan, apiKeys });
    return JSON.parse(result) as ExecutionPlan;
  } catch {
    return null;
  }
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

/**
 * Save a trade decision to the repository
 */
export async function saveTradeDecision(
  decision: Omit<TradeDecision, 'id'>
): Promise<{ success: boolean; decision_id?: string; error?: string }> {
  try {
    const result = await invoke<string>('save_trade_decision', decision);
    return JSON.parse(result);
  } catch (error: any) {
    return { success: false, error: error?.message || String(error) };
  }
}

/**
 * Get trade decisions from repository
 */
export async function getTradeDecisions(filters?: {
  competition_id?: string;
  model_name?: string;
  cycle_number?: number;
  limit?: number;
}): Promise<TradeDecision[]> {
  try {
    const result = await invoke<string>('get_trade_decisions', filters || {});
    const parsed = JSON.parse(result);
    return parsed.decisions || [];
  } catch {
    return [];
  }
}

// =============================================================================
// Export default service object
// =============================================================================

export const agentService = {
  // Core
  runAgent,
  runAgentStructured,
  runTeam,

  // Streaming
  runAgentStreaming,
  runTeamStreaming,
  getActiveStreams,

  // Parallel Execution
  runAgentsParallel,
  runBatchStockAnalysis,

  // Priority Execution
  runAgentHighPriority,
  runAgentLowPriority,

  // Cached Execution
  runAgentCached,

  // Workflows
  runStockAnalysis,
  runPortfolioRebalancing,
  runRiskAssessment,

  // Memory
  storeMemory,
  recallMemories,
  searchKnowledge,

  // Guardrails
  checkGuardrails,
  evaluateResponse,

  // System (cached)
  getSystemInfo,
  getTools,
  getModels,
  getOutputModels,

  // SuperAgent & Routing (v2.1)
  routeQuery,
  executeRoutedQuery,
  discoverAgents,

  // Execution Planner (v2.1)
  createStockAnalysisPlan,
  executePlan,

  // Trade Decisions (v2.1)
  saveTradeDecision,
  getTradeDecisions,

  // Workflows
  runWorkflow,

  // Dynamic Agent Management
  listAgents,
  createAgent,

  // Multi-Query Routing
  executeMultiQuery,

  // Portfolio Plan
  createPortfolioPlan,

  // Paper Trading
  paperExecuteTrade,
  paperGetPortfolio,
  paperGetPositions,

  // Session Management
  saveSession,
  getSession,
  addSessionMessage,

  // Repository Memory
  saveMemoryRepo,
  searchMemoriesRepo,

  // Utils
  clearCache,
  clearResponseCache,
  getCacheStats,
  prewarmAgent,
  buildAgentConfig,
  buildTeamConfig,
  runPortfolioAnalysis,
};

// =============================================================================
// Renaissance Technologies Integration
// =============================================================================

export interface RenTechSignal {
  ticker: string;
  signal_type: string;
  direction: 'long' | 'short';
  strength: number;
  confidence: number;
  p_value: number;
  information_coefficient: number;
}

export interface RenTechConfig {
  max_position_size_pct?: number;
  max_leverage?: number;
  max_drawdown_pct?: number;
  max_daily_var_pct?: number;
  min_signal_confidence?: number;
}

export interface RenTechResult {
  success: boolean;
  signal_id?: string;
  statistical_quality?: string;
  risk_assessment?: string;
  ic_decision?: any;
  execution_plan?: any;
  error?: string;
}

/**
 * Run Renaissance Technologies signal analysis via finagent_core
 */
export async function runRenaissanceTechnologies(
  signal: RenTechSignal,
  config?: RenTechConfig
): Promise<RenTechResult> {
  try {
    const query = JSON.stringify({ command: 'analyze_signal', signal, config: config || {} });
    const payload = {
      action: 'run',
      api_keys: {},
      config: {
        model: { provider: 'openai', model_id: 'gpt-4-turbo' },
        instructions: 'You are a quantitative analyst embodying Renaissance Technologies approach. Apply mathematical pattern recognition, statistical arbitrage, and systematic signal analysis.',
      },
      params: { query },
    };
    const result = await invoke<string>('execute_core_agent', { payload });
    const parsed = JSON.parse(result);
    return parsed.data || parsed;
  } catch (error: any) {
    console.error('[RenTech] Analysis failed:', error);
    return { success: false, error: error.message || 'Renaissance Technologies analysis failed' };
  }
}

/**
 * Run IC Deliberation on a signal via finagent_core
 */
export async function runICDeliberation(deliberationData: {
  signal: any;
  signal_evaluation: any;
  risk_assessment: any;
  sizing_recommendation: any;
}): Promise<RenTechResult> {
  try {
    const query = JSON.stringify({ command: 'run_ic_deliberation', data: deliberationData });
    const payload = {
      action: 'run',
      api_keys: {},
      config: {
        model: { provider: 'openai', model_id: 'gpt-4-turbo' },
        instructions: 'You are an Investment Committee member at a quantitative hedge fund. Deliberate on trade signals and provide IC-level decisions with clear reasoning.',
      },
      params: { query },
    };
    const result = await invoke<string>('execute_core_agent', { payload });
    const parsed = JSON.parse(result);
    return { success: true, ic_decision: parsed.data || parsed.response };
  } catch (error: any) {
    console.error('[RenTech] IC deliberation failed:', error);
    return { success: false, error: error.message || 'IC deliberation failed' };
  }
}

/**
 * Get Renaissance Technologies agent presets via finagent_core
 */
export async function getRenTechPresets(): Promise<{ agents: any[]; teams: any[] }> {
  try {
    const payload = {
      action: 'list_agents',
      api_keys: {},
      params: { category: 'hedge-fund' },
    };
    const result = await invoke<string>('execute_core_agent', { payload });
    const parsed = JSON.parse(result);
    return parsed.data || { agents: parsed.agents || [], teams: [] };
  } catch (error) {
    console.error('[RenTech] Failed to get presets:', error);
    return { agents: [], teams: [] };
  }
}

export default agentService;
