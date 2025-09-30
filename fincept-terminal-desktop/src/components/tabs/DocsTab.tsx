import React, { useState } from 'react';
import { Book, Search, ChevronRight, Code, FileText, Zap, TrendingUp, Database, Terminal } from 'lucide-react';

interface DocSection {
  id: string;
  title: string;
  icon: any;
  subsections: {
    id: string;
    title: string;
    content: string;
    codeExample?: string;
  }[];
}

export default function DocsTab() {
  const [selectedSection, setSelectedSection] = useState('getting-started');
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedSections, setExpandedSections] = useState<string[]>(['finscript']);

  const C = {
    ORANGE: '#FF8C00',
    WHITE: '#FFFFFF',
    GREEN: '#00FF00',
    BLUE: '#4169E1',
    CYAN: '#00FFFF',
    YELLOW: '#FFFF00',
    GRAY: '#787878',
    DARK_BG: '#000000',
    PANEL_BG: '#0a0a0a',
    CODE_BG: '#1a1a1a'
  };

  const docs: DocSection[] = [
    {
      id: 'finscript',
      title: 'FinScript Language',
      icon: Code,
      subsections: [
        {
          id: 'getting-started',
          title: 'Getting Started',
          content: `FinScript is a domain-specific language (DSL) designed for financial analysis and algorithmic trading. It provides a simple, intuitive syntax for defining trading strategies, calculating technical indicators, and generating trading signals.

**Key Features:**
• Simple syntax inspired by Pine Script
• Built-in technical indicators (EMA, SMA, WMA, RSI, MACD, etc.)
• Real-time data integration with Yahoo Finance
• Automatic chart generation
• Buy/Sell signal generation
• Backtesting capabilities`,
          codeExample: `// Your first FinScript program
if ema(AAPL, 21) > wma(AAPL, 50) {
    buy "Bullish crossover signal"
}

plot ema(AAPL, 21), "EMA 21"
plot wma(AAPL, 50), "WMA 50"`
        },
        {
          id: 'syntax',
          title: 'Basic Syntax',
          content: `FinScript uses a clean, declarative syntax. Here are the core language constructs:

**Comments:**
Use // for single-line comments

**Variables:**
Variables are automatically typed based on indicator functions

**Conditionals:**
Use if statements to define trading logic

**Functions:**
Built-in functions for technical analysis`,
          codeExample: `// Comments explain your strategy
// Variables are created by indicators
fast_ema = ema(TSLA, 12)
slow_ema = ema(TSLA, 26)

// Conditionals for signals
if fast_ema > slow_ema {
    buy "Fast EMA crossed above Slow EMA"
}

if fast_ema < slow_ema {
    sell "Fast EMA crossed below Slow EMA"
}`
        },
        {
          id: 'indicators',
          title: 'Technical Indicators',
          content: `FinScript provides a comprehensive suite of technical indicators for market analysis:

**Moving Averages:**
• ema(symbol, period) - Exponential Moving Average
• sma(symbol, period) - Simple Moving Average
• wma(symbol, period) - Weighted Moving Average

**Momentum Indicators:**
• rsi(symbol, period) - Relative Strength Index
• macd(symbol, fast, slow, signal) - MACD
• stochastic(symbol, period) - Stochastic Oscillator

**Volatility:**
• atr(symbol, period) - Average True Range
• bollinger(symbol, period, std) - Bollinger Bands

**Volume:**
• volume(symbol) - Trading Volume
• obv(symbol) - On-Balance Volume`,
          codeExample: `// Using multiple indicators
price = close(AAPL)
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_value = rsi(AAPL, 14)

// Combine indicators for strategy
if ema_fast > ema_slow and rsi_value < 30 {
    buy "Oversold with bullish trend"
}

plot ema_fast, "EMA 12"
plot ema_slow, "EMA 26"
plot rsi_value, "RSI 14"`
        },
        {
          id: 'plotting',
          title: 'Visualization & Plotting',
          content: `FinScript provides powerful charting capabilities to visualize your strategies:

**plot** - Plot indicator values
Syntax: plot indicator, "label"

**plot_candlestick** - Generate OHLC candlestick charts
Syntax: plot_candlestick symbol, "chart_title"

**plot_line** - Draw custom lines
Syntax: plot_line value, "label", color

Charts are automatically generated and saved as PNG files in your working directory.`,
          codeExample: `// Visualize your strategy
symbol = MSFT

// Plot multiple indicators
plot ema(symbol, 7), "EMA 7"
plot ema(symbol, 21), "EMA 21"
plot sma(symbol, 50), "SMA 50"

// Generate candlestick chart
plot_candlestick symbol, "Microsoft Stock Chart"

// Add support/resistance lines
plot_line 300.0, "Resistance", "red"
plot_line 250.0, "Support", "green"`
        },
        {
          id: 'signals',
          title: 'Trading Signals',
          content: `Generate buy and sell signals based on your trading logic:

**buy** - Generate a buy signal
Syntax: buy "reason for buying"

**sell** - Generate a sell signal
Syntax: sell "reason for selling"

Signals are logged with timestamps and can trigger automated trading when integrated with brokers.

**Best Practices:**
• Always include descriptive messages
• Use multiple confirmation indicators
• Implement risk management
• Test thoroughly with historical data`,
          codeExample: `// Crossover strategy with signals
fast = ema(GOOGL, 12)
slow = ema(GOOGL, 26)
momentum = rsi(GOOGL, 14)

// Buy signal with multiple confirmations
if fast > slow and momentum > 50 {
    buy "EMA crossover with positive momentum"
}

// Sell signal with risk management
if fast < slow or momentum > 70 {
    sell "EMA crossover or overbought condition"
}

// Plot for visual confirmation
plot fast, "Fast EMA"
plot slow, "Slow EMA"`
        }
      ]
    },
    {
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

**Authentication:**
Use API keys in the X-API-Key header

**Rate Limits:**
• Free tier: 100 requests/hour
• Pro tier: 1000 requests/hour
• Enterprise: Unlimited`,
          codeExample: `// Example API calls
// Get real-time quote
fetch('https://api.fincept.com/v1/quote/AAPL', {
  headers: {
    'X-API-Key': 'your_api_key_here'
  }
})

// Get historical data
fetch('https://api.fincept.com/v1/historical/AAPL?period=1y', {
  headers: {
    'X-API-Key': 'your_api_key_here'
  }
})`
        },
        {
          id: 'websocket',
          title: 'WebSocket Streaming',
          content: `Real-time data streaming via WebSocket for live market data:

**Connection:**
wss://stream.fincept.com/v1/quotes

**Subscribe to symbols:**
Send JSON: {"action": "subscribe", "symbols": ["AAPL", "TSLA"]}

**Message Format:**
Receive real-time tick data as JSON

**Features:**
• Sub-millisecond latency
• Level 2 order book data
• Trade execution updates
• Market depth information`,
          codeExample: `// WebSocket connection example
const ws = new WebSocket('wss://stream.fincept.com/v1/quotes');

ws.onopen = () => {
  // Subscribe to symbols
  ws.send(JSON.stringify({
    action: 'subscribe',
    symbols: ['AAPL', 'TSLA', 'GOOGL']
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Live quote:', data);
};`
        }
      ]
    },
    {
      id: 'terminal',
      title: 'Terminal Features',
      icon: Terminal,
      subsections: [
        {
          id: 'shortcuts',
          title: 'Keyboard Shortcuts',
          content: `Master the terminal with these keyboard shortcuts:

**Global Shortcuts:**
• F1 - Help
• F2 - Markets
• F3 - News
• F4 - Portfolio
• F5 - Analytics
• F11 - Fullscreen toggle
• Ctrl+S - Save current file
• Ctrl+N - New file
• Ctrl+O - Open file

**Editor Shortcuts:**
• Ctrl+/ - Comment/Uncomment
• Ctrl+F - Find
• Ctrl+R - Run code
• Ctrl+Shift+K - Clear output

**Navigation:**
• Ctrl+Tab - Next tab
• Ctrl+Shift+Tab - Previous tab
• Ctrl+W - Close tab`,
          codeExample: `// Quick actions in editor
// Press Ctrl+/ to toggle comments
// if ema(AAPL, 21) > sma(AAPL, 50) {
//     buy "EMA crosses SMA"
// }

// Press Ctrl+R to execute code
plot ema(AAPL, 21), "EMA 21"

// Press Ctrl+S to save file`
        },
        {
          id: 'customization',
          title: 'Customization',
          content: `Customize the terminal to match your workflow:

**Themes:**
• Bloomberg Classic (default)
• Dark Mode
• Light Mode
• Custom themes via CSS

**Layouts:**
• Single panel
• Dual panel (editor + output)
• Triple panel (editor + output + charts)
• Customizable panel sizes

**Settings:**
• Font size and family
• Tab size (spaces)
• Auto-save interval
• Data refresh rate
• Chart colors and styles`,
          codeExample: `// Example settings.json
{
  "theme": "bloomberg-classic",
  "editor": {
    "fontSize": 14,
    "fontFamily": "Consolas",
    "tabSize": 4,
    "autoSave": true,
    "autoSaveInterval": 5000
  },
  "terminal": {
    "refreshRate": 1000,
    "enableNotifications": true
  }
}`
        }
      ]
    },
    {
      id: 'tutorials',
      title: 'Tutorials',
      icon: Book,
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

This demonstrates how to combine multiple signals for robust trading decisions.`,
          codeExample: `// Advanced Multi-Indicator Strategy
symbol = TSLA

// Indicators
ema_fast = ema(symbol, 12)
ema_slow = ema(symbol, 26)
rsi_val = rsi(symbol, 14)
vol = volume(symbol)
avg_vol = sma(vol, 20)

// Buy conditions (all must be true)
trend_bullish = ema_fast > ema_slow
oversold = rsi_val < 30
volume_spike = vol > avg_vol * 1.5

if trend_bullish and oversold and volume_spike {
    buy "Strong buy: Bullish trend + oversold + volume"
}

// Sell conditions (any can trigger)
trend_bearish = ema_fast < ema_slow
overbought = rsi_val > 70

if trend_bearish or overbought {
    sell "Exit: Trend reversal or overbought"
}

// Visualize everything
plot ema_fast, "EMA 12"
plot ema_slow, "EMA 26"
plot rsi_val, "RSI"`
        }
      ]
    }
  ];

  const toggleSection = (sectionId: string) => {
    setExpandedSections(prev =>
      prev.includes(sectionId)
        ? prev.filter(id => id !== sectionId)
        : [...prev, sectionId]
    );
  };

  const filteredDocs = docs.map(section => ({
    ...section,
    subsections: section.subsections.filter(sub =>
      searchQuery === '' ||
      sub.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
      sub.content.toLowerCase().includes(searchQuery.toLowerCase())
    )
  })).filter(section => section.subsections.length > 0);

  const activeSubsection = docs
    .flatMap(s => s.subsections)
    .find(sub => sub.id === selectedSection);

  return (
    <div style={{ width: '100%', height: '100%', backgroundColor: C.DARK_BG, color: C.WHITE, fontFamily: 'Consolas, monospace', display: 'flex', flexDirection: 'column' }}>
      <style>{`
        .docs-scroll::-webkit-scrollbar { width: 8px; }
        .docs-scroll::-webkit-scrollbar-track { background: #1a1a1a; }
        .docs-scroll::-webkit-scrollbar-thumb { background: #404040; border-radius: 4px; }
        .docs-scroll::-webkit-scrollbar-thumb:hover { background: #525252; }

        .docs-content h1 { color: ${C.ORANGE}; font-size: 24px; margin: 24px 0 16px 0; }
        .docs-content h2 { color: ${C.CYAN}; font-size: 18px; margin: 20px 0 12px 0; }
        .docs-content p { line-height: 1.8; margin: 12px 0; }
        .docs-content ul { margin: 12px 0; padding-left: 24px; }
        .docs-content li { margin: 6px 0; line-height: 1.6; }
      `}</style>

      {/* Header */}
      <div style={{ backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}`, padding: '8px 16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <Book size={20} style={{ color: C.ORANGE }} />
            <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '16px' }}>DOCUMENTATION</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.CYAN, fontSize: '12px' }}>Complete Guide & API Reference</span>
          </div>
        </div>
      </div>

      {/* Search Bar */}
      <div style={{ backgroundColor: C.PANEL_BG, borderBottom: `1px solid ${C.GRAY}`, padding: '12px 16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', backgroundColor: C.DARK_BG, border: `1px solid ${C.GRAY}`, padding: '8px 12px' }}>
          <Search size={16} style={{ color: C.GRAY }} />
          <input
            type="text"
            placeholder="Search documentation..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{ flex: 1, backgroundColor: 'transparent', border: 'none', outline: 'none', color: C.WHITE, fontSize: '13px', fontFamily: 'Consolas, monospace' }}
          />
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
        {/* Sidebar Navigation */}
        <div className="docs-scroll" style={{ width: '280px', borderRight: `1px solid ${C.GRAY}`, backgroundColor: C.PANEL_BG, overflowY: 'auto', padding: '12px' }}>
          {filteredDocs.map(section => (
            <div key={section.id} style={{ marginBottom: '8px' }}>
              <div
                onClick={() => toggleSection(section.id)}
                style={{ display: 'flex', alignItems: 'center', gap: '8px', padding: '8px', backgroundColor: C.DARK_BG, cursor: 'pointer', border: `1px solid ${C.GRAY}` }}
              >
                <section.icon size={14} style={{ color: C.ORANGE }} />
                <span style={{ flex: 1, fontSize: '12px', fontWeight: 'bold', color: C.WHITE }}>{section.title}</span>
                <ChevronRight
                  size={14}
                  style={{ color: C.GRAY, transform: expandedSections.includes(section.id) ? 'rotate(90deg)' : 'rotate(0deg)', transition: 'transform 0.2s' }}
                />
              </div>
              {expandedSections.includes(section.id) && (
                <div style={{ marginTop: '4px', marginLeft: '12px' }}>
                  {section.subsections.map(sub => (
                    <div
                      key={sub.id}
                      onClick={() => setSelectedSection(sub.id)}
                      style={{ padding: '8px 12px', fontSize: '11px', color: selectedSection === sub.id ? C.ORANGE : C.GRAY, backgroundColor: selectedSection === sub.id ? C.DARK_BG : 'transparent', cursor: 'pointer', borderLeft: `2px solid ${selectedSection === sub.id ? C.ORANGE : 'transparent'}` }}
                    >
                      {sub.title}
                    </div>
                  ))}
                </div>
              )}
            </div>
          ))}
        </div>

        {/* Content Area */}
        <div className="docs-scroll" style={{ flex: 1, overflowY: 'auto', padding: '24px', backgroundColor: '#050505' }}>
          <div style={{ maxWidth: '900px', margin: '0 auto' }}>
            {activeSubsection ? (
              <>
                <h1 style={{ color: C.ORANGE, fontSize: '28px', marginBottom: '16px', borderBottom: `2px solid ${C.ORANGE}`, paddingBottom: '12px' }}>
                  {activeSubsection.title}
                </h1>
                <div className="docs-content" style={{ fontSize: '14px', lineHeight: '1.8', color: C.WHITE }}>
                  {activeSubsection.content.split('\n\n').map((para, idx) => (
                    <p key={idx} style={{ marginBottom: '16px', whiteSpace: 'pre-wrap' }}>
                      {para.split('\n').map((line, lineIdx) => {
                        if (line.startsWith('**') && line.endsWith('**')) {
                          return <div key={lineIdx} style={{ color: C.CYAN, fontWeight: 'bold', marginTop: '16px', marginBottom: '8px' }}>{line.replace(/\*\*/g, '')}</div>;
                        }
                        if (line.startsWith('•')) {
                          return <li key={lineIdx} style={{ marginLeft: '20px', marginBottom: '4px' }}>{line.substring(1).trim()}</li>;
                        }
                        return <span key={lineIdx}>{line}<br /></span>;
                      })}
                    </p>
                  ))}
                </div>

                {activeSubsection.codeExample && (
                  <div style={{ marginTop: '24px' }}>
                    <div style={{ color: C.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <Code size={16} />
                      CODE EXAMPLE
                    </div>
                    <pre style={{ backgroundColor: C.CODE_BG, border: `1px solid ${C.GRAY}`, padding: '16px', borderRadius: '4px', overflowX: 'auto', fontSize: '13px', lineHeight: '1.6', color: C.GREEN }}>
                      <code>{activeSubsection.codeExample}</code>
                    </pre>
                  </div>
                )}
              </>
            ) : (
              <div style={{ textAlign: 'center', paddingTop: '60px', color: C.GRAY }}>
                <Book size={48} style={{ marginBottom: '16px', opacity: 0.5 }} />
                <div style={{ fontSize: '16px' }}>Select a topic from the sidebar</div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Footer */}
      <div style={{ borderTop: `2px solid ${C.ORANGE}`, backgroundColor: C.PANEL_BG, padding: '8px 16px', fontSize: '11px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <span style={{ color: C.ORANGE, fontWeight: 'bold' }}>FINCEPT DOCS v3.0</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.CYAN }}>FinScript Language Reference</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.BLUE }}>API Documentation</span>
          </div>
          <div style={{ color: C.GRAY }}>
            {activeSubsection ? activeSubsection.title : 'Browse Documentation'}
          </div>
        </div>
      </div>
    </div>
  );
}