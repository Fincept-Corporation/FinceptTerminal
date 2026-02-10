import React from 'react';
import { X } from 'lucide-react';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  FONT_FAMILY,
  FONT_SIZE,
} from './shared';

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
      zIndex: 9999,
    }} onClick={onClose}>
      <div style={{
        background: FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: BORDER_RADIUS.LG,
        padding: SPACING.XXL,
        maxWidth: '600px',
        width: '90%',
        fontFamily: FONT_FAMILY,
      }} onClick={(e) => e.stopPropagation()}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: SPACING.XL,
        }}>
          <h3 style={{
            color: FINCEPT.ORANGE,
            fontSize: FONT_SIZE.XXL,
            fontWeight: 700,
            margin: 0,
            fontFamily: FONT_FAMILY,
          }}>
            {workflowName} - Results
          </h3>
          <button onClick={onClose} style={{
            background: 'transparent',
            border: 'none',
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            padding: SPACING.XS,
            display: 'flex',
            alignItems: 'center',
          }}>
            <X size={20} />
          </button>
        </div>
        <pre style={{
          background: FINCEPT.DARK_BG,
          padding: SPACING.LG,
          borderRadius: BORDER_RADIUS.SM,
          overflow: 'auto',
          maxHeight: '400px',
          color: FINCEPT.GRAY,
          fontSize: FONT_SIZE.LG,
          fontFamily: FONT_FAMILY,
          margin: 0,
          border: `1px solid ${FINCEPT.BORDER}`,
        }}>
          {JSON.stringify(data, null, 2)}
        </pre>
      </div>
    </div>
  );
};

export default ResultsModal;
