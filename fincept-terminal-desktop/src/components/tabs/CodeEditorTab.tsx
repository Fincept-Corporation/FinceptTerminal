import React, { useState, useRef, useEffect } from 'react';
import { Play, Save, FileText, Plus, X, RefreshCw, Terminal as TerminalIcon, Download, Upload, Trash2, ChevronUp, ChevronDown, Package, History, Search, Clock, Folder } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { open, save } from '@tauri-apps/plugin-dialog';
import { notebookService } from '@/services/notebookService';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface EditorFile {
  id: string;
  name: string;
  content: string;
  language: 'finscript' | 'python' | 'javascript' | 'json';
  unsaved: boolean;
}

interface ExecutionResult {
  output: string;
  error?: string;
  timestamp: Date;
}

interface NotebookCell {
  id: string;
  cellType: string;
  source: string[];
  outputs: CellOutput[];
  executionCount: number | null;
  metadata: Record<string, any>;
}

interface CellOutput {
  outputType: string;
  data?: Record<string, string[]>;
  text?: string[];
  name?: string;
  ename?: string;
  evalue?: string;
  traceback?: string[];
}

interface JupyterNotebook {
  cells: NotebookCell[];
  metadata: {
    kernelspec: {
      displayName: string;
      language: string;
      name: string;
    };
    languageInfo: {
      name: string;
      version: string;
      mimetype: string;
      fileExtension: string;
    };
  };
  nbformat: number;
  nbformatMinor: number;
}

interface NotebookHistory {
  id?: number;
  name: string;
  content: string;
  path: string | null;
  created_at: string;
  updated_at: string;
  cell_count: number;
  execution_count: number;
}

