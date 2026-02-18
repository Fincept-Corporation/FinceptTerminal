/**
 * Backtesting Tab - Reusable UI Primitives
 * Design system compliant: 2px radius, flat backgrounds, no gradients/glows
 */

import React from 'react';
import { FINCEPT, TYPOGRAPHY, COMMON_STYLES, EFFECTS, createFocusHandlers } from '../../portfolio-tab/finceptStyles';
import { Maximize2, Minimize2 } from 'lucide-react';

const FONT_FAMILY = TYPOGRAPHY.MONO;

// ============================================================================
// renderInput - Terminal-style input field
// ============================================================================

export function renderInput(
  label: string,
  value: any,
  onChange: (value: any) => void,
  type: 'text' | 'number' | 'date' = 'text',
  placeholder?: string,
  min?: number,
  max?: number,
  step?: number
): React.ReactElement {
  return React.createElement('div', null,
    React.createElement('label', {
      style: {
        display: 'block',
        fontSize: '9px',
        fontWeight: 700,
        color: FINCEPT.GRAY,
        marginBottom: '4px',
        letterSpacing: '0.5px',
        fontFamily: FONT_FAMILY,
        textTransform: 'uppercase' as const,
      },
    }, label),
    React.createElement('input', {
      type,
      value,
      onChange: (e: React.ChangeEvent<HTMLInputElement>) =>
        onChange(type === 'number' ? Number(e.target.value) : e.target.value),
      placeholder,
      min,
      max,
      step,
      style: {
        width: '100%',
        padding: '8px 10px',
        backgroundColor: FINCEPT.DARK_BG,
        color: type === 'number' ? FINCEPT.CYAN : FINCEPT.WHITE,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        fontSize: '10px',
        fontFamily: FONT_FAMILY,
        outline: 'none',
      },
      onFocus: (e: React.FocusEvent<HTMLInputElement>) => {
        e.target.style.borderColor = FINCEPT.ORANGE;
      },
      onBlur: (e: React.FocusEvent<HTMLInputElement>) => {
        e.target.style.borderColor = FINCEPT.BORDER;
      },
    })
  );
}

// ============================================================================
// renderSelect - Terminal-style select dropdown
// ============================================================================

export function renderSelect(
  label: string,
  value: string,
  onChange: (value: string) => void,
  options: { value: string; label: string }[]
): React.ReactElement {
  return React.createElement('div', null,
    React.createElement('label', {
      style: {
        display: 'block',
        fontSize: '9px',
        fontWeight: 700,
        color: FINCEPT.GRAY,
        marginBottom: '4px',
        letterSpacing: '0.5px',
        fontFamily: FONT_FAMILY,
      },
    }, label.toUpperCase()),
    React.createElement('select', {
      value,
      onChange: (e: React.ChangeEvent<HTMLSelectElement>) => onChange(e.target.value),
      style: {
        width: '100%',
        padding: '8px 10px',
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        fontSize: '10px',
        fontFamily: FONT_FAMILY,
        outline: 'none',
        cursor: 'pointer',
      },
    }, ...options.map(opt =>
      React.createElement('option', { key: opt.value, value: opt.value }, opt.label)
    ))
  );
}

// ============================================================================
// renderSection - Collapsible section with header
// ============================================================================

export function renderSection(
  id: string,
  title: string,
  icon: React.ElementType,
  content: React.ReactNode,
  expandedSections: Record<string, boolean>,
  toggleSection: (id: string) => void
): React.ReactElement {
  const isExpanded = expandedSections[id] ?? true;

  return React.createElement('div', {
    style: {
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden',
    },
  },
    // Header
    React.createElement('div', {
      onClick: () => toggleSection(id),
      style: {
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: isExpanded ? `1px solid ${FINCEPT.BORDER}` : 'none',
        cursor: 'pointer',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        transition: 'all 0.2s',
      },
    },
      React.createElement('div', {
        style: { display: 'flex', alignItems: 'center', gap: '8px' },
      },
        React.createElement(icon, { size: 12, color: FINCEPT.ORANGE }),
        React.createElement('span', {
          style: {
            fontSize: '9px',
            fontWeight: 700,
            color: isExpanded ? FINCEPT.WHITE : FINCEPT.GRAY,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY,
          },
        }, title)
      ),
      React.createElement(isExpanded ? Minimize2 : Maximize2, {
        size: 10,
        color: isExpanded ? FINCEPT.ORANGE : FINCEPT.GRAY,
      })
    ),
    // Content
    isExpanded && React.createElement('div', {
      style: { padding: '12px' },
    }, content)
  );
}

// ============================================================================
// renderMetricRow - Single metric display row
// ============================================================================

export function renderMetricRow(
  label: string,
  value: string,
  color: string
): React.ReactElement {
  return React.createElement('div', {
    style: {
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      padding: '6px 8px',
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
    },
  },
    React.createElement('span', {
      style: {
        fontSize: '9px',
        color: FINCEPT.GRAY,
        fontFamily: FONT_FAMILY,
      },
    }, label),
    React.createElement('span', {
      style: {
        fontSize: '10px',
        fontWeight: 700,
        color,
        fontFamily: FONT_FAMILY,
      },
    }, value)
  );
}

// ============================================================================
// Inline select (smaller, for use within sections)
// ============================================================================

export function renderInlineSelect(
  label: string,
  value: string,
  onChange: (value: string) => void,
  options: { value: string; label: string }[]
): React.ReactElement {
  return React.createElement('div', null,
    React.createElement('div', {
      style: {
        fontSize: '9px',
        color: FINCEPT.GRAY,
        marginBottom: '4px',
        fontWeight: 700,
        letterSpacing: '0.5px',
      },
    }, label.toUpperCase()),
    React.createElement('select', {
      value,
      onChange: (e: React.ChangeEvent<HTMLSelectElement>) => onChange(e.target.value),
      style: {
        width: '100%',
        padding: '6px 8px',
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        fontSize: '9px',
        fontFamily: FONT_FAMILY,
        outline: 'none',
      },
    }, ...options.map(opt =>
      React.createElement('option', { key: opt.value, value: opt.value }, opt.label)
    ))
  );
}
