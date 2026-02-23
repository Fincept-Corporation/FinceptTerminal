import React, { useState, useEffect } from 'react';
import {
  Plus, Edit, Trash2, Rocket, RefreshCw, Code, Settings, ChevronDown,
  FileCode, Filter, Folder, Calendar, ArrowUpRight, ArrowDownRight,
  Shield, Zap, MoreHorizontal,
} from 'lucide-react';
import type { AlgoStrategy, CustomPythonStrategy, PythonStrategy } from '../types';
import {
  listAlgoStrategies,
  deleteAlgoStrategy,
  listCustomPythonStrategies,
  deleteCustomPythonStrategy,
} from '../services/algoTradingService';
import DeployPanel from './DeployPanel';
import PythonStrategyEditor from './PythonStrategyEditor';
import { F } from '../constants/theme';

type StrategyType = 'all' | 'json' | 'python';

interface StrategyManagerProps {
  onEdit: (id: string) => void;
  onNew: () => void;
  onBacktestPython?: (strategy: PythonStrategy) => void;
  onDeployPython?: (strategy: PythonStrategy) => void;
  onDeployedJson?: (deployId: string) => void;
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
  onDeployedJson,
}) => {
  const [strategies, setStrategies] = useState<UnifiedStrategy[]>([]);
  const [loading, setLoading] = useState(true);
  const [deployStrategyId, setDeployStrategyId] = useState<string | null>(null);
  const [deployStrategyType, setDeployStrategyType] = useState<'json' | 'python'>('json');
  const [hoveredId, setHoveredId] = useState<string | null>(null);
  const [filterType, setFilterType] = useState<StrategyType>('all');
  const [showFilterDropdown, setShowFilterDropdown] = useState(false);
  const [editingPythonStrategy, setEditingPythonStrategy] = useState<CustomPythonStrategy | null>(null);
  const [menuOpenId, setMenuOpenId] = useState<string | null>(null);

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
    setMenuOpenId(null);
  };

  const handleDeploy = (strategy: UnifiedStrategy) => {
    if (strategy.type === 'python' && onDeployPython && strategy.originalPython) {
      onDeployPython(strategy.originalPython);
    } else {
      setDeployStrategyId(strategy.id);
      setDeployStrategyType(strategy.type);
    }
    setMenuOpenId(null);
  };

  const handleEdit = (strategy: UnifiedStrategy) => {
    if (strategy.type === 'json') {
      onEdit(strategy.id);
    } else if (strategy.originalPython) {
      setEditingPythonStrategy(strategy.originalPython);
    }
    setMenuOpenId(null);
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
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* ─── HEADER TOOLBAR ─── */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Folder size={14} color={F.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
            MY STRATEGIES
          </span>
          <div style={{ display: 'flex', gap: '4px' }}>
            <span style={{
              padding: '2px 8px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              backgroundColor: `${F.CYAN}15`, color: F.CYAN,
            }}>
              {jsonCount} JSON
            </span>
            <span style={{
              padding: '2px 8px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              backgroundColor: `${F.PURPLE}15`, color: F.PURPLE,
            }}>
              {pythonCount} PYTHON
            </span>
          </div>
        </div>

        <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
          {/* Filter Tabs */}
          <div style={{
            display: 'flex', gap: '0px',
            border: `1px solid ${F.BORDER}`, borderRadius: '2px', overflow: 'hidden',
          }}>
            {(['all', 'json', 'python'] as StrategyType[]).map((type) => (
              <button
                key={type}
                onClick={() => setFilterType(type)}
                style={{
                  padding: '5px 10px',
                  backgroundColor: filterType === type ? `${F.ORANGE}20` : 'transparent',
                  border: 'none',
                  borderRight: type !== 'python' ? `1px solid ${F.BORDER}` : 'none',
                  color: filterType === type ? F.ORANGE : F.GRAY,
                  fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
                  cursor: 'pointer', transition: 'all 0.15s',
                }}
              >
                {type === 'all' ? 'ALL' : type.toUpperCase()}
              </button>
            ))}
          </div>

          <button
            onClick={loadStrategies}
            style={{
              padding: '6px', backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`, color: F.GRAY,
              cursor: 'pointer', borderRadius: '2px', transition: 'all 0.2s',
              display: 'flex', alignItems: 'center',
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
          >
            <RefreshCw size={11} />
          </button>

          <button
            onClick={onNew}
            style={{
              padding: '6px 14px', backgroundColor: F.ORANGE, color: F.DARK_BG,
              border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
              letterSpacing: '0.5px', transition: 'opacity 0.2s',
            }}
            onMouseEnter={e => { e.currentTarget.style.opacity = '0.85'; }}
            onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
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
          onDeployed={(deployId) => {
            setDeployStrategyId(null);
            onDeployedJson?.(deployId);
          }}
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

      {/* ─── STRATEGY CARDS ─── */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {loading ? (
          <div style={{
            display: 'flex', flexDirection: 'column', alignItems: 'center',
            justifyContent: 'center', height: '200px', gap: '8px',
          }}>
            <RefreshCw size={18} color={F.ORANGE} style={{ animation: 'spin 1s linear infinite' }} />
            <span style={{ fontSize: '10px', color: F.MUTED, fontWeight: 700 }}>LOADING STRATEGIES...</span>
            <style>{`@keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
          </div>
        ) : filteredStrategies.length === 0 ? (
          <div style={{
            display: 'flex', flexDirection: 'column', alignItems: 'center',
            justifyContent: 'center', height: '300px', gap: '12px',
          }}>
            <div style={{
              width: '48px', height: '48px', borderRadius: '2px',
              backgroundColor: `${F.ORANGE}10`, border: `1px solid ${F.ORANGE}20`,
              display: 'flex', alignItems: 'center', justifyContent: 'center',
            }}>
              <Rocket size={20} color={F.ORANGE} style={{ opacity: 0.6 }} />
            </div>
            <span style={{ fontSize: '11px', fontWeight: 700, color: F.GRAY }}>
              {filterType === 'all' ? 'NO STRATEGIES YET' : `NO ${filterType.toUpperCase()} STRATEGIES`}
            </span>
            <span style={{ fontSize: '9px', color: F.MUTED, maxWidth: '300px', textAlign: 'center', lineHeight: '1.5' }}>
              {filterType === 'python'
                ? 'Clone strategies from the LIBRARY tab to create Python strategies'
                : 'Create your first strategy using the builder or write a Python strategy'}
            </span>
            {filterType !== 'python' && (
              <button
                onClick={onNew}
                style={{
                  marginTop: '4px', padding: '8px 16px',
                  backgroundColor: 'transparent', border: `1px solid ${F.ORANGE}`,
                  color: F.ORANGE, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                  borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s',
                  display: 'flex', alignItems: 'center', gap: '6px',
                }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.ORANGE}15`; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Plus size={10} />
                CREATE STRATEGY
              </button>
            )}
          </div>
        ) : (
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fill, minmax(320px, 1fr))',
            gap: '10px',
          }}>
            {filteredStrategies.map((strategy) => (
              <StrategyCard
                key={strategy.id}
                strategy={strategy}
                isHovered={hoveredId === strategy.id}
                menuOpen={menuOpenId === strategy.id}
                onMouseEnter={() => setHoveredId(strategy.id)}
                onMouseLeave={() => setHoveredId(null)}
                onEdit={() => handleEdit(strategy)}
                onDeploy={() => handleDeploy(strategy)}
                onDelete={() => handleDelete(strategy)}
                onMenuToggle={() => setMenuOpenId(menuOpenId === strategy.id ? null : strategy.id)}
              />
            ))}
          </div>
        )}
      </div>

      {/* Click outside to close menu */}
      {menuOpenId && (
        <div
          style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, zIndex: 99 }}
          onClick={() => setMenuOpenId(null)}
        />
      )}
    </div>
  );
};

// ── Strategy Card Component ──────────────────────────────────────────────
const StrategyCard: React.FC<{
  strategy: UnifiedStrategy;
  isHovered: boolean;
  menuOpen: boolean;
  onMouseEnter: () => void;
  onMouseLeave: () => void;
  onEdit: () => void;
  onDeploy: () => void;
  onDelete: () => void;
  onMenuToggle: () => void;
}> = ({ strategy, isHovered, menuOpen, onMouseEnter, onMouseLeave, onEdit, onDeploy, onDelete, onMenuToggle }) => {
  const isJson = strategy.type === 'json';
  const typeColor = isJson ? F.CYAN : F.PURPLE;
  const TypeIcon = isJson ? Settings : FileCode;

  return (
    <div
      onMouseEnter={onMouseEnter}
      onMouseLeave={onMouseLeave}
      style={{
        backgroundColor: F.PANEL_BG,
        border: `1px solid ${isHovered ? `${F.ORANGE}40` : F.BORDER}`,
        borderRadius: '2px',
        overflow: 'hidden',
        transition: 'border-color 0.2s',
      }}
    >
      {/* Card Header */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        borderLeft: `3px solid ${typeColor}`,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
      }}>
        <TypeIcon size={12} color={typeColor} />
        <div style={{ flex: 1, minWidth: 0 }}>
          <div style={{
            fontSize: '10px', fontWeight: 700, color: F.WHITE,
            overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
          }}>
            {strategy.name}
          </div>
        </div>
        <span style={{
          padding: '2px 6px', borderRadius: '2px', fontSize: '7px', fontWeight: 700,
          backgroundColor: `${typeColor}15`, color: typeColor, letterSpacing: '0.5px',
        }}>
          {strategy.type.toUpperCase()}
        </span>
        {/* Menu button */}
        <div style={{ position: 'relative' }}>
          <button
            onClick={(e) => { e.stopPropagation(); onMenuToggle(); }}
            style={{
              padding: '3px', backgroundColor: 'transparent', border: 'none',
              color: F.MUTED, cursor: 'pointer', borderRadius: '2px',
              display: 'flex', alignItems: 'center',
            }}
          >
            <MoreHorizontal size={12} />
          </button>
          {menuOpen && (
            <div style={{
              position: 'absolute', top: '100%', right: 0, marginTop: '4px',
              backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`,
              borderRadius: '2px', zIndex: 100, minWidth: '120px',
              boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
            }}>
              <button onClick={onEdit} style={{
                width: '100%', padding: '8px 12px', backgroundColor: 'transparent',
                border: 'none', color: F.WHITE, fontSize: '9px', fontWeight: 700,
                textAlign: 'left', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '6px',
                transition: 'background-color 0.15s',
              }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Edit size={10} /> EDIT
              </button>
              <button onClick={onDeploy} style={{
                width: '100%', padding: '8px 12px', backgroundColor: 'transparent',
                border: 'none', color: F.GREEN, fontSize: '9px', fontWeight: 700,
                textAlign: 'left', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '6px',
                transition: 'background-color 0.15s',
              }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Rocket size={10} /> DEPLOY
              </button>
              <div style={{ borderTop: `1px solid ${F.BORDER}` }} />
              <button onClick={onDelete} style={{
                width: '100%', padding: '8px 12px', backgroundColor: 'transparent',
                border: 'none', color: F.RED, fontSize: '9px', fontWeight: 700,
                textAlign: 'left', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '6px',
                transition: 'background-color 0.15s',
              }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.RED}10`; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Trash2 size={10} /> DELETE
              </button>
            </div>
          )}
        </div>
      </div>

      {/* Card Body */}
      <div style={{ padding: '10px 12px' }}>
        {/* Description */}
        {strategy.description && (
          <div style={{
            fontSize: '9px', color: F.MUTED, lineHeight: '1.5',
            marginBottom: '8px', maxHeight: '28px', overflow: 'hidden',
            textOverflow: 'ellipsis', display: '-webkit-box',
            WebkitLineClamp: 2, WebkitBoxOrient: 'vertical' as const,
          }}>
            {strategy.description}
          </div>
        )}

        {/* Strategy Details */}
        {isJson ? (
          <div style={{
            display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px',
          }}>
            <div style={{ backgroundColor: F.DARK_BG, padding: '6px 8px', borderRadius: '2px', border: `1px solid ${F.BORDER}` }}>
              <div style={{ fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px' }}>TIMEFRAME</div>
              <div style={{ fontSize: '10px', fontWeight: 700, color: F.CYAN }}>{strategy.timeframe || '—'}</div>
            </div>
            <div style={{ backgroundColor: F.DARK_BG, padding: '6px 8px', borderRadius: '2px', border: `1px solid ${F.BORDER}` }}>
              <div style={{ fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px' }}>ENTRY</div>
              <div style={{ fontSize: '10px', fontWeight: 700, color: F.GREEN, display: 'flex', alignItems: 'center', gap: '3px' }}>
                <ArrowUpRight size={9} /> {strategy.entryConditionCount}
              </div>
            </div>
            <div style={{ backgroundColor: F.DARK_BG, padding: '6px 8px', borderRadius: '2px', border: `1px solid ${F.BORDER}` }}>
              <div style={{ fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px' }}>EXIT</div>
              <div style={{ fontSize: '10px', fontWeight: 700, color: F.RED, display: 'flex', alignItems: 'center', gap: '3px' }}>
                <ArrowDownRight size={9} /> {strategy.exitConditionCount}
              </div>
            </div>
            <div style={{ backgroundColor: F.DARK_BG, padding: '6px 8px', borderRadius: '2px', border: `1px solid ${F.BORDER}` }}>
              <div style={{ fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px' }}>RISK</div>
              <div style={{ fontSize: '9px', fontWeight: 700, color: F.YELLOW, display: 'flex', alignItems: 'center', gap: '2px' }}>
                <Shield size={8} />
                {strategy.stopLoss != null ? `${strategy.stopLoss}%` : '—'}
              </div>
            </div>
          </div>
        ) : (
          <div style={{
            display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px',
          }}>
            {strategy.category && (
              <div style={{ backgroundColor: F.DARK_BG, padding: '6px 8px', borderRadius: '2px', border: `1px solid ${F.BORDER}` }}>
                <div style={{ fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px' }}>CATEGORY</div>
                <div style={{ fontSize: '9px', fontWeight: 700, color: F.PURPLE }}>{strategy.category}</div>
              </div>
            )}
            {strategy.baseStrategyId && (
              <div style={{ backgroundColor: F.DARK_BG, padding: '6px 8px', borderRadius: '2px', border: `1px solid ${F.BORDER}` }}>
                <div style={{ fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px' }}>BASE</div>
                <div style={{
                  fontSize: '9px', color: F.GRAY, overflow: 'hidden',
                  textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                }}>
                  {strategy.baseStrategyId}
                </div>
              </div>
            )}
          </div>
        )}
      </div>

      {/* Card Footer — Action Buttons */}
      <div style={{
        padding: '8px 12px',
        borderTop: `1px solid ${F.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          {strategy.updatedAt && (
            <span style={{ fontSize: '8px', color: F.MUTED, display: 'flex', alignItems: 'center', gap: '3px' }}>
              <Calendar size={8} />
              {new Date(strategy.updatedAt).toLocaleDateString()}
            </span>
          )}
        </div>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={onEdit}
            style={{
              padding: '5px 10px', backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`, color: F.GRAY,
              borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '3px',
              transition: 'all 0.2s', letterSpacing: '0.5px',
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
          >
            <Edit size={9} /> EDIT
          </button>
          <button
            onClick={onDeploy}
            style={{
              padding: '5px 10px', backgroundColor: `${F.GREEN}15`,
              border: `1px solid ${F.GREEN}30`, color: F.GREEN,
              borderRadius: '2px', fontSize: '8px', fontWeight: 700,
              cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '3px',
              transition: 'all 0.2s', letterSpacing: '0.5px',
            }}
            onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.GREEN}25`; }}
            onMouseLeave={e => { e.currentTarget.style.backgroundColor = `${F.GREEN}15`; }}
          >
            <Rocket size={9} /> DEPLOY
          </button>
        </div>
      </div>
    </div>
  );
};

export default StrategyManager;
