import React, { useState } from 'react';
import { useWorkspace } from '@/contexts/WorkspaceContext';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

interface WorkspaceDialogProps {
  mode: 'save' | 'open' | 'new' | 'export' | 'import';
  activeTab: string;
  onClose: () => void;
  onLoadWorkspace: (tab: string) => void;
  onStatusMessage?: (msg: string) => void;
}

const dialogOverlayStyle: React.CSSProperties = {
  position: 'fixed',
  top: 0,
  left: 0,
  right: 0,
  bottom: 0,
  backgroundColor: 'rgba(0, 0, 0, 0.7)',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  zIndex: 9999,
};

const dialogStyle: React.CSSProperties = {
  backgroundColor: '#1a1a1a',
  border: '1px solid #404040',
  borderRadius: '6px',
  padding: '16px',
  minWidth: '360px',
  maxWidth: '440px',
  color: '#e5e5e5',
  fontSize: '12px',
  fontFamily: 'Consolas, "Courier New", monospace',
};

const inputStyle: React.CSSProperties = {
  width: '100%',
  padding: '6px 8px',
  backgroundColor: '#0a0a0a',
  border: '1px solid #404040',
  borderRadius: '4px',
  color: '#e5e5e5',
  fontSize: '12px',
  fontFamily: 'inherit',
  outline: 'none',
  boxSizing: 'border-box',
};

const btnStyle: React.CSSProperties = {
  padding: '5px 12px',
  fontSize: '11px',
  borderRadius: '4px',
  border: 'none',
  cursor: 'pointer',
  fontFamily: 'inherit',
};

const btnPrimary: React.CSSProperties = { ...btnStyle, backgroundColor: '#2563eb', color: '#fff' };
const btnSecondary: React.CSSProperties = { ...btnStyle, backgroundColor: '#333', color: '#ccc' };
const btnDanger: React.CSSProperties = { ...btnStyle, backgroundColor: '#7f1d1d', color: '#fca5a5', padding: '3px 8px', fontSize: '10px' };
const btnExport: React.CSSProperties = { ...btnStyle, backgroundColor: '#1e3a5f', color: '#93c5fd', padding: '3px 8px', fontSize: '10px' };

