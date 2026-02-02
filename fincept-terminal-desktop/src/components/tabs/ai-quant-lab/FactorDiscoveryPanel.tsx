/**
 * Factor Discovery Panel - Fincept Professional Design
 * RD-Agent powered autonomous factor mining with full backend integration
 * Real-world finance application with working features
 */

import React, { useState, useEffect } from 'react';
import {
  Sparkles,
  Play,
  Pause,
  Square,
  TrendingUp,
  Code,
  DollarSign,
  Clock,
  CheckCircle,
  AlertCircle,
  Download,
  Copy,
  BarChart2,
  Target,
  Zap,
  RefreshCw
} from 'lucide-react';
import { rdAgentService, type DiscoveredFactor } from '@/services/aiQuantLab/rdAgentService';
import { showError, showWarning, showSuccess } from '@/utils/notifications';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export function FactorDiscoveryPanel() {
  const [taskDescription, setTaskDescription] = useState('');
  const [apiKey, setApiKey] = useState('');
  const [targetMarket, setTargetMarket] = useState('US');
  const [budget, setBudget] = useState(10);
  const [isRunning, setIsRunning] = useState(false);
  const [currentTaskId, setCurrentTaskId] = useState<string | null>(null);
  const [taskStatus, setTaskStatus] = useState<any>(null);
  const [discoveredFactors, setDiscoveredFactors] = useState<DiscoveredFactor[]>([]);
  const [selectedFactor, setSelectedFactor] = useState<DiscoveredFactor | null>(null);
  const [statusPollInterval, setStatusPollInterval] = useState<NodeJS.Timeout | null>(null);

  // Example prompts for quick start
  const examplePrompts = [
    'Find profitable momentum factors for tech stocks with high IC',
    'Discover mean-reversion signals for cryptocurrency markets',
    'Generate volatility-based factors for risk management',
    'Find correlation-based cross-asset trading signals for commodities'
  ];

  // Cleanup polling on unmount
  useEffect(() => {
    return () => {
      if (statusPollInterval) {
        clearInterval(statusPollInterval);
      }
    };
  }, [statusPollInterval]);

  const handleStartMining = async () => {
    if (!taskDescription || !apiKey) {
      showWarning('Please provide task description and OpenAI API key');
      return;
    }

    setIsRunning(true);
    setDiscoveredFactors([]);
    setSelectedFactor(null);
    setTaskStatus(null);

    try {
      const response = await rdAgentService.startFactorMining({
        task_description: taskDescription,
        api_keys: { openai: apiKey },
        target_market: targetMarket,
        budget: budget
      });

      if (response.success && response.task_id) {
        setCurrentTaskId(response.task_id);
        setTaskStatus({
          status: 'running',
          progress: 0,
          message: response.message || 'Starting factor mining...'
        });
        // Start polling for status
        startPolling(response.task_id);
      } else {
        showError('Failed to start factor mining', [
          { label: 'ERROR', value: response.error || 'Unknown error' }
        ]);
        setIsRunning(false);
      }
    } catch (error) {
      console.error('Error starting factor mining:', error);
      showError('Error starting factor mining', [
        { label: 'ERROR', value: String(error) }
      ]);
      setIsRunning(false);
    }
  };

  const startPolling = (taskId: string) => {
    // Clear any existing interval
    if (statusPollInterval) {
      clearInterval(statusPollInterval);
    }

    // Poll every 5 seconds
    const interval = setInterval(async () => {
      try {
        const status = await rdAgentService.getFactorMiningStatus(taskId);
        console.log('[Factor Discovery] Status update:', status);

        if (status.success) {
          setTaskStatus(status);

          // If completed, fetch factors and stop polling
          if (status.status === 'completed') {
            clearInterval(interval);
            setStatusPollInterval(null);
            setIsRunning(false);
            await loadDiscoveredFactors(taskId);
          } else if (status.status === 'failed' || status.status === 'error') {
            clearInterval(interval);
            setStatusPollInterval(null);
            setIsRunning(false);
            showError('Factor mining failed', [
              { label: 'ERROR', value: status.error || 'Unknown error' }
            ]);
          }
        }
      } catch (error) {
        console.error('Error polling status:', error);
      }
    }, 5000);

    setStatusPollInterval(interval);
  };

  const handleStop = () => {
    if (statusPollInterval) {
      clearInterval(statusPollInterval);
      setStatusPollInterval(null);
    }
    setIsRunning(false);
    setTaskStatus(null);
  };

  const loadDiscoveredFactors = async (taskId: string) => {
    try {
      const response = await rdAgentService.getDiscoveredFactors(taskId);
      console.log('[Factor Discovery] Discovered factors:', response);

      if (response.success && response.factors && response.factors.length > 0) {
        setDiscoveredFactors(response.factors);
        setSelectedFactor(response.factors[0]);
      } else {
        console.warn('[Factor Discovery] No factors discovered');
      }
    } catch (error) {
      console.error('Error loading factors:', error);
    }
  };

  const copyToClipboard = async (text: string) => {
    try {
      await navigator.clipboard.writeText(text);
      showSuccess('Copied to clipboard!');
    } catch (error) {
      showError('Failed to copy to clipboard');
    }
  };

  return (
    <div className="flex h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      {/* Left Panel - Configuration */}
      <div
        className="w-96 border-r overflow-auto flex-shrink-0"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        <div className="p-4 space-y-4">
          {/* Header */}
          <div className="pb-3 border-b" style={{ borderColor: FINCEPT.BORDER }}>
            <div className="flex items-center gap-2 mb-1">
              <Sparkles size={18} color={FINCEPT.ORANGE} />
              <h2 className="text-sm font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                AUTONOMOUS FACTOR MINING
              </h2>
            </div>
            <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
              AI-powered discovery of profitable trading factors
            </p>
          </div>

          {/* Task Description */}
          <div>
            <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
              Research Objective
            </label>
            <textarea
              value={taskDescription}
              onChange={(e) => setTaskDescription(e.target.value)}
              placeholder="Describe the type of factors you want to discover..."
              rows={4}
              className="w-full px-3 py-2 rounded text-xs font-mono outline-none resize-none"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            />
            <div className="mt-2 space-y-1">
              <p className="text-xs font-bold uppercase" style={{ color: FINCEPT.GRAY }}>
                Examples:
              </p>
              {examplePrompts.map((prompt, idx) => (
                <button
                  key={idx}
                  onClick={() => setTaskDescription(prompt)}
                  className="block text-xs hover:underline text-left font-mono"
                  style={{ color: FINCEPT.ORANGE }}
                >
                  • {prompt}
                </button>
              ))}
            </div>
          </div>

          {/* API Key */}
          <div>
            <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
              OpenAI API Key
            </label>
            <input
              type="password"
              value={apiKey}
              onChange={(e) => setApiKey(e.target.value)}
              placeholder="sk-proj-..."
              className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            />
            <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
              Required for AI-powered factor generation
            </p>
          </div>

          {/* Settings */}
          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
                Target Market
              </label>
              <select
                value={targetMarket}
                onChange={(e) => setTargetMarket(e.target.value)}
                className="w-full px-3 py-2 rounded text-xs font-mono outline-none uppercase"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
              >
                <option value="US">US Markets</option>
                <option value="CN">China (A-shares)</option>
                <option value="CRYPTO">Cryptocurrency</option>
              </select>
            </div>
            <div>
              <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
                Budget (USD)
              </label>
              <input
                type="number"
                value={budget}
                onChange={(e) => setBudget(Number(e.target.value))}
                min={5}
                max={100}
                className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
              />
            </div>
          </div>

          {/* Control Buttons */}
          <div className="flex gap-2 pt-2">
            <button
              onClick={handleStartMining}
              disabled={isRunning || !taskDescription || !apiKey}
              className="flex-1 py-2.5 rounded font-bold uppercase text-xs tracking-wide flex items-center justify-center gap-2 hover:bg-opacity-90 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
              style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
            >
              {isRunning ? (
                <>
                  <RefreshCw size={14} className="animate-spin" />
                  MINING...
                </>
              ) : (
                <>
                  <Play size={14} />
                  START
                </>
              )}
            </button>
            {isRunning && (
              <button
                onClick={handleStop}
                className="px-4 py-2.5 rounded font-bold uppercase text-xs tracking-wide hover:bg-opacity-90 transition-colors"
                style={{ backgroundColor: FINCEPT.RED, color: FINCEPT.WHITE }}
              >
                <Square size={14} />
              </button>
            )}
          </div>

          {/* Real-time Status */}
          {taskStatus && (
            <div
              className="p-4 rounded border space-y-3"
              style={{ backgroundColor: FINCEPT.DARK_BG, borderColor: FINCEPT.BORDER }}
            >
              <div className="flex items-center justify-between">
                <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                  TASK STATUS
                </span>
                <span
                  className="text-xs px-2 py-0.5 rounded uppercase font-bold"
                  style={{
                    backgroundColor: taskStatus.status === 'completed' ? FINCEPT.GREEN :
                                   taskStatus.status === 'running' ? FINCEPT.ORANGE : FINCEPT.RED,
                    color: FINCEPT.DARK_BG
                  }}
                >
                  {taskStatus.status}
                </span>
              </div>
              <div className="space-y-2 text-xs font-mono">
                {taskStatus.progress !== undefined && (
                  <div>
                    <div className="flex justify-between mb-1">
                      <span style={{ color: FINCEPT.GRAY }}>PROGRESS</span>
                      <span style={{ color: FINCEPT.WHITE }}>{taskStatus.progress}%</span>
                    </div>
                    <div className="w-full h-1.5 rounded overflow-hidden" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                      <div
                        className="h-full transition-all duration-300"
                        style={{
                          width: `${taskStatus.progress}%`,
                          backgroundColor: FINCEPT.ORANGE
                        }}
                      />
                    </div>
                  </div>
                )}
                {taskStatus.factors_generated !== undefined && (
                  <div className="flex justify-between">
                    <span style={{ color: FINCEPT.GRAY }}>FACTORS GENERATED</span>
                    <span style={{ color: FINCEPT.GREEN }}>{taskStatus.factors_generated}</span>
                  </div>
                )}
                {taskStatus.factors_tested !== undefined && (
                  <div className="flex justify-between">
                    <span style={{ color: FINCEPT.GRAY }}>FACTORS TESTED</span>
                    <span style={{ color: FINCEPT.WHITE }}>{taskStatus.factors_tested}</span>
                  </div>
                )}
                {taskStatus.best_ic !== undefined && taskStatus.best_ic !== null && (
                  <div className="flex justify-between">
                    <span style={{ color: FINCEPT.GRAY }}>BEST IC</span>
                    <span style={{ color: FINCEPT.GREEN }}>{taskStatus.best_ic.toFixed(4)}</span>
                  </div>
                )}
                {taskStatus.elapsed_time && (
                  <div className="flex justify-between">
                    <span style={{ color: FINCEPT.GRAY }}>ELAPSED TIME</span>
                    <span style={{ color: FINCEPT.WHITE }}>{taskStatus.elapsed_time}</span>
                  </div>
                )}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-hidden flex flex-col">
        {discoveredFactors.length > 0 ? (
          <div className="flex h-full">
            {/* Factor List */}
            <div
              className="w-80 border-r overflow-auto flex-shrink-0"
              style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
            >
              <div className="p-3">
                <div className="flex items-center justify-between mb-3 pb-2 border-b" style={{ borderColor: FINCEPT.BORDER }}>
                  <h3 className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
                    DISCOVERED FACTORS
                  </h3>
                  <span className="text-xs px-2 py-0.5 rounded font-bold" style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}>
                    {discoveredFactors.length}
                  </span>
                </div>
                <div className="space-y-2">
                  {discoveredFactors.map((factor) => (
                    <button
                      key={factor.factor_id}
                      onClick={() => setSelectedFactor(factor)}
                      className="w-full p-3 rounded text-left transition-all hover:bg-opacity-80"
                      style={{
                        backgroundColor: selectedFactor?.factor_id === factor.factor_id ? FINCEPT.DARK_BG : FINCEPT.HEADER_BG,
                        border: selectedFactor?.factor_id === factor.factor_id ? `1px solid ${FINCEPT.ORANGE}` : `1px solid ${FINCEPT.BORDER}`
                      }}
                    >
                      <div className="flex items-start justify-between mb-2">
                        <span className="font-bold text-xs uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                          {factor.name}
                        </span>
                        <span
                          className="text-xs px-1.5 py-0.5 rounded font-bold"
                          style={{ backgroundColor: FINCEPT.GREEN, color: FINCEPT.DARK_BG }}
                        >
                          IC: {factor.ic.toFixed(4)}
                        </span>
                      </div>
                      <p className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                        {factor.description}
                      </p>
                      <div className="flex items-center gap-3 text-xs font-mono">
                        <span style={{ color: FINCEPT.GRAY }}>
                          SHARPE: <span style={{ color: FINCEPT.WHITE }}>{factor.sharpe.toFixed(2)}</span>
                        </span>
                        <span style={{ color: FINCEPT.GRAY }}>
                          RET: <span style={{ color: FINCEPT.GREEN }}>{factor.performance_metrics.annual_return.toFixed(1)}%</span>
                        </span>
                      </div>
                    </button>
                  ))}
                </div>
              </div>
            </div>

            {/* Factor Details */}
            {selectedFactor && (
              <div className="flex-1 overflow-auto p-6" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                <div className="space-y-6 max-w-5xl">
                  {/* Header */}
                  <div>
                    <h2 className="text-xl font-bold mb-2 uppercase tracking-wide flex items-center gap-3" style={{ color: FINCEPT.WHITE }}>
                      {selectedFactor.name}
                      <span className="text-sm px-3 py-1 rounded font-bold" style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}>
                        FACTOR ID: {selectedFactor.factor_id}
                      </span>
                    </h2>
                    <p className="font-mono text-sm" style={{ color: FINCEPT.GRAY }}>
                      {selectedFactor.description}
                    </p>
                  </div>

                  {/* Performance Metrics */}
                  <div>
                    <h3 className="text-xs font-bold uppercase tracking-wide mb-3" style={{ color: FINCEPT.GRAY }}>
                      PERFORMANCE METRICS
                    </h3>
                    <div className="grid grid-cols-4 gap-3">
                      <MetricCard
                        label="Information Coefficient"
                        value={selectedFactor.ic.toFixed(4)}
                        subtext={`± ${selectedFactor.ic_std.toFixed(4)}`}
                        icon={<TrendingUp size={16} />}
                        color={FINCEPT.GREEN}
                      />
                      <MetricCard
                        label="Sharpe Ratio"
                        value={selectedFactor.sharpe.toFixed(2)}
                        icon={<BarChart2 size={16} />}
                        color={FINCEPT.ORANGE}
                      />
                      <MetricCard
                        label="Annual Return"
                        value={`${selectedFactor.performance_metrics.annual_return.toFixed(1)}%`}
                        icon={<DollarSign size={16} />}
                        color={FINCEPT.GREEN}
                      />
                      <MetricCard
                        label="Max Drawdown"
                        value={`${selectedFactor.performance_metrics.max_drawdown.toFixed(1)}%`}
                        icon={<AlertCircle size={16} />}
                        color={FINCEPT.RED}
                      />
                      <MetricCard
                        label="Win Rate"
                        value={`${selectedFactor.performance_metrics.win_rate.toFixed(1)}%`}
                        icon={<Target size={16} />}
                        color={FINCEPT.CYAN}
                      />
                    </div>
                  </div>

                  {/* Factor Code */}
                  <div>
                    <div className="flex items-center justify-between mb-3">
                      <div className="flex items-center gap-2">
                        <Code size={16} color={FINCEPT.ORANGE} />
                        <h3 className="font-bold uppercase text-xs tracking-wide" style={{ color: FINCEPT.WHITE }}>
                          FACTOR IMPLEMENTATION
                        </h3>
                      </div>
                      <button
                        onClick={() => copyToClipboard(selectedFactor.code)}
                        className="flex items-center gap-2 px-3 py-1.5 rounded text-xs font-bold uppercase hover:bg-opacity-80 transition-colors"
                        style={{ backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE }}
                      >
                        <Copy size={12} />
                        COPY
                      </button>
                    </div>
                    <pre
                      className="p-4 rounded text-xs overflow-x-auto font-mono border"
                      style={{
                        backgroundColor: FINCEPT.PANEL_BG,
                        color: FINCEPT.WHITE,
                        borderColor: FINCEPT.BORDER
                      }}
                    >
                      <code>{selectedFactor.code}</code>
                    </pre>
                  </div>

                  {/* Action Buttons */}
                  <div className="flex gap-3 pt-4 border-t" style={{ borderColor: FINCEPT.BORDER }}>
                    <button
                      className="flex-1 py-3 rounded font-bold uppercase text-sm tracking-wide hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2"
                      style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
                    >
                      <Zap size={16} />
                      DEPLOY TO LIVE TRADING
                    </button>
                    <button
                      className="px-6 py-3 rounded font-bold uppercase text-sm tracking-wide hover:bg-opacity-80 transition-colors flex items-center justify-center gap-2"
                      style={{ backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE }}
                    >
                      <Download size={16} />
                      EXPORT
                    </button>
                  </div>
                </div>
              </div>
            )}
          </div>
        ) : (
          <div className="flex items-center justify-center h-full">
            <div className="text-center max-w-md">
              <Sparkles size={64} color={FINCEPT.GRAY} className="mx-auto mb-4" />
              <h3 className="text-base font-bold uppercase tracking-wide mb-2" style={{ color: FINCEPT.WHITE }}>
                NO FACTORS DISCOVERED YET
              </h3>
              <p className="text-sm font-mono" style={{ color: FINCEPT.GRAY }}>
                Configure your research objective and start factor mining<br/>
                to discover profitable trading signals automatically
              </p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

function MetricCard({
  label,
  value,
  subtext,
  icon,
  color
}: {
  label: string;
  value: string;
  subtext?: string;
  icon: React.ReactNode;
  color: string;
}) {
  return (
    <div
      className="p-3 rounded border"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
    >
      <div className="flex items-center justify-between mb-2">
        <span className="text-xs uppercase font-mono" style={{ color: FINCEPT.GRAY }}>
          {label}
        </span>
        <div style={{ color }}>{icon}</div>
      </div>
      <div className="text-xl font-bold font-mono" style={{ color }}>
        {value}
      </div>
      {subtext && (
        <div className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
          {subtext}
        </div>
      )}
    </div>
  );
}
