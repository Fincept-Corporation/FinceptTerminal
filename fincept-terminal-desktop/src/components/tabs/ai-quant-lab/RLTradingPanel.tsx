/**
 * RL Trading Panel - Reinforcement Learning for Algorithmic Trading
 * Train RL agents (PPO, DQN, A2C, SAC, TD3) for automated trading strategies
 * REFACTORED: Terminal-style UI matching DeepAgent UX
 */

import React, { useState, useEffect, useRef } from 'react';
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
      backgroundColor: FINCEPT.DARK_BG
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Target size={16} color={FINCEPT.PURPLE} />
        <span style={{
          color: FINCEPT.PURPLE,
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
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.WHITE,
            fontSize: '10px',
            fontFamily: 'monospace',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            transition: 'all 0.15s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            e.currentTarget.style.borderColor = FINCEPT.PURPLE;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
          }}
        >
          <RefreshCw size={12} />
          RESET
        </button>
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: envCreated && modelTrained ? FINCEPT.GREEN + '20' : FINCEPT.DARK_BG,
          border: `1px solid ${envCreated && modelTrained ? FINCEPT.GREEN : FINCEPT.BORDER}`,
          color: envCreated && modelTrained ? FINCEPT.GREEN : FINCEPT.MUTED
        }}>
          {envCreated && modelTrained ? 'READY' : 'IDLE'}
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Algorithm Selection */}
        <div style={{
          width: '200px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.MUTED,
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
                    backgroundColor: isSelected ? FINCEPT.HOVER : 'transparent',
                    border: `1px solid ${isSelected ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                    cursor: envCreated ? 'not-allowed' : 'pointer',
                    opacity: envCreated ? 0.5 : 1,
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected && !envCreated) {
                      e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                      e.currentTarget.style.borderColor = FINCEPT.PURPLE;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected && !envCreated) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                    <Zap size={14} style={{ color: FINCEPT.PURPLE }} />
                    <span style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 600, fontFamily: 'monospace' }}>
                      {algo.name}
                    </span>
                  </div>
                  <p style={{ color: FINCEPT.MUTED, fontSize: '9px', margin: 0, lineHeight: '1.3' }}>
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
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Target size={14} style={{ color: FINCEPT.PURPLE }} />
              <span style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, fontFamily: 'monospace' }}>
                ENVIRONMENT CONFIGURATION
              </span>
              <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>•</span>
              <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
                {envCreated ? 'LOCKED' : 'READY TO CONFIGURE'}
              </span>
            </div>

            {/* Configuration Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
                  ALGORITHM
                </label>
                <div style={{
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.PURPLE}`,
                  color: FINCEPT.PURPLE,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  fontWeight: 600
                }}>
                  {algorithm}
                </div>
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: envCreated ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                        backgroundColor: actionSpaceType === type ? FINCEPT.PURPLE : FINCEPT.DARK_BG,
                        border: `1px solid ${actionSpaceType === type ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                        color: FINCEPT.WHITE,
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
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: (isTraining || modelTrained) ? 0.5 : 1
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
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
                  backgroundColor: envCreated ? FINCEPT.GREEN : FINCEPT.ORANGE,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
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
                  backgroundColor: modelTrained ? FINCEPT.GREEN : FINCEPT.PURPLE,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
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
                  backgroundColor: FINCEPT.CYAN,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
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
                  <span style={{ fontSize: '10px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>TRAINING PROGRESS</span>
                  <span style={{ fontSize: '10px', color: FINCEPT.PURPLE, fontFamily: 'monospace' }}>{progress}%</span>
                </div>
                <div style={{ width: '100%', height: '6px', backgroundColor: FINCEPT.DARK_BG, overflow: 'hidden' }}>
                  <div
                    style={{
                      width: `${progress}%`,
                      height: '100%',
                      backgroundColor: FINCEPT.PURPLE,
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
                backgroundColor: FINCEPT.RED + '15',
                border: `1px solid ${FINCEPT.RED}`,
                borderLeft: `3px solid ${FINCEPT.RED}`,
                color: FINCEPT.RED,
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
                  color: FINCEPT.CYAN,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px'
                }}>
                  <Cpu size={13} color={FINCEPT.CYAN} />
                  EXECUTION LOG
                </div>
                <div style={{
                  padding: '12px 14px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderLeft: `3px solid ${FINCEPT.CYAN}`,
                  maxHeight: '180px',
                  overflowY: 'auto',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  color: FINCEPT.CYAN,
                  lineHeight: '1.7'
                }}>
                  {logs.map((log, idx) => (
                    <div key={idx} style={{ marginBottom: '3px', color: log.includes('ERROR') ? FINCEPT.RED : log.includes('✓') ? FINCEPT.GREEN : FINCEPT.CYAN }}>
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
                  color: FINCEPT.GREEN,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px'
                }}>
                  <CheckCircle2 size={13} color={FINCEPT.GREEN} />
                  EVALUATION RESULTS
                </div>

                {/* Key Metrics */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '12px' }}>
                  <div style={{
                    padding: '12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${trainingMetrics.portfolio_return > 0 ? FINCEPT.GREEN : FINCEPT.RED}`,
                    borderLeft: `3px solid ${trainingMetrics.portfolio_return > 0 ? FINCEPT.GREEN : FINCEPT.RED}`
                  }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: 'monospace' }}>
                      PORTFOLIO RETURN
                    </div>
                    <div style={{
                      fontSize: '20px',
                      fontWeight: 700,
                      color: trainingMetrics.portfolio_return > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                      fontFamily: 'monospace'
                    }}>
                      {trainingMetrics.portfolio_return > 0 ? '+' : ''}{trainingMetrics.portfolio_return.toFixed(2)}%
                    </div>
                  </div>

                  <div style={{
                    padding: '12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.CYAN}`,
                    borderLeft: `3px solid ${FINCEPT.CYAN}`
                  }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: 'monospace' }}>
                      PORTFOLIO VALUE
                    </div>
                    <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: 'monospace' }}>
                      ${(trainingMetrics.mean_portfolio_value / 1000).toFixed(0)}K
                    </div>
                  </div>
                </div>

                {/* Detailed Metrics */}
                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: FINCEPT.GRAY, fontFamily: 'monospace' }}>Mean Reward:</span>
                      <span style={{ color: FINCEPT.WHITE, fontFamily: 'monospace', fontWeight: 600 }}>
                        {trainingMetrics.mean_reward.toFixed(4)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: FINCEPT.GRAY, fontFamily: 'monospace' }}>Std Reward:</span>
                      <span style={{ color: FINCEPT.WHITE, fontFamily: 'monospace', fontWeight: 600 }}>
                        {trainingMetrics.std_reward.toFixed(4)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: FINCEPT.GRAY, fontFamily: 'monospace' }}>Episode Length:</span>
                      <span style={{ color: FINCEPT.WHITE, fontFamily: 'monospace', fontWeight: 600 }}>
                        {trainingMetrics.mean_length}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                      <span style={{ color: FINCEPT.GRAY, fontFamily: 'monospace' }}>Episodes:</span>
                      <span style={{ color: FINCEPT.WHITE, fontFamily: 'monospace', fontWeight: 600 }}>
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
                <Brain size={48} color={FINCEPT.MUTED} style={{ margin: '0 auto 16px' }} />
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, marginBottom: '8px', fontFamily: 'monospace' }}>
                  Ready to Train RL Agent
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.5' }}>
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
