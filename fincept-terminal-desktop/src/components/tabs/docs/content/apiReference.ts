import { DocSection } from '../types';
import { Database } from 'lucide-react';

export const apiReferenceDoc: DocSection = {
  id: 'api',
  title: 'API Reference',
  icon: Database,
  subsections: [
    {
      id: 'data-api',
      title: 'Data API',
      content: `Access real-time and historical market data through our unified API:

**Endpoints:**
• GET /api/v1/quote/{symbol} - Get latest quote
• GET /api/v1/historical/{symbol} - Historical OHLCV data
• GET /api/v1/indicators/{symbol} - Calculated indicators
• POST /api/v1/backtest - Run strategy backtest
• GET /api/v1/news/{symbol} - Latest news for symbol
• GET /api/v1/fundamentals/{symbol} - Company fundamentals

**Authentication:**
Use API keys in the X-API-Key header

**Rate Limits:**
• Free tier: 100 requests/hour
• Starter: 500 requests/hour
• Professional: 2,000 requests/hour
• Enterprise: 10,000 requests/hour
• Unlimited: No limits

**Response Format:**
All responses are in JSON format with standard structure:
{
  "success": true,
  "data": {...},
  "timestamp": "2025-01-15T10:30:00Z"
}

**Error Handling:**
Errors return appropriate HTTP status codes and error messages.`,
      codeExample: `// Example API calls
// Get real-time quote
fetch('https://api.fincept.com/v1/quote/AAPL', {
  headers: {
    'X-API-Key': 'your_api_key_here'
  }
})
.then(res => res.json())
.then(data => console.log(data));

// Get historical data
fetch('https://api.fincept.com/v1/historical/AAPL?period=1y', {
  headers: {
    'X-API-Key': 'your_api_key_here'
  }
})
.then(res => res.json())
.then(data => console.log(data));

// Response example:
{
  "success": true,
  "data": {
    "symbol": "AAPL",
    "price": 184.92,
    "change": 1.87,
    "changePercent": 1.02,
    "volume": 48234567,
    "high": 186.45,
    "low": 182.30
  }
}`
    },
    {
      id: 'websocket',
      title: 'WebSocket Streaming',
      content: `Real-time data streaming via WebSocket for live market data:

**Connection:**
wss://stream.fincept.com/v1/quotes

**Subscribe to symbols:**
Send JSON: {"action": "subscribe", "symbols": ["AAPL", "TSLA"]}

**Unsubscribe:**
Send JSON: {"action": "unsubscribe", "symbols": ["AAPL"]}

**Message Format:**
Receive real-time tick data as JSON

**Features:**
• Sub-millisecond latency
• Level 2 order book data
• Trade execution updates
• Market depth information
• Automatic reconnection
• Heartbeat messages

**Data Types:**
• Quotes - Real-time bid/ask
• Trades - Executed trades
• Bars - OHLCV aggregated bars
• Book - Order book depth

**Authentication:**
Include API key in connection URL or initial message`,
      codeExample: `// WebSocket connection example
const ws = new WebSocket('wss://stream.fincept.com/v1/quotes');

ws.onopen = () => {
  // Subscribe to symbols
  ws.send(JSON.stringify({
    action: 'subscribe',
    symbols: ['AAPL', 'TSLA', 'GOOGL'],
    apiKey: 'your_api_key_here'
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Live quote:', data);

  // Data format:
  // {
  //   symbol: "AAPL",
  //   price: 184.92,
  //   volume: 1000,
  //   timestamp: "2025-01-15T10:30:00.123Z"
  // }
};

ws.onerror = (error) => {
  console.error('WebSocket error:', error);
};

ws.onclose = () => {
  console.log('Connection closed');
};`
    },
    {
      id: 'indicators-api',
      title: 'Indicators API',
      content: `Calculate technical indicators via API:

**Available Indicators:**
• Moving Averages: SMA, EMA, WMA, VWAP
• Momentum: RSI, MACD, Stochastic, CCI
• Volatility: ATR, Bollinger Bands, Standard Deviation
• Volume: OBV, MFI, A/D Line
• Trend: ADX, Aroon, Parabolic SAR

**Endpoint:**
GET /api/v1/indicators/{symbol}

**Parameters:**
• symbol - Stock ticker (required)
• indicator - Indicator name (required)
• period - Time period (required)
• interval - Data interval (optional, default: 1d)

**Example Requests:**
• /api/v1/indicators/AAPL?indicator=ema&period=21
• /api/v1/indicators/TSLA?indicator=rsi&period=14
• /api/v1/indicators/NVDA?indicator=macd&fast=12&slow=26&signal=9`,
      codeExample: `// Calculate EMA
fetch('https://api.fincept.com/v1/indicators/AAPL?indicator=ema&period=21', {
  headers: { 'X-API-Key': 'your_api_key' }
})
.then(res => res.json())
.then(data => {
  console.log('EMA values:', data.values);
});

// Calculate RSI
fetch('https://api.fincept.com/v1/indicators/TSLA?indicator=rsi&period=14', {
  headers: { 'X-API-Key': 'your_api_key' }
})
.then(res => res.json())
.then(data => {
  console.log('RSI:', data.current);
  console.log('Overbought:', data.current > 70);
  console.log('Oversold:', data.current < 30);
});

// Response format:
{
  "success": true,
  "data": {
    "symbol": "AAPL",
    "indicator": "ema",
    "period": 21,
    "current": 184.56,
    "values": [180.23, 181.45, ..., 184.56]
  }
}`
    },
    {
      id: 'backtesting-api',
      title: 'Backtesting API',
      content: `Test trading strategies on historical data:

**Endpoint:**
POST /api/v1/backtest

**Request Body:**
{
  "symbol": "AAPL",
  "strategy": "ema_crossover",
  "start_date": "2024-01-01",
  "end_date": "2024-12-31",
  "initial_capital": 10000,
  "parameters": {
    "fast_period": 12,
    "slow_period": 26
  }
}

**Response Metrics:**
• Total return (%)
• Sharpe ratio
• Maximum drawdown
• Win rate
• Total trades
• Profit factor
• Average trade duration

**Strategy Submission:**
You can submit FinScript code for backtesting

**Execution:**
Backtests run asynchronously. Poll /api/v1/backtest/{job_id} for results`,
      codeExample: `// Submit backtest
fetch('https://api.fincept.com/v1/backtest', {
  method: 'POST',
  headers: {
    'X-API-Key': 'your_api_key',
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    symbol: 'AAPL',
    strategy: 'ema_crossover',
    start_date: '2024-01-01',
    end_date: '2024-12-31',
    initial_capital: 10000,
    parameters: {
      fast_period: 12,
      slow_period: 26
    }
  })
})
.then(res => res.json())
.then(result => {
  console.log('Backtest results:');
  console.log('Total return:', result.total_return);
  console.log('Sharpe ratio:', result.sharpe_ratio);
  console.log('Max drawdown:', result.max_drawdown);
  console.log('Win rate:', result.win_rate);
  console.log('Total trades:', result.total_trades);
});

// Example response:
{
  "success": true,
  "data": {
    "total_return": 23.5,
    "sharpe_ratio": 1.8,
    "max_drawdown": -8.2,
    "win_rate": 67.3,
    "total_trades": 45,
    "profit_factor": 2.1
  }
}`
    }
  ]
};
