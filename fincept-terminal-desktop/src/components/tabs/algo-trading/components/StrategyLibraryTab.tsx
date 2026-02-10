import React, { useState, useEffect, useMemo } from 'react';
import {
  Search,
  FolderOpen,
  Code,
  Play,
  Copy,
  ChevronRight,
  Loader2,
  AlertCircle,
  ExternalLink,
  Filter,
  Grid,
  List,
  Eye,
} from 'lucide-react';
import type { PythonStrategy } from '../types';
import {
  listPythonStrategies,
  getStrategyCategories,
  getPythonStrategyCode,
} from '../services/algoTradingService';

const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

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
      <div
        style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          gap: '12px',
          color: F.GRAY,
        }}
      >
        <Loader2 size={24} style={{ animation: 'spin 1s linear infinite' }} />
        <span style={{ fontSize: '11px' }}>Loading strategy library...</span>
        <style>{`@keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
      </div>
    );
  }

  if (error) {
    return (
      <div
        style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          gap: '12px',
          color: F.RED,
        }}
      >
        <AlertCircle size={24} />
        <span style={{ fontSize: '11px' }}>{error}</span>
        <button
          onClick={loadData}
          style={{
            padding: '6px 12px',
            backgroundColor: F.ORANGE,
            color: F.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '10px',
            fontWeight: 700,
            cursor: 'pointer',
          }}
        >
          RETRY
        </button>
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', height: '100%', backgroundColor: F.DARK_BG }}>
      {/* Left Sidebar - Categories */}
      <div
        style={{
          width: '220px',
          backgroundColor: F.PANEL_BG,
          borderRight: `1px solid ${F.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        <div
          style={{
            padding: '12px',
            borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
          }}
        >
          <Filter size={12} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>CATEGORIES</span>
          <span
            style={{
              fontSize: '9px',
              color: F.GRAY,
              marginLeft: 'auto',
            }}
          >
            {categories.length}
          </span>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '4px 0' }}>
          {/* All Strategies */}
          <button
            onClick={() => setSelectedCategory(null)}
            style={{
              width: '100%',
              padding: '8px 12px',
              backgroundColor: selectedCategory === null ? F.HOVER : 'transparent',
              color: selectedCategory === null ? F.ORANGE : F.GRAY,
              border: 'none',
              textAlign: 'left',
              fontSize: '10px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
            }}
          >
            <FolderOpen size={12} />
            <span style={{ flex: 1 }}>All Strategies</span>
            <span style={{ fontSize: '9px', color: F.MUTED }}>{strategies.length}</span>
          </button>

          {/* Category List */}
          {categories.map((cat) => (
            <button
              key={cat}
              onClick={() => setSelectedCategory(cat)}
              style={{
                width: '100%',
                padding: '8px 12px',
                backgroundColor: selectedCategory === cat ? F.HOVER : 'transparent',
                color: selectedCategory === cat ? F.ORANGE : F.GRAY,
                border: 'none',
                textAlign: 'left',
                fontSize: '10px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}
            >
              <ChevronRight
                size={10}
                style={{
                  transform: selectedCategory === cat ? 'rotate(90deg)' : 'none',
                  transition: 'transform 0.2s',
                }}
              />
              <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                {cat}
              </span>
              <span style={{ fontSize: '9px', color: F.MUTED }}>{categoryCounts[cat] || 0}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
        {/* Search Bar */}
        <div
          style={{
            padding: '12px 16px',
            borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
          }}
        >
          <div
            style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              backgroundColor: F.PANEL_BG,
              padding: '8px 12px',
              borderRadius: '4px',
              border: `1px solid ${F.BORDER}`,
            }}
          >
            <Search size={14} style={{ color: F.GRAY }} />
            <input
              type="text"
              placeholder="Search strategies by name, ID, or description..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              style={{
                flex: 1,
                backgroundColor: 'transparent',
                border: 'none',
                color: F.WHITE,
                fontSize: '11px',
                outline: 'none',
              }}
            />
            {searchQuery && (
              <button
                onClick={() => setSearchQuery('')}
                style={{
                  backgroundColor: 'transparent',
                  border: 'none',
                  color: F.GRAY,
                  cursor: 'pointer',
                  fontSize: '12px',
                }}
              >
                ×
              </button>
            )}
          </div>

          <span style={{ fontSize: '10px', color: F.GRAY }}>
            {filteredStrategies.length} strategies
          </span>

          <div style={{ display: 'flex', gap: '4px' }}>
            <button
              onClick={() => setViewMode('grid')}
              style={{
                padding: '6px',
                backgroundColor: viewMode === 'grid' ? F.HOVER : 'transparent',
                color: viewMode === 'grid' ? F.ORANGE : F.GRAY,
                border: 'none',
                borderRadius: '2px',
                cursor: 'pointer',
              }}
            >
              <Grid size={14} />
            </button>
            <button
              onClick={() => setViewMode('list')}
              style={{
                padding: '6px',
                backgroundColor: viewMode === 'list' ? F.HOVER : 'transparent',
                color: viewMode === 'list' ? F.ORANGE : F.GRAY,
                border: 'none',
                borderRadius: '2px',
                cursor: 'pointer',
              }}
            >
              <List size={14} />
            </button>
          </div>
        </div>

        {/* Strategy Grid/List */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {filteredStrategies.length === 0 ? (
            <div
              style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '200px',
                color: F.GRAY,
                fontSize: '11px',
              }}
            >
              <Code size={32} style={{ marginBottom: '12px', opacity: 0.5 }} />
              <span>No strategies found</span>
              {searchQuery && (
                <span style={{ fontSize: '10px', marginTop: '4px' }}>
                  Try a different search term
                </span>
              )}
            </div>
          ) : viewMode === 'grid' ? (
            <div
              style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
                gap: '12px',
              }}
            >
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

      {/* Right Sidebar - Code Preview */}
      {selectedStrategy && (
        <div
          style={{
            width: '400px',
            backgroundColor: F.PANEL_BG,
            borderLeft: `1px solid ${F.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
          }}
        >
          <div
            style={{
              padding: '12px',
              borderBottom: `1px solid ${F.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}
          >
            <div>
              <div style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE }}>
                {selectedStrategy.name}
              </div>
              <div style={{ fontSize: '9px', color: F.GRAY, marginTop: '2px' }}>
                {selectedStrategy.id}
              </div>
            </div>
            <button
              onClick={() => {
                setSelectedStrategy(null);
                setCodePreview(null);
              }}
              style={{
                backgroundColor: 'transparent',
                border: 'none',
                color: F.GRAY,
                cursor: 'pointer',
                fontSize: '16px',
              }}
            >
              ×
            </button>
          </div>

          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            {loadingCode ? (
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '100px',
                  color: F.GRAY,
                }}
              >
                <Loader2 size={16} style={{ animation: 'spin 1s linear infinite' }} />
              </div>
            ) : (
              <pre
                style={{
                  margin: 0,
                  padding: '12px',
                  backgroundColor: F.DARK_BG,
                  borderRadius: '4px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  color: F.WHITE,
                  overflow: 'auto',
                  whiteSpace: 'pre-wrap',
                  wordBreak: 'break-word',
                }}
              >
                {codePreview || '// No code available'}
              </pre>
            )}
          </div>

          <div
            style={{
              padding: '12px',
              borderTop: `1px solid ${F.BORDER}`,
              display: 'flex',
              gap: '8px',
            }}
          >
            {onClone && (
              <button
                onClick={() => onClone(selectedStrategy)}
                style={{
                  flex: 1,
                  padding: '8px',
                  backgroundColor: F.BLUE,
                  color: F.WHITE,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                }}
              >
                <Copy size={12} />
                CLONE
              </button>
            )}
            {onBacktest && (
              <button
                onClick={() => onBacktest(selectedStrategy)}
                style={{
                  flex: 1,
                  padding: '8px',
                  backgroundColor: F.ORANGE,
                  color: F.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                }}
              >
                <Play size={12} />
                BACKTEST
              </button>
            )}
          </div>
        </div>
      )}
    </div>
  );
};

