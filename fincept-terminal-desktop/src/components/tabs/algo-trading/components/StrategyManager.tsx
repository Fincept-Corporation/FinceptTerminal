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
import { S } from '../constants/styles';

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
    <div className="flex flex-col h-full">
      {/* ─── HEADER TOOLBAR ─── */}
      <div
        className="flex items-center justify-between px-4 py-3 shrink-0"
        style={{ backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}
      >
        <div className="flex items-center gap-3">
          <Folder size={16} style={{ color: F.ORANGE }} />
          <span className="text-[12px] font-bold tracking-wide" style={{ color: F.WHITE }}>
            MY STRATEGIES
          </span>
          <div className="flex gap-1.5">
            <span
              className={S.badge}
              style={{ backgroundColor: `${F.CYAN}15`, color: F.CYAN }}
            >
              {jsonCount} JSON
            </span>
            <span
              className={S.badge}
              style={{ backgroundColor: `${F.PURPLE}15`, color: F.PURPLE }}
            >
              {pythonCount} PYTHON
            </span>
          </div>
        </div>

        <div className="flex gap-2 items-center">
          {/* Filter Tabs */}
          <div
            className="flex overflow-hidden rounded"
            style={{ border: `1px solid ${F.BORDER}` }}
          >
            {(['all', 'json', 'python'] as StrategyType[]).map((type) => (
              <button
                key={type}
                onClick={() => setFilterType(type)}
                className="px-3 py-1.5 text-[9px] font-bold tracking-wide cursor-pointer border-none transition-all duration-150"
                style={{
                  backgroundColor: filterType === type ? `${F.ORANGE}20` : 'transparent',
                  borderRight: type !== 'python' ? `1px solid ${F.BORDER}` : 'none',
                  color: filterType === type ? F.ORANGE : F.GRAY,
                }}
              >
                {type === 'all' ? 'ALL' : type.toUpperCase()}
              </button>
            ))}
          </div>

          <button
            onClick={loadStrategies}
            className={S.btnGhost}
            style={{ border: `1px solid ${F.BORDER}` }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
          >
            <RefreshCw size={12} />
          </button>

          <button
            onClick={onNew}
            className={S.btnPrimary}
            style={{ backgroundColor: F.ORANGE, color: F.DARK_BG }}
            onMouseEnter={e => { e.currentTarget.style.opacity = '0.85'; }}
            onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
          >
            <Plus size={12} />
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
      <div className="flex-1 overflow-auto p-4">
        {loading ? (
          <div className="flex flex-col items-center justify-center h-[200px] gap-2">
            <RefreshCw size={20} className="animate-spin" style={{ color: F.ORANGE }} />
            <span className="text-[11px] font-bold" style={{ color: F.MUTED }}>LOADING STRATEGIES...</span>
          </div>
        ) : filteredStrategies.length === 0 ? (
          <div className="flex flex-col items-center justify-center h-[300px] gap-3">
            <div
              className="w-12 h-12 rounded flex items-center justify-center"
              style={{ backgroundColor: `${F.ORANGE}10`, border: `1px solid ${F.ORANGE}20` }}
            >
              <Rocket size={22} style={{ color: F.ORANGE, opacity: 0.6 }} />
            </div>
            <span className="text-[12px] font-bold" style={{ color: F.GRAY }}>
              {filterType === 'all' ? 'NO STRATEGIES YET' : `NO ${filterType.toUpperCase()} STRATEGIES`}
            </span>
            <span className="text-[10px] max-w-[320px] text-center leading-relaxed" style={{ color: F.MUTED }}>
              {filterType === 'python'
                ? 'Clone strategies from the LIBRARY tab to create Python strategies'
                : 'Create your first strategy using the builder or write a Python strategy'}
            </span>
            {filterType !== 'python' && (
              <button
                onClick={onNew}
                className={S.btnOutline + ' mt-1'}
                style={{ borderColor: F.ORANGE, color: F.ORANGE }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.ORANGE}15`; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Plus size={11} />
                CREATE STRATEGY
              </button>
            )}
          </div>
        ) : (
          <div
            className="grid gap-3"
            style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(340px, 1fr))' }}
          >
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
          className="fixed inset-0 z-[99]"
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
      className="rounded overflow-hidden transition-colors duration-200"
      style={{
        backgroundColor: F.PANEL_BG,
        border: `1px solid ${isHovered ? `${F.ORANGE}40` : F.BORDER}`,
      }}
    >
      {/* Card Header */}
      <div
        className="flex items-center gap-2 px-4 py-3"
        style={{
          backgroundColor: F.HEADER_BG,
          borderBottom: `1px solid ${F.BORDER}`,
          borderLeft: `3px solid ${typeColor}`,
        }}
      >
        <TypeIcon size={14} style={{ color: typeColor }} />
        <div className="flex-1 min-w-0">
          <div className="text-[11px] font-bold overflow-hidden text-ellipsis whitespace-nowrap" style={{ color: F.WHITE }}>
            {strategy.name}
          </div>
        </div>
        <span
          className={S.badge}
          style={{ backgroundColor: `${typeColor}15`, color: typeColor }}
        >
          {strategy.type.toUpperCase()}
        </span>
        {/* Menu button */}
        <div className="relative">
          <button
            onClick={(e) => { e.stopPropagation(); onMenuToggle(); }}
            className={S.btnGhost}
          >
            <MoreHorizontal size={14} />
          </button>
          {menuOpen && (
            <div
              className="absolute top-full right-0 mt-1 rounded z-[100] min-w-[130px]"
              style={{
                backgroundColor: F.PANEL_BG,
                border: `1px solid ${F.BORDER}`,
                boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
              }}
            >
              <button
                onClick={onEdit}
                className="w-full flex items-center gap-2 px-4 py-2.5 bg-transparent border-none text-[10px] font-bold text-left cursor-pointer transition-colors duration-150"
                style={{ color: F.WHITE }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Edit size={11} /> EDIT
              </button>
              <button
                onClick={onDeploy}
                className="w-full flex items-center gap-2 px-4 py-2.5 bg-transparent border-none text-[10px] font-bold text-left cursor-pointer transition-colors duration-150"
                style={{ color: F.GREEN }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Rocket size={11} /> DEPLOY
              </button>
              <div style={{ borderTop: `1px solid ${F.BORDER}` }} />
              <button
                onClick={onDelete}
                className="w-full flex items-center gap-2 px-4 py-2.5 bg-transparent border-none text-[10px] font-bold text-left cursor-pointer transition-colors duration-150"
                style={{ color: F.RED }}
                onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.RED}10`; }}
                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Trash2 size={11} /> DELETE
              </button>
            </div>
          )}
        </div>
      </div>

      {/* Card Body */}
      <div className={S.cardBody}>
        {/* Description */}
        {strategy.description && (
          <div
            className="text-[10px] leading-relaxed mb-2 max-h-[32px] overflow-hidden"
            style={{
              color: F.MUTED,
              display: '-webkit-box',
              WebkitLineClamp: 2,
              WebkitBoxOrient: 'vertical' as const,
              textOverflow: 'ellipsis',
            }}
          >
            {strategy.description}
          </div>
        )}

        {/* Strategy Details */}
        {isJson ? (
          <div className="grid grid-cols-4 gap-2">
            <div className="rounded p-2" style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}` }}>
              <div className="text-[8px] font-bold tracking-wide mb-0.5" style={{ color: F.MUTED }}>TIMEFRAME</div>
              <div className="text-[11px] font-bold" style={{ color: F.CYAN }}>{strategy.timeframe || '—'}</div>
            </div>
            <div className="rounded p-2" style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}` }}>
              <div className="text-[8px] font-bold tracking-wide mb-0.5" style={{ color: F.MUTED }}>ENTRY</div>
              <div className="flex items-center gap-1 text-[11px] font-bold" style={{ color: F.GREEN }}>
                <ArrowUpRight size={10} /> {strategy.entryConditionCount}
              </div>
            </div>
            <div className="rounded p-2" style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}` }}>
              <div className="text-[8px] font-bold tracking-wide mb-0.5" style={{ color: F.MUTED }}>EXIT</div>
              <div className="flex items-center gap-1 text-[11px] font-bold" style={{ color: F.RED }}>
                <ArrowDownRight size={10} /> {strategy.exitConditionCount}
              </div>
            </div>
            <div className="rounded p-2" style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}` }}>
              <div className="text-[8px] font-bold tracking-wide mb-0.5" style={{ color: F.MUTED }}>RISK</div>
              <div className="flex items-center gap-1 text-[10px] font-bold" style={{ color: F.YELLOW }}>
                <Shield size={9} />
                {strategy.stopLoss != null ? `${strategy.stopLoss}%` : '—'}
              </div>
            </div>
          </div>
        ) : (
          <div className="grid grid-cols-2 gap-2">
            {strategy.category && (
              <div className="rounded p-2" style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}` }}>
                <div className="text-[8px] font-bold tracking-wide mb-0.5" style={{ color: F.MUTED }}>CATEGORY</div>
                <div className="text-[10px] font-bold" style={{ color: F.PURPLE }}>{strategy.category}</div>
              </div>
            )}
            {strategy.baseStrategyId && (
              <div className="rounded p-2" style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}` }}>
                <div className="text-[8px] font-bold tracking-wide mb-0.5" style={{ color: F.MUTED }}>BASE</div>
                <div className="text-[10px] overflow-hidden text-ellipsis whitespace-nowrap" style={{ color: F.GRAY }}>
                  {strategy.baseStrategyId}
                </div>
              </div>
            )}
          </div>
        )}
      </div>

      {/* Card Footer — Action Buttons */}
      <div
        className="flex items-center justify-between px-4 py-2.5"
        style={{ borderTop: `1px solid ${F.BORDER}` }}
      >
        <div className="flex items-center gap-2">
          {strategy.updatedAt && (
            <span className="flex items-center gap-1 text-[9px]" style={{ color: F.MUTED }}>
              <Calendar size={9} />
              {new Date(strategy.updatedAt).toLocaleDateString()}
            </span>
          )}
        </div>
        <div className="flex gap-1.5">
          <button
            onClick={onEdit}
            className={S.btnOutline}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
          >
            <Edit size={10} /> EDIT
          </button>
          <button
            onClick={onDeploy}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded text-[10px] font-bold tracking-wide cursor-pointer transition-all duration-200"
            style={{
              backgroundColor: `${F.GREEN}15`,
              border: `1px solid ${F.GREEN}30`,
              color: F.GREEN,
            }}
            onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.GREEN}25`; }}
            onMouseLeave={e => { e.currentTarget.style.backgroundColor = `${F.GREEN}15`; }}
          >
            <Rocket size={10} /> DEPLOY
          </button>
        </div>
      </div>
    </div>
  );
};

export default StrategyManager;
