/**
 * Alpha Arena Service (v2.1)
 *
 * TypeScript service layer for interacting with the Alpha Arena Python backend
 * via Tauri Rust commands. Now integrated with the Repository pattern for
 * persistent trade decision storage.
 */

import { invoke } from '@tauri-apps/api/core';
import { withTimeout, deduplicatedFetch } from '@/services/core/apiUtils';
import { validateString } from '@/services/core/validators';
import agentService, {
  type TradeDecision as AgentTradeDecision,
} from '@/services/agentService';
import type {
  CreateCompetitionRequest,
  CreateCompetitionResponse,
  RunCycleResponse,
  LeaderboardResponse,
  DecisionsResponse,
  SnapshotsResponse,
  ListAgentsResponse,
  ModelDecision,
  CompetitionType,
  PolymarketMarketInfo,
} from '../types';

// =============================================================================
// Error Extraction Helper
// =============================================================================

function extractError(err: unknown): string {
  if (err instanceof Error) return err.message;
  if (typeof err === 'string') return err;
  return 'Unknown error';
}

// =============================================================================
// Competition Management
// =============================================================================

/**
 * Create a new competition
 */
export async function createCompetition(
  request: CreateCompetitionRequest
): Promise<CreateCompetitionResponse> {
  try {
    const configJson = JSON.stringify(request);
    // Extract API keys from models to pass separately to the Rust command
    // This ensures keys are available even if the Rust config parser doesn't extract them
    const apiKeys: Record<string, string> = {};
    for (const model of request.models) {
      if (model.api_key && !apiKeys[model.provider]) {
        apiKeys[model.provider] = model.api_key;
      }
    }
    const result = await withTimeout(
      invoke<CreateCompetitionResponse>('create_alpha_competition', {
        configJson,
        apiKeys: Object.keys(apiKeys).length > 0 ? apiKeys : null,
      }),
      30000, 'Create competition timeout'
    );
    return result;
  } catch (error) {
    console.error('Error creating competition:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Start a competition
 */
export async function startCompetition(
  competitionId: string
): Promise<{ success: boolean; error?: string }> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const result = await withTimeout(
      invoke<{ success: boolean; error?: string }>('start_alpha_competition', { competitionId }),
      30000, 'Start competition timeout'
    );
    return result;
  } catch (error) {
    console.error('Error starting competition:', error);
    return { success: false, error: extractError(error) };
  }
}

// Portfolio update callback type
export type PortfolioUpdateCallback = (update: {
  modelName: string;
  portfolioValue: number;
  cash: number;
  totalPnl: number;
  positions: Array<{
    symbol: string;
    side: string;
    quantity: number;
    entry_price: number;
    current_price: number;
    unrealized_pnl: number;
  }>;
  tradesCount: number;
  cycleNumber: number;
  timestamp: string;
}) => void;

// Store for real-time portfolio update callbacks (Set avoids duplicate registrations)
const portfolioUpdateCallbacks = new Set<PortfolioUpdateCallback>();

/**
 * Subscribe to real-time portfolio updates during cycle execution
 */
export function onPortfolioUpdate(callback: PortfolioUpdateCallback): () => void {
  portfolioUpdateCallbacks.add(callback);
  return () => {
    portfolioUpdateCallbacks.delete(callback);
  };
}

/**
 * Run a single competition cycle
 * Now also saves decisions to the repository for persistence
 * AND emits real-time portfolio updates
 */
export async function runCycle(competitionId: string, apiKeys?: Record<string, string>): Promise<RunCycleResponse> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const result = await withTimeout(
      invoke<RunCycleResponse>('run_alpha_cycle', {
        competitionId,
        apiKeys: apiKeys || null,
      }),
      120000, 'Run cycle timeout'
    );

    // If cycle was successful, process events
    if (result.success && result.events) {
      // Process portfolio update events for real-time updates
      const portfolioEvents = result.events.filter(
        e => e.event === 'portfolio_update' && e.metadata
      );

      for (const event of portfolioEvents) {
        const meta = event.metadata as Record<string, unknown>;
        if (meta?.model_name) {
          const update = {
            modelName: meta.model_name as string,
            portfolioValue: (meta.portfolio_value as number) || 0,
            cash: (meta.cash as number) || 0,
            totalPnl: (meta.total_pnl as number) || 0,
            positions: (meta.positions as Array<{
              symbol: string;
              side: string;
              quantity: number;
              entry_price: number;
              current_price: number;
              unrealized_pnl: number;
            }>) || [],
            tradesCount: (meta.trades_count as number) || 0,
            cycleNumber: (meta.cycle_number as number) || 0,
            timestamp: (meta.timestamp as string) || new Date().toISOString(),
          };

          // Notify all subscribers immediately (snapshot copy to avoid mutation during iteration)
          for (const callback of [...portfolioUpdateCallbacks]) {
            try {
              callback(update);
            } catch (err) {
              console.warn('Portfolio update callback error:', err);
            }
          }
        }
      }

      // Save decisions to repository for persistence
      const decisionEvents = result.events.filter(
        e => e.event === 'decision_completed' && e.metadata
      );

      const saveWarnings: string[] = [];
      for (const event of decisionEvents) {
        const meta = event.metadata as Record<string, unknown>;
        if (meta?.model_name && meta?.action && meta?.symbol) {
          try {
            await saveDecisionToRepository({
              competitionId,
              cycleNumber: result.cycle_number || 0,
              modelName: meta.model_name as string,
              action: meta.action as string,
              symbol: meta.symbol as string,
              quantity: (meta.quantity as number) || 0,
              price: meta.price as number | undefined,
              reasoning: (meta.reasoning as string) || '',
              confidence: (meta.confidence as number) || 0,
            });
          } catch (err) {
            const msg = err instanceof Error ? err.message : String(err);
            console.warn('Failed to save decision to repository:', msg);
            saveWarnings.push(`Decision save failed for ${meta.model_name}: ${msg}`);
          }
        }
      }

      if (saveWarnings.length > 0) {
        result.warnings = saveWarnings;
      }
    }

    return result;
  } catch (error) {
    console.error('Error running cycle:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Stop a competition
 */
export async function stopCompetition(
  competitionId: string
): Promise<{ success: boolean; error?: string }> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const result = await withTimeout(
      invoke<{ success: boolean; error?: string }>('stop_alpha_competition', { competitionId }),
      30000, 'Stop competition timeout'
    );
    return result;
  } catch (error) {
    console.error('Error stopping competition:', error);
    return { success: false, error: extractError(error) };
  }
}

