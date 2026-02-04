import React, { createContext, useContext, useReducer, useEffect, useRef, useCallback, ReactNode } from 'react';
import { saveSetting, getSetting, getAllSettings } from '@/services/core/sqliteService';
import { save as saveDialog, open as openDialog } from '@tauri-apps/plugin-dialog';
import { writeTextFile, readTextFile } from '@tauri-apps/plugin-fs';
import { sanitizeAllTabStates } from '@/utils/workspaceSanitizer';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';

const WORKSPACES_KEY = 'workspaces_list';
const CURRENT_WS_KEY = 'current_workspace_id';
const CATEGORY = 'workspace';
const DB_TIMEOUT_MS = 10000;
const FILE_TIMEOUT_MS = 15000;
const MAX_WORKSPACE_NAME_LENGTH = 100;

// Settings categories to include in workspace (exclude credentials/keys)
const INCLUDED_SETTING_CATEGORIES = ['theme', 'timezone', 'i18n', 'market_preferences', 'reports'];

export interface Workspace {
  id: string;
  name: string;
  activeTab: string;
  settings: Record<string, string>; // key -> value for general settings
  tabStates: Record<string, unknown>; // per-tab configuration state
  createdAt: string;
  updatedAt: string;
}

export interface WorkspaceExport {
  version: 1 | 2;
  exportedAt: string;
  workspace: Workspace;
}

type TabStateGetter = () => Record<string, unknown>;
type TabStateSetter = (state: Record<string, unknown>) => void;
interface TabStateRegistration {
  getter: TabStateGetter;
  setter: TabStateSetter;
}

// ============================================================================
// State Machine (Point 1, 2, 9)
// ============================================================================

type WorkspaceStatus = 'idle' | 'loading' | 'ready' | 'error';

interface WorkspaceState {
  status: WorkspaceStatus;
  workspaces: Workspace[];
  currentWorkspace: Workspace | null;
  error: string | null;
}

type WorkspaceAction =
  | { type: 'INIT_START' }
  | { type: 'INIT_SUCCESS'; workspaces: Workspace[]; currentWorkspace: Workspace | null }
  | { type: 'INIT_ERROR'; error: string }
  | { type: 'SET_WORKSPACES'; workspaces: Workspace[]; currentWorkspace?: Workspace | null }
  | { type: 'SET_CURRENT'; workspace: Workspace | null }
  | { type: 'CLEAR_ERROR' };

const initialState: WorkspaceState = {
  status: 'idle',
  workspaces: [],
  currentWorkspace: null,
  error: null,
};

function workspaceReducer(state: WorkspaceState, action: WorkspaceAction): WorkspaceState {
  switch (action.type) {
    case 'INIT_START':
      return { ...state, status: 'loading', error: null };
    case 'INIT_SUCCESS':
      return { status: 'ready', workspaces: action.workspaces, currentWorkspace: action.currentWorkspace, error: null };
    case 'INIT_ERROR':
      return { ...state, status: 'error', error: action.error };
    case 'SET_WORKSPACES':
      return {
        ...state,
        workspaces: action.workspaces,
        currentWorkspace: action.currentWorkspace !== undefined ? action.currentWorkspace : state.currentWorkspace,
      };
    case 'SET_CURRENT':
      return { ...state, currentWorkspace: action.workspace };
    case 'CLEAR_ERROR':
      return { ...state, error: null };
    default:
      return state;
  }
}

// ============================================================================
// Context
// ============================================================================

interface WorkspaceContextType {
  workspaces: Workspace[];
  currentWorkspace: Workspace | null;
  status: WorkspaceStatus;
  error: string | null;
  saveWorkspace: (name: string, activeTab: string) => Promise<void>;
  loadWorkspace: (id: string) => Promise<{ activeTab: string; settings: Record<string, string> } | null>;
  deleteWorkspace: (id: string) => Promise<void>;
  exportWorkspace: (id: string) => Promise<void>;
  importWorkspace: () => Promise<Workspace | null>;
  refreshWorkspaces: () => Promise<void>;
  registerTabState: (tabId: string, getter: TabStateGetter, setter: TabStateSetter) => void;
  unregisterTabState: (tabId: string) => void;
}

