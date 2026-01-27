/**
 * Fincept Terminal Style Constants for Portfolio Tab
 *
 * This file contains all styling constants to ensure consistent
 * Fincept-style terminal aesthetics across all portfolio components.
 */

// ========== COLOR PALETTE ==========
// These reference CSS custom properties set by ThemeContext
// so they update dynamically when user changes theme in Settings
export const FINCEPT = {
  // Primary Colors
  ORANGE: 'var(--ft-color-primary, #FF8800)',
  WHITE: 'var(--ft-color-text, #FFFFFF)',

  // Status Colors
  RED: 'var(--ft-color-alert, #FF3B3B)',
  GREEN: 'var(--ft-color-success, #00D66F)',
  YELLOW: 'var(--ft-color-warning, #FFD700)',

  // Accent Colors
  CYAN: 'var(--ft-color-accent, #00E5FF)',
  BLUE: 'var(--ft-color-info, #0088FF)',
  PURPLE: 'var(--ft-color-purple, #9D4EDD)',

  // Grayscale
  GRAY: 'var(--ft-color-text-muted, #787878)',
  MUTED: '#4A4A4A',

  // Backgrounds
  DARK_BG: 'var(--ft-color-background, #000000)',
  PANEL_BG: 'var(--ft-color-panel, #0F0F0F)',
  HEADER_BG: '#1A1A1A',
  HOVER: '#1F1F1F',

  // Borders
  BORDER: '#2A2A2A',
} as const;

// ========== TYPOGRAPHY ==========
// These reference CSS custom properties set by ThemeContext
// so they update dynamically when user changes font in Settings
export const TYPOGRAPHY = {
  // Font Families
  MONO: 'var(--ft-font-family)',

  // Font Sizes (consistent scale via CSS vars)
  TINY: 'var(--ft-font-size-tiny, 9px)',
  SMALL: 'var(--ft-font-size-small, 10px)',
  BODY: 'var(--ft-font-size-body, 11px)',
  DEFAULT: 'var(--ft-font-size-body, 11px)',
  SUBHEADING: 'var(--ft-font-size-subheading, 13px)',
  HEADING: 'var(--ft-font-size-heading, 15px)',
  LARGE: '16px',

  // Font Weights
  REGULAR: 'var(--ft-font-weight, 400)' as unknown as number,
  SEMIBOLD: 600,
  BOLD: 700,

  // Letter Spacing
  TIGHT: '0px',
  NORMAL: '0.3px',
  WIDE: '0.5px',
} as const;

// ========== SPACING ==========
export const SPACING = {
  // Padding/Margin scale
  NONE: '0px',
  TINY: '2px',
  SMALL: '4px',
  MEDIUM: '8px',
  DEFAULT: '12px',
  LARGE: '16px',
  XLARGE: '24px',

  // Gaps
  GAP_TINY: '4px',
  GAP_SMALL: '6px',
  GAP_MEDIUM: '8px',
  GAP_DEFAULT: '12px',
  GAP_LARGE: '16px',
} as const;

// ========== BORDERS ==========
export const BORDERS = {
  // Border widths
  THIN: '1px',
  MEDIUM: '2px',

  // Border styles
  STANDARD: `1px solid ${FINCEPT.BORDER}`,
  ORANGE: `1px solid ${FINCEPT.ORANGE}`,
  ORANGE_THICK: `2px solid ${FINCEPT.ORANGE}`,
  RED: `1px solid ${FINCEPT.RED}`,
  GREEN: `1px solid ${FINCEPT.GREEN}`,
  CYAN: `1px solid ${FINCEPT.CYAN}`,

  // Border radius (minimal for terminal aesthetic)
  RADIUS_NONE: '0px',
  RADIUS_SMALL: '2px',
  RADIUS_MEDIUM: '4px',
} as const;

// ========== LAYOUT ==========
export const LAYOUT = {
  // Sidebar widths
  SIDEBAR_LEFT: '320px',
  SIDEBAR_RIGHT: '300px',

  // Modal widths
  MODAL_SMALL: '400px',
  MODAL_MEDIUM: '600px',
  MODAL_LARGE: '800px',

  // Header/Footer heights
  HEADER_HEIGHT: '40px',
  FOOTER_HEIGHT: '32px',
  TICKER_BAR_HEIGHT: '36px',

  // Panel heights
  PANEL_MIN_HEIGHT: '200px',
  PANEL_CHART_HEIGHT: '400px',
} as const;

