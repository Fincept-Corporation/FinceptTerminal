/**
 * Equity Trading Tab - Shared Type Definitions
 */
import type { StockExchange, Quote, MarketDepth, TimeFrame, Position, Holding, Order, Trade, Funds } from '@/brokers/stocks/types';
import type { StockBrokerMetadata, IStockBrokerAdapter } from '@/brokers/stocks/types';
import type { SymbolSearchResult, SupportedBroker } from '@/services/trading/masterContractService';

// View state types
export type BottomPanelTab = 'positions' | 'holdings' | 'orders' | 'history' | 'stats' | 'market' | 'grid-trading' | 'algo-trading' | 'monitoring' | 'brokers';
export type LeftSidebarView = 'watchlist' | 'indices' | 'sectors';
export type CenterView = 'chart' | 'depth' | 'trades';
export type RightPanelView = 'orderbook' | 'marketdepth' | 'info';

// Props interfaces for sub-components
export interface EquityTopNavProps {
  activeBroker: string | null;
  activeBrokerMetadata: StockBrokerMetadata | null;
  tradingMode: string;
  isConnected: boolean;
  isPaper: boolean;
  isLive: boolean;
  showSettings: boolean;
  currentTime: Date;
  onTradingModeChange: (mode: 'paper' | 'live') => void;
  onSettingsToggle: () => void;
}

export interface EquityTickerBarProps {
  selectedSymbol: string;
  selectedExchange: StockExchange;
  activeBroker: string | null;
  quote: Quote | null;
  funds: Funds | null;
  isAuthenticated: boolean;
  totalPositionPnl: number;
  priceChange: number;
  priceChangePercent: number;
  currentPrice: number;
  dayRange: number;
  dayRangePercent: number;
  onSymbolSelect: (symbol: string, exchange: StockExchange, result?: SymbolSearchResult) => void;
  fmtPrice: (value: number, compact?: boolean) => string;
}

export interface EquityWatchlistProps {
  watchlist: string[];
  watchlistQuotes: Record<string, Quote>;
  selectedSymbol: string;
  selectedExchange: StockExchange;
  leftSidebarView: LeftSidebarView;
  useWebSocketForWatchlist: boolean;
  onSymbolSelect: (symbol: string, exchange: StockExchange) => void;
  onViewChange: (view: LeftSidebarView) => void;
  onWebSocketToggle: () => void;
}

export interface EquityChartAreaProps {
  selectedSymbol: string;
  selectedExchange: StockExchange;
  centerView: CenterView;
  chartInterval: TimeFrame;
  quote: Quote | null;
  symbolTrades: Trade[];
  isBottomPanelMinimized: boolean;
  isRefreshing: boolean;
  onViewChange: (view: CenterView) => void;
  onIntervalChange: (interval: TimeFrame) => void;
  onRefresh: () => void;
  fmtPrice: (value: number, compact?: boolean) => string;
}

export interface EquityBottomPanelProps {
  activeTab: BottomPanelTab;
  isMinimized: boolean;
  positions: Position[];
  holdings: Holding[];
  orders: Order[];
  funds: Funds | null;
  totalPositionValue: number;
  totalPositionPnl: number;
  totalHoldingsValue: number;
  selectedSymbol: string;
  selectedExchange: StockExchange;
  currentPrice: number;
  activeBroker: string | null;
  isPaper: boolean;
  adapter: IStockBrokerAdapter | null;
  onTabChange: (tab: BottomPanelTab) => void;
  onMinimizeToggle: () => void;
  fmtPrice: (value: number, compact?: boolean) => string;
}

export interface EquityOrderEntryProps {
  selectedSymbol: string;
  selectedExchange: StockExchange;
  currentPrice: number;
  isAuthenticated: boolean;
  isLoading: boolean;
  isConnecting: boolean;
  onConnectBroker: () => void;
}

export interface EquityOrderBookProps {
  rightPanelView: RightPanelView;
  marketDepth: MarketDepth | null;
  quote: Quote | null;
  selectedSymbol: string;
  selectedExchange: StockExchange;
  isAuthenticated: boolean;
  isLoadingQuote: boolean;
  spread: number;
  spreadPercent: number;
  onViewChange: (view: RightPanelView) => void;
  fmtPrice: (value: number, compact?: boolean) => string;
}

export interface EquityStatusBarProps {
  activeBrokerMetadata: StockBrokerMetadata | null;
  tradingMode: string;
  isConnected: boolean;
  isPaper: boolean;
}
