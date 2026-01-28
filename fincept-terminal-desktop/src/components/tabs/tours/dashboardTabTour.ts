import { driver, DriveStep } from 'driver.js';
import 'driver.js/dist/driver.css';
import './tourStyles.css';

export const dashboardTabTourSteps: DriveStep[] = [
  {
    popover: {
      title: 'Welcome to Dashboard',
      description: 'Your customizable financial command center. Add, arrange, and monitor widgets for markets, news, portfolios, and more in real-time.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '#dashboard-add-widget',
    popover: {
      title: 'Add Widget',
      description: 'Click to add new widgets to your dashboard. Choose from 18+ widget types: market data, news feeds, watchlists, portfolios, crypto, commodities, forex, economic indicators, alerts, calendar, and more.',
      side: 'bottom',
      align: 'end'
    }
  },
  {
    element: '#dashboard-save',
    popover: {
      title: 'Save Layout',
      description: 'Save your current dashboard configuration. Your widget arrangement, sizes, and settings are persisted automatically and restored on next login.',
      side: 'bottom',
      align: 'end'
    }
  },
  {
    element: '#dashboard-reset',
    popover: {
      title: 'Reset Layout',
      description: 'Restore the default dashboard layout. This will remove all custom widgets and arrangements, returning to the original configuration.',
      side: 'bottom',
      align: 'end'
    }
  },
  {
    element: '#dashboard-grid',
    popover: {
      title: 'Widget Grid',
      description: 'Drag widgets to rearrange them. Resize by dragging the bottom-right corner. Each widget is interactive and updates in real-time. The layout is fully customizable.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '.widget-indices',
    popover: {
      title: 'Global Indices Widget',
      description: 'Monitor major global stock indices: S&P 500, NASDAQ, Dow Jones, FTSE 100, DAX, Nikkei 225, and more. View real-time prices, daily changes, and percentage movements.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '.widget-news',
    popover: {
      title: 'News Widget',
      description: 'Stay updated with real-time financial news. Customize categories: Markets, Crypto, Regulatory (SEC Press Releases), Tech, and sector-specific news. Click any article to read full details.',
      side: 'left',
      align: 'start'
    }
  },
  {
    element: '.widget-forex',
    popover: {
      title: 'Forex Widget',
      description: 'Track major currency pairs: EUR/USD, GBP/USD, USD/JPY, AUD/USD, USD/CAD, and more. Monitor exchange rates, bid/ask spreads, and daily percentage changes in real-time.',
      side: 'right',
      align: 'start'
    }
  },
  {
    element: '.widget-commodities',
    popover: {
      title: 'Commodities Widget',
      description: 'View prices for gold, silver, crude oil, natural gas, copper, and agricultural commodities. Track daily changes, trends, and futures contracts.',
      side: 'left',
      align: 'start'
    }
  },
  {
    element: '.widget-forum',
    popover: {
      title: 'Forum Widget',
      description: 'Browse trending discussions from the Fincept community forum. View popular topics, latest posts, and engage with other traders and investors.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-maritime',
    popover: {
      title: 'Maritime Intelligence Widget',
      description: 'Track global shipping data, vessel movements, port congestion, freight rates, and trade routes. Essential for supply chain analysis and commodity trading.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-market',
    popover: {
      title: 'Market Data Widget',
      description: 'Monitor specific market segments like Tech Stocks (AAPL, MSFT, GOOGL, NVDA), Banking, Energy, or custom ticker lists. Fully customizable with real-time quotes.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-crypto',
    popover: {
      title: 'Crypto Widget',
      description: 'Track cryptocurrency prices: Bitcoin, Ethereum, Solana, Cardano, and 100+ altcoins. View market cap, 24h volume, price changes, and live charts.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-watchlist',
    popover: {
      title: 'Watchlist Widget',
      description: 'Create and monitor custom watchlists. Track your favorite stocks, crypto, ETFs, and other securities with real-time updates. Add/remove symbols easily.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-portfolio',
    popover: {
      title: 'Portfolio Summary',
      description: 'Overview of your portfolio performance: total value, daily P&L, asset allocations, top holdings, sector breakdown, and performance metrics. Syncs with broker accounts.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-alerts',
    popover: {
      title: 'Alerts Widget',
      description: 'Set price alerts, news alerts, and technical indicator triggers. Get notified when stocks hit target prices, volume spikes, or news breaks.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-calendar',
    popover: {
      title: 'Economic Calendar Widget',
      description: 'Track upcoming economic events: earnings releases, Fed meetings, GDP reports, unemployment data, and other market-moving events with impact ratings.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-quick-trade',
    popover: {
      title: 'Quick Trade Widget',
      description: 'Place orders directly from your dashboard. Quick buy/sell interface for connected brokers with real-time quotes and instant execution.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-geopolitics',
    popover: {
      title: 'Geopolitics Widget',
      description: 'AI-powered geopolitical analysis using frameworks from "The Grand Chessboard" and "Prisoners of Geography". Track global power dynamics affecting markets.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-performance',
    popover: {
      title: 'Performance Widget',
      description: 'Analyze portfolio performance metrics: Sharpe ratio, alpha, beta, max drawdown, win rate, and cumulative returns. Compare against benchmarks.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-data-source',
    popover: {
      title: 'Data Source Widget',
      description: 'Connect to 100+ data sources including Bloomberg, Reuters, Alpha Vantage, and custom APIs. Configure data feeds for each widget.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-polymarket',
    popover: {
      title: 'Polymarket Widget',
      description: 'Track prediction market odds from Polymarket. Monitor betting markets on elections, economic events, tech releases, and real-world outcomes.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-economic-indicators',
    popover: {
      title: 'Economic Indicators Widget',
      description: 'View key economic data: GDP growth, inflation (CPI/PPI), unemployment rate, interest rates, consumer confidence, manufacturing PMI, and more from global economies.',
      side: 'top',
      align: 'start'
    }
  }
];

export const createDashboardTabTour = () => {
  const driverObj = driver({
    showProgress: true,
    steps: dashboardTabTourSteps,
    popoverClass: 'fincept-tour-popover',
    animate: true,
    smoothScroll: true,
    allowClose: true,
    doneBtnText: 'FINISH',
    nextBtnText: 'NEXT',
    prevBtnText: 'BACK',
    showButtons: ['next', 'previous', 'close'],
    stagePadding: 6,
    stageRadius: 4,
    popoverOffset: 12,

    onDestroyed: () => {
      console.log('Dashboard tab tour completed');
    }
  });

  return driverObj;
};
