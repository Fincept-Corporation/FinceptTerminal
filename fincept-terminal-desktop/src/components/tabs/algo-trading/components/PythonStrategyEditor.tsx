import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  Save, X, Code, Settings, RefreshCw, AlertCircle, CheckCircle,
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

// Python syntax keywords for basic highlighting
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
  // State
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

  // Load strategy code
  useEffect(() => {
    loadStrategyCode();
  }, [strategy.id]);

  const loadStrategyCode = async () => {
    setLoading(true);
    setError(null);

    try {
      // Load code
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

      // Load parameters
      if (isCustom && (strategy as CustomPythonStrategy).parameters) {
        try {
          const params = JSON.parse((strategy as CustomPythonStrategy).parameters || '{}');
          setParameterValues(params);
        } catch {
          setParameterValues({});
        }
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  // Extract parameters when code changes
  useEffect(() => {
    if (code && advancedMode) {
      extractParams();
    }
  }, [code, advancedMode]);

  const extractParams = async () => {
    try {
      const result = await extractStrategyParameters(code);
      if (result.success && result.data) {
        setParameters(result.data);
        // Initialize any missing parameter values
        const newValues = { ...parameterValues };
        result.data.forEach((param) => {
          if (!(param.name in newValues)) {
            newValues[param.name] = param.default_value;
          }
        });
        setParameterValues(newValues);
      }
    } catch (err) {
      console.error('Failed to extract parameters:', err);
    }
  };

  // Track unsaved changes
  useEffect(() => {
    const codeChanged = code !== originalCode;
    setHasUnsavedChanges(codeChanged);
    // Reset validation when code changes
    if (codeChanged) {
      setValidation(null);
    }
  }, [code, originalCode]);

  // Validate Python syntax
  const handleValidate = async () => {
    setValidating(true);
    try {
      const result = await validatePythonSyntax(code);
      if (result.success && result.data) {
        setValidation(result.data);
      } else {
        setValidation({ valid: false, error: result.error || 'Validation failed' });
      }
    } catch (err) {
      setValidation({ valid: false, error: err instanceof Error ? err.message : 'Unknown error' });
    } finally {
      setValidating(false);
    }
  };

  // Save strategy
  const handleSave = async () => {
    setSaving(true);
    setError(null);

    try {
      if (isCustom) {
        // Update existing custom strategy
        const result = await updateCustomPythonStrategy({
          id: strategy.id,
          name,
          code: code,  // Always pass current code
          parameters: JSON.stringify(parameterValues),
        });
        if (result.success) {
          setOriginalCode(code);
          setHasUnsavedChanges(false);
          onSave?.(strategy.id);
        } else {
          setError(result.error || 'Failed to update strategy');
        }
      } else {
        // Create new custom strategy from library
        const result = await saveCustomPythonStrategy({
          baseStrategyId: strategy.id,
          name,
          code: code,  // Always pass current code
          parameters: JSON.stringify(parameterValues),
        });
        if (result.success && result.data) {
          setOriginalCode(code);
          setHasUnsavedChanges(false);
          onSave?.(result.data.id);
        } else {
          setError(result.error || 'Failed to save strategy');
        }
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setSaving(false);
    }
  };

  // Handle code change
  const handleCodeChange = (newCode: string) => {
    setCode(newCode);
  };

  // Reset to original
  const handleReset = () => {
    setCode(originalCode);
    setValidation(null);
  };

  // Copy code
  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(code);
    } catch (err) {
      console.error('Failed to copy:', err);
    }
  };

  // Update cursor position
  const updateCursorPosition = useCallback(() => {
    if (!textareaRef.current) return;
    const pos = textareaRef.current.selectionStart;
    const text = code.substring(0, pos);
    const line = text.split('\n').length;
    const col = pos - text.lastIndexOf('\n');
    setCursorPosition({ line, col });
  }, [code]);

  // Handle tab key in textarea
  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Tab') {
      e.preventDefault();
      const textarea = textareaRef.current;
      if (!textarea) return;
      const start = textarea.selectionStart;
      const end = textarea.selectionEnd;
      const newContent = code.substring(0, start) + '    ' + code.substring(end);
      handleCodeChange(newContent);
      setTimeout(() => {
        textarea.selectionStart = textarea.selectionEnd = start + 4;
      }, 0);
    }
  };

  // Calculate line numbers
  const lines = code.split('\n');

  if (loading) {
    return (
      <div style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.85)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 1000,
      }}>
        <div style={{ color: F.GRAY, fontSize: '12px' }}>Loading strategy...</div>
      </div>
    );
  }

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.9)',
      zIndex: 1000,
      display: 'flex',
      flexDirection: 'column',
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        padding: '12px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <FileCode size={18} style={{ color: F.ORANGE }} />
          <div>
            <input
              type="text"
              value={name}
              onChange={(e) => setName(e.target.value)}
              style={{
                backgroundColor: 'transparent',
                border: 'none',
                borderBottom: `1px solid ${F.BORDER}`,
                color: F.WHITE,
                fontSize: '14px',
                fontWeight: 700,
                padding: '4px 0',
                width: '300px',
              }}
              placeholder="Strategy Name"
            />
            <div style={{ fontSize: '10px', color: F.GRAY, marginTop: '2px' }}>
              {isCustom ? 'Editing custom strategy' : `Cloning from ${strategy.id}`}
              {strategy.category && ` • ${strategy.category}`}
            </div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {/* Validation status */}
          {validation && (
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 8px',
              backgroundColor: validation.valid ? `${F.GREEN}20` : `${F.RED}20`,
              border: `1px solid ${validation.valid ? F.GREEN : F.RED}`,
              borderRadius: '2px',
              fontSize: '9px',
              color: validation.valid ? F.GREEN : F.RED,
            }}>
              {validation.valid ? <CheckCircle size={10} /> : <AlertCircle size={10} />}
              {validation.valid ? 'Valid' : 'Syntax Error'}
            </div>
          )}

          {/* Unsaved indicator */}
          {hasUnsavedChanges && (
            <span style={{ fontSize: '9px', color: F.YELLOW }}>● Unsaved</span>
          )}

          {/* Validate button */}
          <button
            onClick={handleValidate}
            disabled={validating}
            style={{
              padding: '6px 12px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              color: F.CYAN,
              fontSize: '9px',
              fontWeight: 700,
              cursor: validating ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: validating ? 0.5 : 1,
            }}
          >
            <CheckCircle size={10} />
            {validating ? 'CHECKING...' : 'VALIDATE'}
          </button>

          {/* Backtest button */}
          {onBacktest && (
            <button
              onClick={() => onBacktest(strategy)}
              style={{
                padding: '6px 12px',
                backgroundColor: `${F.BLUE}20`,
                border: `1px solid ${F.BLUE}`,
                borderRadius: '2px',
                color: F.BLUE,
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <Play size={10} />
              BACKTEST
            </button>
          )}

          {/* Save button */}
          <button
            onClick={handleSave}
            disabled={saving || (!hasUnsavedChanges && isCustom)}
            style={{
              padding: '6px 12px',
              backgroundColor: F.ORANGE,
              border: 'none',
              borderRadius: '2px',
              color: F.DARK_BG,
              fontSize: '9px',
              fontWeight: 700,
              cursor: saving ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: saving || (!hasUnsavedChanges && isCustom) ? 0.5 : 1,
            }}
          >
            <Save size={10} />
            {saving ? 'SAVING...' : isCustom ? 'UPDATE' : 'SAVE AS CUSTOM'}
          </button>

          {/* Close button */}
          <button
            onClick={onClose}
            style={{
              padding: '6px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              color: F.GRAY,
              cursor: 'pointer',
            }}
          >
            <X size={14} />
          </button>
        </div>
      </div>

      {/* Error display */}
      {error && (
        <div style={{
          padding: '8px 16px',
          backgroundColor: `${F.RED}20`,
          borderBottom: `1px solid ${F.RED}`,
          color: F.RED,
          fontSize: '11px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
        }}>
          <AlertCircle size={12} />
          {error}
          <button
            onClick={() => setError(null)}
            style={{
              marginLeft: 'auto',
              background: 'none',
              border: 'none',
              color: F.RED,
              cursor: 'pointer',
            }}
          >
            <X size={12} />
          </button>
        </div>
      )}

      {/* Validation error details */}
      {validation && !validation.valid && validation.error && (
        <div style={{
          padding: '8px 16px',
          backgroundColor: `${F.RED}10`,
          borderBottom: `1px solid ${F.BORDER}`,
          color: F.RED,
          fontSize: '10px',
          fontFamily: 'monospace',
        }}>
          {validation.error}
        </div>
      )}

      {/* Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Parameters */}
        <div style={{
          width: advancedMode ? '300px' : '400px',
          borderRight: `1px solid ${F.BORDER}`,
          backgroundColor: F.PANEL_BG,
          overflow: 'auto',
          transition: 'width 0.2s',
        }}>
          {/* Mode Toggle */}
          <div style={{
            padding: '12px',
            borderBottom: `1px solid ${F.BORDER}`,
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '8px',
            }}>
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>
                EDITING MODE
              </span>
            </div>
            <div style={{ display: 'flex', gap: '4px' }}>
              <button
                onClick={() => setAdvancedMode(false)}
                style={{
                  flex: 1,
                  padding: '8px',
                  backgroundColor: !advancedMode ? F.ORANGE : 'transparent',
                  color: !advancedMode ? F.DARK_BG : F.GRAY,
                  border: `1px solid ${!advancedMode ? F.ORANGE : F.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                }}
              >
                <Settings size={10} />
                PARAMETERS
              </button>
              <button
                onClick={() => setAdvancedMode(true)}
                style={{
                  flex: 1,
                  padding: '8px',
                  backgroundColor: advancedMode ? F.ORANGE : 'transparent',
                  color: advancedMode ? F.DARK_BG : F.GRAY,
                  border: `1px solid ${advancedMode ? F.ORANGE : F.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                }}
              >
                <Code size={10} />
                ADVANCED
              </button>
            </div>
          </div>

          {/* Parameter Section */}
          {!advancedMode && (
            <div style={{ padding: '12px' }}>
              <div
                onClick={() => setShowParamSection(!showParamSection)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  cursor: 'pointer',
                  marginBottom: '8px',
                }}
              >
                {showParamSection ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>
                  STRATEGY PARAMETERS
                </span>
              </div>

              {showParamSection && (
                <ParameterEditor
                  code={code}
                  values={parameterValues}
                  onChange={setParameterValues}
                />
              )}
            </div>
          )}

          {/* Strategy Info */}
          <div style={{
            padding: '12px',
            borderTop: `1px solid ${F.BORDER}`,
          }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, marginBottom: '8px' }}>
              STRATEGY INFO
            </div>
            <div style={{ fontSize: '9px', color: F.GRAY }}>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: F.MUTED }}>ID:</span>{' '}
                <span style={{ color: F.CYAN }}>{strategy.id}</span>
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: F.MUTED }}>Category:</span>{' '}
                <span style={{ color: F.WHITE }}>{strategy.category}</span>
              </div>
              {strategy.description && (
                <div style={{ marginTop: '8px', lineHeight: 1.4, color: F.GRAY }}>
                  {strategy.description}
                </div>
              )}
            </div>
          </div>

          {/* Quick Actions */}
          <div style={{
            padding: '12px',
            borderTop: `1px solid ${F.BORDER}`,
          }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, marginBottom: '8px' }}>
              QUICK ACTIONS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              <button
                onClick={handleCopy}
                style={{
                  padding: '6px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px',
                  color: F.GRAY,
                  fontSize: '9px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                }}
              >
                <Copy size={10} />
                Copy Code
              </button>
              {advancedMode && (
                <button
                  onClick={handleReset}
                  disabled={!hasUnsavedChanges}
                  style={{
                    padding: '6px 8px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px',
                    color: hasUnsavedChanges ? F.YELLOW : F.MUTED,
                    fontSize: '9px',
                    cursor: hasUnsavedChanges ? 'pointer' : 'not-allowed',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    opacity: hasUnsavedChanges ? 1 : 0.5,
                  }}
                >
                  <Undo size={10} />
                  Reset Changes
                </button>
              )}
            </div>
          </div>
        </div>

        {/* Right Panel - Code Editor */}
        <div style={{
          flex: 1,
          display: 'flex',
          flexDirection: 'column',
          backgroundColor: F.DARK_BG,
          overflow: 'hidden',
        }}>
          {/* Editor Toolbar */}
          <div style={{
            padding: '8px 12px',
            borderBottom: `1px solid ${F.BORDER}`,
            backgroundColor: F.HEADER_BG,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Edit3 size={12} style={{ color: F.CYAN }} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>
                {advancedMode ? 'CODE EDITOR' : 'CODE PREVIEW'}
              </span>
              <span style={{ fontSize: '9px', color: F.MUTED }}>
                ({lines.length} lines)
              </span>
            </div>
            <div style={{ fontSize: '9px', color: F.GRAY }}>
              Ln {cursorPosition.line}, Col {cursorPosition.col}
            </div>
          </div>

          {/* Code Editor Area */}
          <div style={{
            flex: 1,
            display: 'flex',
            overflow: 'auto',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
          }}>
            {/* Line Numbers */}
            <div style={{
              padding: '12px 8px',
              backgroundColor: F.PANEL_BG,
              borderRight: `1px solid ${F.BORDER}`,
              textAlign: 'right',
              userSelect: 'none',
              minWidth: '40px',
            }}>
              {lines.map((_, i) => (
                <div
                  key={i}
                  style={{
                    fontSize: '11px',
                    lineHeight: '20px',
                    color: cursorPosition.line === i + 1 ? F.ORANGE : F.MUTED,
                    fontWeight: cursorPosition.line === i + 1 ? 700 : 400,
                  }}
                >
                  {i + 1}
                </div>
              ))}
            </div>

            {/* Code Area */}
            <div style={{ flex: 1, position: 'relative' }}>
              {advancedMode ? (
                <textarea
                  ref={textareaRef}
                  value={code}
                  onChange={(e) => handleCodeChange(e.target.value)}
                  onKeyDown={handleKeyDown}
                  onKeyUp={updateCursorPosition}
                  onClick={updateCursorPosition}
                  spellCheck={false}
                  style={{
                    width: '100%',
                    height: '100%',
                    padding: '12px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    color: F.WHITE,
                    fontSize: '11px',
                    lineHeight: '20px',
                    fontFamily: 'inherit',
                    resize: 'none',
                    outline: 'none',
                    whiteSpace: 'pre',
                    overflowWrap: 'normal',
                    overflowX: 'auto',
                  }}
                />
              ) : (
                <pre style={{
                  margin: 0,
                  padding: '12px',
                  fontSize: '11px',
                  lineHeight: '20px',
                  color: F.WHITE,
                  whiteSpace: 'pre',
                  overflow: 'auto',
                }}>
                  {lines.map((line, i) => (
                    <div key={i} style={{ minHeight: '20px' }}>
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
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderTop: `1px solid ${F.BORDER}`,
        padding: '4px 16px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        fontSize: '9px',
        color: F.GRAY,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>Python Strategy Editor</span>
          <span>Mode: {advancedMode ? 'Advanced (Code)' : 'Parameters Only'}</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
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
  // Very basic highlighting - just colorize keywords, strings, comments
  const parts: React.ReactNode[] = [];
  let remaining = line;
  let key = 0;

  // Check for comment
  const commentIdx = remaining.indexOf('#');
  if (commentIdx !== -1) {
    if (commentIdx > 0) {
      parts.push(
        <span key={key++}>{highlightNonComment(remaining.substring(0, commentIdx))}</span>
      );
    }
    parts.push(
      <span key={key++} style={{ color: F.MUTED }}>{remaining.substring(commentIdx)}</span>
    );
    return <>{parts}</>;
  }

  return <>{highlightNonComment(remaining)}</>;
};

const highlightNonComment = (text: string): React.ReactNode => {
  // Handle strings (basic)
  const stringRegex = /(["'])(?:(?=(\\?))\2.)*?\1/g;
  const parts: React.ReactNode[] = [];
  let lastIndex = 0;
  let match;
  let key = 0;

  while ((match = stringRegex.exec(text)) !== null) {
    if (match.index > lastIndex) {
      parts.push(highlightKeywords(text.substring(lastIndex, match.index), key++));
    }
    parts.push(
      <span key={`str-${key++}`} style={{ color: F.GREEN }}>{match[0]}</span>
    );
    lastIndex = match.index + match[0].length;
  }

  if (lastIndex < text.length) {
    parts.push(highlightKeywords(text.substring(lastIndex), key++));
  }

  return <>{parts}</>;
};

const highlightKeywords = (text: string, baseKey: number): React.ReactNode => {
  const wordRegex = /\b(\w+)\b/g;
  const parts: React.ReactNode[] = [];
  let lastIndex = 0;
  let match;
  let key = 0;

  while ((match = wordRegex.exec(text)) !== null) {
    if (match.index > lastIndex) {
      parts.push(text.substring(lastIndex, match.index));
    }

    const word = match[1];
    if (PYTHON_KEYWORDS.includes(word)) {
      parts.push(
        <span key={`kw-${baseKey}-${key++}`} style={{ color: F.PURPLE }}>{word}</span>
      );
    } else if (/^\d+\.?\d*$/.test(word)) {
      // Numbers
      parts.push(
        <span key={`num-${baseKey}-${key++}`} style={{ color: F.ORANGE }}>{word}</span>
      );
    } else if (word === word.toUpperCase() && word.length > 1 && /[A-Z]/.test(word)) {
      // CONSTANTS
      parts.push(
        <span key={`const-${baseKey}-${key++}`} style={{ color: F.CYAN }}>{word}</span>
      );
    } else {
      parts.push(word);
    }
    lastIndex = match.index + match[0].length;
  }

  if (lastIndex < text.length) {
    parts.push(text.substring(lastIndex));
  }

  return <>{parts}</>;
};

export default PythonStrategyEditor;
