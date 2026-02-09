/**
 * Rolling Retraining Panel - Automated Model Retraining
 * REFACTORED: Terminal-style UI matching DeepAgent/RLTrading UX
 */
import React, { useState } from 'react';
import { RefreshCw, Calendar, Clock, CheckCircle2, Play, Pause, Trash2, Settings, Zap } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

export function RollingRetrainingPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [frequency, setFrequency] = useState('daily');
  const [modelName, setModelName] = useState('');
  const [windowSize, setWindowSize] = useState('252');
  const [schedules] = useState([
    { id: '1', model: 'Model_A', frequency: 'daily', windowSize: 252, lastRun: '2026-02-02 10:00', nextRun: '2026-02-03 10:00', status: 'active' },
    { id: '2', model: 'Model_B', frequency: 'weekly', windowSize: 504, lastRun: '2026-02-01 14:30', nextRun: '2026-02-08 14:30', status: 'paused' },
    { id: '3', model: 'Model_C', frequency: 'hourly', windowSize: 126, lastRun: '2026-02-04 09:00', nextRun: '2026-02-04 10:00', status: 'active' }
  ]);

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
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: colors.success + '20',
          border: `1px solid ${colors.success}`,
          color: colors.success
        }}>
          {schedules.filter(s => s.status === 'active').length} ACTIVE
        </div>
      </div>

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
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: colors.background,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  color: colors.text,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  outline: 'none'
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
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: colors.background,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  color: colors.text,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  outline: 'none'
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
                    style={{
                      padding: '10px',
                      backgroundColor: frequency === freq ? colors.accent + '30' : colors.panel,
                      border: `1px solid ${frequency === freq ? colors.accent : 'var(--ft-border-color, #2A2A2A)'}`,
                      color: frequency === freq ? colors.accent : colors.text,
                      fontSize: '11px',
                      fontWeight: 600,
                      fontFamily: 'monospace',
                      cursor: 'pointer',
                      textAlign: 'left',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      transition: 'all 0.15s',
                      textTransform: 'uppercase',
                      letterSpacing: '0.5px'
                    }}
                    onMouseEnter={(e) => {
                      if (frequency !== freq) {
                        e.currentTarget.style.backgroundColor = '#1F1F1F';
                        e.currentTarget.style.borderColor = colors.accent;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (frequency !== freq) {
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
              style={{
                width: '100%',
                padding: '12px 16px',
                backgroundColor: colors.accent,
                border: 'none',
                color: '#000000',
                fontSize: '11px',
                fontWeight: 700,
                fontFamily: 'monospace',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                letterSpacing: '0.5px',
                transition: 'all 0.15s',
                marginTop: '8px'
              }}
            >
              <Play size={14} />
              CREATE SCHEDULE
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
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
              {schedules.map((sched) => (
                <div
                  key={sched.id}
                  style={{
                    padding: '14px',
                    backgroundColor: colors.panel,
                    border: `1px solid ${sched.status === 'active' ? colors.success : 'var(--ft-border-color, #2A2A2A)'}`,
                    borderLeft: `3px solid ${sched.status === 'active' ? colors.success : colors.purple}`,
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '10px'
                  }}
                >
                  {/* Header */}
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <Zap size={14} color={sched.status === 'active' ? colors.success : colors.purple} />
                      <span style={{
                        color: colors.text,
                        fontSize: '12px',
                        fontWeight: 700,
                        fontFamily: 'monospace'
                      }}>
                        {sched.model}
                      </span>
                    </div>
                    <div style={{
                      fontSize: '9px',
                      fontFamily: 'monospace',
                      padding: '3px 8px',
                      backgroundColor: sched.status === 'active' ? colors.success + '20' : colors.purple + '20',
                      border: `1px solid ${sched.status === 'active' ? colors.success : colors.purple}`,
                      color: sched.status === 'active' ? colors.success : colors.purple,
                      textTransform: 'uppercase'
                    }}>
                      {sched.status}
                    </div>
                  </div>

                  {/* Details Grid */}
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
                    <div>
                      <div style={{ fontSize: '9px', color: colors.text, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        FREQUENCY
                      </div>
                      <div style={{ fontSize: '11px', color: colors.accent, fontFamily: 'monospace', fontWeight: 600 }}>
                        {sched.frequency.toUpperCase()}
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', color: colors.text, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        WINDOW
                      </div>
                      <div style={{ fontSize: '11px', color: colors.accent, fontFamily: 'monospace', fontWeight: 600 }}>
                        {sched.windowSize} days
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', color: colors.text, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        LAST RUN
                      </div>
                      <div style={{ fontSize: '10px', color: colors.text, fontFamily: 'monospace', opacity: 0.7 }}>
                        {sched.lastRun}
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', color: colors.text, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        NEXT RUN
                      </div>
                      <div style={{ fontSize: '10px', color: colors.primary, fontFamily: 'monospace', fontWeight: 600 }}>
                        {sched.nextRun}
                      </div>
                    </div>
                  </div>

                  {/* Action Buttons */}
                  <div style={{ display: 'flex', gap: '8px', marginTop: '4px' }}>
                    <button
                      style={{
                        flex: 1,
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${colors.primary}`,
                        color: colors.primary,
                        fontSize: '9px',
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
                        e.currentTarget.style.backgroundColor = colors.primary + '20';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }}
                    >
                      {sched.status === 'active' ? <Pause size={10} /> : <Play size={10} />}
                      {sched.status === 'active' ? 'PAUSE' : 'RESUME'}
                    </button>
                    <button
                      style={{
                        flex: 1,
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${colors.accent}`,
                        color: colors.accent,
                        fontSize: '9px',
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
                        e.currentTarget.style.backgroundColor = colors.accent + '20';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }}
                    >
                      <Settings size={10} />
                      CONFIGURE
                    </button>
                    <button
                      style={{
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${colors.alert}`,
                        color: colors.alert,
                        fontSize: '9px',
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
                        e.currentTarget.style.backgroundColor = colors.alert + '20';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }}
                    >
                      <Trash2 size={10} />
                    </button>
                  </div>
                </div>
              ))}
            </div>

            {/* Empty State */}
            {schedules.length === 0 && (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '300px',
                textAlign: 'center'
              }}>
                <Calendar size={48} color={colors.textMuted} style={{ opacity: 0.3, marginBottom: '16px' }} />
                <div style={{ color: colors.text, fontSize: '12px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '8px', fontFamily: 'monospace' }}>
                  NO SCHEDULES CONFIGURED
                </div>
                <div style={{ color: colors.textMuted, fontSize: '11px', maxWidth: '400px', lineHeight: '1.6' }}>
                  Create a new retraining schedule to automate model updates
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
