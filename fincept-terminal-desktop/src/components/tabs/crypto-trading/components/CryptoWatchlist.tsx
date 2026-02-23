// CryptoWatchlist.tsx - Professional Terminal-Style Left Sidebar Watchlist
import React from 'react';
import { Star, TrendingUp, TrendingDown } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { WatchlistPrice } from '../types';

interface CryptoWatchlistProps {
  watchlist: string[];
  watchlistPrices: Record<string, WatchlistPrice>;
  selectedSymbol: string;
  onSymbolSelect: (symbol: string) => void;
}

export function CryptoWatchlist({
  watchlist,
  watchlistPrices,
  selectedSymbol,
  onSymbolSelect,
}: CryptoWatchlistProps) {
  const formatPrice = (price: number) => {
    if (price >= 1000) return price.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    if (price >= 1) return price.toFixed(2);
    return price.toFixed(4);
  };

  return (
    <div style={{
      width: '260px',
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      fontFamily: '"IBM Plex Mono", monospace',
    }}>
      {/* Header */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
      }}>
        <Star size={12} color={FINCEPT.ORANGE} />
        <span style={{
          fontSize: '11px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          letterSpacing: '0.5px',
        }}>
          WATCHLIST
        </span>
      </div>

      {/* Column Headers */}
      <div style={{
        padding: '8px 10px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 600, letterSpacing: '0.5px' }}>
          SYMBOL
        </span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 600, letterSpacing: '0.5px' }}>
          PRICE / 24H
        </span>
      </div>

      {/* Watchlist Items */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {watchlist.map((symbol) => {
          const data = watchlistPrices[symbol];
          const isSelected = selectedSymbol === symbol;
          const isPositive = data && data.change >= 0;

          return (
            <div
              key={symbol}
              onClick={() => onSymbolSelect(symbol)}
              style={{
                padding: '8px 10px',
                cursor: 'pointer',
                borderBottom: `1px solid ${FINCEPT.BORDER}20`,
                borderLeftWidth: '3px',
                borderLeftStyle: 'solid',
                borderLeftColor: isSelected ? FINCEPT.ORANGE : 'transparent',
                backgroundColor: isSelected ? `${FINCEPT.ORANGE}10` : 'transparent',
                transition: 'all 0.15s',
              }}
              onMouseEnter={(e) => {
                if (!isSelected) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (!isSelected) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
              }}>
                {/* Symbol */}
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <div style={{
                    width: '24px',
                    height: '24px',
                    backgroundColor: `${isSelected ? FINCEPT.ORANGE : FINCEPT.GRAY}20`,
                    border: `1px solid ${isSelected ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '8px',
                    fontWeight: 700,
                    color: isSelected ? FINCEPT.ORANGE : FINCEPT.WHITE,
                  }}>
                    {symbol.replace('/USD', '').slice(0, 3)}
                  </div>
                  <div>
                    <div style={{
                      fontSize: '11px',
                      fontWeight: 600,
                      color: isSelected ? FINCEPT.ORANGE : FINCEPT.WHITE,
                    }}>
                      {symbol.replace('/USD', '')}
                    </div>
                    <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>
                      USD
                    </div>
                  </div>
                </div>

                {/* Price & Change */}
                {data ? (
                  <div style={{ textAlign: 'right' }}>
                    <div style={{
                      fontSize: '11px',
                      fontWeight: 600,
                      color: FINCEPT.WHITE,
                    }}>
                      ${formatPrice(data.price)}
                    </div>
                    <div style={{
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'flex-end',
                      gap: '3px',
                      fontSize: '9px',
                      fontWeight: 600,
                      color: isPositive ? FINCEPT.GREEN : FINCEPT.RED,
                    }}>
                      {isPositive ? <TrendingUp size={9} /> : <TrendingDown size={9} />}
                      {isPositive ? '+' : ''}{data.change.toFixed(2)}%
                    </div>
                  </div>
                ) : (
                  <div style={{
                    fontSize: '9px',
                    color: FINCEPT.GRAY,
                  }}>
                    Loading...
                  </div>
                )}
              </div>
            </div>
          );
        })}
      </div>

      {/* Footer Stats */}
      <div style={{
        padding: '8px 10px',
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
        fontSize: '9px',
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
          <span style={{ color: FINCEPT.GRAY }}>ASSETS</span>
          <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{watchlist.length}</span>
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
          <span style={{ color: FINCEPT.GRAY }}>UPDATED</span>
          <span style={{ color: FINCEPT.CYAN }}>{new Date().toLocaleTimeString()}</span>
        </div>
      </div>
    </div>
  );
}

export default CryptoWatchlist;
