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
      description: 'Click to add new widgets to your dashboard. Choose from market data, news feeds, watchlists, portfolios, and specialized analytics.',
      side: 'bottom',
      align: 'end'
    }
  },
  {
    element: '#dashboard-reset',
    popover: {
      title: 'Reset Layout',
      description: 'Restore the default dashboard layout. This will remove all custom widgets and arrangements.',
      side: 'bottom',
      align: 'end'
    }
  },
  {
    element: '#dashboard-save',
    popover: {
      title: 'Save Layout',
      description: 'Save your current dashboard configuration. Your widget arrangement and settings are persisted automatically.',
      side: 'bottom',
      align: 'end'
    }
  },
  {
    element: '#dashboard-grid',
    popover: {
      title: 'Widget Grid',
      description: 'Drag widgets to rearrange them. Resize by dragging the bottom-right corner. Each widget is interactive and updates in real-time.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '.widget-indices',
    popover: {
      title: 'Global Indices Widget',
      description: 'Monitor major global stock indices including S&P 500, NASDAQ, Dow Jones, and international markets. View real-time prices and changes.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '.widget-news',
    popover: {
      title: 'News Widget',
      description: 'Stay updated with real-time financial news. Customize categories including markets, crypto, regulatory, and sector-specific news.',
      side: 'left',
      align: 'start'
    }
  },
  {
    element: '.widget-forex',
    popover: {
      title: 'Forex Widget',
      description: 'Track major currency pairs including EUR/USD, GBP/USD, USD/JPY, and more. Monitor exchange rates and changes in real-time.',
      side: 'right',
      align: 'start'
    }
  },
  {
    element: '.widget-commodities',
    popover: {
      title: 'Commodities Widget',
      description: 'View prices for gold, silver, oil, natural gas, and other commodities. Track daily changes and trends.',
      side: 'left',
      align: 'start'
    }
  },
  {
    element: '.widget-portfolio',
    popover: {
      title: 'Portfolio Summary',
      description: 'Overview of your portfolio performance including total value, daily P&L, allocations, and top holdings.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '.widget-watchlist',
    popover: {
      title: 'Watchlist Widget',
      description: 'Create and monitor custom watchlists. Track your favorite stocks, crypto, and other securities with real-time updates.',
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
    overlayColor: 'rgba(0, 0, 0, 0.85)',
    animate: true,
    smoothScroll: true,
    allowClose: true,
    doneBtnText: 'FINISH',
    nextBtnText: 'NEXT',
    prevBtnText: 'BACK',
    stagePadding: 10,
    stageRadius: 5,
    onDestroyed: () => {
      console.log('Dashboard tab tour completed');
    }
  });

  return driverObj;
};
