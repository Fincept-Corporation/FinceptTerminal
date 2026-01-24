import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import { saveSetting, getSetting, getAllSettings, Setting } from '@/services/core/sqliteService';
import { save as saveDialog, open as openDialog } from '@tauri-apps/plugin-dialog';
import { writeTextFile, readTextFile } from '@tauri-apps/plugin-fs';

const WORKSPACES_KEY = 'workspaces_list';
const CURRENT_WS_KEY = 'current_workspace_id';
const CATEGORY = 'workspace';

// Settings categories to include in workspace (exclude credentials/keys)
const INCLUDED_SETTING_CATEGORIES = ['theme', 'timezone', 'i18n', 'market_preferences', 'reports'];

export interface Workspace {
  id: string;
  name: string;
  activeTab: string;
  settings: Record<string, string>; // key -> value for general settings
  createdAt: string;
  updatedAt: string;
}

export interface WorkspaceExport {
  version: 1;
  exportedAt: string;
  workspace: Workspace;
}

interface WorkspaceContextType {
  workspaces: Workspace[];
  currentWorkspace: Workspace | null;
  saveWorkspace: (name: string, activeTab: string) => Promise<void>;
  loadWorkspace: (id: string) => Promise<{ activeTab: string; settings: Record<string, string> } | null>;
  deleteWorkspace: (id: string) => Promise<void>;
  exportWorkspace: (id: string) => Promise<void>;
  importWorkspace: () => Promise<Workspace | null>;
  refreshWorkspaces: () => Promise<void>;
}

const WorkspaceContext = createContext<WorkspaceContextType | undefined>(undefined);

export const useWorkspace = () => {
  const context = useContext(WorkspaceContext);
  if (!context) {
    throw new Error('useWorkspace must be used within WorkspaceProvider');
  }
  return context;
};

function generateId(): string {
  return Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
}

async function loadWorkspaces(): Promise<Workspace[]> {
  try {
    const raw = await getSetting(WORKSPACES_KEY);
    return raw ? JSON.parse(raw) : [];
  } catch {
    return [];
  }
}

async function persistWorkspaces(workspaces: Workspace[]): Promise<void> {
  await saveSetting(WORKSPACES_KEY, JSON.stringify(workspaces), CATEGORY);
}

async function captureGeneralSettings(): Promise<Record<string, string>> {
  try {
    const allSettings = await getAllSettings();
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
      await saveSetting(key, value);
    } catch (err) {
      console.warn(`[Workspace] Failed to apply setting ${key}:`, err);
    }
  }
}

export const WorkspaceProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
  const [workspaces, setWorkspaces] = useState<Workspace[]>([]);
  const [currentWorkspace, setCurrentWorkspace] = useState<Workspace | null>(null);

  useEffect(() => {
    (async () => {
      const loaded = await loadWorkspaces();
      setWorkspaces(loaded);
      const currentId = await getSetting(CURRENT_WS_KEY);
      if (currentId) {
        const current = loaded.find(w => w.id === currentId) || null;
        setCurrentWorkspace(current);
      }
    })();
  }, []);

  const refreshWorkspaces = async () => {
    const loaded = await loadWorkspaces();
    setWorkspaces(loaded);
  };

  const saveWorkspace = async (name: string, activeTab: string) => {
    const now = new Date().toISOString();
    const settings = await captureGeneralSettings();
    const existing = workspaces.find(w => w.name === name);

    let updated: Workspace[];
    let workspace: Workspace;

    if (existing) {
      workspace = { ...existing, activeTab, settings, updatedAt: now };
      updated = workspaces.map(w => w.id === existing.id ? workspace : w);
    } else {
      workspace = { id: generateId(), name, activeTab, settings, createdAt: now, updatedAt: now };
      updated = [...workspaces, workspace];
    }

    await persistWorkspaces(updated);
    setWorkspaces(updated);
    setCurrentWorkspace(workspace);
    await saveSetting(CURRENT_WS_KEY, workspace.id, CATEGORY);
  };

  const loadWorkspaceById = async (id: string): Promise<{ activeTab: string; settings: Record<string, string> } | null> => {
    const workspace = workspaces.find(w => w.id === id);
    if (!workspace) return null;

    // Apply saved settings
    if (workspace.settings && Object.keys(workspace.settings).length > 0) {
      await applySettings(workspace.settings);
    }

    setCurrentWorkspace(workspace);
    await saveSetting(CURRENT_WS_KEY, workspace.id, CATEGORY);
    return { activeTab: workspace.activeTab, settings: workspace.settings };
  };

  const deleteWorkspace = async (id: string) => {
    const updated = workspaces.filter(w => w.id !== id);
    await persistWorkspaces(updated);
    setWorkspaces(updated);
    if (currentWorkspace?.id === id) {
      setCurrentWorkspace(null);
      await saveSetting(CURRENT_WS_KEY, '', CATEGORY);
    }
  };

  const exportWorkspace = async (id: string) => {
    const workspace = workspaces.find(w => w.id === id);
    if (!workspace) return;

    const exportData: WorkspaceExport = {
      version: 1,
      exportedAt: new Date().toISOString(),
      workspace,
    };

    const filePath = await saveDialog({
      title: 'Export Workspace',
      defaultPath: `${workspace.name.replace(/\s+/g, '_')}.fincept-workspace.json`,
      filters: [{ name: 'Fincept Workspace', extensions: ['json'] }],
    });

    if (filePath) {
      await writeTextFile(filePath, JSON.stringify(exportData, null, 2));
    }
  };

  const importWorkspace = async (): Promise<Workspace | null> => {
    const filePath = await openDialog({
      title: 'Import Workspace',
      filters: [{ name: 'Fincept Workspace', extensions: ['json'] }],
      multiple: false,
    });

    if (!filePath) return null;

    const content = await readTextFile(filePath as string);
    const parsed: WorkspaceExport = JSON.parse(content);

    if (!parsed.workspace || !parsed.workspace.name || !parsed.workspace.activeTab) {
      throw new Error('Invalid workspace file');
    }

    // Assign new ID to avoid conflicts
    const imported: Workspace = {
      ...parsed.workspace,
      id: generateId(),
      updatedAt: new Date().toISOString(),
    };

    const updated = [...workspaces, imported];
    await persistWorkspaces(updated);
    setWorkspaces(updated);
    return imported;
  };

  return (
    <WorkspaceContext.Provider value={{
      workspaces,
      currentWorkspace,
      saveWorkspace,
      loadWorkspace: loadWorkspaceById,
      deleteWorkspace,
      exportWorkspace,
      importWorkspace,
      refreshWorkspaces,
    }}>
      {children}
    </WorkspaceContext.Provider>
  );
};
