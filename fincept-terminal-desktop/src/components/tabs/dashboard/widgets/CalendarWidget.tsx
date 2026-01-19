import React, { useState, useEffect } from 'react';
import { Calendar, Clock, AlertTriangle, TrendingUp, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';

interface CalendarWidgetProps {
  id: string;
  limit?: number;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface EconomicEvent {
  id: string;
  time: string;
  country: string;
  event: string;
  impact: 'high' | 'medium' | 'low';
  actual?: string;
  forecast?: string;
  previous?: string;
}

const FINCEPT_GREEN = '#00FF00';
const FINCEPT_RED = '#FF0000';
const FINCEPT_ORANGE = '#FFA500';
const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_GRAY = '#787878';
const FINCEPT_YELLOW = '#FFD700';

const COUNTRY_FLAGS: Record<string, string> = {
  US: '🇺🇸',
  EU: '🇪🇺',
  GB: '🇬🇧',
  JP: '🇯🇵',
  CN: '🇨🇳',
  DE: '🇩🇪',
  CA: '🇨🇦',
  AU: '🇦🇺',
  CH: '🇨🇭',
  NZ: '🇳🇿',
};

export const CalendarWidget: React.FC<CalendarWidgetProps> = ({
  id,
  limit = 6,
  onRemove,
  onNavigate
}) => {
  const [events, setEvents] = useState<EconomicEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadEvents = async () => {
    try {
      setLoading(true);
      setError(null);

      // Demo economic calendar data
      // In production, this would fetch from an economic calendar API
      const now = new Date();
      const demoEvents: EconomicEvent[] = [
        {
          id: '1',
          time: '08:30',
          country: 'US',
          event: 'Initial Jobless Claims',
          impact: 'high',
          forecast: '220K',
          previous: '217K'
        },
        {
          id: '2',
          time: '10:00',
          country: 'US',
          event: 'ISM Manufacturing PMI',
          impact: 'high',
          forecast: '49.5',
          previous: '49.3'
        },
        {
          id: '3',
          time: '14:00',
          country: 'US',
          event: 'FOMC Meeting Minutes',
          impact: 'high'
        },
        {
          id: '4',
          time: '02:00',
          country: 'CN',
          event: 'Caixin Manufacturing PMI',
          impact: 'medium',
          actual: '50.5',
          forecast: '50.2',
          previous: '50.3'
        },
        {
          id: '5',
          time: '04:30',
          country: 'GB',
          event: 'Construction PMI',
          impact: 'medium',
          forecast: '54.0',
          previous: '53.6'
        },
        {
          id: '6',
          time: '05:00',
          country: 'EU',
          event: 'CPI Flash Estimate YoY',
          impact: 'high',
          forecast: '2.4%',
          previous: '2.3%'
        },
        {
          id: '7',
          time: '19:30',
          country: 'AU',
          event: 'RBA Interest Rate Decision',
          impact: 'high',
          forecast: '4.35%',
          previous: '4.35%'
        },
        {
          id: '8',
          time: '21:30',
          country: 'JP',
          event: 'Tankan Manufacturing Index',
          impact: 'medium',
          forecast: '13',
          previous: '13'
        }
      ];

      setEvents(demoEvents);
    } catch (err) {
      setError('Failed to load calendar');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadEvents();
    const interval = setInterval(loadEvents, 300000); // Refresh every 5 min
    return () => clearInterval(interval);
  }, []);

  const getImpactColor = (impact: string) => {
    switch (impact) {
      case 'high':
        return FINCEPT_RED;
      case 'medium':
        return FINCEPT_ORANGE;
      default:
        return FINCEPT_GRAY;
    }
  };

  const getImpactDots = (impact: string) => {
    const count = impact === 'high' ? 3 : impact === 'medium' ? 2 : 1;
    return '●'.repeat(count) + '○'.repeat(3 - count);
  };

  const today = new Date().toLocaleDateString('en-US', {
    weekday: 'short',
    month: 'short',
    day: 'numeric'
  });

  return (
    <BaseWidget
      id={id}
      title={`ECONOMIC CALENDAR - ${today.toUpperCase()}`}
      onRemove={onRemove}
      onRefresh={loadEvents}
      isLoading={loading}
      error={error}
      headerColor={FINCEPT_ORANGE}
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '50px 30px 1fr 60px',
          gap: '4px',
          padding: '4px 8px',
          borderBottom: '1px solid #333',
          color: FINCEPT_GRAY,
          fontSize: '8px'
        }}>
          <span>TIME</span>
          <span></span>
          <span>EVENT</span>
          <span style={{ textAlign: 'right' }}>IMPACT</span>
        </div>

        {events.slice(0, limit).map((event) => (
          <div
            key={event.id}
            style={{
              display: 'grid',
              gridTemplateColumns: '50px 30px 1fr 60px',
              gap: '4px',
              padding: '6px 8px',
              borderBottom: '1px solid #222',
              alignItems: 'center'
            }}
          >
            <span style={{ fontSize: '10px', color: FINCEPT_WHITE, fontFamily: 'monospace' }}>
              {event.time}
            </span>
            <span style={{ fontSize: '12px' }}>
              {COUNTRY_FLAGS[event.country] || event.country}
            </span>
            <div>
              <div style={{ fontSize: '10px', color: FINCEPT_WHITE }}>
                {event.event}
              </div>
              <div style={{ fontSize: '8px', color: FINCEPT_GRAY }}>
                {event.actual && <span style={{ color: FINCEPT_GREEN }}>A: {event.actual} </span>}
                {event.forecast && <span>F: {event.forecast} </span>}
                {event.previous && <span>P: {event.previous}</span>}
              </div>
            </div>
            <span style={{
              textAlign: 'right',
              fontSize: '10px',
              color: getImpactColor(event.impact),
              letterSpacing: '1px'
            }}>
              {getImpactDots(event.impact)}
            </span>
          </div>
        ))}

        {events.length === 0 && !loading && (
          <div style={{ padding: '12px', textAlign: 'center', color: FINCEPT_GRAY, fontSize: '10px' }}>
            <Calendar size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <div>No events scheduled</div>
          </div>
        )}

        {/* Legend */}
        <div style={{
          display: 'flex',
          justifyContent: 'center',
          gap: '12px',
          padding: '6px',
          borderTop: '1px solid #333',
          fontSize: '8px'
        }}>
          <span style={{ color: FINCEPT_RED }}>●●● High</span>
          <span style={{ color: FINCEPT_ORANGE }}>●●○ Medium</span>
          <span style={{ color: FINCEPT_GRAY }}>●○○ Low</span>
        </div>

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: FINCEPT_ORANGE,
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px'
            }}
          >
            <span>View Full Calendar</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
