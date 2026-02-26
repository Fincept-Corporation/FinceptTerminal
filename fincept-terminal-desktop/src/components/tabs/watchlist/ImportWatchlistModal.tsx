import React, { useState, useCallback } from 'react';
import { Upload, Download, CheckCircle, AlertCircle, X } from 'lucide-react';
import { FINCEPT, FONT_FAMILY } from './utils';

// ─── Types ────────────────────────────────────────────────────────────────────

export interface WatchlistImportEntry {
  symbol: string;
  notes?: string;
}

export interface WatchlistImportJSON {
  format_version: string;
  watchlist_name: string;
  description?: string;
  color?: string;
  exported_at?: string;
  stocks: WatchlistImportEntry[];
}

export interface WatchlistImportResult {
  watchlistId: string;
  watchlistName: string;
  added: number;
  skipped: string[];   // already-existing symbols
  errors: string[];
}

export type ImportWatchlistMode = 'new' | 'append';

type Step = 'select-file' | 'configure' | 'importing' | 'done';

// ─── JSON parser ─────────────────────────────────────────────────────────────

export function parseWatchlistJSON(content: string): WatchlistImportJSON {
  let raw: any;
  try {
    raw = JSON.parse(content);
  } catch {
    throw new Error('Invalid JSON — could not parse file.');
  }

  // Accept both formats:
  //   1. { stocks: [{symbol, notes?}, ...], ... }  — Fincept export
  //   2. ["AAPL","MSFT",...]                         — simple symbol array
  //   3. [{symbol:"AAPL",...},...]                   — array of objects

  if (Array.isArray(raw)) {
    const stocks: WatchlistImportEntry[] = raw.map((item: any, i: number) => {
      if (typeof item === 'string') return { symbol: item.trim().toUpperCase() };
      if (item && typeof item.symbol === 'string') return { symbol: item.symbol.trim().toUpperCase(), notes: item.notes };
      if (item && typeof item.ticker === 'string') return { symbol: item.ticker.trim().toUpperCase(), notes: item.notes };
      throw new Error(`Item at index ${i} is not a valid symbol or stock object.`);
    });
    return {
      format_version: '1.0',
      watchlist_name: 'Imported Watchlist',
      stocks,
    };
  }

  if (raw && typeof raw === 'object') {
    // Must have a stocks array
    const stocksRaw = raw.stocks ?? raw.symbols ?? raw.tickers;
    if (!Array.isArray(stocksRaw)) {
      throw new Error('JSON must have a "stocks", "symbols", or "tickers" array, or be a plain array.');
    }
    if (stocksRaw.length === 0) throw new Error('No stocks found in file.');

    const stocks: WatchlistImportEntry[] = stocksRaw.map((item: any, i: number) => {
      if (typeof item === 'string') return { symbol: item.trim().toUpperCase() };
      if (item && typeof item.symbol === 'string') return { symbol: item.symbol.trim().toUpperCase(), notes: item.notes };
      if (item && typeof item.ticker === 'string') return { symbol: item.ticker.trim().toUpperCase(), notes: item.notes };
      throw new Error(`Stock at index ${i} is missing a "symbol" or "ticker" field.`);
    });

    return {
      format_version: raw.format_version ?? '1.0',
      watchlist_name: raw.watchlist_name ?? raw.name ?? 'Imported Watchlist',
      description: raw.description,
      color: raw.color,
      exported_at: raw.exported_at ?? raw.export_date,
      stocks,
    };
  }

  throw new Error('Unrecognized JSON format.');
}

// ─── Props ────────────────────────────────────────────────────────────────────

interface ImportWatchlistModalProps {
  show: boolean;
  existingWatchlists: Array<{ id: string; name: string }>;
  selectedWatchlistId: string | null;
  onClose: () => void;
  onImport: (
    data: WatchlistImportJSON,
    mode: ImportWatchlistMode,
    targetWatchlistId?: string,
    nameOverride?: string,
    colorOverride?: string,
  ) => Promise<WatchlistImportResult>;
  onImportComplete: (result: WatchlistImportResult) => void;
}

