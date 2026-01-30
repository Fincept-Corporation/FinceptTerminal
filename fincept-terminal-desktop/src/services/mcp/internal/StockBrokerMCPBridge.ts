// Stock Broker MCP Bridge - Connects StockBrokerContext to MCP Internal Tools
// Provides unified trading interface for all Indian stock brokers via MCP
// Supports both LIVE and PAPER trading modes

import { terminalMCPProvider } from './TerminalMCPProvider';
import type { IStockBrokerAdapter } from '@/brokers/stocks/types';
import type {
  OrderParams,
  ModifyOrderParams,
  Position,
  Holding,
  Order,
  Trade,
  Funds,
  Quote,
  MarketDepth,
  DepthLevel,
  Instrument,
  TimeFrame,
  StockExchange,
  ProductType,
  OrderType,
  OrderSide,
  OrderValidity,
  MarginRequired,
} from '@/brokers/stocks/types';

/**
 * Format date in IST (Indian Standard Time)
 */
function formatIST(): string {
  return new Date().toLocaleString('en-IN', {
    timeZone: 'Asia/Kolkata',
    year: 'numeric',
    month: 'short',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  });
}

interface StockBrokerMCPBridgeConfig {
  activeAdapter: IStockBrokerAdapter | null;
  tradingMode: 'live' | 'paper';
  activeBroker: string | null;
  isAuthenticated: boolean;
}

/**
 * Bridge that connects StockBrokerContext to MCP tools
 * Allows AI to control stock trading across ALL Indian brokers
 * (AngelOne, Zerodha, Upstox, Fyers, Dhan, AliceBlue, 5Paisa, Groww, IIFL, Kotak, Shoonya, etc.)
 */
export class StockBrokerMCPBridge {
  private config: StockBrokerMCPBridgeConfig | null = null;

  /**
   * Initialize or update the bridge with stock broker context
   */
  connect(config: StockBrokerMCPBridgeConfig): void {
    this.config = config;
    this.updateMCPContexts();
  }

  /**
   * Disconnect and clear contexts
   */
  disconnect(): void {
    this.config = null;
    terminalMCPProvider.setContexts({
      placeStockTrade: undefined,
      cancelStockOrder: undefined,
      modifyStockOrder: undefined,
      getStockPositions: undefined,
      getStockHoldings: undefined,
      getStockFunds: undefined,
      getStockOrders: undefined,
      getStockTrades: undefined,
      getStockMarginRequired: undefined,
      convertStockPosition: undefined,
      exitStockPosition: undefined,
      getStockQuote: undefined,
      getStockMarketDepth: undefined,
      searchStockInstruments: undefined,
      getStockHistoricalData: undefined,
    });
  }

