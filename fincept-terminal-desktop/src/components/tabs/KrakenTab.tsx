// Kraken Trading Terminal - Bloomberg-style interface
import React, { useState, useEffect, useMemo, useRef } from 'react';
import { useWebSocket } from '@/hooks/useWebSocket';
import { getWebSocketManager } from '@/services/websocket';
import { TrendingUp, TrendingDown, Activity, BookOpen, BarChart3, Zap } from 'lucide-react';
import { createChart, ColorType, IChartApi, ISeriesApi, CandlestickData, CandlestickSeries } from 'lightweight-charts';

// Popular Kraken trading pairs
const POPULAR_PAIRS = [
  'BTC/USD', 'ETH/USD', 'XRP/USD', 'SOL/USD', 'ADA/USD',
  'MATIC/USD', 'AVAX/USD', 'DOT/USD', 'LINK/USD', 'UNI/USD'
];

const OHLC_INTERVALS = [
  { label: '1m', value: 1 },
  { label: '5m', value: 5 },
  { label: '15m', value: 15 },
  { label: '30m', value: 30 },
  { label: '1h', value: 60 },
  { label: '4h', value: 240 },
  { label: '1D', value: 1440 }
];

const ORDERBOOK_DEPTHS = [10, 25, 100, 500, 1000];

interface TickerData {
  symbol: string;
  bid: number;
  ask: number;
  last: number;
  volume: number;
  vwap: number;
  high: number;
  low: number;
  change: number;
  change_pct: number;
}

interface TradeData {
  symbol: string;
  price: number;
  qty: number;
  side: 'buy' | 'sell';
  timestamp: number;
}

interface OrderBookLevel {
  price: number;
  qty: number;
}

interface OrderBookData {
  symbol: string;
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  checksum?: number;
}

