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
import { mcpToolService } from '@/services/mcp/mcpToolService';
import { INTERNAL_SERVER_ID } from '@/services/mcp/internal';
import {
  type AgentConfig,
  type TeamConfig,
  type AgentPayload,
  type AgentResponse,
  type SystemInfo,
  type ToolsInfo,
  type ModelsInfo,
  type ParallelTask,
  type ParallelResult,
  type BatchResponse,
  type RoutingResult,
  type AgentCard,
  type PlanStep,
  type ExecutionPlan,
  type TradeDecision,
  AgentPriority,
} from './agentServiceTypes';
export type {
  AgentConfig,
  TeamConfig,
  AgentPayload,
  AgentResponse,
  SystemInfo,
  ToolsInfo,
  ModelsInfo,
  ParallelTask,
  ParallelResult,
  BatchResponse,
  RoutingResult,
  AgentCard,
  PlanStep,
  ExecutionPlan,
  TradeDecision,
};
export { AgentPriority };
import { LRUCache } from './agentServiceCache';

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
 * If `requestedToolNames` is provided, only return tools matching those names.
 */
async function getTerminalToolDefinitions(requestedToolNames?: string[]): Promise<AgentConfig['terminal_tools']> {
  try {
    const allTools = await mcpToolService.getAllTools();
    return allTools
      .filter(t =>
        t.serverId === INTERNAL_SERVER_ID &&
        !BLOCKED_TERMINAL_TOOLS.has(t.name) &&
        (!requestedToolNames || requestedToolNames.includes(t.name))
      )
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
 *
 * For Fincept provider (which has a ~50KB payload limit) we skip the full tool list,
 * but still inject tools when the agent explicitly requests specific ones — those are
 * small enough to stay well under the limit.
 */
async function execute<T = any>(payload: AgentPayload): Promise<AgentResponse<T>> {
  try {
    if (payload.action === 'run' && payload.config) {
      const provider = payload.config.model?.provider?.toLowerCase() ?? '';
      const isFincept = provider === 'fincept' || provider === 'fincept-llm';
      // Requested tool names from the agent config (e.g. polymarket bot tools)
      const requestedTools: string[] | undefined =
        Array.isArray(payload.config.tools) && payload.config.tools.length > 0
          ? payload.config.tools
          : undefined;

      if (!isFincept) {
        // Non-Fincept: inject the full terminal tool set
        const terminalTools = await getTerminalToolDefinitions();
        if (terminalTools && terminalTools.length > 0) {
          payload = {
            ...payload,
            config: { ...payload.config, terminal_tools: terminalTools },
          };
        }
      } else if (requestedTools) {
        // Fincept with specific tool requests: inject only those tools to stay under payload limit
        const terminalTools = await getTerminalToolDefinitions(requestedTools);
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
 * Generate a dynamic execution plan using LLM from a natural-language query.
 * Returns { plan, generated_by } where generated_by is "llm" or "fallback".
 */
export async function generateDynamicPlan(
  query: string,
  apiKeys?: Record<string, string>
): Promise<{ plan: ExecutionPlan; generated_by: string } | null> {
  try {
    const result = await invoke<string>('generate_dynamic_plan', { query, apiKeys: apiKeys ?? {} });
    const parsed = JSON.parse(result);
    if (parsed.success && parsed.plan) {
      return { plan: parsed.plan as ExecutionPlan, generated_by: parsed.generated_by ?? 'llm' };
    }
    return null;
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

  // System (cached)
  getSystemInfo,
  getTools,
  getModels,

  // SuperAgent & Routing (v2.1)
  routeQuery,
  executeRoutedQuery,
  discoverAgents,

  // Execution Planner (v2.1)
  createStockAnalysisPlan,
  generateDynamicPlan,
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
  buildAgentConfig,
  buildTeamConfig,
  runPortfolioAnalysis,
};

export default agentService;
