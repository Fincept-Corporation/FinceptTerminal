import { DocSection } from '../types';
import { Terminal, Keyboard, Settings as SettingsIcon } from 'lucide-react';

export const terminalFeaturesDoc: DocSection = {
  id: 'terminal',
  title: 'Terminal Features',
  icon: Terminal,
  subsections: [
    {
      id: 'overview',
      title: 'Terminal Overview',
      content: `Fincept Terminal is a Bloomberg-style professional financial terminal designed for traders, analysts, and investors.

**Key Features:**
• Real-time market data and quotes
• Advanced charting and technical analysis
• News aggregation and sentiment analysis
• Portfolio management and tracking
• Custom alerts and notifications
• Multi-tab workspace for efficient workflow
• Bloomberg-inspired dark theme interface
• Customizable layouts and settings

**Target Users:**
• Professional traders
• Financial analysts
• Portfolio managers
• Retail investors
• Quant developers`,
      codeExample: `// Access terminal via desktop app
// Available on Windows, macOS, and Linux

// Key capabilities:
- Real-time streaming data
- Historical data analysis
- Custom indicator development
- Automated strategy execution
- Risk management tools
- Performance analytics`
    },
    {
      id: 'shortcuts',
      title: 'Keyboard Shortcuts',
      content: `Master the terminal with these keyboard shortcuts:

**Global Shortcuts:**
• F1 - Help
• F2 - Markets
• F3 - News
• F4 - Portfolio
• F5 - Analytics
• F6 - Watchlist
• F7 - Screener
• F8 - Alerts
• F9 - Chat/AI Assistant
• F10 - Settings
• F11 - Fullscreen toggle
• Ctrl+S - Save current file
• Ctrl+N - New file
• Ctrl+O - Open file
• Ctrl+W - Close tab
• Ctrl+Tab - Next tab
• Ctrl+Shift+Tab - Previous tab

**Editor Shortcuts:**
• Ctrl+/ - Comment/Uncomment
• Ctrl+F - Find
• Ctrl+R - Run code
• Ctrl+Shift+K - Clear output

**Navigation:**
• Ctrl+Tab - Next tab
• Ctrl+Shift+Tab - Previous tab
• Ctrl+W - Close tab`,
      codeExample: `// Quick actions in editor
// Press Ctrl+/ to toggle comments
// if ema(AAPL, 21) > sma(AAPL, 50) {
//     buy "EMA crosses SMA"
// }

// Press Ctrl+R to execute code
plot ema(AAPL, 21), "EMA 21"

// Press Ctrl+S to save file`
    },
    {
      id: 'customization',
      title: 'Customization',
      content: `Customize the terminal to match your workflow:

**Themes:**
• Bloomberg Classic (default)
• Dark Mode
• Light Mode
• Custom themes via CSS

**Layouts:**
• Single panel
• Dual panel (editor + output)
• Triple panel (editor + output + charts)
• Customizable panel sizes
• Draggable and resizable windows

**Settings:**
• Font size and family
• Tab size (spaces)
• Auto-save interval
• Data refresh rate
• Chart colors and styles
• Alert notification preferences

**Workspace:**
• Save custom layouts
• Multiple workspace profiles
• Quick switching between setups`,
      codeExample: `// Example settings.json
{
  "theme": "bloomberg-classic",
  "editor": {
    "fontSize": 14,
    "fontFamily": "Consolas",
    "tabSize": 4,
    "autoSave": true,
    "autoSaveInterval": 5000
  },
  "terminal": {
    "refreshRate": 1000,
    "enableNotifications": true,
    "defaultLayout": "triple-panel"
  },
  "charts": {
    "defaultTheme": "dark",
    "gridLines": true,
    "crosshair": true
  }
}`
    }
  ]
};
