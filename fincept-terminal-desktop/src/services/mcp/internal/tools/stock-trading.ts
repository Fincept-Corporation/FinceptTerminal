// Stock Trading MCP Tools - Unified multi-broker stock trading tools
// Supports ALL Indian stock brokers (AngelOne, Zerodha, Upstox, Fyers, Dhan, etc.)
// Works with both LIVE and PAPER trading modes

import { InternalTool } from '../types';

export const stockTradingTools: InternalTool[] = [
  // ============================================================================
  // ORDER PLACEMENT
  // ============================================================================

  {
    name: 'stock_place_market_order',
    description: '[PRIMARY TOOL FOR STOCKS] Place a stock market order on NSE/BSE (buy/sell at current market price). Automatically searches for the correct instrument. Use this for all stock orders on Indian exchanges.',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol (e.g., RELIANCE, TCS, INFY). Use exact symbol from search results.' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Transaction type', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares to trade' },
        product_type: { type: 'string', description: 'Product type', enum: ['INTRADAY', 'DELIVERY', 'MARGIN', 'BO', 'CO'] },
        tag: { type: 'string', description: 'Optional tag for order identification' },
        auto_search: { type: 'boolean', description: 'Auto-search symbol if not found (default: true)' },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeStockTrade) {
        return { success: false, error: 'Stock trading context not available. Ensure stock broker is connected and authenticated.' };
      }

      // Default product type to DELIVERY if not specified
      const productType = args.product_type || 'DELIVERY';

      // Ensure quantity is a number (AI might send as string)
      const quantity = typeof args.quantity === 'string' ? parseFloat(args.quantity) : args.quantity;

      // Auto-search symbol if enabled (default true)
      let finalSymbol = args.symbol;
      let finalExchange = args.exchange;
      let symbolToken: string | undefined;

      if (args.auto_search !== false && contexts.searchStockInstruments) {
        try {
          const searchResults = await contexts.searchStockInstruments(args.symbol, args.exchange);

          if (searchResults.length === 0) {
            return {
              success: false,
              error: `Symbol "${args.symbol}" not found on ${args.exchange}. Please verify the symbol.`,
            };
          }

          // Use first result (most relevant)
          const instrument = searchResults[0];
          finalSymbol = instrument.tradingSymbol || instrument.symbol;
          finalExchange = instrument.exchange;
          symbolToken = instrument.instrumentToken || instrument.token;

          console.log(`[MCP] Symbol resolved: "${args.symbol}" → "${finalSymbol}" (token: ${symbolToken}) on ${finalExchange}`);
        } catch (searchErr) {
          console.warn(`[MCP] Symbol search failed, using original: ${searchErr}`);
          // Continue with original symbol
        }
      }

      const result = await contexts.placeStockTrade({
        ...args,
        symbol: finalSymbol,
        exchange: finalExchange,
        token: symbolToken,
        quantity: quantity,
        product_type: productType,
        order_type: 'MARKET',
        validity: 'DAY',
      });

      return {
        success: true,
        data: result,
        message: `Market ${args.transaction_type} order placed: ${quantity} ${finalSymbol} @ ${finalExchange} (${productType})`,
      };
    },
  },

  {
    name: 'stock_place_limit_order',
    description: 'Place a limit order for stocks (buy/sell at specified price). Auto-searches symbol if not exact.',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol. Can be partial (e.g., "rites", "tcs") - will auto-search.' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Transaction type', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares' },
        price: { type: 'number', description: 'Limit price' },
        product_type: { type: 'string', description: 'Product type', enum: ['INTRADAY', 'DELIVERY', 'MARGIN', 'BO', 'CO'] },
        disclosed_quantity: { type: 'number', description: 'Disclosed quantity (iceberg order)' },
        validity: { type: 'string', description: 'Order validity', enum: ['DAY', 'IOC'] },
        tag: { type: 'string', description: 'Optional tag' },
        auto_search: { type: 'boolean', description: 'Auto-search symbol (default: true)' },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity', 'price', 'product_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeStockTrade) {
        return { success: false, error: 'Stock trading context not available' };
      }

      // Auto-resolve symbol
      let finalSymbol = args.symbol;
      let finalExchange = args.exchange;
      let symbolToken: string | undefined;

      if (args.auto_search !== false && contexts.searchStockInstruments) {
        try {
          const searchResults = await contexts.searchStockInstruments(args.symbol, args.exchange);

          if (searchResults.length === 0) {
            return {
              success: false,
              error: `Symbol "${args.symbol}" not found on ${args.exchange}. Try stock_search_instruments first.`,
            };
          }

          const instrument = searchResults[0];
          finalSymbol = instrument.tradingSymbol || instrument.symbol;
          finalExchange = instrument.exchange;
          symbolToken = instrument.instrumentToken || instrument.token;

          console.log(`[MCP] Symbol resolved: "${args.symbol}" → "${finalSymbol}" (token: ${symbolToken}) on ${finalExchange}`);
        } catch (searchErr) {
          console.warn(`[MCP] Symbol search failed: ${searchErr}`);
        }
      }

      const result = await contexts.placeStockTrade({
        ...args,
        symbol: finalSymbol,
        exchange: finalExchange,
        token: symbolToken,
        order_type: 'LIMIT',
        validity: args.validity || 'DAY',
      });

      return {
        success: true,
        data: result,
        message: `Limit ${args.transaction_type} order placed: ${args.quantity} ${finalSymbol} @ ₹${args.price}`,
      };
    },
  },

  {
    name: 'stock_place_stop_loss_order',
    description: 'Place a stop-loss order (SL or SL-M) for stocks',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Transaction type', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares' },
        trigger_price: { type: 'number', description: 'Stop-loss trigger price' },
        price: { type: 'number', description: 'Limit price (optional, for SL order. Omit for SL-M)' },
        product_type: { type: 'string', description: 'Product type', enum: ['INTRADAY', 'DELIVERY', 'MARGIN'] },
        tag: { type: 'string', description: 'Optional tag' },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity', 'trigger_price', 'product_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeStockTrade) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const orderType = args.price ? 'SL' : 'SL-M';
      const result = await contexts.placeStockTrade({
        ...args,
        order_type: orderType,
        validity: 'DAY',
      });
      return {
        success: true,
        data: result,
        message: `Stop-loss ${args.transaction_type} order placed: ${args.quantity} ${args.symbol} @ trigger ₹${args.trigger_price}`,
      };
    },
  },

  {
    name: 'stock_place_bracket_order',
    description: 'Place a bracket order (BO) with stop-loss and target',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Transaction type', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares' },
        price: { type: 'number', description: 'Entry price' },
        stop_loss: { type: 'number', description: 'Stop-loss points from entry' },
        target: { type: 'number', description: 'Target profit points from entry' },
        trailing_stop_loss: { type: 'number', description: 'Trailing SL points (optional)' },
        tag: { type: 'string', description: 'Optional tag' },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity', 'price', 'stop_loss', 'target'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeStockTrade) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const result = await contexts.placeStockTrade({
        ...args,
        order_type: 'LIMIT',
        product_type: 'BO',
        variety: 'bo',
        validity: 'DAY',
      });
      return {
        success: true,
        data: result,
        message: `Bracket order placed: ${args.transaction_type} ${args.quantity} ${args.symbol} (SL: ${args.stop_loss}, Target: ${args.target})`,
      };
    },
  },

  {
    name: 'stock_place_cover_order',
    description: 'Place a cover order (CO) with mandatory stop-loss',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Transaction type', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares' },
        trigger_price: { type: 'number', description: 'Stop-loss trigger price' },
        tag: { type: 'string', description: 'Optional tag' },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity', 'trigger_price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeStockTrade) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const result = await contexts.placeStockTrade({
        ...args,
        order_type: 'MARKET',
        product_type: 'CO',
        variety: 'co',
        validity: 'DAY',
      });
      return {
        success: true,
        data: result,
        message: `Cover order placed: ${args.transaction_type} ${args.quantity} ${args.symbol} with SL @ ₹${args.trigger_price}`,
      };
    },
  },

  {
    name: 'stock_place_amo_order',
    description: 'Place an After Market Order (AMO) for next trading day',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Transaction type', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares' },
        order_type: { type: 'string', description: 'Order type', enum: ['MARKET', 'LIMIT', 'SL', 'SL-M'] },
        price: { type: 'number', description: 'Price (for LIMIT/SL orders)' },
        trigger_price: { type: 'number', description: 'Trigger price (for SL/SL-M orders)' },
        product_type: { type: 'string', description: 'Product type', enum: ['INTRADAY', 'DELIVERY'] },
        tag: { type: 'string', description: 'Optional tag' },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity', 'order_type', 'product_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeStockTrade) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const result = await contexts.placeStockTrade({
        ...args,
        variety: 'amo',
        validity: 'DAY',
      });
      return {
        success: true,
        data: result,
        message: `AMO placed: ${args.transaction_type} ${args.quantity} ${args.symbol} (${args.order_type})`,
      };
    },
  },

  // ============================================================================
  // ORDER MANAGEMENT
  // ============================================================================

  {
    name: 'stock_modify_order',
    description: 'Modify an existing order (price, quantity, trigger)',
    inputSchema: {
      type: 'object',
      properties: {
        order_id: { type: 'string', description: 'Order ID to modify' },
        quantity: { type: 'number', description: 'New quantity' },
        price: { type: 'number', description: 'New limit price' },
        trigger_price: { type: 'number', description: 'New trigger price (for SL orders)' },
        order_type: { type: 'string', description: 'New order type', enum: ['MARKET', 'LIMIT', 'SL', 'SL-M'] },
        validity: { type: 'string', description: 'New validity', enum: ['DAY', 'IOC'] },
      },
      required: ['order_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.modifyStockOrder) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const result = await contexts.modifyStockOrder(args);
      return {
        success: true,
        data: result,
        message: `Order ${args.order_id} modified successfully`,
      };
    },
  },

  {
    name: 'stock_cancel_order',
    description: 'Cancel a pending order by ID',
    inputSchema: {
      type: 'object',
      properties: {
        order_id: { type: 'string', description: 'Order ID to cancel' },
        variety: { type: 'string', description: 'Order variety (regular, amo, bo, co)' },
      },
      required: ['order_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.cancelStockOrder) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const result = await contexts.cancelStockOrder(args.order_id, args.variety);
      return {
        success: true,
        data: result,
        message: `Order ${args.order_id} cancelled`,
      };
    },
  },

  {
    name: 'stock_cancel_all_orders',
    description: 'Cancel all open orders (with optional symbol filter)',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Symbol to cancel orders for (optional, cancels all if not provided)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockOrders || !contexts.cancelStockOrder) {
        return { success: false, error: 'Stock trading context not available' };
      }

      const orders = await contexts.getStockOrders({ status: 'open' });
      const targetOrders = args.symbol
        ? orders.filter((o: any) => o.symbol === args.symbol || o.tradingSymbol === args.symbol)
        : orders;

      const results = await Promise.allSettled(
        targetOrders.map((o: any) => contexts.cancelStockOrder!(o.orderId, o.variety))
      );

      const cancelled = results.filter((r) => r.status === 'fulfilled').length;
      return {
        success: true,
        data: { cancelled, total: targetOrders.length },
        message: `Cancelled ${cancelled}/${targetOrders.length} orders`,
      };
    },
  },

  // ============================================================================
  // POSITION & HOLDINGS
  // ============================================================================

  {
    name: 'stock_get_positions',
    description: 'Get all open stock positions',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Filter by symbol (optional)' },
        product: { type: 'string', description: 'Filter by product type (optional)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockPositions) {
        return { success: false, error: 'Stock trading context not available' };
      }
      let positions = await contexts.getStockPositions();

      if (args.symbol) {
        positions = positions.filter((p: any) => p.symbol === args.symbol || p.tradingSymbol === args.symbol);
      }
      if (args.product) {
        positions = positions.filter((p: any) => p.product === args.product);
      }

      return { success: true, data: positions };
    },
  },

  {
    name: 'stock_get_holdings',
    description: 'Get all long-term stock holdings',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Filter by symbol (optional)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockHoldings) {
        return { success: false, error: 'Stock trading context not available' };
      }
      let holdings = await contexts.getStockHoldings();

      if (args.symbol) {
        holdings = holdings.filter((h: any) => h.symbol === args.symbol || h.tradingSymbol === args.symbol);
      }

      return { success: true, data: holdings };
    },
  },

  {
    name: 'stock_convert_position',
    description: 'Convert position from one product type to another (e.g., Intraday to Delivery)',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Position side', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Quantity to convert' },
        old_product_type: { type: 'string', description: 'Current product type', enum: ['INTRADAY', 'DELIVERY', 'MARGIN'] },
        new_product_type: { type: 'string', description: 'New product type', enum: ['INTRADAY', 'DELIVERY', 'MARGIN'] },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity', 'old_product_type', 'new_product_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.convertStockPosition) {
        return { success: false, error: 'Position conversion not available' };
      }
      const result = await contexts.convertStockPosition(args);
      return {
        success: true,
        data: result,
        message: `Position converted: ${args.quantity} ${args.symbol} from ${args.old_product_type} to ${args.new_product_type}`,
      };
    },
  },

  {
    name: 'stock_exit_position',
    description: 'Exit/close an open position completely',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        product_type: { type: 'string', description: 'Product type', enum: ['INTRADAY', 'DELIVERY', 'MARGIN'] },
      },
      required: ['symbol', 'exchange', 'product_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.exitStockPosition) {
        return { success: false, error: 'Position exit not available' };
      }
      const result = await contexts.exitStockPosition(args);
      return {
        success: true,
        data: result,
        message: `Position exited for ${args.symbol}`,
      };
    },
  },

  // ============================================================================
  // ACCOUNT INFO
  // ============================================================================

  {
    name: 'stock_get_funds',
    description: 'Get account funds/margins (equity and commodity)',
    inputSchema: {
      type: 'object',
      properties: {
        segment: { type: 'string', description: 'Segment filter', enum: ['equity', 'commodity'] },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockFunds) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const funds = await contexts.getStockFunds();

      if (args.segment) {
        return { success: true, data: funds[args.segment] };
      }

      return { success: true, data: funds };
    },
  },

  {
    name: 'stock_get_margin_required',
    description: 'Calculate margin required for an order before placing it',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        transaction_type: { type: 'string', description: 'Transaction type', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares' },
        order_type: { type: 'string', description: 'Order type', enum: ['MARKET', 'LIMIT', 'SL', 'SL-M'] },
        product_type: { type: 'string', description: 'Product type', enum: ['INTRADAY', 'DELIVERY', 'MARGIN', 'BO', 'CO'] },
        price: { type: 'number', description: 'Price (for LIMIT orders)' },
      },
      required: ['symbol', 'exchange', 'transaction_type', 'quantity', 'order_type', 'product_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockMarginRequired) {
        return { success: false, error: 'Margin calculation not available' };
      }
      const result = await contexts.getStockMarginRequired(args);
      return { success: true, data: result };
    },
  },

  // ============================================================================
  // ORDER & TRADE HISTORY
  // ============================================================================

  {
    name: 'stock_get_orders',
    description: 'Get order history with optional filters',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Filter by symbol' },
        status: { type: 'string', description: 'Filter by status', enum: ['open', 'complete', 'cancelled', 'rejected', 'pending'] },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockOrders) {
        return { success: false, error: 'Stock trading context not available' };
      }
      const orders = await contexts.getStockOrders(args);
      return { success: true, data: orders };
    },
  },

  {
    name: 'stock_get_trades',
    description: 'Get executed trades (trade book)',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Filter by symbol (optional)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockTrades) {
        return { success: false, error: 'Stock trading context not available' };
      }
      let trades = await contexts.getStockTrades();

      if (args.symbol) {
        trades = trades.filter((t: any) => t.symbol === args.symbol || t.tradingSymbol === args.symbol);
      }

      return { success: true, data: trades };
    },
  },

  // ============================================================================
  // MARKET DATA
  // ============================================================================

  {
    name: 'stock_get_quote',
    description: 'Get real-time quote for a stock symbol',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol (e.g., RELIANCE)' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
      },
      required: ['symbol', 'exchange'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockQuote) {
        return { success: false, error: 'Market data context not available' };
      }
      const quote = await contexts.getStockQuote(args);
      return { success: true, data: quote };
    },
  },

  {
    name: 'stock_get_market_depth',
    description: 'Get market depth (order book with bid/ask levels)',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
      },
      required: ['symbol', 'exchange'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockMarketDepth) {
        return { success: false, error: 'Market data context not available' };
      }
      const depth = await contexts.getStockMarketDepth(args);
      return { success: true, data: depth };
    },
  },

  {
    name: 'stock_search_instruments',
    description: 'Search for stock symbols. Returns exact and fuzzy matches. ALWAYS use this before placing orders with partial/ambiguous symbols.',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Search query - supports fuzzy search (e.g., "rites", "tata", "reliance")' },
        exchange: { type: 'string', description: 'Exchange filter (optional)', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        limit: { type: 'number', description: 'Max results to return (default: 10)' },
      },
      required: ['query'],
    },
    handler: async (args, contexts) => {
      if (!contexts.searchStockInstruments) {
        return { success: false, error: 'Instrument search not available. Ensure broker is authenticated.' };
      }

      try {
        const instruments = await contexts.searchStockInstruments(args.query, args.exchange);

        // Limit results
        const limit = args.limit || 10;
        const limitedResults = instruments.slice(0, limit);

        if (limitedResults.length === 0) {
          return {
            success: false,
            error: `No instruments found matching "${args.query}"${args.exchange ? ` on ${args.exchange}` : ''}. Check spelling or try broader search.`,
            data: [],
          };
        }

        // Format results for better AI understanding
        const formattedResults = limitedResults.map((inst: any) => ({
          symbol: inst.symbol,
          tradingSymbol: inst.tradingSymbol,
          name: inst.name,
          exchange: inst.exchange,
          instrumentType: inst.instrumentType,
          lotSize: inst.lotSize,
        }));

        return {
          success: true,
          data: formattedResults,
          message: `Found ${limitedResults.length} match${limitedResults.length > 1 ? 'es' : ''} for "${args.query}". Use tradingSymbol for orders.`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: `Search failed: ${error.message}`,
        };
      }
    },
  },

  {
    name: 'stock_get_historical_data',
    description: 'Get historical OHLCV data for a stock',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
        timeframe: { type: 'string', description: 'Timeframe', enum: ['1minute', '5minute', '15minute', '30minute', '60minute', 'day', 'week', 'month'] },
        from_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        to_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
      },
      required: ['symbol', 'exchange', 'timeframe', 'from_date', 'to_date'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getStockHistoricalData) {
        return { success: false, error: 'Historical data not available' };
      }
      const data = await contexts.getStockHistoricalData(args);
      return { success: true, data };
    },
  },

  // ============================================================================
  // WATCHLIST & FAVORITES
  // ============================================================================

  {
    name: 'stock_add_to_watchlist',
    description: 'Add a stock symbol to watchlist',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
      },
      required: ['symbol', 'exchange'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addToWatchlist) {
        return { success: false, error: 'Watchlist not available' };
      }
      await contexts.addToWatchlist(`${args.symbol}:${args.exchange}`);
      return { success: true, message: `${args.symbol} added to watchlist` };
    },
  },

  {
    name: 'stock_remove_from_watchlist',
    description: 'Remove a stock symbol from watchlist',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Stock symbol' },
        exchange: { type: 'string', description: 'Exchange', enum: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'] },
      },
      required: ['symbol', 'exchange'],
    },
    handler: async (args, contexts) => {
      if (!contexts.removeFromWatchlist) {
        return { success: false, error: 'Watchlist not available' };
      }
      await contexts.removeFromWatchlist(`${args.symbol}:${args.exchange}`);
      return { success: true, message: `${args.symbol} removed from watchlist` };
    },
  },

  // ============================================================================
  // BROKER & MODE MANAGEMENT
  // ============================================================================

  {
    name: 'stock_switch_broker',
    description: 'Switch active stock broker',
    inputSchema: {
      type: 'object',
      properties: {
        broker: {
          type: 'string',
          description: 'Broker ID',
          enum: ['angelone', 'zerodha', 'upstox', 'fyers', 'dhan', 'aliceblue', 'fivepaisa', 'groww', 'iifl', 'kotak', 'shoonya', 'motilal'],
        },
      },
      required: ['broker'],
    },
    handler: async (args, _contexts) => {
      // This would need integration with StockBrokerContext
      return { success: false, error: 'Broker switching via MCP not yet implemented. Use the UI broker selector.' };
    },
  },

  {
    name: 'stock_switch_trading_mode',
    description: 'Switch between live and paper trading mode',
    inputSchema: {
      type: 'object',
      properties: {
        mode: { type: 'string', description: 'Trading mode', enum: ['live', 'paper'] },
      },
      required: ['mode'],
    },
    handler: async (args, _contexts) => {
      // This would need integration with StockBrokerContext
      return { success: false, error: 'Trading mode switching via MCP not yet implemented. Use the UI mode toggle.' };
    },
  },

  // ============================================================================
  // PORTFOLIO METRICS
  // ============================================================================

  {
    name: 'stock_get_portfolio_summary',
    description: 'Get comprehensive portfolio summary with P&L',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getStockPositions || !contexts.getStockHoldings || !contexts.getStockFunds) {
        return { success: false, error: 'Portfolio data not available' };
      }

      const positions = await contexts.getStockPositions();
      const holdings = await contexts.getStockHoldings();
      const funds = await contexts.getStockFunds();

      const totalPositionPnl = positions.reduce((sum: number, p: any) => sum + (p.pnl || 0), 0);
      const totalHoldingPnl = holdings.reduce((sum: number, h: any) => sum + (h.pnl || 0), 0);
      const totalPnl = totalPositionPnl + totalHoldingPnl;

      const totalPositionValue = positions.reduce((sum: number, p: any) => sum + (p.value || 0), 0);
      const totalHoldingValue = holdings.reduce((sum: number, h: any) => sum + (h.value || 0), 0);
      const totalInvestmentValue = totalPositionValue + totalHoldingValue;

      return {
        success: true,
        data: {
          funds,
          positions: {
            count: positions.length,
            totalValue: totalPositionValue,
            totalPnl: totalPositionPnl,
          },
          holdings: {
            count: holdings.length,
            totalValue: totalHoldingValue,
            totalPnl: totalHoldingPnl,
          },
          overall: {
            totalInvestmentValue,
            totalPnl,
            pnlPercentage: totalInvestmentValue > 0 ? (totalPnl / totalInvestmentValue) * 100 : 0,
          },
        },
      };
    },
  },
];