// =============================================================================
// Data Retrieval
// =============================================================================

/**
 * Get current leaderboard
 */
export async function getLeaderboard(
  competitionId: string
): Promise<LeaderboardResponse> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const result = await deduplicatedFetch(`alpha:leaderboard:${competitionId}`, () =>
      withTimeout(
        invoke<LeaderboardResponse>('get_alpha_leaderboard', { competitionId }),
        15000, 'Get leaderboard timeout'
      )
    );
    return result;
  } catch (error) {
    console.error('[alphaArenaService] Error getting leaderboard:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Get model decisions - now uses repository for historical data
 */
export async function getDecisions(
  competitionId: string,
  modelName?: string
): Promise<DecisionsResponse> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const key = `alpha:decisions:${competitionId}:${modelName || 'all'}`;
    const result = await deduplicatedFetch(key, () =>
      withTimeout(
        invoke<DecisionsResponse>('get_alpha_model_decisions', {
          competitionId,
          modelName: modelName || null,
        }),
        15000, 'Get decisions timeout'
      )
    );
    return result;
  } catch (error) {
    console.error('[alphaArenaService] Error getting decisions:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Get performance snapshots for charting
 */
export async function getSnapshots(
  competitionId: string
): Promise<SnapshotsResponse> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const result = await deduplicatedFetch(`alpha:snapshots:${competitionId}`, () =>
      withTimeout(
        invoke<SnapshotsResponse>('get_alpha_snapshots', { competitionId }),
        15000, 'Get snapshots timeout'
      )
    );
    return result;
  } catch (error) {
    console.error('[alphaArenaService] Error getting snapshots:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * List available agents
 */
export async function listAgents(): Promise<ListAgentsResponse> {
  try {
    const result = await deduplicatedFetch('alpha:listAgents', () =>
      withTimeout(invoke<ListAgentsResponse>('list_alpha_agents'), 15000, 'List agents timeout')
    );
    return result;
  } catch (error) {
    console.error('Error listing agents:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Get evaluation metrics for a model
 */
export async function getEvaluation(
  competitionId: string,
  modelName: string
): Promise<{ success: boolean; metrics?: Record<string, number>; error?: string }> {
  const v1 = validateString(competitionId, { minLength: 1 });
  if (!v1.valid) return { success: false, error: `Invalid competitionId: ${v1.error}` };
  const v2 = validateString(modelName, { minLength: 1 });
  if (!v2.valid) return { success: false, error: `Invalid modelName: ${v2.error}` };

  try {
    const result = await deduplicatedFetch(`alpha:eval:${competitionId}:${modelName}`, () =>
      withTimeout(
        invoke<{ success: boolean; metrics?: Record<string, number> }>('get_alpha_evaluation', { competitionId, modelName }),
        15000, 'Get evaluation timeout'
      )
    );
    return result;
  } catch (error) {
    console.error('Error getting evaluation:', error);
    return { success: false, error: extractError(error) };
  }
}

// =============================================================================
// Repository Integration (NEW in v2.1)
// =============================================================================

interface SaveDecisionParams {
  competitionId: string;
  cycleNumber: number;
  modelName: string;
  action: string;
  symbol: string;
  quantity: number;
  price?: number;
  reasoning: string;
  confidence: number;
}

/**
 * Save a trade decision to the repository for persistence
 */
export async function saveDecisionToRepository(
  params: SaveDecisionParams
): Promise<{ success: boolean; decisionId?: string; error?: string }> {
  try {
    const result = await agentService.saveTradeDecision({
      competition_id: params.competitionId,
      model_name: params.modelName,
      cycle_number: params.cycleNumber,
      symbol: params.symbol,
      action: params.action,
      quantity: params.quantity,
      price: params.price,
      reasoning: params.reasoning,
      confidence: params.confidence,
    });

    return {
      success: result.success,
      decisionId: result.decision_id,
      error: result.error,
    };
  } catch (error) {
    console.error('Error saving decision to repository:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Get historical trade decisions from repository
 */
export async function getHistoricalDecisions(
  competitionId?: string,
  modelName?: string,
  limit: number = 100
): Promise<AgentTradeDecision[]> {
  try {
    return await agentService.getTradeDecisions({
      competition_id: competitionId,
      model_name: modelName,
      limit,
    });
  } catch (error) {
    console.error('Error getting historical decisions:', error);
    return [];
  }
}

/**
 * Get decisions for a specific cycle
 */
export async function getCycleDecisions(
  competitionId: string,
  cycleNumber: number
): Promise<AgentTradeDecision[]> {
  try {
    return await agentService.getTradeDecisions({
      competition_id: competitionId,
      cycle_number: cycleNumber,
    });
  } catch (error) {
    console.error('Error getting cycle decisions:', error);
    return [];
  }
}

/**
 * Get model performance summary from repository
 */
export async function getModelPerformanceSummary(
  competitionId: string,
  modelName: string
): Promise<{
  totalDecisions: number;
  buyCount: number;
  sellCount: number;
  holdCount: number;
  avgConfidence: number;
}> {
  try {
    const decisions = await agentService.getTradeDecisions({
      competition_id: competitionId,
      model_name: modelName,
      limit: 1000,
    });

    const summary = {
      totalDecisions: decisions.length,
      buyCount: 0,
      sellCount: 0,
      holdCount: 0,
      avgConfidence: 0,
    };

    let totalConfidence = 0;
    for (const d of decisions) {
      if (d.action === 'buy') summary.buyCount++;
      else if (d.action === 'sell') summary.sellCount++;
      else if (d.action === 'hold') summary.holdCount++;
      totalConfidence += d.confidence;
    }

    summary.avgConfidence = decisions.length > 0
      ? totalConfidence / decisions.length
      : 0;

    return summary;
  } catch (error) {
    console.error('Error getting model performance summary:', error);
    return {
      totalDecisions: 0,
      buyCount: 0,
      sellCount: 0,
      holdCount: 0,
      avgConfidence: 0,
    };
  }
}

// =============================================================================
// Competition Persistence & History
// =============================================================================

export interface StoredCompetition {
  competition_id: string;
  config: {
    competition_id: string;
    competition_name: string;
    models: Array<{
      name: string;
      provider: string;
      model_id: string;
      api_key?: string;
      initial_capital: number;
    }>;
    symbols: string[];
    initial_capital: number;
    mode: string;
    cycle_interval_seconds: number;
    exchange_id: string;
    competition_type?: CompetitionType;
    polymarket_markets?: PolymarketMarketInfo[];
  };
  status: string;
  cycle_count: number;
  start_time: string | null;
  end_time: string | null;
  created_at: string;
}

export interface ListCompetitionsResponse {
  success: boolean;
  competitions?: StoredCompetition[];
  count?: number;
  error?: string;
}

export interface GetCompetitionResponse {
  success: boolean;
  competition?: StoredCompetition;
  error?: string;
}

/**
 * List all past competitions from the database
 */
export async function listCompetitions(limit: number = 50): Promise<ListCompetitionsResponse> {
  try {
    const result = await deduplicatedFetch(`alpha:listCompetitions:${limit}`, () =>
      withTimeout(
        invoke<ListCompetitionsResponse>('list_alpha_competitions', { limit }),
        15000, 'List competitions timeout'
      )
    );
    return result;
  } catch (error) {
    console.error('Error listing competitions:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Get a specific competition by ID
 */
export async function getCompetition(competitionId: string): Promise<GetCompetitionResponse> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const result = await deduplicatedFetch(`alpha:getCompetition:${competitionId}`, () =>
      withTimeout(
        invoke<GetCompetitionResponse>('get_alpha_competition', { competitionId }),
        15000, 'Get competition timeout'
      )
    );
    return result;
  } catch (error) {
    console.error('Error getting competition:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Delete a competition
 */
export async function deleteCompetition(competitionId: string): Promise<{ success: boolean; error?: string }> {
  const v = validateString(competitionId, { minLength: 1 });
  if (!v.valid) return { success: false, error: `Invalid competitionId: ${v.error}` };

  try {
    const result = await withTimeout(
      invoke<{ success: boolean; error?: string }>('delete_alpha_competition', { competitionId }),
      30000, 'Delete competition timeout'
    );
    return result;
  } catch (error) {
    console.error('Error deleting competition:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Resume/reload a past competition
 * This loads the competition from the database and prepares it for running
 */
export async function resumeCompetition(competitionId: string): Promise<{
  success: boolean;
  competition?: StoredCompetition;
  error?: string;
}> {
  try {
    // First, get the competition data
    const compResult = await getCompetition(competitionId);
    if (!compResult.success || !compResult.competition) {
      return {
        success: false,
        error: compResult.error || 'Competition not found',
      };
    }

    // The backend will restore it from the database when we run the next cycle
    return {
      success: true,
      competition: compResult.competition,
    };
  } catch (error) {
    console.error('Error resuming competition:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Trading Styles (v2.2)
// =============================================================================

export interface TradingStyle {
  id: string;
  name: string;
  description: string;
  style_type: string;
  risk_tolerance: number;
  position_size_pct: number;
  hold_bias: number;
  confidence_threshold: number;
  temperature_modifier: number;
  system_prompt: string;
  instructions: string[];
  max_trades_per_cycle: number;
  stop_loss_pct: number | null;
  take_profit_pct: number | null;
  color: string;
  icon: string;
}

export interface ListTradingStylesResponse {
  success: boolean;
  styles?: TradingStyle[];
  count?: number;
  error?: string;
}

export interface StyledAgent {
  name: string;
  provider: string;
  model_id: string;
  api_key?: string;
  initial_capital: number;
  trading_style: string;
  metadata?: {
    style_config?: TradingStyle;
  };
}

export interface CreateStyledAgentsResponse {
  success: boolean;
  agents?: StyledAgent[];
  count?: number;
  provider?: string;
  model_id?: string;
  styles_used?: string[];
  error?: string;
}

/**
 * List all available trading styles
 * Trading styles allow running the same LLM provider with different behavioral personalities
 */
export async function listTradingStyles(): Promise<ListTradingStylesResponse> {
  try {
    const result = await deduplicatedFetch('alpha:listTradingStyles', () =>
      withTimeout(invoke<ListTradingStylesResponse>('list_alpha_trading_styles'), 15000, 'List trading styles timeout')
    );
    return result;
  } catch (error) {
    console.error('Error listing trading styles:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Create multiple agents with different trading styles using a single provider
 * This is useful for testing how the same model behaves with different strategies
 *
 * @param provider - LLM provider (openai, anthropic, google, deepseek, groq, etc.)
 * @param modelId - Model identifier (gpt-4o-mini, claude-3-5-sonnet, etc.)
 * @param styles - Optional list of style IDs to use (defaults to diverse set)
 * @param initialCapital - Starting capital per agent (default: 10000)
 */
export async function createStyledAgents(
  provider: string,
  modelId: string,
  styles?: string[],
  initialCapital: number = 10000
): Promise<CreateStyledAgentsResponse> {
  const v1 = validateString(provider, { minLength: 1 });
  if (!v1.valid) return { success: false, error: `Invalid provider: ${v1.error}` };
  const v2 = validateString(modelId, { minLength: 1 });
  if (!v2.valid) return { success: false, error: `Invalid modelId: ${v2.error}` };

  try {
    const result = await withTimeout(
      invoke<CreateStyledAgentsResponse>('create_alpha_styled_agents', {
        provider, modelId, styles, initialCapital,
      }),
      30000, 'Create styled agents timeout'
    );
    return result;
  } catch (error) {
    console.error('Error creating styled agents:', error);
    return { success: false, error: extractError(error) };
  }
}

/**
 * Create a competition with styled agents using a single provider
 * Convenience function that combines createStyledAgents and createCompetition
 */
export async function createStyledCompetition(params: {
  name: string;
  provider: string;
  modelId: string;
  styles?: string[];
  symbol?: string;
  initialCapital?: number;
  cycleInterval?: number;
  exchangeId?: string;
}): Promise<CreateCompetitionResponse> {
  try {
    // First, create the styled agents
    const agentsResult = await createStyledAgents(
      params.provider,
      params.modelId,
      params.styles,
      params.initialCapital || 10000
    );

    if (!agentsResult.success || !agentsResult.agents) {
      return {
        success: false,
        error: agentsResult.error || 'Failed to create styled agents',
      };
    }

    // Now create the competition with these agents
    const competitionRequest: CreateCompetitionRequest = {
      competition_name: params.name,
      models: agentsResult.agents.map(agent => ({
        name: agent.name,
        provider: agent.provider,
        model_id: agent.model_id,
        initial_capital: agent.initial_capital,
        trading_style: agent.trading_style,
        metadata: agent.metadata,
      })),
      symbols: [params.symbol || 'BTC/USD'],
      initial_capital: params.initialCapital || 10000,
      mode: 'baseline',
      cycle_interval_seconds: params.cycleInterval || 150,
      exchange_id: params.exchangeId || 'kraken',
    };

    return await createCompetition(competitionRequest);
  } catch (error) {
    console.error('Error creating styled competition:', error);
    return { success: false, error: extractError(error) };
  }
}

// =============================================================================
// Export Service Object
// =============================================================================

export const alphaArenaService = {
  // Competition Management
  createCompetition,
  startCompetition,
  runCycle,
  stopCompetition,

  // Competition Persistence & History
  listCompetitions,
  getCompetition,
  deleteCompetition,
  resumeCompetition,

  // Data Retrieval
  getLeaderboard,
  getDecisions,
  getSnapshots,
  listAgents,
  getEvaluation,

  // Repository Integration (v2.1)
  saveDecisionToRepository,
  getHistoricalDecisions,
  getCycleDecisions,
  getModelPerformanceSummary,

  // Real-time Portfolio Updates
  onPortfolioUpdate,

  // Trading Styles (v2.2) - Single provider with multiple agent personalities
  listTradingStyles,
  createStyledAgents,
  createStyledCompetition,
};

export default alphaArenaService;
