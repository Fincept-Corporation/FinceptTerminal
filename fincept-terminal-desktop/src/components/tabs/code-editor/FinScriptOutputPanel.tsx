import React from 'react';
import { useTranslation } from 'react-i18next';
import { Terminal as TerminalIcon, TrendingUp, TrendingDown, BarChart2, AlertCircle, Clock, CheckCircle } from 'lucide-react';
import { FinScriptChart } from './FinScriptChart';

// Theme colors passed via props from parent

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
  fontFamily?: string;
}

export const FinScriptOutputPanel: React.FC<FinScriptOutputPanelProps> = ({
  result,
  isRunning,
  onClear,
  colors,
  fontFamily = '"IBM Plex Mono", monospace',
}) => {
  const { t } = useTranslation('codeEditor');

  if (isRunning) {
    return (
      <div style={{
        width: '100%', height: '100%',
        display: 'flex', flexDirection: 'column',
        backgroundColor: colors.background,
      }}>
        <div style={{
          flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center',
          flexDirection: 'column', gap: '12px',
        }}>
          <div className="animate-spin" style={{
            width: '24px', height: '24px',
            border: `2px solid ${colors.panel}`,
            borderTop: `2px solid ${colors.primary}`,
            borderRadius: '50%',
          }} />
          <span style={{ color: colors.textMuted, fontSize: '10px', fontFamily: fontFamily, letterSpacing: '0.5px' }}>
            {t('output.executing')}
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
        backgroundColor: colors.background,
      }}>
        <div style={{
          flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center',
          flexDirection: 'column', color: colors.textMuted,
        }}>
          <TerminalIcon size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
          <div style={{ fontSize: '10px', fontFamily: fontFamily }}>{t('output.noOutput')}</div>
          <div style={{ fontSize: '9px', marginTop: '4px', fontFamily: fontFamily, color: colors.textMuted }}>
            {t('output.pressToRun')}
          </div>
        </div>
      </div>
    );
  }

  return (
    <div style={{
      width: '100%', height: '100%',
      display: 'flex', flexDirection: 'column',
      backgroundColor: colors.background,
    }}>
      {/* Scrollable Content */}
      <div className="ce-scroll" style={{
        flex: 1, overflowY: 'auto', padding: '12px',
      }}>
        {/* Execution Summary */}
        <div style={{
          display: 'flex', alignItems: 'center', gap: '8px',
          marginBottom: '12px', padding: '6px 8px',
          backgroundColor: colors.panel,
          border: `1px solid ${colors.panel}`, borderRadius: '2px',
        }}>
          {result.success
            ? <CheckCircle size={10} style={{ color: colors.secondary }} />
            : <AlertCircle size={10} style={{ color: colors.alert }} />
          }
          <span style={{
            fontSize: '9px', fontWeight: 700, fontFamily: fontFamily,
            color: result.success ? colors.secondary : colors.alert,
            letterSpacing: '0.5px',
          }}>
            {result.success ? t('output.completed') : t('output.failed')}
          </span>
          <span style={{ color: colors.textMuted, fontSize: '9px' }}>|</span>
          <span style={{ color: colors.textMuted, fontSize: '9px', fontFamily: fontFamily, display: 'flex', alignItems: 'center', gap: '3px' }}>
            <Clock size={9} /> {result.execution_time_ms}ms
          </span>
          {result.signals.length > 0 && (
            <>
              <span style={{ color: colors.textMuted, fontSize: '9px' }}>|</span>
              <span style={{ color: colors.accent, fontSize: '9px', fontFamily: fontFamily }}>
                {result.signals.length} {t('output.signals')}
              </span>
            </>
          )}
          <div style={{ flex: 1 }} />
          <button onClick={onClear} style={{
            padding: '2px 6px', backgroundColor: 'transparent',
            border: `1px solid ${colors.panel}`, borderRadius: '2px',
            color: colors.textMuted, fontSize: '8px', fontWeight: 700,
            cursor: 'pointer', fontFamily: fontFamily,
            letterSpacing: '0.5px',
          }}>
            {t('output.clear')}
          </button>
        </div>

        {/* Errors Section */}
        {result.errors.length > 0 && (
          <div style={{
            marginBottom: '12px', padding: '8px',
            border: `1px solid ${colors.alert}`, borderRadius: '2px',
            backgroundColor: `${colors.alert}08`,
          }}>
            <div style={{
              display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px',
            }}>
              <AlertCircle size={10} style={{ color: colors.alert }} />
              <span style={{ color: colors.alert, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: fontFamily }}>
                {t('output.errors')} ({result.errors.length})
              </span>
            </div>
            {result.errors.map((error, idx) => (
              <div key={idx} style={{
                color: colors.alert, fontSize: '10px', marginBottom: '3px',
                paddingLeft: '16px', fontFamily: fontFamily, lineHeight: '1.4',
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
              <BarChart2 size={10} style={{ color: colors.accent }} />
              <span style={{ color: colors.accent, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: fontFamily }}>
                {t('output.signalsSection')} ({result.signals.length})
              </span>
            </div>
            {result.signals.map((signal, idx) => (
              <div key={idx} style={{
                display: 'flex', alignItems: 'center', gap: '8px',
                padding: '5px 8px', marginBottom: '3px',
                backgroundColor: signal.signal_type === 'Buy' ? `${colors.secondary}08` : `${colors.alert}08`,
                border: `1px solid ${signal.signal_type === 'Buy' ? colors.secondary : colors.alert}`,
                borderRadius: '2px',
              }}>
                {signal.signal_type === 'Buy' ? (
                  <TrendingUp size={11} style={{ color: colors.secondary }} />
                ) : (
                  <TrendingDown size={11} style={{ color: colors.alert }} />
                )}
                <div style={{ flex: 1 }}>
                  <span style={{
                    color: signal.signal_type === 'Buy' ? colors.secondary : colors.alert,
                    fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: fontFamily,
                  }}>
                    {signal.signal_type.toUpperCase()}
                  </span>
                  <span style={{ color: colors.text, fontSize: '10px', marginLeft: '8px', fontFamily: fontFamily }}>
                    {signal.message}
                  </span>
                </div>
                {signal.price && (
                  <span style={{ color: colors.warning, fontSize: '10px', fontFamily: fontFamily, fontWeight: 700 }}>
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
              <TerminalIcon size={10} style={{ color: colors.secondary }} />
              <span style={{ color: colors.secondary, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: fontFamily }}>
                {t('output.outputSection')}
              </span>
            </div>
            <pre style={{
              color: colors.text, fontSize: '10px', margin: 0,
              whiteSpace: 'pre-wrap', wordBreak: 'break-word',
              fontFamily: fontFamily,
              backgroundColor: colors.panel,
              padding: '8px',
              border: `1px solid ${colors.panel}`,
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
              <BarChart2 size={10} style={{ color: colors.primary }} />
              <span style={{ color: colors.primary, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: fontFamily }}>
                {t('output.charts')} ({result.plots.length})
              </span>
            </div>
            {result.plots.map((plot, idx) => (
              <FinScriptChart
                key={`${plot.label}-${idx}`}
                plot={plot}
                height={200}
                colors={{ background: colors.background, text: colors.text, panel: colors.panel }}
                fontFamily={fontFamily}
              />
            ))}
          </div>
        )}
      </div>
    </div>
  );
};
