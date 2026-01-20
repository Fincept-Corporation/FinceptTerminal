// Fincept color palette for Report Builder
export const FINCEPT_COLORS = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  BLACK: '#000000',
  DARK_BG: '#0a0a0a',
  PANEL_BG: '#1a1a1a',
  BORDER: '#333333',
  HOVER: '#2a2a2a',
  TEXT_PRIMARY: '#FFFFFF',
  TEXT_SECONDARY: '#999999',
} as const;

// Page theme options
export const PAGE_THEMES = {
  classic: 'Classic White',
  minimal: 'Minimal Grid',
  geometric: 'Geometric Triangles',
  pattern: 'Diagonal Pattern',
  modern: 'Modern Gradient (Dark)',
  gradient: 'Professional Gradient (Dark)',
} as const;

// Font family options
export const FONT_FAMILIES = [
  { value: 'Arial, sans-serif', label: 'Arial' },
  { value: "'Times New Roman', serif", label: 'Times New Roman' },
  { value: 'Georgia, serif', label: 'Georgia' },
  { value: "'Courier New', monospace", label: 'Courier New' },
  { value: 'Verdana, sans-serif', label: 'Verdana' },
  { value: "'Trebuchet MS', sans-serif", label: 'Trebuchet MS' },
  { value: "'Palatino Linotype', serif", label: 'Palatino' },
  { value: 'Garamond, serif', label: 'Garamond' },
  { value: "'Comic Sans MS', cursive", label: 'Comic Sans MS' },
  { value: "'Lucida Console', monospace", label: 'Lucida Console' },
] as const;

// Font size options
export const FONT_SIZES = [
  { value: 'xs', label: 'Extra Small' },
  { value: 'sm', label: 'Small' },
  { value: 'base', label: 'Base' },
  { value: 'lg', label: 'Large' },
  { value: 'xl', label: 'Extra Large' },
  { value: '2xl', label: '2X Large' },
  { value: '3xl', label: '3X Large' },
] as const;

// Chart type options
export const CHART_TYPES = [
  { value: 'bar', label: 'Bar Chart' },
  { value: 'line', label: 'Line Chart' },
  { value: 'pie', label: 'Pie Chart' },
  { value: 'area', label: 'Area Chart' },
] as const;

// Preset colors for color picker
export const PRESET_COLORS = [
  '#ffffff', '#f8f9fa', '#e9ecef', '#dee2e6', '#ced4da', '#adb5bd', '#6c757d', '#495057',
  '#fff3cd', '#d1ecf1', '#d4edda', '#f8d7da', '#e2e3e5', '#d6d8db', '#fef5e7', '#ebf5fb',
  '#ff0000', '#ff7f00', '#ffff00', '#00ff00', '#00ffff', '#0000ff', '#8b00ff', '#ff00ff',
  '#fff5f5', '#ffe6e6', '#ffd6cc', '#ffe5cc', '#fff4cc', '#ffffcc', '#f0fff4', '#f0f9ff',
] as const;

// AI Quick action prompts
export const AI_QUICK_ACTIONS: Record<string, string> = {
  improve: 'Can you help me improve the content of this report? Suggest better wording and structure.',
  expand: 'Can you help me expand on the current content with more details and insights?',
  summarize: 'Can you help me create a concise executive summary for this report?',
  grammar: 'Can you check the grammar and suggest improvements for professional writing?',
};
