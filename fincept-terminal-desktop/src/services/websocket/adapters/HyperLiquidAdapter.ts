// File: src/services/websocket/adapters/HyperLiquidAdapter.ts
// HyperLiquid WebSocket API adapter
// Documentation: https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/websocket

import { BaseAdapter } from './BaseAdapter';
import { ProviderConfig, NormalizedMessage } from '../types';
import type {
  HyperLiquidMessage,
  HyperLiquidSubscriptionRequest,
  HyperLiquidSubscriptionType,
  HyperLiquidAllMidsData,
  HyperLiquidL2BookData,
  HyperLiquidTradesData,
  HyperLiquidCandleData,
  HyperLiquidBBOData,
  HyperLiquidUserFillsData,
  HyperLiquidOpenOrdersData,
  HyperLiquidUserFundingsData,
  HyperLiquidCandleInterval
} from '../../../types/hyperliquid';

/**
 * HyperLiquidAdapter - WebSocket adapter for HyperLiquid exchange
 *
 * Supported channels:
 * - allMids: Mid-prices for all coins
 * - l2Book: Level 2 order book (nSigFigs: 2-5, depth configurable)
 * - trades: Recent trades
 * - candle: OHLC candlestick data (intervals: 1m, 3m, 5m, 15m, 30m, 1h, 2h, 4h, 8h, 12h, 1d, 3d, 1w, 1M)
 * - bbo: Best bid/offer updates
 * - userFills: User-specific fill history (requires user address)
 * - openOrders: User open orders (requires user address)
 * - userFundings: User funding payments (requires user address)
 * - clearinghouseState: User position state (requires user address)
 *
 * Topic format: "channel.symbol" e.g., "l2Book.BTC" or "trades.ETH"
 * User-specific: "userFills" (no symbol, requires config.api_key as user address)
 */
export class HyperLiquidAdapter extends BaseAdapter {
  readonly provider = 'hyperliquid';

  private readonly WS_URL = 'wss://api.hyperliquid.xyz/ws';
  private readonly WS_TESTNET_URL = 'wss://api.hyperliquid-testnet.xyz/ws';

  // Track pending subscriptions for response matching
  private pendingSubscriptions = new Map<string, { channel: string; symbol?: string }>();

  /**
   * Get WebSocket URL based on config
   */
  protected getWebSocketUrl(config: ProviderConfig): string {
    // Check if testnet mode is enabled in config_data
    const useTestnet = config.config_data
      ? JSON.parse(config.config_data).testnet === true
      : false;

    return useTestnet ? this.WS_TESTNET_URL : this.WS_URL;
  }

  /**
   * Handle incoming messages from HyperLiquid
   */
  protected handleMessage(event: MessageEvent): void {
    try {
      const message: HyperLiquidMessage = JSON.parse(event.data);

      this.log('Received message:', message);

      // Handle subscription responses
      if (message.channel === 'subscriptionResponse') {
        this.handleSubscriptionResponse(message);
        return;
      }

      // Handle channel data messages
      if (message.channel && message.data) {
        this.handleChannelMessage(message);
      }
    } catch (error) {
      console.error('[HyperLiquidAdapter] Failed to parse message:', error);
    }
  }

  /**
   * Channel aliasing map: Generic channel names → HyperLiquid-specific channels
   * This allows components to use generic names like "ticker" and "book"
   */
  private readonly CHANNEL_ALIAS_MAP: Record<string, string> = {
    'ticker': 'bbo',      // ticker → bbo (best bid/offer - per symbol ticker data)
    'book': 'l2Book',     // book → l2Book (level 2 order book)
    'trade': 'trades',    // trade → trades (recent trades)
    'ohlc': 'candle',     // ohlc → candle (candlestick data)
  };

  /**
   * Reverse alias map for normalization: HyperLiquid channels → Generic names
   */
  private readonly REVERSE_ALIAS_MAP: Record<string, string> = {
    'bbo': 'ticker',
    'l2Book': 'book',
    'trades': 'trade',
    'candle': 'ohlc',
  };

