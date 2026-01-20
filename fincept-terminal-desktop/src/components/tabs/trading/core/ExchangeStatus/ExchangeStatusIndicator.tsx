/**
 * Exchange Status Indicator
 * Compact component showing exchange operational status
 * Uses fetchStatus and fetchTime for real-time monitoring
 */

import React from 'react';
import { Activity, AlertTriangle, XCircle, Clock, Wifi, RefreshCw } from 'lucide-react';
import { useExchangeStatus, useExchangeTime } from '../../hooks/useExchangeStatus';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
};

interface ExchangeStatusIndicatorProps {
  compact?: boolean;
  showLatency?: boolean;
}

export function ExchangeStatusIndicator({ compact = false, showLatency = true }: ExchangeStatusIndicatorProps) {
  const { activeBroker, tradingMode } = useBrokerContext();
  const { status, isLoading, refresh } = useExchangeStatus(true, 30000);
  const { latency, serverTime } = useExchangeTime();

  const getStatusConfig = () => {
    if (!status) {
      return {
        color: FINCEPT.GRAY,
        bgColor: `${FINCEPT.GRAY}20`,
        icon: Activity,
        label: 'Unknown',
        pulse: false,
      };
    }

    switch (status.status) {
      case 'ok':
        return {
          color: FINCEPT.GREEN,
          bgColor: `${FINCEPT.GREEN}20`,
          icon: Activity,
          label: 'Operational',
          pulse: true,
        };
      case 'maintenance':
        return {
          color: FINCEPT.YELLOW,
          bgColor: `${FINCEPT.YELLOW}20`,
          icon: Clock,
          label: 'Maintenance',
          pulse: false,
        };
      case 'degraded':
        return {
          color: FINCEPT.ORANGE,
          bgColor: `${FINCEPT.ORANGE}20`,
          icon: AlertTriangle,
          label: 'Degraded',
          pulse: true,
        };
      case 'offline':
        return {
          color: FINCEPT.RED,
          bgColor: `${FINCEPT.RED}20`,
          icon: XCircle,
          label: 'Offline',
          pulse: false,
        };
      default:
        return {
          color: FINCEPT.GRAY,
          bgColor: `${FINCEPT.GRAY}20`,
          icon: Activity,
          label: 'Unknown',
          pulse: false,
        };
    }
  };

  const config = getStatusConfig();
  const Icon = config.icon;

  const formatLatency = (ms: number | null) => {
    if (ms === null) return '--';
    if (ms < 100) return `${ms}ms`;
    if (ms < 1000) return `${ms}ms`;
    return `${(ms / 1000).toFixed(1)}s`;
  };

  const getLatencyColor = (ms: number | null) => {
    if (ms === null) return FINCEPT.GRAY;
    if (ms < 100) return FINCEPT.GREEN;
    if (ms < 300) return FINCEPT.YELLOW;
    if (ms < 1000) return FINCEPT.ORANGE;
    return FINCEPT.RED;
  };

  const exchangeName = tradingMode === 'paper'
    ? 'Paper Trading'
    : (activeBroker || 'Exchange').charAt(0).toUpperCase() + (activeBroker || 'exchange').slice(1);

  if (compact) {
    return (
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          padding: '4px 8px',
          backgroundColor: config.bgColor,
          borderRadius: '4px',
          cursor: 'pointer',
        }}
        onClick={refresh}
        title={`${exchangeName}: ${config.label}${latency !== null ? ` | Latency: ${formatLatency(latency)}` : ''}`}
      >
        <div
          style={{
            width: '6px',
            height: '6px',
            borderRadius: '50%',
            backgroundColor: config.color,
            animation: config.pulse ? 'pulse 2s infinite' : 'none',
          }}
        />
        <span style={{ fontSize: '9px', color: config.color, fontWeight: 600 }}>
          {config.label.toUpperCase()}
        </span>
        {showLatency && latency !== null && (
          <span style={{ fontSize: '8px', color: getLatencyColor(latency), fontFamily: '"IBM Plex Mono", monospace' }}>
            {formatLatency(latency)}
          </span>
        )}
      </div>
    );
  }

  return (
    <div
      style={{
        background: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        padding: '12px',
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: '12px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Icon size={14} color={config.color} />
          <span style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>
            {exchangeName} Status
          </span>
        </div>
        <button
          onClick={refresh}
          disabled={isLoading}
          style={{
            background: 'transparent',
            border: 'none',
            cursor: isLoading ? 'wait' : 'pointer',
            padding: '4px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
          title="Refresh status"
        >
          <RefreshCw
            size={12}
            color={FINCEPT.GRAY}
            style={{
              animation: isLoading ? 'spin 1s linear infinite' : 'none',
            }}
          />
        </button>
      </div>

      {/* Status Row */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          padding: '10px',
          backgroundColor: config.bgColor,
          borderRadius: '4px',
          marginBottom: '10px',
        }}
      >
        <div
          style={{
            width: '10px',
            height: '10px',
            borderRadius: '50%',
            backgroundColor: config.color,
            boxShadow: config.pulse ? `0 0 8px ${config.color}` : 'none',
            animation: config.pulse ? 'pulse 2s infinite' : 'none',
          }}
        />
        <div>
          <div style={{ fontSize: '12px', color: config.color, fontWeight: 700 }}>
            {config.label}
          </div>
          {status?.eta && (
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              ETA: {status.eta}
            </div>
          )}
        </div>
      </div>

      {/* Stats Row */}
      <div style={{ display: 'flex', gap: '12px' }}>
        {/* Latency */}
        {showLatency && (
          <div
            style={{
              flex: 1,
              background: FINCEPT.DARK_BG,
              borderRadius: '4px',
              padding: '8px',
              textAlign: 'center',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', marginBottom: '4px' }}>
              <Wifi size={10} color={FINCEPT.GRAY} />
              <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 600 }}>LATENCY</span>
            </div>
            <div
              style={{
                fontSize: '14px',
                fontWeight: 700,
                fontFamily: '"IBM Plex Mono", monospace',
                color: getLatencyColor(latency),
              }}
            >
              {formatLatency(latency)}
            </div>
          </div>
        )}

        {/* Last Updated */}
        <div
          style={{
            flex: 1,
            background: FINCEPT.DARK_BG,
            borderRadius: '4px',
            padding: '8px',
            textAlign: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', marginBottom: '4px' }}>
            <Clock size={10} color={FINCEPT.GRAY} />
            <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 600 }}>UPDATED</span>
          </div>
          <div
            style={{
              fontSize: '10px',
              fontWeight: 600,
              fontFamily: '"IBM Plex Mono", monospace',
              color: FINCEPT.WHITE,
            }}
          >
            {status?.updated ? new Date(status.updated).toLocaleTimeString() : '--'}
          </div>
        </div>

        {/* Server Time */}
        {serverTime && (
          <div
            style={{
              flex: 1,
              background: FINCEPT.DARK_BG,
              borderRadius: '4px',
              padding: '8px',
              textAlign: 'center',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', marginBottom: '4px' }}>
              <Activity size={10} color={FINCEPT.GRAY} />
              <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 600 }}>SERVER</span>
            </div>
            <div
              style={{
                fontSize: '10px',
                fontWeight: 600,
                fontFamily: '"IBM Plex Mono", monospace',
                color: FINCEPT.CYAN,
              }}
            >
              {new Date(serverTime).toLocaleTimeString()}
            </div>
          </div>
        )}
      </div>

      {/* CSS for animations */}
      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
