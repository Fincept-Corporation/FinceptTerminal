// Notes Tab Templates Panel Component

import React from 'react';
import { FINCEPT } from '../constants';
import type { NoteTemplate } from '../types';

interface TemplatesPanelProps {
  templates: NoteTemplate[];
  onSelectTemplate: (template: NoteTemplate) => void;
}

export const TemplatesPanel: React.FC<TemplatesPanelProps> = ({
  templates,
  onSelectTemplate
}) => {
  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      padding: '12px',
      flexShrink: 0
    }}>
      <div style={{ color: FINCEPT.ORANGE, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
        SELECT TEMPLATE
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '8px' }}>
        {templates.map(template => (
          <div
            key={template.id}
            onClick={() => onSelectTemplate(template)}
            style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
            }}
          >
            <div style={{ color: FINCEPT.CYAN, fontSize: '11px', fontWeight: 700, marginBottom: '4px' }}>
              {template.name}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', lineHeight: '1.4' }}>
              {template.description}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
