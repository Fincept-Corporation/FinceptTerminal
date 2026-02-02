/**
 * TemplateGallery Component
 *
 * Displays predefined workflow templates that users can browse and import
 * into the node editor to quickly get started.
 */

import React, { useState } from 'react';
import {
  BookOpen,
  ArrowRight,
  Tag,
  Layers,
  X,
  Zap,
  BarChart2,
  Brain,
} from 'lucide-react';
import { WORKFLOW_TEMPLATES, WorkflowTemplate } from '../workflowTemplates';

interface TemplateGalleryProps {
  isOpen: boolean;
  onClose: () => void;
  onImportTemplate: (template: WorkflowTemplate) => void;
}

const CATEGORY_STYLES: Record<string, { color: string; label: string; icon: React.ReactNode }> = {
  beginner: { color: '#10b981', label: 'Beginner', icon: <BookOpen size={12} /> },
  intermediate: { color: '#f59e0b', label: 'Intermediate', icon: <Zap size={12} /> },
  advanced: { color: '#ef4444', label: 'Advanced', icon: <Brain size={12} /> },
};

const TemplateGallery: React.FC<TemplateGalleryProps> = ({
  isOpen,
  onClose,
  onImportTemplate,
}) => {
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [hoveredTemplate, setHoveredTemplate] = useState<string | null>(null);

  if (!isOpen) return null;

  const filteredTemplates =
    selectedCategory === 'all'
      ? WORKFLOW_TEMPLATES
      : WORKFLOW_TEMPLATES.filter((t) => t.category === selectedCategory);

  const handleImport = (template: WorkflowTemplate) => {
    onImportTemplate(template);
    onClose();
  };

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.7)',
        zIndex: 10000,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div
        style={{
          backgroundColor: '#1a1a1a',
          border: '1px solid #2d2d2d',
          borderRadius: '8px',
          width: '900px',
          maxWidth: '90vw',
          maxHeight: '80vh',
          display: 'flex',
          flexDirection: 'column',
          boxShadow: '0 8px 32px rgba(0,0,0,0.6)',
        }}
      >
        {/* Header */}
        <div
          style={{
            padding: '16px 20px',
            borderBottom: '1px solid #2d2d2d',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <Layers size={18} style={{ color: '#ea580c' }} />
            <span style={{ color: '#ffffff', fontSize: '14px', fontWeight: 'bold' }}>
              Workflow Templates
            </span>
            <span style={{ color: '#737373', fontSize: '12px' }}>
              {WORKFLOW_TEMPLATES.length} templates available
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              background: 'transparent',
              border: 'none',
              color: '#737373',
              cursor: 'pointer',
              padding: '4px',
            }}
          >
            <X size={18} />
          </button>
        </div>

        {/* Category Filters */}
        <div
          style={{
            padding: '12px 20px',
            borderBottom: '1px solid #2d2d2d',
            display: 'flex',
            gap: '8px',
          }}
        >
          <button
            onClick={() => setSelectedCategory('all')}
            style={{
              backgroundColor: selectedCategory === 'all' ? '#ea580c' : 'transparent',
              color: selectedCategory === 'all' ? 'white' : '#a3a3a3',
              border: selectedCategory === 'all' ? 'none' : '1px solid #404040',
              padding: '5px 12px',
              fontSize: '11px',
              cursor: 'pointer',
              borderRadius: '4px',
              fontWeight: 'bold',
            }}
          >
            All ({WORKFLOW_TEMPLATES.length})
          </button>
          {Object.entries(CATEGORY_STYLES).map(([key, style]) => {
            const count = WORKFLOW_TEMPLATES.filter((t) => t.category === key).length;
            return (
              <button
                key={key}
                onClick={() => setSelectedCategory(key)}
                style={{
                  backgroundColor: selectedCategory === key ? style.color : 'transparent',
                  color: selectedCategory === key ? 'white' : style.color,
                  border: `1px solid ${selectedCategory === key ? style.color : style.color + '60'}`,
                  padding: '5px 12px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '5px',
                  fontWeight: 'bold',
                }}
              >
                {style.icon}
                {style.label} ({count})
              </button>
            );
          })}
        </div>

        {/* Template Grid */}
        <div
          style={{
            flex: 1,
            overflow: 'auto',
            padding: '16px 20px',
            display: 'grid',
            gridTemplateColumns: 'repeat(2, 1fr)',
            gap: '12px',
            alignContent: 'start',
          }}
        >
          {filteredTemplates.map((template) => {
            const catStyle = CATEGORY_STYLES[template.category];
            const isHovered = hoveredTemplate === template.id;

            return (
              <div
                key={template.id}
                onMouseEnter={() => setHoveredTemplate(template.id)}
                onMouseLeave={() => setHoveredTemplate(null)}
                style={{
                  backgroundColor: isHovered ? '#252525' : '#111111',
                  border: `1px solid ${isHovered ? '#ea580c' : '#2d2d2d'}`,
                  borderRadius: '6px',
                  padding: '16px',
                  cursor: 'pointer',
                  transition: 'all 0.15s ease',
                }}
                onClick={() => handleImport(template)}
              >
                {/* Template Header */}
                <div
                  style={{
                    display: 'flex',
                    alignItems: 'flex-start',
                    justifyContent: 'space-between',
                    marginBottom: '8px',
                  }}
                >
                  <div style={{ flex: 1 }}>
                    <div
                      style={{
                        color: '#ffffff',
                        fontSize: '13px',
                        fontWeight: 'bold',
                        marginBottom: '4px',
                      }}
                    >
                      {template.name}
                    </div>
                    <div
                      style={{
                        display: 'inline-flex',
                        alignItems: 'center',
                        gap: '4px',
                        backgroundColor: catStyle.color + '20',
                        color: catStyle.color,
                        padding: '2px 8px',
                        borderRadius: '3px',
                        fontSize: '10px',
                        fontWeight: 'bold',
                      }}
                    >
                      {catStyle.icon}
                      {catStyle.label}
                    </div>
                  </div>
                  <div
                    style={{
                      color: isHovered ? '#ea580c' : '#404040',
                      transition: 'color 0.15s ease',
                    }}
                  >
                    <ArrowRight size={18} />
                  </div>
                </div>

                {/* Description */}
                <div
                  style={{
                    color: '#a3a3a3',
                    fontSize: '11px',
                    lineHeight: '1.5',
                    marginBottom: '10px',
                  }}
                >
                  {template.description}
                </div>

                {/* Footer: Node/Edge count + Tags */}
                <div
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                  }}
                >
                  <div style={{ display: 'flex', gap: '10px' }}>
                    <span style={{ color: '#737373', fontSize: '10px' }}>
                      <BarChart2
                        size={10}
                        style={{ display: 'inline', marginRight: '3px', verticalAlign: 'middle' }}
                      />
                      {template.nodes.length} nodes
                    </span>
                    <span style={{ color: '#737373', fontSize: '10px' }}>
                      {template.edges.length} connections
                    </span>
                  </div>
                  <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                    {template.tags.slice(0, 3).map((tag) => (
                      <span
                        key={tag}
                        style={{
                          backgroundColor: '#2d2d2d',
                          color: '#a3a3a3',
                          padding: '1px 6px',
                          borderRadius: '2px',
                          fontSize: '9px',
                        }}
                      >
                        {tag}
                      </span>
                    ))}
                  </div>
                </div>
              </div>
            );
          })}
        </div>

        {/* Footer */}
        <div
          style={{
            padding: '12px 20px',
            borderTop: '1px solid #2d2d2d',
            color: '#737373',
            fontSize: '11px',
            textAlign: 'center',
          }}
        >
          Click a template to import it into the editor. Your current workflow will be replaced.
        </div>
      </div>
    </div>
  );
};

export default TemplateGallery;
