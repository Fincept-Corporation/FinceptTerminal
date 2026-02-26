import React, { useState, useEffect, useMemo, useCallback, useRef } from 'react';
import {
  Search, Code, Play, Copy, Loader2, AlertCircle,
  X, BookOpen, ChevronRight, FolderOpen, FileCode, Eye, Layers,
  Edit3, Save, RotateCcw,
} from 'lucide-react';
import type { PythonStrategy } from '../types';
import {
  listPythonStrategies,
  getStrategyCategories,
  getPythonStrategyCode,
  saveCustomPythonStrategy,
} from '../services/algoTradingService';
import { F } from '../constants/theme';

interface StrategyLibraryTabProps {
  onViewCode?: (strategy: PythonStrategy, code: string) => void;
  onClone?: (strategy: PythonStrategy) => void;
  onBacktest?: (strategy: PythonStrategy) => void;
  onDeploy?: (strategy: PythonStrategy) => void;
}

const font = '"IBM Plex Mono", "Consolas", monospace';
const CAT_COLORS = [F.CYAN, F.PURPLE, F.BLUE, F.GREEN, F.YELLOW, F.ORANGE, '#FF6B9D', '#7B68EE'];
function catColor(idx: number) { return CAT_COLORS[idx % CAT_COLORS.length]; }

// ── Truncate long CamelCase names into readable form ──
function shortName(name: string): string {
  // Insert space before capitals: "BasicTemplateAlgorithm" -> "Basic Template Algorithm"
  return name.replace(/([a-z])([A-Z])/g, '$1 $2').replace(/([A-Z]+)([A-Z][a-z])/g, '$1 $2');
}

