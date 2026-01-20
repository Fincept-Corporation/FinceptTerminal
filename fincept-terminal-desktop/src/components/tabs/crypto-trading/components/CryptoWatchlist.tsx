// CryptoWatchlist.tsx - Left Sidebar Watchlist for Crypto Trading
import React from 'react';
import { FINCEPT } from '../constants';
import type { WatchlistPrice, LeftSidebarViewType } from '../types';
import { AIAgentsPanel } from '../../trading/ai-agents/AIAgentsPanel';
import { LeaderboardPanel } from '../../trading/ai-agents/LeaderboardPanel';
import type { Position } from '../types';

interface CryptoWatchlistProps {
  watchlist: string[];
  watchlistPrices: Record<string, WatchlistPrice>;
  selectedSymbol: string;
  leftSidebarView: LeftSidebarViewType;
  positions: Position[];
  equity: number;
  onSymbolSelect: (symbol: string) => void;
  onViewChange: (view: LeftSidebarViewType) => void;
}

export function CryptoWatchlist({
  watchlist,
  watchlistPrices,
  selectedSymbol,
  leftSidebarView,
  positions,
  equity,
  onSymbolSelect,
  onViewChange,
}: CryptoWatchlistProps) {
  return (
    <div style={{
      width: '320px',
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Toggle Header */}
      <div style={{
        padding: '6px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        gap: '4px'
      }}>
        <button
          onClick={() => onViewChange('watchlist')}
          style={{
            flex: 1,
            padding: '6px 8px',
            backgroundColor: leftSidebarView === 'watchlist' ? FINCEPT.ORANGE : 'transparent',
            border: 'none',
            color: leftSidebarView === 'watchlist' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
            cursor: 'pointer',
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            transition: 'all 0.2s',
            borderRadius: '2px'
          }}
        >
          WATCH
        </button>
        <button
          onClick={() => onViewChange('ai-agents')}
          style={{
            flex: 1,
            padding: '6px 8px',
            backgroundColor: leftSidebarView === 'ai-agents' ? FINCEPT.ORANGE : 'transparent',
            border: 'none',
            color: leftSidebarView === 'ai-agents' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
            cursor: 'pointer',
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            transition: 'all 0.2s',
            borderRadius: '2px'
          }}
        >
          AGENTS
        </button>
        <button
          onClick={() => onViewChange('leaderboard')}
          style={{
            flex: 1,
            padding: '6px 8px',
            backgroundColor: leftSidebarView === 'leaderboard' ? FINCEPT.ORANGE : 'transparent',
            border: 'none',
            color: leftSidebarView === 'leaderboard' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
            cursor: 'pointer',
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            transition: 'all 0.2s',
            borderRadius: '2px'
          }}
        >
          LEADER
        </button>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        {leftSidebarView === 'watchlist' && (
          <div style={{
            flex: 1,
            overflow: 'auto',
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: '0',
            alignContent: 'start'
          }}>
            {watchlist.map((symbol, idx) => (
              <div
                key={symbol}
                onClick={() => onSymbolSelect(symbol)}
                style={{
                  padding: '8px 10px',
                  cursor: 'pointer',
                  backgroundColor: selectedSymbol === symbol ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: selectedSymbol === symbol ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  borderRight: idx % 2 === 0 ? `1px solid ${FINCEPT.BORDER}` : 'none',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (selectedSymbol !== symbol) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }
                }}
                onMouseLeave={(e) => {
                  if (selectedSymbol !== symbol) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                {/* Flex container: Ticker on left, Price/Change on right */}
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  gap: '8px'
                }}>
                  {/* Left side: Ticker symbol */}
                  <div style={{
                    fontSize: '11px',
                    fontWeight: 700,
                    color: selectedSymbol === symbol ? FINCEPT.ORANGE : FINCEPT.WHITE,
                    whiteSpace: 'nowrap'
                  }}>
                    {symbol.replace('/USD', '')}
                  </div>

                  {/* Right side: Price and Change */}
                  {watchlistPrices[symbol] ? (
                    <div style={{
                      display: 'flex',
                      flexDirection: 'column',
                      alignItems: 'flex-end',
                      gap: '2px'
                    }}>
                      <div style={{
                        fontSize: '10px',
                        color: FINCEPT.WHITE,
                        fontFamily: 'monospace',
                        fontWeight: 600
                      }}>
                        ${watchlistPrices[symbol].price >= 1000
                          ? watchlistPrices[symbol].price.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })
                          : watchlistPrices[symbol].price >= 1
                            ? watchlistPrices[symbol].price.toFixed(2)
                            : watchlistPrices[symbol].price.toFixed(4)
                        }
                      </div>
                      <div style={{
                        fontSize: '9px',
                        color: watchlistPrices[symbol].change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                        fontFamily: 'monospace',
                        fontWeight: 700
                      }}>
                        {watchlistPrices[symbol].change >= 0 ? '▲' : '▼'} {Math.abs(watchlistPrices[symbol].change).toFixed(2)}%
                      </div>
                    </div>
                  ) : (
                    <div style={{
                      fontSize: '9px',
                      color: FINCEPT.GRAY,
                      fontFamily: 'monospace'
                    }}>
                      ...
                    </div>
                  )}
                </div>
              </div>
            ))}
          </div>
        )}

        {leftSidebarView === 'ai-agents' && (
          <div style={{ flex: 1, overflow: 'auto' }}>
            <AIAgentsPanel
              selectedSymbol={selectedSymbol}
              portfolioData={{
                positions: positions.map(p => ({
                  symbol: p.symbol,
                  quantity: p.quantity,
                  entry_price: p.entryPrice,
                  current_price: p.currentPrice,
                  value: p.positionValue
                })),
                total_value: equity
              }}
            />
          </div>
        )}

        {leftSidebarView === 'leaderboard' && (
          <div style={{ flex: 1, overflow: 'hidden' }}>
            <LeaderboardPanel refreshInterval={10000} />
          </div>
        )}
      </div>
    </div>
  );
}

export default CryptoWatchlist;
