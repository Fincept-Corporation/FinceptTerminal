import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  Save, X, Code, Settings, AlertCircle, CheckCircle,
  Copy, Undo, Play, ChevronDown, ChevronRight, FileCode, Edit3
} from 'lucide-react';
import type { PythonStrategy, CustomPythonStrategy, StrategyParameter } from '../types';
import {
  getPythonStrategyCode,
  saveCustomPythonStrategy,
  updateCustomPythonStrategy,
  validatePythonSyntax,
  extractStrategyParameters,
} from '../services/algoTradingService';
import ParameterEditor from './ParameterEditor';
import { F } from '../constants/theme';
import { S } from '../constants/styles';

const PYTHON_KEYWORDS = [
  'def', 'class', 'if', 'elif', 'else', 'for', 'while', 'try', 'except',
  'finally', 'with', 'as', 'import', 'from', 'return', 'yield', 'raise',
  'pass', 'break', 'continue', 'and', 'or', 'not', 'in', 'is', 'lambda',
  'True', 'False', 'None', 'self', 'async', 'await'
];

interface PythonStrategyEditorProps {
  strategy: PythonStrategy | CustomPythonStrategy;
  isCustom?: boolean;
  onSave?: (customId: string) => void;
  onClose: () => void;
  onBacktest?: (strategy: PythonStrategy) => void;
}

