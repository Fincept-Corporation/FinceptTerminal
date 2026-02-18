import React, { useState, useRef, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  Play, Save, FileText, Plus, X, Terminal as TerminalIcon,
  Upload, Trash2, ChevronRight, Code, Braces, Hash,
  AlertTriangle, CheckCircle, Zap, FolderOpen, Copy,
  RotateCcw, Search, Replace, ChevronDown, Settings,
  Maximize2, Minimize2, SplitSquareHorizontal, BookOpen
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { open, save } from '@tauri-apps/plugin-dialog';
import { FinScriptOutputPanel, FinScriptExecutionResult } from './FinScriptOutputPanel';
import { JupyterNotebookPanel } from './JupyterNotebookPanel';
import { showConfirm } from '@/utils/notifications';
import { FinScriptDocsPanel } from './FinScriptDocsPanel';
import { FinScriptTooltip, FINSCRIPT_FUNCTIONS } from './FinScriptTooltip';
import { EXAMPLE_SCRIPTS, type ExampleScript } from './finscriptExamples';

// ─── FINCEPT Design System ──────────────────────────────────────────────────
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
} as const;
const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ─── Interfaces ─────────────────────────────────────────────────────────────
interface EditorFile {
  id: string;
  name: string;
  content: string;
  language: 'finscript' | 'python' | 'javascript' | 'json';
  unsaved: boolean;
  cursorLine: number;
  cursorCol: number;
}

interface SearchState {
  isOpen: boolean;
  query: string;
  replaceQuery: string;
  showReplace: boolean;
  matchCount: number;
  currentMatch: number;
}

// ─── Language Config (moved inside component to access theme colors) ────────────────

const DEFAULT_FINSCRIPT = `// FinScript - EMA/RSI Crossover Strategy
// Data is generated synthetically for instant execution

// Calculate indicators on AAPL
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_val = rsi(AAPL, 14)

// Get latest values
fast = last(ema_fast)
slow = last(ema_slow)
rsi_now = last(rsi_val)

// Print analysis
print "EMA(12):", fast
print "EMA(26):", slow
print "RSI(14):", rsi_now

// Trading logic
if fast > slow {
    if rsi_now < 70 {
        buy "Bullish crossover + RSI not overbought"
    }
}

if fast < slow {
    if rsi_now > 30 {
        sell "Bearish crossover + RSI not oversold"
    }
}

// Visualize
plot_candlestick AAPL, "AAPL Price"
plot_line ema_fast, "EMA (12)", "cyan"
plot_line ema_slow, "EMA (26)", "orange"
plot rsi_val, "RSI (14)"
`;

