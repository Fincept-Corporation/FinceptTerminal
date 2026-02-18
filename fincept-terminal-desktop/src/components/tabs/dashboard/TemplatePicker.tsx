// TemplatePicker — Dashboard template selection modal
// Shown on first launch (no saved layout) or when user clicks "TEMPLATE" in toolbar.
// Lets user pick from 10 pre-configured professional dashboard layouts.

import React, { useState } from 'react';
import { Check, X, LayoutGrid } from 'lucide-react';
import { DASHBOARD_TEMPLATES, DashboardTemplate } from './dashboardTemplates';

const FC = {
  ORANGE:   '#FF8800',
  WHITE:    '#FFFFFF',
  GRAY:     '#787878',
  DARK_BG:  '#000000',
  PANEL_BG: '#0F0F0F',
  CARD_BG:  '#111111',
  BORDER:   '#2A2A2A',
  HOVER:    '#1A1A1A',
  MUTED:    '#4A4A4A',
  GREEN:    '#00D66F',
  BLUE:     '#0088FF',
  CYAN:     '#00E5FF',
  RED:      '#FF3B3B',
};

interface TemplatePickerProps {
  isOpen: boolean;
  isFirstLaunch: boolean;       // true = first launch (no X / skip option just a CTA)
  currentTemplateId?: string;
  onSelect: (template: DashboardTemplate) => void;
  onClose: () => void;
}