// Strategy Card Component
const StrategyCard: React.FC<{
  strategy: PythonStrategy;
  onViewCode: () => void;
  onClone?: () => void;
  onBacktest?: () => void;
  onDeploy?: () => void;
}> = ({ strategy, onViewCode, onClone, onBacktest, onDeploy }) => {
  return (
    <div
      style={{
        backgroundColor: F.PANEL_BG,
        border: `1px solid ${F.BORDER}`,
        borderRadius: '4px',
        padding: '12px',
        display: 'flex',
        flexDirection: 'column',
        gap: '8px',
      }}
    >
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', gap: '8px' }}>
        <div style={{ flex: 1, minWidth: 0 }}>
          <div
            style={{
              fontSize: '11px',
              fontWeight: 700,
              color: F.WHITE,
              marginBottom: '4px',
              wordBreak: 'break-word',
              lineHeight: '1.3',
            }}
          >
            {strategy.name}
          </div>
          <div style={{ fontSize: '9px', color: F.GRAY, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{strategy.id}</div>
        </div>
        <span
          style={{
            fontSize: '8px',
            padding: '2px 6px',
            backgroundColor: F.HOVER,
            color: F.CYAN,
            borderRadius: '2px',
            flexShrink: 0,
            maxWidth: '80px',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          }}
          title={strategy.category}
        >
          {strategy.category}
        </span>
      </div>

      {/* Description */}
      <div
        style={{
          fontSize: '10px',
          color: F.GRAY,
          lineHeight: '1.4',
          height: '28px',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          display: '-webkit-box',
          WebkitLineClamp: 2,
          WebkitBoxOrient: 'vertical' as const,
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
              fontSize: '8px',
              padding: '2px 4px',
              backgroundColor:
                mode === 'live' ? `${F.GREEN}20` : mode === 'paper' ? `${F.YELLOW}20` : `${F.BLUE}20`,
              color: mode === 'live' ? F.GREEN : mode === 'paper' ? F.YELLOW : F.BLUE,
              borderRadius: '2px',
              textTransform: 'uppercase',
            }}
          >
            {mode}
          </span>
        ))}
      </div>

      {/* Actions */}
      <div
        style={{
          display: 'flex',
          gap: '4px',
          marginTop: '4px',
          borderTop: `1px solid ${F.BORDER}`,
          paddingTop: '8px',
        }}
      >
        <button
          onClick={onViewCode}
          style={{
            flex: 1,
            padding: '6px',
            backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`,
            color: F.GRAY,
            borderRadius: '2px',
            fontSize: '9px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
          }}
        >
          <Eye size={10} />
          VIEW
        </button>
        {onClone && (
          <button
            onClick={onClone}
            style={{
              flex: 1,
              padding: '6px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BLUE}`,
              color: F.BLUE,
              borderRadius: '2px',
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
            }}
          >
            <Copy size={10} />
            CLONE
          </button>
        )}
        {onBacktest && (
          <button
            onClick={onBacktest}
            style={{
              flex: 1,
              padding: '6px',
              backgroundColor: F.ORANGE,
              border: 'none',
              color: F.DARK_BG,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
            }}
          >
            <Play size={10} />
            TEST
          </button>
        )}
      </div>
    </div>
  );
};

