// Types for Crypto Trading Tab

export interface OrderBookLevel {
  price: number;
  size: number;
}

export interface OrderBook {
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  symbol: string;
  checksum?: number;
}

export interface Position {
  symbol: string;
  side: 'long' | 'short';
  quantity: number;
  entryPrice: number;
  positionValue: number;
  currentPrice: number;
  unrealizedPnl: number;
  pnlPercent: number;
}

export interface Order {
  id: string;
  symbol: string;
  side: 'buy' | 'sell';
  type: string;
  quantity: number;
  price?: number;
  status: string;
  createdAt: string;
}

export interface TickerData {
  symbol: string;
  price: number;
  bid?: number;
  ask?: number;
  last?: number;
  volume?: number;
  high?: number;
  low?: number;
  change?: number;
  changePercent?: number;
  open?: number;
  quoteVolume?: number; // USD-denominated 24h volume (e.g. Binance "q" field)
}

export interface TradeData {
  symbol: string;
  price: number;
  quantity: number;
  side: 'buy' | 'sell' | 'unknown';
  timestamp: number;
}

export interface WatchlistPrice {
  price: number;
  change: number;
}

// UI State Types
export type CenterViewType = 'chart' | 'depth' | 'trades' | 'algo';
export type BottomPanelTabType = 'positions' | 'orders' | 'history' | 'trades' | 'stats' | 'features' | 'cross-exchange' | 'grid-trading' | 'monitoring';
export type RightPanelViewType = 'orderbook' | 'order' | 'chat';
export type LeftSidebarViewType = 'watchlist' | 'positions' | 'orders';
