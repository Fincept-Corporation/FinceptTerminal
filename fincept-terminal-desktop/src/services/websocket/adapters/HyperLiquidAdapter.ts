// File: src/services/websocket/adapters/HyperLiquidAdapter.ts
// HyperLiquid WebSocket API adapter
// Documentation: https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/websocket

import { BaseAdapter } from './BaseAdapter';
import { ProviderConfig, NormalizedMessage } from '../types';

/**
 * HyperLiquid WebSocket message types
 */
interface HyperLiquidMessage {
  channel?: string;
  data?: any;
}

interface HyperLiquidSubscription {
  method: 'subscribe' | 'unsubscribe';
  subscription: {
    type: string;
    coin?: string;
    user?: string;
    interval?: string;
  };
}

/**
 * HyperLiquidAdapter - WebSocket adapter for HyperLiquid DEX
 *
 * Supported channels:
 * - allMids: All mid prices for all coins
 * - l2Book: Level 2 order book for specific coin
 * - trades: Recent trades for specific coin
 * - candle: OHLC candlestick data (intervals: 1m, 5m, 15m, 1h, 4h, 1d)
 * - user: User-specific data (fills, orders - requires authentication)
 *
 * Topic format: "channel.symbol" e.g., "book.BTC", "trades.ETH"
 */
export class HyperLiquidAdapter extends BaseAdapter {
  readonly provider = 'hyperliquid';

  private readonly WS_URL = 'wss://api.hyperliquid.xyz/ws';

  /**
   * Get WebSocket URL
   */
  protected getWebSocketUrl(config: ProviderConfig): string {
    return this.WS_URL;
  }

  /**
   * Handle incoming messages from HyperLiquid
   */
  protected handleMessage(event: MessageEvent): void {
    try {
      const message: HyperLiquidMessage = JSON.parse(event.data);

      this.log('Received message:', message);

      // Handle channel messages
      if (message.channel && message.data) {
        this.handleChannelMessage(message);
      }
    } catch (error) {
      console.error('[HyperLiquidAdapter] Failed to parse message:', error);
    }
  }

  /**
   * Subscribe to a channel
   * @param topic - Format: "channel.symbol" e.g., "book.BTC", "trades.ETH"
   * @param params - Optional parameters (interval for candles)
   */
  protected async performSubscribe(topic: string, params?: Record<string, any>): Promise<void> {
    const { channel, symbol } = this.parseTopic(topic);

    const subscribeMessage: HyperLiquidSubscription = {
      method: 'subscribe',
      subscription: {
        type: this.mapChannelToType(channel)
      }
    };

    // Add coin for specific subscriptions
    if (symbol && channel !== 'allMids') {
      subscribeMessage.subscription.coin = symbol;
    }

    // Add interval for candles
    if (channel === 'ohlc' && params?.interval) {
      subscribeMessage.subscription.interval = this.normalizeInterval(params.interval);
    }

    await this.send(subscribeMessage);
    this.log(`Subscribed to ${channel}${symbol ? ` for ${symbol}` : ''}`);
  }

  /**
   * Unsubscribe from a channel
   */
  protected async performUnsubscribe(topic: string): Promise<void> {
    const { channel, symbol } = this.parseTopic(topic);

    const unsubscribeMessage: HyperLiquidSubscription = {
      method: 'unsubscribe',
      subscription: {
        type: this.mapChannelToType(channel)
      }
    };

    if (symbol && channel !== 'allMids') {
      unsubscribeMessage.subscription.coin = symbol;
    }

    await this.send(unsubscribeMessage);
    this.log(`Sent unsubscription request for ${topic}`);
  }

  /**
   * Perform ping (HyperLiquid doesn't require explicit ping)
   */
  protected async performPing(): Promise<void> {
    // HyperLiquid WebSocket maintains connection automatically
    // No explicit ping needed
    this.log('Ping not required for HyperLiquid');
  }

  /**
   * Called after successful connection
   */
  protected async onConnected(): Promise<void> {
    this.log('WebSocket connected successfully');
  }

  /**
   * Handle channel data messages
   */
  private handleChannelMessage(message: HyperLiquidMessage): void {
    if (!message.channel || !message.data) {
      return;
    }

    // Normalize message based on channel type
    const normalizedMessages = this.normalizeMessage(message);

    // Emit each normalized message
    normalizedMessages.forEach(msg => {
      this.notifyMessage(msg);
    });
  }