// Strategy Row Component (List View)
const StrategyRow: React.FC<{
  strategy: PythonStrategy;
  onViewCode: () => void;
  onClone?: () => void;
  onBacktest?: () => void;
}> = ({ strategy, onViewCode, onClone, onBacktest }) => {
  return (
    <div
      style={{
        backgroundColor: F.PANEL_BG,
        border: `1px solid ${F.BORDER}`,
        borderRadius: '2px',
        padding: '8px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
      }}
    >
      <Code size={14} style={{ color: F.ORANGE }} />
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>{strategy.name}</div>
        <div
          style={{
            fontSize: '9px',
            color: F.GRAY,
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          }}
        >
          {strategy.id} - {strategy.description || 'No description'}
        </div>
      </div>
      <span
        style={{
          fontSize: '8px',
          padding: '2px 6px',
          backgroundColor: F.HOVER,
          color: F.CYAN,
          borderRadius: '2px',
        }}
      >
        {strategy.category}
      </span>
      <div style={{ display: 'flex', gap: '4px' }}>
        <button
          onClick={onViewCode}
          style={{
            padding: '4px 8px',
            backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`,
            color: F.GRAY,
            borderRadius: '2px',
            fontSize: '9px',
            cursor: 'pointer',
          }}
        >
          VIEW
        </button>
        {onClone && (
          <button
            onClick={onClone}
            style={{
              padding: '4px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BLUE}`,
              color: F.BLUE,
              borderRadius: '2px',
              fontSize: '9px',
              cursor: 'pointer',
            }}
          >
            CLONE
          </button>
        )}
        {onBacktest && (
          <button
            onClick={onBacktest}
            style={{
              padding: '4px 8px',
              backgroundColor: F.ORANGE,
              border: 'none',
              color: F.DARK_BG,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
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
