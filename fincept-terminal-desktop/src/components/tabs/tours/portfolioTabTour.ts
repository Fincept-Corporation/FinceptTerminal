import { driver, DriveStep } from 'driver.js';
import 'driver.js/dist/driver.css';
import './tourStyles.css';

export const portfolioTabTourSteps: DriveStep[] = [
  {
    popover: {
      title: 'Welcome to Portfolio Management',
      description: 'Professional portfolio tracking and analytics. Monitor positions, analyze performance, manage risk, and optimize your investments with institutional-grade tools.',
      side: 'top',
      align: 'center'
    }
  },
  {
    element: '#portfolio-selector',
    popover: {
      title: 'Portfolio Selector',
      description: 'Select from your portfolios or create a new one. Each portfolio can track different strategies, accounts, or investment goals.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#portfolio-create',
    popover: {
      title: 'Create New Portfolio',
      description: 'Create a new portfolio to track investments separately. Name it, set the owner, and choose the base currency.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#portfolio-buy',
    popover: {
      title: 'Buy Assets',
      description: 'Add new positions to your portfolio. Enter symbol, quantity, and purchase price to record transactions.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#portfolio-sell',
    popover: {
      title: 'Sell Assets',
      description: 'Record asset sales. Enter symbol, quantity, and sell price to update your portfolio and realize gains/losses.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#portfolio-refresh',
    popover: {
      title: 'Refresh Data',
      description: 'Update portfolio with latest market prices. Recalculates P&L, returns, and all performance metrics in real-time.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#portfolio-export',
    popover: {
      title: 'Export Data',
      description: 'Export portfolio data to CSV for external analysis, tax reporting, or record-keeping.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#portfolio-summary',
    popover: {
      title: 'Portfolio Summary',
      description: 'Overview of total value, cash balance, invested capital, unrealized P&L, and overall returns. Key metrics at a glance.',
      side: 'bottom',
      align: 'start'
    }
  },
  {
    element: '#portfolio-subtabs',
    popover: {
      title: 'Analysis Views',
      description: 'Navigate between different analysis views: Positions, History, Analytics, Sectors, Performance, Risk, Optimization, Active Management, Reports, and Alerts.',
      side: 'bottom',
      align: 'center'
    }
  },
  // Positions Tab
  {
    element: '#positions-view',
    popover: {
      title: 'Positions View',
      description: 'Current holdings with live prices, quantities, cost basis, market value, unrealized P&L, and returns. Monitor all active positions.',
      side: 'top',
      align: 'start'
    }
  },
  // History Tab
  {
    element: '#history-view',
    popover: {
      title: 'Transaction History',
      description: 'Complete transaction log with buys, sells, dates, prices, and realized P&L. Track all portfolio activity over time.',
      side: 'top',
      align: 'start'
    }
  },
  // Analytics Tab
  {
    element: '#analytics-view',
    popover: {
      title: 'Portfolio Analytics',
      description: 'Advanced analytics including correlation matrices, factor analysis, attribution, and statistical measures. Deep dive into portfolio dynamics.',
      side: 'top',
      align: 'start'
    }
  },
  // Sectors Tab
  {
    element: '#sectors-view',
    popover: {
      title: 'Sector Allocation',
      description: 'Visual breakdown of portfolio allocation by sector. Understand diversification and concentration risks across industries.',
      side: 'top',
      align: 'start'
    }
  },
  // Performance Tab
  {
    element: '#performance-view',
    popover: {
      title: 'Performance Tracking',
      description: 'Time-series performance charts, cumulative returns, drawdowns, and benchmark comparisons. Analyze portfolio performance over time.',
      side: 'top',
      align: 'start'
    }
  },
  // Risk Tab
  {
    element: '#risk-view',
    popover: {
      title: 'Risk Metrics',
      description: 'Comprehensive risk analysis including volatility, VaR, beta, Sharpe ratio, and drawdown analysis. Assess portfolio risk profile.',
      side: 'top',
      align: 'start'
    }
  },
  // Optimization Tab
  {
    element: '#optimization-view',
    popover: {
      title: 'Portfolio Optimization',
      description: 'Modern Portfolio Theory tools including efficient frontier, optimal allocations, and rebalancing recommendations.',
      side: 'top',
      align: 'start'
    }
  },
  // Active Management Tab
  {
    element: '#active-mgmt-view',
    popover: {
      title: 'Active Management',
      description: 'Active trading strategies, tactical allocations, and dynamic rebalancing tools for active portfolio management.',
      side: 'top',
      align: 'start'
    }
  },
  // Reports Tab
  {
    element: '#reports-view',
    popover: {
      title: 'Portfolio Reports',
      description: 'Generate comprehensive reports for tax purposes, client presentations, or performance reviews. Export as PDF or Excel.',
      side: 'top',
      align: 'start'
    }
  },
  // Alerts Tab
  {
    element: '#alerts-view',
    popover: {
      title: 'Portfolio Alerts',
      description: 'Configure alerts for price movements, P&L thresholds, rebalancing triggers, and risk limits. Stay informed of critical changes.',
      side: 'top',
      align: 'start'
    }
  }
];

export const createPortfolioTabTour = (setActiveSubTab?: (tab: string) => void) => {
  const driverObj = driver({
    showProgress: true,
    steps: portfolioTabTourSteps,
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
      if (setActiveSubTab) {
        const stepElement = options.state.activeElement?.id;
        if (stepElement === 'positions-view') setActiveSubTab('positions');
        else if (stepElement === 'history-view') setActiveSubTab('history');
        else if (stepElement === 'analytics-view') setActiveSubTab('analytics');
        else if (stepElement === 'sectors-view') setActiveSubTab('sectors');
        else if (stepElement === 'performance-view') setActiveSubTab('performance');
        else if (stepElement === 'risk-view') setActiveSubTab('risk');
        else if (stepElement === 'optimization-view') setActiveSubTab('optimization');
        else if (stepElement === 'active-mgmt-view') setActiveSubTab('active-mgmt');
        else if (stepElement === 'reports-view') setActiveSubTab('reports');
        else if (stepElement === 'alerts-view') setActiveSubTab('alerts');
      }
    },
    onDestroyed: () => {
      console.log('Portfolio tab tour completed');
    }
  });

  return driverObj;
};
