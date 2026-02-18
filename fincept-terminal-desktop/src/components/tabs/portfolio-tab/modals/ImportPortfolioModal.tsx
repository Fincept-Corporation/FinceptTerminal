import React, { useState, useCallback } from 'react';
import { Upload, Download, CheckCircle, AlertCircle, ChevronDown, ChevronRight, X } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { portfolioService } from '../../../../services/portfolio/portfolioService';
import type { Portfolio } from '../../../../services/portfolio/portfolioService';
import type { PortfolioExportJSON, ImportMode, ImportResult } from '../types';

interface ImportPortfolioModalProps {
  show: boolean;
  portfolios: Portfolio[];
  onClose: () => void;
  onImportComplete: (result: ImportResult) => void;
  onImport: (
    data: PortfolioExportJSON,
    mode: ImportMode,
    mergeTargetId?: string,
    onProgress?: (current: number, total: number) => void
  ) => Promise<ImportResult>;
}

type Step = 'select-file' | 'configure' | 'importing' | 'done';

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

const ImportPortfolioModal: React.FC<ImportPortfolioModalProps> = ({
  show, portfolios, onClose, onImportComplete, onImport,
}) => {
  const { fontFamily } = useTerminalTheme();

  const [step, setStep] = useState<Step>('select-file');
  const [fileType, setFileType] = useState<'csv' | 'json' | null>(null);
  const [fileName, setFileName] = useState('');
  const [parsedData, setParsedData] = useState<PortfolioExportJSON | null>(null);
  const [parseError, setParseError] = useState<string | null>(null);
  const [importMode, setImportMode] = useState<ImportMode>('new');
  const [mergeTargetId, setMergeTargetId] = useState('');
  const [portfolioNameOverride, setPortfolioNameOverride] = useState('');
  const [progress, setProgress] = useState({ current: 0, total: 0 });
  const [result, setResult] = useState<ImportResult | null>(null);
  const [showErrors, setShowErrors] = useState(false);
  const [isDragging, setIsDragging] = useState(false);

  const reset = useCallback(() => {
    setStep('select-file');
    setFileType(null);
    setFileName('');
    setParsedData(null);
    setParseError(null);
    setImportMode('new');
    setMergeTargetId('');
    setPortfolioNameOverride('');
    setProgress({ current: 0, total: 0 });
    setResult(null);
    setShowErrors(false);
  }, []);

  const handleClose = useCallback(() => {
    reset();
    onClose();
  }, [reset, onClose]);

  const processFileContent = useCallback((content: string, name: string, detectedType: 'csv' | 'json') => {
    setParseError(null);
    try {
      let data: PortfolioExportJSON;
      if (detectedType === 'json') {
        data = portfolioService.parsePortfolioJSON(content);
      } else {
        data = portfolioService.parsePortfolioCSV(content);
      }
      setFileType(detectedType);
      setFileName(name);
      setParsedData(data);
      setPortfolioNameOverride(data.portfolio_name);
      if (portfolios.length > 0) setMergeTargetId(portfolios[0].id);
      setStep('configure');
    } catch (err: any) {
      setParseError(err?.message || 'Failed to parse file');
    }
  }, [portfolios]);

  const downloadTemplate = useCallback((format: 'csv' | 'json') => {
    let content: string;
    let filename: string;

    if (format === 'csv') {
      content = [
        'Portfolio Export',
        '',
        'Portfolio Name:,My Portfolio',
        'Owner:,John Doe',
        'Currency:,USD',
        `Export Date:,${new Date().toISOString()}`,
        '',
        'Holdings',
        'Symbol,Quantity,Avg Buy Price,Current Price,Market Value,Cost Basis,Unrealized P&L,P&L %,Weight %',
        '',
        'Transactions',
        'Date,Symbol,Type,Quantity,Price,Total Value,Notes',
        '2024-01-15T00:00:00.000Z,AAPL,BUY,10,185.50,1855.00,Initial purchase',
        '2024-03-20T00:00:00.000Z,MSFT,BUY,5,420.00,2100.00,',
        '2024-06-01T00:00:00.000Z,AAPL,SELL,3,195.00,585.00,Partial exit',
      ].join('\n');
      filename = 'portfolio_template.csv';
    } else {
      const template = {
        format_version: '1.0',
        portfolio_name: 'My Portfolio',
        owner: 'John Doe',
        currency: 'USD',
        export_date: new Date().toISOString(),
        transactions: [
          { date: '2024-01-15T00:00:00.000Z', symbol: 'AAPL', type: 'BUY', quantity: 10, price: 185.50, total_value: 1855.00, notes: 'Initial purchase' },
          { date: '2024-03-20T00:00:00.000Z', symbol: 'MSFT', type: 'BUY', quantity: 5, price: 420.00, total_value: 2100.00, notes: '' },
          { date: '2024-06-01T00:00:00.000Z', symbol: 'AAPL', type: 'SELL', quantity: 3, price: 195.00, total_value: 585.00, notes: 'Partial exit' },
        ],
      };
      content = JSON.stringify(template, null, 2);
      filename = 'portfolio_template.json';
    }

    const blob = new Blob([content], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, []);

  const handleFileChange = useCallback((file: File) => {
    const name = file.name;
    const detectedType = name.toLowerCase().endsWith('.json') ? 'json' : 'csv';
    const reader = new FileReader();
    reader.onload = (e) => {
      const content = e.target?.result as string;
      processFileContent(content, name, detectedType);
    };
    reader.readAsText(file);
  }, [processFileContent]);

  const handleInputChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) handleFileChange(file);
  }, [handleFileChange]);

  const handleDrop = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    setIsDragging(false);
    const file = e.dataTransfer.files?.[0];
    if (file) handleFileChange(file);
  }, [handleFileChange]);

  const handleExecuteImport = useCallback(async () => {
    if (!parsedData) return;
    setStep('importing');

    // Override portfolio name if user changed it
    const dataToImport: PortfolioExportJSON = {
      ...parsedData,
      portfolio_name: importMode === 'new' ? (portfolioNameOverride || parsedData.portfolio_name) : parsedData.portfolio_name,
    };

    try {
      const importResult = await onImport(
        dataToImport,
        importMode,
        importMode === 'merge' ? mergeTargetId : undefined,
        (current, total) => setProgress({ current, total })
      );
      setResult(importResult);
      setStep('done');
    } catch (err: any) {
      setResult({
        portfolioId: '',
        portfolioName: '',
        transactionsReplayed: 0,
        errors: [err?.message || 'Import failed'],
      });
      setStep('done');
    }
  }, [parsedData, importMode, portfolioNameOverride, mergeTargetId, onImport]);

  const mergeTarget = portfolios.find(p => p.id === mergeTargetId);
  const currencyMismatch =
    importMode === 'merge' && parsedData && mergeTarget && parsedData.currency !== mergeTarget.currency;

  if (!show) return null;

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '7px 10px',
    backgroundColor: DARK_BG,
    color: WHITE,
    border: `1px solid ${BORDER}`,
    fontSize: '11px',
    fontFamily,
    outline: 'none',
    boxSizing: 'border-box',
  };

  const labelStyle: React.CSSProperties = {
    color: GRAY,
    fontSize: '9px',
    fontWeight: 700,
    letterSpacing: '0.5px',
    textTransform: 'uppercase',
    display: 'block',
    marginBottom: '4px',
  };

  return (
    <div style={{
      position: 'fixed', inset: 0, backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 9999,
    }}>
      <div style={{
        backgroundColor: PANEL, border: `2px solid ${CYAN}`,
        padding: '24px', width: '520px', maxWidth: '95vw',
        boxShadow: `0 8px 32px rgba(0,188,212,0.2)`, fontFamily,
        maxHeight: '90vh', overflowY: 'auto',
      }}>
        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '20px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Upload size={14} color={CYAN} />
            <span style={{ fontSize: '13px', fontWeight: 800, color: CYAN, letterSpacing: '1px' }}>
              IMPORT PORTFOLIO
            </span>
          </div>
          <button onClick={handleClose} style={{ background: 'none', border: 'none', color: GRAY, cursor: 'pointer', display: 'flex' }}>
            <X size={14} />
          </button>
        </div>

        {/* Step indicator */}
        <div style={{ display: 'flex', gap: '6px', marginBottom: '20px' }}>
          {(['select-file', 'configure', 'importing', 'done'] as Step[]).map((s, i) => (
            <div key={s} style={{
              flex: 1, height: '2px',
              backgroundColor: step === s ? CYAN : (
                ['select-file', 'configure', 'importing', 'done'].indexOf(step) > i ? `${CYAN}60` : BORDER
              ),
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
              style={{
                border: `2px dashed ${isDragging ? CYAN : BORDER}`,
                backgroundColor: isDragging ? `${CYAN}08` : DARK_BG,
                padding: '32px', textAlign: 'center', cursor: 'pointer',
                transition: 'all 0.2s',
              }}
              onClick={() => document.getElementById('portfolio-file-input')?.click()}
            >
              <Upload size={28} color={isDragging ? CYAN : GRAY} style={{ marginBottom: '12px' }} />
              <div style={{ color: WHITE, fontSize: '12px', fontWeight: 600, marginBottom: '4px' }}>
                Drop file here or click to browse
              </div>
              <div style={{ color: GRAY, fontSize: '10px' }}>
                Supports Fincept CSV and JSON exports
              </div>
              <input
                id="portfolio-file-input"
                type="file"
                accept=".csv,.json"
                style={{ display: 'none' }}
                onChange={handleInputChange}
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

            <div style={{ marginTop: '16px', color: GRAY, fontSize: '9px', lineHeight: '1.6' }}>
              <div style={{ fontWeight: 700, color: WHITE, marginBottom: '4px' }}>SUPPORTED FORMATS</div>
              <div>• <span style={{ color: CYAN }}>CSV</span> — Fincept portfolio export (replays transactions)</div>
              <div>• <span style={{ color: CYAN }}>JSON</span> — Fincept JSON export (full structured data)</div>
            </div>

            {/* Template download */}
            <div style={{
              marginTop: '14px', padding: '10px 12px',
              backgroundColor: `${CYAN}08`, border: `1px solid ${BORDER}`,
            }}>
              <div style={{ fontSize: '9px', color: GRAY, fontWeight: 700, marginBottom: '8px' }}>
                DOWNLOAD TEMPLATE
              </div>
              <div style={{ fontSize: '9px', color: GRAY, marginBottom: '8px' }}>
                New to Fincept import? Download a sample file to see the exact format.
              </div>
              <div style={{ display: 'flex', gap: '6px' }}>
                <button
                  onClick={(e) => { e.stopPropagation(); downloadTemplate('csv'); }}
                  style={{
                    padding: '5px 12px', backgroundColor: 'transparent',
                    border: `1px solid ${CYAN}60`, color: CYAN,
                    fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                    display: 'flex', alignItems: 'center', gap: '4px',
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = `${CYAN}15`; }}
                  onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                >
                  <Download size={9} /> TEMPLATE.CSV
                </button>
                <button
                  onClick={(e) => { e.stopPropagation(); downloadTemplate('json'); }}
                  style={{
                    padding: '5px 12px', backgroundColor: 'transparent',
                    border: `1px solid ${CYAN}60`, color: CYAN,
                    fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                    display: 'flex', alignItems: 'center', gap: '4px',
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = `${CYAN}15`; }}
                  onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                >
                  <Download size={9} /> TEMPLATE.JSON
                </button>
              </div>
            </div>
          </div>
        )}

        {/* ─── STEP: configure ─── */}
        {step === 'configure' && parsedData && (
          <div>
            {/* File preview */}
            <div style={{
              padding: '10px 12px', backgroundColor: DARK_BG,
              border: `1px solid ${BORDER}`, marginBottom: '16px',
            }}>
              <div style={{ fontSize: '9px', color: GRAY, fontWeight: 700, marginBottom: '6px' }}>FILE DETECTED</div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
                {[
                  { l: 'FILE', v: fileName },
                  { l: 'FORMAT', v: fileType?.toUpperCase() || '' },
                  { l: 'PORTFOLIO', v: parsedData.portfolio_name },
                  { l: 'CURRENCY', v: parsedData.currency },
                  { l: 'TRANSACTIONS', v: String(parsedData.transactions.length) },
                  { l: 'OWNER', v: parsedData.owner || '—' },
                ].map(({ l, v }) => (
                  <div key={l}>
                    <span style={{ fontSize: '8px', color: GRAY }}>{l}: </span>
                    <span style={{ fontSize: '9px', color: WHITE, fontWeight: 600 }}>{v}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Import mode */}
            <div style={{ marginBottom: '14px' }}>
              <label style={labelStyle}>IMPORT MODE</label>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {([
                  { value: 'new', label: 'Create new portfolio', desc: 'Import as a fresh portfolio' },
                  { value: 'merge', label: 'Merge into existing', desc: 'Add transactions to an existing portfolio' },
                ] as { value: ImportMode; label: string; desc: string }[]).map(opt => (
                  <label key={opt.value} style={{
                    display: 'flex', alignItems: 'flex-start', gap: '8px',
                    padding: '8px 10px', cursor: 'pointer',
                    backgroundColor: importMode === opt.value ? `${CYAN}10` : 'transparent',
                    border: `1px solid ${importMode === opt.value ? CYAN : BORDER}`,
                  }}>
                    <input
                      type="radio"
                      name="importMode"
                      value={opt.value}
                      checked={importMode === opt.value}
                      onChange={() => setImportMode(opt.value)}
                      style={{ marginTop: '2px', accentColor: CYAN }}
                    />
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 700, color: importMode === opt.value ? CYAN : WHITE }}>
                        {opt.label}
                      </div>
                      <div style={{ fontSize: '9px', color: GRAY }}>{opt.desc}</div>
                    </div>
                  </label>
                ))}
              </div>
            </div>

            {/* Conditional: new portfolio name */}
            {importMode === 'new' && (
              <div style={{ marginBottom: '14px' }}>
                <label style={labelStyle}>PORTFOLIO NAME</label>
                <input
                  type="text"
                  value={portfolioNameOverride}
                  onChange={(e) => setPortfolioNameOverride(e.target.value)}
                  style={inputStyle}
                  onFocus={(e) => { e.currentTarget.style.borderColor = CYAN; }}
                  onBlur={(e) => { e.currentTarget.style.borderColor = BORDER; }}
                />
              </div>
            )}

            {/* Conditional: merge target */}
            {importMode === 'merge' && (
              <div style={{ marginBottom: '14px' }}>
                <label style={labelStyle}>TARGET PORTFOLIO</label>
                {portfolios.length === 0 ? (
                  <div style={{ color: GRAY, fontSize: '10px', padding: '8px', backgroundColor: DARK_BG, border: `1px solid ${BORDER}` }}>
                    No portfolios available. Switch to "Create new" mode.
                  </div>
                ) : (
                  <select
                    value={mergeTargetId}
                    onChange={(e) => setMergeTargetId(e.target.value)}
                    style={{ ...inputStyle, cursor: 'pointer' }}
                  >
                    {portfolios.map(p => (
                      <option key={p.id} value={p.id}>{p.name} ({p.currency})</option>
                    ))}
                  </select>
                )}
              </div>
            )}

            {/* Currency mismatch warning */}
            {currencyMismatch && (
              <div style={{
                padding: '8px 12px', backgroundColor: `${ORANGE}12`,
                border: `1px solid ${ORANGE}40`, marginBottom: '14px',
                display: 'flex', alignItems: 'center', gap: '8px',
              }}>
                <AlertCircle size={12} color={ORANGE} />
                <span style={{ color: ORANGE, fontSize: '10px' }}>
                  Currency mismatch: file is {parsedData.currency}, target is {mergeTarget?.currency}. Prices will be imported as-is.
                </span>
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
                disabled={importMode === 'merge' && !mergeTargetId}
                style={{
                  padding: '6px 16px',
                  backgroundColor: CYAN,
                  color: '#000',
                  border: 'none',
                  fontSize: '10px', fontWeight: 800, cursor: 'pointer',
                  opacity: (importMode === 'merge' && !mergeTargetId) ? 0.5 : 1,
                }}
              >
                IMPORT {parsedData.transactions.length} TRANSACTIONS
              </button>
            </div>
          </div>
        )}

        {/* ─── STEP: importing ─── */}
        {step === 'importing' && (
          <div style={{ textAlign: 'center', padding: '20px 0' }}>
            <div style={{
              width: '48px', height: '48px', border: `3px solid ${BORDER}`,
              borderTopColor: CYAN, borderRadius: '50%', margin: '0 auto 16px',
              animation: 'spin 0.8s linear infinite',
            }} />
            <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
            <div style={{ color: WHITE, fontSize: '12px', fontWeight: 700, marginBottom: '8px' }}>
              IMPORTING...
            </div>
            <div style={{ color: CYAN, fontSize: '10px', marginBottom: '16px' }}>
              {progress.total > 0 ? `${progress.current} / ${progress.total} transactions` : 'Preparing...'}
            </div>
            {progress.total > 0 && (
              <div style={{ backgroundColor: BORDER, height: '4px', borderRadius: '2px', overflow: 'hidden' }}>
                <div style={{
                  height: '100%', backgroundColor: CYAN,
                  width: `${(progress.current / progress.total) * 100}%`,
                  transition: 'width 0.3s',
                }} />
              </div>
            )}
          </div>
        )}

        {/* ─── STEP: done ─── */}
        {step === 'done' && result && (
          <div>
            {result.portfolioId ? (
              <div style={{ textAlign: 'center', marginBottom: '20px' }}>
                <CheckCircle size={40} color={GREEN} style={{ marginBottom: '12px' }} />
                <div style={{ color: GREEN, fontSize: '13px', fontWeight: 800, marginBottom: '4px' }}>
                  IMPORT COMPLETE
                </div>
                <div style={{ color: WHITE, fontSize: '11px' }}>
                  <span style={{ color: CYAN, fontWeight: 700 }}>{result.transactionsReplayed}</span> transactions replayed
                  {result.errors.length > 0 && (
                    <span style={{ color: ORANGE }}> · {result.errors.length} errors</span>
                  )}
                </div>
              </div>
            ) : (
              <div style={{ textAlign: 'center', marginBottom: '20px' }}>
                <AlertCircle size={40} color={RED} style={{ marginBottom: '12px' }} />
                <div style={{ color: RED, fontSize: '13px', fontWeight: 800 }}>IMPORT FAILED</div>
              </div>
            )}

            {result.errors.length > 0 && (
              <div style={{ marginBottom: '16px' }}>
                <button
                  onClick={() => setShowErrors(!showErrors)}
                  style={{
                    display: 'flex', alignItems: 'center', gap: '4px',
                    background: 'none', border: 'none', color: ORANGE,
                    fontSize: '10px', fontWeight: 700, cursor: 'pointer', padding: 0,
                  }}
                >
                  {showErrors ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                  {result.errors.length} ERRORS
                </button>
                {showErrors && (
                  <div style={{
                    marginTop: '6px', maxHeight: '150px', overflowY: 'auto',
                    backgroundColor: DARK_BG, border: `1px solid ${BORDER}`, padding: '8px',
                  }}>
                    {result.errors.map((e, i) => (
                      <div key={i} style={{ fontSize: '9px', color: ORANGE, lineHeight: '1.6', fontFamily: 'monospace' }}>
                        {e}
                      </div>
                    ))}
                  </div>
                )}
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
              {result.portfolioId && (
                <button onClick={() => { onImportComplete(result); reset(); }} style={{
                  padding: '6px 16px', backgroundColor: CYAN, color: '#000',
                  border: 'none', fontSize: '10px', fontWeight: 800, cursor: 'pointer',
                }}>
                  OPEN PORTFOLIO
                </button>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default ImportPortfolioModal;