  /**
   * Update MCP contexts with current adapter methods
   */
  private updateMCPContexts(): void {
    if (!this.config?.activeAdapter || !this.config.isAuthenticated) {
      console.warn('[StockBrokerMCPBridge] No active authenticated adapter available');
      return;
    }

    const { activeAdapter, tradingMode, activeBroker } = this.config;

    terminalMCPProvider.setContexts({
      // ========================================================================
      // ORDER PLACEMENT & MANAGEMENT
      // ========================================================================

      /**
       * Place a stock order (Market, Limit, SL, SL-M, etc.)
       * Supports both live and paper trading
       */
      placeStockTrade: async (params: {
        symbol: string;
        exchange: string;
        transaction_type: 'BUY' | 'SELL';
        quantity: number;
        order_type: string;
        product_type: string;
        price?: number;
        trigger_price?: number;
        disclosed_quantity?: number;
        validity?: string;
        variety?: string;
        tag?: string;
        token?: string;
        tradingSymbol?: string;
      }) => {
        try {
          // Route to unified trading service if in paper mode
          if (tradingMode === 'paper') {
            // Use unified trading service for paper trading
            const { placeOrder: unifiedPlaceOrder } = await import('@/services/unifiedTradingService');

            // Handle both 'side' and 'transaction_type' (AI might send either)
            const transactionType = (params.transaction_type || (params as any).side || 'BUY').toUpperCase();
            const side = transactionType.toLowerCase() as 'buy' | 'sell';

            // For market orders, fetch current market price
            let marketPrice = params.price;
            if (params.order_type.toLowerCase() === 'market' && !marketPrice && activeAdapter) {
              try {
                const quote = await activeAdapter.getQuote(params.symbol, params.exchange as any);
                marketPrice = quote.lastPrice;
                console.log(`[StockBrokerMCPBridge] Fetched market price for ${params.symbol}: ${marketPrice}`);
              } catch (err) {
                console.warn(`[StockBrokerMCPBridge] Failed to fetch market price for ${params.symbol}:`, err);
                // Will fall back to placeholder price in Rust
              }
            }

            const unifiedOrder = {
              symbol: params.symbol,
              exchange: params.exchange,
              side: side,
              order_type: params.order_type.toLowerCase() as 'market' | 'limit' | 'stop' | 'stop_limit',
              quantity: params.quantity,
              price: marketPrice,
              stop_price: params.trigger_price,
              product_type: params.product_type as 'CNC' | 'MIS' | 'NRML',
            };

            const result = await unifiedPlaceOrder(unifiedOrder);

            // Dispatch event to trigger UI refresh
            if (result.success) {
              window.dispatchEvent(new CustomEvent('stock-order-placed', {
                detail: {
                  orderId: result.order_id,
                  symbol: params.symbol,
                  exchange: params.exchange,
                  mode: tradingMode
                }
              }));
            }

            return {
              success: result.success,
              orderId: result.order_id,
              broker: activeBroker,
              mode: tradingMode,
              symbol: params.symbol,
              transactionType: transactionType,
              quantity: params.quantity,
              orderType: params.order_type,
              productType: params.product_type,
              price: params.price,
              message: result.message,
              timestamp: formatIST(),
            };
          }

          // Live mode: use adapter directly
          const orderParams: OrderParams = {
            symbol: params.symbol,
            exchange: params.exchange as StockExchange,
            side: params.transaction_type as OrderSide,
            quantity: params.quantity,
            orderType: params.order_type as OrderType,
            productType: params.product_type as ProductType,
            price: params.price,
            triggerPrice: params.trigger_price,
            disclosedQuantity: params.disclosed_quantity || 0,
            validity: (params.validity || 'DAY') as OrderValidity,
            tag: params.tag,
            token: params.token,
            tradingSymbol: params.tradingSymbol,
          };

          const result = await activeAdapter.placeOrder(orderParams);

          // Dispatch event to trigger UI refresh
          if (result.success) {
            window.dispatchEvent(new CustomEvent('stock-order-placed', {
              detail: {
                orderId: result.orderId,
                symbol: params.symbol,
                exchange: params.exchange,
                mode: tradingMode
              }
            }));
          }

          return {
            success: true,
            orderId: result.orderId,
            broker: activeBroker,
            mode: tradingMode,
            symbol: params.symbol,
            transactionType: params.transaction_type,
            quantity: params.quantity,
            orderType: params.order_type,
            productType: params.product_type,
            price: params.price,
            message: result.message,
            timestamp: formatIST(),
          };
        } catch (error: any) {
          console.error('[StockBrokerMCPBridge] Order placement error:', error);
          const errorMsg = error?.message || error?.toString() || 'Unknown error';
          throw new Error(`Failed to place stock order: ${errorMsg}`);
        }
      },

      /**
       * Modify an existing order
       */
      modifyStockOrder: async (params: {
        order_id: string;
        quantity?: number;
        price?: number;
        trigger_price?: number;
        order_type?: string;
        validity?: string;
      }) => {
        try {
          const modifyParams: ModifyOrderParams = {
            orderId: params.order_id,
            quantity: params.quantity,
            price: params.price,
            triggerPrice: params.trigger_price,
            orderType: params.order_type as OrderType | undefined,
            validity: params.validity as 'DAY' | 'IOC' | undefined,
          };

          const result = await activeAdapter.modifyOrder(modifyParams);

          return {
            success: true,
            orderId: result.orderId,
            message: result.message,
            timestamp: new Date().toISOString(),
          };
        } catch (error: any) {
          throw new Error(`Failed to modify order: ${error.message}`);
        }
      },

      /**
       * Cancel an order by ID
       */
      cancelStockOrder: async (orderId: string, variety?: string) => {
        try {
          const result = await activeAdapter.cancelOrder(orderId);

          return {
            success: true,
            orderId: result.orderId,
            message: result.message,
            timestamp: new Date().toISOString(),
          };
        } catch (error: any) {
          throw new Error(`Failed to cancel order: ${error.message}`);
        }
      },

      // ========================================================================
      // POSITION & HOLDINGS MANAGEMENT
      // ========================================================================

      /**
       * Get all open positions
       */
      getStockPositions: async () => {
        try {
          const positions = await activeAdapter.getPositions();
          return positions.map((p: Position) => ({
            symbol: p.symbol,
            exchange: p.exchange,
            productType: p.productType,
            quantity: p.quantity,
            averagePrice: p.averagePrice,
            lastPrice: p.lastPrice,
            pnl: p.pnl,
            pnlPercent: p.pnlPercent,
            dayPnl: p.dayPnl,
            buyQuantity: p.buyQuantity,
            sellQuantity: p.sellQuantity,
            buyValue: p.buyValue,
            sellValue: p.sellValue,
          }));
        } catch (error: any) {
          console.error('[StockBrokerMCPBridge] Failed to fetch positions:', error);
          return [];
        }
      },

      /**
       * Get all holdings (long-term investments)
       */
      getStockHoldings: async () => {
        try {
          const holdings = await activeAdapter.getHoldings();
          return holdings.map((h: Holding) => ({
            symbol: h.symbol,
            exchange: h.exchange,
            isin: h.isin,
            quantity: h.quantity,
            averagePrice: h.averagePrice,
            lastPrice: h.lastPrice,
            pnl: h.pnl,
            pnlPercent: h.pnlPercent,
            investedValue: h.investedValue,
            currentValue: h.currentValue,
            collateralQuantity: h.collateralQuantity,
            pledgedQuantity: h.pledgedQuantity,
            t1Quantity: h.t1Quantity,
          }));
        } catch (error: any) {
          console.error('[StockBrokerMCPBridge] Failed to fetch holdings:', error);
          return [];
        }
      },

      /**
       * Get account funds/margins
       */
      getStockFunds: async () => {
        try {
          const funds = await activeAdapter.getFunds();
          return {
            availableCash: funds.availableCash,
            usedMargin: funds.usedMargin,
            availableMargin: funds.availableMargin,
            totalBalance: funds.totalBalance,
            currency: funds.currency,
            equityAvailable: funds.equityAvailable,
            commodityAvailable: funds.commodityAvailable,
            collateral: funds.collateral,
            unrealizedPnl: funds.unrealizedPnl,
            realizedPnl: funds.realizedPnl,
          };
        } catch (error: any) {
          throw new Error(`Failed to fetch funds: ${error.message}`);
        }
      },

      // ========================================================================
      // ORDER & TRADE HISTORY
      // ========================================================================

      /**
       * Get all orders (with optional filtering)
       */
      getStockOrders: async (filters?: { status?: string; symbol?: string }) => {
        try {
          const orders = await activeAdapter.getOrders();

          let filteredOrders = orders;

          // Apply status filter
          if (filters?.status) {
            filteredOrders = filteredOrders.filter((o: Order) =>
              o.status.toLowerCase() === filters.status?.toLowerCase()
            );
          }

          // Apply symbol filter
          if (filters?.symbol) {
            filteredOrders = filteredOrders.filter((o: Order) =>
              o.symbol === filters.symbol
            );
          }

          return filteredOrders.map((o: Order) => ({
            orderId: o.orderId,
            symbol: o.symbol,
            exchange: o.exchange,
            side: o.side,
            orderType: o.orderType,
            productType: o.productType,
            quantity: o.quantity,
            price: o.price,
            triggerPrice: o.triggerPrice,
            validity: o.validity,
            status: o.status,
            filledQuantity: o.filledQuantity,
            pendingQuantity: o.pendingQuantity,
            averagePrice: o.averagePrice,
            placedAt: o.placedAt,
            updatedAt: o.updatedAt,
            exchangeOrderId: o.exchangeOrderId,
            statusMessage: o.statusMessage,
            tag: o.tag,
          }));
        } catch (error: any) {
          throw new Error(`Failed to fetch orders: ${error.message}`);
        }
      },

      /**
       * Get trade book (executed trades)
       */
      getStockTrades: async () => {
        try {
          const trades = await activeAdapter.getTradeBook();
          return trades.map((t: Trade) => ({
            tradeId: t.tradeId,
            orderId: t.orderId,
            symbol: t.symbol,
            exchange: t.exchange,
            side: t.side,
            productType: t.productType,
            quantity: t.quantity,
            price: t.price,
            tradedAt: t.tradedAt,
            exchangeTradeId: t.exchangeTradeId,
          }));
        } catch (error: any) {
          throw new Error(`Failed to fetch trades: ${error.message}`);
        }
      },

      // ========================================================================
      // MARGIN & RISK CALCULATION
      // ========================================================================

      /**
       * Calculate margin required for an order
       */
      getStockMarginRequired: async (params: {
        symbol: string;
        exchange: string;
        transaction_type: 'BUY' | 'SELL';
        quantity: number;
        order_type: string;
        product_type: string;
        price?: number;
      }) => {
        try {
          // Check if broker supports margin calculation
          if (typeof activeAdapter.getMarginRequired !== 'function') {
            return {
              success: false,
              error: 'Margin calculation not supported by this broker',
            };
          }

          const orderParams: OrderParams = {
            symbol: params.symbol,
            exchange: params.exchange as StockExchange,
            side: params.transaction_type as OrderSide,
            quantity: params.quantity,
            orderType: params.order_type as OrderType,
            productType: params.product_type as ProductType,
            price: params.price,
          };

          const margin = await activeAdapter.getMarginRequired(orderParams);

          return {
            success: true,
            totalMargin: margin.totalMargin,
            initialMargin: margin.initialMargin,
            exposureMargin: margin.exposureMargin,
            spanMargin: margin.spanMargin,
            optionPremium: margin.optionPremium,
          };
        } catch (error: any) {
          throw new Error(`Failed to calculate margin: ${error.message}`);
        }
      },

      // ========================================================================
      // POSITION CONVERSION & EXIT
      // ========================================================================

      /**
       * Convert position (e.g., Intraday to Delivery)
       */
      convertStockPosition: async (params: {
        symbol: string;
        exchange: string;
        transaction_type: 'BUY' | 'SELL';
        quantity: number;
        old_product_type: string;
        new_product_type: string;
      }) => {
        try {
          // Check if broker supports position conversion
          if (typeof (activeAdapter as any).convertPosition !== 'function') {
            return {
              success: false,
              error: 'Position conversion not supported by this broker',
            };
          }

          await (activeAdapter as any).convertPosition(
            params.symbol,
            params.exchange as StockExchange,
            params.transaction_type,
            params.quantity,
            params.old_product_type as ProductType,
            params.new_product_type as ProductType
          );

          return {
            success: true,
            message: `Position converted from ${params.old_product_type} to ${params.new_product_type}`,
          };
        } catch (error: any) {
          throw new Error(`Failed to convert position: ${error.message}`);
        }
      },

      /**
       * Exit a position (close all quantity)
       */
      exitStockPosition: async (params: {
        symbol: string;
        exchange: string;
        product_type: string;
      }) => {
        try {
          // Get current positions to find the position
          const positions = await activeAdapter.getPositions();
          const position = positions.find(
            (p: Position) =>
              p.symbol === params.symbol &&
              p.exchange === params.exchange &&
              p.productType === params.product_type
          );

          if (!position) {
            throw new Error(`No open position found for ${params.symbol}`);
          }

          // Determine exit side (opposite of current position)
          const exitSide: 'BUY' | 'SELL' = position.quantity > 0 ? 'SELL' : 'BUY';
          const exitQuantity = Math.abs(position.quantity);

          // Place market order to exit
          const orderParams: OrderParams = {
            symbol: params.symbol,
            exchange: params.exchange as StockExchange,
            side: exitSide,
            quantity: exitQuantity,
            orderType: 'MARKET' as OrderType,
            productType: params.product_type as ProductType,
            validity: 'DAY',
          };

          const result = await activeAdapter.placeOrder(orderParams);

          return {
            success: true,
            orderId: result.orderId,
            symbol: params.symbol,
            quantity: exitQuantity,
            side: exitSide,
            message: `Position exited: ${exitSide} ${exitQuantity} ${params.symbol}`,
          };
        } catch (error: any) {
          throw new Error(`Failed to exit position: ${error.message}`);
        }
      },

      // ========================================================================
      // MARKET DATA
      // ========================================================================

      /**
       * Get real-time quote for a symbol
       */
      getStockQuote: async (params: { symbol: string; exchange: string }) => {
        try {
          const quote = await activeAdapter.getQuote(
            params.symbol,
            params.exchange as StockExchange
          );

          return {
            symbol: quote.symbol,
            exchange: quote.exchange,
            lastPrice: quote.lastPrice,
            open: quote.open,
            high: quote.high,
            low: quote.low,
            close: quote.close,
            previousClose: quote.previousClose,
            volume: quote.volume,
            averagePrice: quote.averagePrice,
            change: quote.change,
            changePercent: quote.changePercent,
            bid: quote.bid,
            bidQty: quote.bidQty,
            ask: quote.ask,
            askQty: quote.askQty,
            lowerCircuit: quote.lowerCircuit,
            upperCircuit: quote.upperCircuit,
            timestamp: quote.timestamp,
          };
        } catch (error: any) {
          throw new Error(`Failed to fetch quote: ${error.message}`);
        }
      },

      /**
       * Get market depth (order book)
       */
      getStockMarketDepth: async (params: { symbol: string; exchange: string }) => {
        try {
          const depth = await activeAdapter.getMarketDepth(
            params.symbol,
            params.exchange as StockExchange
          );

          return {
            bids: depth.bids.map((b: DepthLevel) => ({
              price: b.price,
              quantity: b.quantity,
              orders: b.orders,
            })),
            asks: depth.asks.map((s: DepthLevel) => ({
              price: s.price,
              quantity: s.quantity,
              orders: s.orders,
            })),
            timestamp: depth.timestamp,
          };
        } catch (error: any) {
          throw new Error(`Failed to fetch market depth: ${error.message}`);
        }
      },

      /**
       * Search for instruments/symbols
       */
      searchStockInstruments: async (query: string, exchange?: string) => {
        try {
          const instruments = await activeAdapter.searchSymbols(
            query,
            exchange as StockExchange | undefined
          );

          return instruments.map((i: Instrument) => ({
            instrumentToken: i.instrumentToken,
            exchangeToken: i.exchangeToken,
            symbol: i.symbol,
            tradingSymbol: i.tradingSymbol,
            name: i.name,
            exchange: i.exchange,
            segment: i.segment,
            instrumentType: i.instrumentType,
            lotSize: i.lotSize,
            tickSize: i.tickSize,
          }));
        } catch (error: any) {
          throw new Error(`Failed to search instruments: ${error.message}`);
        }
      },

      /**
       * Get historical OHLCV data
       */
      getStockHistoricalData: async (params: {
        symbol: string;
        exchange: string;
        timeframe: string;
        from_date: string;
        to_date: string;
      }) => {
        try {
          // Check if broker supports historical data
          if (typeof (activeAdapter as any).getHistoricalData !== 'function') {
            return {
              success: false,
              error: 'Historical data not supported by this broker',
              data: [],
            };
          }

          const data = await (activeAdapter as any).getHistoricalData(
            params.symbol,
            params.exchange as StockExchange,
            params.timeframe as TimeFrame,
            new Date(params.from_date),
            new Date(params.to_date)
          );

          return data.map((candle: { timestamp: number; open: number; high: number; low: number; close: number; volume: number }) => ({
            timestamp: candle.timestamp,
            open: candle.open,
            high: candle.high,
            low: candle.low,
            close: candle.close,
            volume: candle.volume,
          }));
        } catch (error: any) {
          throw new Error(`Failed to fetch historical data: ${error.message}`);
        }
      },
    });

    console.log(
      `[StockBrokerMCPBridge] Connected to ${activeBroker} (${tradingMode} mode)`
    );
  }

  /**
   * Check if bridge is connected
   */
  isConnected(): boolean {
    return (
      this.config !== null &&
      this.config.activeAdapter !== null &&
      this.config.isAuthenticated
    );
  }

  /**
   * Get current configuration
   */
  getConfig(): StockBrokerMCPBridgeConfig | null {
    return this.config;
  }

  /**
   * Get connection status details
   */
  getStatus(): {
    connected: boolean;
    broker: string | null;
    mode: 'live' | 'paper' | null;
    authenticated: boolean;
  } {
    return {
      connected: this.isConnected(),
      broker: this.config?.activeBroker || null,
      mode: this.config?.tradingMode || null,
      authenticated: this.config?.isAuthenticated || false,
    };
  }
}

// Singleton instance
export const stockBrokerMCPBridge = new StockBrokerMCPBridge();
