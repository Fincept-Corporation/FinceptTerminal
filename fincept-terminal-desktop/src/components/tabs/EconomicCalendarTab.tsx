import React, { useState, useEffect } from 'react';

interface EconomicEvent {
  time: string;
  country: string;
  flag: string;
  indicator: string;
  importance: 'HIGH' | 'MEDIUM' | 'LOW';
  actual: string;
  forecast: string;
  previous: string;
  impact: 'POSITIVE' | 'NEGATIVE' | 'NEUTRAL';
  category: string;
}

const EconomicCalendarTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedDate, setSelectedDate] = useState('2025-03-15');
  const [selectedImportance, setSelectedImportance] = useState('ALL');
  const [selectedCountry, setSelectedCountry] = useState('ALL');

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

  const economicEvents: EconomicEvent[] = [
    {
      time: '08:30',
      country: 'US',
      flag: '🇺🇸',
      indicator: 'Non-Farm Payrolls',
      importance: 'HIGH',
      actual: '275K',
      forecast: '198K',
      previous: '229K',
      impact: 'POSITIVE',
      category: 'Employment'
    },
    {
      time: '08:30',
      country: 'US',
      flag: '🇺🇸',
      indicator: 'Unemployment Rate',
      importance: 'HIGH',
      actual: '3.6%',
      forecast: '3.7%',
      previous: '3.7%',
      impact: 'POSITIVE',
      category: 'Employment'
    },
    {
      time: '08:30',
      country: 'US',
      flag: '🇺🇸',
      indicator: 'Average Hourly Earnings (MoM)',
      importance: 'MEDIUM',
      actual: '0.4%',
      forecast: '0.3%',
      previous: '0.6%',
      impact: 'POSITIVE',
      category: 'Employment'
    },
    {
      time: '10:00',
      country: 'US',
      flag: '🇺🇸',
      indicator: 'ISM Services PMI',
      importance: 'HIGH',
      actual: '53.4',
      forecast: '52.0',
      previous: '50.6',
      impact: 'POSITIVE',
      category: 'Economic Activity'
    },
    {
      time: '14:00',
      country: 'US',
      flag: '🇺🇸',
      indicator: 'FOMC Member Speech',
      importance: 'MEDIUM',
      actual: '-',
      forecast: '-',
      previous: '-',
      impact: 'NEUTRAL',
      category: 'Central Bank'
    },
    {
      time: '03:00',
      country: 'CN',
      flag: '🇨🇳',
      indicator: 'CPI (YoY)',
      importance: 'HIGH',
      actual: '0.8%',
      forecast: '0.5%',
      previous: '0.3%',
      impact: 'POSITIVE',
      category: 'Inflation'
    },
    {
      time: '03:30',
      country: 'CN',
      flag: '🇨🇳',
      indicator: 'PPI (YoY)',
      importance: 'MEDIUM',
      actual: '-2.5%',
      forecast: '-2.6%',
      previous: '-2.7%',
      impact: 'POSITIVE',
      category: 'Inflation'
    },
    {
      time: '05:00',
      country: 'EU',
      flag: '🇪🇺',
      indicator: 'ECB Interest Rate Decision',
      importance: 'HIGH',
      actual: '4.50%',
      forecast: '4.50%',
      previous: '4.50%',
      impact: 'NEUTRAL',
      category: 'Central Bank'
    },
    {
      time: '05:30',
      country: 'EU',
      flag: '🇪🇺',
      indicator: 'ECB Press Conference',
      importance: 'HIGH',
      actual: '-',
      forecast: '-',
      previous: '-',
      impact: 'NEUTRAL',
      category: 'Central Bank'
    },
    {
      time: '06:00',
      country: 'GB',
      flag: '🇬🇧',
      indicator: 'GDP (QoQ)',
      importance: 'HIGH',
      actual: '0.2%',
      forecast: '0.1%',
      previous: '0.0%',
      impact: 'POSITIVE',
      category: 'Economic Activity'
    },
    {
      time: '23:50',
      country: 'JP',
      flag: '🇯🇵',
      indicator: 'BoJ Monetary Policy Statement',
      importance: 'HIGH',
      actual: '-',
      forecast: '-',
      previous: '-',
      impact: 'NEUTRAL',
      category: 'Central Bank'
    },
    {
      time: '07:00',
      country: 'DE',
      flag: '🇩🇪',
      indicator: 'Industrial Production (MoM)',
      importance: 'MEDIUM',
      actual: '1.2%',
      forecast: '0.8%',
      previous: '-1.1%',
      impact: 'POSITIVE',
      category: 'Economic Activity'
    }
  ];

  const upcomingCentralBankMeetings = [
    { date: '2025-03-20', bank: 'FOMC', decision: 'Rate Decision', expected: '5.25-5.50%' },
    { date: '2025-04-10', bank: 'ECB', decision: 'Rate Decision', expected: '4.50%' },
    { date: '2025-04-25', bank: 'BoJ', decision: 'Policy Meeting', expected: 'No Change' },
    { date: '2025-05-01', bank: 'FOMC', decision: 'Rate Decision', expected: '5.00-5.25%' }
  ];

  const economicIndicators = [
    { name: 'GDP Growth (US)', value: '2.4%', change: 0.1, trend: 'up' },
    { name: 'Unemployment (US)', value: '3.6%', change: -0.1, trend: 'down' },
    { name: 'CPI Inflation (US)', value: '3.2%', change: 0.2, trend: 'up' },
    { name: 'Fed Funds Rate', value: '5.50%', change: 0.0, trend: 'stable' },
    { name: 'GDP Growth (EU)', value: '0.5%', change: 0.2, trend: 'up' },
    { name: 'ECB Rate', value: '4.50%', change: 0.0, trend: 'stable' }
  ];

  const filteredEvents = economicEvents.filter(e =>
    (selectedImportance === 'ALL' || e.importance === selectedImportance) &&
    (selectedCountry === 'ALL' || e.country === selectedCountry)
  ).sort((a, b) => a.time.localeCompare(b.time));

  const getImportanceColor = (importance: string) => {
    switch(importance) {
      case 'HIGH': return COLOR_RED;
      case 'MEDIUM': return COLOR_ORANGE;
      case 'LOW': return COLOR_YELLOW;
      default: return COLOR_GRAY;
    }
  };

  const getImpactColor = (impact: string) => {
    switch(impact) {
      case 'POSITIVE': return COLOR_GREEN;
      case 'NEGATIVE': return COLOR_RED;
      case 'NEUTRAL': return COLOR_GRAY;
      default: return COLOR_WHITE;
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
              ECONOMIC CALENDAR
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>GLOBAL EVENTS • INDICATORS • CENTRAL BANKS</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <input
              type="date"
              value={selectedDate}
              onChange={(e) => setSelectedDate(e.target.value)}
              style={{
                backgroundColor: COLOR_DARK_BG,
                color: COLOR_CYAN,
                border: `2px solid ${COLOR_GRAY}`,
                padding: '4px 8px',
                fontSize: '11px',
                fontFamily: 'Consolas, monospace'
              }}
            />
            <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>
              {currentTime.toLocaleTimeString()}
            </span>
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', display: 'flex', gap: '12px', padding: '12px' }}>
        {/* Left Panel - Indicators & CB Meetings */}
        <div style={{ width: '340px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Economic Indicators */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_CYAN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              KEY INDICATORS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {economicIndicators.map((indicator, index) => (
                <div key={index} style={{
                  padding: '10px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  border: `1px solid ${COLOR_GRAY}`
                }}>
                  <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '4px' }}>
                    {indicator.name}
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span style={{ color: COLOR_CYAN, fontSize: '16px', fontWeight: 'bold' }}>
                      {indicator.value}
                    </span>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <span style={{
                        color: indicator.trend === 'up' ? COLOR_GREEN : indicator.trend === 'down' ? COLOR_RED : COLOR_GRAY,
                        fontSize: '12px'
                      }}>
                        {indicator.trend === 'up' ? '▲' : indicator.trend === 'down' ? '▼' : '●'}
                      </span>
                      <span style={{
                        color: indicator.change > 0 ? COLOR_GREEN : indicator.change < 0 ? COLOR_RED : COLOR_GRAY,
                        fontSize: '11px'
                      }}>
                        {indicator.change > 0 ? '+' : ''}{indicator.change.toFixed(1)}%
                      </span>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Central Bank Meetings */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_PURPLE}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              UPCOMING CB MEETINGS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              {upcomingCentralBankMeetings.map((meeting, index) => (
                <div key={index} style={{
                  padding: '10px',
                  backgroundColor: 'rgba(200,100,255,0.05)',
                  border: `1px solid ${COLOR_PURPLE}`
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                    <span style={{ color: COLOR_PURPLE, fontWeight: 'bold', fontSize: '12px' }}>
                      {meeting.bank}
                    </span>
                    <span style={{ color: COLOR_GRAY, fontSize: '10px' }}>
                      {meeting.date}
                    </span>
                  </div>
                  <div style={{ color: COLOR_WHITE, fontSize: '11px', marginBottom: '4px' }}>
                    {meeting.decision}
                  </div>
                  <div style={{ color: COLOR_CYAN, fontSize: '10px' }}>
                    Expected: {meeting.expected}
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Impact Summary */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_GREEN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              TODAY'S IMPACT
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <div style={{
                padding: '10px',
                backgroundColor: 'rgba(255,0,0,0.05)',
                border: `1px solid ${COLOR_RED}`
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '4px' }}>HIGH IMPACT</div>
                <div style={{ color: COLOR_RED, fontSize: '20px', fontWeight: 'bold' }}>7</div>
                <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>events scheduled</div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: 'rgba(255,165,0,0.05)',
                border: `1px solid ${COLOR_ORANGE}`
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '4px' }}>MEDIUM IMPACT</div>
                <div style={{ color: COLOR_ORANGE, fontSize: '20px', fontWeight: 'bold' }}>3</div>
                <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>events scheduled</div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: 'rgba(255,255,0,0.05)',
                border: `1px solid ${COLOR_YELLOW}`
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '4px' }}>LOW IMPACT</div>
                <div style={{ color: COLOR_YELLOW, fontSize: '20px', fontWeight: 'bold' }}>2</div>
                <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>events scheduled</div>
              </div>
            </div>
          </div>
        </div>

        {/* Right Panel - Events Calendar */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
          {/* Filters */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            padding: '12px',
            marginBottom: '12px'
          }}>
            <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>IMPORTANCE:</span>
              {['ALL', 'HIGH', 'MEDIUM', 'LOW'].map((importance) => (
                <button key={importance}
                  onClick={() => setSelectedImportance(importance)}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: selectedImportance === importance ? getImportanceColor(importance) : COLOR_DARK_BG,
                    border: `2px solid ${selectedImportance === importance ? getImportanceColor(importance) : COLOR_GRAY}`,
                    color: selectedImportance === importance ? COLOR_DARK_BG : COLOR_WHITE,
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                  {importance}
                </button>
              ))}
              <span style={{ color: COLOR_GRAY, marginLeft: '16px' }}>|</span>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>COUNTRY:</span>
              {['ALL', 'US', 'EU', 'GB', 'JP', 'CN'].map((country) => (
                <button key={country}
                  onClick={() => setSelectedCountry(country)}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: selectedCountry === country ? COLOR_CYAN : COLOR_DARK_BG,
                    border: `2px solid ${selectedCountry === country ? COLOR_CYAN : COLOR_GRAY}`,
                    color: selectedCountry === country ? COLOR_DARK_BG : COLOR_WHITE,
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                  {country}
                </button>
              ))}
            </div>
          </div>

          {/* Events Table */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            flex: 1,
            overflow: 'auto'
          }}>
            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '70px 60px 280px 80px 100px 100px 100px 80px',
              padding: '10px 12px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              borderBottom: `2px solid ${COLOR_ORANGE}`,
              position: 'sticky',
              top: 0,
              zIndex: 10
            }}>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>TIME</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>CTRY</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>INDICATOR</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>IMPACT</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>ACTUAL</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>FORECAST</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>PREVIOUS</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>RESULT</div>
            </div>

            {/* Table Body */}
            {filteredEvents.map((event, index) => (
              <div key={index}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '70px 60px 280px 80px 100px 100px 100px 80px',
                  padding: '12px',
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  borderBottom: `1px solid ${COLOR_GRAY}`,
                  cursor: 'pointer',
                  fontSize: '11px'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = 'rgba(255,165,0,0.1)';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent';
                }}>
                <div style={{ color: COLOR_CYAN, fontWeight: 'bold' }}>{event.time}</div>
                <div style={{ fontSize: '16px' }}>{event.flag}</div>
                <div>
                  <div style={{ color: COLOR_WHITE, fontWeight: 'bold', marginBottom: '2px' }}>
                    {event.indicator}
                  </div>
                  <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>
                    {event.category}
                  </div>
                </div>
                <div>
                  <span style={{
                    padding: '4px 8px',
                    backgroundColor: `rgba(${event.importance === 'HIGH' ? '255,0,0' : event.importance === 'MEDIUM' ? '255,165,0' : '255,255,0'},0.2)`,
                    border: `1px solid ${getImportanceColor(event.importance)}`,
                    color: getImportanceColor(event.importance),
                    fontSize: '9px',
                    fontWeight: 'bold'
                  }}>
                    {event.importance}
                  </span>
                </div>
                <div style={{ color: COLOR_WHITE, fontWeight: 'bold' }}>{event.actual}</div>
                <div style={{ color: COLOR_YELLOW }}>{event.forecast}</div>
                <div style={{ color: COLOR_GRAY }}>{event.previous}</div>
                <div>
                  <span style={{
                    color: getImpactColor(event.impact),
                    fontSize: '16px'
                  }}>
                    {event.impact === 'POSITIVE' ? '▲' : event.impact === 'NEGATIVE' ? '▼' : '●'}
                  </span>
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
              ECONOMIC CALENDAR v1.0
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>
              Real-time economic events & data
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GREEN }}>
              Showing: {filteredEvents.length} events
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
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F2</span> Export</span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F3</span> Alerts</span>
          </div>
          <div style={{ color: COLOR_GRAY }}>
            © 2025 Fincept Labs • All Rights Reserved
          </div>
        </div>
      </div>
    </div>
  );
};

export default EconomicCalendarTab;