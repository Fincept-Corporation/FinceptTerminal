import React, { useState, useEffect } from 'react';

interface Position {
  ticker: string;
  name: string;
  quantity: number;
  avgPrice: number;
  currentPrice: number;
  marketValue: number;
  costBasis: number;
  unrealizedPnL: number;
  unrealizedPnLPercent: number;
  dayChange: number;
  dayChangePercent: number;
  weight: number;
  sector: string;
}

const PortfolioTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedView, setSelectedView] = useState('POSITIONS');
  const [sortBy, setSortBy] = useState('WEIGHT');

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

  const positions: Position[] = [
    {
      ticker: 'AAPL',
      name: 'Apple Inc.',
      quantity: 250,
      avgPrice: 165.50,
      currentPrice: 184.92,
      marketValue: 46230.00,
      costBasis: 41375.00,
      unrealizedPnL: 4855.00,
      unrealizedPnLPercent: 11.73,
      dayChange: 462.30,
      dayChangePercent: 1.01,
      weight: 28.5,
      sector: 'Technology'
    },
    {
      ticker: 'MSFT',
      name: 'Microsoft Corporation',
      quantity: 120,
      avgPrice: 340.00,
      currentPrice: 378.45,
      marketValue: 45414.00,
      costBasis: 40800.00,
      unrealizedPnL: 4614.00,
      unrealizedPnLPercent: 11.31,
      dayChange: 454.14,
      dayChangePercent: 1.01,
      weight: 28.0,
      sector: 'Technology'
    },
    {
      ticker: 'NVDA',
      name: 'NVIDIA Corporation',
      quantity: 75,
      avgPrice: 450.00,
      currentPrice: 501.23,
      marketValue: 37592.25,
      costBasis: 33750.00,
      unrealizedPnL: 3842.25,
      unrealizedPnLPercent: 11.39,
      dayChange: 375.92,
      dayChangePercent: 1.01,
      weight: 23.2,
      sector: 'Semiconductors'
    },
    {
      ticker: 'GOOGL',
      name: 'Alphabet Inc.',
      quantity: 150,
      avgPrice: 125.00,
      currentPrice: 134.56,
      marketValue: 20184.00,
      costBasis: 18750.00,
      unrealizedPnL: 1434.00,
      unrealizedPnLPercent: 7.65,
      dayChange: 201.84,
      dayChangePercent: 1.01,
      weight: 12.4,
      sector: 'Technology'
    },
    {
      ticker: 'TSLA',
      name: 'Tesla Inc.',
      quantity: 50,
      avgPrice: 225.00,
      currentPrice: 251.78,
      marketValue: 12589.00,
      costBasis: 11250.00,
      unrealizedPnL: 1339.00,
      unrealizedPnLPercent: 11.90,
      dayChange: 125.89,
      dayChangePercent: 1.01,
      weight: 7.8,
      sector: 'Automotive'
    }
  ];

  const totalMarketValue = positions.reduce((sum, pos) => sum + pos.marketValue, 0);
  const totalCostBasis = positions.reduce((sum, pos) => sum + pos.costBasis, 0);
  const totalUnrealizedPnL = totalMarketValue - totalCostBasis;
  const totalUnrealizedPnLPercent = (totalUnrealizedPnL / totalCostBasis) * 100;
  const totalDayChange = positions.reduce((sum, pos) => sum + pos.dayChange, 0);
  const totalDayChangePercent = (totalDayChange / (totalMarketValue - totalDayChange)) * 100;

  const sectorAllocation = positions.reduce((acc, pos) => {
    if (!acc[pos.sector]) {
      acc[pos.sector] = { value: 0, weight: 0 };
    }
    acc[pos.sector].value += pos.marketValue;
    acc[pos.sector].weight += pos.weight;
    return acc;
  }, {} as Record<string, { value: number; weight: number }>);

  const performanceMetrics = [
    { label: 'YTD Return', value: '+18.45%', color: BLOOMBERG_GREEN },
    { label: '1M Return', value: '+5.23%', color: BLOOMBERG_GREEN },
    { label: '3M Return', value: '+12.67%', color: BLOOMBERG_GREEN },
    { label: '1Y Return', value: '+34.89%', color: BLOOMBERG_GREEN },
    { label: 'Sharpe Ratio', value: '1.87', color: BLOOMBERG_CYAN },
    { label: 'Beta', value: '1.12', color: BLOOMBERG_CYAN },
    { label: 'Max Drawdown', value: '-8.23%', color: BLOOMBERG_RED },
    { label: 'Volatility', value: '18.5%', color: BLOOMBERG_YELLOW }
  ];

  const recentTransactions = [
    { date: '2024-01-22', ticker: 'AAPL', type: 'BUY', quantity: 50, price: 182.50, total: 9125.00 },
    { date: '2024-01-20', ticker: 'NVDA', type: 'BUY', quantity: 25, price: 495.00, total: 12375.00 },
    { date: '2024-01-18', ticker: 'GOOGL', type: 'SELL', quantity: 30, price: 138.20, total: 4146.00 },
    { date: '2024-01-15', ticker: 'MSFT', type: 'BUY', quantity: 40, price: 365.00, total: 14600.00 }
  ];

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
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT PORTFOLIO MANAGER</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GREEN }}>● LIVE TRACKING</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_CYAN }}>POSITIONS: {positions.length}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>TOTAL VALUE: ${totalMarketValue.toLocaleString(undefined, {minimumFractionDigits: 2})}</span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>DAY P&L:</span>
          <span style={{ color: totalDayChange >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
            ${totalDayChange >= 0 ? '+' : ''}{totalDayChange.toFixed(2)} ({totalDayChangePercent >= 0 ? '+' : ''}{totalDayChangePercent.toFixed(2)}%)
          </span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>TOTAL P&L:</span>
          <span style={{ color: totalUnrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
            ${totalUnrealizedPnL >= 0 ? '+' : ''}{totalUnrealizedPnL.toFixed(2)} ({totalUnrealizedPnLPercent >= 0 ? '+' : ''}{totalUnrealizedPnLPercent.toFixed(2)}%)
          </span>
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>COST BASIS:</span>
          <span style={{ color: BLOOMBERG_CYAN }}>${totalCostBasis.toLocaleString(undefined, {minimumFractionDigits: 2})}</span>
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
            { key: "F1", label: "POSITIONS", view: "POSITIONS" },
            { key: "F2", label: "SECTORS", view: "SECTORS" },
            { key: "F3", label: "PERFORMANCE", view: "PERFORMANCE" },
            { key: "F4", label: "ANALYTICS", view: "ANALYTICS" },
            { key: "F5", label: "ORDERS", view: "ORDERS" },
            { key: "F6", label: "HISTORY", view: "HISTORY" },
            { key: "F7", label: "ALERTS", view: "ALERTS" },
            { key: "F8", label: "REPORTS", view: "REPORTS" },
            { key: "F9", label: "IMPORT", view: "IMPORT" },
            { key: "F10", label: "EXPORT", view: "EXPORT" },
            { key: "F11", label: "SETTINGS", view: "SETTINGS" },
            { key: "F12", label: "HELP", view: "HELP" }
          ].map(item => (
            <button key={item.key}
              onClick={() => setSelectedView(item.view)}
              style={{
                backgroundColor: selectedView === item.view ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: selectedView === item.view ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: selectedView === item.view ? 'bold' : 'normal',
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

          {/* Left Panel - Portfolio Summary */}
          <div style={{
            width: '320px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              PORTFOLIO SUMMARY
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {/* Key Metrics */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ padding: '6px', backgroundColor: 'rgba(255,165,0,0.1)', marginBottom: '4px' }}>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>TOTAL MARKET VALUE</div>
                <div style={{ color: BLOOMBERG_WHITE, fontSize: '16px', fontWeight: 'bold' }}>
                  ${totalMarketValue.toLocaleString(undefined, {minimumFractionDigits: 2})}
                </div>
              </div>

              <div style={{ padding: '6px', backgroundColor: totalDayChange >= 0 ? 'rgba(0,200,0,0.05)' : 'rgba(255,0,0,0.05)', marginBottom: '4px' }}>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>TODAY'S CHANGE</div>
                <div style={{ color: totalDayChange >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '14px', fontWeight: 'bold' }}>
                  ${totalDayChange >= 0 ? '+' : ''}{totalDayChange.toFixed(2)}
                </div>
                <div style={{ color: totalDayChange >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '10px' }}>
                  ({totalDayChangePercent >= 0 ? '+' : ''}{totalDayChangePercent.toFixed(2)}%)
                </div>
              </div>

              <div style={{ padding: '6px', backgroundColor: totalUnrealizedPnL >= 0 ? 'rgba(0,200,0,0.05)' : 'rgba(255,0,0,0.05)' }}>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '2px' }}>UNREALIZED P&L</div>
                <div style={{ color: totalUnrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '14px', fontWeight: 'bold' }}>
                  ${totalUnrealizedPnL >= 0 ? '+' : ''}{totalUnrealizedPnL.toFixed(2)}
                </div>
                <div style={{ color: totalUnrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '10px' }}>
                  ({totalUnrealizedPnLPercent >= 0 ? '+' : ''}{totalUnrealizedPnLPercent.toFixed(2)}%)
                </div>
              </div>
            </div>

            {/* Additional Metrics */}
            <div style={{ fontSize: '9px', lineHeight: '1.5', marginBottom: '12px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>Cost Basis:</span>
                <span style={{ color: BLOOMBERG_WHITE }}>${totalCostBasis.toLocaleString()}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>Cash Available:</span>
                <span style={{ color: BLOOMBERG_CYAN }}>$25,432.18</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                <span style={{ color: BLOOMBERG_GRAY }}>Buying Power:</span>
                <span style={{ color: BLOOMBERG_CYAN }}>$50,864.36</span>
              </div>
              <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '4px', paddingTop: '4px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>Total Equity:</span>
                  <span style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>
                    ${(totalMarketValue + 25432.18).toLocaleString(undefined, {minimumFractionDigits: 2})}
                  </span>
                </div>
              </div>
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, paddingTop: '8px', marginBottom: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                SECTOR ALLOCATION
              </div>
              {Object.entries(sectorAllocation).map(([sector, data], index) => (
                <div key={index} style={{
                  marginBottom: '3px',
                  padding: '3px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  fontSize: '9px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>{sector}</span>
                    <span style={{ color: BLOOMBERG_CYAN }}>{data.weight.toFixed(1)}%</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', color: BLOOMBERG_GRAY }}>
                    <span>${data.value.toLocaleString()}</span>
                  </div>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                PERFORMANCE METRICS
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.5' }}>
                {performanceMetrics.map((metric, index) => (
                  <div key={index} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                    <span style={{ color: BLOOMBERG_GRAY }}>{metric.label}:</span>
                    <span style={{ color: metric.color }}>{metric.value}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>

          {/* Center Panel - Positions Table */}
          <div style={{
            flex: 1,
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
                CURRENT POSITIONS
              </div>
              <div style={{ display: 'flex', gap: '4px', fontSize: '9px' }}>
                <button onClick={() => setSortBy('WEIGHT')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'WEIGHT' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'WEIGHT' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>WEIGHT</button>
                <button onClick={() => setSortBy('PNL')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'PNL' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'PNL' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>P&L</button>
                <button onClick={() => setSortBy('VALUE')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'VALUE' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'VALUE' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>VALUE</button>
              </div>
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '80px 180px 80px 90px 90px 100px 100px 120px 120px 80px',
              gap: '4px',
              padding: '4px',
              backgroundColor: 'rgba(255,255,255,0.05)',
              fontSize: '9px',
              fontWeight: 'bold',
              marginBottom: '2px'
            }}>
              <div style={{ color: BLOOMBERG_GRAY }}>TICKER</div>
              <div style={{ color: BLOOMBERG_GRAY }}>NAME</div>
              <div style={{ color: BLOOMBERG_GRAY }}>QTY</div>
              <div style={{ color: BLOOMBERG_GRAY }}>AVG PRICE</div>
              <div style={{ color: BLOOMBERG_GRAY }}>CURRENT</div>
              <div style={{ color: BLOOMBERG_GRAY }}>MKT VALUE</div>
              <div style={{ color: BLOOMBERG_GRAY }}>COST BASIS</div>
              <div style={{ color: BLOOMBERG_GRAY }}>UNREALIZED P&L</div>
              <div style={{ color: BLOOMBERG_GRAY }}>DAY CHANGE</div>
              <div style={{ color: BLOOMBERG_GRAY }}>WEIGHT</div>
            </div>

            {/* Position Rows */}
            {positions.map((pos, index) => (
              <div key={index} style={{
                display: 'grid',
                gridTemplateColumns: '80px 180px 80px 90px 90px 100px 100px 120px 120px 80px',
                gap: '4px',
                padding: '6px 4px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                borderLeft: `3px solid ${pos.unrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED}`,
                fontSize: '10px',
                marginBottom: '1px'
              }}>
                <div style={{ color: BLOOMBERG_CYAN, fontWeight: 'bold' }}>{pos.ticker}</div>
                <div style={{ color: BLOOMBERG_WHITE, overflow: 'hidden', textOverflow: 'ellipsis' }}>{pos.name}</div>
                <div style={{ color: BLOOMBERG_WHITE }}>{pos.quantity.toLocaleString()}</div>
                <div style={{ color: BLOOMBERG_GRAY }}>${pos.avgPrice.toFixed(2)}</div>
                <div style={{ color: BLOOMBERG_WHITE }}>${pos.currentPrice.toFixed(2)}</div>
                <div style={{ color: BLOOMBERG_WHITE }}>${pos.marketValue.toLocaleString()}</div>
                <div style={{ color: BLOOMBERG_GRAY }}>${pos.costBasis.toLocaleString()}</div>
                <div>
                  <div style={{ color: pos.unrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
                    ${pos.unrealizedPnL >= 0 ? '+' : ''}{pos.unrealizedPnL.toFixed(2)}
                  </div>
                  <div style={{ color: pos.unrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '8px' }}>
                    ({pos.unrealizedPnLPercent >= 0 ? '+' : ''}{pos.unrealizedPnLPercent.toFixed(2)}%)
                  </div>
                </div>
                <div>
                  <div style={{ color: pos.dayChange >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
                    ${pos.dayChange >= 0 ? '+' : ''}{pos.dayChange.toFixed(2)}
                  </div>
                  <div style={{ color: pos.dayChange >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '8px' }}>
                    ({pos.dayChangePercent >= 0 ? '+' : ''}{pos.dayChangePercent.toFixed(2)}%)
                  </div>
                </div>
                <div style={{ color: BLOOMBERG_YELLOW }}>{pos.weight.toFixed(1)}%</div>
              </div>
            ))}

            {/* Totals Row */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '80px 180px 80px 90px 90px 100px 100px 120px 120px 80px',
              gap: '4px',
              padding: '6px 4px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              borderTop: `2px solid ${BLOOMBERG_ORANGE}`,
              fontSize: '10px',
              fontWeight: 'bold',
              marginTop: '4px'
            }}>
              <div style={{ color: BLOOMBERG_ORANGE }}>TOTAL</div>
              <div></div>
              <div></div>
              <div></div>
              <div></div>
              <div style={{ color: BLOOMBERG_WHITE }}>${totalMarketValue.toLocaleString()}</div>
              <div style={{ color: BLOOMBERG_GRAY }}>${totalCostBasis.toLocaleString()}</div>
              <div>
                <div style={{ color: totalUnrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
                  ${totalUnrealizedPnL >= 0 ? '+' : ''}{totalUnrealizedPnL.toFixed(2)}
                </div>
                <div style={{ color: totalUnrealizedPnL >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '8px' }}>
                  ({totalUnrealizedPnLPercent >= 0 ? '+' : ''}{totalUnrealizedPnLPercent.toFixed(2)}%)
                </div>
              </div>
              <div>
                <div style={{ color: totalDayChange >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED }}>
                  ${totalDayChange >= 0 ? '+' : ''}{totalDayChange.toFixed(2)}
                </div>
                <div style={{ color: totalDayChange >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontSize: '8px' }}>
                  ({totalDayChangePercent >= 0 ? '+' : ''}{totalDayChangePercent.toFixed(2)}%)
                </div>
              </div>
              <div style={{ color: BLOOMBERG_YELLOW }}>100.0%</div>
            </div>
          </div>

          {/* Right Panel - Activity & Transactions */}
          <div style={{
            width: '320px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              RECENT TRANSACTIONS
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {recentTransactions.map((txn, index) => (
              <div key={index} style={{
                marginBottom: '6px',
                padding: '6px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                borderLeft: `3px solid ${txn.type === 'BUY' ? BLOOMBERG_GREEN : BLOOMBERG_RED}`,
                fontSize: '9px'
              }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>{txn.date}</span>
                  <span style={{ color: txn.type === 'BUY' ? BLOOMBERG_GREEN : BLOOMBERG_RED, fontWeight: 'bold' }}>
                    {txn.type}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_CYAN, fontWeight: 'bold' }}>{txn.ticker}</span>
                  <span style={{ color: BLOOMBERG_WHITE }}>{txn.quantity} @ ${txn.price.toFixed(2)}</span>
                </div>
                <div style={{ color: BLOOMBERG_GRAY, textAlign: 'right' }}>
                  Total: ${txn.total.toLocaleString()}
                </div>
              </div>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, paddingTop: '8px', marginTop: '12px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                MARKET INDICATORS
              </div>
              {[
                { name: 'S&P 500', value: '4,850.25', change: '+0.87%', color: BLOOMBERG_GREEN },
                { name: 'NASDAQ', value: '15,234.56', change: '+1.23%', color: BLOOMBERG_GREEN },
                { name: 'DOW JONES', value: '38,742.18', change: '+0.56%', color: BLOOMBERG_GREEN },
                { name: 'VIX', value: '13.45', change: '-5.23%', color: BLOOMBERG_GREEN },
                { name: '10Y YIELD', value: '4.12%', change: '+0.03', color: BLOOMBERG_RED }
              ].map((indicator, index) => (
                <div key={index} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '3px',
                  marginBottom: '2px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  fontSize: '9px'
                }}>
                  <span style={{ color: BLOOMBERG_WHITE }}>{indicator.name}</span>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{ color: BLOOMBERG_CYAN }}>{indicator.value}</div>
                    <div style={{ color: indicator.color, fontSize: '8px' }}>{indicator.change}</div>
                  </div>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, paddingTop: '8px', marginTop: '12px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                PORTFOLIO ALERTS
              </div>
              {[
                { type: 'TARGET', message: 'AAPL reached target price $185', time: '2h ago', color: BLOOMBERG_GREEN },
                { type: 'STOP', message: 'GOOGL approaching stop loss', time: '5h ago', color: BLOOMBERG_RED },
                { type: 'NEWS', message: 'NVDA earnings report tomorrow', time: '1d ago', color: BLOOMBERG_YELLOW }
              ].map((alert, index) => (
                <div key={index} style={{
                  marginBottom: '4px',
                  padding: '4px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  borderLeft: `2px solid ${alert.color}`,
                  fontSize: '8px',
                  lineHeight: '1.3'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: alert.color, fontWeight: 'bold' }}>[{alert.type}]</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>{alert.time}</span>
                  </div>
                  <div style={{ color: BLOOMBERG_WHITE }}>{alert.message}</div>
                </div>
              ))}
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
            <span>Fincept Portfolio Manager v2.1.0 | Real-time position tracking</span>
            <span>Positions: {positions.length} | Market Value: ${totalMarketValue.toLocaleString()}</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>View: {selectedView}</span>
            <span>Sort: {sortBy}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>STATUS: LIVE</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default PortfolioTab;