// ─── Style constants (match FINCEPT palette) ──────────────────────────────────

const { CYAN, ORANGE, GREEN, RED, BORDER, DARK_BG: BG, PANEL_BG: PANEL, GRAY, WHITE, HOVER } = FINCEPT;
const FF = FONT_FAMILY;

const labelStyle: React.CSSProperties = {
  color: GRAY, fontSize: '9px', fontWeight: 700,
  letterSpacing: '0.5px', textTransform: 'uppercase',
  display: 'block', marginBottom: '4px',
};

const inputStyle: React.CSSProperties = {
  width: '100%', padding: '7px 10px', backgroundColor: BG,
  color: WHITE, border: `1px solid ${BORDER}`, fontSize: '11px',
  fontFamily: FF, outline: 'none', boxSizing: 'border-box',
};

// ─── Component ────────────────────────────────────────────────────────────────

const ImportWatchlistModal: React.FC<ImportWatchlistModalProps> = ({
  show, existingWatchlists, selectedWatchlistId, onClose, onImport, onImportComplete,
}) => {
  const [step, setStep] = useState<Step>('select-file');
  const [fileName, setFileName] = useState('');
  const [parsedData, setParsedData] = useState<WatchlistImportJSON | null>(null);
  const [parseError, setParseError] = useState<string | null>(null);
  const [mode, setMode] = useState<ImportWatchlistMode>('new');
  const [targetId, setTargetId] = useState(selectedWatchlistId ?? existingWatchlists[0]?.id ?? '');
  const [nameOverride, setNameOverride] = useState('');
  const [colorOverride, setColorOverride] = useState('#FF8800');
  const [isDragging, setIsDragging] = useState(false);
  const [importing, setImporting] = useState(false);
  const [result, setResult] = useState<WatchlistImportResult | null>(null);

  const reset = useCallback(() => {
    setStep('select-file');
    setFileName('');
    setParsedData(null);
    setParseError(null);
    setMode('new');
    setTargetId(selectedWatchlistId ?? existingWatchlists[0]?.id ?? '');
    setNameOverride('');
    setColorOverride('#FF8800');
    setIsDragging(false);
    setImporting(false);
    setResult(null);
  }, [selectedWatchlistId, existingWatchlists]);

  const handleClose = useCallback(() => { reset(); onClose(); }, [reset, onClose]);

  const processFile = useCallback((content: string, name: string) => {
    setParseError(null);
    try {
      const data = parseWatchlistJSON(content);
      setParsedData(data);
      setFileName(name);
      setNameOverride(data.watchlist_name);
      if (data.color) setColorOverride(data.color);
      if (existingWatchlists.length > 0) {
        setTargetId(selectedWatchlistId ?? existingWatchlists[0].id);
      }
      setStep('configure');
    } catch (err: any) {
      setParseError(err?.message || 'Failed to parse file');
    }
  }, [selectedWatchlistId, existingWatchlists]);

  const handleFile = useCallback((file: File) => {
    if (!file.name.toLowerCase().endsWith('.json')) {
      setParseError('Only JSON files are supported for watchlist import.');
      return;
    }
    const reader = new FileReader();
    reader.onload = (e) => processFile(e.target?.result as string, file.name);
    reader.readAsText(file);
  }, [processFile]);

  const handleDrop = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    setIsDragging(false);
    const file = e.dataTransfer.files?.[0];
    if (file) handleFile(file);
  }, [handleFile]);

  const handleExecuteImport = useCallback(async () => {
    if (!parsedData) return;
    setStep('importing');
    setImporting(true);
    try {
      const importResult = await onImport(
        parsedData,
        mode,
        mode === 'append' ? targetId : undefined,
        mode === 'new' ? (nameOverride.trim() || parsedData.watchlist_name) : undefined,
        mode === 'new' ? colorOverride : undefined,
      );
      setResult(importResult);
      setStep('done');
    } catch (err: any) {
      setResult({
        watchlistId: '',
        watchlistName: '',
        added: 0,
        skipped: [],
        errors: [err?.message || 'Import failed'],
      });
      setStep('done');
    } finally {
      setImporting(false);
    }
  }, [parsedData, mode, targetId, nameOverride, colorOverride, onImport]);

  const downloadTemplate = useCallback(() => {
    const template: WatchlistImportJSON = {
      format_version: '1.0',
      watchlist_name: 'My Tech Watchlist',
      description: 'US tech sector watchlist',
      color: '#00E5FF',
      exported_at: new Date().toISOString(),
      stocks: [
        { symbol: 'AAPL', notes: 'Apple Inc' },
        { symbol: 'MSFT', notes: 'Microsoft' },
        { symbol: 'GOOGL', notes: 'Alphabet' },
        { symbol: 'NVDA', notes: 'Nvidia' },
        { symbol: 'TSLA' },
      ],
    };
    const blob = new Blob([JSON.stringify(template, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'watchlist_template.json';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, []);

  if (!show) return null;

  const steps: Step[] = ['select-file', 'configure', 'importing', 'done'];

  return (
    <div style={{
      position: 'fixed', inset: 0, backgroundColor: 'rgba(0,0,0,0.88)',
      display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 9999,
    }}>
      <div style={{
        backgroundColor: PANEL, border: `2px solid ${CYAN}`,
        padding: '24px', width: '500px', maxWidth: '95vw',
        boxShadow: `0 8px 32px ${CYAN}20`, fontFamily: FF,
        maxHeight: '90vh', overflowY: 'auto',
      }}>

        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '20px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Upload size={14} color={CYAN} />
            <span style={{ fontSize: '13px', fontWeight: 800, color: CYAN, letterSpacing: '1px' }}>
              IMPORT WATCHLIST
            </span>
          </div>
          <button onClick={handleClose} style={{ background: 'none', border: 'none', color: GRAY, cursor: 'pointer', display: 'flex' }}>
            <X size={14} />
          </button>
        </div>

        {/* Step indicator */}
        <div style={{ display: 'flex', gap: '6px', marginBottom: '20px' }}>
          {steps.map((s, i) => (
            <div key={s} style={{
              flex: 1, height: '2px',
              backgroundColor: step === s ? CYAN : (steps.indexOf(step) > i ? `${CYAN}60` : BORDER),
            }} />
          ))}
        </div>

        {/* ─── STEP: select-file ─── */}
        {step === 'select-file' && (
          <div>
            <div
              onDragOver={(e) => { e.preventDefault(); setIsDragging(true); }}
              onDragLeave={() => setIsDragging(false)}
              onDrop={handleDrop}
              onClick={() => document.getElementById('watchlist-file-input')?.click()}
              style={{
                border: `2px dashed ${isDragging ? CYAN : BORDER}`,
                backgroundColor: isDragging ? `${CYAN}08` : BG,
                padding: '32px', textAlign: 'center', cursor: 'pointer',
                transition: 'all 0.2s',
              }}
            >
              <Upload size={28} color={isDragging ? CYAN : GRAY} style={{ marginBottom: '12px' }} />
              <div style={{ color: WHITE, fontSize: '12px', fontWeight: 600, marginBottom: '4px' }}>
                Drop JSON file here or click to browse
              </div>
              <div style={{ color: GRAY, fontSize: '10px' }}>
                Accepts Fincept watchlist JSON or a simple symbol array
              </div>
              <input
                id="watchlist-file-input"
                type="file"
                accept=".json"
                style={{ display: 'none' }}
                onChange={(e) => { const f = e.target.files?.[0]; if (f) handleFile(f); e.target.value = ''; }}
              />
            </div>

            {parseError && (
              <div style={{
                marginTop: '12px', padding: '10px 12px',
                backgroundColor: `${RED}12`, border: `1px solid ${RED}40`,
                display: 'flex', alignItems: 'center', gap: '8px',
              }}>
                <AlertCircle size={12} color={RED} />
                <span style={{ color: RED, fontSize: '10px' }}>{parseError}</span>
              </div>
            )}

            {/* Supported formats */}
            <div style={{ marginTop: '16px', color: GRAY, fontSize: '9px', lineHeight: '1.8' }}>
              <div style={{ fontWeight: 700, color: WHITE, marginBottom: '4px' }}>SUPPORTED FORMATS</div>
              <div>• <span style={{ color: CYAN }}>Fincept export</span> — {"{ watchlist_name, stocks: [{symbol, notes}] }"}</div>
              <div>• <span style={{ color: CYAN }}>Symbol array</span> — {"[\"AAPL\", \"MSFT\", \"GOOGL\"]"}</div>
              <div>• <span style={{ color: CYAN }}>Object array</span> — {"[{symbol: \"AAPL\", notes: \"...\"}, ...]"}</div>
            </div>

            {/* Template download */}
            <div style={{
              marginTop: '14px', padding: '10px 12px',
              backgroundColor: `${CYAN}08`, border: `1px solid ${BORDER}`,
            }}>
              <div style={{ fontSize: '9px', color: GRAY, fontWeight: 700, marginBottom: '6px' }}>
                DOWNLOAD TEMPLATE
              </div>
              <div style={{ fontSize: '9px', color: GRAY, marginBottom: '8px' }}>
                Download a sample JSON to see the exact format expected.
              </div>
              <button
                onClick={(e) => { e.stopPropagation(); downloadTemplate(); }}
                style={{
                  padding: '5px 12px', backgroundColor: 'transparent',
                  border: `1px solid ${CYAN}60`, color: CYAN,
                  fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                  display: 'inline-flex', alignItems: 'center', gap: '4px',
                }}
                onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = `${CYAN}15`; }}
                onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <Download size={9} /> TEMPLATE.JSON
              </button>
            </div>
          </div>
        )}

        {/* ─── STEP: configure ─── */}
        {step === 'configure' && parsedData && (
          <div>
            {/* File preview */}
            <div style={{
              padding: '10px 12px', backgroundColor: BG,
              border: `1px solid ${BORDER}`, marginBottom: '16px',
            }}>
              <div style={{ fontSize: '9px', color: GRAY, fontWeight: 700, marginBottom: '6px' }}>FILE DETECTED</div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
                {[
                  { l: 'FILE', v: fileName },
                  { l: 'WATCHLIST', v: parsedData.watchlist_name },
                  { l: 'SYMBOLS', v: String(parsedData.stocks.length) },
                  { l: 'EXPORTED', v: parsedData.exported_at ? new Date(parsedData.exported_at).toLocaleDateString() : '—' },
                ].map(({ l, v }) => (
                  <div key={l}>
                    <span style={{ fontSize: '8px', color: GRAY }}>{l}: </span>
                    <span style={{ fontSize: '9px', color: WHITE, fontWeight: 600 }}>{v}</span>
                  </div>
                ))}
              </div>

              {/* Symbol preview pills */}
              <div style={{ marginTop: '8px', display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                {parsedData.stocks.slice(0, 12).map(s => (
                  <span key={s.symbol} style={{
                    padding: '2px 6px', fontSize: '8px', fontWeight: 700,
                    backgroundColor: `${CYAN}15`, color: CYAN,
                    border: `1px solid ${CYAN}30`,
                  }}>
                    {s.symbol}
                  </span>
                ))}
                {parsedData.stocks.length > 12 && (
                  <span style={{ fontSize: '8px', color: GRAY, padding: '2px 4px' }}>
                    +{parsedData.stocks.length - 12} more
                  </span>
                )}
              </div>
            </div>

            {/* Import mode */}
            <div style={{ marginBottom: '14px' }}>
              <label style={labelStyle}>IMPORT MODE</label>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {([
                  { value: 'new' as ImportWatchlistMode, label: 'Create new watchlist', desc: 'Import as a brand new watchlist' },
                  { value: 'append' as ImportWatchlistMode, label: 'Append to existing watchlist', desc: 'Add symbols to an existing watchlist (duplicates skipped)' },
                ]).map(opt => (
                  <label key={opt.value} style={{
                    display: 'flex', alignItems: 'flex-start', gap: '8px',
                    padding: '8px 10px', cursor: 'pointer',
                    backgroundColor: mode === opt.value ? `${CYAN}10` : 'transparent',
                    border: `1px solid ${mode === opt.value ? CYAN : BORDER}`,
                  }}>
                    <input
                      type="radio"
                      name="wl-import-mode"
                      value={opt.value}
                      checked={mode === opt.value}
                      onChange={() => setMode(opt.value)}
                      style={{ marginTop: '2px', accentColor: CYAN }}
                    />
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 700, color: mode === opt.value ? CYAN : WHITE }}>
                        {opt.label}
                      </div>
                      <div style={{ fontSize: '9px', color: GRAY }}>{opt.desc}</div>
                    </div>
                  </label>
                ))}
              </div>
            </div>

            {/* New watchlist fields */}
            {mode === 'new' && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: '10px', marginBottom: '14px', alignItems: 'end' }}>
                <div>
                  <label style={labelStyle}>WATCHLIST NAME</label>
                  <input
                    type="text"
                    value={nameOverride}
                    onChange={(e) => setNameOverride(e.target.value)}
                    style={inputStyle}
                    onFocus={(e) => { e.currentTarget.style.borderColor = CYAN; }}
                    onBlur={(e) => { e.currentTarget.style.borderColor = BORDER; }}
                  />
                </div>
                <div>
                  <label style={labelStyle}>COLOR</label>
                  <input
                    type="color"
                    value={colorOverride}
                    onChange={(e) => setColorOverride(e.target.value)}
                    style={{
                      width: '48px', height: '34px', padding: '2px',
                      backgroundColor: BG, border: `1px solid ${BORDER}`,
                      cursor: 'pointer', borderRadius: 0,
                    }}
                  />
                </div>
              </div>
            )}

            {/* Append target */}
            {mode === 'append' && (
              <div style={{ marginBottom: '14px' }}>
                <label style={labelStyle}>TARGET WATCHLIST</label>
                {existingWatchlists.length === 0 ? (
                  <div style={{ color: GRAY, fontSize: '10px', padding: '8px', backgroundColor: BG, border: `1px solid ${BORDER}` }}>
                    No watchlists available. Switch to "Create new" mode.
                  </div>
                ) : (
                  <select
                    value={targetId}
                    onChange={(e) => setTargetId(e.target.value)}
                    style={{ ...inputStyle, cursor: 'pointer' }}
                  >
                    {existingWatchlists.map(w => (
                      <option key={w.id} value={w.id}>{w.name}</option>
                    ))}
                  </select>
                )}
              </div>
            )}

            {/* Actions */}
            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end', marginTop: '4px' }}>
              <button onClick={handleClose} style={{
                padding: '6px 12px', backgroundColor: 'transparent',
                border: `1px solid ${BORDER}`, color: GRAY,
                fontSize: '10px', fontWeight: 700, cursor: 'pointer',
              }}
                onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = HOVER; }}
                onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                CANCEL
              </button>
              <button
                onClick={handleExecuteImport}
                disabled={mode === 'append' && !targetId}
                style={{
                  padding: '6px 16px', backgroundColor: CYAN, color: '#000',
                  border: 'none', fontSize: '10px', fontWeight: 800, cursor: 'pointer',
                  opacity: (mode === 'append' && !targetId) ? 0.5 : 1,
                }}
              >
                IMPORT {parsedData.stocks.length} SYMBOL{parsedData.stocks.length !== 1 ? 'S' : ''}
              </button>
            </div>
          </div>
        )}

        {/* ─── STEP: importing ─── */}
        {step === 'importing' && (
          <div style={{ textAlign: 'center', padding: '32px 0' }}>
            <style>{`@keyframes wl-spin { to { transform: rotate(360deg); } }`}</style>
            <div style={{
              width: '40px', height: '40px', border: `3px solid ${BORDER}`,
              borderTopColor: CYAN, borderRadius: '50%', margin: '0 auto 16px',
              animation: 'wl-spin 0.8s linear infinite',
            }} />
            <div style={{ color: WHITE, fontSize: '12px', fontWeight: 700, marginBottom: '4px' }}>
              IMPORTING SYMBOLS...
            </div>
            <div style={{ color: CYAN, fontSize: '10px' }}>
              Adding stocks to watchlist
            </div>
          </div>
        )}

        {/* ─── STEP: done ─── */}
        {step === 'done' && result && (
          <div>
            {result.watchlistId ? (
              <div style={{ textAlign: 'center', marginBottom: '20px' }}>
                <CheckCircle size={40} color={GREEN} style={{ marginBottom: '12px' }} />
                <div style={{ color: GREEN, fontSize: '13px', fontWeight: 800, marginBottom: '4px' }}>
                  IMPORT COMPLETE
                </div>
                <div style={{ color: WHITE, fontSize: '11px' }}>
                  <span style={{ color: CYAN, fontWeight: 700 }}>{result.added}</span> symbols added
                  {result.skipped.length > 0 && (
                    <span style={{ color: ORANGE }}> · {result.skipped.length} skipped (already exist)</span>
                  )}
                </div>
              </div>
            ) : (
              <div style={{ textAlign: 'center', marginBottom: '20px' }}>
                <AlertCircle size={40} color={RED} style={{ marginBottom: '12px' }} />
                <div style={{ color: RED, fontSize: '13px', fontWeight: 800 }}>IMPORT FAILED</div>
              </div>
            )}

            {/* Skipped symbols */}
            {result.skipped.length > 0 && (
              <div style={{ marginBottom: '12px' }}>
                <div style={{ fontSize: '9px', color: ORANGE, fontWeight: 700, marginBottom: '4px' }}>
                  SKIPPED (ALREADY IN WATCHLIST)
                </div>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                  {result.skipped.map(s => (
                    <span key={s} style={{
                      padding: '2px 6px', fontSize: '8px', fontWeight: 700,
                      backgroundColor: `${ORANGE}15`, color: ORANGE,
                      border: `1px solid ${ORANGE}30`,
                    }}>{s}</span>
                  ))}
                </div>
              </div>
            )}

            {/* Errors */}
            {result.errors.length > 0 && (
              <div style={{
                marginBottom: '12px', padding: '8px',
                backgroundColor: `${RED}10`, border: `1px solid ${RED}30`,
                maxHeight: '120px', overflowY: 'auto',
              }}>
                <div style={{ fontSize: '9px', color: RED, fontWeight: 700, marginBottom: '4px' }}>ERRORS</div>
                {result.errors.map((e, i) => (
                  <div key={i} style={{ fontSize: '9px', color: RED, lineHeight: '1.6', fontFamily: 'monospace' }}>{e}</div>
                ))}
              </div>
            )}

            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button onClick={handleClose} style={{
                padding: '6px 12px', backgroundColor: 'transparent',
                border: `1px solid ${BORDER}`, color: GRAY,
                fontSize: '10px', fontWeight: 700, cursor: 'pointer',
              }}
                onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = HOVER; }}
                onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                CLOSE
              </button>
              {result.watchlistId && (
                <button
                  onClick={() => { onImportComplete(result); reset(); }}
                  style={{
                    padding: '6px 16px', backgroundColor: CYAN, color: '#000',
                    border: 'none', fontSize: '10px', fontWeight: 800, cursor: 'pointer',
                  }}
                >
                  VIEW WATCHLIST
                </button>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default ImportWatchlistModal;
