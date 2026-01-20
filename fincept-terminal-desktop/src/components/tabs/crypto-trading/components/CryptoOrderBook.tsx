// CryptoOrderBook.tsx - Right Sidebar Order Book for Crypto Trading
import React from 'react';
import { FINCEPT } from '../constants';
import type { OrderBook, RightPanelViewType } from '../types';
import { VolumeProfile } from '../../trading/charts';
import { ModelChatPanel } from '../../trading/ai-agents/ModelChatPanel';

interface CryptoOrderBookProps {
  orderBook: OrderBook;
  currentPrice: number;
  selectedSymbol: string;
  rightPanelView: RightPanelViewType;
  lastOrderBookUpdate: number;
  onViewChange: (view: RightPanelViewType) => void;
}

export function CryptoOrderBook({
  orderBook,
  currentPrice,
  selectedSymbol,
  rightPanelView,
  lastOrderBookUpdate,
  onViewChange,
}: CryptoOrderBookProps) {
  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      <div style={{
        padding: '8px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '10px',
        fontWeight: 700,
        color: FINCEPT.ORANGE,
        letterSpacing: '0.5px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', gap: '8px' }}>
          {(['orderbook', 'volume', 'modelchat'] as RightPanelViewType[]).map((view) => (
            <button
              key={view}
              onClick={() => onViewChange(view)}
              style={{
                padding: '4px 8px',
                backgroundColor: rightPanelView === view ? FINCEPT.ORANGE : 'transparent',
                border: 'none',
                color: rightPanelView === view ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: 700,
                transition: 'all 0.2s'
              }}
            >
              {view.toUpperCase()}
            </button>
          ))}
        </div>
        {rightPanelView === 'orderbook' && (
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY }}>
            {orderBook.asks.length + orderBook.bids.length} LEVELS
          </span>
        )}
      </div>
      <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
        {rightPanelView === 'orderbook' && (
          <>
            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: FINCEPT.RED, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                ASKS ({orderBook.asks.length})
              </div>
              <table style={{ width: '100%', fontSize: '10px' }}>
                <tbody>
                  {orderBook.asks.slice(0, 12).reverse().map((ask, idx) => {
                    const depth = (ask.size / Math.max(...orderBook.asks.map(a => a.size))) * 100;
                    return (
                      <tr key={idx} style={{ position: 'relative' }}>
                        <td style={{
                          padding: '2px 4px',
                          color: FINCEPT.RED,
                          position: 'relative',
                          zIndex: 1
                        }}>
                          <div style={{
                            position: 'absolute',
                            right: 0,
                            top: 0,
                            bottom: 0,
                            width: `${depth}%`,
                            backgroundColor: `${FINCEPT.RED}15`,
                            zIndex: -1
                          }} />
                          ${ask.price.toFixed(2)}
                        </td>
                        <td style={{ padding: '2px 4px', textAlign: 'right', color: FINCEPT.GRAY }}>
                          {ask.size.toFixed(4)}
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>

            <div style={{
              height: '1px',
              backgroundColor: FINCEPT.BORDER,
              margin: '8px 0',
              position: 'relative'
            }}>
              <div style={{
                position: 'absolute',
                left: '50%',
                top: '50%',
                transform: 'translate(-50%, -50%)',
                backgroundColor: FINCEPT.PANEL_BG,
                padding: '0 8px',
                fontSize: '11px',
                fontWeight: 700,
                color: FINCEPT.YELLOW
              }}>
                ${currentPrice.toFixed(2)}
              </div>
            </div>

            <div>
              <div style={{ color: FINCEPT.GREEN, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                BIDS ({orderBook.bids.length})
              </div>
              <table style={{ width: '100%', fontSize: '10px' }}>
                <tbody>
                  {orderBook.bids.slice(0, 12).map((bid, idx) => {
                    const depth = (bid.size / Math.max(...orderBook.bids.map(b => b.size))) * 100;
                    return (
                      <tr key={idx} style={{ position: 'relative' }}>
                        <td style={{
                          padding: '2px 4px',
                          color: FINCEPT.GREEN,
                          position: 'relative',
                          zIndex: 1
                        }}>
                          <div style={{
                            position: 'absolute',
                            right: 0,
                            top: 0,
                            bottom: 0,
                            width: `${depth}%`,
                            backgroundColor: `${FINCEPT.GREEN}15`,
                            zIndex: -1
                          }} />
                          ${bid.price.toFixed(2)}
                        </td>
                        <td style={{ padding: '2px 4px', textAlign: 'right', color: FINCEPT.GRAY }}>
                          {bid.size.toFixed(4)}
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
          </>
        )}

        {rightPanelView === 'volume' && (
          <VolumeProfile symbol={selectedSymbol} height={550} />
        )}

        {rightPanelView === 'modelchat' && (
          <ModelChatPanel refreshInterval={5000} />
        )}
      </div>
      <div style={{
        padding: '4px 8px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '8px',
        color: FINCEPT.GRAY
      }}>
        Updated: {lastOrderBookUpdate > 0 ? new Date(lastOrderBookUpdate).toLocaleTimeString() : 'Never'}
      </div>
    </div>
  );
}

export default CryptoOrderBook;
