import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { PortfolioSummary, portfolioService, PortfolioSnapshot } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent } from './utils';

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
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const currency = portfolioSummary.portfolio.currency;
  const [performanceData, setPerformanceData] = useState<PerformanceDataPoint[]>([]);
  const [loading, setLoading] = useState(true);
  const [hoveredRow, setHoveredRow] = useState<number | null>(null);

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
        backgroundColor: colors.background,
        padding: '24px',
        textAlign: 'center',
        fontFamily
      }}>
        <div style={{ color: colors.primary, fontSize: fontSize.tiny, fontWeight: 700, marginBottom: '8px' }}>
          {t('performance.loading')}
        </div>
        <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
          Fetching portfolio snapshots from database
        </div>
      </div>
    );
  }

  if (performanceData.length === 0) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: colors.background,
        padding: '24px',
        textAlign: 'center',
        fontFamily
      }}>
        <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, marginBottom: '8px' }}>
          {t('performance.noData')}
        </div>
        <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
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
      backgroundColor: colors.background,
      padding: '12px',
      overflow: 'auto',
      fontFamily
    }}>
      <div style={{
        padding: '12px',
        backgroundColor: 'var(--ft-color-header, #1A1A1A)',
        borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
        color: colors.primary,
        fontSize: fontSize.body,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '16px',
      }}>
        {t('performance.performanceChart')} ({performanceData.length} DAY{performanceData.length > 1 ? 'S' : ''})
      </div>

      {/* Performance Metrics Bar */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(5, 1fr)',
        gap: '12px',
        marginBottom: '16px'
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderRadius: '2px',
          border: `1px solid ${periodReturn >= 0 ? colors.success : colors.alert}`
        }}>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>{t('performance.totalReturn')}</div>
          <div style={{
            color: periodReturn >= 0 ? colors.success : colors.alert,
            fontSize: fontSize.small,
            fontWeight: 700
          }}>
            {formatPercent(periodReturn)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderRadius: '2px',
          border: `1px solid ${periodGain >= 0 ? colors.success : colors.alert}`
        }}>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>{t('performance.dayChange')}</div>
          <div style={{
            color: periodGain >= 0 ? colors.success : colors.alert,
            fontSize: fontSize.small,
            fontWeight: 700
          }}>
            {formatCurrency(periodGain, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderRadius: '2px',
          border: `1px solid var(--ft-color-warning, #FFD700)`
        }}>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>CURRENT VALUE</div>
          <div style={{
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: fontSize.small,
            fontWeight: 700
          }}>
            {formatCurrency(latestValue, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderRadius: '2px',
          border: `1px solid var(--ft-color-accent, #00E5FF)`
        }}>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>START VALUE</div>
          <div style={{
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: fontSize.small,
            fontWeight: 700
          }}>
            {formatCurrency(earliestValue, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderRadius: '2px',
          border: `1px solid ${colors.primary}`
        }}>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>VOLATILITY</div>
          <div style={{
            color: colors.primary,
            fontSize: fontSize.small,
            fontWeight: 700
          }}>
            {((valueRange / earliestValue) * 100).toFixed(2)}%
          </div>
        </div>
      </div>

      {/* Chart */}
      <div style={{
        backgroundColor: colors.panel,
        border: '1px solid var(--ft-color-border, #2A2A2A)',
        borderRadius: '2px',
        padding: '12px',
        marginBottom: '16px'
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
                  stroke="var(--ft-color-border, #2A2A2A)"
                  strokeWidth="0.5"
                  strokeDasharray="4,4"
                  opacity="0.3"
                />
                <text
                  x={padding.left - 10}
                  y={y + 4}
                  textAnchor="end"
                  fill={colors.textMuted}
                  fontSize="9"
                  fontFamily={fontFamily}
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
                fill={colors.textMuted}
                fontSize="8"
                fontFamily={fontFamily}
              >
                {d.date.substring(5)}
              </text>
            );
          })}

          {/* Area fill */}
          <path
            d={areaPath}
            fill={periodReturn >= 0 ? colors.success : colors.alert}
            opacity="0.1"
          />

          {/* Line */}
          <path
            d={linePath}
            fill="none"
            stroke={periodReturn >= 0 ? colors.success : colors.alert}
            strokeWidth="2"
          />

          {/* Data points */}
          {performanceData.map((d, i) => (
            <circle
              key={i}
              cx={xScale(i)}
              cy={yScale(d.value)}
              r="3"
              fill={d.changePercent >= 0 ? colors.success : colors.alert}
              stroke={colors.background}
              strokeWidth="1"
            />
          ))}

          {/* Cost basis line */}
          <line
            x1={padding.left}
            y1={yScale(portfolioSummary.total_cost_basis)}
            x2={chartWidth - padding.right}
            y2={yScale(portfolioSummary.total_cost_basis)}
            stroke="var(--ft-color-warning, #FFD700)"
            strokeWidth="2"
            strokeDasharray="8,4"
          />
          <text
            x={chartWidth - padding.right - 10}
            y={yScale(portfolioSummary.total_cost_basis) - 8}
            textAnchor="end"
            fill="var(--ft-color-warning, #FFD700)"
            fontSize="10"
            fontWeight="bold"
            fontFamily={fontFamily}
          >
            COST BASIS
          </text>

          {/* Chart title */}
          <text
            x={chartWidth / 2}
            y={15}
            textAnchor="middle"
            fill={colors.primary}
            fontSize="12"
            fontWeight="bold"
            fontFamily={fontFamily}
          >
            {t('performance.portfolioValue')}
          </text>
        </svg>
      </div>

      {/* Performance Table */}
      <div style={{
        color: colors.primary,
        fontSize: fontSize.tiny,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '8px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${colors.primary}`
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
          backgroundColor: 'var(--ft-color-header, #1A1A1A)',
          fontSize: fontSize.tiny,
          fontWeight: 700,
          letterSpacing: '0.5px',
          position: 'sticky',
          top: 0,
          zIndex: 1,
          borderBottom: `1px solid ${colors.primary}`
        }}>
          <div style={{ color: colors.primary }}>DATE</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>VALUE</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>CHANGE</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>CHANGE %</div>
        </div>

        {/* Table Rows */}
        {performanceData.slice().reverse().map((d, index) => (
          <div
            key={d.date}
            onMouseEnter={() => setHoveredRow(index)}
            onMouseLeave={() => setHoveredRow(null)}
            style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1.2fr 1fr 1fr',
              gap: '8px',
              padding: '8px',
              backgroundColor: hoveredRow === index
                ? 'var(--ft-color-hover, #1F1F1F)'
                : index % 2 === 0
                  ? 'rgba(255,255,255,0.03)'
                  : 'transparent',
              borderLeft: `3px solid ${d.changePercent >= 0 ? colors.success : colors.alert}`,
              fontSize: fontSize.tiny,
              marginBottom: '2px',
              transition: 'all 0.2s ease',
              cursor: 'default'
            }}
          >
            <div style={{ color: colors.textMuted }}>{d.date}</div>
            <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right', fontWeight: 700 }}>
              {formatCurrency(d.value, currency)}
            </div>
            <div style={{
              color: d.change >= 0 ? colors.success : colors.alert,
              textAlign: 'right',
              fontWeight: 700
            }}>
              {formatCurrency(d.change, currency)}
            </div>
            <div style={{
              color: d.changePercent >= 0 ? colors.success : colors.alert,
              textAlign: 'right',
              fontWeight: 700
            }}>
              {formatPercent(d.changePercent)}
            </div>
          </div>
        ))}
      </div>

      {/* Info Note */}
      {performanceData.length < 7 && (
        <div style={{
          marginTop: '12px',
          padding: '12px',
          backgroundColor: 'rgba(255,136,0,0.05)',
          border: `1px solid ${colors.primary}`,
          borderRadius: '2px',
          fontSize: fontSize.tiny,
          color: colors.textMuted
        }}>
          <strong style={{ color: colors.primary }}>TIP:</strong> Portfolio snapshots are created automatically.
          More historical data will be available as you continue to use the application.
          Current data points: {performanceData.length}
        </div>
      )}
    </div>
  );
};

export default PerformanceView;
