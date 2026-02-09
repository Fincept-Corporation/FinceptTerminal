/**
 * RL Trading Panel - Reinforcement Learning for Algorithmic Trading
 * Train RL agents (PPO, DQN, A2C, SAC, TD3) for automated trading strategies
 * REFACTORED: Terminal-style UI matching DeepAgent UX
 */

import React, { useState, useEffect, useRef } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import {
  Brain,
  Play,
  Square,
  TrendingUp,
  Activity,
  Target,
  AlertCircle,
  CheckCircle2,
  BarChart3,
  Zap,
  RefreshCw,
  Download,
  Upload,
  Settings,
  Award,
  Info,
  Cpu,
  Network,
  Loader2,
  ChevronRight,
  ChevronDown,
  Clock
} from 'lucide-react';


interface TrainingMetrics {
  mean_reward: number;
  std_reward: number;
  mean_length: number;
  mean_portfolio_value: number;
  portfolio_return: number;
  episode_count: number;
}

interface EnvironmentConfig {
  tickers: string[];
  start_date: string;
  end_date: string;
  initial_cash: number;
  commission: number;
  action_space_type: 'continuous' | 'discrete';
}

export function RLTradingPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  // Environment Configuration
  const [tickers, setTickers] = useState('AAPL,MSFT,GOOGL,NVDA,TSLA');
  const [startDate, setStartDate] = useState('2023-01-01');
  const [endDate, setEndDate] = useState('2024-12-31');
  const [initialCash, setInitialCash] = useState(1000000);
  const [commission, setCommission] = useState(0.001);
  const [actionSpaceType, setActionSpaceType] = useState<'continuous' | 'discrete'>('continuous');

  // RL Algorithm Configuration
  const [algorithm, setAlgorithm] = useState('PPO');
  const [totalTimesteps, setTotalTimesteps] = useState(100000);
  const [learningRate, setLearningRate] = useState(0.0003);

  // State
  const [isTraining, setIsTraining] = useState(false);
  const [isEvaluating, setIsEvaluating] = useState(false);
  const [trainingMetrics, setTrainingMetrics] = useState<TrainingMetrics | null>(null);
  const [envCreated, setEnvCreated] = useState(false);
  const [modelTrained, setModelTrained] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [progress, setProgress] = useState(0);
  const [logs, setLogs] = useState<string[]>([]);

  const algorithms = [
    { id: 'PPO', name: 'PPO', desc: 'Proximal Policy Optimization - Best for continuous actions' },
    { id: 'A2C', name: 'A2C', desc: 'Advantage Actor-Critic - Fast baseline' },
    { id: 'DQN', name: 'DQN', desc: 'Deep Q-Network - For discrete actions' },
    { id: 'SAC', name: 'SAC', desc: 'Soft Actor-Critic - Off-policy, continuous' },
    { id: 'TD3', name: 'TD3', desc: 'Twin Delayed DDPG - Robust continuous control' }
  ];

  const addLog = (message: string) => {
    const timestamp = new Date().toLocaleTimeString();
    setLogs(prev => [`[${timestamp}] ${message}`, ...prev.slice(0, 99)]);
  };

  const handleCreateEnvironment = async () => {
    try {
      setError(null);
      addLog('Creating trading environment...');

      const tickerList = tickers.split(',').map(t => t.trim());

      // Call backend service (will be implemented)
      // const result = await rlService.createEnvironment({
      //   tickers: tickerList,
      //   start_date: startDate,
      //   end_date: endDate,
      //   initial_cash: initialCash,
      //   commission,
      //   action_space_type: actionSpaceType
      // });

      // Simulate for now
      await new Promise(resolve => setTimeout(resolve, 1500));

      setEnvCreated(true);
      addLog(`Environment created: ${tickerList.length} assets, ${actionSpaceType} actions`);
      addLog(`Period: ${startDate} to ${endDate}`);
    } catch (err: any) {
      setError(err.message || 'Failed to create environment');
      addLog(`ERROR: ${err.message}`);
    }
  };

  const handleTrainAgent = async () => {
    if (!envCreated) {
      setError('Please create environment first');
      return;
    }

    try {
      setIsTraining(true);
      setError(null);
      setProgress(0);
      addLog(`Starting ${algorithm} training...`);
      addLog(`Timesteps: ${totalTimesteps}, LR: ${learningRate}`);

      // Simulate training progress
      const interval = setInterval(() => {
        setProgress(prev => {
          const next = prev + 5;
          if (next >= 100) {
            clearInterval(interval);
            return 100;
          }
          return next;
        });
      }, 500);

      // Call backend service
      // const result = await rlService.trainAgent({
      //   algorithm,
      //   total_timesteps: totalTimesteps,
      //   learning_rate: learningRate
      // });

      // Simulate
      await new Promise(resolve => setTimeout(resolve, 10000));

      setModelTrained(true);
      setIsTraining(false);
      clearInterval(interval);
      setProgress(100);
      addLog(`${algorithm} training completed!`);
      addLog('Model ready for evaluation');
    } catch (err: any) {
      setIsTraining(false);
      setError(err.message || 'Training failed');
      addLog(`ERROR: ${err.message}`);
    }
  };

  const handleEvaluate = async () => {
    if (!modelTrained) {
      setError('Please train model first');
      return;
    }

    try {
      setIsEvaluating(true);
      setError(null);
      addLog('Evaluating trained agent...');

      // Call backend service
      // const result = await rlService.evaluateAgent(10);

      // Simulate evaluation results
      await new Promise(resolve => setTimeout(resolve, 3000));

      const mockResults: TrainingMetrics = {
        mean_reward: 0.0234,
        std_reward: 0.0089,
        mean_length: 245,
        mean_portfolio_value: 1234567,
        portfolio_return: 23.46,
        episode_count: 10
      };

      setTrainingMetrics(mockResults);
      setIsEvaluating(false);
      addLog('Evaluation complete!');
      addLog(`Mean Return: ${mockResults.portfolio_return.toFixed(2)}%`);
      addLog(`Portfolio Value: $${mockResults.mean_portfolio_value.toLocaleString()}`);
    } catch (err: any) {
      setIsEvaluating(false);
      setError(err.message || 'Evaluation failed');
      addLog(`ERROR: ${err.message}`);
    }
  };

  const handleReset = () => {
    setEnvCreated(false);
    setModelTrained(false);
    setTrainingMetrics(null);
    setProgress(0);
    setLogs([]);
    addLog('Reset complete - ready for new training');
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: colors.background
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
        backgroundColor: colors.panel,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Target size={16} color={colors.purple} />
        <span style={{
          color: colors.purple,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          RL TRADING AUTOMATION
        </span>
        <div style={{ flex: 1 }} />
        <button
          onClick={handleReset}
          style={{
            padding: '4px 12px',
            backgroundColor: colors.background,
            border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            color: colors.text,
            fontSize: '10px',
            fontFamily: 'monospace',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            transition: 'all 0.15s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#1F1F1F';
            e.currentTarget.style.borderColor = colors.purple;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = colors.background;
            e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
          }}
        >
          <RefreshCw size={12} />
          RESET
        </button>
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: envCreated && modelTrained ? colors.success + '20' : colors.background,
          border: `1px solid ${envCreated && modelTrained ? colors.success : 'var(--ft-border-color, #2A2A2A)'}`,
          color: envCreated && modelTrained ? colors.success : colors.textMuted
        }}>
          {envCreated && modelTrained ? 'READY' : 'IDLE'}
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Algorithm Selection */}
        <div style={{
          width: '200px',
          borderRight: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          backgroundColor: colors.panel,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.textMuted,
            fontFamily: 'monospace',
            letterSpacing: '0.5px'
          }}>
            ALGORITHM TYPE
          </div>
          <div style={{ padding: '8px', display: 'flex', flexDirection: 'column', gap: '6px', overflowY: 'auto' }}>
            {algorithms.map(algo => {
              const isSelected = algorithm === algo.id;
              return (
                <div
                  key={algo.id}
                  onClick={() => !envCreated && setAlgorithm(algo.id)}
                  style={{
                    padding: '10px',
                    backgroundColor: isSelected ? '#1F1F1F' : 'transparent',
                    border: `1px solid ${isSelected ? colors.purple : 'var(--ft-border-color, #2A2A2A)'}`,
                    cursor: envCreated ? 'not-allowed' : 'pointer',
                    opacity: envCreated ? 0.5 : 1,
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected && !envCreated) {
                      e.currentTarget.style.backgroundColor = colors.background;
                      e.currentTarget.style.borderColor = colors.purple;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected && !envCreated) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                    <Zap size={14} style={{ color: colors.purple }} />
                    <span style={{ color: colors.text, fontSize: '11px', fontWeight: 600, fontFamily: 'monospace' }}>
                      {algo.name}
                    </span>
                  </div>
                  <p style={{ color: colors.textMuted, fontSize: '9px', margin: 0, lineHeight: '1.3' }}>
                    {algo.desc}
                  </p>
                </div>
              );
            })}
          </div>
        </div>

        {/* Main Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          {/* Configuration Section */}
          <div style={{
            padding: '16px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Target size={14} style={{ color: colors.purple }} />
              <span style={{ fontSize: '11px', fontWeight: 600, color: colors.text, fontFamily: 'monospace' }}>
                ENVIRONMENT CONFIGURATION
              </span>
              <span style={{ fontSize: '10px', color: colors.textMuted }}>•</span>
              <span style={{ fontSize: '10px', color: colors.textMuted }}>
                {envCreated ? 'LOCKED' : 'READY TO CONFIGURE'}
              </span>
            </div>

            {/* Configuration Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  TICKERS
                </label>
                <input
                  type="text"
                  value={tickers}
                  onChange={(e) => setTickers(e.target.value)}
                  disabled={envCreated}
                  placeholder="AAPL,MSFT,GOOGL"
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  ALGORITHM
                </label>
                <div style={{
                  padding: '8px 10px',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.purple}`,
                  color: colors.purple,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  fontWeight: 600
                }}>
                  {algorithm}
                </div>
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  START DATE
                </label>
                <input
                  type="date"
                  value={startDate}
                  onChange={(e) => setStartDate(e.target.value)}
                  disabled={envCreated}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  END DATE
                </label>
                <input
                  type="date"
                  value={endDate}
                  onChange={(e) => setEndDate(e.target.value)}
                  disabled={envCreated}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  INITIAL CASH ($)
                </label>
                <input
                  type="text"
                  inputMode="numeric"
                  value={initialCash}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setInitialCash(Number(v) || 0); }}
                  disabled={envCreated}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  COMMISSION (%)
                </label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={commission}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setCommission(Number(v) || 0); }}
                  disabled={envCreated}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  ACTION SPACE
                </label>
                <div style={{ display: 'flex', gap: '8px' }}>
                  {['continuous', 'discrete'].map(type => (
                    <button
                      key={type}
                      onClick={() => !envCreated && setActionSpaceType(type as any)}
                      disabled={envCreated}
                      style={{
                        flex: 1,
                        padding: '8px 10px',
                        backgroundColor: actionSpaceType === type ? colors.purple : colors.background,
                        border: `1px solid ${actionSpaceType === type ? colors.purple : 'var(--ft-border-color, #2A2A2A)'}`,
                        color: colors.text,
                        fontSize: '10px',
                        fontFamily: 'monospace',
                        cursor: envCreated ? 'not-allowed' : 'pointer',
                        opacity: envCreated ? 0.5 : 1,
                        transition: 'all 0.15s',
                        textTransform: 'uppercase'
                      }}
                    >
                      {type}
                    </button>
                  ))}
                </div>
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  TIMESTEPS
                </label>
                <input
                  type="text"
                  inputMode="numeric"
                  value={totalTimesteps}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setTotalTimesteps(Number(v) || 0); }}
                  disabled={isTraining || modelTrained}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: (isTraining || modelTrained) ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  LEARNING RATE
                </label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={learningRate}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setLearningRate(Number(v) || 0); }}
                  disabled={isTraining || modelTrained}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: (isTraining || modelTrained) ? 0.5 : 1
                  }}
                />
              </div>
            </div>

            {/* Action Buttons */}
            <div style={{ display: 'flex', gap: '12px', marginTop: '12px' }}>
              <button
                onClick={handleCreateEnvironment}
                disabled={envCreated}
                style={{
                  flex: 1,
                  padding: '10px 16px',
                  backgroundColor: envCreated ? colors.success : colors.primary,
                  border: 'none',
                  color: colors.background,
                  fontSize: '11px',
                  fontWeight: 700,
                  cursor: envCreated ? 'not-allowed' : 'pointer',
                  opacity: envCreated ? 0.6 : 1,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  transition: 'all 0.15s'
                }}
              >
                {envCreated ? <CheckCircle2 size={14} /> : <Play size={14} />}
                {envCreated ? 'ENVIRONMENT CREATED' : 'CREATE ENVIRONMENT'}
              </button>

              <button
                onClick={handleTrainAgent}
                disabled={!envCreated || isTraining || modelTrained}
                style={{
                  flex: 1,
                  padding: '10px 16px',
                  backgroundColor: modelTrained ? colors.success : colors.purple,
                  border: 'none',
                  color: colors.background,
                  fontSize: '11px',
                  fontWeight: 700,
                  cursor: (!envCreated || isTraining || modelTrained) ? 'not-allowed' : 'pointer',
                  opacity: (!envCreated || isTraining || modelTrained) ? 0.6 : 1,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  transition: 'all 0.15s'
                }}
              >
                {isTraining ? (
                  <>
                    <Loader2 size={14} className="animate-spin" />
                    TRAINING...
                  </>
                ) : modelTrained ? (
                  <>
                    <CheckCircle2 size={14} />
                    MODEL TRAINED
                  </>
                ) : (
                  <>
                    <Zap size={14} />
                    TRAIN AGENT
                  </>
                )}
              </button>

              <button
                onClick={handleEvaluate}
                disabled={!modelTrained || isEvaluating}
                style={{
                  flex: 1,
                  padding: '10px 16px',
                  backgroundColor: colors.accent,
                  border: 'none',
                  color: colors.background,
                  fontSize: '11px',
                  fontWeight: 700,
                  cursor: (!modelTrained || isEvaluating) ? 'not-allowed' : 'pointer',
                  opacity: (!modelTrained || isEvaluating) ? 0.6 : 1,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  transition: 'all 0.15s'
                }}
              >
                {isEvaluating ? (
                  <>
                    <Loader2 size={14} className="animate-spin" />
                    EVALUATING...
                  </>
                ) : (
                  <>
                    <Target size={14} />
                    EVALUATE
                  </>
                )}
              </button>
            </div>

            {/* Training Progress */}
            {isTraining && (
              <div style={{ marginTop: '12px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                  <span style={{ fontSize: '10px', color: colors.textMuted, fontFamily: 'monospace' }}>TRAINING PROGRESS</span>
                  <span style={{ fontSize: '10px', color: colors.purple, fontFamily: 'monospace' }}>{progress}%</span>
                </div>
                <div style={{ width: '100%', height: '6px', backgroundColor: colors.background, overflow: 'hidden' }}>
                  <div
                    style={{
                      width: `${progress}%`,
                      height: '100%',
                      backgroundColor: colors.purple,
                      transition: 'width 0.3s ease'
                    }}
                  />
                </div>
              </div>
            )}
          </div>

          {/* Results Area */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {/* Error Display */}
            {error && (
              <div style={{
                padding: '12px 14px',
                backgroundColor: colors.alert + '15',
                border: `1px solid ${colors.alert}`,
                borderLeft: `3px solid ${colors.alert}`,
                color: colors.alert,
                fontSize: '13px',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                fontFamily: 'monospace',
                marginBottom: '16px',
                lineHeight: '1.5'
              }}>
                <AlertCircle size={16} style={{ flexShrink: 0 }} />
                {error}
              </div>
            )}

            {/* Execution Log */}
            {logs.length > 0 && (
              <div style={{ marginBottom: '16px' }}>
                <div style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: colors.accent,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px'
                }}>
                  <Cpu size={13} color={colors.accent} />
                  EXECUTION LOG
                </div>
                <div style={{
                  padding: '12px 14px',
                  backgroundColor: colors.panel,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  borderLeft: `3px solid ${colors.accent}`,
                  maxHeight: '180px',
                  overflowY: 'auto',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  color: colors.accent,
                  lineHeight: '1.7'
                }}>
                  {logs.map((log, idx) => (
                    <div key={idx} style={{ marginBottom: '3px', color: log.includes('ERROR') ? colors.alert : log.includes('✓') ? colors.success : colors.accent }}>
                      {log}
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* Evaluation Results */}
            {trainingMetrics && (
              <div style={{ marginBottom: '16px' }}>
                <div style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: colors.success,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px'
                }}>
                  <CheckCircle2 size={13} color={colors.success} />
                  EVALUATION RESULTS
                </div>

                {/* Key Metrics */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '12px' }}>
                  <div style={{
                    padding: '12px',
                    backgroundColor: colors.panel,
                    border: `1px solid ${trainingMetrics.portfolio_return > 0 ? colors.success : colors.alert}`,
                    borderLeft: `3px solid ${trainingMetrics.portfolio_return > 0 ? colors.success : colors.alert}`
                  }}>
                    <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace' }}>
                      PORTFOLIO RETURN
                    </div>
                    <div style={{
                      fontSize: '20px',
                      fontWeight: 700,
                      color: trainingMetrics.portfolio_return > 0 ? colors.success : colors.alert,
                      fontFamily: 'monospace'
                    }}>
                      {trainingMetrics.portfolio_return > 0 ? '+' : ''}{trainingMetrics.portfolio_return.toFixed(2)}%
                    </div>
                  </div>

                  <div style={{
                    padding: '12px',
                    backgroundColor: colors.panel,
                    border: `1px solid ${colors.accent}`,
                    borderLeft: `3px solid ${colors.accent}`
                  }}>
                    <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace' }}>
                      PORTFOLIO VALUE
                    </div>
                    <div style={{ fontSize: '20px', fontWeight: 700, color: colors.accent, fontFamily: 'monospace' }}>
                      ${(trainingMetrics.mean_portfolio_value / 1000).toFixed(0)}K
                    </div>
                  </div>
                </div>

                {/* Detailed Metrics */}
                <div style={{
                  padding: '12px',
                  backgroundColor: colors.panel,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                }}>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: colors.textMuted, fontFamily: 'monospace' }}>Mean Reward:</span>
                      <span style={{ color: colors.text, fontFamily: 'monospace', fontWeight: 600 }}>
                        {trainingMetrics.mean_reward.toFixed(4)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: colors.textMuted, fontFamily: 'monospace' }}>Std Reward:</span>
                      <span style={{ color: colors.text, fontFamily: 'monospace', fontWeight: 600 }}>
                        {trainingMetrics.std_reward.toFixed(4)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: colors.textMuted, fontFamily: 'monospace' }}>Episode Length:</span>
                      <span style={{ color: colors.text, fontFamily: 'monospace', fontWeight: 600 }}>
                        {trainingMetrics.mean_length}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: colors.textMuted, fontFamily: 'monospace' }}>Episodes:</span>
                      <span style={{ color: colors.text, fontFamily: 'monospace', fontWeight: 600 }}>
                        {trainingMetrics.episode_count}
                      </span>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Empty State */}
            {!isTraining && !isEvaluating && logs.length === 0 && !trainingMetrics && (
              <div style={{
                padding: '40px 20px',
                textAlign: 'center'
              }}>
                <Brain size={48} color={colors.textMuted} style={{ margin: '0 auto 16px' }} />
                <div style={{ fontSize: '11px', color: colors.text, marginBottom: '8px', fontFamily: 'monospace' }}>
                  Ready to Train RL Agent
                </div>
                <div style={{ fontSize: '10px', color: colors.textMuted, lineHeight: '1.5' }}>
                  Configure your environment, select an algorithm, and start training.
                  <br />Results and logs will appear here.
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