// ─── Component ──────────────────────────────────────────────────────────────
export default function CodeEditorTab() {
  const { t } = useTranslation('codeEditor');

  // Language Config
  const LANG_CONFIG: Record<string, { icon: React.ReactNode; color: string; ext: string }> = {
    finscript: { icon: <Zap size={10} />, color: F.ORANGE, ext: '.fincept' },
    python: { icon: <Code size={10} />, color: F.BLUE, ext: '.py' },
    javascript: { icon: <Braces size={10} />, color: F.YELLOW, ext: '.js' },
    json: { icon: <Hash size={10} />, color: F.GREEN, ext: '.json' },
  };

  // ─── State ──────────────────────────────────────────────────────────────
  const [activeMode, setActiveMode] = useState<'finscript' | 'notebook'>('finscript');
  const [files, setFiles] = useState<EditorFile[]>([
    {
      id: '1',
      name: 'strategy.fincept',
      content: DEFAULT_FINSCRIPT,
      language: 'finscript',
      unsaved: false,
      cursorLine: 1,
      cursorCol: 1,
    }
  ]);
  const [activeFileId, setActiveFileId] = useState('1');
  const [isRunning, setIsRunning] = useState(false);
  const [finscriptResult, setFinscriptResult] = useState<FinScriptExecutionResult | null>(null);
  const [showExplorer, setShowExplorer] = useState(true);
  const [outputHeight, setOutputHeight] = useState(280);
  const [isOutputMaximized, setIsOutputMaximized] = useState(false);
  const [showOutput, setShowOutput] = useState(true);
  const [showDocs, setShowDocs] = useState(false);
  const [docsWidth, setDocsWidth] = useState(350);
  const [hoveredFunction, setHoveredFunction] = useState<{ name: string; x: number; y: number } | null>(null);
  const docsResizingRef = useRef(false);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeFileId, showExplorer, showOutput, activeMode,
  }), [activeFileId, showExplorer, showOutput, activeMode]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.activeFileId === 'string') setActiveFileId(state.activeFileId);
    if (typeof state.showExplorer === 'boolean') setShowExplorer(state.showExplorer);
    if (typeof state.showOutput === 'boolean') setShowOutput(state.showOutput);
    if (state.activeMode === 'finscript' || state.activeMode === 'notebook') setActiveMode(state.activeMode);
  }, []);

  useWorkspaceTabState('code-editor', getWorkspaceState, setWorkspaceState);
  const [search, setSearch] = useState<SearchState>({
    isOpen: false, query: '', replaceQuery: '', showReplace: false, matchCount: 0, currentMatch: 0
  });
  const [executionHistory, setExecutionHistory] = useState<Array<{ time: string; status: 'success' | 'error'; ms: number }>>([]);
  const [examplesExpanded, setExamplesExpanded] = useState(true);

  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const editorContainerRef = useRef<HTMLDivElement>(null);
  const resizingRef = useRef(false);

  const activeFile = files.find(f => f.id === activeFileId) || files[0];
  const lines = activeFile.content.split('\n');

  // ─── Keyboard Shortcuts ─────────────────────────────────────────────────
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey) {
        switch (e.key) {
          case 's':
            e.preventDefault();
            saveFile();
            break;
          case 'Enter':
            e.preventDefault();
            runCode();
            break;
          case 'f':
            e.preventDefault();
            setSearch(s => ({ ...s, isOpen: !s.isOpen }));
            break;
          case 'n':
            e.preventDefault();
            createNewFile();
            break;
          case 'b':
            e.preventDefault();
            setShowExplorer(v => !v);
            break;
          case 'j':
            e.preventDefault();
            setShowOutput(v => !v);
            break;
        }
      }
      if (e.key === 'F1') {
        e.preventDefault();
        setShowDocs(v => !v);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [activeFileId, files, isRunning]);

  // ─── Cursor Tracking ────────────────────────────────────────────────────
  const updateCursorPosition = useCallback(() => {
    if (!textareaRef.current) return;
    const pos = textareaRef.current.selectionStart;
    const text = activeFile.content.substring(0, pos);
    const line = text.split('\n').length;
    const col = pos - text.lastIndexOf('\n');
    setFiles(prev => prev.map(f =>
      f.id === activeFileId ? { ...f, cursorLine: line, cursorCol: col } : f
    ));
  }, [activeFileId, activeFile.content]);

  // ─── Output Resize ─────────────────────────────────────────────────────
  const handleResizeStart = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    resizingRef.current = true;
    const startY = e.clientY;
    const startHeight = outputHeight;

    const handleMove = (me: MouseEvent) => {
      if (!resizingRef.current) return;
      const delta = startY - me.clientY;
      setOutputHeight(Math.max(100, Math.min(600, startHeight + delta)));
    };

    const handleUp = () => {
      resizingRef.current = false;
      window.removeEventListener('mousemove', handleMove);
      window.removeEventListener('mouseup', handleUp);
    };

    window.addEventListener('mousemove', handleMove);
    window.addEventListener('mouseup', handleUp);
  }, [outputHeight]);

  // ─── Docs Resize ────────────────────────────────────────────────────────
  const handleDocsResizeStart = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    docsResizingRef.current = true;
    const startX = e.clientX;
    const startWidth = docsWidth;

    const handleMove = (me: MouseEvent) => {
      if (!docsResizingRef.current) return;
      const delta = startX - me.clientX;
      setDocsWidth(Math.max(250, Math.min(600, startWidth + delta)));
    };

    const handleUp = () => {
      docsResizingRef.current = false;
      window.removeEventListener('mousemove', handleMove);
      window.removeEventListener('mouseup', handleUp);
    };

    window.addEventListener('mousemove', handleMove);
    window.addEventListener('mouseup', handleUp);
  }, [docsWidth]);

  // ─── Hover Function Detection ───────────────────────────────────────────
  const handleEditorMouseMove = useCallback((e: React.MouseEvent<HTMLTextAreaElement>) => {
    if (!textareaRef.current) return;

    const textarea = textareaRef.current;
    const rect = textarea.getBoundingClientRect();
    const x = e.clientX - rect.left + textarea.scrollLeft;
    const y = e.clientY - rect.top + textarea.scrollTop;

    // Get character position from mouse coordinates
    const charWidth = 7.2; // approximate monospace char width at 12px
    const lineHeight = 20;
    const lineIndex = Math.floor(y / lineHeight);
    const charIndex = Math.floor(x / charWidth);

    if (lineIndex >= 0 && lineIndex < lines.length) {
      const line = lines[lineIndex];

      // Match function name pattern: word followed by (
      const wordMatch = line.slice(Math.max(0, charIndex - 20), charIndex + 20).match(/(\w+)\s*\(/);
      if (wordMatch) {
        const funcName = wordMatch[1];
        if (FINSCRIPT_FUNCTIONS[funcName]) {
          setHoveredFunction({
            name: funcName,
            x: e.clientX,
            y: e.clientY - 10,
          });
          return;
        }
      }
    }

    setHoveredFunction(null);
  }, [lines]);

  // ─── File Operations ────────────────────────────────────────────────────
  const updateFileContent = (content: string) => {
    setFiles(files.map(f =>
      f.id === activeFileId ? { ...f, content, unsaved: true } : f
    ));
  };

  const saveFile = async () => {
    setFiles(files.map(f =>
      f.id === activeFileId ? { ...f, unsaved: false } : f
    ));
  };

  const createNewFile = () => {
    const newId = Date.now().toString();
    const newFile: EditorFile = {
      id: newId,
      name: `untitled-${files.length + 1}.fincept`,
      content: '// New FinScript file\n\n',
      language: 'finscript',
      unsaved: false,
      cursorLine: 1,
      cursorCol: 1,
    };
    setFiles([...files, newFile]);
    setActiveFileId(newId);
  };

  const closeFile = async (id: string) => {
    if (files.length === 1) return;
    const fileToClose = files.find(f => f.id === id);
    if (fileToClose?.unsaved) {
      const confirmed = await showConfirm(
        t('dialogs.unsavedChanges'),
        { title: t('dialogs.closeUnsaved'), type: 'warning' }
      );
      if (!confirmed) return;
    }
    const newFiles = files.filter(f => f.id !== id);
    setFiles(newFiles);
    if (activeFileId === id) {
      setActiveFileId(newFiles[newFiles.length - 1].id);
    }
  };

  const loadExample = (example: ExampleScript) => {
    // Check if this example is already open
    const existing = files.find(f => f.name === example.name);
    if (existing) {
      setActiveFileId(existing.id);
      return;
    }
    const newId = `ex_${Date.now()}`;
    const newFile: EditorFile = {
      id: newId,
      name: example.name,
      content: example.content,
      language: 'finscript',
      unsaved: false,
      cursorLine: 1,
      cursorCol: 1,
    };
    setFiles(prev => [...prev, newFile]);
    setActiveFileId(newId);
  };

  const openFileFromDisk = async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [
          { name: 'FinScript', extensions: ['fincept', 'fin'] },
          { name: 'Python', extensions: ['py'] },
          { name: 'All Files', extensions: ['*'] },
        ]
      });
      if (!selected) return;
      const path = selected as string;
      const content = await invoke<string>('read_file_content', { filePath: path });
      const name = path.split(/[/\\]/).pop() || 'unknown';
      const ext = name.split('.').pop()?.toLowerCase();
      let language: EditorFile['language'] = 'finscript';
      if (ext === 'py') language = 'python';
      else if (ext === 'js') language = 'javascript';
      else if (ext === 'json') language = 'json';

      const newId = Date.now().toString();
      setFiles([...files, { id: newId, name, content, language, unsaved: false, cursorLine: 1, cursorCol: 1 }]);
      setActiveFileId(newId);
    } catch (error) {
      console.error('Failed to open file:', error);
    }
  };

  const saveFileToDisk = async () => {
    try {
      const selected = await save({
        filters: [
          { name: 'FinScript', extensions: ['fincept'] },
          { name: 'Python', extensions: ['py'] },
          { name: 'All Files', extensions: ['*'] },
        ]
      });
      if (!selected) return;
      await invoke('write_file_content', { filePath: selected, content: activeFile.content });
      setFiles(files.map(f => f.id === activeFileId ? { ...f, unsaved: false } : f));
    } catch (error) {
      console.error('Failed to save file:', error);
    }
  };

  // ─── Execution ──────────────────────────────────────────────────────────
  const executeFinScript = async () => {
    setIsRunning(true);
    setFinscriptResult(null);
    setShowOutput(true);

    try {
      const result = await invoke<FinScriptExecutionResult>('execute_finscript', {
        code: activeFile.content
      });
      setFinscriptResult(result);
      setExecutionHistory(prev => [...prev.slice(-9), {
        time: new Date().toLocaleTimeString(),
        status: result.success ? 'success' : 'error',
        ms: result.execution_time_ms
      }]);
    } catch (error) {
      const errorResult: FinScriptExecutionResult = {
        success: false,
        output: '',
        signals: [],
        plots: [],
        errors: [String(error)],
        execution_time_ms: 0,
      };
      setFinscriptResult(errorResult);
      setExecutionHistory(prev => [...prev.slice(-9), {
        time: new Date().toLocaleTimeString(),
        status: 'error',
        ms: 0
      }]);
    } finally {
      setIsRunning(false);
    }
  };

  const runCode = () => {
    if (activeFile.language === 'finscript') {
      executeFinScript();
    }
  };

  // ─── Editor Tab Indent ──────────────────────────────────────────────────
  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Tab') {
      e.preventDefault();
      const textarea = textareaRef.current;
      if (!textarea) return;
      const start = textarea.selectionStart;
      const end = textarea.selectionEnd;
      const content = activeFile.content;
      const newContent = content.substring(0, start) + '    ' + content.substring(end);
      updateFileContent(newContent);
      setTimeout(() => {
        textarea.selectionStart = textarea.selectionEnd = start + 4;
      }, 0);
    }
  };

  // ─── Search ─────────────────────────────────────────────────────────────
  const getMatchCount = useCallback(() => {
    if (!search.query) return 0;
    try {
      const regex = new RegExp(search.query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi');
      return (activeFile.content.match(regex) || []).length;
    } catch { return 0; }
  }, [search.query, activeFile.content]);

  // ─── Render ─────────────────────────────────────────────────────────────
  const effectiveOutputHeight = isOutputMaximized ? 500 : (showOutput ? outputHeight : 0);

  const editorCSS = [
    '.ce-scroll::-webkit-scrollbar { width: 6px; height: 6px; }',
    `.ce-scroll::-webkit-scrollbar-track { background: ${F.DARK_BG}; }`,
    `.ce-scroll::-webkit-scrollbar-thumb { background: ${F.BORDER}; border-radius: 3px; }`,
    `.ce-scroll::-webkit-scrollbar-thumb:hover { background: ${F.MUTED}; }`,
    `.ce-tab:hover { background-color: ${F.HOVER} !important; }`,
    `.ce-explorer-item:hover { background-color: ${F.HOVER}; }`,
    `.ce-resize-handle:hover { background-color: ${F.ORANGE}; }`,
  ].join('\n');

  return (
    <div style={{
      width: '100%', height: '100%',
      backgroundColor: F.DARK_BG,
      color: F.WHITE,
      fontFamily: FONT,
      display: 'flex', flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* ─── CSS ──────────────────────────────────────────────────────── */}
      <style dangerouslySetInnerHTML={{ __html: editorCSS }} />

      {/* ─── Top Navigation Bar ───────────────────────────────────────── */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        padding: '8px 16px',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${F.ORANGE}20`,
        minHeight: '40px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Zap size={14} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>{t('header.finscriptIde')}</span>
          <div style={{ width: '1px', height: '16px', backgroundColor: F.BORDER }} />

          {/* Mode Switcher Tabs */}
          <div style={{ display: 'flex', gap: '2px' }}>
            {(['finscript', 'notebook'] as const).map(mode => (
              <button
                key={mode}
                onClick={() => setActiveMode(mode)}
                style={{
                  padding: '4px 12px',
                  backgroundColor: activeMode === mode ? F.ORANGE : 'transparent',
                  color: activeMode === mode ? F.DARK_BG : F.GRAY,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                  cursor: 'pointer', fontFamily: FONT,
                  transition: 'all 0.2s',
                }}
              >
                {mode === 'finscript' ? t('tabs.finscript') : t('tabs.notebook')}
              </button>
            ))}
          </div>
        </div>

        {activeMode === 'finscript' && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          {/* Toggle Explorer */}
          <button
            onClick={() => setShowExplorer(v => !v)}
            title={t('toolbar.toggleExplorer')}
            style={{
              padding: '4px 8px', backgroundColor: showExplorer ? `${F.ORANGE}15` : 'transparent',
              border: `1px solid ${showExplorer ? F.ORANGE : F.BORDER}`, borderRadius: '2px',
              color: showExplorer ? F.ORANGE : F.GRAY, fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', fontFamily: FONT, transition: 'all 0.2s',
            }}
          >
            <FolderOpen size={11} />
          </button>

          {/* Open File */}
          <button
            onClick={openFileFromDisk}
            title={t('toolbar.openFile')}
            style={{
              padding: '4px 8px', backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`, borderRadius: '2px',
              color: F.GRAY, fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', fontFamily: FONT, transition: 'all 0.2s',
            }}
          >
            <Upload size={11} />
          </button>

          {/* Save */}
          <button
            onClick={saveFile}
            disabled={!activeFile.unsaved}
            title={t('toolbar.saveShortcut')}
            style={{
              padding: '4px 8px', backgroundColor: 'transparent',
              border: `1px solid ${activeFile.unsaved ? F.YELLOW : F.BORDER}`, borderRadius: '2px',
              color: activeFile.unsaved ? F.YELLOW : F.GRAY, fontSize: '9px', fontWeight: 700,
              cursor: activeFile.unsaved ? 'pointer' : 'default', fontFamily: FONT,
              opacity: activeFile.unsaved ? 1 : 0.5, transition: 'all 0.2s',
            }}
          >
            <Save size={11} />
          </button>

          {/* Search */}
          <button
            onClick={() => setSearch(s => ({ ...s, isOpen: !s.isOpen }))}
            title={t('toolbar.findShortcut')}
            style={{
              padding: '4px 8px', backgroundColor: search.isOpen ? `${F.CYAN}20` : 'transparent',
              border: `1px solid ${search.isOpen ? F.CYAN : F.BORDER}`, borderRadius: '2px',
              color: search.isOpen ? F.CYAN : F.GRAY, fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', fontFamily: FONT, transition: 'all 0.2s',
            }}
          >
            <Search size={11} />
          </button>

          <div style={{ width: '1px', height: '18px', backgroundColor: F.BORDER, margin: '0 4px' }} />

          {/* Docs Toggle Button */}
          <button
            onClick={() => setShowDocs(v => !v)}
            title="Toggle Documentation Sidebar (F1)"
            style={{
              padding: '4px 8px',
              backgroundColor: showDocs ? `${F.CYAN}20` : 'transparent',
              border: `1px solid ${showDocs ? F.CYAN : F.BORDER}`,
              borderRadius: '2px',
              color: showDocs ? F.CYAN : F.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              fontFamily: FONT,
              transition: 'all 0.2s',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <BookOpen size={11} />
            {showDocs ? 'HIDE DOCS' : 'DOCS'}
          </button>

          <div style={{ width: '1px', height: '18px', backgroundColor: F.BORDER, margin: '0 4px' }} />

          {/* Run Button */}
          <button
            onClick={runCode}
            disabled={isRunning || activeFile.language !== 'finscript'}
            title={t('toolbar.runShortcut')}
            style={{
              padding: '5px 14px', display: 'flex', alignItems: 'center', gap: '6px',
              backgroundColor: isRunning ? F.MUTED : F.ORANGE,
              color: F.DARK_BG, border: 'none', borderRadius: '2px',
              fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
              cursor: isRunning ? 'not-allowed' : 'pointer', fontFamily: FONT,
              transition: 'all 0.2s',
            }}
          >
            <Play size={11} fill={F.DARK_BG} />
            {isRunning ? t('toolbar.running') : t('toolbar.run')}
          </button>
        </div>
        )}
      </div>

      {/* ─── Notebook Mode ────────────────────────────────────────────── */}
      {activeMode === 'notebook' && (
        <div style={{ flex: 1, minHeight: 0 }}>
          <JupyterNotebookPanel />
        </div>
      )}

      {/* ─── FinScript Mode: Main Area ─────────────────────────────────── */}
      {activeMode === 'finscript' && <>
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>

        {/* ─── Explorer Panel ─────────────────────────────────────────── */}
        {showExplorer && (
          <div style={{
            width: '240px', backgroundColor: F.PANEL_BG,
            borderRight: `1px solid ${F.BORDER}`,
            display: 'flex', flexDirection: 'column',
          }}>
            {/* Explorer Header */}
            <div style={{
              padding: '12px',
              backgroundColor: F.HEADER_BG,
              borderBottom: `1px solid ${F.BORDER}`,
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
                {t('explorer.title')}
              </span>
              <button
                onClick={createNewFile}
                title={t('toolbar.newFile')}
                style={{
                  padding: '2px 4px', backgroundColor: 'transparent',
                  border: 'none', color: F.GRAY, cursor: 'pointer',
                  transition: 'all 0.2s',
                }}
              >
                <Plus size={12} />
              </button>
            </div>

            {/* File List */}
            <div className="ce-scroll" style={{ flex: 1, overflowY: 'auto', padding: '4px 0' }}>
              {/* Section Label */}
              <div style={{
                padding: '8px 12px', display: 'flex', alignItems: 'center', gap: '4px',
                fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
              }}>
                <ChevronDown size={10} />
                {t('explorer.openFiles')} ({files.length})
              </div>

              {files.map(file => {
                const lang = LANG_CONFIG[file.language];
                const isActive = file.id === activeFileId;
                return (
                  <div
                    key={file.id}
                    className="ce-explorer-item"
                    onClick={() => setActiveFileId(file.id)}
                    style={{
                      padding: '7px 12px 7px 24px',
                      display: 'flex', alignItems: 'center', gap: '8px',
                      backgroundColor: isActive ? `${F.ORANGE}15` : 'transparent',
                      borderLeft: isActive ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer', transition: 'all 0.2s',
                    }}
                  >
                    <span style={{ color: lang.color }}>{lang.icon}</span>
                    <span style={{
                      fontSize: '10px', color: isActive ? F.WHITE : F.GRAY,
                      flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                    }}>
                      {file.name}
                    </span>
                    {file.unsaved && (
                      <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: F.YELLOW }} />
                    )}
                    {files.length > 1 && (
                      <X size={10}
                        onClick={(e) => { e.stopPropagation(); closeFile(file.id); }}
                        style={{ color: F.MUTED, cursor: 'pointer', opacity: 0.6 }}
                      />
                    )}
                  </div>
                );
              })}

              {/* Examples Section */}
              <div style={{
                borderTop: `1px solid ${F.BORDER}`, marginTop: '8px',
              }}>
                <div
                  onClick={() => setExamplesExpanded(!examplesExpanded)}
                  style={{
                    padding: '10px 12px 6px', display: 'flex', alignItems: 'center', gap: '4px',
                    fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
                    cursor: 'pointer',
                  }}
                >
                  <ChevronDown size={10} style={{
                    transform: examplesExpanded ? 'rotate(0deg)' : 'rotate(-90deg)',
                    transition: 'transform 0.2s',
                  }} />
                  <BookOpen size={10} style={{ color: F.ORANGE }} />
                  {t('explorer.examples')}
                </div>
                {examplesExpanded && (
                  <div>
                    {Array.from(new Set(EXAMPLE_SCRIPTS.map(e => e.category))).map(category => (
                      <div key={category}>
                        <div style={{
                          padding: '6px 12px 2px 20px',
                          fontSize: '9px', color: F.MUTED, fontWeight: 700,
                          letterSpacing: '0.5px', textTransform: 'uppercase',
                        }}>
                          {category}
                        </div>
                        {EXAMPLE_SCRIPTS.filter(e => e.category === category).map(example => (
                          <div
                            key={example.id}
                            className="ce-explorer-item"
                            onClick={() => loadExample(example)}
                            title={example.description}
                            style={{
                              padding: '5px 12px 5px 28px',
                              display: 'flex', alignItems: 'center', gap: '6px',
                              cursor: 'pointer', transition: 'all 0.2s',
                            }}
                          >
                            <Zap size={9} style={{ color: F.ORANGE, flexShrink: 0 }} />
                            <span style={{
                              fontSize: '10px', color: F.GRAY,
                              overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                            }}>
                              {example.name.replace('.fincept', '')}
                            </span>
                          </div>
                        ))}
                      </div>
                    ))}
                  </div>
                )}
              </div>

              {/* Execution History */}
              {executionHistory.length > 0 && (
                <>
                  <div style={{
                    padding: '10px 12px 6px', display: 'flex', alignItems: 'center', gap: '4px',
                    fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
                    borderTop: `1px solid ${F.BORDER}`, marginTop: '8px',
                  }}>
                    <ChevronDown size={10} />
                    {t('explorer.runHistory')}
                  </div>
                  {executionHistory.slice().reverse().map((h, idx) => (
                    <div key={idx} style={{
                      padding: '4px 12px 4px 24px',
                      display: 'flex', alignItems: 'center', gap: '6px',
                      fontSize: '9px',
                    }}>
                      {h.status === 'success'
                        ? <CheckCircle size={9} style={{ color: F.GREEN }} />
                        : <AlertTriangle size={9} style={{ color: F.RED }} />
                      }
                      <span style={{ color: F.GRAY }}>{h.time}</span>
                      <span style={{ color: F.CYAN }}>{h.ms}ms</span>
                    </div>
                  ))}
                </>
              )}
            </div>

            {/* Explorer Footer */}
            <div style={{
              padding: '8px 12px',
              borderTop: `1px solid ${F.BORDER}`,
              fontSize: '9px', color: F.GRAY,
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <Zap size={9} style={{ color: F.ORANGE }} />
                <span>{t('explorer.finscriptEngine')}</span>
              </div>
            </div>
          </div>
        )}

        {/* ─── Editor + Output ────────────────────────────────────────── */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>

          {/* ─── Tab Bar ──────────────────────────────────────────────── */}
          <div style={{
            backgroundColor: F.HEADER_BG,
            borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', alignItems: 'center',
            overflow: 'hidden',
          }}>
            <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
              {files.map(file => {
                const lang = LANG_CONFIG[file.language];
                const isActive = file.id === activeFileId;
                return (
                  <div
                    key={file.id}
                    className={isActive ? '' : 'ce-tab'}
                    onClick={() => setActiveFileId(file.id)}
                    style={{
                      padding: '8px 14px',
                      display: 'flex', alignItems: 'center', gap: '6px',
                      backgroundColor: isActive ? F.PANEL_BG : 'transparent',
                      borderRight: `1px solid ${F.BORDER}`,
                      borderBottom: isActive ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer', transition: 'all 0.2s',
                      maxWidth: '180px',
                    }}
                  >
                    <span style={{ color: lang.color }}>{lang.icon}</span>
                    <span style={{
                      fontSize: '10px',
                      color: isActive ? F.WHITE : F.GRAY,
                      overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                    }}>
                      {file.name}{file.unsaved ? ' *' : ''}
                    </span>
                    {files.length > 1 && (
                      <X size={10}
                        onClick={(e) => { e.stopPropagation(); closeFile(file.id); }}
                        style={{ color: F.MUTED, cursor: 'pointer' }}
                      />
                    )}
                  </div>
                );
              })}
            </div>
          </div>

          {/* ─── Search Bar ───────────────────────────────────────────── */}
          {search.isOpen && (
            <div style={{
              backgroundColor: F.HEADER_BG,
              borderBottom: `1px solid ${F.BORDER}`,
              padding: '6px 12px',
              display: 'flex', alignItems: 'center', gap: '8px',
            }}>
              <Search size={12} style={{ color: F.CYAN }} />
              <input
                autoFocus
                type="text"
                value={search.query}
                onChange={(e) => setSearch(s => ({ ...s, query: e.target.value }))}
                placeholder="Find..."
                style={{
                  width: '200px', padding: '6px 10px',
                  backgroundColor: F.DARK_BG, color: F.WHITE,
                  border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                  fontSize: '10px', fontFamily: FONT,
                  outline: 'none',
                }}
              />
              {search.query && (
                <span style={{ fontSize: '9px', color: F.CYAN }}>
                  {getMatchCount()} matches
                </span>
              )}
              <button
                onClick={() => setSearch(s => ({ ...s, showReplace: !s.showReplace }))}
                style={{
                  padding: '2px 6px', backgroundColor: 'transparent',
                  border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                  color: F.GRAY, fontSize: '9px', cursor: 'pointer', fontFamily: FONT,
                }}
              >
                <Replace size={10} />
              </button>
              <X size={12}
                style={{ color: F.GRAY, cursor: 'pointer' }}
                onClick={() => setSearch(s => ({ ...s, isOpen: false }))}
              />
            </div>
          )}

          {/* ─── Editor Area ──────────────────────────────────────────── */}
          <div ref={editorContainerRef} style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
            {/* Editor Section */}
            <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
              {/* Line Numbers Gutter */}
              <div className="ce-scroll" style={{
                width: '52px', backgroundColor: F.DARK_BG,
                borderRight: `1px solid ${F.BORDER}`,
                overflowY: 'hidden', padding: '12px 0',
                userSelect: 'none',
              }}>
                {lines.map((_, i) => (
                  <div key={i} style={{
                    padding: '0 10px', textAlign: 'right',
                    fontSize: '11px', lineHeight: '20px',
                    color: (i + 1) === activeFile.cursorLine ? F.ORANGE : F.MUTED,
                    fontFamily: FONT,
                  }}>
                    {i + 1}
                  </div>
                ))}
              </div>

              {/* Text Editor */}
              <textarea
                ref={textareaRef}
                className="ce-scroll"
                value={activeFile.content}
                onChange={(e) => updateFileContent(e.target.value)}
                onKeyUp={updateCursorPosition}
                onClick={updateCursorPosition}
                onKeyDown={handleKeyDown}
                onMouseMove={handleEditorMouseMove}
                onMouseLeave={() => setHoveredFunction(null)}
                spellCheck={false}
                style={{
                  flex: 1,
                  backgroundColor: F.PANEL_BG,
                  color: F.WHITE,
                  border: 'none', outline: 'none', resize: 'none',
                  padding: '12px 16px',
                  fontSize: '12px', lineHeight: '20px',
                  fontFamily: FONT,
                  whiteSpace: 'pre', overflowWrap: 'normal',
                  overflowX: 'auto',
                  tabSize: 4,
                  caretColor: F.ORANGE,
                }}
              />

              {/* Minimap Gutter */}
              <div style={{
                width: '4px', backgroundColor: F.DARK_BG,
                borderLeft: `1px solid ${F.BORDER}`,
                position: 'relative',
              }}>
                <div style={{
                  position: 'absolute', top: '0',
                  width: '100%', height: `${Math.min(100, (20 / lines.length) * 100)}%`,
                  backgroundColor: `${F.ORANGE}40`, borderRadius: '2px',
                }} />
              </div>
            </div>

            {/* Docs Resize Handle */}
            {showDocs && (
              <div
                onMouseDown={handleDocsResizeStart}
                style={{
                  width: '3px',
                  backgroundColor: F.BORDER,
                  cursor: 'ew-resize',
                  transition: 'background-color 0.2s',
                }}
                onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.CYAN; }}
                onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = F.BORDER; }}
              />
            )}

            {/* Docs Panel */}
            {showDocs && (
              <div style={{
                width: `${docsWidth}px`,
                height: '100%',
                display: 'flex',
                flexDirection: 'column',
                flexShrink: 0,
                overflow: 'hidden'
              }}>
                <FinScriptDocsPanel
                  onClose={() => setShowDocs(false)}
                  onInsertExample={(code) => {
                    updateFileContent(activeFile.content + '\n\n' + code);
                  }}
                />
              </div>
            )}
          </div>

          {/* Tooltip */}
          {hoveredFunction && (
            <FinScriptTooltip
              functionName={hoveredFunction.name}
              position={{ x: hoveredFunction.x, y: hoveredFunction.y }}
            />
          )}

          {/* ─── Resize Handle ────────────────────────────────────────── */}
          {showOutput && (
            <div
              className="ce-resize-handle"
              onMouseDown={handleResizeStart}
              style={{
                height: '3px', backgroundColor: F.BORDER,
                cursor: 'ns-resize', transition: 'background-color 0.2s',
              }}
            />
          )}

          {/* ─── Output Panel ─────────────────────────────────────────── */}
          {showOutput && (
            <div style={{
              height: `${effectiveOutputHeight}px`,
              display: 'flex', flexDirection: 'column',
              borderTop: `1px solid ${F.BORDER}`,
            }}>
              {/* Output Header */}
              <div style={{
                backgroundColor: F.HEADER_BG,
                padding: '8px 12px',
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                borderBottom: `1px solid ${F.BORDER}`,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <TerminalIcon size={11} style={{ color: F.GREEN }} />
                  <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
                    {t('output.title')}
                  </span>
                  {finscriptResult && (
                    <span style={{
                      padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                      backgroundColor: finscriptResult.success ? `${F.GREEN}20` : `${F.RED}20`,
                      color: finscriptResult.success ? F.GREEN : F.RED,
                    }}>
                      {finscriptResult.success ? t('output.success') : t('output.error')}
                    </span>
                  )}
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <button
                    onClick={() => setIsOutputMaximized(v => !v)}
                    style={{
                      padding: '2px 4px', backgroundColor: 'transparent',
                      border: 'none', color: F.GRAY, cursor: 'pointer',
                    }}
                  >
                    {isOutputMaximized ? <Minimize2 size={10} /> : <Maximize2 size={10} />}
                  </button>
                  <button
                    onClick={() => setShowOutput(false)}
                    style={{
                      padding: '2px 4px', backgroundColor: 'transparent',
                      border: 'none', color: F.GRAY, cursor: 'pointer',
                    }}
                  >
                    <X size={10} />
                  </button>
                </div>
              </div>

              {/* Output Content */}
              <div style={{ flex: 1, overflow: 'hidden' }}>
                <FinScriptOutputPanel
                  result={finscriptResult}
                  isRunning={isRunning}
                  onClear={() => setFinscriptResult(null)}
                  colors={{
                    primary: F.ORANGE,
                    text: F.WHITE,
                    secondary: F.GREEN,
                    alert: F.RED,
                    warning: F.YELLOW,
                    info: F.BLUE,
                    accent: F.CYAN,
                    textMuted: F.GRAY,
                    background: F.DARK_BG,
                    panel: F.PANEL_BG,
                  }}
                  fontFamily={FONT}
                />
              </div>
            </div>
          )}
        </div>
      </div>

      {/* ─── Status Bar ───────────────────────────────────────────────── */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderTop: `1px solid ${F.BORDER}`,
        padding: '4px 16px',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        fontSize: '9px', color: F.GRAY, fontFamily: FONT,
        minHeight: '24px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {/* Language Indicator */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span style={{ color: LANG_CONFIG[activeFile.language].color }}>
              {LANG_CONFIG[activeFile.language].icon}
            </span>
            <span style={{ letterSpacing: '0.5px', fontWeight: 700 }}>{activeFile.language.toUpperCase()}</span>
          </div>

          <span style={{ color: F.MUTED }}>|</span>

          {/* Cursor Position */}
          <span style={{ color: F.CYAN }}>Ln {activeFile.cursorLine}, Col {activeFile.cursorCol}</span>

          <span style={{ color: F.MUTED }}>|</span>

          {/* Line Count */}
          <span>{lines.length} {t('statusBar.lines')}</span>

          {activeFile.unsaved && (
            <>
              <span style={{ color: F.MUTED }}>|</span>
              <span style={{ color: F.YELLOW, fontWeight: 700 }}>{t('statusBar.modified')}</span>
            </>
          )}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {/* Output Toggle */}
          {!showOutput && (
            <button
              onClick={() => setShowOutput(true)}
              style={{
                padding: '2px 8px', backgroundColor: 'transparent',
                border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                color: F.GRAY, fontSize: '8px', cursor: 'pointer', fontFamily: FONT,
                fontWeight: 700, letterSpacing: '0.5px',
              }}
            >
              {t('statusBar.showOutput')}
            </button>
          )}

          {/* Encoding */}
          <span>{t('statusBar.encoding')}</span>
          <span style={{ color: F.MUTED }}>|</span>
          <span>{t('statusBar.spaces')}</span>
          <span style={{ color: F.MUTED }}>|</span>

          {/* Shortcuts */}
          <span style={{ color: F.MUTED }}>{t('statusBar.runShortcut')}</span>
        </div>
      </div>
      </>}
    </div>
  );
}
