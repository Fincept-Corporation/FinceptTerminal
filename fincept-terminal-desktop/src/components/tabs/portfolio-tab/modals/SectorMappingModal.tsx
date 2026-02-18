import React, { useState, useMemo, useCallback } from 'react';
import { X, Plus, Trash2, Pencil, Check } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { sectorService } from '../../../../services/data-sources/sectorService';
import type { HoldingWithQuote } from '../../../../services/portfolio/portfolioService';

interface SectorMappingModalProps {
  show: boolean;
  holdings: HoldingWithQuote[];
  onClose: () => void;
  onMappingsChanged: () => void;
}

const CYAN = '#00BCD4';
const ORANGE = '#FF6B35';
const GREEN = '#4CAF50';
const RED = '#F44336';
const BORDER = '#2A2A2A';
const DARK_BG = '#0A0A0A';
const PANEL = '#141414';
const GRAY = '#787878';
const WHITE = '#E8E8E8';
const HOVER = '#1F1F1F';

const COMMON_SECTORS = [
  'Technology', 'Financial Services', 'Healthcare', 'Energy',
  'Consumer Cyclical', 'Consumer Defensive', 'Industrials',
  'Communication Services', 'Real Estate', 'Utilities', 'Basic Materials',
  'Cryptocurrency', 'Fixed Income', 'Commodities',
  'US Equity', 'International Equity', 'Other',
];

