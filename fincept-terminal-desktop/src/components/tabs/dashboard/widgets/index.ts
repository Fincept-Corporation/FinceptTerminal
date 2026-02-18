export { BaseWidget } from './BaseWidget';
export { NewsWidget } from './NewsWidget';
export { MarketDataWidget } from './MarketDataWidget';
export { WatchlistWidget } from './WatchlistWidget';
export { ForumWidget } from './ForumWidget';
export { CryptoWidget } from './CryptoWidget';
export { CommoditiesWidget } from './CommoditiesWidget';
export { GlobalIndicesWidget } from './GlobalIndicesWidget';
export { ForexWidget } from './ForexWidget';
export { DataSourceWidget } from './DataSourceWidget';
export { PolymarketWidget } from './PolymarketWidget';
export { EconomicIndicatorsWidget } from './EconomicIndicatorsWidget';
export { EconomicCalendarWidget } from './EconomicCalendarWidget';
export { PortfolioSummaryWidget } from './PortfolioSummaryWidget';
export { QuickTradeWidget } from './QuickTradeWidget';
export { GeopoliticsWidget } from './GeopoliticsWidget';
export { PerformanceWidget } from './PerformanceWidget';
export { NotesWidget } from './NotesWidget';
export { StockQuoteWidget } from './StockQuoteWidget';
export { TopMoversWidget } from './TopMoversWidget';
export { ScreenerWidget } from './ScreenerWidget';
export { RiskMetricsWidget } from './RiskMetricsWidget';
export { MarketSentimentWidget } from './MarketSentimentWidget';
export { AlgoStatusWidget } from './AlgoStatusWidget';
export { AlphaArenaWidget } from './AlphaArenaWidget';
export { BacktestSummaryWidget } from './BacktestSummaryWidget';
export { WatchlistAlertsWidget } from './WatchlistAlertsWidget';
export { LiveSignalsWidget } from './LiveSignalsWidget';
export { DBnomicsWidget } from './DBnomicsWidget';
export { AkShareWidget } from './AkShareWidget';
export { MaritimeWidget } from './MaritimeWidget';

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
  | 'datasource'
  | 'polymarket'
  | 'economic'
  | 'calendar'
  | 'portfolio'
  | 'quicktrade'
  | 'geopolitics'
  | 'performance'
  | 'heatmap'
  | 'notes'
  | 'stockquote'
  | 'topmovers'
  | 'screener'
  | 'riskmetrics'
  | 'sentiment'
  | 'algostatus'
  | 'alphaarena'
  | 'backtestsummary'
  | 'watchlistalerts'
  | 'livesignals'
  | 'dbnomics'
  | 'akshare'
  | 'maritime';

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

    // Economic Calendar widget config
    country?: string;
    limit?: number;

    // Notes widget config
    notesCategory?: string;
    notesLimit?: number;

    // Performance widget config
    portfolioId?: string;
    portfolioName?: string;

    // StockQuote widget config
    stockSymbol?: string;

    // Screener widget config
    screenerPreset?: 'value' | 'growth' | 'momentum';

    // WatchlistAlerts widget config
    alertThreshold?: number;

    // DBnomics widget config
    dbnomicsSeriesId?: string;

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
  calendar: {
    type: 'calendar',
    title: 'Economic Calendar',
    config: { country: 'US', limit: 10 }
  },
  portfolio: {
    type: 'portfolio',
    title: 'Portfolio Summary',
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
  },
  heatmap: {
    type: 'heatmap',
    title: 'Sector Heatmap',
    config: {}
  },
  notes: {
    type: 'notes',
    title: 'Recent Notes',
    config: {
      notesCategory: 'all',
      notesLimit: 10
    }
  },
  stockquote: {
    type: 'stockquote',
    title: 'Stock Quote',
    config: { stockSymbol: 'AAPL' }
  },
  topmovers: {
    type: 'topmovers',
    title: 'Top Movers',
    config: {}
  },
  screener: {
    type: 'screener',
    title: 'Stock Screener',
    config: { screenerPreset: 'value' }
  },
  riskmetrics: {
    type: 'riskmetrics',
    title: 'Risk Metrics',
    config: {}
  },
  sentiment: {
    type: 'sentiment',
    title: 'Market Sentiment',
    config: {}
  },
  algostatus: {
    type: 'algostatus',
    title: 'Algo Status',
    config: {}
  },
  alphaarena: {
    type: 'alphaarena',
    title: 'Alpha Arena',
    config: {}
  },
  backtestsummary: {
    type: 'backtestsummary',
    title: 'Backtest Summary',
    config: {}
  },
  watchlistalerts: {
    type: 'watchlistalerts',
    title: 'Watchlist Alerts',
    config: { alertThreshold: 3 }
  },
  livesignals: {
    type: 'livesignals',
    title: 'Live AI Signals',
    config: {}
  },
  dbnomics: {
    type: 'dbnomics',
    title: 'DBnomics Data',
    config: { dbnomicsSeriesId: 'FRED/UNRATE' }
  },
  akshare: {
    type: 'akshare',
    title: 'China Markets',
    config: {}
  },
  maritime: {
    type: 'maritime',
    title: 'Maritime Sector',
    config: {}
  }
};
