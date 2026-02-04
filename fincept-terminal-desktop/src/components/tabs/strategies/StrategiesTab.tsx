/**
 * Strategies Tab - Fincept Terminal Strategy Engine
 *
 * Browse, inspect, and deploy 400+ algorithmic trading strategies.
 * Three-panel terminal layout: Category sidebar | Strategy list + detail | Info panel
 */

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { invoke } from '@tauri-apps/api/core';
import {
  Activity, Play, Loader, Search, X, ChevronRight, Code, Eye,
  Zap, Target, Shield, BarChart3, TrendingUp, TrendingDown,
  Layers, Database, GitBranch, Box, DollarSign, Globe,
  Cpu, Filter, Copy, Download, BookOpen, Tag, Clock,
  AlertCircle, CheckCircle, RefreshCw, Settings, Rocket
} from 'lucide-react';

// ============================================================================
// FINCEPT DESIGN SYSTEM
// ============================================================================

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F', HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A', HOVER: '#1F1F1F', MUTED: '#4A4A4A', CYAN: '#00E5FF',
  YELLOW: '#FFD700', BLUE: '#0088FF', PURPLE: '#9D4EDD',
};

// ============================================================================
// TYPES
// ============================================================================

interface StrategyEntry {
  id: string;
  name: string;
  category: string;
  path: string;
  description?: string;
  compatibility?: string;
}

interface CategoryInfo {
  name: string;
  count: number;
  icon: React.ReactNode;
  color: string;
}

// ============================================================================
// CATEGORY CONFIG
// ============================================================================

const CATEGORY_CONFIG: Record<string, { icon: React.ReactNode; color: string }> = {
  'Options':              { icon: <Layers size={12} />,      color: F.PURPLE },
  'Regression Test':      { icon: <GitBranch size={12} />,   color: F.MUTED },
  'General Strategy':     { icon: <Activity size={12} />,    color: F.BLUE },
  'Universe Selection':   { icon: <Globe size={12} />,       color: F.CYAN },
  'Futures':              { icon: <TrendingUp size={12} />,  color: F.YELLOW },
  'Alpha Model':          { icon: <Zap size={12} />,         color: F.ORANGE },
  'Data Consolidation':   { icon: <Database size={12} />,    color: F.GRAY },
  'Indicators':           { icon: <BarChart3 size={12} />,   color: F.CYAN },
  'Benchmark':            { icon: <Target size={12} />,      color: F.MUTED },
  'Template':             { icon: <BookOpen size={12} />,    color: F.GREEN },
  'ETF':                  { icon: <Box size={12} />,         color: F.BLUE },
  'Portfolio Management': { icon: <DollarSign size={12} />,  color: F.GREEN },
  'Risk Management':      { icon: <Shield size={12} />,      color: F.RED },
  'Index':                { icon: <BarChart3 size={12} />,   color: F.YELLOW },
  'Execution Model':      { icon: <Rocket size={12} />,      color: F.ORANGE },
  'Crypto':               { icon: <Cpu size={12} />,         color: F.PURPLE },
  'Machine Learning':     { icon: <Cpu size={12} />,         color: F.CYAN },
  'Custom Data':          { icon: <Database size={12} />,    color: F.BLUE },
  'Forex':                { icon: <Globe size={12} />,       color: F.GREEN },
  'Momentum':             { icon: <TrendingUp size={12} />,  color: F.ORANGE },
  'Brokerage':            { icon: <Settings size={12} />,    color: F.GRAY },
  'Margin':               { icon: <AlertCircle size={12} />, color: F.RED },
  'Corporate Actions':    { icon: <Tag size={12} />,         color: F.YELLOW },
  'Historical Data':      { icon: <Clock size={12} />,       color: F.BLUE },
  'Scheduled Events':     { icon: <Clock size={12} />,       color: F.CYAN },
  'Warmup':               { icon: <RefreshCw size={12} />,   color: F.MUTED },
};

const DEFAULT_CATEGORY = { icon: <Activity size={12} />, color: F.GRAY };

// ============================================================================
// COMPONENT
// ============================================================================

