// Financial Analysis Panel - Fincept Style
import React, { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { AlertCircle, CheckCircle, BarChart3, DollarSign, PieChart, Activity, FileText, ChevronDown, ChevronUp, Loader2 } from 'lucide-react';
import type { StockInfo } from '../types';

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

interface FinancialsInput {
  // Income Statement
  revenue?: number;
  cost_of_sales?: number;
  gross_profit?: number;
  operating_income?: number;
  net_income?: number;
  interest_expense?: number;
  basic_eps?: number;
  diluted_eps?: number;
  // Balance Sheet - Assets
  total_assets?: number;
  current_assets?: number;
  cash_equivalents?: number;
  accounts_receivable?: number;
  inventory?: number;
  // Balance Sheet - Liabilities
  total_liabilities?: number;
  current_liabilities?: number;
  long_term_debt?: number;
  short_term_debt?: number;
  // Balance Sheet - Equity
  total_equity?: number;
  // Cash Flow
  operating_cash_flow?: number;
  capex?: number;
}

interface FinancialDataInput {
  company: { ticker: string; name: string; sector?: string; industry?: string; country?: string };
  period: { period_end: string; fiscal_year: number; period_type?: string; fiscal_period?: string };
  financials: FinancialsInput;
}

interface AnalysisResult { success: boolean; metrics?: Record<string, any>; summary?: string; error?: string; results?: any[]; }
type AnalysisType = 'income' | 'balance' | 'cashflow' | 'comprehensive' | 'metrics';

const ANALYSIS_TYPES = [
  { type: 'income' as AnalysisType, label: 'Income', icon: DollarSign },
  { type: 'balance' as AnalysisType, label: 'Balance', icon: PieChart },
  { type: 'cashflow' as AnalysisType, label: 'Cash Flow', icon: Activity },
  { type: 'comprehensive' as AnalysisType, label: 'Full', icon: FileText },
  { type: 'metrics' as AnalysisType, label: 'Metrics', icon: BarChart3 },
];

interface FinancialAnalysisPanelProps {
  ticker?: string;
  companyName?: string;
  stockInfo?: StockInfo | null;
}

export const FinancialAnalysisPanel: React.FC<FinancialAnalysisPanelProps> = ({
  ticker = 'AAPL',
  companyName = 'Apple Inc.',
  stockInfo
}) => {
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

      // Build financials object from stockInfo if available
      const financials: FinancialsInput = {};
      if (stockInfo) {
        // Income statement related
        if (stockInfo.total_revenue !== null) financials.revenue = stockInfo.total_revenue;
        if (stockInfo.gross_profits !== null) {
          // Calculate cost_of_sales from revenue and gross profits
          if (stockInfo.total_revenue !== null) {
            financials.cost_of_sales = stockInfo.total_revenue - stockInfo.gross_profits;
          }
        }
        // Use operating margin to estimate operating income
        if (stockInfo.operating_margins !== null && stockInfo.total_revenue !== null) {
          financials.operating_income = stockInfo.total_revenue * stockInfo.operating_margins;
        }
        // Net income from profit margin
        if (stockInfo.profit_margins !== null && stockInfo.total_revenue !== null) {
          financials.net_income = stockInfo.total_revenue * stockInfo.profit_margins;
        }

        // Balance sheet related
        if (stockInfo.total_cash !== null) financials.cash_equivalents = stockInfo.total_cash;
        if (stockInfo.total_debt !== null) {
          financials.long_term_debt = stockInfo.total_debt;
          // Use total_debt as an approximation for total_liabilities
          financials.total_liabilities = stockInfo.total_debt;
        }

        // Calculate total equity from book value and shares outstanding
        let totalEquity: number | null = null;
        if (stockInfo.book_value !== null && stockInfo.shares_outstanding !== null) {
          totalEquity = stockInfo.book_value * stockInfo.shares_outstanding;
          financials.total_equity = totalEquity;
        }

        // Derive total_assets from total_liabilities + total_equity (balance sheet equation)
        if (totalEquity !== null) {
          const totalLiabilities = financials.total_liabilities || 0;
          financials.total_assets = totalLiabilities + totalEquity;
        }

        // Cash flow related
        if (stockInfo.operating_cashflow !== null) financials.operating_cash_flow = stockInfo.operating_cashflow;
        if (stockInfo.free_cashflow !== null && stockInfo.operating_cashflow !== null) {
          // capex = operating cash flow - free cash flow
          financials.capex = stockInfo.operating_cashflow - stockInfo.free_cashflow;
        }
      }

      const data: FinancialDataInput = {
        company: {
          ticker,
          name: companyName,
          sector: stockInfo?.sector || undefined,
          industry: stockInfo?.industry || undefined,
          country: stockInfo?.country || 'USA'
        },
        period: {
          period_end: new Date().toISOString().split('T')[0],
          fiscal_year: new Date().getFullYear(),
          period_type: 'annual',
          fiscal_period: 'FY'
        },
        financials
      };

      const res = await invoke<string>(cmds[selected], { data });
      const p = JSON.parse(res);
      setResult({ success: p.success !== false, summary: p.summary || p.error, results: p.results, metrics: p.metrics, error: p.error });
    } catch (e) {
      setResult({ success: false, error: e instanceof Error ? e.message : 'Failed' });
    } finally {
      setLoading(false);
    }
  }, [selected, ticker, companyName, stockInfo]);

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
              backgroundColor: COLORS.DARK_BG,
              border: `1px solid ${COLORS.BORDER}`,
              overflow: 'hidden',
            }}>
              {/* Results Header */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                padding: '12px 16px',
                backgroundColor: result.success ? `${COLORS.GREEN}15` : `${COLORS.RED}15`,
                borderBottom: `2px solid ${result.success ? COLORS.GREEN : COLORS.RED}`,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                  {result.success ?
                    <CheckCircle size={20} color={COLORS.GREEN} /> :
                    <AlertCircle size={20} color={COLORS.RED} />
                  }
                  <span style={{
                    color: result.success ? COLORS.GREEN : COLORS.RED,
                    fontWeight: 700,
                    fontSize: '12px',
                    textTransform: 'uppercase',
                    letterSpacing: '0.5px',
                  }}>
                    {result.success ? 'Analysis Complete' : 'Analysis Failed'}
                  </span>
                </div>
                {result.results && (
                  <span style={{
                    color: COLORS.GRAY,
                    fontSize: '10px',
                    backgroundColor: COLORS.PANEL_BG,
                    padding: '4px 8px',
                    borderRadius: '2px',
                  }}>
                    {result.results.length} METRICS
                  </span>
                )}
              </div>

              {/* Error Display */}
              {result.error && !result.success && (
                <div style={{
                  padding: '16px',
                  color: COLORS.RED,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  backgroundColor: `${COLORS.RED}10`,
                }}>
                  {result.error}
                </div>
              )}

              {/* Results Grid */}
              {result.success && result.results && result.results.length > 0 && (
                <div style={{ padding: '16px' }}>
                  {/* Metrics Grid */}
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
                    gap: '12px',
                    maxHeight: '400px',
                    overflowY: 'auto',
                  }}>
                    {result.results.map((r: any, i: number) => {
                      const riskColor = r.risk_level === 'low' ? COLORS.GREEN :
                                       r.risk_level === 'high' ? COLORS.RED :
                                       r.risk_level === 'medium' ? COLORS.YELLOW : COLORS.CYAN;
                      const formatValue = (val: any) => {
                        if (typeof val === 'number') {
                          if (val >= 1000000000) return `${(val / 1000000000).toFixed(2)}B`;
                          if (val >= 1000000) return `${(val / 1000000).toFixed(2)}M`;
                          if (val >= 1000) return `${(val / 1000).toFixed(2)}K`;
                          if (val < 1 && val > 0) return `${(val * 100).toFixed(1)}%`;
                          return val.toFixed(2);
                        }
                        return val;
                      };

                      return (
                        <div
                          key={i}
                          style={{
                            backgroundColor: COLORS.PANEL_BG,
                            border: `1px solid ${COLORS.BORDER}`,
                            padding: '12px',
                            display: 'flex',
                            flexDirection: 'column',
                            gap: '8px',
                          }}
                        >
                          {/* Metric Header */}
                          <div style={{
                            display: 'flex',
                            justifyContent: 'space-between',
                            alignItems: 'flex-start',
                          }}>
                            <span style={{
                              color: COLORS.WHITE,
                              fontSize: '10px',
                              fontWeight: 700,
                              textTransform: 'uppercase',
                              letterSpacing: '0.5px',
                              flex: 1,
                            }}>
                              {r.metric_name?.replace(/_/g, ' ')}
                            </span>
                            <span style={{
                              color: riskColor,
                              fontSize: '9px',
                              fontWeight: 600,
                              textTransform: 'uppercase',
                              padding: '2px 6px',
                              backgroundColor: `${riskColor}20`,
                              border: `1px solid ${riskColor}`,
                              marginLeft: '8px',
                              whiteSpace: 'nowrap',
                            }}>
                              {r.risk_level || 'N/A'}
                            </span>
                          </div>

                          {/* Value */}
                          <div style={{
                            color: COLORS.ORANGE,
                            fontSize: '20px',
                            fontWeight: 700,
                            fontFamily: 'monospace',
                          }}>
                            {formatValue(r.value)}
                          </div>

                          {/* Interpretation */}
                          {r.interpretation && (
                            <div style={{
                              color: COLORS.GRAY,
                              fontSize: '10px',
                              lineHeight: '1.4',
                              borderTop: `1px solid ${COLORS.BORDER}`,
                              paddingTop: '8px',
                              maxHeight: '60px',
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                            }}>
                              {r.interpretation.length > 150
                                ? `${r.interpretation.substring(0, 150)}...`
                                : r.interpretation}
                            </div>
                          )}

                          {/* Analysis Type Badge */}
                          {r.analysis_type && (
                            <div style={{
                              color: COLORS.CYAN,
                              fontSize: '8px',
                              textTransform: 'uppercase',
                              letterSpacing: '0.5px',
                            }}>
                              {r.analysis_type}
                            </div>
                          )}
                        </div>
                      );
                    })}
                  </div>
                </div>
              )}

              {/* Key Metrics Summary (for get_key_metrics) */}
              {result.success && result.metrics && Object.keys(result.metrics).length > 0 && (
                <div style={{ padding: '16px' }}>
                  <div style={{
                    color: COLORS.ORANGE,
                    fontSize: '11px',
                    fontWeight: 700,
                    textTransform: 'uppercase',
                    letterSpacing: '0.5px',
                    marginBottom: '12px',
                    paddingBottom: '8px',
                    borderBottom: `1px solid ${COLORS.BORDER}`,
                  }}>
                    Key Metrics
                  </div>
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))',
                    gap: '12px',
                  }}>
                    {Object.entries(result.metrics).map(([key, value]: [string, any], i: number) => (
                      <div
                        key={i}
                        style={{
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          padding: '10px',
                        }}
                      >
                        <div style={{
                          color: COLORS.GRAY,
                          fontSize: '9px',
                          textTransform: 'uppercase',
                          letterSpacing: '0.5px',
                          marginBottom: '4px',
                        }}>
                          {key.replace(/_/g, ' ')}
                        </div>
                        <div style={{
                          color: COLORS.CYAN,
                          fontSize: '14px',
                          fontWeight: 700,
                          fontFamily: 'monospace',
                        }}>
                          {typeof value === 'number'
                            ? (value >= 1000000
                                ? `${(value / 1000000).toFixed(2)}M`
                                : value.toFixed(2))
                            : value}
                        </div>
                      </div>
                    ))}
                  </div>
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