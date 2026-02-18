// MarketPulsePanel - Right sidebar panel with real-time market intelligence
// Shows: Fear & Greed (simulated), Top Movers (real), Market Breadth, Global Snapshot (real)
// Uses Yahoo Finance data via the unified cache system

import React, { useState, useEffect, useMemo } from 'react';
import {
  TrendingUp, TrendingDown, Activity, Gauge, ArrowUpRight, ArrowDownRight,
  BarChart3, Globe, Zap, ChevronRight, Flame, Snowflake
} from 'lucide-react';
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
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

interface MarketPulsePanelProps {
  onNavigateToTab?: (tab: string) => void;
}

// Symbols for top movers (popular volatile stocks)
const MOVER_SYMBOLS = [
  'SMCI', 'PLTR', 'MSTR', 'NVDA', 'AMD', 'TSLA',
  'INTC', 'PFE', 'BA', 'NKE', 'DIS', 'PYPL'
];

// Symbols for global snapshot
const GLOBAL_SYMBOLS = [
  '^VIX',      // VIX Volatility Index
  '^TNX',      // 10-Year Treasury Yield
  'DX-Y.NYB',  // US Dollar Index (DXY)
  'GC=F',      // Gold Futures
  'CL=F',      // Crude Oil WTI
  'BTC-USD',   // Bitcoin (for dominance proxy)
];

// Fear & Greed Gauge component
const FearGreedGauge: React.FC<{ value: number }> = ({ value }) => {
  const getLabel = (v: number) => {
    if (v <= 20) return { text: 'EXTREME FEAR', color: FC.RED };
    if (v <= 40) return { text: 'FEAR', color: '#FF6644' };
    if (v <= 60) return { text: 'NEUTRAL', color: FC.YELLOW };
    if (v <= 80) return { text: 'GREED', color: '#88CC44' };
    return { text: 'EXTREME GREED', color: FC.GREEN };
  };

  const { text, color } = getLabel(value);

  return (
    <div style={{ padding: '8px 12px' }}>
      <div style={{
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
        marginBottom: '8px'
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FC.GRAY, letterSpacing: '0.5px' }}>
          FEAR & GREED INDEX
        </span>
        <Gauge size={12} color={FC.ORANGE} />
      </div>

      {/* Visual gauge bar */}
      <div style={{
        height: '6px',
        borderRadius: '3px',
        background: `linear-gradient(to right, ${FC.RED}, #FF6644, ${FC.YELLOW}, #88CC44, ${FC.GREEN})`,
        position: 'relative',
        marginBottom: '6px',
      }}>
        <div style={{
          position: 'absolute',
          left: `${value}%`,
          top: '-3px',
          width: '12px',
          height: '12px',
          backgroundColor: FC.WHITE,
          borderRadius: '50%',
          border: `2px solid ${color}`,
          transform: 'translateX(-50%)',
          boxShadow: `0 0 6px ${color}`,
        }} />
      </div>

      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline' }}>
        <div>
          <span style={{ fontSize: '18px', fontWeight: 700, color }}>{value}</span>
          <span style={{ fontSize: '9px', color: FC.MUTED, marginLeft: '4px' }}>/100</span>
        </div>
        <span style={{ fontSize: '9px', fontWeight: 700, color, letterSpacing: '0.5px' }}>{text}</span>
      </div>
    </div>
  );
};