const PythonStrategyEditor: React.FC<PythonStrategyEditorProps> = ({
  strategy,
  isCustom = false,
  onSave,
  onClose,
  onBacktest,
}) => {
  const [name, setName] = useState(isCustom ? strategy.name : `${strategy.name} (Custom)`);
  const [code, setCode] = useState('');
  const [originalCode, setOriginalCode] = useState('');
  const [parameters, setParameters] = useState<StrategyParameter[]>([]);
  const [parameterValues, setParameterValues] = useState<Record<string, string>>({});
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [validating, setValidating] = useState(false);
  const [validation, setValidation] = useState<{ valid: boolean; error?: string } | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [advancedMode, setAdvancedMode] = useState(false);
  const [hasUnsavedChanges, setHasUnsavedChanges] = useState(false);
  const [cursorPosition, setCursorPosition] = useState({ line: 1, col: 1 });
  const [showParamSection, setShowParamSection] = useState(true);

  const textareaRef = useRef<HTMLTextAreaElement>(null);

  useEffect(() => { loadStrategyCode(); }, [strategy.id]);

  const loadStrategyCode = async () => {
    setLoading(true);
    setError(null);
    try {
      if (isCustom && (strategy as CustomPythonStrategy).code) {
        const customCode = (strategy as CustomPythonStrategy).code || '';
        setCode(customCode);
        setOriginalCode(customCode);
      } else {
        const result = await getPythonStrategyCode(
          isCustom ? (strategy as CustomPythonStrategy).base_strategy_id : strategy.id
        );
        if (result.success && result.data) {
          setCode(result.data.code);
          setOriginalCode(result.data.code);
        } else {
          setError(result.error || 'Failed to load strategy code');
        }
      }
      if (isCustom && (strategy as CustomPythonStrategy).parameters) {
        try {
          const params = JSON.parse((strategy as CustomPythonStrategy).parameters || '{}');
          setParameterValues(params);
        } catch { setParameterValues({}); }
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    if (code && advancedMode) extractParams();
  }, [code, advancedMode]);

  const extractParams = async () => {
    try {
      const result = await extractStrategyParameters(code);
      if (result.success && result.data) {
        setParameters(result.data);
        const newValues = { ...parameterValues };
        result.data.forEach((param) => {
          if (!(param.name in newValues)) newValues[param.name] = param.default_value;
        });
        setParameterValues(newValues);
      }
    } catch (err) { console.error('Failed to extract parameters:', err); }
  };

  useEffect(() => {
    const codeChanged = code !== originalCode;
    setHasUnsavedChanges(codeChanged);
    if (codeChanged) setValidation(null);
  }, [code, originalCode]);

  const handleValidate = async () => {
    setValidating(true);
    try {
      const result = await validatePythonSyntax(code);
      if (result.success && result.data) setValidation(result.data);
      else setValidation({ valid: false, error: result.error || 'Validation failed' });
    } catch (err) {
      setValidation({ valid: false, error: err instanceof Error ? err.message : 'Unknown error' });
    } finally { setValidating(false); }
  };

  const handleSave = async () => {
    setSaving(true);
    setError(null);
    try {
      if (isCustom) {
        const result = await updateCustomPythonStrategy({
          id: strategy.id, name, code, parameters: JSON.stringify(parameterValues),
        });
        if (result.success) {
          setOriginalCode(code);
          setHasUnsavedChanges(false);
          onSave?.(strategy.id);
        } else { setError(result.error || 'Failed to update strategy'); }
      } else {
        const result = await saveCustomPythonStrategy({
          baseStrategyId: strategy.id, name, code, parameters: JSON.stringify(parameterValues),
        });
        if (result.success && result.data) {
          setOriginalCode(code);
          setHasUnsavedChanges(false);
          onSave?.(result.data.id);
        } else { setError(result.error || 'Failed to save strategy'); }
      }
    } catch (err) { setError(err instanceof Error ? err.message : 'Unknown error'); }
    finally { setSaving(false); }
  };

  const handleCodeChange = (newCode: string) => { setCode(newCode); };
  const handleReset = () => { setCode(originalCode); setValidation(null); };
  const handleCopy = async () => {
    try { await navigator.clipboard.writeText(code); } catch (err) { console.error('Failed to copy:', err); }
  };

  const updateCursorPosition = useCallback(() => {
    if (!textareaRef.current) return;
    const pos = textareaRef.current.selectionStart;
    const text = code.substring(0, pos);
    const line = text.split('\n').length;
    const col = pos - text.lastIndexOf('\n');
    setCursorPosition({ line, col });
  }, [code]);

  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Tab') {
      e.preventDefault();
      const textarea = textareaRef.current;
      if (!textarea) return;
      const start = textarea.selectionStart;
      const end = textarea.selectionEnd;
      const newContent = code.substring(0, start) + '    ' + code.substring(end);
      handleCodeChange(newContent);
      setTimeout(() => { textarea.selectionStart = textarea.selectionEnd = start + 4; }, 0);
    }
  };

  const lines = code.split('\n');

  if (loading) {
    return (
      <div className={S.modalOverlay} style={{ backgroundColor: 'rgba(0,0,0,0.85)' }}>
        <div className="text-[12px]" style={{ color: F.GRAY }}>Loading strategy...</div>
      </div>
    );
  }

  return (
    <div className="fixed inset-0 flex flex-col" style={{ backgroundColor: 'rgba(0,0,0,0.9)', zIndex: 1000 }}>
      {/* Header */}
      <div
        className="flex items-center justify-between px-5 py-3 shrink-0"
        style={{ backgroundColor: F.HEADER_BG, borderBottom: `2px solid ${F.ORANGE}` }}
      >
        <div className="flex items-center gap-3">
          <FileCode size={18} style={{ color: F.ORANGE }} />
          <div>
            <input
              type="text"
              value={name}
              onChange={(e) => setName(e.target.value)}
              className="bg-transparent border-none text-[14px] font-bold py-1 outline-none w-[300px]"
              style={{ color: F.WHITE, borderBottom: `1px solid ${F.BORDER}` }}
              placeholder="Strategy Name"
            />
            <div className="text-[10px] mt-0.5" style={{ color: F.GRAY }}>
              {isCustom ? 'Editing custom strategy' : `Cloning from ${strategy.id}`}
              {strategy.category && ` - ${strategy.category}`}
            </div>
          </div>
        </div>

        <div className="flex items-center gap-2">
          {validation && (
            <span
              className={`${S.badge} px-3 py-1`}
              style={{
                backgroundColor: validation.valid ? `${F.GREEN}20` : `${F.RED}20`,
                border: `1px solid ${validation.valid ? F.GREEN : F.RED}`,
                color: validation.valid ? F.GREEN : F.RED,
              }}
            >
              {validation.valid ? <CheckCircle size={12} /> : <AlertCircle size={12} />}
              {validation.valid ? 'Valid' : 'Error'}
            </span>
          )}
          {hasUnsavedChanges && (
            <span className="text-[10px] font-bold" style={{ color: F.YELLOW }}>UNSAVED</span>
          )}
          <button onClick={handleValidate} disabled={validating} className={S.btnOutline}
            style={{ color: F.CYAN, borderColor: `${F.CYAN}40` }}
          >
            <CheckCircle size={12} />
            {validating ? 'CHECKING...' : 'VALIDATE'}
          </button>
          {onBacktest && (
            <button onClick={() => onBacktest(strategy)} className={S.btnOutline}
              style={{ color: F.BLUE, borderColor: `${F.BLUE}40` }}
            >
              <Play size={12} />
              BACKTEST
            </button>
          )}
          <button
            onClick={handleSave}
            disabled={saving || (!hasUnsavedChanges && isCustom)}
            className={`${S.btnPrimary} border-none`}
            style={{
              backgroundColor: F.ORANGE, color: F.DARK_BG,
              opacity: saving || (!hasUnsavedChanges && isCustom) ? 0.5 : 1,
              cursor: saving ? 'not-allowed' : 'pointer',
            }}
          >
            <Save size={12} />
            {saving ? 'SAVING...' : isCustom ? 'UPDATE' : 'SAVE AS CUSTOM'}
          </button>
          <button onClick={onClose} className={S.btnGhost} style={{ color: F.GRAY }}>
            <X size={16} />
          </button>
        </div>
      </div>

      {/* Error display */}
      {error && (
        <div
          className="flex items-center gap-2 px-5 py-2.5 text-[11px] shrink-0"
          style={{ backgroundColor: `${F.RED}20`, borderBottom: `1px solid ${F.RED}`, color: F.RED }}
        >
          <AlertCircle size={14} />
          {error}
          <button onClick={() => setError(null)} className="ml-auto bg-transparent border-none cursor-pointer" style={{ color: F.RED }}>
            <X size={14} />
          </button>
        </div>
      )}

      {/* Validation error details */}
      {validation && !validation.valid && validation.error && (
        <div
          className="px-5 py-2 font-mono text-[11px] shrink-0"
          style={{ backgroundColor: `${F.RED}10`, borderBottom: `1px solid ${F.BORDER}`, color: F.RED }}
        >
          {validation.error}
        </div>
      )}

      {/* Content */}
      <div className="flex flex-1 overflow-hidden">
        {/* Left Panel - Parameters */}
        <div
          className="overflow-auto shrink-0 transition-all duration-200"
          style={{
            width: advancedMode ? '320px' : '420px',
            borderRight: `1px solid ${F.BORDER}`,
            backgroundColor: F.PANEL_BG,
          }}
        >
          {/* Mode Toggle */}
          <div className="p-4" style={{ borderBottom: `1px solid ${F.BORDER}` }}>
            <div className="text-[11px] font-bold tracking-wide mb-2" style={{ color: F.WHITE }}>
              EDITING MODE
            </div>
            <div className="flex gap-1.5">
              <button
                onClick={() => setAdvancedMode(false)}
                className={`${S.btnPrimary} flex-1 py-2 text-[10px] border`}
                style={{
                  backgroundColor: !advancedMode ? F.ORANGE : 'transparent',
                  color: !advancedMode ? F.DARK_BG : F.GRAY,
                  borderColor: !advancedMode ? F.ORANGE : F.BORDER,
                }}
              >
                <Settings size={12} />
                PARAMETERS
              </button>
              <button
                onClick={() => setAdvancedMode(true)}
                className={`${S.btnPrimary} flex-1 py-2 text-[10px] border`}
                style={{
                  backgroundColor: advancedMode ? F.ORANGE : 'transparent',
                  color: advancedMode ? F.DARK_BG : F.GRAY,
                  borderColor: advancedMode ? F.ORANGE : F.BORDER,
                }}
              >
                <Code size={12} />
                ADVANCED
              </button>
            </div>
          </div>

          {/* Parameter Section */}
          {!advancedMode && (
            <div className="p-4">
              <button
                onClick={() => setShowParamSection(!showParamSection)}
                className="flex items-center gap-2 cursor-pointer bg-transparent border-none mb-3"
                style={{ color: F.WHITE }}
              >
                {showParamSection ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
                <span className="text-[11px] font-bold tracking-wide">STRATEGY PARAMETERS</span>
              </button>
              {showParamSection && (
                <ParameterEditor code={code} values={parameterValues} onChange={setParameterValues} />
              )}
            </div>
          )}

          {/* Strategy Info */}
          <div className="p-4" style={{ borderTop: `1px solid ${F.BORDER}` }}>
            <div className="text-[11px] font-bold tracking-wide mb-3" style={{ color: F.WHITE }}>
              STRATEGY INFO
            </div>
            <div className="text-[11px]" style={{ color: F.GRAY }}>
              <div className="mb-1.5">
                <span style={{ color: F.MUTED }}>ID:</span>{' '}
                <span style={{ color: F.CYAN }}>{strategy.id}</span>
              </div>
              <div className="mb-1.5">
                <span style={{ color: F.MUTED }}>Category:</span>{' '}
                <span style={{ color: F.WHITE }}>{strategy.category}</span>
              </div>
              {strategy.description && (
                <div className="mt-2 leading-relaxed" style={{ color: F.GRAY }}>
                  {strategy.description}
                </div>
              )}
            </div>
          </div>

          {/* Quick Actions */}
          <div className="p-4" style={{ borderTop: `1px solid ${F.BORDER}` }}>
            <div className="text-[11px] font-bold tracking-wide mb-3" style={{ color: F.WHITE }}>
              QUICK ACTIONS
            </div>
            <div className="flex flex-col gap-1.5">
              <button onClick={handleCopy} className={S.btnOutline}>
                <Copy size={12} />
                Copy Code
              </button>
              {advancedMode && (
                <button
                  onClick={handleReset}
                  disabled={!hasUnsavedChanges}
                  className={S.btnOutline}
                  style={{
                    color: hasUnsavedChanges ? F.YELLOW : F.MUTED,
                    opacity: hasUnsavedChanges ? 1 : 0.5,
                    cursor: hasUnsavedChanges ? 'pointer' : 'not-allowed',
                  }}
                >
                  <Undo size={12} />
                  Reset Changes
                </button>
              )}
            </div>
          </div>
        </div>

        {/* Right Panel - Code Editor */}
        <div className="flex flex-col flex-1 overflow-hidden" style={{ backgroundColor: F.DARK_BG }}>
          {/* Editor Toolbar */}
          <div
            className="flex items-center justify-between px-4 py-2.5 shrink-0"
            style={{ borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.HEADER_BG }}
          >
            <div className="flex items-center gap-2">
              <Edit3 size={14} style={{ color: F.CYAN }} />
              <span className="text-[11px] font-bold tracking-wide" style={{ color: F.WHITE }}>
                {advancedMode ? 'CODE EDITOR' : 'CODE PREVIEW'}
              </span>
              <span className="text-[10px]" style={{ color: F.MUTED }}>
                ({lines.length} lines)
              </span>
            </div>
            <div className="text-[10px]" style={{ color: F.GRAY }}>
              Ln {cursorPosition.line}, Col {cursorPosition.col}
            </div>
          </div>

          {/* Code Editor Area */}
          <div
            className="flex flex-1 overflow-auto"
            style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}
          >
            {/* Line Numbers */}
            <div
              className="py-3 px-2 shrink-0 text-right select-none"
              style={{
                backgroundColor: F.PANEL_BG,
                borderRight: `1px solid ${F.BORDER}`,
                minWidth: '48px',
              }}
            >
              {lines.map((_, i) => (
                <div
                  key={i}
                  className="text-[12px] leading-[22px]"
                  style={{
                    color: cursorPosition.line === i + 1 ? F.ORANGE : F.MUTED,
                    fontWeight: cursorPosition.line === i + 1 ? 700 : 400,
                  }}
                >
                  {i + 1}
                </div>
              ))}
            </div>

            {/* Code Area */}
            <div className="flex-1 relative">
              {advancedMode ? (
                <textarea
                  ref={textareaRef}
                  value={code}
                  onChange={(e) => handleCodeChange(e.target.value)}
                  onKeyDown={handleKeyDown}
                  onKeyUp={updateCursorPosition}
                  onClick={updateCursorPosition}
                  spellCheck={false}
                  className="w-full h-full py-3 px-4 bg-transparent border-none outline-none resize-none"
                  style={{
                    color: F.WHITE,
                    fontSize: '12px',
                    lineHeight: '22px',
                    fontFamily: 'inherit',
                    whiteSpace: 'pre',
                    overflowWrap: 'normal',
                    overflowX: 'auto',
                  }}
                />
              ) : (
                <pre
                  className="m-0 py-3 px-4 overflow-auto"
                  style={{ fontSize: '12px', lineHeight: '22px', color: F.WHITE, whiteSpace: 'pre' }}
                >
                  {lines.map((line, i) => (
                    <div key={i} style={{ minHeight: '22px' }}>
                      <SyntaxHighlightedLine line={line} />
                    </div>
                  ))}
                </pre>
              )}
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <div
        className="flex items-center justify-between px-5 py-1.5 shrink-0 text-[10px]"
        style={{ backgroundColor: F.HEADER_BG, borderTop: `1px solid ${F.BORDER}`, color: F.GRAY }}
      >
        <div className="flex items-center gap-4">
          <span>Python Strategy Editor</span>
          <span>Mode: {advancedMode ? 'Advanced (Code)' : 'Parameters Only'}</span>
        </div>
        <div className="flex items-center gap-4">
          <span>{lines.length} lines</span>
          <span>{code.length} chars</span>
          <span>QCAlgorithm Compatible</span>
        </div>
      </div>
    </div>
  );
};

