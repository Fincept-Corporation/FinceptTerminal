/**
 * NodeEditorToolbar Component
 *
 * Toolbar for the node editor with actions like execute, save, import/export, etc.
 */

import React from 'react';
import {
  Plus,
  Play,
  Save,
  Download,
  Upload,
  Trash2,
  Rocket,
  LayoutTemplate,
} from 'lucide-react';
import { useTranslation } from 'react-i18next';
import type { NodeEditorToolbarProps } from '../types';

const NodeEditorToolbar: React.FC<NodeEditorToolbarProps> = ({
  activeView,
  setActiveView,
  showNodeMenu,
  setShowNodeMenu,
  isExecuting,
  nodes,
  edges,
  selectedNodes,
  workflowMode,
  editingDraftId,
  onExecute,
  onClearWorkflow,
  onSaveWorkflow,
  onImportWorkflow,
  onExportWorkflow,
  onDeleteSelectedNodes,
  onShowDeployDialog,
  onQuickSaveDraft,
  onShowTemplates,
}) => {
  const { t } = useTranslation('nodeEditor');

  return (
    <div
      style={{
        backgroundColor: '#1a1a1a',
        borderBottom: '1px solid #2d2d2d',
        padding: '8px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexWrap: 'wrap',
        flexShrink: 0,
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span
          style={{
            color: '#ea580c',
            fontSize: '12px',
            fontWeight: 'bold',
            letterSpacing: '0.5px',
          }}
        >
          {t('title')}
        </span>
        <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }} />

        {/* Tab Switcher */}
        <button
          onClick={() => setActiveView('editor')}
          style={{
            backgroundColor: activeView === 'editor' ? '#ea580c' : 'transparent',
            color: activeView === 'editor' ? 'white' : '#a3a3a3',
            border: activeView === 'editor' ? 'none' : '1px solid #404040',
            padding: '4px 10px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '3px',
            fontWeight: 'bold',
          }}
        >
          {t('tabs.editor')}
        </button>
        <button
          onClick={() => setActiveView('workflows')}
          style={{
            backgroundColor: activeView === 'workflows' ? '#ea580c' : 'transparent',
            color: activeView === 'workflows' ? 'white' : '#a3a3a3',
            border: activeView === 'workflows' ? 'none' : '1px solid #404040',
            padding: '4px 10px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '3px',
            fontWeight: 'bold',
          }}
        >
          WORKFLOWS
        </button>
      </div>

      {/* Editor-specific buttons */}
      {activeView === 'editor' && (
        <>
          <button
            onClick={() => setShowNodeMenu(!showNodeMenu)}
            style={{
              backgroundColor: '#ea580c',
              color: 'white',
              border: 'none',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: 'bold',
            }}
            onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = '#dc2626')}
            onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = '#ea580c')}
          >
            <Plus size={14} />
            ADD NODE
          </button>

          <button
            onClick={onShowTemplates}
            style={{
              backgroundColor: 'transparent',
              color: '#8b5cf6',
              border: '1px solid #8b5cf6',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: 'bold',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#8b5cf6';
              e.currentTarget.style.color = 'white';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#8b5cf6';
            }}
            title="Browse predefined workflow templates"
          >
            <LayoutTemplate size={14} />
            TEMPLATES
          </button>

          <button
            onClick={onExecute}
            disabled={isExecuting}
            style={{
              backgroundColor: 'transparent',
              color: isExecuting ? '#737373' : '#10b981',
              border: `1px solid ${isExecuting ? '#737373' : '#10b981'}`,
              padding: '6px 12px',
              fontSize: '11px',
              cursor: isExecuting ? 'not-allowed' : 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              opacity: isExecuting ? 0.5 : 1,
            }}
            onMouseEnter={(e) => {
              if (!isExecuting) {
                e.currentTarget.style.backgroundColor = '#10b981';
                e.currentTarget.style.color = 'white';
              }
            }}
            onMouseLeave={(e) => {
              if (!isExecuting) {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.color = '#10b981';
              }
            }}
          >
            <Play size={14} />
            {isExecuting ? 'EXECUTING...' : 'EXECUTE'}
          </button>

          <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }} />

          {/* New Workflow Button */}
          <button
            onClick={() => {
              if (window.confirm('Start a new workflow? Current work will be cleared unless saved.')) {
                onClearWorkflow();
              }
            }}
            style={{
              backgroundColor: 'transparent',
              color: '#3b82f6',
              border: '1px solid #3b82f6',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: 'bold',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#3b82f6';
              e.currentTarget.style.color = 'white';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#3b82f6';
            }}
            title="Start a new workflow"
          >
            <Plus size={14} />
            NEW
          </button>

          {/* Quick Save for Drafts */}
          {workflowMode === 'draft' && editingDraftId && (
            <button
              onClick={onQuickSaveDraft}
              style={{
                backgroundColor: '#f59e0b',
                color: 'white',
                border: 'none',
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                fontWeight: 'bold',
              }}
              title="Quick save current draft"
            >
              <Save size={14} />
              SAVE DRAFT
            </button>
          )}

          <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }} />

          <button
            onClick={onSaveWorkflow}
            style={{
              backgroundColor: 'transparent',
              color: '#a3a3a3',
              border: '1px solid #404040',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#2d2d2d';
              e.currentTarget.style.color = 'white';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#a3a3a3';
            }}
            title="Save workflow to browser storage"
          >
            <Save size={14} />
            SAVE
          </button>

          <button
            onClick={onImportWorkflow}
            style={{
              backgroundColor: 'transparent',
              color: '#a3a3a3',
              border: '1px solid #404040',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#2d2d2d';
              e.currentTarget.style.color = 'white';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#a3a3a3';
            }}
            title="Import workflow from file"
          >
            <Upload size={14} />
            IMPORT
          </button>

          <button
            onClick={onExportWorkflow}
            style={{
              backgroundColor: 'transparent',
              color: '#a3a3a3',
              border: '1px solid #404040',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#2d2d2d';
              e.currentTarget.style.color = 'white';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#a3a3a3';
            }}
          >
            <Download size={14} />
            EXPORT
          </button>

          <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }} />

          <button
            onClick={() => {
              if (window.confirm('Are you sure you want to clear all nodes? This cannot be undone.')) {
                onClearWorkflow();
              }
            }}
            style={{
              backgroundColor: 'transparent',
              color: '#ef4444',
              border: '1px solid #ef4444',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#ef4444';
              e.currentTarget.style.color = 'white';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#ef4444';
            }}
            title="Clear all nodes and connections"
          >
            <Trash2 size={14} />
            CLEAR ALL
          </button>

          {selectedNodes.length > 0 && (
            <>
              <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }} />
              <button
                onClick={onDeleteSelectedNodes}
                style={{
                  backgroundColor: 'transparent',
                  color: '#ef4444',
                  border: '1px solid #ef4444',
                  padding: '6px 12px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '3px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = '#ef4444';
                  e.currentTarget.style.color = 'white';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.color = '#ef4444';
                }}
              >
                <Trash2 size={14} />
                DELETE ({selectedNodes.length})
              </button>
            </>
          )}

          <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }} />

          {/* DEPLOY Button */}
          <button
            onClick={onShowDeployDialog}
            style={{
              backgroundColor: 'transparent',
              color: '#3b82f6',
              border: '1px solid #3b82f6',
              padding: '6px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: 'bold',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#3b82f6';
              e.currentTarget.style.color = 'white';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#3b82f6';
            }}
            title="Deploy workflow to database"
          >
            <Rocket size={14} />
            DEPLOY
          </button>

          <div
            style={{
              marginLeft: 'auto',
              display: 'flex',
              alignItems: 'center',
              gap: '12px',
            }}
          >
            <span style={{ color: '#737373', fontSize: '10px' }}>NODES: {nodes.length}</span>
            <span style={{ color: '#737373', fontSize: '10px' }}>CONNECTIONS: {edges.length}</span>
          </div>
        </>
      )}
    </div>
  );
};

export default NodeEditorToolbar;
