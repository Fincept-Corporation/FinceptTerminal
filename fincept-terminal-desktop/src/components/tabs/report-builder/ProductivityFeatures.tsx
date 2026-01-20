import React, { useState, useEffect, useCallback, useRef } from 'react';
import {
  Undo2,
  Redo2,
  Search,
  Replace,
  Maximize,
  Minimize,
  Save,
  Keyboard,
  X,
} from 'lucide-react';
import { ReportTemplate, ReportComponent } from '@/services/core/reportService';
import { Dialog, DialogContent, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { toast } from 'sonner';

// ============ Undo/Redo History Hook ============
interface HistoryState<T> {
  past: T[];
  present: T;
  future: T[];
}

export const useUndoRedo = <T,>(initialState: T, maxHistory: number = 50) => {
  const [history, setHistory] = useState<HistoryState<T>>({
    past: [],
    present: initialState,
    future: [],
  });

  const canUndo = history.past.length > 0;
  const canRedo = history.future.length > 0;

  const set = useCallback((newState: T | ((prev: T) => T)) => {
    setHistory(prev => {
      const nextState = typeof newState === 'function'
        ? (newState as (prev: T) => T)(prev.present)
        : newState;

      // Don't add to history if state hasn't changed
      if (JSON.stringify(nextState) === JSON.stringify(prev.present)) {
        return prev;
      }

      return {
        past: [...prev.past, prev.present].slice(-maxHistory),
        present: nextState,
        future: [],
      };
    });
  }, [maxHistory]);

  const undo = useCallback(() => {
    setHistory(prev => {
      if (prev.past.length === 0) return prev;
      const previous = prev.past[prev.past.length - 1];
      const newPast = prev.past.slice(0, -1);
      return {
        past: newPast,
        present: previous,
        future: [prev.present, ...prev.future],
      };
    });
  }, []);

  const redo = useCallback(() => {
    setHistory(prev => {
      if (prev.future.length === 0) return prev;
      const next = prev.future[0];
      const newFuture = prev.future.slice(1);
      return {
        past: [...prev.past, prev.present],
        present: next,
        future: newFuture,
      };
    });
  }, []);

  const reset = useCallback((newState: T) => {
    setHistory({
      past: [],
      present: newState,
      future: [],
    });
  }, []);

  return {
    state: history.present,
    set,
    undo,
    redo,
    reset,
    canUndo,
    canRedo,
    historyLength: history.past.length,
  };
};

// ============ Auto-Save Hook ============
interface AutoSaveConfig {
  interval?: number; // milliseconds
  enabled?: boolean;
}

export const useAutoSave = <T,>(
  data: T,
  onSave: (data: T) => Promise<void>,
  config: AutoSaveConfig = {}
) => {
  const { interval = 30000, enabled = true } = config;
  const [lastSaved, setLastSaved] = useState<Date | null>(null);
  const [isSaving, setIsSaving] = useState(false);
  const [hasUnsavedChanges, setHasUnsavedChanges] = useState(false);
  const lastDataRef = useRef<string>(JSON.stringify(data));
  const timeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    const currentData = JSON.stringify(data);
    if (currentData !== lastDataRef.current) {
      setHasUnsavedChanges(true);
    }
  }, [data]);

  useEffect(() => {
    if (!enabled || !hasUnsavedChanges) return;

    // Clear existing timeout
    if (timeoutRef.current) {
      clearTimeout(timeoutRef.current);
    }

    // Set new timeout
    timeoutRef.current = setTimeout(async () => {
      try {
        setIsSaving(true);
        await onSave(data);
        lastDataRef.current = JSON.stringify(data);
        setLastSaved(new Date());
        setHasUnsavedChanges(false);
      } catch (error) {
        console.error('Auto-save failed:', error);
      } finally {
        setIsSaving(false);
      }
    }, interval);

    return () => {
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
      }
    };
  }, [data, enabled, hasUnsavedChanges, interval, onSave]);

  const saveNow = useCallback(async () => {
    try {
      setIsSaving(true);
      await onSave(data);
      lastDataRef.current = JSON.stringify(data);
      setLastSaved(new Date());
      setHasUnsavedChanges(false);
      toast.success('Saved');
    } catch (error) {
      toast.error('Save failed');
    } finally {
      setIsSaving(false);
    }
  }, [data, onSave]);

  return {
    lastSaved,
    isSaving,
    hasUnsavedChanges,
    saveNow,
    forceSave: saveNow,
  };
};

