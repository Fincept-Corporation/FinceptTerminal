/**
 * FFNView — Full-screen FFN Analytics view within Portfolio Tab
 * Replaces main portfolio layout when active, similar to how DetailViewWrapper works
 * but with a complete dedicated terminal UI for FFN analysis
 */
import React, { useState, useCallback, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Zap, RefreshCw, AlertCircle, BarChart3, Target, TrendingUp, Activity,
  TrendingDown, DollarSign, PieChart, ArrowLeft, Briefcase, Plus, X,
} from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../finceptStyles';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';

interface FFNViewProps {
  portfolioSummary: PortfolioSummary;
  currency: string;
  onBack: () => void;
}

type AnalysisSection = 'overview' | 'benchmark' | 'optimization' | 'rebased' | 'drawdowns' | 'rolling';

interface AnalysisData {
  overview?: any;
  benchmark?: any;
  optimization?: any;
  rebased?: any;
  drawdowns?: any;
  rolling?: any;
}

function safeParse(raw: string): any {
  try { return JSON.parse(raw); } catch { return raw; }
}

const FFNView: React.FC<FFNViewProps> = ({ portfolioSummary, currency, onBack }) => {
  const holdings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
  const defaultSymbols = holdings.map(h => h.symbol);

  // ── State ──
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [activeSection, setActiveSection] = useState<AnalysisSection>('overview');
  const [benchmark, setBenchmark] = useState('SPY');
  const [analysisData, setAnalysisData] = useState<AnalysisData>({});
  const [symbols, setSymbols] = useState<string[]>(defaultSymbols);
  const [symbolInput, setSymbolInput] = useState('');

  // ── Fetch historical prices ──
  const fetchHistoricalPrices = useCallback(async (syms: string[]): Promise<Record<string, Record<string, number>>> => {
    const endDate = new Date().toISOString().split('T')[0];
    const startDate = new Date(Date.now() - 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0];
    const priceMap: Record<string, Record<string, number>> = {};

    const fetches = syms.map(async (sym) => {
      try {
        const raw = await invoke<string>('execute_yfinance_command', {
          command: 'historical', args: [sym, startDate, endDate, '1d'],
        });
        const parsed = JSON.parse(raw);
        if (Array.isArray(parsed) && parsed.length > 0) {
          const datePrices: Record<string, number> = {};
          for (const bar of parsed) {
            const d = new Date(bar.timestamp * 1000).toISOString().split('T')[0];
            datePrices[d] = bar.close;
          }
          priceMap[sym] = datePrices;
        }
      } catch (e) {
        console.error(`Failed to fetch ${sym}:`, e);
      }
    });

    await Promise.all(fetches);
    return priceMap;
  }, []);

  // ── Run analysis ──
  const runAnalysis = useCallback(async (section?: AnalysisSection) => {
    const targetSection = section || activeSection;
    if (symbols.length === 0) return;

    setLoading(true);
    setError(null);

    try {
      const allSymbols = targetSection === 'benchmark' ? [...symbols, benchmark] : symbols;
      const priceMap = await fetchHistoricalPrices(allSymbols);

      const portfolioPrices: Record<string, Record<string, number>> = {};
      for (const sym of symbols) {
        if (priceMap[sym]) portfolioPrices[sym] = priceMap[sym];
      }

      if (Object.keys(portfolioPrices).length === 0) {
        setError('Could not fetch price data for symbols');
        setLoading(false);
        return;
      }

      const pricesJson = JSON.stringify(portfolioPrices);

      if (targetSection === 'overview') {
        const raw = await invoke<string>('ffn_full_analysis', { pricesJson });
        setAnalysisData(prev => ({ ...prev, overview: safeParse(raw) }));
      } else if (targetSection === 'benchmark') {
        const benchmarkPricesJson = priceMap[benchmark]
          ? JSON.stringify({ [benchmark]: priceMap[benchmark] })
          : JSON.stringify({});
        const raw = await invoke<string>('ffn_benchmark_comparison', {
          pricesJson, benchmarkPricesJson, benchmarkName: benchmark,
        });
        setAnalysisData(prev => ({ ...prev, benchmark: safeParse(raw) }));
      } else if (targetSection === 'optimization') {
        const raw = await invoke<string>('ffn_portfolio_optimization', { pricesJson, method: 'erc' });
        setAnalysisData(prev => ({ ...prev, optimization: safeParse(raw) }));
      } else if (targetSection === 'rebased') {
        const raw = await invoke<string>('ffn_rebase_prices', { pricesJson, baseValue: 100.0 });
        setAnalysisData(prev => ({ ...prev, rebased: safeParse(raw) }));
      } else if (targetSection === 'drawdowns') {
        const raw = await invoke<string>('ffn_calculate_drawdowns', { pricesJson });
        setAnalysisData(prev => ({ ...prev, drawdowns: safeParse(raw) }));
      } else if (targetSection === 'rolling') {
        const raw = await invoke<string>('ffn_calculate_rolling_metrics', { pricesJson, window: 30 });
        setAnalysisData(prev => ({ ...prev, rolling: safeParse(raw) }));
      }
    } catch (e: any) {
      setError(e?.message || String(e));
    } finally {
      setLoading(false);
    }
  }, [symbols, activeSection, benchmark, fetchHistoricalPrices]);

  // ── Symbol management ──
  const addSymbol = useCallback(() => {
    const sym = symbolInput.trim().toUpperCase();
    if (sym && !symbols.includes(sym)) {
      setSymbols(prev => [...prev, sym]);
      setSymbolInput('');
    }
  }, [symbolInput, symbols]);

  const removeSymbol = useCallback((sym: string) => {
    setSymbols(prev => prev.filter(s => s !== sym));
  }, []);

  // Auto-run on section change
  useEffect(() => {
    if (symbols.length > 0 && !analysisData[activeSection]) {
      runAnalysis(activeSection);
    }
  }, [activeSection, symbols.length]);

  const currentData = analysisData[activeSection];

  return (
    <div style={{
      height: '100%',
      width: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      overflow: 'hidden',
    }}>
      <style>{`
        ${COMMON_STYLES.scrollbarCSS}
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
      `}</style>

      {/* ═══ COMMAND BAR ═══ */}
      <div style={{
        height: '42px', flexShrink: 0,
        background: 'linear-gradient(180deg, #141414 0%, #0A0A0A 100%)',
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        display: 'flex', alignItems: 'center', padding: '0 12px', gap: '10px',
      }}>
        <button onClick={onBack} style={{
          display: 'flex', alignItems: 'center', gap: '4px',
          padding: '5px 12px',
          backgroundColor: `${FINCEPT.ORANGE}12`,
          border: `1px solid ${FINCEPT.ORANGE}50`,
          color: FINCEPT.ORANGE,
          fontSize: '9px', fontWeight: 700, cursor: 'pointer',
          letterSpacing: '0.5px',
        }}>
          <ArrowLeft size={10} />
          PORTFOLIO
        </button>

        <div style={{ width: '1px', height: '20px', backgroundColor: FINCEPT.BORDER }} />

        <Zap size={14} color={FINCEPT.ORANGE} />
        <span style={{
          fontSize: '12px', fontWeight: 800, color: FINCEPT.ORANGE,
          letterSpacing: '1px',
        }}>
          FFN ANALYTICS
        </span>

        <div style={{ width: '1px', height: '20px', backgroundColor: FINCEPT.BORDER }} />

        {/* Symbol Pills */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', flex: 1, flexWrap: 'wrap' }}>
          {symbols.slice(0, 6).map(sym => (
            <div key={sym} style={{
              padding: '3px 7px',
              backgroundColor: `${FINCEPT.CYAN}15`,
              border: `1px solid ${FINCEPT.CYAN}40`,
              color: FINCEPT.CYAN,
              fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              {sym}
              <button onClick={() => removeSymbol(sym)} style={{
                background: 'none', border: 'none', color: FINCEPT.CYAN,
                cursor: 'pointer', fontSize: '12px', padding: 0, lineHeight: '10px',
              }}>×</button>
            </div>
          ))}
          {symbols.length > 6 && (
            <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>+{symbols.length - 6} more</span>
          )}
          <input
            type="text"
            value={symbolInput}
            onChange={e => setSymbolInput(e.target.value.toUpperCase())}
            onKeyPress={e => e.key === 'Enter' && addSymbol()}
            placeholder="+ ADD"
            style={{
              width: '70px', padding: '3px 6px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
              fontSize: '8px', fontWeight: 600,
            }}
          />
        </div>

        <button onClick={() => runAnalysis()} disabled={loading || symbols.length === 0} style={{
          padding: '6px 14px',
          backgroundColor: loading ? FINCEPT.MUTED : FINCEPT.ORANGE,
          border: 'none', color: '#000',
          fontSize: '9px', fontWeight: 700, letterSpacing: '0.8px',
          cursor: loading ? 'not-allowed' : 'pointer',
          display: 'flex', alignItems: 'center', gap: '4px',
        }}>
          {loading ? <RefreshCw size={9} className="animate-spin" /> : <Zap size={9} />}
          {loading ? 'RUNNING...' : 'RUN'}
        </button>
      </div>

      {/* ═══ STATS RIBBON ═══ */}
      <div style={{
        height: '56px', flexShrink: 0,
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fit, minmax(130px, 1fr))',
        gap: '1px',
        padding: '6px',
      }}>
        {[
          { label: 'PORTFOLIO', value: portfolioSummary.portfolio.name, icon: Briefcase, color: FINCEPT.WHITE },
          { label: 'SYMBOLS', value: symbols.length, icon: PieChart, color: FINCEPT.CYAN },
          { label: 'BENCHMARK', value: benchmark, icon: Target, color: FINCEPT.ORANGE },
          { label: 'PERIOD', value: '1Y', icon: Activity, color: FINCEPT.GREEN },
        ].map(({ label, value, icon: Icon, color }) => (
          <div key={label} style={{
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            padding: '6px 10px',
            display: 'flex', flexDirection: 'column', gap: '3px',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Icon size={9} color={FINCEPT.GRAY} />
              <span style={{
                fontSize: '7px', color: FINCEPT.GRAY, fontWeight: 700,
                letterSpacing: '0.5px',
              }}>
                {label}
              </span>
            </div>
            <div style={{
              fontSize: '11px', color, fontWeight: 700,
              fontFamily: TYPOGRAPHY.MONO,
            }}>
              {value}
            </div>
          </div>
        ))}
      </div>

      {/* ═══ MAIN CONTENT ═══ */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* LEFT SIDEBAR - Section Navigation */}
        <div style={{
          width: '200px', flexShrink: 0,
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex', flexDirection: 'column',
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div style={{
              fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY,
              letterSpacing: '0.8px',
            }}>
              ANALYSIS SECTIONS
            </div>
          </div>

          <div style={{ flex: 1, overflow: 'auto' }}>
            {[
              { id: 'overview' as AnalysisSection, label: 'FULL ANALYSIS', icon: BarChart3 },
              { id: 'benchmark' as AnalysisSection, label: 'BENCHMARK', icon: Target },
              { id: 'optimization' as AnalysisSection, label: 'OPTIMIZATION', icon: TrendingUp },
              { id: 'rebased' as AnalysisSection, label: 'REBASED PRICES', icon: Activity },
              { id: 'drawdowns' as AnalysisSection, label: 'DRAWDOWNS', icon: TrendingDown },
              { id: 'rolling' as AnalysisSection, label: 'ROLLING METRICS', icon: Activity },
            ].map(({ id, label, icon: Icon }) => {
              const active = activeSection === id;
              return (
                <button key={id} onClick={() => setActiveSection(id)} style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: active ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: active ? `3px solid ${FINCEPT.ORANGE}` : '3px solid transparent',
                  border: 'none',
                  borderBottom: `1px solid ${FINCEPT.BORDER}40`,
                  color: active ? FINCEPT.ORANGE : FINCEPT.GRAY,
                  fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
                  textAlign: 'left',
                  cursor: 'pointer',
                  display: 'flex', alignItems: 'center', gap: '6px',
                }}>
                  <Icon size={11} />
                  {label}
                </button>
              );
            })}
          </div>

          {/* Benchmark input in sidebar */}
          {activeSection === 'benchmark' && (
            <div style={{
              padding: '10px 12px',
              borderTop: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <div style={{
                fontSize: '7px', color: FINCEPT.GRAY, fontWeight: 700,
                marginBottom: '6px', letterSpacing: '0.5px',
              }}>
                BENCHMARK SYMBOL
              </div>
              <input
                type="text"
                value={benchmark}
                onChange={e => setBenchmark(e.target.value.toUpperCase())}
                style={{
                  width: '100%',
                  padding: '5px 7px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.CYAN,
                  fontSize: '9px', fontWeight: 700,
                  fontFamily: TYPOGRAPHY.MONO,
                }}
              />
            </div>
          )}
        </div>

        {/* RIGHT CONTENT AREA */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
          {/* Error Banner */}
          {error && (
            <div style={{
              padding: '10px 16px',
              backgroundColor: `${FINCEPT.RED}15`,
              borderBottom: `1px solid ${FINCEPT.RED}`,
              display: 'flex', alignItems: 'center', gap: '8px',
            }}>
              <AlertCircle size={11} color={FINCEPT.RED} />
              <span style={{ color: FINCEPT.RED, fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }}>
                {error}
              </span>
            </div>
          )}

          {/* Data Display */}
          <div style={{
            flex: 1,
            overflow: 'auto',
            padding: '20px 32px',
            width: '100%',
            maxWidth: '100%',
            boxSizing: 'border-box',
          }}>
            {!currentData && !loading && !error && (
              <EmptyState section={activeSection} />
            )}
            {currentData && (
              <div style={{ width: '100%', maxWidth: '100%' }}>
                <ResultSection title={getSectionTitle(activeSection)} data={currentData} />
              </div>
            )}
          </div>
        </div>
      </div>

      {/* ═══ STATUS BAR ═══ */}
      <div style={{
        height: '22px', flexShrink: 0,
        backgroundColor: FINCEPT.PANEL_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex', alignItems: 'center', padding: '0 12px', gap: '12px',
        fontSize: '7px', color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO,
      }}>
        <span>FFN v0.3.6</span>
        <div style={{ width: '1px', height: '10px', backgroundColor: FINCEPT.BORDER }} />
        <span>PYTHON BACKEND</span>
        <div style={{ flex: 1 }} />
        <span>{symbols.length} SYMBOL{symbols.length !== 1 ? 'S' : ''}</span>
        <div style={{ width: '1px', height: '10px', backgroundColor: FINCEPT.BORDER }} />
        <span>{currency}</span>
      </div>
    </div>
  );
};

// ── Helper Components ──

const EmptyState: React.FC<{ section: AnalysisSection }> = ({ section }) => (
  <div style={{ textAlign: 'center', padding: '80px 0' }}>
    <Zap size={40} color={FINCEPT.MUTED} style={{ opacity: 0.2, marginBottom: '16px' }} />
    <div style={{
      color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700,
      letterSpacing: '1px', marginBottom: '8px',
    }}>
      {getSectionTitle(section).toUpperCase()}
    </div>
    <div style={{
      color: FINCEPT.MUTED, fontSize: '9px', maxWidth: '500px',
      margin: '0 auto', lineHeight: '14px',
    }}>
      Click RUN to compute {section} analysis for your portfolio symbols
    </div>
  </div>
);

const ResultSection: React.FC<{ title: string; data: any }> = ({ title, data }) => {
  // Helper to get metric color based on value
  const getMetricColor = (key: string, value: any): string => {
    if (typeof value !== 'number') return FINCEPT.WHITE;

    const k = key.toLowerCase();
    // Positive-is-good metrics
    if (k.includes('return') || k.includes('sharpe') || k.includes('sortino') ||
        k.includes('calmar') || k.includes('best') || k.includes('cagr')) {
      return value >= 0 ? FINCEPT.GREEN : FINCEPT.RED;
    }
    // Negative-is-bad metrics
    if (k.includes('drawdown') || k.includes('worst') || k.includes('volatility')) {
      return value < 0 ? FINCEPT.RED : FINCEPT.YELLOW;
    }
    return FINCEPT.CYAN;
  };

  // Render performance comparison table for multiple assets
  const renderPerformanceTable = (performanceData: Record<string, any>) => {
    const assets = Object.keys(performanceData);
    if (assets.length === 0) return null;

    // Get all metric keys from first asset
    const metrics = Object.keys(performanceData[assets[0]]);
    const keyMetrics = ['total_return', 'cagr', 'sharpe_ratio', 'sortino_ratio', 'max_drawdown', 'volatility', 'calmar_ratio'];
    const displayMetrics = metrics.filter(m => keyMetrics.includes(m));

    return (
      <div style={{ marginBottom: '32px' }}>
        <div style={{
          fontSize: '11px',
          fontWeight: 800,
          color: FINCEPT.ORANGE,
          letterSpacing: '1px',
          marginBottom: '16px',
          textTransform: 'uppercase',
          paddingBottom: '8px',
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        }}>
          PERFORMANCE COMPARISON
        </div>
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          overflow: 'auto',
        }}>
          <table style={{
            width: '100%',
            borderCollapse: 'collapse',
            fontFamily: TYPOGRAPHY.MONO,
          }}>
            <thead>
              <tr style={{ backgroundColor: '#0D0D0D', borderBottom: `2px solid ${FINCEPT.ORANGE}` }}>
                <th style={{
                  padding: '12px 16px',
                  textAlign: 'left',
                  fontSize: '10px',
                  fontWeight: 800,
                  color: FINCEPT.GRAY,
                  letterSpacing: '0.8px',
                  position: 'sticky',
                  left: 0,
                  backgroundColor: '#0D0D0D',
                  zIndex: 1,
                }}>
                  METRIC
                </th>
                {assets.map(asset => (
                  <th key={asset} style={{
                    padding: '12px 16px',
                    textAlign: 'right',
                    fontSize: '10px',
                    fontWeight: 800,
                    color: FINCEPT.CYAN,
                    letterSpacing: '0.8px',
                  }}>
                    {asset}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {displayMetrics.map((metric, idx) => (
                <tr key={metric} style={{
                  borderBottom: `1px solid ${FINCEPT.BORDER}40`,
                  backgroundColor: idx % 2 === 0 ? FINCEPT.DARK_BG : 'transparent',
                }}>
                  <td style={{
                    padding: '12px 16px',
                    fontSize: '10px',
                    fontWeight: 700,
                    color: FINCEPT.WHITE,
                    letterSpacing: '0.5px',
                    position: 'sticky',
                    left: 0,
                    backgroundColor: idx % 2 === 0 ? FINCEPT.DARK_BG : FINCEPT.PANEL_BG,
                    zIndex: 1,
                  }}>
                    {metric.replace(/_/g, ' ').toUpperCase()}
                  </td>
                  {assets.map(asset => {
                    const value = performanceData[asset][metric];
                    return (
                      <td key={asset} style={{
                        padding: '12px 16px',
                        textAlign: 'right',
                        fontSize: '13px',
                        fontWeight: 700,
                        color: getMetricColor(metric, value),
                      }}>
                        {typeof value === 'number' ? formatValue(value) : '-'}
                      </td>
                    );
                  })}
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    );
  };

  // Render monthly returns heat map
  const renderMonthlyReturnsHeatmap = (monthlyData: Record<string, Record<string, number>>) => {
    const years = Object.keys(monthlyData).filter(k => k !== 'year');
    if (years.length === 0) return null;

    const months = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12'];
    const monthNames = ['JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC'];

    return (
      <div style={{ marginBottom: '32px' }}>
        <div style={{
          fontSize: '11px',
          fontWeight: 800,
          color: FINCEPT.ORANGE,
          letterSpacing: '1px',
          marginBottom: '16px',
          textTransform: 'uppercase',
          paddingBottom: '8px',
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        }}>
          MONTHLY RETURNS HEATMAP
        </div>
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          padding: '16px',
          overflow: 'auto',
        }}>
          <table style={{
            width: '100%',
            borderCollapse: 'separate',
            borderSpacing: '4px',
            fontFamily: TYPOGRAPHY.MONO,
          }}>
            <thead>
              <tr>
                <th style={{
                  padding: '8px',
                  fontSize: '9px',
                  fontWeight: 800,
                  color: FINCEPT.GRAY,
                  textAlign: 'center',
                }}>
                  YEAR
                </th>
                {monthNames.map(m => (
                  <th key={m} style={{
                    padding: '8px',
                    fontSize: '9px',
                    fontWeight: 800,
                    color: FINCEPT.GRAY,
                    textAlign: 'center',
                  }}>
                    {m}
                  </th>
                ))}
                <th style={{
                  padding: '8px',
                  fontSize: '9px',
                  fontWeight: 800,
                  color: FINCEPT.ORANGE,
                  textAlign: 'center',
                }}>
                  YTD
                </th>
              </tr>
            </thead>
            <tbody>
              {years.map(year => {
                const yearData = monthlyData[year];
                const ytd = yearData['year'] || 0;
                return (
                  <tr key={year}>
                    <td style={{
                      padding: '10px',
                      fontSize: '11px',
                      fontWeight: 800,
                      color: FINCEPT.CYAN,
                      textAlign: 'center',
                      backgroundColor: '#0D0D0D',
                    }}>
                      {year}
                    </td>
                    {months.map(month => {
                      const value = yearData[month];
                      const hasValue = value !== null && value !== undefined;
                      const bgColor = !hasValue ? FINCEPT.DARK_BG :
                        value > 0.05 ? '#00441B' :
                        value > 0.02 ? '#00662B' :
                        value > 0 ? '#00883A' :
                        value > -0.02 ? '#440000' :
                        value > -0.05 ? '#660000' : '#880000';

                      return (
                        <td key={month} style={{
                          padding: '10px',
                          fontSize: '11px',
                          fontWeight: 700,
                          color: FINCEPT.WHITE,
                          textAlign: 'center',
                          backgroundColor: bgColor,
                          border: `1px solid ${FINCEPT.BORDER}`,
                        }}>
                          {hasValue ? `${(value * 100).toFixed(1)}%` : '-'}
                        </td>
                      );
                    })}
                    <td style={{
                      padding: '10px',
                      fontSize: '12px',
                      fontWeight: 800,
                      color: ytd >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                      textAlign: 'center',
                      backgroundColor: '#0D0D0D',
                      border: `2px solid ${FINCEPT.ORANGE}`,
                    }}>
                      {typeof ytd === 'number' ? `${(ytd * 100).toFixed(1)}%` : '-'}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      </div>
    );
  };

  // Render drawdown cards
  const renderDrawdowns = (drawdownsData: any) => {
    if (!drawdownsData) return null;

    const topDrawdowns = drawdownsData.top_drawdowns || [];
    const maxDrawdown = drawdownsData.max_drawdown;

    return (
      <div style={{ marginBottom: '32px' }}>
        <div style={{
          fontSize: '11px',
          fontWeight: 800,
          color: FINCEPT.ORANGE,
          letterSpacing: '1px',
          marginBottom: '16px',
          textTransform: 'uppercase',
          paddingBottom: '8px',
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        }}>
          DRAWDOWN ANALYSIS
        </div>

        {/* Max Drawdown Banner */}
        {typeof maxDrawdown === 'number' && (
          <div style={{
            padding: '20px 24px',
            backgroundColor: `${FINCEPT.RED}15`,
            border: `2px solid ${FINCEPT.RED}`,
            borderLeft: `6px solid ${FINCEPT.RED}`,
            marginBottom: '20px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY, fontWeight: 700, marginBottom: '6px', letterSpacing: '0.8px' }}>
                MAXIMUM DRAWDOWN
              </div>
              <div style={{ fontSize: '28px', color: FINCEPT.RED, fontWeight: 800, fontFamily: TYPOGRAPHY.MONO }}>
                {(maxDrawdown * 100).toFixed(2)}%
              </div>
            </div>
            <TrendingDown size={32} color={FINCEPT.RED} style={{ opacity: 0.5 }} />
          </div>
        )}

        {/* Top Drawdowns Timeline */}
        {Array.isArray(topDrawdowns) && topDrawdowns.length > 0 && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {topDrawdowns.map((dd: any, idx: number) => (
              <div key={idx} style={{
                padding: '16px 20px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: `4px solid ${FINCEPT.RED}`,
                display: 'grid',
                gridTemplateColumns: '40px 1fr 1fr 120px',
                gap: '20px',
                alignItems: 'center',
              }}>
                <div style={{
                  width: '32px',
                  height: '32px',
                  borderRadius: '50%',
                  backgroundColor: `${FINCEPT.RED}20`,
                  border: `2px solid ${FINCEPT.RED}`,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontSize: '12px',
                  fontWeight: 800,
                  color: FINCEPT.RED,
                }}>
                  {idx + 1}
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', letterSpacing: '0.5px' }}>
                    START DATE
                  </div>
                  <div style={{ fontSize: '12px', color: FINCEPT.WHITE, fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                    {dd.start || '-'}
                  </div>
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', letterSpacing: '0.5px' }}>
                    END DATE
                  </div>
                  <div style={{ fontSize: '12px', color: FINCEPT.WHITE, fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                    {dd.end || '-'}
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', letterSpacing: '0.5px' }}>
                    DRAWDOWN
                  </div>
                  <div style={{ fontSize: '18px', color: FINCEPT.RED, fontWeight: 800, fontFamily: TYPOGRAPHY.MONO }}>
                    {typeof dd.drawdown === 'number' ? `${(dd.drawdown * 100).toFixed(2)}%` : '-'}
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    );
  };

  // Helper to render nested objects recursively
  const renderMetrics = (obj: any, depth: number = 0, parentKey: string = ''): React.ReactElement[] => {
    if (!obj || typeof obj !== 'object') return [];

    const entries = Object.entries(obj);
    const elements: React.ReactElement[] = [];

    // Separate scalar and object values
    const scalars = entries.filter(([, v]) => v !== null && v !== undefined && typeof v !== 'object' && !Array.isArray(v));
    const objects = entries.filter(([, v]) => typeof v === 'object' && v !== null && !Array.isArray(v));
    const arrays = entries.filter(([, v]) => Array.isArray(v));

    // Render scalar values in grid
    if (scalars.length > 0) {
      // Calculate columns based on depth for better distribution
      const minCardWidth = depth === 2 ? 140 : depth === 1 ? 150 : 160;

      elements.push(
        <div key={`scalars-${depth}-${parentKey}`} style={{
          display: 'grid',
          gridTemplateColumns: `repeat(auto-fit, minmax(${minCardWidth}px, 1fr))`,
          gap: '16px',
          marginBottom: (objects.length > 0 || arrays.length > 0) ? '28px' : 0,
          width: '100%',
          maxWidth: '100%',
          boxSizing: 'border-box',
        }}>
          {scalars.map(([k, v]) => (
            <div key={k} style={{
              padding: '16px 18px',
              backgroundColor: depth === 0 ? FINCEPT.PANEL_BG : depth === 1 ? '#0D0D0D' : FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderLeft: `4px solid ${getMetricColor(k, v)}`,
              transition: 'all 0.2s',
              cursor: 'default',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              e.currentTarget.style.borderLeft = `4px solid ${getMetricColor(k, v)}`;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = depth === 0 ? FINCEPT.PANEL_BG : depth === 1 ? '#0D0D0D' : FINCEPT.DARK_BG;
              e.currentTarget.style.borderLeft = `4px solid ${getMetricColor(k, v)}`;
            }}
            >
              <div style={{
                fontSize: '10px',
                color: FINCEPT.GRAY,
                fontWeight: 700,
                letterSpacing: '0.8px',
                marginBottom: '10px',
                textTransform: 'uppercase',
              }}>
                {k.replace(/_/g, ' ')}
              </div>
              <div style={{
                color: getMetricColor(k, v),
                fontSize: depth === 2 ? '15px' : '18px',
                fontWeight: 800,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: '0.3px',
              }}>
                {formatValue(v)}
              </div>
            </div>
          ))}
        </div>
      );
    }

    // Render nested objects with headers (e.g., asset-specific metrics)
    objects.forEach(([sKey, sData]) => {
      // Check if this is an asset section (depth === 1 under PERFORMANCE)
      const isAsset = depth === 1 && parentKey === 'performance';

      elements.push(
        <div key={sKey} style={{ marginBottom: '32px', width: '100%', maxWidth: '100%', boxSizing: 'border-box' }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
            padding: '10px 14px',
            backgroundColor: isAsset ? `${FINCEPT.CYAN}15` : `${FINCEPT.ORANGE}10`,
            borderLeft: `4px solid ${isAsset ? FINCEPT.CYAN : depth === 0 ? FINCEPT.ORANGE : FINCEPT.PURPLE}`,
            marginBottom: '16px',
          }}>
            <div style={{
              fontSize: isAsset ? '13px' : depth === 0 ? '11px' : '10px',
              fontWeight: 800,
              color: isAsset ? FINCEPT.CYAN : depth === 0 ? FINCEPT.ORANGE : FINCEPT.PURPLE,
              letterSpacing: '1px',
              textTransform: 'uppercase',
            }}>
              {sKey.replace(/_/g, ' ')}
            </div>
          </div>
          {renderMetrics(sData, depth + 1, sKey.toLowerCase())}
        </div>
      );
    });

    // Render arrays (e.g., top drawdowns, assets list)
    arrays.forEach(([aKey, aData]) => {
      if (Array.isArray(aData) && aData.length > 0) {
        // Check if array contains objects (like drawdowns) or just values (like asset names)
        const isObjectArray = typeof aData[0] === 'object';

        elements.push(
          <div key={aKey} style={{ marginBottom: '24px', width: '100%' }}>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.YELLOW,
              letterSpacing: '0.8px',
              marginBottom: '12px',
              textTransform: 'uppercase',
            }}>
              {aKey.replace(/_/g, ' ')} ({aData.length})
            </div>

            {isObjectArray ? (
              // Render object arrays as cards (e.g., drawdowns)
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {aData.map((item: any, idx: number) => (
                  <div key={idx} style={{
                    padding: '12px 14px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${FINCEPT.RED}`,
                    display: 'grid',
                    gridTemplateColumns: 'repeat(auto-fit, minmax(150px, 1fr))',
                    gap: '12px',
                  }}>
                    {Object.entries(item).map(([k, v]) => (
                      <div key={k}>
                        <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', textTransform: 'uppercase' }}>
                          {k}
                        </div>
                        <div style={{ fontSize: '12px', color: FINCEPT.WHITE, fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                          {typeof v === 'number' ? formatValue(v) : String(v)}
                        </div>
                      </div>
                    ))}
                  </div>
                ))}
              </div>
            ) : (
              // Render simple arrays as pills (e.g., asset names)
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
                {aData.map((item: any, idx: number) => (
                  <div key={idx} style={{
                    padding: '8px 14px',
                    backgroundColor: `${FINCEPT.CYAN}20`,
                    border: `1px solid ${FINCEPT.CYAN}`,
                    color: FINCEPT.CYAN,
                    fontSize: '11px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                  }}>
                    {String(item)}
                  </div>
                ))}
              </div>
            )}
          </div>
        );
      }
    });

    return elements;
  };

  // Smart rendering based on data structure
  const renderContent = () => {
    if (!data || typeof data !== 'object') {
      return (
        <div style={{
          color: FINCEPT.MUTED, fontSize: '10px', fontFamily: TYPOGRAPHY.MONO,
          padding: '16px', backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          whiteSpace: 'pre-wrap', maxHeight: '400px', overflow: 'auto',
        }}>
          {typeof data === 'string' ? data : JSON.stringify(data, null, 2)}
        </div>
      );
    }

    const components: React.ReactElement[] = [];

    // Render performance comparison table if we have multi-asset performance data
    if (data.performance && typeof data.performance === 'object') {
      const perfKeys = Object.keys(data.performance);
      if (perfKeys.length > 0 && typeof data.performance[perfKeys[0]] === 'object') {
        components.push(<div key="perf-table">{renderPerformanceTable(data.performance)}</div>);
      }
    }

    // Render monthly returns heatmap
    if (data.monthly_returns && typeof data.monthly_returns === 'object') {
      components.push(<div key="monthly-heatmap">{renderMonthlyReturnsHeatmap(data.monthly_returns)}</div>);
    }

    // Render drawdowns analysis
    if (data.drawdowns) {
      components.push(<div key="drawdowns">{renderDrawdowns(data.drawdowns)}</div>);
    }

    // Render remaining metrics using the standard grid
    const remaining = { ...data };
    delete remaining.performance;
    delete remaining.monthly_returns;
    delete remaining.drawdowns;
    delete remaining.success; // Don't show success flag

    const metrics = renderMetrics(remaining, 0, '');
    if (metrics.length > 0) {
      components.push(<div key="metrics">{metrics}</div>);
    }

    return components.length > 0 ? components : null;
  };

  const content = renderContent();

  return (
    <div style={{ width: '100%', maxWidth: '100%', boxSizing: 'border-box' }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        padding: '14px 16px',
        marginBottom: '24px',
        background: `linear-gradient(90deg, ${FINCEPT.ORANGE}20 0%, transparent 100%)`,
        borderLeft: `4px solid ${FINCEPT.ORANGE}`,
      }}>
        <BarChart3 size={16} color={FINCEPT.ORANGE} />
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: '14px',
          fontWeight: 800,
          letterSpacing: '1.5px',
        }}>
          {title}
        </div>
      </div>

      {content || (
        <div style={{
          color: FINCEPT.MUTED, fontSize: '10px', fontFamily: TYPOGRAPHY.MONO,
          padding: '16px', backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          whiteSpace: 'pre-wrap', maxHeight: '400px', overflow: 'auto',
        }}>
          No data available
        </div>
      )}
    </div>
  );
};

// ── Utilities ──

function getSectionTitle(section: AnalysisSection): string {
  const titles: Record<AnalysisSection, string> = {
    'overview': 'FULL PORTFOLIO ANALYSIS',
    'benchmark': 'BENCHMARK COMPARISON',
    'optimization': 'PORTFOLIO OPTIMIZATION (ERC)',
    'rebased': 'REBASED PRICE SERIES',
    'drawdowns': 'DRAWDOWN ANALYSIS',
    'rolling': 'ROLLING METRICS',
  };
  return titles[section];
}

function formatValue(v: any): string {
  if (typeof v === 'number') {
    return Math.abs(v) < 1 ? v.toFixed(4) : v.toFixed(2);
  }
  return String(v);
}

export default FFNView;
