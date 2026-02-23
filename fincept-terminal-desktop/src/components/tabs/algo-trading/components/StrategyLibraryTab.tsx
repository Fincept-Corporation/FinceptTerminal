import React, { useState, useEffect, useMemo } from 'react';
import {
  Search, FolderOpen, Code, Play, Copy, ChevronRight,
  Loader2, AlertCircle, Filter, Grid, List, Eye, Rocket, X, BookOpen,
} from 'lucide-react';
import type { PythonStrategy } from '../types';
import {
  listPythonStrategies,
  getStrategyCategories,
  getPythonStrategyCode,
} from '../services/algoTradingService';
import { F } from '../constants/theme';

interface StrategyLibraryTabProps {
  onViewCode?: (strategy: PythonStrategy, code: string) => void;
  onClone?: (strategy: PythonStrategy) => void;
  onBacktest?: (strategy: PythonStrategy) => void;
  onDeploy?: (strategy: PythonStrategy) => void;
}

const StrategyLibraryTab: React.FC<StrategyLibraryTabProps> = ({
  onViewCode,
  onClone,
  onBacktest,
  onDeploy,
}) => {
  const [strategies, setStrategies] = useState<PythonStrategy[]>([]);
  const [categories, setCategories] = useState<string[]>([]);
  const [selectedCategory, setSelectedCategory] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [viewMode, setViewMode] = useState<'grid' | 'list'>('grid');
  const [selectedStrategy, setSelectedStrategy] = useState<PythonStrategy | null>(null);
  const [codePreview, setCodePreview] = useState<string | null>(null);
  const [loadingCode, setLoadingCode] = useState(false);

  // Load categories and strategies on mount
  useEffect(() => {
    loadData();
  }, []);

  const loadData = async () => {
    setLoading(true);
    setError(null);
    try {
      const [catResult, stratResult] = await Promise.all([
        getStrategyCategories(),
        listPythonStrategies(),
      ]);

      if (!catResult.success) {
        throw new Error(catResult.error || 'Failed to load categories');
      }
      if (!stratResult.success) {
        throw new Error(stratResult.error || 'Failed to load strategies');
      }

      setCategories(catResult.data || []);
      setStrategies(stratResult.data || []);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  // Filter strategies based on category and search
  const filteredStrategies = useMemo(() => {
    return strategies.filter((s) => {
      const matchesCategory = !selectedCategory || s.category === selectedCategory;
      const matchesSearch =
        !searchQuery ||
        s.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        s.id.toLowerCase().includes(searchQuery.toLowerCase()) ||
        (s.description && s.description.toLowerCase().includes(searchQuery.toLowerCase()));
      return matchesCategory && matchesSearch;
    });
  }, [strategies, selectedCategory, searchQuery]);

  // Category counts
  const categoryCounts = useMemo(() => {
    const counts: Record<string, number> = {};
    strategies.forEach((s) => {
      counts[s.category] = (counts[s.category] || 0) + 1;
    });
    return counts;
  }, [strategies]);

  // Load code preview
  const handleViewCode = async (strategy: PythonStrategy) => {
    setSelectedStrategy(strategy);
    setLoadingCode(true);
    try {
      const result = await getPythonStrategyCode(strategy.id);
      if (result.success && result.data) {
        setCodePreview(result.data.code);
        if (onViewCode) {
          onViewCode(strategy, result.data.code);
        }
      } else {
        setCodePreview(`// Error loading code: ${result.error}`);
      }
    } catch (err) {
      setCodePreview(`// Error: ${err instanceof Error ? err.message : 'Unknown error'}`);
    } finally {
      setLoadingCode(false);
    }
  };

  if (loading) {
    return (
      <div style={{
        display: 'flex', flexDirection: 'column', alignItems: 'center',
        justifyContent: 'center', height: '100%', gap: '12px', color: F.GRAY,
      }}>
        <Loader2 size={20} style={{ animation: 'spin 1s linear infinite', color: F.ORANGE }} />
        <span style={{ fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px' }}>
          LOADING STRATEGY LIBRARY...
        </span>
        <style>{`@keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        display: 'flex', flexDirection: 'column', alignItems: 'center',
        justifyContent: 'center', height: '100%', gap: '12px',
      }}>
        <AlertCircle size={20} color={F.RED} />
        <span style={{ fontSize: '10px', color: F.RED, fontWeight: 700 }}>{error}</span>
        <button
          onClick={loadData}
          style={{
            padding: '6px 14px', backgroundColor: F.ORANGE, color: F.DARK_BG,
            border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
            cursor: 'pointer', letterSpacing: '0.5px',
          }}
        >
          RETRY
        </button>
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', height: '100%', backgroundColor: F.DARK_BG }}>
      {/* ─── LEFT SIDEBAR — CATEGORIES ─── */}
      <div style={{
        width: '200px', backgroundColor: F.PANEL_BG,
        borderRight: `1px solid ${F.BORDER}`,
        display: 'flex', flexDirection: 'column',
      }}>
        <div style={{
          padding: '12px', borderBottom: `1px solid ${F.BORDER}`,
          display: 'flex', alignItems: 'center', gap: '8px',
          backgroundColor: F.HEADER_BG,
        }}>
          <BookOpen size={12} color={F.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
            CATEGORIES
          </span>
          <span style={{ fontSize: '8px', color: F.MUTED, marginLeft: 'auto' }}>
            {categories.length}
          </span>
        </div>

        <div style={{ flex: 1, overflow: 'auto' }}>
          {/* All Strategies */}
          <button
            onClick={() => setSelectedCategory(null)}
            style={{
              width: '100%', padding: '8px 12px',
              backgroundColor: selectedCategory === null ? `${F.ORANGE}10` : 'transparent',
              color: selectedCategory === null ? F.ORANGE : F.GRAY,
              border: 'none', borderLeft: selectedCategory === null ? `2px solid ${F.ORANGE}` : '2px solid transparent',
              textAlign: 'left', fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '8px',
              transition: 'all 0.15s', letterSpacing: '0.3px',
            }}
          >
            <FolderOpen size={11} />
            <span style={{ flex: 1 }}>ALL</span>
            <span style={{ fontSize: '8px', color: F.MUTED }}>{strategies.length}</span>
          </button>

          {/* Category List */}
          {categories.map((cat) => (
            <button
              key={cat}
              onClick={() => setSelectedCategory(cat)}
              style={{
                width: '100%', padding: '8px 12px',
                backgroundColor: selectedCategory === cat ? `${F.ORANGE}10` : 'transparent',
                color: selectedCategory === cat ? F.ORANGE : F.GRAY,
                border: 'none', borderLeft: selectedCategory === cat ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                textAlign: 'left', fontSize: '9px',
                cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '8px',
                transition: 'all 0.15s',
              }}
            >
              <ChevronRight
                size={9}
                style={{
                  transform: selectedCategory === cat ? 'rotate(90deg)' : 'none',
                  transition: 'transform 0.2s',
                }}
              />
              <span style={{
                flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                fontWeight: selectedCategory === cat ? 700 : 400,
              }}>
                {cat}
              </span>
              <span style={{ fontSize: '8px', color: F.MUTED }}>{categoryCounts[cat] || 0}</span>
            </button>
          ))}
        </div>
      </div>

      {/* ─── MAIN CONTENT ─── */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
        {/* Search & View Controls */}
        <div style={{
          padding: '10px 16px', borderBottom: `1px solid ${F.BORDER}`,
          display: 'flex', alignItems: 'center', gap: '10px',
          backgroundColor: F.HEADER_BG,
        }}>
          <div style={{
            flex: 1, display: 'flex', alignItems: 'center', gap: '8px',
            backgroundColor: F.DARK_BG, padding: '7px 10px', borderRadius: '2px',
            border: `1px solid ${F.BORDER}`,
          }}>
            <Search size={12} color={F.GRAY} />
            <input
              type="text"
              placeholder="Search strategies by name, ID, or description..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              style={{
                flex: 1, backgroundColor: 'transparent', border: 'none',
                color: F.WHITE, fontSize: '10px', outline: 'none',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            />
            {searchQuery && (
              <button
                onClick={() => setSearchQuery('')}
                style={{
                  backgroundColor: 'transparent', border: 'none',
                  color: F.GRAY, cursor: 'pointer', padding: '2px',
                  display: 'flex', alignItems: 'center',
                }}
              >
                <X size={10} />
              </button>
            )}
          </div>

          <span style={{ fontSize: '9px', color: F.MUTED, fontWeight: 700 }}>
            {filteredStrategies.length}
          </span>

          {/* View Toggle */}
          <div style={{
            display: 'flex', border: `1px solid ${F.BORDER}`,
            borderRadius: '2px', overflow: 'hidden',
          }}>
            <button
              onClick={() => setViewMode('grid')}
              style={{
                padding: '5px 8px',
                backgroundColor: viewMode === 'grid' ? `${F.ORANGE}20` : 'transparent',
                color: viewMode === 'grid' ? F.ORANGE : F.GRAY,
                border: 'none', borderRight: `1px solid ${F.BORDER}`,
                cursor: 'pointer', display: 'flex', alignItems: 'center',
              }}
            >
              <Grid size={12} />
            </button>
            <button
              onClick={() => setViewMode('list')}
              style={{
                padding: '5px 8px',
                backgroundColor: viewMode === 'list' ? `${F.ORANGE}20` : 'transparent',
                color: viewMode === 'list' ? F.ORANGE : F.GRAY,
                border: 'none', cursor: 'pointer',
                display: 'flex', alignItems: 'center',
              }}
            >
              <List size={12} />
            </button>
          </div>
        </div>

        {/* Strategy Grid/List */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {filteredStrategies.length === 0 ? (
            <div style={{
              display: 'flex', flexDirection: 'column', alignItems: 'center',
              justifyContent: 'center', height: '200px', gap: '8px',
            }}>
              <Code size={24} color={F.MUTED} style={{ opacity: 0.4 }} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.GRAY }}>
                NO STRATEGIES FOUND
              </span>
              {searchQuery && (
                <span style={{ fontSize: '9px', color: F.MUTED }}>
                  Try a different search term
                </span>
              )}
            </div>
          ) : viewMode === 'grid' ? (
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
              gap: '10px',
            }}>
              {filteredStrategies.map((strategy) => (
                <StrategyCard
                  key={strategy.id}
                  strategy={strategy}
                  onViewCode={() => handleViewCode(strategy)}
                  onClone={onClone ? () => onClone(strategy) : undefined}
                  onBacktest={onBacktest ? () => onBacktest(strategy) : undefined}
                  onDeploy={onDeploy ? () => onDeploy(strategy) : undefined}
                />
              ))}
            </div>
          ) : (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {filteredStrategies.map((strategy) => (
                <StrategyRow
                  key={strategy.id}
                  strategy={strategy}
                  onViewCode={() => handleViewCode(strategy)}
                  onClone={onClone ? () => onClone(strategy) : undefined}
                  onBacktest={onBacktest ? () => onBacktest(strategy) : undefined}
                />
              ))}
            </div>
          )}
        </div>
      </div>

      {/* ─── RIGHT SIDEBAR — CODE PREVIEW ─── */}
      {selectedStrategy && (
        <div style={{
          width: '380px', backgroundColor: F.PANEL_BG,
          borderLeft: `1px solid ${F.BORDER}`,
          display: 'flex', flexDirection: 'column',
        }}>
          {/* Preview Header */}
          <div style={{
            padding: '12px', borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            backgroundColor: F.HEADER_BG,
          }}>
            <div style={{ minWidth: 0, flex: 1 }}>
              <div style={{
                fontSize: '10px', fontWeight: 700, color: F.WHITE,
                overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
              }}>
                {selectedStrategy.name}
              </div>
              <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
                {selectedStrategy.id}
              </div>
            </div>
            <button
              onClick={() => { setSelectedStrategy(null); setCodePreview(null); }}
              style={{
                padding: '4px', backgroundColor: 'transparent',
                border: `1px solid ${F.BORDER}`, color: F.MUTED,
                cursor: 'pointer', borderRadius: '2px', marginLeft: '8px',
                display: 'flex', alignItems: 'center',
              }}
              onMouseEnter={e => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.RED; }}
              onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.MUTED; }}
            >
              <X size={10} />
            </button>
          </div>

          {/* Strategy Info */}
          <div style={{
            padding: '10px 12px', borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', gap: '6px', flexWrap: 'wrap',
          }}>
            <span style={{
              padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              backgroundColor: `${F.CYAN}15`, color: F.CYAN,
            }}>
              {selectedStrategy.category}
            </span>
            {selectedStrategy.compatibility.map((mode) => (
              <span
                key={mode}
                style={{
                  padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                  backgroundColor: mode === 'live' ? `${F.GREEN}15` : mode === 'paper' ? `${F.YELLOW}15` : `${F.BLUE}15`,
                  color: mode === 'live' ? F.GREEN : mode === 'paper' ? F.YELLOW : F.BLUE,
                  textTransform: 'uppercase',
                }}
              >
                {mode}
              </span>
            ))}
          </div>

          {/* Code Preview */}
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            {loadingCode ? (
              <div style={{
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                height: '100px', color: F.GRAY,
              }}>
                <Loader2 size={14} style={{ animation: 'spin 1s linear infinite' }} />
              </div>
            ) : (
              <pre style={{
                margin: 0, padding: '10px',
                backgroundColor: F.DARK_BG, borderRadius: '2px',
                border: `1px solid ${F.BORDER}`,
                fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace',
                color: F.WHITE, overflow: 'auto',
                whiteSpace: 'pre-wrap', wordBreak: 'break-word',
                lineHeight: '1.5',
              }}>
                {codePreview || '// No code available'}
              </pre>
            )}
          </div>

          {/* Preview Actions */}
          <div style={{
            padding: '10px 12px', borderTop: `1px solid ${F.BORDER}`,
            display: 'flex', gap: '6px',
          }}>
            {onClone && (
              <button
                onClick={() => onClone(selectedStrategy)}
                style={{
                  flex: 1, padding: '8px',
                  backgroundColor: `${F.BLUE}15`, color: F.BLUE,
                  border: `1px solid ${F.BLUE}30`, borderRadius: '2px',
                  fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
                  letterSpacing: '0.5px', transition: 'all 0.2s',
                }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.BLUE}25`; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = `${F.BLUE}15`; }}
              >
                <Copy size={10} />
                CLONE
              </button>
            )}
            {onBacktest && (
              <button
                onClick={() => onBacktest(selectedStrategy)}
                style={{
                  flex: 1, padding: '8px',
                  backgroundColor: F.ORANGE, color: F.DARK_BG,
                  border: 'none', borderRadius: '2px',
                  fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
                  letterSpacing: '0.5px', transition: 'opacity 0.2s',
                }}
                onMouseEnter={e => { e.currentTarget.style.opacity = '0.85'; }}
                onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
              >
                <Play size={10} />
                BACKTEST
              </button>
            )}
          </div>
        </div>
      )}
    </div>
  );
};