export default function CodeEditorTab() {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [editorMode, setEditorMode] = useState<'code' | 'notebook'>('code');
  const [files, setFiles] = useState<EditorFile[]>([
    {
      id: '1',
      name: 'strategy.fincept',
      content: `// FinScript - Financial Analysis Language
// Example: EMA/WMA Crossover Strategy

// Calculate technical indicators
ema_fast = ema(AAPL, 21)
wma_slow = wma(AAPL, 50)
rsi_value = rsi(AAPL, 14)

// Get last values for comparison
ema_current = last(ema_fast)
wma_current = last(wma_slow)
rsi_current = last(rsi_value)

// EMA/WMA Crossover Strategy
if ema_current > wma_current {
    buy "EMA 21 crosses above WMA 50 - Bullish Signal"
}

if ema_current < wma_current {
    sell "EMA 21 crosses below WMA 50 - Bearish Signal"
}

// RSI Overbought/Oversold Strategy
if rsi_current > 70 {
    sell "RSI Overbought - Consider Selling"
}

if rsi_current < 30 {
    buy "RSI Oversold - Consider Buying"
}

// Plot indicators
plot ema(AAPL, 7), "EMA (7)"
plot ema_fast, "EMA (21)"
plot wma_slow, "WMA (50)"
plot rsi_value, "RSI (14)"
plot_candlestick AAPL, "Apple Stock Chart"`,
      language: 'finscript',
      unsaved: false
    }
  ]);
  const [activeFileId, setActiveFileId] = useState('1');
  const [output, setOutput] = useState<ExecutionResult[]>([]);
  const [isRunning, setIsRunning] = useState(false);

  // Jupyter notebook state
  const [notebookCells, setNotebookCells] = useState<NotebookCell[]>([]);
  const [currentNotebookPath, setCurrentNotebookPath] = useState<string | null>(null);
  const [currentNotebookId, setCurrentNotebookId] = useState<number | null>(null);
  const [executionCounter, setExecutionCounter] = useState(1);
  const [pythonVersion, setPythonVersion] = useState<string>('Loading...');
  const [installedPackages, setInstalledPackages] = useState<string[]>([]);
  const [showPackageManager, setShowPackageManager] = useState(false);
  const [packageToInstall, setPackageToInstall] = useState('');

  // History sidebar state
  const [showHistory, setShowHistory] = useState(false);
  const [notebookHistory, setNotebookHistory] = useState<NotebookHistory[]>([]);
  const [historySearch, setHistorySearch] = useState('');
  const [isLoadingHistory, setIsLoadingHistory] = useState(false);

  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // Use theme colors with legacy variable name for minimal changes
  const C = {
    ORANGE: colors.primary,
    WHITE: colors.text,
    GREEN: colors.secondary,
    RED: colors.alert,
    YELLOW: colors.warning,
    BLUE: colors.info,
    CYAN: colors.accent,
    GRAY: colors.textMuted,
    DARK_BG: colors.background,
    PANEL_BG: colors.panel,
    EDITOR_BG: colors.panel
  };

  const activeFile = files.find(f => f.id === activeFileId) || files[0];

  // Initialize Python version and load history on mount
  useEffect(() => {
    if (editorMode === 'notebook') {
      loadPythonVersion();
      loadHistory();
    }
  }, [editorMode]);

  const loadHistory = async () => {
    setIsLoadingHistory(true);
    try {
      const history = await notebookService.getHistory(50);
      setNotebookHistory(history);
    } catch (error) {
      console.error('Failed to load history:', error);
    } finally {
      setIsLoadingHistory(false);
    }
  };

  const searchHistory = async (query: string) => {
    if (!query.trim()) {
      loadHistory();
      return;
    }

    setIsLoadingHistory(true);
    try {
      const results = await notebookService.searchHistory(query);
      setNotebookHistory(results);
    } catch (error) {
      console.error('Failed to search history:', error);
    } finally {
      setIsLoadingHistory(false);
    }
  };

  const saveCurrentToHistory = async () => {
    try {
      const notebookContent = JSON.stringify({
        cells: notebookCells,
        metadata: {
          kernelspec: {
            displayName: 'Python 3',
            language: 'python',
            name: 'python3'
          },
          languageInfo: {
            name: 'python',
            version: '3.12.0',
            mimetype: 'text/x-python',
            fileExtension: '.py'
          }
        },
        nbformat: 4,
        nbformatMinor: 5
      });

      const historyEntry: NotebookHistory = {
        name: currentNotebookPath ? currentNotebookPath.split(/[/\\]/).pop() || 'Untitled' : 'Untitled',
        content: notebookContent,
        path: currentNotebookPath,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        cell_count: notebookCells.length,
        execution_count: executionCounter - 1
      };

      if (currentNotebookId) {
        await notebookService.updateHistory(currentNotebookId, historyEntry);
      } else {
        const id = await notebookService.saveToHistory(historyEntry);
        setCurrentNotebookId(id);
      }

      await loadHistory();
    } catch (error) {
      console.error('Failed to save to history:', error);
    }
  };

  const loadFromHistory = async (historyId: number) => {
    try {
      const entry = await notebookService.getNotebookById(historyId);
      if (!entry) return;

      const notebook: JupyterNotebook = JSON.parse(entry.content);
      setNotebookCells(notebook.cells);
      setCurrentNotebookPath(entry.path);
      setCurrentNotebookId(historyId);

      // Find the highest execution count
      const maxCount = Math.max(
        ...notebook.cells
          .map(c => c.executionCount || 0)
          .filter(c => c > 0),
        0
      );
      setExecutionCounter(maxCount + 1);

      setOutput([...output, {
        output: `Loaded from history: ${entry.name}`,
        timestamp: new Date()
      }]);
    } catch (error) {
      setOutput([...output, {
        output: `Failed to load from history: ${error}`,
        error: String(error),
        timestamp: new Date()
      }]);
    }
  };

  const deleteFromHistory = async (historyId: number) => {
    try {
      await notebookService.deleteFromHistory(historyId);
      await loadHistory();

      if (currentNotebookId === historyId) {
        setCurrentNotebookId(null);
      }
    } catch (error) {
      console.error('Failed to delete from history:', error);
    }
  };

  const loadPythonVersion = async () => {
    try {
      const version = await invoke<string>('get_python_version');
      setPythonVersion(version);
    } catch (error) {
      setPythonVersion('Error loading Python version');
      console.error('Failed to get Python version:', error);
    }
  };

  const loadInstalledPackages = async () => {
    try {
      const packages = await invoke<string[]>('list_python_packages');
      setInstalledPackages(packages);
    } catch (error) {
      console.error('Failed to load packages:', error);
    }
  };

  const installPackage = async () => {
    if (!packageToInstall.trim()) return;

    try {
      setOutput([...output, {
        output: `Installing package: ${packageToInstall}...`,
        timestamp: new Date()
      }]);

      const result = await invoke<string>('install_python_package', {
        packageName: packageToInstall.trim()
      });

      setOutput([...output, {
        output: result,
        timestamp: new Date()
      }]);

      setPackageToInstall('');
      loadInstalledPackages();
    } catch (error) {
      setOutput([...output, {
        output: `Failed to install package: ${error}`,
        error: String(error),
        timestamp: new Date()
      }]);
    }
  };

  const updateFileContent = (content: string) => {
    setFiles(files.map(f =>
      f.id === activeFileId ? { ...f, content, unsaved: true } : f
    ));
  };

  const saveFile = () => {
    setFiles(files.map(f =>
      f.id === activeFileId ? { ...f, unsaved: false } : f
    ));
    setOutput([...output, {
      output: `File '${activeFile.name}' saved successfully`,
      timestamp: new Date()
    }]);
  };

  const createNewFile = () => {
    const newId = Date.now().toString();
    const newFile: EditorFile = {
      id: newId,
      name: `untitled-${files.length + 1}.fincept`,
      content: '// New FinScript file\n',
      language: 'finscript',
      unsaved: false
    };
    setFiles([...files, newFile]);
    setActiveFileId(newId);
  };

  const closeFile = (id: string) => {
    if (files.length === 1) return;
    const newFiles = files.filter(f => f.id !== id);
    setFiles(newFiles);
    if (activeFileId === id) {
      setActiveFileId(newFiles[0].id);
    }
  };

  const executeFinScript = async () => {
    setIsRunning(true);
    setOutput([...output, {
      output: `Executing FinScript: ${activeFile.name}...`,
      timestamp: new Date()
    }]);

    try {
      const result = await invoke<{
        success: boolean;
        output: string;
        signals: Array<{
          signal_type: string;
          message: string;
          timestamp: string;
          price?: number;
        }>;
        plots: Array<{
          plot_type: string;
          label: string;
          data: Array<any>;
        }>;
        errors: string[];
        execution_time_ms: number;
      }>('execute_finscript', {
        code: activeFile.content
      });

      let outputText = `
=== FinScript Execution Results ===
File: ${activeFile.name}
Status: ${result.success ? 'Completed Successfully' : 'Failed'}
Execution Time: ${result.execution_time_ms}ms

${result.output}

`;

      if (result.signals.length > 0) {
        outputText += `\nSignals Generated:\n`;
        result.signals.forEach(signal => {
          const icon = signal.signal_type === 'Buy' ? '✓ BUY' : '✗ SELL';
          outputText += `  ${icon}: ${signal.message}\n`;
        });
      }

      if (result.plots.length > 0) {
        outputText += `\nPlots Generated:\n`;
        result.plots.forEach(plot => {
          outputText += `  ✓ ${plot.plot_type}: ${plot.label} (${plot.data.length} data points)\n`;
        });
      }

      if (result.errors.length > 0) {
        outputText += `\nErrors:\n`;
        result.errors.forEach(error => {
          outputText += `  ✗ ${error}\n`;
        });
      }

      setOutput([...output, {
        output: outputText,
        error: result.errors.length > 0 ? result.errors.join('\n') : undefined,
        timestamp: new Date()
      }]);

      setIsRunning(false);
    } catch (error) {
      setOutput([...output, {
        output: `Execution failed: ${error}`,
        error: String(error),
        timestamp: new Date()
      }]);
      setIsRunning(false);
    }
  };

  const runCode = () => {
    if (activeFile.language === 'finscript') {
      executeFinScript();
    } else if (activeFile.language === 'python') {
      executePythonCode(activeFile.content);
    }
  };

  const executePythonCode = async (code: string) => {
    setIsRunning(true);
    setOutput([...output, {
      output: `Executing Python code...`,
      timestamp: new Date()
    }]);

    try {
      const result = await invoke<{
        success: boolean;
        outputs: CellOutput[];
        executionCount: number;
        error?: string;
      }>('execute_python_code', {
        code: code,
        executionCount: executionCounter
      });

      const outputText = result.outputs
        .map(out => {
          if (out.text) return out.text.join('\n');
          if (out.traceback) return out.traceback.join('\n');
          return '';
        })
        .join('\n');

      setOutput([...output, {
        output: outputText || 'Code executed successfully (no output)',
        error: result.error,
        timestamp: new Date()
      }]);

      setIsRunning(false);
    } catch (error) {
      setOutput([...output, {
        output: `Execution failed: ${error}`,
        error: String(error),
        timestamp: new Date()
      }]);
      setIsRunning(false);
    }
  };

  const clearOutput = () => setOutput([]);

  const getLanguageColor = (lang: string) => {
    switch (lang) {
      case 'finscript': return C.ORANGE;
      case 'python': return C.BLUE;
      case 'javascript': return C.YELLOW;
      default: return C.GRAY;
    }
  };

  const highlightFinScript = (code: string) => {
    const keywords = ['if', 'else', 'buy', 'sell', 'plot', 'plot_candlestick', 'plot_bar', 'plot_area'];
    const functions = ['ema', 'sma', 'wma', 'rsi', 'macd', 'last', 'high', 'low', 'open', 'close', 'volume'];
    const operators = ['>', '<', '>=', '<=', '==', '!=', '&&', '||', '+', '-', '*', '/'];

    let highlighted = code;

    // Highlight comments
    highlighted = highlighted.replace(/(\/\/.*$)/gm, '<span style="color: #787878; font-style: italic;">$1</span>');

    // Highlight strings
    highlighted = highlighted.replace(/"([^"]*)"/g, '<span style="color: #98c379;">\"$1\"</span>');

    // Highlight numbers
    highlighted = highlighted.replace(/\b(\d+\.?\d*)\b/g, '<span style="color: #d19a66;">$1</span>');

    // Highlight keywords
    keywords.forEach(keyword => {
      const regex = new RegExp(`\\b(${keyword})\\b`, 'g');
      highlighted = highlighted.replace(regex, `<span style="color: ${C.ORANGE}; font-weight: bold;">$1</span>`);
    });

    // Highlight functions
    functions.forEach(func => {
      const regex = new RegExp(`\\b(${func})(?=\\()`, 'g');
      highlighted = highlighted.replace(regex, `<span style="color: ${C.CYAN};">$1</span>`);
    });

    return highlighted;
  };

  // Jupyter Notebook Functions
  const createNewNotebook = async () => {
    try {
      // Save current notebook to history before creating new one
      if (notebookCells.length > 0) {
        await saveCurrentToHistory();
      }

      const notebook = await invoke<JupyterNotebook>('create_new_notebook');
      setNotebookCells(notebook.cells);
      setCurrentNotebookPath(null);
      setCurrentNotebookId(null);
      setExecutionCounter(1);

      setOutput([...output, {
        output: 'Created new notebook',
        timestamp: new Date()
      }]);
    } catch (error) {
      console.error('Failed to create notebook:', error);
    }
  };

  const openNotebook = async () => {
    try {
      // Save current notebook to history before opening new one
      if (notebookCells.length > 0) {
        await saveCurrentToHistory();
      }

      const selected = await open({
        multiple: false,
        filters: [{
          name: 'Jupyter Notebook',
          extensions: ['ipynb']
        }]
      });

      if (!selected) return;

      const path = selected as string;
      const notebook = await invoke<JupyterNotebook>('open_notebook', {
        filePath: path
      });

      setNotebookCells(notebook.cells);
      setCurrentNotebookPath(path);
      setCurrentNotebookId(null);

      // Find the highest execution count
      const maxCount = Math.max(
        ...notebook.cells
          .map(c => c.executionCount || 0)
          .filter(c => c > 0),
        0
      );
      setExecutionCounter(maxCount + 1);

      // Save to history
      await saveCurrentToHistory();

      setOutput([...output, {
        output: `Notebook opened: ${path}`,
        timestamp: new Date()
      }]);
    } catch (error) {
      setOutput([...output, {
        output: `Failed to open notebook: ${error}`,
        error: String(error),
        timestamp: new Date()
      }]);
    }
  };

  const saveNotebook = async () => {
    try {
      let path = currentNotebookPath;

      if (!path) {
        const selected = await save({
          filters: [{
            name: 'Jupyter Notebook',
            extensions: ['ipynb']
          }]
        });

        if (!selected) return;
        path = selected as string;
      }

      const notebook: JupyterNotebook = {
        cells: notebookCells,
        metadata: {
          kernelspec: {
            displayName: 'Python 3',
            language: 'python',
            name: 'python3'
          },
          languageInfo: {
            name: 'python',
            version: '3.12.0',
            mimetype: 'text/x-python',
            fileExtension: '.py'
          }
        },
        nbformat: 4,
        nbformatMinor: 5
      };

      await invoke('save_notebook', {
        filePath: path,
        notebook: notebook
      });

      setCurrentNotebookPath(path);

      // Save to history
      await saveCurrentToHistory();

      setOutput([...output, {
        output: `Notebook saved: ${path}`,
        timestamp: new Date()
      }]);
    } catch (error) {
      setOutput([...output, {
        output: `Failed to save notebook: ${error}`,
        error: String(error),
        timestamp: new Date()
      }]);
    }
  };

  const addCell = (index?: number) => {
    const newCell: NotebookCell = {
      id: `cell-${Date.now()}`,
      cellType: 'code',
      source: [],
      outputs: [],
      executionCount: null,
      metadata: {}
    };

    if (index !== undefined) {
      const newCells = [...notebookCells];
      newCells.splice(index + 1, 0, newCell);
      setNotebookCells(newCells);
    } else {
      setNotebookCells([...notebookCells, newCell]);
    }
  };

  const deleteCell = (index: number) => {
    if (notebookCells.length === 1) return;
    setNotebookCells(notebookCells.filter((_, i) => i !== index));
  };

  const moveCellUp = (index: number) => {
    if (index === 0) return;
    const newCells = [...notebookCells];
    [newCells[index - 1], newCells[index]] = [newCells[index], newCells[index - 1]];
    setNotebookCells(newCells);
  };

  const moveCellDown = (index: number) => {
    if (index === notebookCells.length - 1) return;
    const newCells = [...notebookCells];
    [newCells[index], newCells[index + 1]] = [newCells[index + 1], newCells[index]];
    setNotebookCells(newCells);
  };

  const updateCellContent = (index: number, content: string) => {
    const newCells = [...notebookCells];
    newCells[index].source = content.split('\n');
    setNotebookCells(newCells);
  };

  const toggleCellType = (index: number) => {
    const newCells = [...notebookCells];
    newCells[index].cellType = newCells[index].cellType === 'code' ? 'markdown' : 'code';
    setNotebookCells(newCells);
  };

  const executeCell = async (index: number) => {
    const cell = notebookCells[index];
    if (cell.cellType !== 'code') return;

    const code = cell.source.join('\n');
    if (!code.trim()) return;

    try {
      const result = await invoke<{
        success: boolean;
        outputs: CellOutput[];
        executionCount: number;
        error?: string;
      }>('execute_python_code', {
        code: code,
        executionCount: executionCounter
      });

      const newCells = [...notebookCells];
      newCells[index].outputs = result.outputs;
      newCells[index].executionCount = result.executionCount;
      setNotebookCells(newCells);
      setExecutionCounter(executionCounter + 1);
    } catch (error) {
      const errorOutput: CellOutput = {
        outputType: 'error',
        ename: 'ExecutionError',
        evalue: String(error),
        traceback: [String(error)]
      };

      const newCells = [...notebookCells];
      newCells[index].outputs = [errorOutput];
      newCells[index].executionCount = executionCounter;
      setNotebookCells(newCells);
      setExecutionCounter(executionCounter + 1);
    }
  };

  const executeAllCells = async () => {
    for (let i = 0; i < notebookCells.length; i++) {
      if (notebookCells[i].cellType === 'code') {
        await executeCell(i);
      }
    }
  };

  const clearCellOutput = (index: number) => {
    const newCells = [...notebookCells];
    newCells[index].outputs = [];
    newCells[index].executionCount = null;
    setNotebookCells(newCells);
  };

  const clearAllOutputs = () => {
    const newCells = notebookCells.map(cell => ({
      ...cell,
      outputs: [],
      executionCount: null
    }));
    setNotebookCells(newCells);
    setExecutionCounter(1);
  };

  const renderCellOutput = (outputs: CellOutput[]) => {
    if (!outputs || outputs.length === 0) {
      return (
        <div style={{ color: C.GRAY, fontSize: '11px', fontStyle: 'italic' }}>
          Execute cell to see output
        </div>
      );
    }

    return outputs.map((output, idx) => {
      if (output.outputType === 'stream') {
        return (
          <div key={idx} style={{ color: C.GREEN, fontSize: '12px', whiteSpace: 'pre-wrap' }}>
            {output.text?.join('\n')}
          </div>
        );
      } else if (output.outputType === 'error') {
        return (
          <div key={idx} style={{ color: C.RED, fontSize: '12px', whiteSpace: 'pre-wrap' }}>
            <div style={{ fontWeight: 'bold' }}>{output.ename}: {output.evalue}</div>
            {output.traceback && <div>{output.traceback.join('\n')}</div>}
          </div>
        );
      } else if (output.outputType === 'execute_result' || output.outputType === 'display_data') {
        if (output.data) {
          if (output.data['text/plain']) {
            return (
              <div key={idx} style={{ color: C.CYAN, fontSize: '12px', whiteSpace: 'pre-wrap' }}>
                {output.data['text/plain'].join('\n')}
              </div>
            );
          }
          if (output.data['text/html']) {
            return (
              <div key={idx} dangerouslySetInnerHTML={{ __html: output.data['text/html'].join('') }} />
            );
          }
          if (output.data['image/png']) {
            return (
              <img key={idx} src={`data:image/png;base64,${output.data['image/png'][0]}`} alt="Output" style={{ maxWidth: '100%' }} />
            );
          }
        }
      }
      return null;
    });
  };

  // Initialize with a single cell if in notebook mode and no cells exist
  useEffect(() => {
    if (editorMode === 'notebook' && notebookCells.length === 0) {
      createNewNotebook();
    }
  }, [editorMode]);

  const formatDate = (dateString: string) => {
    const date = new Date(dateString);
    const now = new Date();
    const diff = now.getTime() - date.getTime();
    const hours = Math.floor(diff / (1000 * 60 * 60));

    if (hours < 1) {
      const minutes = Math.floor(diff / (1000 * 60));
      return `${minutes}m ago`;
    } else if (hours < 24) {
      return `${hours}h ago`;
    } else {
      const days = Math.floor(hours / 24);
      return `${days}d ago`;
    }
  };

  return (
    <div style={{ width: '100%', height: '100%', backgroundColor: C.DARK_BG, color: C.WHITE, fontFamily: 'Consolas, monospace', display: 'flex', flexDirection: 'column' }}>
      <style>{`
        .code-scroll::-webkit-scrollbar { width: 8px; height: 8px; }
        .code-scroll::-webkit-scrollbar-track { background: #1a1a1a; }
        .code-scroll::-webkit-scrollbar-thumb { background: #404040; border-radius: 4px; }
        .code-scroll::-webkit-scrollbar-thumb:hover { background: #525252; }

        .code-editor {
          font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
          font-size: 14px;
          line-height: 1.6;
          tab-size: 4;
        }

        .line-numbers {
          background: #0a0a0a;
          color: #787878;
          padding: 12px 8px;
          text-align: right;
          font-size: 13px;
          user-select: none;
          border-right: 1px solid #404040;
        }

        .cell-actions {
          opacity: 0;
          transition: opacity 0.2s;
        }

        .notebook-cell:hover .cell-actions {
          opacity: 1;
        }

        .history-item:hover {
          background-color: #2a2a2a;
        }
      `}</style>

      {/* Header */}
      <div style={{ backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}`, padding: '8px 16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '16px' }}>CODE EDITOR</span>
            <span style={{ color: C.GRAY }}>|</span>
            <button onClick={() => setEditorMode('code')} style={{ padding: '4px 12px', backgroundColor: editorMode === 'code' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: editorMode === 'code' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              FINSCRIPT EDITOR
            </button>
            <button onClick={() => setEditorMode('notebook')} style={{ padding: '4px 12px', backgroundColor: editorMode === 'notebook' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: editorMode === 'notebook' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              JUPYTER NOTEBOOK
            </button>
            {editorMode === 'notebook' && (
              <>
                <span style={{ color: C.GRAY }}>|</span>
                <span style={{ color: C.CYAN, fontSize: '10px' }}>{pythonVersion}</span>
              </>
            )}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            {editorMode === 'code' ? (
              <>
                <button onClick={createNewFile} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GREEN}`, color: C.GREEN, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <Plus size={14} style={{ display: 'inline', marginRight: '4px' }} />NEW FILE
                </button>
                <button onClick={saveFile} disabled={!activeFile.unsaved} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${activeFile.unsaved ? C.YELLOW : C.GRAY}`, color: activeFile.unsaved ? C.YELLOW : C.GRAY, fontSize: '11px', fontWeight: 'bold', cursor: activeFile.unsaved ? 'pointer' : 'not-allowed', fontFamily: 'Consolas, monospace' }}>
                  <Save size={14} style={{ display: 'inline', marginRight: '4px' }} />SAVE
                </button>
                <button onClick={runCode} disabled={isRunning} style={{ padding: '6px 12px', backgroundColor: isRunning ? C.GRAY : C.GREEN, border: 'none', color: C.DARK_BG, fontSize: '11px', fontWeight: 'bold', cursor: isRunning ? 'not-allowed' : 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <Play size={14} style={{ display: 'inline', marginRight: '4px' }} />{isRunning ? 'RUNNING...' : 'RUN CODE'}
                </button>
              </>
            ) : (
              <>
                <button onClick={createNewNotebook} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GREEN}`, color: C.GREEN, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <Plus size={14} style={{ display: 'inline', marginRight: '4px' }} />NEW
                </button>
                <button onClick={openNotebook} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${C.BLUE}`, color: C.BLUE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <Upload size={14} style={{ display: 'inline', marginRight: '4px' }} />OPEN
                </button>
                <button onClick={saveNotebook} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${C.YELLOW}`, color: C.YELLOW, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <Save size={14} style={{ display: 'inline', marginRight: '4px' }} />SAVE
                </button>
                <button onClick={executeAllCells} style={{ padding: '6px 12px', backgroundColor: C.GREEN, border: 'none', color: C.DARK_BG, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <Play size={14} style={{ display: 'inline', marginRight: '4px' }} />RUN ALL
                </button>
                <button onClick={() => setShowPackageManager(!showPackageManager)} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${C.CYAN}`, color: C.CYAN, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <Package size={14} style={{ display: 'inline', marginRight: '4px' }} />PACKAGES
                </button>
                <button onClick={() => setShowHistory(!showHistory)} style={{ padding: '6px 12px', backgroundColor: showHistory ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.ORANGE}`, color: showHistory ? C.DARK_BG : C.ORANGE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <History size={14} style={{ display: 'inline', marginRight: '4px' }} />HISTORY
                </button>
              </>
            )}
          </div>
        </div>
      </div>

      {/* File Tabs (Code Mode) */}
      {editorMode === 'code' && (
        <div style={{ backgroundColor: C.PANEL_BG, borderBottom: `1px solid ${C.GRAY}`, display: 'flex', alignItems: 'center', padding: '4px 8px', gap: '4px', overflowX: 'auto' }}>
          {files.map(file => (
            <div key={file.id} onClick={() => setActiveFileId(file.id)} style={{ display: 'flex', alignItems: 'center', gap: '6px', padding: '6px 12px', backgroundColor: activeFileId === file.id ? C.EDITOR_BG : C.DARK_BG, border: `1px solid ${activeFileId === file.id ? getLanguageColor(file.language) : C.GRAY}`, cursor: 'pointer', fontSize: '11px' }}>
              <FileText size={12} style={{ color: getLanguageColor(file.language) }} />
              <span style={{ color: file.unsaved ? C.YELLOW : C.WHITE }}>{file.name}{file.unsaved && '*'}</span>
              {files.length > 1 && (
                <X size={12} onClick={(e) => { e.stopPropagation(); closeFile(file.id); }} style={{ color: C.GRAY, cursor: 'pointer' }} />
              )}
            </div>
          ))}
        </div>
      )}

      {/* Package Manager Modal */}
      {showPackageManager && editorMode === 'notebook' && (
        <div style={{ position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)', backgroundColor: C.PANEL_BG, border: `2px solid ${C.ORANGE}`, padding: '20px', zIndex: 1000, width: '500px', maxHeight: '400px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
            <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '14px' }}>PACKAGE MANAGER</span>
            <X size={16} onClick={() => setShowPackageManager(false)} style={{ cursor: 'pointer', color: C.GRAY }} />
          </div>

          <div style={{ marginBottom: '16px' }}>
            <div style={{ display: 'flex', gap: '8px' }}>
              <input
                type="text"
                value={packageToInstall}
                onChange={(e) => setPackageToInstall(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && installPackage()}
                placeholder="Package name (e.g., numpy, pandas, matplotlib)"
                style={{ flex: 1, backgroundColor: C.EDITOR_BG, border: `1px solid ${C.GRAY}`, color: C.WHITE, padding: '6px 8px', fontSize: '12px', fontFamily: 'Consolas, monospace' }}
              />
              <button onClick={installPackage} style={{ padding: '6px 12px', backgroundColor: C.GREEN, border: 'none', color: C.DARK_BG, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                INSTALL
              </button>
            </div>
          </div>

          <div style={{ fontSize: '11px', color: C.CYAN, marginBottom: '8px' }}>Installed Packages:</div>
          <div className="code-scroll" style={{ maxHeight: '250px', overflowY: 'auto', backgroundColor: C.EDITOR_BG, padding: '8px', fontSize: '10px' }}>
            {installedPackages.length === 0 ? (
              <div style={{ color: C.GRAY }}>Click to load packages...</div>
            ) : (
              installedPackages.map((pkg, idx) => (
                <div key={idx} style={{ color: C.WHITE, marginBottom: '2px' }}>{pkg}</div>
              ))
            )}
          </div>
          {installedPackages.length === 0 && (
            <button onClick={loadInstalledPackages} style={{ marginTop: '8px', padding: '4px 8px', backgroundColor: C.BLUE, border: 'none', color: C.WHITE, fontSize: '10px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              LOAD PACKAGES
            </button>
          )}
        </div>
      )}

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
        {/* History Sidebar */}
        {showHistory && editorMode === 'notebook' && (
          <div style={{ width: '300px', backgroundColor: C.PANEL_BG, borderRight: `1px solid ${C.GRAY}`, display: 'flex', flexDirection: 'column' }}>
            <div style={{ backgroundColor: C.DARK_BG, padding: '8px 12px', borderBottom: `1px solid ${C.GRAY}` }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
                <History size={14} style={{ color: C.ORANGE }} />
                <span style={{ color: C.ORANGE, fontSize: '12px', fontWeight: 'bold' }}>NOTEBOOK HISTORY</span>
              </div>
              <div style={{ display: 'flex', gap: '4px' }}>
                <input
                  type="text"
                  value={historySearch}
                  onChange={(e) => {
                    setHistorySearch(e.target.value);
                    searchHistory(e.target.value);
                  }}
                  placeholder="Search..."
                  style={{ flex: 1, backgroundColor: C.EDITOR_BG, border: `1px solid ${C.GRAY}`, color: C.WHITE, padding: '4px 8px', fontSize: '11px', fontFamily: 'Consolas, monospace' }}
                />
                <button onClick={loadHistory} style={{ padding: '4px 8px', backgroundColor: C.DARK_BG, border: `1px solid ${C.BLUE}`, color: C.BLUE, fontSize: '10px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  <RefreshCw size={12} />
                </button>
              </div>
            </div>

            <div className="code-scroll" style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
              {isLoadingHistory ? (
                <div style={{ color: C.GRAY, textAlign: 'center', padding: '20px', fontSize: '11px' }}>
                  Loading history...
                </div>
              ) : notebookHistory.length === 0 ? (
                <div style={{ color: C.GRAY, textAlign: 'center', padding: '20px', fontSize: '11px' }}>
                  No notebook history yet
                </div>
              ) : (
                notebookHistory.map((entry) => (
                  <div
                    key={entry.id}
                    className="history-item"
                    style={{
                      backgroundColor: currentNotebookId === entry.id ? C.EDITOR_BG : C.DARK_BG,
                      border: `1px solid ${currentNotebookId === entry.id ? C.ORANGE : C.GRAY}`,
                      padding: '8px',
                      marginBottom: '8px',
                      cursor: 'pointer',
                      fontSize: '11px'
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'start', marginBottom: '4px' }}>
                      <div onClick={() => loadFromHistory(entry.id!)} style={{ flex: 1 }}>
                        <div style={{ color: C.CYAN, fontWeight: 'bold', marginBottom: '4px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                          <Folder size={10} />
                          {entry.name}
                        </div>
                        <div style={{ color: C.GRAY, fontSize: '10px', marginBottom: '4px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                          <Clock size={10} />
                          {formatDate(entry.updated_at)}
                        </div>
                        <div style={{ color: C.GRAY, fontSize: '9px', display: 'flex', gap: '12px' }}>
                          <span>{entry.cell_count} cells</span>
                          <span>Ex: {entry.execution_count}</span>
                        </div>
                      </div>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          deleteFromHistory(entry.id!);
                        }}
                        style={{ padding: '2px 4px', backgroundColor: 'transparent', border: `1px solid ${C.RED}`, color: C.RED, fontSize: '9px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}
                      >
                        <Trash2 size={10} />
                      </button>
                    </div>
                    {entry.path && (
                      <div style={{ color: C.GRAY, fontSize: '9px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                        {entry.path}
                      </div>
                    )}
                  </div>
                ))
              )}
            </div>
          </div>
        )}

        {editorMode === 'code' ? (
          <>
            {/* Editor Panel */}
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', borderRight: `1px solid ${C.GRAY}` }}>
              <div style={{ backgroundColor: C.PANEL_BG, padding: '8px 12px', borderBottom: `1px solid ${C.GRAY}`, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{ color: C.ORANGE, fontSize: '12px', fontWeight: 'bold' }}>EDITOR</span>
                  <span style={{ color: C.GRAY }}>|</span>
                  <span style={{ color: getLanguageColor(activeFile.language), fontSize: '11px' }}>{activeFile.language.toUpperCase()}</span>
                </div>
                <div style={{ fontSize: '10px', color: C.GRAY }}>
                  Lines: {activeFile.content.split('\n').length} | Chars: {activeFile.content.length}
                </div>
              </div>

              <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
                {/* Line Numbers */}
                <div className="line-numbers">
                  {activeFile.content.split('\n').map((_, i) => (
                    <div key={i}>{i + 1}</div>
                  ))}
                </div>

                {/* Editor */}
                <textarea
                  ref={textareaRef}
                  className="code-editor code-scroll"
                  value={activeFile.content}
                  onChange={(e) => updateFileContent(e.target.value)}
                  spellCheck={false}
                  style={{
                    flex: 1,
                    backgroundColor: C.EDITOR_BG,
                    color: C.WHITE,
                    border: 'none',
                    outline: 'none',
                    resize: 'none',
                    padding: '12px',
                    whiteSpace: 'pre',
                    overflowWrap: 'normal',
                    overflowX: 'auto'
                  }}
                />
              </div>
            </div>

            {/* Output Panel */}
            <div style={{ width: '450px', display: 'flex', flexDirection: 'column', backgroundColor: C.PANEL_BG }}>
              <div style={{ backgroundColor: C.DARK_BG, padding: '8px 12px', borderBottom: `1px solid ${C.GRAY}`, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <TerminalIcon size={14} style={{ color: C.GREEN }} />
                  <span style={{ color: C.ORANGE, fontSize: '12px', fontWeight: 'bold' }}>OUTPUT CONSOLE</span>
                </div>
                <button onClick={clearOutput} style={{ padding: '3px 8px', backgroundColor: 'transparent', border: `1px solid ${C.RED}`, color: C.RED, fontSize: '9px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  CLEAR
                </button>
              </div>

              <div className="code-scroll" style={{ flex: 1, overflowY: 'auto', padding: '12px', backgroundColor: C.DARK_BG, fontSize: '12px', lineHeight: '1.6' }}>
                {output.length === 0 ? (
                  <div style={{ color: C.GRAY, textAlign: 'center', paddingTop: '40px' }}>
                    <TerminalIcon size={32} style={{ marginBottom: '8px', opacity: 0.5 }} />
                    <div>No output yet</div>
                    <div style={{ fontSize: '10px', marginTop: '4px' }}>Run your code to see results</div>
                  </div>
                ) : (
                  output.map((result, idx) => (
                    <div key={idx} style={{ marginBottom: '16px', paddingBottom: '16px', borderBottom: `1px solid ${C.GRAY}` }}>
                      <div style={{ color: C.CYAN, fontSize: '10px', marginBottom: '6px' }}>
                        [{result.timestamp.toLocaleTimeString()}]
                      </div>
                      <pre style={{ color: result.error ? C.RED : C.GREEN, margin: 0, whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                        {result.error || result.output}
                      </pre>
                    </div>
                  ))
                )}
              </div>
            </div>
          </>
        ) : (
          // Jupyter Notebook Mode
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <div className="code-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px', backgroundColor: '#050505' }}>
              <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
                {notebookCells.map((cell, index) => (
                  <div key={cell.id} className="notebook-cell" style={{ backgroundColor: C.PANEL_BG, border: `1px solid ${C.GRAY}`, marginBottom: '16px', position: 'relative' }}>
                    {/* Cell Header */}
                    <div style={{ backgroundColor: C.DARK_BG, padding: '6px 12px', borderBottom: `1px solid ${C.GRAY}`, fontSize: '10px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ color: cell.cellType === 'code' ? C.BLUE : C.ORANGE }}>
                        {cell.cellType === 'code'
                          ? `In [${cell.executionCount || ' '}]:`
                          : 'Markdown'
                        }
                      </span>
                      <div className="cell-actions" style={{ display: 'flex', gap: '8px' }}>
                        <button onClick={() => toggleCellType(index)} style={{ padding: '2px 6px', backgroundColor: 'transparent', border: `1px solid ${C.CYAN}`, color: C.CYAN, fontSize: '9px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                          {cell.cellType === 'code' ? 'MD' : 'CODE'}
                        </button>
                        <ChevronUp size={12} onClick={() => moveCellUp(index)} style={{ color: index === 0 ? C.GRAY : C.WHITE, cursor: index === 0 ? 'not-allowed' : 'pointer' }} />
                        <ChevronDown size={12} onClick={() => moveCellDown(index)} style={{ color: index === notebookCells.length - 1 ? C.GRAY : C.WHITE, cursor: index === notebookCells.length - 1 ? 'not-allowed' : 'pointer' }} />
                        {cell.cellType === 'code' && (
                          <Play size={12} onClick={() => executeCell(index)} style={{ color: C.GREEN, cursor: 'pointer' }} />
                        )}
                        <Trash2 size={12} onClick={() => deleteCell(index)} style={{ color: notebookCells.length === 1 ? C.GRAY : C.RED, cursor: notebookCells.length === 1 ? 'not-allowed' : 'pointer' }} />
                      </div>
                    </div>

                    {/* Cell Content */}
                    <textarea
                      value={cell.source.join('\n')}
                      onChange={(e) => updateCellContent(index, e.target.value)}
                      placeholder={cell.cellType === 'code' ? "# Enter Python code here..." : "Enter markdown text here..."}
                      style={{
                        width: '100%',
                        minHeight: '100px',
                        backgroundColor: C.EDITOR_BG,
                        color: C.WHITE,
                        border: 'none',
                        outline: 'none',
                        resize: 'vertical',
                        padding: '12px',
                        fontSize: '13px',
                        fontFamily: 'Consolas, monospace'
                      }}
                    />

                    {/* Cell Output (only for code cells) */}
                    {cell.cellType === 'code' && (
                      <div style={{ backgroundColor: C.DARK_BG, padding: '12px', borderTop: `1px solid ${C.GRAY}` }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                          <div style={{ color: C.GREEN, fontSize: '10px' }}>
                            Output {cell.executionCount ? `[${cell.executionCount}]` : ''}:
                          </div>
                          {cell.outputs.length > 0 && (
                            <button onClick={() => clearCellOutput(index)} style={{ padding: '2px 6px', backgroundColor: 'transparent', border: `1px solid ${C.RED}`, color: C.RED, fontSize: '8px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                              CLEAR
                            </button>
                          )}
                        </div>
                        {renderCellOutput(cell.outputs)}
                      </div>
                    )}

                    {/* Add Cell Button */}
                    <div style={{ textAlign: 'center', padding: '4px', backgroundColor: C.DARK_BG, borderTop: `1px solid ${C.GRAY}`, opacity: 0.7 }}>
                      <button onClick={() => addCell(index)} style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px dashed ${C.GRAY}`, color: C.GRAY, fontSize: '9px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                        + ADD CELL BELOW
                      </button>
                    </div>
                  </div>
                ))}

                <div style={{ textAlign: 'center', padding: '20px' }}>
                  <button onClick={() => addCell()} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px dashed ${C.GREEN}`, color: C.GREEN, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    + ADD CELL
                  </button>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* Footer */}
      <div style={{ borderTop: `2px solid ${C.ORANGE}`, backgroundColor: C.PANEL_BG, padding: '8px 16px', fontSize: '11px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <span style={{ color: C.ORANGE, fontWeight: 'bold' }}>FINCEPT CODE EDITOR v1.0</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.CYAN }}>FinScript Language Support</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.BLUE }}>Jupyter Integration</span>
            {editorMode === 'notebook' && currentNotebookPath && (
              <>
                <span style={{ color: C.GRAY }}>|</span>
                <span style={{ color: C.YELLOW, fontSize: '10px' }}>{currentNotebookPath.split(/[/\\]/).pop()}</span>
              </>
            )}
          </div>
          <div style={{ color: C.GRAY }}>
            Mode: <span style={{ color: C.ORANGE, fontWeight: 'bold' }}>{editorMode.toUpperCase()}</span>
            {editorMode === 'code' ? ` | Files: ${files.length}` : ` | Cells: ${notebookCells.length} | Exec: ${executionCounter - 1}`}
          </div>
        </div>
      </div>
    </div>
  );
}
