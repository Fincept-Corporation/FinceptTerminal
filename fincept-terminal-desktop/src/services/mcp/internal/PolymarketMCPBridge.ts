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
          tag_id: params.tag_id,
        });
      },

      polymarketSearch: async (query, type = 'markets') => {
        return polymarketApiService.search(query, type as any);
      },

      polymarketGetMarketDetail: async (marketId) => {
        // Try fetching by ID query param (Gamma API supports ?id=...)
        // Fall back to conditionId search if the first attempt returns nothing
        let market: any = null;

        const byId = await polymarketApiService.getMarkets({ limit: 1 } as any).catch(() => null);
        // Build the URL manually since getMarkets doesn't accept arbitrary params
        const directUrl = `https://gamma-api.polymarket.com/markets?id=${encodeURIComponent(marketId)}&limit=1`;
        try {
          const resp = await fetch(directUrl);
          if (resp.ok) {
            const data = await resp.json();
            market = Array.isArray(data) ? data[0] : data;
          }
        } catch { /* fall through */ }

        // Also try by conditionId if no result yet
        if (!market) {
          const condUrl = `https://gamma-api.polymarket.com/markets?condition_id=${encodeURIComponent(marketId)}&limit=1`;
          try {
            const resp = await fetch(condUrl);
            if (resp.ok) {
              const data = await resp.json();
              market = Array.isArray(data) ? data[0] : data;
            }
          } catch { /* fall through */ }
        }

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
          tag_id: params.tag_id,
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

      // ── Extended tools ──────────────────────────────────────────────────────

      polymarketGetEventBySlug: async (slug) => {
        return polymarketApiService.getEventBySlug(slug);
      },

      polymarketGetMarketBySlug: async (slug) => {
        return polymarketApiService.getMarketBySlug(slug);
      },

      polymarketGetTopHolders: async (conditionId, limit = 20) => {
        return polymarketApiService.getTopHolders(conditionId, limit);
      },

      polymarketGetOpenInterest: async (conditionIds) => {
        return polymarketApiService.getOpenInterest(conditionIds);
      },

      polymarketGetEnrichedOrderBook: async (tokenId) => {
        return polymarketApiService.getOrderBookEnriched(tokenId);
      },

      polymarketGetMidpoint: async (tokenId) => {
        return polymarketApiService.getMidpoint(tokenId);
      },

      polymarketGetTags: async () => {
        return polymarketApiService.getTags();
      },

      polymarketGetBalance: async () => {
        return polymarketApiService.getBalanceAllowance('COLLATERAL');
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