// ============ Keyboard Shortcuts Hook ============
interface ShortcutAction {
  key: string;
  ctrl?: boolean;
  shift?: boolean;
  alt?: boolean;
  action: () => void;
  description: string;
}

type ShortcutMap = Record<string, () => void>;

// Parse shortcut string like 'ctrl+z' into components
const parseShortcut = (shortcutStr: string): { key: string; ctrl: boolean; shift: boolean; alt: boolean } => {
  const parts = shortcutStr.toLowerCase().split('+');
  return {
    key: parts[parts.length - 1],
    ctrl: parts.includes('ctrl'),
    shift: parts.includes('shift'),
    alt: parts.includes('alt'),
  };
};

export const useKeyboardShortcuts = (shortcuts: ShortcutMap, enabled: boolean = true) => {
  useEffect(() => {
    if (!enabled) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      for (const [shortcutStr, action] of Object.entries(shortcuts)) {
        const { key, ctrl, shift, alt } = parseShortcut(shortcutStr);
        const keyMatch = e.key.toLowerCase() === key.toLowerCase();
        const ctrlMatch = ctrl ? (e.ctrlKey || e.metaKey) : !(e.ctrlKey || e.metaKey);
        const shiftMatch = shift ? e.shiftKey : !e.shiftKey;
        const altMatch = alt ? e.altKey : !e.altKey;

        if (keyMatch && ctrlMatch && shiftMatch && altMatch) {
          e.preventDefault();
          action();
          return;
        }
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [shortcuts, enabled]);
};

// ============ Find & Replace Dialog ============
interface FindReplaceDialogProps {
  open: boolean;
  onClose: () => void;
  components: ReportComponent[];
  onReplace: (componentId: string, oldText: string, newText: string) => void;
}

export const FindReplaceDialog: React.FC<FindReplaceDialogProps> = ({
  open,
  onClose,
  components,
  onReplace,
}) => {
  const [findText, setFindText] = useState('');
  const [replaceText, setReplaceText] = useState('');
  const [matches, setMatches] = useState<{ componentId: string; content: string; index: number }[]>([]);
  const [currentMatch, setCurrentMatch] = useState(0);
  const [caseSensitive, setCaseSensitive] = useState(false);

  useEffect(() => {
    if (!findText) {
      setMatches([]);
      return;
    }

    const found: typeof matches = [];
    const searchText = caseSensitive ? findText : findText.toLowerCase();

    components.forEach(comp => {
      if (typeof comp.content === 'string') {
        const content = caseSensitive ? comp.content : comp.content.toLowerCase();
        let index = content.indexOf(searchText);
        while (index !== -1) {
          found.push({ componentId: comp.id, content: comp.content, index });
          index = content.indexOf(searchText, index + 1);
        }
      }
    });

    setMatches(found);
    setCurrentMatch(0);
  }, [findText, components, caseSensitive]);

  const handleReplaceOne = () => {
    if (matches.length === 0) return;
    const match = matches[currentMatch];
    const comp = components.find(c => c.id === match.componentId);
    if (!comp || typeof comp.content !== 'string') return;

    const flags = caseSensitive ? '' : 'i';
    const regex = new RegExp(findText.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), flags);
    const newContent = comp.content.replace(regex, replaceText);
    onReplace(match.componentId, comp.content, newContent);
    toast.success('Replaced 1 occurrence');
  };

  const handleReplaceAll = () => {
    if (matches.length === 0) return;

    const componentIds = [...new Set(matches.map(m => m.componentId))];
    componentIds.forEach(id => {
      const comp = components.find(c => c.id === id);
      if (!comp || typeof comp.content !== 'string') return;

      const flags = caseSensitive ? 'g' : 'gi';
      const regex = new RegExp(findText.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), flags);
      const newContent = comp.content.replace(regex, replaceText);
      onReplace(id, comp.content, newContent);
    });

    toast.success(`Replaced ${matches.length} occurrences`);
  };

  return (
    <Dialog open={open} onOpenChange={onClose}>
      <DialogContent className="bg-[#1a1a1a] border-[#333333] max-w-md">
        <DialogHeader>
          <DialogTitle className="text-[#FFA500] flex items-center gap-2">
            <Search size={18} />
            Find & Replace
          </DialogTitle>
        </DialogHeader>

        <div className="space-y-4">
          {/* Find */}
          <div>
            <label className="text-xs text-gray-400 block mb-1">Find</label>
            <input
              type="text"
              value={findText}
              onChange={(e) => setFindText(e.target.value)}
              placeholder="Search text..."
              className="w-full px-3 py-2 text-sm bg-[#0a0a0a] border border-[#333333] rounded text-white focus:border-[#FFA500] outline-none"
              autoFocus
            />
          </div>

          {/* Replace */}
          <div>
            <label className="text-xs text-gray-400 block mb-1">Replace with</label>
            <input
              type="text"
              value={replaceText}
              onChange={(e) => setReplaceText(e.target.value)}
              placeholder="Replacement text..."
              className="w-full px-3 py-2 text-sm bg-[#0a0a0a] border border-[#333333] rounded text-white focus:border-[#FFA500] outline-none"
            />
          </div>

          {/* Options */}
          <label className="flex items-center gap-2 text-xs text-gray-400">
            <input
              type="checkbox"
              checked={caseSensitive}
              onChange={(e) => setCaseSensitive(e.target.checked)}
              className="rounded"
            />
            Case sensitive
          </label>

          {/* Results */}
          <div className="text-xs text-gray-500">
            {matches.length > 0 ? (
              <span>{matches.length} match(es) found</span>
            ) : findText ? (
              <span>No matches found</span>
            ) : null}
          </div>

          {/* Actions */}
          <div className="flex gap-2">
            <button
              onClick={handleReplaceOne}
              disabled={matches.length === 0}
              className="flex-1 py-2 text-xs bg-[#333333] text-white rounded hover:bg-[#444444] disabled:opacity-50"
            >
              Replace
            </button>
            <button
              onClick={handleReplaceAll}
              disabled={matches.length === 0}
              className="flex-1 py-2 text-xs bg-[#FFA500] text-black rounded hover:bg-[#ff8c00] disabled:opacity-50"
            >
              Replace All
            </button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  );
};