export const TemplatePicker: React.FC<TemplatePickerProps> = ({
  isOpen,
  isFirstLaunch,
  currentTemplateId,
  onSelect,
  onClose,
}) => {
  const [hovered, setHovered] = useState<string | null>(null);
  const [selected, setSelected] = useState<string>(currentTemplateId || '');

  if (!isOpen) return null;

  const handleConfirm = () => {
    const tmpl = DASHBOARD_TEMPLATES.find(t => t.id === selected);
    if (tmpl) onSelect(tmpl);
  };

  const tagColor = (tag: string) => {
    switch (tag) {
      case 'BUY-SIDE': return FC.GREEN;
      case 'ALPHA':    return FC.ORANGE;
      case 'CRYPTO':   return '#F7931A';
      case 'EQUITY':   return FC.BLUE;
      case 'MACRO':    return FC.CYAN;
      case 'INTEL':    return '#9D4EDD';
      case 'INDIA':    return '#FF9933';
      case 'USA':      return '#3C3B6E';
      case 'ASIA':     return '#DE2910';
      case 'CUSTOM':   return FC.GRAY;
      default:         return FC.GRAY;
    }
  };

  return (
    <div style={{
      position: 'fixed',
      inset: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 9999,
      backdropFilter: 'blur(4px)',
    }}>
      <div style={{
        backgroundColor: FC.DARK_BG,
        border: `1px solid ${FC.BORDER}`,
        width: '90vw',
        maxWidth: '960px',
        maxHeight: '90vh',
        display: 'flex',
        flexDirection: 'column',
        fontFamily: '"IBM Plex Mono", monospace',
      }}>

        {/* Header */}
        <div style={{
          padding: '16px 20px',
          borderBottom: `1px solid ${FC.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          backgroundColor: '#0A0A0A',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <LayoutGrid size={16} color={FC.ORANGE} />
            <div>
              <div style={{ fontSize: '13px', fontWeight: 700, color: FC.WHITE, letterSpacing: '1px' }}>
                {isFirstLaunch ? 'WELCOME TO FINCEPT TERMINAL' : 'CHANGE DASHBOARD TEMPLATE'}
              </div>
              <div style={{ fontSize: '10px', color: FC.GRAY, marginTop: '2px' }}>
                {isFirstLaunch
                  ? 'Select a dashboard template that matches your role to get started'
                  : 'Select a template to reconfigure your dashboard widgets'}
              </div>
            </div>
          </div>
          {!isFirstLaunch && (
            <button
              onClick={onClose}
              style={{ background: 'none', border: 'none', cursor: 'pointer', color: FC.GRAY, padding: '4px' }}
            >
              <X size={16} />
            </button>
          )}
        </div>

        {/* Template Grid */}
        <div style={{
          flex: 1,
          overflowY: 'auto',
          padding: '20px',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(260px, 1fr))',
          gap: '12px',
          alignContent: 'start',
        }}>
          {DASHBOARD_TEMPLATES.map(tmpl => {
            const isSelected = selected === tmpl.id;
            const isHov = hovered === tmpl.id;

            return (
              <div
                key={tmpl.id}
                onClick={() => setSelected(tmpl.id)}
                onMouseEnter={() => setHovered(tmpl.id)}
                onMouseLeave={() => setHovered(null)}
                style={{
                  backgroundColor: isSelected ? '#0D1A0D' : isHov ? FC.HOVER : FC.CARD_BG,
                  border: `1px solid ${isSelected ? FC.GREEN : isHov ? FC.ORANGE : FC.BORDER}`,
                  borderRadius: '2px',
                  padding: '14px',
                  cursor: 'pointer',
                  transition: 'border-color 0.15s, background-color 0.15s',
                  position: 'relative',
                }}
              >
                {/* Selected checkmark */}
                {isSelected && (
                  <div style={{
                    position: 'absolute',
                    top: '10px',
                    right: '10px',
                    backgroundColor: FC.GREEN,
                    borderRadius: '50%',
                    width: '18px',
                    height: '18px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                  }}>
                    <Check size={11} color={FC.DARK_BG} />
                  </div>
                )}

                {/* Icon + tag row */}
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
                  <span style={{ fontSize: '22px', lineHeight: 1 }}>{tmpl.icon}</span>
                  <span style={{
                    fontSize: '8px',
                    fontWeight: 700,
                    letterSpacing: '1px',
                    color: tagColor(tmpl.tag),
                    backgroundColor: `${tagColor(tmpl.tag)}18`,
                    border: `1px solid ${tagColor(tmpl.tag)}40`,
                    padding: '2px 6px',
                    borderRadius: '2px',
                  }}>
                    {tmpl.tag}
                  </span>
                  <span style={{ fontSize: '8px', color: FC.MUTED, marginLeft: 'auto', paddingRight: '20px' }}>
                    {tmpl.region}
                  </span>
                </div>

                {/* Name */}
                <div style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: isSelected ? FC.GREEN : FC.WHITE,
                  marginBottom: '6px',
                  letterSpacing: '0.5px',
                }}>
                  {tmpl.name}
                </div>

                {/* Description */}
                <div style={{
                  fontSize: '9px',
                  color: FC.GRAY,
                  lineHeight: '1.5',
                  marginBottom: '10px',
                }}>
                  {tmpl.description}
                </div>

                {/* Widget count */}
                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                  <span style={{
                    fontSize: '8px',
                    color: FC.MUTED,
                    backgroundColor: '#1A1A1A',
                    border: `1px solid ${FC.BORDER}`,
                    padding: '2px 6px',
                    borderRadius: '2px',
                  }}>
                    {tmpl.widgets.length === 0 ? 'EMPTY — BUILD YOUR OWN' : `${tmpl.widgets.length} WIDGETS`}
                  </span>
                </div>
              </div>
            );
          })}
        </div>

        {/* Footer */}
        <div style={{
          padding: '14px 20px',
          borderTop: `1px solid ${FC.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          backgroundColor: '#0A0A0A',
        }}>
          <div style={{ fontSize: '9px', color: FC.MUTED }}>
            You can change your template anytime from the dashboard toolbar.
          </div>

          <div style={{ display: 'flex', gap: '8px' }}>
            {isFirstLaunch && (
              <button
                onClick={() => {
                  // Skip: use default layout
                  onClose();
                }}
                style={{
                  padding: '7px 16px',
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '1px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.GRAY,
                  cursor: 'pointer',
                  borderRadius: '2px',
                }}
              >
                SKIP
              </button>
            )}
            <button
              onClick={handleConfirm}
              disabled={!selected}
              style={{
                padding: '7px 24px',
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '1px',
                fontFamily: '"IBM Plex Mono", monospace',
                backgroundColor: selected ? FC.GREEN : FC.MUTED,
                border: 'none',
                color: selected ? FC.DARK_BG : FC.GRAY,
                cursor: selected ? 'pointer' : 'not-allowed',
                borderRadius: '2px',
              }}
            >
              {isFirstLaunch ? 'GET STARTED →' : 'APPLY TEMPLATE'}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
