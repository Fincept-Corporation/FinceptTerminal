import React, { useState, useEffect } from 'react';
import { PortfolioSummary, portfolioService, PortfolioSnapshot } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency, formatPercent } from './utils';

interface PerformanceViewProps {
  portfolioSummary: PortfolioSummary;
}

interface PerformanceDataPoint {
  date: string;
  value: number;
  change: number;
  changePercent: number;
}

const PerformanceView: React.FC<PerformanceViewProps> = ({ portfolioSummary }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW } = BLOOMBERG_COLORS;
  const currency = portfolioSummary.portfolio.currency;
  const [performanceData, setPerformanceData] = useState<PerformanceDataPoint[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const loadPerformanceData = async () => {
      try {
        // Fetch portfolio snapshots from database
        const snapshots = await portfolioService.getPortfolioPerformanceHistory(
          portfolioSummary.portfolio.id,
          30
        );

        if (snapshots.length === 0) {
          // No historical data, use current value only
          setPerformanceData([{
            date: new Date().toISOString().split('T')[0],
            value: portfolioSummary.total_market_value,
            change: portfolioSummary.total_unrealized_pnl,
            changePercent: portfolioSummary.total_unrealized_pnl_percent
          }]);
        } else {
          // Convert snapshots to performance data points
          const data = snapshots
            .reverse() // Oldest first
            .map(snapshot => ({
              date: snapshot.snapshot_date.split('T')[0],
              value: snapshot.total_value,
              change: snapshot.total_pnl,
              changePercent: snapshot.total_pnl_percent
            }));

          // Add current value as latest data point if not already present
          const latestDate = new Date().toISOString().split('T')[0];
          const hasToday = data.some(d => d.date === latestDate);

          if (!hasToday) {
            data.push({
              date: latestDate,
              value: portfolioSummary.total_market_value,
              change: portfolioSummary.total_unrealized_pnl,
              changePercent: portfolioSummary.total_unrealized_pnl_percent
            });
          }

          setPerformanceData(data);
        }
      } catch (error) {
        console.error('Failed to load performance data:', error);
        // Fallback to current value
        setPerformanceData([{
          date: new Date().toISOString().split('T')[0],
          value: portfolioSummary.total_market_value,
          change: portfolioSummary.total_unrealized_pnl,
          changePercent: portfolioSummary.total_unrealized_pnl_percent
        }]);
      } finally {
        setLoading(false);
      }
    };

    loadPerformanceData();
  }, [portfolioSummary.portfolio.id, portfolioSummary.total_market_value]);

  if (loading) {
    return (
      <div style={{
        padding: '40px',
        textAlign: 'center',
        backgroundColor: 'rgba(255,255,255,0.02)',
        border: `1px solid ${GRAY}`,
        borderRadius: '4px'
      }}>
        <div style={{ color: ORANGE, fontSize: '14px', marginBottom: '8px' }}>
          Loading performance data...
        </div>
        <div style={{ color: GRAY, fontSize: '10px' }}>
          Fetching portfolio snapshots from database
        </div>
      </div>
    );
  }

  if (performanceData.length === 0) {
    return (
      <div style={{
        padding: '40px',
        textAlign: 'center',
        backgroundColor: 'rgba(255,255,255,0.02)',
        border: `1px solid ${GRAY}`,
        borderRadius: '4px'
      }}>
        <div style={{ color: GRAY, fontSize: '14px', marginBottom: '8px' }}>
          No performance data available
        </div>
        <div style={{ color: GRAY, fontSize: '10px' }}>
          Portfolio snapshots will be created automatically as you use the application
        </div>
      </div>
    );
  }

  // Calculate chart dimensions
  const chartWidth = 800;
  const chartHeight = 300;
  const padding = { top: 20, right: 40, bottom: 40, left: 60 };
  const innerWidth = chartWidth - padding.left - padding.right;
  const innerHeight = chartHeight - padding.top - padding.bottom;

  // Scale functions
  const maxValue = Math.max(...performanceData.map(d => d.value));
  const minValue = Math.min(...performanceData.map(d => d.value));
  const valueRange = maxValue - minValue;

  const xScale = (index: number) => {
    return (index / (performanceData.length - 1)) * innerWidth + padding.left;
  };

  const yScale = (value: number) => {
    return chartHeight - padding.bottom - ((value - minValue) / valueRange) * innerHeight;
  };

  // Create path for line chart
  const linePath = performanceData
    .map((d, i) => {
      const x = xScale(i);
      const y = yScale(d.value);
      return `${i === 0 ? 'M' : 'L'} ${x} ${y}`;
    })
    .join(' ');

  // Create area path
  const areaPath = linePath +
    ` L ${xScale(performanceData.length - 1)} ${chartHeight - padding.bottom}` +
    ` L ${padding.left} ${chartHeight - padding.bottom} Z`;

  // Performance metrics
  const latestValue = performanceData[performanceData.length - 1].value;
  const earliestValue = performanceData[0].value;
  const periodReturn = ((latestValue - earliestValue) / earliestValue) * 100;
  const periodGain = latestValue - earliestValue;

  return (
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '16px'
      }}>
        PERFORMANCE CHART ({performanceData.length} DAY{performanceData.length > 1 ? 'S' : ''})
      </div>

      {/* Performance Metrics Bar */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(5, 1fr)',
        gap: '12px',
        marginBottom: '20px'
      }}>
        <div style={{
          padding: '10px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `1px solid ${GRAY}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>PERIOD RETURN</div>
          <div style={{
            color: periodReturn >= 0 ? GREEN : RED,
            fontSize: '14px',
            fontWeight: 'bold'
          }}>
            {formatPercent(periodReturn)}
          </div>
        </div>

        <div style={{
          padding: '10px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `1px solid ${GRAY}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>PERIOD GAIN</div>
          <div style={{
            color: periodGain >= 0 ? GREEN : RED,
            fontSize: '14px',
            fontWeight: 'bold'
          }}>
            {formatCurrency(periodGain, currency)}
          </div>
        </div>

        <div style={{
          padding: '10px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `1px solid ${GRAY}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>CURRENT VALUE</div>
          <div style={{ color: YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
            {formatCurrency(latestValue, currency)}
          </div>
        </div>

        <div style={{
          padding: '10px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `1px solid ${GRAY}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>START VALUE</div>
          <div style={{ color: CYAN, fontSize: '14px', fontWeight: 'bold' }}>
            {formatCurrency(earliestValue, currency)}
          </div>
        </div>

        <div style={{
          padding: '10px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `1px solid ${GRAY}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>VOLATILITY</div>
          <div style={{ color: ORANGE, fontSize: '14px', fontWeight: 'bold' }}>
            {((valueRange / earliestValue) * 100).toFixed(2)}%
          </div>
        </div>
      </div>

      {/* Chart */}
      <div style={{
        backgroundColor: 'rgba(0,0,0,0.3)',
        border: `1px solid ${GRAY}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <svg width={chartWidth} height={chartHeight}>
          {/* Grid lines */}
          {[0, 0.25, 0.5, 0.75, 1].map((ratio, i) => {
            const y = chartHeight - padding.bottom - (innerHeight * ratio);
            const value = minValue + (valueRange * ratio);
            return (
              <g key={i}>
                <line
                  x1={padding.left}
                  y1={y}
                  x2={chartWidth - padding.right}
                  y2={y}
                  stroke={GRAY}
                  strokeWidth="0.5"
                  strokeDasharray="4,4"
                  opacity="0.3"
                />
                <text
                  x={padding.left - 10}
                  y={y + 4}
                  textAnchor="end"
                  fill={GRAY}
                  fontSize="9"
                  fontFamily="Consolas, monospace"
                >
                  {formatCurrency(value, currency)}
                </text>
              </g>
            );
          })}

          {/* Date labels */}
          {performanceData.filter((_, i) => i % 5 === 0).map((d, i) => {
            const originalIndex = i * 5;
            const x = xScale(originalIndex);
            return (
              <text
                key={i}
                x={x}
                y={chartHeight - padding.bottom + 20}
                textAnchor="middle"
                fill={GRAY}
                fontSize="8"
                fontFamily="Consolas, monospace"
              >
                {d.date.substring(5)}
              </text>
            );
          })}

          {/* Area fill */}
          <path
            d={areaPath}
            fill={periodReturn >= 0 ? GREEN : RED}
            opacity="0.1"
          />

          {/* Line */}
          <path
            d={linePath}
            fill="none"
            stroke={periodReturn >= 0 ? GREEN : RED}
            strokeWidth="2"
          />

          {/* Data points */}
          {performanceData.map((d, i) => (
            <circle
              key={i}
              cx={xScale(i)}
              cy={yScale(d.value)}
              r="3"
              fill={d.changePercent >= 0 ? GREEN : RED}
              stroke="#000"
              strokeWidth="1"
            />
          ))}

          {/* Cost basis line */}
          <line
            x1={padding.left}
            y1={yScale(portfolioSummary.total_cost_basis)}
            x2={chartWidth - padding.right}
            y2={yScale(portfolioSummary.total_cost_basis)}
            stroke={YELLOW}
            strokeWidth="2"
            strokeDasharray="8,4"
          />
          <text
            x={chartWidth - padding.right - 10}
            y={yScale(portfolioSummary.total_cost_basis) - 8}
            textAnchor="end"
            fill={YELLOW}
            fontSize="10"
            fontWeight="bold"
            fontFamily="Consolas, monospace"
          >
            COST BASIS
          </text>

          {/* Chart title */}
          <text
            x={chartWidth / 2}
            y={15}
            textAnchor="middle"
            fill={ORANGE}
            fontSize="12"
            fontWeight="bold"
            fontFamily="Consolas, monospace"
          >
            PORTFOLIO VALUE OVER TIME
          </text>
        </svg>
      </div>

      {/* Performance Table */}
      <div style={{
        color: ORANGE,
        fontSize: '11px',
        fontWeight: 'bold',
        marginBottom: '8px',
        paddingBottom: '4px',
        borderBottom: `1px solid ${ORANGE}`
      }}>
        DAILY PERFORMANCE DATA
      </div>

      <div style={{ maxHeight: '300px', overflow: 'auto' }}>
        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1.2fr 1fr 1fr',
          gap: '8px',
          padding: '8px',
          backgroundColor: 'rgba(255,165,0,0.1)',
          fontSize: '10px',
          fontWeight: 'bold',
          position: 'sticky',
          top: 0,
          zIndex: 1
        }}>
          <div style={{ color: WHITE }}>DATE</div>
          <div style={{ color: WHITE, textAlign: 'right' }}>VALUE</div>
          <div style={{ color: WHITE, textAlign: 'right' }}>CHANGE</div>
          <div style={{ color: WHITE, textAlign: 'right' }}>CHANGE %</div>
        </div>

        {/* Table Rows */}
        {performanceData.slice().reverse().map((d, index) => (
          <div
            key={d.date}
            style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1.2fr 1fr 1fr',
              gap: '8px',
              padding: '8px',
              backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
              borderLeft: `3px solid ${d.changePercent >= 0 ? GREEN : RED}`,
              fontSize: '10px',
              marginBottom: '2px'
            }}
          >
            <div style={{ color: GRAY }}>{d.date}</div>
            <div style={{ color: WHITE, textAlign: 'right', fontWeight: 'bold' }}>
              {formatCurrency(d.value, currency)}
            </div>
            <div style={{
              color: d.change >= 0 ? GREEN : RED,
              textAlign: 'right',
              fontWeight: 'bold'
            }}>
              {formatCurrency(d.change, currency)}
            </div>
            <div style={{
              color: d.changePercent >= 0 ? GREEN : RED,
              textAlign: 'right',
              fontWeight: 'bold'
            }}>
              {formatPercent(d.changePercent)}
            </div>
          </div>
        ))}
      </div>

      {/* Info Note */}
      {performanceData.length < 7 && (
        <div style={{
          marginTop: '16px',
          padding: '12px',
          backgroundColor: 'rgba(255,165,0,0.05)',
          border: `1px solid ${ORANGE}`,
          borderRadius: '4px',
          fontSize: '9px',
          color: GRAY
        }}>
          <strong style={{ color: ORANGE }}>TIP:</strong> Portfolio snapshots are created automatically.
          More historical data will be available as you continue to use the application.
          Current data points: {performanceData.length}
        </div>
      )}
    </div>
  );
};

export default PerformanceView;
