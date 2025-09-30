import { DocSection } from '../types';
import { LayoutGrid } from 'lucide-react';

export const tabsReferenceDoc: DocSection = {
  id: 'tabs',
  title: 'Tabs Reference',
  icon: LayoutGrid,
  subsections: [
    {
      id: 'dashboard',
      title: 'Dashboard Tab',
      content: `The Dashboard provides an overview of key market information and indices.

**Features:**
• Real-time market indices (S&P 500, NASDAQ, Dow Jones, etc.)
• Global indices tracking (Asia, Europe, US)
• Economic indicators display
• Sector performance overview
• Market breadth indicators
• Key statistics at a glance

**Data Updates:**
Real-time updates during market hours with 1-second refresh rate.

**Keyboard Shortcut:** F1`,
      codeExample: `// Dashboard shows:
- S&P 500: 4,850.23 (+1.2%)
- NASDAQ: 15,234.56 (+1.8%)
- DOW: 38,456.78 (+0.9%)
- Russell 2000: 2,050.34 (+2.1%)

// Global Markets:
- Nikkei 225: 38,234.56 (+0.5%)
- FTSE 100: 7,654.32 (-0.3%)
- DAX: 17,123.45 (+0.8%)

// Economic Indicators:
- US 10Y Treasury: 4.25%
- VIX: 13.45
- USD Index: 103.25`
    },
    {
      id: 'markets',
      title: 'Markets Tab',
      content: `Real-time market data organized by asset classes and regions.

**Asset Categories:**
• Stocks - Global equity markets
• Forex - Currency pairs
• Commodities - Gold, Oil, Agricultural products
• Bonds - Government and corporate bonds
• ETFs - Exchange-traded funds
• Cryptocurrency - Digital assets

**Regional Coverage:**
• United States markets
• European markets
• Asian markets (China, Japan, India, etc.)
• Emerging markets

**Features:**
• Live price quotes
• Intraday charts
• Volume analysis
• Market depth (Level 2 data)
• Pre-market and after-hours data

**Keyboard Shortcut:** F2`,
      codeExample: `// Market data access:
// Stocks: AAPL, MSFT, GOOGL, TSLA, NVDA
// Forex: EUR/USD, GBP/USD, USD/JPY
// Commodities: GC (Gold), CL (Oil)
// Crypto: BTC/USD, ETH/USD

// Data includes:
- Last price
- Bid/Ask spread
- Volume
- Day high/low
- 52-week high/low
- Market cap
- P/E ratio`
    },
    {
      id: 'news',
      title: 'News Tab',
      content: `Real-time financial news aggregation and analysis.

**News Sources:**
• Bloomberg
• Reuters
• CNBC
• Wall Street Journal
• Financial Times
• Market-specific news feeds

**News Categories:**
• Markets - General market news
• Earnings - Company earnings reports
• M&A - Mergers and acquisitions
• Economic - Economic data releases
• Geopolitics - Global political events
• Regulatory - Regulatory changes
• Technical - Technical analysis insights

**Features:**
• Real-time news feed
• Sentiment analysis (Bullish/Bearish/Neutral)
• Impact rating (High/Medium/Low)
• Ticker association
• News filtering by priority and category
• Search and keyword alerts

**News Priority Levels:**
• FLASH - Breaking market-moving news
• URGENT - High-priority updates
• BREAKING - Important developments
• ROUTINE - Regular news flow

**Keyboard Shortcut:** F3`,
      codeExample: `// Example news items:
{
  time: "16:47:23",
  priority: "FLASH",
  headline: "Fed Signals Rate Cut",
  summary: "Fed indicates 75bps cut...",
  sentiment: "BULLISH",
  impact: "HIGH",
  tickers: ["SPY", "QQQ", "TLT"]
}

// News filtering:
- By category (Markets, Earnings, etc.)
- By priority level
- By affected tickers
- By sentiment
- By time range`
    },
    {
      id: 'forum',
      title: 'Forum Tab',
      content: `Community discussion platform for traders and investors.

**Forum Features:**
• Discussion threads on stocks, crypto, and strategies
• User reputation system
• Post voting and ranking
• Category organization
• Real-time online user count
• Verified user badges

**Post Categories:**
• Markets - General market discussion
• Stocks - Individual stock analysis
• Crypto - Cryptocurrency discussion
• Options - Options trading strategies
• Forex - Currency trading
• Technical Analysis - Chart analysis
• Fundamental Analysis - Company analysis
• Strategy - Trading strategies

**Post Types:**
• Analysis posts with detailed research
• Questions and answers
• Trade ideas and setups
• News discussion
• Educational content

**User Features:**
• Create and reply to posts
• Like and bookmark posts
• Follow other users
• Track post analytics (views, replies, likes)

**Keyboard Shortcut:** Ctrl+F`,
      codeExample: `// Forum post structure:
{
  author: "QuantTrader_Pro",
  title: "Fed Rate Analysis",
  category: "MARKETS",
  tags: ["FED", "RATES", "ANALYSIS"],
  views: 8924,
  replies: 156,
  likes: 342,
  sentiment: "BULLISH",
  verified: true
}

// Sorting options:
- Latest posts
- Most popular
- Most discussed
- Trending topics
- Hot topics`
    },
    {
      id: 'portfolio',
      title: 'Portfolio Tab',
      content: `Track and manage your investment portfolio.

**Portfolio Features:**
• Real-time position tracking
• P&L calculation (realized and unrealized)
• Cost basis tracking
• Performance metrics
• Sector allocation
• Asset allocation
• Risk metrics

**Position Information:**
• Ticker symbol
• Quantity held
• Average purchase price
• Current market price
• Market value
• Unrealized P&L ($ and %)
• Day change ($ and %)
• Portfolio weight
• Sector classification

**Portfolio Analytics:**
• Total portfolio value
• Total gain/loss
• Today's change
• Sector diversification
• Position concentration
• Risk-adjusted returns

**Performance Metrics:**
• Total return (%)
• Sharpe ratio
• Maximum drawdown
• Beta
• Alpha

**Keyboard Shortcut:** F4`,
      codeExample: `// Portfolio position example:
{
  ticker: "AAPL",
  name: "Apple Inc.",
  quantity: 250,
  avgPrice: 165.50,
  currentPrice: 184.92,
  marketValue: 46230.00,
  unrealizedPnL: 4855.00,
  unrealizedPnLPercent: 11.73,
  dayChange: 462.30,
  weight: 28.5,
  sector: "Technology"
}

// Portfolio summary:
Total Value: $162,345.67
Total Cost: $148,000.00
Unrealized P&L: +$14,345.67 (+9.69%)
Today's Change: +$1,234.56 (+0.76%)`
    },
    {
      id: 'watchlist',
      title: 'Watchlist Tab',
      content: `Monitor and track securities of interest.

**Watchlist Features:**
• Multiple watchlist support
• Real-time quote updates
• Quick add/remove securities
• Custom sorting and filtering
• Price alerts
• Technical indicators display

**Data Displayed:**
• Last price
• Change ($ and %)
• Volume
• Day high/low
• Open price
• Previous close
• Market cap
• P/E ratio
• Dividend yield
• 52-week high/low
• Average volume

**Watchlist Types:**
• Primary - Main watchlist
• Earnings - Stocks with upcoming earnings
• Breakouts - Technical breakout candidates
• Dividends - Dividend-paying stocks
• Custom lists

**Actions:**
• Add to watchlist from any tab
• Remove from watchlist
• View detailed quote
• Open chart
• Set price alert
• Trade directly

**Keyboard Shortcut:** F6`,
      codeExample: `// Watchlist entry:
{
  ticker: "NVDA",
  name: "NVIDIA Corporation",
  price: 501.23,
  change: 5.12,
  changePercent: 1.03,
  volume: 35678901,
  marketCap: "1.24T",
  pe: 67.2,
  week52High: 502.66,
  week52Low: 108.13
}

// Watchlist operations:
- Add symbol: watchlist.add("TSLA")
- Remove symbol: watchlist.remove("AAPL")
- Sort by: change%, volume, price
- Filter by: sector, market cap, performance`
    },
    {
      id: 'screener',
      title: 'Screener Tab',
      content: `Discover stocks based on custom criteria and filters.

**Screening Criteria:**

**Fundamental Filters:**
• Market Cap (min/max)
• P/E Ratio (min/max)
• P/S Ratio (min/max)
• P/B Ratio (min/max)
• EPS (min/max)
• Dividend Yield (min/max)
• Revenue Growth (min/max)
• Profit Margin (min/max)
• ROE (min/max)
• ROA (min/max)
• Debt-to-Equity (min/max)

**Technical Filters:**
• Price (min/max)
• Volume (min/max)
• Price Change % (min/max)
• RSI (overbought/oversold)
• Moving average crossovers
• 52-week high/low proximity
• Beta (volatility)

**Sector Filters:**
• Technology
• Healthcare
• Financials
• Consumer
• Energy
• Industrials
• Materials
• Utilities

**Pre-built Screens:**
• Value stocks (low P/E, high dividend)
• Growth stocks (high revenue growth)
• Momentum stocks (strong price action)
• Dividend aristocrats
• Small cap opportunities

**Keyboard Shortcut:** F7`,
      codeExample: `// Screener example:
filters = {
  marketCapMin: "1B",
  peMax: 20,
  dividendMin: 2.0,
  volumeMin: "1M",
  sector: "Technology"
}

// Results show stocks matching:
- Market cap > $1 billion
- P/E ratio < 20
- Dividend yield > 2%
- Volume > 1 million shares
- Technology sector

// Export results:
- CSV export
- Save custom screen
- Set alerts on criteria`
    },
    {
      id: 'alerts',
      title: 'Alerts Tab',
      content: `Create and manage price alerts and notifications.

**Alert Types:**
• Price Alerts - Trigger when price crosses a level
• Volume Alerts - Trigger on volume spikes
• Indicator Alerts - Based on technical indicators
• News Alerts - Keyword-based news alerts
• Earnings Alerts - Upcoming earnings notifications

**Alert Conditions:**
• Price above/below threshold
• Price crosses moving average
• RSI overbought/oversold
• Volume exceeds average
• News contains keywords
• Percentage change exceeds threshold

**Alert Management:**
• Create new alerts
• Edit existing alerts
• Enable/disable alerts
• Delete alerts
• View alert history
• Alert statistics

**Notification Options:**
• Desktop notifications
• Email notifications
• In-app notifications
• Sound alerts
• SMS (premium feature)

**Alert Status:**
• ACTIVE - Alert is monitoring
• TRIGGERED - Alert condition met
• EXPIRED - Time-based alert expired
• DISABLED - Alert temporarily disabled

**Keyboard Shortcut:** F8`,
      codeExample: `// Price alert example:
{
  type: "PRICE",
  symbol: "AAPL",
  condition: "Price Above",
  trigger: "$180.00",
  status: "ACTIVE",
  priority: "HIGH",
  notification: ["desktop", "email"]
}

// Indicator alert:
{
  type: "INDICATOR",
  symbol: "TSLA",
  condition: "RSI Below",
  trigger: "30",
  status: "ACTIVE"
}

// Volume alert:
{
  type: "VOLUME",
  symbol: "NVDA",
  condition: "Volume Above",
  trigger: "50M shares",
  status: "ACTIVE"
}`
    },
    {
      id: 'chat',
      title: 'AI Chat Tab',
      content: `Interact with AI assistant for market analysis and strategy development.

**AI Capabilities:**
• Market analysis and insights
• Trading strategy suggestions
• Technical analysis interpretation
• Fundamental analysis assistance
• Code generation (FinScript)
• Backtesting advice
• Risk management guidance
• Portfolio optimization suggestions

**Chat Features:**
• Multi-session support
• Conversation history
• Code highlighting
• Chart generation
• Real-time market data access
• Strategy backtesting
• Educational content

**AI Commands:**
• /analyze [symbol] - Analyze a stock
• /strategy [description] - Generate strategy
• /backtest [strategy] - Backtest a strategy
• /indicators [symbol] - Show technical indicators
• /news [symbol] - Latest news summary
• /compare [symbols] - Compare multiple stocks

**Use Cases:**
• "Analyze AAPL's technical setup"
• "Create a momentum strategy for tech stocks"
• "Explain this earnings report"
• "What's driving the market today?"
• "Help me optimize my portfolio"

**Keyboard Shortcut:** F9`,
      codeExample: `// Example chat interactions:

User: "Analyze NVDA's technical setup"
AI: "NVDA is showing strong momentum:
- Price above all key moving averages
- RSI at 68 (approaching overbought)
- Volume 20% above average
- Breaking out of ascending triangle
- Next resistance: $510"

User: "Generate a momentum strategy"
AI: "Here's a momentum strategy:

fast = ema(symbol, 12)
slow = ema(symbol, 26)
volume_avg = sma(volume(symbol), 20)

if fast > slow and volume > volume_avg * 1.5 {
    buy 'Momentum breakout with volume'
}

plot fast, 'Fast EMA'
plot slow, 'Slow EMA'"

User: "What's the risk-reward on this trade?"
AI: "Based on support/resistance:
- Entry: $495
- Stop loss: $485 (-2%)
- Target: $520 (+5%)
- Risk-Reward: 1:2.5"`
    },
    {
      id: 'code-editor',
      title: 'Code Editor Tab',
      content: `Write, test, and execute FinScript trading strategies.

**Editor Features:**
• Syntax highlighting for FinScript
• Auto-completion
• Error checking
• Multiple file support
• Code snippets
• Find and replace
• Comment/uncomment
• Auto-save

**Execution:**
• Run strategy on historical data
• View execution output
• Chart generation
• Signal visualization
• Performance metrics
• Error reporting

**File Operations:**
• Create new file
• Open existing file
• Save file
• Export strategy
• Import strategy
• Version control integration

**Output Panel:**
• Execution logs
• Buy/sell signals generated
• Performance statistics
• Error messages
• Chart previews

**Keyboard Shortcuts:**
• Ctrl+R - Run code
• Ctrl+S - Save file
• Ctrl+/ - Comment line
• Ctrl+F - Find
• Ctrl+N - New file

**Shortcut:** Shift+F5`,
      codeExample: `// FinScript editor example:
// File: momentum_strategy.fincept

// Define parameters
symbol = AAPL
fast_period = 12
slow_period = 26

// Calculate indicators
fast_ema = ema(symbol, fast_period)
slow_ema = ema(symbol, slow_period)
rsi_value = rsi(symbol, 14)

// Trading logic
if fast_ema > slow_ema and rsi_value > 50 {
    buy "Bullish momentum detected"
}

if fast_ema < slow_ema or rsi_value < 40 {
    sell "Momentum weakening"
}

// Visualization
plot fast_ema, "Fast EMA"
plot slow_ema, "Slow EMA"
plot_candlestick symbol, "Price Chart"

// Output:
// Signals generated: 15 buy, 12 sell
// Win rate: 67%
// Total return: +23.5%`
    },
    {
      id: 'node-editor',
      title: 'Node Editor / Workflow Tab',
      content: `Visual programming for building complex data pipelines and workflows.

**Node Types:**
• Data Source Nodes - Import data
• Transformation Nodes - Process data
• Analysis Nodes - Perform analysis
• Visualization Nodes - Create charts
• Agent Nodes - AI-powered analysis
• Output Nodes - Export results

**Node Editor Features:**
• Drag-and-drop interface
• Visual connection system
• Real-time data flow
• Node configuration panels
• Save/load workflows
• Template workflows

**Use Cases:**
• Build data pipelines
• Create custom indicators
• Automate analysis workflows
• Combine multiple data sources
• Deploy automated strategies

**Workflow Operations:**
• Add nodes from palette
• Connect nodes with edges
• Configure node parameters
• Execute workflow
• Debug data flow
• Save workflow template

**Available Nodes:**
• Yahoo Finance - Market data
• Alpha Vantage - Historical data
• News API - News data
• Moving Average - Technical indicator
• RSI Calculator - Momentum indicator
• Signal Generator - Buy/sell signals
• Chart Generator - Visualization
• Alert Sender - Notifications

**Shortcut:** Ctrl+Shift+N`,
      codeExample: `// Workflow example:
[Yahoo Finance Node]
    ↓ (AAPL data)
[EMA Calculator] → [Chart Node]
    ↓ (EMA values)
[Signal Generator]
    ↓ (buy/sell signals)
[Alert Sender]

// Node configuration:
Yahoo Finance Node:
- Symbol: AAPL
- Timeframe: 1D
- Period: 90 days

EMA Calculator:
- Period: 21
- Source: Close price

Signal Generator:
- Buy condition: EMA > SMA
- Sell condition: EMA < SMA`
    },
    {
      id: 'data-sources',
      title: 'Data Sources Tab',
      content: `Connect to external data sources and APIs.

**Data Source Categories:**
• Databases - PostgreSQL, MySQL, MongoDB
• APIs - REST, GraphQL, WebSocket
• Files - CSV, JSON, Excel
• Streaming - Real-time data feeds
• Cloud Storage - S3, Google Cloud
• Time-series - InfluxDB, TimescaleDB
• Market Data - Bloomberg, Reuters, IEX

**Supported Data Sources:**
• Yahoo Finance - Free market data
• Alpha Vantage - Stock data API
• IEX Cloud - Real-time market data
• Polygon.io - Market data
• Twelve Data - Financial API
• PostgreSQL - Database
• MongoDB - NoSQL database
• Redis - In-memory database
• Kafka - Streaming platform
• CSV Files - Local files

**Connection Management:**
• Add new data source
• Configure connection parameters
• Test connection
• Edit existing connections
• Delete connections
• Connection status monitoring

**Data Source Operations:**
• Query data
• Test queries
• Schedule data fetches
• Cache management
• Error handling
• Rate limiting

**Keyboard Shortcut:** Ctrl+D`,
      codeExample: `// Data source configuration:

// Yahoo Finance:
{
  name: "Yahoo Finance",
  type: "yahoo-finance",
  config: {
    symbols: ["AAPL", "MSFT", "GOOGL"],
    interval: "1d",
    period: "90d"
  }
}

// PostgreSQL:
{
  name: "Market Database",
  type: "postgresql",
  config: {
    host: "localhost",
    port: 5432,
    database: "market_data",
    user: "trader",
    password: "***"
  }
}

// REST API:
{
  name: "Alpha Vantage",
  type: "rest-api",
  config: {
    baseUrl: "https://alphavantage.co",
    apiKey: "YOUR_API_KEY",
    rateLimit: 5
  }
}`
    },
    {
      id: 'agents',
      title: 'Agents Tab',
      content: `Deploy AI agents for automated trading and monitoring.

**Agent Categories:**

**Hedge Fund Managers:**
• Renaissance Technologies Style - Quantitative strategies
• Bridgewater Associates Style - Macro strategies
• Citadel Style - Multi-strategy approach
• Two Sigma Style - Data-driven analysis

**Trader Styles:**
• Warren Buffett - Value investing
• Ray Dalio - All Weather Portfolio
• Peter Lynch - Growth at reasonable price
• George Soros - Macro trading
• Jim Simons - Quantitative trading

**Analysis Agents:**
• Technical Analysis - Chart patterns and indicators
• Fundamental Analysis - Company financials
• Sentiment Analysis - News and social media
• Risk Analysis - Portfolio risk metrics
• Market Analysis - Macro trends

**Data Agents:**
• Market Data Fetcher - Real-time quotes
• News Aggregator - News collection
• Social Media Monitor - Social sentiment
• Economic Data - Economic indicators

**Execution Agents:**
• Signal Generator - Trading signals
• Order Manager - Order execution
• Risk Manager - Risk control
• Portfolio Rebalancer - Portfolio optimization

**Features:**
• Drag-and-drop agent creation
• Connect agents to data sources
• Configure agent parameters
• Monitor agent status
• View agent output
• 24/7 automated execution

**Dynamic Connections:**
• Unlimited input connections
• Unlimited output connections
• Visual connection badges
• Hover to expand handles
• Real-time connection tracking

**Keyboard Shortcut:** Ctrl+Shift+A`,
      codeExample: `// Agent workflow example:

[Market Data] → [Warren Buffett Agent] → [Portfolio 1]
[News Data]   ↗                        ↘ [Portfolio 2]
                                        ↘ [Portfolio 3]

// Agent configuration:
Warren Buffett Agent:
  Parameters:
    - moat_focus: true
    - min_roe: 15%
    - max_pe: 25
    - min_margin: 20%
    - holding_period: "long-term"

  Inputs: 2 data sources
  Outputs: 3 portfolios
  Status: Running

// Agent can monitor 24/7:
- Scan for undervalued stocks
- Analyze company fundamentals
- Generate buy recommendations
- Rebalance portfolio allocations
- Send alerts on opportunities`
    },
    {
      id: 'settings',
      title: 'Settings Tab',
      content: `Configure terminal preferences and account settings.

**General Settings:**
• Theme selection (Dark/Light/Bloomberg)
• Language preferences
• Time zone settings
• Date format
• Number format
• Currency display

**Editor Settings:**
• Font family and size
• Tab size (spaces)
• Auto-save interval
• Syntax highlighting
• Code completion
• Line numbers

**Data Settings:**
• Data refresh rate
• Auto-update quotes
• Historical data period
• Cache settings
• API rate limits
• Data provider selection

**Notification Settings:**
• Desktop notifications
• Sound alerts
• Email notifications
• SMS alerts (premium)
• Alert frequency
• Notification priority

**Account Settings:**
• User profile
• Subscription plan
• API keys management
• Connected brokers
• Data permissions
• Privacy settings

**Display Settings:**
• Chart color schemes
• Grid line preferences
• Crosshair settings
• Font scaling
• Panel layouts
• Workspace arrangements

**Keyboard Shortcut:** F10`,
      codeExample: `// Settings configuration:
{
  general: {
    theme: "bloomberg-dark",
    language: "en",
    timezone: "America/New_York",
    dateFormat: "MM/DD/YYYY"
  },
  editor: {
    fontSize: 14,
    fontFamily: "Consolas",
    tabSize: 4,
    autoSave: true
  },
  data: {
    refreshRate: 1000,
    autoUpdate: true,
    cacheEnabled: true,
    provider: "yahoo-finance"
  },
  notifications: {
    desktop: true,
    sound: true,
    email: false,
    priority: "high"
  }
}`
    }
  ]
};
