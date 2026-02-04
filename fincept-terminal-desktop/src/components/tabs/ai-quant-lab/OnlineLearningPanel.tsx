/**
 * Online Learning Panel - Real-time Model Updates & Drift Detection
 * Incremental learning with concept drift detection for live trading
 * REFACTORED: Terminal-style UI matching DeepAgent and RL Trading UX
 */

import React, { useState, useEffect, useRef } from 'react';
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
        <Zap size={16} color={FINCEPT.YELLOW} />
        <span style={{
          color: FINCEPT.YELLOW,
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
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.CYAN}`,
              color: FINCEPT.CYAN
            }}>
              MODEL: {modelId.substring(0, 12)}...
            </div>
            {isLive && (
              <div style={{
                fontSize: '10px',
                fontFamily: 'monospace',
                padding: '3px 8px',
                backgroundColor: FINCEPT.GREEN + '20',
                border: `1px solid ${FINCEPT.GREEN}`,
                color: FINCEPT.GREEN,
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}>
                <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: FINCEPT.GREEN, animation: 'pulse 2s infinite' }} />
                LIVE
              </div>
            )}
          </>
        )}

        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: modelId ? (isLive ? FINCEPT.GREEN + '20' : FINCEPT.DARK_BG) : FINCEPT.DARK_BG,
          border: `1px solid ${modelId ? (isLive ? FINCEPT.GREEN : FINCEPT.BORDER) : FINCEPT.BORDER}`,
          color: modelId ? (isLive ? FINCEPT.GREEN : FINCEPT.MUTED) : FINCEPT.MUTED
        }}>
          {isLive ? 'ACTIVE' : modelId ? 'READY' : 'IDLE'}
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Model Type Selection */}
        <div style={{
          width: '220px',
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
                    backgroundColor: isSelected ? FINCEPT.HOVER : 'transparent',
                    border: `1px solid ${isSelected ? FINCEPT.YELLOW : FINCEPT.BORDER}`,
                    cursor: modelId ? 'not-allowed' : 'pointer',
                    opacity: modelId ? 0.5 : 1,
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected && !modelId) {
                      e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                      e.currentTarget.style.borderColor = FINCEPT.YELLOW;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected && !modelId) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                    <Activity size={14} style={{ color: FINCEPT.YELLOW }} />
                    <span style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 600, fontFamily: 'monospace' }}>
                      {type.name}
                    </span>
                  </div>
                  <p style={{ color: FINCEPT.MUTED, fontSize: '9px', margin: 0, lineHeight: '1.3' }}>
                    {type.desc}
                  </p>
                </div>
              );
            })}
          </div>

          {/* Drift Strategy Section */}
          <div style={{
            padding: '12px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            marginTop: 'auto'
          }}>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.MUTED,
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
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
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
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Zap size={14} style={{ color: FINCEPT.YELLOW }} />
              <span style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, fontFamily: 'monospace' }}>
                MODEL CONTROL
              </span>
              <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>•</span>
              <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
                {modelId ? 'MODEL INITIALIZED' : 'NO MODEL'}
              </span>
            </div>

            {/* Control Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', alignItems: 'end' }}>
              <div>
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
                  UPDATE FREQUENCY
                </label>
                <select
                  value={updateFrequency}
                  onChange={(e) => setUpdateFrequency(e.target.value)}
                  disabled={isLive}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
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
                <label style={{ display: 'block', fontSize: '10px', marginBottom: '6px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
                  SELECTED MODEL
                </label>
                <div style={{
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.YELLOW}`,
                  color: FINCEPT.YELLOW,
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
                    backgroundColor: FINCEPT.ORANGE,
                    border: 'none',
                    color: FINCEPT.DARK_BG,
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
                    backgroundColor: FINCEPT.GREEN,
                    border: 'none',
                    color: FINCEPT.DARK_BG,
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
                    backgroundColor: FINCEPT.RED,
                    border: 'none',
                    color: FINCEPT.WHITE,
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
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              backgroundColor: FINCEPT.PANEL_BG
            }}>
              <div style={{
                fontSize: '11px',
                fontWeight: 700,
                color: FINCEPT.CYAN,
                marginBottom: '12px',
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px'
              }}>
                <Cpu size={13} color={FINCEPT.CYAN} />
                REAL-TIME METRICS
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.CYAN}`,
                  borderLeft: `3px solid ${FINCEPT.CYAN}`
                }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: 'monospace' }}>
                    CURRENT MAE
                  </div>
                  <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: 'monospace' }}>
                    {metrics.current_mae.toFixed(4)}
                  </div>
                </div>

                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.GREEN}`,
                  borderLeft: `3px solid ${FINCEPT.GREEN}`
                }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: 'monospace' }}>
                    SAMPLES TRAINED
                  </div>
                  <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.GREEN, fontFamily: 'monospace' }}>
                    {metrics.samples_trained.toLocaleString()}
                  </div>
                </div>

                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${metrics.drift_detected ? FINCEPT.RED : FINCEPT.GREEN}`,
                  borderLeft: `3px solid ${metrics.drift_detected ? FINCEPT.RED : FINCEPT.GREEN}`
                }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: 'monospace', display: 'flex', alignItems: 'center', gap: '4px' }}>
                    DRIFT STATUS
                    {metrics.drift_detected && <Bell size={10} color={FINCEPT.RED} />}
                  </div>
                  <div style={{ fontSize: '14px', fontWeight: 700, color: metrics.drift_detected ? FINCEPT.RED : FINCEPT.GREEN, fontFamily: 'monospace' }}>
                    {metrics.drift_detected ? 'DETECTED' : 'STABLE'}
                  </div>
                </div>

                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: 'monospace' }}>
                    LAST UPDATE
                  </div>
                  <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, fontFamily: 'monospace' }}>
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

            {updateHistory.length > 0 && (
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
                  <Clock size={13} color={FINCEPT.CYAN} />
                  UPDATE HISTORY
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {updateHistory.map((update, idx) => (
                    <div
                      key={idx}
                      style={{
                        padding: '10px 12px',
                        backgroundColor: FINCEPT.PANEL_BG,
                        border: `1px solid ${update.drift ? FINCEPT.RED : FINCEPT.BORDER}`,
                        borderLeft: `3px solid ${update.drift ? FINCEPT.RED : FINCEPT.CYAN}`
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                        <span style={{ fontSize: '10px', fontFamily: 'monospace', color: FINCEPT.GRAY }}>
                          {new Date(update.timestamp).toLocaleTimeString()}
                        </span>
                        {update.drift && (
                          <div style={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                            padding: '2px 6px',
                            backgroundColor: FINCEPT.RED + '20',
                            fontSize: '9px',
                            color: FINCEPT.RED,
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
                          <span style={{ color: FINCEPT.GRAY, fontFamily: 'monospace' }}>MAE: </span>
                          <span style={{ color: FINCEPT.WHITE, fontFamily: 'monospace', fontWeight: 600 }}>
                            {update.mae.toFixed(4)}
                          </span>
                        </div>
                        <div>
                          <span style={{ color: FINCEPT.GRAY, fontFamily: 'monospace' }}>Samples: </span>
                          <span style={{ color: FINCEPT.WHITE, fontFamily: 'monospace', fontWeight: 600 }}>
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
                <Activity size={48} color={FINCEPT.MUTED} style={{ margin: '0 auto 16px' }} />
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, marginBottom: '8px', fontFamily: 'monospace' }}>
                  Ready for Online Learning
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.5' }}>
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
