import React from 'react';
import { Search, ChevronRight } from 'lucide-react';
import type { StockBrokerMetadata, Region } from '@/brokers/stocks/types';
import { COLORS, REGION_LABELS, ConnectionStatus, BrokerStatus } from './types';
import { StatusBadge } from './StatusBadge';

interface BrokerListPanelProps {
  availableBrokers: StockBrokerMetadata[];
  activeBroker: string | null;
  isAuthenticated: boolean;
  selectedBrokerId: string | null;
  searchQuery: string;
  selectedRegion: Region | 'all';
  availableRegions: Region[];
  brokersByRegion: Map<Region, StockBrokerMetadata[]>;
  brokerStatuses: Map<string, BrokerStatus>;
  onSelectBroker: (brokerId: string) => void;
  onSearchChange: (q: string) => void;
  onRegionChange: (r: Region | 'all') => void;
}

export function BrokerListPanel({
  activeBroker,
  isAuthenticated,
  selectedBrokerId,
  searchQuery,
  selectedRegion,
  availableRegions,
  brokersByRegion,
  brokerStatuses,
  onSelectBroker,
  onSearchChange,
  onRegionChange,
}: BrokerListPanelProps) {
  return (
    <div style={{ width: '320px', borderRight: `1px solid ${COLORS.BORDER}`, display: 'flex', flexDirection: 'column' }}>
      {/* Search & Filter */}
      <div style={{ padding: '12px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
        <div style={{ position: 'relative', marginBottom: '8px' }}>
          <Search size={14} color={COLORS.GRAY} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} />
          <input
            type="text"
            placeholder="Search brokers..."
            value={searchQuery}
            onChange={(e) => onSearchChange(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 8px 8px 32px',
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.WHITE,
              fontSize: '11px',
              outline: 'none',
              boxSizing: 'border-box',
            }}
          />
        </div>

        <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
          <button
            onClick={() => onRegionChange('all')}
            style={{
              padding: '4px 8px',
              fontSize: '9px',
              backgroundColor: selectedRegion === 'all' ? COLORS.ORANGE : COLORS.PANEL_BG,
              color: selectedRegion === 'all' ? COLORS.DARK_BG : COLORS.GRAY,
              border: `1px solid ${selectedRegion === 'all' ? COLORS.ORANGE : COLORS.BORDER}`,
              cursor: 'pointer',
              fontWeight: 600,
            }}
          >
            ALL
          </button>
          {availableRegions.map(region => (
            <button
              key={region}
              onClick={() => onRegionChange(region)}
              style={{
                padding: '4px 8px',
                fontSize: '9px',
                backgroundColor: selectedRegion === region ? COLORS.ORANGE : COLORS.PANEL_BG,
                color: selectedRegion === region ? COLORS.DARK_BG : COLORS.GRAY,
                border: `1px solid ${selectedRegion === region ? COLORS.ORANGE : COLORS.BORDER}`,
                cursor: 'pointer',
                fontWeight: 600,
              }}
            >
              {REGION_LABELS[region]?.toUpperCase() || region.toUpperCase()}
            </button>
          ))}
        </div>
      </div>

      {/* Broker List */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
        {Array.from(brokersByRegion.entries()).map(([region, brokers]) => (
          <div key={region} style={{ marginBottom: '16px' }}>
            <div style={{ padding: '4px 8px', color: COLORS.GRAY, fontSize: '9px', fontWeight: 600, letterSpacing: '1px' }}>
              {REGION_LABELS[region] || region}
            </div>
            {brokers.map(broker => {
              const status = brokerStatuses.get(broker.id);
              const isSelected = selectedBrokerId === broker.id;
              const isActive = activeBroker === broker.id && isAuthenticated;

              return (
                <div
                  key={broker.id}
                  onClick={() => onSelectBroker(broker.id)}
                  style={{
                    padding: '10px 12px',
                    backgroundColor: isSelected ? COLORS.HEADER_BG : 'transparent',
                    border: `1px solid ${isSelected ? COLORS.ORANGE : 'transparent'}`,
                    cursor: 'pointer',
                    marginBottom: '4px',
                    transition: 'all 0.15s ease',
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div>
                      <div style={{ color: COLORS.WHITE, fontSize: '11px', fontWeight: 600 }}>
                        {broker.displayName}
                        {isActive && (
                          <span style={{ marginLeft: '6px', padding: '2px 6px', backgroundColor: COLORS.GREEN, color: COLORS.DARK_BG, fontSize: '8px', fontWeight: 700 }}>
                            ACTIVE
                          </span>
                        )}
                      </div>
                      <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '2px' }}>
                        {broker.authType === 'oauth' ? 'OAuth' : broker.authType === 'totp' ? 'TOTP' : 'API Key'}
                      </div>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <StatusBadge status={status?.status || 'not_configured'} />
                      <ChevronRight size={14} color={COLORS.GRAY} />
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        ))}
      </div>
    </div>
  );
}