const SectorMappingModal: React.FC<SectorMappingModalProps> = ({
  show, holdings, onClose, onMappingsChanged,
}) => {
  const { fontFamily } = useTerminalTheme();

  // All symbols in current portfolio + any existing manual mappings
  const portfolioSymbols = useMemo(() => holdings.map(h => h.symbol), [holdings]);

  // Current mappings for all portfolio symbols
  const [mappings, setMappings] = useState<Record<string, string>>(() => {
    const m: Record<string, string> = {};
    portfolioSymbols.forEach(sym => {
      m[sym] = sectorService.getSectorInfo(sym).sector;
    });
    return m;
  });

  // Inline editing state
  const [editingSymbol, setEditingSymbol] = useState<string | null>(null);
  const [editSector, setEditSector] = useState('');
  const [editIndustry, setEditIndustry] = useState('');

  // New custom row
  const [newSymbol, setNewSymbol] = useState('');
  const [newSector, setNewSector] = useState('');
  const [newIndustry, setNewIndustry] = useState('');
  const [showAddRow, setShowAddRow] = useState(false);

  const allMappings = sectorService.getManualMappings();

  // Extra manual mappings not in current portfolio
  const extraSymbols = useMemo(() =>
    Object.keys(allMappings).filter(s => !portfolioSymbols.includes(s)),
  [allMappings, portfolioSymbols]);

  const startEdit = useCallback((sym: string) => {
    const info = sectorService.getSectorInfo(sym);
    setEditingSymbol(sym);
    setEditSector(info.sector);
    setEditIndustry(info.industry || '');
  }, []);

  const saveEdit = useCallback((sym: string) => {
    if (!editSector.trim()) return;
    sectorService.addSectorMapping(sym, { sector: editSector.trim(), industry: editIndustry.trim() || undefined });
    setMappings(prev => ({ ...prev, [sym]: editSector.trim() }));
    setEditingSymbol(null);
    onMappingsChanged();
  }, [editSector, editIndustry, onMappingsChanged]);

  const removeMapping = useCallback((sym: string) => {
    sectorService.removeManualMapping(sym);
    setMappings(prev => ({
      ...prev,
      [sym]: sectorService.getSectorInfo(sym).sector,
    }));
    onMappingsChanged();
  }, [onMappingsChanged]);

  const addNewMapping = useCallback(() => {
    const sym = newSymbol.trim().toUpperCase();
    if (!sym || !newSector.trim()) return;
    sectorService.addSectorMapping(sym, { sector: newSector.trim(), industry: newIndustry.trim() || undefined });
    if (!portfolioSymbols.includes(sym)) {
      setMappings(prev => ({ ...prev, [sym]: newSector.trim() }));
    } else {
      setMappings(prev => ({ ...prev, [sym]: newSector.trim() }));
    }
    setNewSymbol(''); setNewSector(''); setNewIndustry('');
    setShowAddRow(false);
    onMappingsChanged();
  }, [newSymbol, newSector, newIndustry, portfolioSymbols, onMappingsChanged]);

  if (!show) return null;

  const inputStyle: React.CSSProperties = {
    backgroundColor: DARK_BG, color: WHITE,
    border: `1px solid ${BORDER}`, padding: '4px 7px',
    fontSize: '10px', fontFamily, outline: 'none', width: '100%',
  };

  const renderRow = (sym: string, isExtra = false) => {
    const info = sectorService.getSectorInfo(sym);
    const isManual = !!(sectorService.getManualMappings()[sym]);
    const isEditing = editingSymbol === sym;

    return (
      <tr key={sym} style={{ borderBottom: `1px solid ${BORDER}20` }}>
        {/* Symbol */}
        <td style={{ padding: '5px 8px', fontSize: '10px', fontWeight: 700, color: isExtra ? GRAY : WHITE, fontFamily: 'monospace' }}>
          {sym}
          {isManual && <span style={{ marginLeft: '4px', fontSize: '8px', color: CYAN }}>●</span>}
        </td>

        {/* Sector */}
        <td style={{ padding: '4px 6px' }}>
          {isEditing ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '3px' }}>
              <input
                list={`sectors-${sym}`}
                value={editSector}
                onChange={e => setEditSector(e.target.value)}
                style={inputStyle}
                autoFocus
              />
              <datalist id={`sectors-${sym}`}>
                {COMMON_SECTORS.map(s => <option key={s} value={s} />)}
              </datalist>
              <input
                placeholder="Industry (optional)"
                value={editIndustry}
                onChange={e => setEditIndustry(e.target.value)}
                style={{ ...inputStyle, color: GRAY }}
              />
            </div>
          ) : (
            <span style={{ fontSize: '10px', color: isManual ? CYAN : GRAY }}>
              {mappings[sym] || info.sector}
              {info.industry && <span style={{ color: GRAY, fontSize: '8px', marginLeft: '4px' }}>{info.industry}</span>}
            </span>
          )}
        </td>

        {/* Actions */}
        <td style={{ padding: '4px 6px', textAlign: 'right', whiteSpace: 'nowrap' }}>
          {isEditing ? (
            <div style={{ display: 'flex', gap: '4px', justifyContent: 'flex-end' }}>
              <button onClick={() => saveEdit(sym)} style={{ background: 'none', border: `1px solid ${GREEN}60`, color: GREEN, padding: '3px 8px', cursor: 'pointer', fontSize: '9px', fontWeight: 700 }}>
                <Check size={10} />
              </button>
              <button onClick={() => setEditingSymbol(null)} style={{ background: 'none', border: `1px solid ${BORDER}`, color: GRAY, padding: '3px 8px', cursor: 'pointer', fontSize: '9px' }}>
                <X size={10} />
              </button>
            </div>
          ) : (
            <div style={{ display: 'flex', gap: '4px', justifyContent: 'flex-end' }}>
              <button onClick={() => startEdit(sym)} style={{ background: 'none', border: `1px solid ${BORDER}`, color: GRAY, padding: '3px 6px', cursor: 'pointer', display: 'flex' }}
                onMouseEnter={e => { e.currentTarget.style.color = CYAN; e.currentTarget.style.borderColor = CYAN; }}
                onMouseLeave={e => { e.currentTarget.style.color = GRAY; e.currentTarget.style.borderColor = BORDER; }}>
                <Pencil size={9} />
              </button>
              {isManual && (
                <button onClick={() => removeMapping(sym)} style={{ background: 'none', border: `1px solid ${BORDER}`, color: GRAY, padding: '3px 6px', cursor: 'pointer', display: 'flex' }}
                  onMouseEnter={e => { e.currentTarget.style.color = RED; e.currentTarget.style.borderColor = RED; }}
                  onMouseLeave={e => { e.currentTarget.style.color = GRAY; e.currentTarget.style.borderColor = BORDER; }}>
                  <Trash2 size={9} />
                </button>
              )}
            </div>
          )}
        </td>
      </tr>
    );
  };

  return (
    <div style={{
      position: 'fixed', inset: 0, backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 9999,
    }}>
      <div style={{
        backgroundColor: PANEL, border: `2px solid ${CYAN}`,
        width: '560px', maxWidth: '95vw', maxHeight: '85vh',
        display: 'flex', flexDirection: 'column',
        boxShadow: `0 8px 32px rgba(0,188,212,0.2)`, fontFamily,
      }}>
        {/* Header */}
        <div style={{
          padding: '12px 16px', borderBottom: `1px solid ${BORDER}`,
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          backgroundColor: `${CYAN}08`, flexShrink: 0,
        }}>
          <div>
            <div style={{ fontSize: '12px', fontWeight: 800, color: CYAN, letterSpacing: '1px' }}>
              SECTOR MAPPING EDITOR
            </div>
            <div style={{ fontSize: '9px', color: GRAY, marginTop: '2px' }}>
              <span style={{ color: CYAN }}>●</span> = manual override &nbsp;|&nbsp; Changes saved automatically
            </div>
          </div>
          <button onClick={onClose} style={{ background: 'none', border: 'none', color: GRAY, cursor: 'pointer', display: 'flex' }}>
            <X size={14} />
          </button>
        </div>

        {/* Table */}
        <div style={{ flex: 1, overflowY: 'auto' }}>
          <table style={{ width: '100%', borderCollapse: 'collapse' }}>
            <thead style={{ position: 'sticky', top: 0, backgroundColor: '#0D0D0D', zIndex: 1 }}>
              <tr>
                <th style={{ padding: '6px 8px', textAlign: 'left', fontSize: '9px', color: GRAY, fontWeight: 700, borderBottom: `1px solid ${BORDER}`, width: '100px' }}>SYMBOL</th>
                <th style={{ padding: '6px 8px', textAlign: 'left', fontSize: '9px', color: GRAY, fontWeight: 700, borderBottom: `1px solid ${BORDER}` }}>SECTOR / INDUSTRY</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', fontSize: '9px', color: GRAY, fontWeight: 700, borderBottom: `1px solid ${BORDER}`, width: '80px' }}>ACTIONS</th>
              </tr>
            </thead>
            <tbody>
              {/* Portfolio holdings */}
              {portfolioSymbols.map(sym => renderRow(sym))}

              {/* Extra manual mappings */}
              {extraSymbols.length > 0 && (
                <>
                  <tr><td colSpan={3} style={{ padding: '6px 8px', fontSize: '8px', color: GRAY, fontWeight: 700, backgroundColor: '#080808', borderTop: `1px solid ${BORDER}` }}>OTHER MANUAL MAPPINGS</td></tr>
                  {extraSymbols.map(sym => renderRow(sym, true))}
                </>
              )}

              {/* Add new row */}
              {showAddRow && (
                <tr style={{ borderBottom: `1px solid ${BORDER}20` }}>
                  <td style={{ padding: '5px 8px' }}>
                    <input
                      placeholder="TICKER"
                      value={newSymbol}
                      onChange={e => setNewSymbol(e.target.value.toUpperCase())}
                      style={{ ...inputStyle, fontFamily: 'monospace', fontWeight: 700 }}
                      autoFocus
                    />
                  </td>
                  <td style={{ padding: '4px 6px' }}>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '3px' }}>
                      <input
                        list="new-sectors-list"
                        placeholder="Sector"
                        value={newSector}
                        onChange={e => setNewSector(e.target.value)}
                        style={inputStyle}
                      />
                      <datalist id="new-sectors-list">
                        {COMMON_SECTORS.map(s => <option key={s} value={s} />)}
                      </datalist>
                      <input
                        placeholder="Industry (optional)"
                        value={newIndustry}
                        onChange={e => setNewIndustry(e.target.value)}
                        style={{ ...inputStyle, color: GRAY }}
                      />
                    </div>
                  </td>
                  <td style={{ padding: '4px 6px', textAlign: 'right' }}>
                    <div style={{ display: 'flex', gap: '4px', justifyContent: 'flex-end' }}>
                      <button onClick={addNewMapping} style={{ background: 'none', border: `1px solid ${GREEN}60`, color: GREEN, padding: '3px 8px', cursor: 'pointer', fontSize: '9px', fontWeight: 700 }}>
                        <Check size={10} />
                      </button>
                      <button onClick={() => { setShowAddRow(false); setNewSymbol(''); setNewSector(''); setNewIndustry(''); }}
                        style={{ background: 'none', border: `1px solid ${BORDER}`, color: GRAY, padding: '3px 8px', cursor: 'pointer' }}>
                        <X size={10} />
                      </button>
                    </div>
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>

        {/* Footer */}
        <div style={{
          padding: '10px 16px', borderTop: `1px solid ${BORDER}`,
          display: 'flex', justifyContent: 'space-between', alignItems: 'center',
          flexShrink: 0, backgroundColor: '#0D0D0D',
        }}>
          <button
            onClick={() => { setShowAddRow(true); setEditingSymbol(null); }}
            style={{
              padding: '5px 12px', backgroundColor: 'transparent',
              border: `1px solid ${ORANGE}60`, color: ORANGE,
              fontSize: '10px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}
            onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${ORANGE}15`; }}
            onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
          >
            <Plus size={10} /> ADD MAPPING
          </button>
          <button onClick={onClose} style={{
            padding: '5px 16px', backgroundColor: CYAN, color: '#000',
            border: 'none', fontSize: '10px', fontWeight: 800, cursor: 'pointer',
          }}>
            DONE
          </button>
        </div>
      </div>
    </div>
  );
};

export default SectorMappingModal;
