// EconomicCalendarWidget - Economic Calendar from Backend API
// Displays upcoming economic events and releases

import React, { useMemo } from 'react';
import { Calendar, TrendingUp, TrendingDown } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { useTranslation } from 'react-i18next';
import { useCache } from '@/hooks/useCache';
import { useAuth } from '@/contexts/AuthContext';
import { fetch } from '@tauri-apps/plugin-http';

const FC = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
};

interface EconomicCalendarWidgetProps {
  id: string;
  country?: string;
  limit?: number;
  onRemove?: () => void;
  onConfigure?: () => void;
}

interface EconomicEvent {
  id?: number | string;
  event_datetime: string;
  event_name: string;
  currency: string | null;
  impact: string;
  actual?: string;
  forecast?: string;
  previous?: string;
  source?: string;
  detail_url?: string;
}


const getImpactColor = (impact: string) => {
  switch (impact.toLowerCase()) {
    case 'high': return FC.RED;
    case 'medium': return FC.YELLOW;
    case 'low': return FC.CYAN;
    default: return FC.GRAY;
  }
};

export const EconomicCalendarWidget: React.FC<EconomicCalendarWidgetProps> = ({
  id,
  country = 'US',
  limit = 10,
  onRemove,
  onConfigure
}) => {
  const { t } = useTranslation('dashboard');
  const { session } = useAuth();

  const {
    data: events,
    isLoading: loading,
    isFetching,
    error,
    refresh
  } = useCache<EconomicEvent[]>({
    key: `widget:economic-calendar:${country}:${limit}`,
    category: 'api-response',
    ttl: '15m',
    refetchInterval: 15 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      const apiKey = session?.api_key;
      if (!apiKey) {
        throw new Error('Login required to view economic calendar');
      }

      // Call backend API via Tauri with API key
      const response = await fetch(
        `https://api.fincept.in/macro/economic-calendar?country=${country}&limit=${limit}`,
        {
          method: 'GET',
          headers: {
            'Content-Type': 'application/json',
            'X-API-Key': apiKey,
          },
        }
      );

      if (!response.ok) {
        const errorData = await response.json().catch(() => ({}));
        const errorMsg = errorData.message || errorData.error || `API error: ${response.status}`;
        throw new Error(errorMsg);
      }

      const data = await response.json();

      console.log('[EconomicCalendarWidget] API response:', data);

      // Check if response is successful
      if (!data.success) {
        throw new Error(data.message || data.error || 'Failed to load calendar');
      }

      // Extract events array from response
      const eventsList = data.data?.events || [];

      console.log('[EconomicCalendarWidget] Parsed events:', eventsList);
      return eventsList;
    }
  });

  const displayEvents = useMemo(() => {
    if (!events || !Array.isArray(events)) return [];
    return events.slice(0, limit);
  }, [events, limit]);

  return (
    <BaseWidget
      id={id}
      title="ECONOMIC CALENDAR"
      onRemove={onRemove}
      onRefresh={refresh}
      onConfigure={onConfigure}
      isLoading={(loading && !events) || isFetching}
      error={error?.message}
      headerColor={FC.CYAN}
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '50px 1fr 60px',
          gap: '4px',
          fontSize: '8px',
          fontWeight: 700,
          color: FC.GRAY,
          borderBottom: `1px solid ${FC.MUTED}40`,
          padding: '4px 8px',
          marginBottom: '4px',
          textTransform: 'uppercase',
          letterSpacing: '0.5px',
        }}>
          <div>TIME</div>
          <div>EVENT</div>
          <div style={{ textAlign: 'center' }}>IMPACT</div>
        </div>

        {/* Events List */}
        {displayEvents.map((event, index) => (
          <div
            key={event.id || index}
            style={{
              display: 'grid',
              gridTemplateColumns: '50px 1fr 60px',
              gap: '4px',
              padding: '6px 8px',
              borderBottom: `1px solid ${FC.MUTED}20`,
              transition: 'background 0.2s',
              cursor: 'pointer',
            }}
            onMouseEnter={(e) => { (e.currentTarget as HTMLElement).style.backgroundColor = `${FC.MUTED}15`; }}
            onMouseLeave={(e) => { (e.currentTarget as HTMLElement).style.backgroundColor = 'transparent'; }}
          >
            {/* Time + Currency */}
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-start', gap: '2px' }}>
              <div style={{ fontSize: '9px', color: FC.WHITE, fontWeight: 700 }}>
                {new Date(event.event_datetime).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false })}
              </div>
              <div style={{ fontSize: '8px', color: FC.GRAY, fontWeight: 600 }}>
                {event.currency || '--'}
              </div>
            </div>

            {/* Event Details */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '2px', overflow: 'hidden' }}>
              <div style={{
                fontSize: '9px',
                color: FC.WHITE,
                fontWeight: 600,
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap',
              }}>
                {event.event_name}
              </div>

              {/* Forecast / Actual / Previous */}
              {(event.forecast || event.actual || event.previous) && (
                <div style={{
                  display: 'flex',
                  gap: '6px',
                  fontSize: '8px',
                  color: FC.MUTED,
                }}>
                  {event.actual && (
                    <span style={{ color: FC.GREEN }}>
                      A: {event.actual}
                    </span>
                  )}
                  {event.forecast && (
                    <span>
                      F: {event.forecast}
                    </span>
                  )}
                  {event.previous && (
                    <span>
                      P: {event.previous}
                    </span>
                  )}
                </div>
              )}
            </div>

            {/* Impact Badge */}
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
            }}>
              <div style={{
                padding: '2px 6px',
                borderRadius: '2px',
                backgroundColor: `${getImpactColor(event.impact)}20`,
                border: `1px solid ${getImpactColor(event.impact)}40`,
                fontSize: '7px',
                fontWeight: 700,
                color: getImpactColor(event.impact),
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
                display: 'flex',
                alignItems: 'center',
                gap: '2px',
              }}>
                <div style={{
                  width: '4px',
                  height: '4px',
                  borderRadius: '50%',
                  backgroundColor: getImpactColor(event.impact),
                }} />
                {event.impact.substring(0, 3)}
              </div>
            </div>
          </div>
        ))}

        {displayEvents.length === 0 && !loading && !error && (
          <div style={{
            padding: '24px',
            textAlign: 'center',
            color: FC.MUTED,
            fontSize: '9px',
          }}>
            No upcoming events
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
