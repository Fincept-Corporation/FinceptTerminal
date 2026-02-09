import React, { useMemo, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { PortfolioSummary, PortfolioHolding } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent } from './utils';
import { sectorService } from '../../../../services/data-sources/sectorService';

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
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const currency = portfolioSummary.portfolio.currency;
  const [sectorsLoaded, setSectorsLoaded] = useState(false);
  const [, forceUpdate] = useState(0);
  const [hoveredSectorRow, setHoveredSectorRow] = useState<string | null>(null);
  const [hoveredHoldingRow, setHoveredHoldingRow] = useState<string | null>(null);
  const [hoveredLegendRow, setHoveredLegendRow] = useState<string | null>(null);

  // Theme-based color palette for sectors
  const sectorColors = [
    'var(--ft-color-accent, #00E5FF)',
    colors.success,
    colors.primary,
    'var(--ft-color-purple, #A855F7)',
    'var(--ft-color-blue, #3B82F6)',
    'var(--ft-color-warning, #FFD700)',
    colors.alert
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
  }, [portfolioSummary.holdings, portfolioSummary.total_market_value, sectorsLoaded, sectorColors]);

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
      backgroundColor: colors.background,
      padding: '12px',
      overflow: 'auto',
      fontFamily
    }}>
      {/* Section Header */}
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
        {t('sectors.sectorAllocation')}
      </div>

      {!sectorsLoaded && portfolioSummary.holdings.length > 0 && (
        <div style={{
          padding: '8px',
          marginBottom: '8px',
          backgroundColor: colors.panel,
          border: `1px solid var(--ft-color-accent, #00E5FF)`,
          borderRadius: '2px',
          color: 'var(--ft-color-accent, #00E5FF)',
          fontSize: fontSize.tiny,
          fontFamily,
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}>
          <span style={{ animation: 'pulse 1.5s infinite' }}>●</span>
          {t('sectors.loadingSectorData')}
        </div>
      )}

      {sectorAllocations.length === 0 ? (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          color: colors.textMuted,
          fontSize: fontSize.tiny,
          fontFamily
        }}>
          {t('sectors.noSectorData')}
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
                    stroke={colors.background}
                    strokeWidth="2"
                    opacity="0.9"
                  />
                  {/* Label */}
                  {sector.weight > 5 && (
                    <text
                      x={centerX + (radius * 0.7) * Math.cos((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      y={centerY + (radius * 0.7) * Math.sin((((sector.startAngle + sector.endAngle) / 2) - 90) * Math.PI / 180)}
                      textAnchor="middle"
                      fill={colors.text}
                      fontSize={fontSize.tiny}
                      fontWeight={700}
                      fontFamily={fontFamily}
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
                fill={colors.panel}
                stroke={colors.primary}
                strokeWidth="2"
              />
              <text
                x={centerX}
                y={centerY - 10}
                textAnchor="middle"
                fill={colors.primary}
                fontSize={fontSize.small}
                fontWeight={700}
                fontFamily={fontFamily}
              >
                {t('views.sectors')}
              </text>
              <text
                x={centerX}
                y={centerY + 10}
                textAnchor="middle"
                fill={colors.text}
                fontSize={fontSize.body}
                fontWeight={700}
                fontFamily={fontFamily}
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
                    backgroundColor: hoveredLegendRow === sector.sector ? 'var(--ft-color-hover, #1F1F1F)' : colors.panel,
                    borderLeft: `4px solid ${sector.color}`,
                    borderRadius: '2px',
                    transition: 'all 0.2s ease',
                    cursor: 'default'
                  }}
                >
                  <div style={{
                    width: '12px',
                    height: '12px',
                    backgroundColor: sector.color,
                    marginRight: '8px',
                    border: '1px solid var(--ft-color-border, #2A2A2A)',
                    borderRadius: '2px'
                  }} />
                  <div style={{ flex: 1 }}>
                    <span style={{
                      color: colors.text,
                      fontSize: fontSize.tiny,
                      fontWeight: 700,
                      fontFamily
                    }}>
                      {sector.sector}
                    </span>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <span style={{
                      color: 'var(--ft-color-accent, #00E5FF)',
                      fontSize: fontSize.tiny,
                      fontWeight: 700,
                      fontFamily
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
              color: colors.primary,
              fontSize: fontSize.tiny,
              fontWeight: 700,
              fontFamily,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              marginBottom: '8px',
              paddingBottom: '4px',
              borderBottom: `1px solid ${colors.primary}`
            }}>
              {t('sectors.sectorBreakdown')}
            </div>

            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1.5fr 1fr 1fr 1fr',
              gap: '8px',
              padding: '8px 12px',
              backgroundColor: 'var(--ft-color-header, #1A1A1A)',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              fontFamily,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              borderBottom: `1px solid ${colors.primary}`,
              marginBottom: '4px',
              borderRadius: '2px'
            }}>
              <div style={{ color: colors.primary }}>{t('sectors.sector')}</div>
              <div style={{ color: colors.primary, textAlign: 'right' }}>{t('sectors.value')}</div>
              <div style={{ color: colors.primary, textAlign: 'right' }}>{t('sectors.weight')}</div>
              <div style={{ color: colors.primary, textAlign: 'right' }}>{t('sectors.holdings')}</div>
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
                        ? 'var(--ft-color-hover, #1F1F1F)'
                        : index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                      borderLeft: `3px solid ${sector.color}`,
                      borderRadius: '2px',
                      fontSize: fontSize.tiny,
                      fontFamily,
                      marginBottom: '4px',
                      minHeight: '32px',
                      alignItems: 'center',
                      transition: 'all 0.2s ease',
                      cursor: 'default'
                    }}
                  >
                    <div style={{ color: sector.color, fontWeight: 700 }}>
                      {sector.sector}
                    </div>
                    <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right', fontWeight: 700, fontSize: fontSize.tiny }}>
                      {formatCurrency(sector.value, currency)}
                    </div>
                    <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right', fontWeight: 700, fontSize: fontSize.tiny }}>
                      {sector.weight.toFixed(2)}%
                    </div>
                    <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right', fontSize: fontSize.tiny }}>
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
                            ? 'var(--ft-color-hover, #1F1F1F)'
                            : 'rgba(255,255,255,0.01)',
                          fontSize: fontSize.tiny,
                          fontFamily,
                          borderLeft: `1px solid ${sector.color}`,
                          marginLeft: '8px',
                          minHeight: '28px',
                          alignItems: 'center',
                          borderRadius: '2px',
                          transition: 'all 0.2s ease',
                          cursor: 'default'
                        }}
                      >
                        <div style={{ color: 'var(--ft-color-accent, #00E5FF)' }}>{'\u2514\u2500'} {holding.symbol}</div>
                        <div style={{ color: colors.text, textAlign: 'right' }}>
                          {formatCurrency(holding.market_value, currency)}
                        </div>
                        <div style={{ color: colors.textMuted, textAlign: 'right' }}>
                          {holding.weight.toFixed(2)}%
                        </div>
                        <div style={{
                          color: holding.unrealized_pnl >= 0 ? colors.success : colors.alert,
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
              backgroundColor: colors.panel,
              border: `1px solid ${colors.primary}`,
              borderRadius: '2px'
            }}>
              <div style={{
                color: colors.primary,
                fontSize: fontSize.tiny,
                fontWeight: 700,
                fontFamily,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '8px'
              }}>
                {t('sectors.diversificationMetrics')}
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <div>
                  <div style={{
                    color: colors.textMuted,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '2px'
                  }}>
                    {t('sectors.totalSectors')}
                  </div>
                  <div style={{
                    color: 'var(--ft-color-accent, #00E5FF)',
                    fontSize: fontSize.body,
                    fontWeight: 700,
                    fontFamily
                  }}>
                    {sectorAllocations.length}
                  </div>
                </div>
                <div>
                  <div style={{
                    color: colors.textMuted,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '2px'
                  }}>
                    {t('sectors.largestSector')}
                  </div>
                  <div style={{
                    color: 'var(--ft-color-warning, #FFD700)',
                    fontSize: fontSize.body,
                    fontWeight: 700,
                    fontFamily
                  }}>
                    {sectorAllocations[0]?.sector || 'N/A'} ({sectorAllocations[0]?.weight.toFixed(1)}%)
                  </div>
                </div>
                <div>
                  <div style={{
                    color: colors.textMuted,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '2px'
                  }}>
                    {t('sectors.concentrationRisk')}
                  </div>
                  <div style={{
                    color: sectorAllocations[0]?.weight > 40 ? colors.alert : sectorAllocations[0]?.weight > 25 ? 'var(--ft-color-warning, #FFD700)' : colors.success,
                    fontSize: fontSize.body,
                    fontWeight: 700,
                    fontFamily
                  }}>
                    {sectorAllocations[0]?.weight > 40 ? t('sectors.high') : sectorAllocations[0]?.weight > 25 ? t('sectors.medium') : t('sectors.low')}
                  </div>
                </div>
                <div>
                  <div style={{
                    color: colors.textMuted,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '2px'
                  }}>
                    {t('sectors.diversification')}
                  </div>
                  <div style={{
                    color: sectorAllocations.length >= 5 ? colors.success : sectorAllocations.length >= 3 ? 'var(--ft-color-warning, #FFD700)' : colors.alert,
                    fontSize: fontSize.body,
                    fontWeight: 700,
                    fontFamily
                  }}>
                    {sectorAllocations.length >= 5 ? t('sectors.good') : sectorAllocations.length >= 3 ? t('sectors.moderate') : t('sectors.poor')}
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
