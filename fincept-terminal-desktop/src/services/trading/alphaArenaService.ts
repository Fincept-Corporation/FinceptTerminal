/**
 * Alpha Arena Service
 * TypeScript service for NOF1-style AI trading competition
 */

import { invoke } from '@tauri-apps/api/core';
import { sqliteService } from '../core/sqliteService';

export interface CompetitionModel {
  name: string;
  provider: string;
  model_id: string;
}

export interface LeaderboardEntry {
  model: string;
  pnl: number;
  return_pct: number;
  portfolio_value: number;
  cash: number;
  trades: number;
  positions: number;
}

export interface ModelDecision {
  model: string;
  timestamp: string;
  action: string;
  symbol: string;
  confidence: number;
  reasoning: string;
  quantity?: number;
  trade_executed?: any;
}

export interface CompetitionResult {
  success: boolean;
  data?: any;
  error?: string;
}

export interface ConfiguredLLM {
  id: string;
  provider: string;
  model_id: string;
  display_name: string;
  api_key_available: boolean;
}

export const alphaArenaService = {
  /**
   * Get all configured LLMs from database that can be used in Arena
   */
  async getConfiguredLLMs(): Promise<ConfiguredLLM[]> {
    try {
      console.log('[Service] getConfiguredLLMs called');

      const llmConfigs = await sqliteService.getLLMConfigs();
      const modelConfigs = await sqliteService.getLLMModelConfigs();
      const apiKeys = await sqliteService.getAllApiKeys();

      const result: ConfiguredLLM[] = [];

      // Add models from legacy llm_configs table
      for (const config of llmConfigs) {
        const hasApiKey = !!(config.api_key ||
          (config.provider === 'openai' && apiKeys['OPENAI_API_KEY']) ||
          (config.provider === 'google' && apiKeys['GOOGLE_API_KEY']));

        result.push({
          id: `legacy_${config.provider}`,
          provider: config.provider,
          model_id: config.model,
          display_name: `${config.provider.toUpperCase()}: ${config.model}`,
          api_key_available: hasApiKey
        });
      }

      // Add models from new llm_model_configs table
      for (const config of modelConfigs) {
        result.push({
          id: config.id,
          provider: config.provider,
          model_id: config.model_id,
          display_name: config.display_name || `${config.provider.toUpperCase()}: ${config.model_id}`,
          api_key_available: !!config.api_key
        });
      }

      console.log('[Service] Configured LLMs:', result);
      return result;
    } catch (error) {
      console.error('[Service] getConfiguredLLMs error:', error);
      return [];
    }
  },

  /**
   * Get API keys map for all configured providers
   */
  async getApiKeysMap(): Promise<Record<string, string>> {
    try {
      const apiKeys = await sqliteService.getAllApiKeys();
      const llmConfigs = await sqliteService.getLLMConfigs();
      const modelConfigs = await sqliteService.getLLMModelConfigs();

      const result: Record<string, string> = {};

      // From predefined API keys
      if (apiKeys['OPENAI_API_KEY']) {
        result['OPENAI_API_KEY'] = apiKeys['OPENAI_API_KEY'];
      }
      if (apiKeys['ANTHROPIC_API_KEY']) {
        result['ANTHROPIC_API_KEY'] = apiKeys['ANTHROPIC_API_KEY'];
      }

      // From legacy LLM configs
      for (const config of llmConfigs) {
        if (config.api_key) {
          const keyName = `${config.provider.toUpperCase()}_API_KEY`;
          if (!result[keyName]) {
            result[keyName] = config.api_key;
          }
        }
      }

      // From new model configs
      for (const config of modelConfigs) {
        if (config.api_key) {
          const keyName = `${config.provider.toUpperCase()}_API_KEY`;
          if (!result[keyName]) {
            result[keyName] = config.api_key;
          }
        }
      }

      return result;
    } catch (error) {
      console.error('[Service] getApiKeysMap error:', error);
      return {};
    }
  },

  /**
   * Create a new Alpha Arena competition
   */
  async createCompetition(
    name: string,
    models: CompetitionModel[],
    symbol: string = "BTC/USD",
    mode: string = "baseline",
    initialCapital: number = 10000,
    customPrompt?: string
  ): Promise<CompetitionResult> {
    try {
      console.log('[Service] createCompetition called with:', { name, models, symbol, mode });

      // Validate models
      if (!models || models.length === 0) {
        return {
          success: false,
          error: 'No models provided. Please select at least one AI model.'
        };
      }

      // Fetch API keys from all sources
      console.log('[Service] Fetching API keys from database...');
      const apiKeysMap = await this.getApiKeysMap();

      console.log('[Service] API keys prepared:', Object.keys(apiKeysMap));

      // Build models config with API keys
      const modelsWithKeys = [];
      const missingKeys: string[] = [];

      for (const model of models) {
        const provider = model.provider.toLowerCase();
        const possibleKeys = [
          `${provider}_API_KEY`.toUpperCase(),
          `${provider === 'gemini' ? 'GOOGLE' : provider}_API_KEY`.toUpperCase(),
          `${provider === 'google' ? 'GOOGLE' : provider}_API_KEY`.toUpperCase()
        ];

        let apiKey = '';
        for (const keyName of possibleKeys) {
          if (apiKeysMap[keyName] && apiKeysMap[keyName].trim() !== '') {
            apiKey = apiKeysMap[keyName];
            break;
          }
        }

        if (!apiKey) {
          missingKeys.push(`${model.provider} (for ${model.name})`);
        }

        modelsWithKeys.push({
          name: model.name,
          provider: provider,
          model_id: model.model_id,
          api_key: apiKey
        });
      }

      if (missingKeys.length > 0) {
        return {
          success: false,
          error: `Missing API keys for: ${missingKeys.join(', ')}. Please configure them in Settings → LLM Configuration.`
        };
      }

      console.log('[Service] All required API keys present. Creating competition...');

      // Generate competition ID
      const competitionId = `comp_${Date.now()}_${Math.random().toString(36).substring(7)}`;

      // Build config for new Python service
      const config = {
        competition_id: competitionId,
        competition_name: name,
        models: modelsWithKeys,
        symbols: [symbol],
        initial_capital: initialCapital,
        exchange_id: "kraken",
        custom_prompt: customPrompt || undefined
      };

      const configJson = JSON.stringify(config);
      console.log('[Service] Competition config:', configJson);

      // Add timeout to prevent indefinite hanging
      const CREATE_TIMEOUT = 60000; // 60 seconds
      const competitionPromise = invoke('create_alpha_competition', {
        configJson
      }) as Promise<any>;

      const timeoutPromise = new Promise<any>((_, reject) => {
        setTimeout(() => {
          reject(new Error('Competition creation timed out after 60 seconds. The AI models may be taking too long to initialize. Please try again with fewer models.'));
        }, CREATE_TIMEOUT);
      });

      const result = await Promise.race([competitionPromise, timeoutPromise]);

      console.log('[Service] create_alpha_competition response:', result);

      // If successful, persist to database
      if (result.success) {
        try {
          await sqliteService.createAlphaCompetition({
            id: competitionId,
            name,
            symbol,
            mode: mode as any,
            status: 'created',
            cycle_count: 0,
            cycle_interval_seconds: 150,
            initial_capital: initialCapital
          });

          // Initialize model states in database
          for (const model of models) {
            await sqliteService.saveAlphaModelState({
              id: `${competitionId}_${model.name}`,
              competition_id: competitionId,
              model_name: model.name,
              provider: model.provider,
              model_id: model.model_id,
              capital: initialCapital,
              positions_json: '{}',
              trades_count: 0,
              total_pnl: 0,
              portfolio_value: initialCapital
            });
          }

          console.log('[Service] Competition persisted to database');
        } catch (dbError) {
          console.error('[Service] Failed to persist to database:', dbError);
        }
      }

      return {
        success: result.success,
        data: { competition_id: competitionId, ...result },
        error: result.error
      };
    } catch (error) {
      console.error('[Service] createCompetition error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  },

  /**
   * Run a single competition cycle
   */
  async runCompetitionCycle(competitionId: string): Promise<CompetitionResult> {
    try {
      const result = await invoke('run_alpha_cycle', { competitionId }) as any;

      // Update cycle count in database if successful
      if (result.success && result.cycle_number) {
        await sqliteService.updateAlphaCompetition(competitionId, {
          cycle_count: result.cycle_number
        });
      }

      return {
        success: result.success,
        data: result,
        error: result.error
      };
    } catch (error) {
      console.error('[Service] runCompetitionCycle error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  },

  /**
   * Start competition (runs one cycle immediately)
   */
  async startCompetition(competitionId: string): Promise<CompetitionResult> {
    try {
      console.log('[Service] startCompetition called with ID:', competitionId);

      // Update status to running in database
      await sqliteService.updateAlphaCompetition(competitionId, {
        status: 'running',
        started_at: new Date().toISOString()
      });

      const result = await invoke('start_alpha_competition', { competitionId }) as CompetitionResult;
      console.log('[Service] start_alpha_competition response:', result);

      // Save performance snapshots if available
      if (result.success && result.data?.cycle_result) {
        const cycleNumber = result.data.cycle_result.cycle || 0;

        // Update cycle count
        await sqliteService.updateAlphaCompetition(competitionId, {
          cycle_count: cycleNumber
        });

        // TODO: Save performance snapshots from cycle result
      }

      return result as CompetitionResult;
    } catch (error) {
      console.error('[Service] startCompetition error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  },

  /**
   * Get current leaderboard
   */
  async getLeaderboard(competitionId: string): Promise<LeaderboardEntry[]> {
    try {
      const result: any = await invoke('get_alpha_leaderboard', { competitionId });
      if (result.success && result.leaderboard) {
        // Map backend format to frontend format
        return result.leaderboard.map((entry: any) => ({
          model: entry.model_name || entry.model,
          pnl: entry.total_pnl || 0,
          return_pct: entry.return_pct || 0,
          portfolio_value: entry.portfolio_value || 0,
          cash: entry.cash || 0,
          trades: entry.trades_count || 0,
          positions: entry.positions_count || 0
        }));
      }
      return [];
    } catch (error) {
      console.error('Failed to get leaderboard:', error);
      return [];
    }
  },

  /**
   * Get model decisions/chat feed
   */
  async getModelDecisions(competitionId: string, modelName?: string): Promise<ModelDecision[]> {
    try {
      const result: any = await invoke('get_alpha_model_decisions', {
        competitionId,
        modelName: modelName || null
      });
      if (result.success && result.decisions) {
        // Map backend format to frontend format
        return result.decisions.map((decision: any) => ({
          model: decision.model_name,
          timestamp: new Date(decision.timestamp).toISOString(),
          action: decision.action,
          symbol: decision.symbol,
          confidence: decision.confidence || 0,
          reasoning: decision.reasoning || '',
          quantity: decision.quantity,
          trade_executed: decision.trade_executed
        }));
      }
      return [];
    } catch (error) {
      console.error('Failed to get model decisions:', error);
      return [];
    }
  },

  /**
   * Stop active competition
   */
  async stopCompetition(competitionId: string): Promise<CompetitionResult> {
    try {
      // Update status to paused in database
      await sqliteService.updateAlphaCompetition(competitionId, {
        status: 'paused'
      });

      const result = await invoke('stop_alpha_competition', { competitionId }) as CompetitionResult;
      return result;
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  },

  /**
   * Parse text and remove markdown formatting (** bold, etc.)
   */
  cleanText(text: string): string {
    if (!text) return '';
    // Remove ** bold markers
    text = text.replace(/\*\*/g, '');
    // Remove * italic markers
    text = text.replace(/\*/g, '');
    // Remove ``` code blocks
    text = text.replace(/```[\s\S]*?```/g, '');
    return text.trim();
  },

  /**
   * Parse JSON response, handling markdown code blocks
   */
  parseJsonResponse(response: string): any {
    try {
      let cleaned = response.trim();

      // Remove markdown code blocks
      if (cleaned.startsWith('```')) {
        const parts = cleaned.split('```');
        cleaned = parts[1] || parts[0];
        if (cleaned.startsWith('json')) {
          cleaned = cleaned.substring(4);
        }
        cleaned = cleaned.trim();
      }

      return JSON.parse(cleaned);
    } catch (error) {
      console.error('Failed to parse JSON:', error, response);
      return null;
    }
  }
};