const StrategiesTab: React.FC = () => {
  // State
  const [strategies, setStrategies] = useState<StrategyEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedCategory, setSelectedCategory] = useState<string | null>(null);
  const [selectedStrategy, setSelectedStrategy] = useState<StrategyEntry | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [sourceCode, setSourceCode] = useState<string | null>(null);
  const [loadingSource, setLoadingSource] = useState(false);
  const [viewMode, setViewMode] = useState<'list' | 'code'>('list');

  // Workspace state persistence
  const getWorkspaceState = useCallback(() => ({
    selectedCategory, searchQuery, viewMode,
  }), [selectedCategory, searchQuery, viewMode]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.selectedCategory === 'string') setSelectedCategory(state.selectedCategory);
    if (typeof state.searchQuery === 'string') setSearchQuery(state.searchQuery);
    if (state.viewMode === 'list' || state.viewMode === 'code') setViewMode(state.viewMode);
  }, []);

  useWorkspaceTabState('strategies', getWorkspaceState, setWorkspaceState);

  // Load strategies from backend
  useEffect(() => {
    loadStrategies();
  }, []);

  const loadStrategies = async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke<string>('list_strategies');
      const parsed = JSON.parse(result);
      if (parsed.success) {
        setStrategies(parsed.data);
      } else {
        setError(parsed.error || 'Failed to load strategies');
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const loadSourceCode = async (strategy: StrategyEntry) => {
    setLoadingSource(true);
    setSourceCode(null);
    try {
      const result = await invoke<string>('read_strategy_source', { path: strategy.path });
      const parsed = JSON.parse(result);
      if (parsed.success) {
        setSourceCode(parsed.data);
      }
    } catch (err: any) {
      setSourceCode(`// Error loading source: ${err}`);
    } finally {
      setLoadingSource(false);
    }
  };

  // Derived data
  const categories = useMemo<CategoryInfo[]>(() => {
    const counts: Record<string, number> = {};
    strategies.forEach(s => { counts[s.category] = (counts[s.category] || 0) + 1; });
    return Object.entries(counts)
      .map(([name, count]) => ({
        name, count,
        icon: (CATEGORY_CONFIG[name] || DEFAULT_CATEGORY).icon,
        color: (CATEGORY_CONFIG[name] || DEFAULT_CATEGORY).color,
      }))
      .sort((a, b) => b.count - a.count);
  }, [strategies]);

  const filteredStrategies = useMemo(() => {
    let list = strategies;
    if (selectedCategory) {
      list = list.filter(s => s.category === selectedCategory);
    }
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase();
      list = list.filter(s =>
        s.name.toLowerCase().includes(q) ||
        s.id.toLowerCase().includes(q) ||
        s.category.toLowerCase().includes(q)
      );
    }
    return list;
  }, [strategies, selectedCategory, searchQuery]);

  const handleSelectStrategy = (s: StrategyEntry) => {
    setSelectedStrategy(s);
    loadSourceCode(s);
  };

  const totalStrategies = strategies.length;

  // ============================================================================
  // RENDER: LEFT SIDEBAR - CATEGORIES
  // ============================================================================

  const renderCategorySidebar = () => (
    <div style={{
      width: 260, minWidth: 260,
      backgroundColor: F.PANEL_BG,
      borderRight: `1px solid ${F.BORDER}`,
      display: 'flex', flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Sidebar header */}
      <div style={{
        padding: '12px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
          CATEGORIES
        </span>
        <div style={{ fontSize: '9px', color: F.MUTED, marginTop: '4px' }}>
          {totalStrategies} strategies available
        </div>
      </div>

      {/* All button */}
      <div
        onClick={() => setSelectedCategory(null)}
        style={{
          padding: '10px 12px',
          backgroundColor: selectedCategory === null ? `${F.ORANGE}15` : 'transparent',
          borderLeft: selectedCategory === null ? `2px solid ${F.ORANGE}` : '2px solid transparent',
          cursor: 'pointer', transition: 'all 0.2s',
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          borderBottom: `1px solid ${F.BORDER}`,
        }}
        onMouseEnter={e => { if (selectedCategory !== null) (e.currentTarget.style.backgroundColor = F.HOVER); }}
        onMouseLeave={e => { if (selectedCategory !== null) (e.currentTarget.style.backgroundColor = 'transparent'); }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Activity size={12} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: selectedCategory === null ? F.WHITE : F.GRAY }}>
            ALL STRATEGIES
          </span>
        </div>
        <span style={{ fontSize: '9px', color: F.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>
          {totalStrategies}
        </span>
      </div>

      {/* Category list */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {categories.map(cat => (
          <div
            key={cat.name}
            onClick={() => setSelectedCategory(cat.name)}
            style={{
              padding: '8px 12px',
              backgroundColor: selectedCategory === cat.name ? `${cat.color}15` : 'transparent',
              borderLeft: selectedCategory === cat.name ? `2px solid ${cat.color}` : '2px solid transparent',
              cursor: 'pointer', transition: 'all 0.2s',
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            }}
            onMouseEnter={e => { if (selectedCategory !== cat.name) (e.currentTarget.style.backgroundColor = F.HOVER); }}
            onMouseLeave={e => { if (selectedCategory !== cat.name) (e.currentTarget.style.backgroundColor = selectedCategory === cat.name ? `${cat.color}15` : 'transparent'); }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span style={{ color: cat.color }}>{cat.icon}</span>
              <span style={{
                fontSize: '9px', fontWeight: 700, letterSpacing: '0.3px',
                color: selectedCategory === cat.name ? F.WHITE : F.GRAY,
              }}>
                {cat.name.toUpperCase()}
              </span>
            </div>
            <span style={{
              fontSize: '9px', color: F.CYAN,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {cat.count}
            </span>
          </div>
        ))}
      </div>
    </div>
  );

  // ============================================================================
  // RENDER: CENTER - STRATEGY LIST
  // ============================================================================

  const renderStrategyList = () => (
    <div style={{
      flex: 1,
      display: 'flex', flexDirection: 'column',
      borderRight: `1px solid ${F.BORDER}`,
      overflow: 'hidden', minWidth: 0,
    }}>
      {/* Search bar */}
      <div style={{
        padding: '8px 12px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        display: 'flex', alignItems: 'center', gap: '8px',
      }}>
        <Search size={12} style={{ color: F.MUTED, flexShrink: 0 }} />
        <input
          type="text"
          value={searchQuery}
          onChange={e => setSearchQuery(e.target.value)}
          placeholder="Search strategies by name, ID, or category..."
          style={{
            flex: 1,
            padding: '6px 8px',
            backgroundColor: F.DARK_BG,
            color: F.WHITE,
            border: `1px solid ${F.BORDER}`,
            borderRadius: '2px',
            fontSize: '10px',
            fontFamily: '"IBM Plex Mono", monospace',
            outline: 'none',
          }}
          onFocus={e => (e.currentTarget.style.borderColor = F.ORANGE)}
          onBlur={e => (e.currentTarget.style.borderColor = F.BORDER)}
        />
        {searchQuery && (
          <X
            size={12}
            style={{ color: F.MUTED, cursor: 'pointer', flexShrink: 0 }}
            onClick={() => setSearchQuery('')}
          />
        )}
        <span style={{ fontSize: '9px', color: F.CYAN, fontFamily: '"IBM Plex Mono", monospace', flexShrink: 0 }}>
          {filteredStrategies.length}
        </span>
      </div>

      {/* Strategy list */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {filteredStrategies.map(s => {
          const catConf = CATEGORY_CONFIG[s.category] || DEFAULT_CATEGORY;
          const isSelected = selectedStrategy?.id === s.id;
          return (
            <div
              key={s.id}
              onClick={() => handleSelectStrategy(s)}
              style={{
                padding: '10px 12px',
                backgroundColor: isSelected ? `${F.ORANGE}15` : 'transparent',
                borderLeft: isSelected ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                borderBottom: `1px solid ${F.BORDER}10`,
                cursor: 'pointer', transition: 'all 0.2s',
              }}
              onMouseEnter={e => { if (!isSelected) e.currentTarget.style.backgroundColor = F.HOVER; }}
              onMouseLeave={e => { if (!isSelected) e.currentTarget.style.backgroundColor = 'transparent'; }}
            >
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px', minWidth: 0 }}>
                  <span style={{ color: catConf.color, flexShrink: 0 }}>{catConf.icon}</span>
                  <span style={{
                    fontSize: '10px', fontWeight: 600, color: F.WHITE,
                    whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis',
                  }}>
                    {s.name}
                  </span>
                </div>
                <ChevronRight size={10} style={{ color: F.MUTED, flexShrink: 0 }} />
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginLeft: '18px' }}>
                <span style={{
                  fontSize: '8px', fontWeight: 700, color: F.CYAN,
                  fontFamily: '"IBM Plex Mono", monospace',
                }}>
                  {s.id}
                </span>
                <span style={{
                  padding: '1px 4px',
                  backgroundColor: `${catConf.color}20`,
                  color: catConf.color,
                  fontSize: '7px', fontWeight: 700, borderRadius: '2px',
                  letterSpacing: '0.3px',
                }}>
                  {s.category.toUpperCase()}
                </span>
              </div>
            </div>
          );
        })}

        {filteredStrategies.length === 0 && !loading && (
          <div style={{
            display: 'flex', flexDirection: 'column', alignItems: 'center',
            justifyContent: 'center', height: '200px', color: F.MUTED, fontSize: '10px',
          }}>
            <Search size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No strategies found</span>
          </div>
        )}
      </div>
    </div>
  );

  // ============================================================================
  // RENDER: RIGHT PANEL - DETAIL / CODE VIEW
  // ============================================================================

  const renderDetailPanel = () => {
    if (!selectedStrategy) {
      return (
        <div style={{
          width: 380, minWidth: 380,
          backgroundColor: F.PANEL_BG,
          display: 'flex', flexDirection: 'column',
          alignItems: 'center', justifyContent: 'center',
          color: F.MUTED, fontSize: '10px', textAlign: 'center',
        }}>
          <Eye size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
          <span>Select a strategy to view details</span>
        </div>
      );
    }

    const catConf = CATEGORY_CONFIG[selectedStrategy.category] || DEFAULT_CATEGORY;

    return (
      <div style={{
        width: 380, minWidth: 380,
        backgroundColor: F.PANEL_BG,
        display: 'flex', flexDirection: 'column',
        overflow: 'hidden',
      }}>
        {/* Detail header */}
        <div style={{
          padding: '12px',
          backgroundColor: F.HEADER_BG,
          borderBottom: `1px solid ${F.BORDER}`,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
            <span style={{ color: catConf.color }}>{catConf.icon}</span>
            <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE }}>
              {selectedStrategy.name}
            </span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{
              fontSize: '8px', fontWeight: 700, color: F.CYAN,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {selectedStrategy.id}
            </span>
            <span style={{
              padding: '1px 4px',
              backgroundColor: `${catConf.color}20`, color: catConf.color,
              fontSize: '7px', fontWeight: 700, borderRadius: '2px',
            }}>
              {selectedStrategy.category.toUpperCase()}
            </span>
          </div>
        </div>

        {/* View toggle */}
        <div style={{
          padding: '6px 12px',
          backgroundColor: F.DARK_BG,
          borderBottom: `1px solid ${F.BORDER}`,
          display: 'flex', gap: '4px',
        }}>
          {(['list', 'code'] as const).map(mode => (
            <button
              key={mode}
              onClick={() => setViewMode(mode)}
              style={{
                padding: '4px 10px',
                backgroundColor: viewMode === mode ? F.ORANGE : 'transparent',
                color: viewMode === mode ? F.DARK_BG : F.GRAY,
                border: 'none', fontSize: '9px', fontWeight: 700,
                cursor: 'pointer', borderRadius: '2px', letterSpacing: '0.5px',
              }}
            >
              {mode === 'list' ? 'INFO' : 'SOURCE'}
            </button>
          ))}
        </div>

        {/* Content */}
        <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
          {viewMode === 'list' ? renderInfoView() : renderCodeView()}
        </div>

        {/* Action buttons */}
        <div style={{
          padding: '8px 12px',
          backgroundColor: F.HEADER_BG,
          borderTop: `1px solid ${F.BORDER}`,
          display: 'flex', gap: '6px',
        }}>
          <button
            style={{
              flex: 1, padding: '8px', backgroundColor: F.ORANGE,
              color: F.DARK_BG, border: 'none', borderRadius: '2px',
              fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
            }}
          >
            <Play size={10} /> BACKTEST
          </button>
          <button
            style={{
              flex: 1, padding: '8px', backgroundColor: 'transparent',
              color: F.GRAY, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
              fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
            }}
          >
            <Copy size={10} /> CLONE
          </button>
          <button
            style={{
              padding: '8px', backgroundColor: 'transparent',
              color: F.GRAY, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
              fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
            }}
          >
            <Download size={10} />
          </button>
        </div>
      </div>
    );
  };

  const renderInfoView = () => {
    if (!selectedStrategy) return null;

    const infoRows = [
      { label: 'STRATEGY ID', value: selectedStrategy.id, color: F.CYAN },
      { label: 'CATEGORY', value: selectedStrategy.category, color: (CATEGORY_CONFIG[selectedStrategy.category] || DEFAULT_CATEGORY).color },
      { label: 'FILE', value: selectedStrategy.path, color: F.MUTED },
      { label: 'COMPATIBILITY', value: 'Backtesting | Paper Trading | Live', color: F.GREEN },
      { label: 'ENGINE', value: 'Fincept Strategy Engine v1.0', color: F.ORANGE },
      { label: 'MARKETS', value: 'USA, India (NSE/BSE), Crypto, Forex', color: F.BLUE },
    ];

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        {infoRows.map(row => (
          <div key={row.label} style={{
            padding: '8px',
            backgroundColor: F.DARK_BG,
            border: `1px solid ${F.BORDER}`,
            borderRadius: '2px',
          }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>
              {row.label}
            </div>
            <div style={{
              fontSize: '10px', color: row.color,
              fontFamily: '"IBM Plex Mono", monospace',
              wordBreak: 'break-all',
            }}>
              {row.value}
            </div>
          </div>
        ))}

        {/* Capabilities */}
        <div style={{
          padding: '8px',
          backgroundColor: F.DARK_BG,
          border: `1px solid ${F.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>
            CAPABILITIES
          </div>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
            {['Indicators', 'Orders', 'Portfolio', 'Scheduling', 'Risk Mgmt'].map(cap => (
              <span key={cap} style={{
                padding: '2px 6px', backgroundColor: `${F.GREEN}20`,
                color: F.GREEN, fontSize: '8px', fontWeight: 700, borderRadius: '2px',
              }}>
                {cap.toUpperCase()}
              </span>
            ))}
          </div>
        </div>
      </div>
    );
  };

  const renderCodeView = () => {
    if (loadingSource) {
      return (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '200px', color: F.MUTED }}>
          <Loader size={16} className="animate-spin" />
        </div>
      );
    }

    return (
      <pre style={{
        margin: 0, padding: '8px',
        backgroundColor: F.DARK_BG,
        border: `1px solid ${F.BORDER}`,
        borderRadius: '2px',
        fontSize: '9px',
        color: F.CYAN,
        fontFamily: '"IBM Plex Mono", monospace',
        whiteSpace: 'pre-wrap',
        wordBreak: 'break-all',
        lineHeight: '1.5',
        overflowX: 'auto',
      }}>
        {sourceCode || '// No source loaded'}
      </pre>
    );
  };

  // ============================================================================
  // RENDER: MAIN
  // ============================================================================

  if (loading) {
    return (
      <div style={{
        height: '100%', display: 'flex', flexDirection: 'column',
        alignItems: 'center', justifyContent: 'center',
        backgroundColor: F.DARK_BG, color: F.MUTED, fontSize: '10px',
      }}>
        <Loader size={20} className="animate-spin" style={{ marginBottom: '12px', color: F.ORANGE }} />
        <span>Loading strategy engine...</span>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        height: '100%', display: 'flex', flexDirection: 'column',
        alignItems: 'center', justifyContent: 'center',
        backgroundColor: F.DARK_BG, color: F.RED, fontSize: '10px',
      }}>
        <AlertCircle size={20} style={{ marginBottom: '8px' }} />
        <span>{error}</span>
        <button
          onClick={loadStrategies}
          style={{
            marginTop: '12px', padding: '6px 16px', backgroundColor: F.ORANGE,
            color: F.DARK_BG, border: 'none', borderRadius: '2px',
            fontSize: '9px', fontWeight: 700, cursor: 'pointer',
          }}
        >
          RETRY
        </button>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%', display: 'flex', flexDirection: 'column',
      backgroundColor: F.DARK_BG, color: F.WHITE,
    }}>
      {/* Top nav bar */}
      <div style={{
        padding: '8px 16px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${F.ORANGE}20`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Zap size={14} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE }}>
            STRATEGY ENGINE
          </span>
          <span style={{
            padding: '2px 6px', backgroundColor: `${F.ORANGE}20`,
            color: F.ORANGE, fontSize: '8px', fontWeight: 700, borderRadius: '2px',
          }}>
            {totalStrategies} STRATEGIES
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{
            padding: '2px 6px', backgroundColor: `${F.GREEN}20`,
            color: F.GREEN, fontSize: '8px', fontWeight: 700, borderRadius: '2px',
          }}>
            FINCEPT ENGINE v1.0
          </span>
          <span style={{
            padding: '2px 6px', backgroundColor: `${F.CYAN}20`,
            color: F.CYAN, fontSize: '8px', fontWeight: 700, borderRadius: '2px',
          }}>
            PURE PYTHON
          </span>
        </div>
      </div>

      {/* Three-panel layout */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {renderCategorySidebar()}
        {renderStrategyList()}
        {renderDetailPanel()}
      </div>

      {/* Status bar */}
      <div style={{
        padding: '4px 16px',
        backgroundColor: F.HEADER_BG,
        borderTop: `1px solid ${F.BORDER}`,
        fontSize: '9px', color: F.GRAY,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>
            <span style={{ color: F.CYAN }}>{categories.length}</span> categories
          </span>
          <span>
            <span style={{ color: F.CYAN }}>{filteredStrategies.length}</span> / {totalStrategies} visible
          </span>
          {selectedCategory && (
            <span>
              Filter: <span style={{ color: F.ORANGE }}>{selectedCategory}</span>
            </span>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <CheckCircle size={10} style={{ color: F.GREEN }} />
          <span>Engine ready</span>
        </div>
      </div>
    </div>
  );
};

export default StrategiesTab;
