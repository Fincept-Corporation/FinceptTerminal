import React, { useState, useMemo } from 'react';
import { TrendingUp, TrendingDown, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { geopoliticsService } from '@/services/geopolitics/geopoliticsService';
import { useTranslation } from 'react-i18next';
import { useCache } from '@/hooks/useCache';

interface GeopoliticsWidgetProps {
  id: string;
  country?: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface ThreatLevel {
  category: string;
  level: number;
  trend: 'up' | 'down' | 'stable';
  description: string;
}

interface GeopoliticsData {
  threats: ThreatLevel[];
  overallRisk: number;
}

const DEMO_DATA: GeopoliticsData = {
  threats: [
    { category: 'Trade Restrictions', level: 6.5, trend: 'up', description: '23 active restrictions' },
    { category: 'Regulatory Changes', level: 4.2, trend: 'stable', description: '12 pending notifications' },
    { category: 'Tariff Risk', level: 7.1, trend: 'up', description: 'Elevated tariff tensions' },
    { category: 'Supply Chain', level: 5.3, trend: 'stable', description: 'Moderate disruption risk' },
    { category: 'Currency Risk', level: 3.2, trend: 'down', description: 'Stable forex conditions' }
  ],
  overallRisk: 5.3
};

export const GeopoliticsWidget: React.FC<GeopoliticsWidgetProps> = ({
  id,
  country = '840', // US by default
  onRemove,
  onNavigate
}) => {
  const { t } = useTranslation('dashboard');
  const [selectedCountry, setSelectedCountry] = useState(country);

  const countries = [
    { code: '840', name: 'United States' },
    { code: '156', name: 'China' },
    { code: '276', name: 'Germany' },
    { code: '826', name: 'United Kingdom' },
    { code: '392', name: 'Japan' },
  ];

  const {
    data: geoData,
    isLoading: loading,
    isFetching,
    error,
    refresh
  } = useCache<GeopoliticsData>({
    key: `widget:geopolitics:${selectedCountry}`,
    category: 'api-response',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        const data = await geopoliticsService.getComprehensiveGeopoliticsData(selectedCountry);

        const qrCount = data.qrNotifications?.length || 0;
        const epingCount = data.epingNotifications?.length || 0;

        const threats: ThreatLevel[] = [
          {
            category: 'Trade Restrictions',
            level: Math.min(qrCount > 0 ? 2 + (qrCount / 50) * 8 : 0, 10),
            trend: qrCount > 5 ? 'up' : 'stable',
            description: `${qrCount} active restrictions`
          },
          {
            category: 'Regulatory Changes',
            level: Math.min(epingCount > 0 ? 2 + (epingCount / 30) * 6 : 1, 10),
            trend: epingCount > 10 ? 'up' : 'stable',
            description: `${epingCount} pending notifications`
          },
          {
            category: 'Tariff Risk',
            level: 5.5,
            trend: 'up',
            description: 'Elevated tariff tensions'
          },
          {
            category: 'Supply Chain',
            level: 4.2,
            trend: 'stable',
            description: 'Moderate disruption risk'
          },
          {
            category: 'Currency Risk',
            level: 3.8,
            trend: 'down',
            description: 'Stable forex conditions'
          }
        ];

        const overallRisk = threats.reduce((sum, t) => sum + t.level, 0) / threats.length;
        return { threats, overallRisk };
      } catch {
        return DEMO_DATA;
      }
    }
  });

  const { threats, overallRisk } = geoData || DEMO_DATA;

  const getLevelColor = (level: number) => {
    if (level >= 7) return 'var(--ft-color-alert)';
    if (level >= 5) return 'var(--ft-color-primary)';
    if (level >= 3) return 'var(--ft-color-warning)';
    return 'var(--ft-color-success)';
  };

  const getTrendIcon = (trend: string) => {
    switch (trend) {
      case 'up':
        return <TrendingUp size={10} style={{ color: 'var(--ft-color-alert)' }} />;
      case 'down':
        return <TrendingDown size={10} style={{ color: 'var(--ft-color-success)' }} />;
      default:
        return <span style={{ color: 'var(--ft-color-text-muted)' }}>—</span>;
    }
  };

  const getRiskLabel = (level: number) => {
    if (level >= 7) return 'HIGH';
    if (level >= 5) return 'ELEVATED';
    if (level >= 3) return 'MODERATE';
    return 'LOW';
  };

  return (
    <BaseWidget
      id={id}
      title="GEOPOLITICAL RISK MONITOR"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(loading && !geoData) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-purple)"
    >
      <div style={{ padding: '4px' }}>
        {/* Country selector */}
        <div style={{ padding: '4px 8px', borderBottom: '1px solid var(--ft-border-color)' }}>
          <select
            value={selectedCountry}
            onChange={(e) => setSelectedCountry(e.target.value)}
            style={{
              width: '100%',
              backgroundColor: 'var(--ft-color-panel)',
              border: '1px solid var(--ft-border-color)',
              color: 'var(--ft-color-text)',
              padding: '4px',
              fontSize: 'var(--ft-font-size-small)'
            }}
          >
            {countries.map(c => (
              <option key={c.code} value={c.code}>{c.name}</option>
            ))}
          </select>
        </div>

        {/* Overall risk indicator */}
        <div style={{
          padding: '8px',
          backgroundColor: 'var(--ft-color-panel)',
          margin: '8px',
          borderRadius: 'var(--ft-border-radius)',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <div>
            <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>{t('widgets.overallRisk')}</div>
            <div style={{
              fontSize: 'var(--ft-font-size-heading)',
              fontWeight: 'bold',
              color: getLevelColor(overallRisk)
            }}>
              {overallRisk.toFixed(1)}/10
            </div>
          </div>
          <div style={{
            backgroundColor: getLevelColor(overallRisk),
            color: '#000',
            padding: '4px 8px',
            fontSize: 'var(--ft-font-size-small)',
            fontWeight: 'bold',
            borderRadius: 'var(--ft-border-radius)'
          }}>
            {getRiskLabel(overallRisk)}
          </div>
        </div>

        {/* Threat breakdown */}
        <div style={{ padding: '0 4px' }}>
          {threats.map((threat, idx) => (
            <div
              key={idx}
              style={{
                display: 'flex',
                alignItems: 'center',
                padding: '6px 8px',
                borderBottom: '1px solid var(--ft-border-color)'
              }}
            >
              <div style={{ flex: 1 }}>
                <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                  {threat.category}
                </div>
                <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
                  {threat.description}
                </div>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                {getTrendIcon(threat.trend)}
                <div style={{
                  width: '60px',
                  height: '6px',
                  backgroundColor: 'var(--ft-border-color)',
                  borderRadius: '3px',
                  overflow: 'hidden'
                }}>
                  <div style={{
                    width: `${threat.level * 10}%`,
                    height: '100%',
                    backgroundColor: getLevelColor(threat.level),
                    transition: 'width 0.3s'
                  }} />
                </div>
                <span style={{
                  fontSize: 'var(--ft-font-size-small)',
                  fontWeight: 'bold',
                  color: getLevelColor(threat.level),
                  width: '24px',
                  textAlign: 'right'
                }}>
                  {threat.level.toFixed(1)}
                </span>
              </div>
            </div>
          ))}
        </div>

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: 'var(--ft-color-purple)',
              fontSize: 'var(--ft-font-size-tiny)',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
              borderTop: '1px solid var(--ft-border-color)',
              marginTop: '4px'
            }}
          >
            <span>{t('widgets.openGeopoliticsTab')}</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
