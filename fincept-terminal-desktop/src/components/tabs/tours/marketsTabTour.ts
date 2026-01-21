import { driver, DriveStep } from 'driver.js';
import 'driver.js/dist/driver.css';
import './tourStyles.css';

export const marketsTabTourSteps: DriveStep[] = [
  {
    popover: {
      title: 'Welcome to Markets Tab',
      description: 'This interactive tour will guide you through the Markets tab features. View real-time market data for global and regional markets.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '#markets-control-panel',
    popover: {
      title: 'Control Panel',
      description: 'Manage market data updates and settings. Refresh data manually, enable auto-updates, and configure update intervals.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#markets-refresh',
    popover: {
      title: 'Refresh Data',
      description: 'Click to manually refresh all market data. Get the latest prices, volumes, and market statistics.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#markets-auto-update',
    popover: {
      title: 'Auto Update',
      description: 'Toggle automatic data refresh. When enabled, market data will update at the configured interval.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#markets-timeframe',
    popover: {
      title: 'Update Interval',
      description: 'Choose how frequently data should refresh: 10 minutes, 15 minutes, 30 minutes, or 1 hour.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#markets-global-section',
    popover: {
      title: 'Global Markets',
      description: 'Monitor major global markets including indices, commodities, cryptocurrencies, and forex. Each panel shows real-time prices, changes, and 24-hour ranges.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '#markets-regional-section',
    popover: {
      title: 'Regional Markets',
      description: 'Track regional market performance. View stocks and indices from specific geographical regions like US, Europe, Asia, etc.',
      side: 'top',
      align: 'start'
    }
  }
];

export const createMarketsTabTour = () => {
  const driverObj = driver({
    showProgress: true,
    steps: marketsTabTourSteps,
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
      console.log('Markets tab tour completed');
    }
  });

  return driverObj;
};