const WorkspaceDialog: React.FC<WorkspaceDialogProps> = ({ mode, activeTab, onClose, onLoadWorkspace, onStatusMessage }) => {
  const { workspaces, saveWorkspace, loadWorkspace, deleteWorkspace, exportWorkspace, importWorkspace } = useWorkspace();
  const [name, setName] = useState('');
  const [error, setError] = useState('');

  const handleSave = async () => {
    const trimmed = name.trim();
    if (!trimmed) { setError('Name is required'); return; }
    await saveWorkspace(trimmed, activeTab);
    onStatusMessage?.(`Workspace "${trimmed}" saved`);
    onClose();
  };

  const handleNew = async () => {
    const trimmed = name.trim();
    if (!trimmed) { setError('Name is required'); return; }
    await saveWorkspace(trimmed, 'dashboard');
    onLoadWorkspace('dashboard');
    onStatusMessage?.(`Workspace "${trimmed}" created`);
    onClose();
  };

  const handleLoad = async (id: string) => {
    const result = await loadWorkspace(id);
    if (result) {
      onLoadWorkspace(result.activeTab);
      onStatusMessage?.('Workspace loaded');
    }
    onClose();
  };

  const handleDelete = async (e: React.MouseEvent, id: string) => {
    e.stopPropagation();
    await deleteWorkspace(id);
  };

  const handleExport = async (e: React.MouseEvent, id: string) => {
    e.stopPropagation();
    try {
      await exportWorkspace(id);
      onStatusMessage?.('Workspace exported');
    } catch (err) {
      console.error('[Workspace] Export failed:', err);
    }
  };

  const handleImport = async () => {
    try {
      const imported = await importWorkspace();
      if (imported) {
        onStatusMessage?.(`Workspace "${imported.name}" imported`);
      }
    } catch (err) {
      setError('Failed to import workspace file');
      console.error('[Workspace] Import failed:', err);
    }
  };

  // Import mode - trigger immediately
  if (mode === 'import') {
    handleImport().then(() => onClose());
    return null;
  }

  // Export mode - show list to pick which workspace to export
  if (mode === 'export') {
    return (
      <div style={dialogOverlayStyle} onClick={onClose}>
        <div style={dialogStyle} onClick={e => e.stopPropagation()}>
          <h3 style={{ margin: '0 0 12px', fontSize: '13px', color: '#fff' }}>Export Workspace</h3>
          {workspaces.length === 0 ? (
            <p style={{ color: '#737373', margin: '16px 0' }}>No workspaces to export.</p>
          ) : (
            <div style={{ maxHeight: '240px', overflowY: 'auto' }}>
              {workspaces.map(ws => (
                <div
                  key={ws.id}
                  onClick={e => handleExport(e, ws.id)}
                  style={{
                    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                    padding: '8px 10px', marginBottom: '4px',
                    backgroundColor: '#0a0a0a', border: '1px solid #333', borderRadius: '4px', cursor: 'pointer',
                  }}
                  onMouseEnter={e => (e.currentTarget.style.borderColor = '#2563eb')}
                  onMouseLeave={e => (e.currentTarget.style.borderColor = '#333')}
                >
                  <div>
                    <div style={{ color: '#e5e5e5', fontWeight: 500 }}>{ws.name}</div>
                    <div style={{ color: '#737373', fontSize: '10px', marginTop: '2px' }}>
                      Tab: {ws.activeTab} | Settings: {Object.keys(ws.settings || {}).length} | Tab configs: {Object.keys(ws.tabStates || {}).length}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          )}
          <div style={{ marginTop: '12px', display: 'flex', justifyContent: 'flex-end' }}>
            <button style={btnSecondary} onClick={onClose}>Close</button>
          </div>
        </div>
      </div>
    );
  }

  // Open mode
  if (mode === 'open') {
    return (
      <div style={dialogOverlayStyle} onClick={onClose}>
        <div style={dialogStyle} onClick={e => e.stopPropagation()}>
          <h3 style={{ margin: '0 0 12px', fontSize: '13px', color: '#fff' }}>Open Workspace</h3>
          {workspaces.length === 0 ? (
            <p style={{ color: '#737373', margin: '16px 0' }}>No saved workspaces yet.</p>
          ) : (
            <div style={{ maxHeight: '240px', overflowY: 'auto' }}>
              {workspaces.map(ws => (
                <div
                  key={ws.id}
                  onClick={() => handleLoad(ws.id)}
                  style={{
                    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                    padding: '8px 10px', marginBottom: '4px',
                    backgroundColor: '#0a0a0a', border: '1px solid #333', borderRadius: '4px', cursor: 'pointer',
                  }}
                  onMouseEnter={e => (e.currentTarget.style.borderColor = '#2563eb')}
                  onMouseLeave={e => (e.currentTarget.style.borderColor = '#333')}
                >
                  <div>
                    <div style={{ color: '#e5e5e5', fontWeight: 500 }}>{ws.name}</div>
                    <div style={{ color: '#737373', fontSize: '10px', marginTop: '2px' }}>
                      Tab: {ws.activeTab} | Settings: {Object.keys(ws.settings || {}).length} | Tab configs: {Object.keys(ws.tabStates || {}).length}
                    </div>
                  </div>
                  <div style={{ display: 'flex', gap: '4px' }}>
                    <button style={btnExport} onClick={e => handleExport(e, ws.id)}>Export</button>
                    <button style={btnDanger} onClick={e => handleDelete(e, ws.id)}>Del</button>
                  </div>
                </div>
              ))}
            </div>
          )}
          <div style={{ marginTop: '12px', display: 'flex', justifyContent: 'flex-end' }}>
            <button style={btnSecondary} onClick={onClose}>Close</button>
          </div>
        </div>
      </div>
    );
  }

  // Save or New mode
  const title = mode === 'save' ? 'Save Workspace' : 'New Workspace';
  const actionLabel = mode === 'save' ? 'Save' : 'Create';
  const handleSubmit = mode === 'save' ? handleSave : handleNew;

  return (
    <div style={dialogOverlayStyle} onClick={onClose}>
      <div style={dialogStyle} onClick={e => e.stopPropagation()}>
        <h3 style={{ margin: '0 0 12px', fontSize: '13px', color: '#fff' }}>{title}</h3>
        <div style={{ marginBottom: '12px' }}>
          <label style={{ display: 'block', marginBottom: '4px', color: '#a3a3a3' }}>Workspace Name</label>
          <input
            style={inputStyle}
            value={name}
            onChange={e => { setName(e.target.value); setError(''); }}
            onKeyDown={e => e.key === 'Enter' && handleSubmit()}
            placeholder="e.g. Morning Research"
            autoFocus
          />
          {error && <div style={{ color: '#ef4444', fontSize: '10px', marginTop: '4px' }}>{error}</div>}
        </div>
        {mode === 'save' && (
          <div style={{ marginBottom: '12px', color: '#737373', fontSize: '10px' }}>
            Saves current tab ({activeTab}) + general settings + tab configurations
          </div>
        )}
        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '8px' }}>
          <button style={btnSecondary} onClick={onClose}>Cancel</button>
          <button style={btnPrimary} onClick={handleSubmit}>{actionLabel}</button>
        </div>
      </div>
    </div>
  );
};

const WorkspaceDialogWithBoundary: React.FC<WorkspaceDialogProps> = (props) => (
  <ErrorBoundary name="WorkspaceDialog" variant="minimal">
    <WorkspaceDialog {...props} />
  </ErrorBoundary>
);

export default WorkspaceDialogWithBoundary;
