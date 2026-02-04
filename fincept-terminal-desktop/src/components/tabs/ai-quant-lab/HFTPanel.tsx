/**
 * HFT Panel - High Frequency Trading Operations
 * Order book visualization, market making, microstructure analysis
 * REFACTORED: Terminal-style UI with real-time order book display
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Gauge,
  Activity,
  TrendingUp,
  TrendingDown,
  Zap,
  AlertCircle,
  DollarSign,
  BarChart3,
  RefreshCw,
  Play,
  Pause,
  Network,
  Cpu,
  Clock,
  Loader2
} from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',          // Primary theme color for HFT
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  CYAN: '#00E5FF',
  MUTED: '#4A4A4A',
  HOVER: '#1F1F1F'
};

interface OrderBookLevel {
  price: number;
  size: number;
}

interface MarketMakingQuotes {
  bid: { price: number; size: number };
  ask: { price: number; size: number };
  spread: number;
  mid_price: number;
}

export function HFTPanel() {
  const [symbol, setSymbol] = useState('AAPL');
  const [orderBookCreated, setOrderBookCreated] = useState(false);
  const [isLive, setIsLive] = useState(false);
  const [bids, setBids] = useState<OrderBookLevel[]>([]);
  const [asks, setAsks] = useState<OrderBookLevel[]>([]);
  const [spread, setSpread] = useState(0);
  const [midPrice, setMidPrice] = useState(0);
  const [microstructure, setMicrostructure] = useState({
    depth_imbalance: 0,
    vwap_bid: 0,
    vwap_ask: 0,
    toxicity_score: 0
  });
  const [marketMakingQuotes, setMarketMakingQuotes] = useState<MarketMakingQuotes | null>(null);
  const [latencyStats, setLatencyStats] = useState({
    mean: 150,
    p99: 300
  });

  const generateMockOrderBook = () => {
    const basePrice = 150 + Math.random() * 10;
    const spreadSize = 0.01 + Math.random() * 0.05;

    const mockBids: OrderBookLevel[] = Array.from({ length: 10 }, (_, i) => ({
      price: basePrice - spreadSize / 2 - i * 0.01,
      size: Math.floor(Math.random() * 1000) + 100
    }));

    const mockAsks: OrderBookLevel[] = Array.from({ length: 10 }, (_, i) => ({
      price: basePrice + spreadSize / 2 + i * 0.01,
      size: Math.floor(Math.random() * 1000) + 100
    }));

    setBids(mockBids);
    setAsks(mockAsks);
    setSpread(mockAsks[0].price - mockBids[0].price);
    setMidPrice((mockBids[0].price + mockAsks[0].price) / 2);

    const totalBidVol = mockBids.reduce((sum, b) => sum + b.size, 0);
    const totalAskVol = mockAsks.reduce((sum, a) => sum + a.size, 0);
    const imbalance = (totalBidVol - totalAskVol) / (totalBidVol + totalAskVol);

    setMicrostructure({
      depth_imbalance: imbalance,
      vwap_bid: mockBids.slice(0, 5).reduce((sum, b) => sum + b.price * b.size, 0) / mockBids.slice(0, 5).reduce((sum, b) => sum + b.size, 0),
      vwap_ask: mockAsks.slice(0, 5).reduce((sum, a) => sum + a.price * a.size, 0) / mockAsks.slice(0, 5).reduce((sum, a) => sum + a.size, 0),
      toxicity_score: Math.random() * 0.8
    });
  };

  const handleCreateOrderBook = async () => {
    setOrderBookCreated(true);
    generateMockOrderBook();
  };

  const handleStartLive = () => {
    setIsLive(true);
  };

  const handleStopLive = () => {
    setIsLive(false);
  };

  const handleCalculateMarketMaking = () => {
    if (bids.length > 0 && asks.length > 0) {
      const inventory = Math.random() * 200 - 100;
      const quotes: MarketMakingQuotes = {
        bid: {
          price: midPrice - spread * 0.75,
          size: Math.floor(100 * (1 + inventory / 1000))
        },
        ask: {
          price: midPrice + spread * 0.75,
          size: Math.floor(100 * (1 - inventory / 1000))
        },
        spread: spread * 1.5,
        mid_price: midPrice
      };
      setMarketMakingQuotes(quotes);
    }
  };

  useEffect(() => {
    if (isLive && orderBookCreated) {
      const interval = setInterval(() => {
        generateMockOrderBook();
      }, 1000);
      return () => clearInterval(interval);
    }
  }, [isLive, orderBookCreated]);

  const maxBidSize = Math.max(...bids.map(b => b.size), 1);
  const maxAskSize = Math.max(...asks.map(a => a.size), 1);

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Activity size={16} color={FINCEPT.RED} />
        <span style={{
          color: FINCEPT.RED,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          HIGH FREQUENCY TRADING
        </span>
        <div style={{ flex: 1 }} />

        {/* Symbol Display */}
        {orderBookCreated && (
          <div style={{
            fontSize: '11px',
            fontFamily: 'monospace',
            padding: '3px 10px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.ORANGE}`,
            color: FINCEPT.ORANGE,
            fontWeight: 700
          }}>
            {symbol}
          </div>
        )}

        {/* Latency Indicators */}
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.CYAN}`,
          color: FINCEPT.CYAN
        }}>
          {latencyStats.mean}μs
        </div>

        {/* Live Status */}
        {isLive && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: FINCEPT.GREEN + '20',
            border: `1px solid ${FINCEPT.GREEN}`,
            color: FINCEPT.GREEN,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: FINCEPT.GREEN, animation: 'pulse 2s infinite' }} />
            LIVE
          </div>
        )}

        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: orderBookCreated ? (isLive ? FINCEPT.GREEN + '20' : FINCEPT.DARK_BG) : FINCEPT.DARK_BG,
          border: `1px solid ${orderBookCreated ? (isLive ? FINCEPT.GREEN : FINCEPT.BORDER) : FINCEPT.BORDER}`,
          color: orderBookCreated ? (isLive ? FINCEPT.GREEN : FINCEPT.MUTED) : FINCEPT.MUTED
        }}>
          {isLive ? 'STREAMING' : orderBookCreated ? 'READY' : 'IDLE'}
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Main Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          {!orderBookCreated ? (
            /* Initial Configuration */
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              padding: '40px'
            }}>
              <div style={{
                maxWidth: '500px',
                width: '100%',
                padding: '24px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: `3px solid ${FINCEPT.RED}`
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                  marginBottom: '20px'
                }}>
                  <Gauge size={18} color={FINCEPT.RED} />
                  <span style={{
                    fontSize: '12px',
                    fontWeight: 700,
                    color: FINCEPT.RED,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px'
                  }}>
                    ORDER BOOK CONFIGURATION
                  </span>
                </div>

                <div style={{ marginBottom: '20px' }}>
                  <label style={{
                    display: 'block',
                    fontSize: '9px',
                    marginBottom: '6px',
                    color: FINCEPT.WHITE,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    opacity: 0.7
                  }}>
                    TRADING SYMBOL
                  </label>
                  <input
                    type="text"
                    value={symbol}
                    onChange={(e) => setSymbol(e.target.value.toUpperCase())}
                    placeholder="AAPL"
                    style={{
                      width: '100%',
                      padding: '12px 14px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      color: FINCEPT.RED,
                      fontSize: '16px',
                      fontFamily: 'monospace',
                      fontWeight: 700,
                      outline: 'none',
                      transition: 'border-color 0.15s'
                    }}
                    onFocus={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.RED;
                    }}
                    onBlur={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }}
                  />
                </div>

                <button
                  onClick={handleCreateOrderBook}
                  style={{
                    width: '100%',
                    padding: '14px 20px',
                    backgroundColor: FINCEPT.RED,
                    border: 'none',
                    color: '#000000',
                    fontSize: '11px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '8px',
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    transition: 'opacity 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.opacity = '0.9';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.opacity = '1';
                  }}
                >
                  <Play size={14} />
                  CREATE ORDER BOOK
                </button>

                <div style={{
                  marginTop: '16px',
                  padding: '12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  color: FINCEPT.WHITE,
                  opacity: 0.7,
                  lineHeight: '1.6'
                }}>
                  <strong style={{ color: FINCEPT.RED }}>Note:</strong> This creates a simulated HFT order book with real-time price updates, microstructure analytics, and market making capabilities.
                </div>
              </div>
            </div>
          ) : (
            <>
              {/* Metrics Bar */}
              <div style={{
                padding: '12px 16px',
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                backgroundColor: FINCEPT.PANEL_BG
              }}>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '12px' }}>
                  <div style={{
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${FINCEPT.RED}`
                  }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, marginBottom: '3px', fontFamily: 'monospace', letterSpacing: '0.5px' }}>
                      MID PRICE
                    </div>
                    <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.RED, fontFamily: 'monospace' }}>
                      ${midPrice.toFixed(2)}
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${FINCEPT.RED}`
                  }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, marginBottom: '3px', fontFamily: 'monospace', letterSpacing: '0.5px' }}>
                      SPREAD
                    </div>
                    <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.RED, fontFamily: 'monospace' }}>
                      ${spread.toFixed(3)}
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`
                  }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, marginBottom: '3px', fontFamily: 'monospace', letterSpacing: '0.5px' }}>
                      MEAN LATENCY
                    </div>
                    <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: 'monospace' }}>
                      {latencyStats.mean}μs
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`
                  }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, marginBottom: '3px', fontFamily: 'monospace', letterSpacing: '0.5px' }}>
                      P99 LATENCY
                    </div>
                    <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.WHITE, opacity: 0.7, fontFamily: 'monospace' }}>
                      {latencyStats.p99}μs
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${microstructure.toxicity_score > 0.5 ? FINCEPT.RED : FINCEPT.GREEN}`,
                    borderLeft: `3px solid ${microstructure.toxicity_score > 0.5 ? FINCEPT.RED : FINCEPT.GREEN}`
                  }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, opacity: 0.5, marginBottom: '3px', fontFamily: 'monospace', letterSpacing: '0.5px' }}>
                      TOXICITY
                    </div>
                    <div style={{ fontSize: '14px', fontWeight: 700, color: microstructure.toxicity_score > 0.5 ? FINCEPT.RED : FINCEPT.GREEN, fontFamily: 'monospace' }}>
                      {microstructure.toxicity_score > 0.5 ? 'HIGH' : 'LOW'}
                    </div>
                  </div>
                </div>
              </div>

              {/* Control Bar */}
              <div style={{
                padding: '12px 16px',
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                backgroundColor: FINCEPT.PANEL_BG,
                display: 'flex',
                alignItems: 'center',
                gap: '12px'
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <Network size={14} color={FINCEPT.RED} />
                  <span style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: FINCEPT.RED,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px'
                  }}>
                    LIVE ORDER BOOK
                  </span>
                </div>
                <div style={{ flex: 1 }} />
                {!isLive ? (
                  <button
                    onClick={handleStartLive}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: FINCEPT.GREEN,
                      border: 'none',
                      color: '#000000',
                      fontSize: '10px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      transition: 'opacity 0.15s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.opacity = '0.9';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.opacity = '1';
                    }}
                  >
                    <Play size={12} />
                    START LIVE
                  </button>
                ) : (
                  <button
                    onClick={handleStopLive}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: FINCEPT.RED,
                      border: 'none',
                      color: '#000000',
                      fontSize: '10px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      transition: 'opacity 0.15s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.opacity = '0.9';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.opacity = '1';
                    }}
                  >
                    <Pause size={12} />
                    STOP
                  </button>
                )}
                <button
                  onClick={handleCalculateMarketMaking}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${FINCEPT.RED}`,
                    color: FINCEPT.RED,
                    fontSize: '10px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    transition: 'background-color 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.RED + '20';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }}
                >
                  <Zap size={12} />
                  CALC QUOTES
                </button>
              </div>

              {/* Order Book Display */}
              <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
                {/* Order Book */}
                <div style={{ flex: 1, padding: '16px', backgroundColor: FINCEPT.DARK_BG, borderRight: `1px solid ${FINCEPT.BORDER}` }}>
                  {/* Header Row */}
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px', marginBottom: '12px', padding: '0 8px' }}>
                    <div style={{ textAlign: 'right', fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, fontFamily: 'monospace' }}>
                      BID SIZE
                    </div>
                    <div style={{ textAlign: 'center', fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, fontFamily: 'monospace' }}>
                      PRICE
                    </div>
                    <div style={{ textAlign: 'left', fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, fontFamily: 'monospace' }}>
                      ASK SIZE
                    </div>
                  </div>

                  {/* Order Book Rows */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                    {Array.from({ length: 10 }, (_, i) => {
                      const bid = bids[i];
                      const ask = asks[i];

                      return (
                        <div key={i} style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px', alignItems: 'center' }}>
                          {/* Bid Side */}
                          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '6px' }}>
                            <span style={{ fontSize: '11px', fontFamily: 'monospace', color: FINCEPT.GREEN, minWidth: '50px', textAlign: 'right' }}>
                              {bid?.size || ''}
                            </span>
                            <div style={{
                              height: '20px',
                              width: `${bid ? (bid.size / maxBidSize) * 100 : 0}%`,
                              maxWidth: '150px',
                              backgroundColor: FINCEPT.GREEN + '30',
                              borderRight: `2px solid ${FINCEPT.GREEN}`
                            }} />
                          </div>

                          {/* Price */}
                          <div style={{ textAlign: 'center' }}>
                            <div style={{ fontSize: '12px', fontFamily: 'monospace', fontWeight: 600, color: FINCEPT.WHITE }}>
                              {bid ? bid.price.toFixed(2) : ''}
                            </div>
                            {i === 0 && (
                              <div style={{ fontSize: '9px', fontFamily: 'monospace', color: FINCEPT.ORANGE, marginTop: '2px' }}>
                                ↕ ${spread.toFixed(3)}
                              </div>
                            )}
                            <div style={{ fontSize: '12px', fontFamily: 'monospace', fontWeight: 600, color: FINCEPT.WHITE, marginTop: i === 0 ? '2px' : '0' }}>
                              {ask ? ask.price.toFixed(2) : ''}
                            </div>
                          </div>

                          {/* Ask Side */}
                          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                            <div style={{
                              height: '20px',
                              width: `${ask ? (ask.size / maxAskSize) * 100 : 0}%`,
                              maxWidth: '150px',
                              backgroundColor: FINCEPT.RED + '30',
                              borderLeft: `2px solid ${FINCEPT.RED}`
                            }} />
                            <span style={{ fontSize: '11px', fontFamily: 'monospace', color: FINCEPT.RED, minWidth: '50px' }}>
                              {ask?.size || ''}
                            </span>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </div>

                {/* Right Panel - Microstructure & Market Making */}
                <div style={{ width: '300px', display: 'flex', flexDirection: 'column', gap: '0', borderLeft: `1px solid ${FINCEPT.BORDER}` }}>
                  {/* Microstructure Section */}
                  <div style={{ padding: '16px', borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: FINCEPT.PANEL_BG }}>
                    <div style={{
                      fontSize: '11px',
                      fontWeight: 700,
                      color: FINCEPT.CYAN,
                      marginBottom: '12px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}>
                      <Cpu size={13} color={FINCEPT.CYAN} />
                      MICROSTRUCTURE
                    </div>

                    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                      {/* Depth Imbalance */}
                      <div>
                        <div style={{
                          fontSize: '9px',
                          color: FINCEPT.WHITE,
                          opacity: 0.5,
                          marginBottom: '6px',
                          fontFamily: 'monospace',
                          letterSpacing: '0.5px'
                        }}>
                          DEPTH IMBALANCE
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                          <div style={{
                            flex: 1,
                            height: '8px',
                            backgroundColor: FINCEPT.DARK_BG,
                            border: `1px solid ${FINCEPT.BORDER}`,
                            position: 'relative',
                            overflow: 'hidden'
                          }}>
                            <div style={{
                              position: 'absolute',
                              height: '100%',
                              width: `${Math.abs(microstructure.depth_imbalance) * 100}%`,
                              backgroundColor: microstructure.depth_imbalance > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                              left: microstructure.depth_imbalance < 0 ? 'auto' : '0',
                              right: microstructure.depth_imbalance < 0 ? '0' : 'auto'
                            }} />
                          </div>
                          <span style={{
                            fontSize: '11px',
                            fontFamily: 'monospace',
                            color: FINCEPT.WHITE,
                            fontWeight: 600,
                            minWidth: '45px'
                          }}>
                            {(microstructure.depth_imbalance * 100).toFixed(1)}%
                          </span>
                        </div>
                      </div>

                      {/* VWAP Bid/Ask */}
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                        <div style={{
                          padding: '8px',
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${FINCEPT.GREEN}`,
                          borderLeft: `2px solid ${FINCEPT.GREEN}`
                        }}>
                          <div style={{
                            fontSize: '9px',
                            color: FINCEPT.WHITE,
                            opacity: 0.5,
                            marginBottom: '3px',
                            fontFamily: 'monospace',
                            letterSpacing: '0.5px'
                          }}>
                            VWAP BID
                          </div>
                          <div style={{
                            fontSize: '11px',
                            fontFamily: 'monospace',
                            color: FINCEPT.GREEN,
                            fontWeight: 700
                          }}>
                            ${microstructure.vwap_bid.toFixed(2)}
                          </div>
                        </div>
                        <div style={{
                          padding: '8px',
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${FINCEPT.RED}`,
                          borderLeft: `2px solid ${FINCEPT.RED}`
                        }}>
                          <div style={{
                            fontSize: '9px',
                            color: FINCEPT.WHITE,
                            opacity: 0.5,
                            marginBottom: '3px',
                            fontFamily: 'monospace',
                            letterSpacing: '0.5px'
                          }}>
                            VWAP ASK
                          </div>
                          <div style={{
                            fontSize: '11px',
                            fontFamily: 'monospace',
                            color: FINCEPT.RED,
                            fontWeight: 700
                          }}>
                            ${microstructure.vwap_ask.toFixed(2)}
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Market Making Section */}
                  <div style={{ padding: '16px', backgroundColor: FINCEPT.DARK_BG, flex: 1 }}>
                    <div style={{
                      fontSize: '10px',
                      fontWeight: 700,
                      color: FINCEPT.RED,
                      marginBottom: '12px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}>
                      <DollarSign size={13} color={FINCEPT.RED} />
                      MARKET MAKING
                    </div>

                    {marketMakingQuotes ? (
                      <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                        {/* Bid Quote */}
                        <div style={{
                          padding: '12px',
                          backgroundColor: FINCEPT.GREEN + '15',
                          border: `1px solid ${FINCEPT.GREEN}`,
                          borderLeft: `3px solid ${FINCEPT.GREEN}`
                        }}>
                          <div style={{
                            fontSize: '9px',
                            color: FINCEPT.GREEN,
                            marginBottom: '6px',
                            fontFamily: 'monospace',
                            fontWeight: 700,
                            letterSpacing: '0.5px'
                          }}>
                            BID QUOTE
                          </div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <span style={{
                              fontSize: '16px',
                              fontFamily: 'monospace',
                              color: FINCEPT.GREEN,
                              fontWeight: 700
                            }}>
                              ${marketMakingQuotes.bid.price.toFixed(2)}
                            </span>
                            <span style={{
                              fontSize: '10px',
                              fontFamily: 'monospace',
                              color: FINCEPT.GREEN,
                              opacity: 0.8
                            }}>
                              {marketMakingQuotes.bid.size}
                            </span>
                          </div>
                        </div>

                        {/* Ask Quote */}
                        <div style={{
                          padding: '12px',
                          backgroundColor: FINCEPT.RED + '15',
                          border: `1px solid ${FINCEPT.RED}`,
                          borderLeft: `3px solid ${FINCEPT.RED}`
                        }}>
                          <div style={{
                            fontSize: '9px',
                            color: FINCEPT.RED,
                            marginBottom: '6px',
                            fontFamily: 'monospace',
                            fontWeight: 700,
                            letterSpacing: '0.5px'
                          }}>
                            ASK QUOTE
                          </div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <span style={{
                              fontSize: '16px',
                              fontFamily: 'monospace',
                              color: FINCEPT.RED,
                              fontWeight: 700
                            }}>
                              ${marketMakingQuotes.ask.price.toFixed(2)}
                            </span>
                            <span style={{
                              fontSize: '10px',
                              fontFamily: 'monospace',
                              color: FINCEPT.RED,
                              opacity: 0.8
                            }}>
                              {marketMakingQuotes.ask.size}
                            </span>
                          </div>
                        </div>

                        {/* Spread Info */}
                        <div style={{
                          textAlign: 'center',
                          padding: '8px',
                          backgroundColor: FINCEPT.PANEL_BG,
                          border: `1px solid ${FINCEPT.BORDER}`
                        }}>
                          <span style={{
                            fontSize: '9px',
                            color: FINCEPT.WHITE,
                            opacity: 0.6,
                            fontFamily: 'monospace',
                            letterSpacing: '0.5px'
                          }}>
                            QUOTED SPREAD: ${marketMakingQuotes.spread.toFixed(3)}
                          </span>
                        </div>
                      </div>
                    ) : (
                      <div style={{
                        textAlign: 'center',
                        padding: '30px 20px',
                        backgroundColor: FINCEPT.PANEL_BG,
                        border: `1px solid ${FINCEPT.BORDER}`
                      }}>
                        <DollarSign size={32} color={FINCEPT.MUTED} style={{ margin: '0 auto 12px' }} />
                        <div style={{
                          fontSize: '10px',
                          color: FINCEPT.WHITE,
                          opacity: 0.6,
                          fontFamily: 'monospace',
                          lineHeight: '1.6'
                        }}>
                          Click "CALC QUOTES" to generate
                          <br />market making quotes
                        </div>
                      </div>
                    )}
                  </div>
                </div>
              </div>
            </>
          )}
        </div>
      </div>
    </div>
  );
}
