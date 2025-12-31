/**
 * Data Aggregator
 * Unified real-time data stream across all brokers
 */

import { BrokerType, UnifiedQuote, UnifiedMarketDepth, BrokerComparison } from '../core/types';
import { webSocketManager } from './WebSocketManager';
import { brokerOrchestrator } from '../core/BrokerOrchestrator';

interface AggregatedQuote {
  symbol: string;
  exchange: string;
  quotes: Map<BrokerType, UnifiedQuote>;
  lastUpdate: Date;
  bestBid: { broker: BrokerType; price: number } | null;
  bestAsk: { broker: BrokerType; price: number } | null;
}

export class DataAggregator {
  private static instance: DataAggregator;
  private aggregatedQuotes: Map<string, AggregatedQuote> = new Map();
  private subscribers: Map<string, Set<(data: AggregatedQuote) => void>> = new Map();

  private constructor() {}

  static getInstance(): DataAggregator {
    if (!DataAggregator.instance) {
      DataAggregator.instance = new DataAggregator();
    }
    return DataAggregator.instance;
  }

  /**
   * Subscribe to aggregated quotes for a symbol
   */
  subscribeAggregatedQuotes(
    symbol: string,
    exchange: string,
    brokers: BrokerType[],
    callback: (data: AggregatedQuote) => void
  ): string {
    const key = `${exchange}:${symbol}`;

    // Initialize aggregated quote
    if (!this.aggregatedQuotes.has(key)) {
      this.aggregatedQuotes.set(key, {
        symbol,
        exchange,
        quotes: new Map(),
        lastUpdate: new Date(),
        bestBid: null,
        bestAsk: null,
      });
    }

    // Subscribe to each broker
    brokers.forEach(brokerId => {
      webSocketManager.subscribeQuotes(
        brokerId,
        [{ symbol, exchange }],
        (quote) => {
          this.updateAggregatedQuote(key, brokerId, quote);
        }
      );
    });

    // Add subscriber callback
    if (!this.subscribers.has(key)) {
      this.subscribers.set(key, new Set());
    }
    this.subscribers.get(key)!.add(callback);

    console.log(`[DataAggregator] Subscribed to ${key} across ${brokers.length} brokers`);
    return key;
  }

  /**
   * Update aggregated quote with new data
   */
  private updateAggregatedQuote(key: string, brokerId: BrokerType, quote: UnifiedQuote): void {
    const aggregated = this.aggregatedQuotes.get(key);
    if (!aggregated) return;

    // Update quote
    aggregated.quotes.set(brokerId, quote);
    aggregated.lastUpdate = new Date();

    // Update best bid/ask
    this.updateBestPrices(aggregated);

    // Notify subscribers
    const subscribers = this.subscribers.get(key);
    if (subscribers) {
      subscribers.forEach(callback => {
        try {
          callback(aggregated);
        } catch (error) {
          console.error('[DataAggregator] Subscriber callback error:', error);
        }
      });
    }
  }

  /**
   * Update best bid/ask across all brokers
   */
  private updateBestPrices(aggregated: AggregatedQuote): void {
    let bestBid: { broker: BrokerType; price: number } | null = null;
    let bestAsk: { broker: BrokerType; price: number } | null = null;

    aggregated.quotes.forEach((quote, broker) => {
      // Best bid (highest)
      if (quote.bid) {
        if (!bestBid || quote.bid > bestBid.price) {
          bestBid = { broker, price: quote.bid };
        }
      }

      // Best ask (lowest)
      if (quote.ask) {
        if (!bestAsk || quote.ask < bestAsk.price) {
          bestAsk = { broker, price: quote.ask };
        }
      }
    });

    aggregated.bestBid = bestBid;
    aggregated.bestAsk = bestAsk;
  }

  /**
   * Get current aggregated quote
   */
  getAggregatedQuote(symbol: string, exchange: string): AggregatedQuote | null {
    const key = `${exchange}:${symbol}`;
    return this.aggregatedQuotes.get(key) || null;
  }

  /**
   * Compare quotes across brokers (snapshot)
   */
  async compareQuotesSnapshot(symbol: string, exchange: string): Promise<BrokerComparison> {
    return await brokerOrchestrator.compareQuotes(symbol, exchange);
  }

  /**
   * Compare market depth across brokers (snapshot)
   */
  async compareDepthSnapshot(symbol: string, exchange: string): Promise<BrokerComparison> {
    return await brokerOrchestrator.compareMarketDepth(symbol, exchange);
  }

  /**
   * Unsubscribe from aggregated quotes
   */
  unsubscribe(key: string): void {
    this.aggregatedQuotes.delete(key);
    this.subscribers.delete(key);
    console.log(`[DataAggregator] Unsubscribed from ${key}`);
  }

  /**
   * Get spread analysis
   */
  getSpreadAnalysis(symbol: string, exchange: string): {
    avgSpread: number;
    minSpread: number;
    maxSpread: number;
    bestBroker: BrokerType | null;
  } | null {
    const aggregated = this.getAggregatedQuote(symbol, exchange);
    if (!aggregated) return null;

    const spreads: Array<{ broker: BrokerType; spread: number }> = [];

    aggregated.quotes.forEach((quote, broker) => {
      if (quote.bid && quote.ask) {
        spreads.push({
          broker,
          spread: quote.ask - quote.bid,
        });
      }
    });

    if (spreads.length === 0) return null;

    const avgSpread = spreads.reduce((sum, s) => sum + s.spread, 0) / spreads.length;
    const minSpread = Math.min(...spreads.map(s => s.spread));
    const maxSpread = Math.max(...spreads.map(s => s.spread));
    const bestBroker = spreads.find(s => s.spread === minSpread)?.broker || null;

    return { avgSpread, minSpread, maxSpread, bestBroker };
  }

  /**
   * Get latency statistics
   */
  getLatencyStats(): Map<BrokerType, { avgLatency: number; lastUpdate: Date }> {
    const stats = new Map<BrokerType, { avgLatency: number; lastUpdate: Date }>();

    this.aggregatedQuotes.forEach(aggregated => {
      aggregated.quotes.forEach((quote, broker) => {
        if (!stats.has(broker)) {
          stats.set(broker, {
            avgLatency: 0,
            lastUpdate: aggregated.lastUpdate,
          });
        }
      });
    });

    return stats;
  }

  /**
   * Clear all subscriptions
   */
  clearAll(): void {
    this.aggregatedQuotes.clear();
    this.subscribers.clear();
    console.log('[DataAggregator] Cleared all subscriptions');
  }
}

export const dataAggregator = DataAggregator.getInstance();