  /**
   * Normalize HyperLiquid message to standard format
   */
  private normalizeMessage(message: HyperLiquidMessage): NormalizedMessage[] {
    const normalized: NormalizedMessage[] = [];
    const channel = message.channel || 'unknown';
    const data = message.data;

    let type: NormalizedMessage['type'] = 'custom';
    let symbol: string | undefined;

    // Map channel to normalized type and extract data
    switch (channel) {
      case 'allMids':
        // All mid prices - emit as ticker for each coin
        type = 'ticker';
        if (data.mids && typeof data.mids === 'object') {
          Object.entries(data.mids).forEach(([coin, price]) => {
            normalized.push({
              provider: this.provider,
              channel: 'ticker',
              type: 'ticker',
              symbol: coin,
              timestamp: Date.now(),
              data: {
                last: parseFloat(price as string),
                mid: parseFloat(price as string)
              },
              raw: message
            });
          });
        }
        return normalized;

      case 'l2Book':
        // Order book
        type = 'orderbook';
        symbol = data.coin;
        const orderBookData = this.normalizeOrderBookData(data);
        normalized.push({
          provider: this.provider,
          channel: 'book',
          type,
          symbol,
          timestamp: data.time || Date.now(),
          data: orderBookData,
          raw: message
        });
        return normalized;

      case 'trades':
        // Trades
        type = 'trade';
        symbol = data.coin;
        if (Array.isArray(data.trades)) {
          data.trades.forEach((trade: any) => {
            normalized.push({
              provider: this.provider,
              channel: 'trades',
              type: 'trade',
              symbol,
              timestamp: trade.time || Date.now(),
              data: {
                price: parseFloat(trade.px),
                size: parseFloat(trade.sz),
                side: trade.side === 'A' ? 'sell' : 'buy', // A = ask (sell), B = bid (buy)
                time: trade.time
              },
              raw: message
            });
          });
        }
        return normalized;

      case 'candle':
        // OHLC candles
        type = 'candle';
        symbol = data.coin;
        if (data.candle) {
          const candle = data.candle;
          normalized.push({
            provider: this.provider,
            channel: 'ohlc',
            type: 'candle',
            symbol,
            timestamp: candle.t || Date.now(),
            data: {
              time: candle.t,
              open: parseFloat(candle.o),
              high: parseFloat(candle.h),
              low: parseFloat(candle.l),
              close: parseFloat(candle.c),
              volume: parseFloat(candle.v || 0)
            },
            raw: message
          });
        }
        return normalized;

      default:
        type = 'custom';
    }

    // Default push for unhandled channels
    normalized.push({
      provider: this.provider,
      channel,
      type,
      symbol,
      timestamp: Date.now(),
      data,
      raw: message
    });

    return normalized;
  }

  /**
   * Normalize HyperLiquid order book data to standardized format
   */
  private normalizeOrderBookData(data: any): any {
    const normalized: any = {};

    // HyperLiquid format: levels: [[bids], [asks]]
    // Each level: { px: price, sz: size, n: number of orders }
    if (data.levels && Array.isArray(data.levels) && data.levels.length === 2) {
      const [bids, asks] = data.levels;

      // Transform bids
      if (Array.isArray(bids)) {
        normalized.bids = bids.map((level: any) => ({
          price: parseFloat(level.px),
          size: parseFloat(level.sz),
          numOrders: level.n
        }));
      }

      // Transform asks
      if (Array.isArray(asks)) {
        normalized.asks = asks.map((level: any) => ({
          price: parseFloat(level.px),
          size: parseFloat(level.sz),
          numOrders: level.n
        }));
      }
    }

    // Add metadata
    if (data.coin) normalized.symbol = data.coin;
    if (data.time) normalized.time = data.time;

    return normalized;
  }

  /**
   * Map internal channel name to HyperLiquid subscription type
   */
  private mapChannelToType(channel: string): string {
    const channelMap: Record<string, string> = {
      'ticker': 'allMids',
      'book': 'l2Book',
      'trades': 'trades',
      'ohlc': 'candle',
      'candle': 'candle',
      'allMids': 'allMids',
      'l2Book': 'l2Book'
    };

    return channelMap[channel] || channel;
  }

  /**
   * Normalize interval to HyperLiquid format
   */
  private normalizeInterval(interval: string | number): string {
    // Convert number (minutes) to HyperLiquid format
    if (typeof interval === 'number') {
      const intervalMap: Record<number, string> = {
        1: '1m',
        5: '5m',
        15: '15m',
        60: '1h',
        240: '4h',
        1440: '1d'
      };
      return intervalMap[interval] || '1m';
    }

    // Already in correct format
    return interval;
  }

  /**
   * Parse topic into channel and symbol
   */
  private parseTopic(topic: string): { channel: string; symbol?: string } {
    const parts = topic.split('.');

    if (parts.length === 1) {
      // Just channel, no symbol
      return { channel: parts[0] };
    } else {
      // channel.symbol format
      return {
        channel: parts[0],
        symbol: parts.slice(1).join('.')
      };
    }
  }
}

/**
 * HyperLiquid WebSocket Usage Examples:
 *
 * All Mid Prices:
 *   subscribe('allMids')
 *   subscribe('ticker')  // Alias for allMids
 *
 * Order Book Level 2:
 *   subscribe('book.BTC')
 *   subscribe('book.ETH')
 *
 * Trades:
 *   subscribe('trades.BTC')
 *
 * OHLC Candles (intervals: 1m, 5m, 15m, 1h, 4h, 1d):
 *   subscribe('ohlc.BTC', { interval: '1m' })
 *   subscribe('candle.ETH', { interval: '1h' })
 *
 * Note: HyperLiquid uses coin symbols without pairs (e.g., 'BTC' not 'BTC/USD')
 */