const WorkspaceContext = createContext<WorkspaceContextType | undefined>(undefined);

export const useWorkspace = () => {
  const context = useContext(WorkspaceContext);
  if (!context) {
    throw new Error('useWorkspace must be used within WorkspaceProvider');
  }
  return context;
};

// ============================================================================
// Helpers (Point 8: withTimeout on all external calls)
// ============================================================================

function generateId(): string {
  return Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
}

async function loadWorkspacesFromDB(): Promise<Workspace[]> {
  try {
    const raw = await withTimeout(getSetting(WORKSPACES_KEY), DB_TIMEOUT_MS, 'Load workspaces timeout');
    if (!raw) return [];
    const parsed = JSON.parse(raw) as Workspace[];
    return parsed.map(w => ({ ...w, tabStates: w.tabStates || {} }));
  } catch {
    return [];
  }
}

async function persistWorkspaces(workspaces: Workspace[]): Promise<void> {
  await withTimeout(
    saveSetting(WORKSPACES_KEY, JSON.stringify(workspaces), CATEGORY),
    DB_TIMEOUT_MS,
    'Persist workspaces timeout',
  );
}

async function captureGeneralSettings(): Promise<Record<string, string>> {
  try {
    const allSettings = await withTimeout(getAllSettings(), DB_TIMEOUT_MS, 'Get settings timeout');
    const captured: Record<string, string> = {};
    for (const s of allSettings) {
      if (s.category && INCLUDED_SETTING_CATEGORIES.includes(s.category)) {
        captured[s.setting_key] = s.setting_value;
      }
    }
    return captured;
  } catch {
    return {};
  }
}

async function applySettings(settings: Record<string, string>): Promise<void> {
  for (const [key, value] of Object.entries(settings)) {
    try {
      await withTimeout(saveSetting(key, value), DB_TIMEOUT_MS, `Apply setting ${key} timeout`);
    } catch (err) {
      console.warn(`[Workspace] Failed to apply setting ${key}:`, err);
    }
  }
}

// Point 10: Input validation
function validateWorkspaceName(name: string): string | null {
  if (!name || name.trim().length === 0) return 'Name is required';
  if (name.length > MAX_WORKSPACE_NAME_LENGTH) return `Name must be under ${MAX_WORKSPACE_NAME_LENGTH} characters`;
  return null;
}

function validateImportedWorkspace(parsed: unknown): parsed is WorkspaceExport {
  if (!parsed || typeof parsed !== 'object') return false;
  const obj = parsed as Record<string, unknown>;
  if (!obj.workspace || typeof obj.workspace !== 'object') return false;
  const ws = obj.workspace as Record<string, unknown>;
  if (typeof ws.name !== 'string' || ws.name.trim().length === 0) return false;
  if (typeof ws.activeTab !== 'string' || ws.activeTab.trim().length === 0) return false;
  if (ws.settings && typeof ws.settings !== 'object') return false;
  return true;
}

// ============================================================================
// Provider
// ============================================================================