  /**
   * Subscribe to a channel
   * @param topic - Format: "channel.symbol" e.g., "ticker.BTC" or "book.ETH"
   * @param params - Optional parameters (nSigFigs, mantissa, interval, aggregateByTime)
   */
  protected async performSubscribe(topic: string, params?: Record<string, any>): Promise<void> {
    const { channel, symbol } = this.parseTopic(topic);

    // Map generic channel name to HyperLiquid-specific channel
    const hlChannel = this.CHANNEL_ALIAS_MAP[channel] || channel;

    this.log(`Subscribing to topic: ${topic} → HyperLiquid channel: ${hlChannel}${symbol ? ` (${symbol})` : ''}`);

    // Build subscription request
    const subscription: any = {
      type: hlChannel as HyperLiquidSubscriptionType
    };

    // Add symbol if provided (coin parameter in HyperLiquid)
    if (symbol) {
      subscription.coin = symbol;
    }

    // Add user address for user-specific channels
    if (this.isUserSpecificChannel(channel)) {
      if (!this.config?.api_key) {
        throw new Error(`Channel ${channel} requires user address (set api_key in config)`);
      }
      subscription.user = this.config.api_key;
    }

    // Channel-specific parameters (use HyperLiquid channel name)
    if (hlChannel === 'l2Book') {
      subscription.nSigFigs = params?.nSigFigs ?? 5;
      if (params?.mantissa) {
        subscription.mantissa = params.mantissa;
      }
    }

    if (hlChannel === 'candle') {
      subscription.interval = params?.interval ?? '1m';
    }

    if (hlChannel === 'userFills' && params?.aggregateByTime !== undefined) {
      subscription.aggregateByTime = params.aggregateByTime;
    }

    const subscribeMessage: HyperLiquidSubscriptionRequest = {
      method: 'subscribe',
      subscription
    };

    // Track pending subscription (store original generic channel for proper routing)
    const subscriptionKey = this.buildSubscriptionKey(subscribeMessage.subscription);
    this.pendingSubscriptions.set(subscriptionKey, { channel, symbol });

    await this.send(subscribeMessage);
    this.log(`✅ Subscription sent: ${topic} → ${hlChannel}${symbol ? ` (${symbol})` : ''}`);
  }

  /**
   * Unsubscribe from a channel
   */
  protected async performUnsubscribe(topic: string): Promise<void> {
    const { channel, symbol } = this.parseTopic(topic);

    // Map generic channel name to HyperLiquid-specific channel
    const hlChannel = this.CHANNEL_ALIAS_MAP[channel] || channel;

    this.log(`Unsubscribing from topic: ${topic} → HyperLiquid channel: ${hlChannel}${symbol ? ` (${symbol})` : ''}`);

    // Build unsubscribe request (must match original subscription)
    const subscription: any = {
      type: hlChannel as HyperLiquidSubscriptionType
    };

    if (symbol) {
      subscription.coin = symbol;
    }

    if (this.isUserSpecificChannel(channel)) {
      if (!this.config?.api_key) {
        this.log(`Cannot unsubscribe from ${channel}: no user address`);
        return;
      }
      subscription.user = this.config.api_key;
    }

    const unsubscribeMessage: HyperLiquidSubscriptionRequest = {
      method: 'unsubscribe',
      subscription
    };

    await this.send(unsubscribeMessage);
    this.log(`Sent unsubscription request for ${topic}`);
  }

  /**
   * Perform ping (HyperLiquid uses message activity for heartbeat)
   */
  protected async performPing(): Promise<void> {
    // HyperLiquid doesn't have explicit ping/pong
    // Heartbeat is maintained by message activity
    // We can send a subscription status check or just log
    this.log('Heartbeat check (no explicit ping for HyperLiquid)');
  }

  /**
   * Called after successful connection
   */
  protected async onConnected(): Promise<void> {
    this.log('WebSocket connected successfully to HyperLiquid');
  }

  /**
   * Handle subscription response
   */
  private handleSubscriptionResponse(message: any): void {
    this.log('Subscription response:', message);

    if (message.data) {
      const { method, subscription } = message.data;

      if (method === 'subscribe') {
        this.log('Subscription confirmed:', subscription);
      } else if (method === 'unsubscribe') {
        this.log('Unsubscription confirmed:', subscription);
      }
    }
  }

