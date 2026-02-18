/**
 * Rolling Retraining Panel - Automated Model Retraining
 * REFACTORED: Terminal-style UI with full backend integration
 */
import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { RefreshCw, Calendar, Clock, CheckCircle2, Play, Pause, Trash2, Settings, Zap, Loader2, AlertCircle } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface Schedule {
  model_id: string;
  freq: string;
  window: number;
  next_run: string;
  status?: 'active' | 'paused';
}

export function RollingRetrainingPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [frequency, setFrequency] = useState('daily');
  const [modelName, setModelName] = useState('');
  const [windowSize, setWindowSize] = useState('252');
  const [schedules, setSchedules] = useState<Schedule[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [isLoadingSchedules, setIsLoadingSchedules] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  // Load schedules on mount
  useEffect(() => {
    loadSchedules();
  }, []);

  const loadSchedules = async () => {
    setIsLoadingSchedules(true);
    setError(null);
    try {
      const result = await invoke<string>('qlib_rolling_list_schedules');
      const data = JSON.parse(result);

      if (data.success && data.schedules) {
        // Convert object to array and add status
        const schedulesArray = Object.entries(data.schedules).map(([model_id, sched]: [string, any]) => ({
          model_id,
          freq: sched.freq,
          window: sched.window,
          next_run: sched.next_run,
          status: 'active' as const
        }));
        setSchedules(schedulesArray);
      }
    } catch (err: any) {
      setError(err.message || 'Failed to load schedules');
    } finally {
      setIsLoadingSchedules(false);
    }
  };

  const handleCreateSchedule = async () => {
    if (!modelName.trim()) {
      setError('Model name is required');
      return;
    }

    setIsLoading(true);
    setError(null);
    setSuccessMessage(null);

    try {
      const result = await invoke<string>('qlib_rolling_create_schedule', {
        modelId: modelName.trim()
      });
      const data = JSON.parse(result);

      if (data.success) {
        setSuccessMessage(`Schedule created for ${data.model_id}`);
        setModelName('');
        await loadSchedules();
      } else {
        setError(data.error || 'Failed to create schedule');
      }
    } catch (err: any) {
      setError(err.message || 'Failed to create schedule');
    } finally {
      setIsLoading(false);
    }
  };

  const handleRetrain = async (modelId: string) => {
    setError(null);
    setSuccessMessage(null);

    try {
      const result = await invoke<string>('qlib_rolling_retrain', {
        modelId
      });
      const data = JSON.parse(result);

      if (data.success) {
        setSuccessMessage(`Retraining completed for ${modelId}`);
        await loadSchedules();
      } else {
        setError(data.error || 'Retraining failed');
      }
    } catch (err: any) {
      setError(err.message || 'Failed to execute retraining');
    }
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
        <RefreshCw size={16} color={colors.accent} />
        <span style={{
          color: colors.accent,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          ROLLING RETRAINING AUTOMATION
        </span>
        <div style={{ flex: 1 }} />
        {!isLoadingSchedules && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: colors.success + '20',
            border: `1px solid ${colors.success}`,
            color: colors.success
          }}>
            {schedules.length} SCHEDULES
          </div>
        )}
        <button
          onClick={loadSchedules}
          disabled={isLoadingSchedules}
          style={{
            padding: '4px 8px',
            backgroundColor: colors.background,
            border: `1px solid ${colors.accent}`,
            color: colors.accent,
            cursor: isLoadingSchedules ? 'not-allowed' : 'pointer',
            fontSize: '10px',
            fontFamily: 'monospace',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            opacity: isLoadingSchedules ? 0.5 : 1
          }}
        >
          <RefreshCw size={10} className={isLoadingSchedules ? 'animate-spin' : ''} />
          REFRESH
        </button>
      </div>

      {/* Error/Success Banner */}
      {(error || successMessage) && (
        <div style={{
          padding: '12px 16px',
          backgroundColor: error ? '#EF444420' : colors.success + '20',
          border: `1px solid ${error ? '#EF4444' : colors.success}`,
          borderLeft: `3px solid ${error ? '#EF4444' : colors.success}`,
          color: error ? '#EF4444' : colors.success,
          fontSize: '11px',
          fontFamily: 'monospace',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          {error ? <AlertCircle size={14} /> : <CheckCircle2 size={14} />}
          {error || successMessage}
          <button
            onClick={() => { setError(null); setSuccessMessage(null); }}
            style={{
              marginLeft: 'auto',
              background: 'none',
              border: 'none',
              color: error ? '#EF4444' : colors.success,
              cursor: 'pointer',
              fontSize: '10px',
              fontFamily: 'monospace'
            }}
          >
            ✕
          </button>
        </div>
      )}

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Panel - Create Schedule */}
        <div style={{
          width: '320px',
          borderRight: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.primary,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: colors.panel
          }}>
            CREATE NEW SCHEDULE
          </div>

          <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {/* Model Name */}
            <div>
              <label style={{
                display: 'block',
                fontSize: '10px',
                marginBottom: '6px',
                color: colors.text,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                opacity: 0.7
              }}>
                MODEL NAME
              </label>
              <input
                type="text"
                value={modelName}
                onChange={(e) => setModelName(e.target.value)}
                placeholder="e.g., XGBoost_Model"
                disabled={isLoading}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: colors.background,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  color: colors.text,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  outline: 'none',
                  opacity: isLoading ? 0.5 : 1
                }}
              />
            </div>

            {/* Training Window */}
            <div>
              <label style={{
                display: 'block',
                fontSize: '10px',
                marginBottom: '6px',
                color: colors.text,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                opacity: 0.7
              }}>
                TRAINING WINDOW (DAYS)
              </label>
              <input
                type="text"
                inputMode="numeric"
                value={windowSize}
                onChange={(e) => {
                  const v = e.target.value;
                  if (v === '' || /^\d+$/.test(v)) setWindowSize(v);
                }}
                placeholder="252"
                disabled={isLoading}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: colors.background,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  color: colors.text,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  outline: 'none',
                  opacity: isLoading ? 0.5 : 1
                }}
              />
            </div>

            {/* Frequency */}
            <div>
              <label style={{
                display: 'block',
                fontSize: '10px',
                marginBottom: '6px',
                color: colors.text,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                opacity: 0.7
              }}>
                RETRAIN FREQUENCY
              </label>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {['hourly', 'daily', 'weekly'].map(freq => (
                  <button
                    key={freq}
                    onClick={() => setFrequency(freq)}
                    disabled={isLoading}
                    style={{
                      padding: '10px',
                      backgroundColor: frequency === freq ? colors.accent + '30' : colors.panel,
                      border: `1px solid ${frequency === freq ? colors.accent : 'var(--ft-border-color, #2A2A2A)'}`,
                      color: frequency === freq ? colors.accent : colors.text,
                      fontSize: '11px',
                      fontWeight: 600,
                      fontFamily: 'monospace',
                      cursor: isLoading ? 'not-allowed' : 'pointer',
                      textAlign: 'left',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      transition: 'all 0.15s',
                      textTransform: 'uppercase',
                      letterSpacing: '0.5px',
                      opacity: isLoading ? 0.5 : 1
                    }}
                    onMouseEnter={(e) => {
                      if (frequency !== freq && !isLoading) {
                        e.currentTarget.style.backgroundColor = '#1F1F1F';
                        e.currentTarget.style.borderColor = colors.accent;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (frequency !== freq && !isLoading) {
                        e.currentTarget.style.backgroundColor = colors.panel;
                        e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                      }
                    }}
                  >
                    <Clock size={12} />
                    {freq}
                  </button>
                ))}
              </div>
            </div>

            {/* Create Button */}
            <button
              onClick={handleCreateSchedule}
              disabled={isLoading || !modelName.trim()}
              style={{
                width: '100%',
                padding: '12px 16px',
                backgroundColor: colors.accent,
                border: 'none',
                color: '#000000',
                fontSize: '11px',
                fontWeight: 700,
                fontFamily: 'monospace',
                cursor: (isLoading || !modelName.trim()) ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                letterSpacing: '0.5px',
                transition: 'all 0.15s',
                marginTop: '8px',
                opacity: (isLoading || !modelName.trim()) ? 0.5 : 1
              }}
            >
              {isLoading ? (
                <>
                  <Loader2 size={14} className="animate-spin" />
                  CREATING...
                </>
              ) : (
                <>
                  <Play size={14} />
                  CREATE SCHEDULE
                </>
              )}
            </button>
          </div>
        </div>

        {/* Right Panel - Active Schedules */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          <div style={{
            padding: '10px 16px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.success,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: colors.panel
          }}>
            ACTIVE SCHEDULES ({schedules.length})
          </div>

          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {isLoadingSchedules ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                gap: '12px'
              }}>
                <Loader2 size={32} color={colors.accent} className="animate-spin" />
                <div style={{
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  color: colors.accent,
                  fontWeight: 600
                }}>
                  LOADING SCHEDULES...
                </div>
              </div>
            ) : schedules.length === 0 ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                gap: '12px',
                padding: '40px 20px'
              }}>
                <RefreshCw size={40} color={colors.text} style={{ opacity: 0.3 }} />
                <div style={{
                  fontSize: '12px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  fontWeight: 600
                }}>
                  NO SCHEDULES CREATED
                </div>
                <div style={{
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  opacity: 0.6,
                  textAlign: 'center',
                  maxWidth: '300px',
                  lineHeight: '1.6'
                }}>
                  Create your first automated retraining schedule using the form on the left.
                  Models will be retrained automatically based on your chosen frequency.
                </div>
              </div>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {schedules.map((sched) => (
                  <div
                    key={sched.model_id}
                    style={{
                      padding: '14px',
                      backgroundColor: colors.panel,
                      border: `1px solid ${colors.success}`,
                      borderLeft: `3px solid ${colors.success}`,
                      display: 'flex',
                      flexDirection: 'column',
                      gap: '10px'
                    }}
                  >
                    {/* Header */}
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <Zap size={14} color={colors.success} />
                        <span style={{
                          color: colors.text,
                          fontSize: '12px',
                          fontWeight: 700,
                          fontFamily: 'monospace'
                        }}>
                          {sched.model_id}
                        </span>
                      </div>
                      <div style={{
                        fontSize: '9px',
                        fontFamily: 'monospace',
                        padding: '3px 8px',
                        backgroundColor: colors.success + '20',
                        border: `1px solid ${colors.success}`,
                        color: colors.success,
                        textTransform: 'uppercase'
                      }}>
                        ACTIVE
                      </div>
                    </div>

                    {/* Details Grid */}
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
                      <div>
                        <div style={{ fontSize: '9px', color: colors.text, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                          FREQUENCY
                        </div>
                        <div style={{ fontSize: '11px', color: colors.accent, fontFamily: 'monospace', fontWeight: 600 }}>
                          {sched.freq.toUpperCase()}
                        </div>
                      </div>
                      <div>
                        <div style={{ fontSize: '9px', color: colors.text, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                          WINDOW
                        </div>
                        <div style={{ fontSize: '11px', color: colors.accent, fontFamily: 'monospace', fontWeight: 600 }}>
                          {sched.window} days
                        </div>
                      </div>
                      <div style={{ gridColumn: '1 / -1' }}>
                        <div style={{ fontSize: '9px', color: colors.text, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                          NEXT RUN
                        </div>
                        <div style={{ fontSize: '10px', color: colors.primary, fontFamily: 'monospace', fontWeight: 600 }}>
                          {sched.next_run}
                        </div>
                      </div>
                    </div>

                    {/* Action Button */}
                    <div style={{ display: 'flex', gap: '8px', marginTop: '4px' }}>
                      <button
                        onClick={() => handleRetrain(sched.model_id)}
                        style={{
                          flex: 1,
                          padding: '8px 12px',
                          backgroundColor: colors.primary,
                          border: 'none',
                          color: '#000000',
                          fontSize: '10px',
                          fontWeight: 700,
                          fontFamily: 'monospace',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          gap: '6px',
                          letterSpacing: '0.5px',
                          transition: 'all 0.15s'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.opacity = '0.9';
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.opacity = '1';
                        }}
                      >
                        <Play size={10} />
                        RETRAIN NOW
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
