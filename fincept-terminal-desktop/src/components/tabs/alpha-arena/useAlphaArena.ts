/**
 * useAlphaArena Hook
 *
 * Extracts all state, effects, and callbacks from AlphaArenaTab
 * for cleaner separation of logic and presentation.
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { showConfirm } from '@/utils/notifications';
import { sqliteService, type LLMGlobalSettings } from '@/services/core/sqliteService';
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
} from './types';

// Extended model type for Alpha Arena with API key
export interface AlphaArenaModel {
  id: string;
  provider: string;
  model_id: string;
  display_name: string;
  api_key?: string;
  is_enabled: boolean;
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

  // Component mounted ref for async safety
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Auto-run ref
  const autoRunIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // Track if a cycle is currently running (to prevent overlapping cycles)
  const cycleInProgressRef = useRef(false);

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
      setSuccess(`Loaded competition: ${comp.config.competition_name}`);
      setTimeout(() => { if (mountedRef.current) setSuccess(null); }, 3000);
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
        setSuccess('Competition deleted');
        setTimeout(() => { if (mountedRef.current) setSuccess(null); }, 3000);
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
        })),
        symbols: [symbol],
        initial_capital: initialCapital,
        mode,
        cycle_interval_seconds: cycleInterval,
        llm_settings: {
          temperature: llmGlobalSettings.temperature,
          max_tokens: llmGlobalSettings.max_tokens,
          system_prompt: llmGlobalSettings.system_prompt,
        },
      });

      if (!mountedRef.current) return;
      if (result.success && result.competition_id) {
        setCompetitionId(result.competition_id);
        setStatus('created');
        setShowCreatePanel(false);
        setSuccess('Competition created successfully');
        setTimeout(() => { if (mountedRef.current) setSuccess(null); }, 3000);
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
    setIsLoading(true);
    setError(null);
    setProgressSteps([]);
    setCurrentProgress(null);

    try {
      const currentCompetitionId = competitionId;
      const currentModels = [...selectedModels];
      const apiKeys: Record<string, string> = {};
      for (const model of currentModels) {
        if (model.api_key && !apiKeys[model.provider]) {
          apiKeys[model.provider] = model.api_key;
        }
      }
      const result = await alphaArenaService.runCycle(currentCompetitionId, Object.keys(apiKeys).length > 0 ? apiKeys : undefined);

      if (!mountedRef.current) return;
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
          if (mountedRef.current) {
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
        refreshCompetitionData();
        if (result.warnings && result.warnings.length > 0) {
          setError(`Cycle completed with warnings: ${result.warnings[0]}`);
          setTimeout(() => { if (mountedRef.current) setError(null); }, 5000);
        }
      } else {
        setError(result.error || 'Cycle failed');
      }
    } catch (err) {
      if (!mountedRef.current) return;
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      cycleInProgressRef.current = false;
      if (mountedRef.current) setIsLoading(false);
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
    if (autoRunIntervalRef.current) {
      clearInterval(autoRunIntervalRef.current);
      autoRunIntervalRef.current = null;
      setIsAutoRunning(false);
    }
    cycleInProgressRef.current = false;
    setIsLoading(false);
    setIsCancelling(false);
    setProgressSteps([]);
    setCurrentProgress(null);
    setStatus('paused');
    setSuccess('Operation cancelled');
    setTimeout(() => setSuccess(null), 2000);
  }, []);

  // Cleanup auto-run on unmount
  useEffect(() => {
    return () => {
      if (autoRunIntervalRef.current) clearInterval(autoRunIntervalRef.current);
    };
  }, []);

  // Real-time price streaming via Rust WebSocket (Kraken ticker)
  const tickerHook = useRustTicker('kraken', symbol, !!competitionId);

  useEffect(() => {
    if (tickerHook.data && mountedRef.current) {
      setLatestPrice(tickerHook.data.price);
    }
  }, [tickerHook.data]);

  const toggleModelSelection = (model: AlphaArenaModel) => {
    setSelectedModels(prev => {
      const isSelected = prev.some(m => m.id === model.id);
      if (isSelected) return prev.filter(m => m.id !== model.id);
      return [...prev, model];
    });
  };

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
    refreshCompetitionData,
  };
}

export type RightPanelTab = 'decisions' | 'hitl' | 'sentiment' | 'metrics' | 'grid' | 'research' | 'broker';
