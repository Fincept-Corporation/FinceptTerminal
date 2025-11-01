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

// Widget type definitions
export type WidgetType = 'news' | 'market' | 'watchlist' | 'forum' | 'crypto' | 'commodities' | 'indices' | 'forex' | 'maritime' | 'datasource';

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
  }
};
