import { AgentConfig, AgentCategory } from './types';

export const AGENT_CONFIGS: AgentConfig[] = [
  // ========== HEDGE FUND AGENTS (10) ==========
  {
    id: 'macro-cycle',
    name: 'Macro Cycle Agent',
    type: 'macro-cycle',
    category: 'hedge-fund',
    icon: '🔄',
    color: '#3b82f6',
    description: 'Analyzes business cycles and economic phases',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'lookback_period', label: 'Lookback Period (years)', type: 'number', defaultValue: 10, min: 1, max: 30 },
      { name: 'indicators', label: 'Economic Indicators', type: 'multi-select', defaultValue: ['GDP', 'CPI', 'UNRATE'],
        options: [
          { label: 'GDP', value: 'GDP' },
          { label: 'CPI', value: 'CPI' },
          { label: 'Unemployment Rate', value: 'UNRATE' },
          { label: 'Federal Funds Rate', value: 'FEDFUNDS' },
          { label: 'Industrial Production', value: 'INDPRO' }
        ]
      },
      { name: 'confidence_threshold', label: 'Confidence Threshold', type: 'slider', defaultValue: 0.7, min: 0, max: 1, step: 0.1 }
    ],
    requiredDataSources: ['economic-data']
  },
  {
    id: 'central-bank',
    name: 'Central Bank Policy Agent',
    type: 'central-bank',
    category: 'hedge-fund',
    icon: '🏦',
    color: '#10b981',
    description: 'Monitors Federal Reserve and central bank policies',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'target_banks', label: 'Central Banks', type: 'multi-select', defaultValue: ['FED'],
        options: [
          { label: 'Federal Reserve (FED)', value: 'FED' },
          { label: 'European Central Bank (ECB)', value: 'ECB' },
          { label: 'Bank of Japan (BOJ)', value: 'BOJ' },
          { label: 'Bank of England (BOE)', value: 'BOE' }
        ]
      },
      { name: 'analysis_depth', label: 'Analysis Depth', type: 'select', defaultValue: 'comprehensive',
        options: [
          { label: 'Quick', value: 'quick' },
          { label: 'Standard', value: 'standard' },
          { label: 'Comprehensive', value: 'comprehensive' }
        ]
      }
    ],
    requiredDataSources: ['news-feed', 'economic-data']
  },
  {
    id: 'geopolitical',
    name: 'Geopolitical Risk Agent',
    type: 'geopolitical',
    category: 'hedge-fund',
    icon: '🌍',
    color: '#f59e0b',
    description: 'Assesses geopolitical risks and their market impact',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'regions', label: 'Monitor Regions', type: 'multi-select', defaultValue: ['global'],
        options: [
          { label: 'Global', value: 'global' },
          { label: 'North America', value: 'north_america' },
          { label: 'Europe', value: 'europe' },
          { label: 'Asia Pacific', value: 'asia_pacific' },
          { label: 'Middle East', value: 'middle_east' }
        ]
      },
      { name: 'risk_threshold', label: 'Risk Alert Threshold', type: 'slider', defaultValue: 0.6, min: 0, max: 1, step: 0.1 }
    ],
    requiredDataSources: ['news-feed']
  },
  {
    id: 'sentiment',
    name: 'Market Sentiment Agent',
    type: 'sentiment',
    category: 'hedge-fund',
    icon: '😊',
    color: '#ec4899',
    description: 'Analyzes market sentiment from multiple sources',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'sources', label: 'Sentiment Sources', type: 'multi-select', defaultValue: ['news', 'social'],
        options: [
          { label: 'Financial News', value: 'news' },
          { label: 'Social Media', value: 'social' },
          { label: 'Analyst Reports', value: 'analyst' },
          { label: 'Market Indicators', value: 'market' }
        ]
      },
      { name: 'contrarian_mode', label: 'Contrarian Signals', type: 'boolean', defaultValue: true },
      { name: 'sentiment_window', label: 'Analysis Window (hours)', type: 'number', defaultValue: 24, min: 1, max: 168 }
    ],
    requiredDataSources: ['news-feed', 'social-media']
  },
  {
    id: 'institutional-flow',
    name: 'Institutional Flow Agent',
    type: 'institutional-flow',
    category: 'hedge-fund',
    icon: '💼',
    color: '#8b5cf6',
    description: 'Tracks institutional money flows and smart money',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'flow_types', label: 'Flow Types', type: 'multi-select', defaultValue: ['equity', 'bond'],
        options: [
          { label: 'Equity Flows', value: 'equity' },
          { label: 'Bond Flows', value: 'bond' },
          { label: 'ETF Flows', value: 'etf' },
          { label: 'Mutual Fund Flows', value: 'mutual_fund' }
        ]
      },
      { name: 'tracking_period', label: 'Tracking Period (days)', type: 'number', defaultValue: 30, min: 1, max: 365 }
    ],
    requiredDataSources: ['market-data']
  },
  {
    id: 'supply-chain',
    name: 'Supply Chain Agent',
    type: 'supply-chain',
    category: 'hedge-fund',
    icon: '🚚',
    color: '#06b6d4',
    description: 'Monitors supply chain disruptions and logistics',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'industries', label: 'Industries to Monitor', type: 'multi-select', defaultValue: ['technology', 'energy'],
        options: [
          { label: 'Technology', value: 'technology' },
          { label: 'Energy', value: 'energy' },
          { label: 'Manufacturing', value: 'manufacturing' },
          { label: 'Retail', value: 'retail' },
          { label: 'Healthcare', value: 'healthcare' }
        ]
      }
    ],
    requiredDataSources: ['news-feed', 'market-data']
  },
  {
    id: 'innovation',
    name: 'Innovation Trends Agent',
    type: 'innovation',
    category: 'hedge-fund',
    icon: '💡',
    color: '#f97316',
    description: 'Identifies emerging technologies and innovation trends',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'tech_sectors', label: 'Technology Sectors', type: 'multi-select', defaultValue: ['ai', 'blockchain'],
        options: [
          { label: 'Artificial Intelligence', value: 'ai' },
          { label: 'Blockchain', value: 'blockchain' },
          { label: 'Quantum Computing', value: 'quantum' },
          { label: 'Biotechnology', value: 'biotech' },
          { label: 'Clean Energy', value: 'clean_energy' }
        ]
      },
      { name: 'patent_analysis', label: 'Include Patent Analysis', type: 'boolean', defaultValue: true }
    ],
    requiredDataSources: ['news-feed']
  },
  {
    id: 'currency',
    name: 'Currency Analysis Agent',
    type: 'currency',
    category: 'hedge-fund',
    icon: '💱',
    color: '#14b8a6',
    description: 'Analyzes currency movements and forex markets',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'currency_pairs', label: 'Currency Pairs', type: 'multi-select', defaultValue: ['USD/EUR', 'USD/JPY'],
        options: [
          { label: 'USD/EUR', value: 'USD/EUR' },
          { label: 'USD/JPY', value: 'USD/JPY' },
          { label: 'USD/GBP', value: 'USD/GBP' },
          { label: 'USD/CHF', value: 'USD/CHF' },
          { label: 'USD/CNY', value: 'USD/CNY' }
        ]
      }
    ],
    requiredDataSources: ['forex-data']
  },
  {
    id: 'regulatory',
    name: 'Regulatory Impact Agent',
    type: 'regulatory',
    category: 'hedge-fund',
    icon: '⚖️',
    color: '#64748b',
    description: 'Monitors regulatory changes and compliance impacts',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'jurisdictions', label: 'Jurisdictions', type: 'multi-select', defaultValue: ['US', 'EU'],
        options: [
          { label: 'United States', value: 'US' },
          { label: 'European Union', value: 'EU' },
          { label: 'United Kingdom', value: 'UK' },
          { label: 'China', value: 'CN' },
          { label: 'Japan', value: 'JP' }
        ]
      }
    ],
    requiredDataSources: ['news-feed']
  },
  {
    id: 'behavioral',
    name: 'Behavioral Psychology Agent',
    type: 'behavioral',
    category: 'hedge-fund',
    icon: '🧠',
    color: '#a855f7',
    description: 'Analyzes market psychology and behavioral patterns',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'behavioral_signals', label: 'Behavioral Signals', type: 'multi-select', defaultValue: ['fear_greed', 'herding'],
        options: [
          { label: 'Fear & Greed Index', value: 'fear_greed' },
          { label: 'Herding Behavior', value: 'herding' },
          { label: 'Overconfidence', value: 'overconfidence' },
          { label: 'Loss Aversion', value: 'loss_aversion' }
        ]
      }
    ],
    requiredDataSources: ['market-data']
  },

  // ========== TRADER/INVESTOR AGENTS (10) ==========
  {
    id: 'warren-buffett',
    name: 'Warren Buffett Style',
    type: 'warren-buffett',
    category: 'trader',
    icon: '💰',
    color: '#22c55e',
    description: 'Value investing with economic moats and long-term focus',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'moat_focus', label: 'Economic Moat Analysis', type: 'boolean', defaultValue: true },
      { name: 'min_roe', label: 'Minimum ROE (%)', type: 'number', defaultValue: 15, min: 0, max: 100 },
      { name: 'max_pe_ratio', label: 'Maximum P/E Ratio', type: 'number', defaultValue: 25, min: 0, max: 100 }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'charlie-munger',
    name: 'Charlie Munger Style',
    type: 'charlie-munger',
    category: 'trader',
    icon: '🎓',
    color: '#3b82f6',
    description: 'Multidisciplinary thinking and quality businesses',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'quality_threshold', label: 'Quality Score Threshold', type: 'slider', defaultValue: 0.8, min: 0, max: 1, step: 0.1 },
      { name: 'mental_models', label: 'Apply Mental Models', type: 'boolean', defaultValue: true }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'benjamin-graham',
    name: 'Benjamin Graham Style',
    type: 'benjamin-graham',
    category: 'trader',
    icon: '📚',
    color: '#6366f1',
    description: 'Deep value investing with margin of safety',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'margin_of_safety', label: 'Margin of Safety (%)', type: 'number', defaultValue: 30, min: 0, max: 100 },
      { name: 'net_net_focus', label: 'Net-Net Analysis', type: 'boolean', defaultValue: false }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'seth-klarman',
    name: 'Seth Klarman Style',
    type: 'seth-klarman',
    category: 'trader',
    icon: '🛡️',
    color: '#0891b2',
    description: 'Risk-averse value investing with special situations',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'risk_aversion', label: 'Risk Aversion Level', type: 'slider', defaultValue: 0.8, min: 0, max: 1, step: 0.1 },
      { name: 'special_situations', label: 'Include Special Situations', type: 'boolean', defaultValue: true }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'howard-marks',
    name: 'Howard Marks Style',
    type: 'howard-marks',
    category: 'trader',
    icon: '🎯',
    color: '#dc2626',
    description: 'Contrarian investing and cycle awareness',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'contrarian_strength', label: 'Contrarian Strength', type: 'slider', defaultValue: 0.7, min: 0, max: 1, step: 0.1 },
      { name: 'cycle_timing', label: 'Market Cycle Timing', type: 'boolean', defaultValue: true }
    ],
    requiredDataSources: ['market-data', 'sentiment-data']
  },
  {
    id: 'joel-greenblatt',
    name: 'Joel Greenblatt Style',
    type: 'joel-greenblatt',
    category: 'trader',
    icon: '🔢',
    color: '#16a34a',
    description: 'Magic formula: high return on capital, low valuation',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'min_roic', label: 'Minimum ROIC (%)', type: 'number', defaultValue: 20, min: 0, max: 100 },
      { name: 'max_ev_ebit', label: 'Maximum EV/EBIT', type: 'number', defaultValue: 15, min: 0, max: 50 }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'david-einhorn',
    name: 'David Einhorn Style',
    type: 'david-einhorn',
    category: 'trader',
    icon: '🎲',
    color: '#ca8a04',
    description: 'Value investing with short selling and activism',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'short_candidates', label: 'Identify Short Candidates', type: 'boolean', defaultValue: true },
      { name: 'accounting_focus', label: 'Deep Accounting Analysis', type: 'boolean', defaultValue: true }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'bill-miller',
    name: 'Bill Miller Style',
    type: 'bill-miller',
    category: 'trader',
    icon: '📊',
    color: '#7c3aed',
    description: 'Concentrated value with growth potential',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'concentration_level', label: 'Portfolio Concentration', type: 'slider', defaultValue: 0.7, min: 0, max: 1, step: 0.1 },
      { name: 'growth_value_balance', label: 'Growth-Value Balance', type: 'slider', defaultValue: 0.5, min: 0, max: 1, step: 0.1 }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'jean-marie-eveillard',
    name: 'Jean-Marie Eveillard Style',
    type: 'jean-marie-eveillard',
    category: 'trader',
    icon: '🌍',
    color: '#0284c7',
    description: 'Global value investing with downside protection',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'global_scope', label: 'Global Market Scope', type: 'boolean', defaultValue: true },
      { name: 'downside_protection', label: 'Downside Protection Focus', type: 'slider', defaultValue: 0.8, min: 0, max: 1, step: 0.1 }
    ],
    requiredDataSources: ['financial-statements', 'market-data']
  },
  {
    id: 'marty-whitman',
    name: 'Marty Whitman Style',
    type: 'marty-whitman',
    category: 'trader',
    icon: '💎',
    color: '#be123c',
    description: 'Safe and cheap investing with balance sheet analysis',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'balance_sheet_focus', label: 'Balance Sheet Priority', type: 'boolean', defaultValue: true },
      { name: 'asset_coverage', label: 'Minimum Asset Coverage Ratio', type: 'number', defaultValue: 1.5, min: 1, max: 5, step: 0.1 }
    ],
    requiredDataSources: ['financial-statements']
  },

  // ========== ANALYSIS AGENTS (5) ==========
  {
    id: 'technical-analysis',
    name: 'Technical Analysis Agent',
    type: 'technical-analysis',
    category: 'analysis',
    icon: '📈',
    color: '#059669',
    description: 'Technical indicators and chart pattern analysis',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'indicators', label: 'Technical Indicators', type: 'multi-select', defaultValue: ['RSI', 'MACD'],
        options: [
          { label: 'RSI', value: 'RSI' },
          { label: 'MACD', value: 'MACD' },
          { label: 'Moving Averages', value: 'MA' },
          { label: 'Bollinger Bands', value: 'BB' },
          { label: 'Fibonacci Retracement', value: 'FIBO' }
        ]
      },
      { name: 'timeframe', label: 'Timeframe', type: 'select', defaultValue: 'daily',
        options: [
          { label: '1 Hour', value: '1h' },
          { label: '4 Hours', value: '4h' },
          { label: 'Daily', value: 'daily' },
          { label: 'Weekly', value: 'weekly' }
        ]
      }
    ],
    requiredDataSources: ['market-data']
  },
  {
    id: 'fundamental-analysis',
    name: 'Fundamental Analysis Agent',
    type: 'fundamental-analysis',
    category: 'analysis',
    icon: '📊',
    color: '#2563eb',
    description: 'Deep fundamental analysis of financials',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'analysis_type', label: 'Analysis Type', type: 'select', defaultValue: 'comprehensive',
        options: [
          { label: 'Quick Scan', value: 'quick' },
          { label: 'Standard', value: 'standard' },
          { label: 'Comprehensive', value: 'comprehensive' },
          { label: 'Deep Dive', value: 'deep' }
        ]
      },
      { name: 'years_history', label: 'Years of History', type: 'number', defaultValue: 10, min: 1, max: 30 }
    ],
    requiredDataSources: ['financial-statements']
  },
  {
    id: 'risk-assessment',
    name: 'Risk Assessment Agent',
    type: 'risk-assessment',
    category: 'analysis',
    icon: '⚠️',
    color: '#dc2626',
    description: 'Portfolio risk analysis and VaR calculation',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'risk_metrics', label: 'Risk Metrics', type: 'multi-select', defaultValue: ['VaR', 'sharpe'],
        options: [
          { label: 'Value at Risk (VaR)', value: 'VaR' },
          { label: 'Sharpe Ratio', value: 'sharpe' },
          { label: 'Maximum Drawdown', value: 'mdd' },
          { label: 'Beta', value: 'beta' },
          { label: 'Volatility', value: 'volatility' }
        ]
      },
      { name: 'confidence_level', label: 'VaR Confidence Level (%)', type: 'number', defaultValue: 95, min: 90, max: 99 }
    ],
    requiredDataSources: ['market-data', 'portfolio-data']
  },
  {
    id: 'correlation-analysis',
    name: 'Correlation Analysis Agent',
    type: 'correlation-analysis',
    category: 'analysis',
    icon: '🔗',
    color: '#7c3aed',
    description: 'Asset correlation and diversification analysis',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'correlation_window', label: 'Correlation Window (days)', type: 'number', defaultValue: 90, min: 30, max: 365 },
      { name: 'threshold', label: 'Correlation Threshold', type: 'slider', defaultValue: 0.7, min: 0, max: 1, step: 0.1 }
    ],
    requiredDataSources: ['market-data']
  },
  {
    id: 'scenario-analysis',
    name: 'Scenario Analysis Agent',
    type: 'scenario-analysis',
    category: 'analysis',
    icon: '🎭',
    color: '#0891b2',
    description: 'Stress testing and scenario modeling',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'scenarios', label: 'Scenarios to Test', type: 'multi-select', defaultValue: ['recession', 'inflation'],
        options: [
          { label: 'Recession', value: 'recession' },
          { label: 'High Inflation', value: 'inflation' },
          { label: 'Market Crash', value: 'crash' },
          { label: 'Interest Rate Spike', value: 'rate_spike' },
          { label: 'Geopolitical Crisis', value: 'geopolitical' }
        ]
      }
    ],
    requiredDataSources: ['market-data', 'economic-data']
  },

  // ========== DATA AGENTS (3) ==========
  {
    id: 'data-collector',
    name: 'Data Collector',
    type: 'data-collector',
    category: 'data',
    icon: '📥',
    color: '#84cc16',
    description: 'Collects data from configured sources',
    hasInput: false,
    hasOutput: true,
    parameters: [
      { name: 'data_source', label: 'Data Source', type: 'data-source', required: true },
      { name: 'symbols', label: 'Symbols/Tickers', type: 'text', defaultValue: 'AAPL,GOOGL,MSFT' },
      { name: 'interval', label: 'Update Interval (minutes)', type: 'number', defaultValue: 60, min: 1, max: 1440 }
    ]
  },
  {
    id: 'data-transformer',
    name: 'Data Transformer',
    type: 'data-transformer',
    category: 'data',
    icon: '🔄',
    color: '#f59e0b',
    description: 'Transforms and normalizes data',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'transformations', label: 'Transformations', type: 'multi-select', defaultValue: ['normalize'],
        options: [
          { label: 'Normalize', value: 'normalize' },
          { label: 'Log Transform', value: 'log' },
          { label: 'Percentage Change', value: 'pct_change' },
          { label: 'Rolling Average', value: 'rolling_avg' },
          { label: 'Resample', value: 'resample' }
        ]
      }
    ]
  },
  {
    id: 'data-aggregator',
    name: 'Data Aggregator',
    type: 'data-aggregator',
    category: 'data',
    icon: '📊',
    color: '#8b5cf6',
    description: 'Aggregates data from multiple sources',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'aggregation_method', label: 'Aggregation Method', type: 'select', defaultValue: 'merge',
        options: [
          { label: 'Merge', value: 'merge' },
          { label: 'Concatenate', value: 'concat' },
          { label: 'Average', value: 'average' },
          { label: 'Weighted Average', value: 'weighted_avg' }
        ]
      }
    ]
  },

  // ========== EXECUTION AGENTS (2) ==========
  {
    id: 'signal-generator',
    name: 'Signal Generator',
    type: 'signal-generator',
    category: 'execution',
    icon: '📡',
    color: '#10b981',
    description: 'Generates trading signals from analysis',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'signal_threshold', label: 'Signal Threshold', type: 'slider', defaultValue: 0.7, min: 0, max: 1, step: 0.05 },
      { name: 'signal_types', label: 'Signal Types', type: 'multi-select', defaultValue: ['buy', 'sell'],
        options: [
          { label: 'Buy Signal', value: 'buy' },
          { label: 'Sell Signal', value: 'sell' },
          { label: 'Hold Signal', value: 'hold' },
          { label: 'Short Signal', value: 'short' }
        ]
      }
    ]
  },
  {
    id: 'portfolio-manager',
    name: 'Portfolio Manager',
    type: 'portfolio-manager',
    category: 'execution',
    icon: '💼',
    color: '#6366f1',
    description: 'Manages portfolio allocation and rebalancing',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'rebalance_frequency', label: 'Rebalance Frequency', type: 'select', defaultValue: 'monthly',
        options: [
          { label: 'Daily', value: 'daily' },
          { label: 'Weekly', value: 'weekly' },
          { label: 'Monthly', value: 'monthly' },
          { label: 'Quarterly', value: 'quarterly' }
        ]
      },
      { name: 'max_position_size', label: 'Max Position Size (%)', type: 'number', defaultValue: 5, min: 1, max: 100 }
    ]
  },

  // ========== MONITORING AGENTS (2) ==========
  {
    id: 'alert-monitor',
    name: 'Alert Monitor',
    type: 'alert-monitor',
    category: 'monitoring',
    icon: '🔔',
    color: '#f59e0b',
    description: 'Monitors conditions and triggers alerts',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'alert_conditions', label: 'Alert Conditions', type: 'multi-select', defaultValue: ['price_change'],
        options: [
          { label: 'Price Change', value: 'price_change' },
          { label: 'Volume Spike', value: 'volume_spike' },
          { label: 'News Event', value: 'news_event' },
          { label: 'Technical Signal', value: 'technical_signal' }
        ]
      },
      { name: 'notification_channel', label: 'Notification Channel', type: 'select', defaultValue: 'email',
        options: [
          { label: 'Email', value: 'email' },
          { label: 'SMS', value: 'sms' },
          { label: 'Push Notification', value: 'push' },
          { label: 'Webhook', value: 'webhook' }
        ]
      }
    ]
  },
  {
    id: 'performance-tracker',
    name: 'Performance Tracker',
    type: 'performance-tracker',
    category: 'monitoring',
    icon: '📈',
    color: '#22c55e',
    description: 'Tracks and reports strategy performance',
    hasInput: true,
    hasOutput: true,
    parameters: [
      { name: 'metrics', label: 'Performance Metrics', type: 'multi-select', defaultValue: ['returns', 'sharpe'],
        options: [
          { label: 'Total Returns', value: 'returns' },
          { label: 'Sharpe Ratio', value: 'sharpe' },
          { label: 'Max Drawdown', value: 'drawdown' },
          { label: 'Win Rate', value: 'win_rate' },
          { label: 'Alpha/Beta', value: 'alpha_beta' }
        ]
      },
      { name: 'reporting_frequency', label: 'Reporting Frequency', type: 'select', defaultValue: 'daily',
        options: [
          { label: 'Real-time', value: 'realtime' },
          { label: 'Hourly', value: 'hourly' },
          { label: 'Daily', value: 'daily' },
          { label: 'Weekly', value: 'weekly' }
        ]
      }
    ]
  },
];

// Category metadata
export const AGENT_CATEGORIES = [
  { id: 'hedge-fund', label: 'Hedge Fund Agents', color: '#3b82f6', description: 'Institutional-grade market analysis agents' },
  { id: 'trader', label: 'Legendary Traders', color: '#22c55e', description: 'Famous investor/trader strategy agents' },
  { id: 'analysis', label: 'Analysis Agents', color: '#8b5cf6', description: 'Specialized analytical agents' },
  { id: 'data', label: 'Data Agents', color: '#f59e0b', description: 'Data collection and processing agents' },
  { id: 'execution', label: 'Execution Agents', color: '#10b981', description: 'Signal generation and portfolio management' },
  { id: 'monitoring', label: 'Monitoring Agents', color: '#ef4444', description: 'Alert and performance monitoring' },
];

// Helper function to get agent config by ID
export function getAgentConfig(agentId: string): AgentConfig | undefined {
  return AGENT_CONFIGS.find(config => config.id === agentId);
}

// Helper function to get agents by category
export function getAgentsByCategory(category: AgentCategory): AgentConfig[] {
  return AGENT_CONFIGS.filter(config => config.category === category);
}
