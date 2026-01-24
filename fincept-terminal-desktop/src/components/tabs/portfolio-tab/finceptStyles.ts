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

  // Panel
  panel: {
    backgroundColor: FINCEPT.PANEL_BG,
    border: BORDERS.STANDARD,
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

  // Section Header
  sectionHeader: {
    color: FINCEPT.ORANGE,
    fontSize: TYPOGRAPHY.SUBHEADING,
    fontWeight: TYPOGRAPHY.BOLD,
    letterSpacing: TYPOGRAPHY.WIDE,
    textTransform: 'uppercase' as const,
    marginBottom: SPACING.MEDIUM,
  },

  // Data Label
  dataLabel: {
    color: FINCEPT.GRAY,
    fontSize: TYPOGRAPHY.SMALL,
    fontWeight: TYPOGRAPHY.SEMIBOLD,
    textTransform: 'uppercase' as const,
  },

  // Data Value
  dataValue: {
    color: FINCEPT.WHITE,
    fontSize: TYPOGRAPHY.DEFAULT,
    fontWeight: TYPOGRAPHY.REGULAR,
    fontFamily: TYPOGRAPHY.MONO,
  },

  // Metric Card
  metricCard: {
    backgroundColor: FINCEPT.PANEL_BG,
    border: BORDERS.STANDARD,
    padding: SPACING.DEFAULT,
    display: 'flex',
    flexDirection: 'column' as const,
    gap: SPACING.SMALL,
  },

  // Button Primary
  buttonPrimary: {
    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
    backgroundColor: FINCEPT.ORANGE,
    border: BORDERS.ORANGE,
    color: FINCEPT.DARK_BG,
    fontSize: TYPOGRAPHY.BODY,
    fontWeight: TYPOGRAPHY.BOLD,
    cursor: 'pointer',
    transition: EFFECTS.TRANSITION_STANDARD,
    textTransform: 'uppercase' as const,
  },

  // Button Secondary
  buttonSecondary: {
    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
    backgroundColor: 'transparent',
    border: BORDERS.STANDARD,
    color: FINCEPT.WHITE,
    fontSize: TYPOGRAPHY.BODY,
    fontWeight: TYPOGRAPHY.SEMIBOLD,
    cursor: 'pointer',
    transition: EFFECTS.TRANSITION_STANDARD,
    textTransform: 'uppercase' as const,
  },

  // Input Field
  inputField: {
    width: '100%',
    padding: SPACING.MEDIUM,
    backgroundColor: FINCEPT.DARK_BG,
    border: BORDERS.STANDARD,
    color: FINCEPT.WHITE,
    fontSize: TYPOGRAPHY.BODY,
    fontFamily: TYPOGRAPHY.MONO,
    outline: 'none',
  },

  // Table Header
  tableHeader: {
    backgroundColor: FINCEPT.HEADER_BG,
    color: FINCEPT.ORANGE,
    fontSize: TYPOGRAPHY.BODY,
    fontWeight: TYPOGRAPHY.BOLD,
    padding: SPACING.SMALL,
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
