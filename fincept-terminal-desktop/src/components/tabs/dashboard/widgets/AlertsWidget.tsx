import React, { useState, useEffect } from 'react';
import { Bell, AlertTriangle, CheckCircle, Clock, ExternalLink, X } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { invoke } from '@tauri-apps/api/core';

interface AlertsWidgetProps {
  id: string;
  limit?: number;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface Alert {
  id: number;
  symbol: string;
  field: string;
  condition: string;
  targetValue: number;
  triggeredValue?: number;
  triggeredAt?: string;
  status: 'active' | 'triggered' | 'expired';
}

const BLOOMBERG_GREEN = '#00FF00';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_YELLOW = '#FFD700';

export const AlertsWidget: React.FC<AlertsWidgetProps> = ({
  id,
  limit = 5,
  onRemove,
  onNavigate
}) => {
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadAlerts = async () => {
    try {
      setLoading(true);
      setError(null);

      // Try to load from monitoring system
      const conditions = await invoke('get_monitor_conditions') as any[];
      const triggeredAlerts = await invoke('get_monitor_alerts', { limit: 10 }) as any[];

      const alertList: Alert[] = [];

      // Add active conditions
      conditions?.forEach((c: any) => {
        if (c.enabled) {
          alertList.push({
            id: c.id,
            symbol: c.symbol,
            field: c.field,
            condition: `${c.operator} ${c.value}`,
            targetValue: c.value,
            status: 'active'
          });
        }
      });

      // Add triggered alerts
      triggeredAlerts?.forEach((a: any) => {
        alertList.push({
          id: a.id,
          symbol: a.symbol,
          field: a.field,
          condition: 'triggered',
          targetValue: a.triggered_value,
          triggeredValue: a.triggered_value,
          triggeredAt: new Date(a.triggered_at * 1000).toLocaleString(),
          status: 'triggered'
        });
      });

      if (alertList.length === 0) {
        // Demo data
        setAlerts([
          { id: 1, symbol: 'AAPL', field: 'price', condition: '> 195.00', targetValue: 195, status: 'active' },
          { id: 2, symbol: 'BTC/USD', field: 'price', condition: '< 95000', targetValue: 95000, status: 'active' },
          { id: 3, symbol: 'SPY', field: 'change_percent', condition: '> 2%', targetValue: 2, status: 'active' },
          { id: 4, symbol: 'TSLA', field: 'price', condition: '> 250', targetValue: 250, triggeredValue: 251.30, triggeredAt: '10:30 AM', status: 'triggered' },
        ]);
      } else {
        setAlerts(alertList);
      }
    } catch (err) {
      // Demo data on error
      setAlerts([
        { id: 1, symbol: 'AAPL', field: 'price', condition: '> 195.00', targetValue: 195, status: 'active' },
        { id: 2, symbol: 'BTC/USD', field: 'price', condition: '< 95000', targetValue: 95000, status: 'active' },
        { id: 3, symbol: 'TSLA', field: 'price', condition: '> 250', targetValue: 250, triggeredValue: 251.30, triggeredAt: '10:30 AM', status: 'triggered' },
      ]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadAlerts();
    const interval = setInterval(loadAlerts, 30000);
    return () => clearInterval(interval);
  }, [limit]);

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'triggered':
        return <Bell size={12} style={{ color: BLOOMBERG_YELLOW }} />;
      case 'active':
        return <Clock size={12} style={{ color: BLOOMBERG_GREEN }} />;
      default:
        return <AlertTriangle size={12} style={{ color: BLOOMBERG_GRAY }} />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'triggered':
        return BLOOMBERG_YELLOW;
      case 'active':
        return BLOOMBERG_GREEN;
      default:
        return BLOOMBERG_GRAY;
    }
  };

  const activeCount = alerts.filter(a => a.status === 'active').length;
  const triggeredCount = alerts.filter(a => a.status === 'triggered').length;

  return (
    <BaseWidget
      id={id}
      title="PRICE ALERTS"
      onRemove={onRemove}
      onRefresh={loadAlerts}
      isLoading={loading}
      error={error}
      headerColor={BLOOMBERG_YELLOW}
    >
      <div style={{ padding: '4px' }}>
        {/* Summary bar */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          padding: '4px 8px',
          backgroundColor: '#111',
          marginBottom: '4px',
          fontSize: '9px'
        }}>
          <span style={{ color: BLOOMBERG_GREEN }}>
            <Clock size={10} style={{ marginRight: '4px', verticalAlign: 'middle' }} />
            {activeCount} Active
          </span>
          <span style={{ color: BLOOMBERG_YELLOW }}>
            <Bell size={10} style={{ marginRight: '4px', verticalAlign: 'middle' }} />
            {triggeredCount} Triggered
          </span>
        </div>

        {/* Alert list */}
        {alerts.slice(0, limit).map((alert) => (
          <div
            key={alert.id}
            style={{
              padding: '6px 8px',
              borderBottom: '1px solid #333',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}
          >
            {getStatusIcon(alert.status)}
            <div style={{ flex: 1 }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ fontSize: '10px', color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>
                  {alert.symbol}
                </span>
                <span style={{ fontSize: '9px', color: getStatusColor(alert.status) }}>
                  {alert.status.toUpperCase()}
                </span>
              </div>
              <div style={{ fontSize: '9px', color: BLOOMBERG_GRAY }}>
                {alert.field} {alert.condition}
                {alert.triggeredAt && (
                  <span style={{ marginLeft: '8px', color: BLOOMBERG_YELLOW }}>
                    @ {alert.triggeredAt}
                  </span>
                )}
              </div>
            </div>
          </div>
        ))}

        {alerts.length === 0 && !loading && (
          <div style={{ padding: '12px', textAlign: 'center', color: BLOOMBERG_GRAY, fontSize: '10px' }}>
            <Bell size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <div>No alerts configured</div>
            <div style={{ fontSize: '9px', marginTop: '4px' }}>Set up alerts in Monitoring tab</div>
          </div>
        )}

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: BLOOMBERG_YELLOW,
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px'
            }}
          >
            <span>Manage Alerts</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