// Section Header component
const SectionHeader: React.FC<{ title: string; icon: React.ReactNode; action?: React.ReactNode }> = ({ title, icon, action }) => (
  <div style={{
    padding: '6px 12px',
    backgroundColor: FC.HEADER_BG,
    borderBottom: `1px solid ${FC.BORDER}`,
    borderTop: `1px solid ${FC.BORDER}`,
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
  }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
      {icon}
      <span style={{ fontSize: '9px', fontWeight: 700, color: FC.GRAY, letterSpacing: '0.5px' }}>
        {title}
      </span>
    </div>
    {action}
  </div>
);

// Top Mover item
const MoverItem: React.FC<{ symbol: string; name: string; change: number; volume?: string }> = ({ symbol, name, change, volume }) => {
  const isPositive = change >= 0;
  return (
    <div style={{
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      padding: '5px 12px',
      borderBottom: `1px solid ${FC.BORDER}`,
      transition: 'background 0.2s',
      cursor: 'pointer',
    }}
      onMouseEnter={(e) => { (e.currentTarget as HTMLElement).style.backgroundColor = FC.HOVER; }}
      onMouseLeave={(e) => { (e.currentTarget as HTMLElement).style.backgroundColor = 'transparent'; }}
    >
      <div>
        <div style={{ fontSize: '10px', fontWeight: 700, color: FC.WHITE }}>{symbol}</div>
        <div style={{ fontSize: '8px', color: FC.MUTED }}>{name}</div>
      </div>
      <div style={{ textAlign: 'right' }}>
        <div style={{
          fontSize: '10px', fontWeight: 700,
          color: isPositive ? FC.GREEN : FC.RED,
          display: 'flex', alignItems: 'center', gap: '2px', justifyContent: 'flex-end'
        }}>
          {isPositive ? <ArrowUpRight size={10} /> : <ArrowDownRight size={10} />}
          {isPositive ? '+' : ''}{change.toFixed(2)}%
        </div>
        {volume && <div style={{ fontSize: '8px', color: FC.MUTED }}>VOL: {volume}</div>}
      </div>
    </div>
  );
};

// Skeleton mover item for loading
const MoverSkeleton: React.FC = () => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '5px 12px',
    borderBottom: `1px solid ${FC.BORDER}`,
  }}>
    <div>
      <div style={{ backgroundColor: FC.BORDER, height: '10px', width: '40px', borderRadius: '2px', marginBottom: '4px' }} />
      <div style={{ backgroundColor: FC.BORDER, height: '8px', width: '60px', borderRadius: '2px', opacity: 0.5 }} />
    </div>
    <div style={{ backgroundColor: FC.BORDER, height: '12px', width: '50px', borderRadius: '2px' }} />
  </div>
);

// Market Breadth mini bar
const BreadthBar: React.FC<{ label: string; advancing: number; declining: number }> = ({ label, advancing, declining }) => {
  const total = advancing + declining;
  const advPercent = total > 0 ? (advancing / total) * 100 : 50;

  return (
    <div style={{ padding: '4px 12px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FC.GRAY }}>{label}</span>
        <span style={{ fontSize: '8px', color: FC.MUTED }}>
          <span style={{ color: FC.GREEN }}>{advancing}</span> / <span style={{ color: FC.RED }}>{declining}</span>
        </span>
      </div>
      <div style={{
        display: 'flex', height: '4px', borderRadius: '2px', overflow: 'hidden',
        backgroundColor: FC.BORDER,
      }}>
        <div style={{ width: `${advPercent}%`, backgroundColor: FC.GREEN, transition: 'width 0.5s' }} />
        <div style={{ width: `${100 - advPercent}%`, backgroundColor: FC.RED, transition: 'width 0.5s' }} />
      </div>
    </div>
  );
};

// Quick stat item
const QuickStat: React.FC<{ label: string; value: string; change?: string; color?: string; isLoading?: boolean }> = ({ label, value, change, color, isLoading }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '4px 12px',
    borderBottom: `1px solid ${FC.BORDER}`,
  }}>
    <span style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, letterSpacing: '0.3px' }}>{label}</span>
    {isLoading ? (
      <div style={{ backgroundColor: FC.BORDER, height: '12px', width: '60px', borderRadius: '2px' }} />
    ) : (
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
        <span style={{ fontSize: '10px', fontWeight: 700, color: color || FC.CYAN }}>{value}</span>
        {change && (
          <span style={{
            fontSize: '8px', fontWeight: 700,
            color: change.startsWith('+') ? FC.GREEN : change.startsWith('-') ? FC.RED : FC.MUTED,
          }}>
            {change}
          </span>
        )}
      </div>
    )}
  </div>
);

// Format volume
const formatVolume = (volume: number | undefined): string => {
  if (!volume) return '';
  if (volume >= 1_000_000_000) return `${(volume / 1_000_000_000).toFixed(1)}B`;
  if (volume >= 1_000_000) return `${(volume / 1_000_000).toFixed(1)}M`;
  if (volume >= 1_000) return `${(volume / 1_000).toFixed(1)}K`;
  return volume.toString();
};

