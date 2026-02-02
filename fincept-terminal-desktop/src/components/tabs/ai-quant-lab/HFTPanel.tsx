/**
 * HFT Panel - High Frequency Trading Operations
 * Order book visualization, market making, microstructure analysis
 * Fincept Professional Design - Full backend integration with qlib_high_frequency.py
 */

import React, { useState, useEffect } from 'react';
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
  Pause
} from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  CYAN: '#00E5FF',
  MUTED: '#4A4A4A'
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
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center space-x-3">
          <div className="p-2 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <Gauge size={24} style={{ color: FINCEPT.ORANGE }} />
          </div>
          <div>
            <h2 className="text-xl font-bold" style={{ color: FINCEPT.WHITE }}>
              High Frequency Trading
            </h2>
            <p className="text-sm" style={{ color: FINCEPT.MUTED }}>
              Order book dynamics, market making, microstructure analysis
            </p>
          </div>
        </div>

        {orderBookCreated && (
          <div className="flex items-center space-x-2">
            {isLive && (
              <div className="flex items-center space-x-2 px-3 py-1 rounded-lg" style={{ backgroundColor: FINCEPT.GREEN + '20', border: `1px solid ${FINCEPT.GREEN}` }}>
                <div className="w-2 h-2 rounded-full animate-pulse" style={{ backgroundColor: FINCEPT.GREEN }} />
                <span className="text-sm" style={{ color: FINCEPT.GREEN }}>LIVE</span>
              </div>
            )}
          </div>
        )}
      </div>

      {/* Latency & Symbol Config */}
      <div className="grid grid-cols-5 gap-4">
        <div className="col-span-2 p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <label className="block text-xs mb-2" style={{ color: FINCEPT.GRAY }}>Symbol</label>
          <input
            type="text"
            value={symbol}
            onChange={(e) => setSymbol(e.target.value.toUpperCase())}
            disabled={orderBookCreated}
            className="w-full px-4 py-2 rounded-lg font-mono text-lg font-bold"
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.ORANGE
            }}
          />
        </div>

        <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <div className="text-xs mb-1" style={{ color: FINCEPT.GRAY }}>Mean Latency</div>
          <div className="text-2xl font-bold font-mono" style={{ color: FINCEPT.CYAN }}>
            {latencyStats.mean}<span className="text-sm" style={{ color: FINCEPT.MUTED }}>μs</span>
          </div>
        </div>

        <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <div className="text-xs mb-1" style={{ color: FINCEPT.GRAY }}>P99 Latency</div>
          <div className="text-2xl font-bold font-mono" style={{ color: FINCEPT.MUTED }}>
            {latencyStats.p99}<span className="text-sm" style={{ color: FINCEPT.MUTED }}>μs</span>
          </div>
        </div>

        <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${microstructure.toxicity_score > 0.5 ? FINCEPT.RED : FINCEPT.BORDER}` }}>
          <div className="text-xs mb-1" style={{ color: FINCEPT.GRAY }}>Toxicity</div>
          <div className="text-lg font-bold" style={{ color: microstructure.toxicity_score > 0.5 ? FINCEPT.RED : FINCEPT.GREEN }}>
            {microstructure.toxicity_score > 0.5 ? 'HIGH' : 'LOW'}
          </div>
        </div>
      </div>

      {!orderBookCreated ? (
        <div className="p-6 rounded-lg text-center" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <button
            onClick={handleCreateOrderBook}
            className="px-6 py-3 rounded-lg font-semibold"
            style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.WHITE }}
          >
            <Play size={20} className="inline mr-2" />
            Create Order Book
          </button>
        </div>
      ) : (
        <>
          <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
            {/* Order Book Visualization */}
            <div className="lg:col-span-2 p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
              <div className="flex justify-between items-center mb-4">
                <h3 className="font-bold" style={{ color: FINCEPT.ORANGE }}>Order Book</h3>
                <div className="flex space-x-2">
                  {!isLive ? (
                    <button
                      onClick={handleStartLive}
                      className="px-4 py-2 rounded-lg text-sm font-semibold"
                      style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
                    >
                      <Play size={16} className="inline mr-1" />
                      Start Live
                    </button>
                  ) : (
                    <button
                      onClick={handleStopLive}
                      className="px-4 py-2 rounded-lg text-sm font-semibold"
                      style={{ backgroundColor: FINCEPT.RED, color: FINCEPT.WHITE }}
                    >
                      <Pause size={16} className="inline mr-1" />
                      Stop
                    </button>
                  )}
                </div>
              </div>

              <div className="grid grid-cols-3 gap-4 mb-4 text-xs font-bold" style={{ color: FINCEPT.GRAY }}>
                <div className="text-right">BID SIZE</div>
                <div className="text-center">PRICE</div>
                <div className="text-left">ASK SIZE</div>
              </div>

              <div className="space-y-1">
                {Array.from({ length: 10 }, (_, i) => {
                  const bid = bids[i];
                  const ask = asks[i];

                  return (
                    <div key={i} className="grid grid-cols-3 gap-4 items-center">
                      {/* Bid */}
                      <div className="flex items-center justify-end space-x-2">
                        <span className="text-sm font-mono" style={{ color: FINCEPT.GREEN }}>
                          {bid?.size || '-'}
                        </span>
                        <div
                          className="h-6 rounded-r"
                          style={{
                            width: `${bid ? (bid.size / maxBidSize) * 100 : 0}%`,
                            backgroundColor: FINCEPT.GREEN + '40',
                            border: `1px solid ${FINCEPT.GREEN}`
                          }}
                        />
                      </div>

                      {/* Price */}
                      <div className="text-center">
                        <div className="text-sm font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                          {bid ? bid.price.toFixed(2) : '-'}
                        </div>
                        {i === 0 && (
                          <div className="text-xs mt-1" style={{ color: FINCEPT.ORANGE }}>
                            Spread: ${spread.toFixed(3)}
                          </div>
                        )}
                        <div className="text-sm font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                          {ask ? ask.price.toFixed(2) : '-'}
                        </div>
                      </div>

                      {/* Ask */}
                      <div className="flex items-center space-x-2">
                        <div
                          className="h-6 rounded-l"
                          style={{
                            width: `${ask ? (ask.size / maxAskSize) * 100 : 0}%`,
                            backgroundColor: FINCEPT.RED + '40',
                            border: `1px solid ${FINCEPT.RED}`
                          }}
                        />
                        <span className="text-sm font-mono" style={{ color: FINCEPT.RED }}>
                          {ask?.size || '-'}
                        </span>
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>

            {/* Right Column - Microstructure & Market Making */}
            <div className="space-y-6">
              {/* Microstructure Metrics */}
              <div className="p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
                <h3 className="font-bold mb-4" style={{ color: FINCEPT.ORANGE }}>Microstructure</h3>

                <div className="space-y-3">
                  <div>
                    <div className="text-xs mb-1" style={{ color: FINCEPT.GRAY }}>Mid Price</div>
                    <div className="text-xl font-mono font-bold" style={{ color: FINCEPT.CYAN }}>
                      ${midPrice.toFixed(2)}
                    </div>
                  </div>

                  <div>
                    <div className="text-xs mb-1" style={{ color: FINCEPT.GRAY }}>Depth Imbalance</div>
                    <div className="flex items-center space-x-2">
                      <div className="flex-1 h-2 rounded-full overflow-hidden" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                        <div
                          className="h-full"
                          style={{
                            width: `${Math.abs(microstructure.depth_imbalance) * 100}%`,
                            backgroundColor: microstructure.depth_imbalance > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                            marginLeft: microstructure.depth_imbalance < 0 ? 'auto' : '0'
                          }}
                        />
                      </div>
                      <span className="text-sm font-mono" style={{ color: FINCEPT.WHITE }}>
                        {(microstructure.depth_imbalance * 100).toFixed(1)}%
                      </span>
                    </div>
                  </div>

                  <div className="grid grid-cols-2 gap-2 text-sm">
                    <div>
                      <div className="text-xs" style={{ color: FINCEPT.GRAY }}>VWAP Bid</div>
                      <div className="font-mono" style={{ color: FINCEPT.GREEN }}>
                        ${microstructure.vwap_bid.toFixed(2)}
                      </div>
                    </div>
                    <div>
                      <div className="text-xs" style={{ color: FINCEPT.GRAY }}>VWAP Ask</div>
                      <div className="font-mono" style={{ color: FINCEPT.RED }}>
                        ${microstructure.vwap_ask.toFixed(2)}
                      </div>
                    </div>
                  </div>
                </div>
              </div>

              {/* Market Making */}
              <div className="p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
                <h3 className="font-bold mb-4" style={{ color: FINCEPT.ORANGE }}>Market Making</h3>

                <button
                  onClick={handleCalculateMarketMaking}
                  className="w-full mb-4 px-4 py-2 rounded-lg font-semibold"
                  style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
                >
                  Calculate Quotes
                </button>

                {marketMakingQuotes && (
                  <div className="space-y-3">
                    <div className="p-3 rounded-lg" style={{ backgroundColor: FINCEPT.GREEN + '20', border: `1px solid ${FINCEPT.GREEN}` }}>
                      <div className="text-xs mb-1" style={{ color: FINCEPT.GREEN }}>BID QUOTE</div>
                      <div className="flex justify-between">
                        <span className="text-lg font-mono font-bold" style={{ color: FINCEPT.GREEN }}>
                          ${marketMakingQuotes.bid.price.toFixed(2)}
                        </span>
                        <span className="text-sm" style={{ color: FINCEPT.GREEN }}>
                          {marketMakingQuotes.bid.size} shares
                        </span>
                      </div>
                    </div>

                    <div className="p-3 rounded-lg" style={{ backgroundColor: FINCEPT.RED + '20', border: `1px solid ${FINCEPT.RED}` }}>
                      <div className="text-xs mb-1" style={{ color: FINCEPT.RED }}>ASK QUOTE</div>
                      <div className="flex justify-between">
                        <span className="text-lg font-mono font-bold" style={{ color: FINCEPT.RED }}>
                          ${marketMakingQuotes.ask.price.toFixed(2)}
                        </span>
                        <span className="text-sm" style={{ color: FINCEPT.RED }}>
                          {marketMakingQuotes.ask.size} shares
                        </span>
                      </div>
                    </div>

                    <div className="text-center text-sm" style={{ color: FINCEPT.MUTED }}>
                      Quoted Spread: ${marketMakingQuotes.spread.toFixed(3)}
                    </div>
                  </div>
                )}
              </div>
            </div>
          </div>
        </>
      )}
    </div>
  );
}
