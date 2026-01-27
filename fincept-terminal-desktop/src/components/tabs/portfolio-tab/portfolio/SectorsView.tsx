import React, { useMemo, useEffect, useState } from 'react';
import { PortfolioSummary, PortfolioHolding } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent } from './utils';
import { sectorService } from '../../../../services/data-sources/sectorService';
import { FINCEPT, BORDERS, COMMON_STYLES, TYPOGRAPHY, EFFECTS } from '../finceptStyles';

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
  const [sectorsLoaded, setSectorsLoaded] = useState(false);
  const [, forceUpdate] = useState(0);
  const [hoveredSectorRow, setHoveredSectorRow] = useState<string | null>(null);
  const [hoveredHoldingRow, setHoveredHoldingRow] = useState<string | null>(null);
  const [hoveredLegendRow, setHoveredLegendRow] = useState<string | null>(null);

  // Fincept color palette for sectors
  const sectorColors = [
    FINCEPT.CYAN,
    FINCEPT.GREEN,
    FINCEPT.ORANGE,
    FINCEPT.PURPLE,
    FINCEPT.BLUE,
    FINCEPT.YELLOW,
    FINCEPT.RED
  ];

  // Prefetch sector data for all holdings
  useEffect(() => {
    const symbols = portfolioSummary.holdings.map(h => h.symbol);
    if (symbols.length > 0) {
      setSectorsLoaded(false);
      sectorService.prefetchSectors(symbols).then(() => {
        setSectorsLoaded(true);
        forceUpdate(n => n + 1); // Trigger re-render with updated cache
      });
    }
  }, [portfolioSummary.holdings]);

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
  }, [portfolioSummary.holdings, portfolioSummary.total_market_value, sectorsLoaded]);

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
      backgroundColor: FINCEPT.DARK_BG,
      padding: '12px',
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      {/* Section Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: '16px'
      }}>
        SECTOR ALLOCATION
      </div>

      {!sectorsLoaded && portfolioSummary.holdings.length > 0 && (
        <div style={{
          padding: '8px',
          marginBottom: '8px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.CYAN,
          borderRadius: '2px',
          color: FINCEPT.CYAN,
          fontSize: '11px',
          fontFamily: TYPOGRAPHY.MONO,
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}>
          <span style={{ animation: 'pulse 1.5s infinite' }}>●</span>
          Loading sector data from API...
        </div>
      )}

      {sectorAllocations.length === 0 ? (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          color: FINCEPT.GRAY,
          fontSize: '11px',
          fontFamily: TYPOGRAPHY.MONO
        }}>
          No sector data available. Add holdings with sector information.
        </div>
      ) : (
        <div style={{ display: 'grid', gridTemplateColumns: '400px 1fr', gap: '24px' }}>
          {/* Pie Chart */}
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center'
          }}>
            <svg width="300" height="300" style={{ marginBottom: '16px' }}>
              {/* Pie segments */}
              {pieData.map((sector, index) => (
                <g key={sector.sector}>
                  <path
                    d={createArc(sector.startAngle, sector.endAngle)}
                    fill={sector.color}
                    stroke={FINCEPT.DARK_BG}
                    strokeWidth="2"
                    opacity="0.9"
                  />
                  {/* Label */}
                  {sector.weight > 5 && (
                    <text
                      x={centerX + (radius * 0.7) * Math.cos((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      y={centerY + (radius * 0.7) * Math.sin((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      textAnchor="middle"
                      fill={FINCEPT.WHITE}
                      fontSize="11px"
                      fontWeight={700}
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
                fill={FINCEPT.PANEL_BG}
                stroke={FINCEPT.ORANGE}
                strokeWidth="2"
              />
              <text
                x={centerX}
                y={centerY - 10}
                textAnchor="middle"
                fill={FINCEPT.ORANGE}
                fontSize="13px"
                fontWeight={700}
                fontFamily={TYPOGRAPHY.MONO}
              >
                SECTORS
              </text>
              <text
                x={centerX}
                y={centerY + 10}
                textAnchor="middle"
                fill={FINCEPT.WHITE}
                fontSize="15px"
                fontWeight={700}
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
                  onMouseEnter={() => setHoveredLegendRow(sector.sector)}
                  onMouseLeave={() => setHoveredLegendRow(null)}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    marginBottom: '4px',
                    padding: '4px',
                    backgroundColor: hoveredLegendRow === sector.sector ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                    borderLeft: `4px solid ${sector.color}`,
                    borderRadius: '2px',
                    transition: EFFECTS.TRANSITION_STANDARD,
                    cursor: 'default'
                  }}
                >
                  <div style={{
                    width: '12px',
                    height: '12px',
                    backgroundColor: sector.color,
                    marginRight: '8px',
                    border: BORDERS.STANDARD,
                    borderRadius: '2px'
                  }} />
                  <div style={{ flex: 1 }}>
                    <span style={{
                      color: FINCEPT.WHITE,
                      fontSize: '11px',
                      fontWeight: 700,
                      fontFamily: TYPOGRAPHY.MONO
                    }}>
                      {sector.sector}
                    </span>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <span style={{
                      color: FINCEPT.CYAN,
                      fontSize: '10px',
                      fontWeight: 700,
                      fontFamily: TYPOGRAPHY.MONO
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
              color: FINCEPT.ORANGE,
              fontSize: '11px',
              fontWeight: 700,
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              marginBottom: '8px',
              paddingBottom: '4px',
              borderBottom: BORDERS.ORANGE
            }}>
              SECTOR BREAKDOWN
            </div>

            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
              gap: '8px',
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              fontSize: '9px',
              fontWeight: 700,
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              borderBottom: BORDERS.ORANGE,
              marginBottom: '4px',
              borderRadius: '2px'
            }}>
              <div style={{ color: FINCEPT.ORANGE }}>SECTOR</div>
              <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>VALUE</div>
              <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>WEIGHT</div>
              <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>HOLDINGS</div>
            </div>

            {/* Sector Rows */}
            {sectorAllocations.map((sector, index) => {
              const sectorKey = sector.sector;
              return (
                <div key={sectorKey} style={{ marginBottom: '12px' }}>
                  {/* Sector Summary Row */}
                  <div
                    onMouseEnter={() => setHoveredSectorRow(sectorKey)}
                    onMouseLeave={() => setHoveredSectorRow(null)}
                    style={{
                      display: 'grid',
                      gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
                      gap: '8px',
                      padding: '8px',
                      backgroundColor: hoveredSectorRow === sectorKey
                        ? FINCEPT.HOVER
                        : index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                      borderLeft: `3px solid ${sector.color}`,
                      borderRadius: '2px',
                      fontSize: '11px',
                      fontFamily: TYPOGRAPHY.MONO,
                      marginBottom: '4px',
                      minHeight: '32px',
                      alignItems: 'center',
                      transition: EFFECTS.TRANSITION_STANDARD,
                      cursor: 'default'
                    }}
                  >
                    <div style={{ color: sector.color, fontWeight: 700 }}>
                      {sector.sector}
                    </div>
                    <div style={{ color: FINCEPT.CYAN, textAlign: 'right', fontWeight: 700, fontSize: '10px' }}>
                      {formatCurrency(sector.value, currency)}
                    </div>
                    <div style={{ color: FINCEPT.CYAN, textAlign: 'right', fontWeight: 700, fontSize: '10px' }}>
                      {sector.weight.toFixed(2)}%
                    </div>
                    <div style={{ color: FINCEPT.CYAN, textAlign: 'right', fontSize: '10px' }}>
                      {sector.holdings.length}
                    </div>
                  </div>

                  {/* Holdings in Sector */}
                  {sector.holdings.map(holding => {
                    const holdingKey = `${sectorKey}-${holding.id}`;
                    return (
                      <div
                        key={holding.id}
                        onMouseEnter={() => setHoveredHoldingRow(holdingKey)}
                        onMouseLeave={() => setHoveredHoldingRow(null)}
                        style={{
                          display: 'grid',
                          gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
                          gap: '8px',
                          padding: '4px 8px 4px 16px',
                          backgroundColor: hoveredHoldingRow === holdingKey
                            ? FINCEPT.HOVER
                            : 'rgba(255,255,255,0.01)',
                          fontSize: '10px',
                          fontFamily: TYPOGRAPHY.MONO,
                          borderLeft: `1px solid ${sector.color}`,
                          marginLeft: '8px',
                          minHeight: '28px',
                          alignItems: 'center',
                          borderRadius: '2px',
                          transition: EFFECTS.TRANSITION_STANDARD,
                          cursor: 'default'
                        }}
                      >
                        <div style={{ color: FINCEPT.CYAN }}>{'\u2514\u2500'} {holding.symbol}</div>
                        <div style={{ color: FINCEPT.WHITE, textAlign: 'right' }}>
                          {formatCurrency(holding.market_value, currency)}
                        </div>
                        <div style={{ color: FINCEPT.GRAY, textAlign: 'right' }}>
                          {holding.weight.toFixed(2)}%
                        </div>
                        <div style={{
                          color: holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                          textAlign: 'right',
                          fontWeight: 600
                        }}>
                          {formatPercent(holding.unrealized_pnl_percent)}
                        </div>
                      </div>
                    );
                  })}
                </div>
              );
            })}

            {/* Sector Statistics */}
            <div style={{
              marginTop: '16px',
              padding: '12px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: BORDERS.ORANGE,
              borderRadius: '2px'
            }}>
              <div style={{
                color: FINCEPT.ORANGE,
                fontSize: '9px',
                fontWeight: 700,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '8px'
              }}>
                DIVERSIFICATION METRICS
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <div>
                  <div style={{
                    ...COMMON_STYLES.dataLabel,
                    marginBottom: '2px'
                  }}>
                    Total Sectors
                  </div>
                  <div style={{
                    color: FINCEPT.CYAN,
                    fontSize: '15px',
                    fontWeight: 700,
                    fontFamily: TYPOGRAPHY.MONO
                  }}>
                    {sectorAllocations.length}
                  </div>
                </div>
                <div>
                  <div style={{
                    ...COMMON_STYLES.dataLabel,
                    marginBottom: '2px'
                  }}>
                    Largest Sector
                  </div>
                  <div style={{
                    color: FINCEPT.YELLOW,
                    fontSize: '15px',
                    fontWeight: 700,
                    fontFamily: TYPOGRAPHY.MONO
                  }}>
                    {sectorAllocations[0]?.sector || 'N/A'} ({sectorAllocations[0]?.weight.toFixed(1)}%)
                  </div>
                </div>
                <div>
                  <div style={{
                    ...COMMON_STYLES.dataLabel,
                    marginBottom: '2px'
                  }}>
                    Concentration Risk
                  </div>
                  <div style={{
                    color: sectorAllocations[0]?.weight > 40 ? FINCEPT.RED : sectorAllocations[0]?.weight > 25 ? FINCEPT.YELLOW : FINCEPT.GREEN,
                    fontSize: '15px',
                    fontWeight: 700,
                    fontFamily: TYPOGRAPHY.MONO
                  }}>
                    {sectorAllocations[0]?.weight > 40 ? 'HIGH' : sectorAllocations[0]?.weight > 25 ? 'MEDIUM' : 'LOW'}
                  </div>
                </div>
                <div>
                  <div style={{
                    ...COMMON_STYLES.dataLabel,
                    marginBottom: '2px'
                  }}>
                    Diversification
                  </div>
                  <div style={{
                    color: sectorAllocations.length >= 5 ? FINCEPT.GREEN : sectorAllocations.length >= 3 ? FINCEPT.YELLOW : FINCEPT.RED,
                    fontSize: '15px',
                    fontWeight: 700,
                    fontFamily: TYPOGRAPHY.MONO
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