// ========== EFFECTS ==========
export const EFFECTS = {
  // Text shadows (glow effects)
  ORANGE_GLOW: `0 0 10px ${FINCEPT.ORANGE}40`,
  ORANGE_GLOW_STRONG: `0 0 15px ${FINCEPT.ORANGE}60`,

  // Box shadows
  PANEL_SHADOW: `0 2px 8px ${FINCEPT.ORANGE}20`,
  PANEL_SHADOW_STRONG: `0 4px 16px ${FINCEPT.DARK_BG}80`,

  // Drop shadows (for icons)
  ICON_GLOW_ORANGE: `drop-shadow(0 0 4px ${FINCEPT.ORANGE})`,
  ICON_GLOW_GREEN: `drop-shadow(0 0 4px ${FINCEPT.GREEN})`,
  ICON_GLOW_RED: `drop-shadow(0 0 4px ${FINCEPT.RED})`,

  // Transitions
  TRANSITION_FAST: 'all 0.15s ease',
  TRANSITION_STANDARD: 'all 0.2s ease',
  TRANSITION_SLOW: 'all 0.3s ease',
} as const;

// ========== COMMON STYLES ==========
export const COMMON_STYLES = {
  // Container
  container: {
    height: '100%',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    fontFamily: TYPOGRAPHY.MONO,
    overflow: 'hidden',
    display: 'flex',
    flexDirection: 'column' as const,
  },

  // Panel (per design system: 2px radius)
  panel: {
    backgroundColor: FINCEPT.PANEL_BG,
    border: BORDERS.STANDARD,
    borderRadius: '2px',
    padding: SPACING.DEFAULT,
  },

  // Header
  header: {
    backgroundColor: FINCEPT.HEADER_BG,
    borderBottom: BORDERS.ORANGE_THICK,
    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'space-between',
    flexShrink: 0,
    boxShadow: EFFECTS.PANEL_SHADOW,
  },

  // Section Header (panel-style with background per design system)
  sectionHeader: {
    padding: SPACING.DEFAULT,
    backgroundColor: FINCEPT.HEADER_BG,
    borderBottom: `1px solid ${FINCEPT.BORDER}`,
    color: FINCEPT.ORANGE,
    fontSize: '11px',
    fontWeight: TYPOGRAPHY.BOLD,
    letterSpacing: TYPOGRAPHY.WIDE,
    textTransform: 'uppercase' as const,
    marginBottom: SPACING.MEDIUM,
  },

  // Data Label (per design system: 9px, bold, GRAY, letter-spacing, UPPERCASE)
  dataLabel: {
    color: FINCEPT.GRAY,
    fontSize: '9px',
    fontWeight: TYPOGRAPHY.BOLD,
    letterSpacing: '0.5px',
    textTransform: 'uppercase' as const,
  },

  // Data Value (per design system: 10px, CYAN, monospace)
  dataValue: {
    color: FINCEPT.CYAN,
    fontSize: '10px',
    fontWeight: TYPOGRAPHY.REGULAR,
    fontFamily: TYPOGRAPHY.MONO,
  },

  // Metric Card (per design system: panel bg, 2px radius, border)
  metricCard: {
    backgroundColor: FINCEPT.PANEL_BG,
    border: BORDERS.STANDARD,
    borderRadius: '2px',
    padding: SPACING.DEFAULT,
    display: 'flex',
    flexDirection: 'column' as const,
    gap: SPACING.SMALL,
  },

  // Button Primary (per design system: 8px 16px, orange bg, dark text, 9px bold, 2px radius)
  buttonPrimary: {
    padding: '8px 16px',
    backgroundColor: FINCEPT.ORANGE,
    color: FINCEPT.DARK_BG,
    border: 'none',
    borderRadius: '2px',
    fontSize: '9px',
    fontWeight: TYPOGRAPHY.BOLD,
    cursor: 'pointer',
    transition: EFFECTS.TRANSITION_STANDARD,
    textTransform: 'uppercase' as const,
    letterSpacing: '0.5px',
  },

  // Button Secondary / Outline (per design system: 6px 10px, transparent, border, 9px bold)
  buttonSecondary: {
    padding: '6px 10px',
    backgroundColor: 'transparent',
    border: BORDERS.STANDARD,
    color: FINCEPT.GRAY,
    borderRadius: '2px',
    fontSize: '9px',
    fontWeight: TYPOGRAPHY.BOLD,
    cursor: 'pointer',
    transition: EFFECTS.TRANSITION_STANDARD,
    textTransform: 'uppercase' as const,
    letterSpacing: '0.5px',
  },

  // Input Field (per design system: dark bg, 8px 10px padding, 10px, 2px radius)
  inputField: {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: BORDERS.STANDARD,
    borderRadius: '2px',
    fontSize: '10px',
    fontFamily: '"IBM Plex Mono", monospace',
    outline: 'none',
  },

  // Tab Button (per design system: 6px 12px, 9px bold, letter-spacing)
  tabButton: (isActive: boolean) => ({
    padding: '6px 12px',
    backgroundColor: isActive ? FINCEPT.ORANGE : 'transparent',
    color: isActive ? FINCEPT.DARK_BG : FINCEPT.GRAY,
    border: 'none',
    borderRadius: '2px',
    fontSize: '9px',
    fontWeight: TYPOGRAPHY.BOLD,
    letterSpacing: '0.5px',
    cursor: 'pointer',
    transition: EFFECTS.TRANSITION_FAST,
    textTransform: 'uppercase' as const,
  }),

  // Status Badge Success
  badgeSuccess: {
    padding: '2px 6px',
    backgroundColor: `${FINCEPT.GREEN}20`,
    color: FINCEPT.GREEN,
    fontSize: '8px',
    fontWeight: TYPOGRAPHY.BOLD,
    borderRadius: '2px',
  },

  // Status Badge Error
  badgeError: {
    padding: '2px 6px',
    backgroundColor: `${FINCEPT.RED}20`,
    color: FINCEPT.RED,
    fontSize: '8px',
    fontWeight: TYPOGRAPHY.BOLD,
    borderRadius: '2px',
  },

  // Status Badge Info
  badgeInfo: {
    padding: '2px 6px',
    backgroundColor: `${FINCEPT.CYAN}20`,
    color: FINCEPT.CYAN,
    fontSize: '8px',
    fontWeight: TYPOGRAPHY.BOLD,
    borderRadius: '2px',
  },

  // Empty State (per design system)
  emptyState: {
    display: 'flex',
    flexDirection: 'column' as const,
    alignItems: 'center',
    justifyContent: 'center',
    height: '100%',
    color: FINCEPT.MUTED,
    fontSize: '10px',
    textAlign: 'center' as const,
  },

  // Table Header (per design system: HEADER_BG, ORANGE text, 9px bold, UPPERCASE)
  tableHeader: {
    backgroundColor: FINCEPT.HEADER_BG,
    color: FINCEPT.ORANGE,
    fontSize: '9px',
    fontWeight: TYPOGRAPHY.BOLD,
    letterSpacing: '0.5px',
    padding: '8px 12px',
    textAlign: 'left' as const,
    borderBottom: BORDERS.ORANGE,
    textTransform: 'uppercase' as const,
  },

  // Table Row
  tableRow: {
    padding: SPACING.TINY,
    fontSize: TYPOGRAPHY.BODY,
    borderBottom: BORDERS.STANDARD,
    minHeight: '24px',
    display: 'flex',
    alignItems: 'center',
  },

  // Table Row Alternate
  tableRowAlt: {
    backgroundColor: 'rgba(255,255,255,0.03)',
  },

  // Divider
  divider: {
    height: '1px',
    backgroundColor: FINCEPT.BORDER,
    margin: `${SPACING.MEDIUM} 0`,
  },

  // Vertical Divider
  verticalDivider: {
    width: '1px',
    backgroundColor: FINCEPT.BORDER,
    alignSelf: 'stretch',
  },

  // Modal Overlay
  modalOverlay: {
    position: 'fixed' as const,
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0,0,0,0.85)',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    zIndex: 9999,
  },

  // Modal Panel
  modalPanel: {
    backgroundColor: FINCEPT.PANEL_BG,
    border: BORDERS.ORANGE_THICK,
    padding: SPACING.XLARGE,
    minWidth: '400px',
    maxWidth: '600px',
    boxShadow: EFFECTS.PANEL_SHADOW_STRONG,
  },

  // Scrollbar styling (inject into <style> tag)
  scrollbarCSS: `
    *::-webkit-scrollbar {
      width: 6px;
      height: 6px;
    }
    *::-webkit-scrollbar-track {
      background: ${FINCEPT.DARK_BG};
    }
    *::-webkit-scrollbar-thumb {
      background: ${FINCEPT.BORDER};
      border-radius: 3px;
    }
    *::-webkit-scrollbar-thumb:hover {
      background: ${FINCEPT.MUTED};
    }
  `,
} as const;

