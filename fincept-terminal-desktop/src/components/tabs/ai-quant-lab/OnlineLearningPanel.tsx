/**
 * Online Learning Panel - Real-time Model Updates & Drift Detection
 * Incremental learning with concept drift detection for live trading
 * REFACTORED: Terminal-style UI matching DeepAgent and RL Trading UX
 */

import React, { useState, useEffect, useRef } from 'react';
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
  AlertCircle
} from 'lucide-react';

interface ModelMetrics {
  model_id: string;
  current_mae: number;
  samples_trained: number;
  last_updated: string;
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

  const modelTypes = [
    { id: 'linear', name: 'Linear Regression', desc: 'Fast, interpretable' },
    { id: 'tree', name: 'Hoeffding Tree', desc: 'Decision tree for streams' },
    { id: 'adaptive_random_forest', name: 'Adaptive Random Forest', desc: 'Ensemble method' }
  ];

  const handleCreateModel = async () => {
    try {
      setError(null);
      // Call backend service
      // const result = await onlineLearningService.createModel({ model_type: modelType });

      // Simulate
      await new Promise(resolve => setTimeout(resolve, 1000));
      const mockId = `model_${Date.now()}`;
      setModelId(mockId);
      setMetrics({
        model_id: mockId,
        current_mae: 0,
        samples_trained: 0,
        last_updated: new Date().toISOString(),
        drift_detected: false
      });
    } catch (err: any) {
      setError(err.message || 'Failed to create model');
    }
  };

  const handleStartLive = () => {
    setIsLive(true);
    // Simulate live updates
    const interval = setInterval(() => {
      setMetrics(prev => {
        if (!prev) return null;
        const newSamples = prev.samples_trained + Math.floor(Math.random() * 10) + 1;
        const newMae = Math.max(0.001, prev.current_mae + (Math.random() - 0.5) * 0.005);
        const drift = Math.random() > 0.95;

        const update: UpdateHistory = {
          timestamp: new Date().toISOString(),
          mae: newMae,
          samples: newSamples,
          drift
        };
        setUpdateHistory(hist => [update, ...hist.slice(0, 49)]);

        return {
          ...prev,
          current_mae: newMae,
          samples_trained: newSamples,
          last_updated: new Date().toISOString(),
          drift_detected: drift
        };
      });
    }, 2000);

    return () => clearInterval(interval);
  };

  const handleStopLive = () => {
    setIsLive(false);
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

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Model Type Selection */}
        <div style={{
          width: '220px',
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
            MODEL TYPE
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
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                    <Activity size={14} style={{ color: colors.warning }} />
                    <span style={{ color: colors.text, fontSize: '11px', fontWeight: 600, fontFamily: 'monospace' }}>
                      {type.name}
                    </span>
                  </div>
                  <p style={{ color: colors.textMuted, fontSize: '9px', margin: 0, lineHeight: '1.3' }}>
                    {type.desc}
                  </p>
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
              letterSpacing: '0.5px'
            }}>
              DRIFT STRATEGY
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
              <option value="retrain">Retrain</option>
              <option value="adaptive">Adaptive LR</option>
              <option value="ensemble">Ensemble</option>
            </select>
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
            </div>

            {/* Control Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', alignItems: 'end' }}>
              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: colors.textMuted, fontFamily: 'monospace' }}>
                  UPDATE FREQUENCY
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
                  <option value="hourly">Hourly</option>
                  <option value="daily">Daily</option>
                  <option value="weekly">Weekly</option>
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
                  style={{
                    flex: 1,
                    padding: '10px 16px',
                    backgroundColor: colors.primary,
                    border: 'none',
                    color: colors.background,
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
                  <Play size={14} />
                  CREATE ONLINE MODEL
                </button>
              ) : !isLive ? (
                <button
                  onClick={handleStartLive}
                  style={{
                    flex: 1,
                    padding: '10px 16px',
                    backgroundColor: colors.success,
                    border: 'none',
                    color: colors.background,
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
                  <Play size={14} />
                  START LIVE UPDATES
                </button>
              ) : (
                <button
                  onClick={handleStopLive}
                  style={{
                    flex: 1,
                    padding: '10px 16px',
                    backgroundColor: colors.alert,
                    border: 'none',
                    color: colors.text,
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
                  <Pause size={14} />
                  STOP LIVE UPDATES
                </button>
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
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
                <div style={{
                  padding: '12px',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.accent}`,
                  borderLeft: `3px solid ${colors.accent}`
                }}>
                  <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace' }}>
                    CURRENT MAE
                  </div>
                  <div style={{ fontSize: '18px', fontWeight: 700, color: colors.accent, fontFamily: 'monospace' }}>
                    {metrics.current_mae.toFixed(4)}
                  </div>
                </div>

                <div style={{
                  padding: '12px',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.success}`,
                  borderLeft: `3px solid ${colors.success}`
                }}>
                  <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '4px', fontFamily: 'monospace' }}>
                    SAMPLES TRAINED
                  </div>
                  <div style={{ fontSize: '18px', fontWeight: 700, color: colors.success, fontFamily: 'monospace' }}>
                    {metrics.samples_trained.toLocaleString()}
                  </div>
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
                    {new Date(metrics.last_updated).toLocaleTimeString()}
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
                  UPDATE HISTORY
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
                            DRIFT
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
                  Create a model and start live updates to see real-time metrics and drift detection.
                  <br />Results and update history will appear here.
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
