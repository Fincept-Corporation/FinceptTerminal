// HeatMapWidget - Sector/Market Performance Heatmap
// Visual heatmap showing relative performance across S&P 500 sector ETFs
// Uses real data from Yahoo Finance via the unified cache system

import React, { useState, useMemo } from 'react';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { useCache } from '@/hooks/useCache';

const FC = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

interface HeatMapWidgetProps {
  id: string;
  onRemove?: () => void;
}

// S&P 500 Sector ETFs - these are real tickers fetched from Yahoo Finance
const SECTOR_ETFS = [
  'XLK',   // Technology
  'XLV',   // Health Care
  'XLF',   // Financials
  'XLY',   // Consumer Discretionary
  'XLE',   // Energy
  'XLI',   // Industrials
  'XLB',   // Materials
  'XLRE',  // Real Estate
  'XLU',   // Utilities
  'XLC',   // Communication Services
  'XLP',   // Consumer Staples
];

// Sector display names and weights (weight for visual sizing)
const SECTOR_CONFIG: Record<string, { name: string; weight: number }> = {
  'XLK': { name: 'TECH', weight: 5 },
  'XLV': { name: 'HEALTH', weight: 4 },
  'XLF': { name: 'FINANCE', weight: 4 },
  'XLY': { name: 'CONSUMER', weight: 3 },
  'XLE': { name: 'ENERGY', weight: 3 },
  'XLI': { name: 'INDUSTRIAL', weight: 3 },
  'XLB': { name: 'MATERIALS', weight: 2 },
  'XLRE': { name: 'REAL ESTATE', weight: 2 },
  'XLU': { name: 'UTILITIES', weight: 2 },
  'XLC': { name: 'COMM SVCS', weight: 3 },
  'XLP': { name: 'STAPLES', weight: 2 },
};

interface SectorData {
  symbol: string;
  name: string;
  change: number;
  price: number;
  weight: number;
}

// Color interpolation for heatmap
const getHeatColor = (change: number): string => {
  const absChange = Math.abs(change);
  const intensity = Math.min(absChange / 2, 1); // normalize to 0-1 range

  if (change > 0) {
    // Green gradient
    const r = Math.round(0 * intensity);
    const g = Math.round(80 + 135 * intensity);
    const b = Math.round(30 * intensity);
    return `rgb(${r}, ${g}, ${b})`;
  } else if (change < 0) {
    // Red gradient
    const r = Math.round(80 + 135 * intensity);
    const g = Math.round(15 * (1 - intensity));
    const b = Math.round(15 * (1 - intensity));
    return `rgb(${r}, ${g}, ${b})`;
  }
  return FC.MUTED;
};

const getBgOpacity = (change: number): number => {
  return Math.min(Math.abs(change) / 3, 0.6) + 0.15;
};