// ========== UTILITY FUNCTIONS ==========

/**
 * Get color based on value (positive/negative)
 */
export function getValueColor(value: number): string {
  if (value > 0) return FINCEPT.GREEN;
  if (value < 0) return FINCEPT.RED;
  return FINCEPT.WHITE;
}

/**
 * Get formatted percentage with color
 */
export function formatPercentage(value: number, decimals: number = 2): string {
  const sign = value >= 0 ? '+' : '';
  return `${sign}${value.toFixed(decimals)}%`;
}

/**
 * Get formatted currency
 */
export function formatCurrency(value: number, currency: string = '$', decimals: number = 2): string {
  return `${currency}${value.toLocaleString('en-US', { minimumFractionDigits: decimals, maximumFractionDigits: decimals })}`;
}

/**
 * Create hover style
 */
export function createHoverStyle(baseStyle: React.CSSProperties): {
  style: React.CSSProperties;
  onMouseEnter: (e: React.MouseEvent<HTMLElement>) => void;
  onMouseLeave: (e: React.MouseEvent<HTMLElement>) => void;
} {
  return {
    style: baseStyle,
    onMouseEnter: (e) => {
      e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
    },
    onMouseLeave: (e) => {
      e.currentTarget.style.backgroundColor = baseStyle.backgroundColor || 'transparent';
    },
  };
}

