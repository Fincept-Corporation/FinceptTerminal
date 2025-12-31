/**
 * Live Streaming Tab - Real-time WebSocket Quotes
 * Displays live streaming market data from Fyers Data WebSocket
 */

import React, { useState, useEffect, useRef } from 'react';
import { Radio, TrendingUp, ArrowUp, ArrowDown, Wifi, WifiOff, Activity, Plus, X } from 'lucide-react';
import { authManager } from '../services/AuthManager';
import { webSocketManager } from '../services/WebSocketManager';
import { UnifiedQuote } from '../core/types';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  INPUT_BG: '#0A0A0A',
  CYAN: '#00E5FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

// Default symbols - Only RELIANCE for testing
const DEFAULT_SYMBOLS = [
  'RELIANCE',
];

interface LiveQuote extends UnifiedQuote {
  updateCount: number;
  lastUpdate: Date;
}

const LiveStreamingTab: React.FC = () => {
  const [quotes, setQuotes] = useState<Map<string, LiveQuote>>(new Map());
  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [updateCount, setUpdateCount] = useState(0);
  const [searchTerm, setSearchTerm] = useState('');
  const [subscribedSymbols, setSubscribedSymbols] = useState<string[]>(DEFAULT_SYMBOLS);
  const [newSymbol, setNewSymbol] = useState('');
  const subscriptionIdsRef = useRef<Set<string>>(new Set());

  // Shared quote callback - same instance used for all subscriptions
  const handleQuoteUpdate = useRef((quote: UnifiedQuote) => {
    setQuotes(prev => {
      const key = `${quote.exchange}:${quote.symbol}`;
      const existing = prev.get(key);
      const newQuote: LiveQuote = {
        ...quote,
        updateCount: (existing?.updateCount || 0) + 1,
        lastUpdate: new Date(),
      };
      const updated = new Map(prev);
      updated.set(key, newQuote);
      return updated;
    });
    setUpdateCount(prev => prev + 1);
  }).current;

  const subscribeToSymbols = async (symbols: string[]) => {
    try {
      const formattedSymbols = symbols.map(symbol => ({
        symbol: `${symbol}-EQ`,
        exchange: 'NSE',
      }));

      const subscriptionId = webSocketManager.subscribeQuotes(
        'fyers',
        formattedSymbols,
        handleQuoteUpdate
      );

      subscriptionIdsRef.current.add(subscriptionId);
      console.log('[LiveStreamingTab] Subscribed to', symbols.length, 'symbols, total subscriptions:', subscriptionIdsRef.current.size);
    } catch (err: any) {
      console.error('[LiveStreamingTab] Subscription error:', err);
      throw err;
    }
  };

  const connectAndSubscribe = async () => {
    setIsConnecting(true);
    setError(null);

    try {
      const adapter = authManager.getAdapter('fyers');
      if (!adapter) {
        throw new Error('Fyers adapter not found');
      }

      if (!authManager.isAuthenticated('fyers')) {
        throw new Error('Fyers not authenticated. Please configure Fyers first.');
      }

      // Connect WebSocket
      await webSocketManager.connect('fyers');
      setIsConnected(true);

      // Subscribe to all symbols
      await subscribeToSymbols(subscribedSymbols);

    } catch (err: any) {
      console.error('[LiveStreamingTab] Connection error:', err);
      setError(err.message);
      setIsConnected(false);
    } finally {
      setIsConnecting(false);
    }
  };

  const disconnect = async () => {
    try {
      // Unsubscribe all subscriptions
      subscriptionIdsRef.current.forEach(subId => {
        webSocketManager.unsubscribeQuotes(subId);
      });
      subscriptionIdsRef.current.clear();

      await webSocketManager.disconnect('fyers');
      setIsConnected(false);
      setQuotes(new Map());
      setUpdateCount(0);
    } catch (err) {
      console.error('[LiveStreamingTab] Disconnect error:', err);
    }
  };

  const addSymbol = () => {
    const symbol = newSymbol.trim().toUpperCase();
    if (!symbol) return;

    if (subscribedSymbols.includes(symbol)) {
      alert('Symbol already added!');
      return;
    }

    const newSymbols = [...subscribedSymbols, symbol];
    setSubscribedSymbols(newSymbols);
    setNewSymbol('');

    // If connected, subscribe to the new symbol immediately
    if (isConnected) {
      console.log(`[LiveStreamingTab] Adding new symbol while connected: ${symbol}`);
      subscribeToSymbols([symbol]).catch(err => {
        console.error('Failed to subscribe to new symbol:', err);
      });
    } else {
      console.log(`[LiveStreamingTab] Added symbol ${symbol} - will subscribe on connect`);
    }
  };

  const removeSymbol = (symbolToRemove: string) => {
    const newSymbols = subscribedSymbols.filter(s => s !== symbolToRemove);
    setSubscribedSymbols(newSymbols);

    // Remove from quotes
    setQuotes(prev => {
      const updated = new Map(prev);
      const key = `NSE:${symbolToRemove}-EQ`;
      updated.delete(key);
      return updated;
    });
  };

  useEffect(() => {
    return () => {
      // Cleanup on unmount - unsubscribe all
      subscriptionIdsRef.current.forEach(subId => {
        webSocketManager.unsubscribeQuotes(subId);
      });
      subscriptionIdsRef.current.clear();
    };
  }, []);

  const getFilteredQuotes = (): LiveQuote[] => {
    let filtered = Array.from(quotes.values());

    if (searchTerm) {
      const search = searchTerm.toLowerCase();
      filtered = filtered.filter(q =>
        q.symbol.toLowerCase().includes(search)
      );
    }

    return filtered.sort((a, b) => b.changePercent - a.changePercent);
  };

  const filteredQuotes = getFilteredQuotes();
  const connectedSymbols = quotes.size;

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      color: BLOOMBERG.WHITE,
      fontFamily: 'monospace',
      overflow: 'auto',
      padding: '16px'
    }}>
      {/* Header */}
      <div style={{
        marginBottom: '16px',
        padding: '16px',
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          marginBottom: '12px'
        }}>
          <Radio size={16} color={isConnected ? BLOOMBERG.GREEN : BLOOMBERG.GRAY} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: BLOOMBERG.CYAN }}>
            LIVE STREAMING
          </span>
          <span style={{
            fontSize: '10px',
            color: isConnected ? BLOOMBERG.GREEN : BLOOMBERG.RED,
            marginLeft: 'auto',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            {isConnected ? (
              <>
                <Wifi size={14} />
                CONNECTED ({connectedSymbols}/{subscribedSymbols.length})
              </>
            ) : (
              <>
                <WifiOff size={14} />
                DISCONNECTED
              </>
            )}
          </span>
        </div>

        {/* Add Symbol */}
        <div style={{ display: 'flex', gap: '12px', marginBottom: '12px' }}>
          <input
            type="text"
            value={newSymbol}
            onChange={(e) => setNewSymbol(e.target.value.toUpperCase())}
            onKeyPress={(e) => e.key === 'Enter' && addSymbol()}
            placeholder="Add symbol (e.g., RITES, IRCTC)"
            style={{
              padding: '8px 12px',
              backgroundColor: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              fontSize: '11px',
              fontWeight: 600,
              fontFamily: 'monospace',
              outline: 'none',
              flex: 1,
            }}
          />
          <button
            onClick={addSymbol}
            style={{
              padding: '10px 16px',
              backgroundColor: BLOOMBERG.CYAN,
              border: 'none',
              color: BLOOMBERG.DARK_BG,
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              fontFamily: 'monospace',
              textTransform: 'uppercase',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            <Plus size={14} />
            Add
          </button>
        </div>

        {/* Search */}
        <div style={{ display: 'flex', gap: '12px', marginBottom: '12px' }}>
          <input
            type="text"
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            placeholder="Search streaming symbols..."
            style={{
              padding: '8px 12px',
              backgroundColor: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              fontSize: '11px',
              fontWeight: 600,
              fontFamily: 'monospace',
              outline: 'none',
              flex: 1,
            }}
          />

          {!isConnected ? (
            <button
              onClick={connectAndSubscribe}
              disabled={isConnecting}
              style={{
                padding: '10px 16px',
                backgroundColor: BLOOMBERG.GREEN,
                border: 'none',
                color: BLOOMBERG.DARK_BG,
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: isConnecting ? 'not-allowed' : 'pointer',
                fontFamily: 'monospace',
                textTransform: 'uppercase',
                opacity: isConnecting ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              <Radio size={14} />
              {isConnecting ? 'Connecting...' : 'Connect & Stream'}
            </button>
          ) : (
            <button
              onClick={disconnect}
              style={{
                padding: '10px 16px',
                backgroundColor: BLOOMBERG.RED,
                border: 'none',
                color: BLOOMBERG.WHITE,
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily: 'monospace',
                textTransform: 'uppercase',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              <WifiOff size={14} />
              Disconnect
            </button>
          )}
        </div>

        {/* Subscribed Symbols */}
        <div style={{
          maxHeight: '120px',
          overflow: 'auto',
          padding: '8px',
          backgroundColor: BLOOMBERG.INPUT_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          marginBottom: '12px'
        }}>
          <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
            SUBSCRIBED SYMBOLS ({subscribedSymbols.length}):
          </div>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
            {subscribedSymbols.map(symbol => (
              <div
                key={symbol}
                style={{
                  padding: '4px 8px',
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  fontSize: '9px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
              >
                {symbol}
                <X
                  size={10}
                  style={{ cursor: 'pointer', color: BLOOMBERG.RED }}
                  onClick={() => removeSymbol(symbol)}
                />
              </div>
            ))}
          </div>
        </div>

        {/* Stats */}
        {isConnected && (
          <div style={{
            display: 'flex',
            gap: '16px',
            fontSize: '9px',
            color: BLOOMBERG.GRAY,
            paddingTop: '8px',
            borderTop: `1px solid ${BLOOMBERG.BORDER}`
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Activity size={12} color={BLOOMBERG.CYAN} />
              UPDATES: {updateCount.toLocaleString()}
            </div>
            <div>STREAMING: {connectedSymbols}/{subscribedSymbols.length}</div>
            <div>FILTERED: {filteredQuotes.length}</div>
          </div>
        )}

        {error && (
          <div style={{
            marginTop: '12px',
            padding: '8px',
            backgroundColor: BLOOMBERG.INPUT_BG,
            border: `1px solid ${BLOOMBERG.RED}`,
            color: BLOOMBERG.RED,
            fontSize: '10px',
          }}>
            ERROR: {error}
          </div>
        )}
      </div>

      {/* Quote Cards Grid */}
      {isConnected && filteredQuotes.length > 0 && (
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
          gap: '12px',
          maxHeight: 'calc(100vh - 400px)',
          overflow: 'auto',
          paddingRight: '4px'
        }}>
          {filteredQuotes.map((quote) => {
            const isPositive = quote.changePercent >= 0;
            const key = `${quote.exchange}:${quote.symbol}`;
            const timeSinceUpdate = Date.now() - quote.lastUpdate.getTime();
            const isRecent = timeSinceUpdate < 2000;

            return (
              <div
                key={key}
                style={{
                  padding: '12px',
                  backgroundColor: isRecent ? BLOOMBERG.HOVER : BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  borderLeft: `3px solid ${isPositive ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
                  transition: 'all 0.3s',
                  cursor: 'pointer',
                  position: 'relative',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                  e.currentTarget.style.transform = 'translateY(-2px)';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = isRecent ? BLOOMBERG.HOVER : BLOOMBERG.PANEL_BG;
                  e.currentTarget.style.transform = 'translateY(0)';
                }}
              >
                {isRecent && (
                  <div style={{
                    position: 'absolute',
                    top: '4px',
                    right: '4px',
                    width: '6px',
                    height: '6px',
                    borderRadius: '50%',
                    backgroundColor: BLOOMBERG.CYAN,
                    animation: 'pulse 1s infinite',
                  }} />
                )}

                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                  <span style={{ fontSize: '13px', fontWeight: 700, color: BLOOMBERG.WHITE }}>
                    {quote.symbol.replace('-EQ', '')}
                  </span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    {isPositive ? <ArrowUp size={12} color={BLOOMBERG.GREEN} /> : <ArrowDown size={12} color={BLOOMBERG.RED} />}
                    <span style={{
                      fontSize: '11px',
                      fontWeight: 700,
                      color: isPositive ? BLOOMBERG.GREEN : BLOOMBERG.RED
                    }}>
                      {isPositive ? '+' : ''}{quote.changePercent.toFixed(2)}%
                    </span>
                  </div>
                </div>

                <div style={{ marginBottom: '8px' }}>
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY }}>LAST PRICE</div>
                  <div style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.WHITE }}>
                    ₹{quote.lastPrice.toFixed(2)}
                  </div>
                  <div style={{
                    fontSize: '9px',
                    color: isPositive ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                    fontWeight: 600
                  }}>
                    {isPositive ? '+' : ''}{quote.change.toFixed(2)}
                  </div>
                </div>

                {/* OHLCV Data */}
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: '6px',
                  paddingTop: '8px',
                  borderTop: `1px solid ${BLOOMBERG.BORDER}`
                }}>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>OPEN</div>
                    <div style={{ fontSize: '10px', fontWeight: 600 }}>₹{quote.open.toFixed(2)}</div>
                  </div>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>HIGH</div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GREEN }}>₹{quote.high.toFixed(2)}</div>
                  </div>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>LOW</div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.RED }}>₹{quote.low.toFixed(2)}</div>
                  </div>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>PREV CLOSE</div>
                    <div style={{ fontSize: '10px', fontWeight: 600 }}>₹{quote.close.toFixed(2)}</div>
                  </div>
                </div>

                {/* Bid/Ask Spread */}
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: '6px',
                  paddingTop: '6px',
                  borderTop: `1px solid ${BLOOMBERG.BORDER}`,
                  marginTop: '6px'
                }}>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>BID</div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GREEN }}>
                      {quote.bid ? `₹${quote.bid.toFixed(2)}` : 'N/A'}
                    </div>
                    {quote.bidQty !== undefined && (
                      <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>
                        Qty: {quote.bidQty.toLocaleString()}
                      </div>
                    )}
                  </div>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>ASK</div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.RED }}>
                      {quote.ask ? `₹${quote.ask.toFixed(2)}` : 'N/A'}
                    </div>
                    {quote.askQty !== undefined && (
                      <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>
                        Qty: {quote.askQty.toLocaleString()}
                      </div>
                    )}
                  </div>
                </div>

                {/* Volume & Trade Info */}
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: '6px',
                  paddingTop: '6px',
                  borderTop: `1px solid ${BLOOMBERG.BORDER}`,
                  marginTop: '6px'
                }}>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>VOLUME TRADED</div>
                    <div style={{ fontSize: '10px', fontWeight: 600 }}>
                      {quote.volume > 0 ? `${(quote.volume / 1000).toFixed(0)}K` : 'N/A'}
                    </div>
                  </div>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>LAST TRADE QTY</div>
                    <div style={{ fontSize: '10px', fontWeight: 600 }}>
                      {quote.lastTradedQty !== undefined ? quote.lastTradedQty.toLocaleString() : 'N/A'}
                    </div>
                  </div>
                </div>

                {/* Total Buy/Sell Quantities */}
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: '6px',
                  paddingTop: '6px',
                  borderTop: `1px solid ${BLOOMBERG.BORDER}`,
                  marginTop: '6px'
                }}>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>TOTAL BUY QTY</div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GREEN }}>
                      {quote.totalBuyQty !== undefined ? `${(quote.totalBuyQty / 1000).toFixed(0)}K` : 'N/A'}
                    </div>
                  </div>
                  <div>
                    <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>TOTAL SELL QTY</div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.RED }}>
                      {quote.totalSellQty !== undefined ? `${(quote.totalSellQty / 1000).toFixed(0)}K` : 'N/A'}
                    </div>
                  </div>
                </div>

                {/* Update Counter */}
                <div style={{
                  fontSize: '7px',
                  color: BLOOMBERG.GRAY,
                  marginTop: '6px',
                  textAlign: 'right'
                }}>
                  {quote.updateCount} updates
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Empty State */}
      {!isConnected && !error && (
        <div style={{
          padding: '40px',
          textAlign: 'center',
          fontSize: '11px',
          color: BLOOMBERG.GRAY,
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          <Radio size={32} color={BLOOMBERG.GRAY} style={{ margin: '0 auto 16px' }} />
          <div style={{ marginBottom: '8px', fontSize: '12px', fontWeight: 700 }}>
            WebSocket Not Connected
          </div>
          <div>
            Add symbols and click "Connect & Stream" to start receiving real-time market data
          </div>
        </div>
      )}

      {isConnected && filteredQuotes.length === 0 && quotes.size > 0 && (
        <div style={{
          padding: '40px',
          textAlign: 'center',
          fontSize: '11px',
          color: BLOOMBERG.GRAY,
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          No stocks found matching "{searchTerm}"
        </div>
      )}

      {isConnected && quotes.size === 0 && (
        <div style={{
          padding: '40px',
          textAlign: 'center',
          fontSize: '11px',
          color: BLOOMBERG.GRAY,
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          Waiting for data... ({subscribedSymbols.length} symbols subscribed)
        </div>
      )}

      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>
    </div>
  );
};

export default LiveStreamingTab;