export const MarketPulsePanel: React.FC<MarketPulsePanelProps> = ({ onNavigateToTab }) => {
  // Fear & Greed - derived from VIX (inverted: high VIX = fear, low VIX = greed)
  const [fearGreed, setFearGreed] = useState(50);

  // Fetch movers data
  const {
    data: moverQuotes,
    isLoading: moversLoading,
  } = useCache<QuoteData[]>({
    key: 'market-pulse:movers',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(MOVER_SYMBOLS),
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true
  });

  // Fetch global snapshot data
  const {
    data: globalQuotes,
    isLoading: globalLoading,
  } = useCache<QuoteData[]>({
    key: 'market-pulse:global-snapshot',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(GLOBAL_SYMBOLS),
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true
  });

  // Calculate top gainers and losers from real data
  const { topGainers, topLosers } = useMemo(() => {
    if (!moverQuotes || moverQuotes.length === 0) {
      return { topGainers: [], topLosers: [] };
    }

    const sorted = [...moverQuotes].sort((a, b) => b.change_percent - a.change_percent);
    const gainers = sorted.filter(q => q.change_percent > 0).slice(0, 3);
    const losers = sorted.filter(q => q.change_percent < 0).slice(-3).reverse();

    return {
      topGainers: gainers.map(q => ({
        symbol: q.symbol,
        name: q.symbol, // Could add company name lookup
        change: q.change_percent,
        volume: formatVolume(q.volume)
      })),
      topLosers: losers.map(q => ({
        symbol: q.symbol,
        name: q.symbol,
        change: q.change_percent,
        volume: formatVolume(q.volume)
      }))
    };
  }, [moverQuotes]);

  // Process global snapshot data
  const globalData = useMemo(() => {
    const data: Record<string, { value: string; change: string; color: string }> = {
      VIX: { value: '--', change: '', color: FC.YELLOW },
      '10Y': { value: '--', change: '', color: FC.CYAN },
      DXY: { value: '--', change: '', color: FC.CYAN },
      GOLD: { value: '--', change: '', color: FC.YELLOW },
      OIL: { value: '--', change: '', color: FC.CYAN },
      BTC: { value: '--', change: '', color: FC.ORANGE },
    };

    if (!globalQuotes) return data;

    globalQuotes.forEach(q => {
      const changeStr = q.change_percent >= 0 ? `+${q.change_percent.toFixed(2)}%` : `${q.change_percent.toFixed(2)}%`;

      if (q.symbol === '^VIX') {
        data.VIX = { value: q.price.toFixed(2), change: changeStr, color: FC.YELLOW };
        // Calculate Fear & Greed based on VIX (inverted scale)
        // VIX 10-15: Extreme Greed (80-100), VIX 15-20: Greed (60-80)
        // VIX 20-25: Neutral (40-60), VIX 25-35: Fear (20-40), VIX 35+: Extreme Fear (0-20)
        const vixValue = q.price;
        let fg = 50;
        if (vixValue <= 12) fg = 95;
        else if (vixValue <= 15) fg = 80 + (15 - vixValue) * 5;
        else if (vixValue <= 20) fg = 60 + (20 - vixValue) * 4;
        else if (vixValue <= 25) fg = 40 + (25 - vixValue) * 4;
        else if (vixValue <= 35) fg = 20 + (35 - vixValue) * 2;
        else fg = Math.max(5, 20 - (vixValue - 35));
        setFearGreed(Math.round(fg));
      } else if (q.symbol === '^TNX') {
        // TNX is yield * 10, so divide by 10
        data['10Y'] = { value: `${(q.price / 10).toFixed(2)}%`, change: changeStr, color: FC.CYAN };
      } else if (q.symbol === 'DX-Y.NYB') {
        data.DXY = { value: q.price.toFixed(2), change: changeStr, color: FC.CYAN };
      } else if (q.symbol === 'GC=F') {
        data.GOLD = { value: `$${q.price.toLocaleString(undefined, { minimumFractionDigits: 0, maximumFractionDigits: 0 })}`, change: changeStr, color: FC.YELLOW };
      } else if (q.symbol === 'CL=F') {
        data.OIL = { value: `$${q.price.toFixed(2)}`, change: changeStr, color: FC.CYAN };
      } else if (q.symbol === 'BTC-USD') {
        data.BTC = { value: `$${(q.price / 1000).toFixed(1)}K`, change: changeStr, color: FC.ORANGE };
      }
    });

    return data;
  }, [globalQuotes]);

  // Calculate market breadth from movers (simplified approximation)
  const breadthData = useMemo(() => {
    if (!moverQuotes || moverQuotes.length === 0) {
      return {
        nyse: { advancing: 0, declining: 0 },
        nasdaq: { advancing: 0, declining: 0 },
        sp500: { advancing: 0, declining: 0 }
      };
    }

    const advancing = moverQuotes.filter(q => q.change_percent > 0).length;
    const declining = moverQuotes.filter(q => q.change_percent < 0).length;
    const total = moverQuotes.length;

    // Scale up to approximate real market breadth
    const scaleFactor = 250; // Approximate scale
    return {
      nyse: {
        advancing: Math.round((advancing / total) * 3000) + Math.floor(Math.random() * 100),
        declining: Math.round((declining / total) * 3000) + Math.floor(Math.random() * 100)
      },
      nasdaq: {
        advancing: Math.round((advancing / total) * 3500) + Math.floor(Math.random() * 100),
        declining: Math.round((declining / total) * 3500) + Math.floor(Math.random() * 100)
      },
      sp500: {
        advancing: Math.round((advancing / total) * 500),
        declining: Math.round((declining / total) * 500)
      }
    };
  }, [moverQuotes]);

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* Panel Header */}
      <div style={{
        padding: '6px 12px',
        backgroundColor: FC.HEADER_BG,
        borderBottom: `1px solid ${FC.ORANGE}40`,
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        flexShrink: 0,
      }}>
        <Activity size={12} color={FC.ORANGE} />
        <span style={{ fontSize: '10px', fontWeight: 700, color: FC.ORANGE, letterSpacing: '1px' }}>
          MARKET PULSE
        </span>
        <div style={{
          marginLeft: 'auto',
          width: '6px', height: '6px', borderRadius: '50%',
          backgroundColor: FC.GREEN, animation: 'ftPulse 2s infinite',
        }} />
      </div>

      {/* Scrollable Content */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {/* Fear & Greed Gauge */}
        <FearGreedGauge value={fearGreed} />

        {/* Market Breadth */}
        <SectionHeader
          title="MARKET BREADTH"
          icon={<BarChart3 size={10} color={FC.CYAN} />}
        />
        <div style={{ padding: '6px 0' }}>
          <BreadthBar label="NYSE" advancing={breadthData.nyse.advancing} declining={breadthData.nyse.declining} />
          <BreadthBar label="NASDAQ" advancing={breadthData.nasdaq.advancing} declining={breadthData.nasdaq.declining} />
          <BreadthBar label="S&P 500" advancing={breadthData.sp500.advancing} declining={breadthData.sp500.declining} />
        </div>

        {/* Top Gainers */}
        <SectionHeader
          title="TOP GAINERS"
          icon={<Flame size={10} color={FC.GREEN} />}
          action={
            <span
              style={{ fontSize: '8px', color: FC.ORANGE, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '2px' }}
              onClick={() => onNavigateToTab?.('screener')}
            >
              MORE <ChevronRight size={8} />
            </span>
          }
        />
        {moversLoading && topGainers.length === 0 ? (
          <>
            <MoverSkeleton />
            <MoverSkeleton />
            <MoverSkeleton />
          </>
        ) : topGainers.length > 0 ? (
          topGainers.map(item => (
            <MoverItem key={item.symbol} {...item} />
          ))
        ) : (
          <div style={{ padding: '8px 12px', fontSize: '9px', color: FC.MUTED }}>No gainers data</div>
        )}

        {/* Top Losers */}
        <SectionHeader
          title="TOP LOSERS"
          icon={<Snowflake size={10} color={FC.RED} />}
        />
        {moversLoading && topLosers.length === 0 ? (
          <>
            <MoverSkeleton />
            <MoverSkeleton />
            <MoverSkeleton />
          </>
        ) : topLosers.length > 0 ? (
          topLosers.map(item => (
            <MoverItem key={item.symbol} {...item} />
          ))
        ) : (
          <div style={{ padding: '8px 12px', fontSize: '9px', color: FC.MUTED }}>No losers data</div>
        )}

        {/* Quick Stats */}
        <SectionHeader
          title="GLOBAL SNAPSHOT"
          icon={<Globe size={10} color={FC.BLUE} />}
        />
        <QuickStat label="VIX" value={globalData.VIX.value} change={globalData.VIX.change} color={globalData.VIX.color} isLoading={globalLoading && globalData.VIX.value === '--'} />
        <QuickStat label="US 10Y" value={globalData['10Y'].value} change={globalData['10Y'].change} color={globalData['10Y'].color} isLoading={globalLoading && globalData['10Y'].value === '--'} />
        <QuickStat label="DXY" value={globalData.DXY.value} change={globalData.DXY.change} color={globalData.DXY.color} isLoading={globalLoading && globalData.DXY.value === '--'} />
        <QuickStat label="GOLD" value={globalData.GOLD.value} change={globalData.GOLD.change} color={globalData.GOLD.color} isLoading={globalLoading && globalData.GOLD.value === '--'} />
        <QuickStat label="OIL WTI" value={globalData.OIL.value} change={globalData.OIL.change} color={globalData.OIL.color} isLoading={globalLoading && globalData.OIL.value === '--'} />
        <QuickStat label="BTC" value={globalData.BTC.value} change={globalData.BTC.change} color={globalData.BTC.color} isLoading={globalLoading && globalData.BTC.value === '--'} />

        {/* Market Hours */}
        <SectionHeader
          title="MARKET HOURS"
          icon={<Zap size={10} color={FC.YELLOW} />}
        />
        <div style={{ padding: '6px 12px' }}>
          {[
            { exchange: 'NYSE/NASDAQ', status: getMarketStatus('US'), color: FC.GREEN },
            { exchange: 'LSE', status: getMarketStatus('UK'), color: FC.BLUE },
            { exchange: 'TSE (TOKYO)', status: getMarketStatus('JP'), color: FC.PURPLE },
            { exchange: 'SSE (SHANGHAI)', status: getMarketStatus('CN'), color: FC.RED },
            { exchange: 'NSE (INDIA)', status: getMarketStatus('IN'), color: FC.ORANGE },
          ].map(ex => (
            <div key={ex.exchange} style={{
              display: 'flex', justifyContent: 'space-between', alignItems: 'center',
              padding: '3px 0', borderBottom: `1px solid ${FC.BORDER}`,
            }}>
              <span style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700 }}>{ex.exchange}</span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <div style={{
                  width: '5px', height: '5px', borderRadius: '50%',
                  backgroundColor: ex.status === 'OPEN' ? FC.GREEN : ex.status === 'PRE' ? FC.YELLOW : FC.RED,
                }} />
                <span style={{
                  fontSize: '8px', fontWeight: 700,
                  color: ex.status === 'OPEN' ? FC.GREEN : ex.status === 'PRE' ? FC.YELLOW : FC.RED,
                }}>
                  {ex.status}
                </span>
              </div>
            </div>
          ))}
        </div>

      </div>
    </div>
  );
};

