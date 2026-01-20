/**
 * Alpha Arena Service (v2.1)
 *
 * TypeScript service layer for interacting with the Alpha Arena Python backend
 * via Tauri Rust commands. Now integrated with the Repository pattern for
 * persistent trade decision storage.
 */

import { invoke } from '@tauri-apps/api/core';
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
} from '../types';

// =============================================================================
// API Key Management
// =============================================================================

interface ApiKeysResponse {
  success: boolean;
  keys?: Record<string, string>;
  error?: string;
}

/**
 * Get API keys (masked for display)
 */
export async function getApiKeys(): Promise<ApiKeysResponse> {
  try {
    const result = await invoke<ApiKeysResponse>('get_alpha_api_keys');
    return result;
  } catch (error) {
    console.error('Error getting API keys:', error);

    // Fallback to localStorage
    const stored = localStorage.getItem('alpha_arena_api_keys');
    if (stored) {
      return { success: true, keys: JSON.parse(stored) };
    }

    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Save API keys
 */
export async function saveApiKeys(
  keys: Record<string, string>
): Promise<{ success: boolean; error?: string }> {
  try {
    const result = await invoke<{ success: boolean; message?: string }>(
      'save_alpha_api_keys',
      { keys }
    );

    // Also save to localStorage as backup
    localStorage.setItem('alpha_arena_api_keys', JSON.stringify(keys));

    return { success: result.success };
  } catch (error) {
    console.error('Error saving API keys:', error);

    // Fallback to localStorage only
    localStorage.setItem('alpha_arena_api_keys', JSON.stringify(keys));

    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
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
    const result = await invoke<CreateCompetitionResponse>(
      'create_alpha_competition',
      { configJson }
    );
    return result;
  } catch (error) {
    console.error('Error creating competition:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Start a competition
 */
export async function startCompetition(
  competitionId: string
): Promise<{ success: boolean; error?: string }> {
  try {
    const result = await invoke<{ success: boolean; error?: string }>(
      'start_alpha_competition',
      { competitionId }
    );
    return result;
  } catch (error) {
    console.error('Error starting competition:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Run a single competition cycle
 * Now also saves decisions to the repository for persistence
 */
export async function runCycle(competitionId: string): Promise<RunCycleResponse> {
  try {
    const result = await invoke<RunCycleResponse>('run_alpha_cycle', {
      competitionId,
    });

    // If cycle was successful, save decisions to repository for persistence
    if (result.success && result.events) {
      const decisionEvents = result.events.filter(
        e => e.event === 'decision_completed' && e.metadata
      );

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
            console.warn('Failed to save decision to repository:', err);
          }
        }
      }
    }

    return result;
  } catch (error) {
    console.error('Error running cycle:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Stop a competition
 */
export async function stopCompetition(
  competitionId: string
): Promise<{ success: boolean; error?: string }> {
  try {
    const result = await invoke<{ success: boolean; error?: string }>(
      'stop_alpha_competition',
      { competitionId }
    );
    return result;
  } catch (error) {
    console.error('Error stopping competition:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
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
  try {
    console.log('[alphaArenaService] getLeaderboard called for:', competitionId);
    const rawResult = await invoke<unknown>('get_alpha_leaderboard', {
      competitionId,
    });
    console.log('[alphaArenaService] getLeaderboard raw result type:', typeof rawResult);
    console.log('[alphaArenaService] getLeaderboard raw result:', JSON.stringify(rawResult, null, 2));

    // Handle potential type issues - cast to expected type
    const result = rawResult as LeaderboardResponse;
    console.log('[alphaArenaService] getLeaderboard success:', result.success);
    console.log('[alphaArenaService] getLeaderboard leaderboard:', result.leaderboard);
    console.log('[alphaArenaService] getLeaderboard leaderboard length:', result.leaderboard?.length);

    return result;
  } catch (error) {
    console.error('[alphaArenaService] Error getting leaderboard:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Get model decisions - now uses repository for historical data
 */
export async function getDecisions(
  competitionId: string,
  modelName?: string
): Promise<DecisionsResponse> {
  try {
    console.log('[alphaArenaService] getDecisions called for:', competitionId, 'model:', modelName);
    // Try to get from Alpha Arena first
    const result = await invoke<DecisionsResponse>('get_alpha_model_decisions', {
      competitionId,
      modelName: modelName || null,
    });

    console.log('[alphaArenaService] getDecisions raw result:', result);
    console.log('[alphaArenaService] getDecisions success:', result.success);
    console.log('[alphaArenaService] getDecisions decisions:', result.decisions);
    console.log('[alphaArenaService] getDecisions decisions length:', result.decisions?.length);

    // Skip repository merging for now - just return the result
    return result;
  } catch (error) {
    console.error('[alphaArenaService] Error getting decisions:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Get performance snapshots for charting
 */
export async function getSnapshots(
  competitionId: string
): Promise<SnapshotsResponse> {
  try {
    console.log('[alphaArenaService] getSnapshots called for:', competitionId);
    const result = await invoke<SnapshotsResponse>('get_alpha_snapshots', {
      competitionId,
    });
    console.log('[alphaArenaService] getSnapshots raw result:', result);
    console.log('[alphaArenaService] getSnapshots success:', result.success);
    console.log('[alphaArenaService] getSnapshots snapshots:', result.snapshots);
    console.log('[alphaArenaService] getSnapshots snapshots length:', result.snapshots?.length);
    return result;
  } catch (error) {
    console.error('[alphaArenaService] Error getting snapshots:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * List available agents
 */
export async function listAgents(): Promise<ListAgentsResponse> {
  try {
    const result = await invoke<ListAgentsResponse>('list_alpha_agents');
    return result;
  } catch (error) {
    console.error('Error listing agents:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Get evaluation metrics for a model
 */
export async function getEvaluation(
  competitionId: string,
  modelName: string
): Promise<{ success: boolean; metrics?: Record<string, number>; error?: string }> {
  try {
    const result = await invoke<{ success: boolean; metrics?: Record<string, number> }>(
      'get_alpha_evaluation',
      { competitionId, modelName }
    );
    return result;
  } catch (error) {
    console.error('Error getting evaluation:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
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
    const result = await invoke<ListCompetitionsResponse>('list_alpha_competitions', {
      limit,
    });
    return result;
  } catch (error) {
    console.error('Error listing competitions:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Get a specific competition by ID
 */
export async function getCompetition(competitionId: string): Promise<GetCompetitionResponse> {
  try {
    const result = await invoke<GetCompetitionResponse>('get_alpha_competition', {
      competitionId,
    });
    return result;
  } catch (error) {
    console.error('Error getting competition:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

/**
 * Delete a competition
 */
export async function deleteCompetition(competitionId: string): Promise<{ success: boolean; error?: string }> {
  try {
    const result = await invoke<{ success: boolean; error?: string }>('delete_alpha_competition', {
      competitionId,
    });
    return result;
  } catch (error) {
    console.error('Error deleting competition:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
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
// Auto-Run Management
// =============================================================================

let autoRunInterval: ReturnType<typeof setInterval> | null = null;

export function startAutoRun(
  competitionId: string,
  intervalSeconds: number,
  onCycleComplete: (result: RunCycleResponse) => void
): void {
  stopAutoRun();

  autoRunInterval = setInterval(async () => {
    const result = await runCycle(competitionId);
    onCycleComplete(result);
  }, intervalSeconds * 1000);
}

export function stopAutoRun(): void {
  if (autoRunInterval) {
    clearInterval(autoRunInterval);
    autoRunInterval = null;
  }
}

export function isAutoRunning(): boolean {
  return autoRunInterval !== null;
}

// =============================================================================
// Export Service Object
// =============================================================================

export const alphaArenaService = {
  // API Keys
  getApiKeys,
  saveApiKeys,

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

  // Auto-Run
  startAutoRun,
  stopAutoRun,
  isAutoRunning,
};

export default alphaArenaService;
