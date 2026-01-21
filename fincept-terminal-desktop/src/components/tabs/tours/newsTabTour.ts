import { driver, DriveStep } from 'driver.js';
import 'driver.js/dist/driver.css';
import './tourStyles.css';

export const newsTabTourSteps: DriveStep[] = [
  {
    popover: {
      title: 'Welcome to News Terminal',
      description: 'Real-time financial news aggregator with multi-source RSS feeds, AI-powered analysis, and priority alerts. Monitor breaking news, earnings, and market-moving events.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '#news-refresh',
    popover: {
      title: 'Refresh News',
      description: 'Manually refresh all news feeds to get the latest articles. The system also auto-refreshes at your configured interval.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#news-interval-settings',
    popover: {
      title: 'Auto-Refresh Interval',
      description: 'Configure how frequently news feeds refresh automatically. Choose from 1, 2, 5, 10, 15, or 30 minutes.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#news-filters',
    popover: {
      title: 'News Category Filters',
      description: 'Filter news by category using function keys (F1-F12). Categories include ALL, MARKETS, EARNINGS, ECONOMIC, TECH, ENERGY, BANKING, CRYPTO, and GEOPOLITICS.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#news-primary-feed',
    popover: {
      title: 'Primary News Feed',
      description: 'Main news stream with real-time articles. Click any article to view full details, sentiment analysis, and risk assessment.',
      side: 'right',
      align: 'start'
    }
  },
  {
    element: '#news-flash-alerts',
    popover: {
      title: 'Flash Alerts',
      description: 'Critical market-moving news flagged as FLASH priority. These require immediate attention and may impact trading decisions.',
      side: 'left',
      align: 'start'
    }
  },
  {
    element: '#news-urgent-feed',
    popover: {
      title: 'Urgent News',
      description: 'High-priority news items marked as URGENT. Important developments that may affect your portfolio or watchlist.',
      side: 'left',
      align: 'start'
    }
  },
  {
    element: '#news-article-detail',
    popover: {
      title: 'Article Details',
      description: 'Click any article to view full content, AI sentiment analysis, risk assessment, and related market impact analysis.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '#news-ai-analysis',
    popover: {
      title: 'AI News Analysis',
      description: 'Get AI-powered sentiment analysis, urgency rating, market impact assessment, and key insights for each article.',
      side: 'top',
      align: 'start'
    }
  }
];

export const createNewsTabTour = () => {
  const driverObj = driver({
    showProgress: true,
    steps: newsTabTourSteps,
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
      console.log('News tab tour completed');
    }
  });

  return driverObj;
};
