// Fyers Market Data Panel - Bloomberg-style Screener
// Real-time quotes with autocomplete search and dynamic price flashing

import React, { useState, useEffect, useRef } from 'react';
import { Search, TrendingUp, X, Plus, RefreshCw } from 'lucide-react';
import { fyersService, FyersQuote } from '../../../services/fyersService';

interface PriceFlash {
  [symbol: string]: 'up' | 'down' | null;
}

interface HistoricalData {
  [symbol: string]: number[];
}

interface OrderModalState {
  isOpen: boolean;
  symbol: string;
  side: 'BUY' | 'SELL';
  ltp: number;
}

interface GTTModalState {
  isOpen: boolean;
  symbol: string;
  ltp: number;
}

// Nifty 50 stocks
const NIFTY_50_SYMBOLS = [
  'NSE:NIFTY50-INDEX',
  'NSE:RELIANCE-EQ',
  'NSE:TCS-EQ',
  'NSE:HDFCBANK-EQ',
  'NSE:INFY-EQ',
  'NSE:ICICIBANK-EQ',
  'NSE:HINDUNILVR-EQ',
  'NSE:ITC-EQ',
  'NSE:SBIN-EQ',
  'NSE:BHARTIARTL-EQ',
  'NSE:KOTAKBANK-EQ',
  'NSE:BAJFINANCE-EQ',
  'NSE:LT-EQ',
  'NSE:HCLTECH-EQ',
  'NSE:ASIANPAINT-EQ',
  'NSE:MARUTI-EQ',
  'NSE:AXISBANK-EQ',
  'NSE:SUNPHARMA-EQ',
  'NSE:TITAN-EQ',
  'NSE:ULTRACEMCO-EQ',
  'NSE:WIPRO-EQ',
  'NSE:ONGC-EQ',
  'NSE:NTPC-EQ',
  'NSE:NESTLEIND-EQ',
  'NSE:POWERGRID-EQ',
  'NSE:M&M-EQ',
  'NSE:TATAMOTORS-EQ',
  'NSE:TATASTEEL-EQ',
  'NSE:BAJAJFINSV-EQ',
  'NSE:ADANIENT-EQ',
  'NSE:TECHM-EQ',
  'NSE:ADANIPORTS-EQ',
  'NSE:JSWSTEEL-EQ',
  'NSE:HINDALCO-EQ',
  'NSE:INDUSINDBK-EQ',
  'NSE:DIVISLAB-EQ',
  'NSE:BRITANNIA-EQ',
  'NSE:TATACONSUM-EQ',
  'NSE:COALINDIA-EQ',
  'NSE:CIPLA-EQ',
  'NSE:EICHERMOT-EQ',
  'NSE:DRREDDY-EQ',
  'NSE:GRASIM-EQ',
  'NSE:APOLLOHOSP-EQ',
  'NSE:BPCL-EQ',
  'NSE:UPL-EQ',
  'NSE:LTIM-EQ',
  'NSE:HEROMOTOCO-EQ',
  'NSE:BAJAJ-AUTO-EQ',
  'NSE:ADANIGREEN-EQ'
];

