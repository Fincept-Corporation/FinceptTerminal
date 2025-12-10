// File: src/services/websocket/adapters/KrakenAdapter.ts
// Kraken WebSocket v2 API adapter
// Documentation: https://docs.kraken.com/api/docs/websocket-v2

import { BaseAdapter } from './BaseAdapter';
import { ProviderConfig, NormalizedMessage } from '../types';

/**
 * Kraken WebSocket v2 message types
 */
interface KrakenMessage {
  method?: string;
  result?: any;
  success?: boolean;
  error?: string;
  channel?: string;
  type?: 'snapshot' | 'update';
  data?: any[];
  timestamp?: string;
}

/**
 * KrakenAdapter - WebSocket v2 adapter for Kraken exchange
 *
 * Supported channels:
 * - ticker: Real-time ticker updates (bid, ask, last, volume, vwap, high, low, change)
 * - book: Level 2 order book (depth: 10, 25, 100, 500, 1000)
 * - trade: Recent trades
 * - ohlc: OHLC/candlestick data (intervals: 1, 5, 15, 30, 60, 240, 1440, 10080, 21600 minutes)
 * - instrument: Reference data for assets and trading pairs
 * - level3: Level 3 order book (requires authentication)
 *
 * Topic format: "channel.symbol" e.g., "ticker.BTC/USD"
 */
export class KrakenAdapter extends BaseAdapter {
  readonly provider = 'kraken';

  private readonly WS_URL = 'wss://ws.kraken.com/v2';
  private readonly WS_AUTH_URL = 'wss://ws-auth.kraken.com/v2';

  private reqIdCounter: number = 1;

  /**
   * Get WebSocket URL based on config
   */
  protected getWebSocketUrl(config: ProviderConfig): string {
    // Use authenticated endpoint for level3 or if API key provided
    return config.api_key ? this.WS_AUTH_URL : this.WS_URL;
  }

  /**
   * Handle incoming messages from Kraken
   */
  protected handleMessage(event: MessageEvent): void {
    try {
      const message: KrakenMessage = JSON.parse(event.data);

      this.log('Received message:', message);

      // Handle different message types
      if (message.method === 'pong') {
        this.handlePong();
      } else if (message.method === 'subscribe' || message.method === 'unsubscribe') {
        this.handleSubscriptionResponse(message);
      } else if (message.channel) {
        this.handleChannelMessage(message);
      } else if (message.error) {
        this.handleErrorMessage(message);
      }
    } catch (error) {
      console.error('[KrakenAdapter] Failed to parse message:', error);
    }
  }

  /**
   * Subscribe to a channel
   * @param topic - Format: "channel.symbol" e.g., "ticker.BTC/USD"
   * @param params - Optional parameters (depth, interval, event_trigger, snapshot, token)
   */
  protected async performSubscribe(topic: string, params?: Record<string, any>): Promise<void> {
    const { channel, symbol } = this.parseTopic(topic);

    const subscribeMessage: any = {
      method: 'subscribe',
      params: {
        channel,
        snapshot: params?.snapshot ?? true
      },
      req_id: this.getNextReqId()
    };

    // Add symbol array if provided
    if (symbol) {
      // Normalize symbol for Kraken (BTC/USD -> XBT/USD, ensure uppercase)
      const normalizedSymbol = this.normalizeSymbolForKraken(symbol);
      subscribeMessage.params.symbol = [normalizedSymbol];
    }

    // Channel-specific parameters
    if (channel === 'book' || channel === 'level3') {
      // Depth: 10, 25, 100, 500, 1000 (default: 10)
      // Validate and round to nearest valid depth
      let depth = params?.depth ?? 10;
      const validDepths = [10, 25, 100, 500, 1000];

      // Find the closest valid depth
      if (!validDepths.includes(depth)) {
        depth = validDepths.reduce((prev, curr) =>
          Math.abs(curr - depth) < Math.abs(prev - depth) ? curr : prev
        );
      }

      subscribeMessage.params.depth = depth;
    }

    if (channel === 'ohlc') {
      // Interval in minutes: 1, 5, 15, 30, 60, 240, 1440, 10080, 21600
      subscribeMessage.params.interval = params?.interval ?? 1;
    }

    if (channel === 'ticker' && params?.event_trigger) {
      // Event trigger: 'bbo' or 'trades'
      subscribeMessage.params.event_trigger = params.event_trigger;
    }

    if (channel === 'level3' && this.config?.api_key) {
      // Authentication token for level3
      subscribeMessage.params.token = this.config.api_key;
    }

    if (channel === 'instrument' && params?.include_tokenized_assets !== undefined) {
      subscribeMessage.params.include_tokenized_assets = params.include_tokenized_assets;
    }

    await this.send(subscribeMessage);
    this.log(`Subscribed to ${channel}${symbol ? ` for ${symbol}` : ''}`);
  }

  /**
   * Unsubscribe from a channel
   */
  protected async performUnsubscribe(topic: string): Promise<void> {
    const { channel, symbol } = this.parseTopic(topic);

    const unsubscribeMessage: any = {
      method: 'unsubscribe',
      params: {
        channel
      },
      req_id: this.getNextReqId()
    };

    if (symbol) {
      unsubscribeMessage.params.symbol = [symbol];
    }

    await this.send(unsubscribeMessage);
    this.log(`Sent unsubscription request for ${topic}`);
  }

