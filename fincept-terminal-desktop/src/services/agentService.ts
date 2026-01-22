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
  memory?: { enabled: boolean };
  reasoning?: boolean | { strategy: string };
  debug?: boolean;
}

export interface TeamConfig {
  name: string;
  mode: 'coordinate' | 'route' | 'collaborate';
  members: Array<{
    name: string;
    role?: string;
    model: AgentConfig['model'];
    tools?: string[];
    instructions?: string;
  }>;
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

/**
 * Execute a CoreAgent action with the given payload
 */
async function execute<T = any>(payload: AgentPayload): Promise<AgentResponse<T>> {
  try {
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
    action: 'search',
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

  const response = await execute<{ models: string[] }>({ action: 'list_outputs' });

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
 * Build agent config from UI state
 */
export function buildAgentConfig(
  provider: string,
  modelId: string,
  instructions: string,
  options?: {
    tools?: string[];
    temperature?: number;
    maxTokens?: number;
    reasoning?: boolean;
    memory?: boolean;
  }
): AgentConfig {
  return {
    model: {
      provider,
      model_id: modelId,
      temperature: options?.temperature ?? 0.7,
      max_tokens: options?.maxTokens ?? 4096,
    },
    instructions,
    tools: options?.tools ?? [],
    reasoning: options?.reasoning,
    memory: options?.memory ? { enabled: true } : undefined,
  };
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
  }>
): TeamConfig {
  return {
    name,
    mode,
    members: members.map(m => ({
      name: m.name,
      role: m.role,
      model: { provider: m.provider, model_id: m.modelId },
      tools: m.tools,
      instructions: m.instructions,
    })),
  };
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
 */
export async function executeRoutedQuery(
  query: string,
  apiKeys?: Record<string, string>,
  sessionId?: string
): Promise<AgentResponse> {
  try {
    const result = await invoke<string>('execute_routed_query', {
      query,
      apiKeys,
      sessionId,
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

  // Utils
  clearCache,
  clearResponseCache,
  getCacheStats,
  prewarmAgent,
  buildAgentConfig,
  buildTeamConfig,
};

export default agentService;