// Simple syntax highlighting for Python
const SyntaxHighlightedLine: React.FC<{ line: string }> = ({ line }) => {
  const parts: React.ReactNode[] = [];
  const remaining = line;
  let key = 0;

  const commentIdx = remaining.indexOf('#');
  if (commentIdx !== -1) {
    if (commentIdx > 0) {
      parts.push(<span key={key++}>{highlightNonComment(remaining.substring(0, commentIdx))}</span>);
    }
    parts.push(<span key={key++} style={{ color: F.MUTED }}>{remaining.substring(commentIdx)}</span>);
    return <>{parts}</>;
  }
  return <>{highlightNonComment(remaining)}</>;
};

const highlightNonComment = (text: string): React.ReactNode => {
  const stringRegex = /(["'])(?:(?=(\\?))\2.)*?\1/g;
  const parts: React.ReactNode[] = [];
  let lastIndex = 0;
  let match;
  let key = 0;
  while ((match = stringRegex.exec(text)) !== null) {
    if (match.index > lastIndex) parts.push(highlightKeywords(text.substring(lastIndex, match.index), key++));
    parts.push(<span key={`str-${key++}`} style={{ color: F.GREEN }}>{match[0]}</span>);
    lastIndex = match.index + match[0].length;
  }
  if (lastIndex < text.length) parts.push(highlightKeywords(text.substring(lastIndex), key++));
  return <>{parts}</>;
};

const highlightKeywords = (text: string, baseKey: number): React.ReactNode => {
  const wordRegex = /\b(\w+)\b/g;
  const parts: React.ReactNode[] = [];
  let lastIndex = 0;
  let match;
  let key = 0;
  while ((match = wordRegex.exec(text)) !== null) {
    if (match.index > lastIndex) parts.push(text.substring(lastIndex, match.index));
    const word = match[1];
    if (PYTHON_KEYWORDS.includes(word)) {
      parts.push(<span key={`kw-${baseKey}-${key++}`} style={{ color: F.PURPLE }}>{word}</span>);
    } else if (/^\d+\.?\d*$/.test(word)) {
      parts.push(<span key={`num-${baseKey}-${key++}`} style={{ color: F.ORANGE }}>{word}</span>);
    } else if (word === word.toUpperCase() && word.length > 1 && /[A-Z]/.test(word)) {
      parts.push(<span key={`const-${baseKey}-${key++}`} style={{ color: F.CYAN }}>{word}</span>);
    } else {
      parts.push(word);
    }
    lastIndex = match.index + match[0].length;
  }
  if (lastIndex < text.length) parts.push(text.substring(lastIndex));
  return <>{parts}</>;
};

export default PythonStrategyEditor;
