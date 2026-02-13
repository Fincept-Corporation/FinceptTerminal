import React, { useState, useRef, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import {
  Play, Save, Plus, X, Trash2, ChevronUp, ChevronDown,
  Upload, FileText, Package, Code, Type,
  Loader2, Download, RotateCcw, Search, History,
  PlayCircle, StopCircle, Terminal,
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { open, save } from '@tauri-apps/plugin-dialog';
import { notebookService } from '@/services/core/notebookService';

// ─── FINCEPT Design System Colors ────────────────────────────────────────────
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

const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ─── Types matching Rust structs (camelCase due to serde rename) ─────────────
interface CellOutput {
  outputType: string;
  data: Record<string, string[]> | null;
  text: string[] | null;
  name: string | null;
  ename: string | null;
  evalue: string | null;
  traceback: string[] | null;
}

interface NotebookCell {
  id: string;
  cellType: string;
  source: string[];
  outputs: CellOutput[];
  executionCount: number | null;
  metadata: Record<string, unknown>;
}

interface NotebookMetadata {
  kernelspec: { displayName: string; language: string; name: string };
  languageInfo: { name: string; version: string; mimetype: string; fileExtension: string };
}

interface JupyterNotebook {
  cells: NotebookCell[];
  metadata: NotebookMetadata;
  nbformat: number;
  nbformatMinor: number;
}

interface ExecutionResult {
  success: boolean;
  outputs: CellOutput[];
  executionCount: number;
  error: string | null;
}

// ─── Scrollbar CSS ───────────────────────────────────────────────────────────
const notebookCSS = `
.nb-scroll::-webkit-scrollbar { width: 6px; height: 6px; }
.nb-scroll::-webkit-scrollbar-track { background: ${F.DARK_BG}; }
.nb-scroll::-webkit-scrollbar-thumb { background: ${F.BORDER}; border-radius: 3px; }
.nb-scroll::-webkit-scrollbar-thumb:hover { background: ${F.MUTED}; }
`;

// ─── Component ───────────────────────────────────────────────────────────────
export const JupyterNotebookPanel: React.FC = () => {
  const { t } = useTranslation('codeEditor');

  // ─── State ─────────────────────────────────────────────────────────────────
  const [notebook, setNotebook] = useState<JupyterNotebook | null>(null);
  const [filePath, setFilePath] = useState<string | null>(null);
  const [unsaved, setUnsaved] = useState(false);
  const [executionCounter, setExecutionCounter] = useState(1);
  const [runningCellId, setRunningCellId] = useState<string | null>(null);
  const [runningAll, setRunningAll] = useState(false);
  const [pythonVersion, setPythonVersion] = useState<string>('');
  const [selectedCellId, setSelectedCellId] = useState<string | null>(null);
  const [notebookName, setNotebookName] = useState<string>('fincept1');

  // Sidebar
  const [sidebarMode, setSidebarMode] = useState<'none' | 'history' | 'packages'>('none');
  const [historyItems, setHistoryItems] = useState<Array<{
    id?: number; name: string; path: string | null; updated_at: string; cell_count: number;
  }>>([]);
  const [historySearch, setHistorySearch] = useState('');

  // Packages
  const [packages, setPackages] = useState<string[]>([]);
  const [packagesLoading, setPackagesLoading] = useState(false);
  const [installInput, setInstallInput] = useState('');
  const [installing, setInstalling] = useState(false);

  const cellsContainerRef = useRef<HTMLDivElement>(null);
  const notebookCounterRef = useRef(1);

  // ─── Init ──────────────────────────────────────────────────────────────────
  useEffect(() => {
    createNewNotebook();
    loadPythonVersion();
  }, []);

  const loadPythonVersion = async () => {
    try {
      const ver = await invoke<string>('get_python_version');
      setPythonVersion(ver);
    } catch {
      setPythonVersion('Python N/A');
    }
  };

  // ─── Notebook Operations ───────────────────────────────────────────────────
  const createNewNotebook = async () => {
    try {
      const nb = await invoke<JupyterNotebook>('create_new_notebook');

      // Add branded markdown cell at the top
      const brandCell: NotebookCell = {
        id: crypto.randomUUID(),
        cellType: 'markdown',
        source: [
          '# Fincept Terminal - Notebook\n',
          '\n',
          '**Fincept Terminal** | Professional Financial Intelligence Platform\n',
          '\n',
          '---\n',
          '\n',
          '> *CFA-Level Analytics | AI-Powered Automation | Global Market Intelligence*\n',
          '\n',
          'Use this notebook to run Python-based financial analysis, build quantitative models, and explore market data.\n',
        ],
        outputs: [],
        executionCount: null,
        metadata: {},
      };

      nb.cells = [brandCell, ...nb.cells];

      const name = `fincept${notebookCounterRef.current}`;
      notebookCounterRef.current++;

      setNotebook(nb);
      setFilePath(null);
      setNotebookName(name);
      setUnsaved(false);
      setExecutionCounter(1);
      setSelectedCellId(nb.cells[1]?.id || nb.cells[0]?.id || null);
    } catch (err) {
      console.error('Failed to create notebook:', err);
    }
  };

  const openNotebook = async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [{ name: 'Jupyter Notebook', extensions: ['ipynb'] }],
      });
      if (!selected) return;
      const path = selected as string;
      const nb = await invoke<JupyterNotebook>('open_notebook', { filePath: path });
      setNotebook(nb);
      setFilePath(path);
      setNotebookName(path.split(/[/\\]/).pop()?.replace('.ipynb', '') || 'fincept1');
      setUnsaved(false);
      setExecutionCounter(1);
      setSelectedCellId(nb.cells[0]?.id || null);
    } catch (err) {
      console.error('Failed to open notebook:', err);
    }
  };

  const saveNotebook = async () => {
    if (!notebook) return;
    try {
      let path = filePath;
      if (!path) {
        const selected = await save({
          filters: [{ name: 'Jupyter Notebook', extensions: ['ipynb'] }],
        });
        if (!selected) return;
        path = selected as string;
      }
      await invoke('save_notebook', { filePath: path, notebook });
      setFilePath(path);
      setUnsaved(false);

      const name = path.split(/[/\\]/).pop() || `${notebookName}.ipynb`;
      const now = new Date().toISOString();
      await notebookService.saveToHistory({
        name,
        content: JSON.stringify(notebook),
        path,
        created_at: now,
        updated_at: now,
        cell_count: notebook.cells.length,
        execution_count: executionCounter,
      });
    } catch (err) {
      console.error('Failed to save notebook:', err);
    }
  };

  // ─── Cell Operations ──────────────────────────────────────────────────────
  const getCellSource = (cell: NotebookCell): string => cell.source.join('');

  const setCellSource = (cellId: string, value: string) => {
    if (!notebook) return;
    setNotebook({
      ...notebook,
      cells: notebook.cells.map(c =>
        c.id === cellId ? { ...c, source: value.split('\n').map((line, i, arr) => i < arr.length - 1 ? line + '\n' : line) } : c
      ),
    });
    setUnsaved(true);
  };

  const addCell = (afterId: string | null, cellType: string = 'code') => {
    if (!notebook) return;
    const newCell: NotebookCell = {
      id: crypto.randomUUID(),
      cellType,
      source: [],
      outputs: [],
      executionCount: null,
      metadata: {},
    };
    const cells = [...notebook.cells];
    if (afterId) {
      const idx = cells.findIndex(c => c.id === afterId);
      cells.splice(idx + 1, 0, newCell);
    } else {
      cells.push(newCell);
    }
    setNotebook({ ...notebook, cells });
    setSelectedCellId(newCell.id);
    setUnsaved(true);
  };

  const deleteCell = (cellId: string) => {
    if (!notebook || notebook.cells.length <= 1) return;
    const cells = notebook.cells.filter(c => c.id !== cellId);
    setNotebook({ ...notebook, cells });
    if (selectedCellId === cellId) {
      setSelectedCellId(cells[0]?.id || null);
    }
    setUnsaved(true);
  };

  const moveCell = (cellId: string, direction: 'up' | 'down') => {
    if (!notebook) return;
    const cells = [...notebook.cells];
    const idx = cells.findIndex(c => c.id === cellId);
    if (direction === 'up' && idx > 0) {
      [cells[idx - 1], cells[idx]] = [cells[idx], cells[idx - 1]];
    } else if (direction === 'down' && idx < cells.length - 1) {
      [cells[idx], cells[idx + 1]] = [cells[idx + 1], cells[idx]];
    }
    setNotebook({ ...notebook, cells });
    setUnsaved(true);
  };

  const toggleCellType = (cellId: string) => {
    if (!notebook) return;
    setNotebook({
      ...notebook,
      cells: notebook.cells.map(c =>
        c.id === cellId ? { ...c, cellType: c.cellType === 'code' ? 'markdown' : 'code', outputs: [] } : c
      ),
    });
    setUnsaved(true);
  };

  const clearCellOutput = (cellId: string) => {
    if (!notebook) return;
    setNotebook({
      ...notebook,
      cells: notebook.cells.map(c =>
        c.id === cellId ? { ...c, outputs: [], executionCount: null } : c
      ),
    });
  };

  // ─── Execution ────────────────────────────────────────────────────────────
  const runCell = async (cellId: string) => {
    if (!notebook) return;
    const cell = notebook.cells.find(c => c.id === cellId);
    if (!cell || cell.cellType !== 'code') return;

    const code = getCellSource(cell);
    if (!code.trim()) return;

    setRunningCellId(cellId);
    try {
      const result = await invoke<ExecutionResult>('execute_python_code', {
        code,
        executionCount: executionCounter,
      });

      setNotebook(prev => {
        if (!prev) return prev;
        return {
          ...prev,
          cells: prev.cells.map(c =>
            c.id === cellId ? { ...c, outputs: result.outputs, executionCount: result.executionCount } : c
          ),
        };
      });
      setExecutionCounter(prev => prev + 1);
    } catch (err) {
      setNotebook(prev => {
        if (!prev) return prev;
        return {
          ...prev,
          cells: prev.cells.map(c =>
            c.id === cellId
              ? {
                  ...c,
                  outputs: [{
                    outputType: 'error',
                    data: null,
                    text: null,
                    name: null,
                    ename: 'InvocationError',
                    evalue: String(err),
                    traceback: [String(err)],
                  }],
                  executionCount: executionCounter,
                }
              : c
          ),
        };
      });
      setExecutionCounter(prev => prev + 1);
    } finally {
      setRunningCellId(null);
    }
  };

  const runAllCells = async () => {
    if (!notebook) return;
    setRunningAll(true);
    for (const cell of notebook.cells) {
      if (cell.cellType === 'code') {
        await runCell(cell.id);
      }
    }
    setRunningAll(false);
  };

  // ─── History ──────────────────────────────────────────────────────────────
  const loadHistory = async () => {
    try {
      const items = historySearch
        ? await notebookService.searchHistory(historySearch)
        : await notebookService.getHistory();
      setHistoryItems(items);
    } catch {
      setHistoryItems([]);
    }
  };

  useEffect(() => {
    if (sidebarMode === 'history') loadHistory();
  }, [sidebarMode, historySearch]);

  const loadFromHistory = async (item: typeof historyItems[0]) => {
    try {
      if (item.path) {
        const nb = await invoke<JupyterNotebook>('open_notebook', { filePath: item.path });
        setNotebook(nb);
        setFilePath(item.path);
      } else {
        const content = await notebookService.getNotebookById(item.id!);
        if (content?.content) {
          const nb = JSON.parse(content.content) as JupyterNotebook;
          setNotebook(nb);
          setFilePath(null);
        }
      }
      setUnsaved(false);
      setSidebarMode('none');
    } catch (err) {
      console.error('Failed to load from history:', err);
    }
  };

  const deleteHistoryItem = async (id: number) => {
    await notebookService.deleteFromHistory(id);
    loadHistory();
  };

  // ─── Packages ─────────────────────────────────────────────────────────────
  const loadPackages = async () => {
    setPackagesLoading(true);
    try {
      const pkgs = await invoke<string[]>('list_python_packages');
      setPackages(pkgs);
    } catch {
      setPackages([]);
    } finally {
      setPackagesLoading(false);
    }
  };

  useEffect(() => {
    if (sidebarMode === 'packages') loadPackages();
  }, [sidebarMode]);

  const installPackage = async () => {
    if (!installInput.trim()) return;
    setInstalling(true);
    try {
      await invoke('install_python_package', { packageName: installInput.trim() });
      setInstallInput('');
      loadPackages();
    } catch (err) {
      console.error('Failed to install package:', err);
    } finally {
      setInstalling(false);
    }
  };

  // ─── Keyboard ─────────────────────────────────────────────────────────────
  const handleCellKeyDown = useCallback((e: React.KeyboardEvent, cellId: string) => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      runCell(cellId);
    }
    if (e.shiftKey && e.key === 'Enter') {
      e.preventDefault();
      runCell(cellId);
      if (notebook) {
        const idx = notebook.cells.findIndex(c => c.id === cellId);
        if (idx < notebook.cells.length - 1) {
          setSelectedCellId(notebook.cells[idx + 1].id);
        } else {
          addCell(cellId);
        }
      }
    }
  }, [notebook, executionCounter]);

  // ─── Render: Cell Output ──────────────────────────────────────────────────
  const renderCellOutput = (output: CellOutput, idx: number) => {
    if (output.outputType === 'stream' && output.text) {
      return (
        <pre key={idx} style={{
          margin: 0, padding: '10px 16px',
          fontSize: '11px', lineHeight: '1.6',
          fontFamily: FONT, color: F.WHITE,
          whiteSpace: 'pre-wrap', wordBreak: 'break-word',
          backgroundColor: F.DARK_BG,
        }}>
          {output.text.join('\n')}
        </pre>
      );
    }

    if (output.outputType === 'error') {
      return (
        <div key={idx} style={{
          padding: '10px 16px',
          backgroundColor: `${F.RED}08`,
          borderLeft: `3px solid ${F.RED}`,
        }}>
          {output.ename && (
            <div style={{
              color: F.RED, fontSize: '11px', fontWeight: 700,
              fontFamily: FONT, marginBottom: '6px', letterSpacing: '0.3px',
            }}>
              {output.ename}: {output.evalue}
            </div>
          )}
          {output.traceback && (
            <pre style={{
              margin: 0, fontSize: '10px', lineHeight: '1.5',
              fontFamily: FONT, color: F.RED, opacity: 0.8,
              whiteSpace: 'pre-wrap', wordBreak: 'break-word',
            }}>
              {output.traceback.join('\n')}
            </pre>
          )}
        </div>
      );
    }

    if (output.outputType === 'execute_result' || output.outputType === 'display_data') {
      if (output.data) {
        const textData = output.data['text/plain'];
        if (textData) {
          return (
            <pre key={idx} style={{
              margin: 0, padding: '10px 16px',
              fontSize: '11px', lineHeight: '1.6',
              fontFamily: FONT, color: F.CYAN,
              whiteSpace: 'pre-wrap',
              backgroundColor: F.DARK_BG,
            }}>
              {textData.join('\n')}
            </pre>
          );
        }
      }
    }

    return null;
  };

  // ─── Loading state ────────────────────────────────────────────────────────
  if (!notebook) {
    return (
      <div style={{
        width: '100%', height: '100%',
        display: 'flex', flexDirection: 'column',
        alignItems: 'center', justifyContent: 'center',
        backgroundColor: F.DARK_BG, color: F.MUTED, fontSize: '10px',
        fontFamily: FONT, gap: '8px',
      }}>
        <Loader2 size={24} className="animate-spin" style={{ color: F.ORANGE, opacity: 0.5 }} />
        <span style={{ letterSpacing: '0.5px' }}>INITIALIZING NOTEBOOK...</span>
      </div>
    );
  }

  // ─── Computed ─────────────────────────────────────────────────────────────
  const codeCellCount = notebook.cells.filter(c => c.cellType === 'code').length;
  const mdCellCount = notebook.cells.filter(c => c.cellType === 'markdown').length;
  const executedCount = notebook.cells.filter(c => c.executionCount != null).length;
  const fileName = filePath ? filePath.split(/[/\\]/).pop() : `${notebookName}.ipynb`;

  // ─── Render ────────────────────────────────────────────────────────────────
  return (
    <div style={{
      width: '100%', height: '100%',
      display: 'flex', flexDirection: 'column',
      backgroundColor: F.DARK_BG,
    }}>
      <style dangerouslySetInnerHTML={{ __html: notebookCSS }} />

      {/* ═══ Toolbar ═══ */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        padding: '6px 16px',
        display: 'flex', alignItems: 'center', gap: '8px',
        minHeight: '38px',
      }}>
        {/* File actions group */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <button onClick={createNewNotebook} title="New Notebook" style={{
            padding: '6px 10px', backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`, borderRadius: '2px',
            color: F.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '5px',
            fontSize: '9px', fontWeight: 700, fontFamily: FONT, letterSpacing: '0.5px',
            transition: 'all 0.2s',
          }}>
            <FileText size={11} /> NEW
          </button>
          <button onClick={openNotebook} title="Open .ipynb" style={{
            padding: '6px 10px', backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`, borderRadius: '2px',
            color: F.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '5px',
            fontSize: '9px', fontWeight: 700, fontFamily: FONT, letterSpacing: '0.5px',
            transition: 'all 0.2s',
          }}>
            <Upload size={11} /> OPEN
          </button>
          <button onClick={saveNotebook} title="Save .ipynb" style={{
            padding: '6px 10px', backgroundColor: 'transparent',
            border: `1px solid ${unsaved ? F.YELLOW : F.BORDER}`, borderRadius: '2px',
            color: unsaved ? F.YELLOW : F.GRAY,
            cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '5px',
            fontSize: '9px', fontWeight: 700, fontFamily: FONT, letterSpacing: '0.5px',
            transition: 'all 0.2s',
          }}>
            <Save size={11} /> SAVE
          </button>
        </div>

        {/* Divider */}
        <div style={{ width: '1px', height: '20px', backgroundColor: F.BORDER }} />

        {/* Run actions */}
        <button
          onClick={runAllCells}
          disabled={runningAll}
          title="Run All Cells"
          style={{
            padding: '6px 16px', display: 'flex', alignItems: 'center', gap: '6px',
            backgroundColor: runningAll ? F.MUTED : F.ORANGE,
            color: F.DARK_BG, border: 'none', borderRadius: '2px',
            fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
            cursor: runningAll ? 'not-allowed' : 'pointer', fontFamily: FONT,
            transition: 'all 0.2s',
          }}
        >
          {runningAll
            ? <><Loader2 size={11} className="animate-spin" /> RUNNING...</>
            : <><PlayCircle size={11} /> RUN ALL</>
          }
        </button>

        {runningAll && (
          <button
            onClick={() => setRunningAll(false)}
            style={{
              padding: '6px 10px', backgroundColor: `${F.RED}20`,
              border: `1px solid ${F.RED}`, borderRadius: '2px',
              color: F.RED, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '5px',
              fontSize: '9px', fontWeight: 700, fontFamily: FONT, letterSpacing: '0.5px',
            }}
          >
            <StopCircle size={11} /> STOP
          </button>
        )}

        {/* Spacer */}
        <div style={{ flex: 1 }} />

        {/* File name display */}
        <div style={{
          padding: '4px 10px', backgroundColor: F.PANEL_BG,
          border: `1px solid ${F.BORDER}`, borderRadius: '2px',
          display: 'flex', alignItems: 'center', gap: '6px',
        }}>
          <Terminal size={10} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '10px', color: F.WHITE, fontFamily: FONT, fontWeight: 700 }}>
            {fileName}
          </span>
          {unsaved && (
            <span style={{
              padding: '1px 5px', backgroundColor: `${F.YELLOW}20`,
              color: F.YELLOW, fontSize: '8px', fontWeight: 700,
              borderRadius: '2px', letterSpacing: '0.5px',
            }}>
              MODIFIED
            </span>
          )}
        </div>

        {/* Divider */}
        <div style={{ width: '1px', height: '20px', backgroundColor: F.BORDER }} />

        {/* Sidebar toggles */}
        <button
          onClick={() => setSidebarMode(sidebarMode === 'packages' ? 'none' : 'packages')}
          title="Python Packages"
          style={{
            padding: '6px 10px', display: 'flex', alignItems: 'center', gap: '5px',
            backgroundColor: sidebarMode === 'packages' ? F.ORANGE : 'transparent',
            color: sidebarMode === 'packages' ? F.DARK_BG : F.GRAY,
            border: 'none', borderRadius: '2px',
            fontSize: '9px', fontWeight: 700, fontFamily: FONT, letterSpacing: '0.5px',
            cursor: 'pointer', transition: 'all 0.2s',
          }}
        >
          <Package size={11} /> PACKAGES
        </button>
        <button
          onClick={() => setSidebarMode(sidebarMode === 'history' ? 'none' : 'history')}
          title="Notebook History"
          style={{
            padding: '6px 10px', display: 'flex', alignItems: 'center', gap: '5px',
            backgroundColor: sidebarMode === 'history' ? F.ORANGE : 'transparent',
            color: sidebarMode === 'history' ? F.DARK_BG : F.GRAY,
            border: 'none', borderRadius: '2px',
            fontSize: '9px', fontWeight: 700, fontFamily: FONT, letterSpacing: '0.5px',
            cursor: 'pointer', transition: 'all 0.2s',
          }}
        >
          <History size={11} /> HISTORY
        </button>
      </div>

      {/* ═══ Main Area ═══ */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>

        {/* ─── Cells Panel ─── */}
        <div ref={cellsContainerRef} className="nb-scroll" style={{
          flex: 1, overflowY: 'auto',
          padding: '16px 24px',
        }}>
          {notebook.cells.map((cell, cellIdx) => {
            const isSelected = selectedCellId === cell.id;
            const isRunning = runningCellId === cell.id;
            const hasError = cell.outputs.some(o => o.outputType === 'error');
            const hasSuccess = cell.outputs.length > 0 && !hasError;
            const hasOutput = cell.outputs.length > 0;

            // Cell accent color
            const accentColor = isRunning ? F.YELLOW
              : hasError ? F.RED
              : hasSuccess ? F.GREEN
              : isSelected ? F.ORANGE
              : F.BORDER;

            return (
              <div key={cell.id}>
                {/* ── Cell Container ── */}
                <div
                  onClick={() => setSelectedCellId(cell.id)}
                  style={{
                    backgroundColor: F.PANEL_BG,
                    border: `1px solid ${accentColor}`,
                    borderRadius: '2px',
                    borderLeft: `3px solid ${accentColor}`,
                    overflow: 'hidden',
                    transition: 'all 0.2s',
                    marginBottom: '2px',
                  }}
                >
                  {/* Cell header bar */}
                  <div style={{
                    display: 'flex', alignItems: 'center', gap: '6px',
                    padding: '6px 12px',
                    backgroundColor: F.HEADER_BG,
                    borderBottom: `1px solid ${F.BORDER}`,
                  }}>
                    {/* Execution count badge */}
                    <div style={{
                      minWidth: '48px',
                      padding: '2px 6px',
                      backgroundColor: cell.cellType === 'code'
                        ? (isRunning ? `${F.YELLOW}20` : hasError ? `${F.RED}20` : hasSuccess ? `${F.GREEN}20` : `${F.ORANGE}15`)
                        : `${F.PURPLE}15`,
                      borderRadius: '2px',
                      textAlign: 'center',
                    }}>
                      <span style={{
                        fontSize: '10px', fontFamily: FONT, fontWeight: 700,
                        color: cell.cellType === 'code'
                          ? (isRunning ? F.YELLOW : hasError ? F.RED : hasSuccess ? F.GREEN : F.ORANGE)
                          : F.PURPLE,
                      }}>
                        {cell.cellType === 'code'
                          ? (isRunning ? 'In [*]' : `In [${cell.executionCount ?? ' '}]`)
                          : 'MD'
                        }
                      </span>
                    </div>

                    {/* Cell type label */}
                    <span style={{
                      fontSize: '9px', fontWeight: 700, color: F.GRAY,
                      letterSpacing: '0.5px', fontFamily: FONT,
                    }}>
                      {cell.cellType === 'code' ? 'PYTHON' : 'MARKDOWN'}
                    </span>

                    <div style={{ flex: 1 }} />

                    {/* Cell actions (always visible on selected) */}
                    {isSelected && (
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                        {cell.cellType === 'code' && (
                          <button
                            onClick={(e) => { e.stopPropagation(); runCell(cell.id); }}
                            disabled={isRunning}
                            title="Run Cell (Ctrl+Enter)"
                            style={{
                              padding: '3px 8px', display: 'flex', alignItems: 'center', gap: '4px',
                              backgroundColor: isRunning ? `${F.YELLOW}20` : `${F.GREEN}20`,
                              border: `1px solid ${isRunning ? F.YELLOW : F.GREEN}`,
                              borderRadius: '2px', cursor: isRunning ? 'not-allowed' : 'pointer',
                              color: isRunning ? F.YELLOW : F.GREEN,
                              fontSize: '9px', fontWeight: 700, fontFamily: FONT, letterSpacing: '0.5px',
                              transition: 'all 0.2s',
                            }}
                          >
                            {isRunning ? <Loader2 size={10} className="animate-spin" /> : <Play size={10} />}
                            {isRunning ? 'RUNNING' : 'RUN'}
                          </button>
                        )}

                        <div style={{ width: '1px', height: '16px', backgroundColor: F.BORDER }} />

                        <button onClick={(e) => { e.stopPropagation(); toggleCellType(cell.id); }} title="Toggle Code/Markdown" style={{
                          padding: '3px 6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                          borderRadius: '2px', color: F.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center',
                          transition: 'all 0.2s',
                        }}>
                          {cell.cellType === 'code' ? <Type size={10} /> : <Code size={10} />}
                        </button>
                        <button onClick={(e) => { e.stopPropagation(); moveCell(cell.id, 'up'); }} disabled={cellIdx === 0} title="Move Up" style={{
                          padding: '3px 6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                          borderRadius: '2px', color: cellIdx === 0 ? F.MUTED : F.GRAY, cursor: cellIdx === 0 ? 'default' : 'pointer',
                          display: 'flex', alignItems: 'center', opacity: cellIdx === 0 ? 0.5 : 1,
                          transition: 'all 0.2s',
                        }}>
                          <ChevronUp size={10} />
                        </button>
                        <button onClick={(e) => { e.stopPropagation(); moveCell(cell.id, 'down'); }} disabled={cellIdx === notebook.cells.length - 1} title="Move Down" style={{
                          padding: '3px 6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                          borderRadius: '2px',
                          color: cellIdx === notebook.cells.length - 1 ? F.MUTED : F.GRAY,
                          cursor: cellIdx === notebook.cells.length - 1 ? 'default' : 'pointer',
                          display: 'flex', alignItems: 'center',
                          opacity: cellIdx === notebook.cells.length - 1 ? 0.5 : 1,
                          transition: 'all 0.2s',
                        }}>
                          <ChevronDown size={10} />
                        </button>
                        {hasOutput && (
                          <button onClick={(e) => { e.stopPropagation(); clearCellOutput(cell.id); }} title="Clear Output" style={{
                            padding: '3px 6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                            borderRadius: '2px', color: F.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center',
                            transition: 'all 0.2s',
                          }}>
                            <RotateCcw size={10} />
                          </button>
                        )}

                        <div style={{ width: '1px', height: '16px', backgroundColor: F.BORDER }} />

                        <button onClick={(e) => { e.stopPropagation(); deleteCell(cell.id); }} disabled={notebook.cells.length <= 1} title="Delete Cell" style={{
                          padding: '3px 6px', backgroundColor: notebook.cells.length <= 1 ? 'transparent' : `${F.RED}10`,
                          border: `1px solid ${notebook.cells.length <= 1 ? F.MUTED : F.RED}`,
                          borderRadius: '2px', color: notebook.cells.length <= 1 ? F.MUTED : F.RED,
                          cursor: notebook.cells.length <= 1 ? 'default' : 'pointer',
                          display: 'flex', alignItems: 'center',
                          opacity: notebook.cells.length <= 1 ? 0.5 : 1,
                          transition: 'all 0.2s',
                        }}>
                          <Trash2 size={10} />
                        </button>
                      </div>
                    )}
                  </div>

                  {/* Cell editor textarea */}
                  <textarea
                    value={getCellSource(cell)}
                    onChange={(e) => setCellSource(cell.id, e.target.value)}
                    onKeyDown={(e) => handleCellKeyDown(e, cell.id)}
                    onFocus={() => setSelectedCellId(cell.id)}
                    spellCheck={false}
                    placeholder={cell.cellType === 'code' ? '# Enter Python code...' : 'Enter markdown text...'}
                    style={{
                      width: '100%', display: 'block',
                      backgroundColor: F.DARK_BG,
                      color: F.WHITE,
                      border: 'none', outline: 'none', resize: 'none',
                      padding: '12px 16px',
                      fontSize: '12px', lineHeight: '20px',
                      fontFamily: FONT,
                      minHeight: '52px',
                      overflow: 'hidden',
                      boxSizing: 'border-box',
                    }}
                    rows={Math.max(2, getCellSource(cell).split('\n').length)}
                  />

                  {/* Cell output area */}
                  {hasOutput && cell.cellType === 'code' && (
                    <div style={{
                      borderTop: `1px solid ${hasError ? F.RED : F.BORDER}`,
                      maxHeight: '320px',
                      overflowY: 'auto',
                    }}>
                      {/* Output label */}
                      <div style={{
                        padding: '4px 16px',
                        backgroundColor: F.HEADER_BG,
                        borderBottom: `1px solid ${F.BORDER}`,
                        display: 'flex', alignItems: 'center', gap: '6px',
                      }}>
                        <span style={{
                          fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                          fontFamily: FONT,
                          color: hasError ? F.RED : F.GREEN,
                        }}>
                          {hasError ? 'ERROR OUTPUT' : 'OUTPUT'}
                        </span>
                        {cell.executionCount != null && (
                          <>
                            <span style={{ color: F.MUTED, fontSize: '9px' }}>|</span>
                            <span style={{ fontSize: '9px', fontFamily: FONT, color: F.CYAN }}>
                              Out [{cell.executionCount}]
                            </span>
                          </>
                        )}
                      </div>
                      {cell.outputs.map((output, idx) => renderCellOutput(output, idx))}
                    </div>
                  )}
                </div>

                {/* Add cell divider */}
                <div style={{
                  display: 'flex', alignItems: 'center', justifyContent: 'center',
                  padding: '4px 0',
                }}>
                  <div style={{ flex: 1, height: '1px', backgroundColor: F.BORDER }} />
                  <button
                    onClick={() => addCell(cell.id, 'code')}
                    title="Add Code Cell"
                    style={{
                      padding: '2px 10px', fontSize: '9px', fontFamily: FONT, fontWeight: 700,
                      backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                      borderRadius: '2px', color: F.WHITE, cursor: 'pointer', letterSpacing: '0.5px',
                      display: 'flex', alignItems: 'center', gap: '4px',
                      transition: 'all 0.2s',
                    }}
                  >
                    <Plus size={9} /> CODE
                  </button>
                  <button
                    onClick={() => addCell(cell.id, 'markdown')}
                    title="Add Markdown Cell"
                    style={{
                      padding: '2px 10px', fontSize: '9px', fontFamily: FONT, fontWeight: 700,
                      backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                      borderRadius: '2px', color: F.WHITE, cursor: 'pointer', letterSpacing: '0.5px',
                      marginLeft: '4px',
                      display: 'flex', alignItems: 'center', gap: '4px',
                      transition: 'all 0.2s',
                    }}
                  >
                    <Plus size={9} /> MARKDOWN
                  </button>
                  <div style={{ flex: 1, height: '1px', backgroundColor: F.BORDER }} />
                </div>
              </div>
            );
          })}
        </div>

        {/* ─── Sidebar (History / Packages) ─── */}
        {sidebarMode !== 'none' && (
          <div style={{
            width: '300px',
            backgroundColor: F.PANEL_BG,
            borderLeft: `1px solid ${F.BORDER}`,
            display: 'flex', flexDirection: 'column',
          }}>
            {/* Sidebar header */}
            <div style={{
              padding: '12px',
              backgroundColor: F.HEADER_BG,
              borderBottom: `1px solid ${F.BORDER}`,
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', fontFamily: FONT }}>
                {sidebarMode === 'history' ? 'NOTEBOOK HISTORY' : 'PYTHON PACKAGES'}
              </span>
              <button onClick={() => setSidebarMode('none')} style={{
                padding: '3px 6px', background: 'transparent', border: `1px solid ${F.BORDER}`,
                borderRadius: '2px', color: F.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center',
                transition: 'all 0.2s',
              }}>
                <X size={11} />
              </button>
            </div>

            {/* ── History Content ── */}
            {sidebarMode === 'history' && (
              <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                {/* Search input */}
                <div style={{ padding: '8px 12px' }}>
                  <div style={{
                    display: 'flex', alignItems: 'center', gap: '6px',
                    padding: '8px 10px',
                    backgroundColor: F.DARK_BG,
                    border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px',
                  }}>
                    <Search size={11} style={{ color: F.GRAY }} />
                    <input
                      value={historySearch}
                      onChange={(e) => setHistorySearch(e.target.value)}
                      placeholder="Search notebooks..."
                      style={{
                        flex: 1, background: 'none', border: 'none', outline: 'none',
                        color: F.WHITE, fontSize: '10px', fontFamily: FONT,
                      }}
                    />
                  </div>
                </div>

                {/* History list */}
                <div className="nb-scroll" style={{ flex: 1, overflowY: 'auto', padding: '0 8px' }}>
                  {historyItems.length === 0 ? (
                    <div style={{
                      display: 'flex', flexDirection: 'column',
                      alignItems: 'center', justifyContent: 'center',
                      height: '120px', color: F.MUTED, fontSize: '10px',
                      textAlign: 'center', fontFamily: FONT,
                    }}>
                      <History size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                      <span>No notebooks saved yet</span>
                    </div>
                  ) : (
                    historyItems.map((item) => (
                      <div
                        key={item.id}
                        onClick={() => loadFromHistory(item)}
                        style={{
                          padding: '10px 12px', marginBottom: '4px',
                          backgroundColor: 'transparent',
                          borderLeft: '2px solid transparent',
                          borderRadius: '2px', cursor: 'pointer',
                          transition: 'all 0.2s',
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.backgroundColor = F.HOVER;
                          e.currentTarget.style.borderLeftColor = F.ORANGE;
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = 'transparent';
                          e.currentTarget.style.borderLeftColor = 'transparent';
                        }}
                      >
                        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                          <span style={{
                            fontSize: '10px', color: F.WHITE, fontFamily: FONT, fontWeight: 700,
                            flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                          }}>
                            {item.name}
                          </span>
                          {item.id != null && (
                            <button onClick={(e) => { e.stopPropagation(); deleteHistoryItem(item.id!); }} style={{
                              background: 'none', border: 'none', color: F.MUTED, cursor: 'pointer',
                              padding: '2px', display: 'flex', transition: 'color 0.2s',
                            }}
                              onMouseEnter={(e) => { e.currentTarget.style.color = F.RED; }}
                              onMouseLeave={(e) => { e.currentTarget.style.color = F.MUTED; }}
                            >
                              <Trash2 size={10} />
                            </button>
                          )}
                        </div>
                        <div style={{
                          fontSize: '9px', color: F.GRAY, marginTop: '4px',
                          fontFamily: FONT, display: 'flex', gap: '8px',
                        }}>
                          <span style={{ color: F.CYAN }}>{item.cell_count} cells</span>
                          <span>{new Date(item.updated_at).toLocaleDateString()}</span>
                        </div>
                      </div>
                    ))
                  )}
                </div>

                {/* Clear history */}
                <div style={{ padding: '8px 12px', borderTop: `1px solid ${F.BORDER}` }}>
                  <button
                    onClick={async () => { await notebookService.clearHistory(); loadHistory(); }}
                    style={{
                      width: '100%', padding: '6px 10px', fontSize: '9px', fontFamily: FONT, fontWeight: 700,
                      backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                      borderRadius: '2px', color: F.GRAY, cursor: 'pointer', letterSpacing: '0.5px',
                      display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
                      transition: 'all 0.2s',
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.RED; }}
                    onMouseLeave={(e) => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
                  >
                    <RotateCcw size={10} /> CLEAR HISTORY
                  </button>
                </div>
              </div>
            )}

            {/* ── Packages Content ── */}
            {sidebarMode === 'packages' && (
              <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                {/* Install input */}
                <div style={{ padding: '8px 12px', display: 'flex', gap: '6px' }}>
                  <input
                    value={installInput}
                    onChange={(e) => setInstallInput(e.target.value)}
                    onKeyDown={(e) => { if (e.key === 'Enter') installPackage(); }}
                    placeholder="Package name..."
                    style={{
                      flex: 1, padding: '8px 10px',
                      backgroundColor: F.DARK_BG, color: F.WHITE,
                      border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                      fontSize: '10px', fontFamily: FONT, outline: 'none',
                    }}
                    onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
                    onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
                  />
                  <button
                    onClick={installPackage}
                    disabled={installing || !installInput.trim()}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: (installing || !installInput.trim()) ? F.MUTED : F.ORANGE,
                      color: F.DARK_BG, border: 'none', borderRadius: '2px',
                      fontSize: '9px', fontWeight: 700, fontFamily: FONT,
                      cursor: (installing || !installInput.trim()) ? 'not-allowed' : 'pointer',
                      display: 'flex', alignItems: 'center', gap: '4px',
                      letterSpacing: '0.5px',
                    }}
                  >
                    {installing ? <Loader2 size={10} className="animate-spin" /> : <Download size={10} />}
                    INSTALL
                  </button>
                </div>

                {/* Package count */}
                {packages.length > 0 && (
                  <div style={{
                    padding: '4px 12px', borderBottom: `1px solid ${F.BORDER}`,
                    fontSize: '9px', color: F.GRAY, fontFamily: FONT, letterSpacing: '0.5px',
                  }}>
                    <span style={{ color: F.CYAN }}>{packages.length}</span> PACKAGES INSTALLED
                  </div>
                )}

                {/* Package list */}
                <div className="nb-scroll" style={{ flex: 1, overflowY: 'auto' }}>
                  {packagesLoading ? (
                    <div style={{
                      display: 'flex', flexDirection: 'column',
                      alignItems: 'center', justifyContent: 'center',
                      height: '120px', color: F.MUTED, fontSize: '10px',
                      fontFamily: FONT, gap: '8px',
                    }}>
                      <Loader2 size={20} className="animate-spin" style={{ color: F.ORANGE, opacity: 0.5 }} />
                      <span style={{ letterSpacing: '0.5px' }}>LOADING PACKAGES...</span>
                    </div>
                  ) : packages.length === 0 ? (
                    <div style={{
                      display: 'flex', flexDirection: 'column',
                      alignItems: 'center', justifyContent: 'center',
                      height: '120px', color: F.MUTED, fontSize: '10px',
                      textAlign: 'center', fontFamily: FONT,
                    }}>
                      <Package size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                      <span>Click refresh to load</span>
                    </div>
                  ) : (
                    packages.map((pkg, idx) => {
                      const [name, version] = pkg.split('==');
                      return (
                        <div key={idx} style={{
                          padding: '6px 12px',
                          borderBottom: `1px solid ${F.BORDER}`,
                          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                          transition: 'background 0.2s',
                        }}
                          onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                          onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                        >
                          <span style={{ fontSize: '10px', color: F.WHITE, fontFamily: FONT }}>
                            {name}
                          </span>
                          {version && (
                            <span style={{ fontSize: '9px', color: F.CYAN, fontFamily: FONT }}>
                              {version}
                            </span>
                          )}
                        </div>
                      );
                    })
                  )}
                </div>

                {/* Refresh */}
                <div style={{ padding: '8px 12px', borderTop: `1px solid ${F.BORDER}` }}>
                  <button
                    onClick={loadPackages}
                    disabled={packagesLoading}
                    style={{
                      width: '100%', padding: '6px 10px', fontSize: '9px', fontFamily: FONT, fontWeight: 700,
                      backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                      borderRadius: '2px', color: F.GRAY, cursor: packagesLoading ? 'not-allowed' : 'pointer',
                      display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
                      letterSpacing: '0.5px', transition: 'all 0.2s',
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.ORANGE; }}
                    onMouseLeave={(e) => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
                  >
                    <RotateCcw size={10} /> REFRESH LIST
                  </button>
                </div>
              </div>
            )}
          </div>
        )}
      </div>

      {/* ═══ Status Bar ═══ */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderTop: `1px solid ${F.BORDER}`,
        padding: '4px 16px',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        fontSize: '9px', color: F.GRAY, fontFamily: FONT,
        minHeight: '24px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {/* Python version badge */}
          <span style={{
            padding: '2px 6px', backgroundColor: `${F.GREEN}20`,
            color: F.GREEN, fontSize: '8px', fontWeight: 700,
            borderRadius: '2px', letterSpacing: '0.5px',
          }}>
            {pythonVersion || 'PYTHON'}
          </span>

          <span style={{ color: F.MUTED }}>|</span>
          <span><span style={{ color: F.CYAN }}>{codeCellCount}</span> CODE</span>
          <span><span style={{ color: F.PURPLE }}>{mdCellCount}</span> MD</span>
          <span style={{ color: F.MUTED }}>|</span>
          <span>EXECUTED: <span style={{ color: F.CYAN }}>{executedCount}</span></span>
          <span style={{ color: F.MUTED }}>|</span>
          <span>COUNTER: <span style={{ color: F.CYAN }}>{executionCounter - 1}</span></span>

          {unsaved && (
            <>
              <span style={{ color: F.MUTED }}>|</span>
              <span style={{
                padding: '2px 6px', backgroundColor: `${F.YELLOW}20`,
                color: F.YELLOW, fontSize: '8px', fontWeight: 700,
                borderRadius: '2px',
              }}>
                UNSAVED
              </span>
            </>
          )}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ color: F.MUTED }}>SHIFT+ENTER: RUN & NEXT</span>
          <span style={{ color: F.MUTED }}>|</span>
          <span style={{ color: F.MUTED }}>CTRL+ENTER: RUN CELL</span>
        </div>
      </div>
    </div>
  );
};
