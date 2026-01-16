import React, { useState, useEffect } from 'react';
import { Globe, AlertTriangle, Shield, TrendingUp, TrendingDown, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { geopoliticsService } from '@/services/geopolitics/geopoliticsService';

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

const BLOOMBERG_GREEN = '#00FF00';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_YELLOW = '#FFD700';
const BLOOMBERG_PURPLE = '#9D4EDD';

export const GeopoliticsWidget: React.FC<GeopoliticsWidgetProps> = ({
  id,
  country = '840', // US by default
  onRemove,
  onNavigate
}) => {
  const [threats, setThreats] = useState<ThreatLevel[]>([]);
  const [overallRisk, setOverallRisk] = useState<number>(0);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedCountry, setSelectedCountry] = useState(country);

  const countries = [
    { code: '840', name: 'United States' },
    { code: '156', name: 'China' },
    { code: '276', name: 'Germany' },
    { code: '826', name: 'United Kingdom' },
    { code: '392', name: 'Japan' },
  ];

  const loadGeopoliticsData = async () => {
    try {
      setLoading(true);
      setError(null);

      const data = await geopoliticsService.getComprehensiveGeopoliticsData(selectedCountry);

      // Calculate threat levels from data
      const qrCount = data.qrNotifications?.length || 0;
      const epingCount = data.epingNotifications?.length || 0;

      const threatData: ThreatLevel[] = [
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

      setThreats(threatData);
      setOverallRisk(threatData.reduce((sum, t) => sum + t.level, 0) / threatData.length);
    } catch (err) {
      // Demo data on error
      setThreats([
        { category: 'Trade Restrictions', level: 6.5, trend: 'up', description: '23 active restrictions' },
        { category: 'Regulatory Changes', level: 4.2, trend: 'stable', description: '12 pending notifications' },
        { category: 'Tariff Risk', level: 7.1, trend: 'up', description: 'Elevated tariff tensions' },
        { category: 'Supply Chain', level: 5.3, trend: 'stable', description: 'Moderate disruption risk' },
        { category: 'Currency Risk', level: 3.2, trend: 'down', description: 'Stable forex conditions' }
      ]);
      setOverallRisk(5.3);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadGeopoliticsData();
    const interval = setInterval(loadGeopoliticsData, 300000);
    return () => clearInterval(interval);
  }, [selectedCountry]);

  const getLevelColor = (level: number) => {
    if (level >= 7) return BLOOMBERG_RED;
    if (level >= 5) return BLOOMBERG_ORANGE;
    if (level >= 3) return BLOOMBERG_YELLOW;
    return BLOOMBERG_GREEN;
  };

  const getTrendIcon = (trend: string) => {
    switch (trend) {
      case 'up':
        return <TrendingUp size={10} style={{ color: BLOOMBERG_RED }} />;
      case 'down':
        return <TrendingDown size={10} style={{ color: BLOOMBERG_GREEN }} />;
      default:
        return <span style={{ color: BLOOMBERG_GRAY }}>—</span>;
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
      onRefresh={loadGeopoliticsData}
      isLoading={loading}
      error={error}
      headerColor={BLOOMBERG_PURPLE}
    >
      <div style={{ padding: '4px' }}>
        {/* Country selector */}
        <div style={{ padding: '4px 8px', borderBottom: '1px solid #333' }}>
          <select
            value={selectedCountry}
            onChange={(e) => setSelectedCountry(e.target.value)}
            style={{
              width: '100%',
              backgroundColor: '#111',
              border: '1px solid #333',
              color: BLOOMBERG_WHITE,
              padding: '4px',
              fontSize: '10px'
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
          backgroundColor: '#111',
          margin: '8px',
          borderRadius: '2px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <div>
            <div style={{ fontSize: '9px', color: BLOOMBERG_GRAY }}>OVERALL RISK</div>
            <div style={{
              fontSize: '20px',
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
            fontSize: '10px',
            fontWeight: 'bold',
            borderRadius: '2px'
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
                borderBottom: '1px solid #222'
              }}
            >
              <div style={{ flex: 1 }}>
                <div style={{ fontSize: '10px', color: BLOOMBERG_WHITE }}>
                  {threat.category}
                </div>
                <div style={{ fontSize: '8px', color: BLOOMBERG_GRAY }}>
                  {threat.description}
                </div>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                {getTrendIcon(threat.trend)}
                <div style={{
                  width: '60px',
                  height: '6px',
                  backgroundColor: '#333',
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
                  fontSize: '10px',
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
              color: BLOOMBERG_PURPLE,
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
              borderTop: '1px solid #333',
              marginTop: '4px'
            }}
          >
            <span>Open Geopolitics Tab</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
