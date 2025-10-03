import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';


const testGreet = async () => {
  try {
    const greeting = await invoke('greet', { name: 'Bloomberg User' });
    console.log('Greeting from Rust:', greeting);
    // You can also show this in an alert or update state
    alert(greeting);
  } catch (error) {
    console.error('Error calling greet command:', error);
  }
};

async function fetchStockData() {
    try {
        const stockData = await invoke('get_stock_data', { 
            symbol: 'AAPL',
            period: '1mo'
        });
        console.log('Stock data:', stockData);
        return stockData;
    } catch (error) {
        console.error('Error fetching stock data:', error);
        throw error;
    }
}
console.log('Fetching stock data for AAPL...');
await fetchStockData();

const DashboardTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [stockData, setStockData] = useState({
    'AAPL': { price: 175.43, change: 2.34, changePct: 1.35, volume: 45200000, high: 177.89, low: 174.12, open: 175.00 },
    'MSFT': { price: 420.67, change: 4.12, changePct: 0.99, volume: 28700000, high: 422.45, low: 418.90, open: 420.00 },
    'GOOGL': { price: 140.23, change: -1.23, changePct: -0.87, volume: 32100000, high: 142.45, low: 139.78, open: 141.50 },
    'AMZN': { price: 145.32, change: 1.89, changePct: 1.32, volume: 41800000, high: 147.23, low: 144.56, open: 145.00 },
    'NVDA': { price: 800.45, change: 12.45, changePct: 1.58, volume: 52300000, high: 805.67, low: 795.23, open: 798.00 },
    'META': { price: 345.67, change: -2.34, changePct: -0.67, volume: 31500000, high: 348.90, low: 344.12, open: 347.50 }
  });

  const [indices, setIndices] = useState({
    'S&P 500': { value: 5234.18, change: 0.45 },
    'NASDAQ': { value: 16421.67, change: -0.09 },
    'DOW JONES': { value: 38567.89, change: 0.41 },
    'RUSSELL 2000': { value: 2089.45, change: 0.43 },
    'VIX': { value: 18.23, change: -2.41 },
    'FTSE 100': { value: 7634.56, change: 0.16 }
  });

  const [economicIndicators, setEconomicIndicators] = useState({
    'USD/EUR': { value: 1.0845, change: 0.21 },
    'USD/GBP': { value: 1.2578, change: -0.10 },
    'GOLD': { value: 2318.45, change: 0.55 },
    'OIL WTI': { value: 78.89, change: -1.53 },
    '10Y TREASURY': { value: 4.35, change: 0.46 },
    'BITCOIN': { value: 67234.56, change: 1.87 }
  });

  const [newsHeadlines, setNewsHeadlines] = useState([
    'Federal Reserve Signals Potential Rate Cut in Q2 2024',
    'Tech Stocks Rally on AI Investment Surge',
    'Oil Prices Drop on Increased Production Outlook',
    'European Markets Open Higher on GDP Data',
    'Cryptocurrency Regulation Bill Advances in Senate',
    'Banking Sector Shows Strong Q4 Earnings',
    'Supply Chain Disruptions Impact Manufacturing',
    'Green Energy Investments Reach Record High'
  ]);

  // Bloomberg color scheme (exact from Python)
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#1a1a1a';
  const BLOOMBERG_PANEL_BG = '#000000';

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Create left panel (Market Monitor) - Responsive
  const createLeftPanel = () => (
    <div style={{
      flex: '1 1 300px',
      minWidth: '280px',
      maxWidth: '400px',
      minHeight: '500px',
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      padding: '8px',
      overflow: 'auto'
    }}>
      <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
        MARKET MONITOR
      </div>
      <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

      {/* Global Indices */}
      <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
        GLOBAL INDICES
      </div>
      <div style={{
        display: 'grid',
        gridTemplateColumns: '2fr 1fr 1fr',
        gap: '4px',
        fontSize: '9px',
        marginBottom: '12px'
      }}>
        <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', borderBottom: `1px solid ${BLOOMBERG_GRAY}`, padding: '2px 0' }}>Index</div>
        <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', borderBottom: `1px solid ${BLOOMBERG_GRAY}`, padding: '2px 0', textAlign: 'right' }}>Value</div>
        <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', borderBottom: `1px solid ${BLOOMBERG_GRAY}`, padding: '2px 0', textAlign: 'right' }}>Change %</div>

        {Object.entries(indices).map(([index, data]) => (
          <React.Fragment key={index}>
            <div style={{ color: BLOOMBERG_WHITE, padding: '2px 0' }}>{index}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right', padding: '2px 0' }}>{data.value.toFixed(2)}</div>
            <div style={{
              color: data.change > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right',
              padding: '2px 0'
            }}>
              {data.change > 0 ? '+' : ''}{data.change.toFixed(2)}%
            </div>
          </React.Fragment>
        ))}
      </div>

      <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

      {/* Economic Indicators */}
      <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
        ECONOMIC INDICATORS
      </div>
      <div style={{
        display: 'grid',
        gridTemplateColumns: '2fr 1fr 1fr',
        gap: '4px',
        fontSize: '9px',
        marginBottom: '12px'
      }}>
        <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', borderBottom: `1px solid ${BLOOMBERG_GRAY}`, padding: '2px 0' }}>Indicator</div>
        <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', borderBottom: `1px solid ${BLOOMBERG_GRAY}`, padding: '2px 0', textAlign: 'right' }}>Value</div>
        <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold', borderBottom: `1px solid ${BLOOMBERG_GRAY}`, padding: '2px 0', textAlign: 'right' }}>Change</div>

        {Object.entries(economicIndicators).map(([indicator, data]) => (
          <React.Fragment key={indicator}>
            <div style={{ color: BLOOMBERG_WHITE, padding: '2px 0' }}>{indicator}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right', padding: '2px 0' }}>{data.value.toFixed(2)}</div>
            <div style={{
              color: data.change > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right',
              padding: '2px 0'
            }}>
              {data.change > 0 ? '+' : ''}{data.change.toFixed(2)}
            </div>
          </React.Fragment>
        ))}
      </div>

      <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

      {/* Latest News */}
      <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
        LATEST NEWS
      </div>
      {newsHeadlines.slice(0, 4).map((headline, index) => (
        <div key={index} style={{ marginBottom: '8px' }}>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '9px', lineHeight: '1.3' }}>
            {currentTime.toTimeString().substring(0, 5)} - {headline.length > 50 ? headline.substring(0, 47) + '...' : headline}
          </div>
        </div>
      ))}
    </div>
  );

  // Create center panel (Stock Data with Tabs) - Responsive
  const createCenterPanel = () => (
    <div style={{
      flex: '2 1 500px',
      minWidth: '400px',
      minHeight: '500px',
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      padding: '8px',
      overflow: 'auto'
    }}>
      {/* Tab Bar */}
      <div style={{ display: 'flex', marginBottom: '12px', borderBottom: `1px solid ${BLOOMBERG_GRAY}` }}>
        <div style={{
          padding: '8px 16px',
          backgroundColor: BLOOMBERG_ORANGE,
          color: 'black',
          fontSize: '11px',
          fontWeight: 'bold',
          marginRight: '2px'
        }}>
          Market Data
        </div>
        <div style={{
          padding: '8px 16px',
          backgroundColor: BLOOMBERG_DARK_BG,
          color: BLOOMBERG_WHITE,
          fontSize: '11px',
          fontWeight: 'bold',
          marginRight: '2px'
        }}>
          Charts
        </div>
        <div style={{
          padding: '8px 16px',
          backgroundColor: BLOOMBERG_DARK_BG,
          color: BLOOMBERG_WHITE,
          fontSize: '11px',
          fontWeight: 'bold'
        }}>
          News
        </div>
      </div>

      {/* Market Data Tab Content */}
      <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
        TOP STOCKS
      </div>

      {/* Stock Table */}
      <div style={{ marginBottom: '20px' }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr 1fr 1fr 1.5fr 1fr 1fr',
          gap: '4px',
          fontSize: '10px',
          fontWeight: 'bold',
          borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
          padding: '4px 0',
          marginBottom: '4px'
        }}>
          <div style={{ color: BLOOMBERG_WHITE }}>Ticker</div>
          <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>Last</div>
          <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>Chg</div>
          <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>Chg%</div>
          <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>Volume</div>
          <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>High</div>
          <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>Low</div>
        </div>

        {Object.entries(stockData).map(([ticker, data]) => (
          <div key={ticker} style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr 1fr 1fr 1.5fr 1fr 1fr',
            gap: '4px',
            fontSize: '9px',
            borderBottom: `1px solid rgba(120,120,120,0.3)`,
            padding: '2px 0',
            alignItems: 'center'
          }}>
            <div style={{ color: BLOOMBERG_WHITE }}>{ticker}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{data.price.toFixed(2)}</div>
            <div style={{
              color: data.change > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>
              {data.change > 0 ? '+' : ''}{data.change.toFixed(2)}
            </div>
            <div style={{
              color: data.changePct > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>
              {data.changePct > 0 ? '+' : ''}{data.changePct.toFixed(2)}%
            </div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{data.volume.toLocaleString()}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{data.high.toFixed(2)}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{data.low.toFixed(2)}</div>
          </div>
        ))}
      </div>

      <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

      {/* Stock Details */}
      <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
        STOCK DETAILS
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
        <input
          type="text"
          defaultValue="AAPL"
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '4px 8px',
            fontSize: '11px',
            width: '150px'
          }}
        />
        <button style={{
          backgroundColor: BLOOMBERG_ORANGE,
          color: 'black',
          border: 'none',
          padding: '4px 12px',
          fontSize: '11px',
          fontWeight: 'bold'
        }}>
          Load
        </button>
      </div>

      {/* AAPL Details */}
      <div style={{ display: 'flex', gap: '20px' }}>
        <div>
          <div style={{ color: BLOOMBERG_ORANGE, fontSize: '12px', fontWeight: 'bold' }}>Apple Inc (AAPL US Equity)</div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px', marginBottom: '4px' }}>Technology - Consumer Electronics</div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>Last Price: {stockData.AAPL.price.toFixed(2)}</div>
          <div style={{
            color: stockData.AAPL.changePct > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
            fontSize: '10px'
          }}>
            Change: {stockData.AAPL.change > 0 ? '+' : ''}{stockData.AAPL.change.toFixed(2)} ({stockData.AAPL.changePct > 0 ? '+' : ''}{stockData.AAPL.changePct.toFixed(2)}%)
          </div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>Volume: {stockData.AAPL.volume.toLocaleString()}</div>
        </div>
        <div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>High: {stockData.AAPL.high.toFixed(2)}</div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>Low: {stockData.AAPL.low.toFixed(2)}</div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>Open: {stockData.AAPL.open.toFixed(2)}</div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>P/E Ratio: 28.5</div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>Market Cap: $2.8T</div>
        </div>
      </div>
    </div>
  );

  // Create right panel (Command Line) - Responsive
  const createRightPanel = () => (
    <div style={{
      flex: '1 1 300px',
      minWidth: '280px',
      maxWidth: '400px',
      minHeight: '500px',
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      padding: '8px',
      overflow: 'auto'
    }}>
      <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
        COMMAND LINE
      </div>
      <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

      {/* Command History */}
      <div style={{
        height: '200px',
        backgroundColor: BLOOMBERG_DARK_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '8px',
        marginBottom: '12px',
        overflow: 'auto'
      }}>
        <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px', marginBottom: '4px' }}>&gt; AAPL US Equity &lt;GO&gt;</div>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', marginBottom: '8px', marginLeft: '8px' }}>Loading AAPL US Equity...</div>
        <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px', marginBottom: '4px' }}>&gt; TOP &lt;GO&gt;</div>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', marginBottom: '8px', marginLeft: '8px' }}>Loading TOP news...</div>
        <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px', marginBottom: '4px' }}>&gt; WEI &lt;GO&gt;</div>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', marginBottom: '8px', marginLeft: '8px' }}>Loading World Equity Indices...</div>
      </div>

      {/* Command Input */}
      <div style={{ display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
        <span style={{ color: BLOOMBERG_WHITE, marginRight: '8px' }}>&gt;</span>
        <input
          type="text"
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '4px 8px',
            fontSize: '11px',
            flex: 1
          }}
        />
      </div>
      <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '12px' }}>
        &lt;HELP&gt; for commands. Press &lt;GO&gt; to execute.
      </div>

      <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

      {/* Quick Actions */}
      <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
        QUICK ACTIONS
      </div>
      <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
        <button style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          border: `1px solid ${BLOOMBERG_GRAY}`,
          color: BLOOMBERG_WHITE,
          padding: '4px 8px',
          fontSize: '10px',
          textAlign: 'left'
        }}>
          WEI - World Equity Indices
        </button>
        <button style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          border: `1px solid ${BLOOMBERG_GRAY}`,
          color: BLOOMBERG_WHITE,
          padding: '4px 8px',
          fontSize: '10px',
          textAlign: 'left'
        }}>
          TOP - Top News
        </button>
        <button style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          border: `1px solid ${BLOOMBERG_GRAY}`,
          color: BLOOMBERG_WHITE,
          padding: '4px 8px',
          fontSize: '10px',
          textAlign: 'left'
        }}>
          CURY - Currency Rates
        </button>
        <button style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          border: `1px solid ${BLOOMBERG_GRAY}`,
          color: BLOOMBERG_WHITE,
          padding: '4px 8px',
          fontSize: '10px',
          textAlign: 'left'
        }}>
          PORT - Portfolio
        </button>
      </div>

      {/* Status */}
      <div style={{ marginTop: '16px' }}>
        <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
          STATUS
        </div>
        <div style={{ color: BLOOMBERG_GREEN, fontSize: '10px' }}>● Market Data: LIVE</div>
        <div style={{ color: BLOOMBERG_GREEN, fontSize: '10px' }}>● News Feed: ACTIVE</div>
        <div style={{ color: BLOOMBERG_GREEN, fontSize: '10px' }}>● API: CONNECTED</div>
        <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px' }}>● Latency: ~200ms</div>
      </div>
    </div>
  );

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
      {/* Header - Responsive */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        flexWrap: 'wrap',
        padding: '8px 12px',
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        fontSize: '12px',
        gap: '12px',
        flexShrink: 0
      }}>
        <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>BLOOMBERG TERMINAL</span>
        <span style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
          {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
        </span>
        <div style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: BLOOMBERG_GREEN, fontSize: '14px' }}>●</span>
          <span style={{ color: BLOOMBERG_GREEN, fontSize: '10px', fontWeight: 'bold' }}>LIVE</span>
        </div>
      </div>

      {/* Main Content - Responsive 3 Panel Layout */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '12px',
        display: 'flex',
        flexDirection: 'column'
      }}>
        <div style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '12px',
          justifyContent: 'center'
        }}>
          {createLeftPanel()}
          {createCenterPanel()}
          {createRightPanel()}
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
          <span>Bloomberg Professional Service | Real-time financial data</span>
          <span style={{ whiteSpace: 'nowrap' }}>Session: {currentTime.toTimeString().substring(0, 8)}</span>
        </div>
      </div>
    </div>
  );
};

export default DashboardTab;