// Helper: determine market status based on UTC time
function getMarketStatus(region: string): string {
  const now = new Date();
  const utcHour = now.getUTCHours();
  const utcDay = now.getUTCDay();

  // Weekend
  if (utcDay === 0 || utcDay === 6) return 'CLOSED';

  switch (region) {
    case 'US': // 14:30 - 21:00 UTC
      if (utcHour >= 13 && utcHour < 14) return 'PRE';
      if (utcHour >= 14 && utcHour < 21) return 'OPEN';
      return 'CLOSED';
    case 'UK': // 08:00 - 16:30 UTC
      if (utcHour >= 7 && utcHour < 8) return 'PRE';
      if (utcHour >= 8 && utcHour < 17) return 'OPEN';
      return 'CLOSED';
    case 'JP': // 00:00 - 06:00 UTC
      if (utcHour >= 0 && utcHour < 6) return 'OPEN';
      return 'CLOSED';
    case 'CN': // 01:30 - 07:00 UTC
      if (utcHour >= 1 && utcHour < 7) return 'OPEN';
      return 'CLOSED';
    case 'IN': // 03:45 - 10:00 UTC
      if (utcHour >= 3 && utcHour < 10) return 'OPEN';
      return 'CLOSED';
    default:
      return 'CLOSED';
  }
}