const FyersMarketDataPanel: React.FC = () => {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedSymbols, setSelectedSymbols] = useState<string[]>(NIFTY_50_SYMBOLS);
  const [quotes, setQuotes] = useState<Record<string, FyersQuote>>({});
  const [previousQuotes, setPreviousQuotes] = useState<Record<string, FyersQuote>>({});
  const [priceFlash, setPriceFlash] = useState<PriceFlash>({});
  const [isLoading, setIsLoading] = useState(false);
  const [symbolMasterLoaded, setSymbolMasterLoaded] = useState(false);
  const [marketOpen, setMarketOpen] = useState(false);
  const [showSuggestions, setShowSuggestions] = useState(false);
  const [historicalData, setHistoricalData] = useState<HistoricalData>({});
  const [orderModal, setOrderModal] = useState<OrderModalState>({ isOpen: false, symbol: '', side: 'BUY', ltp: 0 });
  const [gttModal, setGttModal] = useState<GTTModalState>({ isOpen: false, symbol: '', ltp: 0 });
  const [orderQty, setOrderQty] = useState(1);
  const [orderType, setOrderType] = useState<1 | 2 | 3>(2); // 1=Limit, 2=Market, 3=Stop
  const [limitPrice, setLimitPrice] = useState(0);
  const [stopPrice, setStopPrice] = useState(0);
  const [productType, setProductType] = useState<'INTRADAY' | 'MARGIN' | 'CNC'>('INTRADAY');
  const [gttSide, setGttSide] = useState<1 | -1>(1); // 1=Buy, -1=Sell
  const [gttQty, setGttQty] = useState(1);
  const [gttPrice, setGttPrice] = useState(0);
  const [gttTriggerPrice, setGttTriggerPrice] = useState(0);
  const [gttProductType, setGttProductType] = useState<'INTRADAY' | 'MARGIN' | 'CNC'>('CNC');
  const searchInputRef = useRef<HTMLInputElement>(null);
  const scrollContainerRef = useRef<HTMLDivElement>(null);

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_RED = '#FF0000';

  useEffect(() => {
    const isOpen = fyersService.isMarketOpen();
    setMarketOpen(isOpen);

    fyersService.loadSymbolMaster().then(() => {
      setSymbolMasterLoaded(true);
    }).catch(console.error);

    loadQuotes();

    // Load historical data incrementally - only load missing data
    const loadMissingHistoricalData = async () => {
      const missingSymbols = selectedSymbols.filter(symbol => !historicalData[symbol]);
      if (missingSymbols.length > 0) {
        await loadHistoricalDataForSymbols(missingSymbols);
      }
    };

    loadMissingHistoricalData();

    const refreshInterval = isOpen ? 3000 : 30000;
    const interval = setInterval(() => {
      loadQuotes();
      setMarketOpen(fyersService.isMarketOpen());
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [selectedSymbols]);

  const loadQuotes = async () => {
    if (selectedSymbols.length === 0) return;

    setIsLoading(true);
    try {
      setPreviousQuotes(quotes);
      const data = await fyersService.getQuotes(selectedSymbols);
      setQuotes(data);

      // Detect price changes and flash
      const newFlash: PriceFlash = {};
      Object.keys(data).forEach(symbol => {
        if (previousQuotes[symbol] && data[symbol]) {
          const oldPrice = previousQuotes[symbol].ltp || previousQuotes[symbol].lp || 0;
          const newPrice = data[symbol].ltp || data[symbol].lp || 0;
          if (oldPrice !== undefined && newPrice !== undefined && oldPrice !== newPrice) {
            newFlash[symbol] = newPrice > oldPrice ? 'up' : 'down';
          }
        }
      });
      setPriceFlash(newFlash);

      setTimeout(() => setPriceFlash({}), 500);
    } catch (error) {
      console.error('[Market Data] Failed to load quotes:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const loadHistoricalDataForSymbols = async (symbols: string[]) => {
    try {
      const now = Math.floor(Date.now() / 1000);
      const thirtyDaysAgo = now - (30 * 24 * 60 * 60);

      // Load history ONE at a time with 1 second delay to respect rate limit
      for (let i = 0; i < symbols.length; i++) {
        const symbol = symbols[i];

        try {
          // Save scroll position before updating
          const scrollTop = scrollContainerRef.current?.scrollTop || 0;

          const history = await fyersService.getHistory({
            symbol,
            resolution: 'D',
            date_format: '0',
            range_from: thirtyDaysAgo.toString(),
            range_to: now.toString(),
            cont_flag: '1'
          });

          if (history.s === 'ok' && history.candles && history.candles.length > 0) {
            // Get last 15 data points for smoother chart
            const closePrices = history.candles.map((candle: any[]) => candle[4]);

            // Update state without triggering scroll jumps
            setHistoricalData(prev => ({ ...prev, [symbol]: closePrices.slice(-15) }));

            // Restore scroll position after state update
            requestAnimationFrame(() => {
              if (scrollContainerRef.current) {
                scrollContainerRef.current.scrollTop = scrollTop;
              }
            });
          }
        } catch (err: any) {
          if (err?.code === 429) {
            console.warn(`Rate limit for ${symbol}, waiting longer...`);
            await new Promise(resolve => setTimeout(resolve, 2000));
          }
        }

        // 1 second delay between each request to respect rate limit
        if (i < symbols.length - 1) {
          await new Promise(resolve => setTimeout(resolve, 1000));
        }
      }
    } catch (error) {
      console.error('[Market Data] Failed to load historical data:', error);
    }
  };

  const loadHistoricalData = async () => {
    await loadHistoricalDataForSymbols(selectedSymbols);
  };

  const handleAddSymbol = (symbol: string) => {
    if (!symbol.trim() || selectedSymbols.includes(symbol)) return;
    setSelectedSymbols([...selectedSymbols, symbol]);
    setSearchQuery('');
    setShowSuggestions(false);
  };

  const handleRemoveSymbol = (symbol: string) => {
    setSelectedSymbols(selectedSymbols.filter(s => s !== symbol));
  };

  const handleOpenOrderModal = (symbol: string, side: 'BUY' | 'SELL', ltp: number) => {
    setOrderModal({ isOpen: true, symbol, side, ltp });
    setLimitPrice(ltp);
    setOrderQty(1);
    setOrderType(2); // Market order by default
  };

  const handleOpenGTTModal = (symbol: string, ltp: number) => {
    setGttModal({ isOpen: true, symbol, ltp });
    setGttPrice(ltp);
    setGttTriggerPrice(ltp);
    setGttQty(1);
    setGttSide(1);
    setGttProductType('CNC');
  };

  const handlePlaceOrder = async () => {
    try {
      const orderPayload = {
        symbol: orderModal.symbol,
        qty: orderQty,
        type: orderType, // 1=Limit, 2=Market, 3=Stop
        side: orderModal.side === 'BUY' ? 1 : -1,
        productType: productType,
        limitPrice: orderType === 1 ? limitPrice : 0,
        stopPrice: orderType === 3 ? stopPrice : 0,
        disclosedQty: 0,
        validity: 'DAY',
        offlineOrder: false,
        stopLoss: 0,
        takeProfit: 0,
        orderTag: 'fincept-terminal'
      };

      const response = await fyersService.placeOrder(orderPayload);

      if (response.orderId || (response as any).s === 'ok') {
        alert(`Order placed successfully! Order ID: ${response.orderId || (response as any).id || 'N/A'}`);
        setOrderModal({ isOpen: false, symbol: '', side: 'BUY', ltp: 0 });
      } else {
        alert(`Order failed: ${response.message || 'Unknown error'}`);
      }
    } catch (error: any) {
      console.error('Order placement error:', error);
      alert(`Order failed: ${error.message || 'Unknown error'}`);
    }
  };

  const handlePlaceGTTOrder = async () => {
    try {
      const gttPayload = {
        side: gttSide,
        symbol: gttModal.symbol,
        productType: gttProductType,
        orderInfo: {
          leg1: {
            price: gttPrice,
            triggerPrice: gttTriggerPrice,
            qty: gttQty
          }
        }
      };

      const response = await fyersService.placeGTTOrder(gttPayload);

      if (response.s === 'ok') {
        alert(`GTT Order placed successfully! Order ID: ${response.id || 'N/A'}`);
        setGttModal({ isOpen: false, symbol: '', ltp: 0 });
      } else {
        alert(`GTT Order failed: ${response.message || 'Unknown error'}`);
      }
    } catch (error: any) {
      console.error('GTT Order placement error:', error);
      alert(`GTT Order failed: ${error.message || 'Unknown error'}`);
    }
  };

  const handleManualRefresh = () => {
    loadQuotes();
    loadHistoricalData();
  };

  const searchResults = () => {
    if (!symbolMasterLoaded || !searchQuery || searchQuery.length < 2) return [];
    return fyersService.searchSymbols(searchQuery, 10);
  };

  const formatCurrency = (amount: number | undefined) => {
    if (amount === undefined || amount === null || isNaN(amount)) return '...';
    return `₹${amount.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;
  };

  const formatPercent = (value: number | undefined) => {
    if (value === undefined || value === null || isNaN(value)) return '...';
    return `${value >= 0 ? '+' : ''}${value.toFixed(2)}%`;
  };

  const getFlashColor = (symbol: string) => {
    const flash = priceFlash[symbol];
    if (!flash) return 'transparent';
    return flash === 'up' ? 'rgba(0, 200, 0, 0.3)' : 'rgba(255, 0, 0, 0.3)';
  };

  const getPriceColor = (symbol: string) => {
    const quote = quotes[symbol];
    if (!quote || !quote.ch) return BLOOMBERG_WHITE;
    return quote.ch > 0 ? BLOOMBERG_GREEN : quote.ch < 0 ? BLOOMBERG_RED : BLOOMBERG_WHITE;
  };

  const Sparkline: React.FC<{ data: number[], width: number, height: number, color: string }> = ({ data, width, height, color }) => {
    if (!data || data.length < 2) return null;

    const min = Math.min(...data);
    const max = Math.max(...data);
    const range = max - min || 1;

    // Create area chart path
    const areaPoints: string[] = [];
    const linePoints: string[] = [];

    data.forEach((value, index) => {
      const x = (index / (data.length - 1)) * width;
      const y = height - ((value - min) / range) * (height - 4); // Leave 4px padding
      linePoints.push(`${x},${y}`);
    });

    // Create area path (line + bottom closing)
    const areaPath = `M 0,${height} L ${linePoints.join(' L ')} L ${width},${height} Z`;

    return (
      <svg width={width} height={height} style={{ display: 'block', margin: 'auto' }}>
        {/* Area fill */}
        <path
          d={areaPath}
          fill={color}
          fillOpacity="0.15"
        />
        {/* Line */}
        <polyline
          points={linePoints.join(' ')}
          fill="none"
          stroke={color}
          strokeWidth="1.5"
          strokeLinecap="round"
          strokeLinejoin="round"
        />
      </svg>
    );
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      display: 'flex',
      flexDirection: 'column',
      fontFamily: 'Consolas, monospace'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '8px 12px',
        flexShrink: 0
      }}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '8px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <TrendingUp size={12} color={BLOOMBERG_ORANGE} />
            <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
              MARKET SCREENER
            </span>
            <span style={{
              fontSize: '9px',
              color: marketOpen ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              fontWeight: 'bold',
              padding: '2px 6px',
              border: `1px solid ${marketOpen ? BLOOMBERG_GREEN : BLOOMBERG_RED}`,
              backgroundColor: marketOpen ? 'rgba(0, 200, 0, 0.1)' : 'rgba(255, 0, 0, 0.1)'
            }}>
              {marketOpen ? 'LIVE' : 'CLOSED'}
            </span>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <button
              onClick={handleManualRefresh}
              style={{
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_ORANGE}`,
                color: BLOOMBERG_ORANGE,
                padding: '4px 10px',
                fontSize: '9px',
                cursor: 'pointer',
                fontWeight: 'bold',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                fontFamily: 'Consolas, monospace'
              }}
            >
              <RefreshCw size={10} />
              REFRESH CHARTS
            </button>
            <span style={{ color: BLOOMBERG_GRAY, fontSize: '9px' }}>
              {selectedSymbols.length} SYMBOLS
            </span>
          </div>
        </div>

        {/* Search with Autocomplete */}
        <div style={{ position: 'relative' }}>
          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            <Search size={12} color={BLOOMBERG_GRAY} />
            <input
              ref={searchInputRef}
              type="text"
              value={searchQuery}
              onChange={(e) => {
                setSearchQuery(e.target.value);
                setShowSuggestions(e.target.value.length >= 2);
              }}
              onFocus={() => setShowSuggestions(searchQuery.length >= 2)}
              onBlur={() => setTimeout(() => setShowSuggestions(false), 200)}
              placeholder="Search symbols... (e.g., RELIANCE, TCS, INFY)"
              style={{
                flex: 1,
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px 8px',
                fontSize: '10px',
                outline: 'none'
              }}
            />
          </div>

          {/* Autocomplete Dropdown */}
          {showSuggestions && searchResults().length > 0 && (
            <div style={{
              position: 'absolute',
              top: '100%',
              left: 0,
              right: 0,
              backgroundColor: BLOOMBERG_PANEL_BG,
              border: `1px solid ${BLOOMBERG_ORANGE}`,
              maxHeight: '200px',
              overflowY: 'auto',
              zIndex: 1000,
              marginTop: '2px'
            }}>
              {searchResults().map((result, idx) => (
                <div
                  key={idx}
                  onClick={() => handleAddSymbol(result.symbol)}
                  style={{
                    padding: '8px',
                    borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
                    cursor: 'pointer',
                    backgroundColor: BLOOMBERG_PANEL_BG
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG_DARK_BG}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG_PANEL_BG}
                >
                  <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px', fontWeight: 'bold' }}>
                    {result.shortName || result.symbol}
                  </div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px' }}>
                    {result.displayName || result.description}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Bloomberg-style Screener Table */}
      <div
        ref={scrollContainerRef}
        style={{
          flex: 1,
          overflow: 'auto',
          backgroundColor: BLOOMBERG_DARK_BG
        }}
        className="custom-scrollbar">
        <table style={{
          width: '100%',
          borderCollapse: 'collapse',
          fontSize: '11px'
        }}>
          <thead style={{
            position: 'sticky',
            top: 0,
            backgroundColor: BLOOMBERG_PANEL_BG,
            borderBottom: `2px solid ${BLOOMBERG_ORANGE}`
          }}>
            <tr>
              <th style={{ padding: '8px', textAlign: 'left', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '14%' }}>SYMBOL</th>
              <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '10%' }}>LTP</th>
              <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '8%' }}>CHG</th>
              <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '8%' }}>CHG%</th>
              <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '9%' }}>HIGH</th>
              <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '9%' }}>LOW</th>
              <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '10%' }}>VOL</th>
              <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '14%' }}>TREND (15D)</th>
              <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '16%' }}>ACTION</th>
              <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold', width: '2%' }}></th>
            </tr>
          </thead>
          <tbody>
            {selectedSymbols.map((symbol, idx) => {
              const quote = quotes[symbol];
              const loading = !quote;

              return (
                <tr
                  key={symbol}
                  style={{
                    borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
                    backgroundColor: idx % 2 === 0 ? BLOOMBERG_DARK_BG : BLOOMBERG_PANEL_BG
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#111111'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = idx % 2 === 0 ? BLOOMBERG_DARK_BG : BLOOMBERG_PANEL_BG}
                >
                  <td style={{ padding: '8px', color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>
                    {symbol.split(':')[1] || symbol}
                  </td>
                  <td style={{
                    padding: '8px',
                    textAlign: 'right',
                    color: loading ? BLOOMBERG_GRAY : getPriceColor(symbol),
                    fontWeight: 'bold',
                    backgroundColor: getFlashColor(symbol),
                    transition: 'background-color 0.5s ease'
                  }}>
                    {loading ? '...' : formatCurrency(quote?.ltp)}
                  </td>
                  <td style={{
                    padding: '8px',
                    textAlign: 'right',
                    color: loading ? BLOOMBERG_GRAY : ((quote?.ch || 0) > 0 ? BLOOMBERG_GREEN : (quote?.ch || 0) < 0 ? BLOOMBERG_RED : BLOOMBERG_WHITE)
                  }}>
                    {loading ? '...' : formatCurrency(Math.abs(quote?.ch || 0))}
                  </td>
                  <td style={{
                    padding: '8px',
                    textAlign: 'right',
                    color: loading ? BLOOMBERG_GRAY : ((quote?.chp || 0) > 0 ? BLOOMBERG_GREEN : (quote?.chp || 0) < 0 ? BLOOMBERG_RED : BLOOMBERG_WHITE),
                    fontWeight: 'bold'
                  }}>
                    {loading ? '...' : formatPercent(quote?.chp)}
                  </td>
                  <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE }}>
                    {loading ? '...' : formatCurrency(quote?.high)}
                  </td>
                  <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE }}>
                    {loading ? '...' : formatCurrency(quote?.low)}
                  </td>
                  <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE }}>
                    {loading ? '...' : ((quote?.volume || 0) > 0 ? (quote?.volume || 0).toLocaleString('en-IN') : '...')}
                  </td>
                  <td style={{
                    padding: '8px 12px',
                    textAlign: 'center',
                    verticalAlign: 'middle'
                  }}>
                    <div style={{
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      minHeight: '24px'
                    }}>
                      {historicalData[symbol] && historicalData[symbol].length > 0 ? (
                        <Sparkline
                          data={historicalData[symbol]}
                          width={100}
                          height={28}
                          color={(quote?.chp || 0) > 0 ? BLOOMBERG_GREEN : (quote?.chp || 0) < 0 ? BLOOMBERG_RED : BLOOMBERG_GRAY}
                        />
                      ) : (
                        <span style={{ color: BLOOMBERG_GRAY, fontSize: '9px' }}>Loading...</span>
                      )}
                    </div>
                  </td>
                  <td style={{ padding: '6px', textAlign: 'center' }}>
                    <div style={{ display: 'flex', gap: '4px', justifyContent: 'center', flexWrap: 'wrap' }}>
                      <button
                        onClick={() => handleOpenOrderModal(symbol, 'BUY', quote?.ltp || 0)}
                        disabled={loading || !quote}
                        style={{
                          backgroundColor: BLOOMBERG_GREEN,
                          color: 'black',
                          border: 'none',
                          padding: '4px 10px',
                          fontSize: '9px',
                          fontWeight: 'bold',
                          cursor: loading || !quote ? 'not-allowed' : 'pointer',
                          opacity: loading || !quote ? 0.5 : 1,
                          fontFamily: 'Consolas, monospace'
                        }}
                      >
                        BUY
                      </button>
                      <button
                        onClick={() => handleOpenOrderModal(symbol, 'SELL', quote?.ltp || 0)}
                        disabled={loading || !quote}
                        style={{
                          backgroundColor: BLOOMBERG_RED,
                          color: BLOOMBERG_WHITE,
                          border: 'none',
                          padding: '4px 10px',
                          fontSize: '9px',
                          fontWeight: 'bold',
                          cursor: loading || !quote ? 'not-allowed' : 'pointer',
                          opacity: loading || !quote ? 0.5 : 1,
                          fontFamily: 'Consolas, monospace'
                        }}
                      >
                        SELL
                      </button>
                      <button
                        onClick={() => handleOpenGTTModal(symbol, quote?.ltp || 0)}
                        disabled={loading || !quote}
                        style={{
                          backgroundColor: BLOOMBERG_ORANGE,
                          color: 'black',
                          border: 'none',
                          padding: '4px 10px',
                          fontSize: '9px',
                          fontWeight: 'bold',
                          cursor: loading || !quote ? 'not-allowed' : 'pointer',
                          opacity: loading || !quote ? 0.5 : 1,
                          fontFamily: 'Consolas, monospace'
                        }}
                      >
                        GTT
                      </button>
                    </div>
                  </td>
                  <td style={{ padding: '8px', textAlign: 'center' }}>
                    <X
                      size={12}
                      color={BLOOMBERG_RED}
                      style={{ cursor: 'pointer' }}
                      onClick={() => handleRemoveSymbol(symbol)}
                    />
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>

        {selectedSymbols.length === 0 && (
          <div style={{
            padding: '40px',
            textAlign: 'center',
            color: BLOOMBERG_GRAY,
            fontSize: '11px'
          }}>
            No symbols added. Use search above to add symbols to your screener.
          </div>
        )}
      </div>

      {/* Order Modal */}
      {orderModal.isOpen && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999
        }}>
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `2px solid ${BLOOMBERG_ORANGE}`,
            padding: '24px',
            minWidth: '400px',
            maxWidth: '500px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '20px',
              borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
              paddingBottom: '12px'
            }}>
              <h3 style={{
                color: orderModal.side === 'BUY' ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                margin: 0,
                fontSize: '16px',
                fontWeight: 'bold'
              }}>
                {orderModal.side} {orderModal.symbol.split(':')[1] || orderModal.symbol}
              </h3>
              <X
                size={18}
                color={BLOOMBERG_GRAY}
                style={{ cursor: 'pointer' }}
                onClick={() => setOrderModal({ isOpen: false, symbol: '', side: 'BUY', ltp: 0 })}
              />
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Order Type
              </label>
              <select
                value={orderType}
                onChange={(e) => setOrderType(Number(e.target.value) as 1 | 2 | 3)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                <option value={2}>MARKET</option>
                <option value={1}>LIMIT</option>
                <option value={3}>STOP</option>
              </select>
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Product Type
              </label>
              <select
                value={productType}
                onChange={(e) => setProductType(e.target.value as 'INTRADAY' | 'MARGIN' | 'CNC')}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                <option value="INTRADAY">INTRADAY</option>
                <option value="CNC">CNC (DELIVERY)</option>
                <option value="MARGIN">MARGIN</option>
              </select>
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Quantity
              </label>
              <input
                type="number"
                value={orderQty}
                onChange={(e) => setOrderQty(Math.max(1, parseInt(e.target.value) || 1))}
                min={1}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            {orderType === 1 && (
              <div style={{ marginBottom: '16px' }}>
                <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                  Limit Price (Current LTP: ₹{orderModal.ltp.toFixed(2)})
                </label>
                <input
                  type="number"
                  value={limitPrice}
                  onChange={(e) => setLimitPrice(parseFloat(e.target.value) || 0)}
                  step={0.05}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '8px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            )}

            {orderType === 3 && (
              <div style={{ marginBottom: '16px' }}>
                <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                  Stop Price
                </label>
                <input
                  type="number"
                  value={stopPrice}
                  onChange={(e) => setStopPrice(parseFloat(e.target.value) || 0)}
                  step={0.05}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '8px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            )}

            <div style={{
              display: 'flex',
              gap: '12px',
              marginTop: '24px',
              paddingTop: '16px',
              borderTop: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              <button
                onClick={handlePlaceOrder}
                style={{
                  flex: 1,
                  backgroundColor: orderModal.side === 'BUY' ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                  color: orderModal.side === 'BUY' ? 'black' : BLOOMBERG_WHITE,
                  border: 'none',
                  padding: '10px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                PLACE {orderModal.side} ORDER
              </button>
              <button
                onClick={() => setOrderModal({ isOpen: false, symbol: '', side: 'BUY', ltp: 0 })}
                style={{
                  flex: 0.4,
                  backgroundColor: BLOOMBERG_DARK_BG,
                  color: BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  padding: '10px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        </div>
      )}

      {/* GTT Order Modal */}
      {gttModal.isOpen && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999
        }}>
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `2px solid ${BLOOMBERG_ORANGE}`,
            padding: '24px',
            minWidth: '400px',
            maxWidth: '500px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '20px',
              borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
              paddingBottom: '12px'
            }}>
              <h3 style={{
                color: BLOOMBERG_ORANGE,
                margin: 0,
                fontSize: '16px',
                fontWeight: 'bold'
              }}>
                GTT ORDER - {gttModal.symbol.split(':')[1] || gttModal.symbol}
              </h3>
              <X
                size={18}
                color={BLOOMBERG_GRAY}
                style={{ cursor: 'pointer' }}
                onClick={() => setGttModal({ isOpen: false, symbol: '', ltp: 0 })}
              />
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Side
              </label>
              <select
                value={gttSide}
                onChange={(e) => setGttSide(Number(e.target.value) as 1 | -1)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                <option value={1}>BUY</option>
                <option value={-1}>SELL</option>
              </select>
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Product Type
              </label>
              <select
                value={gttProductType}
                onChange={(e) => setGttProductType(e.target.value as 'INTRADAY' | 'MARGIN' | 'CNC')}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                <option value="CNC">CNC (DELIVERY)</option>
                <option value="INTRADAY">INTRADAY</option>
                <option value="MARGIN">MARGIN</option>
              </select>
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Quantity
              </label>
              <input
                type="number"
                value={gttQty}
                onChange={(e) => setGttQty(Math.max(1, parseInt(e.target.value) || 1))}
                min={1}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Trigger Price (Current LTP: ₹{gttModal.ltp.toFixed(2)})
              </label>
              <input
                type="number"
                value={gttTriggerPrice}
                onChange={(e) => setGttTriggerPrice(parseFloat(e.target.value) || 0)}
                step={0.05}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: BLOOMBERG_WHITE, fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                Limit Price
              </label>
              <input
                type="number"
                value={gttPrice}
                onChange={(e) => setGttPrice(parseFloat(e.target.value) || 0)}
                step={0.05}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            <div style={{
              display: 'flex',
              gap: '12px',
              marginTop: '24px',
              paddingTop: '16px',
              borderTop: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              <button
                onClick={handlePlaceGTTOrder}
                style={{
                  flex: 1,
                  backgroundColor: BLOOMBERG_ORANGE,
                  color: 'black',
                  border: 'none',
                  padding: '10px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                PLACE GTT ORDER
              </button>
              <button
                onClick={() => setGttModal({ isOpen: false, symbol: '', ltp: 0 })}
                style={{
                  flex: 0.4,
                  backgroundColor: BLOOMBERG_DARK_BG,
                  color: BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  padding: '10px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default FyersMarketDataPanel;