// ============ Keyboard Shortcuts Dialog ============
interface ShortcutsDialogProps {
  open: boolean;
  onClose: () => void;
}

const SHORTCUTS_LIST = [
  { keys: 'Ctrl + Z', action: 'Undo' },
  { keys: 'Ctrl + Shift + Z', action: 'Redo' },
  { keys: 'Ctrl + S', action: 'Save' },
  { keys: 'Ctrl + F', action: 'Find & Replace' },
  { keys: 'Ctrl + E', action: 'Export' },
  { keys: 'Ctrl + T', action: 'Template Gallery' },
  { keys: 'Ctrl + P', action: 'Print Preview' },
  { keys: 'F11', action: 'Toggle Fullscreen' },
  { keys: 'Escape', action: 'Deselect / Close' },
  { keys: 'Delete', action: 'Delete Selected' },
  { keys: 'Ctrl + D', action: 'Duplicate Selected' },
  { keys: 'Ctrl + 1', action: 'Add Heading' },
  { keys: 'Ctrl + 2', action: 'Add Text' },
  { keys: 'Ctrl + 3', action: 'Add Chart' },
  { keys: 'Ctrl + 4', action: 'Add Table' },
];

export const ShortcutsDialog: React.FC<ShortcutsDialogProps> = ({ open, onClose }) => (
  <Dialog open={open} onOpenChange={onClose}>
    <DialogContent className="bg-[#1a1a1a] border-[#333333] max-w-md">
      <DialogHeader>
        <DialogTitle className="text-[#FFA500] flex items-center gap-2">
          <Keyboard size={18} />
          Keyboard Shortcuts
        </DialogTitle>
      </DialogHeader>

      <div className="space-y-1 max-h-[400px] overflow-y-auto">
        {SHORTCUTS_LIST.map((shortcut, idx) => (
          <div
            key={idx}
            className="flex items-center justify-between py-2 px-2 hover:bg-[#2a2a2a] rounded"
          >
            <span className="text-xs text-gray-300">{shortcut.action}</span>
            <kbd className="px-2 py-1 text-[10px] bg-[#0a0a0a] border border-[#333333] rounded text-gray-400">
              {shortcut.keys}
            </kbd>
          </div>
        ))}
      </div>
    </DialogContent>
  </Dialog>
);

