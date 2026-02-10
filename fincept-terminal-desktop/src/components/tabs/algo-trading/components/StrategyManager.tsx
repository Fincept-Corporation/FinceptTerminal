import React, { useState, useEffect } from 'react';
import { Plus, Edit, Trash2, Rocket, RefreshCw, Code, Settings, ChevronDown, FileCode, Filter } from 'lucide-react';
import type { AlgoStrategy, CustomPythonStrategy, PythonStrategy } from '../types';
import {
  listAlgoStrategies,
  deleteAlgoStrategy,
  listCustomPythonStrategies,
  deleteCustomPythonStrategy,
} from '../services/algoTradingService';
import DeployPanel from './DeployPanel';
import PythonStrategyEditor from './PythonStrategyEditor';

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
  PURPLE: '#9D4EDD',
  YELLOW: '#FFD700',
};

type StrategyType = 'all' | 'json' | 'python';

interface StrategyManagerProps {
  onEdit: (id: string) => void;
  onNew: () => void;
  onBacktestPython?: (strategy: PythonStrategy) => void;
  onDeployPython?: (strategy: PythonStrategy) => void;
}

// Unified strategy item for display
interface UnifiedStrategy {
  id: string;
  name: string;
  description?: string;
  type: 'json' | 'python';
  category?: string;
  // JSON strategy specifics
  timeframe?: string;
  entryConditionCount?: number;
  exitConditionCount?: number;
  stopLoss?: number | null;
  takeProfit?: number | null;
  // Python strategy specifics
  baseStrategyId?: string;
  updatedAt?: string;
  // Original data
  originalJson?: AlgoStrategy;
  originalPython?: CustomPythonStrategy;
}

