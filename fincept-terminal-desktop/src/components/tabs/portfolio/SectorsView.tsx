import React, { useMemo } from 'react';
import { PortfolioSummary, PortfolioHolding } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency, formatPercent } from './utils';
import { sectorService } from '../../../services/sectorService';

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
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW, BLUE, PURPLE } = BLOOMBERG_COLORS;
  const currency = portfolioSummary.portfolio.currency;

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

    const colors = [BLUE, GREEN, ORANGE, PURPLE, CYAN, YELLOW, RED];

    return Array.from(sectorMap.entries())
      .map(([sector, data], index) => ({
        sector,
        value: data.value,
        weight: totalValue > 0 ? (data.value / totalValue) * 100 : 0,
        holdings: data.holdings,
        color: colors[index % colors.length]
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
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '16px'
      }}>
        SECTOR ALLOCATION
      </div>

      {sectorAllocations.length === 0 ? (
        <div style={{ padding: '32px', textAlign: 'center', color: GRAY, fontSize: '11px' }}>
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
                    stroke="#000000"
                    strokeWidth="2"
                    opacity="0.8"
                  />
                  {/* Label */}
                  {sector.weight > 5 && (
                    <text
                      x={centerX + (radius * 0.7) * Math.cos((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      y={centerY + (radius * 0.7) * Math.sin((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      textAnchor="middle"
                      fill="#FFFFFF"
                      fontSize="11"
                      fontWeight="bold"
                      fontFamily="Consolas, monospace"
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
                fill="#1a1a1a"
                stroke="#FFA500"
                strokeWidth="2"
              />
              <text
                x={centerX}
                y={centerY - 10}
                textAnchor="middle"
                fill="#FFA500"
                fontSize="12"
                fontWeight="bold"
                fontFamily="Consolas, monospace"
              >
                SECTORS
              </text>
              <text
                x={centerX}
                y={centerY + 10}
                textAnchor="middle"
                fill="#FFFFFF"
                fontSize="14"
                fontWeight="bold"
                fontFamily="Consolas, monospace"
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
                    marginBottom: '6px',
                    padding: '6px',
                    backgroundColor: 'rgba(255,255,255,0.02)',
                    borderLeft: `4px solid ${sector.color}`
                  }}
                >
                  <div style={{
                    width: '12px',
                    height: '12px',
                    backgroundColor: sector.color,
                    marginRight: '8px',
                    border: '1px solid #000'
                  }} />
                  <div style={{ flex: 1 }}>
                    <span style={{ color: WHITE, fontSize: '10px', fontWeight: 'bold' }}>
                      {sector.sector}
                    </span>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <span style={{ color: YELLOW, fontSize: '10px', fontWeight: 'bold' }}>
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
              color: ORANGE,
              fontSize: '11px',
              fontWeight: 'bold',
              marginBottom: '8px',
              paddingBottom: '4px',
              borderBottom: `1px solid ${ORANGE}`
            }}>
              SECTOR BREAKDOWN
            </div>

            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
              gap: '8px',
              padding: '8px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              fontSize: '10px',
              fontWeight: 'bold',
              marginBottom: '4px'
            }}>
              <div style={{ color: WHITE }}>SECTOR</div>
              <div style={{ color: WHITE, textAlign: 'right' }}>VALUE</div>
              <div style={{ color: WHITE, textAlign: 'right' }}>WEIGHT</div>
              <div style={{ color: WHITE, textAlign: 'right' }}>HOLDINGS</div>
            </div>

            {/* Sector Rows */}
            {sectorAllocations.map((sector, index) => {
              return (
                <div key={sector.sector} style={{ marginBottom: '12px' }}>
                  {/* Sector Summary Row */}
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
                    gap: '8px',
                    padding: '8px',
                    backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                    borderLeft: `3px solid ${sector.color}`,
                    fontSize: '10px',
                    marginBottom: '4px'
                  }}>
                    <div style={{ color: sector.color, fontWeight: 'bold' }}>
                      {sector.sector}
                    </div>
                    <div style={{ color: YELLOW, textAlign: 'right', fontWeight: 'bold' }}>
                      {formatCurrency(sector.value, currency)}
                    </div>
                    <div style={{ color: YELLOW, textAlign: 'right', fontWeight: 'bold' }}>
                      {sector.weight.toFixed(2)}%
                    </div>
                    <div style={{ color: CYAN, textAlign: 'right' }}>
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
                        gap: '8px',
                        padding: '6px 8px 6px 20px',
                        backgroundColor: 'rgba(255,255,255,0.01)',
                        fontSize: '9px',
                        borderLeft: `1px solid ${sector.color}`,
                        marginLeft: '8px'
                      }}
                    >
                      <div style={{ color: CYAN }}>└─ {holding.symbol}</div>
                      <div style={{ color: WHITE, textAlign: 'right' }}>
                        {formatCurrency(holding.market_value, currency)}
                      </div>
                      <div style={{ color: GRAY, textAlign: 'right' }}>
                        {holding.weight.toFixed(2)}%
                      </div>
                      <div style={{
                        color: holding.unrealized_pnl >= 0 ? GREEN : RED,
                        textAlign: 'right',
                        fontWeight: 'bold'
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
              marginTop: '16px',
              padding: '12px',
              backgroundColor: 'rgba(255,165,0,0.05)',
              border: `1px solid ${ORANGE}`,
              borderRadius: '4px'
            }}>
              <div style={{
                color: ORANGE,
                fontSize: '10px',
                fontWeight: 'bold',
                marginBottom: '8px'
              }}>
                DIVERSIFICATION METRICS
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <div>
                  <div style={{ color: GRAY, fontSize: '9px', marginBottom: '2px' }}>
                    Total Sectors
                  </div>
                  <div style={{ color: CYAN, fontSize: '14px', fontWeight: 'bold' }}>
                    {sectorAllocations.length}
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '9px', marginBottom: '2px' }}>
                    Largest Sector
                  </div>
                  <div style={{ color: YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
                    {sectorAllocations[0]?.sector || 'N/A'} ({sectorAllocations[0]?.weight.toFixed(1)}%)
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '9px', marginBottom: '2px' }}>
                    Concentration Risk
                  </div>
                  <div style={{
                    color: sectorAllocations[0]?.weight > 40 ? RED : sectorAllocations[0]?.weight > 25 ? YELLOW : GREEN,
                    fontSize: '14px',
                    fontWeight: 'bold'
                  }}>
                    {sectorAllocations[0]?.weight > 40 ? 'HIGH' : sectorAllocations[0]?.weight > 25 ? 'MEDIUM' : 'LOW'}
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '9px', marginBottom: '2px' }}>
                    Diversification
                  </div>
                  <div style={{
                    color: sectorAllocations.length >= 5 ? GREEN : sectorAllocations.length >= 3 ? YELLOW : RED,
                    fontSize: '14px',
                    fontWeight: 'bold'
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
