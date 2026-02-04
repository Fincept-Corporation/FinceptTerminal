/**
 * Rolling Retraining Panel - Automated Model Retraining
 * REFACTORED: Terminal-style UI matching DeepAgent/RLTrading UX
 */
import React, { useState } from 'react';
import { RefreshCw, Calendar, Clock, CheckCircle2, Play, Pause, Trash2, Settings, Zap } from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GREEN: '#00D66F', GRAY: '#787878',
  DARK_BG: '#000000', PANEL_BG: '#0F0F0F', BORDER: '#2A2A2A', CYAN: '#00E5FF',
  MUTED: '#4A4A4A', HOVER: '#1F1F1F', PURPLE: '#9D4EDD', RED: '#FF3B3B'
};

export function RollingRetrainingPanel() {
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
        <RefreshCw size={16} color={FINCEPT.CYAN} />
        <span style={{
          color: FINCEPT.CYAN,
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
          backgroundColor: FINCEPT.GREEN + '20',
          border: `1px solid ${FINCEPT.GREEN}`,
          color: FINCEPT.GREEN
        }}>
          {schedules.filter(s => s.status === 'active').length} ACTIVE
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Panel - Create Schedule */}
        <div style={{
          width: '320px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: FINCEPT.PANEL_BG
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
                color: FINCEPT.WHITE,
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
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
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
                color: FINCEPT.WHITE,
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
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
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
                color: FINCEPT.WHITE,
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
                      backgroundColor: frequency === freq ? FINCEPT.CYAN + '30' : FINCEPT.PANEL_BG,
                      border: `1px solid ${frequency === freq ? FINCEPT.CYAN : FINCEPT.BORDER}`,
                      color: frequency === freq ? FINCEPT.CYAN : FINCEPT.WHITE,
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
                        e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                        e.currentTarget.style.borderColor = FINCEPT.CYAN;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (frequency !== freq) {
                        e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                        e.currentTarget.style.borderColor = FINCEPT.BORDER;
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
                backgroundColor: FINCEPT.CYAN,
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
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.GREEN,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: FINCEPT.PANEL_BG
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
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${sched.status === 'active' ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${sched.status === 'active' ? FINCEPT.GREEN : FINCEPT.PURPLE}`,
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '10px'
                  }}
                >
                  {/* Header */}
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <Zap size={14} color={sched.status === 'active' ? FINCEPT.GREEN : FINCEPT.PURPLE} />
                      <span style={{
                        color: FINCEPT.WHITE,
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
                      backgroundColor: sched.status === 'active' ? FINCEPT.GREEN + '20' : FINCEPT.PURPLE + '20',
                      border: `1px solid ${sched.status === 'active' ? FINCEPT.GREEN : FINCEPT.PURPLE}`,
                      color: sched.status === 'active' ? FINCEPT.GREEN : FINCEPT.PURPLE,
                      textTransform: 'uppercase'
                    }}>
                      {sched.status}
                    </div>
                  </div>

                  {/* Details Grid */}
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
                    <div>
                      <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        FREQUENCY
                      </div>
                      <div style={{ fontSize: '11px', color: FINCEPT.CYAN, fontFamily: 'monospace', fontWeight: 600 }}>
                        {sched.frequency.toUpperCase()}
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        WINDOW
                      </div>
                      <div style={{ fontSize: '11px', color: FINCEPT.CYAN, fontFamily: 'monospace', fontWeight: 600 }}>
                        {sched.windowSize} days
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        LAST RUN
                      </div>
                      <div style={{ fontSize: '10px', color: FINCEPT.WHITE, fontFamily: 'monospace', opacity: 0.7 }}>
                        {sched.lastRun}
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, fontFamily: 'monospace', marginBottom: '3px' }}>
                        NEXT RUN
                      </div>
                      <div style={{ fontSize: '10px', color: FINCEPT.ORANGE, fontFamily: 'monospace', fontWeight: 600 }}>
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
                        border: `1px solid ${FINCEPT.ORANGE}`,
                        color: FINCEPT.ORANGE,
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
                        e.currentTarget.style.backgroundColor = FINCEPT.ORANGE + '20';
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
                        border: `1px solid ${FINCEPT.CYAN}`,
                        color: FINCEPT.CYAN,
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
                        e.currentTarget.style.backgroundColor = FINCEPT.CYAN + '20';
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
                        border: `1px solid ${FINCEPT.RED}`,
                        color: FINCEPT.RED,
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
                        e.currentTarget.style.backgroundColor = FINCEPT.RED + '20';
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
                <Calendar size={48} color={FINCEPT.MUTED} style={{ opacity: 0.3, marginBottom: '16px' }} />
                <div style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '8px', fontFamily: 'monospace' }}>
                  NO SCHEDULES CONFIGURED
                </div>
                <div style={{ color: FINCEPT.MUTED, fontSize: '11px', maxWidth: '400px', lineHeight: '1.6' }}>
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
