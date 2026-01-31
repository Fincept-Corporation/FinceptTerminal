/**
 * CreateIndexModal - Create/Edit Custom Index Modal
 * Allows users to create a new index or edit existing one
 */

import React, { useState, useEffect } from 'react';
import {
  X,
  Plus,
  Trash2,
  Info,
  Search,
  Check,
  AlertCircle,
} from 'lucide-react';
import {
  FINCEPT,
  TYPOGRAPHY,
  SPACING,
  BORDERS,
  COMMON_STYLES,
  EFFECTS,
  createFocusHandlers,
} from '../finceptStyles';
import {
  IndexCalculationMethod,
  IndexConstituentConfig,
  IndexFormState,
  INDEX_METHOD_INFO,
  DEFAULT_INDEX_FORM,
} from './types';
import { customIndexService } from '../../../../services/portfolio/customIndexService';
import { PortfolioSummary, HoldingWithQuote } from '../../../../services/portfolio/portfolioService';

interface CreateIndexModalProps {
  isOpen: boolean;
  onClose: () => void;
  onCreated: () => void;
  portfolioSummary?: PortfolioSummary | null;
  preSelectedSymbols?: string[];
}

const CreateIndexModal: React.FC<CreateIndexModalProps> = ({
  isOpen,
  onClose,
  onCreated,
  portfolioSummary,
  preSelectedSymbols = [],
}) => {
  const [form, setForm] = useState<IndexFormState>({
    ...DEFAULT_INDEX_FORM,
    constituents: [],
  });
  const [symbolInput, setSymbolInput] = useState('');
  const [selectedMethod, setSelectedMethod] = useState<IndexCalculationMethod>('equal_weighted');
  const [showMethodHelp, setShowMethodHelp] = useState(false);
  const [showHistoryHelp, setShowHistoryHelp] = useState(false);
  const [creating, setCreating] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [usePortfolioStocks, setUsePortfolioStocks] = useState(false);
  const [selectedPortfolioStocks, setSelectedPortfolioStocks] = useState<Set<string>>(new Set());

  // Initialize with pre-selected symbols
  useEffect(() => {
    if (isOpen && preSelectedSymbols.length > 0) {
      const constituents = preSelectedSymbols.map(symbol => ({
        symbol: symbol.toUpperCase(),
        shares: 1,
        weight: undefined,
      }));
      setForm(prev => ({ ...prev, constituents }));
    }
  }, [isOpen, preSelectedSymbols]);

  // Initialize portfolio stock selection
  useEffect(() => {
    if (portfolioSummary?.holdings) {
      const stockSet = new Set(
        preSelectedSymbols.length > 0
          ? preSelectedSymbols.map(s => s.toUpperCase())
          : portfolioSummary.holdings.map(h => h.symbol)
      );
      setSelectedPortfolioStocks(stockSet);
    }
  }, [portfolioSummary, preSelectedSymbols]);

  if (!isOpen) return null;

  const focusHandlers = createFocusHandlers();

  const handleMethodChange = (method: IndexCalculationMethod) => {
    setSelectedMethod(method);
    setForm(prev => ({ ...prev, calculation_method: method }));
  };

  const handleAddSymbol = () => {
    const symbol = symbolInput.trim().toUpperCase();
    if (!symbol) return;

    // Check if already exists
    if (form.constituents.some(c => c.symbol === symbol)) {
      setError(`${symbol} is already in the list`);
      return;
    }

    // Add new constituent and recalculate equal weights
    const newConstituents = [
      ...form.constituents,
      { symbol, shares: 1, weight: 0 },
    ];

    const n = newConstituents.length;
    const baseWeight = Math.floor((100 / n) * 100) / 100;
    const remainder = 100 - (baseWeight * n);

    // Distribute weights with first getting remainder
    const weightedConstituents = newConstituents.map((c, idx) => ({
      ...c,
      weight: idx === 0 ? Math.round((baseWeight + remainder) * 100) / 100 : baseWeight,
    }));

    setForm(prev => ({
      ...prev,
      constituents: weightedConstituents,
    }));
    setSymbolInput('');
    setError(null);
  };

  const handleRemoveSymbol = (symbol: string) => {
    const remaining = form.constituents.filter(c => c.symbol !== symbol);

    // Recalculate equal weights for remaining constituents
    if (remaining.length > 0) {
      const n = remaining.length;
      const baseWeight = Math.floor((100 / n) * 100) / 100;
      const remainder = 100 - (baseWeight * n);

      const updatedConstituents = remaining.map((c, idx) => ({
        ...c,
        weight: idx === 0 ? Math.round((baseWeight + remainder) * 100) / 100 : baseWeight,
      }));

      setForm(prev => ({
        ...prev,
        constituents: updatedConstituents,
      }));
    } else {
      setForm(prev => ({
        ...prev,
        constituents: [],
      }));
    }
  };

  const handleConstituentChange = (
    symbol: string,
    field: 'shares' | 'weight',
    value: number
  ) => {
    setForm(prev => ({
      ...prev,
      constituents: prev.constituents.map(c =>
        c.symbol === symbol ? { ...c, [field]: value } : c
      ),
    }));
  };

  // Normalize weights to sum to 100%
  const handleNormalizeWeights = () => {
    const totalWeight = form.constituents.reduce((sum, c) => sum + (c.weight || 0), 0);

    if (totalWeight === 0) {
      // If all weights are 0, set equal weights with precise distribution
      handleEqualWeights();
    } else {
      // Normalize existing weights to 100% with precision handling
      const normalized = form.constituents.map((c, idx) => {
        const normalizedWeight = ((c.weight || 0) / totalWeight) * 100;
        return {
          ...c,
          // Round to 2 decimal places
          weight: Math.round(normalizedWeight * 100) / 100,
        };
      });

      // Calculate the difference due to rounding
      const roundedSum = normalized.reduce((sum, c) => sum + (c.weight || 0), 0);
      const diff = 100 - roundedSum;

      // Add the difference to the largest weight to ensure exact 100%
      if (diff !== 0 && normalized.length > 0) {
        // Find the constituent with the largest weight
        let maxIndex = 0;
        let maxWeight = normalized[0].weight || 0;
        for (let i = 1; i < normalized.length; i++) {
          if ((normalized[i].weight || 0) > maxWeight) {
            maxWeight = normalized[i].weight || 0;
            maxIndex = i;
          }
        }
        // Adjust the largest weight
        normalized[maxIndex].weight = Math.round((maxWeight + diff) * 100) / 100;
      }

      setForm(prev => ({
        ...prev,
        constituents: normalized,
      }));
    }
  };

  // Reset to equal weights with precise distribution
  const handleEqualWeights = () => {
    const n = form.constituents.length;
    if (n === 0) return;

    // Calculate base weight and remainder
    const baseWeight = Math.floor((100 / n) * 100) / 100; // Round down to 2 decimals
    const remainder = 100 - (baseWeight * n);

    // Distribute weights: first constituent gets base + remainder, others get base
    const weights = form.constituents.map((c, idx) => {
      if (idx === 0) {
        // First constituent gets the remainder to ensure exact 100%
        return {
          ...c,
          weight: Math.round((baseWeight + remainder) * 100) / 100,
        };
      } else {
        return {
          ...c,
          weight: baseWeight,
        };
      }
    });

    setForm(prev => ({
      ...prev,
      constituents: weights,
    }));
  };

  const togglePortfolioStock = (symbol: string) => {
    setSelectedPortfolioStocks(prev => {
      const newSet = new Set(prev);
      if (newSet.has(symbol)) {
        newSet.delete(symbol);
      } else {
        newSet.add(symbol);
      }
      return newSet;
    });
  };

  const handleUsePortfolioStocks = () => {
    if (!portfolioSummary?.holdings) return;

    const selectedHoldings = portfolioSummary.holdings.filter(h =>
      selectedPortfolioStocks.has(h.symbol)
    );

    // Calculate equal weights for selected stocks with precise distribution
    const n = selectedHoldings.length;
    if (n === 0) return;

    const baseWeight = Math.floor((100 / n) * 100) / 100;
    const remainder = 100 - (baseWeight * n);

    const constituents = selectedHoldings.map((h, idx) => ({
      symbol: h.symbol,
      shares: h.quantity,
      weight: idx === 0 ? Math.round((baseWeight + remainder) * 100) / 100 : baseWeight,
    }));

    setForm(prev => ({ ...prev, constituents }));
    setUsePortfolioStocks(false);
  };

  const handleCreate = async () => {
    // Validation
    if (!form.name.trim()) {
      setError('Index name is required');
      return;
    }
    if (form.constituents.length === 0) {
      setError('At least one constituent is required');
      return;
    }

    setCreating(true);
    setError(null);

    try {
      console.log('[CreateIndexModal] Creating index with data:', {
        name: form.name,
        description: form.description,
        method: form.calculation_method,
        baseValue: form.base_value,
        capWeight: form.cap_weight,
        currency: form.currency,
        portfolioId: portfolioSummary?.portfolio.id,
        constituents: form.constituents,
      });

      // Create the index
      const index = await customIndexService.createIndex(
        form.name,
        form.description || undefined,
        form.calculation_method,
        form.base_value,
        form.cap_weight > 0 ? form.cap_weight : undefined,
        form.currency,
        portfolioSummary?.portfolio.id,
        form.historical_start_date || undefined
      );

      console.log('[CreateIndexModal] Index created:', index);

      // Add constituents
      for (const constituent of form.constituents) {
        console.log('[CreateIndexModal] Adding constituent:', constituent);
        await customIndexService.addConstituent(index.id, constituent);
      }

      console.log('[CreateIndexModal] All constituents added, calculating summary...');

      // Calculate initial values
      await customIndexService.calculateIndexSummary(index.id);

      // If historical start date provided, backfill historical data
      if (form.historical_start_date) {
        console.log('[CreateIndexModal] Backfilling historical data from', form.historical_start_date);
        try {
          await customIndexService.backfillHistoricalSnapshots(index.id, form.historical_start_date);
          console.log('[CreateIndexModal] Historical data backfilled successfully!');
        } catch (backfillError) {
          console.warn('[CreateIndexModal] Failed to backfill historical data:', backfillError);
          // Don't fail the entire creation - index is still created
        }
      }

      console.log('[CreateIndexModal] Index created successfully!');

      onCreated();
      onClose();
    } catch (err: any) {
      console.error('[CreateIndexModal] Error creating index:', err);
      const errorMessage = err.message || err.toString() || 'Failed to create index';
      setError(`Failed to create index: ${errorMessage}`);
    } finally {
      setCreating(false);
    }
  };

  // Render method selector
  const renderMethodSelector = () => {
    const allMethods = customIndexService.getAllMethods();

    return (
      <div style={{ marginBottom: SPACING.LARGE }}>
        <div
          style={{
            ...COMMON_STYLES.dataLabel,
            marginBottom: SPACING.SMALL,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}
        >
          <span>CALCULATION METHOD</span>
          <button
            type="button"
            onClick={() => setShowMethodHelp(!showMethodHelp)}
            style={{
              background: 'none',
              border: 'none',
              color: FINCEPT.CYAN,
              cursor: 'pointer',
              padding: 0,
            }}
          >
            <Info size={12} />
          </button>
        </div>

        <div
          style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(2, 1fr)',
            gap: SPACING.SMALL,
          }}
        >
          {allMethods.map(({ method, info }) => (
            <button
              key={method}
              type="button"
              onClick={() => handleMethodChange(method)}
              style={{
                padding: SPACING.SMALL,
                backgroundColor:
                  selectedMethod === method ? `${FINCEPT.ORANGE}20` : FINCEPT.DARK_BG,
                border:
                  selectedMethod === method
                    ? `1px solid ${FINCEPT.ORANGE}`
                    : BORDERS.STANDARD,
                borderRadius: '2px',
                cursor: 'pointer',
                textAlign: 'left',
                transition: EFFECTS.TRANSITION_FAST,
              }}
            >
              <div
                style={{
                  color: selectedMethod === method ? FINCEPT.ORANGE : FINCEPT.WHITE,
                  fontSize: TYPOGRAPHY.TINY,
                  fontWeight: TYPOGRAPHY.BOLD,
                  marginBottom: '2px',
                }}
              >
                {info.name}
              </div>
              <div
                style={{
                  color: FINCEPT.GRAY,
                  fontSize: '8px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                }}
              >
                <span
                  style={{
                    padding: '1px 4px',
                    backgroundColor:
                      info.complexity === 'simple'
                        ? `${FINCEPT.GREEN}20`
                        : info.complexity === 'moderate'
                        ? `${FINCEPT.YELLOW}20`
                        : `${FINCEPT.RED}20`,
                    color:
                      info.complexity === 'simple'
                        ? FINCEPT.GREEN
                        : info.complexity === 'moderate'
                        ? FINCEPT.YELLOW
                        : FINCEPT.RED,
                    borderRadius: '2px',
                    textTransform: 'uppercase',
                  }}
                >
                  {info.complexity}
                </span>
              </div>
            </button>
          ))}
        </div>

        {/* Method help */}
        {showMethodHelp && selectedMethod && (
          <div
            style={{
              marginTop: SPACING.SMALL,
              padding: SPACING.DEFAULT,
              backgroundColor: FINCEPT.DARK_BG,
              borderRadius: '2px',
              border: `1px solid ${FINCEPT.CYAN}40`,
            }}
          >
            <div style={{ color: FINCEPT.WHITE, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.TINY }}>
              {INDEX_METHOD_INFO[selectedMethod].description}
            </div>
            <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO }}>
              {INDEX_METHOD_INFO[selectedMethod].formula}
            </div>
          </div>
        )}
      </div>
    );
  };

  // Render portfolio stock selector
  const renderPortfolioStockSelector = () => {
    if (!portfolioSummary?.holdings || portfolioSummary.holdings.length === 0) {
      return null;
    }

    return (
      <div style={{ marginBottom: SPACING.LARGE }}>
        <button
          type="button"
          onClick={() => setUsePortfolioStocks(!usePortfolioStocks)}
          style={{
            ...COMMON_STYLES.buttonSecondary,
            width: '100%',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: SPACING.SMALL,
          }}
        >
          <Plus size={12} />
          USE STOCKS FROM PORTFOLIO
        </button>

        {usePortfolioStocks && (
          <div
            style={{
              marginTop: SPACING.SMALL,
              padding: SPACING.DEFAULT,
              backgroundColor: FINCEPT.DARK_BG,
              borderRadius: '2px',
              border: BORDERS.STANDARD,
              maxHeight: '200px',
              overflowY: 'auto',
            }}
          >
            {portfolioSummary.holdings.map(holding => (
              <div
                key={holding.symbol}
                onClick={() => togglePortfolioStock(holding.symbol)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  padding: SPACING.SMALL,
                  cursor: 'pointer',
                  backgroundColor: selectedPortfolioStocks.has(holding.symbol)
                    ? `${FINCEPT.GREEN}20`
                    : 'transparent',
                  borderRadius: '2px',
                  marginBottom: '2px',
                }}
              >
                <div>
                  <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>
                    {holding.symbol}
                  </span>
                  <span style={{ color: FINCEPT.GRAY, marginLeft: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY }}>
                    {holding.quantity} shares
                  </span>
                </div>
                {selectedPortfolioStocks.has(holding.symbol) && (
                  <Check size={14} style={{ color: FINCEPT.GREEN }} />
                )}
              </div>
            ))}

            <button
              type="button"
              onClick={handleUsePortfolioStocks}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                width: '100%',
                marginTop: SPACING.SMALL,
              }}
            >
              ADD SELECTED ({selectedPortfolioStocks.size})
            </button>
          </div>
        )}
      </div>
    );
  };

  return (
    <div style={COMMON_STYLES.modalOverlay} onClick={onClose}>
      <div
        style={{
          ...COMMON_STYLES.modalPanel,
          width: '600px',
          maxHeight: '90vh',
          overflowY: 'auto',
        }}
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: SPACING.LARGE,
          }}
        >
          <h2
            style={{
              color: FINCEPT.ORANGE,
              fontSize: TYPOGRAPHY.HEADING,
              fontWeight: TYPOGRAPHY.BOLD,
              margin: 0,
            }}
          >
            CREATE CUSTOM INDEX
          </h2>
          <button
            onClick={onClose}
            style={{
              background: 'none',
              border: 'none',
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              padding: SPACING.SMALL,
            }}
          >
            <X size={20} />
          </button>
        </div>

        {/* Error message */}
        {error && (
          <div
            style={{
              padding: SPACING.DEFAULT,
              backgroundColor: `${FINCEPT.RED}20`,
              border: `1px solid ${FINCEPT.RED}`,
              borderRadius: '2px',
              marginBottom: SPACING.LARGE,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
            }}
          >
            <AlertCircle size={16} style={{ color: FINCEPT.RED }} />
            <span style={{ color: FINCEPT.RED, fontSize: TYPOGRAPHY.SMALL }}>{error}</span>
          </div>
        )}

        {/* Form */}
        <form onSubmit={e => { e.preventDefault(); handleCreate(); }}>
          {/* Basic Info */}
          <div
            style={{
              display: 'grid',
              gridTemplateColumns: '2fr 1fr 1fr',
              gap: SPACING.DEFAULT,
              marginBottom: SPACING.LARGE,
            }}
          >
            <div>
              <label style={COMMON_STYLES.dataLabel}>INDEX NAME *</label>
              <input
                type="text"
                value={form.name}
                onChange={e => setForm(prev => ({ ...prev, name: e.target.value }))}
                placeholder="My Custom Index"
                style={{ ...COMMON_STYLES.inputField, marginTop: SPACING.TINY }}
                {...focusHandlers}
              />
            </div>
            <div>
              <label style={COMMON_STYLES.dataLabel}>BASE VALUE</label>
              <input
                type="number"
                value={form.base_value}
                onChange={e =>
                  setForm(prev => ({ ...prev, base_value: parseFloat(e.target.value) || 100 }))
                }
                min={1}
                style={{ ...COMMON_STYLES.inputField, marginTop: SPACING.TINY }}
                {...focusHandlers}
              />
            </div>
            <div>
              <label style={COMMON_STYLES.dataLabel}>CURRENCY</label>
              <select
                value={form.currency}
                onChange={e => setForm(prev => ({ ...prev, currency: e.target.value }))}
                style={{ ...COMMON_STYLES.inputField, marginTop: SPACING.TINY }}
              >
                <option value="USD">USD</option>
                <option value="EUR">EUR</option>
                <option value="GBP">GBP</option>
                <option value="INR">INR</option>
                <option value="JPY">JPY</option>
              </select>
            </div>
          </div>

          <div style={{ marginBottom: SPACING.LARGE }}>
            <label style={COMMON_STYLES.dataLabel}>DESCRIPTION</label>
            <input
              type="text"
              value={form.description}
              onChange={e => setForm(prev => ({ ...prev, description: e.target.value }))}
              placeholder="Optional description for your index"
              style={{ ...COMMON_STYLES.inputField, marginTop: SPACING.TINY }}
              {...focusHandlers}
            />
          </div>

          {/* Historical start date */}
          <div style={{ marginBottom: SPACING.LARGE }}>
            <label style={COMMON_STYLES.dataLabel}>
              HISTORICAL START DATE (Optional)
              <Info
                size={14}
                style={{
                  marginLeft: '6px',
                  cursor: 'pointer',
                  display: 'inline-block',
                  verticalAlign: 'middle'
                }}
                onMouseEnter={() => setShowHistoryHelp(true)}
                onMouseLeave={() => setShowHistoryHelp(false)}
              />
            </label>
            <input
              type="date"
              value={form.historical_start_date}
              onChange={e => setForm(prev => ({ ...prev, historical_start_date: e.target.value }))}
              max={new Date().toISOString().split('T')[0]}  // Can't be future date
              style={{ ...COMMON_STYLES.inputField, marginTop: SPACING.TINY, width: '200px' }}
              {...focusHandlers}
            />
            {showHistoryHelp && (
              <div
                style={{
                  ...EFFECTS.tooltip,
                  marginTop: SPACING.TINY,
                  padding: SPACING.DEFAULT,
                  fontSize: '12px',
                  maxWidth: '400px',
                }}
              >
                <p style={{ margin: 0, marginBottom: '4px' }}>
                  <strong>Leave empty</strong>: Index starts tracking from today with base value (100).
                </p>
                <p style={{ margin: 0 }}>
                  <strong>Select a past date</strong>: System will backfill historical data using constituent prices
                  from that date. You'll see historical performance chart and metrics.
                </p>
              </div>
            )}
          </div>

          {/* Method selector */}
          {renderMethodSelector()}

          {/* Cap weight for capped methods */}
          {(selectedMethod === 'capped_weighted' || selectedMethod === 'modified_market_cap') && (
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={COMMON_STYLES.dataLabel}>MAX WEIGHT PER CONSTITUENT (%)</label>
              <input
                type="number"
                value={form.cap_weight}
                onChange={e =>
                  setForm(prev => ({ ...prev, cap_weight: parseFloat(e.target.value) || 0 }))
                }
                min={0}
                max={100}
                placeholder="e.g., 10 for 10% max"
                style={{ ...COMMON_STYLES.inputField, marginTop: SPACING.TINY, width: '150px' }}
                {...focusHandlers}
              />
            </div>
          )}

          {/* Portfolio stock selector */}
          {renderPortfolioStockSelector()}

          {/* Constituents */}
          <div style={{ marginBottom: SPACING.LARGE }}>
            <div
              style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.SMALL,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
              }}
            >
              <span>CONSTITUENTS ({form.constituents.length})</span>
              {form.constituents.length > 0 && (
                <div style={{ display: 'flex', gap: SPACING.SMALL }}>
                  <button
                    type="button"
                    onClick={handleEqualWeights}
                    style={{
                      padding: '2px 8px',
                      fontSize: TYPOGRAPHY.TINY,
                      backgroundColor: 'transparent',
                      border: `1px solid ${FINCEPT.CYAN}`,
                      borderRadius: '2px',
                      color: FINCEPT.CYAN,
                      cursor: 'pointer',
                      fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD,
                    }}
                    title="Set all weights to equal (100 / n)"
                  >
                    EQUAL
                  </button>
                  <button
                    type="button"
                    onClick={handleNormalizeWeights}
                    style={{
                      padding: '2px 8px',
                      fontSize: TYPOGRAPHY.TINY,
                      backgroundColor: 'transparent',
                      border: `1px solid ${FINCEPT.GREEN}`,
                      borderRadius: '2px',
                      color: FINCEPT.GREEN,
                      cursor: 'pointer',
                      fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD,
                    }}
                    title="Normalize current weights to sum to 100%"
                  >
                    NORMALIZE
                  </button>
                </div>
              )}
            </div>

            {/* Add symbol input */}
            <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.SMALL }}>
              <input
                type="text"
                value={symbolInput}
                onChange={e => setSymbolInput(e.target.value.toUpperCase())}
                onKeyPress={e => {
                  if (e.key === 'Enter') {
                    e.preventDefault();
                    handleAddSymbol();
                  }
                }}
                placeholder="Enter ticker symbol (e.g., AAPL)"
                style={{ ...COMMON_STYLES.inputField, flex: 1 }}
                {...focusHandlers}
              />
              <button
                type="button"
                onClick={handleAddSymbol}
                style={{
                  ...COMMON_STYLES.buttonPrimary,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                }}
              >
                <Plus size={12} />
                ADD
              </button>
            </div>

            {/* Constituents list */}
            <div
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: BORDERS.STANDARD,
                borderRadius: '2px',
                maxHeight: '200px',
                overflowY: 'auto',
              }}
            >
              {form.constituents.length === 0 ? (
                <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.LARGE }}>
                  Add symbols to your index
                </div>
              ) : (
                <>
                  {/* Column Headers */}
                  <div
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: SPACING.DEFAULT,
                      padding: SPACING.SMALL,
                      backgroundColor: FINCEPT.HEADER_BG,
                      borderBottom: `1px solid ${FINCEPT.ORANGE}`,
                      position: 'sticky',
                      top: 0,
                      zIndex: 1,
                    }}
                  >
                    <span
                      style={{
                        color: FINCEPT.ORANGE,
                        fontSize: TYPOGRAPHY.TINY,
                        fontWeight: TYPOGRAPHY.BOLD,
                        width: '80px',
                      }}
                    >
                      SYMBOL
                    </span>
                    {selectedMethod === 'price_weighted' && (
                      <span
                        style={{
                          color: FINCEPT.ORANGE,
                          fontSize: TYPOGRAPHY.TINY,
                          fontWeight: TYPOGRAPHY.BOLD,
                          width: '80px',
                        }}
                      >
                        SHARES
                      </span>
                    )}
                    <span
                      style={{
                        color: FINCEPT.ORANGE,
                        fontSize: TYPOGRAPHY.TINY,
                        fontWeight: TYPOGRAPHY.BOLD,
                        width: '100px',
                      }}
                    >
                      WEIGHT (%)
                    </span>
                    <div style={{ flex: 1 }} />
                    <span style={{ width: '24px' }} />
                  </div>

                  {/* Constituent Rows */}
                  {form.constituents.map(constituent => (
                    <div
                      key={constituent.symbol}
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: SPACING.DEFAULT,
                        padding: SPACING.SMALL,
                        borderBottom: BORDERS.STANDARD,
                      }}
                    >
                      <span
                        style={{
                          color: FINCEPT.WHITE,
                          fontWeight: TYPOGRAPHY.BOLD,
                          width: '80px',
                        }}
                      >
                        {constituent.symbol}
                      </span>

                      {selectedMethod === 'price_weighted' && (
                        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', width: '80px' }}>
                          <input
                            type="number"
                            value={constituent.shares || 1}
                            onChange={e =>
                              handleConstituentChange(
                                constituent.symbol,
                                'shares',
                                parseFloat(e.target.value) || 1
                              )
                            }
                            min={1}
                            style={{
                              ...COMMON_STYLES.inputField,
                              width: '60px',
                              padding: '4px 6px',
                            }}
                          />
                        </div>
                      )}

                      {/* Always show weight input field */}
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px', width: '100px' }}>
                        <input
                          type="number"
                          value={constituent.weight !== undefined ? constituent.weight.toFixed(2) : '0.00'}
                          onChange={e =>
                            handleConstituentChange(
                              constituent.symbol,
                              'weight',
                              parseFloat(e.target.value) || 0
                            )
                          }
                          min={0}
                          max={100}
                          step={0.01}
                          style={{
                            ...COMMON_STYLES.inputField,
                            width: '70px',
                            padding: '4px 6px',
                          }}
                        />
                        <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}>%</span>
                      </div>

                      <div style={{ flex: 1 }} />

                      <button
                        type="button"
                        onClick={() => handleRemoveSymbol(constituent.symbol)}
                        style={{
                          background: 'none',
                          border: 'none',
                          color: FINCEPT.RED,
                          cursor: 'pointer',
                          padding: '4px',
                          width: '24px',
                        }}
                      >
                        <Trash2 size={14} />
                      </button>
                    </div>
                  ))}

                  {/* Total Weight Display */}
                  <div
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: SPACING.DEFAULT,
                      padding: SPACING.SMALL,
                      backgroundColor: FINCEPT.HEADER_BG,
                      borderTop: `1px solid ${FINCEPT.ORANGE}`,
                    }}
                  >
                    <span
                      style={{
                        color: FINCEPT.ORANGE,
                        fontSize: TYPOGRAPHY.TINY,
                        fontWeight: TYPOGRAPHY.BOLD,
                        width: '80px',
                      }}
                    >
                      TOTAL
                    </span>
                    {selectedMethod === 'price_weighted' && (
                      <span style={{ width: '80px' }} />
                    )}
                    <span
                      style={{
                        color: form.constituents.reduce((sum, c) => sum + (c.weight || 0), 0).toFixed(2) === '100.00' ? FINCEPT.GREEN : FINCEPT.YELLOW,
                        fontSize: TYPOGRAPHY.TINY,
                        fontWeight: TYPOGRAPHY.BOLD,
                        width: '100px',
                      }}
                    >
                      {form.constituents.reduce((sum, c) => sum + (c.weight || 0), 0).toFixed(2)}%
                    </span>
                  </div>
                </>
              )}
            </div>
          </div>

          {/* Actions */}
          <div style={{ display: 'flex', gap: SPACING.DEFAULT, justifyContent: 'flex-end' }}>
            <button
              type="button"
              onClick={onClose}
              style={COMMON_STYLES.buttonSecondary}
            >
              CANCEL
            </button>
            <button
              type="submit"
              disabled={creating}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                opacity: creating ? 0.7 : 1,
              }}
            >
              {creating ? 'CREATING...' : 'CREATE INDEX'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default CreateIndexModal;
