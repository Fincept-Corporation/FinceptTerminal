/**
 * Bloomberg Terminal Professional Theme
 * Centralized color palette and styling constants
 */

export const BLOOMBERG = {
  // Core Colors
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',

  // Backgrounds
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  INPUT_BG: '#0A0A0A',
  HOVER: '#1F1F1F',

  // Accent Colors
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',

  // UI Elements
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A'
} as const;

// Common Style Patterns
export const BloombergStyles = {
  // Label Style (for form labels)
  label: {
    display: 'block' as const,
    fontSize: '9px',
    fontWeight: 700,
    color: BLOOMBERG.GRAY,
    marginBottom: '4px',
    letterSpacing: '0.5px',
    textTransform: 'uppercase' as const,
    fontFamily: 'monospace'
  },

  // Input Style (for text inputs, selects)
  input: {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: BLOOMBERG.INPUT_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    borderRadius: '0',
    color: BLOOMBERG.WHITE,
    fontSize: '12px',
    fontWeight: 600,
    fontFamily: 'monospace',
    outline: 'none',
    transition: 'border-color 0.15s ease'
  },

  // Button Style (primary action buttons)
  button: {
    padding: '10px 16px',
    backgroundColor: BLOOMBERG.ORANGE,
    border: `1px solid ${BLOOMBERG.ORANGE}`,
    borderRadius: '0',
    color: BLOOMBERG.DARK_BG,
    fontSize: '11px',
    fontWeight: 700,
    letterSpacing: '1px',
    cursor: 'pointer',
    transition: 'all 0.15s ease',
    fontFamily: 'monospace',
    outline: 'none',
    textTransform: 'uppercase' as const
  },

  // Panel Header Style
  panelHeader: {
    padding: '8px 12px',
    backgroundColor: BLOOMBERG.HEADER_BG,
    borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
    fontSize: '11px',
    fontWeight: 700,
    color: BLOOMBERG.WHITE,
    letterSpacing: '0.5px',
    fontFamily: 'monospace'
  },

  // Table Header Style
  tableHeader: {
    fontSize: '9px',
    fontWeight: 700,
    color: BLOOMBERG.GRAY,
    letterSpacing: '0.5px',
    textTransform: 'uppercase' as const,
    padding: '8px 12px',
    borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
    fontFamily: 'monospace'
  },

  // Table Cell Style
  tableCell: {
    fontSize: '11px',
    fontWeight: 600,
    color: BLOOMBERG.WHITE,
    padding: '10px 12px',
    borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
    fontFamily: 'monospace'
  }
} as const;

// Helper function for select dropdown arrow
export const selectDropdownArrow = `url("data:image/svg+xml,%3Csvg width='10' height='6' viewBox='0 0 10 6' fill='none' xmlns='http://www.w3.org/2000/svg'%3E%3Cpath d='M1 1L5 5L9 1' stroke='%23787878' stroke-width='1.5'/%3E%3C/svg%3E")`;
