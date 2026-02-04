/**
 * EquityOrderBook - Right sidebar order book, market depth, and symbol info
 */
import React from 'react';
import { Layers, Loader2 } from 'lucide-react';
import { FundsPanel } from './FundsPanel';
import { FINCEPT } from '../constants';
import type { EquityOrderBookProps, RightPanelView } from '../types';

export const EquityOrderBook: React.FC<EquityOrderBookProps> = ({
  rightPanelView,
  marketDepth,
  quote,
  selectedSymbol,
  selectedExchange,
  isAuthenticated,
  isLoadingQuote,
  spread,
  spreadPercent,
  onViewChange,
  fmtPrice,
}) => {
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
          {(['orderbook', 'marketdepth', 'info'] as RightPanelView[]).map((view) => (
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
                transition: 'all 0.15s'
              }}
            >
              {view === 'orderbook' ? 'BOOK' : view === 'marketdepth' ? 'DEPTH' : 'INFO'}
            </button>
          ))}
        </div>
        {/* Connection indicator */}
        <div style={{
          width: '6px',
          height: '6px',
          borderRadius: '50%',
          backgroundColor: isAuthenticated
            ? (isLoadingQuote ? FINCEPT.YELLOW : FINCEPT.GREEN)
            : FINCEPT.RED,
          boxShadow: isAuthenticated && !isLoadingQuote
            ? `0 0 4px ${FINCEPT.GREEN}`
            : isLoadingQuote
              ? `0 0 4px ${FINCEPT.YELLOW}`
              : `0 0 4px ${FINCEPT.RED}`,
        }} />
      </div>

      <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
        {rightPanelView === 'orderbook' && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '8px',
            height: '100%'
          }}>
            {/* Buy Orders (Bids) */}
            <div>
              <div style={{ color: FINCEPT.GREEN, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                BIDS (BUY ORDERS)
              </div>
              <table style={{ width: '100%', fontSize: '10px' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    <th style={{ padding: '4px', textAlign: 'left', color: FINCEPT.GRAY }}>ORDERS</th>
                    <th style={{ padding: '4px', textAlign: 'center', color: FINCEPT.GRAY }}>QTY</th>
                    <th style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>PRICE</th>
                  </tr>
                </thead>
                <tbody>
                  {(marketDepth?.bids && marketDepth.bids.length > 0
                    ? marketDepth.bids.slice(0, 5)
                    : [null, null, null, null, null]
                  ).map((bid, i) => (
                    <tr key={i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                      <td style={{ padding: '4px', color: FINCEPT.GRAY }}>
                        {bid?.orders ?? '--'}
                      </td>
                      <td style={{ padding: '4px', textAlign: 'center', color: FINCEPT.WHITE }}>
                        {bid?.quantity ? bid.quantity.toLocaleString('en-IN') : '--'}
                      </td>
                      <td style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GREEN }}>
                        {bid?.price ? fmtPrice(bid.price) : '--'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>

            {/* Spread Divider */}
            <div style={{
              padding: '8px',
              textAlign: 'center',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px'
            }}>
              <span style={{ color: FINCEPT.GRAY }}>Spread: </span>
              <span style={{ color: FINCEPT.YELLOW, fontWeight: 600 }}>
                {spread > 0 ? `${fmtPrice(spread)} (${spreadPercent.toFixed(3)}%)` : '--'}
              </span>
            </div>

            {/* Sell Orders (Asks) */}
            <div>
              <div style={{ color: FINCEPT.RED, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                ASKS (SELL ORDERS)
              </div>
              <table style={{ width: '100%', fontSize: '10px' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    <th style={{ padding: '4px', textAlign: 'left', color: FINCEPT.GRAY }}>PRICE</th>
                    <th style={{ padding: '4px', textAlign: 'center', color: FINCEPT.GRAY }}>QTY</th>
                    <th style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>ORDERS</th>
                  </tr>
                </thead>
                <tbody>
                  {(marketDepth?.asks && marketDepth.asks.length > 0
                    ? marketDepth.asks.slice(0, 5)
                    : [null, null, null, null, null]
                  ).map((ask, i) => (
                    <tr key={i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                      <td style={{ padding: '4px', color: FINCEPT.RED }}>
                        {ask?.price ? fmtPrice(ask.price) : '--'}
                      </td>
                      <td style={{ padding: '4px', textAlign: 'center', color: FINCEPT.WHITE }}>
                        {ask?.quantity ? ask.quantity.toLocaleString('en-IN') : '--'}
                      </td>
                      <td style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>
                        {ask?.orders ?? '--'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {rightPanelView === 'marketdepth' && (
          <div style={{
            height: '100%',
            display: 'flex',
            flexDirection: 'column',
            gap: '8px'
          }}>
            {marketDepth && (marketDepth.bids?.length > 0 || marketDepth.asks?.length > 0) ? (
              <>
                <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  {/* Bid depth bars */}
                  <div style={{ flex: 1, display: 'flex', flexDirection: 'column', justifyContent: 'flex-end', gap: '2px' }}>
                    {marketDepth.bids?.slice(0, 5).reverse().map((bid, i) => {
                      const maxQty = Math.max(
                        ...marketDepth.bids.slice(0, 5).map(b => b.quantity),
                        ...marketDepth.asks.slice(0, 5).map(a => a.quantity)
                      );
                      const widthPercent = (bid.quantity / maxQty) * 100;
                      return (
                        <div key={i} style={{ display: 'flex', alignItems: 'center', gap: '4px', height: '20px' }}>
                          <span style={{ fontSize: '9px', color: FINCEPT.GREEN, width: '60px', textAlign: 'right' }}>
                            {fmtPrice(bid.price)}
                          </span>
                          <div style={{ flex: 1, height: '16px', backgroundColor: FINCEPT.PANEL_BG, position: 'relative' }}>
                            <div style={{
                              position: 'absolute',
                              right: 0,
                              height: '100%',
                              width: `${widthPercent}%`,
                              backgroundColor: `${FINCEPT.GREEN}40`,
                              borderRight: `2px solid ${FINCEPT.GREEN}`,
                            }} />
                          </div>
                          <span style={{ fontSize: '9px', color: FINCEPT.WHITE, width: '50px' }}>
                            {bid.quantity.toLocaleString('en-IN')}
                          </span>
                        </div>
                      );
                    })}
                  </div>

                  {/* Spread line */}
                  <div style={{
                    padding: '4px 8px',
                    backgroundColor: FINCEPT.HEADER_BG,
                    textAlign: 'center',
                    fontSize: '9px',
                    color: FINCEPT.YELLOW,
                    borderTop: `1px solid ${FINCEPT.BORDER}`,
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  }}>
                    Spread: {spread > 0 ? fmtPrice(spread) : '--'}
                  </div>

                  {/* Ask depth bars */}
                  <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '2px' }}>
                    {marketDepth.asks?.slice(0, 5).map((ask, i) => {
                      const maxQty = Math.max(
                        ...marketDepth.bids.slice(0, 5).map(b => b.quantity),
                        ...marketDepth.asks.slice(0, 5).map(a => a.quantity)
                      );
                      const widthPercent = (ask.quantity / maxQty) * 100;
                      return (
                        <div key={i} style={{ display: 'flex', alignItems: 'center', gap: '4px', height: '20px' }}>
                          <span style={{ fontSize: '9px', color: FINCEPT.RED, width: '60px', textAlign: 'right' }}>
                            {fmtPrice(ask.price)}
                          </span>
                          <div style={{ flex: 1, height: '16px', backgroundColor: FINCEPT.PANEL_BG, position: 'relative' }}>
                            <div style={{
                              position: 'absolute',
                              left: 0,
                              height: '100%',
                              width: `${widthPercent}%`,
                              backgroundColor: `${FINCEPT.RED}40`,
                              borderLeft: `2px solid ${FINCEPT.RED}`,
                            }} />
                          </div>
                          <span style={{ fontSize: '9px', color: FINCEPT.WHITE, width: '50px' }}>
                            {ask.quantity.toLocaleString('en-IN')}
                          </span>
                        </div>
                      );
                    })}
                  </div>
                </div>
              </>
            ) : (
              <div style={{
                height: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                flexDirection: 'column',
                gap: '12px'
              }}>
                {isLoadingQuote ? (
                  <>
                    <Loader2 size={32} color={FINCEPT.ORANGE} style={{ animation: 'spin 1s linear infinite' }} />
                    <span style={{ fontSize: '11px', color: FINCEPT.GRAY }}>Loading depth data...</span>
                  </>
                ) : (
                  <>
                    <Layers size={48} color={FINCEPT.GRAY} />
                    <span style={{ fontSize: '12px', color: FINCEPT.GRAY }}>Market Depth</span>
                    <span style={{ fontSize: '10px', color: FINCEPT.MUTED, textAlign: 'center' }}>
                      {selectedSymbol ? 'No depth data available' : 'Select a symbol to view depth'}
                    </span>
                  </>
                )}
              </div>
            )}
          </div>
        )}

        {rightPanelView === 'info' && (
          <div style={{ padding: '8px' }}>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '12px',
              letterSpacing: '0.5px'
            }}>
              SYMBOL INFO
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {[
                { label: 'Symbol', value: selectedSymbol || '--' },
                { label: 'Exchange', value: selectedExchange },
                { label: 'Open', value: quote?.open ? fmtPrice(quote.open) : '--' },
                { label: 'Prev Close', value: quote?.previousClose ? fmtPrice(quote.previousClose) : '--' },
                { label: 'Day High', value: quote?.high ? fmtPrice(quote.high) : '--' },
                { label: 'Day Low', value: quote?.low ? fmtPrice(quote.low) : '--' },
                { label: 'Volume', value: quote?.volume ? quote.volume.toLocaleString('en-IN') : '--' },
              ].map((item) => (
                <div
                  key={item.label}
                  style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    padding: '6px 8px',
                    backgroundColor: FINCEPT.HEADER_BG,
                    border: `1px solid ${FINCEPT.BORDER}`
                  }}
                >
                  <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>{item.label}</span>
                  <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>{item.value}</span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Funds Panel (Compact) */}
      {isAuthenticated && (
        <div style={{
          padding: '6px 8px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          fontSize: '10px',
          flexShrink: 0,
          overflow: 'hidden'
        }}>
          <FundsPanel compact />
        </div>
      )}
    </div>
  );
};
