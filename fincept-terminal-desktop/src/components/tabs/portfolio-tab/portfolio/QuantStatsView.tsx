import React, { useState, useCallback, useEffect, useMemo } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { quantstatsService, FullReport, TimeSeriesPoint, MonteCarloDistribution, MonteCarloSimulationPaths } from '../../../../services/quantstatsService';
import { notificationService } from '../../../../services/notifications';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../finceptStyles';
import { cacheService } from '../../../../services/cache/cacheService';
import {
  BarChart3, TrendingDown, Activity, RefreshCw, Download, AlertCircle, Bell, Zap,
} from 'lucide-react';

/** Decode a base64 string to a Uint8Array (handles non-ASCII correctly). */
function base64ToUint8Array(base64: string): Uint8Array {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

interface QuantStatsViewProps {
  portfolioSummary: PortfolioSummary;
}

const BENCHMARK_SUGGESTIONS = [
  { value: 'SPY', label: 'S&P 500' },
  { value: 'QQQ', label: 'Nasdaq 100' },
  { value: 'IWM', label: 'Russell 2000' },
  { value: 'DIA', label: 'Dow Jones' },
  { value: 'VTI', label: 'Total US Market' },
  { value: 'EFA', label: 'EAFE Intl' },
  { value: 'EEM', label: 'Emerging Mkts' },
  { value: 'AGG', label: 'US Bonds' },
  { value: '^GSPC', label: 'S&P 500 Index' },
  { value: '^NSEI', label: 'Nifty 50' },
  { value: '^BSESN', label: 'Sensex' },
  { value: '^FTSE', label: 'FTSE 100' },
  { value: '^N225', label: 'Nikkei 225' },
  { value: '^HSI', label: 'Hang Seng' },
  { value: '^GDAXI', label: 'DAX' },
  { value: 'GLD', label: 'Gold ETF' },
  { value: 'BTC-USD', label: 'Bitcoin' },
  { value: 'ETH-USD', label: 'Ethereum' },
];
const PERIODS = [
  { value: '3mo', label: '3M' },
  { value: '6mo', label: '6M' },
  { value: '1y', label: '1Y' },
  { value: '2y', label: '2Y' },
  { value: '5y', label: '5Y' },
  { value: 'max', label: 'MAX' },
];

const MONTH_NAMES = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

function fmt(val: number | null | undefined, decimals = 2, suffix = ''): string {
  if (val === null || val === undefined) return 'N/A';
  return `${(val * 100).toFixed(decimals)}${suffix}`;
}

function fmtRaw(val: number | null | undefined, decimals = 2): string {
  if (val === null || val === undefined) return 'N/A';
  return val.toFixed(decimals);
}

function getReturnColor(val: number | null | undefined): string {
  if (val === null || val === undefined) return FINCEPT.GRAY;
  return val >= 0 ? FINCEPT.GREEN : FINCEPT.RED;
}

function getHeatmapColor(val: number | null | undefined): string {
  if (val === null || val === undefined) return FINCEPT.PANEL_BG;
  const v = val * 100;
  if (v > 5) return 'rgba(0,214,111,0.4)';
  if (v > 2) return 'rgba(0,214,111,0.25)';
  if (v > 0) return 'rgba(0,214,111,0.1)';
  if (v > -2) return 'rgba(255,59,59,0.1)';
  if (v > -5) return 'rgba(255,59,59,0.25)';
  return 'rgba(255,59,59,0.4)';
}

const QuantStatsView: React.FC<QuantStatsViewProps> = ({ portfolioSummary }) => {
  const [report, setReport] = useState<FullReport | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [benchmark, setBenchmark] = useState('SPY');
  const [period, setPeriod] = useState('1y');
  const [activeSection, setActiveSection] = useState<'metrics' | 'returns' | 'drawdown' | 'rolling' | 'montecarlo'>('metrics');

  // ── FFN + Fortitudo state ──
  const [fortitudoMetrics, setFortitudoMetrics] = useState<any>(null);

  const buildTickersWeights = useCallback(() => {
    const tickers: Record<string, number> = {};
    const validHoldings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);

    if (validHoldings.length === 0) return tickers;

    // Check if weights are available
    const hasWeights = validHoldings.some(h => h.weight > 0);

    if (hasWeights) {
      for (const h of validHoldings) {
        tickers[h.symbol] = (h.weight || 0) / 100;
      }
    } else {
      // Equal weight fallback
      const equalWeight = 1 / validHoldings.length;
      for (const h of validHoldings) {
        tickers[h.symbol] = equalWeight;
      }
    }
    return tickers;
  }, [portfolioSummary.holdings]);

  // ── Cache key: per portfolio composition + benchmark + period ────────────────
  const cacheKey = useMemo(() => {
    const validHoldings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
    const symbols = validHoldings.map(h => h.symbol).sort().join(',');
    const weights = validHoldings.map(h => Math.round(h.weight * 10)).join(',');
    return `quantstats:${symbols}:${weights}:${benchmark}:${period}`;
  }, [portfolioSummary.holdings, benchmark, period]);

  const fortitudoCacheKey = useMemo(() => {
    const validHoldings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
    const symbols = validHoldings.map(h => h.symbol).sort().join(',');
    const weights = validHoldings.map(h => Math.round(h.weight * 10)).join(',');
    return `quantstats:fortitudo:${symbols}:${weights}`;
  }, [portfolioSummary.holdings]);

  // ── Restore cached results on mount / when key changes ───────────────────────
  useEffect(() => {
    let cancelled = false;
    const restore = async () => {
      const [cachedReport, cachedFortitudo] = await Promise.all([
        cacheService.get<FullReport>(cacheKey),
        cacheService.get<any>(fortitudoCacheKey),
      ]);
      if (cancelled) return;
      if (cachedReport) setReport(cachedReport.data);
      if (cachedFortitudo) setFortitudoMetrics(cachedFortitudo.data);
    };
    restore();
    return () => { cancelled = true; };
  }, [cacheKey, fortitudoCacheKey]);

  const runAnalysis = useCallback(async () => {
    const tickers = buildTickersWeights();
    if (Object.keys(tickers).length === 0) {
      setError('No holdings with valid symbols found');
      return;
    }

    console.log('[QuantStats] Running analysis with tickers:', tickers, 'benchmark:', benchmark, 'period:', period);
    setLoading(true);
    setError(null);
    try {
      const data = await quantstatsService.getFullReport(tickers, benchmark, period);
      setReport(data);
      cacheService.set(cacheKey, data, 'api-response', '1h');
    } catch (e) {
      console.error('[QuantStats] Analysis error:', e);
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setLoading(false);
    }
  }, [buildTickersWeights, benchmark, period, cacheKey]);

  const generateHtmlReport = useCallback(async () => {
    const tickers = buildTickersWeights();
    if (Object.keys(tickers).length === 0) return;

    try {
      const data = await quantstatsService.getHtmlReport(tickers, benchmark, period);
      const bytes = base64ToUint8Array(data.html_base64);
      const blob = new Blob([bytes], { type: 'text/html' });
      const url = URL.createObjectURL(blob);
      window.open(url, '_blank');
    } catch (e) {
      setError(e instanceof Error ? e.message : 'HTML report generation failed');
    }
  }, [buildTickersWeights, benchmark, period]);

  const [sendingNotification, setSendingNotification] = useState(false);

  const sendReportNotification = useCallback(async () => {
    const tickers = buildTickersWeights();
    if (Object.keys(tickers).length === 0) return;
    setSendingNotification(true);
    try {
      if (!notificationService.isInitialized()) {
        await notificationService.initialize();
      }

      const portfolioName = portfolioSummary.portfolio.name || 'Portfolio';

      // Generate full HTML tearsheet and send as file
      const htmlData = await quantstatsService.getHtmlReport(tickers, benchmark, period);
      const bytes = base64ToUint8Array(htmlData.html_base64);
      const blob = new Blob([bytes], { type: 'text/html' });
      const dateStr = new Date().toISOString().slice(0, 10);
      const filename = `QuantStats_${portfolioName.replace(/[^a-zA-Z0-9]/g, '_')}_${dateStr}.html`;

      // Build caption with key metrics
      const s = report?.stats;
      const captionLines = [
        `*QuantStats Report: ${portfolioName}*`,
        `Period: ${period} | Benchmark: ${benchmark}`,
      ];
      if (s) {
        captionLines.push(
          `CAGR: ${s.cagr !== null ? (s.cagr * 100).toFixed(2) + '%' : 'N/A'}`,
          `Sharpe: ${s.sharpe !== null ? s.sharpe.toFixed(2) : 'N/A'}`,
          `Max DD: ${s.max_drawdown !== null ? (s.max_drawdown * 100).toFixed(2) + '%' : 'N/A'}`,
        );
      }

      await notificationService.notifyWithFile({
        filename,
        content: blob,
        caption: captionLines.join('\n'),
      });

      setError(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to send report');
    } finally {
      setSendingNotification(false);
    }
  }, [report, portfolioSummary, benchmark, period, buildTickersWeights]);

  const buildReturnsJson = useCallback(() => {
    const symbols = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0).map(h => h.symbol);
    return JSON.stringify(symbols.join(','));
  }, [portfolioSummary.holdings]);

  const buildWeightsJson = useCallback(() => {
    const holdings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
    const weights = holdings.map(h => h.weight / 100);
    return JSON.stringify(weights);
  }, [portfolioSummary.holdings]);

  const fetchFortitudoMetrics = useCallback(async () => {
    try {
      const returnsJson = buildReturnsJson();
      const weightsJson = buildWeightsJson();
      const result = await invoke<string>('fortitudo_portfolio_metrics', { returnsJson, weightsJson, alpha: 0.05 });
      let parsed: any;
      try { parsed = JSON.parse(result); } catch { parsed = result; }
      setFortitudoMetrics(parsed);
      cacheService.set(fortitudoCacheKey, parsed, 'api-response', '1h');
    } catch { /* silent */ }
  }, [buildReturnsJson, buildWeightsJson, fortitudoCacheKey]);

  const sections = [
    { id: 'metrics' as const, label: 'METRICS', icon: BarChart3 },
    { id: 'returns' as const, label: 'RETURNS', icon: Activity },
    { id: 'drawdown' as const, label: 'DRAWDOWNS', icon: TrendingDown },
    { id: 'rolling' as const, label: 'ROLLING', icon: Activity },
    { id: 'montecarlo' as const, label: 'MONTE CARLO', icon: BarChart3 },
  ];

  // Mini SVG chart renderer for time series data
  const renderMiniChart = (data: TimeSeriesPoint[], color: string, height = 80) => {
    if (!data || data.length < 2) return null;
    const values = data.map(d => d.value).filter((v): v is number => v !== null);
    if (values.length < 2) return null;

    const width = 600;
    const min = Math.min(...values);
    const max = Math.max(...values);
    const range = max - min || 1;

    const points = values.map((v, i) => {
      const x = (i / (values.length - 1)) * width;
      const y = height - ((v - min) / range) * (height - 10) - 5;
      return `${x},${y}`;
    }).join(' ');

    const areaPoints = points + ` ${width},${height} 0,${height}`;

    return (
      <svg width="100%" height={height} viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none">
        <polygon points={areaPoints} fill={color} opacity="0.1" />
        <polyline points={points} fill="none" stroke={color} strokeWidth="1.5" />
      </svg>
    );
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexWrap: 'wrap',
      }}>
        <BarChart3 size={16} color={FINCEPT.ORANGE} />
        <span style={{
          color: FINCEPT.ORANGE,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          QUANTSTATS ANALYTICS
        </span>
        <div style={{ flex: 1 }} />

        {/* Benchmark selector */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ color: FINCEPT.WHITE, fontSize: '10px', letterSpacing: '0.5px', fontFamily: 'monospace', opacity: 0.7 }}>BENCHMARK:</span>
          <input
            list="benchmark-suggestions"
            value={benchmark}
            onChange={(e) => setBenchmark(e.target.value.toUpperCase())}
            placeholder="SPY"
            style={{
              background: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.CYAN}`,
              color: FINCEPT.CYAN,
              padding: '6px 10px',
              fontSize: '11px',
              fontFamily: 'monospace',
              fontWeight: 600,
              width: '100px',
              outline: 'none',
            }}
          />
          <datalist id="benchmark-suggestions">
            {BENCHMARK_SUGGESTIONS.map(b => (
              <option key={b.value} value={b.value}>{b.label}</option>
            ))}
          </datalist>
        </div>

        {/* Period selector */}
        <div style={{ display: 'flex', gap: '4px' }}>
          {PERIODS.map(p => (
            <button
              key={p.value}
              onClick={() => setPeriod(p.value)}
              style={{
                padding: '6px 12px',
                backgroundColor: period === p.value ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                border: `1px solid ${period === p.value ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                color: period === p.value ? '#000000' : FINCEPT.WHITE,
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: 'monospace',
                cursor: 'pointer',
                transition: 'all 0.15s',
                letterSpacing: '0.5px',
                opacity: period === p.value ? 1 : 0.7
              }}
              onMouseEnter={(e) => {
                if (period !== p.value) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  e.currentTarget.style.color = FINCEPT.ORANGE;
                  e.currentTarget.style.opacity = '1';
                }
              }}
              onMouseLeave={(e) => {
                if (period !== p.value) {
                  e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.color = FINCEPT.WHITE;
                  e.currentTarget.style.opacity = '0.7';
                }
              }}
            >
              {p.label}
            </button>
          ))}
        </div>

        {/* Run Analysis button */}
        <button
          onClick={runAnalysis}
          disabled={loading}
          style={{
            padding: '8px 16px',
            backgroundColor: FINCEPT.ORANGE,
            border: 'none',
            color: '#000000',
            fontSize: '11px',
            fontWeight: 700,
            fontFamily: 'monospace',
            cursor: loading ? 'not-allowed' : 'pointer',
            opacity: loading ? 0.6 : 1,
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            letterSpacing: '0.5px',
            transition: 'all 0.15s'
          }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
          {loading ? 'ANALYZING...' : 'RUN ANALYSIS'}
        </button>

        {/* HTML Report button */}
        {report && (
          <button
            onClick={generateHtmlReport}
            style={{
              padding: '8px 14px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.CYAN}`,
              color: FINCEPT.CYAN,
              fontSize: '10px',
              fontWeight: 700,
              fontFamily: 'monospace',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              letterSpacing: '0.5px',
              transition: 'all 0.15s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.CYAN + '30';
              e.currentTarget.style.color = FINCEPT.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
              e.currentTarget.style.color = FINCEPT.CYAN;
            }}
          >
            <Download size={12} />
            HTML
          </button>
        )}

        {/* Send via Notification button */}
        {report && (
          <button
            onClick={sendReportNotification}
            disabled={sendingNotification}
            style={{
              padding: '8px 14px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.GREEN}`,
              color: sendingNotification ? FINCEPT.MUTED : FINCEPT.GREEN,
              fontSize: '10px',
              fontWeight: 700,
              fontFamily: 'monospace',
              cursor: sendingNotification ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              letterSpacing: '0.5px',
              transition: 'all 0.15s',
              opacity: sendingNotification ? 0.6 : 1,
            }}
            onMouseEnter={(e) => {
              if (!sendingNotification) {
                e.currentTarget.style.backgroundColor = FINCEPT.GREEN + '30';
                e.currentTarget.style.color = FINCEPT.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              if (!sendingNotification) {
                e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                e.currentTarget.style.color = FINCEPT.GREEN;
              }
            }}
          >
            <Bell size={12} />
            {sendingNotification ? 'SENDING...' : 'SEND'}
          </button>
        )}
      </div>

      {/* Main Content Area */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px',
      }}>

        {/* Error */}
        {error && (
          <div style={{
            padding: '12px 14px',
            backgroundColor: FINCEPT.RED + '15',
            border: `1px solid ${FINCEPT.RED}`,
            borderLeft: `3px solid ${FINCEPT.RED}`,
            marginBottom: '16px',
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
          }}>
            <AlertCircle size={16} color={FINCEPT.RED} />
            <span style={{ color: FINCEPT.RED, fontSize: '11px', fontFamily: 'monospace' }}>{error}</span>
          </div>
        )}

        {/* No data state */}
        {!report && !loading && !error && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '400px',
            textAlign: 'center',
          }}>
            <BarChart3 size={48} color={FINCEPT.MUTED} style={{ opacity: 0.3, marginBottom: '16px' }} />
            <div style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '8px', fontFamily: 'monospace' }}>
              QUANTSTATS ANALYTICS READY
            </div>
            <div style={{ color: FINCEPT.MUTED, fontSize: '11px', maxWidth: '500px', lineHeight: '1.6' }}>
              Select benchmark, period, and click RUN ANALYSIS to compute 50+ performance metrics,<br />
              drawdown analysis, rolling statistics, and Monte Carlo simulations.
            </div>
          </div>
        )}

        {/* Loading */}
        {loading && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '400px',
            textAlign: 'center',
          }}>
            <RefreshCw size={32} color={FINCEPT.ORANGE} className="animate-spin" style={{ marginBottom: '16px' }} />
            <div style={{ color: FINCEPT.ORANGE, fontSize: '12px', fontWeight: 700, fontFamily: 'monospace', letterSpacing: '0.5px' }}>
              COMPUTING ANALYTICS...
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginTop: '8px' }}>
              Downloading price data and computing metrics
            </div>
          </div>
        )}

        {/* Results */}
        {report && !loading && (
          <>
            {/* Section tabs */}
            <div style={{
              display: 'flex',
              gap: '6px',
              marginBottom: '16px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              paddingBottom: '8px',
            }}>
              {sections.map(s => {
                const Icon = s.icon;
                const isActive = activeSection === s.id;
                return (
                  <button
                    key={s.id}
                    onClick={() => setActiveSection(s.id)}
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      padding: '8px 14px',
                      backgroundColor: isActive ? FINCEPT.ORANGE + '30' : FINCEPT.PANEL_BG,
                      border: `1px solid ${isActive ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                      color: isActive ? FINCEPT.ORANGE : FINCEPT.WHITE,
                      fontSize: '10px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      cursor: 'pointer',
                      letterSpacing: '0.5px',
                      transition: 'all 0.15s',
                      opacity: isActive ? 1 : 0.7
                    }}
                    onMouseEnter={(e) => {
                      if (!isActive) {
                        e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                        e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                        e.currentTarget.style.color = FINCEPT.ORANGE;
                        e.currentTarget.style.opacity = '1';
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (!isActive) {
                        e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                        e.currentTarget.style.borderColor = FINCEPT.BORDER;
                        e.currentTarget.style.color = FINCEPT.WHITE;
                        e.currentTarget.style.opacity = '0.7';
                      }
                    }}
                  >
                    <Icon size={12} />
                    {s.label}
                  </button>
                );
              })}
            </div>

          {/* METRICS SECTION */}
          {activeSection === 'metrics' && report.stats && (
            <div>
              {/* Performance Metrics */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.GREEN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: `1px solid ${FINCEPT.GREEN}`,
                }}>
                  PERFORMANCE METRICS
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: SPACING.MEDIUM }}>
                  {([
                    ['CAGR', report.stats.cagr, true],
                    ['Cumul. Return', report.stats.cumulative_return, true],
                    ['Sharpe Ratio', report.stats.sharpe, false],
                    ['Sortino Ratio', report.stats.sortino, false],
                    ['Calmar Ratio', report.stats.calmar, false],
                    ['Win Rate', report.stats.win_rate, true],
                    ['Profit Factor', report.stats.profit_factor, false],
                    ['Payoff Ratio', report.stats.payoff_ratio, false],
                    ['Kelly Criterion', report.stats.kelly_criterion, true],
                    ['Best Day', report.stats.best_day, true],
                    ['Worst Day', report.stats.worst_day, true],
                    ['Best Month', report.stats.best_month, true],
                    ['Worst Month', report.stats.worst_month, true],
                  ] as [string, number | null, boolean][]).map(([label, val, isPct]) => (
                    <div key={label} style={{
                      ...COMMON_STYLES.metricCard,
                      padding: SPACING.MEDIUM,
                      borderRadius: '2px',
                    }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{label}</div>
                      <div style={{
                        color: getReturnColor(val),
                        fontSize: '13px',
                        fontWeight: 700,
                        fontFamily: TYPOGRAPHY.MONO,
                      }}>
                        {isPct ? fmt(val, 2, '%') : fmtRaw(val)}
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* Risk Metrics */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.RED,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: `1px solid ${FINCEPT.RED}`,
                }}>
                  RISK METRICS
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: SPACING.MEDIUM }}>
                  {([
                    ['Volatility', report.stats.volatility, true],
                    ['Max Drawdown', report.stats.max_drawdown, true],
                    ['Max DD Duration', report.stats.max_drawdown_duration, false, ' days'],
                    ['VaR (95%)', report.stats.var_95, true],
                    ['CVaR (95%)', report.stats.cvar_95, true],
                    ['VaR (99%)', report.stats.var_99, true],
                    ['Skewness', report.stats.skew, false],
                    ['Kurtosis', report.stats.kurtosis, false],
                    ['Tail Ratio', report.stats.tail_ratio, false],
                    ['Common Sense', report.stats.common_sense_ratio, false],
                    ['Risk of Ruin', report.stats.risk_of_ruin, true],
                  ] as [string, number | null, boolean, string?][]).map(([label, val, isPct, suffix]) => (
                    <div key={label} style={{
                      ...COMMON_STYLES.metricCard,
                      padding: SPACING.MEDIUM,
                      borderRadius: '2px',
                      border: `1px solid ${FINCEPT.RED}30`,
                    }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{label}</div>
                      <div style={{
                        color: FINCEPT.RED,
                        fontSize: '13px',
                        fontWeight: 700,
                        fontFamily: TYPOGRAPHY.MONO,
                      }}>
                        {isPct ? fmt(val, 2, '%') : (val !== null && val !== undefined ? `${fmtRaw(val)}${suffix || ''}` : 'N/A')}
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* Advanced Metrics */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.YELLOW,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: `1px solid ${FINCEPT.YELLOW}`,
                }}>
                  ADVANCED METRICS
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: SPACING.MEDIUM }}>
                  {([
                    ['Smart Sharpe', report.stats.smart_sharpe, false],
                    ['Smart Sortino', report.stats.smart_sortino, false],
                    ['Adj. Sortino', report.stats.adjusted_sortino, false],
                    ['Omega Ratio', report.stats.omega, false],
                    ['Ulcer Index', report.stats.ulcer_index, false],
                    ['UPI', report.stats.upi, false],
                    ['Serenity Index', report.stats.serenity_index, false],
                    ['Risk/Return', report.stats.risk_return_ratio, false],
                    ['Recovery Factor', report.stats.recovery_factor, false],
                    ['CPC Index', report.stats.cpc_index, false],
                    ['Exposure', report.stats.exposure, true],
                    ['Expected Return', report.stats.expected_return, true],
                    ['Prob. Sharpe', report.stats.probabilistic_sharpe_ratio, true],
                    ['RAR', report.stats.rar, true],
                    ['Consec. Wins', report.stats.consecutive_wins, false, ''],
                    ['Consec. Losses', report.stats.consecutive_losses, false, ''],
                  ] as [string, number | null, boolean, string?][]).map(([label, val, isPct, suffix]) => (
                    <div key={label} style={{
                      ...COMMON_STYLES.metricCard,
                      padding: SPACING.MEDIUM,
                      borderRadius: '2px',
                      border: `1px solid ${FINCEPT.YELLOW}30`,
                    }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{label}</div>
                      <div style={{
                        color: FINCEPT.YELLOW,
                        fontSize: '13px',
                        fontWeight: 700,
                        fontFamily: TYPOGRAPHY.MONO,
                      }}>
                        {isPct ? fmt(val, 2, '%') : (val !== null && val !== undefined ? `${fmtRaw(val)}${suffix || ''}` : 'N/A')}
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* Fortitudo Risk Metrics (Python) */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: `1px solid ${FINCEPT.CYAN}`,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                }}>
                  FORTITUDO VaR / CVaR
                  <button
                    onClick={fetchFortitudoMetrics}
                    style={{
                      padding: '2px 8px', backgroundColor: `${FINCEPT.CYAN}20`,
                      border: `1px solid ${FINCEPT.CYAN}60`, color: FINCEPT.CYAN,
                      fontSize: '8px', fontWeight: 700, cursor: 'pointer', fontFamily: 'monospace',
                    }}
                  >
                    <Zap size={8} style={{ marginRight: 3 }} />
                    RUN
                  </button>
                </div>
                {fortitudoMetrics && typeof fortitudoMetrics === 'object' ? (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: SPACING.MEDIUM }}>
                    {Object.entries(fortitudoMetrics).filter(([k]) => typeof fortitudoMetrics[k] === 'number' || typeof fortitudoMetrics[k] === 'string').map(([key, val]) => (
                      <div key={key} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px', border: `1px solid ${FINCEPT.CYAN}30` }}>
                        <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{key.replace(/_/g, ' ').toUpperCase()}</div>
                        <div style={{ color: FINCEPT.CYAN, fontSize: '13px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                          {typeof val === 'number' ? (val as number).toFixed(4) : String(val)}
                        </div>
                      </div>
                    ))}
                  </div>
                ) : (
                  <div style={{ color: FINCEPT.MUTED, fontSize: '10px' }}>Click RUN to compute Fortitudo VaR/CVaR metrics via Python</div>
                )}
              </div>

              {/* Benchmark Comparison */}
              {report.stats.benchmark_cagr !== null && (
                <div>
                  <div style={{
                    color: FINCEPT.CYAN,
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    marginBottom: SPACING.MEDIUM,
                    paddingBottom: SPACING.SMALL,
                    borderBottom: `1px solid ${FINCEPT.CYAN}`,
                  }}>
                    BENCHMARK COMPARISON ({benchmark})
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: SPACING.MEDIUM }}>
                    {([
                      ['Alpha', report.stats.alpha, true],
                      ['Beta', report.stats.beta, false],
                      ['Correlation', report.stats.correlation, false],
                      ['Information Ratio', report.stats.information_ratio, false],
                      ['Treynor Ratio', report.stats.treynor_ratio, false],
                      ['R-Squared', report.stats.r_squared, false],
                      ['Excess Return', report.stats.excess_return, true],
                    ] as [string, number | null, boolean][]).map(([label, val, isPct]) => (
                      <div key={label} style={{
                        ...COMMON_STYLES.metricCard,
                        padding: SPACING.MEDIUM,
                        borderRadius: '2px',
                        border: `1px solid ${FINCEPT.CYAN}30`,
                      }}>
                        <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{label}</div>
                        <div style={{
                          color: FINCEPT.CYAN,
                          fontSize: '13px',
                          fontWeight: 700,
                          fontFamily: TYPOGRAPHY.MONO,
                        }}>
                          {isPct ? fmt(val, 2, '%') : fmtRaw(val)}
                        </div>
                      </div>
                    ))}
                  </div>

                  {/* Side-by-side comparison table */}
                  <div style={{
                    marginTop: SPACING.LARGE,
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: BORDERS.STANDARD,
                    borderRadius: '2px',
                    overflow: 'hidden',
                  }}>
                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: '2fr 1fr 1fr',
                      backgroundColor: FINCEPT.HEADER_BG,
                      borderBottom: BORDERS.ORANGE,
                      padding: SPACING.MEDIUM,
                    }}>
                      <div style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700 }}>METRIC</div>
                      <div style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700, textAlign: 'right' }}>PORTFOLIO</div>
                      <div style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700, textAlign: 'right' }}>{benchmark}</div>
                    </div>
                    {([
                      ['CAGR', report.stats.cagr, report.stats.benchmark_cagr],
                      ['Sharpe', report.stats.sharpe, report.stats.benchmark_sharpe],
                      ['Volatility', report.stats.volatility, report.stats.benchmark_volatility],
                      ['Max Drawdown', report.stats.max_drawdown, report.stats.benchmark_max_drawdown],
                    ] as [string, number | null, number | null][]).map(([label, pVal, bVal], i) => (
                      <div key={label} style={{
                        display: 'grid',
                        gridTemplateColumns: '2fr 1fr 1fr',
                        padding: SPACING.MEDIUM,
                        backgroundColor: i % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                        borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      }}>
                        <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>{label}</div>
                        <div style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700, textAlign: 'right' }}>
                          {label === 'Sharpe' ? fmtRaw(pVal) : fmt(pVal, 2, '%')}
                        </div>
                        <div style={{ color: FINCEPT.YELLOW, fontSize: '10px', fontWeight: 700, textAlign: 'right' }}>
                          {label === 'Sharpe' ? fmtRaw(bVal) : fmt(bVal, 2, '%')}
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          )}

          {/* RETURNS SECTION */}
          {activeSection === 'returns' && report.returns && (
            <div>
              {/* Distribution Stats */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: BORDERS.ORANGE,
                }}>
                  RETURN DISTRIBUTION
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(120px, 1fr))', gap: SPACING.MEDIUM }}>
                  {([
                    ['Mean Daily', report.returns.distribution.mean, true],
                    ['Std Dev', report.returns.distribution.std, true],
                    ['Skewness', report.returns.distribution.skew, false],
                    ['Kurtosis', report.returns.distribution.kurtosis, false],
                    ['Min', report.returns.distribution.min, true],
                    ['Max', report.returns.distribution.max, true],
                    ['Median', report.returns.distribution.median, true],
                  ] as [string, number | null, boolean][]).map(([label, val, isPct]) => (
                    <div key={label} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px' }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{label}</div>
                      <div style={{ color: FINCEPT.CYAN, fontSize: '12px', fontWeight: 700 }}>
                        {isPct ? fmt(val, 3, '%') : fmtRaw(val, 3)}
                      </div>
                    </div>
                  ))}
                  <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px' }}>
                    <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>Win/Loss Days</div>
                    <div style={{ fontSize: '12px', fontWeight: 700 }}>
                      <span style={{ color: FINCEPT.GREEN }}>{report.returns.distribution.positive_days}</span>
                      <span style={{ color: FINCEPT.GRAY }}>/</span>
                      <span style={{ color: FINCEPT.RED }}>{report.returns.distribution.negative_days}</span>
                    </div>
                  </div>
                </div>
              </div>

              {/* Monthly Returns Heatmap */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: BORDERS.ORANGE,
                }}>
                  MONTHLY RETURNS HEATMAP
                </div>
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: BORDERS.STANDARD,
                  borderRadius: '2px',
                  padding: SPACING.DEFAULT,
                  overflow: 'auto',
                }}>
                  {/* Header row */}
                  <div style={{ display: 'grid', gridTemplateColumns: '60px repeat(12, 1fr)', gap: '2px', marginBottom: '2px' }}>
                    <div style={{ color: FINCEPT.GRAY, fontSize: '8px', fontWeight: 700, padding: '4px' }}>YEAR</div>
                    {MONTH_NAMES.map(m => (
                      <div key={m} style={{ color: FINCEPT.GRAY, fontSize: '8px', fontWeight: 700, textAlign: 'center', padding: '4px' }}>
                        {m}
                      </div>
                    ))}
                  </div>
                  {/* Data rows */}
                  {Object.keys(report.returns.heatmap).sort().map(year => (
                    <div key={year} style={{ display: 'grid', gridTemplateColumns: '60px repeat(12, 1fr)', gap: '2px', marginBottom: '2px' }}>
                      <div style={{ color: FINCEPT.CYAN, fontSize: '9px', fontWeight: 700, padding: '4px' }}>{year}</div>
                      {Array.from({ length: 12 }, (_, i) => {
                        const val = report.returns.heatmap[year]?.[String(i + 1)] ?? null;
                        return (
                          <div
                            key={i}
                            style={{
                              backgroundColor: getHeatmapColor(val),
                              color: getReturnColor(val),
                              fontSize: '9px',
                              fontWeight: 700,
                              textAlign: 'center',
                              padding: '4px 2px',
                              borderRadius: '1px',
                              fontFamily: TYPOGRAPHY.MONO,
                            }}
                          >
                            {val !== null ? fmt(val, 1, '%') : '-'}
                          </div>
                        );
                      })}
                    </div>
                  ))}
                </div>
              </div>

              {/* Yearly Returns */}
              <div>
                <div style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: BORDERS.ORANGE,
                }}>
                  YEARLY RETURNS
                </div>
                <div style={{ display: 'flex', gap: SPACING.MEDIUM, flexWrap: 'wrap' }}>
                  {report.returns.yearly_returns.map(yr => (
                    <div key={yr.year} style={{
                      ...COMMON_STYLES.metricCard,
                      padding: SPACING.MEDIUM,
                      borderRadius: '2px',
                      minWidth: '80px',
                      textAlign: 'center',
                      border: `1px solid ${getReturnColor(yr.return)}30`,
                    }}>
                      <div style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700 }}>{yr.year}</div>
                      <div style={{
                        color: getReturnColor(yr.return),
                        fontSize: '13px',
                        fontWeight: 700,
                      }}>
                        {fmt(yr.return, 1, '%')}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          )}

          {/* DRAWDOWN SECTION */}
          {activeSection === 'drawdown' && report.drawdown && (
            <div>
              {/* Drawdown Chart */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.RED,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: `1px solid ${FINCEPT.RED}`,
                }}>
                  DRAWDOWN CHART
                </div>
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: BORDERS.STANDARD,
                  borderRadius: '2px',
                  padding: SPACING.DEFAULT,
                }}>
                  {renderMiniChart(
                    report.drawdown.drawdown_series.map(d => ({ date: d.date, value: d.drawdown })),
                    FINCEPT.RED,
                    120
                  )}
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    marginTop: SPACING.SMALL,
                    fontSize: '8px',
                    color: FINCEPT.GRAY,
                  }}>
                    <span>{report.drawdown.drawdown_series[0]?.date}</span>
                    <span>{report.drawdown.drawdown_series[report.drawdown.drawdown_series.length - 1]?.date}</span>
                  </div>
                </div>
              </div>

              {/* Drawdown Periods Table */}
              {report.drawdown.drawdown_periods.length > 0 && (
                <div>
                  <div style={{
                    color: FINCEPT.RED,
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    marginBottom: SPACING.MEDIUM,
                    paddingBottom: SPACING.SMALL,
                    borderBottom: `1px solid ${FINCEPT.RED}`,
                  }}>
                    TOP DRAWDOWN PERIODS
                  </div>
                  <div style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: BORDERS.STANDARD,
                    borderRadius: '2px',
                    overflow: 'hidden',
                  }}>
                    {/* Table header */}
                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: 'repeat(auto-fit, minmax(100px, 1fr))',
                      backgroundColor: FINCEPT.HEADER_BG,
                      borderBottom: BORDERS.ORANGE,
                      padding: SPACING.MEDIUM,
                    }}>
                      {Object.keys(report.drawdown.drawdown_periods[0] || {}).map(key => (
                        <div key={key} style={{
                          color: FINCEPT.ORANGE,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px',
                          textTransform: 'uppercase',
                        }}>
                          {key.replace(/_/g, ' ')}
                        </div>
                      ))}
                    </div>
                    {/* Table rows */}
                    {report.drawdown.drawdown_periods.map((period, i) => (
                      <div key={i} style={{
                        display: 'grid',
                        gridTemplateColumns: 'repeat(auto-fit, minmax(100px, 1fr))',
                        padding: SPACING.MEDIUM,
                        backgroundColor: i % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                        borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      }}>
                        {Object.values(period).map((val, j) => (
                          <div key={j} style={{
                            color: FINCEPT.CYAN,
                            fontSize: '9px',
                            fontFamily: TYPOGRAPHY.MONO,
                          }}>
                            {typeof val === 'number' && !Number.isInteger(val)
                              ? (val * 100).toFixed(2) + '%'
                              : String(val ?? 'N/A')}
                          </div>
                        ))}
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          )}

          {/* ROLLING SECTION */}
          {activeSection === 'rolling' && report.rolling && (
            <div>
              {/* Cumulative Returns */}
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{
                  color: FINCEPT.GREEN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  marginBottom: SPACING.MEDIUM,
                  paddingBottom: SPACING.SMALL,
                  borderBottom: `1px solid ${FINCEPT.GREEN}`,
                }}>
                  CUMULATIVE RETURNS
                </div>
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: BORDERS.STANDARD,
                  borderRadius: '2px',
                  padding: SPACING.DEFAULT,
                }}>
                  {renderMiniChart(report.rolling.cumulative_returns, FINCEPT.GREEN, 120)}
                  {report.rolling.benchmark_cumulative_returns && (
                    <div style={{ marginTop: '-120px', position: 'relative' }}>
                      {renderMiniChart(report.rolling.benchmark_cumulative_returns, FINCEPT.YELLOW, 120)}
                    </div>
                  )}
                  <div style={{ display: 'flex', gap: SPACING.LARGE, marginTop: SPACING.SMALL }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <div style={{ width: '12px', height: '2px', backgroundColor: FINCEPT.GREEN }} />
                      <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>Portfolio</span>
                    </div>
                    {report.rolling.benchmark_cumulative_returns && (
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                        <div style={{ width: '12px', height: '2px', backgroundColor: FINCEPT.YELLOW }} />
                        <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>{benchmark}</span>
                      </div>
                    )}
                  </div>
                </div>
              </div>

              {/* Rolling Sharpe */}
              {report.rolling.rolling_sharpe_63d && report.rolling.rolling_sharpe_63d.length > 0 && (
                <div style={{ marginBottom: SPACING.XLARGE }}>
                  <div style={{
                    color: FINCEPT.CYAN,
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    marginBottom: SPACING.MEDIUM,
                    paddingBottom: SPACING.SMALL,
                    borderBottom: `1px solid ${FINCEPT.CYAN}`,
                  }}>
                    ROLLING SHARPE RATIO (3M)
                  </div>
                  <div style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: BORDERS.STANDARD,
                    borderRadius: '2px',
                    padding: SPACING.DEFAULT,
                  }}>
                    {renderMiniChart(report.rolling.rolling_sharpe_63d, FINCEPT.CYAN, 100)}
                  </div>
                </div>
              )}

              {/* Rolling Volatility */}
              {report.rolling.rolling_volatility_63d && report.rolling.rolling_volatility_63d.length > 0 && (
                <div style={{ marginBottom: SPACING.XLARGE }}>
                  <div style={{
                    color: FINCEPT.YELLOW,
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    marginBottom: SPACING.MEDIUM,
                    paddingBottom: SPACING.SMALL,
                    borderBottom: `1px solid ${FINCEPT.YELLOW}`,
                  }}>
                    ROLLING VOLATILITY (3M)
                  </div>
                  <div style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: BORDERS.STANDARD,
                    borderRadius: '2px',
                    padding: SPACING.DEFAULT,
                  }}>
                    {renderMiniChart(report.rolling.rolling_volatility_63d, FINCEPT.YELLOW, 100)}
                  </div>
                </div>
              )}

              {/* Rolling Sortino */}
              {report.rolling.rolling_sortino_63d && report.rolling.rolling_sortino_63d.length > 0 && (
                <div>
                  <div style={{
                    color: FINCEPT.ORANGE,
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    marginBottom: SPACING.MEDIUM,
                    paddingBottom: SPACING.SMALL,
                    borderBottom: BORDERS.ORANGE,
                  }}>
                    ROLLING SORTINO RATIO (3M)
                  </div>
                  <div style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: BORDERS.STANDARD,
                    borderRadius: '2px',
                    padding: SPACING.DEFAULT,
                  }}>
                    {renderMiniChart(report.rolling.rolling_sortino_63d, FINCEPT.ORANGE, 100)}
                  </div>
                </div>
              )}
            </div>
          )}

          {/* MONTE CARLO SECTION */}
          {activeSection === 'montecarlo' && report.montecarlo && (
            <div>
              <div style={{
                color: FINCEPT.ORANGE,
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                marginBottom: SPACING.LARGE,
                paddingBottom: SPACING.SMALL,
                borderBottom: BORDERS.ORANGE,
              }}>
                MONTE CARLO SIMULATION (1,000 PATHS)
              </div>

              {/* Simulation Paths Chart */}
              {report.montecarlo.simulation_paths && report.montecarlo.simulation_paths.paths.length > 0 && (() => {
                const sp = report.montecarlo.simulation_paths!;
                const chartW = 800;
                const chartH = 300;
                const padL = 50;
                const padR = 10;
                const padT = 10;
                const padB = 25;
                const plotW = chartW - padL - padR;
                const plotH = chartH - padT - padB;
                const steps = sp.time_steps;

                // Find global min/max across all paths and bands
                let globalMin = Infinity;
                let globalMax = -Infinity;
                for (const path of sp.paths) {
                  for (const v of path) {
                    if (v !== null) {
                      if (v < globalMin) globalMin = v;
                      if (v > globalMax) globalMax = v;
                    }
                  }
                }
                if (sp.p5_band) for (const v of sp.p5_band) { if (v !== null && v < globalMin) globalMin = v; }
                if (sp.p95_band) for (const v of sp.p95_band) { if (v !== null && v > globalMax) globalMax = v; }
                const range = globalMax - globalMin || 1;

                const toX = (i: number) => padL + (i / (steps - 1)) * plotW;
                const toY = (v: number) => padT + plotH - ((v - globalMin) / range) * plotH;

                const pathToPolyline = (vals: (number | null)[]) => {
                  return vals
                    .map((v, i) => v !== null ? `${toX(i).toFixed(1)},${toY(v).toFixed(1)}` : null)
                    .filter(Boolean)
                    .join(' ');
                };

                // P5-P95 confidence band as a polygon
                const bandPoints: string[] = [];
                for (let i = 0; i < steps; i++) {
                  const v = sp.p95_band[i];
                  if (v !== null) bandPoints.push(`${toX(i).toFixed(1)},${toY(v).toFixed(1)}`);
                }
                for (let i = steps - 1; i >= 0; i--) {
                  const v = sp.p5_band[i];
                  if (v !== null) bandPoints.push(`${toX(i).toFixed(1)},${toY(v).toFixed(1)}`);
                }

                // Y-axis labels
                const yTicks = 5;
                const yLabels: { y: number; label: string }[] = [];
                for (let i = 0; i <= yTicks; i++) {
                  const val = globalMin + (range * i) / yTicks;
                  yLabels.push({ y: toY(val), label: `${(val * 100).toFixed(0)}%` });
                }

                return (
                  <div style={{ marginBottom: SPACING.XLARGE }}>
                    <div style={{
                      color: FINCEPT.ORANGE,
                      fontSize: '10px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      marginBottom: SPACING.MEDIUM,
                    }}>
                      SIMULATION PATHS ({sp.num_paths} of 1,000)
                    </div>
                    <div style={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: BORDERS.STANDARD,
                      borderRadius: '2px',
                      padding: SPACING.MEDIUM,
                      overflow: 'hidden',
                    }}>
                      <svg
                        width="100%"
                        viewBox={`0 0 ${chartW} ${chartH}`}
                        preserveAspectRatio="xMidYMid meet"
                        style={{ display: 'block' }}
                      >
                        {/* Grid lines */}
                        {yLabels.map((tick, i) => (
                          <g key={i}>
                            <line
                              x1={padL} y1={tick.y} x2={chartW - padR} y2={tick.y}
                              stroke={FINCEPT.BORDER} strokeWidth="0.5" strokeDasharray="3,3"
                            />
                            <text x={padL - 4} y={tick.y + 3} textAnchor="end"
                              fill={FINCEPT.MUTED} fontSize="8" fontFamily={TYPOGRAPHY.MONO}>
                              {tick.label}
                            </text>
                          </g>
                        ))}

                        {/* Zero line */}
                        {globalMin < 0 && globalMax > 0 && (
                          <line
                            x1={padL} y1={toY(0)} x2={chartW - padR} y2={toY(0)}
                            stroke={FINCEPT.GRAY} strokeWidth="0.5"
                          />
                        )}

                        {/* P5-P95 confidence band */}
                        {bandPoints.length > 2 && (
                          <polygon
                            points={bandPoints.join(' ')}
                            fill={FINCEPT.ORANGE}
                            opacity="0.08"
                          />
                        )}

                        {/* Individual simulation paths */}
                        {sp.paths.map((path, idx) => (
                          <polyline
                            key={idx}
                            points={pathToPolyline(path)}
                            fill="none"
                            stroke={FINCEPT.CYAN}
                            strokeWidth="0.4"
                            opacity="0.15"
                          />
                        ))}

                        {/* P5 band line */}
                        <polyline
                          points={pathToPolyline(sp.p5_band)}
                          fill="none"
                          stroke={FINCEPT.RED}
                          strokeWidth="1.2"
                          strokeDasharray="4,2"
                          opacity="0.8"
                        />

                        {/* Median (P50) line */}
                        <polyline
                          points={pathToPolyline(sp.p50_band)}
                          fill="none"
                          stroke={FINCEPT.ORANGE}
                          strokeWidth="1.5"
                          opacity="0.9"
                        />

                        {/* P95 band line */}
                        <polyline
                          points={pathToPolyline(sp.p95_band)}
                          fill="none"
                          stroke={FINCEPT.GREEN}
                          strokeWidth="1.2"
                          strokeDasharray="4,2"
                          opacity="0.8"
                        />

                        {/* Original (actual) path */}
                        {sp.original_path && (
                          <polyline
                            points={pathToPolyline(sp.original_path)}
                            fill="none"
                            stroke={FINCEPT.YELLOW}
                            strokeWidth="2"
                            opacity="1"
                          />
                        )}

                        {/* X-axis labels */}
                        <text x={padL} y={chartH - 3} fill={FINCEPT.MUTED} fontSize="8" fontFamily={TYPOGRAPHY.MONO}>
                          Day 0
                        </text>
                        <text x={chartW - padR} y={chartH - 3} fill={FINCEPT.MUTED} fontSize="8"
                          fontFamily={TYPOGRAPHY.MONO} textAnchor="end">
                          Day {steps - 1}
                        </text>
                      </svg>

                      {/* Legend */}
                      <div style={{ display: 'flex', gap: SPACING.LARGE, marginTop: SPACING.SMALL, flexWrap: 'wrap' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                          <div style={{ width: '14px', height: '2px', backgroundColor: FINCEPT.CYAN, opacity: 0.5 }} />
                          <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>Sim Paths</span>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                          <div style={{ width: '14px', height: '2px', backgroundColor: FINCEPT.ORANGE }} />
                          <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>Median (P50)</span>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                          <div style={{ width: '14px', height: '1px', backgroundColor: FINCEPT.GREEN, borderTop: '1px dashed' }} />
                          <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>P95</span>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                          <div style={{ width: '14px', height: '1px', backgroundColor: FINCEPT.RED, borderTop: '1px dashed' }} />
                          <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>P5</span>
                        </div>
                        {sp.original_path && (
                          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                            <div style={{ width: '14px', height: '2px', backgroundColor: FINCEPT.YELLOW }} />
                            <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>Actual</span>
                          </div>
                        )}
                        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                          <div style={{ width: '14px', height: '8px', backgroundColor: FINCEPT.ORANGE, opacity: 0.15, borderRadius: '1px' }} />
                          <span style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>5th-95th Band</span>
                        </div>
                      </div>
                    </div>
                  </div>
                );
              })()}

              {/* Distribution cards */}
              {([
                { label: 'SHARPE RATIO', dist: report.montecarlo.sharpe_distribution, color: FINCEPT.CYAN, isPct: false },
                { label: 'CAGR', dist: report.montecarlo.cagr_distribution, color: FINCEPT.GREEN, isPct: true },
                { label: 'MAX DRAWDOWN', dist: report.montecarlo.max_drawdown_distribution, color: FINCEPT.RED, isPct: true },
                { label: 'TERMINAL WEALTH', dist: report.montecarlo.wealth_distribution, color: FINCEPT.YELLOW, isPct: false },
              ] as { label: string; dist: MonteCarloDistribution | null; color: string; isPct: boolean }[]).map(({ label, dist, color, isPct }) =>
                dist ? (
                  <div key={label} style={{ marginBottom: SPACING.XLARGE }}>
                    <div style={{
                      color,
                      fontSize: '10px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      marginBottom: SPACING.MEDIUM,
                    }}>
                      {label} DISTRIBUTION {dist.count ? `(${dist.count} sims)` : ''}
                    </div>
                    <div style={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${color}30`,
                      borderRadius: '2px',
                      padding: SPACING.DEFAULT,
                    }}>
                      {/* Distribution bar visualization */}
                      <div style={{
                        display: 'flex',
                        alignItems: 'flex-end',
                        height: '80px',
                        gap: '3px',
                        marginBottom: SPACING.MEDIUM,
                        padding: '0 10px',
                      }}>
                        {([
                          { key: 'min' as const, h: 10 },
                          { key: 'p5' as const, h: 25 },
                          { key: 'p25' as const, h: 45 },
                          { key: 'p50' as const, h: 80 },
                          { key: 'p75' as const, h: 45 },
                          { key: 'p95' as const, h: 25 },
                          { key: 'max' as const, h: 10 },
                        ]).filter(({ key }) => dist[key] !== null && dist[key] !== undefined).map(({ key, h }) => {
                          const val = dist[key];
                          const isMedian = key === 'p50';
                          const displayLabel = key === 'min' ? 'MIN' : key === 'max' ? 'MAX' : key.toUpperCase();
                          return (
                            <div key={key} style={{
                              flex: 1,
                              display: 'flex',
                              flexDirection: 'column',
                              alignItems: 'center',
                              gap: '2px',
                            }}>
                              <div style={{
                                width: '100%',
                                height: `${h}px`,
                                backgroundColor: `${color}${isMedian ? '60' : '25'}`,
                                borderRadius: '2px 2px 0 0',
                                border: isMedian ? `1px solid ${color}` : 'none',
                              }} />
                              <div style={{ color: isMedian ? color : FINCEPT.GRAY, fontSize: '8px', fontWeight: isMedian ? 700 : 400 }}>
                                {isPct ? fmt(val, 1, '%') : fmtRaw(val, 2)}
                              </div>
                              <div style={{ color: FINCEPT.MUTED, fontSize: '7px' }}>
                                {displayLabel}
                              </div>
                            </div>
                          );
                        })}
                      </div>

                      {/* Summary stats grid */}
                      <div style={{
                        display: 'grid',
                        gridTemplateColumns: 'repeat(4, 1fr)',
                        gap: SPACING.MEDIUM,
                        padding: SPACING.MEDIUM,
                        backgroundColor: FINCEPT.HEADER_BG,
                        borderRadius: '2px',
                      }}>
                        <div>
                          <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>MEAN</div>
                          <div style={{ color, fontSize: '12px', fontWeight: 700 }}>
                            {isPct ? fmt(dist.mean, 2, '%') : fmtRaw(dist.mean, 3)}
                          </div>
                        </div>
                        <div>
                          <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>STD DEV</div>
                          <div style={{ color: FINCEPT.GRAY, fontSize: '12px', fontWeight: 700 }}>
                            {isPct ? fmt(dist.std, 2, '%') : fmtRaw(dist.std, 3)}
                          </div>
                        </div>
                        <div>
                          <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>MIN</div>
                          <div style={{ color: FINCEPT.RED, fontSize: '12px', fontWeight: 700 }}>
                            {isPct ? fmt(dist.min, 2, '%') : fmtRaw(dist.min, 3)}
                          </div>
                        </div>
                        <div>
                          <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>MAX</div>
                          <div style={{ color: FINCEPT.GREEN, fontSize: '12px', fontWeight: 700 }}>
                            {isPct ? fmt(dist.max, 2, '%') : fmtRaw(dist.max, 3)}
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>
                ) : null
              )}

              {!report.montecarlo.sharpe_distribution && !report.montecarlo.cagr_distribution && !report.montecarlo.max_drawdown_distribution && (
                <div style={{ color: FINCEPT.GRAY, fontSize: '10px', textAlign: 'center', padding: '40px' }}>
                  Monte Carlo simulation data not available. Ensure sufficient return data.
                </div>
              )}
            </div>
          )}

          </>
        )}
      </div>
    </div>
  );
};

export default QuantStatsView;