interface OHLCData {
  timestamp: number | string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export default function KrakenTab() {
  const [selectedPair, setSelectedPair] = useState('BTC/USD');
  const [ohlcInterval, setOhlcInterval] = useState(60);
  const [bookDepth, setBookDepth] = useState(25);
  const [tickerData, setTickerData] = useState<TickerData | null>(null);
  const [trades, setTrades] = useState<TradeData[]>([]);
  const [orderBook, setOrderBook] = useState<OrderBookData>({ symbol: '', bids: [], asks: [] });
  const [isConnected, setIsConnected] = useState(false);
  const [chartData, setChartData] = useState<OHLCData[]>([]);
  const [searchQuery, setSearchQuery] = useState('');
  const [customPair, setCustomPair] = useState('');

  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candlestickSeriesRef = useRef<any>(null);
  const isMountedRef = useRef(true);
  const lastCandleTimeRef = useRef<number>(0);

  // Initialize WebSocket manager
  useEffect(() => {
    const manager = getWebSocketManager();
    manager.setProviderConfig('kraken', {
      provider_name: 'kraken',
      enabled: true,
      endpoint: 'wss://ws.kraken.com/v2'
    });

    // Connect to Kraken
    const connectToKraken = async () => {
      try {
        await manager.connect('kraken');
        setIsConnected(true);
      } catch (error) {
        console.error('Failed to connect to Kraken:', error);
        setIsConnected(false);
      }
    };

    connectToKraken();

    return () => {
      // Don't disconnect on unmount as other components might be using it
      setIsConnected(false);
    };
  }, []);

  // Fetch historical OHLC data from Kraken REST API
  const fetchHistoricalData = async (pair: string, interval: number) => {
    try {
      // Convert pair format for Kraken API
      // BTC/USD -> XBTUSD, ETH/USD -> ETHUSD, etc.
      let krakenPair = pair.replace('/', '');

      // Special case: BTC is XBT on Kraken
      if (krakenPair.startsWith('BTC')) {
        krakenPair = krakenPair.replace('BTC', 'XBT');
      }

      console.log(`Fetching historical data for ${pair} (Kraken: ${krakenPair}), interval: ${interval}`);

      // Fetch last 100 candles
      const response = await fetch(
        `https://api.kraken.com/0/public/OHLC?pair=${krakenPair}&interval=${interval}`
      );

      const data = await response.json();

      if (data.error && data.error.length > 0) {
        console.error('Kraken API error:', data.error);
        return [];
      }

      const result = data.result;
      const pairKey = Object.keys(result).find(key => key !== 'last');

      if (!pairKey) {
        console.warn('No data found for pair:', krakenPair);
        return [];
      }

      const ohlcData: OHLCData[] = result[pairKey].map((candle: any[]) => ({
        timestamp: candle[0],
        open: parseFloat(candle[1]),
        high: parseFloat(candle[2]),
        low: parseFloat(candle[3]),
        close: parseFloat(candle[4]),
        volume: parseFloat(candle[6])
      }));

      console.log(`Fetched ${ohlcData.length} candles for ${pair}`);
      return ohlcData;
    } catch (error) {
      console.error('Failed to fetch historical data:', error);
      return [];
    }
  };

  // Initialize chart
  useEffect(() => {
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      layout: {
        background: { type: ColorType.Solid, color: '#0a0a0a' },
        textColor: '#d1d5db',
      },
      grid: {
        vertLines: { color: '#1f2937' },
        horzLines: { color: '#1f2937' },
      },
      width: chartContainerRef.current.clientWidth,
      height: chartContainerRef.current.clientHeight,
      timeScale: {
        timeVisible: true,
        secondsVisible: false,
      },
    });

    const candlestickSeries = chart.addSeries(CandlestickSeries, {
      upColor: '#10b981',
      downColor: '#ef4444',
      borderUpColor: '#10b981',
      borderDownColor: '#ef4444',
      wickUpColor: '#10b981',
      wickDownColor: '#ef4444',
    });

    chartRef.current = chart;
    candlestickSeriesRef.current = candlestickSeries;

    // Handle resize
    const handleResize = () => {
      if (isMountedRef.current && chartContainerRef.current && chartRef.current) {
        try {
          chartRef.current.applyOptions({
            width: chartContainerRef.current.clientWidth,
            height: chartContainerRef.current.clientHeight,
          });
        } catch (error) {
          // Ignore errors if chart is being disposed
        }
      }
    };

    window.addEventListener('resize', handleResize);

    return () => {
      isMountedRef.current = false;
      window.removeEventListener('resize', handleResize);
      candlestickSeriesRef.current = null;
      chartRef.current = null;
      setTimeout(() => {
        try {
          chart.remove();
        } catch (error) {
          // Ignore errors during cleanup
        }
      }, 0);
    };
  }, []);

  // Load historical data when pair or interval changes
  useEffect(() => {
    const loadData = async () => {
      // Reset the last candle time when loading new data
      lastCandleTimeRef.current = 0;

      const historical = await fetchHistoricalData(selectedPair, ohlcInterval);
      setChartData(historical);

      if (candlestickSeriesRef.current && historical.length > 0) {
        const chartCandles = historical.map((candle, idx) => {
          // Ensure timestamp is a number (Unix timestamp in seconds)
          let time: number;

          if (typeof candle.timestamp === 'number') {
            time = candle.timestamp;
          } else if (typeof candle.timestamp === 'string') {
            time = Math.floor(new Date(candle.timestamp).getTime() / 1000);
          } else {
            console.error('Invalid timestamp type in historical data:', candle.timestamp, 'at index:', idx);
            time = 0;
          }

          // If it's in milliseconds, convert to seconds
          if (time > 10000000000) {
            time = Math.floor(time / 1000);
          }

          return {
            time,
            open: candle.open,
            high: candle.high,
            low: candle.low,
            close: candle.close,
          };
        }).filter(candle => candle.time > 0) // Filter out invalid candles
          .sort((a, b) => a.time - b.time); // Sort by time ascending

        console.log('Setting chart data with', chartCandles.length, 'candles, first:', chartCandles[0], 'last:', chartCandles[chartCandles.length - 1]);

        // Use setData to replace all chart data
        candlestickSeriesRef.current.setData(chartCandles);

        // Track the last candle time so we can update it with live data
        const lastCandle = chartCandles[chartCandles.length - 1];
        if (lastCandle) {
          lastCandleTimeRef.current = lastCandle.time;
          console.log('Last candle time set to:', lastCandleTimeRef.current);
        }
      }
    };

    loadData();
  }, [selectedPair, ohlcInterval]);

  // Ticker subscription
  const { message: tickerMessage } = useWebSocket(
    isConnected ? `kraken.ticker.${selectedPair}` : null,
    null,
    { autoSubscribe: true }
  );

  // Trade subscription
  const { message: tradeMessage } = useWebSocket(
    isConnected ? `kraken.trade.${selectedPair}` : null,
    null,
    { autoSubscribe: true }
  );

  // Order book subscription
  const { message: bookMessage } = useWebSocket(
    isConnected ? `kraken.book.${selectedPair}` : null,
    null,
    { autoSubscribe: true, params: { depth: bookDepth } }
  );

  // OHLC subscription
  const { message: ohlcMessage } = useWebSocket(
    isConnected ? `kraken.ohlc.${selectedPair}` : null,
    null,
    { autoSubscribe: true, params: { interval: ohlcInterval } }
  );

  // Update ticker data
  useEffect(() => {
    if (tickerMessage?.data) {
      console.log('Ticker message received:', tickerMessage.data);
      setTickerData(tickerMessage.data as TickerData);
    }
  }, [tickerMessage]);

  // Update trades
  useEffect(() => {
    if (tradeMessage?.data) {
      const trade = tradeMessage.data as TradeData;
      setTrades(prev => [trade, ...prev].slice(0, 100));
    }
  }, [tradeMessage]);

  // Update order book - merge instead of replace
  useEffect(() => {
    if (bookMessage?.data) {
      const newBook = bookMessage.data as OrderBookData;
      const messageType = (bookMessage.data as any).messageType;

      if (messageType === 'snapshot') {
        // Snapshot: replace entire book
        setOrderBook(newBook);
      } else {
        // Update: merge with existing book
        setOrderBook(prev => {
          const updatedBids = [...prev.bids];
          const updatedAsks = [...prev.asks];

          // Update bids
          newBook.bids?.forEach(newBid => {
            const index = updatedBids.findIndex(b => b.price === newBid.price);
            if (newBid.qty === 0) {
              // Remove if quantity is 0
              if (index !== -1) updatedBids.splice(index, 1);
            } else {
              if (index !== -1) {
                // Update existing
                updatedBids[index] = newBid;
              } else {
                // Add new
                updatedBids.push(newBid);
              }
            }
          });

          // Update asks
          newBook.asks?.forEach(newAsk => {
            const index = updatedAsks.findIndex(a => a.price === newAsk.price);
            if (newAsk.qty === 0) {
              // Remove if quantity is 0
              if (index !== -1) updatedAsks.splice(index, 1);
            } else {
              if (index !== -1) {
                // Update existing
                updatedAsks[index] = newAsk;
              } else {
                // Add new
                updatedAsks.push(newAsk);
              }
            }
          });

          // Sort bids descending, asks ascending, and limit to depth
          updatedBids.sort((a, b) => b.price - a.price);
          updatedAsks.sort((a, b) => a.price - b.price);

          return {
            symbol: newBook.symbol || prev.symbol,
            bids: updatedBids.slice(0, bookDepth),
            asks: updatedAsks.slice(0, bookDepth),
            checksum: newBook.checksum
          };
        });
      }
    }
  }, [bookMessage, bookDepth]);

  // Update chart with live OHLC data
  useEffect(() => {
    if (ohlcMessage?.data && candlestickSeriesRef.current) {
      try {
        const ohlc = ohlcMessage.data as any;

        // Parse the interval_begin timestamp to determine the candle's time
        // This represents when the candle period started
        let candleTime: number;

        if (ohlc.interval_begin) {
          // Parse ISO string from interval_begin
          candleTime = Math.floor(new Date(ohlc.interval_begin).getTime() / 1000);
        } else {
          // Fallback to the message timestamp
          let timestamp = ohlcMessage.timestamp;
          candleTime = typeof timestamp === 'number' ? Math.floor(timestamp / 1000) : 0;
        }

        // Validate that candleTime is actually a number
        if (typeof candleTime !== 'number' || isNaN(candleTime)) {
          console.error('Invalid candleTime:', candleTime, 'type:', typeof candleTime);
          return;
        }

        // Skip old candles (this can happen when historical data is still loading)
        // Only update if the candle is >= the last tracked candle
        if (lastCandleTimeRef.current > 0 && candleTime < lastCandleTimeRef.current) {
          console.log('Skipping old candle:', candleTime, 'last:', lastCandleTimeRef.current);
          return;
        }

        console.log('Updating candle:', candleTime, 'last:', lastCandleTimeRef.current);

        // Check if this is the same candle we're already tracking (update)
        // or a new candle (which means the previous interval completed)
        if (lastCandleTimeRef.current === candleTime) {
          // Same candle - update it (this makes it "animate" like TradingView)
          candlestickSeriesRef.current.update({
            time: candleTime as any,
            open: parseFloat(ohlc.open),
            high: parseFloat(ohlc.high),
            low: parseFloat(ohlc.low),
            close: parseFloat(ohlc.close),
          });
        } else {
          // New candle - the interval completed
          lastCandleTimeRef.current = candleTime;
          candlestickSeriesRef.current.update({
            time: candleTime as any,
            open: parseFloat(ohlc.open),
            high: parseFloat(ohlc.high),
            low: parseFloat(ohlc.low),
            close: parseFloat(ohlc.close),
          });
        }
      } catch (error) {
        console.error('Failed to update chart with OHLC data:', error, ohlcMessage);
      }
    }
  }, [ohlcMessage]);

  const priceChangeColor = useMemo(() => {
    if (!tickerData) return 'text-gray-400';
    return tickerData.change >= 0 ? 'text-green-400' : 'text-red-400';
  }, [tickerData]);

  const spread = useMemo(() => {
    if (!tickerData) return 0;
    return ((tickerData.ask - tickerData.bid) / tickerData.bid * 100).toFixed(4);
  }, [tickerData]);

  // Filter popular pairs based on search query
  const filteredPairs = useMemo(() => {
    if (!searchQuery) return POPULAR_PAIRS;
    const query = searchQuery.toLowerCase();
    return POPULAR_PAIRS.filter(pair => pair.toLowerCase().includes(query));
  }, [searchQuery]);

  // Handle custom pair submission
  const handleCustomPairSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (customPair.trim()) {
      // Format the pair (add / if not present)
      const formattedPair = customPair.includes('/')
        ? customPair.toUpperCase()
        : customPair.toUpperCase().replace(/([A-Z]{3,})([A-Z]{3,})/, '$1/$2');
      setSelectedPair(formattedPair);
      setCustomPair('');
      setSearchQuery('');
    }
  };

  return (
    <div className="h-full bg-[#0a0a0a] text-gray-100 flex flex-col overflow-hidden font-mono">
      {/* Header */}
      <div className="bg-[#111111] border-b border-gray-800 px-4 py-2 flex items-center justify-between">
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <Zap className="w-5 h-5 text-yellow-400" />
            <span className="text-lg font-bold text-yellow-400">KRAKEN</span>
            <span className="text-xs text-gray-500">TERMINAL</span>
          </div>
          <div className="h-6 w-px bg-gray-700" />
          <div className={`flex items-center gap-2 ${priceChangeColor}`}>
            <span className="text-2xl font-bold">
              {selectedPair}
            </span>
            {tickerData && (
              <>
                {tickerData.change >= 0 ? <TrendingUp className="w-5 h-5" /> : <TrendingDown className="w-5 h-5" />}
                <span className="text-sm">
                  {tickerData.change >= 0 ? '+' : ''}{tickerData.change_pct?.toFixed(2)}%
                </span>
              </>
            )}
          </div>
        </div>
        <div className="flex items-center gap-2">
          <div className={`w-2 h-2 rounded-full ${isConnected ? 'bg-green-400 animate-pulse' : 'bg-red-400'}`} />
          <span className="text-xs text-gray-400">
            {isConnected ? 'LIVE' : 'DISCONNECTED'}
          </span>
        </div>
      </div>

      {/* Symbol Selector */}
      <div className="bg-[#0d0d0d] border-b border-gray-800 px-4 py-2">
        <div className="flex items-center gap-3">
          {/* Search Input */}
          <div className="flex items-center gap-2 flex-shrink-0">
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search pairs..."
              className="bg-[#1a1a1a] text-gray-200 text-xs px-3 py-1 border border-gray-700 focus:outline-none focus:border-orange-500 w-32"
            />
            {/* Custom Pair Form */}
            <form onSubmit={handleCustomPairSubmit} className="flex items-center gap-1">
              <input
                type="text"
                value={customPair}
                onChange={(e) => setCustomPair(e.target.value)}
                placeholder="BTC/USD"
                className="bg-[#1a1a1a] text-gray-200 text-xs px-3 py-1 border border-gray-700 focus:outline-none focus:border-orange-500 w-24"
              />
              <button
                type="submit"
                className="bg-orange-600 text-black text-xs px-2 py-1 font-semibold hover:bg-orange-500 transition-colors"
              >
                Go
              </button>
            </form>
          </div>

          {/* Pair Buttons */}
          <div className="flex items-center gap-2 overflow-x-auto flex-1">
            {filteredPairs.map(pair => (
              <button
                key={pair}
                onClick={() => setSelectedPair(pair)}
                className={`px-3 py-1 rounded text-xs font-semibold whitespace-nowrap transition-colors ${
                  selectedPair === pair
                    ? 'bg-orange-600 text-black'
                    : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                }`}
              >
                {pair}
              </button>
            ))}
            {filteredPairs.length === 0 && (
              <span className="text-xs text-gray-500">No pairs found</span>
            )}
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left Panel - Ticker & Stats */}
        <div className="w-80 border-r border-gray-800 flex flex-col">
          {/* Price Display */}
          <div className="bg-[#0d0d0d] border-b border-gray-800 p-4">
            <div className="mb-2">
              <div className="text-xs text-gray-500 mb-1">LAST PRICE</div>
              <div className={`text-3xl font-bold ${priceChangeColor}`}>
                ${tickerData?.last?.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 }) || '---'}
              </div>
            </div>
            <div className="grid grid-cols-2 gap-3 text-xs">
              <div>
                <div className="text-gray-500">24H CHANGE</div>
                <div className={priceChangeColor}>
                  {tickerData && tickerData.change !== undefined
                    ? `${tickerData.change >= 0 ? '+' : ''}${tickerData.change.toFixed(2)}`
                    : '---'}
                  <span className="ml-1">
                    ({tickerData?.change_pct?.toFixed(2) || '---'}%)
                  </span>
                </div>
              </div>
              <div>
                <div className="text-gray-500">SPREAD</div>
                <div className="text-gray-300">{spread}%</div>
              </div>
              <div>
                <div className="text-gray-500">24H HIGH</div>
                <div className="text-green-400">${tickerData?.high?.toLocaleString() || '---'}</div>
              </div>
              <div>
                <div className="text-gray-500">24H LOW</div>
                <div className="text-red-400">${tickerData?.low?.toLocaleString() || '---'}</div>
              </div>
              <div>
                <div className="text-gray-500">24H VOLUME</div>
                <div className="text-gray-300">{tickerData?.volume?.toFixed(2) || '---'}</div>
              </div>
              <div>
                <div className="text-gray-500">VWAP</div>
                <div className="text-gray-300">${tickerData?.vwap?.toFixed(2) || '---'}</div>
              </div>
            </div>
          </div>

          {/* BID/ASK */}
          <div className="bg-[#0d0d0d] border-b border-gray-800 p-4">
            <div className="grid grid-cols-2 gap-3">
              <div>
                <div className="text-xs text-gray-500 mb-1">BID</div>
                <div className="text-lg font-bold text-green-400">
                  ${tickerData?.bid?.toLocaleString('en-US', { minimumFractionDigits: 2 }) || '---'}
                </div>
              </div>
              <div>
                <div className="text-xs text-gray-500 mb-1">ASK</div>
                <div className="text-lg font-bold text-red-400">
                  ${tickerData?.ask?.toLocaleString('en-US', { minimumFractionDigits: 2 }) || '---'}
                </div>
              </div>
            </div>
          </div>

          {/* Trades Feed */}
          <div className="flex-1 flex flex-col bg-[#0a0a0a]">
            <div className="bg-[#0d0d0d] border-b border-gray-800 px-4 py-2 flex items-center gap-2">
              <Activity className="w-4 h-4 text-orange-500" />
              <span className="text-xs font-semibold text-gray-400">RECENT TRADES</span>
            </div>
            <div className="flex-1 overflow-y-auto">
              <div className="text-[10px] px-2 py-1 bg-[#0d0d0d] grid grid-cols-3 gap-2 text-gray-500 sticky top-0">
                <div>TIME</div>
                <div className="text-right">PRICE</div>
                <div className="text-right">SIZE</div>
              </div>
              {trades.map((trade, idx) => (
                <div
                  key={`${trade.timestamp}-${idx}`}
                  className={`text-xs px-2 py-1 grid grid-cols-3 gap-2 border-b border-gray-900 hover:bg-gray-900 ${
                    trade.side === 'buy' ? 'bg-green-950/10' : 'bg-red-950/10'
                  }`}
                >
                  <div className="text-gray-500">
                    {new Date(trade.timestamp).toLocaleTimeString('en-US', {
                      hour12: false,
                      hour: '2-digit',
                      minute: '2-digit',
                      second: '2-digit'
                    })}
                  </div>
                  <div className={`text-right font-semibold ${trade.side === 'buy' ? 'text-green-400' : 'text-red-400'}`}>
                    ${trade.price.toFixed(2)}
                  </div>
                  <div className="text-right text-gray-300">
                    {trade.qty.toFixed(6)}
                  </div>
                </div>
              ))}
              {trades.length === 0 && (
                <div className="p-4 text-center text-gray-600 text-xs">
                  Waiting for trades...
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Center Panel - Chart */}
        <div className="flex-1 flex flex-col bg-[#0a0a0a]">
          {/* Chart Controls */}
          <div className="bg-[#0d0d0d] border-b border-gray-800 px-4 py-2 flex items-center justify-between">
            <div className="flex items-center gap-2">
              <BarChart3 className="w-4 h-4 text-orange-500" />
              <span className="text-xs font-semibold text-gray-400">CANDLESTICK CHART</span>
            </div>
            <div className="flex items-center gap-2">
              {OHLC_INTERVALS.map(interval => (
                <button
                  key={interval.value}
                  onClick={() => setOhlcInterval(interval.value)}
                  className={`px-2 py-1 rounded text-[10px] font-semibold transition-colors ${
                    ohlcInterval === interval.value
                      ? 'bg-orange-600 text-black'
                      : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                  }`}
                >
                  {interval.label}
                </button>
              ))}
            </div>
          </div>

          {/* Chart */}
          <div ref={chartContainerRef} className="flex-1 bg-[#0a0a0a]" />
        </div>

        {/* Right Panel - Order Book */}
        <div className="w-96 border-l border-gray-800 flex flex-col bg-[#0a0a0a]">
          {/* Order Book Header */}
          <div className="bg-[#0d0d0d] border-b border-gray-800 px-4 py-2 flex items-center justify-between">
            <div className="flex items-center gap-2">
              <BookOpen className="w-4 h-4 text-orange-500" />
              <span className="text-xs font-semibold text-gray-400">ORDER BOOK</span>
            </div>
            <select
              value={bookDepth}
              onChange={(e) => setBookDepth(Number(e.target.value))}
              className="bg-gray-800 text-gray-300 text-[10px] px-2 py-1 rounded border border-gray-700 focus:outline-none focus:border-orange-500"
            >
              {ORDERBOOK_DEPTHS.map(depth => (
                <option key={depth} value={depth}>
                  DEPTH: {depth}
                </option>
              ))}
            </select>
          </div>

          {/* Order Book Content */}
          <div className="flex-1 flex flex-col overflow-hidden">
            {/* Asks (Sell Orders) */}
            <div className="flex-1 flex flex-col-reverse overflow-y-auto">
              <div className="text-[10px] px-3 py-1 bg-[#0d0d0d] grid grid-cols-3 gap-2 text-gray-500 sticky bottom-0">
                <div className="text-right">PRICE</div>
                <div className="text-right">SIZE</div>
                <div className="text-right">TOTAL</div>
              </div>
              {orderBook.asks.slice(0, 15).reverse().map((level, idx) => {
                const total = orderBook.asks.slice(0, idx + 1).reduce((sum, l) => sum + l.qty, 0);
                const maxTotal = Math.max(...orderBook.asks.slice(0, 15).map((_, i) =>
                  orderBook.asks.slice(0, i + 1).reduce((sum, l) => sum + l.qty, 0)
                ));
                const percentage = maxTotal > 0 ? (total / maxTotal) * 100 : 0;

                return (
                  <div
                    key={`ask-${level.price}-${idx}`}
                    className="text-xs px-3 py-0.5 grid grid-cols-3 gap-2 relative hover:bg-red-950/20"
                  >
                    <div
                      className="absolute inset-0 bg-red-950/20"
                      style={{ width: `${percentage}%`, right: 0, left: 'auto' }}
                    />
                    <div className="text-right text-red-400 font-semibold relative z-10">
                      {level.price.toFixed(2)}
                    </div>
                    <div className="text-right text-gray-300 relative z-10">
                      {level.qty.toFixed(6)}
                    </div>
                    <div className="text-right text-gray-500 relative z-10">
                      {total.toFixed(6)}
                    </div>
                  </div>
                );
              })}
            </div>

            {/* Spread */}
            <div className="bg-[#0d0d0d] border-y border-gray-800 px-3 py-2 text-center">
              <div className="text-[10px] text-gray-500">SPREAD</div>
              <div className="text-sm font-bold text-yellow-400">{spread}%</div>
            </div>

            {/* Bids (Buy Orders) */}
            <div className="flex-1 overflow-y-auto">
              <div className="text-[10px] px-3 py-1 bg-[#0d0d0d] grid grid-cols-3 gap-2 text-gray-500 sticky top-0">
                <div className="text-right">PRICE</div>
                <div className="text-right">SIZE</div>
                <div className="text-right">TOTAL</div>
              </div>
              {orderBook.bids.slice(0, 15).map((level, idx) => {
                const total = orderBook.bids.slice(0, idx + 1).reduce((sum, l) => sum + l.qty, 0);
                const maxTotal = Math.max(...orderBook.bids.slice(0, 15).map((_, i) =>
                  orderBook.bids.slice(0, i + 1).reduce((sum, l) => sum + l.qty, 0)
                ));
                const percentage = maxTotal > 0 ? (total / maxTotal) * 100 : 0;

                return (
                  <div
                    key={`bid-${level.price}-${idx}`}
                    className="text-xs px-3 py-0.5 grid grid-cols-3 gap-2 relative hover:bg-green-950/20"
                  >
                    <div
                      className="absolute inset-0 bg-green-950/20"
                      style={{ width: `${percentage}%`, right: 0, left: 'auto' }}
                    />
                    <div className="text-right text-green-400 font-semibold relative z-10">
                      {level.price.toFixed(2)}
                    </div>
                    <div className="text-right text-gray-300 relative z-10">
                      {level.qty.toFixed(6)}
                    </div>
                    <div className="text-right text-gray-500 relative z-10">
                      {total.toFixed(6)}
                    </div>
                  </div>
                );
              })}
              {orderBook.bids.length === 0 && orderBook.asks.length === 0 && (
                <div className="p-4 text-center text-gray-600 text-xs">
                  Loading order book...
                </div>
              )}
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <div className="bg-[#0d0d0d] border-t border-gray-800 px-4 py-1 flex items-center justify-between text-[10px] text-gray-500">
        <div className="flex items-center gap-4">
          <span>KRAKEN WEBSOCKET v2</span>
          <span>•</span>
          <span>PAIR: {selectedPair}</span>
          <span>•</span>
          <span>INTERVAL: {OHLC_INTERVALS.find(i => i.value === ohlcInterval)?.label}</span>
          <span>•</span>
          <span>DEPTH: {bookDepth}</span>
        </div>
        <div className="flex items-center gap-2">
          <span>{new Date().toLocaleTimeString('en-US', { hour12: false })}</span>
        </div>
      </div>
    </div>
  );
}
