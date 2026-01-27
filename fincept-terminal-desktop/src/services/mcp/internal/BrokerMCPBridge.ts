// Broker MCP Bridge - Connects BrokerContext to MCP Internal Tools
// Provides unified trading interface for all brokers via MCP

import { terminalMCPProvider } from './TerminalMCPProvider';
import type { IExchangeAdapter } from '@/brokers/crypto/types';
import type { PaperTradingAdapter } from '@/paper-trading';

interface BrokerMCPBridgeConfig {
  activeAdapter: IExchangeAdapter | PaperTradingAdapter | null;
  tradingMode: 'live' | 'paper';
  activeBroker: string;
}

/**
 * Bridge that connects BrokerContext to MCP tools
 * Allows AI to control trading across all brokers (Kraken, Binance, HyperLiquid, etc.)
 */
export class BrokerMCPBridge {
  private config: BrokerMCPBridgeConfig | null = null;

  /**
   * Initialize or update the bridge with broker context
   */
  connect(config: BrokerMCPBridgeConfig): void {
    this.config = config;
    this.updateMCPContexts();
  }

  /**
   * Disconnect and clear contexts
   */
  disconnect(): void {
    this.config = null;
    terminalMCPProvider.setContexts({
      placeTrade: undefined,
      cancelOrder: undefined,
      modifyOrder: undefined,
      getPositions: undefined,
      getBalance: undefined,
      getOrders: undefined,
      getHoldings: undefined,
      closePosition: undefined,
    });
  }

