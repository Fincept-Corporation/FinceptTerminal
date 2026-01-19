import React, { useState, useEffect } from 'react';
import { PortfolioSummary, portfolioService, PortfolioSnapshot } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent } from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../finceptStyles';

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
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        padding: SPACING.XLARGE,
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.SUBHEADING, marginBottom: SPACING.SMALL }}>
          Loading performance data...
        </div>
        <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>
          Fetching portfolio snapshots from database
        </div>
      </div>
    );
  }

  if (performanceData.length === 0) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        padding: SPACING.XLARGE,
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SUBHEADING, marginBottom: SPACING.SMALL }}>
          No performance data available
        </div>
        <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>
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
  const valueRange = maxValue - minValue || 1; // Prevent division by zero

  const xScale = (index: number) => {
    // Handle single data point case
    if (performanceData.length === 1) return chartWidth / 2;
    return (index / (performanceData.length - 1)) * innerWidth + padding.left;
  };

  const yScale = (value: number) => {
    // Handle single data point or zero range case
    if (valueRange === 0 || valueRange === 1) return chartHeight / 2;
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
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      padding: SPACING.DEFAULT,
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.LARGE
      }}>
        PERFORMANCE CHART ({performanceData.length} DAY{performanceData.length > 1 ? 'S' : ''})
      </div>

      {/* Performance Metrics Bar */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(5, 1fr)',
        gap: SPACING.DEFAULT,
        marginBottom: SPACING.LARGE
      }}>
        <div style={{
          ...COMMON_STYLES.metricCard,
          border: periodReturn >= 0 ? BORDERS.GREEN : BORDERS.RED
        }}>
          <div style={COMMON_STYLES.dataLabel}>PERIOD RETURN</div>
          <div style={{
            color: periodReturn >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatPercent(periodReturn)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: periodGain >= 0 ? BORDERS.GREEN : BORDERS.RED
        }}>
          <div style={COMMON_STYLES.dataLabel}>PERIOD GAIN</div>
          <div style={{
            color: periodGain >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatCurrency(periodGain, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: `1px solid ${FINCEPT.YELLOW}`
        }}>
          <div style={COMMON_STYLES.dataLabel}>CURRENT VALUE</div>
          <div style={{
            color: FINCEPT.YELLOW,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatCurrency(latestValue, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.CYAN
        }}>
          <div style={COMMON_STYLES.dataLabel}>START VALUE</div>
          <div style={{
            color: FINCEPT.CYAN,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatCurrency(earliestValue, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.ORANGE
        }}>
          <div style={COMMON_STYLES.dataLabel}>VOLATILITY</div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {((valueRange / earliestValue) * 100).toFixed(2)}%
          </div>
        </div>
      </div>

      {/* Chart */}
      <div style={{
        backgroundColor: 'rgba(0,0,0,0.3)',
        border: BORDERS.STANDARD,
        padding: SPACING.DEFAULT,
        marginBottom: SPACING.LARGE
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
                  stroke={FINCEPT.BORDER}
                  strokeWidth="0.5"
                  strokeDasharray="4,4"
                  opacity="0.3"
                />
                <text
                  x={padding.left - 10}
                  y={y + 4}
                  textAnchor="end"
                  fill={FINCEPT.GRAY}
                  fontSize="9"
                  fontFamily={TYPOGRAPHY.MONO}
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
                fill={FINCEPT.GRAY}
                fontSize="8"
                fontFamily={TYPOGRAPHY.MONO}
              >
                {d.date.substring(5)}
              </text>
            );
          })}

          {/* Area fill */}
          <path
            d={areaPath}
            fill={periodReturn >= 0 ? FINCEPT.GREEN : FINCEPT.RED}
            opacity="0.1"
          />

          {/* Line */}
          <path
            d={linePath}
            fill="none"
            stroke={periodReturn >= 0 ? FINCEPT.GREEN : FINCEPT.RED}
            strokeWidth="2"
          />

          {/* Data points */}
          {performanceData.map((d, i) => (
            <circle
              key={i}
              cx={xScale(i)}
              cy={yScale(d.value)}
              r="3"
              fill={d.changePercent >= 0 ? FINCEPT.GREEN : FINCEPT.RED}
              stroke={FINCEPT.DARK_BG}
              strokeWidth="1"
            />
          ))}

          {/* Cost basis line */}
          <line
            x1={padding.left}
            y1={yScale(portfolioSummary.total_cost_basis)}
            x2={chartWidth - padding.right}
            y2={yScale(portfolioSummary.total_cost_basis)}
            stroke={FINCEPT.YELLOW}
            strokeWidth="2"
            strokeDasharray="8,4"
          />
          <text
            x={chartWidth - padding.right - 10}
            y={yScale(portfolioSummary.total_cost_basis) - 8}
            textAnchor="end"
            fill={FINCEPT.YELLOW}
            fontSize="10"
            fontWeight="bold"
            fontFamily={TYPOGRAPHY.MONO}
          >
            COST BASIS
          </text>

          {/* Chart title */}
          <text
            x={chartWidth / 2}
            y={15}
            textAnchor="middle"
            fill={FINCEPT.ORANGE}
            fontSize="12"
            fontWeight="bold"
            fontFamily={TYPOGRAPHY.MONO}
          >
            PORTFOLIO VALUE OVER TIME
          </text>
        </svg>
      </div>

      {/* Performance Table */}
      <div style={{
        color: FINCEPT.ORANGE,
        fontSize: TYPOGRAPHY.DEFAULT,
        fontWeight: TYPOGRAPHY.BOLD,
        marginBottom: SPACING.SMALL,
        paddingBottom: SPACING.SMALL,
        borderBottom: BORDERS.ORANGE
      }}>
        DAILY PERFORMANCE DATA
      </div>

      <div style={{ maxHeight: '300px', overflow: 'auto' }}>
        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1.2fr 1fr 1fr',
          gap: SPACING.SMALL,
          padding: SPACING.SMALL,
          backgroundColor: FINCEPT.HEADER_BG,
          fontSize: TYPOGRAPHY.BODY,
          fontWeight: TYPOGRAPHY.BOLD,
          position: 'sticky',
          top: 0,
          zIndex: 1,
          borderBottom: BORDERS.ORANGE
        }}>
          <div style={{ color: FINCEPT.ORANGE }}>DATE</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>VALUE</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>CHANGE</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>CHANGE %</div>
        </div>

        {/* Table Rows */}
        {performanceData.slice().reverse().map((d, index) => (
          <div
            key={d.date}
            style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1.2fr 1fr 1fr',
              gap: SPACING.SMALL,
              padding: SPACING.SMALL,
              backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
              borderLeft: `3px solid ${d.changePercent >= 0 ? FINCEPT.GREEN : FINCEPT.RED}`,
              fontSize: TYPOGRAPHY.BODY,
              marginBottom: '2px'
            }}
          >
            <div style={{ color: FINCEPT.GRAY }}>{d.date}</div>
            <div style={{ color: FINCEPT.WHITE, textAlign: 'right', fontWeight: TYPOGRAPHY.BOLD }}>
              {formatCurrency(d.value, currency)}
            </div>
            <div style={{
              color: d.change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
              textAlign: 'right',
              fontWeight: TYPOGRAPHY.BOLD
            }}>
              {formatCurrency(d.change, currency)}
            </div>
            <div style={{
              color: d.changePercent >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
              textAlign: 'right',
              fontWeight: TYPOGRAPHY.BOLD
            }}>
              {formatPercent(d.changePercent)}
            </div>
          </div>
        ))}
      </div>

      {/* Info Note */}
      {performanceData.length < 7 && (
        <div style={{
          marginTop: SPACING.DEFAULT,
          padding: SPACING.MEDIUM,
          backgroundColor: 'rgba(255,136,0,0.05)',
          border: BORDERS.ORANGE,
          fontSize: TYPOGRAPHY.SMALL,
          color: FINCEPT.GRAY
        }}>
          <strong style={{ color: FINCEPT.ORANGE }}>TIP:</strong> Portfolio snapshots are created automatically.
          More historical data will be available as you continue to use the application.
          Current data points: {performanceData.length}
        </div>
      )}
    </div>
  );
};

export default PerformanceView;
