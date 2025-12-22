/**
 * FuturesPanel - Kraken futures trading interface
 * Bloomberg Terminal Style
 */

import React, { useState, useCallback, useEffect } from 'react';
import { TrendingUp, Activity, AlertTriangle, ExternalLink, Loader } from 'lucide-react';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface FuturesMarket {
  symbol: string;
  baseAsset: string;
  quoteAsset: string;
  price: number;
  change24h: number;
  volume24h: number;
  openInterest: number;
  fundingRate: number;
  nextFunding: string;
  maxLeverage: number;
}

export function FuturesPanel() {
  const { activeBroker, activeAdapter } = useBrokerContext();
  const [futuresMarkets, setFuturesMarkets] = useState<FuturesMarket[]>([]);
  const [selectedMarket, setSelectedMarket] = useState<FuturesMarket | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (activeBroker === 'kraken') {
      fetchFuturesMarkets();
    }
  }, [activeBroker]);

  const fetchFuturesMarkets = useCallback(async () => {
    setIsLoading(true);
    setError(null);
    try {
      const mockMarkets: FuturesMarket[] = [
        { symbol: 'PF_BTCUSD', baseAsset: 'BTC', quoteAsset: 'USD', price: 43250.5, change24h: 2.35, volume24h: 1234567890, openInterest: 567890000, fundingRate: 0.0001, nextFunding: '4h 23m', maxLeverage: 50 },
        { symbol: 'PF_ETHUSD', baseAsset: 'ETH', quoteAsset: 'USD', price: 2345.67, change24h: -1.23, volume24h: 456789012, openInterest: 234567000, fundingRate: 0.00005, nextFunding: '4h 23m', maxLeverage: 50 },
        { symbol: 'PF_SOLUSD', baseAsset: 'SOL', quoteAsset: 'USD', price: 98.45, change24h: 5.67, volume24h: 123456789, openInterest: 45678900, fundingRate: 0.00015, nextFunding: '4h 23m', maxLeverage: 25 },
        { symbol: 'PF_XRPUSD', baseAsset: 'XRP', quoteAsset: 'USD', price: 0.5234, change24h: 3.45, volume24h: 89012345, openInterest: 23456789, fundingRate: -0.00002, nextFunding: '4h 23m', maxLeverage: 20 },
      ];

      setFuturesMarkets(mockMarkets);
      if (mockMarkets.length > 0 && !selectedMarket) {
        setSelectedMarket(mockMarkets[0]);
      }
    } catch (err) {
      setError('Failed to fetch futures markets');
      console.error('Futures fetch error:', err);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, selectedMarket]);

  const formatNumber = (num: number, decimals: number = 2): string => {
    return num.toLocaleString('en-US', { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
  };

  const formatLargeNumber = (num: number): string => {
    if (num >= 1e9) return `$${(num / 1e9).toFixed(2)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(2)}M`;
    if (num >= 1e3) return `$${(num / 1e3).toFixed(2)}K`;
    return `$${num.toFixed(2)}`;
  };

  if (activeBroker !== 'kraken') {
    return null;
  }

  if (isLoading) {
    return (
      <div style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '4px', padding: '24px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px 0' }}>
          <Loader style={{ width: 24, height: 24, color: BLOOMBERG.BLUE, animation: 'spin 1s linear infinite' }} />
        </div>
      </div>
    );
  }

  return (
    <div style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '4px', padding: '16px', height: '100%', overflow: 'auto' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px', paddingBottom: '12px', borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Activity style={{ width: 18, height: 18, color: BLOOMBERG.PURPLE }} />
          <span style={{ fontSize: '13px', fontWeight: 600, color: BLOOMBERG.WHITE, letterSpacing: '0.5px' }}>KRAKEN FUTURES</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <button onClick={fetchFuturesMarkets}
            style={{ padding: '6px 12px', fontSize: '10px', color: BLOOMBERG.CYAN, backgroundColor: 'transparent', border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s' }}
            onMouseEnter={(e) => { e.currentTarget.style.borderColor = BLOOMBERG.CYAN; e.currentTarget.style.backgroundColor = `${BLOOMBERG.CYAN}15`; }}
            onMouseLeave={(e) => { e.currentTarget.style.borderColor = BLOOMBERG.BORDER; e.currentTarget.style.backgroundColor = 'transparent'; }}>
            REFRESH
          </button>
          <a href="https://futures.kraken.com" target="_blank" rel="noopener noreferrer"
            style={{ padding: '6px 12px', fontSize: '10px', color: BLOOMBERG.GRAY, backgroundColor: 'transparent', border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px', textDecoration: 'none', display: 'flex', alignItems: 'center', gap: '6px', transition: 'all 0.2s' }}
            onMouseEnter={(e) => { e.currentTarget.style.borderColor = BLOOMBERG.MUTED; e.currentTarget.style.color = BLOOMBERG.WHITE; }}
            onMouseLeave={(e) => { e.currentTarget.style.borderColor = BLOOMBERG.BORDER; e.currentTarget.style.color = BLOOMBERG.GRAY; }}>
            <ExternalLink style={{ width: 12, height: 12 }} />
            KRAKEN FUTURES
          </a>
        </div>
      </div>

      {/* Warning Banner */}
      <div style={{ marginBottom: '16px', padding: '10px', backgroundColor: `${BLOOMBERG.YELLOW}08`, border: `1px solid ${BLOOMBERG.YELLOW}25`, borderRadius: '2px', display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
        <AlertTriangle style={{ width: 14, height: 14, color: BLOOMBERG.YELLOW, flexShrink: 0, marginTop: '2px' }} />
        <div style={{ fontSize: '10px', color: BLOOMBERG.YELLOW, lineHeight: '1.5' }}>
          <strong>FUTURES TRADING RISK WARNING:</strong> Futures trading involves high risk and leverage. Only trade with funds you can afford to lose. Requires separate Kraken Futures account.
        </div>
      </div>

      {/* Markets Grid */}
      {futuresMarkets.length === 0 ? (
        <div style={{ textAlign: 'center', padding: '40px 0', color: BLOOMBERG.MUTED }}>
          <Activity style={{ width: 48, height: 48, margin: '0 auto 12px', color: BLOOMBERG.MUTED }} />
          <div style={{ fontSize: '11px' }}>NO FUTURES MARKETS AVAILABLE</div>
        </div>
      ) : (
        <>
          {/* Market Selector */}
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '8px', marginBottom: '16px' }}>
            {futuresMarkets.map((market) => (
              <div key={market.symbol} onClick={() => setSelectedMarket(market)}
                style={{ padding: '10px', borderRadius: '2px', border: `2px solid ${selectedMarket?.symbol === market.symbol ? BLOOMBERG.PURPLE : BLOOMBERG.BORDER}`, backgroundColor: selectedMarket?.symbol === market.symbol ? `${BLOOMBERG.PURPLE}10` : BLOOMBERG.HEADER_BG, cursor: 'pointer', transition: 'all 0.2s' }}
                onMouseEnter={(e) => { if (selectedMarket?.symbol !== market.symbol) e.currentTarget.style.borderColor = BLOOMBERG.MUTED; }}
                onMouseLeave={(e) => { if (selectedMarket?.symbol !== market.symbol) e.currentTarget.style.borderColor = BLOOMBERG.BORDER; }}>
                <div style={{ marginBottom: '4px', color: BLOOMBERG.WHITE, fontWeight: 700, fontSize: '11px', fontFamily: 'monospace' }}>{market.baseAsset}/USD</div>
                <div style={{ fontSize: '11px', color: BLOOMBERG.GRAY, marginBottom: '2px', fontFamily: 'monospace' }}>${formatNumber(market.price)}</div>
                <div style={{ fontSize: '10px', fontWeight: 700, color: market.change24h >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
                  {market.change24h >= 0 ? '+' : ''}{market.change24h.toFixed(2)}%
                </div>
              </div>
            ))}
          </div>

          {/* Selected Market Details */}
          {selectedMarket && (
            <div style={{ borderTop: `1px solid ${BLOOMBERG.BORDER}`, paddingTop: '16px' }}>
              {/* Market Stats */}
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '16px' }}>
                {/* Price */}
                <div style={{ padding: '12px', backgroundColor: BLOOMBERG.HEADER_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px' }}>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>MARK PRICE</div>
                  <div style={{ fontSize: '18px', fontWeight: 700, color: BLOOMBERG.WHITE, fontFamily: 'monospace', marginBottom: '4px' }}>
                    ${formatNumber(selectedMarket.price)}
                  </div>
                  <div style={{ fontSize: '11px', fontWeight: 700, color: selectedMarket.change24h >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
                    {selectedMarket.change24h >= 0 ? '+' : ''}{selectedMarket.change24h.toFixed(2)}% 24h
                  </div>
                </div>

                {/* Volume */}
                <div style={{ padding: '12px', backgroundColor: BLOOMBERG.HEADER_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px' }}>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>24H VOLUME</div>
                  <div style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.WHITE, fontFamily: 'monospace', marginBottom: '2px' }}>
                    {formatLargeNumber(selectedMarket.volume24h)}
                  </div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.MUTED }}>TRADING VOLUME</div>
                </div>

                {/* Open Interest */}
                <div style={{ padding: '12px', backgroundColor: BLOOMBERG.HEADER_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px' }}>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>OPEN INTEREST</div>
                  <div style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.WHITE, fontFamily: 'monospace', marginBottom: '2px' }}>
                    {formatLargeNumber(selectedMarket.openInterest)}
                  </div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.MUTED }}>TOTAL POSITIONS</div>
                </div>

                {/* Funding Rate */}
                <div style={{ padding: '12px', backgroundColor: BLOOMBERG.HEADER_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px' }}>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>FUNDING RATE</div>
                  <div style={{ fontSize: '16px', fontWeight: 700, color: selectedMarket.fundingRate >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED, fontFamily: 'monospace', marginBottom: '2px' }}>
                    {(selectedMarket.fundingRate * 100).toFixed(4)}%
                  </div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.MUTED }}>EVERY 8 HOURS</div>
                </div>

                {/* Next Funding */}
                <div style={{ padding: '12px', backgroundColor: BLOOMBERG.HEADER_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px' }}>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>NEXT FUNDING</div>
                  <div style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.WHITE, fontFamily: 'monospace', marginBottom: '2px' }}>
                    {selectedMarket.nextFunding}
                  </div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.MUTED }}>COUNTDOWN</div>
                </div>

                {/* Max Leverage */}
                <div style={{ padding: '12px', backgroundColor: BLOOMBERG.HEADER_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px' }}>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>MAX LEVERAGE</div>
                  <div style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.ORANGE, fontFamily: 'monospace', marginBottom: '2px' }}>
                    {selectedMarket.maxLeverage}x
                  </div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.MUTED }}>AVAILABLE</div>
                </div>
              </div>

              {/* Contract Details */}
              <div style={{ padding: '12px', backgroundColor: `${BLOOMBERG.CYAN}08`, border: `1px solid ${BLOOMBERG.CYAN}25`, borderRadius: '2px', marginBottom: '12px' }}>
                <div style={{ fontSize: '11px', fontWeight: 700, color: BLOOMBERG.CYAN, marginBottom: '8px', letterSpacing: '0.5px' }}>CONTRACT DETAILS</div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '10px' }}>
                  <div>
                    <span style={{ color: BLOOMBERG.GRAY }}>CONTRACT TYPE:</span>
                    <span style={{ color: BLOOMBERG.WHITE, marginLeft: '8px', fontWeight: 600 }}>Perpetual</span>
                  </div>
                  <div>
                    <span style={{ color: BLOOMBERG.GRAY }}>SETTLEMENT:</span>
                    <span style={{ color: BLOOMBERG.WHITE, marginLeft: '8px', fontWeight: 600 }}>USD</span>
                  </div>
                  <div>
                    <span style={{ color: BLOOMBERG.GRAY }}>CONTRACT SIZE:</span>
                    <span style={{ color: BLOOMBERG.WHITE, marginLeft: '8px', fontWeight: 600 }}>1 {selectedMarket.baseAsset}</span>
                  </div>
                  <div>
                    <span style={{ color: BLOOMBERG.GRAY }}>MIN ORDER:</span>
                    <span style={{ color: BLOOMBERG.WHITE, marginLeft: '8px', fontWeight: 600 }}>0.0001 {selectedMarket.baseAsset}</span>
                  </div>
                </div>
              </div>

              {/* Trading Info */}
              <div style={{ padding: '12px', backgroundColor: BLOOMBERG.HEADER_BG, border: `1px solid ${BLOOMBERG.BORDER}`, borderRadius: '2px', marginBottom: '12px' }}>
                <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, lineHeight: '1.6' }}>
                  <div style={{ marginBottom: '8px' }}>
                    <strong style={{ color: BLOOMBERG.WHITE }}>HOW TO TRADE:</strong> Futures trading on Kraken requires a separate Kraken Futures account. Visit{' '}
                    <a href="https://futures.kraken.com" target="_blank" rel="noopener noreferrer"
                      style={{ color: BLOOMBERG.CYAN, textDecoration: 'underline' }}>futures.kraken.com</a>{' '}
                    to create an account and start trading.
                  </div>
                  <div>
                    <strong style={{ color: BLOOMBERG.WHITE }}>API INTEGRATION:</strong> To trade futures directly from this terminal, configure Kraken Futures API credentials in Settings.
                  </div>
                </div>
              </div>

              {/* Action Button */}
              <a href={`https://futures.kraken.com/trade/${selectedMarket.symbol}`} target="_blank" rel="noopener noreferrer"
                style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px', padding: '12px 16px', backgroundColor: BLOOMBERG.PURPLE, color: BLOOMBERG.WHITE, fontSize: '11px', fontWeight: 700, border: 'none', borderRadius: '2px', textDecoration: 'none', transition: 'all 0.2s', letterSpacing: '0.5px' }}
                onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = '#B565FF'; }}
                onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = BLOOMBERG.PURPLE; }}>
                <ExternalLink style={{ width: 14, height: 14 }} />
                TRADE ON KRAKEN FUTURES
              </a>
            </div>
          )}
        </>
      )}
    </div>
  );
}