  /**
   * Perform ping
   */
  protected async performPing(): Promise<void> {
    const pingMessage = {
      method: 'ping',
      req_id: this.getNextReqId()
    };

    await this.send(pingMessage);
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
  private handleChannelMessage(message: KrakenMessage): void {
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
   * Normalize Kraken v2 message to standard format
   */
  private normalizeMessage(message: KrakenMessage): NormalizedMessage[] {
    const normalized: NormalizedMessage[] = [];

    if (!message.data || message.data.length === 0) {
      return normalized;
    }

    const channel = message.channel || 'unknown';
    const messageType = message.type || 'update';

    message.data.forEach((dataItem: any) => {
      let type: NormalizedMessage['type'] = 'custom';
      const symbol = dataItem.symbol;

      // Map channel to normalized type
      switch (channel) {
        case 'ticker':
          type = 'ticker';
          break;
        case 'trade':
          type = 'trade';
          break;
        case 'ohlc':
          type = 'candle';
          break;
        case 'book':
        case 'level3':
          type = 'orderbook';
          // Transform Kraken order book format to standardized format
          const normalizedData = this.normalizeOrderBookData(dataItem);
          normalized.push({
            provider: this.provider,
            channel,
            type,
            symbol,
            timestamp: message.timestamp ? new Date(message.timestamp).getTime() : Date.now(),
            data: normalizedData,
            raw: message
          });
          return; // Skip the default push below
        case 'instrument':
          type = 'status';
          break;
        default:
          type = 'custom';
      }

      normalized.push({
        provider: this.provider,
        channel,
        type,
        symbol,
        timestamp: message.timestamp ? new Date(message.timestamp).getTime() : Date.now(),
        data: {
          ...dataItem,
          messageType // 'snapshot' or 'update'
        },
        raw: message
      });
    });

    return normalized;
  }

  /**
   * Normalize Kraken order book data to standardized format
   */
  private normalizeOrderBookData(data: any): any {
    const normalized: any = {
      messageType: data.type || 'update'
    };

    // Transform bids array to standardized format
    if (data.bids && Array.isArray(data.bids)) {
      normalized.bids = data.bids.map((bid: any) => ({
        price: parseFloat(bid.price || bid[0] || 0),
        size: parseFloat(bid.qty || bid[1] || 0)
      }));
    }

    // Transform asks array to standardized format
    if (data.asks && Array.isArray(data.asks)) {
      normalized.asks = data.asks.map((ask: any) => ({
        price: parseFloat(ask.price || ask[0] || 0),
        size: parseFloat(ask.qty || ask[1] || 0)
      }));
    }

    // Copy over other fields
    if (data.symbol) normalized.symbol = data.symbol;
    if (data.checksum) normalized.checksum = data.checksum;

    return normalized;
  }

  /**
   * Handle subscription response
   */
  private handleSubscriptionResponse(message: KrakenMessage): void {
    if (message.success) {
      this.log('Subscription successful:', message);
    } else if (message.error) {
      // Log error but don't add to error queue for common subscription issues
      const errorStr = String(message.error);

      // Categorize errors
      const isCommonError =
        errorStr.includes('not Found') ||
        errorStr.includes('invalid') ||
        errorStr.includes('not supported');

      // Only log verbose errors in development
      if (isCommonError) {
        console.debug('[KrakenAdapter] Subscription info:', message.error);
      } else {
        console.error('[KrakenAdapter] Subscription error:', message.error);
        this.addError({
          timestamp: Date.now(),
          error: message.error
        });
      }
    }
  }

  /**
   * Handle error messages
   */
  private handleErrorMessage(message: KrakenMessage): void {
    const error = {
      timestamp: Date.now(),
      error: message.error || 'Unknown error'
    };

    this.addError(error);
    this.notifyError(error);
    this.log('Error message:', message.error);
  }

  /**
   * Handle pong response
   */
  private handlePong(): void {
    this.log('Received pong');
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
   * Get next request ID
   */
  private getNextReqId(): number {
    return this.reqIdCounter++;
  }

  /**
   * Normalize symbol for Kraken WebSocket API
   * Kraken WebSocket v2 uses different formats than v1
   * Examples: BTC/USD, ETH/USD (not XBT/USD)
   */
  private normalizeSymbolForKraken(symbol: string): string {
    // Ensure uppercase
    let normalized = symbol.toUpperCase();

    // Kraken WebSocket v2 actually uses BTC/USD format, not XBT/USD
    // The v1 API used XBT, but v2 standardized on BTC
    // No conversion needed for v2 API

    return normalized;
  }
}

/**
 * Kraken WebSocket v2 Usage Examples:
 *
 * Ticker (bid, ask, last, volume, vwap, high, low, change):
 *   subscribe('ticker.BTC/USD')
 *   subscribe('ticker.ETH/USD', { event_trigger: 'bbo' })
 *
 * Order Book Level 2 (depth: 10, 25, 100, 500, 1000):
 *   subscribe('book.BTC/USD', { depth: 10 })
 *   subscribe('book.ETH/USD', { depth: 100, snapshot: true })
 *
 * Trades:
 *   subscribe('trade.BTC/USD')
 *
 * OHLC Candles (intervals: 1, 5, 15, 30, 60, 240, 1440, 10080, 21600 minutes):
 *   subscribe('ohlc.BTC/USD', { interval: 1 })
 *   subscribe('ohlc.ETH/USD', { interval: 60 })
 *
 * Instrument Reference Data:
 *   subscribe('instrument', { include_tokenized_assets: false })
 *
 * Level 3 Order Book (requires authentication):
 *   subscribe('level3.BTC/USD', { depth: 10 })
 */
