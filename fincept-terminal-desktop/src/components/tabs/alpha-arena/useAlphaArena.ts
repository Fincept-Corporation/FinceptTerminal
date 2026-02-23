/**
 * useAlphaArena Hook
 *
 * Extracts all state, effects, and callbacks from AlphaArenaTab
 * for cleaner separation of logic and presentation.
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { showConfirm } from '@/utils/notifications';
import { sqliteService, buildApiKeysMap, type LLMGlobalSettings } from '@/services/core/sqliteService';
import { withTimeout } from '@/services/core/apiUtils';
import { validateString } from '@/services/core/validators';
import { useCache, cacheKey } from '@/hooks/useCache';
import alphaArenaService, { type StoredCompetition } from './services/alphaArenaService';
import { useRustTicker } from '@/hooks/useRustWebSocket';
import { useAuth } from '@/contexts/AuthContext';
import type {
  CompetitionStatus,
  LeaderboardEntry,
  ModelDecision,
  PerformanceSnapshot,
  CompetitionMode,
  AgentAdvancedConfig,
  CompetitionType,
  PolymarketMarketInfo,
} from './types';
import { DEFAULT_ADVANCED_CONFIG } from './types';
import polymarketService from '@/services/polymarket/polymarketService';

// Extended model type for Alpha Arena with API key and advanced config
export interface AlphaArenaModel {
  id: string;
  provider: string;
  model_id: string;
  display_name: string;
  api_key?: string;
  is_enabled: boolean;
  advancedConfig: AgentAdvancedConfig;
}

// Progress event type from Rust backend
export interface AlphaArenaProgress {
  event_type: 'step' | 'info' | 'warning' | 'error' | 'complete';
  step: number;
  total_steps: number;
  message: string;
  details?: unknown;
}

// Competition data shape for useCache
interface CompetitionData {
  leaderboard: LeaderboardEntry[];
  decisions: ModelDecision[];
  snapshots: PerformanceSnapshot[];
}

export function useAlphaArena() {
  // Auth context for Fincept API key
  const { session } = useAuth();

  // Competition state
  const [competitionId, setCompetitionId] = useState<string | null>(null);
  const [status, setStatus] = useState<CompetitionStatus | null>(null);
  const [cycleCount, setCycleCount] = useState(0);

  // Data state
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [decisions, setDecisions] = useState<ModelDecision[]>([]);
  const [snapshots, setSnapshots] = useState<PerformanceSnapshot[]>([]);

  // LLM state - from Settings
  const [configuredLLMs, setConfiguredLLMs] = useState<AlphaArenaModel[]>([]);
  const [selectedModels, setSelectedModels] = useState<AlphaArenaModel[]>([]);
  const [llmGlobalSettings, setLlmGlobalSettings] = useState<LLMGlobalSettings>({ temperature: 0.7, max_tokens: 2000, system_prompt: '' });

  // UI state
  const [isLoading, setIsLoading] = useState(false);
  const [isAutoRunning, setIsAutoRunning] = useState(false);
  const [isCancelling, setIsCancelling] = useState(false);
  const [showCreatePanel, setShowCreatePanel] = useState(false);
  const [showPastCompetitions, setShowPastCompetitions] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  // Enhanced features state
  type RightPanelTab = 'decisions' | 'hitl' | 'sentiment' | 'metrics' | 'grid' | 'research' | 'broker';
  const [rightPanelTab, setRightPanelTab] = useState<RightPanelTab>('decisions');
  const [selectedBroker, setSelectedBroker] = useState<string | null>(null);
  const [latestPrice, setLatestPrice] = useState<number | null>(null);

  // Progress tracking state
  const [progressSteps, setProgressSteps] = useState<AlphaArenaProgress[]>([]);
  const [currentProgress, setCurrentProgress] = useState<AlphaArenaProgress | null>(null);
  const progressListenerRef = useRef<UnlistenFn | null>(null);

  // Form state
  const [competitionName, setCompetitionName] = useState('Alpha Arena Competition');
  const [symbol, setSymbol] = useState('BTC/USD');
  const [mode, setMode] = useState<CompetitionMode>('baseline');
  const [initialCapital, setInitialCapital] = useState(10000);
  const [cycleInterval, setCycleInterval] = useState(150);
  const [exchangeId, setExchangeId] = useState('kraken');

  // Competition type state (crypto vs polymarket)
  const [competitionType, setCompetitionType] = useState<CompetitionType>('crypto');

  // Polymarket-specific state
  const [availablePolymarkets, setAvailablePolymarkets] = useState<PolymarketMarketInfo[]>([]);
  const [selectedPolymarkets, setSelectedPolymarkets] = useState<PolymarketMarketInfo[]>([]);
  const [polymarketCategory, setPolymarketCategory] = useState<string>('');
  const [polymarketPrices, setPolymarketPrices] = useState<Record<string, number>>({});
  const [loadingPolymarkets, setLoadingPolymarkets] = useState(false);
  const [polymarketSortBy, setPolymarketSortBy] = useState<'volume' | 'liquidity' | 'end_date' | 'spread'>('volume');
  const [polymarketMinVolume, setPolymarketMinVolume] = useState(0);
  const [polymarketMinLiquidity, setPolymarketMinLiquidity] = useState(0);

  // Component mounted ref for async safety
  const mountedRef = useRef(true);

  // Timeout refs for auto-clearing success/error messages (prevents flickering)
  const successTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const errorTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      if (successTimeoutRef.current) clearTimeout(successTimeoutRef.current);
      if (errorTimeoutRef.current) clearTimeout(errorTimeoutRef.current);
    };
  }, []);

  // Helpers that auto-clear messages and cancel any pending clear timers
  const showSuccess = useCallback((msg: string, duration = 3000) => {
    if (successTimeoutRef.current) clearTimeout(successTimeoutRef.current);
    setSuccess(msg);
    successTimeoutRef.current = setTimeout(() => {
      if (mountedRef.current) setSuccess(null);
      successTimeoutRef.current = null;
    }, duration);
  }, []);

  const showErrorTimed = useCallback((msg: string, duration = 5000) => {
    if (errorTimeoutRef.current) clearTimeout(errorTimeoutRef.current);
    setError(msg);
    errorTimeoutRef.current = setTimeout(() => {
      if (mountedRef.current) setError(null);
      errorTimeoutRef.current = null;
    }, duration);
  }, []);

  // Auto-run ref
  const autoRunIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // Track if a cycle is currently running (to prevent overlapping cycles)
  const cycleInProgressRef = useRef(false);

  // Cancellation flag — when true, in-flight cycles should discard their results
  const cancelledRef = useRef(false);

  // Ref to always hold the latest handleRunCycle to avoid stale closures in setInterval
  const handleRunCycleRef = useRef<() => Promise<void>>(() => Promise.resolve());

  // ===========================================================================
  // Load LLM Models from Settings (llm_model_configs table)
  // ===========================================================================

  const loadLLMConfigs = useCallback(async () => {
    try {
      // Auto-fix any Google model IDs with incorrect format
      await withTimeout(sqliteService.fixGoogleModelIds(), 15000);

      // Load from llm_model_configs table (individual models configured in Settings)
      const modelConfigs = await withTimeout(sqliteService.getLLMModelConfigs(), 15000);
      // Load provider configs for API key fallback and for fincept (which is auto-configured)
      const providerConfigs = await withTimeout(sqliteService.getLLMConfigs(), 15000);

      // Load global LLM settings (temperature, max_tokens, system_prompt)
      try {
        const globalSettings = await withTimeout(sqliteService.getLLMGlobalSettings(), 10000);
        if (mountedRef.current && globalSettings) {
          setLlmGlobalSettings(globalSettings);
        }
      } catch (settingsErr) {
        console.warn('[AlphaArena] Failed to load global LLM settings, using defaults:', settingsErr);
      }

      // Process models - filter and validate
      const validModels: AlphaArenaModel[] = [];

      // First, add fincept from provider configs (it's auto-configured, not in llm_model_configs)
      const finceptProvider = providerConfigs.find(c => (c.provider || '').toLowerCase() === 'fincept');
      const finceptApiKey = session?.api_key || finceptProvider?.api_key;
      if (finceptProvider || finceptApiKey) {
        validModels.push({
          id: 'fincept-auto',
          provider: 'fincept',
          model_id: finceptProvider?.model || 'fincept-llm',
          display_name: 'Fincept LLM (Auto)',
          api_key: finceptApiKey || undefined,
          is_enabled: true,
          advancedConfig: { ...DEFAULT_ADVANCED_CONFIG },
        });
      }

      // Then process models from llm_model_configs table
      for (const model of modelConfigs) {
        if (!model.is_enabled) continue;

        const providerLower = (model.provider || '').toLowerCase();

        // Get API key - try model config first, then provider config
        let apiKey = model.api_key;
        if (!apiKey || apiKey.trim() === '') {
          const providerConfig = providerConfigs.find(
            c => (c.provider || '').toLowerCase() === providerLower
          );
          if (providerConfig?.api_key && providerConfig.api_key.trim() !== '') {
            apiKey = providerConfig.api_key;
          }
        }

        // Skip if no API key (except ollama and fincept which may not need one)
        const noApiKeyRequired = providerLower === 'ollama' || providerLower === 'fincept';
        if ((!apiKey || apiKey.trim() === '') && !noApiKeyRequired) continue;

        validModels.push({
          id: model.id,
          provider: model.provider,
          model_id: model.model_id,
          display_name: model.display_name || model.model_id,
          api_key: apiKey || undefined,
          is_enabled: model.is_enabled,
          advancedConfig: { ...DEFAULT_ADVANCED_CONFIG },
        });
      }

      if (!mountedRef.current) return;
      setConfiguredLLMs(validModels);

      // Pre-select first 2 models
      if (validModels.length >= 2) {
        setSelectedModels(validModels.slice(0, 2));
      } else if (validModels.length === 1) {
        setSelectedModels([validModels[0]]);
      } else {
        setSelectedModels([]);
      }
    } catch (err) {
      if (!mountedRef.current) return;
      console.error('[AlphaArena] Failed to load LLM configs:', err);
    }
  }, [session?.api_key]);

  useEffect(() => {
    loadLLMConfigs();
  }, [loadLLMConfigs]);

  // Reload LLM configs when user changes them in Settings tab
  useEffect(() => {
    const handler = () => { loadLLMConfigs(); };
    window.addEventListener('llm-config-changed', handler);
    return () => { window.removeEventListener('llm-config-changed', handler); };
  }, [loadLLMConfigs]);

  // ===========================================================================
  // Polymarket Markets Loading
  // ===========================================================================

  const loadPolymarkets = useCallback(async () => {
    if (competitionType !== 'polymarket') return;
    setLoadingPolymarkets(true);
    try {
      const result = await withTimeout(
        polymarketService.getMarkets({
          active: true,
          limit: 50,
          tag_id: polymarketCategory || undefined,
        }),
        15000
      );
      if (!mountedRef.current) return;

      // Transform to PolymarketMarketInfo
      let markets: PolymarketMarketInfo[] = (result || []).map((m: any) => {
        let outcomes: string[] = [];
        let outcomePrices: number[] = [];
        let tokenIds: string[] = [];
        try {
          if (typeof m.outcomes === 'string') outcomes = JSON.parse(m.outcomes);
          else if (Array.isArray(m.outcomes)) outcomes = m.outcomes;
        } catch { /* ignore */ }
        try {
          if (typeof m.outcomePrices === 'string') outcomePrices = JSON.parse(m.outcomePrices).map(Number);
          else if (Array.isArray(m.outcomePrices)) outcomePrices = m.outcomePrices.map(Number);
        } catch { /* ignore */ }
        try {
          if (typeof m.clobTokenIds === 'string') tokenIds = JSON.parse(m.clobTokenIds);
          else if (Array.isArray(m.clobTokenIds)) tokenIds = m.clobTokenIds;
        } catch { /* ignore */ }

        return {
          id: m.id,
          question: m.question,
          description: m.description,
          outcomes,
          outcome_prices: outcomePrices,
          token_ids: tokenIds,
          volume: m.volumeNum ?? m.volume ?? 0,
          liquidity: m.liquidityNum ?? m.liquidity ?? 0,
          end_date: m.endDate,
          image: m.image,
          category: m.tags?.[0]?.label,
        };
      });

      // Apply volume/liquidity filters
      if (polymarketMinVolume > 0) {
        markets = markets.filter(m => m.volume >= polymarketMinVolume);
      }
      if (polymarketMinLiquidity > 0) {
        markets = markets.filter(m => m.liquidity >= polymarketMinLiquidity);
      }

      // Sort
      markets.sort((a, b) => {
        switch (polymarketSortBy) {
          case 'volume': return b.volume - a.volume;
          case 'liquidity': return b.liquidity - a.liquidity;
          case 'end_date': {
            if (!a.end_date) return 1;
            if (!b.end_date) return -1;
            return new Date(a.end_date).getTime() - new Date(b.end_date).getTime();
          }
          case 'spread': {
            const spreadA = Math.abs(0.5 - (a.outcome_prices[0] || 0.5));
            const spreadB = Math.abs(0.5 - (b.outcome_prices[0] || 0.5));
            return spreadB - spreadA; // Highest spread first (most decisive markets)
          }
          default: return 0;
        }
      });

      setAvailablePolymarkets(markets);
    } catch (err) {
      console.error('[AlphaArena] Failed to load Polymarket markets:', err);
      if (mountedRef.current) setAvailablePolymarkets([]);
    } finally {
      if (mountedRef.current) setLoadingPolymarkets(false);
    }
  }, [competitionType, polymarketCategory, polymarketSortBy, polymarketMinVolume, polymarketMinLiquidity]);

  useEffect(() => {
    loadPolymarkets();
  }, [loadPolymarkets]);

  const togglePolymarketSelection = useCallback((market: PolymarketMarketInfo) => {
    setSelectedPolymarkets(prev => {
      const exists = prev.some(m => m.id === market.id);
      if (exists) return prev.filter(m => m.id !== market.id);
      return [...prev, market];
    });
  }, []);

  // ===========================================================================
  // Past Competitions (useCache)
  // ===========================================================================

  const pastCompetitionsCache = useCache<StoredCompetition[]>({
    key: cacheKey('alpha-arena', 'past-competitions'),
    category: 'alpha-arena',
    fetcher: async () => {
      const result = await alphaArenaService.listCompetitions(20);
      if (result.success && result.competitions) return result.competitions;
      return [];
    },
    ttl: 120,
    staleWhileRevalidate: true,
  });

  const pastCompetitions = pastCompetitionsCache.data || [];
  const loadingPastCompetitions = pastCompetitionsCache.isLoading;

  const handleResumeCompetition = async (comp: StoredCompetition) => {
    setIsLoading(true);
    setError(null);
    setProgressSteps([]);

    try {
      setCompetitionId(comp.competition_id);
      setCycleCount(comp.cycle_count);
      setStatus(comp.status as CompetitionStatus);
      setShowPastCompetitions(false);

      // Restore selected models from competition config so API keys are available for cycles
      if (comp.config.models && comp.config.models.length > 0) {
        const restoredModels: AlphaArenaModel[] = comp.config.models.map((m, idx) => ({
          id: m.model_id ? `${m.provider}-${m.model_id}-${idx}` : `restored-${idx}`,
          provider: m.provider,
          model_id: m.model_id,
          display_name: m.name,
          api_key: m.api_key,
          is_enabled: true,
          advancedConfig: { ...DEFAULT_ADVANCED_CONFIG },
        }));
        setSelectedModels(restoredModels);
      }

      // Restore form state from competition config
      if (comp.config.symbols?.length > 0) {
        setSymbol(comp.config.symbols[0]);
      }
      if (comp.config.initial_capital) {
        setInitialCapital(comp.config.initial_capital);
      }
      if (comp.config.cycle_interval_seconds) {
        setCycleInterval(comp.config.cycle_interval_seconds);
      }
      if (comp.config.exchange_id) {
        setExchangeId(comp.config.exchange_id);
      }

      // Restore competition type and polymarket markets if applicable
      if (comp.config.competition_type) {
        setCompetitionType(comp.config.competition_type);
      }
      if (comp.config.polymarket_markets && comp.config.polymarket_markets.length > 0) {
        setSelectedPolymarkets(comp.config.polymarket_markets);
      }

      showSuccess(`Loaded competition: ${comp.config.competition_name}`);
    } catch (err) {
      if (!mountedRef.current) return;
      setError(err instanceof Error ? err.message : 'Failed to resume competition');
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  };

  const handleDeleteCompetition = async (targetId: string, e: React.MouseEvent) => {
    e.stopPropagation();

    const confirmed = await showConfirm(
      'This action cannot be undone.',
      { title: 'Delete this competition?', type: 'danger' }
    );
    if (!confirmed) return;

    try {
      const result = await alphaArenaService.deleteCompetition(targetId);
      if (!mountedRef.current) return;
      if (result.success) {
        pastCompetitionsCache.invalidate();
        if (competitionId === targetId) {
          setCompetitionId(null);
          setStatus(null);
          setCycleCount(0);
          setLeaderboard([]);
          setDecisions([]);
          setSnapshots([]);
        }
        showSuccess('Competition deleted');
      } else {
        setError(result.error || 'Failed to delete competition');
      }
    } catch (err) {
      if (!mountedRef.current) return;
      setError(err instanceof Error ? err.message : 'Failed to delete competition');
    }
  };

  // ===========================================================================
  // Progress Event Listener
  // ===========================================================================

  useEffect(() => {
    const setupListener = async () => {
      progressListenerRef.current = await listen<AlphaArenaProgress>(
        'alpha-arena-progress',
        (event) => {
          const progress = event.payload;
          setCurrentProgress(progress);
          setProgressSteps(prev => {
            if (prev.some(p => p.step === progress.step && p.message === progress.message)) return prev;
            return [...prev, progress];
          });

          if (progress.event_type === 'complete') {
            setTimeout(() => {
              setCurrentProgress(null);
              setProgressSteps([]);
            }, 2000);
          }

          if (progress.event_type === 'error') {
            setError(progress.message);
          }
        }
      );
    };

    setupListener();

    return () => {
      if (progressListenerRef.current) progressListenerRef.current();
    };
  }, []);

  // ===========================================================================
  // Competition Data (useCache with auto-refresh)
  // ===========================================================================

  const compDataCache = useCache<CompetitionData>({
    key: cacheKey('alpha-arena', 'comp-data', competitionId || 'none'),
    category: 'alpha-arena',
    fetcher: async () => {
      const targetId = competitionId;
      if (!targetId) return { leaderboard: [], decisions: [], snapshots: [] };

      const [leaderboardResult, decisionsResult, snapshotsResult] = await Promise.all([
        alphaArenaService.getLeaderboard(targetId),
        alphaArenaService.getDecisions(targetId),
        alphaArenaService.getSnapshots(targetId),
      ]);

      return {
        leaderboard: leaderboardResult.success && leaderboardResult.leaderboard
          ? leaderboardResult.leaderboard : [],
        decisions: decisionsResult.success && decisionsResult.decisions
          ? decisionsResult.decisions : [],
        snapshots: snapshotsResult.success && snapshotsResult.snapshots
          ? snapshotsResult.snapshots : [],
      };
    },
    ttl: 30,
    enabled: !!competitionId,
    staleWhileRevalidate: true,
    refetchInterval: 15000,
  });

  // Sync cache data to local state
  useEffect(() => {
    if (compDataCache.data) {
      setLeaderboard(compDataCache.data.leaderboard);
      setDecisions(compDataCache.data.decisions);
      setSnapshots(compDataCache.data.snapshots);
    }
  }, [compDataCache.data]);

  const refreshCompetitionData = useCallback(() => {
    compDataCache.refresh();
  }, [compDataCache]);

  // ===========================================================================
  // Competition Actions
  // ===========================================================================

  const handleCreateCompetition = async () => {
    const nameCheck = validateString(competitionName, { minLength: 1, maxLength: 200 });
    if (!nameCheck.valid) {
      setError(`Competition name: ${nameCheck.error}`);
      return;
    }
    if (selectedModels.length < 2) {
      setError('Select at least 2 AI models from your configured models in Settings');
      return;
    }
    if (!Number.isFinite(initialCapital) || initialCapital < 100) {
      setError('Initial capital must be at least $100');
      return;
    }
    if (!Number.isFinite(cycleInterval) || cycleInterval < 10) {
      setError('Cycle interval must be at least 10 seconds');
      return;
    }
    // Polymarket validation
    if (competitionType === 'polymarket' && selectedPolymarkets.length < 1) {
      setError('Select at least 1 prediction market for Polymarket competition');
      return;
    }

    setIsLoading(true);
    setError(null);
    setProgressSteps([]);
    setCurrentProgress(null);

    try {
      const result = await alphaArenaService.createCompetition({
        competition_name: competitionName,
        models: selectedModels.map(m => ({
          name: m.display_name,
          provider: m.provider,
          model_id: m.model_id,
          api_key: m.api_key,
          initial_capital: initialCapital,
          trading_style: m.advancedConfig.trading_style || undefined,
          advanced_config: m.advancedConfig,
        })),
        symbols: competitionType === 'crypto' ? [symbol] : undefined,
        initial_capital: initialCapital,
        mode,
        cycle_interval_seconds: cycleInterval,
        exchange_id: competitionType === 'crypto' ? exchangeId : undefined,
        llm_settings: {
          temperature: llmGlobalSettings.temperature,
          max_tokens: llmGlobalSettings.max_tokens,
          system_prompt: llmGlobalSettings.system_prompt,
        },
        // Polymarket-specific params
        competition_type: competitionType,
        polymarket_market_ids: competitionType === 'polymarket'
          ? selectedPolymarkets.map(m => m.id)
          : undefined,
        polymarket_category: competitionType === 'polymarket'
          ? polymarketCategory || undefined
          : undefined,
      });

      if (!mountedRef.current) return;
      if (result.success && result.competition_id) {
        setCompetitionId(result.competition_id);
        setStatus('created');
        setShowCreatePanel(false);
        showSuccess('Competition created successfully');
      } else {
        setError(result.error || 'Failed to create competition');
      }
    } catch (err) {
      if (!mountedRef.current) return;
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  };

  const handleRunCycle = useCallback(async () => {
    if (!competitionId) return;
    if (cycleInProgressRef.current) return;

    cycleInProgressRef.current = true;
    cancelledRef.current = false;
    setIsLoading(true);
    setError(null);
    setProgressSteps([]);
    setCurrentProgress(null);

    try {
      const currentCompetitionId = competitionId;
      const apiKeys = await buildApiKeysMap();
      const result = await alphaArenaService.runCycle(currentCompetitionId, Object.keys(apiKeys).length > 0 ? apiKeys : undefined);

      // If cancelled or unmounted while awaiting, discard results
      if (!mountedRef.current || cancelledRef.current) return;

      if (result.success) {
        setCycleCount(prev => result.cycle_number || prev + 1);
        setStatus('running');
        // Update leaderboard directly from cycle result if available
        if (result.leaderboard && result.leaderboard.length > 0) {
          setLeaderboard(result.leaderboard);
        }
        // Fetch fresh decisions and snapshots immediately (not from cache)
        try {
          const [decisionsResult, snapshotsResult] = await Promise.all([
            alphaArenaService.getDecisions(currentCompetitionId),
            alphaArenaService.getSnapshots(currentCompetitionId),
          ]);
          if (mountedRef.current && !cancelledRef.current) {
            if (decisionsResult.success && decisionsResult.decisions) {
              setDecisions(decisionsResult.decisions);
            }
            if (snapshotsResult.success && snapshotsResult.snapshots) {
              setSnapshots(snapshotsResult.snapshots);
            }
          }
        } catch {
          // Fallback to cache refresh
        }
        if (!cancelledRef.current) {
          refreshCompetitionData();
        }
        if (result.warnings && result.warnings.length > 0 && !cancelledRef.current) {
          showErrorTimed(`Cycle completed with warnings: ${result.warnings[0]}`);
        }
      } else {
        if (!cancelledRef.current) {
          setError(result.error || 'Cycle failed');
        }
      }
    } catch (err) {
      if (!mountedRef.current || cancelledRef.current) return;
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      cycleInProgressRef.current = false;
      if (mountedRef.current && !cancelledRef.current) setIsLoading(false);
    }
  }, [competitionId, selectedModels, refreshCompetitionData]);

  // Keep ref in sync so setInterval always calls the latest version
  useEffect(() => {
    handleRunCycleRef.current = handleRunCycle;
  }, [handleRunCycle]);

  const handleToggleAutoRun = useCallback(() => {
    if (isAutoRunning) {
      if (autoRunIntervalRef.current) {
        clearInterval(autoRunIntervalRef.current);
        autoRunIntervalRef.current = null;
      }
      setIsAutoRunning(false);
      setStatus('paused');
    } else {
      setIsAutoRunning(true);
      setStatus('running');
      handleRunCycleRef.current();
      autoRunIntervalRef.current = setInterval(() => {
        handleRunCycleRef.current();
      }, cycleInterval * 1000);
    }
  }, [isAutoRunning, cycleInterval]);

  const handleReset = useCallback(() => {
    // Signal any in-flight cycle to discard its results
    cancelledRef.current = true;
    if (autoRunIntervalRef.current) {
      clearInterval(autoRunIntervalRef.current);
      autoRunIntervalRef.current = null;
    }
    cycleInProgressRef.current = false;
    setCompetitionId(null);
    setStatus(null);
    setCycleCount(0);
    setLeaderboard([]);
    setDecisions([]);
    setSnapshots([]);
    setIsAutoRunning(false);
    setIsLoading(false);
    setIsCancelling(false);
    setError(null);
    setProgressSteps([]);
    setCurrentProgress(null);
  }, []);

  const handleCancel = useCallback(() => {
    setIsCancelling(true);
    // Signal in-flight cycle to discard results when it completes
    cancelledRef.current = true;
    if (autoRunIntervalRef.current) {
      clearInterval(autoRunIntervalRef.current);
      autoRunIntervalRef.current = null;
      setIsAutoRunning(false);
    }
    // Note: cycleInProgressRef stays true until the actual async operation finishes,
    // but cancelledRef ensures state won't be updated from stale results
    setIsLoading(false);
    setIsCancelling(false);
    setProgressSteps([]);
    setCurrentProgress(null);
    setStatus('paused');
    showSuccess('Operation cancelled', 2000);
  }, [showSuccess]);

  // Cleanup auto-run on unmount
  useEffect(() => {
    return () => {
      if (autoRunIntervalRef.current) clearInterval(autoRunIntervalRef.current);
    };
  }, []);

  // Real-time price streaming via Rust WebSocket — uses competition's configured exchange (crypto only)
  const tickerHook = useRustTicker(
    exchangeId,
    symbol,
    !!competitionId && competitionType === 'crypto'
  );

  useEffect(() => {
    if (tickerHook.data && mountedRef.current && competitionType === 'crypto') {
      setLatestPrice(tickerHook.data.price);
    }
  }, [tickerHook.data, competitionType]);

  const toggleModelSelection = (model: AlphaArenaModel) => {
    setSelectedModels(prev => {
      const isSelected = prev.some(m => m.id === model.id);
      if (isSelected) return prev.filter(m => m.id !== model.id);
      return [...prev, { ...model }];
    });
  };

  const updateModelConfig = useCallback((modelId: string, config: Partial<AgentAdvancedConfig>) => {
    setSelectedModels(prev => prev.map(m =>
      m.id === modelId ? { ...m, advancedConfig: { ...m.advancedConfig, ...config } } : m
    ));
  }, []);

  return {
    // Competition state
    competitionId,
    status,
    cycleCount,

    // Data
    leaderboard,
    decisions,
    snapshots,

    // LLM state
    configuredLLMs,
    selectedModels,
    llmGlobalSettings,

    // UI state
    isLoading,
    isAutoRunning,
    isCancelling,
    showCreatePanel,
    setShowCreatePanel,
    showPastCompetitions,
    setShowPastCompetitions,
    error,
    setError,
    success,
    setSuccess,

    // Enhanced features
    rightPanelTab,
    setRightPanelTab,
    selectedBroker,
    setSelectedBroker,
    latestPrice,

    // Progress
    progressSteps,
    currentProgress,

    // Form state
    competitionName,
    setCompetitionName,
    symbol,
    setSymbol,
    mode,
    setMode,
    initialCapital,
    setInitialCapital,
    cycleInterval,
    setCycleInterval,
    exchangeId,
    setExchangeId,

    // Competition type (crypto vs polymarket)
    competitionType,
    setCompetitionType,

    // Polymarket state
    availablePolymarkets,
    selectedPolymarkets,
    setSelectedPolymarkets,
    polymarketCategory,
    setPolymarketCategory,
    polymarketPrices,
    loadingPolymarkets,
    loadPolymarkets,
    togglePolymarketSelection,
    polymarketSortBy,
    setPolymarketSortBy,
    polymarketMinVolume,
    setPolymarketMinVolume,
    polymarketMinLiquidity,
    setPolymarketMinLiquidity,

    // Past competitions
    pastCompetitions,
    loadingPastCompetitions,
    pastCompetitionsCache,

    // Actions
    loadLLMConfigs,
    handleCreateCompetition,
    handleRunCycle,
    handleToggleAutoRun,
    handleReset,
    handleCancel,
    handleResumeCompetition,
    handleDeleteCompetition,
    toggleModelSelection,
    updateModelConfig,
    refreshCompetitionData,
  };
}

export type RightPanelTab = 'decisions' | 'hitl' | 'sentiment' | 'metrics' | 'grid' | 'research' | 'broker';
