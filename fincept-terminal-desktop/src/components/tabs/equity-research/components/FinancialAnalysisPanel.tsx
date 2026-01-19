// Financial Analysis Panel - Fincept Style
import React, { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { AlertCircle, CheckCircle, BarChart3, DollarSign, PieChart, Activity, FileText, ChevronDown, ChevronUp, Loader2 } from 'lucide-react';

// Fincept color constants
const COLORS = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  YELLOW: '#FFD700',
  GRAY: '#787878',
  CYAN: '#00E5FF',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

interface FinancialsInput { revenue?: number; net_income?: number; total_assets?: number; operating_cash_flow?: number; }
interface FinancialDataInput { company: { ticker: string; name: string; sector?: string; country?: string }; period: { period_end: string; fiscal_year: number }; financials: FinancialsInput; }
interface AnalysisResult { success: boolean; metrics?: Record<string, any>; summary?: string; error?: string; results?: any[]; }
type AnalysisType = 'income' | 'balance' | 'cashflow' | 'comprehensive' | 'metrics';

const ANALYSIS_TYPES = [
  { type: 'income' as AnalysisType, label: 'Income', icon: DollarSign },
  { type: 'balance' as AnalysisType, label: 'Balance', icon: PieChart },
  { type: 'cashflow' as AnalysisType, label: 'Cash Flow', icon: Activity },
  { type: 'comprehensive' as AnalysisType, label: 'Full', icon: FileText },
  { type: 'metrics' as AnalysisType, label: 'Metrics', icon: BarChart3 },
];

export const FinancialAnalysisPanel: React.FC<{ ticker?: string; companyName?: string; }> = ({ ticker = 'AAPL', companyName = 'Apple Inc.' }) => {
  const [selected, setSelected] = useState<AnalysisType>('comprehensive');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<AnalysisResult | null>(null);
  const [expanded, setExpanded] = useState(true);

  const run = useCallback(async () => {
    setLoading(true);
    try {
      const cmds: Record<AnalysisType, string> = {
        income: 'analyze_income_statement',
        balance: 'analyze_balance_sheet',
        cashflow: 'analyze_cash_flow',
        comprehensive: 'analyze_financial_statements',
        metrics: 'get_financial_key_metrics'
      };
      const res = await invoke<string>(cmds[selected], {
        data: {
          company: { ticker, name: companyName },
          period: { period_end: new Date().toISOString().split('T')[0], fiscal_year: new Date().getFullYear() },
          financials: {}
        }
      });
      const p = JSON.parse(res);
      setResult({ success: p.success !== false, summary: p.summary || p.error, results: p.results, metrics: p.metrics, error: p.error });
    } catch (e) {
      setResult({ success: false, error: e instanceof Error ? e.message : 'Failed' });
    } finally {
      setLoading(false);
    }
  }, [selected, ticker, companyName]);

  return (
    <div style={{
      backgroundColor: COLORS.PANEL_BG,
      border: `1px solid ${COLORS.BORDER}`,
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '12px 16px',
          backgroundColor: COLORS.HEADER_BG,
          borderBottom: `2px solid ${COLORS.ORANGE}`,
          cursor: 'pointer',
        }}
        onClick={() => setExpanded(!expanded)}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <BarChart3 size={20} color={COLORS.ORANGE} />
          <h3 style={{
            fontWeight: 700,
            color: COLORS.WHITE,
            fontSize: '12px',
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
          }}>
            Financial Analysis
          </h3>
        </div>
        {expanded ?
          <ChevronUp size={20} color={COLORS.GRAY} /> :
          <ChevronDown size={20} color={COLORS.GRAY} />
        }
      </div>

      {/* Content */}
      {expanded && (
        <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {/* Analysis Type Buttons */}
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
            {ANALYSIS_TYPES.map(({ type, label, icon: Icon }) => (
              <button
                key={type}
                onClick={() => setSelected(type)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  padding: '8px 16px',
                  backgroundColor: selected === type ? COLORS.ORANGE : 'transparent',
                  border: `1px solid ${selected === type ? COLORS.ORANGE : COLORS.BORDER}`,
                  color: selected === type ? COLORS.DARK_BG : COLORS.GRAY,
                  fontSize: '10px',
                  fontWeight: 700,
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  transition: 'all 0.15s ease',
                }}
                onMouseEnter={(e) => {
                  if (selected !== type) {
                    e.currentTarget.style.backgroundColor = COLORS.HOVER;
                    e.currentTarget.style.borderColor = COLORS.ORANGE;
                  }
                }}
                onMouseLeave={(e) => {
                  if (selected !== type) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.borderColor = COLORS.BORDER;
                  }
                }}
              >
                <Icon size={14} />
                <span>{label}</span>
              </button>
            ))}
          </div>

          {/* Run Analysis Button */}
          <button
            onClick={run}
            disabled={loading}
            style={{
              width: '100%',
              padding: '12px',
              backgroundColor: loading ? COLORS.BORDER : COLORS.ORANGE,
              border: 'none',
              color: loading ? COLORS.GRAY : COLORS.DARK_BG,
              fontSize: '11px',
              fontWeight: 700,
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
              cursor: loading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px',
              opacity: loading ? 0.6 : 1,
              transition: 'all 0.15s ease',
            }}
            onMouseEnter={(e) => {
              if (!loading) e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              if (!loading) e.currentTarget.style.opacity = '1';
            }}
          >
            {loading ? (
              <>
                <Loader2 size={18} className="animate-spin" />
                <span>Analyzing...</span>
              </>
            ) : (
              <>
                <BarChart3 size={18} />
                <span>Run Analysis</span>
              </>
            )}
          </button>

          {/* Results */}
          {result && (
            <div style={{
              padding: '16px',
              backgroundColor: result.success ? `${COLORS.GREEN}15` : `${COLORS.RED}15`,
              border: `1px solid ${result.success ? COLORS.GREEN : COLORS.RED}`,
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
                {result.success ?
                  <CheckCircle size={18} color={COLORS.GREEN} /> :
                  <AlertCircle size={18} color={COLORS.RED} />
                }
                <span style={{
                  color: result.success ? COLORS.GREEN : COLORS.RED,
                  fontWeight: 700,
                  fontSize: '11px',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                }}>
                  {result.success ? 'Analysis Complete' : 'Analysis Failed'}
                </span>
              </div>

              {result.summary && (
                <div style={{ color: COLORS.WHITE, fontSize: '10px', marginBottom: '8px' }}>
                  {result.summary}
                </div>
              )}

              {result.error && !result.success && (
                <div style={{ color: COLORS.RED, fontSize: '10px' }}>
                  {result.error}
                </div>
              )}

              {result.results && result.results.length > 0 && (
                <div style={{ marginTop: '8px', maxHeight: '160px', overflowY: 'auto' }}>
                  <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '4px' }}>
                    {result.results.length} metrics analyzed
                  </div>
                  {result.results.slice(0, 5).map((r: any, i: number) => (
                    <div
                      key={i}
                      style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: '10px',
                        padding: '4px 0',
                        borderBottom: `1px solid ${COLORS.BORDER}`,
                      }}
                    >
                      <span style={{ color: COLORS.CYAN, textTransform: 'uppercase' }}>
                        {r.metric_name}
                      </span>
                      <span style={{
                        color: r.risk_level === 'low' ? COLORS.GREEN :
                               r.risk_level === 'high' ? COLORS.RED :
                               COLORS.YELLOW,
                        fontWeight: 600,
                      }}>
                        {typeof r.value === 'number' ? r.value.toFixed(4) : r.value}
                      </span>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default FinancialAnalysisPanel;