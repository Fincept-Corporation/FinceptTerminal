/**
 * Grid Configuration Panel
 *
 * UI for configuring and creating new grid trading bots.
 * Works with both crypto and stock brokers.
 */

import React, { useState, useMemo, useCallback } from 'react';
import {
  Grid, TrendingUp, TrendingDown, Minus, DollarSign,
  ArrowUpRight, ArrowDownRight, AlertCircle, Info, Calculator,
  ChevronDown, Check,
} from 'lucide-react';
import type { GridConfig, GridType, GridDirection, GridCalculation } from '../../../../services/gridTrading/types';
import { calculateGridDetails, validateGridConfig } from '../../../../services/gridTrading';

// Fincept-style colors
const COLORS = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#0F0F0F',
  PANEL_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
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
  // Form state
  const [upperPrice, setUpperPrice] = useState(currentPrice * 1.1);
  const [lowerPrice, setLowerPrice] = useState(currentPrice * 0.9);
  const [gridLevels, setGridLevels] = useState(10);
  const [gridType, setGridType] = useState<GridType>('arithmetic');
  const [totalInvestment, setTotalInvestment] = useState(1000);
  const [direction, setDirection] = useState<GridDirection>('neutral');
  const [stopLoss, setStopLoss] = useState<number | undefined>(undefined);
  const [takeProfit, setTakeProfit] = useState<number | undefined>(undefined);
  const [productType, setProductType] = useState('CASH'); // For stocks

  // Validation
  const [errors, setErrors] = useState<string[]>([]);

  // Calculate grid details
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
    } catch (error) {
      return null;
    }
  }, [symbol, exchange, upperPrice, lowerPrice, gridLevels, gridType, totalInvestment, direction, brokerType, brokerId]);

  // Handle form submission
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

  // Quick presets
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
    <div className="bg-[#0F0F0F] border border-[#2A2A2A] rounded-lg p-4">
      {/* Header */}
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <Grid className="w-5 h-5 text-[#FF8800]" />
          <span className="text-white font-semibold">Grid Trading Configuration</span>
        </div>
        <div className="text-sm text-[#787878]">
          {symbol} @ {currentPrice.toFixed(2)}
        </div>
      </div>

      {/* Quick Presets */}
      <div className="mb-4">
        <div className="text-xs text-[#787878] mb-2">Quick Presets</div>
        <div className="flex gap-2">
          <button
            onClick={() => applyPreset('conservative')}
            className="px-3 py-1.5 text-xs bg-[#1A1A1A] hover:bg-[#2A2A2A] text-[#00D66F] border border-[#00D66F]/30 rounded transition-colors"
          >
            Conservative (5%)
          </button>
          <button
            onClick={() => applyPreset('moderate')}
            className="px-3 py-1.5 text-xs bg-[#1A1A1A] hover:bg-[#2A2A2A] text-[#FF8800] border border-[#FF8800]/30 rounded transition-colors"
          >
            Moderate (10%)
          </button>
          <button
            onClick={() => applyPreset('aggressive')}
            className="px-3 py-1.5 text-xs bg-[#1A1A1A] hover:bg-[#2A2A2A] text-[#FF3B3B] border border-[#FF3B3B]/30 rounded transition-colors"
          >
            Aggressive (20%)
          </button>
        </div>
      </div>

      {/* Price Range */}
      <div className="grid grid-cols-2 gap-4 mb-4">
        <div>
          <label className="block text-xs text-[#787878] mb-1">Upper Price</label>
          <div className="relative">
            <input
              type="text"
              inputMode="decimal"
              value={upperPrice}
              onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setUpperPrice(parseFloat(v) || 0); }}
              className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#FF8800] focus:outline-none"
            />
            <ArrowUpRight className="absolute right-3 top-1/2 -translate-y-1/2 w-4 h-4 text-[#00D66F]" />
          </div>
          <div className="text-xs text-[#787878] mt-1">
            +{(((upperPrice - currentPrice) / currentPrice) * 100).toFixed(1)}%
          </div>
        </div>
        <div>
          <label className="block text-xs text-[#787878] mb-1">Lower Price</label>
          <div className="relative">
            <input
              type="text"
              inputMode="decimal"
              value={lowerPrice}
              onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setLowerPrice(parseFloat(v) || 0); }}
              className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#FF8800] focus:outline-none"
            />
            <ArrowDownRight className="absolute right-3 top-1/2 -translate-y-1/2 w-4 h-4 text-[#FF3B3B]" />
          </div>
          <div className="text-xs text-[#787878] mt-1">
            {(((lowerPrice - currentPrice) / currentPrice) * 100).toFixed(1)}%
          </div>
        </div>
      </div>

      {/* Grid Settings */}
      <div className="grid grid-cols-2 gap-4 mb-4">
        <div>
          <label className="block text-xs text-[#787878] mb-1">Grid Levels</label>
          <input
            type="text"
            inputMode="numeric"
            value={gridLevels}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setGridLevels(Math.min(parseInt(v) || 2, 200)); }}
            className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#FF8800] focus:outline-none"
          />
        </div>
        <div>
          <label className="block text-xs text-[#787878] mb-1">Grid Type</label>
          <select
            value={gridType}
            onChange={e => setGridType(e.target.value as GridType)}
            className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#FF8800] focus:outline-none"
          >
            <option value="arithmetic">Arithmetic (Equal Price)</option>
            <option value="geometric">Geometric (Equal %)</option>
          </select>
        </div>
      </div>

      {/* Investment */}
      <div className="mb-4">
        <label className="block text-xs text-[#787878] mb-1">Total Investment</label>
        <div className="relative">
          <input
            type="text"
            inputMode="decimal"
            value={totalInvestment}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setTotalInvestment(parseFloat(v) || 0); }}
            className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#FF8800] focus:outline-none"
          />
          <DollarSign className="absolute right-3 top-1/2 -translate-y-1/2 w-4 h-4 text-[#787878]" />
        </div>
        <div className="text-xs text-[#787878] mt-1">
          Available: ${availableBalance.toFixed(2)}
        </div>
      </div>

      {/* Direction */}
      <div className="mb-4">
        <label className="block text-xs text-[#787878] mb-1">Trading Direction</label>
        <div className="grid grid-cols-3 gap-2">
          <button
            onClick={() => setDirection('long')}
            className={`flex items-center justify-center gap-1 px-3 py-2 rounded text-sm transition-colors ${
              direction === 'long'
                ? 'bg-[#00D66F]/20 border border-[#00D66F] text-[#00D66F]'
                : 'bg-[#1A1A1A] border border-[#2A2A2A] text-[#787878] hover:text-white'
            }`}
          >
            <TrendingUp className="w-4 h-4" />
            Long
          </button>
          <button
            onClick={() => setDirection('neutral')}
            className={`flex items-center justify-center gap-1 px-3 py-2 rounded text-sm transition-colors ${
              direction === 'neutral'
                ? 'bg-[#FF8800]/20 border border-[#FF8800] text-[#FF8800]'
                : 'bg-[#1A1A1A] border border-[#2A2A2A] text-[#787878] hover:text-white'
            }`}
          >
            <Minus className="w-4 h-4" />
            Neutral
          </button>
          <button
            onClick={() => setDirection('short')}
            className={`flex items-center justify-center gap-1 px-3 py-2 rounded text-sm transition-colors ${
              direction === 'short'
                ? 'bg-[#FF3B3B]/20 border border-[#FF3B3B] text-[#FF3B3B]'
                : 'bg-[#1A1A1A] border border-[#2A2A2A] text-[#787878] hover:text-white'
            }`}
          >
            <TrendingDown className="w-4 h-4" />
            Short
          </button>
        </div>
      </div>

      {/* Stop Loss / Take Profit */}
      <div className="grid grid-cols-2 gap-4 mb-4">
        <div>
          <label className="block text-xs text-[#787878] mb-1">Stop Loss (Optional)</label>
          <input
            type="text"
            inputMode="decimal"
            value={stopLoss || ''}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setStopLoss(v ? parseFloat(v) : undefined); }}
            placeholder={`< ${lowerPrice.toFixed(2)}`}
            className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#FF3B3B] focus:outline-none placeholder-[#4A4A4A]"
          />
        </div>
        <div>
          <label className="block text-xs text-[#787878] mb-1">Take Profit (Optional)</label>
          <input
            type="text"
            inputMode="decimal"
            value={takeProfit || ''}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setTakeProfit(v ? parseFloat(v) : undefined); }}
            placeholder={`> ${upperPrice.toFixed(2)}`}
            className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#00D66F] focus:outline-none placeholder-[#4A4A4A]"
          />
        </div>
      </div>

      {/* Stock-specific: Product Type */}
      {brokerType === 'stock' && (
        <div className="mb-4">
          <label className="block text-xs text-[#787878] mb-1">Product Type</label>
          <select
            value={productType}
            onChange={e => setProductType(e.target.value)}
            className="w-full bg-[#1A1A1A] border border-[#2A2A2A] rounded px-3 py-2 text-white text-sm focus:border-[#FF8800] focus:outline-none"
          >
            <option value="CASH">Cash (Delivery)</option>
            <option value="INTRADAY">Intraday (MIS)</option>
            <option value="MARGIN">Margin</option>
          </select>
        </div>
      )}

      {/* Grid Calculation Summary */}
      {gridCalculation && (
        <div className="bg-[#1A1A1A] rounded p-3 mb-4">
          <div className="flex items-center gap-2 mb-2">
            <Calculator className="w-4 h-4 text-[#00E5FF]" />
            <span className="text-sm text-white">Grid Summary</span>
          </div>
          <div className="grid grid-cols-2 gap-2 text-xs">
            <div className="flex justify-between">
              <span className="text-[#787878]">Grid Spacing:</span>
              <span className="text-white">
                {gridType === 'arithmetic'
                  ? `$${gridCalculation.gridSpacing.toFixed(2)}`
                  : `${gridCalculation.gridSpacing.toFixed(2)}%`}
              </span>
            </div>
            <div className="flex justify-between">
              <span className="text-[#787878]">Qty/Level:</span>
              <span className="text-white">{gridCalculation.quantityPerLevel.toFixed(4)}</span>
            </div>
            <div className="flex justify-between">
              <span className="text-[#787878]">Total Qty:</span>
              <span className="text-white">{gridCalculation.totalQuantity.toFixed(4)}</span>
            </div>
            <div className="flex justify-between">
              <span className="text-[#787878]">Break-even:</span>
              <span className="text-white">${gridCalculation.breakEvenPrice.toFixed(2)}</span>
            </div>
            <div className="flex justify-between col-span-2">
              <span className="text-[#787878]">Potential Profit/Cycle:</span>
              <span className="text-[#00D66F]">${gridCalculation.potentialProfit.toFixed(2)}</span>
            </div>
          </div>
        </div>
      )}

      {/* Errors */}
      {errors.length > 0 && (
        <div className="bg-[#FF3B3B]/10 border border-[#FF3B3B]/30 rounded p-3 mb-4">
          <div className="flex items-center gap-2 text-[#FF3B3B] text-sm mb-1">
            <AlertCircle className="w-4 h-4" />
            Validation Errors
          </div>
          <ul className="text-xs text-[#FF3B3B] list-disc list-inside">
            {errors.map((error, i) => (
              <li key={i}>{error}</li>
            ))}
          </ul>
        </div>
      )}

      {/* Actions */}
      <div className="flex gap-2">
        {onCancel && (
          <button
            onClick={onCancel}
            className="flex-1 py-2 bg-[#1A1A1A] hover:bg-[#2A2A2A] text-[#787878] hover:text-white rounded transition-colors text-sm"
          >
            Cancel
          </button>
        )}
        <button
          onClick={handleCreate}
          disabled={errors.length > 0 || !gridCalculation}
          className="flex-1 py-2 bg-[#FF8800] hover:bg-[#FF9900] disabled:bg-[#4A4A4A] disabled:cursor-not-allowed text-black font-semibold rounded transition-colors text-sm flex items-center justify-center gap-2"
        >
          <Grid className="w-4 h-4" />
          Create Grid
        </button>
      </div>

      {/* Info */}
      <div className="mt-4 flex items-start gap-2 text-xs text-[#787878]">
        <Info className="w-4 h-4 flex-shrink-0 mt-0.5" />
        <p>
          Grid trading places buy orders below and sell orders above the current price.
          When orders fill, opposite orders are placed to capture profits from price oscillations.
        </p>
      </div>
    </div>
  );
}

export default GridConfigPanel;