/**
 * Create focus style handlers for inputs
 */
export function createFocusHandlers() {
  return {
    onFocus: (e: React.FocusEvent<HTMLInputElement | HTMLTextAreaElement>) => {
      e.currentTarget.style.borderColor = FINCEPT.ORANGE;
    },
    onBlur: (e: React.FocusEvent<HTMLInputElement | HTMLTextAreaElement>) => {
      e.currentTarget.style.borderColor = FINCEPT.BORDER;
    },
  };
}

// ========== GRID TEMPLATES ==========
export const GRID_TEMPLATES = {
  // 2-column grid
  twoColumn: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr',
    gap: SPACING.GAP_DEFAULT,
  },

  // 3-column grid
  threeColumn: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr 1fr',
    gap: SPACING.GAP_DEFAULT,
  },

  // 4-column grid
  fourColumn: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr 1fr 1fr',
    gap: SPACING.GAP_DEFAULT,
  },

  // Auto-fit grid (responsive)
  autoFit: (minWidth: string = '250px') => ({
    display: 'grid',
    gridTemplateColumns: `repeat(auto-fit, minmax(${minWidth}, 1fr))`,
    gap: SPACING.GAP_DEFAULT,
  }),
} as const;

// ========== ANIMATION KEYFRAMES ==========
export const ANIMATIONS = {
  // Price flash animation
  priceFlash: `
    @keyframes priceFlash {
      0% { background-color: ${FINCEPT.YELLOW}40; }
      100% { background-color: transparent; }
    }
  `,

  // Pulse animation
  pulse: `
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
  `,

  // Fade in animation
  fadeIn: `
    @keyframes fadeIn {
      from { opacity: 0; }
      to { opacity: 1; }
    }
  `,
} as const;

export default {
  FINCEPT,
  TYPOGRAPHY,
  SPACING,
  BORDERS,
  LAYOUT,
  EFFECTS,
  COMMON_STYLES,
  GRID_TEMPLATES,
  ANIMATIONS,
  getValueColor,
  formatPercentage,
  formatCurrency,
  createHoverStyle,
  createFocusHandlers,
};
