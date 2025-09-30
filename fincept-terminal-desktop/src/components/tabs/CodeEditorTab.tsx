import React, { useState, useRef } from 'react';
import { Play, Save, FileText, Plus, X, RefreshCw, Terminal as TerminalIcon, Download } from 'lucide-react';

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

export default function CodeEditorTab() {
  const [editorMode, setEditorMode] = useState<'code' | 'notebook'>('code');
  const [files, setFiles] = useState<EditorFile[]>([
    {
      id: '1',
      name: 'strategy.fincept',
      content: `// FinScript - Financial Analysis Language
// Example: EMA/WMA Crossover Strategy

if ema(AAPL, 21) > wma(AAPL, 50) {
    buy "EMA 21 crosses WMA 50 - Buy Signal"
}

if ema(AAPL, 21) < wma(AAPL, 50) {
    sell "EMA 21 crosses below WMA 50 - Sell Signal"
}

plot ema(AAPL, 7), "EMA (7)"
plot wma(AAPL, 21), "WMA (21)"
plot_candlestick AAPL, "Apple Stock Chart"`,
      language: 'finscript',
      unsaved: false
    }
  ]);
  const [activeFileId, setActiveFileId] = useState('1');
  const [output, setOutput] = useState<ExecutionResult[]>([]);
  const [isRunning, setIsRunning] = useState(false);

  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // Colors
  const C = {
    ORANGE: '#FF8C00',
    WHITE: '#FFFFFF',
    GREEN: '#00FF00',
    RED: '#FF0000',
    YELLOW: '#FFFF00',
    BLUE: '#4169E1',
    CYAN: '#00FFFF',
    GRAY: '#787878',
    DARK_BG: '#000000',
    PANEL_BG: '#0a0a0a',
    EDITOR_BG: '#1a1a1a'
  };

  const activeFile = files.find(f => f.id === activeFileId) || files[0];

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

    // Simulate execution (in production, this would call the Rust backend)
    setTimeout(() => {
      const mockOutput = `
=== FinScript Execution Results ===
File: ${activeFile.name}
Status: Completed Successfully

Fetching data for AAPL...
Calculating EMA(7), EMA(21), WMA(21), WMA(50)...

Signal Generated:
  ✓ BUY: EMA 21 crosses WMA 50 - Buy Signal

Chart Generated:
  ✓ Candlestick Chart: Apple Stock Chart
  ✓ Plot: EMA (7) - Current: 184.52
  ✓ Plot: WMA (21) - Current: 182.34

Execution Time: 1.24s
Memory Used: 2.4 MB
`;
      setOutput([...output, {
        output: mockOutput,
        timestamp: new Date()
      }]);
      setIsRunning(false);
    }, 1500);
  };

  const executePython = async () => {
    setIsRunning(true);
    setOutput([...output, {
      output: `Executing Python code...`,
      timestamp: new Date()
    }]);

    setTimeout(() => {
      const mockOutput = `
=== Python Execution Results ===
Hello from Python!
Numpy version: 1.24.3
Pandas version: 2.0.2

Successfully executed.
`;
      setOutput([...output, {
        output: mockOutput,
        timestamp: new Date()
      }]);
      setIsRunning(false);
    }, 1000);
  };

  const runCode = () => {
    if (activeFile.language === 'finscript') {
      executeFinScript();
    } else if (activeFile.language === 'python') {
      executePython();
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
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <button onClick={createNewFile} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GREEN}`, color: C.GREEN, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              <Plus size={14} style={{ display: 'inline', marginRight: '4px' }} />NEW FILE
            </button>
            <button onClick={saveFile} disabled={!activeFile.unsaved} style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${activeFile.unsaved ? C.YELLOW : C.GRAY}`, color: activeFile.unsaved ? C.YELLOW : C.GRAY, fontSize: '11px', fontWeight: 'bold', cursor: activeFile.unsaved ? 'pointer' : 'not-allowed', fontFamily: 'Consolas, monospace' }}>
              <Save size={14} style={{ display: 'inline', marginRight: '4px' }} />SAVE
            </button>
            <button onClick={runCode} disabled={isRunning} style={{ padding: '6px 12px', backgroundColor: isRunning ? C.GRAY : C.GREEN, border: 'none', color: C.DARK_BG, fontSize: '11px', fontWeight: 'bold', cursor: isRunning ? 'not-allowed' : 'pointer', fontFamily: 'Consolas, monospace' }}>
              <Play size={14} style={{ display: 'inline', marginRight: '4px' }} />{isRunning ? 'RUNNING...' : 'RUN CODE'}
            </button>
          </div>
        </div>
      </div>

      {/* File Tabs */}
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

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
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
            <div style={{ backgroundColor: C.PANEL_BG, padding: '12px', borderBottom: `1px solid ${C.GRAY}` }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ color: C.ORANGE, fontSize: '12px', fontWeight: 'bold' }}>JUPYTER NOTEBOOK</span>
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button style={{ padding: '6px 12px', backgroundColor: C.DARK_BG, border: `2px solid ${C.BLUE}`, color: C.BLUE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    <Plus size={14} style={{ display: 'inline', marginRight: '4px' }} />ADD CELL
                  </button>
                  <button style={{ padding: '6px 12px', backgroundColor: C.GREEN, border: 'none', color: C.DARK_BG, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    <Play size={14} style={{ display: 'inline', marginRight: '4px' }} />RUN ALL
                  </button>
                </div>
              </div>
            </div>

            <div className="code-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px', backgroundColor: '#050505' }}>
              <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
                {/* Cell 1 */}
                <div style={{ backgroundColor: C.PANEL_BG, border: `1px solid ${C.GRAY}`, marginBottom: '16px' }}>
                  <div style={{ backgroundColor: C.DARK_BG, padding: '6px 12px', borderBottom: `1px solid ${C.GRAY}`, fontSize: '10px', color: C.BLUE }}>
                    In [1]: Python Code
                  </div>
                  <textarea
                    placeholder="import pandas as pd&#10;import numpy as np&#10;&#10;# Your Python code here..."
                    style={{
                      width: '100%',
                      minHeight: '120px',
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
                  <div style={{ backgroundColor: C.DARK_BG, padding: '12px', borderTop: `1px solid ${C.GRAY}` }}>
                    <div style={{ color: C.GREEN, fontSize: '10px', marginBottom: '4px' }}>Output:</div>
                    <div style={{ color: C.GRAY, fontSize: '11px', fontStyle: 'italic' }}>Execute cell to see output</div>
                  </div>
                </div>

                {/* Cell 2 */}
                <div style={{ backgroundColor: C.PANEL_BG, border: `1px solid ${C.GRAY}`, marginBottom: '16px' }}>
                  <div style={{ backgroundColor: C.DARK_BG, padding: '6px 12px', borderBottom: `1px solid ${C.GRAY}`, fontSize: '10px', color: C.BLUE }}>
                    In [2]: Data Analysis
                  </div>
                  <textarea
                    placeholder="# Financial data analysis&#10;# Use pandas, matplotlib, seaborn..."
                    style={{
                      width: '100%',
                      minHeight: '120px',
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
                </div>

                <div style={{ textAlign: 'center', padding: '20px' }}>
                  <button style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px dashed ${C.GRAY}`, color: C.GRAY, fontSize: '11px', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    + Add New Cell
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
          </div>
          <div style={{ color: C.GRAY }}>
            Mode: <span style={{ color: C.ORANGE, fontWeight: 'bold' }}>{editorMode.toUpperCase()}</span> | Files: {files.length}
          </div>
        </div>
      </div>
    </div>
  );
}