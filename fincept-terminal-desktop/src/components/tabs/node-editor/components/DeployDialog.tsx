/**
 * DeployDialog Component
 *
 * Modal dialog for deploying workflows with name and description
 */

import React from 'react';
import { Rocket } from 'lucide-react';
import type { DeployDialogProps } from '../types';

const DeployDialog: React.FC<DeployDialogProps> = ({
  isOpen,
  onClose,
  workflowName,
  setWorkflowName,
  workflowDescription,
  setWorkflowDescription,
  onDeploy,
  onSaveDraft,
}) => {
  if (!isOpen) return null;

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.7)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 2000,
      }}
      onClick={onClose}
    >
      <div
        style={{
          backgroundColor: '#1a1a1a',
          border: '1px solid #2d2d2d',
          borderRadius: '6px',
          padding: '24px',
          minWidth: '400px',
          maxWidth: '500px',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        <div
          style={{
            color: '#ea580c',
            fontSize: '14px',
            fontWeight: 'bold',
            marginBottom: '20px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
          }}
        >
          <Rocket size={16} />
          DEPLOY WORKFLOW
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label
            style={{
              display: 'block',
              color: '#a3a3a3',
              fontSize: '11px',
              marginBottom: '6px',
              fontWeight: 'bold',
            }}
          >
            WORKFLOW NAME *
          </label>
          <input
            type="text"
            value={workflowName}
            onChange={(e) => setWorkflowName(e.target.value)}
            placeholder="Enter workflow name..."
            autoFocus
            style={{
              width: '100%',
              backgroundColor: '#0a0a0a',
              border: '1px solid #2d2d2d',
              color: 'white',
              padding: '8px 12px',
              fontSize: '12px',
              borderRadius: '4px',
              outline: 'none',
              boxSizing: 'border-box',
            }}
          />
        </div>

        <div style={{ marginBottom: '20px' }}>
          <label
            style={{
              display: 'block',
              color: '#a3a3a3',
              fontSize: '11px',
              marginBottom: '6px',
              fontWeight: 'bold',
            }}
          >
            DESCRIPTION (OPTIONAL)
          </label>
          <textarea
            value={workflowDescription}
            onChange={(e) => setWorkflowDescription(e.target.value)}
            placeholder="Enter workflow description..."
            rows={3}
            style={{
              width: '100%',
              backgroundColor: '#0a0a0a',
              border: '1px solid #2d2d2d',
              color: 'white',
              padding: '8px 12px',
              fontSize: '12px',
              borderRadius: '4px',
              outline: 'none',
              resize: 'vertical',
              fontFamily: 'Consolas, monospace',
              boxSizing: 'border-box',
            }}
          />
        </div>

        <div style={{ display: 'flex', gap: '12px', justifyContent: 'flex-end' }}>
          <button
            onClick={() => {
              onClose();
              setWorkflowName('');
              setWorkflowDescription('');
            }}
            style={{
              backgroundColor: 'transparent',
              color: '#a3a3a3',
              border: '1px solid #404040',
              padding: '8px 16px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '4px',
              fontWeight: 'bold',
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onSaveDraft}
            style={{
              backgroundColor: 'transparent',
              color: '#f59e0b',
              border: '1px solid #f59e0b',
              padding: '8px 16px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '4px',
              fontWeight: 'bold',
            }}
          >
            SAVE AS DRAFT
          </button>
          <button
            onClick={onDeploy}
            style={{
              backgroundColor: '#3b82f6',
              color: 'white',
              border: 'none',
              padding: '8px 16px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '4px',
              fontWeight: 'bold',
            }}
          >
            DEPLOY & EXECUTE
          </button>
        </div>
      </div>
    </div>
  );
};

export default DeployDialog;
