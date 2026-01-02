import React, { useMemo } from 'react';
import { PortfolioSummary, PortfolioHolding } from '../../../../services/portfolioService';
import { formatCurrency, formatPercent } from './utils';
import { sectorService } from '../../../../services/sectorService';
import { BLOOMBERG, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../bloombergStyles';

interface SectorsViewProps {
  portfolioSummary: PortfolioSummary;
}

interface SectorAllocation {
  sector: string;
  value: number;
  weight: number;
  holdings: PortfolioHolding[];
  color: string;
}

const SectorsView: React.FC<SectorsViewProps> = ({ portfolioSummary }) => {
  const currency = portfolioSummary.portfolio.currency;

  // Bloomberg color palette for sectors
  const sectorColors = [
    BLOOMBERG.CYAN,
    BLOOMBERG.GREEN,
    BLOOMBERG.ORANGE,
    BLOOMBERG.PURPLE,
    BLOOMBERG.BLUE,
    BLOOMBERG.YELLOW,
    BLOOMBERG.RED
  ];

  // Group holdings by sector using real sector classification
  const sectorAllocations = useMemo((): SectorAllocation[] => {
    const sectorMap = new Map<string, { value: number; holdings: PortfolioHolding[] }>();
    const totalValue = portfolioSummary.total_market_value;

    portfolioSummary.holdings.forEach(holding => {
      const sectorInfo = sectorService.getSectorInfo(holding.symbol);
      const sector = sectorInfo.sector;

      const existing = sectorMap.get(sector) || { value: 0, holdings: [] };
      sectorMap.set(sector, {
        value: existing.value + holding.market_value,
        holdings: [...existing.holdings, holding]
      });
    });

    return Array.from(sectorMap.entries())
      .map(([sector, data], index) => ({
        sector,
        value: data.value,
        weight: totalValue > 0 ? (data.value / totalValue) * 100 : 0,
        holdings: data.holdings,
        color: sectorColors[index % sectorColors.length]
      }))
      .sort((a, b) => b.weight - a.weight);
  }, [portfolioSummary.holdings, portfolioSummary.total_market_value]);

  // Calculate pie chart angles
  let currentAngle = 0;
  const pieData = sectorAllocations.map(sector => {
    const startAngle = currentAngle;
    const angleSize = (sector.weight / 100) * 360;
    currentAngle += angleSize;
    return {
      ...sector,
      startAngle,
      endAngle: currentAngle
    };
  });

  // SVG Pie Chart
  const radius = 120;
  const centerX = 150;
  const centerY = 150;

  const polarToCartesian = (angle: number) => {
    const radian = (angle - 90) * Math.PI / 180;
    return {
      x: centerX + radius * Math.cos(radian),
      y: centerY + radius * Math.sin(radian)
    };
  };

  const createArc = (startAngle: number, endAngle: number) => {
    const start = polarToCartesian(startAngle);
    const end = polarToCartesian(endAngle);
    const largeArc = endAngle - startAngle > 180 ? 1 : 0;

    return `M ${centerX} ${centerY} L ${start.x} ${start.y} A ${radius} ${radius} 0 ${largeArc} 1 ${end.x} ${end.y} Z`;
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      padding: SPACING.DEFAULT,
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      {/* Section Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.LARGE
      }}>
        SECTOR ALLOCATION
      </div>

      {sectorAllocations.length === 0 ? (
        <div style={{
          padding: SPACING.XLARGE,
          textAlign: 'center',
          color: BLOOMBERG.GRAY,
          fontSize: TYPOGRAPHY.DEFAULT
        }}>
          No sector data available. Add holdings with sector information.
        </div>
      ) : (
        <div style={{ display: 'grid', gridTemplateColumns: '400px 1fr', gap: SPACING.XLARGE }}>
          {/* Pie Chart */}
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center'
          }}>
            <svg width="300" height="300" style={{ marginBottom: SPACING.LARGE }}>
              {/* Pie segments */}
              {pieData.map((sector, index) => (
                <g key={sector.sector}>
                  <path
                    d={createArc(sector.startAngle, sector.endAngle)}
                    fill={sector.color}
                    stroke={BLOOMBERG.DARK_BG}
                    strokeWidth="2"
                    opacity="0.9"
                  />
                  {/* Label */}
                  {sector.weight > 5 && (
                    <text
                      x={centerX + (radius * 0.7) * Math.cos((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      y={centerY + (radius * 0.7) * Math.sin((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      textAnchor="middle"
                      fill={BLOOMBERG.WHITE}
                      fontSize={TYPOGRAPHY.DEFAULT}
                      fontWeight={TYPOGRAPHY.BOLD}
                      fontFamily={TYPOGRAPHY.MONO}
                    >
                      {sector.weight.toFixed(1)}%
                    </text>
                  )}
                </g>
              ))}
              {/* Center circle for donut effect */}
              <circle
                cx={centerX}
                cy={centerY}
                r={radius * 0.4}
                fill={BLOOMBERG.PANEL_BG}
                stroke={BLOOMBERG.ORANGE}
                strokeWidth="2"
              />
              <text
                x={centerX}
                y={centerY - 10}
                textAnchor="middle"
                fill={BLOOMBERG.ORANGE}
                fontSize={TYPOGRAPHY.SUBHEADING}
                fontWeight={TYPOGRAPHY.BOLD}
                fontFamily={TYPOGRAPHY.MONO}
              >
                SECTORS
              </text>
              <text
                x={centerX}
                y={centerY + 10}
                textAnchor="middle"
                fill={BLOOMBERG.WHITE}
                fontSize={TYPOGRAPHY.HEADING}
                fontWeight={TYPOGRAPHY.BOLD}
                fontFamily={TYPOGRAPHY.MONO}
              >
                {sectorAllocations.length}
              </text>
            </svg>

            {/* Legend */}
            <div style={{ width: '100%' }}>
              {sectorAllocations.map(sector => (
                <div
                  key={sector.sector}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    marginBottom: SPACING.SMALL,
                    padding: SPACING.SMALL,
                    backgroundColor: BLOOMBERG.PANEL_BG,
                    borderLeft: `4px solid ${sector.color}`
                  }}
                >
                  <div style={{
                    width: '12px',
                    height: '12px',
                    backgroundColor: sector.color,
                    marginRight: SPACING.MEDIUM,
                    border: BORDERS.STANDARD
                  }} />
                  <div style={{ flex: 1 }}>
                    <span style={{
                      color: BLOOMBERG.WHITE,
                      fontSize: TYPOGRAPHY.BODY,
                      fontWeight: TYPOGRAPHY.BOLD
                    }}>
                      {sector.sector}
                    </span>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <span style={{
                      color: BLOOMBERG.YELLOW,
                      fontSize: TYPOGRAPHY.BODY,
                      fontWeight: TYPOGRAPHY.BOLD
                    }}>
                      {sector.weight.toFixed(1)}%
                    </span>
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Sector Details Table */}
          <div>
            <div style={{
              color: BLOOMBERG.ORANGE,
              fontSize: TYPOGRAPHY.DEFAULT,
              fontWeight: TYPOGRAPHY.BOLD,
              marginBottom: SPACING.MEDIUM,
              paddingBottom: SPACING.SMALL,
              borderBottom: BORDERS.ORANGE
            }}>
              SECTOR BREAKDOWN
            </div>

            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
              gap: SPACING.MEDIUM,
              padding: SPACING.MEDIUM,
              backgroundColor: BLOOMBERG.HEADER_BG,
              fontSize: TYPOGRAPHY.BODY,
              fontWeight: TYPOGRAPHY.BOLD,
              borderBottom: BORDERS.ORANGE,
              marginBottom: SPACING.SMALL
            }}>
              <div style={{ color: BLOOMBERG.ORANGE }}>SECTOR</div>
              <div style={{ color: BLOOMBERG.ORANGE, textAlign: 'right' }}>VALUE</div>
              <div style={{ color: BLOOMBERG.ORANGE, textAlign: 'right' }}>WEIGHT</div>
              <div style={{ color: BLOOMBERG.ORANGE, textAlign: 'right' }}>HOLDINGS</div>
            </div>

            {/* Sector Rows */}
            {sectorAllocations.map((sector, index) => {
              return (
                <div key={sector.sector} style={{ marginBottom: SPACING.DEFAULT }}>
                  {/* Sector Summary Row */}
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
                    gap: SPACING.MEDIUM,
                    padding: SPACING.MEDIUM,
                    backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                    borderLeft: `3px solid ${sector.color}`,
                    fontSize: TYPOGRAPHY.BODY,
                    marginBottom: SPACING.SMALL,
                    minHeight: '32px',
                    alignItems: 'center'
                  }}>
                    <div style={{ color: sector.color, fontWeight: TYPOGRAPHY.BOLD }}>
                      {sector.sector}
                    </div>
                    <div style={{ color: BLOOMBERG.YELLOW, textAlign: 'right', fontWeight: TYPOGRAPHY.BOLD }}>
                      {formatCurrency(sector.value, currency)}
                    </div>
                    <div style={{ color: BLOOMBERG.YELLOW, textAlign: 'right', fontWeight: TYPOGRAPHY.BOLD }}>
                      {sector.weight.toFixed(2)}%
                    </div>
                    <div style={{ color: BLOOMBERG.CYAN, textAlign: 'right' }}>
                      {sector.holdings.length}
                    </div>
                  </div>

                  {/* Holdings in Sector */}
                  {sector.holdings.map(holding => (
                    <div
                      key={holding.id}
                      style={{
                        display: 'grid',
                        gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
                        gap: SPACING.MEDIUM,
                        padding: `${SPACING.SMALL} ${SPACING.MEDIUM} ${SPACING.SMALL} ${SPACING.LARGE}`,
                        backgroundColor: 'rgba(255,255,255,0.01)',
                        fontSize: TYPOGRAPHY.SMALL,
                        borderLeft: `1px solid ${sector.color}`,
                        marginLeft: SPACING.MEDIUM,
                        minHeight: '28px',
                        alignItems: 'center'
                      }}
                    >
                      <div style={{ color: BLOOMBERG.CYAN }}>└─ {holding.symbol}</div>
                      <div style={{ color: BLOOMBERG.WHITE, textAlign: 'right' }}>
                        {formatCurrency(holding.market_value, currency)}
                      </div>
                      <div style={{ color: BLOOMBERG.GRAY, textAlign: 'right' }}>
                        {holding.weight.toFixed(2)}%
                      </div>
                      <div style={{
                        color: holding.unrealized_pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                        textAlign: 'right',
                        fontWeight: TYPOGRAPHY.SEMIBOLD
                      }}>
                        {formatPercent(holding.unrealized_pnl_percent)}
                      </div>
                    </div>
                  ))}
                </div>
              );
            })}

            {/* Sector Statistics */}
            <div style={{
              marginTop: SPACING.LARGE,
              padding: SPACING.DEFAULT,
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: BORDERS.ORANGE,
            }}>
              <div style={{
                color: BLOOMBERG.ORANGE,
                fontSize: TYPOGRAPHY.BODY,
                fontWeight: TYPOGRAPHY.BOLD,
                marginBottom: SPACING.MEDIUM
              }}>
                DIVERSIFICATION METRICS
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
                <div>
                  <div style={{
                    color: BLOOMBERG.GRAY,
                    fontSize: TYPOGRAPHY.SMALL,
                    marginBottom: SPACING.TINY
                  }}>
                    Total Sectors
                  </div>
                  <div style={{
                    color: BLOOMBERG.CYAN,
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD
                  }}>
                    {sectorAllocations.length}
                  </div>
                </div>
                <div>
                  <div style={{
                    color: BLOOMBERG.GRAY,
                    fontSize: TYPOGRAPHY.SMALL,
                    marginBottom: SPACING.TINY
                  }}>
                    Largest Sector
                  </div>
                  <div style={{
                    color: BLOOMBERG.YELLOW,
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD
                  }}>
                    {sectorAllocations[0]?.sector || 'N/A'} ({sectorAllocations[0]?.weight.toFixed(1)}%)
                  </div>
                </div>
                <div>
                  <div style={{
                    color: BLOOMBERG.GRAY,
                    fontSize: TYPOGRAPHY.SMALL,
                    marginBottom: SPACING.TINY
                  }}>
                    Concentration Risk
                  </div>
                  <div style={{
                    color: sectorAllocations[0]?.weight > 40 ? BLOOMBERG.RED : sectorAllocations[0]?.weight > 25 ? BLOOMBERG.YELLOW : BLOOMBERG.GREEN,
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD
                  }}>
                    {sectorAllocations[0]?.weight > 40 ? 'HIGH' : sectorAllocations[0]?.weight > 25 ? 'MEDIUM' : 'LOW'}
                  </div>
                </div>
                <div>
                  <div style={{
                    color: BLOOMBERG.GRAY,
                    fontSize: TYPOGRAPHY.SMALL,
                    marginBottom: SPACING.TINY
                  }}>
                    Diversification
                  </div>
                  <div style={{
                    color: sectorAllocations.length >= 5 ? BLOOMBERG.GREEN : sectorAllocations.length >= 3 ? BLOOMBERG.YELLOW : BLOOMBERG.RED,
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD
                  }}>
                    {sectorAllocations.length >= 5 ? 'GOOD' : sectorAllocations.length >= 3 ? 'MODERATE' : 'POOR'}
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default SectorsView;
