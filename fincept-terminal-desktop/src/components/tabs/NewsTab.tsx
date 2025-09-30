import React, { useState, useEffect } from 'react';

interface NewsArticle {
  id: string;
  time: string;
  priority: 'FLASH' | 'URGENT' | 'BREAKING' | 'ROUTINE';
  category: string;
  headline: string;
  summary: string;
  source: string;
  region: string;
  sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL';
  impact: 'HIGH' | 'MEDIUM' | 'LOW';
  tickers: string[];
  classification: string;
}

interface MarketAlert {
  time: string;
  type: 'EARNINGS' | 'MERGER' | 'REGULATORY' | 'ECONOMIC' | 'TECHNICAL';
  message: string;
  severity: 'CRITICAL' | 'HIGH' | 'MEDIUM' | 'LOW';
  affected: string[];
}

const NewsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeFilter, setActiveFilter] = useState('ALL');
  const [newsTicker, setNewsTicker] = useState(0);
  const [alertCount, setAlertCount] = useState(47);

  // Bloomberg color scheme (exact from DearPyGUI)
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_BLUE = '#6496FA';
  const BLOOMBERG_PURPLE = '#C864FF';
  const BLOOMBERG_CYAN = '#00FFFF';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
      setNewsTicker(prev => prev + 1);
      if (Math.random() > 0.7) setAlertCount(prev => prev + 1);
    }, 3000);
    return () => clearInterval(timer);
  }, []);

  const newsArticles: NewsArticle[] = [
    {
      id: '1',
      time: '16:47:23',
      priority: 'FLASH',
      category: 'MARKETS',
      headline: 'Federal Reserve Signals Emergency Rate Cut - 75bps Expected',
      summary: 'Fed Chair Powell indicates potential emergency monetary policy action following inflation data showing 2.8% core PCE. Markets rally on dovish pivot expectations.',
      source: 'BLOOMBERG',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'HIGH',
      tickers: ['SPY', 'QQQ', 'TLT', 'USD'],
      classification: 'PUBLIC'
    },
    {
      id: '2',
      time: '16:45:12',
      priority: 'URGENT',
      category: 'EARNINGS',
      headline: 'Apple Q4 Earnings Beat - iPhone Sales Surge 18% YoY',
      summary: 'AAPL reports $1.89 EPS vs $1.70 est. Revenue $89.5B vs $94.5B est. Services revenue hits record $22.3B. China sales rebound strongly.',
      source: 'REUTERS',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'HIGH',
      tickers: ['AAPL', 'NASDAQ', 'QQQ'],
      classification: 'PUBLIC'
    },
    {
      id: '3',
      time: '16:43:56',
      priority: 'BREAKING',
      category: 'GEOPOLITICS',
      headline: 'China Announces $2.8T Infrastructure Investment Plan',
      summary: 'Beijing unveils massive Belt & Road 2.0 initiative targeting Southeast Asia and Africa. Focus on AI, renewable energy, and digital infrastructure.',
      source: 'XINHUA',
      region: 'ASIA',
      sentiment: 'BULLISH',
      impact: 'MEDIUM',
      tickers: ['FXI', 'ASHR', 'MCHI', 'EEM'],
      classification: 'PUBLIC'
    },
    {
      id: '4',
      time: '16:42:31',
      priority: 'URGENT',
      category: 'ENERGY',
      headline: 'Saudi Arabia Extends Oil Production Cuts Through Q2 2024',
      summary: 'Kingdom maintains 1M bpd voluntary cut. OPEC+ meeting confirms coordinated supply management. Brent crude jumps 4.2% to $87.45.',
      source: 'WSJ',
      region: 'MIDEAST',
      sentiment: 'BULLISH',
      impact: 'HIGH',
      tickers: ['USO', 'XLE', 'CVX', 'XOM'],
      classification: 'PUBLIC'
    },
    {
      id: '5',
      time: '16:41:08',
      priority: 'FLASH',
      category: 'CRYPTO',
      headline: 'Bitcoin ETF Approval - SEC Greenlights 11 Spot ETFs',
      summary: 'Historic regulatory milestone as SEC approves spot Bitcoin ETFs from BlackRock, Fidelity, Grayscale. BTC surges 12% to $48,500.',
      source: 'COINDESK',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'HIGH',
      tickers: ['BTC', 'GBTC', 'COIN', 'MSTR'],
      classification: 'PUBLIC'
    },
    {
      id: '6',
      time: '16:39:45',
      priority: 'URGENT',
      category: 'TECH',
      headline: 'Microsoft Azure AI Revenue Hits $1B Quarterly Run Rate',
      summary: 'MSFT Azure OpenAI services exceed $1B ARR. Enterprise AI adoption accelerates. Cloud segment shows 28% growth. Stock up 6% after hours.',
      source: 'CNBC',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'MEDIUM',
      tickers: ['MSFT', 'NVDA', 'GOOGL', 'META'],
      classification: 'PUBLIC'
    },
    {
      id: '7',
      time: '16:38:22',
      priority: 'BREAKING',
      category: 'BANKING',
      headline: 'JPMorgan Reports Record Q4 Net Income - $15.2B',
      summary: 'JPM beats on all metrics. Net interest income $22.9B vs $22.1B est. Credit loss provisions down 34%. Jamie Dimon optimistic on 2024 outlook.',
      source: 'FINANCIAL TIMES',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'MEDIUM',
      tickers: ['JPM', 'BAC', 'WFC', 'XLF'],
      classification: 'PUBLIC'
    },
    {
      id: '8',
      time: '16:37:01',
      priority: 'ROUTINE',
      category: 'ECONOMIC',
      headline: 'EU GDP Growth Revised Higher - 0.6% QoQ vs 0.4% Est',
      summary: 'Eurostat revises Q3 GDP growth upward. German manufacturing shows resilience. ECB likely to pause rate hikes. EUR/USD rallies to 1.0890.',
      source: 'EUROSTAT',
      region: 'EUROPE',
      sentiment: 'NEUTRAL',
      impact: 'LOW',
      tickers: ['EZU', 'FEZ', 'VGK', 'EUR'],
      classification: 'PUBLIC'
    }
  ];

  const marketAlerts: MarketAlert[] = [
    {
      time: '16:48:15',
      type: 'TECHNICAL',
      message: 'S&P 500 breaks above 4,800 resistance - Bullish momentum building',
      severity: 'HIGH',
      affected: ['SPY', 'SPX', 'ES']
    },
    {
      time: '16:46:33',
      type: 'ECONOMIC',
      message: 'VIX drops below 12 - Complacency levels reaching extremes',
      severity: 'MEDIUM',
      affected: ['VIX', 'VXX', 'UVXY']
    },
    {
      time: '16:44:27',
      type: 'EARNINGS',
      message: 'Tesla earnings whisper numbers suggest 15% beat possible',
      severity: 'HIGH',
      affected: ['TSLA', 'EV sector']
    },
    {
      time: '16:42:18',
      type: 'REGULATORY',
      message: 'FDA fast-tracks Alzheimer drug - Biotech sector surge expected',
      severity: 'MEDIUM',
      affected: ['XBI', 'BIIB', 'SGEN']
    }
  ];

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'FLASH': return BLOOMBERG_RED;
      case 'URGENT': return BLOOMBERG_ORANGE;
      case 'BREAKING': return BLOOMBERG_YELLOW;
      case 'ROUTINE': return BLOOMBERG_GREEN;
      default: return BLOOMBERG_GRAY;
    }
  };

  const getSentimentColor = (sentiment: string) => {
    switch (sentiment) {
      case 'BULLISH': return BLOOMBERG_GREEN;
      case 'BEARISH': return BLOOMBERG_RED;
      case 'NEUTRAL': return BLOOMBERG_YELLOW;
      default: return BLOOMBERG_GRAY;
    }
  };

  const getImpactColor = (impact: string) => {
    switch (impact) {
      case 'HIGH': return BLOOMBERG_RED;
      case 'MEDIUM': return BLOOMBERG_YELLOW;
      case 'LOW': return BLOOMBERG_GREEN;
      default: return BLOOMBERG_GRAY;
    }
  };

  const getSeverityColor = (severity: string) => {
    switch (severity) {
      case 'CRITICAL': return BLOOMBERG_RED;
      case 'HIGH': return BLOOMBERG_ORANGE;
      case 'MEDIUM': return BLOOMBERG_YELLOW;
      case 'LOW': return BLOOMBERG_GREEN;
      default: return BLOOMBERG_GRAY;
    }
  };

  const filteredNews = activeFilter === 'ALL'
    ? newsArticles
    : newsArticles.filter(article => article.category === activeFilter);

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Complex Header with News Ticker */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        {/* Main Status Line */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>BLOOMBERG PROFESSIONAL NEWS</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_RED, fontWeight: 'bold', animation: 'blink 1s infinite' }}>BREAKING NEWS</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>ALERTS: {alertCount}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN }}>● LIVE FEED</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_BLUE }}>UPDATES: {newsTicker}</span>
        </div>

        {/* News Ticker */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px', marginBottom: '2px' }}>
          <span style={{ color: BLOOMBERG_YELLOW, fontWeight: 'bold' }}>TICKER:</span>
          <div style={{
            flex: 1,
            overflow: 'hidden',
            whiteSpace: 'nowrap'
          }}>
            <div style={{
              display: 'inline-block',
              animation: 'scroll-left 60s linear infinite',
              color: BLOOMBERG_WHITE
            }}>
              🔴 FED RATE CUT EXPECTED • AAPL BEATS Q4 EARNINGS • CHINA $2.8T INFRASTRUCTURE PLAN • SAUDI EXTENDS OIL CUTS •
              BITCOIN ETF APPROVED • MSFT AZURE AI $1B+ • JPM RECORD Q4 PROFIT • EU GDP REVISED HIGHER •
              S&P 500 BREAKS 4800 • VIX BELOW 12 • TESLA EARNINGS BEAT EXPECTED • FDA BIOTECH APPROVAL
            </div>
          </div>
        </div>

        {/* Sources and Stats */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>SOURCES:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>BLOOMBERG●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>REUTERS●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>WSJ●</span>
          <span style={{ color: BLOOMBERG_GREEN }}>FT●</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>PROCESSING:</span>
          <span style={{ color: BLOOMBERG_CYAN }}>2.3M articles/day</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>SENTIMENT:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>BULLISH 62%</span>
          <span style={{ color: BLOOMBERG_RED }}>BEARISH 23%</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>NEUTRAL 15%</span>
        </div>
      </div>

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '2px 4px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
          {[
            { key: "F1", label: "ALL", filter: "ALL" },
            { key: "F2", label: "MARKETS", filter: "MARKETS" },
            { key: "F3", label: "EARNINGS", filter: "EARNINGS" },
            { key: "F4", label: "ECON", filter: "ECONOMIC" },
            { key: "F5", label: "TECH", filter: "TECH" },
            { key: "F6", label: "ENERGY", filter: "ENERGY" },
            { key: "F7", label: "BANKING", filter: "BANKING" },
            { key: "F8", label: "CRYPTO", filter: "CRYPTO" },
            { key: "F9", label: "GEOPO", filter: "GEOPOLITICS" },
            { key: "F10", label: "ALERTS", filter: "ALERTS" },
            { key: "F11", label: "SEARCH", filter: "SEARCH" },
            { key: "F12", label: "SETTINGS", filter: "SETTINGS" }
          ].map(item => (
            <button key={item.key}
              onClick={() => setActiveFilter(item.filter)}
              style={{
                backgroundColor: activeFilter === item.filter ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: activeFilter === item.filter ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: activeFilter === item.filter ? 'bold' : 'normal'
              }}>
              <span style={{ color: BLOOMBERG_YELLOW }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content - 3 Panel Layout */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
        <div style={{ display: 'flex', gap: '4px', height: '100%' }}>

          {/* Left Panel - Breaking News Stream */}
          <div style={{
            width: '400px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              BREAKING NEWS STREAM [{activeFilter}]
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '4px' }}></div>

            {filteredNews.map((article, index) => (
              <div key={article.id} style={{
                marginBottom: '6px',
                borderLeft: `3px solid ${getPriorityColor(article.priority)}`,
                paddingLeft: '4px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent'
              }}>
                <div style={{ display: 'flex', gap: '4px', marginBottom: '1px', fontSize: '8px' }}>
                  <span style={{ color: BLOOMBERG_GRAY, minWidth: '50px' }}>{article.time}</span>
                  <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold', minWidth: '60px' }}>
                    [{article.priority}]
                  </span>
                  <span style={{ color: BLOOMBERG_BLUE, minWidth: '50px' }}>[{article.category}]</span>
                  <span style={{ color: BLOOMBERG_PURPLE, minWidth: '30px' }}>{article.region}</span>
                  <span style={{ color: getSentimentColor(article.sentiment), minWidth: '50px' }}>{article.sentiment}</span>
                </div>

                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', fontSize: '10px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.headline}
                </div>

                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.summary}
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>{article.source}</span>
                  <span style={{ color: getImpactColor(article.impact) }}>IMPACT: {article.impact}</span>
                </div>

                <div style={{ fontSize: '8px', marginTop: '1px' }}>
                  <span style={{ color: BLOOMBERG_YELLOW }}>TICKERS: </span>
                  {article.tickers.map((ticker, i) => (
                    <span key={i} style={{ color: BLOOMBERG_GREEN, marginRight: '4px' }}>{ticker}</span>
                  ))}
                </div>
              </div>
            ))}
          </div>

          {/* Center Panel - Market Alerts & Analysis */}
          <div style={{
            flex: 1,
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              REAL-TIME MARKET ALERTS & ANALYSIS
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {/* Market Alerts Section */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                ACTIVE MARKET ALERTS
              </div>
              {marketAlerts.map((alert, index) => (
                <div key={index} style={{
                  marginBottom: '4px',
                  padding: '3px',
                  backgroundColor: 'rgba(255,0,0,0.1)',
                  border: `1px solid ${getSeverityColor(alert.severity)}`,
                  borderRadius: '2px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px', marginBottom: '1px' }}>
                    <span style={{ color: BLOOMBERG_GRAY }}>{alert.time}</span>
                    <span style={{ color: getSeverityColor(alert.severity), fontWeight: 'bold' }}>[{alert.severity}]</span>
                    <span style={{ color: BLOOMBERG_BLUE }}>[{alert.type}]</span>
                  </div>
                  <div style={{ color: BLOOMBERG_WHITE, fontSize: '9px', lineHeight: '1.2', marginBottom: '1px' }}>
                    {alert.message}
                  </div>
                  <div style={{ fontSize: '8px' }}>
                    <span style={{ color: BLOOMBERG_YELLOW }}>AFFECTED: </span>
                    {alert.affected.map((item, i) => (
                      <span key={i} style={{ color: BLOOMBERG_GREEN, marginRight: '4px' }}>{item}</span>
                    ))}
                  </div>
                </div>
              ))}
            </div>

            {/* Market Movers Section */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                TOP MARKET MOVERS - NEWS DRIVEN
              </div>
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr 1fr 1fr',
                gap: '4px',
                fontSize: '9px',
                marginBottom: '4px'
              }}>
                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>TICKER</div>
                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>PRICE</div>
                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>CHANGE</div>
                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>NEWS IMPACT</div>
              </div>

              {[
                { ticker: 'AAPL', price: '184.92', change: '+8.34%', impact: 'EARNINGS BEAT' },
                { ticker: 'MSFT', price: '378.45', change: '+6.12%', impact: 'AI REVENUE' },
                { ticker: 'TSLA', price: '251.78', change: '+4.89%', impact: 'DELIVERY BEAT' },
                { ticker: 'XLE', price: '89.23', change: '+4.21%', impact: 'SAUDI CUTS' },
                { ticker: 'BTC', price: '48,567', change: '+12.45%', impact: 'ETF APPROVAL' },
                { ticker: 'JPM', price: '178.34', change: '+3.67%', impact: 'RECORD PROFIT' }
              ].map((stock, index) => (
                <div key={index} style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr 1fr 1fr',
                  gap: '4px',
                  fontSize: '9px',
                  marginBottom: '1px',
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent'
                }}>
                  <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{stock.ticker}</div>
                  <div style={{ color: BLOOMBERG_WHITE }}>{stock.price}</div>
                  <div style={{ color: BLOOMBERG_GREEN }}>{stock.change}</div>
                  <div style={{ color: BLOOMBERG_CYAN }}>{stock.impact}</div>
                </div>
              ))}
            </div>

            {/* Sector Impact */}
            <div>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                NEWS IMPACT BY SECTOR
              </div>
              {[
                { sector: 'Technology', impact: '+4.2%', news: 'AI revenue surge, cloud growth' },
                { sector: 'Energy', impact: '+3.8%', news: 'OPEC cuts, geopolitical tensions' },
                { sector: 'Financials', impact: '+2.9%', news: 'Strong earnings, rate environment' },
                { sector: 'Healthcare', impact: '+1.4%', news: 'FDA approvals, M&A activity' },
                { sector: 'Consumer', impact: '-0.8%', news: 'Inflation concerns, spending data' }
              ].map((sector, index) => (
                <div key={index} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  fontSize: '9px',
                  marginBottom: '2px',
                  padding: '1px 2px'
                }}>
                  <span style={{ color: BLOOMBERG_WHITE, minWidth: '80px' }}>{sector.sector}</span>
                  <span style={{ color: sector.impact.startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED, minWidth: '50px' }}>
                    {sector.impact}
                  </span>
                  <span style={{ color: BLOOMBERG_GRAY, flex: 1, marginLeft: '8px' }}>{sector.news}</span>
                </div>
              ))}
            </div>
          </div>

          {/* Right Panel - News Analytics */}
          <div style={{
            width: '300px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              NEWS ANALYTICS DASHBOARD
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {/* Sentiment Analysis */}
            <div style={{ marginBottom: '10px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                REAL-TIME SENTIMENT
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.3' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Bullish Articles:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>162 (62%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Bearish Articles:</span>
                  <span style={{ color: BLOOMBERG_RED }}>59 (23%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Neutral Articles:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>41 (15%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '2px', paddingTop: '2px', borderTop: `1px solid ${BLOOMBERG_GRAY}` }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>Sentiment Score:</span>
                  <span style={{ color: BLOOMBERG_GREEN, fontWeight: 'bold' }}>+0.73 (BULLISH)</span>
                </div>
              </div>
            </div>

            {/* News Flow Statistics */}
            <div style={{ marginBottom: '10px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                NEWS FLOW STATISTICS
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.3' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Articles Today:</span>
                  <span style={{ color: BLOOMBERG_WHITE }}>2,847</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Breaking News:</span>
                  <span style={{ color: BLOOMBERG_RED }}>23</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Earnings Reports:</span>
                  <span style={{ color: BLOOMBERG_BLUE }}>78</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Economic Data:</span>
                  <span style={{ color: BLOOMBERG_PURPLE }}>12</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Fed News:</span>
                  <span style={{ color: BLOOMBERG_ORANGE }}>8</span>
                </div>
              </div>
            </div>

            {/* Trending Topics */}
            <div style={{ marginBottom: '10px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                TRENDING TOPICS (1H)
              </div>
              <div style={{ fontSize: '9px' }}>
                {[
                  { topic: 'Federal Reserve', mentions: 156, trend: 'up' },
                  { topic: 'Artificial Intelligence', mentions: 134, trend: 'up' },
                  { topic: 'Bitcoin ETF', mentions: 89, trend: 'up' },
                  { topic: 'Oil Prices', mentions: 67, trend: 'up' },
                  { topic: 'Bank Earnings', mentions: 45, trend: 'stable' },
                  { topic: 'China Economy', mentions: 38, trend: 'down' }
                ].map((item, index) => (
                  <div key={index} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>{item.topic}</span>
                    <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
                      <span style={{ color: BLOOMBERG_CYAN }}>{item.mentions}</span>
                      <span style={{
                        color: item.trend === 'up' ? BLOOMBERG_GREEN :
                               item.trend === 'down' ? BLOOMBERG_RED : BLOOMBERG_YELLOW
                      }}>
                        {item.trend === 'up' ? '↑' : item.trend === 'down' ? '↓' : '→'}
                      </span>
                    </div>
                  </div>
                ))}
              </div>
            </div>

            {/* Key Economic Events */}
            <div>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                UPCOMING EVENTS
              </div>
              <div style={{ fontSize: '8px' }}>
                {[
                  { time: '18:00', event: 'Fed Chair Powell Speech', impact: 'HIGH' },
                  { time: '22:30', event: 'Core PCE Data Release', impact: 'HIGH' },
                  { time: 'Thu 08:30', event: 'Initial Jobless Claims', impact: 'MEDIUM' },
                  { time: 'Thu 10:00', event: 'ISM Manufacturing PMI', impact: 'MEDIUM' },
                  { time: 'Fri 08:30', event: 'Non-Farm Payrolls', impact: 'HIGH' }
                ].map((event, index) => (
                  <div key={index} style={{ marginBottom: '2px', display: 'flex', gap: '4px' }}>
                    <span style={{ color: BLOOMBERG_GRAY, minWidth: '55px' }}>{event.time}</span>
                    <span style={{ color: BLOOMBERG_WHITE, flex: 1 }}>{event.event}</span>
                    <span style={{ color: getImpactColor(event.impact), minWidth: '40px' }}>[{event.impact}]</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        backgroundColor: BLOOMBERG_PANEL_BG,
        padding: '2px 8px',
        fontSize: '10px',
        color: BLOOMBERG_GRAY,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Bloomberg News Terminal v4.1.2 | Real-time financial news analysis</span>
            <span>Sources: 47 Active | Processing: 2.3M articles/day | Sentiment: +0.73 Bullish</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Alerts: {alertCount} | Updates: {newsTicker}</span>
            <span>Session: {currentTime.toTimeString().substring(0, 8)}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>FEED: LIVE</span>
          </div>
        </div>
      </div>

      {/* CSS for animations */}
      <style>{`
        @keyframes scroll-left {
          0% { transform: translateX(100%); }
          100% { transform: translateX(-100%); }
        }
        @keyframes blink {
          0%, 50% { opacity: 1; }
          51%, 100% { opacity: 0.3; }
        }
      `}</style>
    </div>
  );
};

export default NewsTab;