export const HeatMapWidget: React.FC<HeatMapWidgetProps> = ({ id, onRemove }) => {
  const [hoveredSector, setHoveredSector] = useState<string | null>(null);

  // Fetch real sector ETF data using the unified cache system
  const {
    data: quotes,
    isLoading,
    isFetching,
    error,
    refresh
  } = useCache<QuoteData[]>({
    key: 'market-widget:sector-heatmap',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(SECTOR_ETFS),
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000, // Auto-refresh every 5 minutes
    staleWhileRevalidate: true
  });

  // Transform quotes to sector data with weights
  const sectors: SectorData[] = useMemo(() => {
    if (!quotes || quotes.length === 0) return [];

    return quotes.map(quote => {
      const config = SECTOR_CONFIG[quote.symbol] || { name: quote.symbol, weight: 2 };
      return {
        symbol: quote.symbol,
        name: config.name,
        change: quote.change_percent,
        price: quote.price,
        weight: config.weight,
      };
    });
  }, [quotes]);

  // Sort by weight for visual hierarchy
  const sortedSectors = useMemo(() =>
    [...sectors].sort((a, b) => b.weight - a.weight),
    [sectors]
  );

  // Calculate total weight for grid
  const totalWeight = useMemo(() =>
    sortedSectors.reduce((s, d) => s + d.weight, 0),
    [sortedSectors]
  );

  // Skeleton loader for sectors
  const SkeletonGrid = () => (
    <div style={{
      flex: 1,
      display: 'flex',
      flexWrap: 'wrap',
      gap: '2px',
      alignContent: 'flex-start',
    }}>
      {SECTOR_ETFS.map((symbol, idx) => {
        const config = SECTOR_CONFIG[symbol] || { weight: 2 };
        return (
          <div
            key={symbol}
            style={{
              flexBasis: config.weight >= 4 ? 'calc(50% - 2px)' : config.weight >= 3 ? 'calc(33.33% - 2px)' : 'calc(25% - 2px)',
              flexGrow: 0,
              flexShrink: 0,
              minHeight: config.weight >= 4 ? '52px' : '40px',
              backgroundColor: FC.BORDER,
              border: `1px solid ${FC.BORDER}`,
              borderRadius: '2px',
              animation: 'pulse 1.5s infinite',
              animationDelay: `${idx * 0.1}s`,
            }}
          />
        );
      })}
      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 0.3; }
          50% { opacity: 0.6; }
        }
      `}</style>
    </div>
  );

  return (
    <BaseWidget
      id={id}
      title="SECTOR HEATMAP"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !quotes) || isFetching}
      headerColor={FC.ORANGE}
      error={error?.message}
    >
      <div style={{
        padding: '4px',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        gap: '2px',
      }}>
        {/* Legend */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          padding: '2px 4px',
          marginBottom: '2px',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <div style={{ width: '8px', height: '8px', backgroundColor: '#AA1111', borderRadius: '1px' }} />
            <span style={{ fontSize: '8px', color: FC.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>BEARISH</span>
          </div>
          <span style={{ fontSize: '8px', color: FC.MUTED, fontWeight: 700 }}>S&P 500 SECTORS</span>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span style={{ fontSize: '8px', color: FC.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>BULLISH</span>
            <div style={{ width: '8px', height: '8px', backgroundColor: '#119944', borderRadius: '1px' }} />
          </div>
        </div>

        {/* Heatmap Grid - Treemap style */}
        {isLoading && sectors.length === 0 ? (
          <SkeletonGrid />
        ) : sectors.length === 0 && !isLoading ? (
          <div style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: FC.MUTED,
            fontSize: '10px',
          }}>
            No sector data available
          </div>
        ) : (
          <div style={{
            flex: 1,
            display: 'flex',
            flexWrap: 'wrap',
            gap: '2px',
            alignContent: 'flex-start',
          }}>
            {sortedSectors.map(sector => {
              const isHovered = hoveredSector === sector.symbol;
              const bgColor = getHeatColor(sector.change);

              return (
                <div
                  key={sector.symbol}
                  onMouseEnter={() => setHoveredSector(sector.symbol)}
                  onMouseLeave={() => setHoveredSector(null)}
                  style={{
                    flexBasis: sector.weight >= 4 ? 'calc(50% - 2px)' : sector.weight >= 3 ? 'calc(33.33% - 2px)' : 'calc(25% - 2px)',
                    flexGrow: 0,
                    flexShrink: 0,
                    minHeight: sector.weight >= 4 ? '52px' : '40px',
                    backgroundColor: bgColor,
                    opacity: getBgOpacity(sector.change) + (isHovered ? 0.2 : 0),
                    border: isHovered ? `1px solid ${FC.WHITE}40` : `1px solid ${FC.BORDER}`,
                    borderRadius: '2px',
                    padding: '4px 6px',
                    display: 'flex',
                    flexDirection: 'column',
                    justifyContent: 'center',
                    alignItems: 'center',
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                    position: 'relative',
                    overflow: 'hidden',
                  }}
                >
                  <div style={{
                    fontSize: sector.weight >= 4 ? '10px' : '9px',
                    fontWeight: 700,
                    color: FC.WHITE,
                    letterSpacing: '0.5px',
                    textShadow: '0 1px 2px rgba(0,0,0,0.8)',
                  }}>
                    {sector.name}
                  </div>
                  <div style={{
                    fontSize: sector.weight >= 4 ? '12px' : '10px',
                    fontWeight: 700,
                    color: FC.WHITE,
                    textShadow: '0 1px 2px rgba(0,0,0,0.8)',
                  }}>
                    {sector.change >= 0 ? '+' : ''}{sector.change.toFixed(2)}%
                  </div>
                  {isHovered && (
                    <div style={{
                      fontSize: '8px',
                      color: FC.WHITE,
                      textShadow: '0 1px 2px rgba(0,0,0,0.8)',
                      marginTop: '1px',
                    }}>
                      {sector.symbol} | ${sector.price.toFixed(2)}
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