export const WorkspaceProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
  const [state, dispatch] = useReducer(workspaceReducer, initialState);

  // Tab state registry (no re-renders on registration)
  const tabStateRegistry = useRef<Map<string, TabStateRegistration>>(new Map());
  const restoredTabStates = useRef<Record<string, unknown>>({});
  const mountedRef = useRef(true);

  // Point 3: Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  const registerTabState = useCallback((tabId: string, getter: TabStateGetter, setter: TabStateSetter) => {
    tabStateRegistry.current.set(tabId, { getter, setter });
    const pending = restoredTabStates.current[tabId];
    if (pending && typeof pending === 'object' && !Array.isArray(pending)) {
      setter(pending as Record<string, unknown>);
      delete restoredTabStates.current[tabId];
    }
  }, []);

  const unregisterTabState = useCallback((tabId: string) => {
    tabStateRegistry.current.delete(tabId);
  }, []);

  const captureTabStates = useCallback((): Record<string, unknown> => {
    const states: Record<string, unknown> = {};
    for (const [tabId, reg] of tabStateRegistry.current.entries()) {
      try {
        states[tabId] = reg.getter();
      } catch (err) {
        console.warn(`[Workspace] Failed to capture state for tab ${tabId}:`, err);
      }
    }
    return sanitizeAllTabStates(states);
  }, []);

  // Point 2, 3: State machine init with AbortController + mountedRef
  useEffect(() => {
    const controller = new AbortController();

    const init = async () => {
      dispatch({ type: 'INIT_START' });
      try {
        const loaded = await loadWorkspacesFromDB();
        if (controller.signal.aborted || !mountedRef.current) return;

        const currentId = await withTimeout(getSetting(CURRENT_WS_KEY), DB_TIMEOUT_MS, 'Get current workspace timeout');
        if (controller.signal.aborted || !mountedRef.current) return;

        const currentWorkspace = currentId ? loaded.find(w => w.id === currentId) || null : null;
        dispatch({ type: 'INIT_SUCCESS', workspaces: loaded, currentWorkspace });
      } catch (err) {
        if (!controller.signal.aborted && mountedRef.current) {
          dispatch({ type: 'INIT_ERROR', error: err instanceof Error ? err.message : 'Failed to load workspaces' });
        }
      }
    };

    init();
    return () => controller.abort();
  }, []);

  const refreshWorkspaces = useCallback(async () => {
    try {
      const loaded = await loadWorkspacesFromDB();
      if (mountedRef.current) {
        dispatch({ type: 'SET_WORKSPACES', workspaces: loaded });
      }
    } catch (err) {
      console.warn('[Workspace] Refresh failed:', err);
    }
  }, []);

  const saveWorkspace = useCallback(async (name: string, activeTab: string) => {
    // Point 10: Validate input
    const sanitizedName = sanitizeInput(name);
    const nameError = validateWorkspaceName(sanitizedName);
    if (nameError) throw new Error(nameError);

    const now = new Date().toISOString();
    const settings = await captureGeneralSettings();
    const tabStates = captureTabStates();
    const existing = state.workspaces.find(w => w.name === sanitizedName);

    let updated: Workspace[];
    let workspace: Workspace;

    if (existing) {
      workspace = { ...existing, activeTab, settings, tabStates, updatedAt: now };
      updated = state.workspaces.map(w => w.id === existing.id ? workspace : w);
    } else {
      workspace = { id: generateId(), name: sanitizedName, activeTab, settings, tabStates, createdAt: now, updatedAt: now };
      updated = [...state.workspaces, workspace];
    }

    await persistWorkspaces(updated);
    if (mountedRef.current) {
      dispatch({ type: 'SET_WORKSPACES', workspaces: updated, currentWorkspace: workspace });
    }
    await withTimeout(saveSetting(CURRENT_WS_KEY, workspace.id, CATEGORY), DB_TIMEOUT_MS, 'Save current workspace ID timeout');
  }, [state.workspaces, captureTabStates]);

  const loadWorkspaceById = useCallback(async (id: string): Promise<{ activeTab: string; settings: Record<string, string> } | null> => {
    const workspace = state.workspaces.find(w => w.id === id);
    if (!workspace) return null;

    if (workspace.settings && Object.keys(workspace.settings).length > 0) {
      await applySettings(workspace.settings);
    }

    // Restore tab states: push to mounted tabs, store rest for lazy tabs
    const sanitizedTabStates = sanitizeAllTabStates(workspace.tabStates || {});
    restoredTabStates.current = { ...sanitizedTabStates };
    for (const [tabId, tabState] of Object.entries(sanitizedTabStates)) {
      const reg = tabStateRegistry.current.get(tabId);
      if (reg && tabState && typeof tabState === 'object' && !Array.isArray(tabState)) {
        reg.setter(tabState as Record<string, unknown>);
        delete restoredTabStates.current[tabId];
      }
    }

    if (mountedRef.current) {
      dispatch({ type: 'SET_CURRENT', workspace });
    }
    await withTimeout(saveSetting(CURRENT_WS_KEY, workspace.id, CATEGORY), DB_TIMEOUT_MS, 'Save current workspace ID timeout');
    return { activeTab: workspace.activeTab, settings: workspace.settings };
  }, [state.workspaces]);

  const deleteWorkspace = useCallback(async (id: string) => {
    const updated = state.workspaces.filter(w => w.id !== id);
    await persistWorkspaces(updated);
    if (mountedRef.current) {
      const newCurrent = state.currentWorkspace?.id === id ? null : state.currentWorkspace;
      dispatch({ type: 'SET_WORKSPACES', workspaces: updated, currentWorkspace: newCurrent });
    }
    if (state.currentWorkspace?.id === id) {
      await withTimeout(saveSetting(CURRENT_WS_KEY, '', CATEGORY), DB_TIMEOUT_MS, 'Clear current workspace timeout');
    }
  }, [state.workspaces, state.currentWorkspace]);

  const exportWorkspace = useCallback(async (id: string) => {
    const workspace = state.workspaces.find(w => w.id === id);
    if (!workspace) return;

    const exportData: WorkspaceExport = {
      version: 2,
      exportedAt: new Date().toISOString(),
      workspace: {
        ...workspace,
        tabStates: sanitizeAllTabStates(workspace.tabStates || {}),
      },
    };

    const filePath = await saveDialog({
      title: 'Export Workspace',
      defaultPath: `${workspace.name.replace(/\s+/g, '_')}.fincept-workspace.json`,
      filters: [{ name: 'Fincept Workspace', extensions: ['json'] }],
    });

    if (filePath) {
      await withTimeout(
        writeTextFile(filePath, JSON.stringify(exportData, null, 2)),
        FILE_TIMEOUT_MS,
        'Export workspace file write timeout',
      );
    }
  }, [state.workspaces]);

  const importWorkspace = useCallback(async (): Promise<Workspace | null> => {
    const filePath = await openDialog({
      title: 'Import Workspace',
      filters: [{ name: 'Fincept Workspace', extensions: ['json'] }],
      multiple: false,
    });

    if (!filePath) return null;

    const content = await withTimeout(
      readTextFile(filePath as string),
      FILE_TIMEOUT_MS,
      'Import workspace file read timeout',
    );

    let parsed: unknown;
    try {
      parsed = JSON.parse(content);
    } catch {
      throw new Error('Invalid JSON in workspace file');
    }

    // Point 10: Validate imported data
    if (!validateImportedWorkspace(parsed)) {
      throw new Error('Invalid workspace file: missing required fields (name, activeTab)');
    }

    const sanitizedName = sanitizeInput(parsed.workspace.name).slice(0, MAX_WORKSPACE_NAME_LENGTH);
    const sanitizedActiveTab = sanitizeInput(parsed.workspace.activeTab);

    const imported: Workspace = {
      ...parsed.workspace,
      id: generateId(),
      name: sanitizedName,
      activeTab: sanitizedActiveTab,
      tabStates: parsed.version === 2
        ? sanitizeAllTabStates(parsed.workspace.tabStates || {})
        : {},
      updatedAt: new Date().toISOString(),
    };

    const updated = [...state.workspaces, imported];
    await persistWorkspaces(updated);
    if (mountedRef.current) {
      dispatch({ type: 'SET_WORKSPACES', workspaces: updated });
    }
    return imported;
  }, [state.workspaces]);

  return (
    <WorkspaceContext.Provider value={{
      workspaces: state.workspaces,
      currentWorkspace: state.currentWorkspace,
      status: state.status,
      error: state.error,
      saveWorkspace,
      loadWorkspace: loadWorkspaceById,
      deleteWorkspace,
      exportWorkspace,
      importWorkspace,
      refreshWorkspaces,
      registerTabState,
      unregisterTabState,
    }}>
      {children}
    </WorkspaceContext.Provider>
  );
};
