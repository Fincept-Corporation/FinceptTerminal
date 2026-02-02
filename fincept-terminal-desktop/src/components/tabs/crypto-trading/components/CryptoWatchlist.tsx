// CryptoWatchlist.tsx - Professional Terminal-Style Left Sidebar Watchlist
import React from 'react';
import { Star, Bot, Trophy, TrendingUp, TrendingDown } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { WatchlistPrice, LeftSidebarViewType, Position } from '../types';
import { AIAgentsPanel } from '../../trading/ai-agents/AIAgentsPanel';
import { LeaderboardPanel } from '../../trading/ai-agents/LeaderboardPanel';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

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
      {/* Tab Header - Terminal style */}
      <div style={{
        display: 'flex',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
      }}>
        {[
          { id: 'watchlist' as LeftSidebarViewType, label: 'WATCH', icon: <Star size={10} /> },
          { id: 'ai-agents' as LeftSidebarViewType, label: 'AGENTS', icon: <Bot size={10} /> },
          { id: 'leaderboard' as LeftSidebarViewType, label: 'RANK', icon: <Trophy size={10} /> },
        ].map((tab) => {
          const isActive = leftSidebarView === tab.id;
          return (
            <button
              key={tab.id}
              onClick={() => onViewChange(tab.id)}
              style={{
                flex: 1,
                padding: '8px 6px',
                backgroundColor: isActive ? FINCEPT.ORANGE : 'transparent',
                border: 'none',
                borderBottom: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                color: isActive ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: 700,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '4px',
                transition: 'all 0.15s',
              }}
              onMouseEnter={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  e.currentTarget.style.color = FINCEPT.WHITE;
                }
              }}
              onMouseLeave={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }
              }}
            >
              {tab.icon}
              {tab.label}
            </button>
          );
        })}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        {leftSidebarView === 'watchlist' && (
          <>
            {/* Watchlist Header */}
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
                      borderLeft: isSelected ? `3px solid ${FINCEPT.ORANGE}` : '3px solid transparent',
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
          </>
        )}

        {leftSidebarView === 'ai-agents' && (
          <div style={{ flex: 1, overflow: 'auto' }}>
            <ErrorBoundary name="AIAgentsPanel" variant="minimal">
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
            </ErrorBoundary>
          </div>
        )}

        {leftSidebarView === 'leaderboard' && (
          <div style={{ flex: 1, overflow: 'hidden' }}>
            <ErrorBoundary name="LeaderboardPanel" variant="minimal">
              <LeaderboardPanel refreshInterval={10000} />
            </ErrorBoundary>
          </div>
        )}
      </div>
    </div>
  );
}

export default CryptoWatchlist;