  /**
   * Handle channel data messages
   */
  private handleChannelMessage(message: HyperLiquidMessage): void {
    const channel = message.channel;

    // Normalize based on channel type
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
    const channel = message.channel;

    if (!message.data) {
      return normalized;
    }

    try {
      switch (channel) {
        case 'allMids':
          normalized.push(...this.normalizeAllMids(message.data as HyperLiquidAllMidsData));
          break;

        case 'l2Book':
          normalized.push(this.normalizeL2Book(message.data as HyperLiquidL2BookData));
          break;

        case 'trades':
          normalized.push(this.normalizeTrades(message.data as HyperLiquidTradesData));
          break;

        case 'candle':
          normalized.push(this.normalizeCandle(message.data as HyperLiquidCandleData));
          break;

        case 'bbo':
          normalized.push(this.normalizeBBO(message.data as HyperLiquidBBOData));
          break;

        case 'userFills':
          normalized.push(this.normalizeUserFills(message.data as HyperLiquidUserFillsData));
          break;

        case 'openOrders':
          normalized.push(this.normalizeOpenOrders(message.data as HyperLiquidOpenOrdersData));
          break;

        case 'userFundings':
          normalized.push(this.normalizeUserFundings(message.data as HyperLiquidUserFundingsData));
          break;

        default:
          // Generic normalization for other channels
          normalized.push({
            provider: this.provider,
            channel,
            type: 'custom',
            timestamp: Date.now(),
            data: message.data,
            raw: message
          });
      }
    } catch (error) {
      console.error(`[HyperLiquidAdapter] Error normalizing ${channel} message:`, error);
    }

    return normalized;
  }

  /**
   * Normalize allMids (ticker data for all coins)
   */
  private normalizeAllMids(data: HyperLiquidAllMidsData): NormalizedMessage[] {
    const messages: NormalizedMessage[] = [];

    if (data.mids) {
      Object.entries(data.mids).forEach(([coin, midPrice]) => {
        messages.push({
          provider: this.provider,
          channel: 'allMids',
          type: 'ticker',
          symbol: coin,
          timestamp: Date.now(),
          data: {
            mid: parseFloat(midPrice),
            coin
          },
          raw: data
        });
      });
    }

    return messages;
  }

  /**
   * Normalize L2 order book
   */
  private normalizeL2Book(data: HyperLiquidL2BookData): NormalizedMessage {
    const [bids, asks] = data.levels;

    return {
      provider: this.provider,
      channel: this.REVERSE_ALIAS_MAP['l2Book'] || 'l2Book', // Use generic 'book' channel name
      type: 'orderbook',
      symbol: data.coin,
      timestamp: data.time,
      data: {
        coin: data.coin,
        bids: bids.map(level => ({
          price: parseFloat(level.px),
          size: parseFloat(level.sz),
          count: level.n
        })),
        asks: asks.map(level => ({
          price: parseFloat(level.px),
          size: parseFloat(level.sz),
          count: level.n
        })),
        time: data.time
      },
      raw: data
    };
  }

  /**
   * Normalize trades
   */
  private normalizeTrades(data: HyperLiquidTradesData): NormalizedMessage {
    return {
      provider: this.provider,
      channel: this.REVERSE_ALIAS_MAP['trades'] || 'trades', // Use generic 'trade' channel name
      type: 'trade',
      symbol: data.coin,
      timestamp: Date.now(),
      data: {
        coin: data.coin,
        trades: data.trades.map(trade => ({
          price: parseFloat(trade.px),
          size: parseFloat(trade.sz),
          side: trade.side === 'A' ? 'sell' : 'buy', // A = Ask (sell), B = Bid (buy)
          time: trade.time,
          tradeId: trade.tid,
          hash: trade.hash
        }))
      },
      raw: data
    };
  }

  /**
   * Normalize candle data
   */
  private normalizeCandle(data: HyperLiquidCandleData): NormalizedMessage {
    const candle = data.candle;

    if (!candle) {
      throw new Error('No candle data in message');
    }

    return {
      provider: this.provider,
      channel: this.REVERSE_ALIAS_MAP['candle'] || 'candle', // Use generic 'ohlc' channel name
      type: 'candle',
      symbol: candle.s,
      timestamp: candle.t,
      data: {
        symbol: candle.s,
        interval: candle.i,
        openTime: candle.t,
        closeTime: candle.T,
        open: parseFloat(candle.o),
        high: parseFloat(candle.h),
        low: parseFloat(candle.l),
        close: parseFloat(candle.c),
        volume: parseFloat(candle.v),
        trades: candle.n
      },
      raw: data
    };
  }

  /**
   * Normalize BBO (best bid/offer) - This is HyperLiquid's ticker data
   */
  private normalizeBBO(data: HyperLiquidBBOData): NormalizedMessage {
    const bbo = data.bbo;

    if (!bbo) {
      throw new Error('No BBO data in message');
    }

    const bid = parseFloat(bbo.bid);
    const ask = parseFloat(bbo.ask);
    const mid = (bid + ask) / 2;

    return {
      provider: this.provider,
      channel: this.REVERSE_ALIAS_MAP['bbo'] || 'bbo', // Use generic 'ticker' channel name
      type: 'ticker',
      symbol: bbo.coin,
      timestamp: bbo.time,
      data: {
        coin: bbo.coin,
        symbol: bbo.coin,
        bid: bid,
        ask: ask,
        mid: mid,
        last: mid, // Use mid as last price approximation
        spread: ask - bid,
        time: bbo.time
      },
      raw: data
    };
  }

