/**
 * Leverage Panel
 * Control leverage settings, margin mode, and position mode
 */

import React, { useState, useEffect } from 'react';
import { Settings, AlertTriangle, Check, Loader2, Info, TrendingUp, Shield } from 'lucide-react';
import { useFetchLeverage, useSetLeverage, useLeverageTiers, useSetMarginMode, usePositionMode, useSetPositionMode } from '../../hooks/useLeverageManagement';
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
  PURPLE: '#9D4EDD',
};

interface LeveragePanelProps {
  symbol?: string;
}

export function LeveragePanel({ symbol }: LeveragePanelProps) {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { leverage, isLoading: fetchingLeverage, isSupported: leverageSupported, refresh } = useFetchLeverage(symbol);
  const { setLeverage, isLoading: settingLeverage, error: setError, isSupported: setSupported } = useSetLeverage();
  const { tiers, isLoading: fetchingTiers, isSupported: tiersSupported } = useLeverageTiers(symbol);
  const { setMarginMode, isLoading: settingMargin, isSupported: marginSupported } = useSetMarginMode();
  const { positionMode, isSupported: positionModeSupported } = usePositionMode(symbol);
  const { setPositionMode, isLoading: settingPositionMode, isSupported: setPositionSupported } = useSetPositionMode();

  const [selectedLeverage, setSelectedLeverage] = useState(1);
  const [selectedMarginMode, setSelectedMarginMode] = useState<'cross' | 'isolated'>('cross');
  const [selectedPositionMode, setSelectedPositionMode] = useState<'one-way' | 'hedge'>('one-way');
  const [success, setSuccess] = useState<string | null>(null);

  // Sync with fetched values
  useEffect(() => {
    if (leverage) {
      setSelectedLeverage(leverage.leverage);
      if (leverage.marginMode) {
        setSelectedMarginMode(leverage.marginMode);
      }
    }
  }, [leverage]);

  useEffect(() => {
    if (positionMode) {
      setSelectedPositionMode(positionMode);
    }
  }, [positionMode]);

  // Calculate max leverage from tiers
  const maxLeverage = tiers.length > 0 ? Math.max(...tiers.map((t) => t.maxLeverage)) : leverage?.maxLeverage || 125;

  // Handle leverage change
  const handleLeverageChange = async () => {
    if (!symbol) return;
    const result = await setLeverage(selectedLeverage, symbol);
    if (result) {
      setSuccess('Leverage updated successfully');
      setTimeout(() => setSuccess(null), 3000);
      refresh();
    }
  };

  // Handle margin mode change
  const handleMarginModeChange = async (mode: 'cross' | 'isolated') => {
    if (!symbol) return;
    setSelectedMarginMode(mode);
    const result = await setMarginMode(mode, symbol);
    if (result) {
      setSuccess('Margin mode updated');
      setTimeout(() => setSuccess(null), 3000);
    }
  };

  // Handle position mode change
  const handlePositionModeChange = async (mode: 'one-way' | 'hedge') => {
    if (!symbol) return;
    setSelectedPositionMode(mode);
    const result = await setPositionMode(mode === 'hedge', symbol);
    if (result) {
      setSuccess('Position mode updated');
      setTimeout(() => setSuccess(null), 3000);
    }
  };

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertTriangle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Leverage settings not available in paper trading</div>
      </div>
    );
  }

  if (!leverageSupported) {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <Info size={32} color={FINCEPT.GRAY} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Leverage not supported by {activeBroker}</div>
      </div>
    );
  }

  const isLoading = fetchingLeverage || settingLeverage || settingMargin || settingPositionMode;

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '16px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px', paddingBottom: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Settings size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>Leverage Settings</span>
        </div>
        {symbol && (
          <span style={{ fontSize: '10px', color: FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>{symbol}</span>
        )}
      </div>

      {/* Success Message */}
      {success && (
        <div style={{ padding: '10px', backgroundColor: `${FINCEPT.GREEN}20`, borderRadius: '4px', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Check size={14} color={FINCEPT.GREEN} />
          <span style={{ fontSize: '11px', color: FINCEPT.GREEN }}>{success}</span>
        </div>
      )}

      {/* Error Message */}
      {setError && (
        <div style={{ padding: '10px', backgroundColor: `${FINCEPT.RED}20`, borderRadius: '4px', marginBottom: '12px', fontSize: '11px', color: FINCEPT.RED }}>
          {setError.message}
        </div>
      )}

      {/* Leverage Slider */}
      <div style={{ marginBottom: '20px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
          <label style={{ fontSize: '10px', color: FINCEPT.GRAY }}>LEVERAGE</label>
          <span style={{ fontSize: '14px', color: FINCEPT.ORANGE, fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace' }}>
            {selectedLeverage}x
          </span>
        </div>

        <input
          type="range"
          min={1}
          max={maxLeverage}
          value={selectedLeverage}
          onChange={(e) => setSelectedLeverage(parseInt(e.target.value))}
          style={{
            width: '100%',
            height: '6px',
            borderRadius: '3px',
            appearance: 'none',
            background: `linear-gradient(to right, ${FINCEPT.ORANGE} 0%, ${FINCEPT.ORANGE} ${(selectedLeverage / maxLeverage) * 100}%, ${FINCEPT.BORDER} ${(selectedLeverage / maxLeverage) * 100}%, ${FINCEPT.BORDER} 100%)`,
            cursor: 'pointer',
          }}
        />

        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '4px' }}>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>1x</span>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{maxLeverage}x</span>
        </div>

        {/* Quick Select Buttons */}
        <div style={{ display: 'flex', gap: '6px', marginTop: '12px', flexWrap: 'wrap' }}>
          {[1, 5, 10, 20, 50, 100].filter((v) => v <= maxLeverage).map((val) => (
            <button
              key={val}
              onClick={() => setSelectedLeverage(val)}
              style={{
                padding: '6px 12px',
                backgroundColor: selectedLeverage === val ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                border: `1px solid ${selectedLeverage === val ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: selectedLeverage === val ? FINCEPT.WHITE : FINCEPT.GRAY,
                fontSize: '10px',
                fontWeight: 600,
                cursor: 'pointer',
              }}
            >
              {val}x
            </button>
          ))}
        </div>

        <button
          onClick={handleLeverageChange}
          disabled={isLoading || selectedLeverage === leverage?.leverage}
          style={{
            width: '100%',
            marginTop: '12px',
            padding: '10px',
            backgroundColor: selectedLeverage !== leverage?.leverage ? FINCEPT.ORANGE : FINCEPT.BORDER,
            border: 'none',
            borderRadius: '4px',
            color: selectedLeverage !== leverage?.leverage ? FINCEPT.WHITE : FINCEPT.GRAY,
            fontSize: '11px',
            fontWeight: 700,
            cursor: selectedLeverage !== leverage?.leverage ? 'pointer' : 'not-allowed',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
          }}
        >
          {settingLeverage ? (
            <>
              <Loader2 size={12} style={{ animation: 'spin 1s linear infinite' }} />
              UPDATING...
            </>
          ) : (
            <>
              <TrendingUp size={12} />
              APPLY LEVERAGE
            </>
          )}
        </button>
      </div>

      {/* Margin Mode */}
      {marginSupported && (
        <div style={{ marginBottom: '20px' }}>
          <label style={{ display: 'block', fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>MARGIN MODE</label>
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => handleMarginModeChange('cross')}
              disabled={settingMargin}
              style={{
                flex: 1,
                padding: '10px',
                backgroundColor: selectedMarginMode === 'cross' ? `${FINCEPT.CYAN}20` : FINCEPT.DARK_BG,
                border: `1px solid ${selectedMarginMode === 'cross' ? FINCEPT.CYAN : FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: selectedMarginMode === 'cross' ? FINCEPT.CYAN : FINCEPT.GRAY,
                fontSize: '11px',
                fontWeight: 600,
                cursor: 'pointer',
              }}
            >
              <div>Cross</div>
              <div style={{ fontSize: '8px', marginTop: '2px', opacity: 0.7 }}>Shared margin</div>
            </button>
            <button
              onClick={() => handleMarginModeChange('isolated')}
              disabled={settingMargin}
              style={{
                flex: 1,
                padding: '10px',
                backgroundColor: selectedMarginMode === 'isolated' ? `${FINCEPT.PURPLE}20` : FINCEPT.DARK_BG,
                border: `1px solid ${selectedMarginMode === 'isolated' ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: selectedMarginMode === 'isolated' ? FINCEPT.PURPLE : FINCEPT.GRAY,
                fontSize: '11px',
                fontWeight: 600,
                cursor: 'pointer',
              }}
            >
              <div>Isolated</div>
              <div style={{ fontSize: '8px', marginTop: '2px', opacity: 0.7 }}>Per-position</div>
            </button>
          </div>
        </div>
      )}

      {/* Position Mode */}
      {positionModeSupported && setPositionSupported && (
        <div style={{ marginBottom: '16px' }}>
          <label style={{ display: 'block', fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>POSITION MODE</label>
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => handlePositionModeChange('one-way')}
              disabled={settingPositionMode}
              style={{
                flex: 1,
                padding: '10px',
                backgroundColor: selectedPositionMode === 'one-way' ? `${FINCEPT.GREEN}20` : FINCEPT.DARK_BG,
                border: `1px solid ${selectedPositionMode === 'one-way' ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: selectedPositionMode === 'one-way' ? FINCEPT.GREEN : FINCEPT.GRAY,
                fontSize: '11px',
                fontWeight: 600,
                cursor: 'pointer',
              }}
            >
              <div>One-way</div>
              <div style={{ fontSize: '8px', marginTop: '2px', opacity: 0.7 }}>Single direction</div>
            </button>
            <button
              onClick={() => handlePositionModeChange('hedge')}
              disabled={settingPositionMode}
              style={{
                flex: 1,
                padding: '10px',
                backgroundColor: selectedPositionMode === 'hedge' ? `${FINCEPT.YELLOW}20` : FINCEPT.DARK_BG,
                border: `1px solid ${selectedPositionMode === 'hedge' ? FINCEPT.YELLOW : FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: selectedPositionMode === 'hedge' ? FINCEPT.YELLOW : FINCEPT.GRAY,
                fontSize: '11px',
                fontWeight: 600,
                cursor: 'pointer',
              }}
            >
              <div>Hedge</div>
              <div style={{ fontSize: '8px', marginTop: '2px', opacity: 0.7 }}>Long & Short</div>
            </button>
          </div>
        </div>
      )}

      {/* Leverage Tiers Info */}
      {tiersSupported && tiers.length > 0 && (
        <div style={{ backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', padding: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '8px' }}>
            <Shield size={12} color={FINCEPT.CYAN} />
            <span style={{ fontSize: '10px', color: FINCEPT.CYAN, fontWeight: 600 }}>LEVERAGE TIERS</span>
          </div>
          <div style={{ maxHeight: '120px', overflowY: 'auto' }}>
            {tiers.slice(0, 5).map((tier) => (
              <div
                key={tier.tier}
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '6px 0',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '9px',
                }}
              >
                <span style={{ color: FINCEPT.GRAY }}>
                  {tier.minNotional.toLocaleString()} - {tier.maxNotional === Infinity ? '...' : tier.maxNotional.toLocaleString()}
                </span>
                <span style={{ color: FINCEPT.WHITE }}>Max {tier.maxLeverage}x</span>
                <span style={{ color: FINCEPT.ORANGE }}>{(tier.maintenanceMarginRate * 100).toFixed(2)}% MMR</span>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Warning */}
      <div style={{ marginTop: '12px', padding: '10px', backgroundColor: `${FINCEPT.YELLOW}10`, borderRadius: '4px', display: 'flex', gap: '8px', alignItems: 'flex-start' }}>
        <AlertTriangle size={14} color={FINCEPT.YELLOW} style={{ flexShrink: 0, marginTop: '2px' }} />
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
          <strong style={{ color: FINCEPT.YELLOW }}>Risk Warning:</strong> Higher leverage increases both potential profits and losses.
          You may lose more than your initial margin.
        </div>
      </div>

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
        input[type="range"]::-webkit-slider-thumb {
          appearance: none;
          width: 16px;
          height: 16px;
          border-radius: 50%;
          background: ${FINCEPT.ORANGE};
          cursor: pointer;
          border: 2px solid ${FINCEPT.WHITE};
        }
      `}</style>
    </div>
  );
}
