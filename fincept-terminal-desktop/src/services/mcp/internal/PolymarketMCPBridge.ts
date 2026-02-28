// Polymarket MCP Bridge - Connects polymarketServiceEnhanced to MCP Internal Tools
// Provides AI agents with read-only access to Polymarket prediction market data.

import { terminalMCPProvider } from './TerminalMCPProvider';
import { polymarketApiService } from '@/services/polymarket/polymarketApiService';

/**
 * Bridge that wires polymarketServiceEnhanced methods into the MCP context
 * so agents can call Polymarket tools via the terminal toolkit.
 */
export class PolymarketMCPBridge {
  private connected: boolean = false;

  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({
      polymarketGetMarkets: async (params) => {
        return polymarketApiService.getMarkets({
          limit: params.limit ?? 20,
          active: params.active ?? true,
          closed: params.closed ?? false,
          order: params.order,
          ascending: params.ascending ?? false,
        });
      },

      polymarketSearch: async (query, type = 'markets') => {
        return polymarketApiService.search(query, type as any);
      },

      polymarketGetMarketDetail: async (marketId) => {
        // Fetch the market and its order book (if CLOB token IDs are available)
        const market = await polymarketApiService.getMarkets({
          limit: 1,
          // Use market_ids filter when available; fallback to full list search
        }).then((markets) =>
          markets.find((m: any) => m.id === marketId || m.conditionId === marketId)
        );

        if (!market) {
          throw new Error(`Market ${marketId} not found`);
        }

        // Enrich with order book data if token IDs are available
        let orderBookData: any = null;
        const tokenId = market.clobTokenIds?.[0];
        if (tokenId) {
          try {
            orderBookData = await polymarketApiService.getOrderBook(tokenId);
          } catch {
            // Non-fatal — order book may not be available for all markets
          }
        }

        return { ...market, orderBook: orderBookData };
      },

      polymarketGetOrderBook: async (tokenId) => {
        return polymarketApiService.getOrderBook(tokenId);
      },

      polymarketGetPriceHistory: async (params) => {
        return polymarketApiService.getPriceHistory({
          token_id: params.token_id,
          interval: (params.interval ?? '1w') as any,
          fidelity: params.fidelity ?? 60,
        });
      },

      polymarketGetEvents: async (params) => {
        return polymarketApiService.getEvents({
          limit: params.limit ?? 10,
          active: params.active ?? true,
          closed: params.closed ?? false,
        });
      },

      polymarketGetOpenOrders: async () => {
        return polymarketApiService.getOpenOrders();
      },

      polymarketGetTrades: async (tokenId, limit = 20) => {
        return polymarketApiService.getTrades({
          token_id: tokenId,
          limit,
        });
      },
    });

    this.connected = true;
    console.log('[PolymarketMCPBridge] Connected — Polymarket tools available to agents');
  }

  disconnect(): void {
    this.connected = false;
  }

  get isConnected(): boolean {
    return this.connected;
  }
}

export const polymarketMCPBridge = new PolymarketMCPBridge();
