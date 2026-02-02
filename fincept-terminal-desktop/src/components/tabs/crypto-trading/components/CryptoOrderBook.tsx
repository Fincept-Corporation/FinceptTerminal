// CryptoOrderBook.tsx - Professional Order Book Display
// Direct Kraken WebSocket connection with incremental updates
import React, { useCallback, useRef, useEffect, memo, useState } from 'react';
import { FINCEPT } from '../constants';
import type { RightPanelViewType } from '../types';
import { VolumeProfile } from '../../trading/charts';
import { ModelChatPanel } from '../../trading/ai-agents/ModelChatPanel';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

const NUM_LEVELS = 12;

interface CryptoOrderBookProps {
  currentPrice: number;
  selectedSymbol: string;
  activeBroker: string | null;
  rightPanelView: RightPanelViewType;
  onViewChange: (view: RightPanelViewType) => void;
}

export const CryptoOrderBook = memo(function CryptoOrderBook({
  currentPrice,
  selectedSymbol,
  activeBroker,
  rightPanelView,
  onViewChange,
}: CryptoOrderBookProps) {
  const [status, setStatus] = useState<'connecting' | 'connected' | 'disconnected'>('disconnected');

  const wsRef = useRef<WebSocket | null>(null);
  const asksRef = useRef<Record<string, number>>({});
  const bidsRef = useRef<Record<string, number>>({});
  const asksContainerRef = useRef<HTMLDivElement>(null);
  const bidsContainerRef = useRef<HTMLDivElement>(null);
  const spreadValueRef = useRef<HTMLSpanElement>(null);
  const spreadPercentRef = useRef<HTMLSpanElement>(null);
  const priceRef = useRef<HTMLDivElement>(null);
  const levelCountRef = useRef<HTMLSpanElement>(null);
  const updateTimeRef = useRef<HTMLSpanElement>(null);
  const askTotalRef = useRef<HTMLSpanElement>(null);
  const bidTotalRef = useRef<HTMLSpanElement>(null);

  const formatPrice = (price: number): string => {
    if (price >= 1000) return price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    if (price >= 1) return price.toFixed(2);
    return price.toFixed(6);
  };

  const formatQty = (qty: number): string => {
    if (qty >= 1000) return qty.toLocaleString(undefined, { maximumFractionDigits: 2 });
    if (qty >= 1) return qty.toFixed(4);
    return qty.toFixed(6);
  };

  const render = useCallback(() => {
    const asks = asksRef.current;
    const bids = bidsRef.current;

    const askList = Object.entries(asks)
      .map(([p, q]) => [parseFloat(p), q] as [number, number])
      .sort((a, b) => a[0] - b[0])
      .slice(0, NUM_LEVELS);

    const bidList = Object.entries(bids)
      .map(([p, q]) => [parseFloat(p), q] as [number, number])
      .sort((a, b) => b[0] - a[0])
      .slice(0, NUM_LEVELS);

    // Calculate totals and max for depth visualization
    const askTotal = askList.reduce((sum, [, q]) => sum + q, 0);
    const bidTotal = bidList.reduce((sum, [, q]) => sum + q, 0);
    const maxTotal = Math.max(askTotal, bidTotal) || 1;

    // Calculate cumulative depths
    let askCumulative = 0;
    const askWithCumulative = askList.map(([p, q]) => {
      askCumulative += q;
      return [p, q, askCumulative] as [number, number, number];
    });

    let bidCumulative = 0;
    const bidWithCumulative = bidList.map(([p, q]) => {
      bidCumulative += q;
      return [p, q, bidCumulative] as [number, number, number];
    });

    // Render asks (reversed so best ask at bottom, near spread)
    if (asksContainerRef.current) {
      if (askWithCumulative.length === 0) {
        asksContainerRef.current.innerHTML = `
          <div style="display:flex;align-items:center;justify-content:center;height:100%;color:#666;font-size:10px;">
            Waiting for ask data...
          </div>`;
      } else {
        asksContainerRef.current.innerHTML = [...askWithCumulative].reverse().map(([price, qty, cumulative]) => `
          <div class="ob-row">
            <div class="ob-depth ob-depth-ask" style="width:${(cumulative / maxTotal) * 100}%"></div>
            <span class="ob-price ask">${formatPrice(price)}</span>
            <span class="ob-qty">${formatQty(qty)}</span>
            <span class="ob-total">${formatQty(cumulative)}</span>
          </div>
        `).join('');
      }
    }

    // Render bids (best bid at top, near spread)
    if (bidsContainerRef.current) {
      if (bidWithCumulative.length === 0) {
        bidsContainerRef.current.innerHTML = `
          <div style="display:flex;align-items:center;justify-content:center;height:100%;color:#666;font-size:10px;">
            Waiting for bid data...
          </div>`;
      } else {
        bidsContainerRef.current.innerHTML = bidWithCumulative.map(([price, qty, cumulative]) => `
          <div class="ob-row">
            <div class="ob-depth ob-depth-bid" style="width:${(cumulative / maxTotal) * 100}%"></div>
            <span class="ob-price bid">${formatPrice(price)}</span>
            <span class="ob-qty">${formatQty(qty)}</span>
            <span class="ob-total">${formatQty(cumulative)}</span>
          </div>
        `).join('');
      }
    }

    // Update spread
    if (askList.length > 0 && bidList.length > 0) {
      const bestAsk = askList[0][0];
      const bestBid = bidList[0][0];
      const spread = bestAsk - bestBid;
      const spreadPercent = (spread / bestAsk) * 100;

      if (spreadValueRef.current) {
        spreadValueRef.current.textContent = formatPrice(spread);
      }
      if (spreadPercentRef.current) {
        spreadPercentRef.current.textContent = `(${spreadPercent.toFixed(3)}%)`;
      }
    } else {
      if (spreadValueRef.current) {
        spreadValueRef.current.textContent = '--';
      }
      if (spreadPercentRef.current) {
        spreadPercentRef.current.textContent = '';
      }
    }

    // Update totals
    if (askTotalRef.current) {
      askTotalRef.current.textContent = formatQty(askTotal);
    }
    if (bidTotalRef.current) {
      bidTotalRef.current.textContent = formatQty(bidTotal);
    }

    // Update level count
    if (levelCountRef.current) {
      levelCountRef.current.textContent = `${Object.keys(asks).length + Object.keys(bids).length}`;
    }

    // Update time
    if (updateTimeRef.current) {
      updateTimeRef.current.textContent = new Date().toLocaleTimeString();
    }
  }, []);

  const connect = useCallback(() => {
    if (wsRef.current) {
      wsRef.current.close();
      wsRef.current = null;
    }

    asksRef.current = {};
    bidsRef.current = {};
    render();

    if (activeBroker !== 'kraken' || !selectedSymbol) {
      setStatus('disconnected');
      return;
    }

    setStatus('connecting');

    const ws = new WebSocket('wss://ws.kraken.com/v2');
    wsRef.current = ws;

    ws.onopen = () => {
      setStatus('connected');
      ws.send(JSON.stringify({
        method: 'subscribe',
        params: {
          channel: 'book',
          symbol: [selectedSymbol],
          depth: 25
        }
      }));
    };

    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);

        if (msg.channel === 'book') {
          if (msg.type === 'snapshot') {
            asksRef.current = {};
            bidsRef.current = {};

            if (msg.data[0].asks && Array.isArray(msg.data[0].asks)) {
              msg.data[0].asks.forEach((a: { price: number; qty: number }) => {
                asksRef.current[String(a.price)] = a.qty;
              });
            }

            if (msg.data[0].bids && Array.isArray(msg.data[0].bids)) {
              msg.data[0].bids.forEach((b: { price: number; qty: number }) => {
                bidsRef.current[String(b.price)] = b.qty;
              });
            }
            render();
          } else if (msg.type === 'update') {
            if (msg.data[0].asks && Array.isArray(msg.data[0].asks)) {
              msg.data[0].asks.forEach((a: { price: number; qty: number }) => {
                if (a.qty === 0) {
                  delete asksRef.current[String(a.price)];
                } else {
                  asksRef.current[String(a.price)] = a.qty;
                }
              });
            }
            if (msg.data[0].bids && Array.isArray(msg.data[0].bids)) {
              msg.data[0].bids.forEach((b: { price: number; qty: number }) => {
                if (b.qty === 0) {
                  delete bidsRef.current[String(b.price)];
                } else {
                  bidsRef.current[String(b.price)] = b.qty;
                }
              });
            }
            render();
          }
        }
      } catch (err) {
        // Parse error - silent fail
      }
    };

    ws.onclose = () => setStatus('disconnected');
    ws.onerror = () => setStatus('disconnected');
  }, [activeBroker, selectedSymbol, render]);

  useEffect(() => {
    connect();
    return () => {
      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, [connect]);

  useEffect(() => {
    if (priceRef.current) {
      priceRef.current.textContent = `$${formatPrice(currentPrice)}`;
    }
  }, [currentPrice]);

  const handleViewChange = useCallback((view: RightPanelViewType) => {
    onViewChange(view);
  }, [onViewChange]);

  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.PANEL_BG }}>
      <style>{`
        .ob-row {
          display: grid;
          grid-template-columns: 1fr 1fr 1fr;
          padding: 2px 8px;
          font-size: 11px;
          position: relative;
          font-family: "IBM Plex Mono", monospace;
          height: 22px;
          align-items: center;
        }
        .ob-row:hover {
          background: rgba(255,255,255,0.03);
        }
        .ob-depth {
          position: absolute;
          top: 1px;
          bottom: 1px;
          z-index: 0;
          border-radius: 2px;
        }
        .ob-depth-ask {
          background: linear-gradient(90deg, transparent, rgba(239,68,68,0.15));
          right: 0;
        }
        .ob-depth-bid {
          background: linear-gradient(270deg, transparent, rgba(34,197,94,0.15));
          left: 0;
        }
        .ob-row span {
          position: relative;
          z-index: 1;
        }
        .ob-price {
          font-weight: 600;
          text-align: left;
        }
        .ob-price.ask { color: ${FINCEPT.RED}; }
        .ob-price.bid { color: ${FINCEPT.GREEN}; }
        .ob-qty {
          color: ${FINCEPT.WHITE};
          text-align: right;
        }
        .ob-total {
          color: ${FINCEPT.GRAY};
          text-align: right;
          font-size: 10px;
        }
        .ob-header {
          display: grid;
          grid-template-columns: 1fr 1fr 1fr;
          padding: 4px 8px;
          font-size: 9px;
          color: ${FINCEPT.GRAY};
          border-bottom: 1px solid ${FINCEPT.BORDER};
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }
        .ob-header span:nth-child(2),
        .ob-header span:nth-child(3) {
          text-align: right;
        }
        .tab-btn {
          padding: 6px 12px;
          background: transparent;
          border: none;
          color: ${FINCEPT.GRAY};
          cursor: pointer;
          font-size: 10px;
          font-weight: 600;
          letter-spacing: 0.5px;
          transition: all 0.15s ease;
          border-bottom: 2px solid transparent;
        }
        .tab-btn:hover {
          color: ${FINCEPT.WHITE};
        }
        .tab-btn.active {
          color: ${FINCEPT.ORANGE};
          border-bottom-color: ${FINCEPT.ORANGE};
        }
      `}</style>

      {/* Tab Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <div style={{ display: 'flex' }}>
          {(['orderbook', 'volume', 'modelchat'] as RightPanelViewType[]).map((view) => (
            <button
              key={view}
              className={`tab-btn ${rightPanelView === view ? 'active' : ''}`}
              onClick={() => handleViewChange(view)}
            >
              {view === 'orderbook' ? 'ORDER BOOK' : view === 'volume' ? 'VOLUME' : 'AI CHAT'}
            </button>
          ))}
        </div>
        {rightPanelView === 'orderbook' && (
          <div style={{ padding: '0 12px', fontSize: '9px', color: FINCEPT.GRAY }}>
            <span ref={levelCountRef}>0</span> levels
          </div>
        )}
      </div>

      {/* Order Book Content */}
      {rightPanelView === 'orderbook' && (
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Connection Status */}
          <div style={{
            padding: '6px 12px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            backgroundColor: 'rgba(0,0,0,0.2)',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <div style={{
                width: '6px',
                height: '6px',
                borderRadius: '50%',
                backgroundColor: status === 'connected' ? FINCEPT.GREEN : status === 'connecting' ? FINCEPT.YELLOW : FINCEPT.RED,
                boxShadow: status === 'connected' ? `0 0 6px ${FINCEPT.GREEN}` : 'none',
              }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                {status === 'connected' ? 'LIVE' : status === 'connecting' ? 'CONNECTING' : 'OFFLINE'}
              </span>
            </div>
            {activeBroker !== 'kraken' && (
              <span style={{ fontSize: '9px', color: FINCEPT.YELLOW }}>Select Kraken</span>
            )}
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              <span ref={updateTimeRef}>--:--:--</span>
            </span>
          </div>

          {/* Column Headers */}
          <div className="ob-header">
            <span>Price</span>
            <span>Size</span>
            <span>Total</span>
          </div>

          {/* Asks Section */}
          <div style={{
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div style={{
              padding: '4px 8px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              backgroundColor: 'rgba(239,68,68,0.05)',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.RED }}>ASKS</span>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                Total: <span ref={askTotalRef} style={{ color: FINCEPT.RED }}>0</span>
              </span>
            </div>
            <div
              ref={asksContainerRef}
              style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column', justifyContent: 'flex-end' }}
            />
          </div>

          {/* Spread / Current Price */}
          <div style={{
            padding: '8px 12px',
            backgroundColor: FINCEPT.HEADER_BG,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div
              ref={priceRef}
              style={{
                fontSize: '16px',
                fontWeight: 700,
                color: FINCEPT.YELLOW,
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              $0.00
            </div>
            <div style={{ fontSize: '10px', display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={{ color: FINCEPT.GRAY }}>Spread:</span>
              <span ref={spreadValueRef} style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>--</span>
              <span ref={spreadPercentRef} style={{ color: FINCEPT.YELLOW }}></span>
            </div>
          </div>

          {/* Bids Section */}
          <div style={{
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}>
            <div style={{
              padding: '4px 8px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              backgroundColor: 'rgba(34,197,94,0.05)',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GREEN }}>BIDS</span>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                Total: <span ref={bidTotalRef} style={{ color: FINCEPT.GREEN }}>0</span>
              </span>
            </div>
            <div
              ref={bidsContainerRef}
              style={{ flex: 1, overflow: 'hidden' }}
            />
          </div>
        </div>
      )}

      {/* Volume Profile View */}
      {rightPanelView === 'volume' && (
        <div style={{ flex: 1, overflow: 'hidden', padding: '8px' }}>
          <ErrorBoundary name="VolumeProfile" variant="minimal">
            <VolumeProfile symbol={selectedSymbol} height={500} />
          </ErrorBoundary>
        </div>
      )}

      {/* AI Chat View */}
      {rightPanelView === 'modelchat' && (
        <div style={{ flex: 1, overflow: 'hidden' }}>
          <ErrorBoundary name="ModelChatPanel" variant="minimal">
            <ModelChatPanel refreshInterval={5000} />
          </ErrorBoundary>
        </div>
      )}
    </div>
  );
});

export default CryptoOrderBook;