// ── Strategy Card Component ──────────────────────────────────────────────
const StrategyCard: React.FC<{
  strategy: PythonStrategy;
  onViewCode: () => void;
  onClone?: () => void;
  onBacktest?: () => void;
  onDeploy?: () => void;
}> = ({ strategy, onViewCode, onClone, onBacktest, onDeploy }) => {
  return (
    <div style={{
      backgroundColor: F.PANEL_BG,
      border: `1px solid ${F.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden',
      display: 'flex', flexDirection: 'column',
      transition: 'border-color 0.2s',
    }}
      onMouseEnter={e => { (e.currentTarget as HTMLElement).style.borderColor = `${F.ORANGE}40`; }}
      onMouseLeave={e => { (e.currentTarget as HTMLElement).style.borderColor = F.BORDER; }}
    >
      {/* Header */}
      <div style={{
        padding: '10px 12px', backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        borderLeft: `3px solid ${F.PURPLE}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', gap: '8px' }}>
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{
              fontSize: '10px', fontWeight: 700, color: F.WHITE,
              marginBottom: '2px', wordBreak: 'break-word', lineHeight: '1.3',
            }}>
              {strategy.name}
            </div>
            <div style={{
              fontSize: '8px', color: F.MUTED,
              overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
            }}>
              {strategy.id}
            </div>
          </div>
          <span style={{
            fontSize: '7px', padding: '2px 6px',
            backgroundColor: `${F.CYAN}15`, color: F.CYAN,
            borderRadius: '2px', fontWeight: 700, flexShrink: 0,
            maxWidth: '80px', overflow: 'hidden', textOverflow: 'ellipsis',
            whiteSpace: 'nowrap', letterSpacing: '0.5px',
          }}
            title={strategy.category}
          >
            {strategy.category}
          </span>
        </div>
      </div>

      {/* Body */}
      <div style={{ padding: '10px 12px', flex: 1 }}>
        {/* Description */}
        <div style={{
          fontSize: '9px', color: F.GRAY, lineHeight: '1.4',
          height: '26px', overflow: 'hidden', textOverflow: 'ellipsis',
          display: '-webkit-box', WebkitLineClamp: 2,
          WebkitBoxOrient: 'vertical' as const, marginBottom: '8px',
        }}
          title={strategy.description || 'No description available'}
        >
          {strategy.description || 'No description available'}
        </div>

        {/* Compatibility Tags */}
        <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
          {strategy.compatibility.map((mode) => (
            <span
              key={mode}
              style={{
                fontSize: '7px', padding: '2px 5px', fontWeight: 700,
                backgroundColor: mode === 'live' ? `${F.GREEN}15` : mode === 'paper' ? `${F.YELLOW}15` : `${F.BLUE}15`,
                color: mode === 'live' ? F.GREEN : mode === 'paper' ? F.YELLOW : F.BLUE,
                borderRadius: '2px', textTransform: 'uppercase', letterSpacing: '0.5px',
              }}
            >
              {mode}
            </span>
          ))}
        </div>
      </div>

      {/* Actions */}
      <div style={{
        display: 'flex', gap: '4px', padding: '8px 12px',
        borderTop: `1px solid ${F.BORDER}`,
      }}>
        <button
          onClick={onViewCode}
          style={{
            flex: 1, padding: '5px',
            backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
            color: F.GRAY, borderRadius: '2px', fontSize: '8px', fontWeight: 700,
            cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px',
            transition: 'all 0.2s', letterSpacing: '0.5px',
          }}
          onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
          onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
        >
          <Eye size={9} /> VIEW
        </button>
        {onClone && (
          <button
            onClick={onClone}
            style={{
              flex: 1, padding: '5px',
              backgroundColor: 'transparent', border: `1px solid ${F.BLUE}30`,
              color: F.BLUE, borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px',
              transition: 'all 0.2s', letterSpacing: '0.5px',
            }}
            onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.BLUE}10`; }}
            onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
          >
            <Copy size={9} /> CLONE
          </button>
        )}
        {onBacktest && (
          <button
            onClick={onBacktest}
            style={{
              flex: 1, padding: '5px',
              backgroundColor: F.ORANGE, border: 'none',
              color: F.DARK_BG, borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px',
              letterSpacing: '0.5px', transition: 'opacity 0.2s',
            }}
            onMouseEnter={e => { e.currentTarget.style.opacity = '0.85'; }}
            onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
          >
            <Play size={9} /> TEST
          </button>
        )}
      </div>
    </div>
  );
};

// ── Strategy Row Component (List View) ──────────────────────────────────
const StrategyRow: React.FC<{
  strategy: PythonStrategy;
  onViewCode: () => void;
  onClone?: () => void;
  onBacktest?: () => void;
}> = ({ strategy, onViewCode, onClone, onBacktest }) => {
  return (
    <div style={{
      backgroundColor: F.PANEL_BG,
      border: `1px solid ${F.BORDER}`,
      borderRadius: '2px',
      padding: '8px 12px',
      display: 'flex', alignItems: 'center', gap: '12px',
      transition: 'border-color 0.15s',
    }}
      onMouseEnter={e => { (e.currentTarget as HTMLElement).style.borderColor = `${F.ORANGE}40`; }}
      onMouseLeave={e => { (e.currentTarget as HTMLElement).style.borderColor = F.BORDER; }}
    >
      <Code size={12} color={F.PURPLE} />
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>{strategy.name}</div>
        <div style={{
          fontSize: '8px', color: F.MUTED,
          overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
        }}>
          {strategy.id} — {strategy.description || 'No description'}
        </div>
      </div>
      <span style={{
        fontSize: '7px', padding: '2px 6px',
        backgroundColor: `${F.CYAN}15`, color: F.CYAN,
        borderRadius: '2px', fontWeight: 700,
      }}>
        {strategy.category}
      </span>
      <div style={{ display: 'flex', gap: '4px' }}>
        <button
          onClick={onViewCode}
          style={{
            padding: '4px 8px', backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`, color: F.GRAY,
            borderRadius: '2px', fontSize: '8px', fontWeight: 700,
            cursor: 'pointer', transition: 'all 0.2s',
          }}
          onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
          onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
        >
          VIEW
        </button>
        {onClone && (
          <button
            onClick={onClone}
            style={{
              padding: '4px 8px', backgroundColor: 'transparent',
              border: `1px solid ${F.BLUE}30`, color: F.BLUE,
              borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              cursor: 'pointer', transition: 'all 0.2s',
            }}
          >
            CLONE
          </button>
        )}
        {onBacktest && (
          <button
            onClick={onBacktest}
            style={{
              padding: '4px 8px', backgroundColor: F.ORANGE,
              border: 'none', color: F.DARK_BG, borderRadius: '2px',
              fontSize: '8px', fontWeight: 700, cursor: 'pointer',
            }}
          >
            TEST
          </button>
        )}
      </div>
    </div>
  );
};

export default StrategyLibraryTab;
