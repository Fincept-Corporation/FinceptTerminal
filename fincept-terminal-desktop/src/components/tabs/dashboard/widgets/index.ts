export { BaseWidget } from './BaseWidget';
export { NewsWidget } from './NewsWidget';
export { MarketDataWidget } from './MarketDataWidget';
export { WatchlistWidget } from './WatchlistWidget';
export { ForumWidget } from './ForumWidget';
export { CryptoWidget } from './CryptoWidget';
export { CommoditiesWidget } from './CommoditiesWidget';
export { GlobalIndicesWidget } from './GlobalIndicesWidget';
export { ForexWidget } from './ForexWidget';
export { MaritimeWidget } from './MaritimeWidget';
export { DataSourceWidget } from './DataSourceWidget';
// New widgets
export { PolymarketWidget } from './PolymarketWidget';
export { EconomicIndicatorsWidget } from './EconomicIndicatorsWidget';
export { PortfolioSummaryWidget } from './PortfolioSummaryWidget';
export { AlertsWidget } from './AlertsWidget';
export { CalendarWidget } from './CalendarWidget';
export { QuickTradeWidget } from './QuickTradeWidget';
export { GeopoliticsWidget } from './GeopoliticsWidget';
export { PerformanceWidget } from './PerformanceWidget';

// Widget type definitions
export type WidgetType =
  | 'news'
  | 'market'
  | 'watchlist'
  | 'forum'
  | 'crypto'
  | 'commodities'
  | 'indices'
  | 'forex'
  | 'maritime'
  | 'datasource'
  // New widget types
  | 'polymarket'
  | 'economic'
  | 'portfolio'
  | 'alerts'
  | 'calendar'
  | 'quicktrade'
  | 'geopolitics'
  | 'performance';

export interface WidgetConfig {
  id: string;
  type: WidgetType;
  title: string;
  config?: {
    // News widget config
    newsCategory?: string;
    newsLimit?: number;

    // Market widget config
    marketCategory?: string;
    marketTickers?: string[];

    // Watchlist widget config
    watchlistId?: string;
    watchlistName?: string;

    // Forum widget config
    forumCategoryId?: number;
    forumCategoryName?: string;
    forumLimit?: number;

    // Data Source widget config
    dataSourceAlias?: string;
    dataSourceDisplayName?: string;
  };
}

export const DEFAULT_WIDGET_CONFIGS: Record<WidgetType, Partial<WidgetConfig>> = {
  news: {
    type: 'news',
    title: 'Latest News',
    config: {
      newsCategory: 'ALL',
      newsLimit: 5
    }
  },
  market: {
    type: 'market',
    title: 'Market Indices',
    config: {
      marketCategory: 'Indices',
      marketTickers: ['^GSPC', '^IXIC', '^DJI', '^RUT']
    }
  },
  watchlist: {
    type: 'watchlist',
    title: 'My Watchlist',
    config: {
      watchlistName: 'Default'
    }
  },
  forum: {
    type: 'forum',
    title: 'Forum Posts',
    config: {
      forumCategoryName: 'Trending',
      forumLimit: 5
    }
  },
  crypto: {
    type: 'crypto',
    title: 'Cryptocurrency Markets',
    config: {}
  },
  commodities: {
    type: 'commodities',
    title: 'Commodities',
    config: {}
  },
  indices: {
    type: 'indices',
    title: 'Global Indices',
    config: {}
  },
  forex: {
    type: 'forex',
    title: 'Forex - Major Pairs',
    config: {}
  },
  maritime: {
    type: 'maritime',
    title: 'Maritime Intelligence',
    config: {}
  },
  datasource: {
    type: 'datasource',
    title: 'Data Source',
    config: {
      dataSourceAlias: '',
      dataSourceDisplayName: ''
    }
  },
  polymarket: {
    type: 'polymarket',
    title: 'Polymarket - Prediction Markets',
    config: {}
  },
  economic: {
    type: 'economic',
    title: 'Economic Indicators',
    config: {}
  },
  portfolio: {
    type: 'portfolio',
    title: 'Portfolio Summary',
    config: {}
  },
  alerts: {
    type: 'alerts',
    title: 'Price Alerts',
    config: {}
  },
  calendar: {
    type: 'calendar',
    title: 'Economic Calendar',
    config: {}
  },
  quicktrade: {
    type: 'quicktrade',
    title: 'Quick Trade',
    config: {}
  },
  geopolitics: {
    type: 'geopolitics',
    title: 'Geopolitical Risk',
    config: {}
  },
  performance: {
    type: 'performance',
    title: 'Performance Tracker',
    config: {}
  }
};
