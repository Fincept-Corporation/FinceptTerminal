import React, { useState, useRef, useEffect } from 'react';
import { createChart, LineSeries } from 'lightweight-charts';
import { FINCEPT } from '../../portfolio-tab/finceptStyles';
import { formatNumber, formatLargeNumber, getYearFromPeriod } from '../utils/formatters';
import type { FinancialsData } from '../types';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  BLUE: FINCEPT.BLUE,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  BORDER: FINCEPT.BORDER,
  MAGENTA: '#FF00FF',
};

interface FinancialsTabProps {
  financials: FinancialsData | null;
  loading: boolean;
}

export const FinancialsTab: React.FC<FinancialsTabProps> = ({
  financials,
  loading,
}) => {
  const [fontSize, setFontSize] = useState(12);
  const [showYearsCount, setShowYearsCount] = useState(4);
  const [selectedMetrics, setSelectedMetrics] = useState<string[]>(['Total Revenue', 'Gross Profit', 'Operating Income', 'Net Income', 'EBITDA', 'Basic EPS', 'Diluted EPS']);
  const [chart1Metrics, setChart1Metrics] = useState<string[]>(['Total Revenue', 'Gross Profit']);
  const [chart2Metrics, setChart2Metrics] = useState<string[]>(['Net Income', 'Operating Income']);
  const [chart3Metrics, setChart3Metrics] = useState<string[]>(['EBITDA']);

  const financialChart1Ref = useRef<HTMLDivElement>(null);
  const financialChart2Ref = useRef<HTMLDivElement>(null);
  const financialChart3Ref = useRef<HTMLDivElement>(null);
  const financialChart1InstanceRef = useRef<any>(null);
  const financialChart2InstanceRef = useRef<any>(null);
  const financialChart3InstanceRef = useRef<any>(null);

  // Get all available metrics from income statement
  const getAllMetrics = (): string[] => {
    if (!financials || Object.keys(financials.income_statement).length === 0) return [];
    const firstPeriod = Object.keys(financials.income_statement)[0];
    return Object.keys(financials.income_statement[firstPeriod]);
  };

  const toggleMetric = (metric: string) => {
    setSelectedMetrics(prev =>
      prev.includes(metric)
        ? prev.filter(m => m !== metric)
        : [...prev, metric]
    );
  };

  // Helper function to create a financial chart with dynamic metrics
  const createFinancialChart = (
    container: HTMLDivElement | null,
    metrics: string[],
    chartColors: string[]
  ) => {
    if (!container || !financials) return null;

    const chart = createChart(container, {
      layout: {
        background: { color: COLORS.DARK_BG },
        textColor: COLORS.WHITE,
      },
      grid: {
        vertLines: { color: COLORS.BORDER },
        horzLines: { color: COLORS.BORDER },
      },
      width: container.clientWidth,
      height: 250,
      timeScale: {
        borderColor: COLORS.GRAY,
        timeVisible: true,
      },
      rightPriceScale: {
        borderColor: COLORS.GRAY,
      },
    });

    const periods = Object.keys(financials.income_statement);

    metrics.forEach((metric, idx) => {
      const color = chartColors[idx % chartColors.length];
      const series = chart.addSeries(LineSeries, {
        color,
        lineWidth: 2,
        title: metric,
      });

      const data: any[] = [];
      periods.forEach((period) => {
        const timestamp = new Date(period).getTime() / 1000;
        const statements = financials.income_statement[period];

        if (statements[metric] !== undefined && statements[metric] !== null) {
          const value = metric.includes('EPS')
            ? statements[metric]
            : statements[metric] / 1e9; // Convert to billions
          data.push({ time: timestamp, value });
        }
      });

      if (data.length > 0) {
        series.setData(data.sort((a, b) => a.time - b.time));
      }
    });

    chart.timeScale().fitContent();
    return chart;
  };

  // Update financial charts when data or metrics change
  useEffect(() => {
    if (financials) {
      // Cleanup existing charts
      [financialChart1InstanceRef, financialChart2InstanceRef, financialChart3InstanceRef].forEach(ref => {
        if (ref.current) {
          try {
            ref.current.remove();
          } catch (e) {
            // Chart already disposed
          }
          ref.current = null;
        }
      });

      const chartColors = [COLORS.GREEN, COLORS.CYAN, COLORS.ORANGE, COLORS.YELLOW, COLORS.MAGENTA, COLORS.BLUE];

      if (financialChart1Ref.current && chart1Metrics.length > 0) {
        financialChart1InstanceRef.current = createFinancialChart(financialChart1Ref.current, chart1Metrics, chartColors);
      }

      if (financialChart2Ref.current && chart2Metrics.length > 0) {
        financialChart2InstanceRef.current = createFinancialChart(financialChart2Ref.current, chart2Metrics, chartColors);
      }

      if (financialChart3Ref.current && chart3Metrics.length > 0) {
        financialChart3InstanceRef.current = createFinancialChart(financialChart3Ref.current, chart3Metrics, chartColors);
      }

      // Handle resize
      const handleResize = () => {
        if (financialChart1Ref.current && financialChart1InstanceRef.current) {
          financialChart1InstanceRef.current.applyOptions({ width: financialChart1Ref.current.clientWidth });
        }
        if (financialChart2Ref.current && financialChart2InstanceRef.current) {
          financialChart2InstanceRef.current.applyOptions({ width: financialChart2Ref.current.clientWidth });
        }
        if (financialChart3Ref.current && financialChart3InstanceRef.current) {
          financialChart3InstanceRef.current.applyOptions({ width: financialChart3Ref.current.clientWidth });
        }
      };

      window.addEventListener('resize', handleResize);
      return () => {
        window.removeEventListener('resize', handleResize);
        [financialChart1InstanceRef, financialChart2InstanceRef, financialChart3InstanceRef].forEach(ref => {
          if (ref.current) {
            try {
              ref.current.remove();
            } catch (e) {
              // Chart already disposed
            }
            ref.current = null;
          }
        });
      };
    }
  }, [financials, chart1Metrics, chart2Metrics, chart3Metrics]);

  if (!financials) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '400px',
        gap: '16px',
      }}>
        {loading && (
          <div style={{
            width: '50px',
            height: '50px',
            border: '4px solid #404040',
            borderTop: '4px solid #ea580c',
            borderRadius: '50%',
            animation: 'spin 1s linear infinite'
          }} />
        )}
        <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
          {loading ? 'LOADING FINANCIAL DATA...' : '[WARN] NO FINANCIAL DATA AVAILABLE'}
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
          {loading ? 'Please wait while we fetch the data' : 'Financial statements are not available for this symbol'}
        </div>
      </div>
    );
  }

  const allMetrics = getAllMetrics();

  return (
    <div id="research-financials" style={{ padding: '8px' }}>
      {/* Controls Panel */}
      <div id="financials-controls" style={{
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '10px 12px',
        marginBottom: '8px',
        display: 'flex',
        gap: '20px',
        alignItems: 'center',
        flexWrap: 'wrap',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>FONT SIZE:</span>
          <button
            onClick={() => setFontSize(Math.max(10, fontSize - 1))}
            style={{
              backgroundColor: COLORS.DARK_BG,
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.WHITE,
              padding: '4px 8px',
              fontSize: `${fontSize}px`,
              cursor: 'pointer',
            }}
          >
            -
          </button>
          <span style={{ color: COLORS.ORANGE, fontSize: `${fontSize}px`, fontWeight: 'bold', minWidth: '30px', textAlign: 'center' }}>{fontSize}px</span>
          <button
            onClick={() => setFontSize(Math.min(20, fontSize + 1))}
            style={{
              backgroundColor: COLORS.DARK_BG,
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.WHITE,
              padding: '4px 8px',
              fontSize: `${fontSize}px`,
              cursor: 'pointer',
            }}
          >
            +
          </button>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>SHOW YEARS:</span>
          {[2, 3, 4, 5].map((count) => (
            <button
              key={count}
              onClick={() => setShowYearsCount(count)}
              style={{
                backgroundColor: showYearsCount === count ? COLORS.ORANGE : COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: showYearsCount === count ? COLORS.DARK_BG : COLORS.WHITE,
                padding: '4px 10px',
                fontSize: `${fontSize - 1}px`,
                cursor: 'pointer',
                fontWeight: showYearsCount === count ? 'bold' : 'normal',
              }}
            >
              {count}
            </button>
          ))}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>SELECT METRICS:</span>
          <button
            onClick={() => {
              setSelectedMetrics(selectedMetrics.length === allMetrics.length ? [] : allMetrics);
            }}
            style={{
              backgroundColor: COLORS.BLUE,
              border: 'none',
              color: COLORS.WHITE,
              padding: '4px 10px',
              fontSize: `${fontSize - 1}px`,
              cursor: 'pointer',
              fontWeight: 'bold',
            }}
          >
            {selectedMetrics.length === allMetrics.length ? 'DESELECT ALL' : 'SELECT ALL'}
          </button>
        </div>
      </div>

      {/* Metric Selector */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '12px',
        marginBottom: '8px',
      }}>
        <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '10px' }}>
          SELECT METRICS TO COMPARE
        </div>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
          {allMetrics.map((metric) => (
            <button
              key={metric}
              onClick={() => toggleMetric(metric)}
              style={{
                backgroundColor: selectedMetrics.includes(metric) ? COLORS.GREEN : COLORS.DARK_BG,
                border: `1px solid ${selectedMetrics.includes(metric) ? COLORS.GREEN : COLORS.BORDER}`,
                color: selectedMetrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                padding: '6px 12px',
                fontSize: `${fontSize - 1}px`,
                cursor: 'pointer',
                fontWeight: selectedMetrics.includes(metric) ? 'bold' : 'normal',
                borderRadius: '3px',
              }}
            >
              {metric}
            </button>
          ))}
        </div>
      </div>

      {/* Financial Comparison Charts - 3 Charts in a Row */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '12px',
        marginBottom: '8px',
      }}>
        <div style={{ color: COLORS.ORANGE, fontSize: `${fontSize + 3}px`, fontWeight: 'bold', marginBottom: '12px' }}>
          FINANCIAL TRENDS COMPARISON (in Billions $)
        </div>

        {/* Chart Controls */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px', marginBottom: '12px' }}>
          {[
            { label: 'CHART 1 METRICS:', metrics: chart1Metrics, setMetrics: setChart1Metrics, color: COLORS.GREEN },
            { label: 'CHART 2 METRICS:', metrics: chart2Metrics, setMetrics: setChart2Metrics, color: COLORS.ORANGE },
            { label: 'CHART 3 METRICS:', metrics: chart3Metrics, setMetrics: setChart3Metrics, color: COLORS.MAGENTA },
          ].map(({ label, metrics, setMetrics, color }) => (
            <div key={label}>
              <div style={{ color: COLORS.CYAN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px' }}>
                {label}
              </div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                {allMetrics.slice(0, 8).map((metric) => (
                  <button
                    key={metric}
                    onClick={() => {
                      setMetrics(prev =>
                        prev.includes(metric)
                          ? prev.filter(m => m !== metric)
                          : [...prev, metric]
                      );
                    }}
                    style={{
                      backgroundColor: metrics.includes(metric) ? color : COLORS.DARK_BG,
                      border: `1px solid ${metrics.includes(metric) ? color : COLORS.BORDER}`,
                      color: metrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                      padding: '3px 6px',
                      fontSize: `${fontSize - 3}px`,
                      cursor: 'pointer',
                      fontWeight: metrics.includes(metric) ? 'bold' : 'normal',
                    }}
                  >
                    {metric.substring(0, 15)}
                  </button>
                ))}
              </div>
            </div>
          ))}
        </div>

        {/* 3 Charts Side by Side */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
          <div>
            <div style={{ color: COLORS.GREEN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
              {chart1Metrics.join(', ') || 'Select metrics'}
            </div>
            <div ref={financialChart1Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
          </div>
          <div>
            <div style={{ color: COLORS.ORANGE, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
              {chart2Metrics.join(', ') || 'Select metrics'}
            </div>
            <div ref={financialChart2Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
          </div>
          <div>
            <div style={{ color: COLORS.MAGENTA, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
              {chart3Metrics.join(', ') || 'Select metrics'}
            </div>
            <div ref={financialChart3Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
          </div>
        </div>
      </div>

      {/* Financial Comparison Table */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '12px',
        marginBottom: '8px',
      }}>
        <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 3}px`, fontWeight: 'bold', marginBottom: '12px' }}>
          YEAR-OVER-YEAR COMPARISON ({selectedMetrics.length} Metrics)
        </div>

        {selectedMetrics.length === 0 ? (
          <div style={{ color: COLORS.YELLOW, fontSize: `${fontSize + 1}px`, textAlign: 'center', padding: '20px' }}>
            Please select at least one metric to compare
          </div>
        ) : (
          <div style={{ overflowX: 'auto' }} className="custom-scrollbar">
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: `${fontSize}px` }}>
              <thead>
                <tr style={{ borderBottom: `2px solid ${COLORS.BORDER}` }}>
                  <th style={{ textAlign: 'left', padding: '10px', color: COLORS.GRAY, fontSize: `${fontSize + 1}px` }}>METRIC</th>
                  {Object.keys(financials.income_statement).slice(0, showYearsCount).map((period) => (
                    <th key={period} style={{ textAlign: 'right', padding: '10px', color: COLORS.ORANGE, fontSize: `${fontSize + 1}px` }}>
                      {getYearFromPeriod(period)}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {selectedMetrics.map((metric) => {
                  const periods = Object.keys(financials.income_statement).slice(0, showYearsCount);
                  return (
                    <tr key={metric} style={{ borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                      <td style={{ padding: '10px', color: COLORS.WHITE, fontWeight: 'bold', fontSize: `${fontSize}px` }}>{metric}</td>
                      {periods.map((period) => {
                        const value = financials.income_statement[period][metric];
                        return (
                          <td key={period} style={{ textAlign: 'right', padding: '10px', color: COLORS.CYAN, fontSize: `${fontSize}px` }}>
                            {value ? (metric.includes('EPS') ? `$${formatNumber(value)}` : formatLargeNumber(value)) : 'N/A'}
                          </td>
                        );
                      })}
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Detailed Financial Statements */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
        {/* Income Statement */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          padding: '8px',
        }}>
          <div style={{ color: COLORS.GREEN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
            INCOME STATEMENT (Latest)
          </div>
          <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
            {Object.keys(financials.income_statement).length > 0 ? (
              Object.entries(Object.values(financials.income_statement)[0] || {}).map(([metric, _], idx) => {
                const latestPeriod = Object.keys(financials.income_statement)[0];
                const value = financials.income_statement[latestPeriod][metric];
                return (
                  <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                    <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                    <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                  </div>
                );
              })
            ) : (
              <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
            )}
          </div>
        </div>

        {/* Balance Sheet */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          padding: '8px',
        }}>
          <div style={{ color: COLORS.YELLOW, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
            BALANCE SHEET (Latest)
          </div>
          <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
            {Object.keys(financials.balance_sheet).length > 0 ? (
              Object.entries(Object.values(financials.balance_sheet)[0] || {}).map(([metric, _], idx) => {
                const latestPeriod = Object.keys(financials.balance_sheet)[0];
                const value = financials.balance_sheet[latestPeriod][metric];
                return (
                  <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                    <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                    <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                  </div>
                );
              })
            ) : (
              <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
            )}
          </div>
        </div>

        {/* Cash Flow */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          padding: '8px',
        }}>
          <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
            CASH FLOW STATEMENT (Latest)
          </div>
          <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
            {Object.keys(financials.cash_flow).length > 0 ? (
              Object.entries(Object.values(financials.cash_flow)[0] || {}).map(([metric, _], idx) => {
                const latestPeriod = Object.keys(financials.cash_flow)[0];
                const value = financials.cash_flow[latestPeriod][metric];
                return (
                  <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                    <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                    <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                  </div>
                );
              })
            ) : (
              <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default FinancialsTab;
