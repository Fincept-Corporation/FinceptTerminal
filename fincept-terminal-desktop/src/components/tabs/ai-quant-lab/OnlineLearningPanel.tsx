/**
 * Online Learning Panel - Real-time Model Updates & Drift Detection
 * Incremental learning with concept drift detection for live trading
 * Connected to Python backend via River library
 */

import React, { useState, useEffect, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import {
  Zap,
  Activity,
  AlertTriangle,
  TrendingUp,
  RefreshCw,
  CheckCircle2,
  Play,
  Pause,
  BarChart3,
  Bell,
  Settings,
  Clock,
  Cpu,
  Network,
  Loader2,
  AlertCircle,
  HelpCircle,
  BookOpen,
  X
} from 'lucide-react';

interface ModelMetrics {
  model_id: string;
  model_type: string;
  current_mae: number;
  samples_trained: number;
  last_updated: string | null;
  created_at: string;
  drift_detected: boolean;
}

interface UpdateHistory {
  timestamp: string;
  mae: number;
  samples: number;
  drift: boolean;
}

export function OnlineLearningPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [modelType, setModelType] = useState('linear');
  const [modelId, setModelId] = useState<string | null>(null);
  const [isLive, setIsLive] = useState(false);
  const [metrics, setMetrics] = useState<ModelMetrics | null>(null);
  const [updateHistory, setUpdateHistory] = useState<UpdateHistory[]>([]);
  const [updateFrequency, setUpdateFrequency] = useState('daily');
  const [driftStrategy, setDriftStrategy] = useState('retrain');
  const [error, setError] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [showHelp, setShowHelp] = useState(false);
  const liveIntervalRef = useRef<NodeJS.Timeout | null>(null);

  const modelTypes = [
    // Linear models
    {
      id: 'linear',
      name: 'Linear Regression',
      desc: 'Fast, interpretable',
      helpText: 'Simple prediction model - learns straight-line relationships. Good for stable markets. Example: Predicting stock price based on volume.'
    },
    {
      id: 'bayesian_linear',
      name: 'Bayesian Linear',
      desc: 'Probabilistic linear model',
      helpText: 'Like linear regression but gives confidence levels. Useful when you need to know prediction reliability. Example: "90% confident price will be $100-$105"'
    },
    {
      id: 'pa',
      name: 'Passive-Aggressive',
      desc: 'Margin-based regression',
      helpText: 'Adapts aggressively to errors. Good for volatile markets with sudden changes. Example: Crypto trading with rapid price swings.'
    },

    // Tree models
    {
      id: 'tree',
      name: 'Hoeffding Tree',
      desc: 'Decision tree for streams',
      helpText: 'Makes decisions like a flowchart - handles complex patterns. Example: "If volume > 1M AND RSI < 30, predict price increase"'
    },
    {
      id: 'adaptive_tree',
      name: 'Adaptive Hoeffding Tree',
      desc: 'Adapts to drift',
      helpText: 'Decision tree that rebuilds itself when market conditions change. Best for trending markets. Example: Detects bull→bear market shifts.'
    },
    {
      id: 'isoup_tree',
      name: 'iSOUP Tree',
      desc: 'Incremental tree',
      helpText: 'Memory-efficient decision tree. Good for limited computing resources. Example: Running on mobile devices or low-spec servers.'
    },

    // Ensemble models
    {
      id: 'bagging',
      name: 'Bagging Ensemble',
      desc: 'Bootstrap aggregating',
      helpText: 'Combines 10 decision trees voting together - more accurate but slower. Example: Hedge fund strategy combining multiple signals.'
    },
    {
      id: 'ewa',
      name: 'Exponentially Weighted Average',
      desc: 'Weighted ensemble',
      helpText: 'Combines 3 models, giving more weight to recent performers. Example: Portfolio that adapts to which strategy works best now.'
    },
    {
      id: 'srp',
      name: 'Streaming Random Patches',
      desc: 'Random subspace ensemble',
      helpText: 'Each model looks at different features - robust to noise. Example: One model uses price data, another uses volume, another uses technicals.'
    }
  ];

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (liveIntervalRef.current) {
        clearInterval(liveIntervalRef.current);
      }
    };
  }, []);

  const handleCreateModel = async () => {
    try {
      setError(null);
      setIsLoading(true);

      // Create model via Python backend
      const params = JSON.stringify({
        model_type: modelType
      });

      const result = await invoke<string>('qlib_online_create_model', { params });
      const data = JSON.parse(result);

      if (!data.success) {
        throw new Error(data.error || 'Failed to create model');
      }

      setModelId(data.model_id);

      // Get initial performance
      const perfResult = await invoke<string>('qlib_online_performance', { modelId: data.model_id });
      const perfData = JSON.parse(perfResult);

      if (perfData.success) {
        setMetrics(perfData as ModelMetrics);
      }

    } catch (err: any) {
      setError(err.message || 'Failed to create model');
    } finally {
      setIsLoading(false);
    }
  };

  const generateMockFeatures = () => {
    // Generate normalized features for demonstration (0-1 scale)
    const basePrice = 0.5 + Math.random() * 0.2; // 0.5-0.7 range
    return {
      open: basePrice + (Math.random() - 0.5) * 0.01,
      high: basePrice + Math.random() * 0.01,
      low: basePrice - Math.random() * 0.01,
      close: basePrice,
      volume: Math.random() * 0.5 + 0.5, // 0.5-1.0
      rsi: Math.random(), // 0-1
      macd: (Math.random() - 0.5) * 0.1 // -0.05 to 0.05
    };
  };

  const handleStartLive = async () => {
    if (!modelId) return;

    // Set live immediately for instant UI feedback
    setIsLive(true);

    try {
      setError(null);

      // Setup rolling update schedule in background
      const scheduleParams = JSON.stringify({
        model_id: modelId,
        update_frequency: updateFrequency,
        retrain_window: 252,
        min_samples: 100
      });

      invoke<string>('qlib_online_setup_rolling', { params: scheduleParams }).catch(err => {
        console.warn('Rolling schedule setup failed:', err);
      });

      // Start simulated live trading with incremental learning
      liveIntervalRef.current = setInterval(async () => {
        try {
          const features = generateMockFeatures();
          const target = features.close + (Math.random() - 0.5) * 0.01; // Target is future price (+/- 1%)

          // Train and get performance in parallel (fire and forget for smoother UI)
          invoke<string>('qlib_online_train', {
            modelId: modelId,
            features: JSON.stringify(features),
            target: target.toString()
          }).then(trainResult => {
            const trainData = JSON.parse(trainResult);
            if (trainData.success) {
              // Get updated performance
              return invoke<string>('qlib_online_performance', { modelId });
            }
          }).then(perfResult => {
            if (perfResult) {
              const perfData = JSON.parse(perfResult);
              if (perfData.success) {
                setMetrics(perfData as ModelMetrics);

                // Add to history
                const update: UpdateHistory = {
                  timestamp: new Date().toISOString(),
                  mae: perfData.current_mae,
                  samples: perfData.samples_trained,
                  drift: perfData.drift_detected
                };
                setUpdateHistory(hist => [update, ...hist.slice(0, 49)]);

                // Handle drift if detected
                if (perfData.drift_detected) {
                  const driftParams = JSON.stringify({
                    model_id: modelId,
                    strategy: driftStrategy
                  });
                  invoke<string>('qlib_online_handle_drift', { params: driftParams }).catch(console.error);
                }
              }
            }
          }).catch(err => {
            console.error('Live update error:', err);
          });
        } catch (err) {
          console.error('Live update error:', err);
        }
      }, 2000); // Update every 2 seconds

    } catch (err: any) {
      setError(err.message || 'Failed to start live updates');
    }
  };

  const handleStopLive = () => {
    setIsLive(false);
    if (liveIntervalRef.current) {
      clearInterval(liveIntervalRef.current);
      liveIntervalRef.current = null;
    }
  };

  const handleResetModel = () => {
    if (liveIntervalRef.current) {
      clearInterval(liveIntervalRef.current);
      liveIntervalRef.current = null;
    }
    setIsLive(false);
    setModelId(null);
    setMetrics(null);
    setUpdateHistory([]);
    setError(null);
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
        <Zap size={16} color={colors.warning} />
        <span style={{
          color: colors.warning,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          ONLINE LEARNING SYSTEM
        </span>

        {/* Help Button */}
        <button
          onClick={() => setShowHelp(!showHelp)}
          style={{
            padding: '4px 8px',
            backgroundColor: showHelp ? colors.primary : 'transparent',
            border: `1px solid ${colors.primary}`,
            color: showHelp ? colors.background : colors.primary,
            fontSize: '10px',
            fontFamily: 'monospace',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            fontWeight: 600,
            transition: 'all 0.15s'
          }}
        >
          <BookOpen size={12} />
          {showHelp ? 'HIDE GUIDE' : 'SHOW GUIDE'}
        </button>

        <div style={{ flex: 1 }} />

        {/* Status Indicators */}
        {modelId && (
          <>
            <div style={{
              fontSize: '10px',
              fontFamily: 'monospace',
              padding: '3px 8px',
              backgroundColor: colors.background,
              border: `1px solid ${colors.accent}`,
              color: colors.accent
            }}>
              MODEL: {modelId.substring(0, 12)}...
            </div>
            {isLive && (
              <div style={{
                fontSize: '10px',
                fontFamily: 'monospace',
                padding: '3px 8px',
                backgroundColor: colors.success + '20',
                border: `1px solid ${colors.success}`,
                color: colors.success,
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}>
                <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: colors.success, animation: 'pulse 2s infinite' }} />
                LIVE
              </div>
            )}
          </>
        )}

        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: modelId ? (isLive ? colors.success + '20' : colors.background) : colors.background,
          border: `1px solid ${modelId ? (isLive ? colors.success : 'var(--ft-border-color, #2A2A2A)') : 'var(--ft-border-color, #2A2A2A)'}`,
          color: modelId ? (isLive ? colors.success : colors.textMuted) : colors.textMuted
        }}>
          {isLive ? 'ACTIVE' : modelId ? 'READY' : 'IDLE'}
        </div>
      </div>

      {/* Help Panel */}
      {showHelp && (
        <div style={{
          padding: '16px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          maxHeight: '40vh',
          overflowY: 'auto'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'start', marginBottom: '12px' }}>
            <div>
              <div style={{ fontSize: '13px', fontWeight: 700, color: colors.primary, marginBottom: '4px', fontFamily: 'monospace' }}>
                📚 WHAT IS ONLINE LEARNING?
              </div>
              <div style={{ fontSize: '11px', color: colors.text, lineHeight: '1.6', marginBottom: '12px' }}>
                Online Learning is a type of machine learning where the model learns <strong>continuously</strong> from new data,
                one sample at a time, instead of training on all historical data at once.
              </div>
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '12px' }}>
            <div style={{ padding: '12px', backgroundColor: colors.background, border: `1px solid ${colors.accent}` }}>
              <div style={{ fontSize: '11px', fontWeight: 700, color: colors.accent, marginBottom: '6px', fontFamily: 'monospace' }}>
                💡 WHY USE IT IN FINANCE?
              </div>
              <ul style={{ fontSize: '10px', color: colors.text, lineHeight: '1.5', margin: 0, paddingLeft: '16px' }}>
                <li><strong>Real-time adaptation:</strong> Markets change constantly - model adapts instantly</li>
                <li><strong>Memory efficient:</strong> Doesn't need to store years of historical data</li>
                <li><strong>Low latency:</strong> Fast predictions for high-frequency trading</li>
                <li><strong>Drift detection:</strong> Alerts when market regime changes</li>
              </ul>
            </div>

            <div style={{ padding: '12px', backgroundColor: colors.background, border: `1px solid ${colors.warning}` }}>
              <div style={{ fontSize: '11px', fontWeight: 700, color: colors.warning, marginBottom: '6px', fontFamily: 'monospace' }}>
                🎯 REAL-WORLD USE CASES
              </div>
              <ul style={{ fontSize: '10px', color: colors.text, lineHeight: '1.5', margin: 0, paddingLeft: '16px' }}>
                <li><strong>Intraday trading:</strong> Update model with every tick</li>
                <li><strong>Portfolio rebalancing:</strong> Adjust weights as correlations shift</li>
                <li><strong>Risk management:</strong> Detect volatility regime changes</li>
                <li><strong>Market making:</strong> Adapt spread in real-time</li>
              </ul>
            </div>
          </div>

          <div style={{ padding: '12px', backgroundColor: colors.background, border: `1px solid ${colors.success}`, marginBottom: '12px' }}>
            <div style={{ fontSize: '11px', fontWeight: 700, color: colors.success, marginBottom: '8px', fontFamily: 'monospace' }}>
              📊 KEY METRICS EXPLAINED
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', fontSize: '10px', color: colors.text }}>
              <div>
                <strong style={{ color: colors.accent }}>MAE (Mean Absolute Error):</strong> Average prediction error in dollars.
                Lower is better. Example: MAE of 0.50 means predictions are off by $0.50 on average.
              </div>
              <div>
                <strong style={{ color: colors.success }}>Samples Trained:</strong> Number of data points the model has learned from.
                More samples = more experience. Example: 10,000 samples = 10,000 price predictions made and learned from.
              </div>
              <div>
                <strong style={{ color: colors.alert }}>Drift Detected:</strong> Market conditions changed significantly.
                Algorithm detected your model is becoming inaccurate. Example: Bull market → Bear market shift detected.
              </div>
              <div>
                <strong style={{ color: colors.textMuted }}>Update Frequency:</strong> How often to retrain the model.
                Hourly = Every hour, Daily = End of day, Weekly = End of week. High-frequency trading uses hourly.
              </div>
            </div>
          </div>

          <div style={{ padding: '12px', backgroundColor: colors.background, border: `1px solid ${colors.alert}` }}>
            <div style={{ fontSize: '11px', fontWeight: 700, color: colors.alert, marginBottom: '8px', fontFamily: 'monospace' }}>
              ⚙️ DRIFT STRATEGIES
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', fontSize: '10px', color: colors.text }}>
              <div>
                <strong style={{ color: colors.alert }}>Retrain:</strong> Resets model to fresh state when drift detected.
                Conservative approach. Use when: Market fundamentally changed (e.g., Fed policy shift, crisis).
              </div>
              <div>
                <strong style={{ color: colors.warning }}>Adaptive LR:</strong> Increases learning rate temporarily to adapt faster.
                Aggressive approach. Use when: Temporary volatility spike (e.g., earnings announcement, news event).
              </div>
            </div>
          </div>

          <div style={{ fontSize: '10px', color: colors.textMuted, marginTop: '12px', fontStyle: 'italic', textAlign: 'center' }}>
            💡 Tip: Start with Linear Regression on daily frequency to understand the system, then experiment with tree models on hourly for better accuracy.
          </div>
        </div>
      )}

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Model Type Selection */}
        <div style={{
          width: '240px',
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
            letterSpacing: '0.5px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}>
            MODEL TYPE
            <HelpCircle
              size={12}
              style={{ color: colors.primary, cursor: 'pointer' }}
            />
          </div>
          <div style={{ padding: '8px', display: 'flex', flexDirection: 'column', gap: '6px', overflowY: 'auto' }}>
            {modelTypes.map(type => {
              const isSelected = modelType === type.id;
              return (
                <div
                  key={type.id}
                  onClick={() => !modelId && setModelType(type.id)}
                  style={{
                    padding: '10px',
                    backgroundColor: isSelected ? '#1F1F1F' : 'transparent',
                    border: `1px solid ${isSelected ? colors.warning : 'var(--ft-border-color, #2A2A2A)'}`,
                    cursor: modelId ? 'not-allowed' : 'pointer',
                    opacity: modelId ? 0.5 : 1,
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected && !modelId) {
                      e.currentTarget.style.backgroundColor = colors.background;
                      e.currentTarget.style.borderColor = colors.warning;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected && !modelId) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                    }
                  }}
                  title={type.helpText}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                    <Activity size={14} style={{ color: colors.warning }} />
                    <span style={{ color: colors.text, fontSize: '11px', fontWeight: 600, fontFamily: 'monospace' }}>
                      {type.name}
                    </span>
                  </div>
                  <p style={{ color: colors.textMuted, fontSize: '9px', margin: 0, lineHeight: '1.3', marginBottom: '4px' }}>
                    {type.desc}
                  </p>
                  {showHelp && (
                    <p style={{ color: colors.primary, fontSize: '8px', margin: 0, lineHeight: '1.4', fontStyle: 'italic' }}>
                      {type.helpText}
                    </p>
                  )}
                </div>
              );
            })}
          </div>

          {/* Drift Strategy Section */}
          <div style={{
            padding: '12px',
            borderTop: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            marginTop: 'auto'
          }}>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: colors.textMuted,
              marginBottom: '8px',
              fontFamily: 'monospace',
              letterSpacing: '0.5px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}>
              DRIFT STRATEGY
              <HelpCircle
                size={10}
                style={{ color: colors.alert, cursor: 'pointer' }}
              />
            </div>
            <select
              value={driftStrategy}
              onChange={(e) => setDriftStrategy(e.target.value)}
              disabled={isLive}
              style={{
                width: '100%',
                padding: '6px 8px',
                backgroundColor: colors.background,
                border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                color: colors.text,
                fontSize: '10px',
                fontFamily: 'monospace',
                opacity: isLive ? 0.5 : 1
              }}
            >
              <option value="retrain">Retrain (Reset)</option>
              <option value="adaptive">Adaptive LR (Faster Learning)</option>
            </select>
            {showHelp && (
              <div style={{ fontSize: '8px', color: colors.textMuted, marginTop: '6px', lineHeight: '1.3' }}>
                {driftStrategy === 'retrain'
                  ? '🔄 Model resets when drift detected. Best for fundamental changes.'
                  : '⚡ Model learns faster when drift detected. Best for temporary volatility.'}
              </div>
            )}
          </div>
        </div>

        {/* Main Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          {/* Configuration & Control Section */}
          <div style={{
            padding: '16px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Zap size={14} style={{ color: colors.warning }} />
              <span style={{ fontSize: '11px', fontWeight: 600, color: colors.text, fontFamily: 'monospace' }}>
                MODEL CONTROL
              </span>
              <span style={{ fontSize: '10px', color: colors.textMuted }}>•</span>
              <span style={{ fontSize: '10px', color: colors.textMuted }}>
                {modelId ? 'MODEL INITIALIZED' : 'NO MODEL'}
              </span>
              {showHelp && (
                <span style={{ fontSize: '9px', color: colors.primary, marginLeft: 'auto', fontStyle: 'italic' }}>
                  💡 {!modelId ? 'Step 1: Create a model' : !isLive ? 'Step 2: Start live updates' : 'Step 3: Monitor performance'}
                </span>
              )}
            </div>

            {/* Control Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', alignItems: 'end' }}>
              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  UPDATE FREQUENCY {showHelp && <span style={{ fontSize: '8px', color: colors.primary }}>ℹ️ How often to retrain</span>}
                </label>
                <select
                  value={updateFrequency}
                  onChange={(e) => setUpdateFrequency(e.target.value)}
                  disabled={isLive}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace',
                    opacity: isLive ? 0.5 : 1
                  }}
                >
                  <option value="hourly">Hourly (High-frequency trading)</option>
                  <option value="daily">Daily (Swing trading)</option>
                  <option value="weekly">Weekly (Position trading)</option>
                </select>
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  SELECTED MODEL
                </label>
                <div style={{
                  padding: '8px 10px',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.warning}`,
                  color: colors.warning,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  fontWeight: 600
                }}>
                  {modelTypes.find(t => t.id === modelType)?.name || modelType}
                </div>
              </div>
            </div>

            {/* Action Buttons */}
            <div style={{ display: 'flex', gap: '12px', marginTop: '12px' }}>
              {!modelId ? (
                <button
                  onClick={handleCreateModel}
                  disabled={isLoading}
                  style={{
                    flex: 1,
                    padding: '10px 16px',
                    backgroundColor: isLoading ? colors.textMuted : colors.primary,
                    border: 'none',
                    color: colors.background,
                    fontSize: '11px',
                    fontWeight: 700,
                    cursor: isLoading ? 'not-allowed' : 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '8px',
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    transition: 'all 0.15s',
                    opacity: isLoading ? 0.6 : 1
                  }}
                >
                  {isLoading ? <Loader2 size={14} className="animate-spin" /> : <Play size={14} />}
                  {isLoading ? 'CREATING...' : 'CREATE ONLINE MODEL'}
                </button>
              ) : (
                <>
                  <button
                    onClick={isLive ? handleStopLive : handleStartLive}
                    style={{
                      flex: 1,
                      padding: '10px 16px',
                      backgroundColor: isLive ? colors.alert : colors.success,
                      border: 'none',
                      color: isLive ? colors.text : colors.background,
                      fontSize: '11px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '8px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      transition: 'all 0.15s'
                    }}
                  >
                    {isLive ? <Pause size={14} /> : <Play size={14} />}
                    {isLive ? 'STOP LIVE UPDATES' : 'START LIVE UPDATES'}
                  </button>
                  <button
                    onClick={handleResetModel}
                    disabled={isLive}
                    style={{
                      padding: '10px 16px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${colors.alert}`,
                      color: colors.alert,
                      fontSize: '11px',
                      fontWeight: 700,
                      cursor: isLive ? 'not-allowed' : 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '8px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      transition: 'all 0.15s',
                      opacity: isLive ? 0.5 : 1
                    }}
                  >
                    <RefreshCw size={14} />
                    RESET
                  </button>
                </>
              )}
            </div>
          </div>

          {/* Metrics Dashboard */}
          {metrics && (
            <div style={{
              padding: '16px',
              borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
              backgroundColor: colors.panel
            }}>
              <div style={{
                fontSize: '11px',
                fontWeight: 700,
                color: colors.accent,
                marginBottom: '12px',
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px'
              }}>
                <Cpu size={13} color={colors.accent} />
                REAL-TIME METRICS
                {showHelp && (
                  <span style={{ fontSize: '9px', color: colors.primary, fontWeight: 400, marginLeft: 'auto' }}>
                    📊 Lower MAE = Better predictions
                  </span>
                )}
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
                <div style={{
                  padding: '12px',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.accent}`,
                  borderLeft: `3px solid ${colors.accent}`
                }}>
                  <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace' }}>
                    CURRENT MAE {showHelp && <span style={{ color: colors.primary }}>ℹ️</span>}
                  </div>
                  <div style={{ fontSize: '18px', fontWeight: 700, color: colors.accent, fontFamily: 'monospace' }}>
                    {metrics.current_mae.toFixed(4)}
                  </div>
                  {showHelp && (
                    <div style={{ fontSize: '8px', color: colors.textMuted, marginTop: '4px' }}>
                      Avg error: ${metrics.current_mae.toFixed(2)}
                    </div>
                  )}
                </div>

                <div style={{
                  padding: '12px',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.success}`,
                  borderLeft: `3px solid ${colors.success}`
                }}>
                  <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace' }}>
                    SAMPLES TRAINED {showHelp && <span style={{ color: colors.primary }}>ℹ️</span>}
                  </div>
                  <div style={{ fontSize: '18px', fontWeight: 700, color: colors.success, fontFamily: 'monospace' }}>
                    {metrics.samples_trained.toLocaleString()}
                  </div>
                  {showHelp && (
                    <div style={{ fontSize: '8px', color: colors.textMuted, marginTop: '4px' }}>
                      Experience level
                    </div>
                  )}
                </div>

                <div style={{
                  padding: '12px',
                  backgroundColor: colors.background,
                  border: `1px solid ${metrics.drift_detected ? colors.alert : colors.success}`,
                  borderLeft: `3px solid ${metrics.drift_detected ? colors.alert : colors.success}`
                }}>
                  <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace', display: 'flex', alignItems: 'center', gap: '4px' }}>
                    DRIFT STATUS
                    {metrics.drift_detected && <Bell size={10} color={colors.alert} />}
                  </div>
                  <div style={{ fontSize: '14px', fontWeight: 700, color: metrics.drift_detected ? colors.alert : colors.success, fontFamily: 'monospace' }}>
                    {metrics.drift_detected ? 'DETECTED' : 'STABLE'}
                  </div>
                  {showHelp && (
                    <div style={{ fontSize: '8px', color: colors.textMuted, marginTop: '4px' }}>
                      {metrics.drift_detected ? 'Market changed!' : 'Conditions stable'}
                    </div>
                  )}
                </div>

                <div style={{
                  padding: '12px',
                  backgroundColor: colors.background,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                }}>
                  <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace' }}>
                    LAST UPDATE
                  </div>
                  <div style={{ fontSize: '11px', fontWeight: 600, color: colors.text, fontFamily: 'monospace' }}>
                    {metrics.last_updated ? new Date(metrics.last_updated).toLocaleTimeString() : 'Never'}
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* Update History */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
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

            {updateHistory.length > 0 && (
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
                  <Clock size={13} color={colors.accent} />
                  UPDATE HISTORY ({updateHistory.length})
                  {showHelp && (
                    <span style={{ fontSize: '9px', color: colors.primary, fontWeight: 400, marginLeft: 'auto' }}>
                      📈 Track model learning over time
                    </span>
                  )}
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {updateHistory.map((update, idx) => (
                    <div
                      key={idx}
                      style={{
                        padding: '10px 12px',
                        backgroundColor: colors.panel,
                        border: `1px solid ${update.drift ? colors.alert : 'var(--ft-border-color, #2A2A2A)'}`,
                        borderLeft: `3px solid ${update.drift ? colors.alert : colors.accent}`
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                        <span style={{ fontSize: '10px', fontFamily: 'monospace', color: colors.textMuted }}>
                          {new Date(update.timestamp).toLocaleTimeString()}
                        </span>
                        {update.drift && (
                          <div style={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                            padding: '2px 6px',
                            backgroundColor: colors.alert + '20',
                            fontSize: '9px',
                            color: colors.alert,
                            fontFamily: 'monospace',
                            fontWeight: 700
                          }}>
                            <AlertTriangle size={10} />
                            DRIFT {showHelp && <span>🚨</span>}
                          </div>
                        )}
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '10px' }}>
                        <div>
                          <span style={{ color: colors.textMuted, fontFamily: 'monospace' }}>MAE: </span>
                          <span style={{ color: colors.text, fontFamily: 'monospace', fontWeight: 600 }}>
                            {update.mae.toFixed(4)}
                          </span>
                        </div>
                        <div>
                          <span style={{ color: colors.textMuted, fontFamily: 'monospace' }}>Samples: </span>
                          <span style={{ color: colors.text, fontFamily: 'monospace', fontWeight: 600 }}>
                            {update.samples}
                          </span>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* Empty State */}
            {!modelId && updateHistory.length === 0 && (
              <div style={{
                padding: '40px 20px',
                textAlign: 'center'
              }}>
                <Activity size={48} color={colors.textMuted} style={{ margin: '0 auto 16px' }} />
                <div style={{ fontSize: '11px', color: colors.text, marginBottom: '8px', fontFamily: 'monospace' }}>
                  Ready for Online Learning
                </div>
                <div style={{ fontSize: '10px', color: colors.textMuted, lineHeight: '1.5' }}>
                  {showHelp ? (
                    <>
                      <strong>Quick Start Guide:</strong>
                      <br />
                      1️⃣ Select a model type from the left (Start with Linear Regression)
                      <br />
                      2️⃣ Choose update frequency (Daily recommended for beginners)
                      <br />
                      3️⃣ Click "CREATE ONLINE MODEL" to initialize
                      <br />
                      4️⃣ Click "START LIVE UPDATES" to begin real-time learning
                      <br />
                      5️⃣ Watch MAE decrease as the model learns!
                    </>
                  ) : (
                    <>
                      Create a model and start live updates to see real-time metrics and drift detection.
                      <br />
                      The system will incrementally learn from simulated market data.
                      <br />
                      <strong style={{ color: colors.primary }}>Click "SHOW GUIDE" above for help</strong>
                    </>
                  )}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