const StrategyManager: React.FC<StrategyManagerProps> = ({
  onEdit,
  onNew,
  onBacktestPython,
  onDeployPython,
}) => {
  const [strategies, setStrategies] = useState<UnifiedStrategy[]>([]);
  const [loading, setLoading] = useState(true);
  const [deployStrategyId, setDeployStrategyId] = useState<string | null>(null);
  const [deployStrategyType, setDeployStrategyType] = useState<'json' | 'python'>('json');
  const [hoveredId, setHoveredId] = useState<string | null>(null);
  const [filterType, setFilterType] = useState<StrategyType>('all');
  const [showFilterDropdown, setShowFilterDropdown] = useState(false);
  const [editingPythonStrategy, setEditingPythonStrategy] = useState<CustomPythonStrategy | null>(null);

  const loadStrategies = async () => {
    setLoading(true);
    const unified: UnifiedStrategy[] = [];

    // Load JSON strategies
    const jsonResult = await listAlgoStrategies();
    if (jsonResult.success && jsonResult.data) {
      jsonResult.data.forEach((s) => {
        unified.push({
          id: s.id,
          name: s.name,
          description: s.description,
          type: 'json',
          timeframe: s.timeframe,
          entryConditionCount: parseConditionCount(s.entry_conditions),
          exitConditionCount: parseConditionCount(s.exit_conditions),
          stopLoss: s.stop_loss,
          takeProfit: s.take_profit,
          originalJson: s,
        });
      });
    }

    // Load custom Python strategies
    const pythonResult = await listCustomPythonStrategies();
    if (pythonResult.success && pythonResult.data) {
      pythonResult.data.forEach((s) => {
        unified.push({
          id: s.id,
          name: s.name,
          description: s.description,
          type: 'python',
          category: s.category,
          baseStrategyId: s.base_strategy_id,
          updatedAt: s.updated_at,
          originalPython: s,
        });
      });
    }

    // Sort by updated_at or created_at (most recent first)
    unified.sort((a, b) => {
      const aDate = a.originalPython?.updated_at || a.originalJson?.updated_at || '';
      const bDate = b.originalPython?.updated_at || b.originalJson?.updated_at || '';
      return bDate.localeCompare(aDate);
    });

    setStrategies(unified);
    setLoading(false);
  };

  useEffect(() => {
    loadStrategies();
  }, []);

  const parseConditionCount = (json: string): number => {
    try {
      return JSON.parse(json).filter((c: unknown) => typeof c === 'object').length;
    } catch {
      return 0;
    }
  };

  const handleDelete = async (strategy: UnifiedStrategy) => {
    if (strategy.type === 'json') {
      const result = await deleteAlgoStrategy(strategy.id);
      if (result.success) {
        setStrategies((prev) => prev.filter((s) => s.id !== strategy.id));
      }
    } else {
      const result = await deleteCustomPythonStrategy(strategy.id);
      if (result.success) {
        setStrategies((prev) => prev.filter((s) => s.id !== strategy.id));
      }
    }
  };

  const handleDeploy = (strategy: UnifiedStrategy) => {
    if (strategy.type === 'python' && onDeployPython && strategy.originalPython) {
      onDeployPython(strategy.originalPython);
    } else {
      setDeployStrategyId(strategy.id);
      setDeployStrategyType(strategy.type);
    }
  };

  const handleEdit = (strategy: UnifiedStrategy) => {
    if (strategy.type === 'json') {
      onEdit(strategy.id);
    } else if (strategy.originalPython) {
      setEditingPythonStrategy(strategy.originalPython);
    }
  };

  const handlePythonEditorClose = () => {
    setEditingPythonStrategy(null);
    loadStrategies(); // Refresh list
  };

  const filteredStrategies = strategies.filter((s) => {
    if (filterType === 'all') return true;
    return s.type === filterType;
  });

  const jsonCount = strategies.filter((s) => s.type === 'json').length;
  const pythonCount = strategies.filter((s) => s.type === 'python').length;

  return (
    <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Section Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
            MY STRATEGIES
          </span>
          <div style={{ display: 'flex', gap: '4px', fontSize: '9px' }}>
            <span style={{
              padding: '2px 6px',
              backgroundColor: `${F.CYAN}20`,
              color: F.CYAN,
              borderRadius: '2px',
            }}>
              {jsonCount} JSON
            </span>
            <span style={{
              padding: '2px 6px',
              backgroundColor: `${F.PURPLE}20`,
              color: F.PURPLE,
              borderRadius: '2px',
            }}>
              {pythonCount} Python
            </span>
          </div>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          {/* Filter Dropdown */}
          <div style={{ position: 'relative' }}>
            <button
              onClick={() => setShowFilterDropdown(!showFilterDropdown)}
              style={{
                padding: '6px 8px',
                backgroundColor: 'transparent',
                border: `1px solid ${F.BORDER}`,
                color: F.GRAY,
                cursor: 'pointer',
                borderRadius: '2px',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                fontSize: '9px',
              }}
            >
              <Filter size={10} />
              {filterType === 'all' ? 'ALL' : filterType.toUpperCase()}
              <ChevronDown size={10} />
            </button>
            {showFilterDropdown && (
              <div style={{
                position: 'absolute',
                top: '100%',
                right: 0,
                marginTop: '4px',
                backgroundColor: F.PANEL_BG,
                border: `1px solid ${F.BORDER}`,
                borderRadius: '2px',
                zIndex: 100,
                minWidth: '100px',
              }}>
                {(['all', 'json', 'python'] as StrategyType[]).map((type) => (
                  <button
                    key={type}
                    onClick={() => {
                      setFilterType(type);
                      setShowFilterDropdown(false);
                    }}
                    style={{
                      width: '100%',
                      padding: '6px 10px',
                      backgroundColor: filterType === type ? `${F.ORANGE}20` : 'transparent',
                      border: 'none',
                      color: filterType === type ? F.ORANGE : F.WHITE,
                      fontSize: '9px',
                      textAlign: 'left',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                    }}
                  >
                    {type === 'all' && 'All Strategies'}
                    {type === 'json' && (
                      <>
                        <Settings size={10} style={{ color: F.CYAN }} />
                        JSON Strategies
                      </>
                    )}
                    {type === 'python' && (
                      <>
                        <Code size={10} style={{ color: F.PURPLE }} />
                        Python Strategies
                      </>
                    )}
                  </button>
                ))}
              </div>
            )}
          </div>

          <button
            onClick={loadStrategies}
            style={{
              padding: '6px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`,
              color: F.GRAY,
              cursor: 'pointer',
              borderRadius: '2px',
              transition: 'all 0.2s',
            }}
          >
            <RefreshCw size={12} />
          </button>
          <button
            onClick={onNew}
            style={{
              padding: '8px 16px',
              backgroundColor: F.ORANGE,
              color: F.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <Plus size={10} />
            NEW STRATEGY
          </button>
        </div>
      </div>

      {/* Deploy Panel */}
      {deployStrategyId && deployStrategyType === 'json' && (
        <DeployPanel
          strategyId={deployStrategyId}
          strategyName={strategies.find((s) => s.id === deployStrategyId)?.name || ''}
          onClose={() => setDeployStrategyId(null)}
        />
      )}

      {/* Python Strategy Editor */}
      {editingPythonStrategy && (
        <PythonStrategyEditor
          strategy={editingPythonStrategy}
          isCustom={true}
          onClose={handlePythonEditorClose}
          onSave={() => {
            handlePythonEditorClose();
          }}
          onBacktest={onBacktestPython}
        />
      )}

      {/* List */}
      {loading ? (
        <div style={{ textAlign: 'center', padding: '32px', fontSize: '10px', color: F.MUTED }}>
          Loading strategies...
        </div>
      ) : filteredStrategies.length === 0 ? (
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: F.MUTED,
            fontSize: '10px',
            textAlign: 'center',
          }}
        >
          <Rocket size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
          <span>
            {filterType === 'all'
              ? 'No strategies yet'
              : `No ${filterType} strategies yet`}
          </span>
          {filterType === 'all' && (
            <button
              onClick={onNew}
              style={{
                marginTop: '8px',
                padding: '6px 12px',
                backgroundColor: 'transparent',
                border: `1px solid ${F.BORDER}`,
                color: F.ORANGE,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
              }}
            >
              CREATE YOUR FIRST STRATEGY
            </button>
          )}
          {filterType === 'python' && (
            <div style={{ marginTop: '8px', color: F.GRAY, fontSize: '9px' }}>
              Clone strategies from the LIBRARY tab to create Python strategies
            </div>
          )}
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          {filteredStrategies.map((strategy) => (
            <div
              key={strategy.id}
              onMouseEnter={() => setHoveredId(strategy.id)}
              onMouseLeave={() => setHoveredId(null)}
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                padding: '10px 12px',
                backgroundColor: hoveredId === strategy.id ? `${F.ORANGE}15` : F.PANEL_BG,
                borderLeft:
                  hoveredId === strategy.id ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                border: `1px solid ${F.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                transition: 'all 0.2s',
              }}
            >
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  {/* Type indicator */}
                  {strategy.type === 'json' ? (
                    <Settings size={12} style={{ color: F.CYAN, flexShrink: 0 }} />
                  ) : (
                    <FileCode size={12} style={{ color: F.PURPLE, flexShrink: 0 }} />
                  )}
                  <div
                    style={{
                      fontSize: '10px',
                      fontWeight: 700,
                      color: F.WHITE,
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                    }}
                  >
                    {strategy.name}
                  </div>
                  {/* Type badge */}
                  <span
                    style={{
                      fontSize: '8px',
                      padding: '2px 4px',
                      backgroundColor: strategy.type === 'json' ? `${F.CYAN}20` : `${F.PURPLE}20`,
                      color: strategy.type === 'json' ? F.CYAN : F.PURPLE,
                      borderRadius: '2px',
                      flexShrink: 0,
                    }}
                  >
                    {strategy.type.toUpperCase()}
                  </span>
                </div>
                {strategy.description && (
                  <div
                    style={{
                      fontSize: '9px',
                      color: F.MUTED,
                      marginTop: '2px',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                      marginLeft: '20px',
                    }}
                  >
                    {strategy.description}
                  </div>
                )}
                <div
                  style={{
                    display: 'flex',
                    gap: '12px',
                    marginTop: '4px',
                    fontSize: '9px',
                    color: F.GRAY,
                    marginLeft: '20px',
                  }}
                >
                  {/* JSON strategy details */}
                  {strategy.type === 'json' && (
                    <>
                      <span>
                        TF: <span style={{ color: F.CYAN }}>{strategy.timeframe}</span>
                      </span>
                      <span>
                        ENTRY: <span style={{ color: F.CYAN }}>{strategy.entryConditionCount}</span>
                      </span>
                      <span>
                        EXIT: <span style={{ color: F.CYAN }}>{strategy.exitConditionCount}</span>
                      </span>
                      {strategy.stopLoss != null && (
                        <span>
                          SL: <span style={{ color: F.RED }}>{strategy.stopLoss}%</span>
                        </span>
                      )}
                      {strategy.takeProfit != null && (
                        <span>
                          TP: <span style={{ color: F.GREEN }}>{strategy.takeProfit}%</span>
                        </span>
                      )}
                    </>
                  )}
                  {/* Python strategy details */}
                  {strategy.type === 'python' && (
                    <>
                      {strategy.category && (
                        <span>
                          Category: <span style={{ color: F.PURPLE }}>{strategy.category}</span>
                        </span>
                      )}
                      {strategy.baseStrategyId && (
                        <span>
                          Base: <span style={{ color: F.MUTED }}>{strategy.baseStrategyId}</span>
                        </span>
                      )}
                    </>
                  )}
                </div>
              </div>
              <div style={{ display: 'flex', gap: '4px', marginLeft: '12px' }}>
                <button
                  onClick={() => handleDeploy(strategy)}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: `${F.GREEN}20`,
                    color: F.GREEN,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '8px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '3px',
                    transition: 'all 0.2s',
                  }}
                >
                  <Rocket size={10} /> DEPLOY
                </button>
                <button
                  onClick={() => handleEdit(strategy)}
                  style={{
                    padding: '4px 6px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${F.BORDER}`,
                    color: F.GRAY,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                  }}
                >
                  <Edit size={11} />
                </button>
                <button
                  onClick={() => handleDelete(strategy)}
                  style={{
                    padding: '4px 6px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${F.BORDER}`,
                    color: F.GRAY,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    (e.currentTarget as HTMLElement).style.color = F.RED;
                    (e.currentTarget as HTMLElement).style.borderColor = F.RED;
                  }}
                  onMouseLeave={(e) => {
                    (e.currentTarget as HTMLElement).style.color = F.GRAY;
                    (e.currentTarget as HTMLElement).style.borderColor = F.BORDER;
                  }}
                >
                  <Trash2 size={11} />
                </button>
              </div>
            </div>
          ))}
        </div>
      )}

      {/* Click outside to close dropdown */}
      {showFilterDropdown && (
        <div
          style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            zIndex: 99,
          }}
          onClick={() => setShowFilterDropdown(false)}
        />
      )}
    </div>
  );
};

export default StrategyManager;
