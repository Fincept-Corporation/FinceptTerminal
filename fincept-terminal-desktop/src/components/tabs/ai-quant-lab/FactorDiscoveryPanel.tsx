/**
 * Factor Discovery Panel - Fincept Terminal Design
 * RD-Agent powered autonomous factor mining with full backend integration
 * Real-world finance application with working features
 * PURPLE THEME
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
import { buildApiKeysMap } from '@/services/core/sqliteService';
import { showError, showWarning, showSuccess } from '@/utils/notifications';
import { useTerminalTheme } from '@/contexts/ThemeContext';

export function FactorDiscoveryPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
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
      const apiKeys = await buildApiKeysMap();
      // Allow manual override from the input field for backward compat
      if (apiKey.trim()) {
        apiKeys['OPENAI_API_KEY'] = apiKey.trim();
        apiKeys['openai'] = apiKey.trim();
      }
      const response = await rdAgentService.startFactorMining({
        task_description: taskDescription,
        api_keys: apiKeys,
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
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', backgroundColor: colors.background }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
        backgroundColor: colors.panel,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Sparkles size={16} color={colors.purple} />
        <span style={{
          color: colors.purple,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          AUTONOMOUS FACTOR MINING
        </span>
        <div style={{ flex: 1 }} />
        {isRunning && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: colors.purple + '20',
            border: `1px solid ${colors.purple}`,
            color: colors.purple,
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}>
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              backgroundColor: colors.purple,
              animation: 'pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite'
            }} />
            MINING IN PROGRESS
          </div>
        )}
        {discoveredFactors.length > 0 && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: colors.success + '20',
            border: `1px solid ${colors.success}`,
            color: colors.success
          }}>
            {discoveredFactors.length} FACTORS DISCOVERED
          </div>
        )}
      </div>

      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Panel - Configuration */}
        <div style={{
          width: '360px',
          borderRight: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          backgroundColor: colors.panel,
          display: 'flex',
          flexDirection: 'column',
          overflowY: 'auto'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.purple,
            fontFamily: 'monospace',
            letterSpacing: '0.5px'
          }}>
            CONFIGURATION
          </div>

          <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '16px' }}>

            {/* Task Description */}
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontFamily: 'monospace',
                color: colors.text,
                opacity: 0.5,
                marginBottom: '8px',
                letterSpacing: '0.5px'
              }}>
                RESEARCH OBJECTIVE
              </label>
              <textarea
                value={taskDescription}
                onChange={(e) => setTaskDescription(e.target.value)}
                placeholder="Describe the type of factors you want to discover..."
                rows={4}
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: colors.background,
                  color: colors.text,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  outline: 'none',
                  resize: 'none'
                }}
              />
              <div style={{ marginTop: '8px', display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <div style={{
                  fontSize: '9px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  opacity: 0.5,
                  letterSpacing: '0.5px'
                }}>
                  EXAMPLES:
                </div>
                {examplePrompts.map((prompt, idx) => (
                  <button
                    key={idx}
                    onClick={() => setTaskDescription(prompt)}
                    style={{
                      textAlign: 'left',
                      fontSize: '9px',
                      fontFamily: 'monospace',
                      color: colors.purple,
                      background: 'none',
                      border: 'none',
                      cursor: 'pointer',
                      padding: '2px 0',
                      transition: 'opacity 0.15s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.opacity = '0.7';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.opacity = '1';
                    }}
                  >
                    • {prompt}
                  </button>
                ))}
              </div>
            </div>

            {/* API Key */}
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontFamily: 'monospace',
                color: colors.text,
                opacity: 0.5,
                marginBottom: '8px',
                letterSpacing: '0.5px'
              }}>
                OPENAI API KEY
              </label>
              <input
                type="password"
                value={apiKey}
                onChange={(e) => setApiKey(e.target.value)}
                placeholder="sk-proj-..."
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: colors.background,
                  color: colors.text,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  fontWeight: 700,
                  outline: 'none'
                }}
              />
              <div style={{
                fontSize: '9px',
                fontFamily: 'monospace',
                color: colors.text,
                opacity: 0.5,
                marginTop: '6px'
              }}>
                Required for AI-powered factor generation
              </div>
            </div>

            {/* Settings */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '9px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  opacity: 0.5,
                  marginBottom: '8px',
                  letterSpacing: '0.5px'
                }}>
                  TARGET MARKET
                </label>
                <select
                  value={targetMarket}
                  onChange={(e) => setTargetMarket(e.target.value)}
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: colors.background,
                    color: colors.text,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    fontSize: '10px',
                    fontFamily: 'monospace',
                    fontWeight: 700,
                    outline: 'none',
                    cursor: 'pointer'
                  }}
                >
                  <option value="US">US MARKETS</option>
                  <option value="CN">CHINA (A-SHARES)</option>
                  <option value="CRYPTO">CRYPTOCURRENCY</option>
                </select>
              </div>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '9px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  opacity: 0.5,
                  marginBottom: '8px',
                  letterSpacing: '0.5px'
                }}>
                  BUDGET (USD)
                </label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={String(budget)}
                  onChange={(e) => {
                    const v = e.target.value;
                    if (v === '' || /^\d*\.?\d*$/.test(v)) {
                      setBudget(v === '' ? 10 : Number(v));
                    }
                  }}
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: colors.background,
                    color: colors.text,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    fontSize: '10px',
                    fontFamily: 'monospace',
                    fontWeight: 700,
                    outline: 'none'
                  }}
                />
              </div>
            </div>

            {/* Control Buttons */}
            <div style={{ display: 'flex', gap: '8px', paddingTop: '8px' }}>
              <button
                onClick={handleStartMining}
                disabled={isRunning || !taskDescription || !apiKey}
                style={{
                  flex: 1,
                  padding: '12px 16px',
                  backgroundColor: (!taskDescription || !apiKey) ? colors.background : colors.purple,
                  border: 'none',
                  color: (!taskDescription || !apiKey) ? colors.text : '#000000',
                  opacity: (!taskDescription || !apiKey) ? 0.5 : 1,
                  fontSize: '11px',
                  fontWeight: 700,
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  cursor: (!taskDescription || !apiKey) ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  transition: 'all 0.15s'
                }}
                onMouseEnter={(e) => {
                  if (taskDescription && apiKey && !isRunning) {
                    e.currentTarget.style.opacity = '0.9';
                  }
                }}
                onMouseLeave={(e) => {
                  if (taskDescription && apiKey) {
                    e.currentTarget.style.opacity = '1';
                  }
                }}
              >
                {isRunning ? (
                  <>
                    <RefreshCw size={14} style={{ animation: 'spin 1s linear infinite' }} />
                    MINING...
                  </>
                ) : (
                  <>
                    <Play size={14} />
                    START MINING
                  </>
                )}
              </button>
              {isRunning && (
                <button
                  onClick={handleStop}
                  style={{
                    padding: '12px 16px',
                    backgroundColor: colors.alert,
                    border: 'none',
                    color: colors.text,
                    fontSize: '11px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    cursor: 'pointer',
                    transition: 'opacity 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.opacity = '0.9';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.opacity = '1';
                  }}
                >
                  <Square size={14} />
                </button>
              )}
            </div>

            {/* Real-time Status */}
            {taskStatus && (
              <div style={{
                padding: '14px',
                backgroundColor: colors.background,
                border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                borderLeft: `3px solid ${colors.purple}`
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  marginBottom: '12px'
                }}>
                  <span style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    color: colors.text,
                    letterSpacing: '0.5px'
                  }}>
                    TASK STATUS
                  </span>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    padding: '3px 8px',
                    backgroundColor: taskStatus.status === 'completed' ? colors.success :
                                   taskStatus.status === 'running' ? colors.purple : colors.alert,
                    color: '#000000',
                    letterSpacing: '0.5px'
                  }}>
                    {taskStatus.status.toUpperCase()}
                  </div>
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', fontSize: '10px', fontFamily: 'monospace' }}>
                  {taskStatus.progress !== undefined && (
                    <div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                        <span style={{ color: colors.text, opacity: 0.5 }}>PROGRESS</span>
                        <span style={{ color: colors.text, fontWeight: 700 }}>{taskStatus.progress}%</span>
                      </div>
                      <div style={{
                        width: '100%',
                        height: '3px',
                        backgroundColor: colors.panel,
                        overflow: 'hidden',
                        border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                      }}>
                        <div style={{
                          height: '100%',
                          width: `${taskStatus.progress}%`,
                          backgroundColor: colors.purple,
                          transition: 'width 0.3s ease'
                        }} />
                      </div>
                    </div>
                  )}
                  {taskStatus.factors_generated !== undefined && (
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: colors.text, opacity: 0.5 }}>FACTORS GENERATED</span>
                      <span style={{ color: colors.success, fontWeight: 700 }}>{taskStatus.factors_generated}</span>
                    </div>
                  )}
                  {taskStatus.factors_tested !== undefined && (
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: colors.text, opacity: 0.5 }}>FACTORS TESTED</span>
                      <span style={{ color: colors.text, fontWeight: 700 }}>{taskStatus.factors_tested}</span>
                    </div>
                  )}
                  {taskStatus.best_ic !== undefined && taskStatus.best_ic !== null && (
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: colors.text, opacity: 0.5 }}>BEST IC</span>
                      <span style={{ color: colors.success, fontWeight: 700 }}>{taskStatus.best_ic.toFixed(4)}</span>
                    </div>
                  )}
                  {taskStatus.elapsed_time && (
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: colors.text, opacity: 0.5 }}>ELAPSED TIME</span>
                      <span style={{ color: colors.text, fontWeight: 700 }}>{taskStatus.elapsed_time}</span>
                    </div>
                  )}
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Right Panel - Results */}
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {discoveredFactors.length > 0 ? (
            <div style={{ display: 'flex', flex: 1 }}>
              {/* Factor List */}
              <div style={{
                width: '340px',
                borderRight: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                backgroundColor: colors.panel,
                display: 'flex',
                flexDirection: 'column'
              }}>
                <div style={{
                  padding: '10px 12px',
                  borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between'
                }}>
                  <span style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    color: colors.purple,
                    letterSpacing: '0.5px'
                  }}>
                    DISCOVERED FACTORS
                  </span>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    padding: '3px 8px',
                    backgroundColor: colors.purple + '20',
                    border: `1px solid ${colors.purple}`,
                    color: colors.purple
                  }}>
                    {discoveredFactors.length}
                  </div>
                </div>
                <div style={{ flex: 1, overflowY: 'auto', padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
                  {discoveredFactors.map((factor, idx) => {
                    const isSelected = selectedFactor?.factor_id === factor.factor_id;
                    return (
                      <button
                        key={factor.factor_id}
                        onClick={() => setSelectedFactor(factor)}
                        style={{
                          padding: '12px',
                          backgroundColor: isSelected ? '#1F1F1F' : 'transparent',
                          border: `1px solid ${isSelected ? colors.purple : 'var(--ft-border-color, #2A2A2A)'}`,
                          borderTop: idx === 0 ? `1px solid ${isSelected ? colors.purple : 'var(--ft-border-color, #2A2A2A)'}` : '0',
                          cursor: 'pointer',
                          transition: 'all 0.15s',
                          marginTop: idx === 0 ? '0' : '-1px',
                          textAlign: 'left'
                        }}
                        onMouseEnter={(e) => {
                          if (!isSelected) {
                            e.currentTarget.style.backgroundColor = colors.background;
                            e.currentTarget.style.borderColor = colors.purple;
                          }
                        }}
                        onMouseLeave={(e) => {
                          if (!isSelected) {
                            e.currentTarget.style.backgroundColor = 'transparent';
                            e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                          }
                        }}
                      >
                        <div style={{ display: 'flex', alignItems: 'start', justifyContent: 'space-between', marginBottom: '8px' }}>
                          <span style={{
                            fontSize: '11px',
                            fontWeight: 700,
                            fontFamily: 'monospace',
                            color: colors.text
                          }}>
                            {factor.name}
                          </span>
                          <div style={{
                            fontSize: '9px',
                            fontWeight: 700,
                            fontFamily: 'monospace',
                            padding: '2px 6px',
                            backgroundColor: colors.success + '20',
                            border: `1px solid ${colors.success}`,
                            color: colors.success
                          }}>
                            IC: {factor.ic.toFixed(4)}
                          </div>
                        </div>
                        <div style={{
                          fontSize: '9px',
                          fontFamily: 'monospace',
                          color: colors.text,
                          opacity: 0.6,
                          marginBottom: '8px',
                          lineHeight: '1.4'
                        }}>
                          {factor.description}
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', fontSize: '9px', fontFamily: 'monospace' }}>
                          <span style={{ color: colors.text, opacity: 0.5 }}>
                            SHARPE: <span style={{ color: colors.text, opacity: 1, fontWeight: 700 }}>{factor.sharpe.toFixed(2)}</span>
                          </span>
                          <span style={{ color: colors.text, opacity: 0.5 }}>
                            RET: <span style={{ color: colors.success, opacity: 1, fontWeight: 700 }}>{factor.performance_metrics.annual_return.toFixed(1)}%</span>
                          </span>
                        </div>
                      </button>
                    );
                  })}
                </div>
              </div>

              {/* Factor Details */}
              {selectedFactor && (
                <div style={{ flex: 1, overflowY: 'auto', padding: '16px', backgroundColor: colors.background }}>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '20px', maxWidth: '1200px' }}>
                    {/* Header */}
                    <div>
                      <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '12px',
                        marginBottom: '12px'
                      }}>
                        <h2 style={{
                          fontSize: '18px',
                          fontWeight: 700,
                          fontFamily: 'monospace',
                          color: colors.text,
                          letterSpacing: '0.5px'
                        }}>
                          {selectedFactor.name}
                        </h2>
                        <div style={{
                          fontSize: '10px',
                          fontWeight: 700,
                          fontFamily: 'monospace',
                          padding: '4px 10px',
                          backgroundColor: colors.purple + '20',
                          border: `1px solid ${colors.purple}`,
                          color: colors.purple
                        }}>
                          FACTOR ID: {selectedFactor.factor_id}
                        </div>
                      </div>
                      <div style={{
                        fontSize: '11px',
                        fontFamily: 'monospace',
                        color: colors.text,
                        opacity: 0.6,
                        lineHeight: '1.5'
                      }}>
                        {selectedFactor.description}
                      </div>
                    </div>

                    {/* Performance Metrics */}
                    <div>
                      <div style={{
                        fontSize: '10px',
                        fontWeight: 700,
                        fontFamily: 'monospace',
                        color: colors.purple,
                        letterSpacing: '0.5px',
                        marginBottom: '12px'
                      }}>
                        PERFORMANCE METRICS
                      </div>
                      <div style={{
                        display: 'grid',
                        gridTemplateColumns: 'repeat(4, 1fr)',
                        gap: '12px'
                      }}>
                      <MetricCard
                        label="Information Coefficient"
                        value={selectedFactor.ic.toFixed(4)}
                        subtext={`± ${selectedFactor.ic_std.toFixed(4)}`}
                        icon={<TrendingUp size={16} />}
                        color={colors.success}
                      />
                      <MetricCard
                        label="Sharpe Ratio"
                        value={selectedFactor.sharpe.toFixed(2)}
                        icon={<BarChart2 size={16} />}
                        color={colors.primary}
                      />
                      <MetricCard
                        label="Annual Return"
                        value={`${selectedFactor.performance_metrics.annual_return.toFixed(1)}%`}
                        icon={<DollarSign size={16} />}
                        color={colors.success}
                      />
                      <MetricCard
                        label="Max Drawdown"
                        value={`${selectedFactor.performance_metrics.max_drawdown.toFixed(1)}%`}
                        icon={<AlertCircle size={16} />}
                        color={colors.alert}
                      />
                      <MetricCard
                        label="Win Rate"
                        value={`${selectedFactor.performance_metrics.win_rate.toFixed(1)}%`}
                        icon={<Target size={16} />}
                        color={colors.accent}
                      />
                    </div>
                  </div>

                    {/* Factor Code */}
                    <div>
                      <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'space-between',
                        marginBottom: '12px'
                      }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                          <Code size={16} color={colors.purple} />
                          <span style={{
                            fontSize: '10px',
                            fontWeight: 700,
                            fontFamily: 'monospace',
                            color: colors.text,
                            letterSpacing: '0.5px'
                          }}>
                            FACTOR IMPLEMENTATION
                          </span>
                        </div>
                        <button
                          onClick={() => copyToClipboard(selectedFactor.code)}
                          style={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                            padding: '8px 12px',
                            backgroundColor: 'transparent',
                            border: `1px solid ${colors.purple}`,
                            color: colors.purple,
                            fontSize: '10px',
                            fontWeight: 700,
                            fontFamily: 'monospace',
                            letterSpacing: '0.5px',
                            cursor: 'pointer',
                            transition: 'all 0.15s'
                          }}
                          onMouseEnter={(e) => {
                            e.currentTarget.style.backgroundColor = colors.purple + '20';
                          }}
                          onMouseLeave={(e) => {
                            e.currentTarget.style.backgroundColor = 'transparent';
                          }}
                        >
                          <Copy size={12} />
                          COPY
                        </button>
                      </div>
                      <pre style={{
                        padding: '14px',
                        backgroundColor: colors.panel,
                        color: colors.text,
                        border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                        fontSize: '10px',
                        fontFamily: 'monospace',
                        overflowX: 'auto',
                        lineHeight: '1.5'
                      }}>
                        <code>{selectedFactor.code}</code>
                      </pre>
                    </div>

                    {/* Action Buttons */}
                    <div style={{ display: 'flex', gap: '12px', paddingTop: '16px', borderTop: `1px solid ${'var(--ft-border-color, #2A2A2A)'}` }}>
                      <button
                        style={{
                          flex: 1,
                          padding: '12px 16px',
                          backgroundColor: colors.purple,
                          border: 'none',
                          color: '#000000',
                          fontSize: '11px',
                          fontWeight: 700,
                          fontFamily: 'monospace',
                          letterSpacing: '0.5px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          gap: '8px',
                          transition: 'opacity 0.15s'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.opacity = '0.9';
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.opacity = '1';
                        }}
                      >
                        <Zap size={14} />
                        DEPLOY TO LIVE TRADING
                      </button>
                      <button
                        style={{
                          padding: '12px 24px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${colors.purple}`,
                          color: colors.purple,
                          fontSize: '11px',
                          fontWeight: 700,
                          fontFamily: 'monospace',
                          letterSpacing: '0.5px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          gap: '8px',
                          transition: 'all 0.15s'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.backgroundColor = colors.purple + '20';
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = 'transparent';
                        }}
                      >
                        <Download size={14} />
                        EXPORT
                      </button>
                    </div>
                  </div>
                </div>
              )}
            </div>
          ) : (
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              flex: 1,
              flexDirection: 'column',
              gap: '12px'
            }}>
              <Sparkles size={32} color={'var(--ft-border-color, #2A2A2A)'} />
              <div style={{
                fontSize: '11px',
                fontFamily: 'monospace',
                color: colors.text,
                opacity: 0.5,
                textAlign: 'center',
                letterSpacing: '0.5px'
              }}>
                NO FACTORS DISCOVERED YET
                <br />
                CONFIGURE AND START MINING
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

