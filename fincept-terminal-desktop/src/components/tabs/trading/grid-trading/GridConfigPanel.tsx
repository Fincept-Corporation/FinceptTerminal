/**
 * Grid Configuration Panel
 * Fincept Terminal Style - Inline styles
 */

import React, { useState, useMemo, useCallback } from 'react';
import {
  Grid, TrendingUp, TrendingDown, Minus, DollarSign,
  ArrowUpRight, ArrowDownRight, AlertCircle, Info, Calculator, CheckCircle,
} from 'lucide-react';
import type { GridConfig, GridType, GridDirection, GridCalculation } from '../../../../services/gridTrading/types';
import { calculateGridDetails, validateGridConfig } from '../../../../services/gridTrading';

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
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

interface GridConfigPanelProps {
  symbol: string;
  exchange?: string;
  currentPrice: number;
  brokerType: 'crypto' | 'stock';
  brokerId: string;
  availableBalance: number;
  onCreateGrid: (config: GridConfig) => void;
  onCancel?: () => void;
}

export function GridConfigPanel({
  symbol,
  exchange,
  currentPrice,
  brokerType,
  brokerId,
  availableBalance,
  onCreateGrid,
  onCancel,
}: GridConfigPanelProps) {
  const [upperPrice, setUpperPrice] = useState(currentPrice * 1.1);
  const [lowerPrice, setLowerPrice] = useState(currentPrice * 0.9);
  const [gridLevels, setGridLevels] = useState(10);
  const [gridType, setGridType] = useState<GridType>('arithmetic');
  const [totalInvestment, setTotalInvestment] = useState(1000);
  const [direction, setDirection] = useState<GridDirection>('neutral');
  const [stopLoss, setStopLoss] = useState<number | undefined>(undefined);
  const [takeProfit, setTakeProfit] = useState<number | undefined>(undefined);
  const [productType, setProductType] = useState('CASH');
  const [errors, setErrors] = useState<string[]>([]);

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: FINCEPT.DARK_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    color: FINCEPT.WHITE,
    fontSize: '11px',
    fontFamily: '"IBM Plex Mono", monospace',
    outline: 'none',
  };

  const selectStyle: React.CSSProperties = {
    ...inputStyle,
    cursor: 'pointer',
  };

  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    color: FINCEPT.GRAY,
    marginBottom: '4px',
    textTransform: 'uppercase',
    letterSpacing: '0.5px',
  };

  const gridCalculation = useMemo<GridCalculation | null>(() => {
    try {
      const config: Partial<GridConfig> = {
        symbol,
        exchange,
        upperPrice,
        lowerPrice,
        gridLevels,
        gridType,
        totalInvestment,
        direction,
        brokerType,
        brokerId,
      };

      const validationErrors = validateGridConfig(config);
      if (validationErrors.length > 0) {
        setErrors(validationErrors);
        return null;
      }

      setErrors([]);
      return calculateGridDetails(config as GridConfig);
    } catch {
      return null;
    }
  }, [symbol, exchange, upperPrice, lowerPrice, gridLevels, gridType, totalInvestment, direction, brokerType, brokerId]);

  const handleCreate = useCallback(() => {
    const config: GridConfig = {
      id: `grid_${Date.now()}`,
      symbol,
      exchange,
      upperPrice,
      lowerPrice,
      gridLevels,
      gridType,
      totalInvestment,
      quantityPerGrid: gridCalculation?.quantityPerLevel || 0,
      direction,
      stopLoss,
      takeProfit,
      brokerType,
      brokerId,
      productType: brokerType === 'stock' ? productType : undefined,
    };

    const validationErrors = validateGridConfig(config);
    if (validationErrors.length > 0) {
      setErrors(validationErrors);
      return;
    }

    onCreateGrid(config);
  }, [symbol, exchange, upperPrice, lowerPrice, gridLevels, gridType, totalInvestment, direction, stopLoss, takeProfit, brokerType, brokerId, productType, gridCalculation, onCreateGrid]);

  const applyPreset = (preset: 'conservative' | 'moderate' | 'aggressive') => {
    switch (preset) {
      case 'conservative':
        setUpperPrice(currentPrice * 1.05);
        setLowerPrice(currentPrice * 0.95);
        setGridLevels(5);
        break;
      case 'moderate':
        setUpperPrice(currentPrice * 1.10);
        setLowerPrice(currentPrice * 0.90);
        setGridLevels(10);
        break;
      case 'aggressive':
        setUpperPrice(currentPrice * 1.20);
        setLowerPrice(currentPrice * 0.80);
        setGridLevels(20);
        break;
    }
  };

  return (
    <div style={{ padding: '12px', fontFamily: '"IBM Plex Mono", monospace' }}>
      {/* Symbol & Price Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '10px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        marginBottom: '12px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Grid size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{symbol}</span>
        </div>
        <div style={{ fontSize: '12px', color: FINCEPT.CYAN, fontWeight: 600 }}>
          ${currentPrice.toFixed(2)}
        </div>
      </div>

      {/* Quick Presets */}
      <div style={{ marginBottom: '12px' }}>
        <div style={labelStyle}>QUICK PRESETS</div>
        <div style={{ display: 'flex', gap: '6px' }}>
          {[
            { key: 'conservative', label: 'CONSERVATIVE (5%)', color: FINCEPT.GREEN },
            { key: 'moderate', label: 'MODERATE (10%)', color: FINCEPT.ORANGE },
            { key: 'aggressive', label: 'AGGRESSIVE (20%)', color: FINCEPT.RED },
          ].map(preset => (
            <button
              key={preset.key}
              onClick={() => applyPreset(preset.key as any)}
              style={{
                flex: 1,
                padding: '6px 8px',
                fontSize: '9px',
                fontWeight: 700,
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${preset.color}40`,
                color: preset.color,
                cursor: 'pointer',
                fontFamily: '"IBM Plex Mono", monospace',
                transition: 'all 0.15s',
              }}
              onMouseEnter={e => {
                e.currentTarget.style.backgroundColor = `${preset.color}15`;
                e.currentTarget.style.borderColor = preset.color;
              }}
              onMouseLeave={e => {
                e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
                e.currentTarget.style.borderColor = `${preset.color}40`;
              }}
            >
              {preset.label}
            </button>
          ))}
        </div>
      </div>

      {/* Price Range */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '12px' }}>
        <div>
          <label style={labelStyle}>UPPER PRICE</label>
          <div style={{ position: 'relative' }}>
            <input
              type="text"
              inputMode="decimal"
              value={upperPrice}
              onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setUpperPrice(parseFloat(v) || 0); }}
              style={inputStyle}
            />
            <ArrowUpRight size={12} color={FINCEPT.GREEN} style={{ position: 'absolute', right: 10, top: '50%', transform: 'translateY(-50%)' }} />
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.GREEN, marginTop: 2 }}>
            +{(((upperPrice - currentPrice) / currentPrice) * 100).toFixed(1)}%
          </div>
        </div>
        <div>
          <label style={labelStyle}>LOWER PRICE</label>
          <div style={{ position: 'relative' }}>
            <input
              type="text"
              inputMode="decimal"
              value={lowerPrice}
              onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setLowerPrice(parseFloat(v) || 0); }}
              style={inputStyle}
            />
            <ArrowDownRight size={12} color={FINCEPT.RED} style={{ position: 'absolute', right: 10, top: '50%', transform: 'translateY(-50%)' }} />
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.RED, marginTop: 2 }}>
            {(((lowerPrice - currentPrice) / currentPrice) * 100).toFixed(1)}%
          </div>
        </div>
      </div>

      {/* Grid Settings */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '12px' }}>
        <div>
          <label style={labelStyle}>GRID LEVELS</label>
          <input
            type="text"
            inputMode="numeric"
            value={gridLevels}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setGridLevels(Math.min(parseInt(v) || 2, 200)); }}
            style={inputStyle}
          />
        </div>
        <div>
          <label style={labelStyle}>GRID TYPE</label>
          <select
            value={gridType}
            onChange={e => setGridType(e.target.value as GridType)}
            style={selectStyle}
          >
            <option value="arithmetic">ARITHMETIC</option>
            <option value="geometric">GEOMETRIC</option>
          </select>
        </div>
      </div>

      {/* Investment */}
      <div style={{ marginBottom: '12px' }}>
        <label style={labelStyle}>TOTAL INVESTMENT</label>
        <div style={{ position: 'relative' }}>
          <input
            type="text"
            inputMode="decimal"
            value={totalInvestment}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setTotalInvestment(parseFloat(v) || 0); }}
            style={inputStyle}
          />
          <DollarSign size={12} color={FINCEPT.GRAY} style={{ position: 'absolute', right: 10, top: '50%', transform: 'translateY(-50%)' }} />
        </div>
        <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: 2 }}>
          Available: ${availableBalance.toFixed(2)}
        </div>
      </div>

      {/* Direction */}
      <div style={{ marginBottom: '12px' }}>
        <label style={labelStyle}>TRADING DIRECTION</label>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '6px' }}>
          {[
            { key: 'long', label: 'LONG', icon: TrendingUp, color: FINCEPT.GREEN },
            { key: 'neutral', label: 'NEUTRAL', icon: Minus, color: FINCEPT.ORANGE },
            { key: 'short', label: 'SHORT', icon: TrendingDown, color: FINCEPT.RED },
          ].map(dir => {
            const Icon = dir.icon;
            const isActive = direction === dir.key;
            return (
              <button
                key={dir.key}
                onClick={() => setDirection(dir.key as GridDirection)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                  padding: '8px',
                  fontSize: '10px',
                  fontWeight: 700,
                  backgroundColor: isActive ? `${dir.color}20` : FINCEPT.HEADER_BG,
                  border: `1px solid ${isActive ? dir.color : FINCEPT.BORDER}`,
                  color: isActive ? dir.color : FINCEPT.GRAY,
                  cursor: 'pointer',
                  fontFamily: '"IBM Plex Mono", monospace',
                  transition: 'all 0.15s',
                }}
              >
                <Icon size={12} />
                {dir.label}
              </button>
            );
          })}
        </div>
      </div>

      {/* Stop Loss / Take Profit */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '12px' }}>
        <div>
          <label style={labelStyle}>STOP LOSS (OPTIONAL)</label>
          <input
            type="text"
            inputMode="decimal"
            value={stopLoss || ''}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setStopLoss(v ? parseFloat(v) : undefined); }}
            placeholder={`< ${lowerPrice.toFixed(2)}`}
            style={{ ...inputStyle, borderColor: stopLoss ? FINCEPT.RED : FINCEPT.BORDER }}
          />
        </div>
        <div>
          <label style={labelStyle}>TAKE PROFIT (OPTIONAL)</label>
          <input
            type="text"
            inputMode="decimal"
            value={takeProfit || ''}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setTakeProfit(v ? parseFloat(v) : undefined); }}
            placeholder={`> ${upperPrice.toFixed(2)}`}
            style={{ ...inputStyle, borderColor: takeProfit ? FINCEPT.GREEN : FINCEPT.BORDER }}
          />
        </div>
      </div>

      {/* Stock Product Type */}
      {brokerType === 'stock' && (
        <div style={{ marginBottom: '12px' }}>
          <label style={labelStyle}>PRODUCT TYPE</label>
          <select
            value={productType}
            onChange={e => setProductType(e.target.value)}
            style={selectStyle}
          >
            <option value="CASH">CASH (DELIVERY)</option>
            <option value="INTRADAY">INTRADAY (MIS)</option>
            <option value="MARGIN">MARGIN</option>
          </select>
        </div>
      )}

      {/* Grid Summary */}
      {gridCalculation && (
        <div style={{
          padding: '10px 12px',
          backgroundColor: `${FINCEPT.CYAN}10`,
          border: `1px solid ${FINCEPT.CYAN}30`,
          marginBottom: '12px',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: 8 }}>
            <Calculator size={12} color={FINCEPT.CYAN} />
            <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE }}>GRID SUMMARY</span>
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', fontSize: '10px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.GRAY }}>Grid Spacing:</span>
              <span style={{ color: FINCEPT.WHITE }}>
                {gridType === 'arithmetic' ? `$${gridCalculation.gridSpacing.toFixed(2)}` : `${gridCalculation.gridSpacing.toFixed(2)}%`}
              </span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.GRAY }}>Qty/Level:</span>
              <span style={{ color: FINCEPT.WHITE }}>{gridCalculation.quantityPerLevel.toFixed(4)}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.GRAY }}>Total Qty:</span>
              <span style={{ color: FINCEPT.WHITE }}>{gridCalculation.totalQuantity.toFixed(4)}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.GRAY }}>Break-even:</span>
              <span style={{ color: FINCEPT.WHITE }}>${gridCalculation.breakEvenPrice.toFixed(2)}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', gridColumn: 'span 2' }}>
              <span style={{ color: FINCEPT.GRAY }}>Potential Profit/Cycle:</span>
              <span style={{ color: FINCEPT.GREEN, fontWeight: 600 }}>${gridCalculation.potentialProfit.toFixed(2)}</span>
            </div>
          </div>
        </div>
      )}

      {/* Errors */}
      {errors.length > 0 && (
        <div style={{
          padding: '10px 12px',
          backgroundColor: `${FINCEPT.RED}15`,
          border: `1px solid ${FINCEPT.RED}30`,
          marginBottom: '12px',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: 4 }}>
            <AlertCircle size={12} color={FINCEPT.RED} />
            <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.RED }}>VALIDATION ERRORS</span>
          </div>
          <ul style={{ margin: 0, paddingLeft: 16, fontSize: '9px', color: FINCEPT.RED }}>
            {errors.map((error, i) => (
              <li key={i}>{error}</li>
            ))}
          </ul>
        </div>
      )}

      {/* Actions */}
      <div style={{ display: 'flex', gap: '8px' }}>
        {onCancel && (
          <button
            onClick={onCancel}
            style={{
              flex: 1,
              padding: '10px',
              fontSize: '11px',
              fontWeight: 700,
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              fontFamily: '"IBM Plex Mono", monospace',
              transition: 'all 0.15s',
            }}
            onMouseEnter={e => e.currentTarget.style.color = FINCEPT.WHITE}
            onMouseLeave={e => e.currentTarget.style.color = FINCEPT.GRAY}
          >
            CANCEL
          </button>
        )}
        <button
          onClick={handleCreate}
          disabled={errors.length > 0 || !gridCalculation}
          style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            padding: '10px',
            fontSize: '11px',
            fontWeight: 700,
            backgroundColor: errors.length > 0 || !gridCalculation ? FINCEPT.MUTED : FINCEPT.ORANGE,
            border: 'none',
            color: FINCEPT.DARK_BG,
            cursor: errors.length > 0 || !gridCalculation ? 'not-allowed' : 'pointer',
            fontFamily: '"IBM Plex Mono", monospace',
            transition: 'all 0.15s',
          }}
        >
          <CheckCircle size={12} />
          CREATE GRID
        </button>
      </div>

      {/* Info */}
      <div style={{
        display: 'flex',
        alignItems: 'flex-start',
        gap: '6px',
        marginTop: '12px',
        padding: '10px',
        backgroundColor: FINCEPT.HEADER_BG,
        fontSize: '9px',
        color: FINCEPT.MUTED,
        lineHeight: 1.4,
      }}>
        <Info size={12} style={{ flexShrink: 0, marginTop: 1 }} />
        <p style={{ margin: 0 }}>
          Grid trading places buy orders below and sell orders above the current price.
          When orders fill, opposite orders are placed to capture profits from price oscillations.
        </p>
      </div>
    </div>
  );
}

export default GridConfigPanel;
