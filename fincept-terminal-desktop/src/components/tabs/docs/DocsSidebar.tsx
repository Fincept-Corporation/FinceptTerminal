import React from 'react';
import { ChevronRight } from 'lucide-react';
import { DocSection } from './types';
import { COLORS } from './constants';

interface DocsSidebarProps {
  docs: DocSection[];
  selectedSection: string;
  expandedSections: string[];
  onToggleSection: (sectionId: string) => void;
  onSelectSubsection: (subsectionId: string) => void;
}

export function DocsSidebar({
  docs,
  selectedSection,
  expandedSections,
  onToggleSection,
  onSelectSubsection
}: DocsSidebarProps) {
  return (
    <div
      className="docs-scroll"
      style={{
        flex: '0 0 280px',
        minWidth: '240px',
        maxWidth: '320px',
        borderRight: `1px solid ${COLORS.GRAY}`,
        backgroundColor: COLORS.PANEL_BG,
        overflowY: 'auto',
        padding: '12px'
      }}
    >
      {docs.map((section) => (
        <div key={section.id} style={{ marginBottom: '8px' }}>
          <div
            onClick={() => onToggleSection(section.id)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '8px',
              backgroundColor: COLORS.DARK_BG,
              cursor: 'pointer',
              border: `1px solid ${COLORS.GRAY}`
            }}
          >
            <section.icon size={14} style={{ color: COLORS.ORANGE }} />
            <span style={{ flex: 1, fontSize: '12px', fontWeight: 'bold', color: COLORS.WHITE }}>
              {section.title}
            </span>
            <ChevronRight
              size={14}
              style={{
                color: COLORS.GRAY,
                transform: expandedSections.includes(section.id) ? 'rotate(90deg)' : 'rotate(0deg)',
                transition: 'transform 0.2s'
              }}
            />
          </div>
          {expandedSections.includes(section.id) && (
            <div style={{ marginTop: '4px', marginLeft: '12px' }}>
              {section.subsections.map((sub) => (
                <div
                  key={sub.id}
                  onClick={() => onSelectSubsection(sub.id)}
                  style={{
                    padding: '8px 12px',
                    fontSize: '11px',
                    color: selectedSection === sub.id ? COLORS.ORANGE : COLORS.GRAY,
                    backgroundColor: selectedSection === sub.id ? COLORS.DARK_BG : 'transparent',
                    cursor: 'pointer',
                    borderLeft: `2px solid ${selectedSection === sub.id ? COLORS.ORANGE : 'transparent'}`
                  }}
                >
                  {sub.title}
                </div>
              ))}
            </div>
          )}
        </div>
      ))}
    </div>
  );
}