// Add keyframe animations
const style = document.createElement('style');
style.textContent = `
  @keyframes spin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }
  @keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }
`;
if (!document.querySelector('[data-factor-discovery-animations]')) {
  style.setAttribute('data-factor-discovery-animations', 'true');
  document.head.appendChild(style);
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
    <div style={{
      padding: '12px',
      backgroundColor: 'var(--ft-color-panel, #0a0a0a)',
      border: '1px solid var(--ft-border-color, #2A2A2A)',
      borderLeft: `3px solid ${color}`
    }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '8px'
      }}>
        <span style={{
          fontSize: 'var(--ft-font-size-tiny, 9px)',
          fontFamily: 'var(--ft-font-family, monospace)',
          color: 'var(--ft-color-text, #FFFFFF)',
          opacity: 0.5,
          letterSpacing: '0.5px'
        }}>
          {label.toUpperCase()}
        </span>
        <div style={{ color }}>{icon}</div>
      </div>
      <div style={{
        fontSize: '18px',
        fontWeight: 700,
        fontFamily: 'var(--ft-font-family, monospace)',
        color
      }}>
        {value}
      </div>
      {subtext && (
        <div style={{
          fontSize: 'var(--ft-font-size-tiny, 9px)',
          fontFamily: 'var(--ft-font-family, monospace)',
          color: 'var(--ft-color-text, #FFFFFF)',
          opacity: 0.5,
          marginTop: '4px'
        }}>
          {subtext}
        </div>
      )}
    </div>
  );
}
