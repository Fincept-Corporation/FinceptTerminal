import React from 'react';
import { Calendar, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { useTranslation } from 'react-i18next';
import { useCache } from '@/hooks/useCache';

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

const DEMO_EVENTS: EconomicEvent[] = [
  { id: '1', time: '08:30', country: 'US', event: 'Initial Jobless Claims', impact: 'high', forecast: '220K', previous: '217K' },
  { id: '2', time: '10:00', country: 'US', event: 'ISM Manufacturing PMI', impact: 'high', forecast: '49.5', previous: '49.3' },
  { id: '3', time: '14:00', country: 'US', event: 'FOMC Meeting Minutes', impact: 'high' },
  { id: '4', time: '02:00', country: 'CN', event: 'Caixin Manufacturing PMI', impact: 'medium', actual: '50.5', forecast: '50.2', previous: '50.3' },
  { id: '5', time: '04:30', country: 'GB', event: 'Construction PMI', impact: 'medium', forecast: '54.0', previous: '53.6' },
  { id: '6', time: '05:00', country: 'EU', event: 'CPI Flash Estimate YoY', impact: 'high', forecast: '2.4%', previous: '2.3%' },
  { id: '7', time: '19:30', country: 'AU', event: 'RBA Interest Rate Decision', impact: 'high', forecast: '4.35%', previous: '4.35%' },
  { id: '8', time: '21:30', country: 'JP', event: 'Tankan Manufacturing Index', impact: 'medium', forecast: '13', previous: '13' },
];

export const CalendarWidget: React.FC<CalendarWidgetProps> = ({
  id,
  limit = 6,
  onRemove,
  onNavigate
}) => {
  const { t } = useTranslation('dashboard');

  const {
    data: events,
    isLoading: loading,
    error,
    refresh
  } = useCache<EconomicEvent[]>({
    key: 'widget:economic-calendar',
    category: 'api-response',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      // Demo economic calendar data
      // In production, this would fetch from an economic calendar API
      return DEMO_EVENTS;
    }
  });

  const displayEvents = events || DEMO_EVENTS;

  const getImpactColor = (impact: string) => {
    switch (impact) {
      case 'high':
        return 'var(--ft-color-alert)';
      case 'medium':
        return 'var(--ft-color-primary)';
      default:
        return 'var(--ft-color-text-muted)';
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
      onRefresh={refresh}
      isLoading={loading && !events}
      error={error?.message || null}
      headerColor="var(--ft-color-primary)"
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '50px 30px 1fr 60px',
          gap: '4px',
          padding: '4px 8px',
          borderBottom: '1px solid var(--ft-border-color)',
          color: 'var(--ft-color-text-muted)',
          fontSize: 'var(--ft-font-size-tiny)'
        }}>
          <span>{t('widgets.time')}</span>
          <span></span>
          <span>{t('widgets.event')}</span>
          <span style={{ textAlign: 'right' }}>{t('widgets.impact')}</span>
        </div>

        {displayEvents.slice(0, limit).map((event) => (
          <div
            key={event.id}
            style={{
              display: 'grid',
              gridTemplateColumns: '50px 30px 1fr 60px',
              gap: '4px',
              padding: '6px 8px',
              borderBottom: '1px solid var(--ft-border-color)',
              alignItems: 'center'
            }}
          >
            <span style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)', fontFamily: 'monospace' }}>
              {event.time}
            </span>
            <span style={{ fontSize: '12px' }}>
              {COUNTRY_FLAGS[event.country] || event.country}
            </span>
            <div>
              <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                {event.event}
              </div>
              <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
                {event.actual && <span style={{ color: 'var(--ft-color-success)' }}>A: {event.actual} </span>}
                {event.forecast && <span>F: {event.forecast} </span>}
                {event.previous && <span>P: {event.previous}</span>}
              </div>
            </div>
            <span style={{
              textAlign: 'right',
              fontSize: 'var(--ft-font-size-small)',
              color: getImpactColor(event.impact),
              letterSpacing: '1px'
            }}>
              {getImpactDots(event.impact)}
            </span>
          </div>
        ))}

        {displayEvents.length === 0 && !loading && (
          <div style={{ padding: '12px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
            <Calendar size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <div>{t('widgets.noEventsScheduled')}</div>
          </div>
        )}

        {/* Legend */}
        <div style={{
          display: 'flex',
          justifyContent: 'center',
          gap: '12px',
          padding: '6px',
          borderTop: '1px solid var(--ft-border-color)',
          fontSize: 'var(--ft-font-size-tiny)'
        }}>
          <span style={{ color: 'var(--ft-color-alert)' }}>●●● {t('widgets.impactHigh')}</span>
          <span style={{ color: 'var(--ft-color-primary)' }}>●●○ {t('widgets.impactMedium')}</span>
          <span style={{ color: 'var(--ft-color-text-muted)' }}>●○○ {t('widgets.impactLow')}</span>
        </div>

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: 'var(--ft-color-primary)',
              fontSize: 'var(--ft-font-size-tiny)',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px'
            }}
          >
            <span>{t('widgets.viewFullCalendar')}</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
