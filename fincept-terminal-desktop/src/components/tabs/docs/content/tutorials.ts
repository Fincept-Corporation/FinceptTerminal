import { DocSection } from '../types';
import { BookOpen } from 'lucide-react';

export const tutorialsDoc: DocSection = {
  id: 'tutorials',
  title: 'Tutorials',
  icon: BookOpen,
  subsections: [
    {
      id: 'first-strategy',
      title: 'Build Your First Strategy',
      content: `Let's build a simple moving average crossover strategy from scratch:

**Step 1:** Define your indicators
Calculate fast and slow moving averages

**Step 2:** Create trading logic
Buy when fast MA crosses above slow MA
Sell when fast MA crosses below slow MA

**Step 3:** Add visualization
Plot your indicators on a chart

**Step 4:** Run and test
Execute your strategy and analyze results

**Step 5:** Optimize
Adjust parameters to improve performance

This is one of the most popular strategies used by traders worldwide!`,
      codeExample: `// Complete MA Crossover Strategy
symbol = AAPL

// Step 1: Define indicators
fast_ma = ema(symbol, 12)
slow_ma = ema(symbol, 26)

// Step 2: Trading logic
if fast_ma > slow_ma {
    buy "Bullish crossover - Fast MA above Slow MA"
}

if fast_ma < slow_ma {
    sell "Bearish crossover - Fast MA below Slow MA"
}

// Step 3: Visualization
plot fast_ma, "Fast EMA (12)"
plot slow_ma, "Slow EMA (26)"
plot_candlestick symbol, "Apple Stock"

// Done! Press Ctrl+R to run`
    },
    {
      id: 'advanced-strategy',
      title: 'Advanced: Multi-Indicator Strategy',
      content: `Build a sophisticated strategy combining multiple indicators:

**Indicators Used:**
• EMA (12, 26) for trend
• RSI (14) for momentum
• Bollinger Bands for volatility
• Volume for confirmation

**Strategy Rules:**
• Buy: Bullish trend + oversold + high volume
• Sell: Bearish trend + overbought OR stop loss

**Risk Management:**
• Position sizing based on volatility
• Stop loss at 2% below entry
• Take profit at 5% above entry
• Maximum 3 concurrent positions

This demonstrates how to combine multiple signals for robust trading decisions.`,
      codeExample: `// Advanced Multi-Indicator Strategy
symbol = TSLA

// Indicators
ema_fast = ema(symbol, 12)
ema_slow = ema(symbol, 26)
rsi_val = rsi(symbol, 14)
vol = volume(symbol)
avg_vol = sma(vol, 20)
bb_upper, bb_middle, bb_lower = bollinger(symbol, 20, 2)

// Buy conditions (all must be true)
trend_bullish = ema_fast > ema_slow
oversold = rsi_val < 30
volume_spike = vol > avg_vol * 1.5
price_near_lower_bb = close(symbol) < bb_middle

if trend_bullish and oversold and volume_spike {
    buy "Strong buy: Bullish trend + oversold + volume"
}

// Sell conditions (any can trigger)
trend_bearish = ema_fast < ema_slow
overbought = rsi_val > 70
price_at_upper_bb = close(symbol) > bb_upper

if trend_bearish or overbought or price_at_upper_bb {
    sell "Exit: Reversal signal detected"
}

// Visualize everything
plot ema_fast, "EMA 12"
plot ema_slow, "EMA 26"
plot rsi_val, "RSI"
plot bb_upper, "BB Upper"
plot bb_lower, "BB Lower"`
    },
    {
      id: 'portfolio-tutorial',
      title: 'Managing Your Portfolio',
      content: `Learn to track and manage your investment portfolio:

**Step 1: Add Positions**
Enter your current holdings with purchase details

**Step 2: Monitor Performance**
Track real-time P&L and portfolio value

**Step 3: Analyze Allocation**
Review sector and asset allocation

**Step 4: Set Alerts**
Create alerts for portfolio events

**Step 5: Rebalance**
Adjust positions based on target allocation

**Best Practices:**
• Diversify across sectors (aim for 5-10 sectors)
• Limit individual position size (max 10-15% each)
• Review portfolio weekly
• Rebalance quarterly
• Monitor correlation between holdings
• Track risk-adjusted returns (Sharpe ratio)`,
      codeExample: `// Portfolio management workflow:

// 1. Add positions
portfolio.add({
  ticker: "AAPL",
  quantity: 100,
  avgPrice: 150.00
});

// 2. View portfolio summary
portfolio.summary();
// Output:
// Total Value: $162,345.67
// Total P&L: +$14,345.67 (+9.69%)
// Number of positions: 8
// Top performer: NVDA (+23.5%)

// 3. Check allocation
portfolio.allocation();
// Technology: 45%
// Healthcare: 20%
// Financials: 15%
// Consumer: 12%
// Energy: 8%

// 4. Identify rebalancing needs
portfolio.rebalance_suggestions();
// Overweight: Technology (target: 35%)
// Underweight: Healthcare (target: 25%)

// 5. Set portfolio alerts
alert.create({
  type: "portfolio_value",
  condition: "below",
  threshold: 150000
});`
    },
    {
      id: 'watchlist-tutorial',
      title: 'Using Watchlists Effectively',
      content: `Master watchlist management for efficient market monitoring:

**Creating Effective Watchlists:**

**By Strategy:**
• Momentum - Stocks with strong price action
• Value - Undervalued stocks
• Dividend - High-yielding stocks
• Earnings - Upcoming earnings plays

**By Sector:**
• Technology watchlist
• Healthcare watchlist
• Financial watchlist

**By Event:**
• Earnings this week
• Ex-dividend dates
• Technical breakouts

**Watchlist Workflow:**
1. Scan for candidates using Screener
2. Add to appropriate watchlist
3. Monitor for entry signals
4. Set price alerts
5. Execute trades when conditions met
6. Move to Portfolio or remove

**Tips:**
• Keep watchlists focused (10-20 stocks each)
• Review and update weekly
• Use multiple watchlists for organization
• Set alerts on all watchlist stocks`,
      codeExample: `// Watchlist management:

// Create specialized watchlists
watchlist.create("Tech Momentum");
watchlist.create("Value Plays");
watchlist.create("Earnings This Week");

// Add stocks with criteria
watchlist.add("Tech Momentum", "NVDA", {
  alert: {
    type: "price",
    trigger: 510,
    condition: "above"
  },
  notes: "Breaking out of triangle"
});

// Bulk add from screener results
screener_results = screen({
  marketCap: "1B+",
  peRatio: "< 20",
  dividendYield: "> 3%"
});

watchlist.add_multiple("Value Plays", screener_results);

// Review watchlist
watchlist.show("Tech Momentum");
// NVDA: $501.23 (+1.03%)
// AAPL: $184.92 (+1.02%)
// MSFT: $378.45 (+1.02%)
// GOOGL: $134.56 (+0.76%)

// Set alerts on entire watchlist
watchlist.set_alerts("Tech Momentum", {
  type: "price_change",
  threshold: 3,
  period: "day"
});`
    },
    {
      id: 'alerts-tutorial',
      title: 'Setting Up Smart Alerts',
      content: `Create effective alerts to never miss trading opportunities:

**Alert Types & Use Cases:**

**Price Alerts:**
• Breakout levels
• Support/resistance
• Round numbers
• All-time highs

**Technical Alerts:**
• RSI overbought/oversold
• MA crossovers
• MACD signals
• Volume spikes

**News Alerts:**
• Earnings announcements
• FDA approvals (healthcare)
• Merger rumors
• Analyst upgrades/downgrades

**Volume Alerts:**
• Unusual volume (3x average)
• End-of-day volume surge
• Dark pool activity

**Best Practices:**
• Use multiple confirmation alerts
• Set both entry and exit alerts
• Include stop-loss alerts
• Adjust alerts as market changes
• Review triggered alerts regularly`,
      codeExample: `// Smart alert examples:

// Breakout alert
alert.create({
  symbol: "AAPL",
  type: "price",
  condition: "above",
  level: 185.00,
  message: "AAPL breaking out above $185",
  notifications: ["desktop", "email"]
});

// RSI oversold alert
alert.create({
  symbol: "TSLA",
  type: "indicator",
  indicator: "rsi",
  period: 14,
  condition: "below",
  level: 30,
  message: "TSLA RSI oversold"
});

// Volume spike alert
alert.create({
  symbol: "NVDA",
  type: "volume",
  condition: "above",
  multiplier: 2.0, // 2x average volume
  message: "NVDA unusual volume"
});

// Earnings alert
alert.create({
  symbol: "GOOGL",
  type: "event",
  event: "earnings",
  advance_notice: "1 day",
  message: "GOOGL earnings tomorrow"
});

// Combined multi-condition alert
alert.create({
  symbol: "MSFT",
  type: "multi",
  conditions: [
    { type: "price", above: 380 },
    { type: "rsi", above: 50 },
    { type: "volume", multiplier: 1.5 }
  ],
  logic: "all", // all conditions must be met
  message: "MSFT strong buy signal"
});`
    },
    {
      id: 'agent-workflow',
      title: 'Building Agent Workflows',
      content: `Create automated agent workflows for 24/7 market monitoring:

**Workflow Components:**
• Data Sources - Market data feeds
• Agents - Analysis and decision making
• Connections - Data flow between nodes
• Outputs - Alerts, trades, reports

**Example Workflows:**

**Workflow 1: Multi-Source Analysis**
Yahoo Finance → Warren Buffett Agent → Portfolio Alerts
Alpha Vantage →           ↑
News Feed     →           ↑

**Workflow 2: Diversified Strategies**
Market Data → Momentum Agent → Aggressive Portfolio
           → Value Agent    → Conservative Portfolio
           → Risk Agent     → Balanced Portfolio

**Workflow 3: News-Driven Trading**
News Feed → Sentiment Agent → Signal Generator → Trade Executor
                             → Alert Manager

**Best Practices:**
• Start with simple workflows
• Test with paper trading first
• Monitor agent performance
• Use risk management agents
• Log all agent decisions
• Review and optimize regularly`,
      codeExample: `// Agent workflow configuration:

// 1. Create data source node
data_source = create_node({
  type: "data_source",
  source: "yahoo_finance",
  symbols: ["AAPL", "MSFT", "GOOGL"],
  interval: "1m"
});

// 2. Create analysis agent
agent = create_node({
  type: "agent",
  style: "warren_buffett",
  parameters: {
    min_roe: 15,
    max_pe: 25,
    moat_focus: true
  }
});

// 3. Create output nodes
portfolio = create_node({
  type: "output",
  action: "rebalance_portfolio",
  target_allocation: {
    "AAPL": 0.35,
    "MSFT": 0.35,
    "GOOGL": 0.30
  }
});

alerts = create_node({
  type: "output",
  action: "send_alerts",
  channels: ["email", "desktop"]
});

// 4. Connect nodes
connect(data_source, agent, "input-0");
connect(agent, portfolio, "output-0");
connect(agent, alerts, "output-1");

// 5. Start workflow
workflow.start();

// Monitor workflow
workflow.status();
// Running: ✓
// Nodes: 4
// Connections: 3
// Last execution: 10s ago
// Signals generated: 2 buy, 0 sell`
    }
  ]
};