  /**
   * Normalize user fills
   */
  private normalizeUserFills(data: HyperLiquidUserFillsData): NormalizedMessage {
    return {
      provider: this.provider,
      channel: 'userFills',
      type: 'trade',
      timestamp: Date.now(),
      data: {
        user: data.user,
        isSnapshot: data.isSnapshot,
        fills: data.fills.map(fill => ({
          coin: fill.coin,
          price: parseFloat(fill.px),
          size: parseFloat(fill.sz),
          side: fill.side === 'A' ? 'sell' : 'buy',
          time: fill.time,
          closedPnl: parseFloat(fill.closedPnl),
          fee: parseFloat(fill.fee),
          orderId: fill.oid,
          tradeId: fill.tid,
          hash: fill.hash
        }))
      },
      raw: data
    };
  }

  /**
   * Normalize open orders
   */
  private normalizeOpenOrders(data: HyperLiquidOpenOrdersData): NormalizedMessage {
    return {
      provider: this.provider,
      channel: 'openOrders',
      type: 'status',
      timestamp: Date.now(),
      data: {
        user: data.user,
        orders: data.orders?.map(order => ({
          coin: order.coin,
          side: order.side === 'A' ? 'sell' : 'buy',
          price: parseFloat(order.limitPx),
          size: parseFloat(order.sz),
          origSize: parseFloat(order.origSz),
          orderId: order.oid,
          clientOrderId: order.cloid,
          timestamp: order.timestamp
        })) || []
      },
      raw: data
    };
  }

  /**
   * Normalize user fundings
   */
  private normalizeUserFundings(data: HyperLiquidUserFundingsData): NormalizedMessage {
    return {
      provider: this.provider,
      channel: 'userFundings',
      type: 'custom',
      timestamp: Date.now(),
      data: {
        user: data.user,
        isSnapshot: data.isSnapshot,
        fundings: data.fundings.map(funding => ({
          time: funding.time,
          coin: funding.coin,
          usdc: parseFloat(funding.usdc),
          szi: parseFloat(funding.szi),
          fundingRate: parseFloat(funding.fundingRate)
        }))
      },
      raw: data
    };
  }

  /**
   * Check if channel requires user address
   */
  private isUserSpecificChannel(channel: string): boolean {
    const userChannels = [
      'userFills',
      'openOrders',
      'userFundings',
      'clearinghouseState',
      'userEvents',
      'orderUpdates',
      'userNonFundingLedgerUpdates',
      'userTwapSliceFills',
      'userTwapHistory',
      'activeAssetData',
      'twapStates',
      'notification',
      'webData2',
      'webData3'
    ];

    return userChannels.includes(channel);
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

  /**
   * Build subscription key for tracking
   */
  private buildSubscriptionKey(subscription: any): string {
    const parts = [subscription.type];
    if (subscription.coin) parts.push(subscription.coin);
    if (subscription.user) parts.push(subscription.user);
    if (subscription.interval) parts.push(subscription.interval);
    return parts.join(':');
  }
}

/**
 * HyperLiquid WebSocket Usage Examples:
 *
 * All Mids (mid-prices for all coins):
 *   subscribe('allMids')
 *
 * Level 2 Order Book (nSigFigs: 2-5):
 *   subscribe('l2Book.BTC', { nSigFigs: 5 })
 *   subscribe('l2Book.ETH', { nSigFigs: 5, mantissa: 1 })
 *
 * Trades:
 *   subscribe('trades.BTC')
 *
 * Candles (intervals: 1m, 3m, 5m, 15m, 30m, 1h, 2h, 4h, 8h, 12h, 1d, 3d, 1w, 1M):
 *   subscribe('candle.BTC', { interval: '1m' })
 *   subscribe('candle.ETH', { interval: '1h' })
 *
 * Best Bid/Offer:
 *   subscribe('bbo.BTC')
 *
 * User Fills (requires api_key set to wallet address):
 *   subscribe('userFills', { aggregateByTime: true })
 *
 * Open Orders (requires api_key set to wallet address):
 *   subscribe('openOrders')
 */
