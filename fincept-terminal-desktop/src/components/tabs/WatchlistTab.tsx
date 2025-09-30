import React, { useState, useEffect } from 'react';

interface WatchlistStock {
  ticker: string;
  name: string;
  price: number;
  change: number;
  changePercent: number;
  volume: number;
  high: number;
  low: number;
  open: number;
  prevClose: number;
  marketCap: string;
  pe: number;
  yield: number;
  week52High: number;
  week52Low: number;
  avgVolume: string;
}

const WatchlistTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedList, setSelectedList] = useState('PRIMARY');
  const [sortBy, setSortBy] = useState('CHANGE');
  const [viewMode, setViewMode] = useState('DETAILED');

  // Bloomberg color scheme
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
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  const watchlistStocks: WatchlistStock[] = [
    {
      ticker: 'AAPL',
      name: 'Apple Inc.',
      price: 184.92,
      change: 1.87,
      changePercent: 1.02,
      volume: 48234567,
      high: 186.45,
      low: 182.30,
      open: 183.50,
      prevClose: 183.05,
      marketCap: '2.87T',
      pe: 29.8,
      yield: 0.52,
      week52High: 198.23,
      week52Low: 164.08,
      avgVolume: '52.3M'
    },
    {
      ticker: 'MSFT',
      name: 'Microsoft Corporation',
      price: 378.45,
      change: 3.82,
      changePercent: 1.02,
      volume: 23456789,
      high: 380.12,
      low: 374.56,
      open: 375.20,
      prevClose: 374.63,
      marketCap: '2.81T',
      pe: 35.2,
      yield: 0.78,
      week52High: 398.45,
      week52Low: 309.45,
      avgVolume: '26.8M'
    },
    {
      ticker: 'NVDA',
      name: 'NVIDIA Corporation',
      price: 501.23,
      change: 5.12,
      changePercent: 1.03,
      volume: 35678901,
      high: 505.67,
      low: 495.34,
      open: 497.80,
      prevClose: 496.11,
      marketCap: '1.24T',
      pe: 67.2,
      yield: 0.03,
      week52High: 502.66,
      week52Low: 108.13,
      avgVolume: '41.2M'
    },
    {
      ticker: 'GOOGL',
      name: 'Alphabet Inc.',
      price: 134.56,
      change: -1.23,
      changePercent: -0.91,
      volume: 28901234,
      high: 136.78,
      low: 133.45,
      open: 135.90,
      prevClose: 135.79,
      marketCap: '1.68T',
      pe: 25.4,
      yield: 0.00,
      week52High: 153.78,
      week52Low: 83.34,
      avgVolume: '31.5M'
    },
    {
      ticker: 'TSLA',
      name: 'Tesla Inc.',
      price: 251.78,
      change: -2.54,
      changePercent: -1.00,
      volume: 89012345,
      high: 256.45,
      low: 249.12,
      open: 254.30,
      prevClose: 254.32,
      marketCap: '798.4B',
      pe: 68.4,
      yield: 0.00,
      week52High: 299.29,
      week52Low: 101.81,
      avgVolume: '112.5M'
    },
    {
      ticker: 'AMZN',
      name: 'Amazon.com Inc.',
      price: 151.94,
      change: 1.45,
      changePercent: 0.96,
      volume: 45678901,
      high: 153.23,
      low: 150.12,
      open: 150.50,
      prevClose: 150.49,
      marketCap: '1.57T',
      pe: 58.9,
      yield: 0.00,
      week52High: 161.72,
      week52Low: 81.43,
      avgVolume: '51.8M'
    },
    {
      ticker: 'META',
      name: 'Meta Platforms Inc.',
      price: 389.67,
      change: 3.21,
      changePercent: 0.83,
      volume: 18234567,
      high: 392.45,
      low: 385.34,
      open: 386.50,
      prevClose: 386.46,
      marketCap: '987.3B',
      pe: 28.7,
      yield: 0.44,
      week52High: 401.82,
      week52Low: 88.09,
      avgVolume: '21.3M'
    },
    {
      ticker: 'BRK.B',
      name: 'Berkshire Hathaway Inc.',
      price: 389.45,
      change: 0.87,
      changePercent: 0.22,
      volume: 3456789,
      high: 391.23,
      low: 387.89,
      open: 388.60,
      prevClose: 388.58,
      marketCap: '854.2B',
      pe: 21.3,
      yield: 0.00,
      week52High: 404.52,
      week52Low: 298.45,
      avgVolume: '4.2M'
    }
  ];

  const watchlistGroups = [
    { name: 'PRIMARY', count: 8, color: BLOOMBERG_ORANGE },
    { name: 'TECH', count: 12, color: BLOOMBERG_BLUE },
    { name: 'ENERGY', count: 6, color: BLOOMBERG_GREEN },
    { name: 'FINANCE', count: 9, color: BLOOMBERG_CYAN },
    { name: 'HEALTHCARE', count: 7, color: BLOOMBERG_PURPLE }
  ];

  const marketStats = {
    gainers: 234,
    losers: 156,
    unchanged: 45,
    highVolume: 89,
    allTimeHigh: 12,
    allTimeLow: 8
  };

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
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT WATCHLIST MONITOR</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN }}>● LIVE QUOTES</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_CYAN }}>STOCKS TRACKED: {watchlistStocks.length}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>LISTS: {watchlistGroups.length}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>MARKET:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>GAINERS {marketStats.gainers}</span>
          <span style={{ color: BLOOMBERG_RED }}>LOSERS {marketStats.losers}</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>UNCHANGED {marketStats.unchanged}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>ALERTS:</span>
          <span style={{ color: BLOOMBERG_CYAN }}>HIGH VOL {marketStats.highVolume}</span>
          <span style={{ color: BLOOMBERG_GREEN }}>52W HIGH {marketStats.allTimeHigh}</span>
          <span style={{ color: BLOOMBERG_RED }}>52W LOW {marketStats.allTimeLow}</span>
        </div>
      </div>

      {/* Function Keys */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '2px 4px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
          {[
            { key: "F1", label: "DETAILED", view: "DETAILED" },
            { key: "F2", label: "COMPACT", view: "COMPACT" },
            { key: "F3", label: "CHARTS", view: "CHARTS" },
            { key: "F4", label: "ADD", view: "ADD" },
            { key: "F5", label: "REMOVE", view: "REMOVE" },
            { key: "F6", label: "ALERTS", view: "ALERTS" },
            { key: "F7", label: "SORT", view: "SORT" },
            { key: "F8", label: "FILTER", view: "FILTER" },
            { key: "F9", label: "IMPORT", view: "IMPORT" },
            { key: "F10", label: "EXPORT", view: "EXPORT" },
            { key: "F11", label: "SETTINGS", view: "SETTINGS" },
            { key: "F12", label: "REFRESH", view: "REFRESH" }
          ].map(item => (
            <button key={item.key}
              onClick={() => setViewMode(item.view)}
              style={{
                backgroundColor: viewMode === item.view ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: viewMode === item.view ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: viewMode === item.view ? 'bold' : 'normal',
                cursor: 'pointer'
              }}>
              <span style={{ color: BLOOMBERG_YELLOW }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
        <div style={{ display: 'flex', gap: '4px', height: '100%' }}>

          {/* Left Panel - Watchlist Groups */}
          <div style={{
            width: '280px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              WATCHLIST GROUPS
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {watchlistGroups.map((group, index) => (
              <button key={index}
                onClick={() => setSelectedList(group.name)}
                style={{
                  width: '100%',
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '6px',
                  marginBottom: '3px',
                  backgroundColor: selectedList === group.name ? 'rgba(255,165,0,0.1)' : 'rgba(255,255,255,0.02)',
                  border: `1px solid ${selectedList === group.name ? BLOOMBERG_ORANGE : 'transparent'}`,
                  textAlign: 'left',
                  cursor: 'pointer',
                  fontSize: '10px'
                }}>
                <span style={{ color: group.color, fontWeight: 'bold' }}>{group.name}</span>
                <span style={{ color: BLOOMBERG_CYAN }}>{group.count} stocks</span>
              </button>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                MARKET MOVERS
              </div>
              {[
                { ticker: 'NVDA', change: '+5.12%', type: 'GAINER', color: BLOOMBERG_GREEN },
                { ticker: 'META', change: '+3.21%', type: 'GAINER', color: BLOOMBERG_GREEN },
                { ticker: 'TSLA', change: '-2.54%', type: 'LOSER', color: BLOOMBERG_RED },
                { ticker: 'GOOGL', change: '-1.23%', type: 'LOSER', color: BLOOMBERG_RED }
              ].map((mover, index) => (
                <div key={index} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '3px',
                  marginBottom: '2px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  fontSize: '9px'
                }}>
                  <div>
                    <span style={{ color: BLOOMBERG_CYAN }}>{mover.ticker}</span>
                    <span style={{ color: BLOOMBERG_GRAY, marginLeft: '4px', fontSize: '8px' }}>[{mover.type}]</span>
                  </div>
                  <span style={{ color: mover.color, fontWeight: 'bold' }}>{mover.change}</span>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                VOLUME LEADERS
              </div>
              {[
                { ticker: 'TSLA', volume: '89.0M', pctAvg: '+179%' },
                { ticker: 'AAPL', volume: '48.2M', pctAvg: '-8%' },
                { ticker: 'AMZN', volume: '45.7M', pctAvg: '-12%' },
                { ticker: 'NVDA', volume: '35.7M', pctAvg: '-13%' }
              ].map((stock, index) => (
                <div key={index} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '3px',
                  marginBottom: '2px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  fontSize: '9px'
                }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>{stock.ticker}</span>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{ color: BLOOMBERG_WHITE }}>{stock.volume}</div>
                    <div style={{ color: stock.pctAvg.startsWith('+') ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '8px' }}>
                      {stock.pctAvg}
                    </div>
                  </div>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                QUICK ACTIONS
              </div>
              {[
                { label: 'Add Stock', color: BLOOMBERG_GREEN },
                { label: 'Create List', color: BLOOMBERG_BLUE },
                { label: 'Set Alert', color: BLOOMBERG_YELLOW },
                { label: 'Import CSV', color: BLOOMBERG_CYAN },
                { label: 'Export Data', color: BLOOMBERG_PURPLE }
              ].map((action, index) => (
                <button key={index} style={{
                  width: '100%',
                  padding: '4px',
                  marginBottom: '2px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: action.color,
                  fontSize: '9px',
                  cursor: 'pointer',
                  textAlign: 'left'
                }}>
                  {action.label}
                </button>
              ))}
            </div>
          </div>

          {/* Center Panel - Stock List */}
          <div style={{
            flex: 1,
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
                WATCHLIST: {selectedList}
              </div>
              <div style={{ display: 'flex', gap: '4px', fontSize: '9px' }}>
                <button onClick={() => setSortBy('TICKER')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'TICKER' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'TICKER' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>TICKER</button>
                <button onClick={() => setSortBy('CHANGE')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'CHANGE' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'CHANGE' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>CHANGE</button>
                <button onClick={() => setSortBy('VOLUME')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'VOLUME' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'VOLUME' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>VOLUME</button>
              </div>
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '80px 200px 90px 90px 90px 90px 90px 90px',
              gap: '4px',
              padding: '4px',
              backgroundColor: 'rgba(255,255,255,0.05)',
              fontSize: '9px',
              fontWeight: 'bold',
              marginBottom: '2px'
            }}>
              <div style={{ color: BLOOMBERG_GRAY }}>TICKER</div>
              <div style={{ color: BLOOMBERG_GRAY }}>NAME</div>
              <div style={{ color: BLOOMBERG_GRAY }}>PRICE</div>
              <div style={{ color: BLOOMBERG_GRAY }}>CHANGE</div>
              <div style={{ color: BLOOMBERG_GRAY }}>VOLUME</div>
              <div style={{ color: BLOOMBERG_GRAY }}>HIGH/LOW</div>
              <div style={{ color: BLOOMBERG_GRAY }}>MKT CAP</div>
              <div style={{ color: BLOOMBERG_GRAY }}>P/E</div>
            </div>

            {/* Stock Rows */}
            {watchlistStocks.map((stock, index) => (
              <div key={index} style={{
                display: 'grid',
                gridTemplateColumns: '80px 200px 90px 90px 90px 90px 90px 90px',
                gap: '4px',
                padding: '6px 4px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                borderLeft: `3px solid ${stock.change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED}`,
                fontSize: '10px',
                marginBottom: '1px'
              }}>
                <div style={{ color: BLOOMBERG_CYAN, fontWeight: 'bold' }}>{stock.ticker}</div>
                <div style={{ color: BLOOMBERG_WHITE, overflow: 'hidden', textOverflow: 'ellipsis' }}>{stock.name}</div>
                <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>${stock.price.toFixed(2)}</div>
                <div>
                  <div style={{ color: stock.change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
                    {stock.change >= 0 ? '+' : ''}{stock.change.toFixed(2)}
                  </div>
                  <div style={{ color: stock.change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '8px' }}>
                    ({stock.changePercent >= 0 ? '+' : ''}{stock.changePercent.toFixed(2)}%)
                  </div>
                </div>
                <div style={{ color: BLOOMBERG_YELLOW }}>{(stock.volume / 1000000).toFixed(1)}M</div>
                <div style={{ fontSize: '9px' }}>
                  <div style={{ color: BLOOMBERG_GREEN }}>${stock.high.toFixed(2)}</div>
                  <div style={{ color: BLOOMBERG_RED }}>${stock.low.toFixed(2)}</div>
                </div>
                <div style={{ color: BLOOMBERG_CYAN }}>{stock.marketCap}</div>
                <div style={{ color: BLOOMBERG_WHITE }}>{stock.pe.toFixed(1)}</div>
              </div>
            ))}
          </div>

          {/* Right Panel - Stock Details */}
          <div style={{
            width: '320px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              STOCK DETAIL: {watchlistStocks[0].ticker}
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {/* Price Info */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ padding: '6px', backgroundColor: 'rgba(255,165,0,0.1)', marginBottom: '4px' }}>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>LAST PRICE</div>
                <div style={{ color: BLOOMBERG_WHITE, fontSize: '20px', fontWeight: 'bold' }}>
                  ${watchlistStocks[0].price.toFixed(2)}
                </div>
                <div style={{ color: watchlistStocks[0].change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '12px' }}>
                  {watchlistStocks[0].change >= 0 ? '+' : ''}{watchlistStocks[0].change.toFixed(2)} (
                  {watchlistStocks[0].changePercent >= 0 ? '+' : ''}{watchlistStocks[0].changePercent.toFixed(2)}%)
                </div>
              </div>
            </div>

            {/* Trading Stats */}
            <div style={{ fontSize: '9px', lineHeight: '1.5', marginBottom: '12px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>Open:</span>
                <span style={{ color: BLOOMBERG_WHITE }}>${watchlistStocks[0].open.toFixed(2)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>Prev Close:</span>
                <span style={{ color: BLOOMBERG_WHITE }}>${watchlistStocks[0].prevClose.toFixed(2)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>Day High:</span>
                <span style={{ color: BLOOMBERG_GREEN }}>${watchlistStocks[0].high.toFixed(2)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>Day Low:</span>
                <span style={{ color: BLOOMBERG_RED }}>${watchlistStocks[0].low.toFixed(2)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>52W High:</span>
                <span style={{ color: BLOOMBERG_GREEN }}>${watchlistStocks[0].week52High.toFixed(2)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>52W Low:</span>
                <span style={{ color: BLOOMBERG_RED }}>${watchlistStocks[0].week52Low.toFixed(2)}</span>
              </div>
              <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '4px', paddingTop: '4px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Volume:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>{(watchlistStocks[0].volume / 1000000).toFixed(1)}M</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Avg Volume:</span>
                  <span style={{ color: BLOOMBERG_CYAN }}>{watchlistStocks[0].avgVolume}</span>
                </div>
              </div>
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, paddingTop: '8px', marginBottom: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                FUNDAMENTAL DATA
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.5' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Market Cap:</span>
                  <span style={{ color: BLOOMBERG_WHITE }}>{watchlistStocks[0].marketCap}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>P/E Ratio:</span>
                  <span style={{ color: BLOOMBERG_CYAN }}>{watchlistStocks[0].pe.toFixed(2)}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Dividend Yield:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>{watchlistStocks[0].yield.toFixed(2)}%</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>EPS (TTM):</span>
                  <span style={{ color: BLOOMBERG_WHITE }}>$6.42</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Beta:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>1.23</span>
                </div>
              </div>
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                PRICE ALERTS
              </div>
              {[
                { type: 'ABOVE', price: 190.00, status: 'ACTIVE', color: BLOOMBERG_GREEN },
                { type: 'BELOW', price: 175.00, status: 'ACTIVE', color: BLOOMBERG_RED },
                { type: 'TARGET', price: 200.00, status: 'PENDING', color: BLOOMBERG_YELLOW }
              ].map((alert, index) => (
                <div key={index} style={{
                  marginBottom: '4px',
                  padding: '4px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  borderLeft: `2px solid ${alert.color}`,
                  fontSize: '9px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: alert.color, fontWeight: 'bold' }}>[{alert.type}]</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>{alert.status}</span>
                  </div>
                  <div style={{ color: BLOOMBERG_WHITE }}>Target: ${alert.price.toFixed(2)}</div>
                </div>
              ))}

              <button style={{
                width: '100%',
                marginTop: '6px',
                padding: '4px',
                backgroundColor: 'rgba(255,165,0,0.1)',
                border: `1px solid ${BLOOMBERG_ORANGE}`,
                color: BLOOMBERG_ORANGE,
                fontSize: '9px',
                cursor: 'pointer'
              }}>
                + CREATE NEW ALERT
              </button>
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
            <span>Fincept Watchlist Monitor v1.8.2 | Real-time market data</span>
            <span>Tracking: {watchlistStocks.length} stocks | Lists: {watchlistGroups.length}</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>List: {selectedList}</span>
            <span>Sort: {sortBy}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>QUOTES: LIVE</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default WatchlistTab;