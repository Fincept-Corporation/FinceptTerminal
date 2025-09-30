import React, { useState, useEffect } from 'react';

interface Alert {
  id: string;
  type: 'PRICE' | 'VOLUME' | 'INDICATOR' | 'NEWS' | 'EARNING';
  symbol: string;
  condition: string;
  trigger: string;
  status: 'ACTIVE' | 'TRIGGERED' | 'EXPIRED' | 'DISABLED';
  priority: 'HIGH' | 'MEDIUM' | 'LOW';
  created: string;
  lastTriggered?: string;
  triggerCount: number;
}

interface Notification {
  id: string;
  time: string;
  symbol: string;
  type: string;
  message: string;
  priority: 'HIGH' | 'MEDIUM' | 'LOW';
  read: boolean;
}

const AlertsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedType, setSelectedType] = useState('ALL');
  const [selectedStatus, setSelectedStatus] = useState('ALL');
  const [showCreateModal, setShowCreateModal] = useState(false);

  const COLOR_ORANGE = '#FFA500';
  const COLOR_WHITE = '#FFFFFF';
  const COLOR_RED = '#FF0000';
  const COLOR_GREEN = '#00C800';
  const COLOR_YELLOW = '#FFFF00';
  const COLOR_GRAY = '#787878';
  const COLOR_BLUE = '#6496FA';
  const COLOR_PURPLE = '#C864FF';
  const COLOR_CYAN = '#00FFFF';
  const COLOR_DARK_BG = '#000000';
  const COLOR_PANEL_BG = '#0a0a0a';

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  const alerts: Alert[] = [
    {
      id: 'ALT001',
      type: 'PRICE',
      symbol: 'AAPL',
      condition: 'Price Above',
      trigger: '$180.00',
      status: 'ACTIVE',
      priority: 'HIGH',
      created: '2025-03-10 09:30',
      triggerCount: 0
    },
    {
      id: 'ALT002',
      type: 'PRICE',
      symbol: 'TSLA',
      condition: 'Price Below',
      trigger: '$185.00',
      status: 'TRIGGERED',
      priority: 'HIGH',
      created: '2025-03-12 14:20',
      lastTriggered: '2025-03-15 11:34',
      triggerCount: 3
    },
    {
      id: 'ALT003',
      type: 'VOLUME',
      symbol: 'NVDA',
      condition: 'Volume Above',
      trigger: '50M shares',
      status: 'ACTIVE',
      priority: 'MEDIUM',
      created: '2025-03-14 10:15',
      triggerCount: 0
    },
    {
      id: 'ALT004',
      type: 'INDICATOR',
      symbol: 'SPY',
      condition: 'RSI Below',
      trigger: '30',
      status: 'ACTIVE',
      priority: 'MEDIUM',
      created: '2025-03-13 15:45',
      triggerCount: 0
    },
    {
      id: 'ALT005',
      type: 'NEWS',
      symbol: 'MSFT',
      condition: 'News Sentiment',
      trigger: 'Negative < -0.5',
      status: 'DISABLED',
      priority: 'LOW',
      created: '2025-03-11 08:00',
      triggerCount: 0
    },
    {
      id: 'ALT006',
      type: 'EARNING',
      symbol: 'GOOGL',
      condition: 'Earnings Date',
      trigger: '2025-04-23',
      status: 'ACTIVE',
      priority: 'HIGH',
      created: '2025-03-01 12:00',
      triggerCount: 0
    },
    {
      id: 'ALT007',
      type: 'PRICE',
      symbol: 'JPM',
      condition: '% Change Above',
      trigger: '+5%',
      status: 'TRIGGERED',
      priority: 'HIGH',
      created: '2025-03-08 09:45',
      lastTriggered: '2025-03-15 10:23',
      triggerCount: 1
    },
    {
      id: 'ALT008',
      type: 'INDICATOR',
      symbol: 'QQQ',
      condition: 'MACD Cross',
      trigger: 'Bullish',
      status: 'ACTIVE',
      priority: 'MEDIUM',
      created: '2025-03-09 13:30',
      triggerCount: 0
    }
  ];

  const notifications: Notification[] = [
    {
      id: 'NOT001',
      time: '11:34:23',
      symbol: 'TSLA',
      type: 'PRICE ALERT',
      message: 'TSLA dropped below $185.00 - Current: $183.45',
      priority: 'HIGH',
      read: false
    },
    {
      id: 'NOT002',
      time: '10:23:45',
      symbol: 'JPM',
      type: 'PRICE ALERT',
      message: 'JPM gained +5.2% in last hour - Current: $187.23',
      priority: 'HIGH',
      read: false
    },
    {
      id: 'NOT003',
      time: '09:15:12',
      symbol: 'NVDA',
      type: 'VOLUME ALERT',
      message: 'NVDA volume spike: 52.3M shares traded (above 50M threshold)',
      priority: 'MEDIUM',
      read: true
    },
    {
      id: 'NOT004',
      time: '08:45:33',
      symbol: 'AAPL',
      type: 'NEWS ALERT',
      message: 'Breaking news: AAPL announces new product launch event',
      priority: 'HIGH',
      read: true
    },
    {
      id: 'NOT005',
      time: 'Yesterday',
      symbol: 'SPY',
      type: 'INDICATOR',
      message: 'SPY RSI dropped to 28.4 (oversold territory)',
      priority: 'MEDIUM',
      read: true
    }
  ];

  const watchedSymbols = [
    { symbol: 'AAPL', price: 178.45, change: 1.33, alerts: 2, status: 'NORMAL' },
    { symbol: 'TSLA', price: 183.45, change: -2.34, alerts: 3, status: 'TRIGGERED' },
    { symbol: 'NVDA', price: 878.34, change: 1.44, alerts: 1, status: 'NORMAL' },
    { symbol: 'MSFT', price: 412.34, change: 1.39, alerts: 1, status: 'DISABLED' },
    { symbol: 'GOOGL', price: 145.67, change: -0.84, alerts: 1, status: 'NORMAL' },
    { symbol: 'JPM', price: 187.23, change: 5.21, alerts: 2, status: 'TRIGGERED' }
  ];

  const filteredAlerts = alerts.filter(a =>
    (selectedType === 'ALL' || a.type === selectedType) &&
    (selectedStatus === 'ALL' || a.status === selectedStatus)
  );

  const getTypeColor = (type: string) => {
    switch(type) {
      case 'PRICE': return COLOR_GREEN;
      case 'VOLUME': return COLOR_BLUE;
      case 'INDICATOR': return COLOR_PURPLE;
      case 'NEWS': return COLOR_CYAN;
      case 'EARNING': return COLOR_YELLOW;
      default: return COLOR_WHITE;
    }
  };

  const getStatusColor = (status: string) => {
    switch(status) {
      case 'ACTIVE': return COLOR_GREEN;
      case 'TRIGGERED': return COLOR_ORANGE;
      case 'EXPIRED': return COLOR_GRAY;
      case 'DISABLED': return COLOR_RED;
      default: return COLOR_WHITE;
    }
  };

  const getPriorityColor = (priority: string) => {
    switch(priority) {
      case 'HIGH': return COLOR_RED;
      case 'MEDIUM': return COLOR_ORANGE;
      case 'LOW': return COLOR_YELLOW;
      default: return COLOR_GRAY;
    }
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: COLOR_DARK_BG,
      color: COLOR_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: COLOR_PANEL_BG,
        borderBottom: `2px solid ${COLOR_GRAY}`,
        padding: '8px 16px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '16px' }}>
              ALERTS & MONITORING
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>REAL-TIME NOTIFICATIONS • CUSTOM TRIGGERS</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <button
              onClick={() => setShowCreateModal(!showCreateModal)}
              style={{
                padding: '6px 16px',
                backgroundColor: COLOR_GREEN,
                border: 'none',
                color: COLOR_DARK_BG,
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}>
              + CREATE ALERT
            </button>
            <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>
              {currentTime.toLocaleTimeString()}
            </span>
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', display: 'flex', gap: '12px', padding: '12px' }}>
        {/* Left Panel - Notifications & Watched */}
        <div style={{ width: '340px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Recent Notifications */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_ORANGE}`,
            padding: '12px',
            maxHeight: '400px',
            overflow: 'auto'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              RECENT NOTIFICATIONS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {notifications.map((notif, index) => (
                <div key={index} style={{
                  padding: '10px',
                  backgroundColor: notif.read ? 'rgba(255,255,255,0.02)' : 'rgba(255,165,0,0.1)',
                  border: `1px solid ${notif.read ? COLOR_GRAY : COLOR_ORANGE}`,
                  borderLeft: `4px solid ${getPriorityColor(notif.priority)}`
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                    <span style={{ color: COLOR_CYAN, fontWeight: 'bold', fontSize: '11px' }}>
                      {notif.symbol}
                    </span>
                    <span style={{ color: COLOR_GRAY, fontSize: '9px' }}>
                      {notif.time}
                    </span>
                  </div>
                  <div style={{ color: getPriorityColor(notif.priority), fontSize: '9px', fontWeight: 'bold', marginBottom: '4px' }}>
                    {notif.type}
                  </div>
                  <div style={{ color: COLOR_WHITE, fontSize: '10px', lineHeight: '1.4' }}>
                    {notif.message}
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Watched Symbols */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_CYAN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              WATCHED SYMBOLS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {watchedSymbols.map((watch, index) => (
                <div key={index} style={{
                  padding: '10px',
                  backgroundColor: watch.status === 'TRIGGERED' ? 'rgba(255,165,0,0.1)' : 'rgba(255,255,255,0.02)',
                  border: `1px solid ${watch.status === 'TRIGGERED' ? COLOR_ORANGE : COLOR_GRAY}`,
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center'
                }}>
                  <div>
                    <div style={{ color: COLOR_CYAN, fontWeight: 'bold', fontSize: '12px', marginBottom: '2px' }}>
                      {watch.symbol}
                    </div>
                    <div style={{ color: watch.change > 0 ? COLOR_GREEN : COLOR_RED, fontSize: '10px' }}>
                      ${watch.price.toFixed(2)} ({watch.change > 0 ? '+' : ''}{watch.change.toFixed(2)}%)
                    </div>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{
                      color: getStatusColor(watch.status),
                      fontSize: '9px',
                      fontWeight: 'bold',
                      marginBottom: '4px'
                    }}>
                      {watch.status}
                    </div>
                    <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>
                      {watch.alerts} alert{watch.alerts !== 1 ? 's' : ''}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Alert Statistics */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_PURPLE}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              ALERT STATISTICS
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              <div style={{
                padding: '10px',
                backgroundColor: 'rgba(0,200,0,0.05)',
                border: `1px solid ${COLOR_GREEN}`,
                textAlign: 'center'
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '9px', marginBottom: '4px' }}>ACTIVE</div>
                <div style={{ color: COLOR_GREEN, fontSize: '20px', fontWeight: 'bold' }}>5</div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: 'rgba(255,165,0,0.05)',
                border: `1px solid ${COLOR_ORANGE}`,
                textAlign: 'center'
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '9px', marginBottom: '4px' }}>TRIGGERED</div>
                <div style={{ color: COLOR_ORANGE, fontSize: '20px', fontWeight: 'bold' }}>2</div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: 'rgba(255,0,0,0.05)',
                border: `1px solid ${COLOR_RED}`,
                textAlign: 'center'
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '9px', marginBottom: '4px' }}>DISABLED</div>
                <div style={{ color: COLOR_RED, fontSize: '20px', fontWeight: 'bold' }}>1</div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: 'rgba(120,120,120,0.05)',
                border: `1px solid ${COLOR_GRAY}`,
                textAlign: 'center'
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '9px', marginBottom: '4px' }}>TOTAL</div>
                <div style={{ color: COLOR_CYAN, fontSize: '20px', fontWeight: 'bold' }}>8</div>
              </div>
            </div>
          </div>
        </div>

        {/* Right Panel - Alerts List */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
          {/* Filters */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            padding: '12px',
            marginBottom: '12px'
          }}>
            <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>TYPE:</span>
              {['ALL', 'PRICE', 'VOLUME', 'INDICATOR', 'NEWS', 'EARNING'].map((type) => (
                <button key={type}
                  onClick={() => setSelectedType(type)}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: selectedType === type ? getTypeColor(type) : COLOR_DARK_BG,
                    border: `2px solid ${selectedType === type ? getTypeColor(type) : COLOR_GRAY}`,
                    color: selectedType === type ? COLOR_DARK_BG : COLOR_WHITE,
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                  {type}
                </button>
              ))}
              <span style={{ color: COLOR_GRAY, marginLeft: '16px' }}>|</span>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>STATUS:</span>
              {['ALL', 'ACTIVE', 'TRIGGERED', 'DISABLED'].map((status) => (
                <button key={status}
                  onClick={() => setSelectedStatus(status)}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: selectedStatus === status ? getStatusColor(status) : COLOR_DARK_BG,
                    border: `2px solid ${selectedStatus === status ? getStatusColor(status) : COLOR_GRAY}`,
                    color: selectedStatus === status ? COLOR_DARK_BG : COLOR_WHITE,
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                  {status}
                </button>
              ))}
            </div>
          </div>

          {/* Alerts Table */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            flex: 1,
            overflow: 'auto'
          }}>
            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '80px 80px 80px 200px 120px 100px 80px 140px 80px 80px',
              padding: '10px 12px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              borderBottom: `2px solid ${COLOR_ORANGE}`,
              position: 'sticky',
              top: 0,
              zIndex: 10,
              fontSize: '10px'
            }}>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>ID</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>TYPE</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>SYMBOL</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>CONDITION</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>TRIGGER</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>STATUS</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>PRIORITY</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>CREATED</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>TRIGGERS</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>ACTIONS</div>
            </div>

            {/* Table Body */}
            {filteredAlerts.map((alert, index) => (
              <div key={index}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '80px 80px 80px 200px 120px 100px 80px 140px 80px 80px',
                  padding: '12px',
                  backgroundColor: alert.status === 'TRIGGERED' ? 'rgba(255,165,0,0.05)' : (index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent'),
                  borderBottom: `1px solid ${COLOR_GRAY}`,
                  borderLeft: `4px solid ${getPriorityColor(alert.priority)}`,
                  cursor: 'pointer',
                  fontSize: '11px'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = 'rgba(255,165,0,0.1)';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = alert.status === 'TRIGGERED' ? 'rgba(255,165,0,0.05)' : (index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent');
                }}>
                <div style={{ color: COLOR_GRAY }}>{alert.id}</div>
                <div>
                  <span style={{
                    padding: '4px 6px',
                    backgroundColor: `rgba(${alert.type === 'PRICE' ? '0,200,0' : alert.type === 'VOLUME' ? '100,150,250' : alert.type === 'INDICATOR' ? '200,100,255' : alert.type === 'NEWS' ? '0,255,255' : '255,255,0'},0.2)`,
                    border: `1px solid ${getTypeColor(alert.type)}`,
                    color: getTypeColor(alert.type),
                    fontSize: '9px',
                    fontWeight: 'bold'
                  }}>
                    {alert.type}
                  </span>
                </div>
                <div style={{ color: COLOR_CYAN, fontWeight: 'bold' }}>{alert.symbol}</div>
                <div style={{ color: COLOR_WHITE }}>{alert.condition}</div>
                <div style={{ color: COLOR_YELLOW, fontWeight: 'bold' }}>{alert.trigger}</div>
                <div>
                  <span style={{
                    padding: '4px 8px',
                    backgroundColor: `rgba(${alert.status === 'ACTIVE' ? '0,200,0' : alert.status === 'TRIGGERED' ? '255,165,0' : alert.status === 'DISABLED' ? '255,0,0' : '120,120,120'},0.2)`,
                    border: `1px solid ${getStatusColor(alert.status)}`,
                    color: getStatusColor(alert.status),
                    fontSize: '9px',
                    fontWeight: 'bold'
                  }}>
                    {alert.status}
                  </span>
                </div>
                <div style={{ color: getPriorityColor(alert.priority), fontWeight: 'bold' }}>
                  {alert.priority}
                </div>
                <div style={{ color: COLOR_GRAY, fontSize: '10px' }}>{alert.created}</div>
                <div style={{ color: alert.triggerCount > 0 ? COLOR_ORANGE : COLOR_GRAY }}>
                  {alert.triggerCount}x
                </div>
                <div style={{ display: 'flex', gap: '4px' }}>
                  <button style={{
                    padding: '4px 8px',
                    backgroundColor: COLOR_BLUE,
                    border: 'none',
                    color: COLOR_DARK_BG,
                    fontSize: '9px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                    EDIT
                  </button>
                  <button style={{
                    padding: '4px 8px',
                    backgroundColor: COLOR_RED,
                    border: 'none',
                    color: COLOR_DARK_BG,
                    fontSize: '9px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                    DEL
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Footer */}
      <div style={{
        borderTop: `3px solid ${COLOR_ORANGE}`,
        backgroundColor: COLOR_PANEL_BG,
        padding: '12px 16px',
        fontSize: '11px',
        color: COLOR_WHITE,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <span style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '13px' }}>
              ALERTS & MONITORING v1.0
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>
              Real-time market alerts & notifications
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GREEN }}>
              {filteredAlerts.length} alerts shown
            </span>
          </div>
        </div>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          fontSize: '10px',
          paddingTop: '8px',
          borderTop: `1px solid ${COLOR_GRAY}`
        }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F1</span> Help</span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F2</span> Create Alert</span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F3</span> Test Alert</span>
          </div>
          <div style={{ color: COLOR_GRAY }}>
            © 2025 Fincept Labs • All Rights Reserved
          </div>
        </div>
      </div>
    </div>
  );
};

export default AlertsTab;