/**
 * Factor Discovery Panel
 * RD-Agent powered autonomous factor mining
 */

import React, { useState, useEffect } from 'react';
import { Sparkles, Play, Pause, TrendingUp, Code, DollarSign, Clock, CheckCircle, AlertCircle } from 'lucide-react';
import { rdAgentService, type DiscoveredFactor } from '@/services/aiQuantLab/rdAgentService';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
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

  // Example prompts
  const examplePrompts = [
    'Find profitable momentum factors for tech stocks',
    'Discover mean-reversion signals for cryptocurrency markets',
    'Generate volatility-based factors for risk management',
    'Find correlation-based cross-asset trading signals'
  ];

  const handleStartMining = async () => {
    if (!taskDescription || !apiKey) {
      alert('Please provide task description and API key');
      return;
    }

    setIsRunning(true);
    try {
      const response = await rdAgentService.startFactorMining({
        task_description: taskDescription,
        api_keys: { openai: apiKey },
        target_market: targetMarket,
        budget: budget
      });

      if (response.success && response.task_id) {
        setCurrentTaskId(response.task_id);
        // Start polling for status
        pollTaskStatus(response.task_id);
      } else {
        alert('Failed to start factor mining: ' + response.error);
        setIsRunning(false);
      }
    } catch (error) {
      console.error('Error starting factor mining:', error);
      setIsRunning(false);
    }
  };

  const pollTaskStatus = async (taskId: string) => {
    const interval = setInterval(async () => {
      try {
        const status = await rdAgentService.getFactorMiningStatus(taskId);
        setTaskStatus(status);

        // If completed, fetch factors
        if (status.status === 'completed') {
          clearInterval(interval);
          setIsRunning(false);
          loadDiscoveredFactors(taskId);
        }
      } catch (error) {
        console.error('Error polling status:', error);
        clearInterval(interval);
        setIsRunning(false);
      }
    }, 5000); // Poll every 5 seconds
  };

  const loadDiscoveredFactors = async (taskId: string) => {
    try {
      const response = await rdAgentService.getDiscoveredFactors(taskId);
      if (response.success && response.factors) {
        setDiscoveredFactors(response.factors);
        if (response.factors.length > 0) {
          setSelectedFactor(response.factors[0]);
        }
      }
    } catch (error) {
      console.error('Error loading factors:', error);
    }
  };

  return (
    <div className="flex h-full">
      {/* Left Panel - Configuration */}
      <div className="w-1/3 border-r overflow-auto" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
        <div className="p-6 space-y-6">
          {/* Header */}
          <div>
            <div className="flex items-center gap-2 mb-2">
              <Sparkles size={20} color={BLOOMBERG.ORANGE} />
              <h2 className="text-lg font-semibold uppercase" style={{ color: BLOOMBERG.WHITE }}>
                Autonomous Factor Mining
              </h2>
            </div>
            <p className="text-sm" style={{ color: BLOOMBERG.GRAY }}>
              Use AI to discover profitable trading factors automatically
            </p>
          </div>

          {/* Task Description */}
          <div>
            <label className="block text-sm font-medium mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
              What do you want to discover?
            </label>
            <textarea
              value={taskDescription}
              onChange={(e) => setTaskDescription(e.target.value)}
              placeholder="Describe the type of factors you're looking for..."
              rows={4}
              className="w-full px-3 py-2 rounded text-sm outline-none"
              style={{
                backgroundColor: BLOOMBERG.DARK_BG,
                color: BLOOMBERG.WHITE,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            />
            <div className="mt-2 space-y-1">
              <p className="text-xs" style={{ color: BLOOMBERG.GRAY }}>Examples:</p>
              {examplePrompts.map((prompt, idx) => (
                <button
                  key={idx}
                  onClick={() => setTaskDescription(prompt)}
                  className="block text-xs hover:underline text-left"
                  style={{ color: BLOOMBERG.ORANGE }}
                >
                  • {prompt}
                </button>
              ))}
            </div>
          </div>

          {/* API Key */}
          <div>
            <label className="block text-sm font-medium mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
              OpenAI API Key
            </label>
            <input
              type="password"
              value={apiKey}
              onChange={(e) => setApiKey(e.target.value)}
              placeholder="sk-..."
              className="w-full px-3 py-2 rounded text-sm outline-none"
              style={{
                backgroundColor: BLOOMBERG.DARK_BG,
                color: BLOOMBERG.WHITE,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            />
          </div>

          {/* Settings */}
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block text-sm font-medium mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
                Market
              </label>
              <select
                value={targetMarket}
                onChange={(e) => setTargetMarket(e.target.value)}
                className="w-full px-3 py-2 rounded text-sm outline-none uppercase"
                style={{
                  backgroundColor: BLOOMBERG.DARK_BG,
                  color: BLOOMBERG.WHITE,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              >
                <option value="US">US</option>
                <option value="CN">China</option>
                <option value="CRYPTO">Crypto</option>
              </select>
            </div>
            <div>
              <label className="block text-sm font-medium mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
                Budget ($)
              </label>
              <input
                type="number"
                value={budget}
                onChange={(e) => setBudget(Number(e.target.value))}
                min={5}
                max={50}
                className="w-full px-3 py-2 rounded text-sm outline-none"
                style={{
                  backgroundColor: BLOOMBERG.DARK_BG,
                  color: BLOOMBERG.WHITE,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              />
            </div>
          </div>

          {/* Start Button */}
          <button
            onClick={handleStartMining}
            disabled={isRunning || !taskDescription || !apiKey}
            className="w-full py-3 rounded font-semibold uppercase flex items-center justify-center gap-2 hover:bg-opacity-90 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            {isRunning ? (
              <>
                <Pause size={18} />
                Mining in Progress...
              </>
            ) : (
              <>
                <Play size={18} />
                Start Factor Mining
              </>
            )}
          </button>

          {/* Status */}
          {taskStatus && (
            <div className="p-4 rounded space-y-3" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
              <div className="flex items-center justify-between">
                <span className="text-sm font-medium uppercase" style={{ color: BLOOMBERG.WHITE }}>Status</span>
                <span className="text-xs px-2 py-1 rounded uppercase" style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}>
                  {taskStatus.status}
                </span>
              </div>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span style={{ color: BLOOMBERG.GRAY }}>Progress</span>
                  <span style={{ color: BLOOMBERG.WHITE }}>{taskStatus.progress || 0}%</span>
                </div>
                <div className="flex justify-between">
                  <span style={{ color: BLOOMBERG.GRAY }}>Factors Generated</span>
                  <span style={{ color: BLOOMBERG.GREEN }}>{taskStatus.factors_generated || 0}</span>
                </div>
                <div className="flex justify-between">
                  <span style={{ color: BLOOMBERG.GRAY }}>Factors Tested</span>
                  <span style={{ color: BLOOMBERG.WHITE }}>{taskStatus.factors_tested || 0}</span>
                </div>
                <div className="flex justify-between">
                  <span style={{ color: BLOOMBERG.GRAY }}>Best IC</span>
                  <span style={{ color: BLOOMBERG.GREEN }}>{taskStatus.best_ic?.toFixed(4) || '-'}</span>
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-auto" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        {discoveredFactors.length > 0 ? (
          <div className="flex h-full">
            {/* Factor List */}
            <div className="w-1/3 border-r overflow-auto" style={{ borderColor: BLOOMBERG.BORDER }}>
              <div className="p-4 space-y-2">
                <h3 className="text-sm font-semibold mb-3 uppercase" style={{ color: BLOOMBERG.GRAY }}>
                  DISCOVERED FACTORS ({discoveredFactors.length})
                </h3>
                {discoveredFactors.map((factor) => (
                  <button
                    key={factor.factor_id}
                    onClick={() => setSelectedFactor(factor)}
                    className="w-full p-3 rounded text-left transition-colors hover:bg-opacity-80"
                    style={{
                      backgroundColor: selectedFactor?.factor_id === factor.factor_id ? BLOOMBERG.HEADER_BG : BLOOMBERG.PANEL_BG,
                      border: selectedFactor?.factor_id === factor.factor_id ? `1px solid ${BLOOMBERG.ORANGE}` : `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  >
                    <div className="flex items-start justify-between mb-2">
                      <span className="font-medium text-sm" style={{ color: BLOOMBERG.WHITE }}>
                        {factor.name}
                      </span>
                      <span className="text-xs px-2 py-0.5 rounded uppercase" style={{ backgroundColor: BLOOMBERG.GREEN, color: BLOOMBERG.DARK_BG }}>
                        IC: {factor.ic.toFixed(4)}
                      </span>
                    </div>
                    <p className="text-xs mb-2" style={{ color: BLOOMBERG.GRAY }}>
                      {factor.description}
                    </p>
                    <div className="flex items-center gap-3 text-xs">
                      <span style={{ color: BLOOMBERG.GRAY }}>
                        Sharpe: <span style={{ color: BLOOMBERG.WHITE }}>{factor.sharpe.toFixed(2)}</span>
                      </span>
                      <span style={{ color: BLOOMBERG.GRAY }}>
                        Return: <span style={{ color: BLOOMBERG.GREEN }}>{factor.performance_metrics.annual_return.toFixed(1)}%</span>
                      </span>
                    </div>
                  </button>
                ))}
              </div>
            </div>

            {/* Factor Details */}
            {selectedFactor && (
              <div className="flex-1 overflow-auto p-6">
                <div className="space-y-6">
                  <div>
                    <h2 className="text-xl font-bold mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
                      {selectedFactor.name}
                    </h2>
                    <p style={{ color: BLOOMBERG.GRAY }}>{selectedFactor.description}</p>
                  </div>

                  {/* Metrics */}
                  <div className="grid grid-cols-2 gap-4">
                    <MetricCard
                      label="Information Coefficient"
                      value={selectedFactor.ic.toFixed(4)}
                      icon={<TrendingUp size={20} />}
                      color={BLOOMBERG.GREEN}
                    />
                    <MetricCard
                      label="Sharpe Ratio"
                      value={selectedFactor.sharpe.toFixed(2)}
                      icon={<TrendingUp size={20} />}
                      color={BLOOMBERG.ORANGE}
                    />
                    <MetricCard
                      label="Annual Return"
                      value={`${selectedFactor.performance_metrics.annual_return.toFixed(1)}%`}
                      icon={<DollarSign size={20} />}
                      color={BLOOMBERG.GREEN}
                    />
                    <MetricCard
                      label="Max Drawdown"
                      value={`${selectedFactor.performance_metrics.max_drawdown.toFixed(1)}%`}
                      icon={<AlertCircle size={20} />}
                      color={BLOOMBERG.RED}
                    />
                  </div>

                  {/* Code */}
                  <div>
                    <div className="flex items-center gap-2 mb-3">
                      <Code size={18} color={BLOOMBERG.ORANGE} />
                      <h3 className="font-semibold uppercase" style={{ color: BLOOMBERG.WHITE }}>Factor Code</h3>
                    </div>
                    <pre
                      className="p-4 rounded text-sm overflow-x-auto font-mono"
                      style={{ backgroundColor: BLOOMBERG.PANEL_BG, color: BLOOMBERG.WHITE }}
                    >
                      <code>{selectedFactor.code}</code>
                    </pre>
                  </div>

                  {/* Deploy Button */}
                  <button
                    className="w-full py-3 rounded font-semibold uppercase hover:bg-opacity-90 transition-colors"
                    style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
                  >
                    Deploy to Live Trading
                  </button>
                </div>
              </div>
            )}
          </div>
        ) : (
          <div className="flex items-center justify-center h-full">
            <div className="text-center max-w-md">
              <Sparkles size={48} color={BLOOMBERG.GRAY} className="mx-auto mb-4" />
              <h3 className="text-lg font-semibold mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
                No Factors Discovered Yet
              </h3>
              <p className="text-sm" style={{ color: BLOOMBERG.GRAY }}>
                Start factor mining to discover profitable trading signals automatically
              </p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

function MetricCard({ label, value, icon, color }: { label: string; value: string; icon: React.ReactNode; color: string }) {
  return (
    <div className="p-4 rounded" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
      <div className="flex items-center justify-between mb-2">
        <span className="text-xs uppercase" style={{ color: BLOOMBERG.GRAY }}>{label}</span>
        <div style={{ color }}>{icon}</div>
      </div>
      <div className="text-2xl font-bold" style={{ color }}>
        {value}
      </div>
    </div>
  );
}
