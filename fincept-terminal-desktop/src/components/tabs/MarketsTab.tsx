import React, { useState, useEffect } from 'react';

const MarketsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [lastUpdate, setLastUpdate] = useState(new Date());
  const [isUpdating, setIsUpdating] = useState(false);
  const [autoUpdate, setAutoUpdate] = useState(true);

  // Bloomberg color scheme (exact from Python)
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#1a1a1a';
  const BLOOMBERG_PANEL_BG = '#000000';

  // Market data (exact structure from Python)
  const marketData: { [key: string]: string[][] } = {
    'Stock Indices': [
      ['S&P 500', '5232.35', '+23.45', '+0.45%', '+2.1%', '+5.4%'],
      ['Nasdaq 100', '16412.87', '-15.23', '-0.09%', '+1.8%', '+7.2%'],
      ['Dow Jones', '38546.12', '+156.78', '+0.41%', '+1.5%', '+3.9%'],
      ['Russell 2000', '2087.45', '+8.90', '+0.43%', '+2.3%', '+4.1%'],
      ['VIX', '18.23', '-0.45', '-2.41%', '-5.2%', '-8.7%'],
      ['FTSE 100', '7623.98', '+12.34', '+0.16%', '+0.8%', '+2.3%'],
      ['DAX 40', '18245.67', '+89.23', '+0.49%', '+1.2%', '+4.6%'],
      ['Nikkei 225', '35897.45', '-123.45', '-0.34%', '-0.5%', '+1.8%'],
      ['CAC 40', '7890.12', '+45.67', '+0.58%', '+1.8%', '+4.2%'],
      ['Hang Seng', '18654.23', '-89.45', '-0.48%', '-1.2%', '+2.8%']
    ],
    'Forex': [
      ['EUR/USD', '1.0840', '+0.0023', '+0.21%', '+0.8%', '+2.1%'],
      ['GBP/USD', '1.2567', '-0.0012', '-0.10%', '-0.3%', '+1.5%'],
      ['USD/JPY', '151.45', '+0.35', '+0.23%', '+1.2%', '+3.4%'],
      ['USD/CHF', '0.8923', '-0.0034', '-0.38%', '-0.7%', '-1.2%'],
      ['USD/CAD', '1.3578', '+0.0089', '+0.66%', '+0.4%', '+1.8%'],
      ['AUD/USD', '0.6634', '+0.0067', '+1.02%', '+2.1%', '+4.3%'],
      ['NZD/USD', '0.6089', '-0.0034', '-0.56%', '-0.8%', '+1.2%'],
      ['EUR/GBP', '0.8623', '+0.0012', '+0.14%', '+0.3%', '+0.9%'],
      ['EUR/JPY', '164.23', '+0.89', '+0.54%', '+1.8%', '+5.1%'],
      ['GBP/JPY', '190.45', '-1.23', '-0.64%', '-0.9%', '+2.3%']
    ],
    'Commodities': [
      ['Gold', '2312.80', '+12.45', '+0.54%', '+1.8%', '+5.2%'],
      ['Silver', '28.45', '-0.67', '-2.30%', '-1.2%', '+3.1%'],
      ['Crude Oil (WTI)', '78.35', '-1.23', '-1.55%', '-2.8%', '-0.5%'],
      ['Brent Crude', '82.10', '-0.89', '-1.07%', '-2.1%', '+1.2%'],
      ['Natural Gas', '3.45', '+0.12', '+3.60%', '+8.2%', '+12.4%'],
      ['Copper', '4.23', '+0.045', '+1.08%', '+2.3%', '+6.7%'],
      ['Platinum', '987.50', '+8.90', '+0.91%', '+2.1%', '+4.8%'],
      ['Palladium', '1045.20', '-12.45', '-1.18%', '-2.3%', '-5.1%'],
      ['Wheat', '567.80', '+5.67', '+1.01%', '+3.2%', '+7.9%'],
      ['Corn', '478.90', '-3.45', '-0.72%', '-1.8%', '+2.4%']
    ],
    'Bonds': [
      ['US 10Y Treasury', '4.35%', '+0.02', '+0.46%', '+1.2%', '+3.4%'],
      ['US 30Y Treasury', '4.52%', '+0.01', '+0.22%', '+0.8%', '+2.1%'],
      ['US 2Y Treasury', '4.89%', '-0.03', '-0.61%', '-0.5%', '+1.8%'],
      ['Germany 10Y', '2.45%', '+0.01', '+0.41%', '+0.6%', '+1.9%'],
      ['UK 10Y', '4.12%', '-0.02', '-0.48%', '-0.3%', '+2.3%'],
      ['Japan 10Y', '0.78%', '+0.01', '+1.30%', '+2.1%', '+8.7%'],
      ['France 10Y', '2.98%', '+0.02', '+0.67%', '+0.9%', '+2.8%'],
      ['Italy 10Y', '4.23%', '-0.01', '-0.24%', '-0.2%', '+1.5%'],
      ['Spain 10Y', '3.67%', '+0.03', '+0.82%', '+1.1%', '+3.2%'],
      ['Canada 10Y', '3.89%', '+0.01', '+0.26%', '+0.7%', '+2.1%']
    ],
    'ETFs': [
      ['SPY (S&P 500)', '523.45', '+2.34', '+0.45%', '+2.1%', '+5.4%'],
      ['QQQ (Nasdaq)', '412.67', '-1.23', '-0.30%', '+1.8%', '+7.2%'],
      ['DIA (Dow)', '385.23', '+1.56', '+0.41%', '+1.5%', '+3.9%'],
      ['EEM (Emerging)', '41.78', '+0.34', '+0.82%', '+3.2%', '+6.8%'],
      ['GLD (Gold)', '231.45', '+1.23', '+0.53%', '+1.8%', '+5.1%'],
      ['XLK (Tech)', '198.90', '-0.89', '-0.45%', '+2.3%', '+8.9%'],
      ['XLE (Energy)', '89.34', '+1.45', '+1.65%', '+3.8%', '+7.2%'],
      ['XLF (Finance)', '42.56', '-0.67', '-1.55%', '-1.2%', '+2.1%'],
      ['XLV (Health)', '156.78', '+0.98', '+0.63%', '+1.1%', '+3.7%'],
      ['VNQ (REIT)', '78.90', '-1.12', '-1.40%', '-2.3%', '+1.8%']
    ],
    'Cryptocurrencies': [
      ['Bitcoin', '67890.45', '+1234.56', '+1.85%', '+8.2%', '+15.7%'],
      ['Ethereum', '3789.23', '-67.89', '-1.76%', '+5.4%', '+12.3%'],
      ['Binance Coin', '567.89', '+12.34', '+2.22%', '+6.8%', '+18.9%'],
      ['Solana', '234.56', '+8.90', '+3.94%', '+12.1%', '+25.6%'],
      ['Dogecoin', '0.1234', '-0.0045', '-3.52%', '-2.1%', '+8.7%'],
      ['Polygon', '1.23', '+0.045', '+3.80%', '+9.8%', '+21.4%'],
      ['Chainlink', '23.45', '+0.89', '+3.94%', '+7.2%', '+14.8%'],
      ['Cardano', '0.67', '-0.023', '-3.32%', '-1.8%', '+5.4%'],
      ['Polkadot', '12.34', '+0.56', '+4.76%', '+8.9%', '+19.2%'],
      ['Avalanche', '45.67', '-1.23', '-2.62%', '-3.1%', '+11.7%']
    ]
  };

  // Regional data (exact from Python)
  const regionalData: { [key: string]: string[][] } = {
    'India': [
      ['RELIANCE.NS', 'Reliance Industries', '2500.45', '+23.45', '+0.95%'],
      ['TCS.NS', 'Tata Consultancy', '3456.78', '-12.34', '-0.36%'],
      ['HDFCBANK.NS', 'HDFC Bank', '1678.90', '+8.90', '+0.53%'],
      ['INFY.NS', 'Infosys', '1234.56', '+15.67', '+1.29%'],
      ['HINDUNILVR.NS', 'Hindustan Unilever', '2345.67', '-5.43', '-0.23%'],
      ['ICICIBANK.NS', 'ICICI Bank', '987.65', '+12.34', '+1.27%'],
      ['SBIN.NS', 'State Bank of India', '678.90', '+3.45', '+0.51%'],
      ['BHARTIARTL.NS', 'Bharti Airtel', '876.54', '-7.89', '-0.89%']
    ],
    'China': [
      ['BABA', 'Alibaba Group', '85.67', '+2.34', '+2.81%'],
      ['PDD', 'PDD Holdings', '123.45', '-1.23', '-0.99%'],
      ['JD', 'JD.com', '34.56', '+0.89', '+2.64%'],
      ['BIDU', 'Baidu', '145.67', '+3.45', '+2.43%'],
      ['TCEHY', 'Tencent Holdings', '45.78', '-0.67', '-1.44%'],
      ['NIO', 'NIO Inc', '12.34', '+0.45', '+3.78%'],
      ['LI', 'Li Auto', '23.45', '+1.23', '+5.54%'],
      ['XPEV', 'XPeng', '15.67', '-0.89', '-5.38%']
    ],
    'United States': [
      ['AAPL', 'Apple Inc', '175.43', '+2.34', '+1.35%'],
      ['MSFT', 'Microsoft Corp', '420.67', '+4.12', '+0.99%'],
      ['GOOGL', 'Alphabet Inc', '140.23', '-1.23', '-0.87%'],
      ['AMZN', 'Amazon.com', '145.32', '+1.89', '+1.32%'],
      ['NVDA', 'NVIDIA Corp', '800.45', '+12.45', '+1.58%'],
      ['META', 'Meta Platforms', '345.67', '-2.34', '-0.67%'],
      ['TSLA', 'Tesla Inc', '200.78', '-5.67', '-2.75%'],
      ['JPM', 'JPMorgan Chase', '156.89', '+1.23', '+0.79%']
    ]
  };

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
      if (autoUpdate) {
        setIsUpdating(true);
        setLastUpdate(new Date());
        setTimeout(() => setIsUpdating(false), 1000);
      }
    }, 5000);
    return () => clearInterval(timer);
  }, [autoUpdate]);

  // Create market panel (responsive layout)
  const createMarketPanel = (title: string, data: string[][]) => (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      flex: '1 1 calc(33.333% - 16px)',
      minWidth: '300px',
      maxWidth: '600px',
      height: '280px',
      margin: '8px',
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Panel Header */}
      <div style={{
        backgroundColor: BLOOMBERG_DARK_BG,
        color: BLOOMBERG_ORANGE,
        padding: '8px',
        fontSize: '14px',
        fontWeight: 'bold',
        textAlign: 'center',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        {title.toUpperCase()}
      </div>

      {/* Table Header */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '2fr 1fr 1fr 1fr 1fr 1fr',
        gap: '4px',
        padding: '4px 8px',
        backgroundColor: BLOOMBERG_DARK_BG,
        color: BLOOMBERG_WHITE,
        fontSize: '12px',
        fontWeight: 'bold',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        <div>SYMBOL</div>
        <div style={{ textAlign: 'right' }}>PRICE</div>
        <div style={{ textAlign: 'right' }}>CHANGE</div>
        <div style={{ textAlign: 'right' }}>%CHG</div>
        <div style={{ textAlign: 'right' }}>7D</div>
        <div style={{ textAlign: 'right' }}>30D</div>
      </div>

      {/* Data Rows */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {data.map((row, index) => (
          <div key={index} style={{
            display: 'grid',
            gridTemplateColumns: '2fr 1fr 1fr 1fr 1fr 1fr',
            gap: '4px',
            padding: '2px 8px',
            fontSize: '10px',
            backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
            borderBottom: `1px solid rgba(120,120,120,0.3)`,
            minHeight: '18px',
            alignItems: 'center'
          }}>
            <div style={{ color: BLOOMBERG_WHITE }}>{row[0]}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{row[1]}</div>
            <div style={{
              color: row[2].startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>{row[2]}</div>
            <div style={{
              color: row[3].startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>{row[3]}</div>
            <div style={{
              color: row[4].startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>{row[4]}</div>
            <div style={{
              color: row[5].startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>{row[5]}</div>
          </div>
        ))}
      </div>
    </div>
  );

  // Create regional panel (responsive layout)
  const createRegionalPanel = (title: string, data: string[][]) => (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      flex: '1 1 calc(33.333% - 16px)',
      minWidth: '300px',
      maxWidth: '600px',
      height: '300px',
      margin: '8px',
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Panel Header */}
      <div style={{
        backgroundColor: BLOOMBERG_DARK_BG,
        color: BLOOMBERG_ORANGE,
        padding: '8px',
        fontSize: '14px',
        fontWeight: 'bold',
        textAlign: 'center',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        {title.toUpperCase()} - LIVE DATA
      </div>

      {/* Table Header */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '1fr 2fr 1fr 1fr 1fr',
        gap: '4px',
        padding: '4px 8px',
        backgroundColor: BLOOMBERG_DARK_BG,
        color: BLOOMBERG_WHITE,
        fontSize: '12px',
        fontWeight: 'bold',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        <div>SYMBOL</div>
        <div>NAME</div>
        <div style={{ textAlign: 'right' }}>PRICE</div>
        <div style={{ textAlign: 'right' }}>CHANGE</div>
        <div style={{ textAlign: 'right' }}>%CHG</div>
      </div>

      {/* Data Rows */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {data.map((row, index) => (
          <div key={index} style={{
            display: 'grid',
            gridTemplateColumns: '1fr 2fr 1fr 1fr 1fr',
            gap: '4px',
            padding: '2px 8px',
            fontSize: '10px',
            backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
            borderBottom: `1px solid rgba(120,120,120,0.3)`,
            minHeight: '18px',
            alignItems: 'center'
          }}>
            <div style={{ color: BLOOMBERG_WHITE }}>{row[0]}</div>
            <div style={{ color: BLOOMBERG_WHITE, overflow: 'hidden', textOverflow: 'ellipsis' }}>{row[1]}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{row[2]}</div>
            <div style={{
              color: row[3].startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>{row[3]}</div>
            <div style={{
              color: row[4].startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>{row[4]}</div>
          </div>
        ))}
      </div>
    </div>
  );

  const categories = Object.keys(marketData);

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        /* Custom scrollbar styles */
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: #1a1a1a;
        }
        *::-webkit-scrollbar-thumb {
          background: #404040;
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #525252;
        }
      `}</style>
      {/* Header Bar - Responsive */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        flexWrap: 'wrap',
        padding: '8px 12px',
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        fontSize: '13px',
        gap: '8px',
        flexShrink: 0
      }}>
        <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT</span>
        <span style={{ color: BLOOMBERG_WHITE }}>MARKET TERMINAL</span>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <input
          type="text"
          placeholder="Search Symbol"
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '4px 8px',
            flex: '0 1 200px',
            minWidth: '120px',
            fontSize: '11px'
          }}
        />
        <button style={{
          backgroundColor: BLOOMBERG_ORANGE,
          color: 'black',
          border: 'none',
          padding: '4px 12px',
          fontSize: '11px',
          fontWeight: 'bold',
          cursor: 'pointer',
          minWidth: '70px'
        }}>
          SEARCH
        </button>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
          {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
        </span>
      </div>

      {/* Control Panel - Responsive */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        flexWrap: 'wrap',
        padding: '8px 12px',
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        fontSize: '12px',
        gap: '8px',
        flexShrink: 0
      }}>
        <button
          onClick={() => { setIsUpdating(true); setLastUpdate(new Date()); setTimeout(() => setIsUpdating(false), 1000); }}
          style={{
            backgroundColor: BLOOMBERG_ORANGE,
            color: 'black',
            border: 'none',
            padding: '4px 12px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer',
            minWidth: '70px'
          }}
        >
          REFRESH
        </button>
        <button
          onClick={() => setAutoUpdate(!autoUpdate)}
          style={{
            backgroundColor: autoUpdate ? BLOOMBERG_GREEN : BLOOMBERG_GRAY,
            color: 'black',
            border: 'none',
            padding: '4px 12px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer',
            minWidth: '70px'
          }}
        >
          AUTO {autoUpdate ? 'ON' : 'OFF'}
        </button>
        <select style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          border: `1px solid ${BLOOMBERG_GRAY}`,
          color: BLOOMBERG_WHITE,
          padding: '4px 8px',
          fontSize: '11px',
          minWidth: '70px',
          cursor: 'pointer'
        }}>
          <option>1 min</option>
          <option>5 min</option>
          <option>10 min</option>
          <option>30 min</option>
        </select>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: BLOOMBERG_GRAY, fontSize: '11px' }}>LAST UPDATE:</span>
        <span style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
          {lastUpdate.toTimeString().substring(0, 8)}
        </span>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: isUpdating ? BLOOMBERG_ORANGE : BLOOMBERG_GREEN, fontSize: '14px' }}>●</span>
        <span style={{ color: isUpdating ? BLOOMBERG_ORANGE : BLOOMBERG_GREEN, fontSize: '11px', fontWeight: 'bold' }}>
          {isUpdating ? 'UPDATING' : 'LIVE'}
        </span>
      </div>

      {/* Main Content - Responsive */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '12px',
        display: 'flex',
        flexDirection: 'column'
      }}>
        {/* Global Markets Section */}
        <div style={{
          color: BLOOMBERG_ORANGE,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '8px',
          letterSpacing: '0.5px'
        }}>
          GLOBAL MARKETS
        </div>
        <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '16px' }}></div>

        {/* Responsive Market Grid */}
        <div style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '0',
          marginBottom: '24px'
        }}>
          {categories.map(category =>
            createMarketPanel(category, marketData[category])
          )}
        </div>

        {/* Regional Markets Section */}
        <div style={{
          color: BLOOMBERG_ORANGE,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '8px',
          marginTop: '16px',
          letterSpacing: '0.5px'
        }}>
          REGIONAL MARKETS - LIVE DATA
        </div>
        <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '16px' }}></div>

        {/* Responsive Regional Panels */}
        <div style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '0'
        }}>
          {Object.keys(regionalData).map(region =>
            createRegionalPanel(region, regionalData[region])
          )}
        </div>
      </div>

      {/* Status Bar - Responsive */}
      <div style={{
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        backgroundColor: BLOOMBERG_PANEL_BG,
        padding: '6px 12px',
        fontSize: '10px',
        color: BLOOMBERG_GRAY,
        flexShrink: 0
      }}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexWrap: 'wrap',
          gap: '8px'
        }}>
          <span>Data provided by Bloomberg Terminal API | Updates every 5 seconds</span>
          <span style={{ whiteSpace: 'nowrap' }}>Connected: {Object.keys(regionalData).length} regional markets</span>
        </div>
      </div>
    </div>
  );
};

export default MarketsTab;