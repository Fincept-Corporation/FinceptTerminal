import { driver, DriveStep } from 'driver.js';
import 'driver.js/dist/driver.css';
import './tourStyles.css';

export const equityResearchTabTourSteps: DriveStep[] = [
  {
    popover: {
      title: 'Welcome to Equity Research',
      description: 'This comprehensive research tool provides in-depth analysis of stocks including financials, peer comparison, analyst ratings, and technical charts.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '#research-search',
    popover: {
      title: 'Symbol Search',
      description: 'Enter a stock ticker symbol (e.g., AAPL, TSLA, MSFT) and press Enter or click SEARCH to load comprehensive research data.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#research-tabs',
    popover: {
      title: 'Analysis Tabs',
      description: 'Switch between Overview, Financials, Analysis, Technicals, Peers, and News to explore different aspects of the company. Each tab provides specialized research tools.',
      side: 'bottom',
      align: 'start'
    }
  },
  // Overview Tab Steps
  {
    element: '#overview-trading',
    popover: {
      title: 'Today\'s Trading Data',
      description: 'View current trading session data including open, high, low, previous close, and volume. Monitor intraday price movements.',
      side: 'right',
      align: 'start'
    }
  },
  {
    element: '#overview-valuation',
    popover: {
      title: 'Valuation Metrics',
      description: 'Key valuation ratios including P/E, Forward P/E, P/B, PEG ratio, and dividend yield. Compare against industry benchmarks.',
      side: 'right',
      align: 'start'
    }
  },
  {
    element: '#overview-price-chart',
    popover: {
      title: 'Price Performance Chart',
      description: 'Historical price chart with customizable timeframes (1M, 3M, 6M, 1Y, 5Y). Visualize stock performance over different periods.',
      side: 'left',
      align: 'center'
    }
  },
  {
    element: '#research-analyst-ratings',
    popover: {
      title: 'Analyst Ratings & Targets',
      description: 'View consensus analyst recommendations, price targets (high, mean, low), and number of analysts covering the stock.',
      side: 'left',
      align: 'start'
    }
  },
  {
    element: '#overview-profitability',
    popover: {
      title: 'Profitability Metrics',
      description: 'Analyze profit margins including gross, operating, EBITDA, and net margins. Track return on equity (ROE) and return on assets (ROA).',
      side: 'left',
      align: 'start'
    }
  },
  // Financials Tab Steps
  {
    element: '#research-financials',
    popover: {
      title: 'Financial Statements',
      description: 'Access detailed financial statements including income statement, balance sheet, and cash flow statement. Analyze multi-year trends.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '#financials-controls',
    popover: {
      title: 'Financial Controls',
      description: 'Customize font size and select the number of years to display. Adjust view to focus on specific time periods.',
      side: 'bottom',
      align: 'start'
    }
  },
  // Technicals Tab
  {
    element: '#research-chart',
    popover: {
      title: 'Technical Chart & Analysis',
      description: 'Advanced charting with technical indicators, drawing tools, and multiple timeframes. Perform technical analysis with various chart types.',
      side: 'top',
      align: 'center'
    }
  },
  // Peers Tab
  {
    element: '#research-peer-comparison',
    popover: {
      title: 'Peer Comparison',
      description: 'Compare the stock against industry peers. Analyze relative valuations, growth rates, profitability, and market positioning.',
      side: 'top',
      align: 'start'
    }
  },
  // Analysis Tab
  {
    element: '#analysis-cfa-panel',
    popover: {
      title: 'CFA-Level Financial Analysis',
      description: 'Professional-grade financial analysis panel with comprehensive metrics, ratios, and valuations following CFA Institute standards.',
      side: 'top',
      align: 'start'
    }
  },
  {
    element: '#analysis-financial-health',
    popover: {
      title: 'Financial Health Metrics',
      description: 'Evaluate the company\'s financial strength with liquidity ratios, solvency metrics, debt levels, and cash flow analysis.',
      side: 'top',
      align: 'start'
    }
  },
  // News Tab
  {
    element: '#news-tab-content',
    popover: {
      title: 'Latest News & Events',
      description: 'Stay updated with real-time news, press releases, earnings announcements, and market-moving events for the selected stock.',
      side: 'top',
      align: 'start'
    }
  }
];

export const createEquityResearchTabTour = (setActiveTab?: (tab: string) => void) => {
  const driverObj = driver({
    showProgress: true,
    steps: equityResearchTabTourSteps,
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
    onHighlightStarted: (element, step, options) => {
      // Navigate to appropriate sub-tab based on step
      if (setActiveTab) {
        const stepElement = options.state.activeElement?.id;
        // Overview tab elements
        if (stepElement?.startsWith('overview-')) {
          setActiveTab('overview');
        }
        // Financials tab elements
        else if (stepElement?.startsWith('financials-') || stepElement === 'research-financials') {
          setActiveTab('financials');
        }
        // Analysis tab elements
        else if (stepElement?.startsWith('analysis-')) {
          setActiveTab('analysis');
        }
        // Technicals/Chart tab elements
        else if (stepElement === 'research-chart') {
          setActiveTab('technicals');
        }
        // Peers tab elements
        else if (stepElement === 'research-peer-comparison') {
          setActiveTab('peers');
        }
        // News tab elements
        else if (stepElement === 'news-tab-content') {
          setActiveTab('news');
        }
      }
    },
    onDestroyed: () => {
      console.log('Equity Research tab tour completed');
    }
  });

  return driverObj;
};