const StrategyLibraryTab: React.FC<StrategyLibraryTabProps> = ({
  onViewCode, onClone, onBacktest, onDeploy,
}) => {
  const [strategies, setStrategies] = useState<PythonStrategy[]>([]);
  const [categories, setCategories] = useState<string[]>([]);
  const [selectedCategory, setSelectedCategory] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [codePreview, setCodePreview] = useState<string | null>(null);
  const [loadingCode, setLoadingCode] = useState(false);
  const [editing, setEditing] = useState(false);
  const [editedCode, setEditedCode] = useState('');
  const [saving, setSaving] = useState(false);
  const [originalCode, setOriginalCode] = useState('');
  const editorRef = useRef<HTMLTextAreaElement>(null);

  useEffect(() => { loadData(); }, []);

  const loadData = async () => {
    setLoading(true); setError(null);
    try {
      const [catResult, stratResult] = await Promise.all([getStrategyCategories(), listPythonStrategies()]);
      if (!catResult.success) throw new Error(catResult.error || 'Failed to load categories');
      if (!stratResult.success) throw new Error(stratResult.error || 'Failed to load strategies');
      setCategories(catResult.data || []);
      setStrategies(stratResult.data || []);
    } catch (err) {
      console.error('[StrategyLibrary] loadData error:', err);
      setError(err instanceof Error ? err.message : typeof err === 'string' ? err : JSON.stringify(err) || 'Unknown error');
    } finally { setLoading(false); }
  };

  const filtered = useMemo(() => {
    const q = searchQuery.toLowerCase();
    return strategies.filter(s => {
      if (selectedCategory && s.category !== selectedCategory) return false;
      if (!q) return true;
      return s.name.toLowerCase().includes(q) || s.id.toLowerCase().includes(q) || s.category.toLowerCase().includes(q);
    });
  }, [strategies, selectedCategory, searchQuery]);

  const categoryCounts = useMemo(() => {
    const c: Record<string, number> = {};
    strategies.forEach(s => { c[s.category] = (c[s.category] || 0) + 1; });
    return c;
  }, [strategies]);

  const catColorMap = useMemo(() => {
    const m: Record<string, string> = {};
    categories.forEach((c, i) => { m[c] = catColor(i); });
    return m;
  }, [categories]);

  const selectedStrategy = useMemo(() => strategies.find(s => s.id === selectedId) || null, [strategies, selectedId]);

  const handleSelect = useCallback(async (strategy: PythonStrategy) => {
    if (selectedId === strategy.id) { setSelectedId(null); setCodePreview(null); setEditing(false); return; }
    setSelectedId(strategy.id);
    setCodePreview(null); setLoadingCode(true); setEditing(false);
    try {
      const result = await getPythonStrategyCode(strategy.id);
      if (result.success && result.data) {
        setCodePreview(result.data.code);
        setOriginalCode(result.data.code);
        setEditedCode(result.data.code);
        if (onViewCode) onViewCode(strategy, result.data.code);
      } else { setCodePreview(`# Error: ${result.error || 'Unknown'}`); }
    } catch (err) { setCodePreview(`# Error: ${err instanceof Error ? err.message : 'Unknown'}`); }
    finally { setLoadingCode(false); }
  }, [selectedId, onViewCode]);

  const handleStartEdit = useCallback(() => {
    if (codePreview) {
      setEditedCode(codePreview);
      setEditing(true);
      setTimeout(() => editorRef.current?.focus(), 50);
    }
  }, [codePreview]);

  const handleCancelEdit = useCallback(() => {
    setEditing(false);
    setEditedCode(originalCode);
  }, [originalCode]);

  const handleSave = useCallback(async () => {
    if (!selectedStrategy || !editedCode) return;
    setSaving(true);
    try {
      const result = await saveCustomPythonStrategy({
        baseStrategyId: selectedStrategy.id,
        name: selectedStrategy.name,
        description: `Modified from ${selectedStrategy.id}`,
        code: editedCode,
        category: selectedStrategy.category,
      });
      if (result.success) {
        setCodePreview(editedCode);
        setOriginalCode(editedCode);
        setEditing(false);
      } else {
        alert(`Save failed: ${result.error || 'Unknown error'}`);
      }
    } catch (err) {
      alert(`Save error: ${err instanceof Error ? err.message : 'Unknown'}`);
    } finally { setSaving(false); }
  }, [selectedStrategy, editedCode]);

  const hasChanges = editing && editedCode !== originalCode;

  const chipBase: React.CSSProperties = {
    padding: '3px 10px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
    fontFamily: font, cursor: 'pointer', letterSpacing: '0.3px',
    borderWidth: '1px', borderStyle: 'solid', whiteSpace: 'nowrap', transition: 'all 0.15s',
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', fontFamily: font, backgroundColor: F.DARK_BG }}>

      {/* ━━━ TOP BAR ━━━ */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '10px', padding: '8px 14px',
        backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0,
      }}>
        <BookOpen size={14} style={{ color: F.ORANGE }} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>STRATEGY LIBRARY</span>
        <span style={{ fontSize: '9px', color: F.MUTED, fontWeight: 600 }}>{strategies.length} strategies · {categories.length} categories</span>
        <div style={{ flex: 1 }} />
        <div style={{
          display: 'flex', alignItems: 'center', gap: '6px', padding: '4px 10px',
          backgroundColor: F.DARK_BG, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, borderRadius: '2px', minWidth: '260px',
        }}>
          <Search size={11} style={{ color: F.GRAY }} />
          <input type="text" placeholder="Search strategies..." value={searchQuery} onChange={e => setSearchQuery(e.target.value)}
            style={{ flex: 1, backgroundColor: 'transparent', border: 'none', outline: 'none', color: F.WHITE, fontSize: '10px', fontFamily: font }} />
          {searchQuery && <X size={10} style={{ color: F.GRAY, cursor: 'pointer' }} onClick={() => setSearchQuery('')} />}
        </div>
        <span style={{ fontSize: '10px', fontWeight: 700, color: filtered.length > 0 ? F.ORANGE : F.MUTED }}>{filtered.length}</span>
      </div>

      {/* ━━━ CATEGORY CHIPS ━━━ */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '5px', padding: '6px 14px',
        borderBottom: `1px solid ${F.BORDER}`, overflowX: 'auto', flexShrink: 0, backgroundColor: F.PANEL_BG,
      }}>
        <button onClick={() => setSelectedCategory(null)} style={{
          ...chipBase,
          backgroundColor: selectedCategory === null ? `${F.ORANGE}18` : 'transparent',
          borderColor: selectedCategory === null ? `${F.ORANGE}50` : F.BORDER,
          color: selectedCategory === null ? F.ORANGE : F.GRAY,
        }}>ALL</button>
        {categories.map(cat => {
          const col = catColorMap[cat]; const active = selectedCategory === cat;
          return (
            <button key={cat} onClick={() => setSelectedCategory(active ? null : cat)} style={{
              ...chipBase,
              backgroundColor: active ? `${col}18` : 'transparent',
              borderColor: active ? `${col}50` : F.BORDER,
              color: active ? col : F.GRAY,
            }}>
              {cat} <span style={{ opacity: 0.6, marginLeft: '3px' }}>{categoryCounts[cat] || 0}</span>
            </button>
          );
        })}
      </div>

      {/* ━━━ MAIN: Card grid + Code preview ━━━ */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden', minHeight: 0 }}>

        {/* ── CARD GRID ── */}
        <div style={{ flex: 1, overflow: 'auto', padding: '14px', minWidth: 0 }}>
          {loading ? (
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '200px', gap: '8px' }}>
              <Loader2 size={16} className="animate-spin" style={{ color: F.ORANGE }} />
              <span style={{ fontSize: '10px', fontWeight: 600, color: F.GRAY }}>Loading strategies...</span>
            </div>
          ) : error ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '200px', gap: '8px' }}>
              <AlertCircle size={20} style={{ color: F.RED }} />
              <span style={{ fontSize: '10px', fontWeight: 600, color: F.RED }}>{error}</span>
              <button onClick={loadData} style={{
                padding: '5px 14px', fontSize: '9px', fontWeight: 700, fontFamily: font,
                backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                color: F.GRAY, borderRadius: '2px', cursor: 'pointer',
              }}>RETRY</button>
            </div>
          ) : filtered.length === 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '200px', gap: '8px' }}>
              <FolderOpen size={24} style={{ color: F.MUTED, opacity: 0.3 }} />
              <span style={{ fontSize: '10px', fontWeight: 600, color: F.GRAY }}>No strategies match</span>
            </div>
          ) : (
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(260px, 1fr))', gap: '10px' }}>
              {filtered.map(strategy => {
                const isSelected = selectedId === strategy.id;
                const col = catColorMap[strategy.category] || F.CYAN;
                return (
                  <div key={strategy.id} onClick={() => handleSelect(strategy)} style={{
                    display: 'flex', flexDirection: 'column', borderRadius: '3px', cursor: 'pointer',
                    backgroundColor: F.PANEL_BG, overflow: 'hidden', transition: 'all 0.15s',
                    borderWidth: '1px', borderStyle: 'solid',
                    borderColor: isSelected ? F.ORANGE : F.BORDER,
                    boxShadow: isSelected ? `0 0 0 1px ${F.ORANGE}40` : 'none',
                  }}
                    onMouseEnter={e => { if (!isSelected) { e.currentTarget.style.borderColor = `${col}60`; e.currentTarget.style.transform = 'translateY(-1px)'; } }}
                    onMouseLeave={e => { if (!isSelected) { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.transform = 'none'; } }}
                  >
                    {/* Card top accent */}
                    <div style={{ height: '3px', backgroundColor: col, opacity: isSelected ? 1 : 0.4, transition: 'opacity 0.15s' }} />

                    {/* Card body */}
                    <div style={{ padding: '12px 14px 10px' }}>
                      {/* Category + ID */}
                      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                        <span style={{
                          fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
                          padding: '2px 6px', borderRadius: '2px',
                          backgroundColor: `${col}15`, color: col,
                        }}>{strategy.category}</span>
                        <span style={{ fontSize: '8px', color: F.MUTED, fontFamily: font }}>{strategy.id}</span>
                      </div>

                      {/* Name */}
                      <div style={{
                        fontSize: '11px', fontWeight: 700, color: isSelected ? F.WHITE : '#DDDDDD',
                        lineHeight: '1.35', marginBottom: '6px',
                        display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical' as const,
                        overflow: 'hidden', textOverflow: 'ellipsis', wordBreak: 'break-word',
                      }} title={strategy.name}>
                        {shortName(strategy.name)}
                      </div>

                      {/* File path */}
                      <div style={{ fontSize: '8px', color: F.MUTED, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', marginBottom: '10px' }} title={strategy.path}>
                        <FileCode size={9} style={{ display: 'inline', verticalAlign: 'middle', marginRight: '4px' }} />
                        {strategy.path}
                      </div>
                    </div>

                    {/* Card footer — action buttons */}
                    <div style={{
                      display: 'flex', gap: '6px', padding: '8px 14px',
                      borderTop: `1px solid ${F.BORDER}`, backgroundColor: `${F.HEADER_BG}80`,
                    }}>
                      <button onClick={e => { e.stopPropagation(); handleSelect(strategy); }} style={{
                        flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
                        padding: '5px 0', fontSize: '9px', fontWeight: 700, fontFamily: font,
                        backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                        color: F.GRAY, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.15s',
                      }}
                        onMouseEnter={e => { e.currentTarget.style.borderColor = F.CYAN; e.currentTarget.style.color = F.CYAN; }}
                        onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
                      ><Eye size={10} /> VIEW</button>
                      {onClone && (
                        <button onClick={e => { e.stopPropagation(); onClone(strategy); }} style={{
                          flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
                          padding: '5px 0', fontSize: '9px', fontWeight: 700, fontFamily: font,
                          backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: `${F.BLUE}35`,
                          color: F.BLUE, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.15s',
                        }}
                          onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.BLUE}10`; }}
                          onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                        ><Copy size={10} /> CLONE</button>
                      )}
                      {onBacktest && (
                        <button onClick={e => { e.stopPropagation(); onBacktest(strategy); }} style={{
                          flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
                          padding: '5px 0', fontSize: '9px', fontWeight: 700, fontFamily: font,
                          backgroundColor: F.ORANGE, border: 'none',
                          color: F.DARK_BG, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.15s',
                        }}
                          onMouseEnter={e => { e.currentTarget.style.opacity = '0.85'; }}
                          onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
                        ><Play size={10} /> TEST</button>
                      )}
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* ── CODE PREVIEW PANEL ── */}
        {selectedStrategy && (
          <div style={{
            width: '480px', flexShrink: 0, display: 'flex', flexDirection: 'column',
            borderLeft: `1px solid ${F.BORDER}`, backgroundColor: F.PANEL_BG, overflow: 'hidden',
          }}>
            {/* Header */}
            <div style={{
              display: 'flex', alignItems: 'center', gap: '8px', padding: '10px 14px',
              backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0,
            }}>
              <div style={{
                width: '28px', height: '28px', borderRadius: '3px', display: 'flex', alignItems: 'center', justifyContent: 'center',
                backgroundColor: `${catColorMap[selectedStrategy.category] || F.CYAN}15`,
              }}>
                <FileCode size={14} style={{ color: catColorMap[selectedStrategy.category] || F.CYAN }} />
              </div>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {shortName(selectedStrategy.name)}
                </div>
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <span>{selectedStrategy.id}</span>
                  <span style={{ color: catColorMap[selectedStrategy.category] || F.CYAN }}>{selectedStrategy.category}</span>
                  <span>{selectedStrategy.path}</span>
                </div>
              </div>
              <button onClick={() => { setSelectedId(null); setCodePreview(null); }}
                style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', color: F.MUTED, cursor: 'pointer', borderRadius: '2px', display: 'flex' }}
                onMouseEnter={e => { e.currentTarget.style.color = F.RED; }}
                onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
              ><X size={13} /></button>
            </div>

            {/* Action bar */}
            <div style={{ display: 'flex', gap: '6px', padding: '8px 14px', borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0, alignItems: 'center' }}>
              {/* Edit / Save / Cancel */}
              {!editing ? (
                <button onClick={handleStartEdit} disabled={!codePreview || loadingCode} style={{
                  display: 'flex', alignItems: 'center', gap: '4px', padding: '5px 12px',
                  fontSize: '9px', fontWeight: 700, fontFamily: font, borderRadius: '2px', cursor: 'pointer',
                  backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: `${F.CYAN}40`,
                  color: F.CYAN, transition: 'all 0.15s', opacity: !codePreview || loadingCode ? 0.4 : 1,
                }}
                  onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.CYAN}12`; }}
                  onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                ><Edit3 size={10} /> EDIT</button>
              ) : (
                <>
                  <button onClick={handleSave} disabled={saving || !hasChanges} style={{
                    display: 'flex', alignItems: 'center', gap: '4px', padding: '5px 12px',
                    fontSize: '9px', fontWeight: 700, fontFamily: font, borderRadius: '2px', cursor: 'pointer',
                    backgroundColor: hasChanges ? F.GREEN : 'transparent', border: 'none',
                    color: hasChanges ? F.DARK_BG : F.MUTED, transition: 'all 0.15s',
                    opacity: saving ? 0.5 : 1,
                  }}>
                    {saving ? <Loader2 size={10} className="animate-spin" /> : <Save size={10} />}
                    {saving ? 'SAVING...' : 'SAVE COPY'}
                  </button>
                  <button onClick={handleCancelEdit} style={{
                    display: 'flex', alignItems: 'center', gap: '4px', padding: '5px 12px',
                    fontSize: '9px', fontWeight: 700, fontFamily: font, borderRadius: '2px', cursor: 'pointer',
                    backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                    color: F.GRAY, transition: 'all 0.15s',
                  }}
                    onMouseEnter={e => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.RED; }}
                    onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
                  ><RotateCcw size={10} /> CANCEL</button>
                  {hasChanges && <span style={{ fontSize: '8px', color: F.YELLOW, fontWeight: 600 }}>MODIFIED</span>}
                </>
              )}

              <div style={{ flex: 1 }} />

              {onClone && (
                <button onClick={() => onClone(selectedStrategy)} style={{
                  display: 'flex', alignItems: 'center', gap: '4px', padding: '5px 12px',
                  fontSize: '9px', fontWeight: 700, fontFamily: font, borderRadius: '2px', cursor: 'pointer',
                  backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: `${F.BLUE}40`,
                  color: F.BLUE, transition: 'all 0.15s',
                }}
                  onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.BLUE}12`; }}
                  onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                ><Copy size={10} /> CLONE</button>
              )}
              {onBacktest && (
                <button onClick={() => onBacktest(selectedStrategy)} style={{
                  display: 'flex', alignItems: 'center', gap: '4px', padding: '5px 12px',
                  fontSize: '9px', fontWeight: 700, fontFamily: font, borderRadius: '2px', cursor: 'pointer',
                  backgroundColor: F.ORANGE, border: 'none', color: F.DARK_BG, transition: 'all 0.15s',
                }}
                  onMouseEnter={e => { e.currentTarget.style.opacity = '0.85'; }}
                  onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
                ><Play size={10} /> BACKTEST</button>
              )}
            </div>

            {/* Code editor / viewer */}
            <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
              {loadingCode ? (
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100px', gap: '8px' }}>
                  <Loader2 size={14} className="animate-spin" style={{ color: F.ORANGE }} />
                  <span style={{ fontSize: '10px', color: F.GRAY }}>Loading source...</span>
                </div>
              ) : editing ? (
                <textarea
                  ref={editorRef}
                  value={editedCode}
                  onChange={e => setEditedCode(e.target.value)}
                  spellCheck={false}
                  style={{
                    flex: 1, margin: 0, padding: '14px', resize: 'none',
                    fontSize: '10px', lineHeight: '1.65', fontFamily: font,
                    color: '#CCCCCC', backgroundColor: '#050505',
                    border: 'none', outline: 'none', whiteSpace: 'pre',
                    tabSize: 4, overflowX: 'auto',
                  }}
                  onKeyDown={e => {
                    // Tab key inserts 4 spaces
                    if (e.key === 'Tab') {
                      e.preventDefault();
                      const ta = e.currentTarget;
                      const start = ta.selectionStart;
                      const end = ta.selectionEnd;
                      const val = ta.value;
                      setEditedCode(val.substring(0, start) + '    ' + val.substring(end));
                      setTimeout(() => { ta.selectionStart = ta.selectionEnd = start + 4; }, 0);
                    }
                  }}
                />
              ) : (
                <pre style={{
                  flex: 1, margin: 0, padding: '14px', fontSize: '10px', lineHeight: '1.65',
                  fontFamily: font, color: '#CCCCCC', whiteSpace: 'pre', overflowX: 'auto', overflow: 'auto',
                  backgroundColor: F.DARK_BG, cursor: 'text',
                }} onClick={handleStartEdit}>
                  {codePreview || '# Select a strategy to view source code'}
                </pre>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default StrategyLibraryTab;
