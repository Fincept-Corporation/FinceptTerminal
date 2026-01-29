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

    setForm(prev => ({
      ...prev,
      constituents: [
        ...prev.constituents,
        { symbol, shares: 1, weight: undefined },
      ],
    }));
    setSymbolInput('');
    setError(null);
  };

  const handleRemoveSymbol = (symbol: string) => {
    setForm(prev => ({
      ...prev,
      constituents: prev.constituents.filter(c => c.symbol !== symbol),
    }));
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

    const constituents = selectedHoldings.map(h => ({
      symbol: h.symbol,
      shares: h.quantity,
      weight: undefined,
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
      // Create the index
      const index = await customIndexService.createIndex(
        form.name,
        form.description || undefined,
        form.calculation_method,
        form.base_value,
        form.cap_weight > 0 ? form.cap_weight : undefined,
        form.currency,
        portfolioSummary?.portfolio.id
      );

      // Add constituents
      for (const constituent of form.constituents) {
        await customIndexService.addConstituent(index.id, constituent);
      }

      // Calculate initial values
      await customIndexService.calculateIndexSummary(index.id);

      onCreated();
      onClose();
    } catch (err: any) {
      setError(err.message || 'Failed to create index');
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
              }}
            >
              CONSTITUENTS ({form.constituents.length})
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
                form.constituents.map(constituent => (
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
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                        <label
                          style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}
                        >
                          Shares:
                        </label>
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

                    {(selectedMethod === 'fundamental_weighted' ||
                      selectedMethod === 'factor_weighted') && (
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                        <label
                          style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}
                        >
                          Weight:
                        </label>
                        <input
                          type="number"
                          value={constituent.weight || 0}
                          onChange={e =>
                            handleConstituentChange(
                              constituent.symbol,
                              'weight',
                              parseFloat(e.target.value) || 0
                            )
                          }
                          min={0}
                          max={100}
                          style={{
                            ...COMMON_STYLES.inputField,
                            width: '60px',
                            padding: '4px 6px',
                          }}
                        />
                        <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}>%</span>
                      </div>
                    )}

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
                      }}
                    >
                      <Trash2 size={14} />
                    </button>
                  </div>
                ))
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
