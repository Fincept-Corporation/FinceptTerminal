// File: src/components/tabs/TradingTab.tsx
// Complete Trading Tab with Paper Trading Integration

import React, { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../contexts/BrokerContext';
import { useWebSocket } from '../../hooks/useWebSocket';
import { OrderForm } from './trading/OrderForm';
import type { OrderRequest } from '../../types/trading';

interface OrderBookLevel {
  price: number;
  size: number;
}

interface OrderBook {
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  symbol: string;
  checksum?: number;
}

interface Position {
  symbol: string;
  side: 'long' | 'short';
  quantity: number;
  entryPrice: number;
  positionValue: number;
  currentPrice: number;
  unrealizedPnl: number;
  pnlPercent: number;
}

interface Order {
  id: string;
  symbol: string;
  side: 'buy' | 'sell';
  type: string;
  quantity: number;
  price?: number;
  status: string;
  createdAt: string;
}

export function TradingTab() {
  const {
    activeBroker,
    tradingMode,
    activeAdapter,
    paperAdapter,
    defaultSymbols,
    isConnecting,
  } = useBrokerContext();

  // Debug logging
  useEffect(() => {
    console.log('[TradingTab] Context updated:');
    console.log('  - activeBroker:', activeBroker);
    console.log('  - tradingMode:', tradingMode);
    console.log('  - paperAdapter:', paperAdapter ? 'EXISTS' : 'NULL');
    console.log('  - paperAdapter.isConnected():', paperAdapter?.isConnected());
    console.log('  - isConnecting:', isConnecting);
  }, [activeBroker, tradingMode, paperAdapter, isConnecting]);

  const [selectedSymbol, setSelectedSymbol] = useState(defaultSymbols[0] || 'BTC/USD');
  const [tickerData, setTickerData] = useState<any>(null);
  const [orderBook, setOrderBook] = useState<OrderBook>({ bids: [], asks: [], symbol: '' });
  const [tradesData, setTradesData] = useState<any[]>([]);
  const [isPlacingOrder, setIsPlacingOrder] = useState(false);
  const [lastOrderBookUpdate, setLastOrderBookUpdate] = useState<number>(0);

  // Symbol search state
  const [searchQuery, setSearchQuery] = useState('');
  const [availableSymbols, setAvailableSymbols] = useState<string[]>([]);
  const [isLoadingSymbols, setIsLoadingSymbols] = useState(false);
  const [showSymbolDropdown, setShowSymbolDropdown] = useState(false);

  // Paper trading state
  const [positions, setPositions] = useState<Position[]>([]);
  const [orders, setOrders] = useState<Order[]>([]);
  const [trades, setTrades] = useState<any[]>([]);
  const [balance, setBalance] = useState(0);
  const [equity, setEquity] = useState(0);
  const [stats, setStats] = useState<any>(null);
  const [activeTab, setActiveTab] = useState<'positions' | 'orders' | 'trades' | 'stats'>('positions');

  // Trading settings
  const [slippage, setSlippage] = useState(0);
  const [makerFee, setMakerFee] = useState(0.0002); // 0.02% default
  const [takerFee, setTakerFee] = useState(0.0005); // 0.05% default
  const [showSettings, setShowSettings] = useState(false);

  // Clear data when symbol changes
  useEffect(() => {
    console.log('[TradingTab] Symbol changed to:', selectedSymbol);
    // Clear existing data for new symbol
    setTickerData(null);
    setOrderBook({ bids: [], asks: [], symbol: selectedSymbol });
    setTradesData([]);
    setLastOrderBookUpdate(0);
  }, [selectedSymbol]);

  // Subscribe to ticker WebSocket
  const { message: tickerMessage } = useWebSocket(
    activeBroker ? `${activeBroker}.ticker.${selectedSymbol}` : null
  );

  // Subscribe to orderbook WebSocket
  const { message: orderbookMessage } = useWebSocket(
    activeBroker ? `${activeBroker}.book.${selectedSymbol}` : null
  );

  // Subscribe to trades WebSocket
  const { message: tradesMessage } = useWebSocket(
    activeBroker ? `${activeBroker}.trade.${selectedSymbol}` : null
  );


  // Update ticker data
  useEffect(() => {
    if (tickerMessage && tickerMessage.data) {
      const data = tickerMessage.data;

      // Verify the ticker data is for the current selected symbol
      if (data.symbol === selectedSymbol) {
        console.log('[TradingTab] WebSocket ticker update for', selectedSymbol, ':', data);
        setTickerData(data);

        // Update matching engine with WebSocket price for order monitoring
        if (tradingMode === 'paper' && paperAdapter) {
          const matchingEngine = (paperAdapter as any).matchingEngine;
          if (matchingEngine && typeof matchingEngine.updatePriceFromWebSocket === 'function') {
            matchingEngine.updatePriceFromWebSocket(selectedSymbol, {
              bid: data.bid,
              ask: data.ask,
              last: data.last,
            });
            console.log('[TradingTab] Price fed to matching engine:', data.last);
          }
        }
      } else {
        console.log('[TradingTab] Ignoring ticker for', data.symbol, '- current symbol is', selectedSymbol);
      }
    }
  }, [tickerMessage, tradingMode, paperAdapter, selectedSymbol]);

  // Update orderbook data
  useEffect(() => {
    if (!orderbookMessage || !orderbookMessage.data) return;

    const data = orderbookMessage.data;
    const messageType = data.messageType;
    const dataSymbol = data.symbol || selectedSymbol;

    // Only process if it's for the current selected symbol
    if (dataSymbol !== selectedSymbol) {
      console.log('[TradingTab] Ignoring orderbook for', dataSymbol, '- current symbol is', selectedSymbol);
      return;
    }

    if (messageType === 'snapshot') {
      setOrderBook({
        bids: data.bids || [],
        asks: data.asks || [],
        symbol: dataSymbol,
        checksum: data.checksum
      });
      setLastOrderBookUpdate(Date.now());
    } else if (messageType === 'update') {
      setOrderBook(prevBook => {
        const newBook = { ...prevBook };

        if (data.bids && data.bids.length > 0) {
          const bidsMap = new Map(prevBook.bids.map(b => [b.price, b.size]));
          data.bids.forEach((bid: OrderBookLevel) => {
            if (bid.size === 0) {
              bidsMap.delete(bid.price);
            } else {
              bidsMap.set(bid.price, bid.size);
            }
          });
          newBook.bids = Array.from(bidsMap.entries())
            .map(([price, size]) => ({ price, size }))
            .sort((a, b) => b.price - a.price)
            .slice(0, 25);
        }

        if (data.asks && data.asks.length > 0) {
          const asksMap = new Map(prevBook.asks.map(a => [a.price, a.size]));
          data.asks.forEach((ask: OrderBookLevel) => {
            if (ask.size === 0) {
              asksMap.delete(ask.price);
            } else {
              asksMap.set(ask.price, ask.size);
            }
          });
          newBook.asks = Array.from(asksMap.entries())
            .map(([price, size]) => ({ price, size }))
            .sort((a, b) => a.price - b.price)
            .slice(0, 25);
        }

        newBook.checksum = data.checksum;
        setLastOrderBookUpdate(Date.now());
        return newBook;
      });
    }
  }, [orderbookMessage, selectedSymbol]);

  // Update trades data
  useEffect(() => {
    if (tradesMessage && tradesMessage.data) {
      const data = tradesMessage.data;
      const dataSymbol = data.symbol || selectedSymbol;

      // Only add trades for current selected symbol
      if (dataSymbol === selectedSymbol) {
        setTradesData(prev => [data, ...prev].slice(0, 50));
      } else {
        console.log('[TradingTab] Ignoring trade for', dataSymbol, '- current symbol is', selectedSymbol);
      }
    }
  }, [tradesMessage, selectedSymbol]);


  // Load paper trading data
  useEffect(() => {
    const loadPaperTradingData = async () => {
      if (tradingMode !== 'paper' || !paperAdapter) return;

      try {
        console.log('[TradingTab] ==========================================');
        console.log('[TradingTab] Loading paper trading data...');
        console.log('[TradingTab] Portfolio ID:', (paperAdapter as any).paperConfig?.portfolioId);
        console.log('[TradingTab] ==========================================');

        // Fetch balance
        const balanceData = await paperAdapter.fetchBalance();
        console.log('[TradingTab] Balance:', balanceData);
        const usdBalance = (balanceData.free as any)?.USD || 0;
        const totalEquity = (balanceData.total as any)?.USD || 0;
        setBalance(usdBalance);
        setEquity(totalEquity);

        // Fetch positions
        const positionsData = await paperAdapter.fetchPositions();
        console.log('[TradingTab] Positions RAW from adapter:', positionsData);
        console.log('[TradingTab] Number of positions:', positionsData.length);

        const mappedPositions: Position[] = positionsData.map((p: any) => {
          console.log('[TradingTab] Mapping position:', {
            symbol: p.symbol,
            side: p.side,
            contracts: p.contracts,
            entryPrice: p.entryPrice,
            markPrice: p.markPrice,
            unrealizedPnl: p.unrealizedPnl,
            percentage: p.percentage,
            positionValue: p.info?.positionValue
          });

          return {
            symbol: p.symbol,
            side: p.side,
            quantity: p.contracts,
            entryPrice: p.entryPrice,
            positionValue: p.info?.positionValue || (p.entryPrice * p.contracts),
            currentPrice: p.markPrice,
            unrealizedPnl: p.unrealizedPnl,
            pnlPercent: p.percentage || 0,
          };
        });

        console.log('[TradingTab] Mapped positions:', mappedPositions);
        console.log('[TradingTab] Setting positions state with', mappedPositions.length, 'positions');
        setPositions(mappedPositions);

        // Fetch open orders
        const ordersData = await paperAdapter.fetchOpenOrders();
        console.log('[TradingTab] Orders RAW from adapter:', ordersData);
        console.log('[TradingTab] Number of orders:', ordersData.length);

        const mappedOrders: Order[] = ordersData.map((o: any) => {
          console.log('[TradingTab] Mapping order:', {
            id: o.id,
            symbol: o.symbol,
            side: o.side,
            type: o.type,
            amount: o.amount,
            price: o.price,
            status: o.status
          });

          return {
            id: o.id,
            symbol: o.symbol,
            side: o.side,
            type: o.type,
            quantity: o.amount,
            price: o.price,
            status: o.status,
            createdAt: o.datetime,
          };
        });

        console.log('[TradingTab] Mapped orders:', mappedOrders);
        console.log('[TradingTab] Setting orders state with', mappedOrders.length, 'orders');
        setOrders(mappedOrders);

        // Fetch trades
        const tradesData = await paperAdapter.fetchMyTrades(undefined, undefined, 50);
        console.log('[TradingTab] Trades:', tradesData);
        console.log('[TradingTab] Trades details:');
        tradesData.forEach((trade: any, idx: number) => {
          console.log(`  Trade ${idx + 1}:`, {
            side: trade.side,
            amount: trade.amount,
            price: trade.price,
            cost: trade.cost,
            fee: trade.fee?.cost,
            timestamp: trade.datetime
          });
        });
        setTrades(tradesData);

        // Fetch statistics
        if (typeof (paperAdapter as any).getStatistics === 'function') {
          const statsData = await (paperAdapter as any).getStatistics();
          console.log('[TradingTab] Stats:', statsData);
          setStats(statsData);
        }
      } catch (error) {
        console.error('[TradingTab] Failed to load paper trading data:', error);
      }
    };

    loadPaperTradingData();

    // Refresh every 2 seconds (not too spammy)
    const interval = setInterval(loadPaperTradingData, 2000);
    return () => clearInterval(interval);
  }, [tradingMode, paperAdapter]);

  // Update slippage when it changes
  useEffect(() => {
    if (paperAdapter) {
      (paperAdapter as any).paperConfig.slippage.market = slippage;
      console.log('[TradingTab] Slippage updated to:', slippage);
    }
  }, [slippage, paperAdapter]);

  // Update fees when they change
  useEffect(() => {
    if (paperAdapter) {
      (paperAdapter as any).paperConfig.fees.maker = makerFee;
      (paperAdapter as any).paperConfig.fees.taker = takerFee;
      console.log('[TradingTab] Fees updated - Maker:', makerFee, 'Taker:', takerFee);
    }
  }, [makerFee, takerFee, paperAdapter]);

  // Fetch available markets from broker
  useEffect(() => {
    const fetchMarkets = async () => {
      if (!activeAdapter) return;

      setIsLoadingSymbols(true);
      try {
        console.log('[TradingTab] Fetching markets from adapter...');
        const markets = await activeAdapter.fetchMarkets();
        console.log('[TradingTab] Markets fetched:', markets.length);

        // Extract symbols and filter for spot/USD pairs
        const symbols = markets
          .filter((m: any) => {
            // Filter for active, spot markets
            return m.active && m.spot && (m.quote === 'USD' || m.quote === 'USDT' || m.quote === 'USDC');
          })
          .map((m: any) => m.symbol)
          .sort();

        console.log('[TradingTab] Available symbols:', symbols.length);
        setAvailableSymbols(symbols);
      } catch (error) {
        console.error('[TradingTab] Failed to fetch markets:', error);
        // Fallback to default symbols
        setAvailableSymbols(defaultSymbols);
      } finally {
        setIsLoadingSymbols(false);
      }
    };

    fetchMarkets();
  }, [activeAdapter, defaultSymbols]);

  // Close dropdown when clicking outside
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      const target = event.target as HTMLElement;
      if (showSymbolDropdown && !target.closest('[data-symbol-selector]')) {
        setShowSymbolDropdown(false);
        setSearchQuery('');
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [showSymbolDropdown]);

  // Handle order placement
  const handlePlaceOrder = useCallback(async (orderRequest: OrderRequest) => {
    console.log('[TradingTab] ========== ORDER PLACEMENT START ==========');
    console.log('[TradingTab] Order request:', orderRequest);
    console.log('[TradingTab] Slippage:', slippage);
    console.log('[TradingTab] Trading mode:', tradingMode);
    console.log('[TradingTab] paperAdapter:', paperAdapter ? 'EXISTS' : 'NULL');
    console.log('[TradingTab] paperAdapter connected:', paperAdapter?.isConnected());

    if (!paperAdapter) {
      const errorMsg = 'Paper trading adapter not available. Please wait for initialization.';
      console.error('[TradingTab]', errorMsg);
      alert(errorMsg);
      throw new Error(errorMsg);
    }

    if (!paperAdapter.isConnected()) {
      const errorMsg = 'Paper trading adapter not connected. Please wait.';
      console.error('[TradingTab]', errorMsg);
      alert(errorMsg);
      throw new Error(errorMsg);
    }

    setIsPlacingOrder(true);

    try {
      console.log('[TradingTab] Calling paperAdapter.createOrder...');

      const order = await paperAdapter.createOrder(
        orderRequest.symbol,
        orderRequest.type,
        orderRequest.side,
        orderRequest.quantity,
        orderRequest.price,
        { stopPrice: orderRequest.stopPrice }
      );

      console.log('[TradingTab] ✓ Order created successfully:', order);
      console.log('[TradingTab] Order ID:', order.id);
      console.log('[TradingTab] Order status:', order.status);

      // Immediate refresh
      console.log('[TradingTab] Refreshing orders and balance...');

      const ordersData = await paperAdapter.fetchOpenOrders();
      console.log('[TradingTab] fetchOpenOrders returned:', ordersData);
      console.log('[TradingTab] Number of orders:', ordersData.length);

      const mappedOrders: Order[] = ordersData.map((o: any) => ({
        id: o.id,
        symbol: o.symbol,
        side: o.side,
        type: o.type,
        quantity: o.amount,
        price: o.price,
        status: o.status,
        createdAt: o.datetime,
      }));

      console.log('[TradingTab] Setting orders state:', mappedOrders);
      setOrders(mappedOrders);

      const balanceData = await paperAdapter.fetchBalance();
      console.log('[TradingTab] New balance:', balanceData);
      const usdBalance = (balanceData.free as any)?.USD || 0;
      setBalance(usdBalance);

      alert(`Order placed successfully! ID: ${order.id}`);
      console.log('[TradingTab] ========== ORDER PLACEMENT END ==========');
    } catch (error) {
      console.error('[TradingTab] ✗ Order placement failed:', error);
      console.error('[TradingTab] Error details:', error);
      alert(`Order failed: ${(error as Error).message}`);
      throw error;
    } finally {
      setIsPlacingOrder(false);
    }
  }, [paperAdapter, tradingMode]);

  // Handle order cancellation
  const handleCancelOrder = useCallback(async (orderId: string, symbol: string) => {
    if (!paperAdapter) return;

    try {
      console.log('[TradingTab] Cancelling order:', orderId);
      await paperAdapter.cancelOrder(orderId, symbol);
      console.log('[TradingTab] ✓ Order cancelled');

      // Refresh orders immediately
      const ordersData = await paperAdapter.fetchOpenOrders();
      const mappedOrders: Order[] = ordersData.map((o: any) => ({
        id: o.id,
        symbol: o.symbol,
        side: o.side,
        type: o.type,
        quantity: o.amount,
        price: o.price,
        status: o.status,
        createdAt: o.datetime,
      }));
      setOrders(mappedOrders);
    } catch (error) {
      console.error('[TradingTab] ✗ Cancel failed:', error);
    }
  }, [paperAdapter]);

  // Handle balance reset
  const handleResetBalance = useCallback(async () => {
    if (!paperAdapter) return;

    const confirmed = window.confirm(
      '⚠️ Reset Paper Trading Account?\n\n' +
      'This will:\n' +
      '• Close all open positions\n' +
      '• Cancel all pending orders\n' +
      '• Reset balance to initial capital\n' +
      '• Clear trade history\n\n' +
      'This action cannot be undone!'
    );

    if (!confirmed) return;

    try {
      console.log('[TradingTab] Resetting paper trading account...');

      // Call reset method on paper adapter
      if (typeof (paperAdapter as any).resetAccount === 'function') {
        await (paperAdapter as any).resetAccount();
        console.log('[TradingTab] ✓ Account reset successfully');
        alert('✓ Paper trading account has been reset!');
      } else {
        console.error('[TradingTab] resetAccount method not found on paperAdapter');
        alert('❌ Reset method not available');
      }
    } catch (error) {
      console.error('[TradingTab] ✗ Reset failed:', error);
      alert(`❌ Reset failed: ${(error as Error).message}`);
    }
  }, [paperAdapter]);

  const currentPrice = tickerData?.last || orderBook.asks[0]?.price || 0;

  return (
    <div style={{
      padding: '20px',
      color: 'white',
      backgroundColor: '#0d0d0d',
      height: '100%',
      overflow: 'auto'
    }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px', gap: '16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', flex: 1 }}>
          <h1 style={{ fontSize: '24px', margin: 0 }}>
            Trading
          </h1>

          {/* Symbol Selector with Search */}
          <div style={{ position: 'relative', minWidth: '200px' }} data-symbol-selector>
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                padding: '8px 12px',
                backgroundColor: '#1a1a1a',
                border: '1px solid #333',
                borderRadius: '6px',
                cursor: 'pointer'
              }}
              onClick={() => setShowSymbolDropdown(!showSymbolDropdown)}
            >
              <span style={{ fontSize: '14px', fontWeight: 'bold', flex: 1 }}>
                {selectedSymbol}
              </span>
              <span style={{ fontSize: '12px', color: '#666' }}>▼</span>
            </div>

            {/* Dropdown */}
            {showSymbolDropdown && (
              <div style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                right: 0,
                marginTop: '4px',
                backgroundColor: '#1a1a1a',
                border: '1px solid #333',
                borderRadius: '6px',
                maxHeight: '400px',
                overflow: 'hidden',
                zIndex: 1000,
                boxShadow: '0 4px 12px rgba(0,0,0,0.5)'
              }}>
                {/* Search Input */}
                <div style={{ padding: '12px', borderBottom: '1px solid #333' }}>
                  <input
                    type="text"
                    placeholder="Search pairs... (e.g., BTC, ETH)"
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    autoFocus
                    style={{
                      width: '100%',
                      padding: '8px 12px',
                      backgroundColor: '#0d0d0d',
                      color: 'white',
                      border: '1px solid #333',
                      borderRadius: '4px',
                      fontSize: '13px',
                      outline: 'none'
                    }}
                  />
                </div>

                {/* Loading State */}
                {isLoadingSymbols ? (
                  <div style={{ padding: '20px', textAlign: 'center', color: '#666', fontSize: '13px' }}>
                    Loading markets...
                  </div>
                ) : (
                  /* Symbol List */
                  <div style={{ maxHeight: '300px', overflowY: 'auto' }}>
                    {availableSymbols
                      .filter(symbol =>
                        symbol.toLowerCase().includes(searchQuery.toLowerCase())
                      )
                      .map(symbol => (
                        <div
                          key={symbol}
                          onClick={() => {
                            setSelectedSymbol(symbol);
                            setShowSymbolDropdown(false);
                            setSearchQuery('');
                          }}
                          style={{
                            padding: '10px 16px',
                            cursor: 'pointer',
                            fontSize: '13px',
                            backgroundColor: symbol === selectedSymbol ? '#f59e0b20' : 'transparent',
                            color: symbol === selectedSymbol ? '#f59e0b' : 'white',
                            borderBottom: '1px solid #222',
                            transition: 'background-color 0.15s'
                          }}
                          onMouseEnter={(e) => {
                            if (symbol !== selectedSymbol) {
                              e.currentTarget.style.backgroundColor = '#ffffff10';
                            }
                          }}
                          onMouseLeave={(e) => {
                            if (symbol !== selectedSymbol) {
                              e.currentTarget.style.backgroundColor = 'transparent';
                            }
                          }}
                        >
                          {symbol}
                        </div>
                      ))
                    }
                    {/* No Results */}
                    {availableSymbols.filter(s => s.toLowerCase().includes(searchQuery.toLowerCase())).length === 0 && (
                      <div style={{ padding: '20px', textAlign: 'center', color: '#666', fontSize: '13px' }}>
                        No pairs found matching "{searchQuery}"
                      </div>
                    )}
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Mode Badge */}
          <span style={{
            padding: '4px 12px',
            backgroundColor: tradingMode === 'paper' ? '#10b98120' : '#dc262620',
            color: tradingMode === 'paper' ? '#10b981' : '#dc2626',
            border: `1px solid ${tradingMode === 'paper' ? '#10b981' : '#dc2626'}`,
            borderRadius: '4px',
            fontSize: '12px',
            fontWeight: 'bold'
          }}>
            {tradingMode === 'paper' ? '📝 PAPER' : '🔴 LIVE'}
          </span>
        </div>

        <button
          onClick={() => setShowSettings(!showSettings)}
          style={{
            padding: '8px 16px',
            backgroundColor: '#1a1a1a',
            color: 'white',
            border: '1px solid #333',
            borderRadius: '6px',
            cursor: 'pointer',
            fontSize: '13px'
          }}
        >
          ⚙️ Settings
        </button>
      </div>

      {/* Settings Panel */}
      {showSettings && (
        <div style={{
          marginBottom: '20px',
          padding: '16px',
          backgroundColor: '#1a1a1a',
          borderRadius: '6px',
          border: '1px solid #333'
        }}>
          <h3 style={{ fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>Trading Settings</h3>

          {/* Slippage Control */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '12px' }}>
            <label style={{ fontSize: '13px', color: '#999', minWidth: '100px' }}>
              Slippage (%):
            </label>
            <input
              type="number"
              value={slippage * 100}
              onChange={(e) => setSlippage(parseFloat(e.target.value) / 100)}
              step="0.01"
              min="0"
              max="5"
              style={{
                padding: '6px 12px',
                backgroundColor: '#0d0d0d',
                color: 'white',
                border: '1px solid #333',
                borderRadius: '4px',
                width: '100px',
                fontSize: '13px'
              }}
            />
            <span style={{ fontSize: '12px', color: '#666' }}>
              (0% = exact price, 0.1% = realistic)
            </span>
          </div>

          {/* Maker Fee Control */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '12px' }}>
            <label style={{ fontSize: '13px', color: '#999', minWidth: '100px' }}>
              Maker Fee (%):
            </label>
            <input
              type="number"
              value={makerFee * 100}
              onChange={(e) => setMakerFee(parseFloat(e.target.value) / 100)}
              step="0.001"
              min="0"
              max="1"
              style={{
                padding: '6px 12px',
                backgroundColor: '#0d0d0d',
                color: 'white',
                border: '1px solid #333',
                borderRadius: '4px',
                width: '100px',
                fontSize: '13px'
              }}
            />
            <span style={{ fontSize: '12px', color: '#666' }}>
              (0.02% Kraken default)
            </span>
          </div>

          {/* Taker Fee Control */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
            <label style={{ fontSize: '13px', color: '#999', minWidth: '100px' }}>
              Taker Fee (%):
            </label>
            <input
              type="number"
              value={takerFee * 100}
              onChange={(e) => setTakerFee(parseFloat(e.target.value) / 100)}
              step="0.001"
              min="0"
              max="1"
              style={{
                padding: '6px 12px',
                backgroundColor: '#0d0d0d',
                color: 'white',
                border: '1px solid #333',
                borderRadius: '4px',
                width: '100px',
                fontSize: '13px'
              }}
            />
            <span style={{ fontSize: '12px', color: '#666' }}>
              (0.05% Kraken default)
            </span>
          </div>

          {/* Reset Balance Button */}
          <div style={{
            borderTop: '1px solid #333',
            paddingTop: '16px',
            marginTop: '4px'
          }}>
            <button
              onClick={handleResetBalance}
              style={{
                padding: '8px 16px',
                backgroundColor: '#dc262620',
                color: '#dc2626',
                border: '1px solid #dc2626',
                borderRadius: '6px',
                cursor: 'pointer',
                fontSize: '13px',
                fontWeight: 'bold',
                width: '100%',
                transition: 'all 0.2s'
              }}
              onMouseOver={(e) => {
                e.currentTarget.style.backgroundColor = '#dc2626';
                e.currentTarget.style.color = 'white';
              }}
              onMouseOut={(e) => {
                e.currentTarget.style.backgroundColor = '#dc262620';
                e.currentTarget.style.color = '#dc2626';
              }}
            >
              🔄 Reset Paper Trading Account
            </button>
            <div style={{ fontSize: '11px', color: '#666', marginTop: '8px', textAlign: 'center' }}>
              Resets balance, closes all positions, and clears history
            </div>
          </div>
        </div>
      )}

      {/* Status Bar */}
      <div style={{ marginBottom: '20px', padding: '12px', backgroundColor: '#1a1a1a', borderRadius: '6px' }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(150px, 1fr))', gap: '16px', fontSize: '13px' }}>
          <div>
            <div style={{ color: '#666', marginBottom: '4px' }}>Symbol</div>
            <div style={{ fontWeight: 'bold', fontSize: '16px' }}>{selectedSymbol}</div>
          </div>
          <div>
            <div style={{ color: '#666', marginBottom: '4px' }}>Last Price</div>
            <div style={{ fontWeight: 'bold', fontSize: '16px', color: tickerData ? '#10b981' : '#666' }}>
              {tickerData ? `$${tickerData.last?.toFixed(2) || '0.00'}` : 'Loading...'}
            </div>
          </div>
          <div>
            <div style={{ color: '#666', marginBottom: '4px' }}>Bid / Ask</div>
            <div style={{ fontWeight: 'bold', fontSize: '13px' }}>
              {tickerData ? (
                <>
                  <span style={{ color: '#10b981' }}>${tickerData.bid?.toFixed(2) || '0.00'}</span>
                  {' / '}
                  <span style={{ color: '#dc2626' }}>${tickerData.ask?.toFixed(2) || '0.00'}</span>
                </>
              ) : (
                <span style={{ color: '#666' }}>--</span>
              )}
            </div>
          </div>
          {tradingMode === 'paper' && (
            <>
              <div>
                <div style={{ color: '#666', marginBottom: '4px' }}>Balance</div>
                <div style={{ fontWeight: 'bold' }}>${balance.toFixed(2)}</div>
              </div>
              <div>
                <div style={{ color: '#666', marginBottom: '4px' }}>Equity</div>
                <div style={{ fontWeight: 'bold' }}>${equity.toFixed(2)}</div>
              </div>
            </>
          )}
          <div>
            <div style={{ color: '#666', marginBottom: '4px' }}>Status</div>
            <div style={{ fontWeight: 'bold', color: activeAdapter?.isConnected() ? '#10b981' : '#dc2626' }}>
              {isConnecting ? 'Connecting...' : activeAdapter?.isConnected() ? 'Connected' : 'Disconnected'}
            </div>
          </div>
        </div>
      </div>

      {/* Main Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 350px', gap: '20px', marginBottom: '20px' }}>
        {/* Left Column: Positions */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
          {/* Trading Activity Tabs (Paper Trading) */}
          {tradingMode === 'paper' && (
            <div style={{ padding: '16px', backgroundColor: '#1a1a1a', borderRadius: '6px' }}>
              {/* Tab Headers */}
              <div style={{ display: 'flex', gap: '8px', marginBottom: '16px', borderBottom: '1px solid #333' }}>
                {(['positions', 'orders', 'trades', 'stats'] as const).map((tab) => (
                  <button
                    key={tab}
                    onClick={() => setActiveTab(tab)}
                    style={{
                      padding: '10px 16px',
                      backgroundColor: activeTab === tab ? '#f59e0b20' : 'transparent',
                      color: activeTab === tab ? '#f59e0b' : '#999',
                      border: 'none',
                      borderBottom: activeTab === tab ? '2px solid #f59e0b' : '2px solid transparent',
                      cursor: 'pointer',
                      fontSize: '13px',
                      fontWeight: activeTab === tab ? 'bold' : 'normal',
                      textTransform: 'capitalize',
                      transition: 'all 0.2s'
                    }}
                  >
                    {tab}
                    {tab === 'positions' && ` (${positions.length})`}
                    {tab === 'orders' && ` (${orders.length})`}
                    {tab === 'trades' && ` (${trades.length})`}
                  </button>
                ))}
              </div>

              {/* Positions Tab */}
              {activeTab === 'positions' && (
                <div>
                  {positions.length === 0 ? (
                    <div style={{ padding: '40px', textAlign: 'center', color: '#666', fontSize: '13px' }}>
                      No open positions
                    </div>
                  ) : (
                    <table style={{ width: '100%', fontSize: '12px', fontFamily: 'monospace', borderCollapse: 'collapse' }}>
                      <thead>
                        <tr style={{ borderBottom: '1px solid #333' }}>
                          <th style={{ textAlign: 'left', padding: '8px', color: '#666' }}>Symbol</th>
                          <th style={{ textAlign: 'center', padding: '8px', color: '#666' }}>Side</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Qty</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Value</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Entry</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Current</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>PnL ($)</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>PnL (%)</th>
                        </tr>
                      </thead>
                      <tbody>
                        {positions.map((pos, idx) => {
                          // Calculate price difference
                          const priceDiff = pos.entryPrice - pos.currentPrice;

                          // For SHORT: profit when price goes DOWN (priceDiff > 0)
                          // For LONG: profit when price goes UP (priceDiff < 0)
                          const totalPnlDollar = pos.side === 'short'
                            ? priceDiff * pos.quantity
                            : -priceDiff * pos.quantity;

                          // Calculate P&L percentage based on position value
                          const pnlPercent = pos.positionValue > 0 ? (totalPnlDollar / pos.positionValue) * 100 : 0;

                          return (
                            <tr key={idx} style={{ borderBottom: '1px solid #222' }}>
                              <td style={{ padding: '8px' }}>{pos.symbol}</td>
                              <td style={{ padding: '8px', textAlign: 'center' }}>
                                <span style={{
                                  padding: '4px 8px',
                                  borderRadius: '4px',
                                  backgroundColor: pos.side === 'long' ? '#10b98120' : '#dc262620',
                                  color: pos.side === 'long' ? '#10b981' : '#dc2626',
                                  fontSize: '11px',
                                  fontWeight: 'bold'
                                }}>
                                  {pos.side.toUpperCase()}
                                </span>
                              </td>
                              <td style={{ padding: '8px', textAlign: 'right' }}>{pos.quantity.toFixed(4)}</td>
                              <td style={{ padding: '8px', textAlign: 'right', color: '#999' }}>
                                ${pos.positionValue.toFixed(2)}
                              </td>
                              <td style={{ padding: '8px', textAlign: 'right' }}>${pos.entryPrice.toFixed(2)}</td>
                              <td style={{ padding: '8px', textAlign: 'right', fontWeight: 'bold' }}>
                                ${pos.currentPrice.toFixed(2)}
                              </td>
                              <td style={{
                                padding: '8px',
                                textAlign: 'right',
                                color: totalPnlDollar >= 0 ? '#10b981' : '#dc2626',
                                fontWeight: 'bold'
                              }}>
                                {totalPnlDollar >= 0 ? '+' : '-'}${Math.abs(totalPnlDollar).toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                              </td>
                              <td style={{
                                padding: '8px',
                                textAlign: 'right',
                                color: pnlPercent >= 0 ? '#10b981' : '#dc2626',
                                fontWeight: 'bold'
                              }}>
                                {pnlPercent >= 0 ? '+' : ''}{pnlPercent.toFixed(2)}%
                              </td>
                            </tr>
                          );
                        })}
                      </tbody>
                    </table>
                  )}
                </div>
              )}

              {/* Orders Tab */}
              {activeTab === 'orders' && (
                <div>
                  {orders.length === 0 ? (
                    <div style={{ padding: '40px', textAlign: 'center', color: '#666', fontSize: '13px' }}>
                      No open orders
                    </div>
                  ) : (
                    <table style={{ width: '100%', fontSize: '12px', fontFamily: 'monospace', borderCollapse: 'collapse' }}>
                      <thead>
                        <tr style={{ borderBottom: '1px solid #333' }}>
                          <th style={{ textAlign: 'left', padding: '8px', color: '#666' }}>Symbol</th>
                          <th style={{ textAlign: 'center', padding: '8px', color: '#666' }}>Type</th>
                          <th style={{ textAlign: 'center', padding: '8px', color: '#666' }}>Side</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Qty</th>
                          <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Price</th>
                          <th style={{ textAlign: 'center', padding: '8px', color: '#666' }}>Status</th>
                          <th style={{ textAlign: 'center', padding: '8px', color: '#666' }}>Action</th>
                        </tr>
                      </thead>
                      <tbody>
                        {orders.map((order) => (
                          <tr key={order.id} style={{ borderBottom: '1px solid #222' }}>
                            <td style={{ padding: '8px' }}>{order.symbol}</td>
                            <td style={{ padding: '8px', textAlign: 'center', textTransform: 'uppercase', fontSize: '11px' }}>
                              {order.type}
                            </td>
                            <td style={{ padding: '8px', textAlign: 'center' }}>
                              <span style={{
                                padding: '4px 8px',
                                borderRadius: '4px',
                                backgroundColor: order.side === 'buy' ? '#10b98120' : '#dc262620',
                                color: order.side === 'buy' ? '#10b981' : '#dc2626',
                                fontSize: '11px',
                                fontWeight: 'bold'
                              }}>
                                {order.side.toUpperCase()}
                              </span>
                            </td>
                            <td style={{ padding: '8px', textAlign: 'right' }}>{order.quantity.toFixed(4)}</td>
                            <td style={{ padding: '8px', textAlign: 'right' }}>
                              {order.price ? `$${order.price.toFixed(2)}` : 'Market'}
                            </td>
                            <td style={{ padding: '8px', textAlign: 'center', textTransform: 'uppercase', fontSize: '11px', color: '#f59e0b' }}>
                              {order.status}
                            </td>
                            <td style={{ padding: '8px', textAlign: 'center' }}>
                              <button
                                onClick={() => handleCancelOrder(order.id, order.symbol)}
                                style={{
                                  padding: '4px 12px',
                                  backgroundColor: '#dc262620',
                                  color: '#dc2626',
                                  border: '1px solid #dc2626',
                                  borderRadius: '4px',
                                  cursor: 'pointer',
                                  fontSize: '11px',
                                  fontWeight: 'bold',
                                  transition: 'all 0.2s'
                                }}
                                onMouseOver={(e) => {
                                  e.currentTarget.style.backgroundColor = '#dc2626';
                                  e.currentTarget.style.color = 'white';
                                }}
                                onMouseOut={(e) => {
                                  e.currentTarget.style.backgroundColor = '#dc262620';
                                  e.currentTarget.style.color = '#dc2626';
                                }}
                              >
                                Cancel
                              </button>
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  )}
                </div>
              )}

              {/* Trades Tab */}
              {activeTab === 'trades' && (
                <div>
                  {trades.length === 0 ? (
                    <div style={{ padding: '40px', textAlign: 'center', color: '#666', fontSize: '13px' }}>
                      No trade history
                    </div>
                  ) : (
                    <div style={{ maxHeight: '400px', overflowY: 'auto' }}>
                      <table style={{ width: '100%', fontSize: '12px', fontFamily: 'monospace', borderCollapse: 'collapse' }}>
                        <thead style={{ position: 'sticky', top: 0, backgroundColor: '#1a1a1a' }}>
                          <tr style={{ borderBottom: '1px solid #333' }}>
                            <th style={{ textAlign: 'left', padding: '8px', color: '#666' }}>Time</th>
                            <th style={{ textAlign: 'left', padding: '8px', color: '#666' }}>Symbol</th>
                            <th style={{ textAlign: 'center', padding: '8px', color: '#666' }}>Side</th>
                            <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Qty</th>
                            <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Price</th>
                            <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Value</th>
                            <th style={{ textAlign: 'right', padding: '8px', color: '#666' }}>Fee</th>
                          </tr>
                        </thead>
                        <tbody>
                          {trades.map((trade, idx) => (
                            <tr key={idx} style={{ borderBottom: '1px solid #222' }}>
                              <td style={{ padding: '8px', fontSize: '11px', color: '#999' }}>
                                {new Date(trade.timestamp).toLocaleTimeString()}
                              </td>
                              <td style={{ padding: '8px' }}>{trade.symbol}</td>
                              <td style={{ padding: '8px', textAlign: 'center' }}>
                                <span style={{
                                  padding: '4px 8px',
                                  borderRadius: '4px',
                                  backgroundColor: trade.side === 'buy' ? '#10b98120' : '#dc262620',
                                  color: trade.side === 'buy' ? '#10b981' : '#dc2626',
                                  fontSize: '11px',
                                  fontWeight: 'bold'
                                }}>
                                  {trade.side.toUpperCase()}
                                </span>
                              </td>
                              <td style={{ padding: '8px', textAlign: 'right' }}>{trade.amount?.toFixed(4) || '0.0000'}</td>
                              <td style={{ padding: '8px', textAlign: 'right' }}>${trade.price?.toFixed(2) || '0.00'}</td>
                              <td style={{ padding: '8px', textAlign: 'right' }}>${trade.cost?.toFixed(2) || '0.00'}</td>
                              <td style={{ padding: '8px', textAlign: 'right', color: '#dc2626' }}>
                                ${trade.fee?.cost?.toFixed(2) || '0.00'}
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  )}
                </div>
              )}

              {/* Stats Tab */}
              {activeTab === 'stats' && (
                <div>
                  {!stats ? (
                    <div style={{ padding: '40px', textAlign: 'center', color: '#666', fontSize: '13px' }}>
                      Loading statistics...
                    </div>
                  ) : (
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '16px' }}>
                      {/* Initial Balance */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Initial Balance</div>
                        <div style={{ fontSize: '20px', fontWeight: 'bold', fontFamily: 'monospace' }}>
                          ${stats.initialBalance?.toFixed(2) || '0.00'}
                        </div>
                      </div>

                      {/* Current Balance */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Current Balance</div>
                        <div style={{ fontSize: '20px', fontWeight: 'bold', fontFamily: 'monospace' }}>
                          ${stats.currentBalance?.toFixed(2) || '0.00'}
                        </div>
                      </div>

                      {/* Total Value */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Total Value</div>
                        <div style={{ fontSize: '20px', fontWeight: 'bold', fontFamily: 'monospace' }}>
                          ${stats.totalValue?.toFixed(2) || '0.00'}
                        </div>
                      </div>

                      {/* Total PnL */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Total PnL</div>
                        <div style={{
                          fontSize: '20px',
                          fontWeight: 'bold',
                          fontFamily: 'monospace',
                          color: stats.totalPnL >= 0 ? '#10b981' : '#dc2626'
                        }}>
                          {stats.totalPnL >= 0 ? '+' : ''}${stats.totalPnL?.toFixed(2) || '0.00'}
                        </div>
                      </div>

                      {/* Realized PnL */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Realized PnL</div>
                        <div style={{
                          fontSize: '20px',
                          fontWeight: 'bold',
                          fontFamily: 'monospace',
                          color: stats.realizedPnL >= 0 ? '#10b981' : '#dc2626'
                        }}>
                          {stats.realizedPnL >= 0 ? '+' : ''}${stats.realizedPnL?.toFixed(2) || '0.00'}
                        </div>
                      </div>

                      {/* Unrealized PnL */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Unrealized PnL</div>
                        <div style={{
                          fontSize: '20px',
                          fontWeight: 'bold',
                          fontFamily: 'monospace',
                          color: stats.unrealizedPnL >= 0 ? '#10b981' : '#dc2626'
                        }}>
                          {stats.unrealizedPnL >= 0 ? '+' : ''}${stats.unrealizedPnL?.toFixed(2) || '0.00'}
                        </div>
                      </div>

                      {/* Return % */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Return</div>
                        <div style={{
                          fontSize: '20px',
                          fontWeight: 'bold',
                          fontFamily: 'monospace',
                          color: parseFloat(stats.returnPercent || '0') >= 0 ? '#10b981' : '#dc2626'
                        }}>
                          {parseFloat(stats.returnPercent || '0') >= 0 ? '+' : ''}{stats.returnPercent || '0.00'}%
                        </div>
                      </div>

                      {/* Total Trades */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Total Trades</div>
                        <div style={{ fontSize: '20px', fontWeight: 'bold', fontFamily: 'monospace' }}>
                          {stats.totalTrades || 0}
                        </div>
                      </div>

                      {/* Open Positions */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Open Positions</div>
                        <div style={{ fontSize: '20px', fontWeight: 'bold', fontFamily: 'monospace', color: '#f59e0b' }}>
                          {stats.openPositions || 0}
                        </div>
                      </div>

                      {/* Closed Positions */}
                      <div style={{ padding: '16px', backgroundColor: '#0d0d0d', borderRadius: '6px' }}>
                        <div style={{ fontSize: '11px', color: '#666', marginBottom: '4px' }}>Closed Positions</div>
                        <div style={{ fontSize: '20px', fontWeight: 'bold', fontFamily: 'monospace' }}>
                          {stats.closedPositions || 0}
                        </div>
                      </div>
                    </div>
                  )}
                </div>
              )}
            </div>
          )}
        </div>

        {/* Right Column: Order Form + Order Book */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
          {/* Order Form */}
          {tradingMode === 'paper' && (
            <div>
              {!paperAdapter || !paperAdapter.isConnected() ? (
                <div style={{
                  padding: '20px',
                  backgroundColor: '#1a1a1a',
                  borderRadius: '6px',
                  textAlign: 'center',
                  border: '1px solid #f59e0b'
                }}>
                  <div style={{ fontSize: '14px', color: '#f59e0b', marginBottom: '8px', fontWeight: 'bold' }}>
                    ⚠️ Paper Trading Initializing...
                  </div>
                  <div style={{ fontSize: '12px', color: '#999' }}>
                    {isConnecting ? 'Connecting to broker...' : 'Waiting for adapter...'}
                  </div>
                  <div style={{
                    marginTop: '12px',
                    width: '100%',
                    height: '4px',
                    backgroundColor: '#333',
                    borderRadius: '2px',
                    overflow: 'hidden'
                  }}>
                    <div style={{
                      width: '50%',
                      height: '100%',
                      backgroundColor: '#f59e0b',
                      animation: 'loading 1.5s ease-in-out infinite'
                    }} />
                  </div>
                </div>
              ) : (
                <OrderForm
                  symbol={selectedSymbol}
                  currentPrice={currentPrice}
                  onPlaceOrder={handlePlaceOrder}
                  isLoading={isPlacingOrder}
                />
              )}
            </div>
          )}

          {/* Order Book */}
          <div style={{ padding: '16px', backgroundColor: '#1a1a1a', borderRadius: '6px' }}>
            <h3 style={{ marginBottom: '12px', fontSize: '14px', fontWeight: 'bold' }}>
              Order Book - {orderBook.bids.length} bids, {orderBook.asks.length} asks
            </h3>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              {/* Asks */}
              <div>
                <h4 style={{ color: '#dc2626', fontSize: '12px', marginBottom: '8px', fontWeight: 'bold' }}>ASKS</h4>
                <table style={{ width: '100%', fontSize: '11px', fontFamily: 'monospace' }}>
                  <tbody>
                    {orderBook.asks.slice(0, 10).map((ask, idx) => (
                      <tr key={idx}>
                        <td style={{ padding: '2px 4px', color: '#dc2626' }}>
                          ${ask.price.toFixed(1)}
                        </td>
                        <td style={{ padding: '2px 4px', textAlign: 'right', color: '#999' }}>
                          {ask.size.toFixed(4)}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>

              {/* Bids */}
              <div>
                <h4 style={{ color: '#10b981', fontSize: '12px', marginBottom: '8px', fontWeight: 'bold' }}>BIDS</h4>
                <table style={{ width: '100%', fontSize: '11px', fontFamily: 'monospace' }}>
                  <tbody>
                    {orderBook.bids.slice(0, 10).map((bid, idx) => (
                      <tr key={idx}>
                        <td style={{ padding: '2px 4px', color: '#10b981' }}>
                          ${bid.price.toFixed(1)}
                        </td>
                        <td style={{ padding: '2px 4px', textAlign: 'right', color: '#999' }}>
                          {bid.size.toFixed(4)}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
            <div style={{ marginTop: '12px', fontSize: '10px', color: '#555' }}>
              Last update: {lastOrderBookUpdate > 0 ? new Date(lastOrderBookUpdate).toLocaleTimeString() : 'Never'}
            </div>
          </div>
        </div>
      </div>

      {/* Footer */}
      <div style={{
        marginTop: '20px',
        padding: '16px',
        backgroundColor: '#1a1a1a',
        borderRadius: '6px',
        borderTop: '1px solid #333',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        fontSize: '12px',
        color: '#666'
      }}>
        <div style={{ display: 'flex', gap: '24px', alignItems: 'center' }}>
          <div>
            <span style={{ color: '#999', marginRight: '8px' }}>Broker:</span>
            <span style={{ color: '#f59e0b', fontWeight: 'bold' }}>
              {activeBroker || 'None'}
            </span>
          </div>
          <div>
            <span style={{ color: '#999', marginRight: '8px' }}>Mode:</span>
            <span style={{
              color: tradingMode === 'paper' ? '#10b981' : '#dc2626',
              fontWeight: 'bold'
            }}>
              {tradingMode === 'paper' ? 'Paper Trading' : 'Live Trading'}
            </span>
          </div>
          <div>
            <span style={{ color: '#999', marginRight: '8px' }}>Connection:</span>
            <span style={{
              color: activeAdapter?.isConnected() ? '#10b981' : '#dc2626',
              fontWeight: 'bold'
            }}>
              {activeAdapter?.isConnected() ? '● Connected' : '○ Disconnected'}
            </span>
          </div>
        </div>

        <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
          <div style={{ fontSize: '11px', color: '#555' }}>
            Fincept Terminal v3.0.12
          </div>
          <div style={{ fontSize: '11px', color: '#555' }}>
            © {new Date().getFullYear()} Fincept Corporation
          </div>
        </div>
      </div>
    </div>
  );
}
