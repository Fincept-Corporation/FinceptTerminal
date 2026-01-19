/**
 * MasterContractManager Component
 *
 * Standalone component for managing master contract symbol data.
 * Can be used in Settings or as a dedicated panel.
 */

import React, { useState, useEffect } from 'react';
import { Database, Download, RefreshCw, Check, AlertCircle, Trash2, Clock } from 'lucide-react';
import {
  masterContractService,
  type SupportedBroker,
  type DownloadResult,
  getBrokerDisplayName,
} from '@/services/trading/masterContractService';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A'
};

interface BrokerSymbolStatus {
  brokerId: SupportedBroker;
  symbolCount: number;
  lastUpdated: Date | null;
  isLoading: boolean;
  error: string | null;
}

interface MasterContractManagerProps {
  /** List of brokers to manage */
  brokers?: SupportedBroker[];
  /** Compact mode for embedding in other panels */
  compact?: boolean;
  /** Callback when symbols are updated */
  onUpdate?: (brokerId: SupportedBroker, result: DownloadResult) => void;
}

export function MasterContractManager({
  brokers = ['angelone', 'fyers', 'zerodha'],
  compact = false,
  onUpdate
}: MasterContractManagerProps) {
  const [brokerStatus, setBrokerStatus] = useState<Record<string, BrokerSymbolStatus>>({});
  const [isUpdatingAll, setIsUpdatingAll] = useState(false);

  // Load initial status for all brokers
  useEffect(() => {
    const loadStatus = async () => {
      const status: Record<string, BrokerSymbolStatus> = {};

      for (const brokerId of brokers) {
        try {
          const count = await masterContractService.getSymbolCount(brokerId);
          const cache = await masterContractService.getMasterContractCache(brokerId);

          status[brokerId] = {
            brokerId,
            symbolCount: count,
            lastUpdated: cache ? new Date(cache.last_updated * 1000) : null,
            isLoading: false,
            error: null
          };
        } catch (error) {
          status[brokerId] = {
            brokerId,
            symbolCount: 0,
            lastUpdated: null,
            isLoading: false,
            error: null
          };
        }
      }

      setBrokerStatus(status);
    };

    loadStatus();
  }, [brokers]);

  // Update a single broker
  const handleUpdateBroker = async (brokerId: SupportedBroker) => {
    setBrokerStatus(prev => ({
      ...prev,
      [brokerId]: { ...prev[brokerId], isLoading: true, error: null }
    }));

    try {
      const result = await masterContractService.downloadMasterContract(brokerId);

      setBrokerStatus(prev => ({
        ...prev,
        [brokerId]: {
          ...prev[brokerId],
          symbolCount: result.symbol_count,
          lastUpdated: new Date(),
          isLoading: false,
          error: result.success ? null : result.message
        }
      }));

      onUpdate?.(brokerId, result);
    } catch (error: any) {
      setBrokerStatus(prev => ({
        ...prev,
        [brokerId]: {
          ...prev[brokerId],
          isLoading: false,
          error: error.message || 'Update failed'
        }
      }));
    }
  };

  // Update all brokers
  const handleUpdateAll = async () => {
    setIsUpdatingAll(true);

    for (const brokerId of brokers) {
      await handleUpdateBroker(brokerId);
    }

    setIsUpdatingAll(false);
  };

  // Format last updated time
  const formatLastUpdated = (date: Date | null): string => {
    if (!date) return 'Never';

    const now = new Date();
    const diff = now.getTime() - date.getTime();
    const hours = Math.floor(diff / (1000 * 60 * 60));
    const days = Math.floor(hours / 24);

    if (days > 0) return `${days}d ago`;
    if (hours > 0) return `${hours}h ago`;
    return 'Just now';
  };

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      padding: compact ? '12px' : '16px'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: compact ? '12px' : '16px',
        paddingBottom: '12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Database size={compact ? 16 : 20} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: compact ? '12px' : '14px',
            fontWeight: 700,
            color: FINCEPT.WHITE
          }}>
            Symbol Database
          </span>
        </div>

        <button
          onClick={handleUpdateAll}
          disabled={isUpdatingAll}
          style={{
            padding: '6px 12px',
            backgroundColor: FINCEPT.ORANGE,
            border: 'none',
            color: FINCEPT.DARK_BG,
            cursor: isUpdatingAll ? 'not-allowed' : 'pointer',
            fontSize: '10px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            opacity: isUpdatingAll ? 0.7 : 1
          }}
        >
          {isUpdatingAll ? (
            <RefreshCw size={12} className="animate-spin" />
          ) : (
            <Download size={12} />
          )}
          UPDATE ALL
        </button>
      </div>

      {/* Broker List */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        {brokers.map(brokerId => {
          const status = brokerStatus[brokerId] || {
            brokerId,
            symbolCount: 0,
            lastUpdated: null,
            isLoading: false,
            error: null
          };

          return (
            <div
              key={brokerId}
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '10px 12px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              {/* Broker Info */}
              <div style={{ flex: 1 }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{
                    fontSize: '12px',
                    fontWeight: 600,
                    color: FINCEPT.WHITE
                  }}>
                    {getBrokerDisplayName(brokerId)}
                  </span>
                  {status.error && (
                    <AlertCircle size={12} color={FINCEPT.RED} />
                  )}
                </div>
                <div style={{
                  display: 'flex',
                  gap: '12px',
                  marginTop: '4px',
                  fontSize: '10px',
                  color: FINCEPT.GRAY
                }}>
                  <span>
                    {status.symbolCount > 0
                      ? `${status.symbolCount.toLocaleString()} symbols`
                      : 'No data'
                    }
                  </span>
                  <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <Clock size={10} />
                    {formatLastUpdated(status.lastUpdated)}
                  </span>
                </div>
                {status.error && (
                  <div style={{
                    fontSize: '9px',
                    color: FINCEPT.RED,
                    marginTop: '4px'
                  }}>
                    {status.error}
                  </div>
                )}
              </div>

              {/* Update Button */}
              <button
                onClick={() => handleUpdateBroker(brokerId)}
                disabled={status.isLoading}
                style={{
                  padding: '6px 10px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${status.isLoading ? FINCEPT.MUTED : FINCEPT.CYAN}`,
                  color: status.isLoading ? FINCEPT.MUTED : FINCEPT.CYAN,
                  cursor: status.isLoading ? 'not-allowed' : 'pointer',
                  fontSize: '9px',
                  fontWeight: 600,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
              >
                {status.isLoading ? (
                  <>
                    <RefreshCw size={10} className="animate-spin" />
                    UPDATING...
                  </>
                ) : status.symbolCount > 0 ? (
                  <>
                    <RefreshCw size={10} />
                    REFRESH
                  </>
                ) : (
                  <>
                    <Download size={10} />
                    DOWNLOAD
                  </>
                )}
              </button>
            </div>
          );
        })}
      </div>

      {/* Info Text */}
      <div style={{
        marginTop: '12px',
        padding: '8px',
        backgroundColor: FINCEPT.DARK_BG,
        fontSize: '9px',
        color: FINCEPT.MUTED,
        lineHeight: 1.4
      }}>
        Symbol data is cached locally for fast offline search.
        Update daily to get latest instruments including new F&O expiries.
        Angel One: ~70k symbols | Fyers: ~100k symbols
      </div>

      {/* Spinner animation CSS */}
      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
        .animate-spin {
          animation: spin 1s linear infinite;
        }
      `}</style>
    </div>
  );
}

export default MasterContractManager;
