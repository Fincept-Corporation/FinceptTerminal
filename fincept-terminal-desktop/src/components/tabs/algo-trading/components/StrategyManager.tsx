import React, { useState, useEffect } from 'react';
import { Plus, Edit, Trash2, Rocket, RefreshCw } from 'lucide-react';
import type { AlgoStrategy } from '../types';
import { listAlgoStrategies, deleteAlgoStrategy } from '../services/algoTradingService';
import DeployPanel from './DeployPanel';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', HOVER: '#1F1F1F',
  MUTED: '#4A4A4A', CYAN: '#00E5FF',
};

interface StrategyManagerProps {
  onEdit: (id: string) => void;
  onNew: () => void;
}

const StrategyManager: React.FC<StrategyManagerProps> = ({ onEdit, onNew }) => {
  const [strategies, setStrategies] = useState<AlgoStrategy[]>([]);
  const [loading, setLoading] = useState(true);
  const [deployStrategyId, setDeployStrategyId] = useState<string | null>(null);
  const [hoveredId, setHoveredId] = useState<string | null>(null);

  const loadStrategies = async () => {
    setLoading(true);
    const result = await listAlgoStrategies();
    if (result.success && result.data) setStrategies(result.data);
    setLoading(false);
  };

  useEffect(() => { loadStrategies(); }, []);

  const handleDelete = async (id: string) => {
    const result = await deleteAlgoStrategy(id);
    if (result.success) setStrategies(prev => prev.filter(s => s.id !== id));
  };

  const parseConditionCount = (json: string): number => {
    try { return JSON.parse(json).filter((c: unknown) => typeof c === 'object').length; }
    catch { return 0; }
  };

  return (
    <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Section Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>SAVED STRATEGIES</span>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button onClick={loadStrategies} style={{
            padding: '6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
            color: F.GRAY, cursor: 'pointer', borderRadius: '2px', transition: 'all 0.2s',
          }}>
            <RefreshCw size={12} />
          </button>
          <button onClick={onNew} style={{
            padding: '8px 16px', backgroundColor: F.ORANGE, color: F.DARK_BG, border: 'none',
            borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <Plus size={10} />
            NEW STRATEGY
          </button>
        </div>
      </div>

      {/* Deploy Panel */}
      {deployStrategyId && (
        <DeployPanel
          strategyId={deployStrategyId}
          strategyName={strategies.find(s => s.id === deployStrategyId)?.name || ''}
          onClose={() => setDeployStrategyId(null)}
        />
      )}

      {/* List */}
      {loading ? (
        <div style={{ textAlign: 'center', padding: '32px', fontSize: '10px', color: F.MUTED }}>Loading strategies...</div>
      ) : strategies.length === 0 ? (
        <div style={{
          display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
          height: '200px', color: F.MUTED, fontSize: '10px', textAlign: 'center',
        }}>
          <Rocket size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
          <span>No strategies yet</span>
          <button onClick={onNew} style={{
            marginTop: '8px', padding: '6px 12px', backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`, color: F.ORANGE, fontSize: '9px',
            fontWeight: 700, borderRadius: '2px', cursor: 'pointer',
          }}>CREATE YOUR FIRST STRATEGY</button>
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          {strategies.map(strategy => (
            <div
              key={strategy.id}
              onMouseEnter={() => setHoveredId(strategy.id)}
              onMouseLeave={() => setHoveredId(null)}
              style={{
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                padding: '10px 12px',
                backgroundColor: hoveredId === strategy.id ? `${F.ORANGE}15` : F.PANEL_BG,
                borderLeft: hoveredId === strategy.id ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                border: `1px solid ${F.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                transition: 'all 0.2s',
              }}
            >
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {strategy.name}
                </div>
                {strategy.description && (
                  <div style={{ fontSize: '9px', color: F.MUTED, marginTop: '2px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {strategy.description}
                  </div>
                )}
                <div style={{ display: 'flex', gap: '12px', marginTop: '4px', fontSize: '9px', color: F.GRAY }}>
                  <span>TF: <span style={{ color: F.CYAN }}>{strategy.timeframe}</span></span>
                  <span>ENTRY: <span style={{ color: F.CYAN }}>{parseConditionCount(strategy.entry_conditions)}</span></span>
                  <span>EXIT: <span style={{ color: F.CYAN }}>{parseConditionCount(strategy.exit_conditions)}</span></span>
                  {strategy.stop_loss != null && <span>SL: <span style={{ color: F.RED }}>{strategy.stop_loss}%</span></span>}
                  {strategy.take_profit != null && <span>TP: <span style={{ color: F.GREEN }}>{strategy.take_profit}%</span></span>}
                </div>
              </div>
              <div style={{ display: 'flex', gap: '4px', marginLeft: '12px' }}>
                <button onClick={() => setDeployStrategyId(strategy.id)} style={{
                  padding: '4px 8px', backgroundColor: `${F.GREEN}20`, color: F.GREEN, border: 'none',
                  borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: 'pointer',
                  display: 'flex', alignItems: 'center', gap: '3px', transition: 'all 0.2s',
                }}>
                  <Rocket size={10} /> DEPLOY
                </button>
                <button onClick={() => onEdit(strategy.id)} style={{
                  padding: '4px 6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                  color: F.GRAY, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s',
                }}>
                  <Edit size={11} />
                </button>
                <button onClick={() => handleDelete(strategy.id)} style={{
                  padding: '4px 6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                  color: F.GRAY, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s',
                }}
                onMouseEnter={e => { (e.currentTarget as HTMLElement).style.color = F.RED; (e.currentTarget as HTMLElement).style.borderColor = F.RED; }}
                onMouseLeave={e => { (e.currentTarget as HTMLElement).style.color = F.GRAY; (e.currentTarget as HTMLElement).style.borderColor = F.BORDER; }}
                >
                  <Trash2 size={11} />
                </button>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default StrategyManager;
