import React from 'react';
import { X } from 'lucide-react';

interface ResultsModalProps {
  data: any;
  workflowName: string;
  onClose: () => void;
}

const ResultsModal: React.FC<ResultsModalProps> = ({ data, workflowName, onClose }) => {
  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.8)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 9999
    }} onClick={onClose}>
      <div style={{
        background: '#1a1a1a',
        border: '1px solid #2d2d2d',
        borderRadius: '8px',
        padding: '20px',
        maxWidth: '600px',
        width: '90%'
      }} onClick={(e) => e.stopPropagation()}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '16px'
        }}>
          <h3 style={{ color: '#ea580c' }}>{workflowName} - Results</h3>
          <button onClick={onClose} style={{
            background: 'transparent',
            border: 'none',
            color: '#a3a3a3',
            cursor: 'pointer'
          }}>
            <X size={20} />
          </button>
        </div>
        <pre style={{
          background: '#0a0a0a',
          padding: '12px',
          borderRadius: '4px',
          overflow: 'auto',
          maxHeight: '400px',
          color: '#a3a3a3'
        }}>
          {JSON.stringify(data, null, 2)}
        </pre>
      </div>
    </div>
  );
};

export default ResultsModal;
