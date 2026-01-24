import React from 'react';
import { Terminal as TerminalIcon, TrendingUp, TrendingDown, BarChart2, AlertCircle, Clock, CheckCircle } from 'lucide-react';
import { FinScriptChart } from './FinScriptChart';

// ─── Design System Constants ────────────────────────────────────────────────
const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
};

const FONT = '"IBM Plex Mono", "Consolas", monospace';

interface PlotPoint {
  timestamp: number;
  value?: number;
  open?: number;
  high?: number;
  low?: number;
  close?: number;
  volume?: number;
}

export interface FinScriptExecutionResult {
  success: boolean;
  output: string;
  signals: Array<{
    signal_type: string;
    message: string;
    timestamp: string;
    price?: number;
  }>;
  plots: Array<{
    plot_type: string;
    label: string;
    data: PlotPoint[];
    color?: string;
  }>;
  errors: string[];
  execution_time_ms: number;
}

interface FinScriptOutputPanelProps {
  result: FinScriptExecutionResult | null;
  isRunning: boolean;
  onClear: () => void;
  colors: {
    primary: string;
    text: string;
    secondary: string;
    alert: string;
    warning: string;
    info: string;
    accent: string;
    textMuted: string;
    background: string;
    panel: string;
  };
}

export const FinScriptOutputPanel: React.FC<FinScriptOutputPanelProps> = ({
  result,
  isRunning,
  onClear,
}) => {
  if (isRunning) {
    return (
      <div style={{
        width: '100%', height: '100%',
        display: 'flex', flexDirection: 'column',
        backgroundColor: F.DARK_BG,
      }}>
        <div style={{
          flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center',
          flexDirection: 'column', gap: '12px',
        }}>
          <div className="animate-spin" style={{
            width: '24px', height: '24px',
            border: `2px solid ${F.BORDER}`,
            borderTop: `2px solid ${F.ORANGE}`,
            borderRadius: '50%',
          }} />
          <span style={{ color: F.GRAY, fontSize: '10px', fontFamily: FONT, letterSpacing: '0.5px' }}>
            EXECUTING FINSCRIPT...
          </span>
        </div>
      </div>
    );
  }

  if (!result) {
    return (
      <div style={{
        width: '100%', height: '100%',
        display: 'flex', flexDirection: 'column',
        backgroundColor: F.DARK_BG,
      }}>
        <div style={{
          flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center',
          flexDirection: 'column', color: F.MUTED,
        }}>
          <TerminalIcon size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
          <div style={{ fontSize: '10px', fontFamily: FONT }}>No output yet</div>
          <div style={{ fontSize: '9px', marginTop: '4px', fontFamily: FONT, color: F.MUTED }}>
            Press Ctrl+Enter to run
          </div>
        </div>
      </div>
    );
  }

  return (
    <div style={{
      width: '100%', height: '100%',
      display: 'flex', flexDirection: 'column',
      backgroundColor: F.DARK_BG,
    }}>
      {/* Scrollable Content */}
      <div className="ce-scroll" style={{
        flex: 1, overflowY: 'auto', padding: '12px',
      }}>
        {/* Execution Summary */}
        <div style={{
          display: 'flex', alignItems: 'center', gap: '8px',
          marginBottom: '12px', padding: '6px 8px',
          backgroundColor: F.PANEL_BG,
          border: `1px solid ${F.BORDER}`, borderRadius: '2px',
        }}>
          {result.success
            ? <CheckCircle size={10} style={{ color: F.GREEN }} />
            : <AlertCircle size={10} style={{ color: F.RED }} />
          }
          <span style={{
            fontSize: '9px', fontWeight: 700, fontFamily: FONT,
            color: result.success ? F.GREEN : F.RED,
            letterSpacing: '0.5px',
          }}>
            {result.success ? 'COMPLETED' : 'FAILED'}
          </span>
          <span style={{ color: F.MUTED, fontSize: '9px' }}>|</span>
          <span style={{ color: F.GRAY, fontSize: '9px', fontFamily: FONT, display: 'flex', alignItems: 'center', gap: '3px' }}>
            <Clock size={9} /> {result.execution_time_ms}ms
          </span>
          {result.signals.length > 0 && (
            <>
              <span style={{ color: F.MUTED, fontSize: '9px' }}>|</span>
              <span style={{ color: F.CYAN, fontSize: '9px', fontFamily: FONT }}>
                {result.signals.length} signals
              </span>
            </>
          )}
          <div style={{ flex: 1 }} />
          <button onClick={onClear} style={{
            padding: '2px 6px', backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`, borderRadius: '2px',
            color: F.GRAY, fontSize: '8px', fontWeight: 700,
            cursor: 'pointer', fontFamily: FONT,
            letterSpacing: '0.5px',
          }}>
            CLEAR
          </button>
        </div>

        {/* Errors Section */}
        {result.errors.length > 0 && (
          <div style={{
            marginBottom: '12px', padding: '8px',
            border: `1px solid ${F.RED}`, borderRadius: '2px',
            backgroundColor: `${F.RED}08`,
          }}>
            <div style={{
              display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px',
            }}>
              <AlertCircle size={10} style={{ color: F.RED }} />
              <span style={{ color: F.RED, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: FONT }}>
                ERRORS ({result.errors.length})
              </span>
            </div>
            {result.errors.map((error, idx) => (
              <div key={idx} style={{
                color: F.RED, fontSize: '10px', marginBottom: '3px',
                paddingLeft: '16px', fontFamily: FONT, lineHeight: '1.4',
              }}>
                {error}
              </div>
            ))}
          </div>
        )}

        {/* Signals Section */}
        {result.signals.length > 0 && (
          <div style={{ marginBottom: '12px' }}>
            <div style={{
              display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px',
            }}>
              <BarChart2 size={10} style={{ color: F.CYAN }} />
              <span style={{ color: F.CYAN, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: FONT }}>
                SIGNALS ({result.signals.length})
              </span>
            </div>
            {result.signals.map((signal, idx) => (
              <div key={idx} style={{
                display: 'flex', alignItems: 'center', gap: '8px',
                padding: '5px 8px', marginBottom: '3px',
                backgroundColor: signal.signal_type === 'Buy' ? `${F.GREEN}08` : `${F.RED}08`,
                border: `1px solid ${signal.signal_type === 'Buy' ? F.GREEN : F.RED}`,
                borderRadius: '2px',
              }}>
                {signal.signal_type === 'Buy' ? (
                  <TrendingUp size={11} style={{ color: F.GREEN }} />
                ) : (
                  <TrendingDown size={11} style={{ color: F.RED }} />
                )}
                <div style={{ flex: 1 }}>
                  <span style={{
                    color: signal.signal_type === 'Buy' ? F.GREEN : F.RED,
                    fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: FONT,
                  }}>
                    {signal.signal_type.toUpperCase()}
                  </span>
                  <span style={{ color: F.WHITE, fontSize: '10px', marginLeft: '8px', fontFamily: FONT }}>
                    {signal.message}
                  </span>
                </div>
                {signal.price && (
                  <span style={{ color: F.YELLOW, fontSize: '10px', fontFamily: FONT, fontWeight: 700 }}>
                    ${signal.price.toFixed(2)}
                  </span>
                )}
              </div>
            ))}
          </div>
        )}

        {/* Output Section */}
        {result.output && (
          <div style={{ marginBottom: '12px' }}>
            <div style={{
              display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px',
            }}>
              <TerminalIcon size={10} style={{ color: F.GREEN }} />
              <span style={{ color: F.GREEN, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: FONT }}>
                OUTPUT
              </span>
            </div>
            <pre style={{
              color: F.WHITE, fontSize: '10px', margin: 0,
              whiteSpace: 'pre-wrap', wordBreak: 'break-word',
              fontFamily: FONT,
              backgroundColor: F.PANEL_BG,
              padding: '8px',
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              lineHeight: '1.5',
            }}>
              {result.output}
            </pre>
          </div>
        )}

        {/* Charts Section */}
        {result.plots.length > 0 && (
          <div>
            <div style={{
              display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '8px',
            }}>
              <BarChart2 size={10} style={{ color: F.ORANGE }} />
              <span style={{ color: F.ORANGE, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: FONT }}>
                CHARTS ({result.plots.length})
              </span>
            </div>
            {result.plots.map((plot, idx) => (
              <FinScriptChart key={`${plot.label}-${idx}`} plot={plot} height={200} />
            ))}
          </div>
        )}
      </div>
    </div>
  );
};