// ============ Fullscreen Mode Hook ============
export const useFullscreen = (externalRef?: React.RefObject<HTMLElement>) => {
  const internalRef = useRef<HTMLElement>(null);
  const elementRef = externalRef || internalRef;
  const [isFullscreen, setIsFullscreen] = useState(false);

  useEffect(() => {
    const handleChange = () => {
      setIsFullscreen(!!document.fullscreenElement);
    };

    document.addEventListener('fullscreenchange', handleChange);
    return () => document.removeEventListener('fullscreenchange', handleChange);
  }, []);

  const enterFullscreen = useCallback(() => {
    const element = elementRef.current || document.documentElement;
    element.requestFullscreen?.();
  }, [elementRef]);

  const exitFullscreen = useCallback(() => {
    document.exitFullscreen?.();
  }, []);

  const toggleFullscreen = useCallback(() => {
    if (isFullscreen) {
      exitFullscreen();
    } else {
      enterFullscreen();
    }
  }, [isFullscreen, enterFullscreen, exitFullscreen]);

  return { isFullscreen, enterFullscreen, exitFullscreen, toggleFullscreen, fullscreenRef: elementRef };
};

// ============ Toolbar Component ============
interface ProductivityToolbarProps {
  canUndo: boolean;
  canRedo: boolean;
  onUndo: () => void;
  onRedo: () => void;
  onSave: () => void;
  onFindReplace: () => void;
  onShortcuts: () => void;
  onFullscreen: () => void;
  isFullscreen: boolean;
  isSaving: boolean;
  hasUnsavedChanges: boolean;
  lastSaved: Date | null;
}

export const ProductivityToolbar: React.FC<ProductivityToolbarProps> = ({
  canUndo,
  canRedo,
  onUndo,
  onRedo,
  onSave,
  onFindReplace,
  onShortcuts,
  onFullscreen,
  isFullscreen,
  isSaving,
  hasUnsavedChanges,
  lastSaved,
}) => (
  <div className="flex items-center gap-1 px-2 py-1 bg-[#1a1a1a] border-b border-[#333333]">
    {/* Undo/Redo */}
    <button
      onClick={onUndo}
      disabled={!canUndo}
      className="p-1.5 rounded hover:bg-[#2a2a2a] disabled:opacity-30 disabled:hover:bg-transparent"
      title="Undo (Ctrl+Z)"
    >
      <Undo2 size={14} className="text-gray-400" />
    </button>
    <button
      onClick={onRedo}
      disabled={!canRedo}
      className="p-1.5 rounded hover:bg-[#2a2a2a] disabled:opacity-30 disabled:hover:bg-transparent"
      title="Redo (Ctrl+Shift+Z)"
    >
      <Redo2 size={14} className="text-gray-400" />
    </button>

    <div className="w-px h-4 bg-[#333333] mx-1" />

    {/* Save */}
    <button
      onClick={onSave}
      className="p-1.5 rounded hover:bg-[#2a2a2a] relative"
      title="Save (Ctrl+S)"
    >
      <Save size={14} className={hasUnsavedChanges ? 'text-[#FFA500]' : 'text-gray-400'} />
      {isSaving && (
        <span className="absolute -top-0.5 -right-0.5 w-2 h-2 bg-[#FFA500] rounded-full animate-pulse" />
      )}
    </button>

    {/* Find/Replace */}
    <button
      onClick={onFindReplace}
      className="p-1.5 rounded hover:bg-[#2a2a2a]"
      title="Find & Replace (Ctrl+F)"
    >
      <Search size={14} className="text-gray-400" />
    </button>

    <div className="w-px h-4 bg-[#333333] mx-1" />

    {/* Fullscreen */}
    <button
      onClick={onFullscreen}
      className="p-1.5 rounded hover:bg-[#2a2a2a]"
      title="Toggle Fullscreen (F11)"
    >
      {isFullscreen ? (
        <Minimize size={14} className="text-gray-400" />
      ) : (
        <Maximize size={14} className="text-gray-400" />
      )}
    </button>

    {/* Shortcuts */}
    <button
      onClick={onShortcuts}
      className="p-1.5 rounded hover:bg-[#2a2a2a]"
      title="Keyboard Shortcuts"
    >
      <Keyboard size={14} className="text-gray-400" />
    </button>

    {/* Auto-save indicator */}
    {lastSaved && (
      <span className="ml-auto text-[10px] text-gray-500">
        Saved {lastSaved.toLocaleTimeString()}
      </span>
    )}
  </div>
);

export default {
  useUndoRedo,
  useAutoSave,
  useKeyboardShortcuts,
  useFullscreen,
  FindReplaceDialog,
  ShortcutsDialog,
  ProductivityToolbar,
};
