// MarketTickerBar - Scrolling market data ticker strip
// Uses pure CSS animation for buttery-smooth 60fps scrolling on the compositor thread

import React, { useState, useEffect, useRef, useMemo } from 'react';
import { TrendingUp, TrendingDown } from 'lucide-react';
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
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  MUTED: '#4A4A4A',
};

// Major global tickers to display
const TICKER_SYMBOLS = [
  '^GSPC', '^DJI', '^IXIC', '^RUT', '^FTSE', '^GDAXI', '^N225', '^HSI',
  'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA', 'TSLA',
  'BTC-USD', 'ETH-USD',
  'GC=F', 'CL=F',
  'EURUSD=X', 'GBPUSD=X', 'USDJPY=X'
];

const TICKER_LABELS: Record<string, string> = {
  '^GSPC': 'S&P 500',
  '^DJI': 'DOW',
  '^IXIC': 'NASDAQ',
  '^RUT': 'RUSSELL',
  '^FTSE': 'FTSE',
  '^GDAXI': 'DAX',
  '^N225': 'NIKKEI',
  '^HSI': 'HANG SENG',
  'AAPL': 'AAPL',
  'MSFT': 'MSFT',
  'GOOGL': 'GOOGL',
  'AMZN': 'AMZN',
  'NVDA': 'NVDA',
  'TSLA': 'TSLA',
  'BTC-USD': 'BTC',
  'ETH-USD': 'ETH',
  'GC=F': 'GOLD',
  'CL=F': 'CRUDE',
  'EURUSD=X': 'EUR/USD',
  'GBPUSD=X': 'GBP/USD',
  'USDJPY=X': 'USD/JPY'
};

interface TickerItemData {
  symbol: string;
  label: string;
  price: number;
  change: number;
  changePercent: number;
}

// Generate demo data as fallback
const generateDemoData = (): TickerItemData[] => {
  return TICKER_SYMBOLS.map(sym => {
    const isPositive = Math.random() > 0.4;
    const basePrice = sym.includes('^') ? 10000 + Math.random() * 30000
      : sym.includes('BTC') ? 60000 + Math.random() * 10000
      : sym.includes('ETH') ? 3000 + Math.random() * 1000
      : sym.includes('GC') ? 2000 + Math.random() * 300
      : sym.includes('CL') ? 70 + Math.random() * 20
      : sym.includes('=X') ? 0.8 + Math.random() * 0.7
      : 100 + Math.random() * 400;
    const changeAmt = basePrice * (0.001 + Math.random() * 0.025) * (isPositive ? 1 : -1);
    return {
      symbol: sym,
      label: TICKER_LABELS[sym] || sym,
      price: basePrice,
      change: changeAmt,
      changePercent: (changeAmt / basePrice) * 100,
    };
  });
};

const TickerItem: React.FC<{ item: TickerItemData }> = React.memo(({ item }) => {
  const isPositive = item.change >= 0;
  const changeColor = isPositive ? FC.GREEN : FC.RED;

  return (
    <div style={{
      display: 'inline-flex',
      alignItems: 'center',
      gap: '6px',
      padding: '0 16px',
      whiteSpace: 'nowrap',
      borderRight: `1px solid ${FC.BORDER}`,
      height: '100%',
      flexShrink: 0,
    }}>
      <span style={{ color: FC.CYAN, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px' }}>
        {item.label}
      </span>
      <span style={{ color: FC.WHITE, fontSize: '10px', fontWeight: 700 }}>
        {item.price >= 1000 ? item.price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })
          : item.price.toFixed(item.price < 10 ? 4 : 2)}
      </span>
      <div style={{ display: 'flex', alignItems: 'center', gap: '2px' }}>
        {isPositive ? <TrendingUp size={8} color={changeColor} /> : <TrendingDown size={8} color={changeColor} />}
        <span style={{ color: changeColor, fontSize: '9px', fontWeight: 700 }}>
          {isPositive ? '+' : ''}{item.changePercent.toFixed(2)}%
        </span>
      </div>
    </div>
  );
});

export const MarketTickerBar: React.FC = () => {
  const [isPaused, setIsPaused] = useState(false);
  const trackRef = useRef<HTMLDivElement>(null);
  const [trackWidth, setTrackWidth] = useState(0);

  // Try fetching real data; fallback to demo
  const {
    data: quotes,
  } = useCache<QuoteData[]>({
    key: 'ticker-bar:major-quotes',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(TICKER_SYMBOLS),
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
  });

  const tickerData: TickerItemData[] = useMemo(() => {
    if (quotes && quotes.length > 0) {
      return quotes.map(q => ({
        symbol: q.symbol,
        label: TICKER_LABELS[q.symbol] || q.symbol,
        price: q.price,
        change: q.change,
        changePercent: q.change_percent,
      }));
    }
    return generateDemoData();
  }, [quotes]);

  // Measure one copy of the ticker track so we know how far to translate
  useEffect(() => {
    if (trackRef.current) {
      // Each child is one full set; measure the first half
      const fullWidth = trackRef.current.scrollWidth / 2;
      setTrackWidth(fullWidth);
    }
  }, [tickerData]);

  // Duration scales with content width for consistent perceived speed (~50px/s)
  const duration = trackWidth > 0 ? trackWidth / 50 : 40;

  return (
    <>
      {/* Inject the CSS keyframes dynamically based on measured width */}
      <style>{`
        @keyframes ftTickerScroll {
          0%   { transform: translateX(0); }
          100% { transform: translateX(-${trackWidth}px); }
        }
      `}</style>

      <div
        style={{
          backgroundColor: FC.PANEL_BG,
          borderBottom: `1px solid ${FC.BORDER}`,
          height: '24px',
          overflow: 'hidden',
          flexShrink: 0,
          position: 'relative',
          cursor: 'default',
        }}
        onMouseEnter={() => setIsPaused(true)}
        onMouseLeave={() => setIsPaused(false)}
      >
        {/* Left fade */}
        <div style={{
          position: 'absolute', left: 0, top: 0, bottom: 0, width: '40px',
          background: `linear-gradient(to right, ${FC.PANEL_BG}, transparent)`,
          zIndex: 2, pointerEvents: 'none',
        }} />
        {/* Right fade */}
        <div style={{
          position: 'absolute', right: 0, top: 0, bottom: 0, width: '40px',
          background: `linear-gradient(to left, ${FC.PANEL_BG}, transparent)`,
          zIndex: 2, pointerEvents: 'none',
        }} />

        {/* Scrolling track: two copies side-by-side, CSS-animated */}
        <div
          ref={trackRef}
          style={{
            display: 'flex',
            alignItems: 'center',
            height: '100%',
            width: 'max-content',
            animationName: trackWidth > 0 ? 'ftTickerScroll' : 'none',
            animationDuration: `${duration}s`,
            animationTimingFunction: 'linear',
            animationIterationCount: 'infinite',
            animationPlayState: isPaused ? 'paused' : 'running',
            willChange: 'transform',
          }}
        >
          {/* First copy */}
          {tickerData.map((item, idx) => (
            <TickerItem key={`a-${item.symbol}-${idx}`} item={item} />
          ))}
          {/* Second copy (seamless loop) */}
          {tickerData.map((item, idx) => (
            <TickerItem key={`b-${item.symbol}-${idx}`} item={item} />
          ))}
        </div>
      </div>
    </>
  );
};
