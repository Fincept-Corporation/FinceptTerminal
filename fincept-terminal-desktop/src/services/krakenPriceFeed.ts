/**
 * Kraken WebSocket Price Feed
 * Simple real-time price updates for NOF1 Alpha Arena style trading
 */

import ccxt from 'ccxt';

class KrakenPriceFeed {
  private exchange: any = null;
  private subscribers: Map<string, Set<(price: number) => void>> = new Map();
  private currentPrices: Map<string, number> = new Map();
  private isConnected = false;

  async connect() {
    if (this.isConnected) return;

    try {
      // Use CCXT Pro for WebSocket
      const kraken = new (ccxt.pro as any).kraken();
      this.exchange = kraken;
      this.isConnected = true;
      console.log('[KrakenPriceFeed] Connected to Kraken WebSocket');
    } catch (error) {
      console.error('[KrakenPriceFeed] Failed to connect:', error);
      throw error;
    }
  }

  async subscribe(symbol: string, callback: (price: number) => void) {
    if (!this.isConnected) {
      await this.connect();
    }

    // Add subscriber
    if (!this.subscribers.has(symbol)) {
      this.subscribers.set(symbol, new Set());
      this.startWatching(symbol);
    }
    this.subscribers.get(symbol)!.add(callback);

    // Return current price immediately if available
    const currentPrice = this.currentPrices.get(symbol);
    if (currentPrice) {
      callback(currentPrice);
    }
  }

  unsubscribe(symbol: string, callback: (price: number) => void) {
    const subs = this.subscribers.get(symbol);
    if (subs) {
      subs.delete(callback);
      if (subs.size === 0) {
        this.subscribers.delete(symbol);
      }
    }
  }

  private async startWatching(symbol: string) {
    if (!this.exchange) return;

    const watch = async () => {
      while (this.subscribers.has(symbol)) {
        try {
          const ticker = await this.exchange.watchTicker(symbol);
          const price = ticker.last;

          if (price) {
            this.currentPrices.set(symbol, price);

            // Notify all subscribers
            const subs = this.subscribers.get(symbol);
            if (subs) {
              subs.forEach(callback => callback(price));
            }
          }
        } catch (error) {
          console.error(`[KrakenPriceFeed] Error watching ${symbol}:`, error);
          await new Promise(resolve => setTimeout(resolve, 5000)); // Retry after 5s
        }
      }
    };

    watch();
  }

  getCurrentPrice(symbol: string): number | null {
    return this.currentPrices.get(symbol) || null;
  }

  async disconnect() {
    if (this.exchange) {
      await this.exchange.close();
      this.isConnected = false;
      this.subscribers.clear();
      console.log('[KrakenPriceFeed] Disconnected');
    }
  }
}

// Singleton instance
export const krakenPriceFeed = new KrakenPriceFeed();
