/**
 * Stock Broker Selector Component - Bloomberg Style
 *
 * Dropdown for selecting stock brokers with region grouping
 */

import React, { useState, useRef, useEffect } from 'react';
import { ChevronDown, Check, Globe, Building2 } from 'lucide-react';
import { useStockBrokerSelection } from '@/contexts/StockBrokerContext';
import type { Region, StockBrokerMetadata } from '@/brokers/stocks/types';

// Bloomberg color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  HOVER_BG: '#252525',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

const REGION_LABELS: Record<Region, string> = {
  india: 'INDIA',
  us: 'UNITED STATES',
  europe: 'EUROPE',
  uk: 'UNITED KINGDOM',
  asia: 'ASIA PACIFIC',
  australia: 'AUSTRALIA',
  canada: 'CANADA',
};

const REGION_FLAGS: Record<Region, string> = {
  india: '🇮🇳',
  us: '🇺🇸',
  europe: '🇪🇺',
  uk: '🇬🇧',
  asia: '🌏',
  australia: '🇦🇺',
  canada: '🇨🇦',
};

interface BrokerSelectorProps {
  className?: string;
}

export function BrokerSelector({ className = '' }: BrokerSelectorProps) {
  const { broker, metadata, brokers, setBroker, isLoading } = useStockBrokerSelection();
  const [isOpen, setIsOpen] = useState(false);
  const [hoveredBroker, setHoveredBroker] = useState<string | null>(null);
  const dropdownRef = useRef<HTMLDivElement>(null);

  // Group brokers by region
  const brokersByRegion = React.useMemo(() => {
    const grouped: Record<Region, StockBrokerMetadata[]> = {
      india: [],
      us: [],
      europe: [],
      uk: [],
      asia: [],
      australia: [],
      canada: [],
    };

    brokers.forEach((b) => {
      if (grouped[b.region]) {
        grouped[b.region].push(b);
      }
    });

    return grouped;
  }, [brokers]);

  // Close dropdown on outside click
  useEffect(() => {
    function handleClickOutside(event: MouseEvent) {
      if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
        setIsOpen(false);
      }
    }

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  const handleSelect = async (brokerId: string) => {
    try {
      await setBroker(brokerId);
      setIsOpen(false);
    } catch (err) {
      console.error('Failed to set broker:', err);
    }
  };

  return (
    <div style={{ position: 'relative' }} ref={dropdownRef}>
      <button
        onClick={() => setIsOpen(!isOpen)}
        disabled={isLoading}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '5px 12px',
          backgroundColor: COLORS.HEADER_BG,
          border: `1px solid ${isOpen ? COLORS.ORANGE : COLORS.BORDER}`,
          color: COLORS.WHITE,
          cursor: isLoading ? 'not-allowed' : 'pointer',
          minWidth: '180px',
          opacity: isLoading ? 0.6 : 1,
          transition: 'all 0.15s ease'
        }}
        onMouseEnter={(e) => {
          if (!isOpen) e.currentTarget.style.borderColor = COLORS.ORANGE;
        }}
        onMouseLeave={(e) => {
          if (!isOpen) e.currentTarget.style.borderColor = COLORS.BORDER;
        }}
      >
        <Building2 size={14} color={COLORS.ORANGE} />
        <span style={{
          flex: 1,
          textAlign: 'left',
          fontSize: '11px',
          fontWeight: 600,
          fontFamily: '"IBM Plex Mono", monospace',
          color: metadata ? COLORS.WHITE : COLORS.GRAY,
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap'
        }}>
          {metadata ? metadata.displayName.toUpperCase() : 'SELECT BROKER'}
        </span>
        {metadata && (
          <span style={{ fontSize: '12px' }}>
            {REGION_FLAGS[metadata.region]}
          </span>
        )}
        <ChevronDown
          size={12}
          color={COLORS.GRAY}
          style={{
            transition: 'transform 0.15s ease',
            transform: isOpen ? 'rotate(180deg)' : 'rotate(0deg)'
          }}
        />
      </button>

      {isOpen && (
        <div style={{
          position: 'absolute',
          top: '100%',
          left: 0,
          marginTop: '4px',
          width: '280px',
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          boxShadow: `0 4px 16px ${COLORS.DARK_BG}80`,
          zIndex: 1000,
          maxHeight: '400px',
          overflowY: 'auto'
        }}>
          {Object.entries(brokersByRegion).map(([region, regionBrokers]) => {
            if (regionBrokers.length === 0) return null;

            return (
              <div key={region}>
                {/* Region Header */}
                <div style={{
                  padding: '8px 12px',
                  backgroundColor: COLORS.DARK_BG,
                  borderBottom: `1px solid ${COLORS.BORDER}`,
                  position: 'sticky',
                  top: 0,
                  zIndex: 1
                }}>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: COLORS.GRAY,
                    letterSpacing: '0.5px'
                  }}>
                    <Globe size={10} color={COLORS.GRAY} />
                    <span>{REGION_FLAGS[region as Region]}</span>
                    <span>{REGION_LABELS[region as Region]}</span>
                  </div>
                </div>

                {/* Brokers in this region */}
                {regionBrokers.map((b) => {
                  const isSelected = broker === b.id;
                  const isHovered = hoveredBroker === b.id;

                  return (
                    <button
                      key={b.id}
                      onClick={() => handleSelect(b.id)}
                      onMouseEnter={() => setHoveredBroker(b.id)}
                      onMouseLeave={() => setHoveredBroker(null)}
                      style={{
                        width: '100%',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '10px',
                        padding: '10px 12px',
                        backgroundColor: isSelected ? `${COLORS.ORANGE}15` : isHovered ? COLORS.HOVER_BG : 'transparent',
                        border: 'none',
                        borderLeft: isSelected ? `3px solid ${COLORS.ORANGE}` : '3px solid transparent',
                        cursor: 'pointer',
                        transition: 'all 0.1s ease'
                      }}
                    >
                      {/* Logo placeholder */}
                      <div style={{
                        width: '28px',
                        height: '28px',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        backgroundColor: isSelected ? `${COLORS.ORANGE}30` : COLORS.HEADER_BG,
                        border: `1px solid ${isSelected ? COLORS.ORANGE : COLORS.BORDER}`,
                        fontSize: '10px',
                        fontWeight: 700,
                        color: isSelected ? COLORS.ORANGE : COLORS.GRAY,
                        flexShrink: 0
                      }}>
                        {b.name.substring(0, 2).toUpperCase()}
                      </div>

                      {/* Broker info */}
                      <div style={{ flex: 1, textAlign: 'left', overflow: 'hidden' }}>
                        <div style={{
                          fontSize: '11px',
                          fontWeight: 600,
                          color: isSelected ? COLORS.ORANGE : COLORS.WHITE,
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis'
                        }}>
                          {b.displayName}
                        </div>
                        <div style={{
                          fontSize: '9px',
                          color: COLORS.MUTED,
                          marginTop: '2px',
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis'
                        }}>
                          {b.exchanges.slice(0, 3).join(' • ')}
                          {b.exchanges.length > 3 && ` +${b.exchanges.length - 3}`}
                        </div>
                      </div>

                      {/* Selected check */}
                      {isSelected && (
                        <Check size={14} color={COLORS.GREEN} style={{ flexShrink: 0 }} />
                      )}
                    </button>
                  );
                })}
              </div>
            );
          })}

          {brokers.length === 0 && (
            <div style={{
              padding: '24px 16px',
              textAlign: 'center',
              color: COLORS.GRAY,
              fontSize: '11px'
            }}>
              No brokers available
            </div>
          )}
        </div>
      )}
    </div>
  );
}