  /**
   * Update MCP contexts with current adapter methods
   */
  private updateMCPContexts(): void {
    if (!this.config?.activeAdapter) {
      console.warn('[BrokerMCPBridge] No active adapter available');
      return;
    }

    const { activeAdapter, tradingMode, activeBroker } = this.config;

    terminalMCPProvider.setContexts({
      // Place trade (market, limit, stop, etc.)
      placeTrade: async (params: any) => {
        try {
          const order = await activeAdapter.createOrder(
            params.symbol,
            params.order_type || 'market',
            params.side,
            params.amount || params.quantity,
            params.price,
            {
              stopPrice: params.stop_price,
              ...params
            }
          );
          return {
            success: true,
            orderId: order.id,
            symbol: order.symbol,
            side: order.side,
            type: order.type,
            amount: order.amount,
            price: order.price,
            status: order.status,
            timestamp: order.timestamp,
          };
        } catch (error: any) {
          throw new Error(`Failed to place order: ${error.message}`);
        }
      },

      // Cancel order
      cancelOrder: async (orderId: string) => {
        try {
          // Try to extract symbol from order ID or fetch open orders to find it
          const openOrders = await activeAdapter.fetchOpenOrders();
          const order = openOrders.find(o => o.id === orderId);

          if (!order) {
            throw new Error(`Order ${orderId} not found`);
          }

          const cancelled = await activeAdapter.cancelOrder(orderId, order.symbol);
          return {
            success: true,
            orderId: cancelled.id,
            status: cancelled.status,
          };
        } catch (error: any) {
          throw new Error(`Failed to cancel order: ${error.message}`);
        }
      },

      // Modify order (cancel + replace)
      modifyOrder: async (orderId: string, updates: any) => {
        try {
          const openOrders = await activeAdapter.fetchOpenOrders();
          const order = openOrders.find(o => o.id === orderId);

          if (!order) {
            throw new Error(`Order ${orderId} not found`);
          }

          // Cancel existing order
          await activeAdapter.cancelOrder(orderId, order.symbol);

          // Place new order with updates
          const newOrder = await activeAdapter.createOrder(
            order.symbol,
            order.type as any,
            order.side as any,
            updates.quantity || order.amount,
            updates.price || order.price
          );

          return {
            success: true,
            oldOrderId: orderId,
            newOrderId: newOrder.id,
            newOrder,
          };
        } catch (error: any) {
          throw new Error(`Failed to modify order: ${error.message}`);
        }
      },

      // Get positions
      getPositions: async () => {
        try {
          const positions = await activeAdapter.fetchPositions();
          return positions.map(p => ({
            symbol: p.symbol,
            side: p.side,
            amount: p.contracts || p.contractSize || 0,
            entryPrice: p.entryPrice,
            markPrice: p.markPrice,
            unrealizedPnl: p.unrealizedPnl,
            leverage: p.leverage,
            marginType: p.marginMode,
            liquidationPrice: p.liquidationPrice,
          }));
        } catch (error: any) {
          console.error('[BrokerMCPBridge] Failed to fetch positions:', error);
          return [];
        }
      },

      // Get balance
      getBalance: async () => {
        try {
          const balance = await activeAdapter.fetchBalance();
          return balance;
        } catch (error: any) {
          throw new Error(`Failed to fetch balance: ${error.message}`);
        }
      },

      // Get orders
      getOrders: async (filters?: any) => {
        try {
          let orders: any[] = [];

          if (!filters?.status || filters.status === 'open' || filters.status === 'all') {
            const openOrders = await activeAdapter.fetchOpenOrders(filters?.symbol);
            orders.push(...openOrders);
          }

          // Try to fetch closed orders if adapter supports it
          if (!filters?.status || filters.status === 'closed' || filters.status === 'all') {
            try {
              if ('fetchClosedOrders' in activeAdapter && typeof activeAdapter.fetchClosedOrders === 'function') {
                const closedOrders = await activeAdapter.fetchClosedOrders(filters?.symbol, filters?.limit || 100);
                orders.push(...closedOrders);
              }
            } catch (err) {
              console.warn('[BrokerMCPBridge] Closed orders not supported by adapter');
            }
          }

          return orders.map(o => ({
            id: o.id,
            symbol: o.symbol,
            type: o.type,
            side: o.side,
            amount: o.amount,
            price: o.price,
            status: o.status,
            timestamp: o.timestamp,
            filled: o.filled,
            remaining: o.remaining,
            cost: o.cost,
            fee: o.fee,
          }));
        } catch (error: any) {
          throw new Error(`Failed to fetch orders: ${error.message}`);
        }
      },

      // Get holdings (balance formatted as holdings)
      getHoldings: async () => {
        try {
          const balance = await activeAdapter.fetchBalance();
          const holdings = Object.entries(balance.total || {})
            .filter(([_, amount]) => (amount as number) > 0)
            .map(([currency, amount]) => ({
              currency,
              amount: amount as number,
              free: balance.free?.[currency] || 0,
              used: balance.used?.[currency] || 0,
            }));
          return holdings;
        } catch (error: any) {
          throw new Error(`Failed to fetch holdings: ${error.message}`);
        }
      },

      // Close position
      closePosition: async (symbol: string) => {
        try {
          const positions = await activeAdapter.fetchPositions([symbol]);
          const position = positions.find(p => p.symbol === symbol);

          if (!position) {
            throw new Error(`No open position for ${symbol}`);
          }

          // Determine close side (opposite of position side)
          const closeSide = position.side === 'long' ? 'sell' : 'buy';
          const amount = Math.abs(position.contracts || position.contractSize || 0);

          const closeOrder = await activeAdapter.createOrder(
            symbol,
            'market',
            closeSide as any,
            amount
          );

          return {
            success: true,
            symbol,
            closedPosition: position,
            closeOrder,
          };
        } catch (error: any) {
          throw new Error(`Failed to close position: ${error.message}`);
        }
      },
    });

    console.log(`[BrokerMCPBridge] Connected to ${activeBroker} (${tradingMode} mode)`);
  }

  /**
   * Check if bridge is connected
   */
  isConnected(): boolean {
    return this.config !== null && this.config.activeAdapter !== null;
  }

  /**
   * Get current configuration
   */
  getConfig(): BrokerMCPBridgeConfig | null {
    return this.config;
  }
}

// Singleton instance
export const brokerMCPBridge = new BrokerMCPBridge();
