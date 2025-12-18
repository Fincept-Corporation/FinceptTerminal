/**
 * WebSocket Message Types
 * 
 * Centralized type definitions for WebSocket messages across all providers.
 * These types normalize different provider formats into a common structure.
 */

// =============================================================================
// Core Market Data Types
// =============================================================================

/**
 * Standard ticker/quote data
 */
export interface TickerData {
    symbol: string;
    price: number;
    open?: number;
    high?: number;
    low?: number;
    close?: number;
    volume?: number;
    change?: number;
    changePercent?: number;
    bid?: number;
    ask?: number;
    bidSize?: number;
    askSize?: number;
    timestamp: number;
    provider: string;
}

/**
 * Trade execution data
 */
export interface TradeData {
    id: string;
    symbol: string;
    price: number;
    quantity: number;
    side: 'buy' | 'sell' | 'unknown';
    timestamp: number;
    provider: string;
    tradeId?: string;
    orderId?: string;
}

/**
 * Order book level
 */
export interface OrderBookLevel {
    price: number;
    quantity: number;
    count?: number;
}

/**
 * Order book snapshot/update
 */
export interface OrderBookData {
    symbol: string;
    bids: OrderBookLevel[];
    asks: OrderBookLevel[];
    timestamp: number;
    provider: string;
    isSnapshot?: boolean;
}

/**
 * OHLCV candle data
 */
export interface CandleData {
    symbol: string;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
    timestamp: number;
    interval: string;
    provider: string;
}

// =============================================================================
// Provider-Specific Message Types
// =============================================================================

/**
 * Kraken WebSocket message types
 */
export interface KrakenTickerMessage {
    channelID: number;
    channelName: string;
    pair: string;
    data: {
        a: [string, string, string]; // ask [price, wholeLotVolume, lotVolume]
        b: [string, string, string]; // bid
        c: [string, string];         // close [price, lotVolume]
        v: [string, string];         // volume [today, 24h]
        p: [string, string];         // vwap [today, 24h]
        t: [number, number];         // trades [today, 24h]
        l: [string, string];         // low [today, 24h]
        h: [string, string];         // high [today, 24h]
        o: [string, string];         // open [today, 24h]
    };
}

export interface KrakenTradeMessage {
    channelID: number;
    channelName: string;
    pair: string;
    data: Array<[string, string, string, string, string, string]>;
    // [price, volume, time, side, orderType, misc]
}

export interface KrakenOrderBookMessage {
    channelID: number;
    channelName: string;
    pair: string;
    data: {
        as?: Array<[string, string, string]>; // asks [price, volume, timestamp]
        bs?: Array<[string, string, string]>; // bids
        a?: Array<[string, string, string]>;  // ask updates
        b?: Array<[string, string, string]>;  // bid updates
    };
}

/**
 * HyperLiquid WebSocket message types
 */
export interface HyperLiquidTickerMessage {
    channel: string;
    data: {
        coin: string;
        markPx: string;
        midPx: string;
        premium: string;
        fundingRate: string;
        openInterest: string;
        dayNtlVlm: string;
        prevDayPx: string;
    };
}

export interface HyperLiquidTradeMessage {
    channel: string;
    data: {
        coin: string;
        side: string;
        px: string;
        sz: string;
        time: number;
        hash: string;
    };
}

export interface HyperLiquidL2Message {
    channel: string;
    data: {
        coin: string;
        levels: Array<{
            px: string;
            sz: string;
            n: number;
        }>[];
        time: number;
    };
}

// =============================================================================
// Fyers WebSocket Types (re-export for convenience)
// =============================================================================

export interface FyersOrderUpdate {
    clientId: string;
    id: string;
    exchOrdId: string;
    qty: number;
    filledQty: number;
    limitPrice: number;
    type: number;
    fyToken: string;
    exchange: number;
    segment: number;
    symbol: string;
    orderDateTime: string;
    orderValidity: string;
    productType: string;
    side: number;
    status: number;
    ex_sym: string;
    description: string;
}

export interface FyersTradeUpdate {
    id: string;
    orderId: string;
    symbol: string;
    side: number;
    qty: number;
    price: number;
    tradeDateTime: string;
}

export interface FyersPositionUpdate {
    symbol: string;
    netQty: number;
    avgPrice: number;
    ltp: number;
    pl: number;
    side: number;
}

// =============================================================================
// WebSocket Event Types
// =============================================================================

export type WebSocketEventType =
    | 'ticker'
    | 'trade'
    | 'orderbook'
    | 'candle'
    | 'order'
    | 'position'
    | 'status'
    | 'error'
    | 'heartbeat';

export interface WebSocketEvent<T = unknown> {
    type: WebSocketEventType;
    provider: string;
    channel: string;
    symbol?: string;
    timestamp: number;
    data: T;
    raw?: unknown;
}

// =============================================================================
// Connection Types
// =============================================================================

export type ConnectionState =
    | 'disconnected'
    | 'connecting'
    | 'connected'
    | 'reconnecting'
    | 'error';

export interface ConnectionInfo {
    provider: string;
    state: ConnectionState;
    connectedAt?: number;
    lastMessageAt?: number;
    messagesReceived: number;
    subscriptions: string[];
    latency?: number;
    error?: string;
}
