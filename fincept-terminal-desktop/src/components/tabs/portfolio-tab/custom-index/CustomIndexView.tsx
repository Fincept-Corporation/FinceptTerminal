/**
 * CustomIndexView - Aggregate Index Management Component
 * Allows users to create, monitor, and manage custom indices
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  TrendingUp,
  TrendingDown,
  Plus,
  Trash2,
  RefreshCw,
  BarChart2,
  Info,
  Settings,
  Download,
  ChevronRight,
  Clock,
  Layers,
  Edit3,
  X,
  Save,
} from 'lucide-react';
import {
  FINCEPT,
  TYPOGRAPHY,
  SPACING,
  BORDERS,
  COMMON_STYLES,
  EFFECTS,
  getValueColor,
  formatPercentage,
  formatCurrency,
} from '../finceptStyles';
import {
  CustomIndex,
  IndexSummary,
  IndexConstituent,
  IndexCalculationMethod,
  INDEX_METHOD_INFO,
} from './types';
import { customIndexService } from '../../../../services/portfolio/customIndexService';

interface CustomIndexViewProps {
  portfolioId?: string;
  onCreateIndex: () => void;
}

const CustomIndexView: React.FC<CustomIndexViewProps> = ({
  portfolioId,
  onCreateIndex,
}) => {
  const [indices, setIndices] = useState<CustomIndex[]>([]);
  const [selectedIndex, setSelectedIndex] = useState<CustomIndex | null>(null);
  const [indexSummary, setIndexSummary] = useState<IndexSummary | null>(null);
  const [loading, setLoading] = useState(false);
  const [refreshing, setRefreshing] = useState(false);
  const [showMethodInfo, setShowMethodInfo] = useState<IndexCalculationMethod | null>(null);
  const [editingConstituent, setEditingConstituent] = useState<IndexConstituent | null>(null);
  const [editForm, setEditForm] = useState({ shares: '', weight: '', marketCap: '', fundamentalScore: '', customPrice: '', priceDate: '' });
  const [fetchingPrice, setFetchingPrice] = useState(false);

  // Load indices on mount
  useEffect(() => {
    loadIndices();
  }, []);

  // Load index summary when selected index changes
  useEffect(() => {
    if (selectedIndex) {
      loadIndexSummary(selectedIndex.id);
    } else {
      setIndexSummary(null);
    }
  }, [selectedIndex?.id]);

  const loadIndices = async () => {
    setLoading(true);
    try {
      const allIndices = await customIndexService.getAllIndices();
      // Filter by portfolio if provided
      const filtered = portfolioId
        ? allIndices.filter(i => i.portfolio_id === portfolioId || !i.portfolio_id)
        : allIndices;
      setIndices(filtered);

      // Select first index if none selected
      if (!selectedIndex && filtered.length > 0) {
        setSelectedIndex(filtered[0]);
      }
    } catch (error) {
      console.error('Failed to load indices:', error);
    } finally {
      setLoading(false);
    }
  };

  const loadIndexSummary = async (indexId: string) => {
    setRefreshing(true);
    try {
      const summary = await customIndexService.calculateIndexSummary(indexId);
      setIndexSummary(summary);
    } catch (error) {
      console.error('Failed to load index summary:', error);
    } finally {
      setRefreshing(false);
    }
  };

  const handleRefresh = useCallback(() => {
    if (selectedIndex) {
      loadIndexSummary(selectedIndex.id);
    }
  }, [selectedIndex]);

  const handleDeleteIndex = async (indexId: string) => {
    if (!confirm('Are you sure you want to delete this index?')) return;

    try {
      await customIndexService.deleteIndex(indexId);
      await loadIndices();
      if (selectedIndex?.id === indexId) {
        setSelectedIndex(null);
      }
    } catch (error) {
      console.error('Failed to delete index:', error);
    }
  };

  const handleSaveSnapshot = async () => {
    if (!selectedIndex) return;

    try {
      await customIndexService.saveDailySnapshot(selectedIndex.id);
      alert('Snapshot saved successfully!');
    } catch (error) {
      console.error('Failed to save snapshot:', error);
      alert('Failed to save snapshot');
    }
  };

  const handleEditConstituent = (constituent: IndexConstituent) => {
    setEditingConstituent(constituent);
    setEditForm({
      shares: constituent.shares?.toString() || '',
      weight: constituent.weight?.toString() || '',
      marketCap: constituent.marketCap?.toString() || '',
      fundamentalScore: constituent.fundamentalScore?.toString() || '',
      customPrice: constituent.customPrice?.toString() || '',
      priceDate: constituent.priceDate || '',
    });
  };

  const handleDateChange = async (date: string) => {
    if (!editingConstituent || !date) return;

    setEditForm(prev => ({ ...prev, priceDate: date }));

    // Only fetch if no custom price is set
    if (!editForm.customPrice) {
      setFetchingPrice(true);
      try {
        const price = await customIndexService.getHistoricalPrice(editingConstituent.symbol, date);
        if (price) {
          setEditForm(prev => ({ ...prev, customPrice: price.toString() }));
        }
      } catch (error) {
        console.error('Failed to fetch historical price:', error);
      } finally {
        setFetchingPrice(false);
      }
    }
  };

  const handleSaveConstituent = async () => {
    if (!editingConstituent) return;

    try {
      await customIndexService.updateConstituent(
        editingConstituent.id,
        editForm.shares ? parseFloat(editForm.shares) : undefined,
        editForm.weight ? parseFloat(editForm.weight) : undefined,
        editForm.marketCap ? parseFloat(editForm.marketCap) : undefined,
        editForm.fundamentalScore ? parseFloat(editForm.fundamentalScore) : undefined,
        editForm.customPrice ? parseFloat(editForm.customPrice) : undefined,
        editForm.priceDate || undefined
      );
      setEditingConstituent(null);
      handleRefresh();
    } catch (error) {
      console.error('Failed to update constituent:', error);
    }
  };

  const handleRemoveConstituent = async (symbol: string) => {
    if (!selectedIndex || !confirm(`Remove ${symbol} from index?`)) return;

    try {
      await customIndexService.removeConstituent(selectedIndex.id, symbol);
      handleRefresh();
    } catch (error) {
      console.error('Failed to remove constituent:', error);
    }
  };

  // Render method info modal
  const renderMethodInfoModal = () => {
    if (!showMethodInfo) return null;

    const info = INDEX_METHOD_INFO[showMethodInfo];

    return (
      <div style={COMMON_STYLES.modalOverlay} onClick={() => setShowMethodInfo(null)}>
        <div
          style={{
            ...COMMON_STYLES.modalPanel,
            maxWidth: '500px',
          }}
          onClick={e => e.stopPropagation()}
        >
          <h3
            style={{
              color: FINCEPT.ORANGE,
              fontSize: TYPOGRAPHY.HEADING,
              fontWeight: TYPOGRAPHY.BOLD,
              marginBottom: SPACING.MEDIUM,
            }}
          >
            {info.name}
          </h3>

          <p style={{ color: FINCEPT.WHITE, marginBottom: SPACING.DEFAULT, lineHeight: '1.5' }}>
            {info.description}
          </p>

          <div
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              padding: SPACING.DEFAULT,
              borderRadius: '2px',
              marginBottom: SPACING.DEFAULT,
            }}
          >
            <div style={COMMON_STYLES.dataLabel}>FORMULA</div>
            <div
              style={{
                color: FINCEPT.CYAN,
                fontFamily: TYPOGRAPHY.MONO,
                fontSize: TYPOGRAPHY.BODY,
                marginTop: SPACING.SMALL,
              }}
            >
              {info.formula}
            </div>
          </div>

          <div style={{ marginBottom: SPACING.DEFAULT }}>
            <div style={COMMON_STYLES.dataLabel}>EXAMPLES</div>
            <ul style={{ paddingLeft: '20px', marginTop: SPACING.SMALL }}>
              {info.examples.map((ex, i) => (
                <li key={i} style={{ color: FINCEPT.WHITE, fontSize: TYPOGRAPHY.BODY, marginBottom: '4px' }}>
                  {ex}
                </li>
              ))}
            </ul>
          </div>

          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
            }}
          >
            <span style={COMMON_STYLES.dataLabel}>COMPLEXITY:</span>
            <span
              style={{
                padding: '2px 8px',
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
                fontSize: '9px',
                fontWeight: TYPOGRAPHY.BOLD,
                textTransform: 'uppercase',
              }}
            >
              {info.complexity}
            </span>
          </div>

          <button
            onClick={() => setShowMethodInfo(null)}
            style={{
              ...COMMON_STYLES.buttonSecondary,
              marginTop: SPACING.LARGE,
              width: '100%',
            }}
          >
            CLOSE
          </button>
        </div>
      </div>
    );
  };

  // Render index list
  const renderIndexList = () => (
    <div
      style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: BORDERS.STANDARD,
        borderRadius: '2px',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
      }}
    >
      <div
        style={{
          ...COMMON_STYLES.sectionHeader,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: 0,
        }}
      >
        <span>INDICES</span>
        <button
          onClick={onCreateIndex}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            padding: '4px 8px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
          }}
        >
          <Plus size={12} />
          NEW
        </button>
      </div>

      <div style={{ flex: 1, overflowY: 'auto', padding: SPACING.SMALL }}>
        {loading ? (
          <div style={COMMON_STYLES.emptyState}>Loading...</div>
        ) : indices.length === 0 ? (
          <div style={COMMON_STYLES.emptyState}>
            <Layers size={32} style={{ marginBottom: SPACING.SMALL, opacity: 0.5 }} />
            <div>No indices created</div>
            <button
              onClick={onCreateIndex}
              style={{ ...COMMON_STYLES.buttonPrimary, marginTop: SPACING.MEDIUM }}
            >
              CREATE YOUR FIRST INDEX
            </button>
          </div>
        ) : (
          indices.map(index => (
            <div
              key={index.id}
              onClick={() => setSelectedIndex(index)}
              style={{
                padding: SPACING.DEFAULT,
                backgroundColor:
                  selectedIndex?.id === index.id ? `${FINCEPT.ORANGE}20` : 'transparent',
                borderLeft:
                  selectedIndex?.id === index.id
                    ? `2px solid ${FINCEPT.ORANGE}`
                    : '2px solid transparent',
                cursor: 'pointer',
                marginBottom: SPACING.TINY,
                transition: EFFECTS.TRANSITION_FAST,
              }}
            >
              <div
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  marginBottom: SPACING.TINY,
                }}
              >
                <span
                  style={{
                    color: FINCEPT.WHITE,
                    fontWeight: TYPOGRAPHY.BOLD,
                    fontSize: TYPOGRAPHY.BODY,
                  }}
                >
                  {index.name}
                </span>
                <ChevronRight size={14} style={{ color: FINCEPT.GRAY }} />
              </div>
              <div
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                }}
              >
                <span
                  style={{
                    color: FINCEPT.CYAN,
                    fontSize: TYPOGRAPHY.SMALL,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}
                >
                  {formatCurrency(index.current_value, '', 2)}
                </span>
                <span
                  style={{
                    color: getValueColor(index.current_value - index.previous_close),
                    fontSize: TYPOGRAPHY.TINY,
                  }}
                >
                  {formatPercentage(
                    index.previous_close > 0
                      ? ((index.current_value - index.previous_close) / index.previous_close) * 100
                      : 0
                  )}
                </span>
              </div>
              <div
                style={{
                  color: FINCEPT.GRAY,
                  fontSize: TYPOGRAPHY.TINY,
                  marginTop: SPACING.TINY,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                }}
              >
                <Info
                  size={10}
                  style={{ cursor: 'pointer' }}
                  onClick={e => {
                    e.stopPropagation();
                    setShowMethodInfo(index.calculation_method as IndexCalculationMethod);
                  }}
                />
                {INDEX_METHOD_INFO[index.calculation_method as IndexCalculationMethod]?.name || index.calculation_method}
              </div>
            </div>
          ))
        )}
      </div>
    </div>
  );

  // Render index details
  const renderIndexDetails = () => {
    if (!selectedIndex) {
      return (
        <div style={{ ...COMMON_STYLES.emptyState, height: '100%' }}>
          <BarChart2 size={48} style={{ marginBottom: SPACING.DEFAULT, opacity: 0.5 }} />
          <div>Select an index to view details</div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, marginTop: SPACING.SMALL }}>
            Or create a new index to get started
          </div>
        </div>
      );
    }

    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
        {/* Header */}
        <div
          style={{
            ...COMMON_STYLES.sectionHeader,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: 0,
          }}
        >
          <div>
            <span>{selectedIndex.name}</span>
            {selectedIndex.description && (
              <span style={{ color: FINCEPT.GRAY, marginLeft: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY }}>
                {selectedIndex.description}
              </span>
            )}
          </div>
          <div style={{ display: 'flex', gap: SPACING.SMALL }}>
            <button
              onClick={handleRefresh}
              style={{
                ...COMMON_STYLES.buttonSecondary,
                padding: '4px 8px',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
              disabled={refreshing}
            >
              <RefreshCw size={12} className={refreshing ? 'animate-spin' : ''} />
            </button>
            <button
              onClick={handleSaveSnapshot}
              style={{
                ...COMMON_STYLES.buttonSecondary,
                padding: '4px 8px',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
              title="Save daily snapshot"
            >
              <Download size={12} />
            </button>
            <button
              onClick={() => handleDeleteIndex(selectedIndex.id)}
              style={{
                ...COMMON_STYLES.buttonSecondary,
                padding: '4px 8px',
                color: FINCEPT.RED,
              }}
            >
              <Trash2 size={12} />
            </button>
          </div>
        </div>

        {/* Index Value Card */}
        <div
          style={{
            padding: SPACING.LARGE,
            borderBottom: BORDERS.STANDARD,
            backgroundColor: FINCEPT.DARK_BG,
          }}
        >
          <div
            style={{
              display: 'flex',
              alignItems: 'flex-end',
              gap: SPACING.DEFAULT,
            }}
          >
            <span
              style={{
                fontSize: '28px',
                fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.WHITE,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {formatCurrency(indexSummary?.index_value || selectedIndex.current_value, '', 2)}
            </span>
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.SMALL,
                marginBottom: '4px',
              }}
            >
              {(indexSummary?.day_change || 0) >= 0 ? (
                <TrendingUp size={16} style={{ color: FINCEPT.GREEN }} />
              ) : (
                <TrendingDown size={16} style={{ color: FINCEPT.RED }} />
              )}
              <span
                style={{
                  color: getValueColor(indexSummary?.day_change || 0),
                  fontSize: TYPOGRAPHY.SUBHEADING,
                  fontWeight: TYPOGRAPHY.BOLD,
                }}
              >
                {formatPercentage(indexSummary?.day_change_percent || 0)}
              </span>
              <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL }}>
                ({formatCurrency(indexSummary?.day_change || 0, '', 2)})
              </span>
            </div>
          </div>

          {/* Index Stats */}
          <div
            style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(4, 1fr)',
              gap: SPACING.DEFAULT,
              marginTop: SPACING.LARGE,
            }}
          >
            <div>
              <div style={COMMON_STYLES.dataLabel}>BASE VALUE</div>
              <div style={COMMON_STYLES.dataValue}>{formatCurrency(selectedIndex.base_value, '', 0)}</div>
            </div>
            <div>
              <div style={COMMON_STYLES.dataLabel}>MARKET VALUE</div>
              <div style={COMMON_STYLES.dataValue}>
                {formatCurrency(indexSummary?.total_market_value || 0, '$', 0)}
              </div>
            </div>
            <div>
              <div style={COMMON_STYLES.dataLabel}>DIVISOR</div>
              <div style={COMMON_STYLES.dataValue}>{(indexSummary?.index.divisor || 1).toFixed(4)}</div>
            </div>
            <div>
              <div style={COMMON_STYLES.dataLabel}>CONSTITUENTS</div>
              <div style={COMMON_STYLES.dataValue}>{indexSummary?.constituents.length || 0}</div>
            </div>
          </div>
        </div>

        {/* Constituents Table */}
        <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          <div
            style={{
              ...COMMON_STYLES.tableHeader,
              display: 'grid',
              gridTemplateColumns: '1fr 70px 70px 70px 70px 60px 60px',
            }}
          >
            <span>SYMBOL</span>
            <span style={{ textAlign: 'right' }}>PRICE</span>
            <span style={{ textAlign: 'right' }}>CHANGE</span>
            <span style={{ textAlign: 'right' }}>WEIGHT</span>
            <span style={{ textAlign: 'right' }}>CONTRIB</span>
            <span style={{ textAlign: 'right' }}>MKT VAL</span>
            <span style={{ textAlign: 'center' }}>ACTIONS</span>
          </div>

          <div style={{ flex: 1, overflowY: 'auto' }}>
            {refreshing ? (
              <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.LARGE }}>
                <RefreshCw size={24} className="animate-spin" />
                <span style={{ marginTop: SPACING.SMALL }}>Calculating...</span>
              </div>
            ) : indexSummary?.constituents.length === 0 ? (
              <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.LARGE }}>
                No constituents in this index
              </div>
            ) : (
              indexSummary?.constituents.map((c, i) => (
                <div
                  key={c.id}
                  style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 70px 70px 70px 70px 60px 60px',
                    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                    borderBottom: BORDERS.STANDARD,
                    backgroundColor: i % 2 === 1 ? 'rgba(255,255,255,0.02)' : 'transparent',
                    fontSize: TYPOGRAPHY.BODY,
                    alignItems: 'center',
                  }}
                >
                  <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>
                    {c.symbol}
                  </span>
                  <span style={{ textAlign: 'right', color: FINCEPT.CYAN, fontFamily: TYPOGRAPHY.MONO }}>
                    {formatCurrency(c.current_price, '$', 2)}
                  </span>
                  <span
                    style={{
                      textAlign: 'right',
                      color: getValueColor(c.day_change_percent),
                      fontFamily: TYPOGRAPHY.MONO,
                    }}
                  >
                    {formatPercentage(c.day_change_percent)}
                  </span>
                  <span style={{ textAlign: 'right', color: FINCEPT.WHITE, fontFamily: TYPOGRAPHY.MONO }}>
                    {c.effective_weight.toFixed(2)}%
                  </span>
                  <span
                    style={{
                      textAlign: 'right',
                      color: getValueColor(c.contribution),
                      fontFamily: TYPOGRAPHY.MONO,
                    }}
                  >
                    {(c.contribution * 100).toFixed(3)}%
                  </span>
                  <span style={{ textAlign: 'right', color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
                    {formatCurrency(c.market_value / 1000, '', 0)}K
                  </span>
                  <div style={{ display: 'flex', justifyContent: 'center', gap: '4px' }}>
                    <button
                      onClick={() => handleEditConstituent(c)}
                      style={{
                        background: 'none',
                        border: 'none',
                        cursor: 'pointer',
                        padding: '2px',
                        color: FINCEPT.CYAN,
                      }}
                      title="Edit"
                    >
                      <Edit3 size={12} />
                    </button>
                    <button
                      onClick={() => handleRemoveConstituent(c.symbol)}
                      style={{
                        background: 'none',
                        border: 'none',
                        cursor: 'pointer',
                        padding: '2px',
                        color: FINCEPT.RED,
                      }}
                      title="Remove"
                    >
                      <Trash2 size={12} />
                    </button>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Footer with method info */}
        <div
          style={{
            padding: SPACING.DEFAULT,
            borderTop: BORDERS.STANDARD,
            backgroundColor: FINCEPT.DARK_BG,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <Clock size={12} style={{ color: FINCEPT.GRAY }} />
            <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}>
              Last updated: {indexSummary?.last_updated ? new Date(indexSummary.last_updated).toLocaleString() : 'N/A'}
            </span>
          </div>
          <button
            onClick={() =>
              setShowMethodInfo(selectedIndex.calculation_method as IndexCalculationMethod)
            }
            style={{
              ...COMMON_STYLES.buttonSecondary,
              padding: '4px 8px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <Info size={12} />
            {INDEX_METHOD_INFO[selectedIndex.calculation_method as IndexCalculationMethod]?.name}
          </button>
        </div>
      </div>
    );
  };

  return (
    <div
      style={{
        height: '100%',
        display: 'grid',
        gridTemplateColumns: '280px 1fr',
        gap: SPACING.DEFAULT,
      }}
    >
      {/* Left sidebar: Index list */}
      {renderIndexList()}

      {/* Main content: Index details */}
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.STANDARD,
          borderRadius: '2px',
          overflow: 'hidden',
        }}
      >
        {renderIndexDetails()}
      </div>

      {/* Method info modal */}
      {renderMethodInfoModal()}

      {/* Edit Constituent Modal */}
      {editingConstituent && (
        <div style={COMMON_STYLES.modalOverlay} onClick={() => setEditingConstituent(null)}>
          <div
            style={{
              ...COMMON_STYLES.modalPanel,
              width: '360px',
              minWidth: '360px',
            }}
            onClick={e => e.stopPropagation()}
          >
            <div
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: SPACING.DEFAULT,
              }}
            >
              <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>
                EDIT {editingConstituent.symbol}
              </span>
              <button
                onClick={() => setEditingConstituent(null)}
                style={{ background: 'none', border: 'none', color: FINCEPT.GRAY, cursor: 'pointer' }}
              >
                <X size={16} />
              </button>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
                <div>
                  <label style={COMMON_STYLES.dataLabel}>PRICE DATE</label>
                  <input
                    type="date"
                    value={editForm.priceDate}
                    onChange={e => handleDateChange(e.target.value)}
                    style={{ ...COMMON_STYLES.inputField, width: '100%' }}
                  />
                </div>
                <div>
                  <label style={COMMON_STYLES.dataLabel}>
                    PRICE {fetchingPrice && <RefreshCw size={10} className="animate-spin" style={{ marginLeft: 4 }} />}
                  </label>
                  <input
                    type="number"
                    value={editForm.customPrice}
                    onChange={e => setEditForm({ ...editForm, customPrice: e.target.value })}
                    style={{ ...COMMON_STYLES.inputField, width: '100%' }}
                    placeholder="Auto-fetch or manual"
                  />
                </div>
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
                <div>
                  <label style={COMMON_STYLES.dataLabel}>SHARES</label>
                  <input
                    type="number"
                    value={editForm.shares}
                    onChange={e => setEditForm({ ...editForm, shares: e.target.value })}
                    style={{ ...COMMON_STYLES.inputField, width: '100%' }}
                    placeholder="Shares"
                  />
                </div>
                <div>
                  <label style={COMMON_STYLES.dataLabel}>WEIGHT (%)</label>
                  <input
                    type="number"
                    value={editForm.weight}
                    onChange={e => setEditForm({ ...editForm, weight: e.target.value })}
                    style={{ ...COMMON_STYLES.inputField, width: '100%' }}
                    placeholder="0-100"
                  />
                </div>
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
                <div>
                  <label style={COMMON_STYLES.dataLabel}>MARKET CAP</label>
                  <input
                    type="number"
                    value={editForm.marketCap}
                    onChange={e => setEditForm({ ...editForm, marketCap: e.target.value })}
                    style={{ ...COMMON_STYLES.inputField, width: '100%' }}
                    placeholder="Market cap"
                  />
                </div>
                <div>
                  <label style={COMMON_STYLES.dataLabel}>FUND. SCORE</label>
                  <input
                    type="number"
                    value={editForm.fundamentalScore}
                    onChange={e => setEditForm({ ...editForm, fundamentalScore: e.target.value })}
                    style={{ ...COMMON_STYLES.inputField, width: '100%' }}
                    placeholder="1-100"
                  />
                </div>
              </div>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginTop: SPACING.TINY }}>
                Set date to auto-fetch closing price, or enter price manually
              </div>
            </div>

            <div style={{ display: 'flex', gap: SPACING.SMALL, marginTop: SPACING.LARGE }}>
              <button
                onClick={() => setEditingConstituent(null)}
                style={{ ...COMMON_STYLES.buttonSecondary, flex: 1 }}
              >
                CANCEL
              </button>
              <button
                onClick={handleSaveConstituent}
                style={{
                  ...COMMON_STYLES.buttonPrimary,
                  flex: 1,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                }}
              >
                <Save size={12} />
                SAVE
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default CustomIndexView;
