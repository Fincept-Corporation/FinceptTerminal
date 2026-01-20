/**
 * Alpha Arena Tab - Fincept Terminal Design (v2.1)
 *
 * AI Trading Competition Platform with:
 * - LLM configuration from Settings
 * - Professional terminal-style UI
 * - Real-time leaderboard & performance charts
 * - Repository integration for trade decisions
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import {
  Bot, Trophy, Play, Pause, RotateCcw, Plus, Settings, Key,
  TrendingUp, TrendingDown, Activity, Zap, AlertCircle, CheckCircle,
  ChevronRight, Loader2, BarChart3, Clock, Target, Brain,
  RefreshCw, Eye, Users, Sparkles, Check, Circle, History, Trash2, PlayCircle,
  StopCircle, XCircle,
} from 'lucide-react';
import { TabFooter } from '@/components/common/TabFooter';
import { sqliteService, type LLMModelConfig, type LLMConfig } from '@/services/core/sqliteService';
import alphaArenaService, { type StoredCompetition } from './services/alphaArenaService';
import { useAuth } from '@/contexts/AuthContext';

// Extended model type for Alpha Arena with API key
interface AlphaArenaModel {
  id: string;
  provider: string;
  model_id: string;
  display_name: string;
  api_key?: string;
  is_enabled: boolean;
}

// Progress event type from Rust backend
interface AlphaArenaProgress {
  event_type: 'step' | 'info' | 'warning' | 'error' | 'complete';
  step: number;
  total_steps: number;
  message: string;
  details?: unknown;
}
import type {
  CompetitionStatus,
  LeaderboardEntry,
  ModelDecision,
  PerformanceSnapshot,
  CompetitionMode,
} from './types';
import { formatCurrency, formatPercent, getModelColor } from './types';

// =============================================================================
// Design Constants - Fincept Terminal Style
// =============================================================================

const COLORS = {
  ORANGE: '#FF8800',
  ORANGE_HOVER: '#FF9900',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#3B82F6',
  PURPLE: '#8B5CF6',
  YELLOW: '#EAB308',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  BORDER_LIGHT: '#3A3A3A',
  HOVER: '#1A1A1A',
  MUTED: '#4A4A4A',
};

const TRADING_SYMBOLS = [
  { value: 'BTC/USD', label: 'Bitcoin (BTC/USD)' },
  { value: 'ETH/USD', label: 'Ethereum (ETH/USD)' },
  { value: 'SOL/USD', label: 'Solana (SOL/USD)' },
  { value: 'XRP/USD', label: 'Ripple (XRP/USD)' },
  { value: 'DOGE/USD', label: 'Dogecoin (DOGE/USD)' },
];

const COMPETITION_MODES: { value: CompetitionMode; label: string; desc: string }[] = [
  { value: 'baseline', label: 'Baseline', desc: 'Standard trading with balanced risk' },
  { value: 'monk', label: 'Monk', desc: 'Conservative - capital preservation' },
  { value: 'situational', label: 'Situational', desc: 'Competitive - models aware of others' },
  { value: 'max_leverage', label: 'Max Leverage', desc: 'Aggressive - maximum positions' },
];

// =============================================================================
// Component
// =============================================================================

const AlphaArenaTab: React.FC = () => {
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

  // UI state
  const [isLoading, setIsLoading] = useState(false);
  const [isAutoRunning, setIsAutoRunning] = useState(false);
  const [isCancelling, setIsCancelling] = useState(false);
  const [showCreatePanel, setShowCreatePanel] = useState(false);
  const [showPastCompetitions, setShowPastCompetitions] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  // Abort controller for cancelling operations
  const abortControllerRef = useRef<AbortController | null>(null);

  // Past competitions state
  const [pastCompetitions, setPastCompetitions] = useState<StoredCompetition[]>([]);
  const [loadingPastCompetitions, setLoadingPastCompetitions] = useState(false);

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

  // Auto-run ref
  const autoRunIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // =============================================================================
  // Load LLM Models from Settings (llm_model_configs table)
  // =============================================================================

  const loadLLMConfigs = useCallback(async () => {
    try {
      // Auto-fix any Google model IDs with incorrect format
      const fixResult = await sqliteService.fixGoogleModelIds();
      if (fixResult.success) {
        console.log('[AlphaArena] Google model IDs check:', fixResult.message);
      }

      // Load from llm_model_configs table (individual models configured in Settings)
      const modelConfigs = await sqliteService.getLLMModelConfigs();
      // Load provider configs for API key fallback and for fincept (which is auto-configured)
      const providerConfigs = await sqliteService.getLLMConfigs();

      console.log('[AlphaArena] Loaded model configs:', modelConfigs);
      console.log('[AlphaArena] Loaded provider configs:', providerConfigs);

      // Process models - filter and validate
      const validModels: AlphaArenaModel[] = [];

      // First, add fincept from provider configs (it's auto-configured, not in llm_model_configs)
      // Use session API key for fincept (the same key used for all Fincept backend APIs)
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
        console.log('[AlphaArena] Added Fincept LLM (auto-configured) with API key:', finceptApiKey ? 'present' : 'missing');
      }

      // Then process models from llm_model_configs table
      for (const model of modelConfigs) {
        // Skip disabled models
        if (!model.is_enabled) {
          console.log(`[AlphaArena] Skipping ${model.display_name} - disabled`);
          continue;
        }

        const providerLower = (model.provider || '').toLowerCase();

        // Get API key - try model config first, then provider config
        let apiKey = model.api_key;
        if (!apiKey || apiKey.trim() === '') {
          const providerConfig = providerConfigs.find(
            c => (c.provider || '').toLowerCase() === providerLower
          );
          if (providerConfig?.api_key && providerConfig.api_key.trim() !== '') {
            apiKey = providerConfig.api_key;
            console.log(`[AlphaArena] Using provider API key for ${model.display_name}`);
          }
        }

        // Skip if no API key (except ollama and fincept which may not need one in the same way)
        const noApiKeyRequired = providerLower === 'ollama' || providerLower === 'fincept';
        if ((!apiKey || apiKey.trim() === '') && !noApiKeyRequired) {
          console.log(`[AlphaArena] Skipping ${model.display_name} - no API key`);
          continue;
        }

        validModels.push({
          id: model.id,
          provider: model.provider,
          model_id: model.model_id,
          display_name: model.display_name || model.model_id,
          api_key: apiKey || undefined,
          is_enabled: model.is_enabled,
        });

        console.log(`[AlphaArena] Added model: ${model.display_name}`);
      }

      console.log(`[AlphaArena] Total valid models: ${validModels.length}`);
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
      console.error('[AlphaArena] Failed to load LLM configs:', err);
    }
  }, [session?.api_key]);

  useEffect(() => {
    loadLLMConfigs();
  }, [loadLLMConfigs]);

  // =============================================================================
  // Load Past Competitions from Database
  // =============================================================================

  const loadPastCompetitions = useCallback(async () => {
    setLoadingPastCompetitions(true);
    try {
      const result = await alphaArenaService.listCompetitions(20);
      if (result.success && result.competitions) {
        setPastCompetitions(result.competitions);
      }
    } catch (err) {
      console.error('[AlphaArena] Failed to load past competitions:', err);
    } finally {
      setLoadingPastCompetitions(false);
    }
  }, []);

  // Load past competitions on mount
  useEffect(() => {
    loadPastCompetitions();
  }, [loadPastCompetitions]);

  // Resume a past competition
  const handleResumeCompetition = async (comp: StoredCompetition) => {
    setIsLoading(true);
    setError(null);
    setProgressSteps([]);

    try {
      // Set the competition ID - the backend will restore from DB when we run a cycle
      setCompetitionId(comp.competition_id);
      setCycleCount(comp.cycle_count);
      setStatus(comp.status as CompetitionStatus);
      setShowPastCompetitions(false);
      setSuccess(`Loaded competition: ${comp.config.competition_name}`);
      setTimeout(() => setSuccess(null), 3000);

      // Fetch competition data - pass ID directly since state hasn't updated yet
      await fetchCompetitionData(comp.competition_id);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to resume competition');
    } finally {
      setIsLoading(false);
    }
  };

  // Delete a competition
  const handleDeleteCompetition = async (competitionId: string, e: React.MouseEvent) => {
    e.stopPropagation();

    if (!confirm('Are you sure you want to delete this competition? This cannot be undone.')) {
      return;
    }

    try {
      const result = await alphaArenaService.deleteCompetition(competitionId);
      if (result.success) {
        setPastCompetitions(prev => prev.filter(c => c.competition_id !== competitionId));
        setSuccess('Competition deleted');
        setTimeout(() => setSuccess(null), 3000);
      } else {
        setError(result.error || 'Failed to delete competition');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to delete competition');
    }
  };

  // =============================================================================
  // Progress Event Listener
  // =============================================================================

  useEffect(() => {
    // Listen for progress events from Rust backend
    const setupListener = async () => {
      progressListenerRef.current = await listen<AlphaArenaProgress>(
        'alpha-arena-progress',
        (event) => {
          const progress = event.payload;
          console.log('[AlphaArena] Progress:', progress);

          setCurrentProgress(progress);
          setProgressSteps(prev => {
            // Avoid duplicates
            if (prev.some(p => p.step === progress.step && p.message === progress.message)) {
              return prev;
            }
            return [...prev, progress];
          });

          // Handle completion
          if (progress.event_type === 'complete') {
            setTimeout(() => {
              setCurrentProgress(null);
              setProgressSteps([]);
            }, 2000);
          }

          // Handle errors
          if (progress.event_type === 'error') {
            setError(progress.message);
          }
        }
      );
    };

    setupListener();

    return () => {
      if (progressListenerRef.current) {
        progressListenerRef.current();
      }
    };
  }, []);

  // =============================================================================
  // Competition Data
  // =============================================================================

  const fetchCompetitionData = useCallback(async (overrideCompetitionId?: string) => {
    const targetCompetitionId = overrideCompetitionId || competitionId;
    if (!targetCompetitionId) return;

    try {
      console.log('[AlphaArena] Fetching competition data for:', targetCompetitionId);
      const [leaderboardResult, decisionsResult, snapshotsResult] = await Promise.all([
        alphaArenaService.getLeaderboard(targetCompetitionId),
        alphaArenaService.getDecisions(targetCompetitionId),
        alphaArenaService.getSnapshots(targetCompetitionId),
      ]);

      console.log('[AlphaArena] Leaderboard result:', leaderboardResult);
      console.log('[AlphaArena] Decisions result:', decisionsResult);
      console.log('[AlphaArena] Snapshots result:', snapshotsResult);

      if (leaderboardResult.success && leaderboardResult.leaderboard) {
        console.log('[AlphaArena] Setting leaderboard:', leaderboardResult.leaderboard.length, 'entries');
        setLeaderboard(leaderboardResult.leaderboard);
      } else {
        console.warn('[AlphaArena] Leaderboard not available:', leaderboardResult.error || 'no data');
      }
      if (decisionsResult.success && decisionsResult.decisions) {
        console.log('[AlphaArena] Setting decisions:', decisionsResult.decisions.length, 'entries');
        setDecisions(decisionsResult.decisions);
      } else {
        console.warn('[AlphaArena] Decisions not available:', decisionsResult.error || 'no data');
      }
      if (snapshotsResult.success && snapshotsResult.snapshots) {
        console.log('[AlphaArena] Setting snapshots:', snapshotsResult.snapshots.length, 'entries');
        setSnapshots(snapshotsResult.snapshots);
      } else {
        console.warn('[AlphaArena] Snapshots not available:', snapshotsResult.error || 'no data');
      }
    } catch (err) {
      console.error('[AlphaArena] Failed to fetch data:', err);
    }
  }, [competitionId]);

  useEffect(() => {
    if (!competitionId) return;
    fetchCompetitionData();
    const interval = setInterval(fetchCompetitionData, 5000);
    return () => clearInterval(interval);
  }, [competitionId, fetchCompetitionData]);

  // =============================================================================
  // Competition Actions
  // =============================================================================

  const handleCreateCompetition = async () => {
    if (selectedModels.length < 2) {
      setError('Select at least 2 AI models from your configured models in Settings');
      return;
    }

    setIsLoading(true);
    setError(null);
    setProgressSteps([]); // Reset progress
    setCurrentProgress(null);

    try {
      const result = await alphaArenaService.createCompetition({
        competition_name: competitionName,
        models: selectedModels.map(m => ({
          name: m.display_name,
          provider: m.provider,
          model_id: m.model_id,
          api_key: m.api_key, // Include API key from model config
          initial_capital: initialCapital,
        })),
        symbols: [symbol],
        initial_capital: initialCapital,
        mode,
        cycle_interval_seconds: cycleInterval,
      });

      if (result.success && result.competition_id) {
        setCompetitionId(result.competition_id);
        setStatus('created');
        setShowCreatePanel(false);
        setSuccess('Competition created successfully');
        setTimeout(() => setSuccess(null), 3000);
      } else {
        setError(result.error || 'Failed to create competition');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  const handleRunCycle = useCallback(async () => {
    if (!competitionId || isLoading) return;

    setIsLoading(true);
    setError(null);
    setProgressSteps([]); // Reset progress
    setCurrentProgress(null);

    try {
      const currentCompetitionId = competitionId; // Capture before async
      const result = await alphaArenaService.runCycle(currentCompetitionId);

      if (result.success) {
        setCycleCount(result.cycle_number || cycleCount + 1);
        setStatus('running');
        // Pass ID explicitly to avoid stale closure issues
        await fetchCompetitionData(currentCompetitionId);
      } else {
        setError(result.error || 'Cycle failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  }, [competitionId, isLoading, cycleCount, fetchCompetitionData]);

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
      handleRunCycle();
      autoRunIntervalRef.current = setInterval(() => {
        handleRunCycle();
      }, cycleInterval * 1000);
    }
  }, [isAutoRunning, handleRunCycle, cycleInterval]);

  const handleReset = useCallback(() => {
    if (autoRunIntervalRef.current) {
      clearInterval(autoRunIntervalRef.current);
      autoRunIntervalRef.current = null;
    }
    // Cancel any ongoing operation
    if (abortControllerRef.current) {
      abortControllerRef.current.abort();
      abortControllerRef.current = null;
    }
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

  // Cancel ongoing operation
  const handleCancel = useCallback(() => {
    setIsCancelling(true);

    // Stop auto-run if active
    if (autoRunIntervalRef.current) {
      clearInterval(autoRunIntervalRef.current);
      autoRunIntervalRef.current = null;
      setIsAutoRunning(false);
    }

    // Abort any ongoing fetch/operation
    if (abortControllerRef.current) {
      abortControllerRef.current.abort();
      abortControllerRef.current = null;
    }

    // Reset loading states
    setIsLoading(false);
    setIsCancelling(false);
    setProgressSteps([]);
    setCurrentProgress(null);
    setStatus('paused');
    setSuccess('Operation cancelled');
    setTimeout(() => setSuccess(null), 2000);
  }, []);

  useEffect(() => {
    return () => {
      if (autoRunIntervalRef.current) {
        clearInterval(autoRunIntervalRef.current);
      }
    };
  }, []);

  const toggleModelSelection = (model: AlphaArenaModel) => {
    setSelectedModels(prev => {
      const isSelected = prev.some(m => m.id === model.id);
      if (isSelected) {
        return prev.filter(m => m.id !== model.id);
      }
      return [...prev, model];
    });
  };

  // =============================================================================
  // Render: Header
  // =============================================================================

  const renderHeader = () => (
    <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER, backgroundColor: COLORS.PANEL_BG }}>
      <div className="flex items-center gap-3">
        <Trophy size={20} style={{ color: COLORS.ORANGE }} />
        <h1 className="text-lg font-semibold" style={{ color: COLORS.WHITE }}>Alpha Arena</h1>
        <span className="text-xs px-2 py-0.5 rounded" style={{ backgroundColor: COLORS.PURPLE + '20', color: COLORS.PURPLE }}>
          AI Trading Competition
        </span>
      </div>

      <div className="flex items-center gap-2">
        {competitionId && (
          <div className="flex items-center gap-2 mr-4">
            <div className="flex items-center gap-1 px-2 py-1 rounded" style={{ backgroundColor: COLORS.BORDER }}>
              <Activity size={12} style={{ color: isAutoRunning ? COLORS.GREEN : COLORS.GRAY }} />
              <span className="text-xs" style={{ color: COLORS.WHITE }}>Cycle: {cycleCount}</span>
            </div>
            <div
              className="px-2 py-1 rounded text-xs"
              style={{
                backgroundColor: status === 'running' ? COLORS.GREEN + '20' : COLORS.GRAY + '20',
                color: status === 'running' ? COLORS.GREEN : COLORS.GRAY,
              }}
            >
              {isAutoRunning ? 'AUTO-RUNNING' : status?.toUpperCase() || 'IDLE'}
            </div>
          </div>
        )}
        <button
          onClick={() => loadLLMConfigs()}
          className="p-2 rounded transition-colors hover:bg-[#1A1A1A]"
          title="Refresh LLM configs"
        >
          <RefreshCw size={16} style={{ color: COLORS.GRAY }} />
        </button>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Progress Panel (Real-time streaming updates)
  // =============================================================================

  const renderProgressPanel = () => {
    if (!isLoading && progressSteps.length === 0) return null;

    return (
      <div
        className="p-4 rounded-lg mb-4"
        style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.ORANGE}40`,
          boxShadow: `0 0 20px ${COLORS.ORANGE}10`
        }}
      >
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center gap-2">
            <Loader2 size={18} className="animate-spin" style={{ color: COLORS.ORANGE }} />
            <h3 className="font-medium" style={{ color: COLORS.WHITE }}>
              {currentProgress ? currentProgress.message : 'Processing...'}
            </h3>
          </div>
          {/* STOP button in progress panel */}
          <button
            onClick={handleCancel}
            disabled={isCancelling}
            className="px-3 py-1.5 rounded text-xs font-medium flex items-center gap-1.5 transition-colors"
            style={{
              backgroundColor: COLORS.RED,
              color: COLORS.WHITE,
              opacity: isCancelling ? 0.5 : 1,
            }}
          >
            {isCancelling ? <Loader2 size={12} className="animate-spin" /> : <XCircle size={12} />}
            {isCancelling ? 'Stopping...' : 'Cancel'}
          </button>
        </div>

        {/* Progress Bar */}
        {currentProgress && (
          <div className="mb-3">
            <div className="h-2 rounded-full overflow-hidden" style={{ backgroundColor: COLORS.BORDER }}>
              <div
                className="h-full transition-all duration-300 ease-out"
                style={{
                  width: `${(currentProgress.step / currentProgress.total_steps) * 100}%`,
                  backgroundColor: currentProgress.event_type === 'error' ? COLORS.RED :
                                   currentProgress.event_type === 'complete' ? COLORS.GREEN : COLORS.ORANGE
                }}
              />
            </div>
            <div className="flex justify-between mt-1">
              <span className="text-xs" style={{ color: COLORS.GRAY }}>
                Step {currentProgress.step} of {currentProgress.total_steps}
              </span>
              <span className="text-xs" style={{ color: COLORS.GRAY }}>
                {Math.round((currentProgress.step / currentProgress.total_steps) * 100)}%
              </span>
            </div>
          </div>
        )}

        {/* Steps Log */}
        <div className="space-y-1 max-h-40 overflow-y-auto">
          {progressSteps.map((step, idx) => (
            <div key={idx} className="flex items-center gap-2 text-xs">
              {step.event_type === 'complete' ? (
                <Check size={12} style={{ color: COLORS.GREEN }} />
              ) : step.event_type === 'error' ? (
                <AlertCircle size={12} style={{ color: COLORS.RED }} />
              ) : step.event_type === 'warning' ? (
                <AlertCircle size={12} style={{ color: COLORS.YELLOW }} />
              ) : step.event_type === 'step' ? (
                <Circle size={12} style={{ color: COLORS.ORANGE }} />
              ) : (
                <ChevronRight size={12} style={{ color: COLORS.GRAY }} />
              )}
              <span style={{
                color: step.event_type === 'error' ? COLORS.RED :
                       step.event_type === 'complete' ? COLORS.GREEN :
                       step.event_type === 'warning' ? COLORS.YELLOW : COLORS.GRAY
              }}>
                {step.message}
              </span>
            </div>
          ))}
        </div>
      </div>
    );
  };

  // =============================================================================
  // Render: Past Competitions Panel
  // =============================================================================

  const renderPastCompetitions = () => (
    <div className="p-4 rounded-lg" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <History size={18} style={{ color: COLORS.ORANGE }} />
          <h2 className="font-semibold" style={{ color: COLORS.WHITE }}>Past Competitions</h2>
          <span className="text-xs px-2 py-0.5 rounded" style={{ backgroundColor: COLORS.BORDER, color: COLORS.GRAY }}>
            {pastCompetitions.length} saved
          </span>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={() => loadPastCompetitions()}
            disabled={loadingPastCompetitions}
            className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
            title="Refresh"
          >
            <RefreshCw size={14} className={loadingPastCompetitions ? 'animate-spin' : ''} style={{ color: COLORS.GRAY }} />
          </button>
          <button
            onClick={() => setShowPastCompetitions(false)}
            className="text-xs px-2 py-1 rounded"
            style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
          >
            Close
          </button>
        </div>
      </div>

      {loadingPastCompetitions ? (
        <div className="flex items-center justify-center py-8">
          <Loader2 size={24} className="animate-spin" style={{ color: COLORS.ORANGE }} />
        </div>
      ) : pastCompetitions.length === 0 ? (
        <div className="text-center py-8">
          <History size={32} className="mx-auto mb-2" style={{ color: COLORS.GRAY, opacity: 0.5 }} />
          <p className="text-sm" style={{ color: COLORS.GRAY }}>No past competitions found</p>
        </div>
      ) : (
        <div className="space-y-2 max-h-[400px] overflow-y-auto">
          {pastCompetitions.map(comp => (
            <div
              key={comp.competition_id}
              onClick={() => handleResumeCompetition(comp)}
              className="p-3 rounded cursor-pointer transition-colors"
              style={{
                backgroundColor: COLORS.CARD_BG,
                border: `1px solid ${COLORS.BORDER}`,
              }}
              onMouseEnter={e => {
                (e.currentTarget as HTMLElement).style.backgroundColor = COLORS.HOVER;
                (e.currentTarget as HTMLElement).style.borderColor = COLORS.ORANGE + '40';
              }}
              onMouseLeave={e => {
                (e.currentTarget as HTMLElement).style.backgroundColor = COLORS.CARD_BG;
                (e.currentTarget as HTMLElement).style.borderColor = COLORS.BORDER;
              }}
            >
              <div className="flex items-center justify-between mb-2">
                <div className="flex items-center gap-2">
                  <Trophy size={14} style={{ color: COLORS.ORANGE }} />
                  <span className="font-medium text-sm" style={{ color: COLORS.WHITE }}>
                    {comp.config.competition_name}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  <span
                    className="text-xs px-2 py-0.5 rounded"
                    style={{
                      backgroundColor:
                        comp.status === 'running' ? COLORS.GREEN + '20' :
                        comp.status === 'completed' ? COLORS.BLUE + '20' :
                        comp.status === 'failed' ? COLORS.RED + '20' :
                        COLORS.GRAY + '20',
                      color:
                        comp.status === 'running' ? COLORS.GREEN :
                        comp.status === 'completed' ? COLORS.BLUE :
                        comp.status === 'failed' ? COLORS.RED :
                        COLORS.GRAY,
                    }}
                  >
                    {comp.status.toUpperCase()}
                  </span>
                  <button
                    onClick={(e) => handleDeleteCompetition(comp.competition_id, e)}
                    className="p-1 rounded transition-colors hover:bg-red-500/20"
                    title="Delete competition"
                  >
                    <Trash2 size={12} style={{ color: COLORS.RED }} />
                  </button>
                </div>
              </div>

              <div className="grid grid-cols-3 gap-2 text-xs">
                <div>
                  <span style={{ color: COLORS.GRAY }}>Cycles: </span>
                  <span style={{ color: COLORS.WHITE }}>{comp.cycle_count}</span>
                </div>
                <div>
                  <span style={{ color: COLORS.GRAY }}>Models: </span>
                  <span style={{ color: COLORS.WHITE }}>{comp.config.models.length}</span>
                </div>
                <div>
                  <span style={{ color: COLORS.GRAY }}>Capital: </span>
                  <span style={{ color: COLORS.WHITE }}>${comp.config.initial_capital.toLocaleString()}</span>
                </div>
              </div>

              <div className="flex items-center gap-2 mt-2">
                <span className="text-xs" style={{ color: COLORS.GRAY }}>
                  {comp.config.symbols.join(', ')}
                </span>
                <span className="text-xs" style={{ color: COLORS.MUTED }}>•</span>
                <span className="text-xs" style={{ color: COLORS.GRAY }}>
                  {new Date(comp.created_at).toLocaleDateString()}
                </span>
              </div>

              <div className="flex flex-wrap gap-1 mt-2">
                {comp.config.models.map((model, idx) => (
                  <span
                    key={idx}
                    className="text-xs px-1.5 py-0.5 rounded"
                    style={{ backgroundColor: COLORS.BORDER, color: COLORS.GRAY }}
                  >
                    {model.name}
                  </span>
                ))}
              </div>

              <div className="flex items-center justify-end mt-2 pt-2 border-t" style={{ borderColor: COLORS.BORDER }}>
                <div className="flex items-center gap-1 text-xs" style={{ color: COLORS.ORANGE }}>
                  <PlayCircle size={12} />
                  <span>Click to resume</span>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Render: Create Competition Panel
  // =============================================================================

  const renderCreatePanel = () => (
    <div className="p-4 rounded-lg" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
      <div className="flex items-center gap-2 mb-4">
        <Plus size={18} style={{ color: COLORS.ORANGE }} />
        <h2 className="font-semibold" style={{ color: COLORS.WHITE }}>Create Competition</h2>
      </div>

      <div className="space-y-4">
        {/* Competition Name */}
        <div>
          <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Competition Name</label>
          <input
            type="text"
            value={competitionName}
            onChange={e => setCompetitionName(e.target.value)}
            className="w-full px-3 py-2 rounded text-sm"
            style={{ backgroundColor: COLORS.CARD_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
          />
        </div>

        {/* Symbol & Mode */}
        <div className="grid grid-cols-2 gap-4">
          <div>
            <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Trading Symbol</label>
            <select
              value={symbol}
              onChange={e => setSymbol(e.target.value)}
              className="w-full px-3 py-2 rounded text-sm"
              style={{ backgroundColor: COLORS.CARD_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
            >
              {TRADING_SYMBOLS.map(s => (
                <option key={s.value} value={s.value}>{s.label}</option>
              ))}
            </select>
          </div>
          <div>
            <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Competition Mode</label>
            <select
              value={mode}
              onChange={e => setMode(e.target.value as CompetitionMode)}
              className="w-full px-3 py-2 rounded text-sm"
              style={{ backgroundColor: COLORS.CARD_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
            >
              {COMPETITION_MODES.map(m => (
                <option key={m.value} value={m.value}>{m.label}</option>
              ))}
            </select>
          </div>
        </div>

        {/* Capital & Interval */}
        <div className="grid grid-cols-2 gap-4">
          <div>
            <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Initial Capital (USD)</label>
            <input
              type="number"
              value={initialCapital}
              onChange={e => setInitialCapital(Number(e.target.value))}
              min={1000}
              step={1000}
              className="w-full px-3 py-2 rounded text-sm"
              style={{ backgroundColor: COLORS.CARD_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
            />
          </div>
          <div>
            <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Cycle Interval (seconds)</label>
            <input
              type="number"
              value={cycleInterval}
              onChange={e => setCycleInterval(Number(e.target.value))}
              min={30}
              max={600}
              step={30}
              className="w-full px-3 py-2 rounded text-sm"
              style={{ backgroundColor: COLORS.CARD_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
            />
          </div>
        </div>

        {/* LLM Model Selection - FROM SETTINGS */}
        <div>
          <label className="text-xs mb-2 block flex items-center gap-2" style={{ color: COLORS.GRAY }}>
            <Brain size={12} />
            SELECT AI MODELS (from Settings - min 2 required)
          </label>

          {configuredLLMs.length === 0 ? (
            <div className="p-4 rounded text-center" style={{ backgroundColor: COLORS.CARD_BG, border: `1px solid ${COLORS.RED}40` }}>
              <AlertCircle size={24} className="mx-auto mb-2" style={{ color: COLORS.RED }} />
              <p className="text-sm" style={{ color: COLORS.RED }}>No AI models configured</p>
              <p className="text-xs mt-1" style={{ color: COLORS.GRAY }}>
                Go to Settings → LLM Config → My Model Library to add models with API keys
              </p>
            </div>
          ) : (
            <div className="grid grid-cols-2 gap-2">
              {configuredLLMs.map((model) => {
                const isSelected = selectedModels.some(m => m.id === model.id);
                return (
                  <button
                    key={model.id}
                    type="button"
                    onClick={() => toggleModelSelection(model)}
                    className="flex items-center gap-2 p-3 rounded text-left transition-colors"
                    style={{
                      backgroundColor: isSelected ? COLORS.ORANGE + '15' : COLORS.CARD_BG,
                      border: `1px solid ${isSelected ? COLORS.ORANGE : COLORS.BORDER}`,
                    }}
                  >
                    <div
                      className="w-5 h-5 rounded flex items-center justify-center"
                      style={{
                        backgroundColor: isSelected ? COLORS.ORANGE : COLORS.BORDER,
                        border: `1px solid ${isSelected ? COLORS.ORANGE : COLORS.BORDER}`,
                      }}
                    >
                      {isSelected && <CheckCircle size={12} style={{ color: COLORS.DARK_BG }} />}
                    </div>
                    <div className="flex-1 min-w-0">
                      <div className="text-sm font-medium truncate" style={{ color: COLORS.WHITE }}>
                        {model.display_name}
                      </div>
                      <div className="text-xs truncate" style={{ color: COLORS.GRAY }}>
                        {model.provider} • {model.model_id}
                      </div>
                    </div>
                  </button>
                );
              })}
            </div>
          )}
          <p className="text-xs mt-2" style={{ color: COLORS.GRAY }}>
            Selected: {selectedModels.length} model(s)
          </p>
        </div>

        {/* Actions */}
        <div className="flex gap-2 pt-2">
          <button
            onClick={() => setShowCreatePanel(false)}
            className="px-4 py-2 rounded text-sm"
            style={{ backgroundColor: COLORS.BORDER, color: COLORS.GRAY }}
          >
            Cancel
          </button>
          <button
            onClick={handleCreateCompetition}
            disabled={isLoading || selectedModels.length < 2 || configuredLLMs.length === 0}
            className="flex-1 py-2 rounded font-medium text-sm flex items-center justify-center gap-2"
            style={{
              backgroundColor: COLORS.ORANGE,
              color: COLORS.DARK_BG,
              opacity: (isLoading || selectedModels.length < 2) ? 0.5 : 1,
            }}
          >
            {isLoading ? <Loader2 size={16} className="animate-spin" /> : <Zap size={16} />}
            Create Competition
          </button>
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Competition Controls
  // =============================================================================

  const renderControls = () => (
    <div className="flex items-center gap-2 p-3 rounded-lg" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
      {/* Show STOP button when loading, otherwise show Run Cycle */}
      {isLoading ? (
        <button
          onClick={handleCancel}
          disabled={isCancelling}
          className="px-4 py-2 rounded text-sm font-medium flex items-center gap-2"
          style={{
            backgroundColor: COLORS.RED,
            color: COLORS.WHITE,
            opacity: isCancelling ? 0.5 : 1,
          }}
        >
          {isCancelling ? <Loader2 size={14} className="animate-spin" /> : <StopCircle size={14} />}
          {isCancelling ? 'Stopping...' : 'STOP'}
        </button>
      ) : (
        <button
          onClick={handleRunCycle}
          disabled={isAutoRunning}
          className="px-4 py-2 rounded text-sm font-medium flex items-center gap-2"
          style={{
            backgroundColor: COLORS.ORANGE,
            color: COLORS.DARK_BG,
            opacity: isAutoRunning ? 0.5 : 1,
          }}
        >
          <Play size={14} />
          Run Cycle
        </button>
      )}

      <button
        onClick={handleToggleAutoRun}
        disabled={isLoading}
        className="px-4 py-2 rounded text-sm font-medium flex items-center gap-2"
        style={{
          backgroundColor: isAutoRunning ? COLORS.RED + '20' : COLORS.GREEN + '20',
          color: isAutoRunning ? COLORS.RED : COLORS.GREEN,
          border: `1px solid ${isAutoRunning ? COLORS.RED + '40' : COLORS.GREEN + '40'}`,
          opacity: isLoading ? 0.5 : 1,
        }}
      >
        {isAutoRunning ? <Pause size={14} /> : <Zap size={14} />}
        {isAutoRunning ? 'Stop Auto' : 'Auto Run'}
      </button>

      <button
        onClick={handleReset}
        className="px-4 py-2 rounded text-sm flex items-center gap-2"
        style={{ backgroundColor: COLORS.BORDER, color: COLORS.GRAY }}
      >
        <RotateCcw size={14} />
        Reset
      </button>

      <div className="flex-1" />

      <div className="text-xs px-3 py-1 rounded" style={{ backgroundColor: COLORS.CARD_BG, color: COLORS.GRAY }}>
        <Clock size={12} className="inline mr-1" />
        Interval: {cycleInterval}s
      </div>
    </div>
  );

  // =============================================================================
  // Render: Leaderboard
  // =============================================================================

  const renderLeaderboard = () => {
    console.log('[AlphaArena] Rendering leaderboard, entries:', leaderboard.length, leaderboard);
    return (
    <div className="rounded-lg overflow-hidden" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <Trophy size={16} style={{ color: COLORS.ORANGE }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>Leaderboard</span>
        </div>
        <span className="text-xs" style={{ color: COLORS.GRAY }}>Cycle {cycleCount}</span>
      </div>

      {leaderboard.length === 0 ? (
        <div className="p-8 text-center" style={{ color: COLORS.GRAY }}>
          <Trophy size={32} className="mx-auto mb-2 opacity-30" />
          <p className="text-sm">No competition data yet</p>
        </div>
      ) : (
        <div className="divide-y" style={{ borderColor: COLORS.BORDER }}>
          {leaderboard.map(entry => {
            const isPositive = entry.total_pnl >= 0;
            const modelColor = getModelColor(entry.model_name);

            return (
              <div
                key={entry.model_name}
                className="flex items-center gap-3 px-4 py-3 hover:bg-[#1A1A1A] transition-colors"
              >
                {/* Rank */}
                <div
                  className="w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold"
                  style={{
                    backgroundColor: entry.rank === 1 ? COLORS.YELLOW + '20' : entry.rank === 2 ? COLORS.GRAY + '20' : entry.rank === 3 ? COLORS.ORANGE + '20' : COLORS.BORDER,
                    color: entry.rank === 1 ? COLORS.YELLOW : entry.rank === 2 ? COLORS.GRAY : entry.rank === 3 ? COLORS.ORANGE : COLORS.GRAY,
                  }}
                >
                  {entry.rank}
                </div>

                {/* Color indicator */}
                <div className="w-1 h-10 rounded-full" style={{ backgroundColor: modelColor }} />

                {/* Model info */}
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2">
                    <span className="font-medium truncate" style={{ color: COLORS.WHITE }}>
                      {entry.model_name}
                    </span>
                    {entry.rank === 1 && <Trophy size={14} style={{ color: COLORS.YELLOW }} />}
                  </div>
                  <div className="flex items-center gap-3 text-xs" style={{ color: COLORS.GRAY }}>
                    <span>{entry.trades_count} trades</span>
                    <span>{entry.positions_count} positions</span>
                    {entry.win_rate !== undefined && (
                      <span>{(entry.win_rate * 100).toFixed(0)}% win</span>
                    )}
                  </div>
                </div>

                {/* P&L */}
                <div className="text-right">
                  <div className="flex items-center justify-end gap-1">
                    {isPositive ? (
                      <TrendingUp size={14} style={{ color: COLORS.GREEN }} />
                    ) : (
                      <TrendingDown size={14} style={{ color: COLORS.RED }} />
                    )}
                    <span style={{ color: isPositive ? COLORS.GREEN : COLORS.RED }}>
                      {formatPercent(entry.return_pct)}
                    </span>
                  </div>
                  <span className="text-xs" style={{ color: COLORS.GRAY }}>
                    {formatCurrency(entry.portfolio_value)}
                  </span>
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
  };

  // =============================================================================
  // Render: Decisions Panel
  // =============================================================================

  const renderDecisions = () => {
    console.log('[AlphaArena] Rendering decisions, entries:', decisions.length, decisions);
    return (
    <div className="h-full flex flex-col" style={{ backgroundColor: COLORS.CARD_BG }}>
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <Brain size={16} style={{ color: COLORS.PURPLE }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>AI Decisions</span>
        </div>
        <span className="text-xs px-2 py-0.5 rounded" style={{ backgroundColor: COLORS.BORDER, color: COLORS.GRAY }}>
          {decisions.length}
        </span>
      </div>

      <div className="flex-1 overflow-y-auto">
        {decisions.length === 0 ? (
          <div className="p-8 text-center" style={{ color: COLORS.GRAY }}>
            <Sparkles size={32} className="mx-auto mb-2 opacity-30" />
            <p className="text-sm">No decisions yet</p>
            <p className="text-xs mt-1">Run a cycle to see AI trading decisions</p>
          </div>
        ) : (
          <div className="divide-y" style={{ borderColor: COLORS.BORDER }}>
            {decisions.slice(0, 50).map((decision, idx) => (
              <div key={idx} className="p-3 hover:bg-[#0F0F0F] transition-colors">
                <div className="flex items-center justify-between mb-1">
                  <span className="text-xs font-medium" style={{ color: COLORS.WHITE }}>
                    {decision.model_name}
                  </span>
                  <span className="text-xs" style={{ color: COLORS.GRAY }}>
                    Cycle {decision.cycle_number}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  <span
                    className="text-xs px-2 py-0.5 rounded font-medium"
                    style={{
                      backgroundColor: decision.action === 'buy' ? COLORS.GREEN + '20' :
                        decision.action === 'sell' ? COLORS.RED + '20' :
                        decision.action === 'short' ? COLORS.PURPLE + '20' :
                        COLORS.GRAY + '20',
                      color: decision.action === 'buy' ? COLORS.GREEN :
                        decision.action === 'sell' ? COLORS.RED :
                        decision.action === 'short' ? COLORS.PURPLE :
                        COLORS.GRAY,
                    }}
                  >
                    {decision.action.toUpperCase()}
                  </span>
                  <span className="text-xs" style={{ color: COLORS.GRAY }}>
                    {decision.symbol}
                  </span>
                  <span className="text-xs" style={{ color: COLORS.GRAY }}>
                    Qty: {decision.quantity.toFixed(4)}
                  </span>
                </div>
                {decision.reasoning && (
                  <p className="text-xs mt-1 line-clamp-2" style={{ color: COLORS.MUTED }}>
                    {decision.reasoning}
                  </p>
                )}
                <div className="flex items-center justify-between mt-1">
                  <span className="text-xs" style={{ color: COLORS.GRAY }}>
                    Confidence: {(decision.confidence * 100).toFixed(0)}%
                  </span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
  };

  // =============================================================================
  // Render: Performance Chart (Simple)
  // =============================================================================

  const renderPerformanceChart = () => (
    <div className="rounded-lg p-4" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
      <div className="flex items-center gap-2 mb-4">
        <BarChart3 size={16} style={{ color: COLORS.ORANGE }} />
        <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>Performance</span>
      </div>

      {snapshots.length === 0 ? (
        <div className="h-40 flex items-center justify-center" style={{ color: COLORS.GRAY }}>
          <div className="text-center">
            <BarChart3 size={32} className="mx-auto mb-2 opacity-30" />
            <p className="text-sm">No performance data yet</p>
          </div>
        </div>
      ) : (
        <div className="h-40 flex items-end gap-1">
          {/* Simple bar chart */}
          {leaderboard.map(entry => {
            const maxVal = Math.max(...leaderboard.map(e => Math.abs(e.return_pct)), 10);
            const height = Math.abs(entry.return_pct) / maxVal * 100;
            const isPositive = entry.return_pct >= 0;

            return (
              <div key={entry.model_name} className="flex-1 flex flex-col items-center">
                <div className="w-full flex-1 flex items-end justify-center">
                  <div
                    className="w-full max-w-8 rounded-t transition-all"
                    style={{
                      height: `${Math.max(height, 5)}%`,
                      backgroundColor: isPositive ? COLORS.GREEN : COLORS.RED,
                      opacity: 0.7,
                    }}
                  />
                </div>
                <span className="text-xs mt-1 truncate w-full text-center" style={{ color: COLORS.GRAY }}>
                  {entry.model_name.split(' ')[0]}
                </span>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Main Render
  // =============================================================================

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: COLORS.DARK_BG, color: COLORS.WHITE }}>
      {/* Header */}
      {renderHeader()}

      {/* Notifications */}
      {error && (
        <div className="mx-4 mt-2 p-3 rounded flex items-center gap-2" style={{ backgroundColor: COLORS.RED + '20', color: COLORS.RED }}>
          <AlertCircle size={16} />
          <span className="text-sm flex-1">{error}</span>
          <button onClick={() => setError(null)}>×</button>
        </div>
      )}
      {success && (
        <div className="mx-4 mt-2 p-3 rounded flex items-center gap-2" style={{ backgroundColor: COLORS.GREEN + '20', color: COLORS.GREEN }}>
          <CheckCircle size={16} />
          <span className="text-sm">{success}</span>
        </div>
      )}

      {/* Main Content */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left Panel */}
        <div className="flex-1 flex flex-col p-4 gap-4 overflow-y-auto">
          {/* No Competition State */}
          {!competitionId && !showCreatePanel && !showPastCompetitions && (
            <div className="p-8 rounded-lg text-center" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
              <Trophy size={48} className="mx-auto mb-4" style={{ color: COLORS.ORANGE, opacity: 0.5 }} />
              <h3 className="text-lg font-semibold mb-2" style={{ color: COLORS.WHITE }}>No Active Competition</h3>
              <p className="text-sm mb-4" style={{ color: COLORS.GRAY }}>
                Create a new AI trading competition or resume a past competition.
              </p>
              <div className="flex justify-center gap-3">
                <button
                  onClick={() => setShowCreatePanel(true)}
                  className="flex items-center gap-2 px-4 py-2 rounded font-medium"
                  style={{ backgroundColor: COLORS.ORANGE, color: COLORS.DARK_BG }}
                >
                  <Plus size={16} />
                  Create Competition
                </button>
                {pastCompetitions.length > 0 && (
                  <button
                    onClick={() => setShowPastCompetitions(true)}
                    className="flex items-center gap-2 px-4 py-2 rounded font-medium transition-colors"
                    style={{
                      backgroundColor: COLORS.CARD_BG,
                      color: COLORS.WHITE,
                      border: `1px solid ${COLORS.BORDER}`,
                    }}
                  >
                    <History size={16} />
                    Past Competitions ({pastCompetitions.length})
                  </button>
                )}
              </div>
              {configuredLLMs.length === 0 && (
                <p className="text-xs mt-4" style={{ color: COLORS.RED }}>
                  No AI models configured. Go to Settings → LLM Config → My Model Library first.
                </p>
              )}
            </div>
          )}

          {/* Past Competitions Panel */}
          {!competitionId && showPastCompetitions && renderPastCompetitions()}

          {/* Progress Panel - Shows during loading */}
          {renderProgressPanel()}

          {/* Create Panel */}
          {showCreatePanel && renderCreatePanel()}

          {/* Competition Active */}
          {competitionId && (
            <>
              {renderControls()}
              {renderPerformanceChart()}
              {renderLeaderboard()}
            </>
          )}
        </div>

        {/* Right Panel - Decisions */}
        <div className="w-[380px] border-l" style={{ borderColor: COLORS.BORDER }}>
          {renderDecisions()}
        </div>
      </div>

      {/* Footer */}
      <TabFooter
        tabName="ALPHA ARENA"
        leftInfo={[
          {
            label: competitionId ? `ID: ${competitionId.slice(0, 8)}...` : 'No Competition',
            color: COLORS.GRAY,
          },
          {
            label: `Models: ${selectedModels.length}`,
            color: COLORS.GRAY,
          },
          {
            label: `AI Models: ${configuredLLMs.length} available`,
            color: COLORS.GRAY,
          },
        ]}
        statusInfo={`Cycle: ${cycleCount} | Decisions: ${decisions.length}`}
        backgroundColor={COLORS.PANEL_BG}
        borderColor={COLORS.BORDER}
      />
    </div>
  );
};

export default AlphaArenaTab;
